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

#include <spu_mfcio.h>
#include <spu_intrinsics.h>

////////////////////////////////////////////////////////////////////////////////////////////////////

void clearInitFunc(vec_uint4* params, vec_uint4* scratch, vec_int4 hdx, vec_int4 hdy)
{
	vec_float4 wA	= (vec_float4) params[3];
        vec_float4 rA	= (vec_float4) params[4];
        vec_float4 gA	= (vec_float4) params[5];
        vec_float4 bA	= (vec_float4) params[6];

	vec_float4 w = spu_splats(1.0f)/wA;
	vec_float4 t_r = spu_mul(rA, w);
	vec_float4 t_g = spu_mul(gA, w);
	vec_float4 t_b = spu_mul(bA, w);

	vec_uint4 red = spu_and(spu_rlmask(spu_convtu(t_r,32),-8), 0xff0000);
	vec_uint4 green = spu_and(spu_rlmask(spu_convtu(t_g,32),-16), 0xff00);
	vec_uint4 blue = spu_rlmask(spu_convtu(t_b,32),-24);

	vec_uint4 colour = spu_or(spu_or(blue, green),red);

	scratch[0] = colour;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void clearRenderFunc(vec_uint4* params, vec_uint4* scratch, vec_int4 A, vec_int4 hdx, vec_int4 hdy, vec_uint4* pixelbuffer)
{
/*

*/
///////

	vec_uint4 left = spu_promote(32U * 8U, 0);
	vec_uint4* ptr = pixelbuffer;

	vec_uint4 colour = scratch[0];
	
	do {
		*ptr++ = colour;
		left = spu_sub( left, spu_splats(1U) );
	} while (spu_extract(left,0)>0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
