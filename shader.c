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
			vec_uint4 block_id, vec_uint4 s, vec_uint4 t, 
			vec_uint4 cache_not_found, vec_uint4 pixel);

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
	vec_uint4 tex_keep = spu_splats((unsigned int)0);

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

			vec_float4 tf_s = extract(tri->s, tAa, tAb, tAc);
			vec_float4 tf_t = extract(tri->t, tAa, tAb, tAc);

			vec_uint4 t_s = spu_convtu(tf_s,32);
			vec_uint4 t_t = spu_convtu(tf_t,32);

			vec_uint4 s_sub = spu_and(spu_rlmask(t_s,-17), 0xf80);	//19-2
			vec_uint4 t_sub = spu_and(spu_rlmask(t_t,-22), 0x7c);	//24-2
			vec_uint4 sub_block_pixel = spu_or(s_sub,t_sub);

			vec_uint4 s_blk = spu_and(spu_rlmask(t_s,-29), 0x7);	//24+5
			vec_uint4 t_blk = spu_and(spu_rlmask(t_t,-26), 0x38);	//24+2
			vec_uint4 block_id = spu_add(tex_id_base,spu_or(s_blk,t_blk));

			vec_uchar16 shuf_cmp_0 = (vec_uchar16) spu_splats((unsigned short)0x203);
			vec_ushort8 copy_cmp_0 = (vec_ushort8) spu_shuffle(block_id,block_id,shuf_cmp_0);
			vec_ushort8 matches1_0 = spu_cmpeq(TEXcache1,copy_cmp_0);
			vec_ushort8 matches2_0 = spu_cmpeq(TEXcache2,copy_cmp_0);
			vec_uint4 gather1_0 = spu_gather((vec_uchar16)matches1_0);
			vec_uint4 gather2_0 = spu_gather((vec_uchar16)matches2_0);

			vec_uchar16 shuf_cmp_1 = (vec_uchar16) spu_splats((unsigned short)0x607);
			vec_ushort8 copy_cmp_1 = (vec_ushort8) spu_shuffle(block_id,block_id,shuf_cmp_1);
			vec_ushort8 matches1_1 = spu_cmpeq(TEXcache1,copy_cmp_1);
			vec_ushort8 matches2_1 = spu_cmpeq(TEXcache2,copy_cmp_1);
			vec_uint4 gather1_1 = spu_gather((vec_uchar16)matches1_1);
			vec_uint4 gather2_1 = spu_gather((vec_uchar16)matches2_1);

			vec_uchar16 shuf_cmp_2 = (vec_uchar16) spu_splats((unsigned short)0xa0b);
			vec_ushort8 copy_cmp_2 = (vec_ushort8) spu_shuffle(block_id,block_id,shuf_cmp_2);
			vec_ushort8 matches1_2 = spu_cmpeq(TEXcache1,copy_cmp_2);
			vec_ushort8 matches2_2 = spu_cmpeq(TEXcache2,copy_cmp_2);
			vec_uint4 gather1_2 = spu_gather((vec_uchar16)matches1_2);
			vec_uint4 gather2_2 = spu_gather((vec_uchar16)matches2_2);
		
			vec_uchar16 shuf_cmp_3 = (vec_uchar16) spu_splats((unsigned short)0xe0f);
			vec_ushort8 copy_cmp_3 = (vec_ushort8) spu_shuffle(block_id,block_id,shuf_cmp_3);
			vec_ushort8 matches1_3 = spu_cmpeq(TEXcache1,copy_cmp_3);
			vec_ushort8 matches2_3 = spu_cmpeq(TEXcache2,copy_cmp_3);
			vec_uint4 gather1_3 = spu_gather((vec_uchar16)matches1_3);
			vec_uint4 gather2_3 = spu_gather((vec_uchar16)matches2_3);
		
			vec_uint4 gather_merge= (vec_uint4) spu_splats((unsigned short)0x5555);
		
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
				vec_uint4 s = spu_rlmask(t_s,-29);
				vec_uint4 t = spu_rlmask(t_t,-29);
				return loadMissingTextures(self, block, active, tag,
					A, left, ptr, tex_keep,
					block_id, s, t, cache_not_found, pixel);
			}

			// pixel is mask of 1's where we want to draw
		
			vec_uint4 local_tex_base = spu_splats((unsigned int)&textureCache);
			vec_ushort8 mulconst = spu_splats((unsigned short)((33*32+4*40)*4));
			vec_uint4 tex_ofs = spu_mulo( (vec_ushort8)cache,mulconst);
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

//////////////////////////////////////////////////////////////////////////////

