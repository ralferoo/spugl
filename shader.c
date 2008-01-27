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

#define COUNT_BLOCKED_DMA

extern _bitmap_image screen;

u32 textureTemp[32] __attribute__((aligned(128)));


vec_int4 _a[10];

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

unsigned long wait_screen_block(screen_block* block)
{
#ifndef COUNT_BLOCKED_DMA
	mfc_write_tag_mask(1 << block->tagid);
	unsigned long mask = mfc_read_tag_status_any();
	return mask;
#else
	unsigned long mask = 1 << block->tagid;
	unsigned long done;
	do {
		mfc_write_tag_mask(mask);
		done = mfc_read_tag_status_any();
		control.block_count++;
	} while (!(done&mask));
	return done&mask;
#endif
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
	vec_float4 what,
	triangle* tri, vec_float4 tAa, vec_float4 tAb, vec_float4 tAc)
{
	vec_float4 r_w = what; //spu_shuffle(what, what, tri->shuffle);
	vec_float4 t_w = spu_madd(spu_splats(spu_extract(r_w,0)),tAa,
			 spu_madd(spu_splats(spu_extract(r_w,1)),tAb,
			 spu_mul (spu_splats(spu_extract(r_w,2)),tAc)));
	return t_w;
}
	

static inline void sub_block(vec_uint4* ptr, 
	vec_float4 lx, vec_float4 rx, vec_float4 vx_base,
	triangle* tri, vec_float4 tAa, vec_float4 tAb, vec_float4 tAc)
{
	int i;

#ifdef DEBUG_4
	for (i=0; i<4; i++) {
		printf("(tAa=%2.2f tAb=%2.2f tAc=%2.2f) ",
			spu_extract(tAa,i),
			spu_extract(tAb,i),
			spu_extract(tAc,i));
	}
	printf("\n");
#endif

	vec_uint4 left = spu_cmpgt(vx_base, lx);
	vec_uint4 right = spu_cmpgt(rx, vx_base);
	vec_uint4 pixel = spu_nand(right, left);

	vec_float4 t_w = extract(tri->w, tri, tAa, tAb, tAc);
	vec_float4 w = spu_splats(1.0f)/t_w;

	tAa = spu_mul(tAa,w);
	tAb = spu_mul(tAb,w);
	tAc = spu_mul(tAc,w);

	vec_float4 t_r = extract(tri->r, tri, tAa, tAb, tAc);
	vec_float4 t_g = extract(tri->g, tri, tAa, tAb, tAc);
	vec_float4 t_b = extract(tri->b, tri, tAa, tAb, tAc);

#ifdef DEBUG_4
	for (i=0; i<4; i++) {
		printf("%2.2f->(%2.2f %2.2f %2.2f) ",
			spu_extract(t_w,i),
			spu_extract(t_r,i),
			spu_extract(t_g,i),
			spu_extract(t_b,i));
	}
	printf("\n");
#endif

	vec_uint4 red = spu_and(spu_rlmask(spu_convtu(t_r,32),-8), 0xff0000);
	vec_uint4 green = spu_and(spu_rlmask(spu_convtu(t_g,32),-16), 0xff00);
	vec_uint4 blue = spu_rlmask(spu_convtu(t_b,32),-24);

	vec_uint4 colour = spu_or(spu_or(blue, green),red);

//	unsigned int col = (spu_extract(red,0) << 16) |
//			   (spu_extract(green,0) << 8) |
//			    spu_extract(blue,0);
//	vec_uint4 colour = spu_splats(col);
	
	vec_uint4 current = *ptr;
	*ptr = spu_sel(colour, current, pixel);
}

vec_float4 vx_base_0 = {0.5f, 1.5f, 2.5f, 3.5f};
vec_float4 vx_base_4 = {4.5f, 5.5f, 6.5f, 7.5f};
vec_float4 vx_base_8 = {8.5f, 9.5f, 10.5f, 11.5f};
vec_float4 vx_base_12 = {12.5f, 13.5f, 14.5f, 15.5f};
vec_float4 vx_base_16 = {16.5f, 17.5f, 18.5f, 19.5f};
vec_float4 vx_base_20 = {20.5f, 21.5f, 22.5f, 23.5f};
vec_float4 vx_base_24 = {24.5f, 25.5f, 26.5f, 27.5f};
vec_float4 vx_base_28 = {28.5f, 29.5f, 30.5f, 31.5f};

