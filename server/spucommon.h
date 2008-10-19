/****************************************************************************
 *
 * SPU GL - 3d rasterisation library for the PS3
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net>
 *
 * This library may not be used or distributed without a licence, please
 * contact me for information if you wish to use it.
 *
 ***************************************************************************/

#ifndef __SPU_COMMON_H
#define __SPU_COMMON_H

#ifdef SPU_REGS
#include <spu_mfcio.h>
#include <spu_intrinsics.h>
#endif // SPU_REGS

///////////////////////////////////////////////////////////////////////////////

typedef struct __TRIANGLE Triangle;
typedef struct __CONTEXT Context;

////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef SPU_REGS

// trianglebuffer.c
extern Triangle* getTriangleBuffer(Context* context);
extern void writeTriangleBuffer(Triangle* endTriangle);

#endif // SPU_REGS

////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // __SPU_COMMON_H
