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

#define MAX_MIPMAP_LEVELS 10

typedef struct {
	unsigned int tex_x_y_shift;
	unsigned int tex_mipmap_shift;
	unsigned int tex_max_mipmap;
	u32* tex_data[MAX_MIPMAP_LEVELS];
	unsigned int tex_t_mult[MAX_MIPMAP_LEVELS];
	unsigned int tex_id_base[MAX_MIPMAP_LEVELS];
} _texture;
typedef _texture* Texture;

typedef struct {
	u64 address;
	u32 width;
	u32 height;
	u32 bytes_per_line;
//	short frag_height;
//	short frag_width;
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