vec_float4 muls = {0.0f, 1.0f, 2.0f, 3.0f};
vec_float4 muls4 = {4.0f, 4.0f, 4.0f, 4.0f};

static inline void process_block(vec_uint4* block_ptr,
		unsigned int start_line, unsigned int end_line,
		vec_float4 lx, vec_float4 rx, 
		vec_float4 dl, vec_float4 dr, 
		triangle* tri, vec_float4 A)
{
	block_ptr += 8*start_line;

	vec_float4 Aa = spu_splats(spu_extract(A,0));
	vec_float4 Ab = spu_splats(spu_extract(A,1));
	vec_float4 Ac = spu_splats(spu_extract(A,2));

	vec_float4 Aa_dx = spu_splats(spu_extract(tri->dAdx,0));
	vec_float4 Ab_dx = spu_splats(spu_extract(tri->dAdx,1));
	vec_float4 Ac_dx = spu_splats(spu_extract(tri->dAdx,2));

	Aa = spu_madd(muls,Aa_dx,Aa);
	Ab = spu_madd(muls,Ab_dx,Ab);
	Ac = spu_madd(muls,Ac_dx,Ac);

	vec_float4 Aa_dx4 = spu_mul(muls4,Aa_dx);
	vec_float4 Ab_dx4 = spu_mul(muls4,Ab_dx);
	vec_float4 Ac_dx4 = spu_mul(muls4,Ac_dx);

	vec_float4 Aa_dy = spu_splats(spu_extract(tri->dAdy,0));
	vec_float4 Ab_dy = spu_splats(spu_extract(tri->dAdy,1));
	vec_float4 Ac_dy = spu_splats(spu_extract(tri->dAdy,2));

	vec_float4 muldy = spu_convtf(spu_splats(start_line),0);
	
	Aa = spu_madd(muldy,Aa_dy,Aa);
	Ab = spu_madd(muldy,Ab_dy,Ab);
	Ac = spu_madd(muldy,Ac_dy,Ac);

	unsigned int line;
	for (line=start_line; line<end_line; line++) {
		vec_float4 tAa = Aa;
		vec_float4 tAb = Ab;
		vec_float4 tAc = Ac;

		Aa += Aa_dy;
		Ab += Ab_dy;
		Ac += Ac_dy;

		sub_block(&block_ptr[0], lx, rx, vx_base_0, tri, tAa,tAb,tAc);
		tAa += Aa_dx4; tAb += Ab_dx4; tAc += Ac_dx4;
		sub_block(&block_ptr[1], lx, rx, vx_base_4, tri, tAa,tAb,tAc);
		tAa += Aa_dx4; tAb += Ab_dx4; tAc += Ac_dx4;
		sub_block(&block_ptr[2], lx, rx, vx_base_8, tri, tAa,tAb,tAc);
		tAa += Aa_dx4; tAb += Ab_dx4; tAc += Ac_dx4;
		sub_block(&block_ptr[3], lx, rx, vx_base_12, tri, tAa,tAb,tAc);
		tAa += Aa_dx4; tAb += Ab_dx4; tAc += Ac_dx4;
		sub_block(&block_ptr[4], lx, rx, vx_base_16, tri, tAa,tAb,tAc);
		tAa += Aa_dx4; tAb += Ab_dx4; tAc += Ac_dx4;
		sub_block(&block_ptr[5], lx, rx, vx_base_20, tri, tAa,tAb,tAc);
		tAa += Aa_dx4; tAb += Ab_dx4; tAc += Ac_dx4;
		sub_block(&block_ptr[6], lx, rx, vx_base_24, tri, tAa,tAb,tAc);
		tAa += Aa_dx4; tAb += Ab_dx4; tAc += Ac_dx4;
		sub_block(&block_ptr[7], lx, rx, vx_base_28, tri, tAa,tAb,tAc);

		lx = spu_add(lx, dl);
		rx = spu_add(rx, dr);
		block_ptr += 8;
	}
}

