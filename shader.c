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
// #define DEBUG_2		// corrupt each block we load in
// #define DEBUG_3

#define COUNT_BLOCKED_DMA
#define TEXTURE_MAPPED

extern _bitmap_image screen;

u32 textureTemp0[32] __attribute__((aligned(128)));
u32 textureTemp1[32] __attribute__((aligned(128)));
u32 textureTemp2[32] __attribute__((aligned(128)));
u32 textureTemp3[32] __attribute__((aligned(128)));


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
		control.block_count++;
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
	
/*
 * the vectors below contain Aa, Ab, Ac for each of the 4 corners of the block
 *
 * only the first element of the result is set, and will be all ones if
 * the block should be displayed, all zeros if not
 */
/*
static inline vec_uint4 block_test(
	vec_float4 A0, vec_float4 A1, vec_float4 A2, vec_float4 A3) 
{
	vec_float4 Aa = spu_splats(spu_extract(A,0));
	vec_float4 Ab = spu_splats(spu_extract(A,1));
	vec_float4 Ac = spu_splats(spu_extract(A,2));

	vec_uint4 uAa = (vec_uint4) tAa;
	vec_uint4 uAb = (vec_uint4) tAb;
	vec_uint4 uAc = (vec_uint4) tAc;

	vec_uint4 allNeg = spu_and(spu_and(spu_orx(uAa),spu_orx(uAb)),
				   spu_orx(uAc));
	vec_uint4 pixel = spu_rlmaska(allNeg,-31);
	return pixel;
}
*/

const vec_uchar16 rgba_argb = {
	3,0,1,2, 7,4,5,6, 11,8,9,10, 15,12,13,14}; 

#ifdef TEXTURE_MAPPED
static inline void sub_block(vec_uint4* ptr, 
	triangle* tri, vec_float4 tAa, vec_float4 tAb, vec_float4 tAc)
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
	vec_float4 t_w = extract(tri->w, tAa, tAb, tAc);
	vec_float4 w = spu_splats(1.0f)/t_w;

	tAa = spu_mul(tAa,w);
	tAb = spu_mul(tAb,w);
	tAc = spu_mul(tAc,w);

	vec_float4 t_s = extract(tri->s, tAa, tAb, tAc);
	vec_float4 t_t = extract(tri->t, tAa, tAb, tAc);

	vec_uint4 s = spu_and(spu_rlmask(spu_convtu(t_s,32),-14), 0x3fc00);
	vec_uint4 t = spu_and(spu_rlmask(spu_convtu(t_t,32),-22), 0x3fc);
	vec_uint4 offset = spu_or(s,t);

//	printf("offset is %lx,%lx,%lx,%lx, texture at %llx\n", 
//			spu_extract(offset,0),
//			spu_extract(offset,1),
//			spu_extract(offset,2),
//			spu_extract(offset,3),
//			control.texture_hack);

	unsigned long texAddrBase = control.texture_hack[tri->texture];

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

//	colour = spu_and(colour, spu_splats((unsigned int)0xff));
	colour = spu_shuffle(colour, colour, rgba_argb);

	vec_uint4 current = *ptr;
	*ptr = spu_sel(current, colour, pixel);
}
#else
static inline void sub_block(vec_uint4* ptr, 
	triangle* tri, vec_float4 tAa, vec_float4 tAb, vec_float4 tAc)
{
	vec_uint4 uAa = (vec_uint4) tAa;
	vec_uint4 uAb = (vec_uint4) tAb;
	vec_uint4 uAc = (vec_uint4) tAc;

	vec_uint4 allNeg = spu_and(spu_and(uAa,uAb),uAc);
	vec_uint4 pixel = spu_rlmaska(allNeg,-31);

	vec_float4 t_w = extract(tri->w, tAa, tAb, tAc);
	vec_float4 w = spu_splats(1.0f)/t_w;

	tAa = spu_mul(tAa,w);
	tAb = spu_mul(tAb,w);
	tAc = spu_mul(tAc,w);

	vec_float4 t_r = extract(tri->r, tAa, tAb, tAc);
	vec_float4 t_g = extract(tri->g, tAa, tAb, tAc);
	vec_float4 t_b = extract(tri->b, tAa, tAb, tAc);

	vec_uint4 red = spu_and(spu_rlmask(spu_convtu(t_r,32),-8), 0xff0000);
	vec_uint4 green = spu_and(spu_rlmask(spu_convtu(t_g,32),-16), 0xff00);
	vec_uint4 blue = spu_rlmask(spu_convtu(t_b,32),-24);

	vec_uint4 colour = spu_or(spu_or(blue, green),red);

	vec_uint4 current = *ptr;
	*ptr = spu_sel(current, colour, pixel);
}
#endif

