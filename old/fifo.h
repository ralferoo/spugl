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

#ifndef __3d_h
#define __3d_h

#include "types.h"

typedef void* DriverContext;

#define SPU_MBOX_3D_TERMINATE 0
#define SPU_MBOX_3D_FLUSH 1
#define SPU_MBOX_3D_INITIALISE_MASTER 2
#define SPU_MBOX_3D_INITIALISE_NORMAL 3

#define __SPUMEM_ALIGNED__ __attribute__((aligned(16)))
#define __CACHE_ALIGNED__ __attribute__((aligned(128)))

#define BYTE_ALIGNMENT 127
#define PIXEL_ALIGNMENT (BYTE_ALIGNMENT>>2)

#include "gen_spu_command_defs.h"

//////////////////////////////////////////////////////////////////////////////

#if defined(USERLAND_32_BITS)
#define _MAKE_EA(x) ( (u64) ((u32)((void*)x)) )
#define _FROM_EA(x) ((void*)((u32)((u64)x)))
#define FIFO_EA_SIZE 1
#define OUT_RINGea(addr) \
{ u64 ea = _MAKE_EA(addr); \
  *__fifo_ptr++ = (u32)(ea&0xffffffffUL); \
} 
#define __READ_EA(from) \
{ u32 _____eal; \
  _____eal = *from++; \
  ea = ((u64)_____eal); \
} 

#elif defined(USERLAND_64_BITS)
#define _MAKE_EA(x) ((u64)((void*)x))
#define _FROM_EA(x) ((void*)((u64)x))
#define FIFO_EA_SIZE 2
#define OUT_RINGea(addr) \
{ u64 _____ea = _MAKE_EA(addr); \
  *__fifo_ptr++ = (u32)(_____ea>>32); \
  *__fifo_ptr++ = (u32)(_____ea&0xffffffffUL); \
} 
#define __READ_EA(from) \
{ u32 eah, eal; \
  eah = *from++; \
  eal = *from++; \
  ea = (((u64)eah)<<32) | ((u64)eal); \
} 

#else
#error Must define USERLAND_32_BITS or USERLAND_64_BITS
#endif

//////////////////////////////////////////////////////////////////////////////

//#define FIFO_PROLOGUE \
//u32* __fifo_ptr = (u32*)_FROM_EA( *_fifo_write_pointer ); \
//printf("prologue %llx\n", __fifo_ptr);

//#define FIFO_EPILOGUE \
//*_fifo_write_pointer = _MAKE_EA(__fifo_ptr);

#define FIFO_PROLOGUE(fifo,minsize) \
DriverContext __fifo = (fifo); \
u32* __fifo_head = _begin_fifo(__fifo, (minsize)*4); \
u32* __fifo_ptr = __fifo_head;

#define FIFO_EPILOGUE(fifo) _end_fifo(__fifo, __fifo_head, __fifo_ptr); \
__fifo_ptr = 0UL;

#define OUT_RING(v) { *(__fifo_ptr)++ = (u32)v; }
#define OUT_RINGf(v) { union __f2i temp; temp.f = v;  *(__fifo_ptr)++ = temp.i; }
#define BEGIN_RING(c,s,sp) OUT_RING(c | (((s)+FIFO_EA_SIZE*(sp))<<24))

union __f2i
{
  float f;
  u32 i;
};

//////////////////////////////////////////////////////////////////////////////

#define SPU_MIN_FIFO 128

// this structure implements a FIFO command buffer.
// the idea was shamelessly stolen from the stuff i learned from the RSX guys
// and it basically means that I can have almost free data transfer... ;)
//
// DMA is good, but this is a bit better still as it doesn't need to be
// marshalled using mailboxes.
//
typedef struct {
	// the first 128 bytes are writable by the PPU-side driver
	volatile u64 fifo_written;	// the address the driver has written
	volatile u64 my_local_address;	// the address this SPU lives at
	volatile u64 fragment_buffer;	// the address of the fragment buffer
	volatile u32 fragment_buflen;	// the length of the fragment buffer
	volatile u32 pad1[32-7];

	// the next 128 bytes are writable by the SPU-side driver
	volatile u64 fifo_read;		// the current position in FIFO read
	volatile u32 fifo_size;		// the length of the FIFO area

	volatile u32 idle_count;	// this is our count of idle cycles
	volatile u32 last_count;	// last value of counter we saw
	volatile u32 error;		// the error code, if any
	volatile u32 block_count;	// this is our count of blocked cycles

	volatile u32 blocks_produced_count;
	volatile u32 cache_miss_count;

	volatile u32 pad2[32-9];
} SPU_CONTROL;

extern u64* _get_fifo_address(DriverContext _context);
extern DriverContext _init_3d_driver(int master);
extern int _exit_3d_driver(DriverContext _context);
extern u32* _begin_fifo(DriverContext _context, u32 minsize);
extern void _end_fifo(DriverContext _context, u32* fifo_head, u32* fifo);
extern void _bind_child(DriverContext _parent, DriverContext _child, int assign);
extern u32 _3d_idle_count(DriverContext _context);
extern u32 _3d_spu_address(DriverContext _context, u32* address);

struct __TRIANGLE;

typedef void* SPU_COMMAND(void* data, struct __TRIANGLE * triangle);

#endif // __3d_h