static void big_block(unsigned int bx, unsigned int by,
		screen_block* current_block,
		unsigned int start_line, unsigned int end_line,
		vec_float4 lx, vec_float4 rx, 
		vec_float4 dl, vec_float4 dr, 
		triangle* tri, vec_float4 A)
{
	u64 scrbuf = screen.address + screen.bytes_per_line*by*32+bx*128;

	load_screen_block(current_block, scrbuf, screen.bytes_per_line, 32);
	wait_screen_block(current_block);

	vec_uint4* block_ptr = (vec_uint4*) ((void*)&current_block->pixels[0]);

#ifdef DEBUG_2
	int a;
	unsigned long* q = (unsigned long*) block_ptr;
	for (a=0; a<32*8*4; a++) {
		q[a] += 0x060000;
	}
#endif

	process_block(block_ptr, start_line, end_line, lx, rx, dl, dr, tri, A);

//	wait_screen_block(current_block);
//	flush_screen_block(current_block);
}

vec_uchar16 copy_from_a = {
	SEL_A0 SEL_A1 SEL_A2 SEL_A3
};

vec_float4 _base_add4 = {4.0, 4.0, 4.0, 4.0};
vec_float4 _base_add8 = {8.0, 8.0, 8.0, 8.0};
vec_float4 _base_add16 = {16.0, 16.0, 16.0, 16.0};
vec_float4 _base_add32 = {32.0, 32.0, 32.0, 32.0};

void triangle_half_blockline(
	unsigned int start_line,
	unsigned int end_line,
	unsigned int bx,
	unsigned int by,

	unsigned int end_y,

	vec_float4 lx, vec_float4 rx, vec_float4 dl, vec_float4 dr,
	screen_block* current_block,

	vec_uchar16 dl_sel,
	vec_uchar16 dr_sel,
	triangle* tri, vec_float4 A_left
) {
		// this should probably all be done with if_then_else as
		// we only care about one element of the register

		signed int l = (end_line-start_line-1);
	if (l<0) return;

		vec_float4 mult = spu_splats((float)l);
		vec_float4 lx_bot = spu_add(lx, spu_mul(dl,mult));
		vec_float4 rx_bot = spu_add(rx, spu_mul(dr,mult));

		vec_float4 lx_min = spu_shuffle(lx_bot, lx, dl_sel);
		vec_float4 rx_max = spu_shuffle(rx, rx_bot, dr_sel);

		vec_int4 lx_int = spu_convts(lx_min,0);
		vec_int4 rx_int = spu_add(spu_splats(31),spu_convts(rx_max,0));

		vec_int4 left_block_v = spu_rlmaska(lx_int,-5);
		vec_int4 right_block_v = spu_rlmaska(rx_int,-5);

		signed int left_block = spu_extract(left_block_v, 0);
		signed int right_block = spu_extract(right_block_v, 0);

		vec_float4 block_x_delta = spu_convtf(spu_and(spu_convts(lx_min,0),~31),0);

		vec_float4 block_x_delta_1 = spu_convtf(spu_or(spu_and(spu_convts(lx_min,1),~62),spu_splats(1)),1);
		vec_float4 A = spu_madd(tri->dAdx,block_x_delta_1,A_left);
		vec_float4 dA_dx32 = spu_mul(spu_splats(32.0f),tri->dAdx);

//		printf("lx_min = %f\n", spu_extract(lx_min,0));
//		printf("l %d left %d right %d\n", l, left_block, right_block);

		int cur_block;
		for (cur_block = left_block; cur_block<=right_block; cur_block++) {
			big_block(
				cur_block, by,
				current_block,
				start_line, end_line,
				spu_sub(lx, block_x_delta),
				spu_sub(rx, block_x_delta),
				dl, dr, tri, A);
			block_x_delta = spu_add(block_x_delta, _base_add32);
			A += dA_dx32;
		}
}


