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
	float4 v;
	float4 colour;
} vertex_state;

extern void imp_point(vertex_state a);
extern void imp_line(vertex_state a, vertex_state b);
extern void imp_triangle(vertex_state a, vertex_state b, vertex_state c);

#endif // __struct_h
