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
	volatile u32 fifo_written;	// the address the driver has written
	volatile u32 pad0;
	volatile u64 my_local_address;	// the address this SPU lives at
	volatile u32 pad1[32-4];

	// the next 128 bytes are writable by the SPU-side driver
	volatile u32 fifo_start;	// the start of the FIFO area
	volatile u32 fifo_size;		// the length of the FIFO area
	volatile u32 fifo_read;		// the current position in FIFO read

	volatile u32 idle_count;	// this is our count of idle cycles
	volatile u32 last_count;	// last value of counter we saw
	volatile u32 pad2[32-5];
} SPU_CONTROL;

#endif // __3d_h
