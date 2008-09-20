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

#ifndef __SPU_SPUCONTEXT_H
#define __SPU_SPUCONTEXT_H

#include <spu_mfcio.h>
#include <spu_intrinsics.h>

extern unsigned int eah_buffer_tables;
extern unsigned int eal_buffer_lock_table;
extern unsigned int eal_buffer_memory_table;
extern unsigned int eal_renderables_table;

#define MAX_TRIANGLES			256
#define MAX_CHUNKS			64

typedef struct {
	unsigned long pixel_eah;
	unsigned long pixel_eal;
	unsigned long zbuf_eah;
	unsigned long zbuf_eal;
} RenderableTile;

typedef struct {
	unsigned long	base_pixel_eah;
	unsigned long	base_pixel_eal;
	unsigned long	base_zbuf_eah;
	unsigned long	base_zbuf_eal;				// 16 bytes
	unsigned short	pixel_eal_tile_dx;
	unsigned short 	pixel_eal_tile_dy;
	unsigned short	zbuf_eal_tile_dx;
	unsigned short	zbuf_eal_tile_dy;			// 24 bytes
	unsigned short	left_edge;
	unsigned short	right_edge;
	unsigned short	top_edge;
	unsigned short	bottom_edge;				// 32 bytes
} RenderableTarget;

typedef struct {
	RenderableTarget* target;
	unsigned short	number_of_chunks;
	unsigned short	x_y[0];
} RenderableChunkList;


/////////////////////////////////////////////////////////////////////////////////////////////////
//
// Plan for atomic cache line stuff...
//
// Have one cache line for each renderable target and therefore its own triangle buffer etc...

#define NUMBER_OF_CHUNKS 32

struct __RenderableCacheLine {
	unsigned int	chunksBusy;				// bitmask of chunks being rendered
	unsigned int	chunksFree;				// bitmask of chunks that can be allocated

	unsigned short	chunkOffset[NUMBER_OF_CHUNKS];		// start of chunk (max 4096 = 64*64)
	unsigned char	chunkLength[NUMBER_OF_CHUNKS];		// length of chunk
	unsigned char	chunkTriangle[NUMBER_OF_CHUNKS];	// triangle last drawn by chunk
} __attribute__((aligned(128)));


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
