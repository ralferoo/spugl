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

#ifndef __client_fifo_h
#define __client_fifo_h

#include "gen_command_defs.h"

typedef struct __CommandQueue CommandQueue;

//////////////////////////////////////////////////////////////////////////////
//
// This is the definition of the queue structure (pretty simple)

struct __CommandQueue {
	unsigned int write_ptr;	// relative to &write_ptr
	unsigned int read_ptr;		// relative to &write_ptr

	unsigned int buffer_start;
	unsigned int buffer_end;
	unsigned int pad[128/4 - 4];	// doesn't really need to be reserved, but might as well be

	unsigned int data[0];
};

//////////////////////////////////////////////////////////////////////////////
//
// Used to store a float on the queue

union __f2i
{
  float f;
  unsigned int i;
};

//////////////////////////////////////////////////////////////////////////////
//
// These are macros to generate the queue commands, PPC only

#ifdef __PPC__

extern CommandQueue* _SPUGL_fifo;

#define FIFO_PROLOGUE(minsize) \
unsigned int* __fifo_ptr = (unsigned int*) ( ((void*)_SPUGL_fifo) + _SPUGL_fifo->write_ptr );

#define FIFO_EPILOGUE(fifo) \
__asm__("lwsync"); \
_SPUGL_fifo->write_ptr = ((void*)__fifo_ptr) - ((void*)_SPUGL_fifo); \
__asm__("lwsync"); \
__fifo_ptr = 0UL;

#define OUT_RING(v) { *(__fifo_ptr)++ = (unsigned int)v; }
#define OUT_RINGf(v) { union __f2i temp; temp.f = v;  *(__fifo_ptr)++ = temp.i; }
#define BEGIN_RING(c,s) OUT_RING(c | (s<<24))

/*
inline unsigned int* _begin_fifo(CommandQueue* queue, unsigned int minsize) {
	unsigned int* ptr = (unsigned int*) ( ((void*)queue) + queue->write_ptr );
	return ptr;
}

inline void _end_fifo(CommandQueue* queue, unsigned int* ptr) {
	__asm__("lwsync");
	queue->write_ptr = ((void*)ptr) - ((void*)queue);
	__asm__("lwsync");
}
*/

#endif // __PPC__

//////////////////////////////////////////////////////////////////////////////

#endif //  __client_fifo_h
