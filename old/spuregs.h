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

// this header file is simply to reserve registers globally for specific
// variables, although we go to quite a lot of effort to ensure that
// "make depend" verifies that all SPU sources do actually use this file
//
// note that on the SPU, registers 80-127 are non-volatile so anyone who
// corrupts them must restore them on exit. all our code knows about these
// registers, so they will not generally be used, but library calls that
// use them will at least preserve them for us.
// note also that if we call a library function that calls us in return,
// these registers might have been corrupted, however, I don't beleive we
// do this anywhere.

#include "struct.h"

#ifndef __spuregs_h
#define __spuregs_h

#ifdef SPU_REGS
#include <spu_intrinsics.h>
register u32 ___foo asm ("80");

// current triangle transformation state
// -------------------------------------
//
// note this is used internally in the vertex transformation, the fragment
// shader and is updated by imp_vertex.

register vec_float4	TRIx		asm ("101"); // the normalised screen
register vec_float4	TRIy		asm ("102"); // coordinates
register vec_float4	TRIz		asm ("103"); // depth buffer value
register vec_float4	TRIw		asm ("104"); // 1/z recip

register vec_float4	TRIr		asm ("105"); // primary colour
register vec_float4	TRIg		asm ("106");
register vec_float4	TRIb		asm ("107");
register vec_float4	TRIa		asm ("108");

register vec_float4	TRIs		asm ("109"); // primary tex coords
register vec_float4	TRIt		asm ("110");
register vec_float4	TRIu		asm ("111");
register vec_float4	TRIv		asm ("112");

// these track the loaded texture block IDs

register vec_ushort8	TEXcache1	asm ("113");
register vec_ushort8	TEXcache2	asm ("114");

// these define the standard projection matrix

register vec_float4	PROJ_x		asm ("115");
register vec_float4	PROJ_y		asm ("116");
register vec_float4	PROJ_z		asm ("117");
register vec_float4	PROJ_w		asm ("118");

#define SEL_A0 0,1,2,3,
#define SEL_A1 4,5,6,7,
#define SEL_A2 8,9,10,11,
#define SEL_A3 12,13,14,15,
#define SEL_B0 16,17,18,19,
#define SEL_B1 20,21,22,23,
#define SEL_B2 24,25,26,27,
#define SEL_B3 28,29,30,31,
#define SEL_00 0x80,0x80,0x80,0x80,
#define SEL_FF 0xC0,0xC0,0xC0,0xC0,
#define SEL_80 0xE0,0xE0,0xE0,0xE0,




/*
static inline vec_int4 log2(vec_float4 a) {
	qword ra = (qword) a;
	qword t0 = si_rotmi(ra,-23);
	qword t1 = si_andi(t0,255);
	qword t2 = si_ai(t1,-127);
	return (vec_int4) t2;
}
*/

static inline vec_int4 log2_sqrt_clamp(vec_float4 a, vec_int4 adjust) {
	qword ra = (qword) a;
	qword rb = (qword) adjust;
	qword t0 = si_shli(ra,1);
	qword t1 = si_rotmi(t0,-24);
	qword t2p = si_sfi(t1,127);
	qword t2 = si_sf(t2p,rb);
	qword t2a = si_rotmai(t2,-1);
	qword t3 = si_rotmai(t2,-31);
	qword t4 = si_andc(t2a,t3);
	return (vec_int4) t4;
}


static inline vec_float4 max(vec_float4 a, vec_float4 b) {
	vec_uint4 cmp = spu_cmpgt(a,b);
	return (vec_float4) spu_sel(b,a,cmp);
}

static inline vec_float4 div(vec_float4 a, vec_float4 b) {
	qword ra = (qword) a;
	qword rb = (qword) b;
	qword t0 = si_frest(rb);
	qword t1 = si_fi(rb,t0);
	qword t2 = si_fm(ra,t1);
	qword t3 = si_fnms(t2,rb,ra);
	qword res = si_fma(t3,t1,t2);
	return (vec_float4) res;
}

static inline unsigned int cmp_ge0(int a) {
	vec_int4 rr;
	asm("cgti %0,%1,-1" : "=r" (rr) : "r" (a));
//	vec_int4 aa = (vec_int4) (a);
//	vec_uint4 rr = spu_cmpgt(aa, -1);
	return spu_extract(rr, 0);
}

static inline unsigned int cmp_eq(unsigned int a, unsigned int b) {
	vec_int4 rr;
	asm("ceq %0,%1,%2" : "=r" (rr) : "r" (a), "r" (b));
	//vec_uint4 aa = (vec_uint4) a;
	//vec_uint4 bb = (vec_uint4) b;
	//vec_uint4 rr = spu_cmpeq(aa, bb);
	return spu_extract(rr, 0);
}

static inline unsigned int if_then_else(unsigned int c, 
		unsigned int a, unsigned int b) {
	vec_int4 rr;
	asm("selb %0,%1,%2,%3" : "=r" (rr) : "r" (b), "r" (a), "r" (c));
	//vec_uint4 aa = (vec_uint4) a;
	//vec_uint4 bb = (vec_uint4) b;
	//vec_uint4 cc = (vec_uint4) c;
	//vec_uint4 rr = spu_sel(bb, aa, cc);
	return spu_extract(rr, 0);
}
	

static inline int first_bit(unsigned int x) {
	vec_int4 clz;
	asm("clz %0,%1" : "=r" (clz) : "r" (x));
	//vec_uint4 xx = (vec_uint4) x;
	//vec_uint4 clz = spu_cntlz(xx);
	return (int) 31-spu_extract(clz,0);
}

#endif

#endif // __spuregs_h

