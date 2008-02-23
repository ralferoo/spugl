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
////////////////////
			vec_float4 t_s = extract(tri->s, tAa, tAb, tAc);
			vec_float4 t_t = extract(tri->t, tAa, tAb, tAc);

			vec_uint4 s_sub = spu_and(spu_rlmask(spu_convtu(t_s,32),-17), 0xf80);	//19-2
			vec_uint4 t_sub = spu_and(spu_rlmask(spu_convtu(t_t,32),-22), 0x7c);	//24-2
			vec_uint4 sub_block_pixel = spu_or(s_sub,t_sub);

			vec_uint4 s_blk = spu_and(spu_rlmask(spu_convtu(t_s,32),-26), 0x38);	//24+2
			vec_uint4 t_blk = spu_and(spu_rlmask(spu_convtu(t_t,32),-29), 0x7);	//24+5
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
			vec_uint4 tex_ofs = spu_sl(cache, 5+5+2);	// offset into texture
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
