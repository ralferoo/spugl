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

//////////////////////////////////////////////////////////////////////////////
//
// This file holds the standard shaders, the idea is that a shader processes
// a block of 32x32 pixels at once using an unrolled function that processes
// 4 pixels at once. This function is sandwiched between the PROCESS_BLOCK_HEAD
// and PROCESS_BLOCK_END macros.
//
// tAa, tAb, tAc hold the adjusted mutliplication factors for the vertices
// ready to be passed into extract.
//
// pixel holds the word mask of pixels to be considered for inclusion.

#include <spu_mfcio.h>
#include "fifo.h"
#include "struct.h"
#include "queue.h"

static const vec_float4 muls = {0.0f, 1.0f, 2.0f, 3.0f};

static inline vec_float4 extract(
	vec_float4 what, vec_float4 tAa, vec_float4 tAb, vec_float4 tAc)
{
	return	spu_madd(spu_splats(spu_extract(what,0)),tAa,
		spu_madd(spu_splats(spu_extract(what,1)),tAb,
		spu_mul (spu_splats(spu_extract(what,2)),tAc)));
}
	
//////////////////////////////////////////////////////////////////////////////

void* linearColourFill(void* self, Block* block, ActiveBlock* active, int tag)
{
	Triangle* tri = block->triangle;

	vec_float4 A_dx = tri->A_dx;
	vec_float4 Aa_dx = spu_splats(spu_extract(A_dx,0));
	vec_float4 Ab_dx = spu_splats(spu_extract(A_dx,1));
	vec_float4 Ac_dx = spu_splats(spu_extract(A_dx,2));

	vec_float4 A_dy = tri->blockA_dy;
	vec_float4 Aa_dy = spu_splats(spu_extract(A_dy,0));
	vec_float4 Ab_dy = spu_splats(spu_extract(A_dy,1));
	vec_float4 Ac_dy = spu_splats(spu_extract(A_dy,2));

	vec_float4 A_dx4 = tri->A_dx4;
	vec_float4 Aa_dx4 = spu_splats(spu_extract(A_dx4,0));
	vec_float4 Ab_dx4 = spu_splats(spu_extract(A_dx4,1));
	vec_float4 Ac_dx4 = spu_splats(spu_extract(A_dx4,2));

	vec_float4 A = block->A;
	vec_float4 Aa = spu_madd(muls,Aa_dx,spu_splats(spu_extract(A,0)));
	vec_float4 Ab = spu_madd(muls,Ab_dx,spu_splats(spu_extract(A,1)));
	vec_float4 Ac = spu_madd(muls,Ac_dx,spu_splats(spu_extract(A,2)));

	vec_uint4 left = spu_splats(block->left);
	vec_uint4* ptr = block->pixels;
	do {
		vec_uint4 uAa = (vec_uint4) Aa;
		vec_uint4 uAb = (vec_uint4) Ab;
		vec_uint4 uAc = (vec_uint4) Ac;
		vec_uint4 allNeg = spu_and(spu_and(uAa,uAb),uAc);
		vec_uint4 pixel = spu_rlmaska(allNeg,-31);
		vec_uint4 bail = spu_orx(pixel);
		if (spu_extract(bail,0)) {
			vec_float4 t_w = extract(tri->w, Aa, Ab, Ac);
			vec_float4 w = spu_splats(1.0f)/t_w;
			vec_float4 tAa = spu_mul(Aa,w);
			vec_float4 tAb = spu_mul(Ab,w);
			vec_float4 tAc = spu_mul(Ac,w);

//PROCESS_BLOCK_HEAD(process_colour_block)
			vec_float4 t_r = extract(tri->r, tAa, tAb, tAc);
			vec_float4 t_g = extract(tri->g, tAa, tAb, tAc);
			vec_float4 t_b = extract(tri->b, tAa, tAb, tAc);

			vec_uint4 red = spu_and(spu_rlmask(spu_convtu(t_r,32),-8), 0xff0000);
			vec_uint4 green = spu_and(spu_rlmask(spu_convtu(t_g,32),-16), 0xff00);
			vec_uint4 blue = spu_rlmask(spu_convtu(t_b,32),-24);

			vec_uint4 colour = spu_or(spu_or(blue, green),red);

			vec_uint4 current = *ptr;
			*ptr = spu_sel(current, colour, pixel);
//PROCESS_BLOCK_END
		} 
		vec_uint4 which = spu_and(left,spu_splats((unsigned int)7));
		vec_uint4 sel = spu_cmpeq(which,1);
		ptr++;
		left -= spu_splats(1);
		Aa += spu_sel(Aa_dx4,Aa_dy,sel);
		Ab += spu_sel(Ab_dx4,Ab_dy,sel);
		Ac += spu_sel(Ac_dx4,Ac_dy,sel);
	} while (spu_extract(left,0)>0);

	block->triangle->count--;
	return 0;

}

