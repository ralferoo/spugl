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
#include "queue.h"

// #define DEBUG_1
// #define DEBUG_2		// corrupt each block we load in
// #define DEBUG_3

#define COUNT_BLOCKED_DMA
#define TEXTURE_MAPPED
//#define TRY_TO_CULL_BLOCKS

extern _bitmap_image screen;

u32 textureTemp0[32] __attribute__((aligned(128)));
u32 textureTemp1[32] __attribute__((aligned(128)));
u32 textureTemp2[32] __attribute__((aligned(128)));
u32 textureTemp3[32] __attribute__((aligned(128)));


int qs(int a) { return a>>5; }

unsigned int qu(unsigned int a) { return a>>5; }

float myrecip(float b) { return 1.0/b; }
float mydiv(float a,float b) { return a/b; }

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

typedef struct {
	u32 pixels[32*32];
	vec_uint4 dma1[16];
	vec_uint4 dma2[16];

	vec_uint4* current_dma;
	vec_uint4* new_dma;
	unsigned long current_length;
	unsigned long eah;
	unsigned long tagid;
	unsigned long pad;
} screen_block __attribute__((aligned(16)));

void init_screen_block(screen_block* block, unsigned long tagid)
{
	block->new_dma = &block->dma1[0];
	block->current_dma = &block->dma2[0];
	block->current_length = 0;
	block->tagid = tagid;
//	printf("new block %lx, dma lists at %lx and %lx\n",
//		block, block->current_dma, block->new_dma);
}

void flush_screen_block(screen_block* block)
{
	unsigned long len = block->current_length;

	if (len) {
		unsigned long eah = block->eah;
		unsigned long eal = (unsigned long) ((void*)block->current_dma);
		spu_mfcdma64(&block->pixels[0], eah, eal, len, block->tagid,
			MFC_PUTLF_CMD);
		block->current_length = 0;
#ifdef DEBUG_1
	printf("flush_screen_block: block=%lx eal=%llx len=%d\n",
		block, eal, len);
#endif

	}
}

extern SPU_CONTROL control;

unsigned long wait_for_dma(unsigned long mask)
{
#ifndef COUNT_BLOCKED_DMA
	mfc_write_tag_mask(mask);
	unsigned long mask = mfc_read_tag_status_any();
	return mask;
#else
	unsigned long done;
	do {
		mfc_write_tag_mask(mask);
		done = mfc_read_tag_status_any();
		control.block_count+=9;		// guesstimate how much longer this takes than nop
	} while (!(done&mask));
	return done&mask;
#endif
}
unsigned long wait_screen_block(screen_block* block)
{
	return wait_for_dma(1 << block->tagid);
}


// BUGS: this only works if lines is a multiple of 2, and <=32
void load_screen_block(screen_block* block, 
		unsigned long long ea, unsigned long stride, unsigned int lines)
{
#ifdef DEBUG_1
	printf("load_screen_block: block=%lx screen=%llx stride=%d lines=%d\n",
		block, ea, stride, lines);
#endif

	unsigned long eah = ea >> 32;
	unsigned long eal = ((unsigned long) (ea&0xffffffff));

	build_blit_list(block->new_dma, eal, stride);

	unsigned long old_size = block->current_length;
	unsigned long half_new_size = lines * 4;
	unsigned long new_size = half_new_size * 2;
	unsigned long store_new_size = new_size;

	unsigned long eal_old = (unsigned long) ((void*)block->current_dma);
	unsigned long eal_new = (unsigned long) ((void*)block->new_dma);

	// if this is an unused block, then we have no data to blit out
	// so to avoid branches, split the read block in half
//TODO: why do this fix work???
	unsigned long is_new = 0; //cmp_eq(old_size, 0);
	unsigned long cmd = if_then_else(is_new, MFC_GETL_CMD, MFC_PUTLF_CMD);
	eal_old = if_then_else(is_new, eal_new+half_new_size, eal_old);
	new_size = if_then_else(is_new, half_new_size, new_size);
	old_size = if_then_else(is_new, half_new_size, old_size);
	unsigned long tag = block->tagid;

#ifdef DEBUG_1
	printf("old_size %d, is_new %d store_new %d\n",
		block->current_length, is_new&1, store_new_size);
	printf("DMA[%02X]: ls=%lx eah=%lx list=%lx, size=%d, tag=%d\n",
		cmd, &block->pixels[0],eah,eal_old,old_size,tag);
	printf("DMA[%02X]: ls=%lx eah=%lx list=%lx, size=%d, tag=%d\n",
		MFC_GETL_CMD, &block->pixels[0],eah,eal_new,new_size,tag);
#endif

	spu_mfcdma64(&block->pixels[0],eah,eal_old,old_size,tag, cmd);
	spu_mfcdma64(&block->pixels[0],eah,eal_new,new_size,tag, MFC_GETL_CMD);

	// update the buffer pointers
	block->current_length = store_new_size;
	vec_uint4* t = block->current_dma; 
	block->current_dma = block->new_dma; 
	block->new_dma = t;
	block->eah = eah;
}

