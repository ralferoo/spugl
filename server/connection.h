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

#ifndef __SERVER_CONNECTION_H
#define __SERVER_CONNECTION_H

//////////////////////////////////////////////////////////////////////////////
// Defines the maximum number of PPU/SPU buffers on the system.
//
// This is also used get the offset of the EA addresses for each buffer
// relative to the EA of the lock buffers, which is what is initially
// passed into the SPU function.
//
// The first MAX_COMMAND_BUFFERS are used only for command queues, the rest
// are suitable for general purpose data.

#define MAX_DATA_BUFFERS	1024	// power of two, must be >=128
#define MAX_COMMAND_BUFFERS	128	// power of two, must be <=128

//////////////////////////////////////////////////////////////////////////////

extern char SPUGL_VERSION[];

typedef struct __Allocation Allocation;
typedef struct __Connection Connection;
typedef struct __ConnectionList ConnectionList;
typedef struct __SPU_HANDLE* SPU_HANDLE;

//////////////////////////////////////////////////////////////////////////////
//
// List of client connections in the system

struct __Connection {
	// populated by main.c
	Connection* nextConnection;
	int fd;
	
	// populated by connection.c
	Allocation* firstAllocation;
};

//////////////////////////////////////////////////////////////////////////////
//
// List of client's allocations

struct __Allocation {
	Allocation* nextAllocation;
	void* buffer;
	unsigned long size;
	unsigned long id;
	int fd;
	int flags;
	int locks;
};

#define ALLOCATION_FLAGS_ISCOMMANDQUEUE 1
#define ALLOCATION_FLAGS_FLUSHWAIT 2
#define ALLOCATION_FLAGS_FLUSHDONE 4

//////////////////////////////////////////////////////////////////////////////
//
// Main list of connections on server

struct __ConnectionList {
	Connection* first;
	Connection* firstClosed;
};

#endif // __SERVER_CONNECTION_H
