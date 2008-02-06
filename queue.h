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
	unsigned int	id;			// the id of this command
	unsigned int	next;			// the command to process after this one is finished
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

///////////////////////////////////////////////////////////////////////////////

#endif // __queue_h