void triangle_half(
	unsigned int start_line,
	unsigned int end_line,
	unsigned int bx,
	unsigned int by,

	unsigned int end_y,

	vec_float4 lx, vec_float4 rx, vec_float4 dl, vec_float4 dr,
	screen_block* current_block,

	triangle* tri, float block_top_y
) {
	vec_float4 A = spu_madd(spu_splats(block_top_y),tri->dAdy,tri->A);
	vec_float4 dA_dy32 = spu_mul(spu_splats(32.0f),tri->dAdy);

	vec_uchar16 dl_inc = (vec_uchar16) spu_cmpgt(dl, spu_splats(0.0f));
	vec_uchar16 dr_inc = (vec_uchar16) spu_cmpgt(dr, spu_splats(0.0f));
	vec_uchar16 plus16 = spu_splats((unsigned char)0x10);

	vec_uchar16 dl_sel = spu_or(copy_from_a, spu_and(dl_inc, plus16));
	vec_uchar16 dr_sel = spu_or(copy_from_a, spu_and(dr_inc, plus16));

	while (by < end_y)
	{
		triangle_half_blockline(start_line, 32, bx, by, end_y, 
			lx, rx, dl, dr, current_block,
			dl_sel, dr_sel, tri, A);
		by++;
		vec_float4 mult = spu_splats((float)(32-start_line));
		lx = spu_add(lx, spu_mul(dl,mult));
		rx = spu_add(rx, spu_mul(dr,mult));
		start_line = 0;
		A += dA_dy32;
	}

	triangle_half_blockline(start_line, end_line, bx, by, end_y, 
		lx, rx, dl, dr, current_block,
		dl_sel, dr_sel, tri, A);

}


void fast_triangle(triangle* tri, screen_block* current_block)
{
	vec_float4 vx = tri->x; //spu_shuffle(tri->x, tri->x, tri->shuffle);
	vec_float4 vy = tri->y; //spu_shuffle(tri->y, tri->y, tri->shuffle);

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

	vec_float4 dy=spu_sub(vy,spu_splats(spu_extract(vy,0)));
	vec_float4 dx=spu_sub(vx,spu_splats(spu_extract(vx,0)));
	vec_float4 grad=div(dx,dy);

	vec_float4 lx = spu_splats(spu_extract(vx, 0));
	vec_float4 rx = spu_splats(spu_extract(vx, 0));
	vec_float4 y = spu_splats(spu_extract(vy, 0));

	vec_float4 dl = spu_splats(spu_extract(grad,2));
	vec_float4 dr = spu_splats(spu_extract(grad,1));

	vec_float4 vy_block_top = spu_convtf(spu_and(vy_1,~62),1);

	unsigned int right_flag = tri->right;
	unsigned int bottom_pos = if_then_else(right_flag, 2, 1);
	unsigned int middle_pos = if_then_else(right_flag, 1, 2);

	unsigned int start_line = spu_extract(vy_int,0) & 31;
	unsigned int middle_line = spu_extract(vy_int,middle_pos) & 31;
	unsigned int bottom_line = spu_extract(vy_int,bottom_pos) & 31;

	int bx = spu_extract(vx_block,0);

	unsigned int top_y = spu_extract(vy_block,0);
	unsigned int middle_y = spu_extract(vy_block,middle_pos);
	unsigned int bottom_y = spu_extract(vy_block,bottom_pos);

	float m = (float)(spu_extract(vy_int,middle_pos)-spu_extract(vy_int,0));

	triangle_half(start_line, middle_line, bx, top_y, middle_y, 
		lx, rx, dl, dr, current_block,
		tri, spu_extract(vy_block_top,0)
	);

	if (right_flag) {	
		float ndy = spu_extract(vy,2)-spu_extract(vy,1);
		float ndx = spu_extract(vx,2)-spu_extract(vx,1);
		float ngrad = ndx/ndy;
		rx = spu_splats(spu_extract(vx, 1));
		dr = spu_splats(ngrad);
		lx = spu_add(lx, spu_mul(spu_splats(m), dl));
	} else {
		float ndy = spu_extract(vy,1)-spu_extract(vy,2);
		float ndx = spu_extract(vx,1)-spu_extract(vx,2);
		float ngrad = ndx/ndy;
		lx = spu_splats(spu_extract(vx, 2));
		dl = spu_splats(ngrad);
		rx = spu_add(rx, spu_mul(spu_splats(m), dr));
	}

	int nbx = spu_extract(vx_block,middle_pos);
	vec_float4 dbx = spu_splats((float)((nbx-bx)*32));
	bx = nbx;

	triangle_half(middle_line, bottom_line, bx, middle_y, bottom_y, 
		lx, rx, dl, dr, current_block,
		tri, spu_extract(vy_block_top,middle_pos)
	);
}






//////////////////////////////////////////////////////////////////////////////





screen_block buffer __attribute__((aligned(128)));

void _draw_imp_triangle(triangle* tri)
{
	init_screen_block(&buffer, 31);

	fast_triangle(tri, &buffer);
}