//////////////////////////////////////////////////////////////////////////////

static const vec_uchar16 shuf_gath_01 = {
	2,3,128,128, 18,19,128,128, SEL_00 SEL_00};

static const vec_uchar16 shuf_gath_23 = {
	SEL_00 SEL_00 2,3,128,128, 18,19,128,128,};

extern void* textureCache;
extern void* loadMissingTextures(void* self, Block* block, ActiveBlock* active, int tag,
			vec_float4 A, vec_uint4 left, vec_uint4* ptr, vec_uint4 tex_keep,
			vec_uint4 block_id, vec_uint4 cache_not_found, vec_uint4 pixel);

//////////////////////////////////////////////////////////////////////////////

void* textureMapFill(void* self, Block* block, ActiveBlock* active, int tag)
{
	Triangle* tri = block->triangle;

	vec_float4 A_dx = tri->A_dx;
	vec_float4 Aa_dx = spu_splats(spu_extract(A_dx,0));
	vec_float4 Ab_dx = spu_splats(spu_extract(A_dx,1));
	vec_float4 Ac_dx = spu_splats(spu_extract(A_dx,2));

	vec_float4 A_dy = tri->blockA_dy;
	vec_float4 Aa_dy = spu_splats(spu_extract(A_dy,0));
	vec_float4 Ab_dy = spu_splats(spu_extract(A_dy,1));
	vec_float4 Ac_dy = spu_splats(spu_extract(A_dy,2));

	vec_float4 A_dx4 = tri->A_dx4;
	vec_float4 Aa_dx4 = spu_splats(spu_extract(A_dx4,0));
	vec_float4 Ab_dx4 = spu_splats(spu_extract(A_dx4,1));
	vec_float4 Ac_dx4 = spu_splats(spu_extract(A_dx4,2));

	vec_float4 A = block->A;
	vec_float4 Aa = spu_madd(muls,Aa_dx,spu_splats(spu_extract(A,0)));
	vec_float4 Ab = spu_madd(muls,Ab_dx,spu_splats(spu_extract(A,1)));
	vec_float4 Ac = spu_madd(muls,Ac_dx,spu_splats(spu_extract(A,2)));

	vec_uint4 left = spu_splats(block->left);
	vec_uint4* ptr = block->pixels;
	vec_uint4 tex_id_base = spu_splats((unsigned int)tri->tex_id_base);
	vec_uint4 tex_keep = spu_splats(0);

	do {
		vec_uint4 uAa = (vec_uint4) Aa;
		vec_uint4 uAb = (vec_uint4) Ab;
		vec_uint4 uAc = (vec_uint4) Ac;
		vec_uint4 allNeg = spu_and(spu_and(uAa,uAb),uAc);
		vec_uint4 pixel = spu_rlmaska(allNeg,-31);
		vec_uint4 bail = spu_orx(pixel);
		if (spu_extract(bail,0)) {
			vec_float4 t_w = extract(tri->w, Aa, Ab, Ac);
			vec_float4 w = spu_splats(1.0f)/t_w;
			vec_float4 tAa = spu_mul(Aa,w);
			vec_float4 tAb = spu_mul(Ab,w);
			vec_float4 tAc = spu_mul(Ac,w);

//PROCESS_BLOCK_HEAD(process_tex_block)

			vec_float4 t_s = extract(tri->s, tAa, tAb, tAc);
			vec_float4 t_t = extract(tri->t, tAa, tAb, tAc);

			vec_uint4 s_sub = spu_and(spu_rlmask(spu_convtu(t_s,32),-17), 0xf80);	//19-2
			vec_uint4 t_sub = spu_and(spu_rlmask(spu_convtu(t_t,32),-22), 0x7c);	//24-2
			vec_uint4 sub_block_pixel = spu_or(s_sub,t_sub);

			vec_uint4 s_blk = spu_and(spu_rlmask(spu_convtu(t_s,32),-29), 0x7);	//24+5
			vec_uint4 t_blk = spu_and(spu_rlmask(spu_convtu(t_t,32),-26), 0x38);	//24+2
			vec_uint4 block_id = spu_add(tex_id_base,spu_or(s_blk,t_blk));

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
		
			vec_uint4 gather_01 = spu_shuffle(gather_0,gather_1,shuf_gath_01);
			vec_uint4 gather_23 = spu_shuffle(gather_2,gather_3,shuf_gath_23);
			vec_uint4 gather = spu_or(gather_01,gather_23);
			tex_keep = spu_or(tex_keep, gather);
			vec_uint4 cache = spu_cntlz(gather);
			//vec_uint4 cache_not_found = spu_and(pixel,spu_cmpeq(cache,spu_splats((unsigned int)32)));
			vec_uint4 cache_not_found = spu_cmpeq(cache,spu_splats((unsigned int)32));
			unsigned int cache_orx = spu_extract(spu_orx(cache_not_found),0);
			if (cache_orx) {  // amazingly gcc does move this out of the loop :)
				return loadMissingTextures(self, block, active, tag,
					A, left, ptr, tex_keep,
					block_id, cache_not_found, pixel);
			}

			// pixel is mask of 1's where we want to draw
		
			vec_uint4 local_tex_base = spu_splats((unsigned int)&textureCache);
			vec_uint4 tex_ofs = spu_mulo( (vec_ushort8)cache,(vec_ushort8)((33*32+64)*4));
			//vec_uint4 tex_ofs = spu_sl(cache, 5+5+2);	// offset into texture
			vec_uint4 addr = spu_add(tex_ofs,spu_add(sub_block_pixel,local_tex_base));
		
			unsigned long pixel0 = *((u32*)spu_extract(addr,0));
			unsigned long pixel1 = *((u32*)spu_extract(addr,1));
			unsigned long pixel2 = *((u32*)spu_extract(addr,2));
			unsigned long pixel3 = *((u32*)spu_extract(addr,3));
			vec_uint4 colour = {pixel0, pixel1, pixel2, pixel3};
		
//			colour = spu_shuffle(colour, colour, rgba_argb);
		
			vec_uint4 current = *ptr;
			*ptr = spu_sel(current, colour, pixel);
//PROCESS_BLOCK_END

		} 
		vec_uint4 which = spu_and(left,spu_splats((unsigned int)7));
		vec_uint4 sel = spu_cmpeq(which,1);
		ptr++;
		left -= spu_splats(1);
		 A += spu_sel( A_dx4, A_dy,sel);
		Aa += spu_sel(Aa_dx4,Aa_dy,sel);
		Ab += spu_sel(Ab_dx4,Ab_dy,sel);
		Ac += spu_sel(Ac_dx4,Ac_dy,sel);
	} while (spu_extract(left,0)>0);

	block->triangle->count--;
	return 0;

}