static inline void process_block(vec_uint4* block_ptr,
		triangle* tri,
		vec_float4 Aa,vec_float4 Ab,vec_float4 Ac,
		vec_float4 Aa_dx4,vec_float4 Ab_dx4,vec_float4 Ac_dx4,
		vec_float4 Aa_dy,vec_float4 Ab_dy,vec_float4 Ac_dy)
{
	unsigned int line;
	for (line=0; line<32; line++) {
		vec_float4 tAa = Aa;
		vec_float4 tAb = Ab;
		vec_float4 tAc = Ac;

		Aa += Aa_dy;
		Ab += Ab_dy;
		Ac += Ac_dy;

		sub_block(&block_ptr[0], tri, tAa,tAb,tAc);
		tAa += Aa_dx4; tAb += Ab_dx4; tAc += Ac_dx4;
		sub_block(&block_ptr[1], tri, tAa,tAb,tAc);
		tAa += Aa_dx4; tAb += Ab_dx4; tAc += Ac_dx4;
		sub_block(&block_ptr[2], tri, tAa,tAb,tAc);
		tAa += Aa_dx4; tAb += Ab_dx4; tAc += Ac_dx4;
		sub_block(&block_ptr[3], tri, tAa,tAb,tAc);
		tAa += Aa_dx4; tAb += Ab_dx4; tAc += Ac_dx4;
		sub_block(&block_ptr[4], tri, tAa,tAb,tAc);
		tAa += Aa_dx4; tAb += Ab_dx4; tAc += Ac_dx4;
		sub_block(&block_ptr[5], tri, tAa,tAb,tAc);
		tAa += Aa_dx4; tAb += Ab_dx4; tAc += Ac_dx4;
		sub_block(&block_ptr[6], tri, tAa,tAb,tAc);
		tAa += Aa_dx4; tAb += Ab_dx4; tAc += Ac_dx4;
		sub_block(&block_ptr[7], tri, tAa,tAb,tAc);

		block_ptr += 8;
	}
}

static void big_block(unsigned int bx, unsigned int by,
		screen_block* current_block,
		triangle* tri,
		vec_float4 Aa,vec_float4 Ab,vec_float4 Ac,
		vec_float4 Aa_dx4,vec_float4 Ab_dx4,vec_float4 Ac_dx4,
		vec_float4 Aa_dy,vec_float4 Ab_dy,vec_float4 Ac_dy)
{
	u64 scrbuf = screen.address + screen.bytes_per_line*by*32+bx*128;

	load_screen_block(current_block, scrbuf, screen.bytes_per_line, 32);
	wait_screen_block(current_block);

	vec_uint4* block_ptr = (vec_uint4*) ((void*)&current_block->pixels[0]);

#ifdef DEBUG_2
	int a;
	unsigned long* q = (unsigned long*) block_ptr;
	for (a=0; a<32*8*4; a++) {
		q[a] += 0x090000;
	}
#endif

	process_block(block_ptr, tri, 
		Aa, Ab,Ac, Aa_dx4,Ab_dx4,Ac_dx4, Aa_dy,Ab_dy,Ac_dy);

//	wait_screen_block(current_block);
//	flush_screen_block(current_block);
}

const vec_float4 muls = {0.0f, 1.0f, 2.0f, 3.0f};
const vec_float4 muls4 = {4.0f, 4.0f, 4.0f, 4.0f};
const vec_float4 muls32 = {32.0f, 32.0f, 32.0f, 32.0f};

void fast_triangle(triangle* tri, screen_block* current_block)
{
	vec_float4 vx = tri->x;
	vec_float4 vy = tri->y;
	vec_float4 minmax_ = tri->minmax;

	vec_int4 minmax = spu_convts(tri->minmax,0);
	vec_int4 minmax_block = spu_rlmaska(minmax,-5);
	vec_int4 minmax_block_mask = minmax & spu_splats(~31);
	vec_float4 minmax_block_topleft = spu_convtf(minmax_block_mask,0);

	vec_float4 A = spu_madd(spu_splats(spu_extract(minmax_block_topleft,0)),tri->dAdx,
		       spu_madd(spu_splats(spu_extract(minmax_block_topleft,1)),tri->dAdy,tri->A));

//	vec_float4 dA_dx32 = spu_mul(spu_splats(32.0f),tri->dAdx);
//	vec_float4 dA_dy32 = spu_mul(spu_splats(32.0f),tri->dAdy);

	int block_left = spu_extract(minmax_block,0);
	int block_top = spu_extract(minmax_block,1);
	int block_right = spu_extract(minmax_block,2);
	int block_bottom = spu_extract(minmax_block,3);

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

	vec_float4 Aa_dx32 = spu_mul(muls32,Aa_dx);
	vec_float4 Ab_dx32 = spu_mul(muls32,Ab_dx);
	vec_float4 Ac_dx32 = spu_mul(muls32,Ac_dx);

	vec_float4 Aa_dy32 = spu_mul(muls32,Aa_dy);
	vec_float4 Ab_dy32 = spu_mul(muls32,Ab_dy);
	vec_float4 Ac_dy32 = spu_mul(muls32,Ac_dy);

	int bx,by;
	for (by=block_top; by<= block_bottom; by++) {
		vec_float4 tAa = Aa;
		vec_float4 tAb = Ab;
		vec_float4 tAc = Ac;
		for (bx=block_left; bx<=block_right; bx++) {
			big_block(bx, by, current_block, tri,
				tAa,tAb,tAc,
				Aa_dx4,Ab_dx4,Ac_dx4,
				Aa_dy,Ab_dy,Ac_dy);
			tAa += Aa_dx32;
			tAb += Ab_dx32;
			tAc += Ac_dx32;
		}
		Aa += Aa_dy32;
		Ab += Ab_dy32;
		Ac += Ac_dy32;
	}
}



//////////////////////////////////////////////////////////////////////////////





screen_block buffer __attribute__((aligned(128)));

void _draw_imp_triangle(triangle* tri)
{
	init_screen_block(&buffer, 31);

	fast_triangle(tri, &buffer);

	// this shouldn't be necessary... AND... it sometimes causes duff
	// stuff to be blitted to screen, but i haven't sorted out the block
	// cache stuff yet
	flush_screen_block(&buffer);
	wait_screen_block(&buffer);
}

