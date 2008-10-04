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
#include "queue.h"

//#define DEBUG_1
//#define DEBUG_2

extern _bitmap_image screen;

//////////////////////////////////////////////////////////////////////////////

void activeBlockInit(ActiveBlock* active)
{
	active->new_dma = &active->dma1[0];
	active->current_dma = &active->dma2[0];
	active->current_length = 0;
	active->eah = 0;
	active->ea_copy = 0;
}

void activeBlockFlush(ActiveBlock* active, int tag)
{
	unsigned long len = active->current_length;
	if (len) {
		unsigned long eah = active->eah;
		unsigned long eal = (unsigned long) ((void*)active->current_dma);
		spu_mfcdma64(&active->pixels[0], eah, eal, len, tag, MFC_PUTLF_CMD);
		active->current_length = 0;
//	printf("flushed...\n");
	}
}

//////////////////////////////////////////////////////////////////////////////

static inline void build_blit_list(
		vec_uint4* dma_list_buffer,
		unsigned long eal, unsigned long stride)
{
#ifdef DEBUG_1
	printf("build_blit_list: eal=%lx stride=%d\n", eal, stride);
#endif

	unsigned long eal1 = eal + stride;
	unsigned long stride2 = 2 * stride;
	unsigned long stride8 = 8 * stride;
	vec_uint4 block0 = { 128, eal, 128, eal1 };
	vec_uint4 step2 = { 0, stride2, 0, stride2};
	vec_uint4 step4 = spu_add(step2, step2);
	vec_uint4 step6 = spu_add(step4, step2);
	vec_uint4 step8 = { 0, stride8, 0, stride8};
	vec_uint4 step16 = spu_add(step8, step8);
	vec_uint4 block2 = spu_add(block0, step2);
	vec_uint4 block4 = spu_add(block0, step4);
	vec_uint4 block6 = spu_add(block0, step6);
	vec_uint4 block8 = spu_add(block0, step8);
	vec_uint4 block10 = spu_add(block8, step2);
	vec_uint4 block12 = spu_add(block8, step4);
	vec_uint4 block14 = spu_add(block8, step6);
	vec_uint4 block16 = spu_add(block8, step8);
	vec_uint4 block18 = spu_add(step16, block2);
	vec_uint4 block20 = spu_add(step16, block4);
	vec_uint4 block22 = spu_add(step16, block6);
	vec_uint4 block24 = spu_add(step16, block8);
	vec_uint4 block26 = spu_add(step16, block10);
	vec_uint4 block28 = spu_add(step16, block12);
	vec_uint4 block30 = spu_add(step16, block14);

	dma_list_buffer[0] = block0;
	dma_list_buffer[1] = block2;
	dma_list_buffer[2] = block4;
	dma_list_buffer[3] = block6;
	dma_list_buffer[4] = block8;
	dma_list_buffer[5] = block10;
	dma_list_buffer[6] = block12;
	dma_list_buffer[7] = block14;

	dma_list_buffer[8] = block16;
	dma_list_buffer[9] = block18;
	dma_list_buffer[10] = block20;
	dma_list_buffer[11] = block22;
	dma_list_buffer[12] = block24;
	dma_list_buffer[13] = block26;
	dma_list_buffer[14] = block28;
	dma_list_buffer[15] = block30;

#ifdef DEBUG_1
	int i,j;
	for (i=0;i<16;i++) {
		printf("%lx(%x): ", &dma_list_buffer[i], i);
		for (j=0; j<4; j++) {
			printf("%lx ", spu_extract(dma_list_buffer[i], j));
		}
		printf("\n");
	}
#endif
}

//////////////////////////////////////////////////////////////////////////////

void blockActivater(Block* block, ActiveBlock* active, int tag)
{
	unsigned int bx=block->bx, by=block->by;
	unsigned long long ea = screen.address + screen.bytes_per_line*by*32+bx*128;

	block->pixels = (vec_uint4*) ((void*)&active->pixels[0]);

	if (active->ea_copy == ea) {
//		printf("re-using same ea %llx in %x -> %x\n", ea, block, active);
		return;
	}
	
	active->ea_copy = ea;

	unsigned long stride = screen.bytes_per_line;
	unsigned int lines = 32;

	/////////////

	unsigned long eah = ea >> 32;
	unsigned long eal = ((unsigned long) (ea&0xffffffff));

	build_blit_list(active->new_dma, eal, stride);

	unsigned long old_size = active->current_length;
	unsigned long half_new_size = lines * 4;
	unsigned long new_size = half_new_size * 2;
	unsigned long store_new_size = new_size;

	unsigned long eal_old = (unsigned long) ((void*)active->current_dma);
	unsigned long eal_new = (unsigned long) ((void*)active->new_dma);

	// if this is an unused block, then we have no data to blit out
	// so to avoid branches, split the read block in half
//TODO: why do this fix work???
	unsigned long is_new = 0; //cmp_eq(old_size, 0);
	unsigned long cmd = if_then_else(is_new, MFC_GETL_CMD, MFC_PUTLF_CMD);
	eal_old = if_then_else(is_new, eal_new+half_new_size, eal_old);
	new_size = if_then_else(is_new, half_new_size, new_size);
	old_size = if_then_else(is_new, half_new_size, old_size);

#ifdef DEBUG_2
	printf("old_size %d, is_new %d store_new %d\n",
		active->current_length, is_new&1, store_new_size);
	printf("DMA[%02X]: ls=%lx eah=%lx list=%lx, size=%d, tag=%d\n",
		cmd, &active->pixels[0],eah,eal_old,old_size,tag);
	printf("DMA[%02X]: ls=%lx eah=%lx list=%lx, size=%d, tag=%d\n",
		MFC_GETLF_CMD, &active->pixels[0],eah,eal_new,new_size,tag);
#endif

	spu_mfcdma64(&active->pixels[0],eah,eal_old,old_size,tag, cmd);
	spu_mfcdma64(&active->pixels[0],eah,eal_new,new_size,tag, MFC_GETLF_CMD);

	// update the buffer pointers
	active->current_length = store_new_size;
	vec_uint4* t = active->current_dma; 
	active->current_dma = active->new_dma; 
	active->new_dma = t;
	active->eah = eah;
}

//////////////////////////////////////////////////////////////////////////////

#ifdef __IGNORE_ALL_JUNK

int qs(int a) { return a>>5; }
unsigned int qu(unsigned int a) { return a>>5; }

float myrecip(float b) { return 1.0/b; }
float mydiv(float a,float b) { return a/b; }

#endif // __IGNORE_ALL_JUNK
