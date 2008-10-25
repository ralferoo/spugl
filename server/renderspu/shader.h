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
	unsigned long long	magic;			// "spugl-ps"
	unsigned long		length;			// length in bytes of shader
	unsigned long		defintionOffset;	// definition of shader (offset from this structure)
	unsigned long		initFunctionOffset;	// init function (offset from this structure)
	unsigned long		renderFunctionOffset;	// render function (offset from this structure)
	unsigned long		reserved;		// not used currently, must be 0
} PixelShaderHeader;

static const unsigned long long PixelShaderHeaderMagic = 0x737075676c2d7073;

typedef void (*PixelShaderInitFunc)	(vec_uint4* params, vec_uint4* scratch, vec_int4 hdx, vec_int4 hdy);
typedef void (*PixelShaderRenderFunc)	(vec_uint4* params, vec_uint4* scratch, vec_int4 A, vec_int4 hdx, vec_int4 hdy, vec_uint4* pixelbuffer);

extern PixelShaderRenderFunc load_shader(Triangle* triangle, vec_uint4* params, vec_int4 hdx, vec_int4 hdy);
extern vec_uint4*	shader_scratch;