//////////////////////////////////////////////////////////////////////////////

static inline vec_float4 extract(
	vec_float4 what, vec_float4 tAa, vec_float4 tAb, vec_float4 tAc)
{
	return	spu_madd(spu_splats(spu_extract(what,0)),tAa,
		spu_madd(spu_splats(spu_extract(what,1)),tAb,
		spu_mul (spu_splats(spu_extract(what,2)),tAc)));
}
	
const vec_uchar16 rgba_argb = {
	3,0,1,2, 7,4,5,6, 11,8,9,10, 15,12,13,14}; 

#ifdef TEXTURE_MAPPED
static inline void sub_block(vec_uint4* ptr, 
	Queue* tri, vec_float4 tAa, vec_float4 tAb, vec_float4 tAc)
{
	vec_uint4 uAa = (vec_uint4) tAa;
	vec_uint4 uAb = (vec_uint4) tAb;
	vec_uint4 uAc = (vec_uint4) tAc;

	vec_uint4 allNeg = spu_and(spu_and(uAa,uAb),uAc);
	vec_uint4 pixel = spu_rlmaska(allNeg,-31);

	vec_uint4 bail = spu_orx(pixel);
	if (!spu_extract(bail,0)) return;

/*
	printf("pixel %03d, tAa=%06.2f, tAb=%06.2f, tAc=%06.2f, tA=%06.2f\n",
		spu_extract(pixel,0),
		spu_extract(tAa,0),
		spu_extract(tAb,0),
		spu_extract(tAc,0),
		spu_extract(tAa,0)+spu_extract(tAb,0)+spu_extract(tAc,0));
*/
	vec_float4 t_w = extract(tri->triangle.w, tAa, tAb, tAc);
	vec_float4 w = spu_splats(1.0f)/t_w;

	tAa = spu_mul(tAa,w);
	tAb = spu_mul(tAb,w);
	tAc = spu_mul(tAc,w);

	vec_float4 t_s = extract(tri->triangle.s, tAa, tAb, tAc);
	vec_float4 t_t = extract(tri->triangle.t, tAa, tAb, tAc);

	vec_uint4 s_sub = spu_and(spu_rlmask(spu_convtu(t_s,32),-14), 0x3fc00);
	vec_uint4 t_sub = spu_and(spu_rlmask(spu_convtu(t_t,32),-22), 0x3fc);
	vec_uint4 offset = spu_or(s_sub,t_sub);

	vec_uint4 sub_block_pixel = spu_and(offset, 0xffc); // 10+2 bits
	vec_uint4 block_id = spu_rlmask(offset,-12);

	vec_uchar16 shuf_cmp_0 = spu_splats((unsigned short)0x203);
	vec_ushort8 copy_cmp_0 = spu_shuffle(block_id,block_id,shuf_cmp_0);
	vec_ushort8 matches1_0 = spu_cmpeq(TEXcache1,copy_cmp_0);
	vec_ushort8 matches2_0 = spu_cmpeq(TEXcache2,copy_cmp_0);
	vec_uint4 gather1_0 = spu_gather((vec_uchar16)matches1_0);
	vec_uint4 gather2_0 = spu_gather((vec_uchar16)matches2_0);

	vec_uchar16 shuf_cmp_1 = spu_splats((unsigned short)0x607);
	vec_ushort8 copy_cmp_1 = spu_shuffle(block_id,block_id,shuf_cmp_1);
	vec_ushort8 matches1_1 = spu_cmpeq(TEXcache1,copy_cmp_1);
	vec_ushort8 matches2_1 = spu_cmpeq(TEXcache2,copy_cmp_1);
	vec_uint4 gather1_1 = spu_gather((vec_uchar16)matches1_1);
	vec_uint4 gather2_1 = spu_gather((vec_uchar16)matches2_1);

	vec_uchar16 shuf_cmp_2 = spu_splats((unsigned short)0xa0b);
	vec_ushort8 copy_cmp_2 = spu_shuffle(block_id,block_id,shuf_cmp_2);
	vec_ushort8 matches1_2 = spu_cmpeq(TEXcache1,copy_cmp_2);
	vec_ushort8 matches2_2 = spu_cmpeq(TEXcache2,copy_cmp_2);
	vec_uint4 gather1_2 = spu_gather((vec_uchar16)matches1_2);
	vec_uint4 gather2_2 = spu_gather((vec_uchar16)matches2_2);

	vec_uchar16 shuf_cmp_3 = spu_splats((unsigned short)0xe0f);
	vec_ushort8 copy_cmp_3 = spu_shuffle(block_id,block_id,shuf_cmp_3);
	vec_ushort8 matches1_3 = spu_cmpeq(TEXcache1,copy_cmp_3);
	vec_ushort8 matches2_3 = spu_cmpeq(TEXcache2,copy_cmp_3);
	vec_uint4 gather1_3 = spu_gather((vec_uchar16)matches1_3);
	vec_uint4 gather2_3 = spu_gather((vec_uchar16)matches2_3);

	vec_uint4 gather_merge=spu_splats((unsigned short)0x5555);

	vec_uint4 gather_0 = spu_sel(gather1_0, gather2_0, gather_merge);
	vec_uint4 gather_2 = spu_sel(gather1_2, gather2_2, gather_merge);
	vec_uint4 gather_1 = spu_sel(gather1_1, gather2_1, gather_merge);
	vec_uint4 gather_3 = spu_sel(gather1_3, gather2_3, gather_merge);

//	vec_uint4 gather_01 = spu_shuffle(gather_0,gather_1,shuf_gath_01);
//	vec_uint4 gather_23 = spu_shuffle(gather_2,gather_3,shuf_gath_23);
//	vec_uint4 gather = spu_shuffle(gather_01,gather_23,shuf_gath_0123);

	unsigned int cache_index_0 = spu_extract(spu_cntlz(gather_0),0);
	unsigned int cache_index_1 = spu_extract(spu_cntlz(gather_1),0);
	unsigned int cache_index_2 = spu_extract(spu_cntlz(gather_2),0);
	unsigned int cache_index_3 = spu_extract(spu_cntlz(gather_3),0);

//	tri->triangle.dummy = cache_index_0 | cache_index_1 | cache_index_2 | cache_index_3;
//	printf("%d %d %d %d\n",
//		cache_index_0, cache_index_1, cache_index_2, cache_index_3); 

	unsigned long texAddrBase = control.texture_hack[tri->triangle.texture_base];

	int tag_id=3;
	u32* pixels = (u32*)ptr;
	unsigned long texAddr0 = texAddrBase + spu_extract(offset,0);
	unsigned long texAddr1 = texAddrBase + spu_extract(offset,1);
	unsigned long texAddr2 = texAddrBase + spu_extract(offset,2);
	unsigned long texAddr3 = texAddrBase + spu_extract(offset,3);
	if (spu_extract(pixel,0))
		mfc_get(textureTemp0, texAddr0 & ~127, 128, tag_id, 0, 0);
	if (spu_extract(pixel,1))
		mfc_get(textureTemp1, texAddr1 & ~127, 128, tag_id, 0, 0);
	if (spu_extract(pixel,2))
		mfc_get(textureTemp2, texAddr2 & ~127, 128, tag_id, 0, 0);
	if (spu_extract(pixel,3))
		mfc_get(textureTemp3, texAddr3 & ~127, 128, tag_id, 0, 0);
	wait_for_dma(1<<tag_id);

	vec_uint4 colour = {
		textureTemp0[(texAddr0&127)>>2],
		textureTemp1[(texAddr1&127)>>2],
		textureTemp2[(texAddr2&127)>>2],
		textureTemp3[(texAddr3&127)>>2]};

	colour = spu_shuffle(colour, colour, rgba_argb);

	vec_uint4 current = *ptr;
	*ptr = spu_sel(current, colour, pixel);
}
#else
static inline void sub_block(vec_uint4* ptr, 
	Queue* tri, vec_float4 tAa, vec_float4 tAb, vec_float4 tAc)
{
	vec_uint4 uAa = (vec_uint4) tAa;
	vec_uint4 uAb = (vec_uint4) tAb;
	vec_uint4 uAc = (vec_uint4) tAc;

	vec_uint4 allNeg = spu_and(spu_and(uAa,uAb),uAc);
	vec_uint4 pixel = spu_rlmaska(allNeg,-31);

	vec_float4 t_w = extract(tri->triangle.w, tAa, tAb, tAc);
	vec_float4 w = spu_splats(1.0f)/t_w;

	tAa = spu_mul(tAa,w);
	tAb = spu_mul(tAb,w);
	tAc = spu_mul(tAc,w);

	vec_float4 t_r = extract(tri->triangle.r, tAa, tAb, tAc);
	vec_float4 t_g = extract(tri->triangle.g, tAa, tAb, tAc);
	vec_float4 t_b = extract(tri->triangle.b, tAa, tAb, tAc);

	vec_uint4 red = spu_and(spu_rlmask(spu_convtu(t_r,32),-8), 0xff0000);
	vec_uint4 green = spu_and(spu_rlmask(spu_convtu(t_g,32),-16), 0xff00);
	vec_uint4 blue = spu_rlmask(spu_convtu(t_b,32),-24);

	vec_uint4 colour = spu_or(spu_or(blue, green),red);

	vec_uint4 current = *ptr;
	*ptr = spu_sel(current, colour, pixel);
}
#endif

