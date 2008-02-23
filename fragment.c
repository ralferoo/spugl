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
#include "fifo.h"
#include "struct.h"
#include "primitives.h"

//#include <stdlib.h>
#include <GL/gl.h>

extern SPU_CONTROL control;
_bitmap_image screen = { .address = 0};

static struct {
	unsigned int wide;
	unsigned int high;
	u64 base_ea;
	u32 buffer_length;
	unsigned int num_fragments;
} frags = {
};

///////////////////////////////////////////////////////////////////////////////

/*4*/void* impScreenInfo(u32* from, struct __TRIANGLE * triangle) {
	u64 ea;
	__READ_EA(from)
	screen.address = ea;
	screen.width = *from++;
	screen.height = *from++;
	screen.bytes_per_line = *from++;

	frags.wide = (screen.width+FRAGMENT_WIDTH-1)/FRAGMENT_WIDTH;
	frags.high = (screen.width+FRAGMENT_HEIGHT-1)/FRAGMENT_HEIGHT;
	frags.base_ea = control.fragment_buffer;
	frags.buffer_length = control.fragment_buflen;
	frags.num_fragments = frags.buffer_length/FRAGMENT_SIZE;

	return from;
}
	
/*5*/void* impClearScreen(u32* from, struct __TRIANGLE * triangle) {
	void* lb = malloc( (((screen.width<<2)+BYTE_ALIGNMENT)&~BYTE_ALIGNMENT) + BYTE_ALIGNMENT);
	u32* p = (u32*)((u32)(lb+BYTE_ALIGNMENT-1) & ~BYTE_ALIGNMENT);
	int i;
	static int r=0;
	r+=0x10000;
	for (i=0; i<screen.width; i++)
		p[i] = ((i&255)<<1) | ((i>>7)<<20); // + r;
	for (i=0; i<screen.height; i++) {
		u64 addr = screen.address + screen.bytes_per_line*i;
		mfc_putf(lb, addr, (((screen.width<<2)+BYTE_ALIGNMENT)&~BYTE_ALIGNMENT), 0, 0, 0);
	}
	mfc_write_tag_mask(1);
	mfc_read_tag_status_all();
	free(lb);
	return from;
}
