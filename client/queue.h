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

//////////////////////////////////////////////////////////////////////////
//
// This is the simple structure of a command queue

typedef struct __CommandQueue CommandQueue;

struct __CommandQueue {
	unsigned long write_ptr;	// relative to &write_ptr
	unsigned long read_ptr;		// relative to &write_ptr

	unsigned long data[0];
};