void process_block(vec_uint4* block_ptr,
		Queue* tri,
		vec_float4 Aa,vec_float4 Ab,vec_float4 Ac,
		vec_float4 Aa_dx4,vec_float4 Ab_dx4,vec_float4 Ac_dx4,
		vec_float4 Aa_dy,vec_float4 Ab_dy,vec_float4 Ac_dy)
{
	vec_uint4 left = spu_splats(32*8);
	while (spu_extract(left,0)>0) {
		sub_block(block_ptr, tri, Aa,Ab,Ac);

		left -= spu_splats(1);
		block_ptr++;
		vec_uint4 which = spu_and(left,spu_splats((unsigned int)7));
		vec_uint4 sel = spu_cmpeq(which,0);

		vec_float4 Aa_d = spu_sel(Aa_dx4,Aa_dy,sel);
		vec_float4 Ab_d = spu_sel(Ab_dx4,Ab_dy,sel);
		vec_float4 Ac_d = spu_sel(Ac_dx4,Ac_dy,sel);

		Aa += Aa_d; Ab += Ab_d; Ac += Ac_d;
	}
}

const vec_float4 muls = {0.0f, 1.0f, 2.0f, 3.0f};
const vec_float4 muls4 = {4.0f, 4.0f, 4.0f, 4.0f};
const vec_float4 muls32 = {32.0f, 32.0f, 32.0f, 32.0f};
const vec_float4 mulsn28 = {-28.0f, -28.0f, -28.0f, -28.0f};

