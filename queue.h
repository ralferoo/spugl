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

#ifndef __queue_h
#define __queue_h

#include "types.h"
#include <spu_intrinsics.h>

///////////////////////////////////////////////////////////////////////////////
//
// these might be interesting to tweak, particularly as reducing these values will leave more
// room for textures, at the expense of possibly increased latency for very small triangles
//
// NUMBER_OF_TRIS		no limit, but there's no advantage to having this bigger than blocks
// NUMBER_OF_QUEUED_BLOCKS	maximum and optimally 32; needs to fit in bitmask
// NUMBER_OF_ACTIVE_BLOCKS	should be at least 2; any more than 4 and the DMA queue could fill

#define NUMBER_OF_TRIS	10	
#define NUMBER_OF_QUEUED_BLOCKS 32
#define NUMBER_OF_ACTIVE_BLOCKS 3

typedef struct __BLOCK Block;
typedef struct __TRIANGLE Triangle;
typedef struct __ACTIVE ActiveBlock;

typedef int (TriangleGenerator)(Triangle* tri);
typedef int (TriangleHandler)(Triangle* tri, Block* block);
typedef void* (BlockHandler)(void* self, Block* block, int tag);
typedef void (BlockActivater)(Block* block, ActiveBlock* active, int tag);
typedef void (ActiveBlockInit)(ActiveBlock* active);
typedef void (ActiveBlockFlush)(ActiveBlock* active, int tag);

// this holds a triangle, i.e. something that creates blocks to be rendered
struct __TRIANGLE {
	vec_float4	x,y,z,w;	// coords
	vec_float4	r,g,b,a;	// primary colour
	vec_float4	s,t,u,v;	// primary texture

	vec_float4	A,A_dx,A_dy;	// weight information
	vec_float4	minmax;		// bounding box (xmin,ymin,xmax,ymax)

	TriangleHandler*	produce;
	BlockHandler*	init_block;

		 short	left;		// count of blocks left to produce
	unsigned short	count;		// count of blocks that still have reference
	unsigned long	texture_base;	// the base texture address for block(0,0)
	unsigned char	texture_y_shift;// log2(texture_width_in_blocks)
	unsigned char	step, cur_x, cur_y;	// current x and y values
	unsigned short	tex_id_base;	// base of texture ids (to guarantee unique)
	unsigned short	tex_id_mask;	// mask of valid bits of texture id
} __attribute__ ((aligned(16)));

// this holds a block waiting to be rendered, in whatever state it is in
struct __BLOCK {
	vec_float4	A,A_dx,A_dy,A_dx4;	// the weighting of the top left of the block
	Triangle*	triangle;		// used to get parametric data
	vec_float4*	z_buffer;
	vec_uint4*	pixels;
	char*		tex_temp;
	BlockHandler*	process;
	char		tex_override;

	unsigned int	bx,by;
	unsigned int	hash;
	unsigned int	left;

	vec_ushort8	TEXmerge1,TEXmerge2;	// for texture blits
	unsigned int	texturesMask;
} __attribute__ ((aligned(16)));

// this holds the temporary data used when rendering one of the above blocks
struct __ACTIVE {
	u32 pixels[32*32];
	char textemp[32*8];
	vec_uint4 dma1[16];
	vec_uint4 dma2[16];

	vec_uint4* current_dma;
	vec_uint4* new_dma;
	unsigned long current_length;
	unsigned long eah;
	unsigned long long ea_copy;
} __attribute__((aligned(16)));


extern void flush_queue();		// push all active blocks to the screen
extern void init_queue(ActiveBlockInit* init, ActiveBlockFlush* flush);
extern void process_queue(TriangleGenerator* generator, BlockActivater* activate);

extern void _init_buffers();

///////////////////////////////////////////////////////////////////////////////

#endif // __queue_h

