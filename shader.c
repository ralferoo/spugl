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

// #define DEBUG_1
// #define DEBUG_2
// #define DEBUG_3

extern _bitmap_image screen;

u32 textureTemp[32] __attribute__((aligned(128)));


vec_int4 _a[10];

int qs(int a) { return a>>5; }

unsigned int qu(unsigned int a) { return a>>5; }

float myrecip(float b) { return 1.0/b; }
float mydiv(float a,float b) { return a/b; }

//////////////////////////////////////////////////////////////////////////////

static inline vec_float4 div(vec_float4 a, vec_float4 b) {
	qword ra = (qword) a;
	qword rb = (qword) b;
	qword t0 = si_frest(rb);
	qword t1 = si_fi(rb,t0);
	qword t2 = si_fm(ra,t1);
	qword t3 = si_fnms(t2,rb,ra);
	qword res = si_fma(t3,t1,t2);
	return (vec_float4) res;
}

static inline unsigned long cmp_eq(unsigned long a, unsigned long b) {
	vec_uint4 aa = (vec_uint4) a;
	vec_uint4 bb = (vec_uint4) b;
	vec_uint4 rr = spu_cmpeq(aa, bb);
	return spu_extract(rr, 0);
}

static inline unsigned long if_then_else(unsigned long c, 
		unsigned long a, unsigned long b) {
	vec_uint4 aa = (vec_uint4) a;
	vec_uint4 bb = (vec_uint4) b;
	vec_uint4 cc = (vec_uint4) c;
	vec_uint4 rr = spu_sel(bb, aa, cc);
	return spu_extract(rr, 0);
}
	
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
			MFC_PUTLB_CMD);
		block->current_length = 0;
#ifdef DEBUG_1
	printf("flush_screen_block: block=%lx eal=%llx len=%d\n",
		block, eal, len);
#endif

	}
}

