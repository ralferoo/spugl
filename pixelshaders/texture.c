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

register vec_float4 	   	_part1          asm ("107");
register void (*_nextfunc)()  			asm ("108");

void magic()
{
	_part1 = spu_splats(2.0f);

	asm volatile( "bi %0" : /*no outputs*/ : "r" (_nextfunc) );
//	return _nextfunc();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

unsigned int _texture_buffer[ 36*33 ];
unsigned int need_to_do = 1;

void textureInitFunc(vec_uint4* params, vec_uint4* scratch, vec_int4 hdx, vec_int4 hdy)
{
	if (need_to_do) {
		for( int y=0; y<33; y++ ) {
			for( int x=0; x<36; x++) {
				int rgb;
				if (x==0 || y ==0)
					rgb = 0xffffffff;
				else {
					rgb = 0xff000000;
					rgb |= ((y*255)/32) << 16;
					rgb |= (x*65535)/36;
				}
				_texture_buffer[ y*36 + 33 ] = rgb;
			}	
		}
		need_to_do = 0;
	}

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
        
void textureRenderFunc(vec_uint4* params, vec_uint4* scratch, vec_int4 A, vec_int4 hdx, vec_int4 hdy, vec_uint4* pixelbuffer)
{

	vec_int4 A_dx = spu_rlmaska(hdx, -5);
	vec_int4 A_dx4 = spu_sl(A_dx, 2);
	vec_int4 A_dx32 = spu_sl(A_dx, 5);
	vec_int4 A_dx28 = spu_sub(A_dx32, A_dx4);
	vec_int4 A_dy = spu_rlmaska(hdy, -5);
	vec_int4 A_dy_sub_dx28 = spu_sub(A_dy, A_dx28);	// dy=realdy-4.realdx

	vec_int4 Aa_dx = spu_splats(spu_extract(A_dx,0));
	vec_int4 Ab_dx = spu_splats(spu_extract(A_dx,1));
	vec_int4 Ac_dx = spu_splats(spu_extract(A_dx,2));

	vec_int4 Aa_dy = spu_splats(spu_extract(A_dy,0));
	vec_int4 Ab_dy = spu_splats(spu_extract(A_dy,1));
	vec_int4 Ac_dy = spu_splats(spu_extract(A_dy,2));

	vec_int4 Aa_dy_sub_dx28 = spu_splats(spu_extract(A_dy_sub_dx28,0));
	vec_int4 Ab_dy_sub_dx28 = spu_splats(spu_extract(A_dy_sub_dx28,1));
	vec_int4 Ac_dy_sub_dx28 = spu_splats(spu_extract(A_dy_sub_dx28,2));

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

	vec_float4 fAa_dy_sub_dx28	= spu_convtf(Aa_dy_sub_dx28, 5);
	vec_float4 fAb_dy_sub_dx28	= spu_convtf(Ab_dy_sub_dx28, 5);
	vec_float4 fAc_dy_sub_dx28	= spu_convtf(Ac_dy_sub_dx28, 5);

	vec_float4 wA_dx4		= extract(params[3], fAa_dx4, fAb_dx4, fAc_dx4);
	vec_float4 wA_dy_sub_dx28	= extract(params[3], fAa_dy_sub_dx28, fAb_dy_sub_dx28, fAc_dy_sub_dx28);

        vec_float4 sA_dx4		= extract(params[4], fAa_dx4, fAb_dx4, fAc_dx4);
        vec_float4 sA_dy_sub_dx28	= extract(params[4], fAa_dy_sub_dx28, fAb_dy_sub_dx28, fAc_dy_sub_dx28);

        vec_float4 tA_dx4		= extract(params[5], fAa_dx4, fAb_dx4, fAc_dx4);
        vec_float4 tA_dy_sub_dx28	= extract(params[5], fAa_dy_sub_dx28, fAb_dy_sub_dx28, fAc_dy_sub_dx28);




//	vec_int4 adjust = tex_def->mipmapshifts;
//	vec_uint4 mip_block_shift_tmp = (vec_uint4) tex_shift_count;
//	vec_uint4 mip_block_shift = spu_add(mip_block_shift_tmp, spu_rlmask(mip_block_shift_tmp, -16));
//	vec_int4 max_mipmap = spu_splats((int)tex_def->tex_max_mipmap);



	// calculate intermediates for mipmap calcs (this could be much simpler!)
	vec_float4 fAa_dx	= spu_convtf(Aa_dx, 5);
	vec_float4 fAb_dx	= spu_convtf(Ab_dx, 5);
	vec_float4 fAc_dx	= spu_convtf(Ac_dx, 5);

	vec_float4 fAa_dy	= spu_convtf(Aa_dy, 5);
	vec_float4 fAb_dy	= spu_convtf(Ab_dy, 5);
	vec_float4 fAc_dy	= spu_convtf(Ac_dy, 5);

	vec_float4 wA_dx		= extract(params[3], fAa_dx, fAb_dx, fAc_dx);
	vec_float4 wA_dy		= extract(params[3], fAa_dy, fAb_dy, fAc_dy);
        vec_float4 sA_dx		= extract(params[4], fAa_dx, fAb_dx, fAc_dx);
        vec_float4 sA_dy		= extract(params[4], fAa_dy, fAb_dy, fAc_dy);
        vec_float4 tA_dx		= extract(params[5], fAa_dx, fAb_dx, fAc_dx);
        vec_float4 tA_dy		= extract(params[5], fAa_dy, fAb_dy, fAc_dy);

	vec_float4 fA			= spu_convtf(A, 5);
	vec_float4 fAa			= spu_splats(spu_extract(fA,0));
	vec_float4 fAb			= spu_splats(spu_extract(fA,1));
	vec_float4 fAc			= spu_splats(spu_extract(fA,2));

	vec_float4 wA			= extract(params[3], fAa, fAb, fAc);
        vec_float4 sA			= extract(params[4], fAa, fAb, fAc);
        vec_float4 tA			= extract(params[5], fAa, fAb, fAc);

	// generate the mipmap start and deltas
	vec_float4 mip_wA_mult = sA_dx*tA_dy - sA_dy*tA_dx;
	vec_float4 mip_sA_mult = tA_dx*wA_dy - tA_dy*wA_dx;
	vec_float4 mip_tA_mult = wA_dx*sA_dy - wA_dy*sA_dx;

	vec_float4 kA    = mip_wA_mult*wA     + mip_sA_mult*sA     + mip_tA_mult*tA;
	vec_float4 kA_dx4 = mip_wA_mult*wA_dx4 + mip_sA_mult*sA_dx4 + mip_tA_mult*tA_dx4;
	vec_float4 kA_dy_sub_dx28  = mip_wA_mult*wA_dy_sub_dx28  + mip_sA_mult*sA_dy_sub_dx28  + mip_tA_mult*tA_dy_sub_dx28;



///////

	vec_int4 Aa = spu_add( Aa_dx0123, spu_splats(spu_extract(A,0)));
	vec_int4 Ab = spu_add( Ab_dx0123, spu_splats(spu_extract(A,1)));
	vec_int4 Ac = spu_add( Ac_dx0123, spu_splats(spu_extract(A,2)));

//	vec_float4 fAa		= spu_convtf(Aa, 5);
//	vec_float4 fAb		= spu_convtf(Ab, 5);
//	vec_float4 fAc		= spu_convtf(Ac, 5);

//	vec_float4 wA		= extract(params[3], fAa, fAb, fAc);
//      vec_float4 sA		= extract(params[4], fAa, fAb, fAc);
//      vec_float4 tA		= extract(params[5], fAa, fAb, fAc);
//      vec_float4 kA		= extract(params[6], fAa, fAb, fAc);

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

			vec_float4 t_r = spu_mul(sA, w);
			vec_float4 t_g = spu_mul(tA, w);
			vec_float4 t_b = spu_mul(kA, w);

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
		Aa = spu_add( Aa, spu_sel(Aa_dx4,Aa_dy_sub_dx28,sel));
		Ab = spu_add( Ab, spu_sel(Ab_dx4,Ab_dy_sub_dx28,sel));
		Ac = spu_add( Ac, spu_sel(Ac_dx4,Ac_dy_sub_dx28,sel));

		wA = spu_add( wA, spu_sel(wA_dx4,wA_dy_sub_dx28,sel));
		sA = spu_add( sA, spu_sel(sA_dx4,sA_dy_sub_dx28,sel));
		tA = spu_add( tA, spu_sel(tA_dx4,tA_dy_sub_dx28,sel));
		kA = spu_add( kA, spu_sel(kA_dx4,kA_dy_sub_dx28,sel));
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



