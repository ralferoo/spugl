/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

// This file holds the standard shaders

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
	
#define PROCESS_BLOCK_HEAD(name) void name (vec_uint4* ptr, Queue* tri, \
		vec_float4 Aa,vec_float4 Ab,vec_float4 Ac, \
		vec_float4 Aa_dx4,vec_float4 Ab_dx4,vec_float4 Ac_dx4, \
		vec_float4 Aa_dy,vec_float4 Ab_dy,vec_float4 Ac_dy) { \
	vec_uint4 left = spu_splats(32*8); \
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
}

//////////////////////////////////////////////////////////////////////////////
//
// This is the simplest shader function; just does a linear interpolation of
// colours between the vertices
//
PROCESS_BLOCK_HEAD(process_colour_block)
	vec_float4 t_r = extract(tri->triangle.r, tAa, tAb, tAc);
	vec_float4 t_g = extract(tri->triangle.g, tAa, tAb, tAc);
	vec_float4 t_b = extract(tri->triangle.b, tAa, tAb, tAc);

	vec_uint4 red = spu_and(spu_rlmask(spu_convtu(t_r,32),-8), 0xff0000);
	vec_uint4 green = spu_and(spu_rlmask(spu_convtu(t_g,32),-16), 0xff00);
	vec_uint4 blue = spu_rlmask(spu_convtu(t_b,32),-24);

	vec_uint4 colour = spu_or(spu_or(blue, green),red);

	vec_uint4 current = *ptr;
	*ptr = spu_sel(current, colour, pixel);
PROCESS_BLOCK_END

//////////////////////////////////////////////////////////////////////////////
//
// This shader does a very ugly form of texture mapping - for each pixel that
// needs mapping, this fires off a DMA for 128 bytes just to then copy across
// the 4 bytes of the texture data
//
	static const vec_uchar16 rgba_argb = {
		3,0,1,2, 7,4,5,6, 11,8,9,10, 15,12,13,14}; 

PROCESS_BLOCK_HEAD(process_texture_block)
/*
	printf("pixel %03d, tAa=%06.2f, tAb=%06.2f, tAc=%06.2f, tA=%06.2f\n",
		spu_extract(pixel,0),
		spu_extract(tAa,0),
		spu_extract(tAb,0),
		spu_extract(tAc,0),
		spu_extract(tAa,0)+spu_extract(tAb,0)+spu_extract(tAc,0));
*/
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
PROCESS_BLOCK_END

//////////////////////////////////////////////////////////////////////////////
extern void block_handler(Queue* queue);

RenderFuncs _standard_texture_triangle = {
	.init = block_handler,
	.process = process_texture_block
};

RenderFuncs _standard_colour_triangle = {
	.init = block_handler,
	.process = process_colour_block
};