unsigned long wait_screen_block(screen_block* block)
{
	mfc_write_tag_mask(1 << block->tagid);
	unsigned long mask = mfc_read_tag_status_any();
	return mask;
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
	unsigned long is_new = cmp_eq(old_size, 0);
	unsigned long cmd = if_then_else(is_new, MFC_GETL_CMD, MFC_PUTLB_CMD);
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

/*
	vec_uint4 v_os = (vec_uint4) old_size;
	vec_uint4 v_ns = (vec_uint4) new_size;

	vec_uint4 v_eal_old = (vec_uint4) ((void*)block->current_dma);
	vec_uint4 v_eal_new = (vec_uint4) ((void*)block->new_dma);

	vec_uint4 v_ns_2 = (vec_uint4) new_size >> 1;
	vec_uint4 v_eal_half_b = spu_add(v_eal_new, v_ns_2);
	vec_uint4 v_os_z = spu_cmpeq(v_os, 0);		// ones if ==0
	v_ns = spu_sel(v_ns, v_ns_2, v_os_z);		// half if == 0
	v_os = spu_sel(v_os, v_ns_2, v_os_z);		// half ns if==0 else os
	v_eal_old = spu_sel(v_eal_old, v_eal_half_b, v_os_z)	// skip
	vec_uint4 v_cmd = spu_sel(MFC_PUTLB_CMD, MFC_GETL_CMD, v_os_z);
*/


//////////////////////////////////////////////////////////////////////////////

static inline void sub_block(vec_uint4* ptr, 
	vec_float4 lx, vec_float4 rx, vec_float4 vx_base_28)
{
	vec_uint4 left_28 = spu_cmpgt(vx_base_28, lx);
	vec_uint4 right_28 = spu_cmpgt(rx, vx_base_28);
	//vec_uint4 pixel = spu_xor(left_28, right_28);
	vec_uint4 pixel = spu_nand(right_28, left_28);

	vec_uint4 current = *ptr;
	vec_uint4 colour = spu_splats(0xffff00);
	*ptr = spu_sel(colour, current, pixel);

#ifdef DEBUG_2
	vec_uint4 test = spu_or(spu_or(spu_and(left_28,(1)),
				       spu_and(right_28,(2))),
			 	spu_and(pixel,(4)));
	printf("%lx %c%c%c%c ", ptr,
		'0'+spu_extract(test,0),
		'0'+spu_extract(test,1),
		'0'+spu_extract(test,2),
		'0'+spu_extract(test,3));
//		spu_extract(pixel,0)?'.':'*',
//		spu_extract(pixel,1)?'.':'*',
//		spu_extract(pixel,2)?'.':'*',
//		spu_extract(pixel,3)?'.':'*');
#endif
}

static void big_block(unsigned int bx, unsigned int by,
		screen_block* current_block,
		unsigned int start_line, unsigned int end_line,
		vec_float4 lx, vec_float4 rx, 
		vec_float4 dl, vec_float4 dr, 
		vec_float4 vx_base_0, vec_float4 vx_base_4,
		vec_float4 vx_base_8, vec_float4 vx_base_12,
		vec_float4 vx_base_16, vec_float4 vx_base_20,
		vec_float4 vx_base_24, vec_float4 vx_base_28)
{
	u64 scrbuf = screen.address + screen.bytes_per_line*by*32+bx*128;

	load_screen_block(current_block, scrbuf, screen.bytes_per_line, 32);
	wait_screen_block(current_block);

	vec_uint4* block_ptr = (vec_uint4*) ((void*)&current_block->pixels[0]);

	int a;
	unsigned long* q = (unsigned long*) block_ptr;
	for (a=0; a<32*8*4; a++) {
		//block_ptr[a]|=spu_splats(0xff0000);
		q[a] = (q[a]&0xffff)>>4 | 0xff0000;
	}
/*
*/

	block_ptr += 8*start_line;

	unsigned int line;
//	for (line=0; line<32; line++) {
	for (line=start_line; line<end_line; line++) {
#ifdef DEBUG_3
		printf("block %d,%d line %d, lx %f rx %f (%f %f)\n",
			bx, by, line, spu_extract(lx,0), spu_extract(rx,0),
			spu_extract(vx_base_0,0), spu_extract(vx_base_28,3));
#endif
		sub_block(&block_ptr[0], lx, rx, vx_base_0);
		sub_block(&block_ptr[1], lx, rx, vx_base_4);
		sub_block(&block_ptr[2], lx, rx, vx_base_8);
		sub_block(&block_ptr[3], lx, rx, vx_base_12);
		sub_block(&block_ptr[4], lx, rx, vx_base_16);
		sub_block(&block_ptr[5], lx, rx, vx_base_20);
		sub_block(&block_ptr[6], lx, rx, vx_base_24);
		sub_block(&block_ptr[7], lx, rx, vx_base_28);

		lx = spu_add(lx, dl);
		rx = spu_add(rx, dr);
		block_ptr += 8;
#ifdef DEBUG_2
		printf("\n");
#endif
	}
	wait_screen_block(current_block);
	flush_screen_block(current_block);
}

vec_uchar16 copy_from_a = {
	SEL_A0 SEL_A1 SEL_A2 SEL_A3
};
//vec_uchar16 copy_from_b = {
//	SEL_B0 SEL_B1 SEL_B2 SEL_B3
//};

vec_float4 _base_add4 = {4.0, 4.0, 4.0, 4.0};
vec_float4 _base_add8 = {8.0, 8.0, 8.0, 8.0};
vec_float4 _base_add16 = {16.0, 16.0, 16.0, 16.0};
vec_float4 _base_add32 = {32.0, 32.0, 32.0, 32.0};

void triangle_half(
	unsigned int start_line,
	unsigned int end_line,
	unsigned int bx,
	unsigned int by,

	unsigned int end_y,

	vec_float4 lx, vec_float4 rx, vec_float4 dl, vec_float4 dr,
	screen_block* current_block,

	vec_float4 vx_base_0,
	vec_float4 vx_base_4,
	vec_float4 vx_base_8,
	vec_float4 vx_base_12,
	vec_float4 vx_base_16,
	vec_float4 vx_base_20,
	vec_float4 vx_base_24,
	vec_float4 vx_base_28
) {

	vec_uchar16 dl_inc = (vec_uchar16) spu_cmpgt(dl, spu_splats(0.0f));
	vec_uchar16 dr_inc = (vec_uchar16) spu_cmpgt(dr, spu_splats(0.0f));
	vec_uchar16 plus16 = spu_splats((unsigned char)0x10);

	vec_uchar16 dl_sel = spu_or(copy_from_a, spu_and(dl_inc, plus16));
	vec_uchar16 dr_sel = spu_or(copy_from_a, spu_and(dr_inc, plus16));

	while (by < end_y)
	{
		// this should probably all be done with if_then_else as
		// we only care about one element of the register
		vec_float4 mult = spu_splats((float)(31-start_line));
		vec_float4 lx_bot = spu_add(lx, spu_mul(dl,mult));
		vec_float4 rx_bot = spu_add(rx, spu_mul(dr,mult));

		vec_float4 lx_min = spu_shuffle(lx_bot, lx, dl_sel);
		vec_float4 rx_max = spu_shuffle(rx, rx_bot, dr_sel);

		printf("lx=%f,lx_bot=%f,lx_min=%f, dl_sel=%x\n",
			spu_extract(lx,0),		
			spu_extract(lx_bot,0),
			spu_extract(lx_min,0), 
			spu_extract(dl_sel,3));

		printf("rx=%f,rx_bot=%f,rx_max=%f, dr_sel=%x\n\n",
			spu_extract(rx,0),		
			spu_extract(rx_bot,0),
			spu_extract(rx_max,0), 
			spu_extract(dr_sel,3));
		
		vec_int4 lx_int = spu_convts(lx_min,0);
		vec_int4 rx_int = spu_add(spu_splats(31),spu_convts(rx_max,0));

		vec_int4 left_block_v = spu_rlmaska(lx_int,-5);
		vec_int4 right_block_v = spu_rlmaska(rx_int,-5);

		unsigned int left_block = spu_extract(left_block_v, 0);
		unsigned int right_block = spu_extract(right_block_v, 0);

		printf("For line %d, blocks are %d..%d\n", by,
			left_block, right_block);

		int cur_block;
		vec_float4 block_x_delta = {0.0, 0.0, 0.0, 0.0};

		for (cur_block = bx; cur_block>=left_block; cur_block--) {
			big_block(
				cur_block, by,
				current_block,
				start_line, end_line,
				lx, rx, dl, dr,
				spu_add(block_x_delta, vx_base_0),
				spu_add(block_x_delta, vx_base_4),
				spu_add(block_x_delta, vx_base_8),
				spu_add(block_x_delta, vx_base_12),
				spu_add(block_x_delta, vx_base_16),
				spu_add(block_x_delta, vx_base_20),
				spu_add(block_x_delta, vx_base_24),
				spu_add(block_x_delta, vx_base_28));
			block_x_delta = spu_sub(block_x_delta, _base_add32);
		}

		block_x_delta = _base_add32;
		for (cur_block = bx+1; cur_block<=right_block; cur_block++) {
			big_block(
				cur_block, by,
				current_block,
				start_line, end_line,
				lx, rx, dl, dr,
				spu_add(block_x_delta, vx_base_0),
				spu_add(block_x_delta, vx_base_4),
				spu_add(block_x_delta, vx_base_8),
				spu_add(block_x_delta, vx_base_12),
				spu_add(block_x_delta, vx_base_16),
				spu_add(block_x_delta, vx_base_20),
				spu_add(block_x_delta, vx_base_24),
				spu_add(block_x_delta, vx_base_28));
			block_x_delta = spu_add(block_x_delta, _base_add32);
		}

		by++;
		lx = spu_add(lx_bot, dl);
		rx = spu_add(rx_bot, dr);
		start_line = 0;
	}
}


void fast_triangle(triangle* tri, screen_block* current_block)
{
	vec_float4 vx = spu_shuffle(tri->x, tri->x, tri->shuffle);
	vec_float4 vy = spu_shuffle(tri->y, tri->y, tri->shuffle);

	printf("fast_triangle: A %f,%f B %f,%f C %f,%f\n",
		spu_extract(vx,0),spu_extract(vy,0),
		spu_extract(vx,1),spu_extract(vy,1),
		spu_extract(vx,2),spu_extract(vy,2));

	// these are ((int(coord)+0.5)*2)
	vec_int4 vx_int = spu_convts(vx,0);
	vec_int4 vy_int = spu_convts(vy,0);
	vec_int4 _1 = {1,1,1,1};
	vec_int4 vx_x2 = spu_sl(vx_int,1);
	vec_int4 vx_1 = spu_or(vx_x2,_1);
	vec_int4 vy_x2 = spu_sl(vy_int,1);
	vec_int4 vy_1 = spu_or(vy_x2,_1);

	// so, now these are the mid-pixel points
	vec_float4 vx_mid = spu_convtf(vx_1,1);
	vec_float4 vy_mid = spu_convtf(vy_1,1);

	vec_int4 vx_block = spu_rlmaska(vx_int,-5);
	vec_int4 vy_block = spu_rlmaska(vy_int,-5);

#ifdef DEBUG_3
	printf("blocks: top %d,%d mid %d,%d bottom %d,%d\n",
		spu_extract(vx_block,0),spu_extract(vy_block,0),
		spu_extract(vx_block,2),spu_extract(vy_block,2),
		spu_extract(vx_block,1),spu_extract(vy_block,1));
#endif

	vec_int4 _bases = {0,2,4,6};
	vec_int4 vx_base_i = spu_or(spu_splats(spu_extract(vx_1,0)&~62),_bases);
	vec_int4 vy_base_i = spu_splats(spu_extract(vy_1,0)&~62);


	vec_float4 dy=spu_sub(vy,spu_splats(spu_extract(vy,0)));
	vec_float4 dx=spu_sub(vx,spu_splats(spu_extract(vx,0)));
	vec_float4 grad=div(dx,dy);

#ifdef DEBUG_3
	printf("da %f,%f->%f db %f,%f->%f dc %f,%f->%f\n",
		spu_extract(dx,0),spu_extract(dy,0),spu_extract(grad,0),
		spu_extract(dx,2),spu_extract(dy,2),spu_extract(grad,2),
		spu_extract(dx,1),spu_extract(dy,1),spu_extract(grad,1));
#endif

int qx;
int qy;

/*
for (qx=1;qx<1600; qx+=128) {
	for (qy=1;qy<500; qy+=128) {

	u64 scrbuf = screen.address + screen.bytes_per_line*(qy>>1)+(qx>>1);
	vx_base_i = spu_or(spu_splats(qx),_bases);
	vy_base_i = spu_splats(qy);
*/

/////

	vec_float4 vy_base_0 = spu_convtf(vy_base_i,1);
	vec_float4 vx_base_0 = spu_convtf(vx_base_i,1);
	vec_float4 vx_base_4 = spu_add(vx_base_0,_base_add4);
	vec_float4 vx_base_8 = spu_add(vx_base_0,_base_add8);
	vec_float4 vx_base_12 = spu_add(vx_base_8,_base_add4);
	vec_float4 vx_base_16 = spu_add(vx_base_0,_base_add16);
	vec_float4 vx_base_20 = spu_add(vx_base_4,_base_add16);
	vec_float4 vx_base_24 = spu_add(vx_base_8,_base_add16);
	vec_float4 vx_base_28 = spu_add(vx_base_12,_base_add16);

	vec_float4 lx = spu_splats(spu_extract(vx, 0));
	vec_float4 rx = spu_splats(spu_extract(vx, 0));
	vec_float4 y = spu_splats(spu_extract(vy, 0));

	vec_float4 dl = spu_splats(spu_extract(grad,2));
	vec_float4 dr = spu_splats(spu_extract(grad,1));

	unsigned int right_flag = tri->right;
	unsigned int bottom_pos = if_then_else(right_flag, 2, 1);
	unsigned int middle_pos = if_then_else(right_flag, 1, 2);

	printf("dl %f, dr %f\n", dl, dr);

	printf("blocks: A %d,%d B %d,%d C %d,%d\n\ttop: %d mid: %d bot: %d\n",
		spu_extract(vx_block,0),spu_extract(vy_block,0),
		spu_extract(vx_block,1),spu_extract(vy_block,1),
		spu_extract(vx_block,2),spu_extract(vy_block,2),
		spu_extract(vy_block,0),
		spu_extract(vy_block,middle_pos),
		spu_extract(vy_block,bottom_pos));

	unsigned int start_line = spu_extract(vy_int,0) & 31;
	unsigned int end_line = 32;
	unsigned int bx = spu_extract(vx_block,0);
	unsigned int by = spu_extract(vy_block,0);

	unsigned int middle_y = spu_extract(vy_block,middle_pos);
	unsigned int bottom_y = spu_extract(vy_block,bottom_pos);

	triangle_half(start_line, end_line, bx, by, middle_y, 
		lx, rx, dl, dr, current_block,
		vx_base_0, vx_base_4, vx_base_8, vx_base_12,
		vx_base_16, vx_base_20, vx_base_24, vx_base_28
	);

/////
/*
	}
}
*/
/////
/*
	vec_uint4 bottom_shuffle = spu_sel(
		spu_splats(0x10111213), spu_splats(0x00010203), right);
	vec_uint4 middle_shuffle = spu_sel(
		spu_splats(0x00010203), spu_splats(0x10111213), right);
*/
}






//////////////////////////////////////////////////////////////////////////////





screen_block buffer __attribute__((aligned(128)));

void _draw_imp_triangle(triangle* tri)
{
	init_screen_block(&buffer, 31);

	fast_triangle(tri, &buffer);
}