const vec_float4 muls31x = {0.0f, 31.0f, 0.0f, 31.0f};
const vec_float4 muls31y = {0.0f, 0.0f, 31.0f, 31.0f};

//////////////////////////////////////////////////////////////////////////////

#define NUMBER_TEX_MAPS 24

unsigned int freeTextureMaps = 0;

screen_block buffer __attribute__((aligned(128)));

void _init_buffers()
{
	freeTextureMaps = (1<<NUMBER_TEX_MAPS)-1;
	TEXcache1 = TEXcache2 = spu_splats((unsigned short)-1);
	init_screen_block(&buffer, 31);
}

//////////////////////////////////////////////////////////////////////////////





void block_handler(Queue* queue)
{
//	printf("block handler\n");

	Queue* tri = queue->block.triangle;

	vec_float4 A_dx = tri->triangle.A_dx;
	vec_float4 Aa_dx = spu_splats(spu_extract(A_dx,0));
	vec_float4 Ab_dx = spu_splats(spu_extract(A_dx,1));
	vec_float4 Ac_dx = spu_splats(spu_extract(A_dx,2));

	vec_float4 A_dy = queue->block.A_dy;
	vec_float4 Aa_dy = spu_splats(spu_extract(A_dy,0));
	vec_float4 Ab_dy = spu_splats(spu_extract(A_dy,1));
	vec_float4 Ac_dy = spu_splats(spu_extract(A_dy,2));

	vec_float4 A_dx4 = spu_mul(muls4,A_dx);
	vec_float4 Aa_dx4 = spu_splats(spu_extract(A_dx4,0));
	vec_float4 Ab_dx4 = spu_splats(spu_extract(A_dx4,1));
	vec_float4 Ac_dx4 = spu_splats(spu_extract(A_dx4,2));

	vec_float4 A = queue->block.A;
	vec_float4 Aa = spu_madd(muls,Aa_dx,spu_splats(spu_extract(A,0)));
	vec_float4 Ab = spu_madd(muls,Ab_dx,spu_splats(spu_extract(A,1)));
	vec_float4 Ac = spu_madd(muls,Ac_dx,spu_splats(spu_extract(A,2)));

	////////////////////////////////////

	screen_block* current_block = &buffer;

	unsigned int bx=queue->block.bx, by=queue->block.by;
	u64 scrbuf = screen.address + screen.bytes_per_line*by*32+bx*128;

	load_screen_block(current_block, scrbuf, screen.bytes_per_line, 32);
	wait_screen_block(current_block);

	vec_uint4* block_ptr = (vec_uint4*) ((void*)&current_block->pixels[0]);
	
	////////////////////////////////////

	tri->triangle.functions->process(block_ptr, tri, 
				Aa, Ab, Ac,
				Aa_dx4, Ab_dx4, Ac_dx4,
				Aa_dy, Ab_dy, Ac_dy);

	// this shouldn't be necessary... AND... it sometimes causes duff
	// stuff to be blitted to screen, but i haven't sorted out the block
	// cache stuff yet
	flush_screen_block(&buffer);
	wait_screen_block(&buffer);
	queue->block.triangle->triangle.count--;
}

