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

#include "struct.h"

#ifndef __spuregs_h
#define __spuregs_h

#ifdef SPU_REGS
register u32 ___foo asm ("82");
#endif

#endif // __spuregs_h

