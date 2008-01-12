/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

#ifndef __3d_h

typedef unsigned int u32;
typedef unsigned long long u64;
typedef void* DriverContext;

#define SPU_MBOX_3D_TERMINATE 0
#define SPU_MBOX_3D_FLUSH 1
#define SPU_MBOX_3D_INITIALISE_MASTER 2
#define SPU_MBOX_3D_INITIALISE_NORMAL 3

#define __SPUMEM_ALIGNED__ __attribute__((aligned(16)))
#define __CACHE_ALIGNED__ __attribute__((aligned(128)))

#define SPU_COMMAND_NOP 0
#define SPU_COMMAND_JMP 1
#define SPU_COMMAND_ADD_CHILD 2
#define SPU_COMMAND_DEL_CHILD 3

union __f2i
{
  float f;
  u32 i;
};

#define _OUTu(fifo,v) { *(fifo)++ = (u32)v; }
#define _OUTf(fifo,v) { union __f2i temp; temp.f = v;  *(fifo)++ = temp.i; }

#if defined(USERLAND_32_BITS)
#define _MAKE_EA(x) ( (u64) ((u32)((void*)x)) )
#define _FROM_EA(x) ((void*)((u32)((u64)x)))
#define _OUT_EA(fifo,addr) \
{ u64 ea = _MAKE_EA(addr); \
  *fifo++ = (u32)(ea&0xffffffffUL); \
} 
#define __READ_EA(from) \
{ u32 eal; \
  eal = *from++; \
  ea = ((u64)eal); \
} 

#elif defined(USERLAND_64_BITS)
#define _MAKE_EA(x) ((u64)((void*)x))
#define _FROM_EA(x) ((void*)((u64)x))
#define _OUT_EA(fifo,addr) \
{ u64 ea = _MAKE_EA(addr); \
  *fifo++ = (u32)(ea>>32); \
  *fifo++ = (u32)(ea&0xffffffffUL); \
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
	volatile u32 pad1[32-4];

	// the next 128 bytes are writable by the SPU-side driver
	volatile u64 fifo_read;		// the current position in FIFO read
	volatile u32 fifo_start;	// the start of the FIFO area
	volatile u32 fifo_size;		// the length of the FIFO area

	volatile u32 idle_count;	// this is our count of idle cycles
	volatile u32 last_count;	// last value of counter we saw
	volatile u32 pad2[32-6];
} SPU_CONTROL;

extern DriverContext _init_3d_driver(int master);
extern int _exit_3d_driver(DriverContext _context);
extern u32* _begin_fifo(DriverContext _context);
extern void _end_fifo(DriverContext _context, u32* fifo);
extern void _bind_child(DriverContext _parent, DriverContext _child, int assign);
extern u32 _3d_idle_count(DriverContext _context);
extern u32 _3d_spu_address(DriverContext _context, u32* address);




#endif // __3d_h
