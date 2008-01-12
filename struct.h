/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

#ifndef __struct_h

typedef struct {
	float x, y, z, w;
} float4;

extern void imp_point(float4 a);
extern void imp_line(float4 a, float4 b);
extern void imp_triangle(float4 a, float4 b, float4 c);

#endif // __struct_h
