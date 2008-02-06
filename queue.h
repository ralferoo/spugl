/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

#ifndef __queue_h
#define __queue_h

#define QUEUE_PADDING 17

#include "types.h"
#include <spu_intrinsics.h>

typedef struct __QUEUE Queue;

struct __QUEUE {
	void		(*handler)(Queue*);	// the dispatch that knows how to 
	unsigned int	id;			// the id of this command
	unsigned int	next;			// the command to process after this one is finished
	unsigned int	dmamask;		// the DMA mask (if any) of this command
		
	union {	
		struct {
			vec_float4	x,y,z,w;	// coords
			vec_float4	r,g,b,a;	// primary colour
			vec_float4	s,t,u,v;	// primary texture
			vec_float4	A,dAdx,dAdy;	// weight information
			vec_float4	minmax;		// bounding box (xmin,ymin,xmax,ymax)

			u32 		texture;
			void *		shader;
			unsigned int dummy;
		} triangle;
		vec_uchar16 padding[QUEUE_PADDING];
	};
} ;


// http://www.thescripts.com/forum/thread220022.html
//
// If you get an error on the next line, you need to increase the size of
// QUEUE_PADDING at the top of this file...
typedef char constraint_violated[1 - 2*(sizeof(struct __QUEUE) != 16*(1+QUEUE_PADDING))];

///////////////////////////////////////////////////////////////////////////////

#endif // __queue_h

