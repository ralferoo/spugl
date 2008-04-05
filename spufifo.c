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

#include "gen_spu_command_exts.h"

SPU_COMMAND* spu_commands[] = {
#include "gen_spu_command_table.h"
};

// extern int fifoTriangleGenerator(Triangle* tri);

static u64 cache_read_base = 0ULL;
static u64 cache_read_upto = 0ULL;

static unsigned char fifo_area[FIFO_SIZE] __attribute__((aligned(128)));

int fifoTriangleGenerator(Triangle* tri)
{
	mfc_write_tag_mask(1<<FIFO_DMA_TAG);
	unsigned int completed = mfc_read_tag_status_all();
	
	// check to see if there's any data waiting on FIFO
	u64 read = control.fifo_read;
	u64 written = control.fifo_written;
	u64 ls = control.my_local_address;
	tri->count = 0;
	while (read!=written && !tri->count) {
//		printf("fifo: buffer=%llx-%llx, read_ptr=%llx, write_ptr=%llx\n",
//			cache_read_base, cache_read_upto, read, written);

		if (cache_read_base<=read && cache_read_upto>=(read+4)) {
			// we have at least one word in our buffer
			void* start_cmd = &fifo_area[read-cache_read_base];
			u32* from = (u32*) start_cmd;
			u32 cmd = *from++;
			int size = cmd>>24;
			u32 command = cmd & (1<<24)-1;
//			printf("fifo cmd: %08lx -> size %d command %d\n", cmd, size, command);

			if (cache_read_upto>=(read+4+4*size)) {
				// entire command has already been read
				SPU_COMMAND* func = command < sizeof(spu_commands)
							? spu_commands[command] : 0;
				if (func) {
					from = (*func)(from, tri);
//					printf("%llx (%06lx): Processed command %d from %lx to %lx\n",
//						read, start_cmd, command, start_cmd, from);
					if (!from) {
						// null return from func means it can't be
						// processed currently or it has managed the
						// fifo_read directly (JMP)
						return 0;
					}
				} else {
					printf("%llx (%06lx): command %d(%lx) not recognised\n",
						read, start_cmd, command, cmd);
				}
				int len = ((void*)from)-start_cmd;
				read += len;
				control.fifo_read = read;

				continue;	// repeat loop
			}
		}

		cache_read_base = read & ~127;

		u32 length = (u32)(written-cache_read_base);
		if (written<read || length>FIFO_SIZE)
			length = FIFO_SIZE;

		cache_read_upto = cache_read_base+length;
		spu_mfcdma64(&fifo_area[0], mfc_ea2h(cache_read_base), mfc_ea2l(cache_read_base), 
				(length+127)&~127, FIFO_DMA_TAG, MFC_GET_CMD);

		return 0;
	}
	return tri->count;
}

extern void blockActivater(Block* block, ActiveBlock* active, int tag);
extern void activeBlockInit(ActiveBlock* active);
extern void activeBlockFlush(ActiveBlock* active, int tag);

int main(unsigned long long spe_id, unsigned long long program_data_ea, unsigned long long env) {
	control.fifo_size = 0;
	control.fifo_written = program_data_ea;
	control.fifo_read = program_data_ea;
	control.error = ERROR_NONE;

//	printf("set values in control %lx to %llx\n", &control, program_data_ea);

	control.blocks_produced_count = 0;
	control.cache_miss_count = 0;

	control.last_count = 0;
	control.idle_count = 0;
	control.block_count = 0;
	spu_write_out_mbox((u32)&control);

	init_queue(activeBlockInit, activeBlockFlush);
	init_texture_cache();

	int running = 1;
	while (running) {
		asm("sync");
		process_queue(&fifoTriangleGenerator, &blockActivater);
	}
}
