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

#ifndef __SPU_RENDER_H
#define __SPU_RENDER_H

#ifdef SPU_REGS
#include <spu_mfcio.h>
#include <spu_intrinsics.h>
#endif // SPU_REGS

///////////////////////////////////////////////////////////////////////////////

#define MAX_SHADER_PARAMS			32	// maximum number of parameters in a pixel shader
#define NUMBER_OF_TILES				(64*64)	// number of tiles
#define NUM_MIPMAP_LEVELS			10	// number of mipmap levels in a texture definition

#define NUMBER_OF_TILES_PER_CHUNK		16	// number of tiles an SPU can process at once
#define CHUNK_DIVIDE_THRESHOLD			3	// only subdivide if we have less than this free
							// i _think_ this*num_spus+1 <= 16
						
#define CHUNKNEXT_BUSY_BIT			32	// bit that indicates chunk is in use
#define CHUNKNEXT_MASK				31	// mask to get the "next chunk" pointer
#define CHUNKNEXT_FREE_BLOCK			255	// indicates a block that is free
#define CHUNKNEXT_RESERVED			254	// was free, but now claimed


///////////////////////////////////////////////////////////////////////////////

struct __Renderable;
extern unsigned int _SPUID;

// handle cache line details
void process_render_tasks(unsigned long eah_render_tasks, unsigned long eal_render_tasks);

// process a chunk of render work
unsigned short process_render_chunk(unsigned short chunkStart, unsigned short chunkLength,
				    unsigned short chunkTriangle, unsigned short endTriangle,
				    unsigned long long triangleBase, struct __Renderable* renderable);

///////////////////////////////////////////////////////////////////////////////
//
// Format of the cache line used to drive the render SPUs

typedef struct {
	union {
#ifdef SPU_REGS
		vec_ushort8	chunkTriangle[2];
#endif // SPU_REGS
		unsigned short	chunkTriangleArray[16];
	};
// 32
	union {
#ifdef SPU_REGS
		vec_ushort8	chunkStart[2];
#endif // SPU_REGS
		unsigned short	chunkStartArray[16];
	};
// 64
	union {
#ifdef SPU_REGS
		vec_uchar16	chunkNext;
#endif // SPU_REGS
		unsigned char	chunkNextArray[16];
	};
// 80
	unsigned long long	next;
	unsigned long long	pixelBuffer;
	unsigned long long 	zBuffer;
// 104
	unsigned int		pixelTileDx;			// this will always be 4*32
	unsigned int		pixelTileDy;
	unsigned int		pixelLineDy;
	unsigned int		zTileDx;			// this will always be 4*32 or 2*32...
	unsigned int		zTileDy;
// 124
	unsigned short		endTriangle;			// triangle buffer that is waiting to be filled
// 126
	unsigned short		pad;
// 128
} RenderableCacheLine;

#define TRIANGLE_OFFSET_FROM_CACHE_LINE	128

///////////////////////////////////////////////////////////////////////////////
//
// Format of the triangle data, used by the render SPUs

typedef struct __TRIANGLE Triangle;

struct __TRIANGLE {
	// overlap next_triangle pointer with last word of area_dy as that's not used for anything
	union {
		struct {
#ifdef SPU_REGS
			vec_int4	area, area_dx, area_dy;
#endif // SPU_REGS
		};
		struct {
			unsigned short	pad[8];
		};
	};

	// information about the shader being used
	unsigned long long	shader_ea;
	unsigned short		shader_length;
	unsigned short		next_triangle;
	unsigned int		padding;

	// parameters for the shader (specific to shader)
#ifdef SPU_REGS
	vec_uint4		shader_params[0];
#endif // SPU_REGS
} __attribute__ ((aligned(16)));

#define TRIANGLE_MAX_SIZE	((MAX_SHADER_PARAMS*16)+sizeof(struct __TRIANGLE))

///////////////////////////////////////////////////////////////////////////////
//
// Format of texture data (actually, I suspect this won't be used)

/*
#ifdef SPU_REGS
struct TextureData {
	vec_short8	shifts;		// interleaved shift masks,  odd: log2(height)  (s_blk_max)
					// interleaved shift masks, even: log2(width)	(t_blk_max)
	vec_int4	mipmapshifts;
//32
	vec_uchar16	tex_base_lo;	// together these hold the mipmap base id
	vec_uchar16	tex_base_hi; 	// texture block ids (to guarantee unique)
//64
	unsigned long long tex_pixel_base[NUM_MIPMAP_LEVELS]; // the base texture address for block(0,0)
//144
	unsigned short	tex_t_blk_mult[NUM_MIPMAP_LEVELS]; // how to find the offset of a t block (s is easy ;)
//164
	unsigned short	tex_max_mipmap;	// how many levels of mipmap are present
	unsigned short	tex_mask_x;
	unsigned short	tex_mask_y;	// base mask on block count (>>mipmap)
//170
};
#endif // SPU_REGS
*/

#endif // __SPU_RENDER_H
