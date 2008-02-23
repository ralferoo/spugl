/****************************************************************************
 *
 * SPU 3d rasterisation library
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

#define FRAG_IS_SCREEN	0x8000
#define FRAG_ID_MASK	0x7fff

unsigned char _local_fragment_base[LOCAL_FRAGMENTS * FRAGMENT_SIZE] __attribute__((aligned(128)));

static struct {
	unsigned int wide;
	unsigned int high;
	u64 base_ea;
	u32 buffer_length;
	unsigned int num_fragments;
	unsigned short id[LOCAL_FRAGMENTS];
	void* fragment_local_base;
	unsigned int next_fragment;
} frags = {
	.fragment_local_base = (void*) &_local_fragment_base,
	.next_fragment = 0,
	.id = {0}
};

///////////////////////////////////////////////////////////////////////////////

#define MAX_SCREEN_FRAGMENTS (1920*1280/(FRAGMENT_WIDTH*FRAGMENT_HEIGHT))

u32* getFragment(unsigned int id, unsigned long dma_tag)
{
	unsigned int next_fragment = frags.next_fragment;
	frags.next_fragment = (frags.next_fragment+1)%LOCAL_FRAGMENTS;
	if (frags.id[next_fragment]) {
//		if (
	}
}

///////////////////////////////////////////////////////////////////////////////

/*4*/void* impScreenInfo(u32* from, struct __TRIANGLE * triangle) {
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

//	printf("screen is %dx%d frags, we have %d fragments at 0x%llx, local 0x%lx\n",
//		frags.wide, frags.high, frags.num_fragments, frags.base_ea,
//		frags.fragment_local_base);

	return from;
}
	
/*5*/void* impClearScreen(u32* from, struct __TRIANGLE * triangle) {
	void* lb = malloc( (((screen.width<<2)+BYTE_ALIGNMENT)&~BYTE_ALIGNMENT) + BYTE_ALIGNMENT);
	u32* p = (u32*)((u32)(lb+BYTE_ALIGNMENT-1) & ~BYTE_ALIGNMENT);
	int i;
	for (i=0; i<screen.width; i++)
		p[i] = ((i&255)<<1) | ((i>>7)<<20);
	for (i=0; i<screen.height; i++) {
		u64 addr = screen.address + screen.bytes_per_line*i;
		mfc_putf(lb, addr, (((screen.width<<2)+BYTE_ALIGNMENT)&~BYTE_ALIGNMENT), 0, 0, 0);
		mfc_write_tag_mask(1);
	}
	mfc_read_tag_status_any();
	free(lb);
	return from;
}
