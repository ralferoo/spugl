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

#ifdef SPU_REGS
#include <spu_mfcio.h>
#include <spu_intrinsics.h>
#endif // SPU_REGS

#define NUMBER_OF_TILES_PER_CHUNK		7	// number of tiles an SPU can process at once
#define NUMBER_OF_CHUNK_SLOTS_TO_PRESERVE	9	// leave each SPU with a spare chunk to play with

extern unsigned int _SPUID;

// handle cache line details
void process_render_tasks(unsigned long eah_render_tasks, unsigned long eal_render_tasks);

// process a chunk of render work
unsigned short process_render_chunk(unsigned short chunkStart, unsigned short chunkLength,
				    unsigned short chunkTriangle, unsigned short endTriangle,
				    unsigned long long triangleBase, unsigned long long chunkBase);

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
		vec_ushort8	chunkLength[2];
#endif // SPU_REGS
		unsigned short	chunkLengthArray[16];
	};
// 96
	unsigned long long	next;
	unsigned long long	triangleBase;
	unsigned long long	chunkBase;
// 120
	unsigned short	chunksWaiting;				// bitmask of chunks waiting to be rendered
	unsigned short	chunksFree;				// bitmask of chunks not yet allocated
	unsigned short	endTriangle;				// triangle buffer that is waiting to be filled

//	unsigned short	chunksFree; // ~(chunksWaiting|chunksBusy)	// bitmask of chunks that can be allocated
// 126
} RenderableCacheLine ;

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

#endif // __SPU_SPUCONTEXT_H
