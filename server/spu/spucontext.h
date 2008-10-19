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

#include "../spucommon.h"

#include "../../server/spu/spuregs.h"

extern unsigned int eah_buffer_tables;
extern unsigned int eal_buffer_lock_table;
extern unsigned int eal_buffer_memory_table;
extern unsigned int eal_renderables_table;

/////////////////////////////////////////////////////////////////////////////////////////////////


typedef struct {
	float x, y, z, w;
} float4;









#define NUM_VERTEX_UNIFORM_VECTORS	128
#define NUM_FRAGMENT_UNIFORM_VECTORS	 16

typedef struct {
	int	uniform_rows;		// this many rows of vectors; size = uniform_rows*16 bytes
	int	varying_rows;		// this many rows of varying vectors
	int	attrib_rows;		// this many rows of attributes
} VertexShaderContext;

struct __CONTEXT {
	unsigned long long	renderableCacheLine;
	unsigned long long	pad;

	vec_float4   vertex_uniform_vectors[  NUM_VERTEX_UNIFORM_VECTORS];
	vec_float4 fragment_uniform_vectors[NUM_FRAGMENT_UNIFORM_VECTORS];

	vec_float4 projection_matrix_x, projection_matrix_y, projection_matrix_z, projection_matrix_w;
	vec_float4  modelview_matrix_x,  modelview_matrix_y,  modelview_matrix_z,  modelview_matrix_w;
	vec_float4    texture_matrix_x,    texture_matrix_y,    texture_matrix_z,    texture_matrix_w;
};









//#define MAX_MIPMAP_LEVELS 10



/*

typedef struct __TEXTURE TextureDefinition;

typedef struct __ACTIVE ActiveBlock;
typedef struct __BLOCK Block;

typedef int (TriangleGenerator)(Triangle* tri);
typedef int (TriangleHandler)(Triangle* tri, Block* block);
typedef void* (BlockHandler)(void* self, Block* block, ActiveBlock* active, int tag);
typedef void (BlockActivater)(Block* block, ActiveBlock* active, int tag);
typedef void (ActiveBlockInit)(ActiveBlock* active);
typedef void (ActiveBlockFlush)(ActiveBlock* active, int tag);
*/

/*
// this holds data needed for texture calculations
struct __TEXTURE {
	vec_short8	shifts;		// interleaved shift masks,  odd: log2(height)  (s_blk_max)
					// interleaved shift masks, even: log2(width)	(t_blk_max)
	vec_int4	mipmapshifts;
	unsigned short	users;		// number of triangle producers still using this texture
	unsigned short	tex_max_mipmap;	// how many levels of mipmap are present
	unsigned short	tex_mask_x;
	unsigned short	tex_mask_y;	// base mask on block count (>>mipmap)

	vec_uchar16	tex_base_lo;	// together these hold the mipmap base id
	vec_uchar16	tex_base_hi; 	// texture block ids (to guarantee unique)

	unsigned long long 	tex_pixel_base[MAX_MIPMAP_LEVELS]; // the base texture address for block(0,0)
	unsigned short	tex_t_blk_mult[MAX_MIPMAP_LEVELS]; // how to find the offset of a t block (s is easy ;)
} __attribute__ ((aligned(16)));
*/

/*
// this holds a block waiting to be rendered, in whatever state it is in
struct __BLOCK {
	vec_float4	A;			// the weighting of the top left of the block
	Triangle*	triangle;		// used to get parametric data
	vec_float4*	z_buffer;
	vec_uint4*	pixels;
	BlockHandler*	process;

	unsigned int	bx,by;
//	unsigned int	hash;
	unsigned int	left;
} __attribute__ ((aligned(16)));

// this holds the temporary data used when rendering one of the above blocks
struct __ACTIVE {
	u32 pixels[32*32];
	vec_uint4 dma1[16];
	vec_uint4 dma2[16];

	vec_uint4* current_dma;
	vec_uint4* new_dma;
	unsigned long current_length;
	unsigned long eah;
	unsigned long long ea_copy;
	unsigned long long pad;

	vec_ushort8	TEXmerge1,TEXmerge2;	// for texture blits
	unsigned int	texturesMask;
	BlockHandler*	tex_continue;
	unsigned int	temp;
} __attribute__((aligned(16)));
*/


/*
extern void flush_queue();		// push all active blocks to the screen
extern void init_queue(ActiveBlockInit* init, ActiveBlockFlush* flush);
extern void process_queue(TriangleGenerator* generator, BlockActivater* activate);

extern void _init_buffers();
*/



///////////////////////////////////////////////////////////////////////////////








extern int current_state;
extern int imp_clear_screen(float4 in, Context* context);
extern int imp_vertex(float4 in, Context* context);
extern void imp_close_segment(Context* context);
extern int imp_validate_state(int state);
extern int has_finished();

// returns 0 if finished processing queue, non-zero if still processing
extern int stillProcessingQueue(Context* context);



#endif // __SPU_SPUCONTEXT_H
