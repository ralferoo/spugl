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

#define NUMBER_OF_TILES				4096

#define NUMBER_OF_TILES_PER_CHUNK		16	// number of tiles an SPU can process at once
#define CHUNK_DIVIDE_THRESHOLD			3	// only subdivide if we have less than this free
							// i _think_ this*num_spus+1 <= 16
						
#define CHUNK_NEXT_MASK				31
#define CHUNK_NEXT_END				64	// mostly so it wraps around
#define CHUNK_NEXT_INVALID			255	// if next chunk == 255, then it's free
#define CHUNK_NEXT_BUSY_BIT			32
#define CHUNK_NEXT_RESERVED			254	// was free, but now claimed

struct __Renderable;
extern unsigned int _SPUID;

// handle cache line details
void process_render_tasks(unsigned long eah_render_tasks, unsigned long eal_render_tasks);

// process a chunk of render work
unsigned short process_render_chunk(unsigned short chunkStart, unsigned short chunkLength,
				    unsigned short chunkTriangle, unsigned short endTriangle,
				    unsigned long long triangleBase, struct __Renderable* renderable);

/*
 *
 * vec_ushort8	[2]	chunkstart[16]	32 bytes	  0  32
 * vec_ushort8	[2]	triangle[16]	32 bytes	 32  64
 * vec_uchar16	[1]	chunknext[16]	16 bytes	 64  80
 *
 * unsigned long long	nextcachethingy	 8 bytes	 80  88
 * unsigned long long	memorybuffer	 8 bytes	 88  96
 * unsigned int		id		 4 bytes	 96 100
 * unsigned short	width		 2 bytes	100 102
 * unsigned short	height		 2 bytes	102 104
 * unsigned int		stride		 4 bytes	104 108
 * unsigned int		format		 4 bytes	108 112
 * unsigned int		width		 4 bytes	112 116
 * unsigned short	endTriangle	 2 bytes	116 118
 * 
 */

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
// 96
	unsigned int		pixelTotalDx;
	unsigned int		pixelTotalDy;
// 104
	unsigned short		endTriangle;			// triangle buffer that is waiting to be filled
// 106
//	22 bytes free
} RenderableCacheLine ;

#define TRIANGLE_OFFSET_FROM_CACHE_LINE	128

struct RenderableChunk {
	unsigned long	pixel_eal;
	unsigned long	depth_eal;
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



#endif // __SPU_RENDER_H
