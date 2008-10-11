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

typedef struct {
	unsigned int dataSize;
	//unsigned int pad0[3];
	void (*initFunction)(vec_uint4* info);
	//unsigned int pad1[3];
	void (*renderFunction)(vec_uint4* pixelbuffer, vec_uint4 *userData, vec_int4 A, vec_int4 hdx, vec_int4 hdy);
	//unsigned int pad2[3];
} PixelShaderInfo;