void* linearTextureMapFill(void* self, Block* block, ActiveBlock* active, int tag)
{
	Triangle* tri = block->triangle;
	TextureDefinition* tex_def = tri->texture;

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
	vec_uint4 tex_keep = spu_splats((unsigned int)0);

	const vec_uchar16 shuf_cmp_0 = (vec_uchar16) spu_splats((unsigned short)0x203);
	const vec_uchar16 shuf_cmp_1 = (vec_uchar16) spu_splats((unsigned short)0x607);
	const vec_uchar16 shuf_cmp_2 = (vec_uchar16) spu_splats((unsigned short)0xa0b);
	const vec_uchar16 shuf_cmp_3 = (vec_uchar16) spu_splats((unsigned short)0xe0f);
	const vec_uint4 gather_merge=spu_splats((unsigned int)0x55555555);

	const vec_uchar16 merge_shr8 = (vec_uchar16) {
        	17,18, 1,2,  21,22,5,6,  25,26,9,10,  29,30,13,14};

	const vec_uchar16 splats_01 = (vec_uchar16) {         // low 16 bits, copy alternately
        	6,7, 2,3, 6,7, 2,3, 6,7, 2,3, 6,7, 2,3}; 
	const vec_uchar16 splats_23 = (vec_uchar16) {         // low 16 bits, copy alternately
		14,15,10,11, 14,15,10,11, 14,15,10,11, 14,15,10,11}; 

	const vec_uchar16 rejoin = (vec_uchar16) {
		3,7,11,15,	1,5,9,13,	19,23,27,31,	17,21,25,29};

	const vec_uchar16 get0 = (vec_uchar16) {
        	S_0, 16, S_0, 0, S_0, 17, S_0, 1,
	        S_0, 18, S_0, 2, S_0, 19, S_0, 3};

       const vec_uchar16 get1 = (vec_uchar16) {
               S_0, 20, S_0, 4, S_0, 21, S_0, 5,
               S_0, 22, S_0, 6, S_0, 23, S_0, 7};

       const vec_uchar16 copy_as_is = (vec_uchar16) {
               0,1,2,3, 4,5,6,7, 8,9,10,11, 12,13,14,15};

       const vec_uchar16 extract_add_0 = (vec_uchar16) {
               3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3}; 
       const vec_uchar16 extract_add_1 = (vec_uchar16) {
               7,7,7,7, 7,7,7,7, 7,7,7,7, 7,7,7,7}; 
       const vec_uchar16 extract_add_2 = (vec_uchar16) {
               11,11,11,11, 11,11,11,11, 11,11,11,11, 11,11,11,11}; 
       const vec_uchar16 extract_add_3 = (vec_uchar16) {
               15,15,15,15, 15,15,15,15, 15,15,15,15, 15,15,15,15}; 

	const vec_float4 f1_0 = spu_splats(1.0f);
	const vec_ushort8 tex_ofs_mul = spu_splats((unsigned short)((33*32+4*40)*4));
	const vec_uint4 tex_ofs32_add = spu_splats((unsigned int)(33*32*4));

	const vec_uchar16 merge = (vec_uchar16) {      // high 16 bits->low 16 bits, merge alternately
        	16,17, 0,1,  20,21,4,5,  24,25,8,9,  28,29,12,13};

	const vec_uchar16 merge_lo = (vec_uchar16) {    // low 16 bits, merge alternately
        	18,19, 2,3,  22,23,6,7,  26,27,10,11,  30,31,14,15};

	const vec_uchar16 merge_pixels_01 = (vec_uchar16) {
		1,5,9,13, 17,21,25,29, S_0,S_0,S_0,S_0, S_0,S_0,S_0,S_0};

	const vec_uchar16 merge_pixels_23 = (vec_uchar16) {
		S_0,S_0,S_0,S_0, S_0,S_0,S_0,S_0, 1,5,9,13, 17,21,25,29};

	const vec_uchar16 select_halfword0_as_uint = (vec_uchar16) {
		S_0,S_0,0,1, S_0,S_0,0,1, S_0,S_0,0,1, S_0,S_0,0,1}; 

	const vec_short8 tex_shift_count_add = spu_splats((signed short)(64-32));

	vec_short8 tex_shift_count = tex_def->shifts;

	vec_int4 tex_sblk_shift = (vec_int4) spu_add(tex_shift_count, (short)64-32);
	vec_int4 tex_tblk_shift = spu_rlmask(tex_sblk_shift,-16);
	vec_uint4 tex_tblk_shift_mrg = (vec_uint4)tex_shift_count;

	vec_int4 shift_s_fract = (vec_int4) spu_add(tex_shift_count, (short)64-32+5+8);
	vec_int4 shift_t_fract = spu_rlmask(shift_s_fract,-16);

	const vec_short8 shift_add_sub = (vec_short8) {
		 64-32+5+2, 64-32+5+5+2,
		 64-32+5+2, 64-32+5+5+2,
		 64-32+5+2, 64-32+5+5+2,
		 64-32+5+2, 64-32+5+5+2};

	vec_int4 shift_s_sub = (vec_int4) spu_add(tex_shift_count, shift_add_sub);
	vec_int4 shift_t_sub = spu_rlmask(shift_s_sub, -16);

	vec_int4 shift_s_y = (vec_int4) spu_add(tex_shift_count, (short)64-32+5+4);

	// actually these are always constant, but certainly 0xf80 is too big for andi
	const vec_uint4 mask_s_sub=spu_splats((unsigned int)0xf80);
	const vec_uint4 mask_t_sub=spu_splats((unsigned int)0x7c);

	vec_float4 tex_cover = tri->tex_cover;

	do {
		vec_uint4 uAa = (vec_uint4) Aa;
		vec_uint4 uAb = (vec_uint4) Ab;
		vec_uint4 uAc = (vec_uint4) Ac;
		vec_uint4 allNeg = spu_and(spu_and(uAa,uAb),uAc);
		vec_uint4 pixel = spu_rlmaska(allNeg,-31);
		vec_uint4 bail = spu_orx(pixel);
		if (__builtin_expect(spu_extract(bail,0),0)) {
			vec_float4 t_w = extract(tri->w, Aa, Ab, Ac);
			vec_float4 w = f1_0/t_w;
			vec_float4 tAa = spu_mul(Aa,w);
			vec_float4 tAb = spu_mul(Ab,w);
			vec_float4 tAc = spu_mul(Ac,w);

//PROCESS_BLOCK_HEAD(process_tex_block)

			vec_float4 tf_s = extract(tri->s, tAa, tAb, tAc);
			vec_float4 tf_t = extract(tri->t, tAa, tAb, tAc);

			vec_uint4 t_s = spu_convtu(tf_s,32);
			vec_uint4 t_t = spu_convtu(tf_t,32);

			vec_uint4 block_s = spu_rlmask(t_s,tex_sblk_shift);
			vec_uint4 block_t = spu_rlmask(t_t,tex_tblk_shift);

			vec_uint4 s_blk = block_s;
			vec_uint4 t_blk = spu_sl(block_t,tex_tblk_shift_mrg);
			vec_uint4 block_id = spu_add(t_blk,spu_add(s_blk,tex_id_base));

			vec_ushort8 copy_cmp_0 = (vec_ushort8) spu_shuffle(block_id,block_id,shuf_cmp_0);
			vec_ushort8 matches1_0 = spu_cmpeq(TEXcache1,copy_cmp_0);
			vec_ushort8 matches2_0 = spu_cmpeq(TEXcache2,copy_cmp_0);
			vec_uint4 gather1_0 = spu_gather((vec_uchar16)matches1_0);
			vec_uint4 gather2_0 = spu_gather((vec_uchar16)matches2_0);

			vec_ushort8 copy_cmp_1 = (vec_ushort8) spu_shuffle(block_id,block_id,shuf_cmp_1);
			vec_ushort8 matches1_1 = spu_cmpeq(TEXcache1,copy_cmp_1);
			vec_ushort8 matches2_1 = spu_cmpeq(TEXcache2,copy_cmp_1);
			vec_uint4 gather1_1 = spu_gather((vec_uchar16)matches1_1);
			vec_uint4 gather2_1 = spu_gather((vec_uchar16)matches2_1);

			vec_ushort8 copy_cmp_2 = (vec_ushort8) spu_shuffle(block_id,block_id,shuf_cmp_2);
			vec_ushort8 matches1_2 = spu_cmpeq(TEXcache1,copy_cmp_2);
			vec_ushort8 matches2_2 = spu_cmpeq(TEXcache2,copy_cmp_2);
			vec_uint4 gather1_2 = spu_gather((vec_uchar16)matches1_2);
			vec_uint4 gather2_2 = spu_gather((vec_uchar16)matches2_2);
		
			vec_ushort8 copy_cmp_3 = (vec_ushort8) spu_shuffle(block_id,block_id,shuf_cmp_3);
			vec_ushort8 matches1_3 = spu_cmpeq(TEXcache1,copy_cmp_3);
			vec_ushort8 matches2_3 = spu_cmpeq(TEXcache2,copy_cmp_3);
			vec_uint4 gather1_3 = spu_gather((vec_uchar16)matches1_3);
			vec_uint4 gather2_3 = spu_gather((vec_uchar16)matches2_3);
		
		
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
			if (__builtin_expect(cache_orx,0)) {  // amazingly gcc does move this out of the loop :)
				return loadMissingTextures(self, block, active, tag,
					A, left, ptr, tex_keep,
					block_id, block_s, block_t, cache_not_found, pixel);
			}

			// pixel is mask of 1's where we want to draw
		
			vec_uint4 s_sub = spu_and(spu_rlmask(t_s,shift_s_sub), mask_s_sub);	
			vec_uint4 t_sub = spu_and(spu_rlmask(t_t,shift_t_sub), mask_t_sub);
			vec_uint4 sub_block_pixel = spu_or(s_sub,t_sub);

			vec_uint4 tex_ofs = spu_mulo( (vec_ushort8)cache, tex_ofs_mul);
			vec_uint4 tex_ofs32 = spu_add(tex_ofs, tex_ofs32_add);
			vec_uint4 addr00 = spu_add(tex_ofs,sub_block_pixel);
		
			// (x,y+1) is always the next line in physical memory
			vec_uint4 addr01 = spu_add(addr00, (unsigned int)(32*4));

			// if x<32
			vec_uint4 addr10a = spu_add(addr00, (unsigned int)16);
			vec_uint4 addr11a = spu_add(addr00, (unsigned int)(32*4+16));

			// if x==32
			vec_uint4 sb_sub = spu_and(spu_rlmask(t_s,shift_s_y), 0x1f0);
			vec_uint4 addr10b = spu_add(tex_ofs32,sb_sub);
			vec_uint4 addr11b = spu_add(addr10b, spu_splats((unsigned int)16));

			// decision
			vec_uint4 is_x_32 = spu_cmpeq(t_sub,0x7c);
			vec_uint4 addr10 = spu_sel(addr10a,addr10b,is_x_32);
			vec_uint4 addr11 = spu_sel(addr11a,addr11b,is_x_32);

			unsigned int local_tex_base = (unsigned int)&textureCache;

			vec_uint4 x_shuf_base = spu_and(addr00,(unsigned int)0xc);
			vec_uchar16 x_shuf0 = (vec_uchar16)spu_add( (vec_uint4)copy_as_is,
				spu_shuffle(x_shuf_base,x_shuf_base,extract_add_0));
			vec_uchar16 x_shuf1 = (vec_uchar16)spu_add( (vec_uint4)copy_as_is,
				spu_shuffle(x_shuf_base,x_shuf_base,extract_add_1));
			vec_uchar16 x_shuf2 = (vec_uchar16)spu_add( (vec_uint4)copy_as_is,
				spu_shuffle(x_shuf_base,x_shuf_base,extract_add_2));
			vec_uchar16 x_shuf3 = (vec_uchar16)spu_add( (vec_uint4)copy_as_is,
				spu_shuffle(x_shuf_base,x_shuf_base,extract_add_3));

			vec_uint4 pix0_00 = *((vec_uint4*)(local_tex_base+spu_extract(addr00,0)));
			vec_uint4 pix0_10 = *((vec_uint4*)(local_tex_base+spu_extract(addr10,0)));
			vec_uint4 pix0_01 = *((vec_uint4*)(local_tex_base+spu_extract(addr01,0)));
			vec_uint4 pix0_11 = *((vec_uint4*)(local_tex_base+spu_extract(addr11,0)));
			vec_uint4 pix0_0 = spu_shuffle(pix0_00,pix0_10,x_shuf0);
			vec_uint4 pix0_1 = spu_shuffle(pix0_01,pix0_11,x_shuf0);

			vec_uint4 pix1_00 = *((vec_uint4*)(local_tex_base+spu_extract(addr00,1)));
			vec_uint4 pix1_10 = *((vec_uint4*)(local_tex_base+spu_extract(addr10,1)));
			vec_uint4 pix1_01 = *((vec_uint4*)(local_tex_base+spu_extract(addr01,1)));
			vec_uint4 pix1_11 = *((vec_uint4*)(local_tex_base+spu_extract(addr11,1)));
			vec_uint4 pix1_0 = spu_shuffle(pix1_00,pix1_10,x_shuf1);
			vec_uint4 pix1_1 = spu_shuffle(pix1_01,pix1_11,x_shuf1);
			
			vec_uint4 pix2_00 = *((vec_uint4*)(local_tex_base+spu_extract(addr00,2)));
			vec_uint4 pix2_10 = *((vec_uint4*)(local_tex_base+spu_extract(addr10,2)));
			vec_uint4 pix2_01 = *((vec_uint4*)(local_tex_base+spu_extract(addr01,2)));
			vec_uint4 pix2_11 = *((vec_uint4*)(local_tex_base+spu_extract(addr11,2)));
			vec_uint4 pix2_0 = spu_shuffle(pix2_00,pix2_10,x_shuf2);
			vec_uint4 pix2_1 = spu_shuffle(pix2_01,pix2_11,x_shuf2);
			
			vec_uint4 pix3_00 = *((vec_uint4*)(local_tex_base+spu_extract(addr00,3)));
			vec_uint4 pix3_10 = *((vec_uint4*)(local_tex_base+spu_extract(addr10,3)));
			vec_uint4 pix3_01 = *((vec_uint4*)(local_tex_base+spu_extract(addr01,3)));
			vec_uint4 pix3_11 = *((vec_uint4*)(local_tex_base+spu_extract(addr11,3)));
			vec_uint4 pix3_0 = spu_shuffle(pix3_00,pix3_10,x_shuf3);
			vec_uint4 pix3_1 = spu_shuffle(pix3_01,pix3_11,x_shuf3);

			const vec_uint4 ff_mask = spu_splats((unsigned int)0xff);
			vec_uint4 s_pxofs = spu_and(spu_rlmask(t_s,shift_s_fract), ff_mask);
			vec_uint4 t_pxofs = spu_and(spu_rlmask(t_t,shift_t_fract), ff_mask);

			vec_short8 pixel01_ = (vec_short8) spu_shuffle((vec_uint4)pix0_0, (vec_uint4)pix1_0, get0);
			vec_short8 pixel01_Y = (vec_short8) spu_shuffle((vec_uint4)pix0_1, (vec_uint4)pix1_1, get0);
			vec_short8 pixel01_XY = (vec_short8) spu_shuffle((vec_uint4)pix0_1, (vec_uint4)pix1_1, get1);
			vec_short8 pixel01_X = (vec_short8) spu_shuffle((vec_uint4)pix0_0, (vec_uint4)pix1_0, get1);
			vec_short8 pixel01_h = spu_sub(pixel01_Y,pixel01_);
			vec_short8 pixel01_Xh = spu_sub(pixel01_XY,pixel01_X);

			vec_short8 pixel01_multx = (vec_short8)spu_shuffle(s_pxofs,s_pxofs,splats_01);
			vec_short8 pixel01_multy = (vec_short8)spu_shuffle(t_pxofs,t_pxofs,splats_01);

			vec_int4 pixel0_th = spu_mulo(pixel01_h, pixel01_multx);
			vec_int4 pixel0_bh = spu_mulo(pixel01_Xh, pixel01_multx);
			vec_int4 pixel1_th = spu_mule(pixel01_h, pixel01_multx);
			vec_int4 pixel1_bh = spu_mule(pixel01_Xh, pixel01_multx);
			vec_short8 pixel01_th = (vec_short8)spu_shuffle(pixel0_th, pixel1_th, merge_shr8);
			vec_short8 pixel01_bh = (vec_short8)spu_shuffle(pixel0_bh, pixel1_bh, merge_shr8);

			vec_short8 pixel01_tm = spu_add(pixel01_, pixel01_th);
			vec_short8 pixel01_bm = spu_add(pixel01_X, pixel01_bh);

			vec_short8 pixel01_d = spu_sub(pixel01_bm,pixel01_tm);

			vec_int4 pixel0_d = spu_mulo(pixel01_d, pixel01_multy);
			vec_int4 pixel1_d = spu_mule(pixel01_d, pixel01_multy);
			vec_short8 pixel01_mm = (vec_short8)spu_shuffle(pixel0_d, pixel1_d, merge_shr8);
			vec_short8 pixel01_done = spu_add(pixel01_mm, pixel01_tm);

///////////
			vec_short8 pixel23_ = (vec_short8) spu_shuffle((vec_uint4)pix2_0, (vec_uint4)pix3_0, get0);
			vec_short8 pixel23_Y = (vec_short8) spu_shuffle((vec_uint4)pix2_1, (vec_uint4)pix3_1, get0);
			vec_short8 pixel23_XY = (vec_short8) spu_shuffle((vec_uint4)pix2_1, (vec_uint4)pix3_1, get1);
			vec_short8 pixel23_X = (vec_short8) spu_shuffle((vec_uint4)pix2_0, (vec_uint4)pix3_0, get1);
			vec_short8 pixel23_h = spu_sub(pixel23_Y,pixel23_);
			vec_short8 pixel23_Xh = spu_sub(pixel23_XY,pixel23_X);

			vec_short8 pixel23_multx = (vec_short8)spu_shuffle(s_pxofs,s_pxofs,splats_23);
			vec_short8 pixel23_multy = (vec_short8)spu_shuffle(t_pxofs,t_pxofs,splats_23);

			vec_int4 pixel2_th = spu_mulo(pixel23_h, pixel23_multx);
			vec_int4 pixel2_bh = spu_mulo(pixel23_Xh, pixel23_multx);
			vec_int4 pixel3_th = spu_mule(pixel23_h, pixel23_multx);
			vec_int4 pixel3_bh = spu_mule(pixel23_Xh, pixel23_multx);
			vec_short8 pixel23_th = (vec_short8)spu_shuffle(pixel2_th, pixel3_th, merge_shr8);
			vec_short8 pixel23_bh = (vec_short8)spu_shuffle(pixel2_bh, pixel3_bh, merge_shr8);

			vec_short8 pixel23_tm = spu_add(pixel23_, pixel23_th);
			vec_short8 pixel23_bm = spu_add(pixel23_X, pixel23_bh);

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

//////////////////////////////////////////////////////////////////////////////

void* lessMulsLinearTextureMapFill(void* self, Block* block, ActiveBlock* active, int tag)
{
	Triangle* tri = block->triangle;
	TextureDefinition* tex_def = tri->texture;

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

	vec_float4 A_rdy = tri->A_dy;
	vec_float4 Aa_rdy = spu_splats(spu_extract(A_rdy,0));
	vec_float4 Ab_rdy = spu_splats(spu_extract(A_rdy,1));
	vec_float4 Ac_rdy = spu_splats(spu_extract(A_rdy,2));

	vec_float4 A_rdx = tri->A_dx;
	vec_float4 Aa_rdx = spu_splats(spu_extract(A_rdx,0));
	vec_float4 Ab_rdx = spu_splats(spu_extract(A_rdx,1));
	vec_float4 Ac_rdx = spu_splats(spu_extract(A_rdx,2));

	vec_float4 A = block->A;
	vec_float4 Aa = spu_madd(muls,Aa_dx,spu_splats(spu_extract(A,0)));
	vec_float4 Ab = spu_madd(muls,Ab_dx,spu_splats(spu_extract(A,1)));
	vec_float4 Ac = spu_madd(muls,Ac_dx,spu_splats(spu_extract(A,2)));

	vec_uint4 left = spu_splats(block->left);
	vec_uint4* ptr = block->pixels;
	vec_uint4 tex_id_base = spu_splats((unsigned int)tri->tex_id_base);
	vec_uint4 tex_keep = spu_splats((unsigned int)0);

	const vec_uchar16 shuf_cmp_0 = (vec_uchar16) spu_splats((unsigned short)0x203);
	const vec_uchar16 shuf_cmp_1 = (vec_uchar16) spu_splats((unsigned short)0x607);
	const vec_uchar16 shuf_cmp_2 = (vec_uchar16) spu_splats((unsigned short)0xa0b);
	const vec_uchar16 shuf_cmp_3 = (vec_uchar16) spu_splats((unsigned short)0xe0f);
	const vec_uint4 gather_merge=spu_splats((unsigned int)0x55555555);

	const vec_uchar16 merge_shr8 = (vec_uchar16) {
        	17,18, 1,2,  21,22,5,6,  25,26,9,10,  29,30,13,14};

	const vec_uchar16 splats_01 = (vec_uchar16) {         // low 16 bits, copy alternately
        	6,7, 2,3, 6,7, 2,3, 6,7, 2,3, 6,7, 2,3}; 
	const vec_uchar16 splats_23 = (vec_uchar16) {         // low 16 bits, copy alternately
		14,15,10,11, 14,15,10,11, 14,15,10,11, 14,15,10,11}; 

	const vec_uchar16 rejoin = (vec_uchar16) {
		3,7,11,15,	1,5,9,13,	19,23,27,31,	17,21,25,29};

	const vec_uchar16 get0 = (vec_uchar16) {
        	S_0, 16, S_0, 0, S_0, 17, S_0, 1,
	        S_0, 18, S_0, 2, S_0, 19, S_0, 3};

       const vec_uchar16 get1 = (vec_uchar16) {
               S_0, 20, S_0, 4, S_0, 21, S_0, 5,
               S_0, 22, S_0, 6, S_0, 23, S_0, 7};

       const vec_uchar16 copy_as_is = (vec_uchar16) {
               0,1,2,3, 4,5,6,7, 8,9,10,11, 12,13,14,15};

       const vec_uchar16 extract_add_0 = (vec_uchar16) {
               3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3}; 
       const vec_uchar16 extract_add_1 = (vec_uchar16) {
               7,7,7,7, 7,7,7,7, 7,7,7,7, 7,7,7,7}; 
       const vec_uchar16 extract_add_2 = (vec_uchar16) {
               11,11,11,11, 11,11,11,11, 11,11,11,11, 11,11,11,11}; 
       const vec_uchar16 extract_add_3 = (vec_uchar16) {
               15,15,15,15, 15,15,15,15, 15,15,15,15, 15,15,15,15}; 

	const vec_float4 f1_0 = spu_splats(1.0f);
	const vec_ushort8 tex_ofs_mul = spu_splats((unsigned short)((33*32+4*40)*4));
	const vec_uint4 tex_ofs32_add = spu_splats((unsigned int)(33*32*4));

	const vec_uchar16 merge = (vec_uchar16) {      // high 16 bits->low 16 bits, merge alternately
        	16,17, 0,1,  20,21,4,5,  24,25,8,9,  28,29,12,13};

	const vec_uchar16 merge_lo = (vec_uchar16) {    // low 16 bits, merge alternately
        	18,19, 2,3,  22,23,6,7,  26,27,10,11,  30,31,14,15};

	const vec_uchar16 merge_pixels_01 = (vec_uchar16) {
		1,5,9,13, 17,21,25,29, S_0,S_0,S_0,S_0, S_0,S_0,S_0,S_0};

	const vec_uchar16 merge_pixels_23 = (vec_uchar16) {
		S_0,S_0,S_0,S_0, S_0,S_0,S_0,S_0, 1,5,9,13, 17,21,25,29};

	const vec_uchar16 select_halfword0_as_uint = (vec_uchar16) {
		S_0,S_0,0,1, S_0,S_0,0,1, S_0,S_0,0,1, S_0,S_0,0,1}; 

	const vec_short8 tex_shift_count_add = spu_splats((signed short)(64-32));

	vec_short8 tex_shift_count = tex_def->shifts;

	vec_int4 tex_sblk_shift = (vec_int4) spu_add(tex_shift_count, (short)64-32);
	vec_int4 tex_tblk_shift = spu_rlmask(tex_sblk_shift,-16);
	vec_uint4 tex_tblk_shift_mrg = (vec_uint4)tex_shift_count;

	vec_int4 shift_s_fract = (vec_int4) spu_add(tex_shift_count, (short)64-32+5+8);
	vec_int4 shift_t_fract = spu_rlmask(shift_s_fract,-16);

	const vec_short8 shift_add_sub = (vec_short8) {
		 64-32+5+2, 64-32+5+5+2,
		 64-32+5+2, 64-32+5+5+2,
		 64-32+5+2, 64-32+5+5+2,
		 64-32+5+2, 64-32+5+5+2};

	vec_int4 shift_s_sub = (vec_int4) spu_add(tex_shift_count, shift_add_sub);
	vec_int4 shift_t_sub = spu_rlmask(shift_s_sub, -16);

	vec_int4 shift_s_y = (vec_int4) spu_add(tex_shift_count, (short)64-32+5+4);

	// actually these are always constant, but certainly 0xf80 is too big for andi
	const vec_uint4 mask_s_sub=spu_splats((unsigned int)0xf80);
	const vec_uint4 mask_t_sub=spu_splats((unsigned int)0x7c);

	vec_float4 tex_cover = tri->tex_cover;

	vec_float4 wA = extract(tri->w, Aa, Ab, Ac);
	vec_float4 wA_dx4 = extract(tri->w, Aa_dx4, Ab_dx4, Ac_dx4);
	vec_float4 wA_dy = extract(tri->w, Aa_dy, Ab_dy, Ac_dy);
	vec_float4 wA_rdx = extract(tri->w, Aa_rdx, Ab_rdx, Ac_rdx);
	vec_float4 wA_rdy = extract(tri->w, Aa_rdy, Ab_rdy, Ac_rdy);

	vec_float4 sA = extract(tri->s, Aa, Ab, Ac);
	vec_float4 sA_dx4 = extract(tri->s, Aa_dx4, Ab_dx4, Ac_dx4);
	vec_float4 sA_dy = extract(tri->s, Aa_dy, Ab_dy, Ac_dy);
	vec_float4 sA_rdx = extract(tri->s, Aa_rdx, Ab_rdx, Ac_rdx);
	vec_float4 sA_rdy = extract(tri->s, Aa_rdy, Ab_rdy, Ac_rdy);

	vec_float4 tA = extract(tri->t, Aa, Ab, Ac);
	vec_float4 tA_dx4 = extract(tri->t, Aa_dx4, Ab_dx4, Ac_dx4);
	vec_float4 tA_dy = extract(tri->t, Aa_dy, Ab_dy, Ac_dy);
	vec_float4 tA_rdx = extract(tri->t, Aa_rdx, Ab_rdx, Ac_rdx);
	vec_float4 tA_rdy = extract(tri->t, Aa_rdy, Ab_rdy, Ac_rdy);

	// vec_int4 adj_a = si_xshw(tex_shift_count);
	// vec_int4 adj_b = spu_rlmask( (vec_uint4)tex_shift_count, -16);
	// vec_int4 adjust = spu_add(adj_a, adj_b);
	vec_int4 adjust = tex_def->mipmapshifts;

	static vec_int4 ll = {-1,-1,-1,-1}; //spu_splats((unsigned int)0x123456);
	do {
		vec_uint4 uAa = (vec_uint4) Aa;
		vec_uint4 uAb = (vec_uint4) Ab;
		vec_uint4 uAc = (vec_uint4) Ac;
		vec_uint4 allNeg = spu_and(spu_and(uAa,uAb),uAc);
		vec_uint4 pixel = spu_rlmaska(allNeg,-31);
		vec_uint4 bail = spu_orx(pixel);
		if (__builtin_expect(spu_extract(bail,0),0)) {
			vec_float4 w = f1_0/wA;

// #define WWWW

//PROCESS_BLOCK_HEAD(process_tex_block)

			vec_float4 k =  sA_rdx*tA_rdy*wA + sA*tA_rdx*wA_rdy + sA_rdy*tA*wA_rdx
				      - sA_rdy*tA_rdx*wA - sA*tA_rdy*wA_rdx - sA_rdx*tA*wA_rdy;
			vec_float4 j = k*w*w*w;
			vec_int4 l = log2_sqrt_clamp(j, adjust);

#ifdef WWWW
			vec_uint4 t = spu_cmpeq(l,ll);
			//if (spu_extract(spu_orx(spu_nor(t,t)),0) != 0) {
//			if (spu_extract(t,0) == 0) {
//				printf("%f->%d %f->%d %f->%d %f->%d\n",
//					spu_extract(j,0), spu_extract(l,0), 
//					spu_extract(j,1), spu_extract(l,1),
//					spu_extract(j,2), spu_extract(l,2),
//					spu_extract(j,3), spu_extract(l,3));
				printf("%d\n%d\n%d\n%d\n",
					spu_extract(l,0), 
					spu_extract(l,1),
					spu_extract(l,2),
					spu_extract(l,3));
//				ll = l;
//			}
#endif

			vec_uint4 pattern = 
				spu_or(spu_and(spu_cmpeq(spu_and(l,1),1),0xff0000),
					spu_or(spu_and(spu_cmpeq(spu_and(l,2),2),0xff00),
					       spu_and(spu_cmpeq(spu_and(l,4),4),0xff)));

#ifdef MIP_COLOURS
	colour = spu_xor(colour,
			spu_or(spu_or(spu_sl(spu_and(l,1),7),
			       spu_sl(spu_and(l,2),14)),
			       spu_sl(spu_and(l,4),21)));
#endif


			vec_float4 tf_s = spu_mul(sA, w);
			vec_float4 tf_t = spu_mul(tA, w);

			vec_uint4 t_s = spu_convtu(tf_s,32);
			vec_uint4 t_t = spu_convtu(tf_t,32);

			vec_uint4 block_s = spu_rlmask(t_s,tex_sblk_shift);
			vec_uint4 block_t = spu_rlmask(t_t,tex_tblk_shift);

			vec_uint4 s_blk = block_s;
			vec_uint4 t_blk = spu_sl(block_t,tex_tblk_shift_mrg);
			vec_uint4 block_id = spu_add(t_blk,spu_add(s_blk,tex_id_base));

			vec_ushort8 copy_cmp_0 = (vec_ushort8) spu_shuffle(block_id,block_id,shuf_cmp_0);
			vec_ushort8 matches1_0 = spu_cmpeq(TEXcache1,copy_cmp_0);
			vec_ushort8 matches2_0 = spu_cmpeq(TEXcache2,copy_cmp_0);
			vec_uint4 gather1_0 = spu_gather((vec_uchar16)matches1_0);
			vec_uint4 gather2_0 = spu_gather((vec_uchar16)matches2_0);

			vec_ushort8 copy_cmp_1 = (vec_ushort8) spu_shuffle(block_id,block_id,shuf_cmp_1);
			vec_ushort8 matches1_1 = spu_cmpeq(TEXcache1,copy_cmp_1);
			vec_ushort8 matches2_1 = spu_cmpeq(TEXcache2,copy_cmp_1);
			vec_uint4 gather1_1 = spu_gather((vec_uchar16)matches1_1);
			vec_uint4 gather2_1 = spu_gather((vec_uchar16)matches2_1);

			vec_ushort8 copy_cmp_2 = (vec_ushort8) spu_shuffle(block_id,block_id,shuf_cmp_2);
			vec_ushort8 matches1_2 = spu_cmpeq(TEXcache1,copy_cmp_2);
			vec_ushort8 matches2_2 = spu_cmpeq(TEXcache2,copy_cmp_2);
			vec_uint4 gather1_2 = spu_gather((vec_uchar16)matches1_2);
			vec_uint4 gather2_2 = spu_gather((vec_uchar16)matches2_2);
		
			vec_ushort8 copy_cmp_3 = (vec_ushort8) spu_shuffle(block_id,block_id,shuf_cmp_3);
			vec_ushort8 matches1_3 = spu_cmpeq(TEXcache1,copy_cmp_3);
			vec_ushort8 matches2_3 = spu_cmpeq(TEXcache2,copy_cmp_3);
			vec_uint4 gather1_3 = spu_gather((vec_uchar16)matches1_3);
			vec_uint4 gather2_3 = spu_gather((vec_uchar16)matches2_3);
		
		
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
			if (__builtin_expect(cache_orx,0)) {  // amazingly gcc does move this out of the loop :)
				return loadMissingTextures(self, block, active, tag,
					A, left, ptr, tex_keep,
					block_id, block_s, block_t, cache_not_found, pixel);
			}

			// pixel is mask of 1's where we want to draw
		
			vec_uint4 s_sub = spu_and(spu_rlmask(t_s,shift_s_sub), mask_s_sub);	
			vec_uint4 t_sub = spu_and(spu_rlmask(t_t,shift_t_sub), mask_t_sub);
			vec_uint4 sub_block_pixel = spu_or(s_sub,t_sub);

			vec_uint4 tex_ofs = spu_mulo( (vec_ushort8)cache, tex_ofs_mul);
			vec_uint4 tex_ofs32 = spu_add(tex_ofs, tex_ofs32_add);
			vec_uint4 addr00 = spu_add(tex_ofs,sub_block_pixel);
		
			// (x,y+1) is always the next line in physical memory
			vec_uint4 addr01 = spu_add(addr00, (unsigned int)(32*4));

			// if x<32
			vec_uint4 addr10a = spu_add(addr00, (unsigned int)16);
			vec_uint4 addr11a = spu_add(addr00, (unsigned int)(32*4+16));

			// if x==32
			vec_uint4 sb_sub = spu_and(spu_rlmask(t_s,shift_s_y), 0x1f0);
			vec_uint4 addr10b = spu_add(tex_ofs32,sb_sub);
			vec_uint4 addr11b = spu_add(addr10b, spu_splats((unsigned int)16));

			// decision
			vec_uint4 is_x_32 = spu_cmpeq(t_sub,0x7c);
			vec_uint4 addr10 = spu_sel(addr10a,addr10b,is_x_32);
			vec_uint4 addr11 = spu_sel(addr11a,addr11b,is_x_32);

			unsigned int local_tex_base = (unsigned int)&textureCache;

			vec_uint4 x_shuf_base = spu_and(addr00,(unsigned int)0xc);
			vec_uchar16 x_shuf0 = (vec_uchar16)spu_add( (vec_uint4)copy_as_is,
				spu_shuffle(x_shuf_base,x_shuf_base,extract_add_0));
			vec_uchar16 x_shuf1 = (vec_uchar16)spu_add( (vec_uint4)copy_as_is,
				spu_shuffle(x_shuf_base,x_shuf_base,extract_add_1));
			vec_uchar16 x_shuf2 = (vec_uchar16)spu_add( (vec_uint4)copy_as_is,
				spu_shuffle(x_shuf_base,x_shuf_base,extract_add_2));
			vec_uchar16 x_shuf3 = (vec_uchar16)spu_add( (vec_uint4)copy_as_is,
				spu_shuffle(x_shuf_base,x_shuf_base,extract_add_3));

			vec_uint4 pix0_00 = *((vec_uint4*)(local_tex_base+spu_extract(addr00,0)));
			vec_uint4 pix0_10 = *((vec_uint4*)(local_tex_base+spu_extract(addr10,0)));
			vec_uint4 pix0_01 = *((vec_uint4*)(local_tex_base+spu_extract(addr01,0)));
			vec_uint4 pix0_11 = *((vec_uint4*)(local_tex_base+spu_extract(addr11,0)));
			vec_uint4 pix0_0 = spu_shuffle(pix0_00,pix0_10,x_shuf0);
			vec_uint4 pix0_1 = spu_shuffle(pix0_01,pix0_11,x_shuf0);

			vec_uint4 pix1_00 = *((vec_uint4*)(local_tex_base+spu_extract(addr00,1)));
			vec_uint4 pix1_10 = *((vec_uint4*)(local_tex_base+spu_extract(addr10,1)));
			vec_uint4 pix1_01 = *((vec_uint4*)(local_tex_base+spu_extract(addr01,1)));
			vec_uint4 pix1_11 = *((vec_uint4*)(local_tex_base+spu_extract(addr11,1)));
			vec_uint4 pix1_0 = spu_shuffle(pix1_00,pix1_10,x_shuf1);
			vec_uint4 pix1_1 = spu_shuffle(pix1_01,pix1_11,x_shuf1);
			
			vec_uint4 pix2_00 = *((vec_uint4*)(local_tex_base+spu_extract(addr00,2)));
			vec_uint4 pix2_10 = *((vec_uint4*)(local_tex_base+spu_extract(addr10,2)));
			vec_uint4 pix2_01 = *((vec_uint4*)(local_tex_base+spu_extract(addr01,2)));
			vec_uint4 pix2_11 = *((vec_uint4*)(local_tex_base+spu_extract(addr11,2)));
			vec_uint4 pix2_0 = spu_shuffle(pix2_00,pix2_10,x_shuf2);
			vec_uint4 pix2_1 = spu_shuffle(pix2_01,pix2_11,x_shuf2);
			
			vec_uint4 pix3_00 = *((vec_uint4*)(local_tex_base+spu_extract(addr00,3)));
			vec_uint4 pix3_10 = *((vec_uint4*)(local_tex_base+spu_extract(addr10,3)));
			vec_uint4 pix3_01 = *((vec_uint4*)(local_tex_base+spu_extract(addr01,3)));
			vec_uint4 pix3_11 = *((vec_uint4*)(local_tex_base+spu_extract(addr11,3)));
			vec_uint4 pix3_0 = spu_shuffle(pix3_00,pix3_10,x_shuf3);
			vec_uint4 pix3_1 = spu_shuffle(pix3_01,pix3_11,x_shuf3);

			const vec_uint4 ff_mask = spu_splats((unsigned int)0xff);
			vec_uint4 s_pxofs = spu_and(spu_rlmask(t_s,shift_s_fract), ff_mask);
			vec_uint4 t_pxofs = spu_and(spu_rlmask(t_t,shift_t_fract), ff_mask);

			vec_short8 pixel01_ = (vec_short8) spu_shuffle((vec_uint4)pix0_0, (vec_uint4)pix1_0, get0);
			vec_short8 pixel01_Y = (vec_short8) spu_shuffle((vec_uint4)pix0_1, (vec_uint4)pix1_1, get0);
			vec_short8 pixel01_XY = (vec_short8) spu_shuffle((vec_uint4)pix0_1, (vec_uint4)pix1_1, get1);
			vec_short8 pixel01_X = (vec_short8) spu_shuffle((vec_uint4)pix0_0, (vec_uint4)pix1_0, get1);
			vec_short8 pixel01_h = spu_sub(pixel01_Y,pixel01_);
			vec_short8 pixel01_Xh = spu_sub(pixel01_XY,pixel01_X);

			vec_short8 pixel01_multx = (vec_short8)spu_shuffle(s_pxofs,s_pxofs,splats_01);
			vec_short8 pixel01_multy = (vec_short8)spu_shuffle(t_pxofs,t_pxofs,splats_01);

			vec_int4 pixel0_th = spu_mulo(pixel01_h, pixel01_multx);
			vec_int4 pixel0_bh = spu_mulo(pixel01_Xh, pixel01_multx);
			vec_int4 pixel1_th = spu_mule(pixel01_h, pixel01_multx);
			vec_int4 pixel1_bh = spu_mule(pixel01_Xh, pixel01_multx);
			vec_short8 pixel01_th = (vec_short8)spu_shuffle(pixel0_th, pixel1_th, merge_shr8);
			vec_short8 pixel01_bh = (vec_short8)spu_shuffle(pixel0_bh, pixel1_bh, merge_shr8);

			vec_short8 pixel01_tm = spu_add(pixel01_, pixel01_th);
			vec_short8 pixel01_bm = spu_add(pixel01_X, pixel01_bh);

			vec_short8 pixel01_d = spu_sub(pixel01_bm,pixel01_tm);

			vec_int4 pixel0_d = spu_mulo(pixel01_d, pixel01_multy);
			vec_int4 pixel1_d = spu_mule(pixel01_d, pixel01_multy);
			vec_short8 pixel01_mm = (vec_short8)spu_shuffle(pixel0_d, pixel1_d, merge_shr8);
			vec_short8 pixel01_done = spu_add(pixel01_mm, pixel01_tm);

///////////
			vec_short8 pixel23_ = (vec_short8) spu_shuffle((vec_uint4)pix2_0, (vec_uint4)pix3_0, get0);
			vec_short8 pixel23_Y = (vec_short8) spu_shuffle((vec_uint4)pix2_1, (vec_uint4)pix3_1, get0);
			vec_short8 pixel23_XY = (vec_short8) spu_shuffle((vec_uint4)pix2_1, (vec_uint4)pix3_1, get1);
			vec_short8 pixel23_X = (vec_short8) spu_shuffle((vec_uint4)pix2_0, (vec_uint4)pix3_0, get1);
			vec_short8 pixel23_h = spu_sub(pixel23_Y,pixel23_);
			vec_short8 pixel23_Xh = spu_sub(pixel23_XY,pixel23_X);

			vec_short8 pixel23_multx = (vec_short8)spu_shuffle(s_pxofs,s_pxofs,splats_23);
			vec_short8 pixel23_multy = (vec_short8)spu_shuffle(t_pxofs,t_pxofs,splats_23);

			vec_int4 pixel2_th = spu_mulo(pixel23_h, pixel23_multx);
			vec_int4 pixel2_bh = spu_mulo(pixel23_Xh, pixel23_multx);
			vec_int4 pixel3_th = spu_mule(pixel23_h, pixel23_multx);
			vec_int4 pixel3_bh = spu_mule(pixel23_Xh, pixel23_multx);
			vec_short8 pixel23_th = (vec_short8)spu_shuffle(pixel2_th, pixel3_th, merge_shr8);
			vec_short8 pixel23_bh = (vec_short8)spu_shuffle(pixel2_bh, pixel3_bh, merge_shr8);

			vec_short8 pixel23_tm = spu_add(pixel23_, pixel23_th);
			vec_short8 pixel23_bm = spu_add(pixel23_X, pixel23_bh);

			vec_short8 pixel23_d = spu_sub(pixel23_bm,pixel23_tm);
			vec_int4 pixel2_d = spu_mulo(pixel23_d, pixel23_multy);
			vec_int4 pixel3_d = spu_mule(pixel23_d, pixel23_multy);
			vec_short8 pixel23_mm = (vec_short8)spu_shuffle(pixel2_d, pixel3_d, merge_shr8);
			vec_short8 pixel23_done = spu_add(pixel23_mm, pixel23_tm);

///////////
			vec_uint4 colour = (vec_uint4) spu_shuffle(pixel01_done,pixel23_done,rejoin);

	colour = spu_xor(colour, pattern);

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

		wA += spu_sel(wA_dx4,wA_dy,sel);
		sA += spu_sel(sA_dx4,sA_dy,sel);
		tA += spu_sel(tA_dx4,tA_dy,sel);
	} while (spu_extract(left,0)>0);

	block->triangle->count--;
	return 0;

}