//////////////////////////////////////////////////////////////////////////////

#define S_0 128

static vec_uchar16 get0 = (vec_uchar16) {
        S_0, 16, S_0, 0, S_0, 17, S_0, 1,
        S_0, 18, S_0, 2, S_0, 19, S_0, 3};

static vec_uchar16 merge = (vec_uchar16) {         // high 16 bits -> low 16 bits, merge alternately
        16,17, 0,1,  20,21,4,5,  24,25,8,9,  28,29,12,13};

static vec_uchar16 merge_shr8 = (vec_uchar16) {
        17,18, 1,2,  21,22,5,6,  25,26,9,10,  29,30,13,14};

static vec_uchar16 merge_lo = (vec_uchar16) {         // low 16 bits, merge alternately
        18,19, 2,3,  22,23,6,7,  26,27,10,11,  30,31,14,15};

static vec_uchar16 merge_pixels_01 = (vec_uchar16) {
	1,5,9,13, 17,21,25,29, S_0,S_0,S_0,S_0, S_0,S_0,S_0,S_0};

static vec_uchar16 merge_pixels_23 = (vec_uchar16) {
	S_0,S_0,S_0,S_0, S_0,S_0,S_0,S_0, 1,5,9,13, 17,21,25,29};

//static vec_uchar16 copy_lo_to_hi = (vec_uchar16) {         
//        0,1, 0,1,  4,5,4,5,  8,9,8,9,  12,13,12,13};

