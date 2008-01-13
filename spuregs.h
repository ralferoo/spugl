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
register u32 ___foo asm ("80");
#endif

#endif // __spuregs_h

