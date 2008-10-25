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

void flatInitFunc(vec_uint4* params, vec_uint4* scratch, vec_int4 hdx, vec_int4 hdy)
{
//	TRIr = spu_splats(1234.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

static const vec_float4 muls = {0.0f, 1.0f, 2.0f, 3.0f};

static const vec_uchar16 shuf_0101 = { 0x80,0x80,0x80,0x80, 0,1,2,3, 0x80,0x80,0x80,0x80, 0,1,2,3 };
static const vec_uchar16 shuf_0011 = { 0x80,0x80,0x80,0x80, 0x80,0x80,0x80,0x80, 0,1,2,3, 0,1,2,3 };

static inline vec_float4 extract(
        vec_uint4 param, vec_float4 tAa, vec_float4 tAb, vec_float4 tAc)
{
	vec_float4 what = (vec_float4) param;

        return  spu_madd(spu_splats(spu_extract(what,0)),tAa,
                spu_madd(spu_splats(spu_extract(what,1)),tAb,
                spu_mul (spu_splats(spu_extract(what,2)),tAc)));
}
        
void flatRenderFunc(vec_uint4* params, vec_uint4* scratch, vec_int4 A, vec_int4 hdx, vec_int4 hdy, vec_uint4* pixelbuffer)
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

	vec_float4 fAa_dx4	= spu_convtf(Aa_dx4, 5);
	vec_float4 fAb_dx4	= spu_convtf(Ab_dx4, 5);
	vec_float4 fAc_dx4	= spu_convtf(Ac_dx4, 5);

	vec_float4 fAa_dy	= spu_convtf(Aa_dy, 5);
	vec_float4 fAb_dy	= spu_convtf(Ab_dy, 5);
	vec_float4 fAc_dy	= spu_convtf(Ac_dy, 5);

	vec_float4 wA_dx4	= extract(params[3], fAa_dx4, fAb_dx4, fAc_dx4);
	vec_float4 wA_dy	= extract(params[3], fAa_dy, fAb_dy, fAc_dy);

        vec_float4 rA_dx4	= extract(params[4], fAa_dx4, fAb_dx4, fAc_dx4);
        vec_float4 rA_dy	= extract(params[4], fAa_dy, fAb_dy, fAc_dy);

        vec_float4 gA_dx4	= extract(params[5], fAa_dx4, fAb_dx4, fAc_dx4);
        vec_float4 gA_dy	= extract(params[5], fAa_dy, fAb_dy, fAc_dy);

        vec_float4 bA_dx4	= extract(params[6], fAa_dx4, fAb_dx4, fAc_dx4);
        vec_float4 bA_dy	= extract(params[6], fAa_dy, fAb_dy, fAc_dy);

///////

	vec_int4 Aa = spu_add( Aa_dx0123, spu_splats(spu_extract(A,0)));
	vec_int4 Ab = spu_add( Ab_dx0123, spu_splats(spu_extract(A,1)));
	vec_int4 Ac = spu_add( Ac_dx0123, spu_splats(spu_extract(A,2)));

	vec_float4 fAa		= spu_convtf(Aa, 5);
	vec_float4 fAb		= spu_convtf(Ab, 5);
	vec_float4 fAc		= spu_convtf(Ac, 5);

	vec_float4 wA		= extract(params[3], fAa, fAb, fAc);
        vec_float4 rA		= extract(params[4], fAa, fAb, fAc);
        vec_float4 gA		= extract(params[5], fAa, fAb, fAc);
        vec_float4 bA		= extract(params[6], fAa, fAb, fAc);

///////

	vec_uint4 left = spu_promote(32U * 8U, 0);
	vec_uint4* ptr = pixelbuffer;
	
	do {
		vec_uint4 allNeg = (vec_uint4) spu_and(spu_and(Aa,Ab),Ac);
		vec_uint4 pixel = spu_rlmaska(allNeg,-31);
		vec_uint4 bail = spu_orx(pixel);

//		if (spu_extract(bail,0))
		{
			vec_float4 w = spu_splats(1.0f)/wA;

			vec_float4 t_r = spu_mul(rA, w);
			vec_float4 t_g = spu_mul(gA, w);
			vec_float4 t_b = spu_mul(bA, w);

			vec_uint4 red = spu_and(spu_rlmask(spu_convtu(t_r,32),-8), 0xff0000);
			vec_uint4 green = spu_and(spu_rlmask(spu_convtu(t_g,32),-16), 0xff00);
			vec_uint4 blue = spu_rlmask(spu_convtu(t_b,32),-24);

			vec_uint4 colour = spu_or(spu_or(blue, green),red);

			vec_uint4 current = *ptr;

//			current = spu_and(spu_rlmaska(current,-1), spu_splats(0x7f7f7fu));

			*ptr = spu_sel(current, colour, pixel);
		} 
		vec_uint4 which = spu_and(left,spu_splats((unsigned int)7));
		vec_uint4 sel = spu_cmpeq(which,1);
		ptr++;
		left = spu_sub( left, spu_splats(1U) );
		Aa = spu_add( Aa, spu_sel(Aa_dx4,Aa_dy,sel));
		Ab = spu_add( Ab, spu_sel(Ab_dx4,Ab_dy,sel));
		Ac = spu_add( Ac, spu_sel(Ac_dx4,Ac_dy,sel));

		wA = spu_add( wA, spu_sel(wA_dx4,wA_dy,sel));
		rA = spu_add( rA, spu_sel(rA_dx4,rA_dy,sel));
		gA = spu_add( gA, spu_sel(gA_dx4,gA_dy,sel));
		bA = spu_add( bA, spu_sel(bA_dx4,bA_dy,sel));
	} while (spu_extract(left,0)>0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

/*
register vec_float4    	TRIg            asm ("106");
register vec_int4	TRIgi           asm ("106");

register vec_float4    	TRIr            asm ("107");
register vec_int4	TRIri           asm ("107");

typedef union {
	vec_float4    	vf;
	vec_int4	vi;
	vec_uint4	vu;
	vec_ushort8	vus;
	vec_short8	vs;
	unsigned int	ui;
	int		i;
	unsigned short	us;
	short		s;
	float		f;
} AllTypes;

register AllTypes __test asm("108");

//const int& testint = __test.i;
//register int testint __attribute__((weak, alias("__test.i")));

register union {
	vec_float4    	f;
	vec_int4	i;
	vec_uint4	u;
} TEST asm ("107");

void a(void) { TEST.f = spu_splats(123.4f); }
void b(void) { TEST.i = spu_splats(-1234); }
void c(void) { TEST.u = spu_splats(1234U); }

*/



