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

// #define DOTS

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/mman.h>
#include <fcntl.h>

#include "connection.h"
#include "functions.h"

#ifdef USE_LIBSPE2
	#include <libspe2.h>
	#include <pthread.h>
	#include <sched.h>
#else
	#include <libspe.h>
#endif

extern spe_program_handle_t main_spu_handle;
spe_program_handle_t* main_spu_program = &main_spu_handle;

extern spe_program_handle_t main_render_handle;
spe_program_handle_t* main_render_program = &main_render_handle;

struct __SPU_HANDLE {
#ifdef USE_LIBSPE2
	pthread_t thread;
	spe_context_ptr_t spe_ctx;
#else
        speid_t spe_id;
	int master;
#endif
	void* local_store;
	void* list;
	int id;
};

#ifdef USE_LIBSPE2
/* PPE Callback Function */
int flush_callback(void *ls_base_tmp, unsigned int data) {
	char *ls_base = (char *)ls_base_tmp; 
	int id = *((int*)(ls_base + data));
	receivedFlush(id);
	return 0;
}

int sleep_callback(void *ls_base_tmp, unsigned int data) {
	usleep(125);
	// sched_yield();
	// usleep(125000);

	return 0;
}

int my_callback(void *ls_base_tmp, unsigned int data) {
	char *ls_base = (char *)ls_base_tmp; 
//	spe_offset_t params_offset = *((spe_offset_t *)(ls_base + data));
/*
	my_strlen_param_t *params = (my_strlen_param_t *)(ls_base + params_offset);
	char *the_string = ls_base + params->str;
	params->length = strlen(the_string);
*/

#ifdef DOTS
	write(1,".",1);
#endif
	// usleep(125000);

	// allow other jobs to run, but start up almost immediately
	sched_yield();
	return 0;
}

void *main_program_thread(SPU_HANDLE context)
{
	int retval;
	unsigned int entry_point = SPE_DEFAULT_ENTRY;

/*
	#define MAP_SIZE 4096

	char name[256];
	sprintf(name, "/spu/spethread-%d-%d/psmap", getpid(), context->spe_ctx);
	int fd = open(name, O_RDONLY);
	void* ps = NULL;
	if (fd>=0) {
		ps = mmap(0, MAP_SIZE, PROT_READ, MAP_SHARED, fd, 0);
	}
	printf("psmap='%s', fd=%d, mmap_addr=%x\n", name, fd, ps);
*/

	do {
		spe_stop_info_t stop_info;
		//printf("[%d] restarting at %x, LS=%x\n", context->id, entry_point, context->local_store);
		retval = spe_context_run(context->spe_ctx, &entry_point, 0, context->list, (void*)(context->id), &stop_info);

		if (retval<0) {
			switch (stop_info.stop_reason) {
				case SPE_EXIT:
					printf("[%d] SPE_EXIT 0x%x at 0x%05x\n", context->id, stop_info.result.spe_exit_code, entry_point);
					break;
				case SPE_STOP_AND_SIGNAL:
					printf("[%d] SPE_STOP_AND_SIGNAL 0x%x at 0x%05x\n", context->id, stop_info.result.spe_signal_code, entry_point);
					break;
				case SPE_RUNTIME_ERROR:
					printf("[%d] SPE_RUNTIME_ERROR 0x%x at 0x%05x\n", context->id, stop_info.result.spe_runtime_error, entry_point);
					break;
				case SPE_RUNTIME_EXCEPTION:
					switch (stop_info.result.spe_runtime_exception) {
						case SPE_DMA_ALIGNMENT:
							printf("[%d] SPE_RUNTIME_EXCEPTION SPE_DMA_ALIGNMENT at 0x%05x\n", context->id, entry_point);
							break;
						case SPE_DMA_SEGMENTATION:
							printf("[%d] SPE_RUNTIME_EXCEPTION SPE_DMA_SEGMENTATION at 0x%05x\n", context->id, entry_point);
							break;
						case SPE_DMA_STORAGE:
							printf("[%d] SPE_RUNTIME_EXCEPTION SPE_DMA_STORAGE at 0x%05x\n", context->id, entry_point);
							break;
						//case SPE_INVALID_DMA:
							//printf("[%d] SPE_RUNTIME_EXCEPTION SPE_INVALID_DMA at 0x%05x\n", context->id, entry_point);
							//break;
						default:
							printf("[%d] SPE_RUNTIME_EXCEPTION 0x%x at 0x%05x\n", context->id, stop_info.result.spe_runtime_exception, entry_point);
					}
					break;
				case SPE_RUNTIME_FATAL:
					printf("[%d] SPE_RUNTIME_FATAL 0x%x at 0x%05x\n", context->id, stop_info.result.spe_runtime_fatal, entry_point);
					break;
				case SPE_CALLBACK_ERROR:
					printf("[%d] SPE_CALLBACK_ERROR 0x%x at 0x%05x\n", context->id, stop_info.result.spe_callback_error, entry_point);
					break;
				case SPE_ISOLATION_ERROR:
					printf("[%d] SPE_ISOLATION_ERROR 0x%x at 0x%05x\n", context->id, stop_info.result.spe_isolation_error, entry_point);
					break;
				default:
					printf("[%d] unknown stop reason %d at 0x%05x\n", context->id, stop_info.stop_reason, entry_point);
					break;
			}
		}

		char* p = ((char*)context->local_store) + entry_point;
		unsigned int* ip = (unsigned int*) p;
		//printf("[%d] retval %d exited at %x, LS=%x, instruction %08x\n", context->id, retval, entry_point, context->local_store, *ip);
	} while (retval > 0);

/*
	if (ps != MAP_FAILED) {
		munmap(ps, MAP_SIZE);
	}
	if (fd>=0) {
		close(fd);
	}
*/

	pthread_exit(NULL);
}
#endif

