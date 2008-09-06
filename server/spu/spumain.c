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

void process_queue(unsigned int id, volatile char* buf_ptr) {
	unsigned int eal_memptr = eal_buffer_memory_table + id*sizeof(long long);

	// discover the actual ea for the buffer
	spu_mfcdma64(buf_ptr, eah_buffer_tables, eal_memptr & ~15, 16, 0, MFC_GET_CMD);	// tag 0
	mfc_write_tag_mask(1<<0);							// tag 0
	mfc_read_tag_status_all();							// wait for read

	volatile long* long_ptr = (volatile long*) (buf_ptr + (eal_memptr&8) );
	unsigned int eah = long_ptr[0];
	unsigned int eal = long_ptr[1];

	// now, read/lock the command queue initial data structure
	spu_mfcdma64(buf_ptr, eah, eal, 128, 0, MFC_GETLLAR_CMD);
	spu_readch(MFC_RdAtomicStat);
	volatile CommandQueue* queue = (volatile CommandQueue*) buf_ptr;

	// now, queue holds the queue data structure :)
	
	unsigned int wptr = queue->write_ptr;
	unsigned int rptr = queue->read_ptr;

	while (wptr != rptr) {
		printf("could process queue %x at %x:%x, buffer %x:%x, write %x, read %x\n", 
			id, eah_buffer_tables, eal_memptr, eah, eal, queue->write_ptr, queue->read_ptr);

		// process here
		rptr = wptr;
retry_update:
		queue->read_ptr = rptr;

		// now, flush the data back to the buffer
		spu_mfcdma64(buf_ptr, eah, eal, 128, 0, MFC_PUTLLC_CMD);
		unsigned int status = spu_readch(MFC_RdAtomicStat) & MFC_PUTLLC_STATUS;
		if (status) {
			// data is dirty, reload it and attempt the write again
			spu_mfcdma64(buf_ptr, eah, eal, 128, 0, MFC_GETLLAR_CMD);
			spu_readch(MFC_RdAtomicStat);
			goto retry_update;
		}

		// check to see if any new data arrived
		wptr = queue->write_ptr;
	}

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
			next_queue = current+1;
			process_queue(current, buf_ptr);
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

		// just call the callback routine (debug)
		__asm("stop 0x2110\n\t.word 0");
	}
	return 0;
}



