/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
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

extern vec_uchar16 shuffle_tri_cw;
extern vec_uchar16 shuffle_tri_ccw;

#endif

#endif // __spuregs_h

