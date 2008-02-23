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

#include "spuregs.h" // all SPU files must include spuregs.h
#include <spu_mfcio.h>
#include "fifo.h"
#include "queue.h"
#include <stdio.h>

SPU_CONTROL control __CACHE_ALIGNED__;

void raise_error(int error) {
	if (control.error == ERROR_NONE)
		control.error = error;
}

int init_fifo(int fifo_size) {
	u64 ls = control.my_local_address;

	if (control.fifo_start)
		free(control.fifo_start);

	control.fifo_start = (u32) (fifo_size ? malloc(fifo_size) : 0UL);
	if (fifo_size>0 && control.fifo_start == 0UL)
		return 1;

	u64 ea = fifo_size > 0 ? ls + ((u64)control.fifo_start) : 0;

	control.fifo_size = fifo_size;
	control.fifo_read = ea;
	control.fifo_written = ea;
	control.fifo_host_start = ea;
	control.error = ERROR_NONE;
	return 0;
}

#include "gen_spu_command_exts.h"

SPU_COMMAND* spu_commands[] = {
#include "gen_spu_command_table.h"
};

/* Process commands on the FIFO */
void process_fifo(u32* from, u32* to, Triangle* tri) {
	while (!tri->count && from != to) { // && (COUNT_ONES(free_job_queues)>8)) {
		u32* addr = from;

		u32 command = *from++;
		SPU_COMMAND* func = command < sizeof(spu_commands)
					? spu_commands[command] : 0;

		if (func) {
			from = (*func)(from, tri);
			if (!from)
				return;
		} else {
			printf("%06lx: command %lx\n", addr, command);
			from++;
		}
		u64 ls = control.my_local_address;
		control.fifo_read = ls+((u64)((u32)from));
	}
}


// extern int fifoTriangleGenerator(Triangle* tri);

int fifoTriangleGenerator(Triangle* tri)
{
//	write(1,">",1);
	// check to see if there's any data waiting on FIFO
	u64 read = control.fifo_read;
	u64 written = control.fifo_written;
	u64 ls = control.my_local_address;
	tri->count = 0;
	if (read!=written && !tri->count) {
		u32* to = (u32*) ((u32)(written-ls));
		u32* from = (u32*) ((u32)(read-ls));
		process_fifo(from, to, tri);
		u64 new_read = control.fifo_read;
	}
//	write(1,"<",1);
	return tri->count;
}

extern void blockActivater(Block* block, ActiveBlock* active, int tag);
extern void activeBlockInit(ActiveBlock* active);
extern void activeBlockFlush(ActiveBlock* active, int tag);

/* I'm deliberately going to ignore the arguments passed in as early versions
 * of libspe2 have the upper and lower 32 bits swapped.
 */
int main(unsigned long long spe_id, unsigned long long program_data_ea, unsigned long long env) {
	control.fifo_start = 0;
	control.fifo_size = 0;
	control.fifo_written = 0;
	control.fifo_read = 0;
	control.fifo_host_start = 0;

	control.last_count = 0;
	control.idle_count = 0;
	control.block_count = 0;
	spu_write_out_mbox((u32)&control);

	init_queue(activeBlockInit, activeBlockFlush);
	init_texture_cache();

	int running = 1;
	while (running) {
		while (spu_stat_in_mbox() == 0) {
			process_queue(&fifoTriangleGenerator, &blockActivater);
			control.idle_count += 9;
		}

		unsigned long msg = spu_read_in_mbox();
		switch (msg) {
			case SPU_MBOX_3D_TERMINATE:
				running = 0;
				continue;
			case SPU_MBOX_3D_FLUSH:
				spu_write_out_mbox(0);
				break; 
			case SPU_MBOX_3D_INITIALISE_MASTER:
				if (init_fifo(512)) {
					printf("couldn't allocate FIFO\n");
					spu_write_out_mbox(0);
					running = 0;
				} else {
					spu_write_out_mbox((u32)&control);
				}
				break;
			case SPU_MBOX_3D_INITIALISE_NORMAL:
				if (init_fifo(512)) {
					printf("couldn't allocate FIFO\n");
					spu_write_out_mbox(0);
					running = 0;
				} else {
					spu_write_out_mbox((u32)&control);
				}
				break;
		}
	}

}
