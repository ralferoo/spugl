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

// #define DEBUG

#define FIFO_SIZE 1024

//#include "spuregs.h" // all SPU files must include spuregs.h
#include <spu_mfcio.h>
#include <spu_intrinsics.h>
#include <stdio.h>

#include "../connection.h"
#include "../../client/queue.h"

////////////////////////////////////////////////////////////////////////////////
//
// These point to the lock tables and buffer pointers
unsigned int eah_buffer_tables;
unsigned int eal_buffer_lock_table;
unsigned int eal_buffer_memory_table;

////////////////////////////////////////////////////////////////////////////////

static unsigned char fifo_area[FIFO_SIZE] __attribute__((aligned(128)));
static unsigned int fifo_eah = 0;
static unsigned int fifo_eal = 0;
static unsigned int fifo_ofs = 0;
static unsigned int fifo_len = 0;

////////////////////////////////////////////////////////////////////////////////

typedef int FIFO_COMMAND(unsigned int *data, unsigned int queue_id);

#include "../../client/gen_command_defs.h"	// numeric definitions
#include "gen_command_exts.h"			// extern definitions

FIFO_COMMAND* fifo_commands[] = {
	#include "gen_command_table.h"		// table entries
};


////////////////////////////////////////////////////////////////////////////////

void process_queue(unsigned int id, volatile char* buf_ptr) {
	unsigned int eal_memptr = eal_buffer_memory_table + id*sizeof(long long);

	// discover the actual ea for the buffer
	spu_mfcdma64(buf_ptr, eah_buffer_tables, eal_memptr & ~15, 16, 0, MFC_GET_CMD);	// tag 0
	mfc_write_tag_mask(1<<0);							// tag 0
	mfc_read_tag_status_all();							// wait for read

	volatile long long* long_ptr = (volatile long long*) (buf_ptr + (eal_memptr&8) );
	long long ea = *long_ptr;
	unsigned int eah = ea>>32;
	unsigned int eal = ea&0xffffffff;

	// now, read/lock the command queue initial data structure
	spu_mfcdma64(buf_ptr, eah, eal, 128, 0, MFC_GETLLAR_CMD);
	spu_readch(MFC_RdAtomicStat);
	volatile CommandQueue* queue = (volatile CommandQueue*) buf_ptr;

	// now, queue holds the queue data structure :)
	
	unsigned int wptr = queue->write_ptr;
	unsigned int rptr = queue->read_ptr;

	while (wptr != rptr) {
//		printf("could process queue %x at %x:%x, buffer %x:%x, write %x, read %x\n",
//			id, eah_buffer_tables, eal_memptr, eah, eal, queue->write_ptr, queue->read_ptr);

		// process here
		if (eah == fifo_eah && eal == fifo_eal) {
retry_loop:		;
			unsigned int upto = fifo_ofs + fifo_len;
			if (fifo_ofs <= rptr && upto >= (rptr+4) ) {
				// we have at least one word in the buffer, can read the command
				void *start_cmd = &fifo_area[rptr-fifo_ofs];
				unsigned int *cmd_buf = (unsigned int*) start_cmd;
				unsigned int cmd = *cmd_buf++;
				int size = cmd>>24;
				unsigned int command = cmd & ((1<<24)-1);
				if (upto >= (rptr+4 + 4*size)) {
					// process function from cmd_buf
					FIFO_COMMAND *func = command < sizeof(fifo_commands) ? fifo_commands[command] : 0;
					if (func) {
#ifdef DEBUG
						printf("[%02x:%08x] command %x, data length %d\n",
								id, rptr, command, size);
#endif
						if ( (*func)(cmd_buf, id) ) {
							if (command != FIFO_COMMAND_JUMP) {
								// cannot process at the moment, try another queue
								return;
							}
							// otherwise we're processing a JUMP, update pointer
							rptr = *cmd_buf;

							// invalidate FIFO
							fifo_eah = fifo_eal = 0;

							// write the JUMP target address back
retry_jump:
							queue->read_ptr = rptr;
							spu_mfcdma64(buf_ptr, eah, eal, 128, 0, MFC_PUTLLC_CMD);
							unsigned int status = spu_readch(MFC_RdAtomicStat);
							if (status & MFC_PUTLLC_STATUS) {
								// data is dirty, reload it and attempt the write again
								spu_mfcdma64(buf_ptr, eah, eal, 128, 0, MFC_GETLLAR_CMD);
								spu_readch(MFC_RdAtomicStat);
								goto retry_jump;
							}
							return;
						}
					} else {
#ifdef DEBUG
						printf("[%02x:%08x] INVALID COMMAND %x, data length %d\n",
								id, rptr, command, size);
#endif
					}

					// update read ptr
					rptr += 4 + 4*size;
retry_update:
					// attempt to store the current read pointer
					queue->read_ptr = rptr;
					spu_mfcdma64(buf_ptr, eah, eal, 128, 0, MFC_PUTLLC_CMD);
					unsigned int status = spu_readch(MFC_RdAtomicStat) & MFC_PUTLLC_STATUS;
					if (status) {
						// data is dirty, reload it and attempt the write again
						spu_mfcdma64(buf_ptr, eah, eal, 128, 0, MFC_GETLLAR_CMD);
						spu_readch(MFC_RdAtomicStat);
						goto retry_update;
					}

					// check to see if any new data arrived and attempt to process it
					wptr = queue->write_ptr;
					continue;		// drop back to while loop
				} // fifo contains command word, but not data
			} // fifo doesn't contain single command word

		} // fifo_eah, fifo_eal


		// cache isn't sufficient, re-read cache
		fifo_ofs = rptr & ~127;
		fifo_len = wptr - fifo_ofs;
		if (wptr < rptr || fifo_len>FIFO_SIZE)		// if wptr<rptr we have no idea how big buffer is
			fifo_len = FIFO_SIZE;			// if fifo_len>FIFO_SIZE, cap at FIFO_SIZE

		long long fifo_ea = ea + fifo_ofs;
#ifdef DEBUG
		printf("DMA %llx len %d\n", fifo_ea, fifo_len);
#endif
		spu_mfcdma64(&fifo_area[0], mfc_ea2h(fifo_ea), mfc_ea2l(fifo_ea),
				(fifo_len+127) & ~127, 0, MFC_GET_CMD);		// tag 0
		fifo_eah = eah;
		fifo_eal = eal;
		mfc_write_tag_mask(1<<0);					// tag 0
		mfc_read_tag_status_all();					// wait for read

		// rejoin while loop and reprocess now we have the data
		goto retry_loop;					// could just fall through, but skip the if
	} // while

}

