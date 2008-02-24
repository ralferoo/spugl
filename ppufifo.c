/****************************************************************************
 *
 * SPU GL - 3d rasterisation library for the PS3
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> 
 *
 * This library may not be used or distributed without a licence, please
 * contact me for information if you wish to use it.
 *
 ****************************************************************************/

#include <stdlib.h>
#include <stdio.h>

#include <sys/mman.h>
#include <libspe.h>
#include "fifo.h"

// #define DOTS_WHEN_WAITING_FOR_FIFO

// the size of the fragment buffer
// it needs to be at least the size of a frame in order to be able
// to store a z-buffer and we will need other fragments as well
// for partially rendered things
#define FRAGMENT_SLOP 1024
#define FRAGMENT_BUFFER_SIZE 4096
//(1920*1080*4 + FRAGMENT_SLOP*1024)

#define FIFO_BUFFER_SIZE (2*1024*1024)

extern spe_program_handle_t spu_3d_handle;
spe_program_handle_t* spu_3d_program = &spu_3d_handle;

typedef struct {
        speid_t spe_id;
	void* local_store;
	SPU_CONTROL* control;
	int master;
	void* fragment_buffer;
	void* fifo_buffer;
} __DRIVER_CONTEXT;

static u32 spe_read(speid_t spe_id);

u32* prepare_texture(gimp_image* source);
extern gimp_image berlin;
extern gimp_image mim;
extern gimp_image ralf;
extern gimp_image gate;
extern gimp_image space;
extern gimp_image tongariro;

// initialise
DriverContext _init_3d_driver(int master)
{
	__DRIVER_CONTEXT* context = (__DRIVER_CONTEXT*) malloc(sizeof(__DRIVER_CONTEXT));
	if (context == NULL)
		return NULL;

	context->fragment_buffer = NULL;
	context->fifo_buffer = NULL;

	if (master) {
		void *fragment = mmap(NULL, FRAGMENT_BUFFER_SIZE,
			PROT_READ | PROT_WRITE, 
			MAP_SHARED | MAP_NORESERVE | MAP_ANONYMOUS,
			-1, 0);
		if (context->fragment_buffer != (void*)-1) {
			context->fragment_buffer = fragment;
		}

		void *fifo = mmap(NULL, FIFO_BUFFER_SIZE,
			PROT_READ | PROT_WRITE, 
			MAP_SHARED | MAP_NORESERVE | MAP_ANONYMOUS,
			-1, 0);
		if (context->fifo_buffer != (void*)-1) {
			context->fifo_buffer = fifo;
		}
	}

	context->spe_id = spe_create_thread(0, spu_3d_program, context->fifo_buffer, NULL, -1, 0);
	context->master = master;

	if (context->spe_id==0) {
		if (context->fifo_buffer)
			munmap(context->fragment_buffer, FIFO_BUFFER_SIZE);
		if (context->fragment_buffer)
			munmap(context->fragment_buffer, FRAGMENT_BUFFER_SIZE);
		free(context);
		return NULL;
	}

	void* ls = spe_get_ls(context->spe_id);
	context->local_store = ls;
//	printf("Allocated spe %lx, local store at %lx\n",
//		context->spe_id, context->local_store);

	u32 addr = spe_read(context->spe_id);
	context->control = addr + ls;
	context->control->my_local_address = _MAKE_EA(ls);
	context->control->fragment_buffer = _MAKE_EA(context->fragment_buffer);
	context->control->fragment_buflen = FRAGMENT_BUFFER_SIZE;
	context->control->fifo_size = FIFO_BUFFER_SIZE;

	// TODO: ugly hack!
	context->control->texture_hack[0] = (u64)prepare_texture(&berlin);
	context->control->texture_hack[1] = (u64)prepare_texture(&mim);
	context->control->texture_hack[2] = (u64)prepare_texture(&ralf);
	context->control->texture_hack[3] = (u64)prepare_texture(&gate);
	context->control->texture_hack[4] = (u64)prepare_texture(&space);
	context->control->texture_hack[5] = (u64)prepare_texture(&tongariro);

	return (DriverContext) context;
}

