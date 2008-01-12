/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

#include <stdlib.h>
#include <stdio.h>

#include <libspe.h>
#include "3d.h"

extern spe_program_handle_t spu_3d_handle;
spe_program_handle_t* spu_3d_program = &spu_3d_handle;

typedef struct {
        speid_t spe_id;
	void* local_store;
	SPU_CONTROL* control;
	int master;
} __DRIVER_CONTEXT;

static u32 spe_read(speid_t spe_id);

// initialise
DriverContext _init_3d_driver(int master)
{
	__DRIVER_CONTEXT* context = (__DRIVER_CONTEXT*) malloc(sizeof(__DRIVER_CONTEXT));
	if (context == NULL)
		return NULL;

	context->spe_id = spe_create_thread(0, spu_3d_program, NULL, NULL, -1, 0);
	context->master = master;

	if (context->spe_id==0) {
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

	spe_write_in_mbox(context->spe_id, 
		 master ? SPU_MBOX_3D_INITIALISE_MASTER
			: SPU_MBOX_3D_INITIALISE_NORMAL); 
	spe_read(context->spe_id);

	return (DriverContext) context;
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
	return status;
}

u32 _3d_spu_address(DriverContext _context, u32* address)
{
	return (u32)((void*)(address) -
			((__DRIVER_CONTEXT*)_context)->local_store);
}

u32 _3d_idle_count(DriverContext _context)
{
	__DRIVER_CONTEXT* context = (__DRIVER_CONTEXT*) _context;
	return (u32)(context->control->idle_count);
}

u32* _begin_fifo(DriverContext _context)
{
	__DRIVER_CONTEXT* context = (__DRIVER_CONTEXT*) _context;
	return (u32*)_FROM_EA(context->control->fifo_written);
}

void _end_fifo(DriverContext _context, u32* fifo)
{
	__DRIVER_CONTEXT* context = (__DRIVER_CONTEXT*) _context;
	SPU_CONTROL* control = context->control;
	control->fifo_written = _MAKE_EA(fifo);

	if (control->fifo_written - control->fifo_start
			> control->fifo_size - SPU_MIN_FIFO) {
		*fifo++ = SPU_COMMAND_JMP;
		*fifo++ = control->fifo_start;
		control->fifo_written = ((void*)fifo) - context->local_store;
	}
/*
	int written = ((u32*)fifo) - ((u32*)context->fifo_write);
	context->fifo_left -= written;
	
	if (context->fifo_left < SPU_MIN_FIFO) {
		printf("Only %d bytes remaining; causing jump to %lx\n",
			context->fifo_left, control->fifo_start);
		*fifo++ = SPU_COMMAND_JMP;
		*fifo++ = control->fifo_start;

		control->fifo_written = control->fifo_start;
		context->fifo_left = control->fifo_size;

		control->fifo_written = context->fifo_write;
	}
*/
}

void _bind_child(DriverContext _parent, DriverContext _child, int assign)
{
	__DRIVER_CONTEXT* parent = (__DRIVER_CONTEXT*) _parent;
	__DRIVER_CONTEXT* child = (__DRIVER_CONTEXT*) _child;
	if (parent->master && !child->master) {
		u32* fifo = _begin_fifo(parent);
		*fifo++ = assign ?
			SPU_COMMAND_ADD_CHILD :
			SPU_COMMAND_DEL_CHILD;
		u64 ea = _MAKE_EA(child->control);
		*fifo++ = (u32)(ea>>32);
		*fifo++ = (u32)(ea&0xffffffffUL);
		*fifo++ = SPU_COMMAND_NOP;
		_end_fifo(parent,fifo);
	}
}

static u32 spe_read(speid_t spe_id) {
	while(!spe_stat_out_mbox(spe_id))
		; //usleep(10);
	return spe_read_out_mbox(spe_id);
}