void process_queues() {
	volatile char buf[256];
	unsigned int eah = eah_buffer_tables;
	unsigned int eal = eal_buffer_lock_table;
	volatile char* buf_ptr = (char*) (((unsigned int)buf+127)&~127);
	unsigned int status;

	static int next_queue = 0;
	int current = next_queue;
	int stopat = next_queue;
retry:
	// attempt to acquire lock
	spu_mfcdma64(buf_ptr, eah, eal, 128, 0, MFC_GETLLAR_CMD);
	spu_readch(MFC_RdAtomicStat);

	// now, read in the list of command queues to process
	do {
		if ( ((signed char)buf_ptr[current]) > 0) {
			// attempt to update lock
			buf_ptr[current]++;
			spu_mfcdma64(buf_ptr, eah, eal, 128, 0, MFC_PUTLLC_CMD);
			status = spu_readch(MFC_RdAtomicStat) & MFC_PUTLLC_STATUS;
			if (status)
				goto retry;		// failed to lock the command queue

			// lock succeeded, now process block
			process_queue(current, buf_ptr);
			next_queue = (current+1) & (MAX_COMMAND_BUFFERS-1);
retry_unlock:
			// attempt to acquire lock
			spu_mfcdma64(buf_ptr, eah, eal, 128, 0, MFC_GETLLAR_CMD);
			spu_readch(MFC_RdAtomicStat);

			// attempt to update lock
			buf_ptr[current]--;
			spu_mfcdma64(buf_ptr, eah, eal, 128, 0, MFC_PUTLLC_CMD);
			status = spu_readch(MFC_RdAtomicStat) & MFC_PUTLLC_STATUS;
			if (status)
				goto retry_unlock;	// failed to unlock the command queue
		}
		current = (current+1) & (MAX_COMMAND_BUFFERS-1);
	} while (current != stopat);
}

int main(unsigned long long spe_id, unsigned long long program_data_ea, unsigned long long env) {
	eah_buffer_tables = (unsigned int)(program_data_ea >> 32);
	eal_buffer_lock_table = (unsigned int)(program_data_ea & ~127);
	eal_buffer_memory_table = eal_buffer_lock_table + MAX_DATA_BUFFERS;

	int i = 0;
	for(;;) {
		// process all the client FIFOs
		process_queues(program_data_ea);

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



