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
#include "connection.h"

#ifdef USE_LIBSPE2
	#include <libspe2.h>
	#include <pthread.h>
#else
	#include <libspe.h>
#endif

extern spe_program_handle_t spu_main_handle;
spe_program_handle_t* spu_main_program = &spu_main_handle;

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
};

#ifdef USE_LIBSPE2
/* PPE Callback Function */
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
	usleep(125000);

	// allow other jobs to run, but start up almost immediately
	// sched_yield();
	return 0;
}

void *spu_main_program_thread(SPU_HANDLE context)
{
	int retval;
	unsigned int entry_point = SPE_DEFAULT_ENTRY;
	do {
//		printf("restarting at %x\n", entry_point);
		retval = spe_context_run(context->spe_ctx, &entry_point, 0, context->list, NULL, NULL);
//		printf("exited at %x\n", entry_point);
	} while (retval > 0);
//	printf("retval = %d\n", retval);
	pthread_exit(NULL);
}
#endif

// initialise
SPU_HANDLE _init_spu_thread(void* list, int master)
{
	SPU_HANDLE context = (SPU_HANDLE) malloc(sizeof(struct __SPU_HANDLE));
	if (context == NULL)
		return NULL;

	//context->master = master;
	context->list = list;

	spe_callback_handler_register(my_callback, 0x10, SPE_CALLBACK_NEW);

#ifdef USE_LIBSPE2
	context->spe_ctx = spe_context_create(SPE_EVENTS_ENABLE|SPE_MAP_PS, NULL);
	spe_program_load(context->spe_ctx, spu_main_program);

	int retval = pthread_create(&context->thread, NULL, (void*)&spu_main_program_thread, context);
	if (retval) {
#else
	context->spe_id = spe_create_thread(0, spu_main_program, context->list, NULL, -1, 0);
	if (context->spe_id==0) {
#endif
		free(context);
		return NULL;
	}

//	u32 addr;
#ifdef USE_LIBSPE2
	void* ls = spe_ls_area_get(context->spe_ctx);

//	while(!spe_out_mbox_status(context->spe_ctx)) {
//		sched_yield();
//		asm("sync");
//	}
//	spe_out_mbox_read(context->spe_ctx, &addr, 1);
#else
	void* ls = spe_get_ls(context->spe_id);

//	while(!spe_stat_out_mbox(context->spe_id)) {
//		sched_yield();
//		asm("sync");
//	}
//	addr = spe_read_out_mbox(context->spe_id);
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

