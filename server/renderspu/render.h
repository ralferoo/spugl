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

#ifndef __SPU_SPUCONTEXT_H
#define __SPU_SPUCONTEXT_H

#include <spu_mfcio.h>
#include <spu_intrinsics.h>

typedef union {
	struct {
		unsigned long eah;
		unsigned long eal;
	};
	unsigned long long ea;
} EA;

#define NUMBER_OF_CHUNKS 29				// small enough to fit, can't be over 31 in any case

struct __RenderableCacheLine {
	vec_uchar16	chunkTriangle1;				// triangle ID last drawn by corresponding chunk
	vec_uchar16	chunkTriangle2;				// last triangle ID in index 31 (byte 15 of #2)
//32
	unsigned int	chunksBusy;				// bitmask of chunks being rendered
	unsigned int	chunksFree;				// bitmask of chunks that can be allocated
//40
	unsigned short	chunkOffset[NUMBER_OF_CHUNKS];		// start of chunk (max 4096 = 64*64 = 16*256)
	unsigned char	chunkLength[NUMBER_OF_CHUNKS];		// length of chunk
//40+3*NUMBER_OF_CHUNKS = 40+87 = 127
} __attribute__((aligned(128)));

struct RenderableChunk {
	unsigned char	dx;
	unsigned char	dy;
};

struct RenderableTarget {
	unsigned long long	next;			// next renderable target in queue
	unsigned long long	atomic;			// pointer to atomic structure (128 byte aligned)

	unsigned long long	pixels;			// pixel buffer
	unsigned long long	zbuffer;		// depth buffer
// 32
	unsigned int		pixel_eal_tile_dx;
	unsigned int 		pixel_eal_tile_dy;
	unsigned int 		pixel_eal_stride;
// 44
	unsigned int		zbuf_eal_tile_dx;
	unsigned int		zbuf_eal_tile_dy;
	unsigned int 		zbuf_eal_stride;
// 56
	unsigned short		left_edge;
	unsigned short		right_edge;
	unsigned short		top_edge;
	unsigned short		bottom_edge;
// 64
	struct RenderableChunk 	chunks[0];
};


/////////////////////////////////////////////////////////////////////////////////////////////////
//
// Plan for atomic cache line stuff...
//
// Have one cache line for each renderable target and therefore its own triangle buffer etc...

#define NUM_VERTEX_UNIFORM_VECTORS	128
#define NUM_FRAGMENT_UNIFORM_VECTORS	 16

typedef struct {
	int	uniform_rows;		// this many rows of vectors; size = uniform_rows*16 bytes
	int	varying_rows;		// this many rows of varying vectors
	int	attrib_rows;		// this many rows of attributes
} VertexShaderContext;

typedef struct {
	vec_float4   vertex_uniform_vectors[  NUM_VERTEX_UNIFORM_VECTORS];
	vec_float4 fragment_uniform_vectors[NUM_FRAGMENT_UNIFORM_VECTORS];

	vec_float4 projection_matrix_x, projection_matrix_y, projection_matrix_z, projection_matrix_w;
	vec_float4  modelview_matrix_x,  modelview_matrix_y,  modelview_matrix_z,  modelview_matrix_w;
	vec_float4    texture_matrix_x,    texture_matrix_y,    texture_matrix_z,    texture_matrix_w;
} Context;

typedef struct {
	float		recip[3];
	float		x[3];
	float		y[3];

	float		A[3];
	float		Adx[3];
	float		Ady[3];

	vec_float4	gl_Position[3];		// pre-multiplied by recip
	vec_float4	varings[8][3];		// pre-multiplied by recip
} Triangle;

#endif // __SPU_SPUCONTEXT_H
