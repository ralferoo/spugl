/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

#include <spu_mfcio.h>
#include "3d.h"

SPU_CONTROL control __CACHE_ALIGNED__;

int init_fifo(int fifo_size) {
	u64 ls = control.my_local_address;
//	printf("init_fifo local address %llx\n", ls);

	if (control.fifo_start)
		free(control.fifo_start);

	control.fifo_start = (u32) (fifo_size ? malloc(fifo_size) : 0UL);
	if (fifo_size>0 && control.fifo_start == 0UL)
		return 1;

//	printf("Allocated FIFO of %d bytes at %lx ctrl %lx\n",
//		fifo_size, control.fifo_start, &control);

	u64 ea = fifo_size > 0 ? ls + ((u64)control.fifo_start) : 0;

//	printf("FIFO ea = %llx\n", ea);

	control.fifo_read = ea;
	control.fifo_written = ea;
	return 0;
}

#include "gen_spu_command_exts.h"

SPU_COMMAND* spu_commands[] = {
#include "gen_spu_command_table.h"
};

/* Process commands on the FIFO */
void process_fifo(u32* from, u32* to) {
	while (from != to) {
		u32* addr = from;

		u32 command = *from++;
		SPU_COMMAND* func = command < sizeof(spu_commands)
					? spu_commands[command] : 0;

		if (func) {
			from = (*func)(from);
			if (!from)
				return;
		} else {
			printf("%06lx: command %lx\n", addr, command);
			from++;
			printf("from %lx to %lx\n", from, to);
		}
	}
	u64 ls = control.my_local_address;
	control.fifo_read = ls+((u64)((u32)from));
}

/* I'm deliberately going to ignore the arguments passed in as early versions
 * of libspe2 have the upper and lower 32 bits swapped.
 */
int main(unsigned long long spe_id, unsigned long long program_data_ea, unsigned long long env) {
//	printf("spumain running on spe %lx\n", spe_id);

	control.fifo_start = 0;
	control.fifo_size = 0;
	control.fifo_written = 0;
	control.fifo_read = 0;

	control.last_count = 0;
	control.idle_count = 0;
	spu_write_out_mbox((u32)&control);

	int running = 1;
	while (running) {
		while (spu_stat_in_mbox() == 0) {
			control.idle_count ++;
			// check to see if there's any data waiting on FIFO
			u64 read = control.fifo_read;
			u64 written = control.fifo_written;
			u64 ls = control.my_local_address;
			if (read!=written) {
				u32* to = (u32*) ((u32)(written-ls));
				u32* from = (u32*) ((u32)(read-ls));
				process_fifo(from, to);
				u64 new_read = control.fifo_read;
//				printf("Processed FIFO from %llx to %llx "
//					"(ends %llx)\n", read,new_read,written);
			}
		}

		unsigned long msg = spu_read_in_mbox();
//		printf("received message %ld\n", msg);
		switch (msg) {
			case SPU_MBOX_3D_TERMINATE:
				running = 0;
				continue;
			case SPU_MBOX_3D_FLUSH:
				spu_write_out_mbox(0);
				break; 
			case SPU_MBOX_3D_INITIALISE_MASTER:
				if (init_fifo(4096)) {
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

//	printf("spumain exiting\n");
}
