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

typedef void* DriverContext;

#define __SPUMEM_ALIGNED__ __attribute__((aligned(16)))
#define __CACHE_ALIGNED__ __attribute__((aligned(128)))

#define BYTE_ALIGNMENT 127
#define PIXEL_ALIGNMENT (BYTE_ALIGNMENT>>2)

#include "gen_command_defs.h"

//////////////////////////////////////////////////////////////////////////////

#define FIFO_PROLOGUE(fifo,minsize) \
CommandQueue* __fifo = (fifo); \
unsigned int* __fifo_ptr = _begin_fifo(__fifo, (minsize)*4);

#define FIFO_EPILOGUE(fifo) _end_fifo(__fifo, __fifo_ptr); \
__fifo_ptr = 0UL;

#define OUT_RING(v) { *(__fifo_ptr)++ = (unsigned int)v; }
#define OUT_RINGf(v) { union __f2i temp; temp.f = v;  *(__fifo_ptr)++ = temp.i; }
#define BEGIN_RING(c,s) OUT_RING(c | (s<<24))

union __f2i
{
  float f;
  unsigned int i;
};

//////////////////////////////////////////////////////////////////////////////

#define SPU_MIN_FIFO 128

inline unsigned int* _begin_fifo(CommandQueue* queue, unsigned int minsize) {
	unsigned int* ptr = (unsigned int*) ( ((void*)queue) + queue->write_ptr );
	return ptr;
}

inline void _end_fifo(CommandQueue* queue, unsigned int* ptr) {
	__asm__("lwsync");
	queue->write_ptr = ((void*)ptr) - ((void*)queue);
	__asm__("lwsync");
}

struct __TRIANGLE;

typedef void* SPU_COMMAND(void* data, struct __TRIANGLE * triangle);

#endif //  __client_fifo_h
