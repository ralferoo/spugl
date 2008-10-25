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
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////////////////////////

void maskInitFunc(vec_uint4* params, vec_uint4* scratch, vec_int4 hdx, vec_int4 hdy)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

static const vec_float4 muls = {0.0f, 1.0f, 2.0f, 3.0f};

static const vec_uchar16 shuf_0101 = { 0x80,0x80,0x80,0x80, 0,1,2,3, 0x80,0x80,0x80,0x80, 0,1,2,3 };
static const vec_uchar16 shuf_0011 = { 0x80,0x80,0x80,0x80, 0x80,0x80,0x80,0x80, 0,1,2,3, 0,1,2,3 };

void maskRenderFunc(vec_uint4* params, vec_uint4* scratch, vec_int4 A, vec_int4 hdx, vec_int4 hdy, vec_uint4* pixelbuffer)
{

	vec_int4 A_dx = spu_rlmaska(hdx, -5);
	vec_int4 A_dx4 = spu_sl(A_dx, 2);
	vec_int4 A_dx32 = spu_sl(A_dx, 5);
	vec_int4 A_dx28 = spu_sub(A_dx32, A_dx4);
	vec_int4 A_dy = spu_sub(spu_rlmaska(hdy, -5), A_dx28);	// dy=realdy-4.realdx

	vec_int4 Aa_dx = spu_splats(spu_extract(A_dx,0));
	vec_int4 Ab_dx = spu_splats(spu_extract(A_dx,1));
	vec_int4 Ac_dx = spu_splats(spu_extract(A_dx,2));

	vec_int4 Aa_dy = spu_splats(spu_extract(A_dy,0));
	vec_int4 Ab_dy = spu_splats(spu_extract(A_dy,1));
	vec_int4 Ac_dy = spu_splats(spu_extract(A_dy,2));

	vec_int4 Aa_dx4 = spu_splats(spu_extract(A_dx4,0));
	vec_int4 Ab_dx4 = spu_splats(spu_extract(A_dx4,1));
	vec_int4 Ac_dx4 = spu_splats(spu_extract(A_dx4,2));

	vec_int4 Aa_dx0101 = spu_shuffle( Aa_dx, Aa_dx, shuf_0101);
	vec_int4 Ab_dx0101 = spu_shuffle( Ab_dx, Aa_dx, shuf_0101);
	vec_int4 Ac_dx0101 = spu_shuffle( Ac_dx, Aa_dx, shuf_0101);
	vec_int4 Aa_dx0022 = spu_shuffle( spu_sl(Aa_dx,1), Aa_dx, shuf_0011);
	vec_int4 Ab_dx0022 = spu_shuffle( spu_sl(Ab_dx,1), Ab_dx, shuf_0011);
	vec_int4 Ac_dx0022 = spu_shuffle( spu_sl(Ac_dx,1), Ac_dx, shuf_0011);

	vec_int4 Aa_dx0123 = spu_add(Aa_dx0101, Aa_dx0022);
	vec_int4 Ab_dx0123 = spu_add(Ab_dx0101, Ab_dx0022);
	vec_int4 Ac_dx0123 = spu_add(Ac_dx0101, Ac_dx0022);

	//vec_int4 Aa_dx0123 = spu_sub(spu_add(Aa_dx0101, Aa_dx0022), spu_splats(1U));
	//vec_int4 Ab_dx0123 = spu_sub(spu_add(Ab_dx0101, Ab_dx0022), spu_splats(1U));
	//vec_int4 Ac_dx0123 = spu_sub(spu_add(Ac_dx0101, Ac_dx0022), spu_splats(1U));

	vec_int4 Aa = spu_add( Aa_dx0123, spu_splats(spu_extract(A,0)));
	vec_int4 Ab = spu_add( Ab_dx0123, spu_splats(spu_extract(A,1)));
	vec_int4 Ac = spu_add( Ac_dx0123, spu_splats(spu_extract(A,2)));

	vec_uint4 left = spu_promote(32U * 8U, 0);
	vec_uint4* ptr = pixelbuffer;
	
	do {
		vec_uint4 allNeg = (vec_uint4) spu_and(spu_and(Aa,Ab),Ac);
		vec_uint4 pixel = spu_rlmaska(allNeg,-31);
		vec_uint4 bail = spu_orx(pixel);

//		if (spu_extract(bail,0)) {

//			vec_float4 t_w = extract(tri->w, Aa, Ab, Ac);
//			vec_float4 w = spu_splats(1.0f)/t_w;
//			vec_float4 tAa = spu_mul(Aa,w);
//			vec_float4 tAb = spu_mul(Ab,w);
//			vec_float4 tAc = spu_mul(Ac,w);

			vec_uint4 colour = spu_splats(0x7fff30U);

			vec_uint4 current = *ptr;

			//current = spu_rlmask(current, -1);

			*ptr = spu_sel(current, colour, pixel);
//		} 
		vec_uint4 which = spu_and(left,spu_splats((unsigned int)7));
		vec_uint4 sel = spu_cmpeq(which,1);
		ptr++;
		left = spu_sub( left, spu_splats(1U) );
		Aa = spu_add( Aa, spu_sel(Aa_dx4,Aa_dy,sel));
		Ab = spu_add( Ab, spu_sel(Ab_dx4,Ab_dy,sel));
		Ac = spu_add( Ac, spu_sel(Ac_dx4,Ac_dy,sel));
	} while (spu_extract(left,0)>0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
