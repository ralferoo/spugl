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

u32 textureTemp0[32] __attribute__((aligned(128)));
u32 textureTemp1[32] __attribute__((aligned(128)));
u32 textureTemp2[32] __attribute__((aligned(128)));
u32 textureTemp3[32] __attribute__((aligned(128)));

static inline vec_float4 extract(
	vec_float4 what, vec_float4 tAa, vec_float4 tAb, vec_float4 tAc)
{
	return	spu_madd(spu_splats(spu_extract(what,0)),tAa,
		spu_madd(spu_splats(spu_extract(what,1)),tAb,
		spu_mul (spu_splats(spu_extract(what,2)),tAc)));
}
	
#define PROCESS_BLOCK_HEAD(name) vec_ushort8 name (Queue* queue, \
		vec_float4 Aa,vec_float4 Ab,vec_float4 Ac, \
		vec_float4 Aa_dx4,vec_float4 Ab_dx4,vec_float4 Ac_dx4, \
		vec_float4 Aa_dy,vec_float4 Ab_dy,vec_float4 Ac_dy) { \
	vec_uint4 left = spu_splats(32*8); \
	vec_uint4* ptr = queue->block.pixels; \
	Queue* tri = queue->block.triangle; \
	do { \
		vec_uint4 uAa = (vec_uint4) Aa; \
		vec_uint4 uAb = (vec_uint4) Ab; \
		vec_uint4 uAc = (vec_uint4) Ac; \
		vec_uint4 allNeg = spu_and(spu_and(uAa,uAb),uAc); \
		vec_uint4 pixel = spu_rlmaska(allNeg,-31); \
		vec_uint4 bail = spu_orx(pixel); \
		if (spu_extract(bail,0)) { \
			vec_float4 t_w = extract(tri->triangle.w, Aa, Ab, Ac); \
			vec_float4 w = spu_splats(1.0f)/t_w; \
			vec_float4 tAa = spu_mul(Aa,w); \
			vec_float4 tAb = spu_mul(Ab,w); \
			vec_float4 tAc = spu_mul(Ac,w);

#define PROCESS_BLOCK_END \
			} \
		vec_uint4 which = spu_and(left,spu_splats((unsigned int)7)); \
		vec_uint4 sel = spu_cmpeq(which,1); \
		ptr++; \
		left -= spu_splats(1); \
		Aa += spu_sel(Aa_dx4,Aa_dy,sel); \
		Ab += spu_sel(Ab_dx4,Ab_dy,sel); \
		Ac += spu_sel(Ac_dx4,Ac_dy,sel); \
	} while (spu_extract(left,0)>0); \
	return (vec_ushort8) spu_splats((unsigned short)-1); \
}