extern int last_triangle;

void finish_triangle_handler(Queue* tri)
{
	if (tri->triangle.count) {
		QUEUE_JOB(tri, finish_triangle_handler);
		READY_JOB(tri);
		return;
	}
}

void triangle_handler(Queue* tri)
{
	int slots = COUNT_ONES(free_job_queues);
	if (slots==0) {
		QUEUE_JOB(tri, triangle_handler);
		READY_JOB(tri);
		return;
	}

	vec_float4 minmax_ = tri->triangle.minmax;
	vec_int4 minmax = spu_convts(tri->triangle.minmax,0);
	vec_int4 minmax_block = spu_rlmaska(minmax,-5);
	vec_int4 minmax_block_mask = minmax & spu_splats(~31);
	vec_float4 minmax_block_topleft = spu_convtf(minmax_block_mask,0);

	int block_left = spu_extract(minmax_block,0);
	int block_top = spu_extract(minmax_block,1);
	int block_right = spu_extract(minmax_block,2);
	int block_bottom = spu_extract(minmax_block,3);

	vec_float4 pixels_wide = spu_splats(spu_extract(minmax_block_topleft,2)-spu_extract(minmax_block_topleft,0));

////////////////////////////////////////////

	int bx = tri->triangle.cur_x, by = tri->triangle.cur_y;
	int left = tri->triangle.left;
	vec_int4 step = spu_splats(tri->triangle.step);

	vec_float4 A = tri->triangle.A;

	vec_int4 step_start = spu_splats(block_right - block_left);
	if (by<0) {
		bx = block_left;
		by = block_top;
		step = step_start;
		left = (block_bottom+1-block_top)*(block_right+1-block_left);
		A = spu_madd(spu_splats(spu_extract(minmax_block_topleft,0)),tri->triangle.A_dx,
	            spu_madd(spu_splats(spu_extract(minmax_block_topleft,1)),tri->triangle.A_dy,A));
	}

////////////////////////////////////////////

	vec_float4 A_dx = tri->triangle.A_dx;
	vec_float4 A_dy = tri->triangle.A_dy;
	vec_float4 A_dx4 = spu_mul(muls4,A_dx);
	vec_float4 A_dx32 = spu_mul(muls32,A_dx);
	vec_float4 A_dy32 = spu_nmsub(pixels_wide,A_dx,spu_mul(muls32,A_dy));
	vec_float4 blockA_dy = spu_madd(mulsn28,A_dx,A_dy);

#ifdef TRY_TO_CULL_BLOCKS
	vec_float4 Aa_corners = spu_madd(muls31y,Aa_dy,
				spu_madd(muls31x,tri->triangle.A_dx,Aa));
	vec_float4 Ab_corners = spu_madd(muls31y,Ab_dy,
				spu_madd(muls31x,tri->triangle.A_dx,Ab));
	vec_float4 Ac_corners = spu_madd(muls31y,Ac_dy,
				spu_madd(muls31x,tri->triangle.A_dx,Ac));
#endif

	while (left) {
#ifdef TRY_TO_CULL_BLOCKS
		vec_uint4 uAa = (vec_uint4) tAa_corners;
		vec_uint4 uAb = (vec_uint4) tAb_corners;
		vec_uint4 uAc = (vec_uint4) tAc_corners;

		vec_uint4 allNeg = spu_or(spu_or(uAa,uAb),uAc);
		vec_uint4 pixel = spu_rlmaska(allNeg,-31);

		vec_uint4 bail = spu_orx(pixel);
		if (spu_extract(bail,0)) {
#endif
			int free_queue = FIRST_JOB(free_job_queues);
			if (free_queue<0) {
				tri->triangle.cur_x = bx;
				tri->triangle.cur_y = by;
				tri->triangle.step = spu_extract(step,0);
				tri->triangle.left = left;
				tri->triangle.A = A;
				QUEUE_JOB(tri, triangle_handler);
				READY_JOB(tri);
				return;
			}
			unsigned int free_queue_mask = 1<<free_queue;
			Queue* block = &job_queue[free_queue];

			block->block.bx = bx;
			block->block.by = by;
			block->block.triangle = tri;
			block->block.A=A;
			block->block.A_dx=A_dx;
			block->block.A_dy=blockA_dy;
			block->next = -1;
			ENQUEUE_JOB(block, tri->triangle.functions->init);
			READY_JOB(block);
			tri->triangle.count++;
#ifdef TRY_TO_CULL_BLOCKS
		}
#endif
		vec_uint4 step_eq0 = spu_cmpeq(step,spu_splats(0));
		vec_float4 A_d32 = spu_sel(A_dx32,A_dy32,step_eq0);
		A += A_d32;
#ifdef TRY_TO_CULL_BLOCKS
		Aa_corners += Aa_d32;
		Ab_corners += Ab_d32;
		Ac_corners += Ac_d32;
#endif
		bx = if_then_else(spu_extract(step_eq0,0), block_left, bx+1);
		by = if_then_else(spu_extract(step_eq0,0), by+1, by);
		vec_int4 t_step = spu_add(step, spu_splats(-1));
		step = spu_sel(t_step, step_start, step_eq0);
		left--;
	}

	QUEUE_JOB(tri, finish_triangle_handler);
	READY_JOB(tri);
	last_triangle = if_then_else(cmp_eq(last_triangle,tri->id), -1, last_triangle);
}

RenderFuncs _standard_triangle = {
	.init = block_handler,
	.process = process_block
};
