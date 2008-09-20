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

#include <spu_mfcio.h>
#include <spu_intrinsics.h>
#include <stdio.h>

#include "render.h"

unsigned int _SPUID;

unsigned long eah_render_tasks;
unsigned long eal_render_tasks;

int main(unsigned long long spe_id, unsigned long long program_data_ea, unsigned long long env) {
	_SPUID = (unsigned int) env;

	eah_render_tasks = (unsigned int)(program_data_ea >> 32);
	eal_render_tasks = (unsigned int)(program_data_ea & 0xffffffff);
	printf("[%d] Render SPU thread started, render tasks at %x:%08x...\n", _SPUID, eah_render_tasks, eal_render_tasks);

	int i = 0;
	for(;;) {
		process_render_tasks(eah_render_tasks, eal_render_tasks);

		// look for termination command
		while (spu_stat_in_mbox()) {
			unsigned int mbox = spu_read_in_mbox();
			// don't bother checking actual mailbox value, just quit
			return 0;
		}

//		write(1,"_",1);
		// just call the callback routine (debug)
		__asm("stop 0x2110\n\t.word 0");
	}
	return 0;
}



