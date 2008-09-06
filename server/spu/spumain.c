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

void process_queue(unsigned long long lock_ea, int id) {
	printf("could process queue %x\n", id);
}

void process_queues(unsigned long long ea) {
	volatile char buf[256];
	unsigned int eah = (unsigned int)(ea >> 32);
	unsigned int eal = (unsigned int)(ea & ~127);
	volatile char* buf_ptr = (char*) (((unsigned int)buf+127)&~127);
	unsigned int status;

	static int next_queue = 0;
retry:
	// attempt to acquire lock
	spu_mfcdma64(buf_ptr, eah, eal, 128, 0, MFC_GETLLAR_CMD);
	spu_readch(MFC_RdAtomicStat);

	// now, read in the list of command queues to process
	for (int i=next_queue; i<next_queue+128; i++) {
		int j = i&127;
		signed char v = buf_ptr[j];
		if (v>0) {
			// attempt to update lock
			buf_ptr[j] = v+1;
			spu_mfcdma64(buf_ptr, eah, eal, 128, 0, MFC_PUTLLC_CMD);
			status = spu_readch(MFC_RdAtomicStat) & MFC_PUTLLC_STATUS;
			if (status)
				goto retry;		// failed to lock the command queue

			// lock succeeded, now process block
			next_queue = j+1;
			process_queue(ea, j);
retry_unlock:
			// attempt to acquire lock
			spu_mfcdma64(buf_ptr, eah, eal, 128, 0, MFC_GETLLAR_CMD);
			spu_readch(MFC_RdAtomicStat);

			// attempt to update lock
			buf_ptr[j]--;
			spu_mfcdma64(buf_ptr, eah, eal, 128, 0, MFC_PUTLLC_CMD);
			status = spu_readch(MFC_RdAtomicStat) & MFC_PUTLLC_STATUS;
			if (status)
				goto retry_unlock;	// failed to unlock the command queue
		}
	};
}

int main(unsigned long long spe_id, unsigned long long program_data_ea, unsigned long long env) {
	printf("started SPU with ea %llx\n", program_data_ea);
	
	int i = 0;
	for(;;) {
		process_queues(program_data_ea);

		// look for termination command
		while (spu_stat_in_mbox()) {
			unsigned int mbox = spu_read_in_mbox();
			// don't bother checkign actual mailbox value, just quit
			return;
		}

		// just call the callback routine (debug)
		__asm("stop 0x2110\n\t.word 0");
	}
	return 0;
}