// initialise
SPU_HANDLE _init_spu_thread(void* list, int master)
{
	SPU_HANDLE context = (SPU_HANDLE) malloc(sizeof(struct __SPU_HANDLE));
	if (context == NULL)
		return NULL;

	static int id = 0;
	context->id = id++;

	//context->master = master;
	context->list = list;

	spe_callback_handler_register(my_callback, 0x10, SPE_CALLBACK_NEW);
	spe_callback_handler_register(sleep_callback, 0x11, SPE_CALLBACK_NEW);
	spe_callback_handler_register(flush_callback, 0x12, SPE_CALLBACK_NEW);

#ifdef USE_LIBSPE2
	// prioritise the master thread
	pthread_attr_t tattr;
	struct sched_param param;

	int ret = -1;
	if ( master &&
	     0==pthread_attr_init (&tattr) &&
	     0==pthread_attr_getschedparam (&tattr, &param) ) {
		param.sched_priority = 20;
		ret = pthread_attr_setschedparam (&tattr, &param);
	}

	context->spe_ctx = spe_context_create(SPE_EVENTS_ENABLE|SPE_MAP_PS, NULL);
	spe_program_load(context->spe_ctx, master ? main_spu_program : main_render_program);

	int retval = pthread_create(&context->thread, ret ? NULL : &tattr, (void*)&main_program_thread, context);
	if (retval) {
#else
	context->spe_id = spe_create_thread(0, master ? main_spu_program : main_render_program,
							context->list, context->id, -1, 0);
	if (context->spe_id==0) {
#endif
		free(context);
		return NULL;
	}

//	u32 addr;
#ifdef USE_LIBSPE2
	void* ls = spe_ls_area_get(context->spe_ctx);
#else
	void* ls = spe_get_ls(context->spe_id);
#endif

	context->local_store = ls;
	return context;
}

int _exit_spu_thread(SPU_HANDLE context)
{
	if (context == NULL)
		return 0;

	// send the SPU a quit command
#ifdef USE_LIBSPE2
	unsigned int term_cmd = 0;
	spe_in_mbox_write(context->spe_ctx, &term_cmd, 1, SPE_MBOX_ANY_NONBLOCKING);
#else
	spe_write_in_mbox(context->spe_id, 0);
#endif

	// wait for completion and return exit code
	int status = 0;
#ifdef USE_LIBSPE2
	status = pthread_join(context->thread, NULL);
#else
	spe_wait(context->spe_id, &status, 0);
#endif
	free(context);

	return status;
}
/*
u32 _3d_spu_address(SPU_HANDLE context, u32* address)
{
	return (u32)((void*)(address) -
			context->local_store);
}
*/

