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
// This file holds the current POC shader; the idea is that a shader processes
// a block of 32x32 pixels at once, ultimately using a shader function that will
// be generated from shader language. At the moment, I'm still testing ideas, but
// eventually, I want to have chunks of logic for things like texture lookup and
// the main loop and chain these together using "bi" (obviously trying to ensure
// we don't go overboard and stall the pipeline)
// At the heart of this function, is a chunk of code that processes
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

#define S_0 128
#define S_80 0xe0

//////////////////////////////////////////////////////////////////////////////

static const vec_uchar16 shuf_gath_01 = {
	2,3,128,128, 18,19,128,128, SEL_00 SEL_00};

static const vec_uchar16 shuf_gath_23 = {
	SEL_00 SEL_00 2,3,128,128, 18,19,128,128,};

static const vec_uchar16 merge_tex_base = {
       	S_80, S_80, 0x3, 0x13, S_80, S_80, 0x7, 0x17,
       	S_80, S_80, 0xb, 0x1b, S_80, S_80, 0xf, 0x1f};	// 0x80 -> 0 on subsequent shuffle

extern void* textureCache;
extern void* loadMissingTextures(void* self, Block* block, ActiveBlock* active, int tag,
			vec_float4 A, vec_uint4 left, vec_uint4* ptr, vec_uint4 tex_keep,
			vec_int4 mipmap, vec_uint4 block_id, vec_uint4 s, vec_uint4 t, 
			vec_uint4 cache_not_found, vec_uint4 pixel);

//////////////////////////////////////////////////////////////////////////////
//
// this is the current best example, does GL_LINEAR with full mipmapping
// support. still need to add alpha blending support to final write

void* lessMulsLinearTextureMapFill(void* self, Block* block, ActiveBlock* active, int tag)
{
	Triangle* tri = block->triangle;
	TextureDefinition* tex_def = tri->texture;

	// the following are all wasteful as all elements are the same...
	// also, i'm aiming to move all the barycentric weightings to integers to prevent
	// tearing, which currently happens very rarely

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
	vec_uint4 tex_keep = spu_splats((unsigned int)0);

	vec_uchar16 tex_base_lo = tex_def->tex_base_lo;
	vec_uchar16 tex_base_hi = tex_def->tex_base_hi;

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

	vec_int4 adjust = tex_def->mipmapshifts;
	vec_uint4 mip_block_shift_tmp = (vec_uint4) tex_shift_count;
	vec_uint4 mip_block_shift = spu_add(mip_block_shift_tmp, spu_rlmask(mip_block_shift_tmp, -16));
	vec_int4 max_mipmap = spu_splats((int)tex_def->tex_max_mipmap);

	// generate the mipmap start and deltas
	vec_float4 mip_wA_mult = sA_rdx*tA_rdy - sA_rdy*tA_rdx;
	vec_float4 mip_sA_mult = tA_rdx*wA_rdy - tA_rdy*wA_rdx;
	vec_float4 mip_tA_mult = wA_rdx*sA_rdy - wA_rdy*sA_rdx;

	vec_float4 k     = mip_wA_mult*wA     + mip_sA_mult*sA     + mip_tA_mult*tA;
	vec_float4 k_dx4 = mip_wA_mult*wA_dx4 + mip_sA_mult*sA_dx4 + mip_tA_mult*tA_dx4;
	vec_float4 k_dy  = mip_wA_mult*wA_dy  + mip_sA_mult*sA_dy  + mip_tA_mult*tA_dy;

	do {
		vec_uint4 uAa = (vec_uint4) Aa;
		vec_uint4 uAb = (vec_uint4) Ab;
		vec_uint4 uAc = (vec_uint4) Ac;
		vec_uint4 allNeg = spu_and(spu_and(uAa,uAb),uAc);
		vec_uint4 pixel = spu_rlmaska(allNeg,-31);
		vec_uint4 bail = spu_orx(pixel);
		if (__builtin_expect(spu_extract(bail,0),0)) {
			vec_float4 w = f1_0/wA;

//PROCESS_BLOCK_HEAD(process_tex_block)
			vec_int4 mipmap_real = log2_sqrt_clamp(k*w*w*w, adjust);			// determine mipmap level
			vec_uint4 mipmap_clamp_sel = spu_cmpgt(max_mipmap,mipmap_real);
			vec_int4 mipmap = spu_sel(max_mipmap,mipmap_real,mipmap_clamp_sel);		// clamp mipmap between 0..max_mipmap

			vec_float4 tf_s = spu_mul(sA, w);
			vec_float4 tf_t = spu_mul(tA, w);

			vec_uint4 t_s = spu_convtu(tf_s,32);
			vec_uint4 t_t = spu_convtu(tf_t,32);

			vec_uint4 block_s = spu_rlmask(t_s,spu_sub(tex_sblk_shift,mipmap));
			vec_uint4 block_t = spu_rlmask(t_t,spu_sub(tex_tblk_shift,mipmap));

			vec_uint4 s_blk = block_s;
			vec_uint4 t_blk = spu_sl(block_t,spu_sub(tex_tblk_shift_mrg,(vec_uint4)mipmap));

			// new improved way of determining block id
			vec_uchar16 base_shuffle = spu_shuffle(mipmap,spu_or(mipmap,0x10),merge_tex_base);
			vec_uint4 base_id = (vec_uint4) spu_shuffle(tex_base_hi, tex_base_lo, base_shuffle);
			vec_uint4 block_id = spu_add(t_blk,spu_add(s_blk,base_id));

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
			vec_uint4 cache_not_found = spu_cmpeq(cache,spu_splats((unsigned int)32));
			unsigned int cache_orx = spu_extract(spu_orx(cache_not_found),0);
			if (__builtin_expect(cache_orx,0)) {  // amazingly gcc does move this out of the loop :)
				return loadMissingTextures(self, block, active, tag,
					A, left, ptr, tex_keep, mipmap,
					block_id, block_s, block_t, cache_not_found, pixel);
			}

			// pixel is mask of 1's where we want to draw
		
			vec_uint4 s_sub = spu_and(spu_rlmask(t_s,spu_sub(shift_s_sub,mipmap)), mask_s_sub);	
			vec_uint4 t_sub = spu_and(spu_rlmask(t_t,spu_sub(shift_t_sub,mipmap)), mask_t_sub);
			vec_uint4 sub_block_pixel = spu_or(s_sub,t_sub);

			vec_uint4 tex_ofs = spu_mulo( (vec_ushort8)cache, tex_ofs_mul);
			vec_uint4 tex_ofs32 = spu_add(tex_ofs, tex_ofs32_add);
			vec_uint4 addr00 = spu_add(tex_ofs,sub_block_pixel);
		
			// (x,y+2) is always the next line in physical memory
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

		wA += spu_sel(wA_dx4,wA_dy,sel);
		sA += spu_sel(sA_dx4,sA_dy,sel);
		tA += spu_sel(tA_dx4,tA_dy,sel);
		 k += spu_sel( k_dx4, k_dy,sel);

		// TODO: at 2 clocks per sel, it's probably worth using a pre-hinted branch here
		// TODO: and just do two adds on the dy transition, especially as all these are
		// TODO: on pipeline 0

		// TODO: also, each word of dx4/dy is the same, perhaps adds should be done with
		// TODO: 4 attributes at a time and the shuffle to pull the data
	} while (spu_extract(left,0)>0);

	block->triangle->count--;
	return 0;

}
