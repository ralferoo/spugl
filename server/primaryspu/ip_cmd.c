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

#include <stdio.h>
#include <spu_mfcio.h>

#include "spucontext.h"
#include "../connection.h"

/*0*/int imp_nop(void* p) {
//	printf("NOP\n");
	return 0;
}

/*1*/int imp_jump(unsigned int *from) {
//	printf("JUMP %x\n", *from);
	return 1;
}

static vec_uint4 poke_value;

/*5*/int impPoke(unsigned int* from)
{
	unsigned int bufid = *from++;
	unsigned int offset = *from++;
	unsigned int value = *from++;

	// reserve some memory
	volatile char buffer_[16+15];
	volatile char* buf_ptr=(volatile char*)( (((int)&buffer_)+15) & ~15 );

	// DMA the buffer start address
	unsigned id = bufid & BLOCK_ID_MASK;
	unsigned int eal_memptr = eal_buffer_memory_table + id*sizeof(long long);
	spu_mfcdma64(buf_ptr, eah_buffer_tables, eal_memptr & ~15, 16, 0, MFC_GET_CMD);	// tag 0
	mfc_write_tag_mask(1<<0);							// tag 0
	mfc_read_tag_status_all();							// wait for read

	// calculate the pointer to the EA and fetch it
	volatile long long* long_ptr = (volatile long long*) (buf_ptr + (eal_memptr&8) );
	long long ea = *long_ptr;
	ea += offset;

	poke_value = spu_splats(value);
	void* buffer = ((void*)&poke_value)+(ea&15);

#ifdef INFO
	printf("Storing %x into ea %llx (bufid=%x, offset=%x) from %x\n", value, ea, id, offset, buffer);
#endif

	spu_mfcdma64(buffer, mfc_ea2h(ea), mfc_ea2l(ea), sizeof(unsigned int), 0, MFC_PUT_CMD);
	// we know this function is effectively single threaded because of the wait on tag 0 above
	// however, this does seem to be required... :(
	mfc_write_tag_mask(1<<0);							// tag 0
	mfc_read_tag_status_all();							// wait for read

	return 0;
}
