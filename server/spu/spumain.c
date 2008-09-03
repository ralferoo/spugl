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

int lock(unsigned long long ea) {
	// based on CBEA page 582
	volatile char buf[256];
	unsigned int eah = (unsigned int)(ea >> 32);
	unsigned int eal = (unsigned int)(ea & ~127);
	volatile char* buf_ptr = (char*) (((unsigned int)buf+127)&~127);
	volatile LOCK* lock_ptr = (volatile LOCK*)(buf_ptr+(ea&127));

	unsigned int status;
	do {
		// attempt to acquire lock
		spu_mfcdma64(buf_ptr, eah, eal, 128, 0, MFC_GETLLAR_CMD);
		spu_readch(MFC_RdAtomicStat);

		// update value
		if (*lock_ptr != LOCK_free)
			return 0;
		*lock_ptr = LOCK_SPU;

		// attempt to update lock
		spu_mfcdma64(buf_ptr, eah, eal, 128, 0, MFC_PUTLLC_CMD);
		status = spu_readch(MFC_RdAtomicStat) & MFC_PUTLLC_STATUS;
	} while (status);
	return 1;
}

void unlock(unsigned long long ea) {
	// based on CBEA page 582
	volatile char buf[256];
	unsigned int eah = (unsigned int)(ea >> 32);
	unsigned int eal = (unsigned int)(ea & ~127);
	volatile char* buf_ptr = (char*) (((unsigned int)buf+127)&~127);
	volatile LOCK* lock_ptr = (volatile LOCK*)(buf_ptr+(ea&127));

	unsigned int status;
	do {
		// attempt to acquire lock
		spu_mfcdma64(buf_ptr, eah, eal, 128, 0, MFC_GETLLAR_CMD);
		spu_readch(MFC_RdAtomicStat);

		// update value
		*lock_ptr = (*lock_ptr == LOCK_PPU_wait) ? LOCK_PPU_may_proceed : LOCK_free;

		// attempt to update lock
		spu_mfcdma64(buf_ptr, eah, eal, 128, 0, MFC_PUTLLC_CMD);
		status = spu_readch(MFC_RdAtomicStat) & MFC_PUTLLC_STATUS;
	} while (status);
	return 1;
}

int main(unsigned long long spe_id, unsigned long long program_data_ea, unsigned long long env) {
	// printf("started SPU with ea %llx\n", program_data_ea);
	
	for(;;) {
		if (lock(program_data_ea)) {
			printf("got lock\n");
			unlock(program_data_ea);
		}
		while (spu_stat_in_mbox()) {
			unsigned int mbox = spu_read_in_mbox();
			// don't bother checkign actual mailbox value, just quit
			return;
		}
		__asm("stop 0x2110\n\t.word 0");
	}
	return 0;
}



