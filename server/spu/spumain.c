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
//#include "fifo.h"
//#include "queue.h"
#include <stdio.h>

int main(unsigned long long spe_id, unsigned long long program_data_ea, unsigned long long env) {
	// printf("started SPU with ea %llx\n", program_data_ea);
	
	for(;;) {
		while (spu_stat_in_mbox()) {
			unsigned int mbox = spu_read_in_mbox();
			// don't bother checkign actual mailbox value, just quit
			return;
		}
		__asm("stop 0x2110\n\t.word 0");
	}
	return 0;
}