//////////////////////////////////////////////////////////////////////////////
//
// This is the simplest shader function; just does a linear interpolation of
// colours between the vertices
//
PROCESS_BLOCK_HEAD(process_colour_block)
{
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
PROCESS_BLOCK_END

//////////////////////////////////////////////////////////////////////////////

static const vec_uchar16 rgba_argb = {
	3,0,1,2, 7,4,5,6, 11,8,9,10, 15,12,13,14}; 

//////////////////////////////////////////////////////////////////////////////
//
// This shader does a very ugly form of texture mapping - for each pixel that
// needs mapping, this fires off a DMA for 128 bytes just to then copy across
// the 4 bytes of the texture data
//
PROCESS_BLOCK_HEAD(process_simple_texture_block)
{
	vec_float4 t_s = extract(tri->triangle.s, tAa, tAb, tAc);
	vec_float4 t_t = extract(tri->triangle.t, tAa, tAb, tAc);

	vec_uint4 s_sub = spu_and(spu_rlmask(spu_convtu(t_s,32),-14), 0x3fc00);
	vec_uint4 t_sub = spu_and(spu_rlmask(spu_convtu(t_t,32),-22), 0x3fc);
	vec_uint4 offset = spu_or(s_sub,t_sub);
	unsigned long texAddrBase = tri->triangle.texture_base;
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
PROCESS_BLOCK_END

//////////////////////////////////////////////////////////////////////////////

extern void* textureCache;

static const vec_uchar16 shuf_gath_01 = {
	2,3,128,128, 18,19,128,128, SEL_00 SEL_00};

static const vec_uchar16 shuf_gath_23 = {
	SEL_00 SEL_00 2,3,128,128, 18,19,128,128,};

/*
static const vec_uchar16 splat0 = {
	18,19,0,1, 2,3,4,5, 6,7,8,9, 10,11,12,13};
static const vec_uchar16 splat1 = {
	22,23,0,1, 2,3,4,5, 6,7,8,9, 10,11,12,13};
static const vec_uchar16 splat2 = {
	26,27,0,1, 2,3,4,5, 6,7,8,9, 10,11,12,13};
static const vec_uchar16 splat3 = {
	30,31,0,1, 2,3,4,5, 6,7,8,9, 10,11,12,13};
*/

static const vec_uchar16 dup_check0 = {SEL_FF SEL_A0 SEL_A0 SEL_A0};
static const vec_uchar16 dup_check1 = {SEL_FF SEL_FF SEL_A1 SEL_A1};
static const vec_uchar16 dup_check2 = {SEL_FF SEL_FF SEL_FF SEL_A2};

static const vec_uchar16 splats[] = {
	/* 0000 */ {0,1, 2,3,4,5, 6,7,8,9, 10,11,12,13, 14,15},
	/* 0001 */ {30,31,0,1, 2,3,4,5, 6,7,8,9, 10,11,12,13},
	/* 0010 */ {26,27,0,1, 2,3,4,5, 6,7,8,9, 10,11,12,13},
	/* 0011 */ {26,27,30,31, 0,1, 2,3,4,5, 6,7,8,9, 10,11},
	/* 0100 */ {22,23,0,1, 2,3,4,5, 6,7,8,9, 10,11,12,13},
	/* 0101 */ {22,23,30,31, 0,1, 2,3,4,5, 6,7,8,9, 10,11},
	/* 0110 */ {22,23,26,27, 0,1, 2,3,4,5, 6,7,8,9, 10,11},
	/* 0111 */ {22,23,26,27,30,31, 0,1, 2,3,4,5, 6,7,8,9},

	/* 1000 */ {18,19, 0,1, 2,3,4,5, 6,7,8,9, 10,11,12,13},
	/* 1001 */ {18,19, 30,31,0,1, 2,3,4,5, 6,7,8,9, 10,11},
	/* 1010 */ {18,19, 26,27,0,1, 2,3,4,5, 6,7,8,9, 10,11},
	/* 1011 */ {18,19, 26,27,30,31, 0,1, 2,3,4,5, 6,7,8,9},
	/* 1100 */ {18,19, 22,23,0,1, 2,3,4,5, 6,7,8,9, 10,11},
	/* 1101 */ {18,19, 22,23,30,31, 0,1, 2,3,4,5, 6,7,8,9},
	/* 1110 */ {18,19, 22,23,26,27, 0,1, 2,3,4,5, 6,7,8,9},
	/* 1111 */ {18,19, 22,23,26,27,30,31, 0,1, 2,3,4,5, 6,7},
};
//////////////////////////////////////////////////////////////////////////////
//
// This shader does a very ugly form of texture mapping - for each pixel that
// needs mapping, this fires off a DMA for 128 bytes just to then copy across
// the 4 bytes of the texture data
//
vec_ushort8 process_texture_block(Queue* queue, 
		vec_float4 Aa,vec_float4 Ab,vec_float4 Ac, 
		vec_float4 Aa_dx4,vec_float4 Ab_dx4,vec_float4 Ac_dx4, 
		vec_float4 Aa_dy,vec_float4 Ab_dy,vec_float4 Ac_dy) { 
	vec_uint4 left = spu_splats(32*8); 
	vec_uint4* ptr = queue->block.pixels; 
	vec_ushort8 need1 = spu_splats((unsigned short)-1); 
	char* tex_mask_ptr = queue->block.tex_temp; 
	Queue* tri = queue->block.triangle; 
	vec_uint4 tex_id_base = spu_splats((unsigned int)tri->triangle.tex_id_base);
	do { 
		vec_uint4 uAa = (vec_uint4) Aa; 
		vec_uint4 uAb = (vec_uint4) Ab; 
		vec_uint4 uAc = (vec_uint4) Ac; 
		vec_uint4 allNeg = spu_and(spu_and(uAa,uAb),uAc); 
		vec_uint4 pixel = spu_rlmaska(allNeg,-31); 
		char _tex_mask = *tex_mask_ptr | queue->block.tex_override; 
		vec_uint4 tex_mask = si_fsm((vec_uint4)_tex_mask); 
		pixel = spu_and(pixel,tex_mask); 
		vec_uint4 bail = spu_orx(pixel); 
		if (spu_extract(bail,0)) { 
			vec_float4 t_w = extract(tri->triangle.w, Aa, Ab, Ac); 
			vec_float4 w = spu_splats(1.0f)/t_w; 
			vec_float4 tAa = spu_mul(Aa,w); 
			vec_float4 tAb = spu_mul(Ab,w); 
			vec_float4 tAc = spu_mul(Ac,w);
////////////////////
			vec_float4 t_s = extract(tri->triangle.s, tAa, tAb, tAc);
			vec_float4 t_t = extract(tri->triangle.t, tAa, tAb, tAc);

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
			vec_uint4 cache = spu_cntlz(gather);
			vec_uint4 cache_not_found = spu_cmpeq(cache,spu_splats((unsigned int)32));
		
			// pixel is mask of 1's where we want to draw
			// cache_not_found mask of 0's where we have valid data
			// tex_mask is mask of 1's where we need to draw
			pixel = spu_andc(pixel, cache_not_found);
			tex_mask = spu_andc(tex_mask, pixel);		// remove pixels we can draw
		
			vec_uint4 local_tex_base = spu_splats((unsigned int)&textureCache);
			vec_uint4 tex_ofs = spu_sl(cache, 5+5+2);	// offset into texture
			vec_uint4 addr = spu_add(tex_ofs,spu_add(sub_block_pixel,local_tex_base));
		
			unsigned long pixel0 = *((u32*)spu_extract(addr,0));
			unsigned long pixel1 = *((u32*)spu_extract(addr,1));
			unsigned long pixel2 = *((u32*)spu_extract(addr,2));
			unsigned long pixel3 = *((u32*)spu_extract(addr,3));
			vec_uint4 colour = {pixel0, pixel1, pixel2, pixel3};
		
			// now try to work out what textures were missing
			// copy_cmp_[0123] hold required texture ids splatted across all 8 halfwords
		
			vec_uint4 check_dups0 = spu_cmpeq(block_id,
						spu_shuffle(block_id,block_id,dup_check0));
			vec_uint4 check_dups1 = spu_cmpeq(block_id,
						spu_shuffle(block_id,block_id,dup_check1));
			vec_uint4 check_dups2 = spu_cmpeq(block_id,
						spu_shuffle(block_id,block_id,dup_check2));
			vec_uint4 is_duplicate = spu_or(spu_or(check_dups0,check_dups1), check_dups2);

			vec_uint4 gotneeds_0 = spu_gather(spu_cmpeq(need1,copy_cmp_0));
			vec_uint4 gotneeds_1 = spu_gather(spu_cmpeq(need1,copy_cmp_1));
			vec_uint4 gotneeds_2 = spu_gather(spu_cmpeq(need1,copy_cmp_2));
			vec_uint4 gotneeds_3 = spu_gather(spu_cmpeq(need1,copy_cmp_3));

			vec_uint4 gotneeds_all = {spu_extract(gotneeds_0,0),spu_extract(gotneeds_1,0),
				  		spu_extract(gotneeds_2,0), spu_extract(gotneeds_3,0)};
			vec_uint4 gotneeds_mask = spu_andc(spu_cmpeq(gotneeds_all,0),is_duplicate);
			
			vec_uint4 want_textures = spu_and(cache_not_found,gotneeds_mask);
			
			vec_uint4 want_gather = spu_gather(want_textures);
			need1 = spu_shuffle(need1,(vec_ushort8)block_id,
							splats[spu_extract(want_gather,0)]);
		
		/*
			printf("%d %d %d %d -> %02lx %02lx %02lx %02lx"
				" got %02x %02x %02x %02x %x%x%x%x"
				" needs %04x %04x %04x %04x %04x %04x %04x %04x [%x %x]\n",
				spu_extract(block_id,0), spu_extract(block_id,1), 
				spu_extract(block_id,2), spu_extract(block_id,3),
				spu_extract(cache,0), spu_extract(cache,1),
				spu_extract(cache,2), spu_extract(cache,3),
			
				spu_extract(gotneeds_0,0), spu_extract(gotneeds_1,0),
				spu_extract(gotneeds_2,0), spu_extract(gotneeds_3,0),
				spu_extract(gotneeds_mask,0)&1, spu_extract(gotneeds_mask,1)&1,
				spu_extract(gotneeds_mask,2)&1, spu_extract(gotneeds_mask,3)&1,

				spu_extract(need1,0), spu_extract(need1,1),
				spu_extract(need1,2), spu_extract(need1,3),
				spu_extract(need1,4), spu_extract(need1,5),
				spu_extract(need1,6), spu_extract(need1,7),
				spu_extract(want_gather,0),
				spu_extract(gotneeds_all,0)
				); 
		*/
		
			colour = spu_shuffle(colour, colour, rgba_argb);
		
			vec_uint4 current = *ptr;
			*ptr = spu_sel(current, colour, pixel);
////////////////////
		} 
		vec_uint4 which = spu_and(left,spu_splats((unsigned int)7)); 
		vec_uint4 sel = spu_cmpeq(which,1); 
		ptr++; 
		*tex_mask_ptr++ = (char) spu_extract(spu_gather(tex_mask),0); 
		left -= spu_splats(1); 
		Aa += spu_sel(Aa_dx4,Aa_dy,sel); 
		Ab += spu_sel(Ab_dx4,Ab_dy,sel); 
		Ac += spu_sel(Ac_dx4,Ac_dy,sel); 
	} while (spu_extract(left,0)>0); 
	queue->block.tex_override = 0; 
	return need1;
}

//////////////////////////////////////////////////////////////////////////////

extern void block_handler(Queue* queue);

RenderFuncs _standard_simple_texture_triangle = {
	.init = block_handler,
	.process = process_simple_texture_block
};

RenderFuncs _standard_texture_triangle = {
	.init = block_handler,
	.process = process_texture_block
};

RenderFuncs _standard_colour_triangle = {
	.init = block_handler,
	.process = process_colour_block
};