int _flush_3d_driver(DriverContext _context)
{
	__DRIVER_CONTEXT* context = (__DRIVER_CONTEXT*) _context;

	SPU_CONTROL* control = context->control;

	// use a flush as an opportunity to reset the fifo buffer
	u32* __fifo_ptr = (u32*)_FROM_EA(control->fifo_written);
	BEGIN_RING(SPU_COMMAND_JUMP,1);
	OUT_RINGea(context->fifo_buffer);
	control->fifo_written = context->fifo_buffer;

	if (control->fifo_written != control->fifo_read) {
		while (control->fifo_written != control->fifo_read) 
			sched_yield();
	}
}

int _exit_3d_driver(DriverContext _context)
{
	__DRIVER_CONTEXT* context = (__DRIVER_CONTEXT*) _context;

	if (context == NULL)
		return 0;

	// send the SPU a quit command
	spe_write_in_mbox(context->spe_id, SPU_MBOX_3D_TERMINATE);

	// wait for completion and return exit code
	int status = 0;
	spe_wait(context->spe_id, &status, 0);

	if (context->fragment_buffer)
		munmap(context->fragment_buffer, FRAGMENT_BUFFER_SIZE);

	free(context);

	return status;
}

u32 _3d_spu_address(DriverContext _context, u32* address)
{
	return (u32)((void*)(address) -
			((__DRIVER_CONTEXT*)_context)->local_store);
}

u32 _3d_blocked_count(DriverContext _context)
{
	__DRIVER_CONTEXT* context = (__DRIVER_CONTEXT*) _context;
	return (u32)(context->control->block_count);
}

u32 _3d_idle_count(DriverContext _context)
{
	__DRIVER_CONTEXT* context = (__DRIVER_CONTEXT*) _context;
	return (u32)(context->control->idle_count);
}

u32 _3d_error(DriverContext _context)
{
	__DRIVER_CONTEXT* context = (__DRIVER_CONTEXT*) _context;
	u32 error = (u32)(context->control->error);
	context->control->error = 0;
	return error;
}

u64* _get_fifo_address(DriverContext _context)
{
	__DRIVER_CONTEXT* context = (__DRIVER_CONTEXT*) _context;
	SPU_CONTROL* control = context->control;
	return &(control->fifo_written);
}

u32* _begin_fifo(DriverContext _context, u32 minsize)
{
	__DRIVER_CONTEXT* context = (__DRIVER_CONTEXT*) _context;
	SPU_CONTROL* control = context->control;
	return (u32*)_FROM_EA(control->fifo_written);
}

void _end_fifo(DriverContext _context, u32* fifo_head, u32* fifo)
{
	__DRIVER_CONTEXT* context = (__DRIVER_CONTEXT*) _context;
	SPU_CONTROL* control = context->control;
	control->fifo_written = _MAKE_EA(fifo);
}

/*
void _bind_child(DriverContext _parent, DriverContext _child, int assign)
{
	__DRIVER_CONTEXT* parent = (__DRIVER_CONTEXT*) _parent;
	__DRIVER_CONTEXT* child = (__DRIVER_CONTEXT*) _child;
	if (parent->master && !child->master) {
		u32* fifo = _begin_fifo(parent);
		*fifo++ = assign ?
			SPU_COMMAND_ADD_CHILD :
			SPU_COMMAND_DELETE_CHILD;
		u64 ea = _MAKE_EA(child->control);
		*fifo++ = (u32)(ea>>32);
		*fifo++ = (u32)(ea&0xffffffffUL);
		*fifo++ = SPU_COMMAND_NOP;
		_end_fifo(parent,fifo);
	}
}
*/

static u32 spe_read(speid_t spe_id) {
	while(!spe_stat_out_mbox(spe_id))
		; //usleep(10);
	return spe_read_out_mbox(spe_id);
}