static vec_uchar16 splats_01 = (vec_uchar16) {         // low 16 bits, copy alternately
        6,7, 2,3, 6,7, 2,3, 6,7, 2,3, 6,7, 2,3}; 
static vec_uchar16 splats_23 = (vec_uchar16) {         // low 16 bits, copy alternately
	14,15,10,11, 14,15,10,11, 14,15,10,11, 14,15,10,11}; 

static vec_uchar16 rejoin = (vec_uchar16) {
	3,7,11,15,	1,5,9,13,	19,23,27,31,	17,21,25,29};


//////////////////////////////////////////////////////////////////////////////

void* linearTextureMapFill(void* self, Block* block, ActiveBlock* active, int tag)
{
	Triangle* tri = block->triangle;

	vec_float4 A_dx = tri->A_dx;
	vec_float4 Aa_dx = spu_splats(spu_extract(A_dx,0));
	vec_float4 Ab_dx = spu_splats(spu_extract(A_dx,1));
	vec_float4 Ac_dx = spu_splats(spu_extract(A_dx,2));

	vec_float4 A_dy = tri->blockA_dy;
	vec_float4 Aa_dy = spu_splats(spu_extract(A_dy,0));
	vec_float4 Ab_dy = spu_splats(spu_extract(A_dy,1));
	vec_float4 Ac_dy = spu_splats(spu_extract(A_dy,2));

	vec_float4 A_dx4 = tri->A_dx4;
	vec_float4 Aa_dx4 = spu_splats(spu_extract(A_dx4,0));
	vec_float4 Ab_dx4 = spu_splats(spu_extract(A_dx4,1));
	vec_float4 Ac_dx4 = spu_splats(spu_extract(A_dx4,2));

	vec_float4 A = block->A;
	vec_float4 Aa = spu_madd(muls,Aa_dx,spu_splats(spu_extract(A,0)));
	vec_float4 Ab = spu_madd(muls,Ab_dx,spu_splats(spu_extract(A,1)));
	vec_float4 Ac = spu_madd(muls,Ac_dx,spu_splats(spu_extract(A,2)));

	vec_uint4 left = spu_splats(block->left);
	vec_uint4* ptr = block->pixels;
	vec_uint4 tex_id_base = spu_splats((unsigned int)tri->tex_id_base);
	vec_uint4 tex_keep = spu_splats(0);

	do {
		vec_uint4 uAa = (vec_uint4) Aa;
		vec_uint4 uAb = (vec_uint4) Ab;
		vec_uint4 uAc = (vec_uint4) Ac;
		vec_uint4 allNeg = spu_and(spu_and(uAa,uAb),uAc);
		vec_uint4 pixel = spu_rlmaska(allNeg,-31);
		vec_uint4 bail = spu_orx(pixel);
		if (spu_extract(bail,0)) {
			vec_float4 t_w = extract(tri->w, Aa, Ab, Ac);
			vec_float4 w = spu_splats(1.0f)/t_w;
			vec_float4 tAa = spu_mul(Aa,w);
			vec_float4 tAb = spu_mul(Ab,w);
			vec_float4 tAc = spu_mul(Ac,w);

//PROCESS_BLOCK_HEAD(process_tex_block)

			vec_float4 t_s = extract(tri->s, tAa, tAb, tAc);
			vec_float4 t_t = extract(tri->t, tAa, tAb, tAc);

			vec_uint4 s_blk = spu_and(spu_rlmask(spu_convtu(t_s,32),-29), 0x7);	//24+5
			vec_uint4 t_blk = spu_and(spu_rlmask(spu_convtu(t_t,32),-26), 0x38);	//24+2
			vec_uint4 block_id = spu_add(tex_id_base,spu_or(s_blk,t_blk));

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
		
			vec_uint4 gather_01 = spu_shuffle(gather_0,gather_1,shuf_gath_01);
			vec_uint4 gather_23 = spu_shuffle(gather_2,gather_3,shuf_gath_23);
			vec_uint4 gather = spu_or(gather_01,gather_23);
			tex_keep = spu_or(tex_keep, gather);
			vec_uint4 cache = spu_cntlz(gather);
			//vec_uint4 cache_not_found = spu_and(pixel,spu_cmpeq(cache,spu_splats((unsigned int)32)));
			vec_uint4 cache_not_found = spu_cmpeq(cache,spu_splats((unsigned int)32));
			unsigned int cache_orx = spu_extract(spu_orx(cache_not_found),0);
			if (cache_orx) {  // amazingly gcc does move this out of the loop :)
				return loadMissingTextures(self, block, active, tag,
					A, left, ptr, tex_keep,
					block_id, cache_not_found, pixel);
			}

			// pixel is mask of 1's where we want to draw
		
			vec_uint4 s_sub = spu_and(spu_rlmask(spu_convtu(t_s,32),-17), 0xf80);	//19-2
			vec_uint4 t_sub = spu_and(spu_rlmask(spu_convtu(t_t,32),-22), 0x7c);	//24-2
			vec_uint4 sub_block_pixel = spu_or(s_sub,t_sub);

			vec_uint4 local_tex_base = spu_splats((unsigned int)&textureCache);
			vec_uint4 tex_ofs = spu_mulo( (vec_ushort8)cache,(vec_ushort8)((33*32+64)*4));
			vec_uint4 tex_ofs32 = spu_add(tex_ofs, spu_splats((unsigned int)(33*32*4)));
			vec_uint4 addr00 = spu_add(tex_ofs,spu_add(sub_block_pixel,local_tex_base));
		
			// (x,y+1) is always the next line in physical memory
			vec_uint4 addr01 = spu_add(addr00, (unsigned int)(32*4));

			// if x<32
			vec_uint4 addr10a = spu_add(addr00, (unsigned int)4);
			vec_uint4 addr11a = spu_add(addr00, (unsigned int)(32*4+4));

			// if x==32
			vec_uint4 sb_sub = spu_and(spu_rlmask(spu_convtu(t_s,32),-22), 0x7c);
			vec_uint4 addr10b = spu_add(tex_ofs32,spu_add(sb_sub,local_tex_base));
			vec_uint4 addr11b = spu_add(addr10b, spu_splats((unsigned int)4));

			// decision
			vec_uint4 is_x_32 = spu_cmpeq(t_sub,0x7c);
			vec_uint4 addr10 = spu_sel(addr10a,addr10b,is_x_32);
			vec_uint4 addr11 = spu_sel(addr11a,addr11b,is_x_32);

			// load pixel data for all 4 pixels
			unsigned long pixel0_00 = *((u32*)spu_extract(addr00,0));
			unsigned long pixel1_00 = *((u32*)spu_extract(addr00,1));
			unsigned long pixel2_00 = *((u32*)spu_extract(addr00,2));
			unsigned long pixel3_00 = *((u32*)spu_extract(addr00,3));
//			vec_uint4 colour00 = {pixel0_00, pixel1_00, pixel2_00, pixel3_00};
		
			unsigned long pixel0_01 = *((u32*)spu_extract(addr01,0));
			unsigned long pixel1_01 = *((u32*)spu_extract(addr01,1));
			unsigned long pixel2_01 = *((u32*)spu_extract(addr01,2));
			unsigned long pixel3_01 = *((u32*)spu_extract(addr01,3));
//			vec_uint4 colour01 = {pixel0_01, pixel1_01, pixel2_01, pixel3_01};
		
			unsigned long pixel0_10 = *((u32*)spu_extract(addr10,0));
			unsigned long pixel1_10 = *((u32*)spu_extract(addr10,1));
			unsigned long pixel2_10 = *((u32*)spu_extract(addr10,2));
			unsigned long pixel3_10 = *((u32*)spu_extract(addr10,3));
//			vec_uint4 colour10 = {pixel0_10, pixel1_10, pixel2_10, pixel3_10};
		
			unsigned long pixel0_11 = *((u32*)spu_extract(addr11,0));
			unsigned long pixel1_11 = *((u32*)spu_extract(addr11,1));
			unsigned long pixel2_11 = *((u32*)spu_extract(addr11,2));
			unsigned long pixel3_11 = *((u32*)spu_extract(addr11,3));
//			vec_uint4 colour11 = {pixel0_11, pixel1_11, pixel2_11, pixel3_11};
		

			vec_uint4 s_pxofs = spu_and(spu_rlmask(spu_convtu(t_s,32),-16), (vec_uint4)0xff);
			vec_uint4 t_pxofs = spu_and(spu_rlmask(spu_convtu(t_t,32),-16), (vec_uint4)0xff);

//			s_pxofs = (vec_uint4) 0x80;
//			t_pxofs = (vec_uint4) 0x1;

///////////
			vec_short8 pixel01_ = (vec_short8) spu_shuffle((vec_uint4)pixel0_00, (vec_uint4)pixel1_00, get0);
			vec_short8 pixel01_x = (vec_short8) spu_shuffle((vec_uint4)pixel0_01, (vec_uint4)pixel1_01, get0);
			vec_short8 pixel01_xy = (vec_short8) spu_shuffle((vec_uint4)pixel0_11, (vec_uint4)pixel1_11, get0);
			vec_short8 pixel01_y = (vec_short8) spu_shuffle((vec_uint4)pixel0_10, (vec_uint4)pixel1_10, get0);
			vec_short8 pixel01_h = spu_sub(pixel01_x,pixel01_);
			vec_short8 pixel01_yh = spu_sub(pixel01_xy,pixel01_y);

			vec_short8 pixel01_multx = (vec_short8)spu_shuffle(s_pxofs,s_pxofs,splats_01);
			vec_short8 pixel01_multy = (vec_short8)spu_shuffle(t_pxofs,t_pxofs,splats_01);

			vec_int4 pixel0_th = spu_mulo(pixel01_h, pixel01_multx);
			vec_int4 pixel0_bh = spu_mulo(pixel01_yh, pixel01_multx);
			vec_int4 pixel1_th = spu_mule(pixel01_h, pixel01_multx);
			vec_int4 pixel1_bh = spu_mule(pixel01_yh, pixel01_multx);
			vec_short8 pixel01_th = (vec_short8)spu_shuffle(pixel0_th, pixel1_th, merge_shr8);
			vec_short8 pixel01_bh = (vec_short8)spu_shuffle(pixel0_bh, pixel1_bh, merge_shr8);

			vec_short8 pixel01_tm = spu_add(pixel01_, pixel01_th);
			vec_short8 pixel01_bm = spu_add(pixel01_y, pixel01_bh);

			vec_short8 pixel01_d = spu_sub(pixel01_bm,pixel01_tm);
			vec_int4 pixel0_d = spu_mulo(pixel01_d, pixel01_multy);
			vec_int4 pixel1_d = spu_mule(pixel01_d, pixel01_multy);
			vec_short8 pixel01_mm = (vec_short8)spu_shuffle(pixel0_d, pixel1_d, merge_shr8);
			vec_short8 pixel01_done = spu_add(pixel01_mm, pixel01_tm);

///////////
			vec_short8 pixel23_ = (vec_short8) spu_shuffle((vec_uint4)pixel2_00, (vec_uint4)pixel3_00, get0);
			vec_short8 pixel23_x = (vec_short8) spu_shuffle((vec_uint4)pixel2_01, (vec_uint4)pixel3_01, get0);
			vec_short8 pixel23_xy = (vec_short8) spu_shuffle((vec_uint4)pixel2_11, (vec_uint4)pixel3_11, get0);
			vec_short8 pixel23_y = (vec_short8) spu_shuffle((vec_uint4)pixel2_10, (vec_uint4)pixel3_10, get0);
			vec_short8 pixel23_h = spu_sub(pixel23_x,pixel23_);
			vec_short8 pixel23_yh = spu_sub(pixel23_xy,pixel23_y);

			vec_short8 pixel23_multx = (vec_short8)spu_shuffle(s_pxofs,s_pxofs,splats_23);
			vec_short8 pixel23_multy = (vec_short8)spu_shuffle(t_pxofs,t_pxofs,splats_23);

			vec_int4 pixel2_th = spu_mulo(pixel23_h, pixel23_multx);
			vec_int4 pixel2_bh = spu_mulo(pixel23_yh, pixel23_multx);
			vec_int4 pixel3_th = spu_mule(pixel23_h, pixel23_multx);
			vec_int4 pixel3_bh = spu_mule(pixel23_yh, pixel23_multx);
			vec_short8 pixel23_th = (vec_short8)spu_shuffle(pixel2_th, pixel3_th, merge_shr8);
			vec_short8 pixel23_bh = (vec_short8)spu_shuffle(pixel2_bh, pixel3_bh, merge_shr8);

			vec_short8 pixel23_tm = spu_add(pixel23_, pixel23_th);
			vec_short8 pixel23_bm = spu_add(pixel23_y, pixel23_bh);

			vec_short8 pixel23_d = spu_sub(pixel23_bm,pixel23_tm);
			vec_int4 pixel2_d = spu_mulo(pixel23_d, pixel23_multy);
			vec_int4 pixel3_d = spu_mule(pixel23_d, pixel23_multy);
			vec_short8 pixel23_mm = (vec_short8)spu_shuffle(pixel2_d, pixel3_d, merge_shr8);
			vec_short8 pixel23_done = spu_add(pixel23_mm, pixel23_tm);

///////////
			vec_uint4 colour = (vec_uint4) spu_shuffle(pixel01_done,pixel23_done,rejoin);

			vec_uint4 current = *ptr;
			*ptr = spu_sel(current, colour, pixel);
//PROCESS_BLOCK_END

		} 
		vec_uint4 which = spu_and(left,spu_splats((unsigned int)7));
		vec_uint4 sel = spu_cmpeq(which,1);
		ptr++;
		left -= spu_splats(1);
		 A += spu_sel( A_dx4, A_dy,sel);
		Aa += spu_sel(Aa_dx4,Aa_dy,sel);
		Ab += spu_sel(Ab_dx4,Ab_dy,sel);
		Ac += spu_sel(Ac_dx4,Ac_dy,sel);
	} while (spu_extract(left,0)>0);

	block->triangle->count--;
	return 0;

}
