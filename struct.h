/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

#ifndef __struct_h
#define __struct_h

#include "types.h"

typedef struct {
	u64 address;
	u32 width;
	u32 height;
	u32 bytes_per_line;
} _bitmap_image;
typedef _bitmap_image* BitmapImage;

typedef struct {
	float x, y, z, w;
} float4;

typedef struct {
	float4 coords;
	float4 colour;
} vertex_state;

#include "spuregs.h"

#endif // __struct_h