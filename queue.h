/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

#ifndef __queue_h
#define __queue_h

#include "types.h"
#include <spu_intrinsics.h>

#define NUMBER_OF_QUEUE_JOBS 32

typedef struct __QUEUE Queue;

struct __QUEUE {
	void		(*handler)(Queue*);	// the dispatch that knows how to 
	char*		name;			// the debug name
	unsigned short	id;			// the command ID
		 short	next;			// the command to process after this one is finished
	unsigned int	dmamask;		// the DMA mask (if any) of this command
		
	union {	
		// this holds a triangle, i.e. something that creates blocks to be rendered
		struct {
			vec_float4	x,y,z,w;	// coords
			vec_float4	r,g,b,a;	// primary colour
			vec_float4	s,t,u,v;	// primary texture
			vec_float4	A,dAdx,dAdy;	// weight information
			vec_float4	minmax;		// bounding box (xmin,ymin,xmax,ymax)

			unsigned int	count;		// count of blocks that still have reference
			void		(*init)(Queue*);// the dispatch that can initialise itself
			unsigned short	texture_base;	// the base texture ID for block(0,0)
			unsigned short	texture_y_shift;// log2(texture_width_in_blocks)
		} triangle;

		// this holds a block waiting to be rendered, in whatever state it is in
		struct {
			vec_float4	Aa,Ab,Ac;
			vec_float4	Aa_dx4,Ab_dx4,Ac_dx4;
			vec_float4	Aa_dy,Ab_dy,Ac_dy;
			
			Queue*		triangle;
			vec_float4*	z_buffer;
			vec_uint4*	pixels;
			vec_ushort8*	tex_temp;
		} block;

#define QUEUE_PADDING 17
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

extern Queue job_queue[];
extern unsigned int free_job_queues;
extern unsigned int ready_job_queues;

extern void debug_queue(void);
extern void process_queue(void);

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

///////////////////////////////////////////////////////////////////////////////

#endif // __queue_h

