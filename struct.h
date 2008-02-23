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

#ifndef __struct_h
#define __struct_h

#include "types.h"

#define LOCAL_FRAGMENTS 1

#define FRAGMENT_WIDTH 32
#define FRAGMENT_HEIGHT 32
#define FRAGMENT_SIZE (4*FRAGMENT_WIDTH*FRAGMENT_HEIGHT)

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

///////////////////////////////////////////////////////////////////////////////

#ifdef SPU_REGS
#include <spu_intrinsics.h>

typedef struct {
	vec_float4	x,y,z,w;	// coords
	vec_float4	r,g,b,a;	// primary colour
	vec_float4	s,t,u,v;	// primary texture
	vec_float4	A,dAdx,dAdy;	// weight information
	vec_float4	minmax;		// bounding box (xmin,ymin,xmax,ymax)

	u32 		texture;
	void *		shader;
	unsigned int dummy;
} triangle;

typedef struct {
	
} deferred;

//#include "queue.h"

#endif

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//
// application errors
#define ERROR_NONE			0 //GL_NO_ERROR
#define ERROR_NESTED_GLBEGIN		1
#define ERROR_GLBEGIN_INVALID_STATE	2
#define ERROR_GLEND_WITHOUT_GLBEGIN	3

// internal errors
#define ERROR_VERTEX_INVALID_STATE	1004
#define ERROR_VERTEX_INVALID_SHUFFLE	1005

extern void raise_error(int error);

///////////////////////////////////////////////////////////////////////////////

#include "spuregs.h"

#endif // __struct_h
