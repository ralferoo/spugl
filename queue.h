/****************************************************************************
 *
 * SPU 3d rasterisation library
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

#define NUMBER_OF_TRIS	16
#define NUMBER_OF_QUEUED_BLOCKS 32
#define NUMBER_OF_ACTIVE_BLOCKS 4

/*
typedef struct __QUEUE OLD_Queue;

typedef struct {
	// block has been added, this is it's initial function
	void (*init)(Block*);

	// the is the main render function
	vec_ushort8 (*process)(Block* queue,
		vec_float4 Aa,vec_float4 Ab,vec_float4 Ac,
		vec_float4 Aa_dx4,vec_float4 Ab_dx4,vec_float4 Ac_dx4,
		vec_float4 Aa_dy,vec_float4 Ab_dy,vec_float4 Ac_dy);
} RenderFuncs;
*/


typedef struct __BLOCK Block;
typedef struct __TRIANGLE Triangle;
typedef struct __ACTIVE ActiveBlock;

typedef int (TriangleGenerator)(Triangle* tri);
typedef int (TriangleHandler)(Triangle* tri, Block* block);
typedef void* (BlockHandler)(void* self, Block* block);
typedef void (BlockActivater)(Block* block, ActiveBlock* active);

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
	vec_float4	A,A_dx,A_dy;	// the weighting of the top left of the block
	Triangle*	triangle;	// used to get parametric data
	vec_float4*	z_buffer;
	vec_uint4*	pixels;
	char*		tex_temp;
	BlockHandler*	process;
	char		tex_override;

	unsigned int	bx,by;

	vec_ushort8	TEXmerge1,TEXmerge2;	// for texture blits
	unsigned int	texturesMask;
} __attribute__ ((aligned(16)));

struct __ACTIVE {
	u32 pixels[32*32];
	char textemp[32*8];
	vec_uint4 dma1[16];
	vec_uint4 dma2[16];

	vec_uint4* current_dma;
	vec_uint4* new_dma;
	unsigned long current_length;
	unsigned long eah;
	unsigned long tagid;
	unsigned long pad;
} __attribute__((aligned(16)));


/*
#define NUMBER_OF_QUEUE_JOBS 32
#define QUEUE_PADDING 18


struct __QUEUE {
	void		(*handler)(Queue*);	// the dispatch that knows how to 
	char*		name;			// the debug name
	unsigned short	id;			// the command ID
		 short	next;			// the command to process after this one is finished
	unsigned int	dmamask;		// the DMA mask (if any) of this command
		
	union {	

		// padding, number above must be at least as big as number of qwords in any struct
		vec_uchar16 padding[QUEUE_PADDING];
	};
} ;
// http://www.thescripts.com/forum/thread220022.html
//
// If you get an error on the next line, you need to increase the size of
// QUEUE_PADDING at the top of this file...
typedef char constraint_violated[1 - 2*(sizeof(struct __QUEUE) != 16*(1+QUEUE_PADDING))];

///////////////////////////////////////////////////////////////////////////////
*/


extern void init_queue(void);
extern void process_queue(TriangleGenerator* generator, BlockActivater* activate);

extern void _init_buffers();

/*
extern void debug_queue(void);
extern Queue job_queue[];
extern unsigned int free_job_queues;
extern unsigned int ready_job_queues;
extern unsigned int dma_wait_job_queues;


#define ENQUEUE_JOB(q,h) do {Queue* a=(q); a->handler=h; a->name=(#h); \
if (a->handler) free_job_queues&=~(1<<a->id); else free_job_queues|=(1<<a->id); \
} while(0)
#define QUEUE_JOB(q,h) do {Queue* a=(q); a->handler=h; a->name=(#h);} while(0)
#define READY_JOB(q) (ready_job_queues|=1<<(q)->id)
#define BLOCK_JOB(q) (ready_job_queues&=~(1<<(q)->id))

static inline int FIRST_JOB(unsigned int x) {
	vec_uint4 xx = (vec_uint4) x;
	vec_uint4 clz = spu_cntlz(xx);
	return (int) 31-spu_extract(clz,0);
}

static inline int COUNT_ONES(unsigned int x)
{
	vec_uchar16 xx = (vec_uchar16) ((vec_uint4)x);
	vec_uchar16 yy = spu_cntb(xx);
	vec_ushort8 zz = (vec_ushort8) spu_sumb(yy,yy);
	return spu_extract(zz,1);
}

// we want to keep the FIFO jobs from filling up the entire queue, so reserve
// a few slots for other things...
#define FIFO_VALID_QUEUE_MASK (~0xff)

// this rather convuluted expression is just (1<<n)-1 but rearranged to 
// get rid of the warnings :(
//= (1<<NUMBER_OF_QUEUE_JOBS)-1;
#define ALL_QUEUE_JOBS ((((1<<(NUMBER_OF_QUEUE_JOBS-2))-1)<<2)|3)
*/

///////////////////////////////////////////////////////////////////////////////

#endif // __queue_h

