/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

#include <spu_mfcio.h>
#include "fifo.h"
#include "struct.h"
#include "primitives.h"

#include <GL/gl.h>

extern SPU_CONTROL control;
_bitmap_image screen = { .address = 0};

#define LOCAL_FRAGMENTS 40

#define FRAGMENT_WIDTH 32
#define FRAGMENT_HEIGHT 32
#define FRAGMENT_SIZE (4*FRAGMENT_WIDTH*FRAGMENT_HEIGHT)

static struct {
	unsigned int wide;
	unsigned int high;
	u64 base_ea;
	u32 buffer_length;
	unsigned int num_fragments;
	void* fragment_local_base = 0;
	unsigned short id[LOCAL_FRAGMENTS] = {0};
} frags;

///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////

/*4*/void* impScreenInfo(u32* from) {
	u64 ea;
	__READ_EA(from)
	screen.address = ea;
	screen.width = *from++;
	screen.height = *from++;
	screen.bytes_per_line = *from++;
//	printf("screen at %llx, width %d, height %d, bpl %d\n",
//		screen.address, screen.width, screen.height, screen.bytes_per_line);

	frags.wide = (screen.width+FRAGMENT_WIDTH-1)/FRAGMENT_WIDTH;
	frags.high = (screen.width+FRAGMENT_HEIGHT-1)/FRAGMENT_HEIGHT;
	frags.base_ea = control.fragment_buffer;
	frags.buffer_length = control.fragment_buflen;
	frags.num_fragments = frags.buffer_length/FRAGMENT_SIZE;

	printf("screen is %dx%d frags, we have %d fragments at 0x%llx\n",
		frags.wide, frags.high, frags.num_fragments, frags.base_ea);

	return from;
}
	
/*5*/void* impClearScreen(u32* from) {
	void* lb = malloc( (((screen.width<<2)+BYTE_ALIGNMENT)&~BYTE_ALIGNMENT) + BYTE_ALIGNMENT);
	u32* p = lb;
	int i;
	for (i=0; i<screen.width; i++)
		p[i] = 0;
	for (i=0; i<screen.height; i++) {
		u64 addr = screen.address + screen.bytes_per_line*i;
		mfc_putf(lb, addr, (((screen.width<<2)+BYTE_ALIGNMENT)&~BYTE_ALIGNMENT), 0, 0, 0);
		mfc_write_tag_mask(1);
		mfc_read_tag_status_any();
	}
	free(lb);
	return from;
}
