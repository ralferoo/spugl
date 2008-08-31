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

#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define VERSION_REVISION 25
#define VERSION_STRING "0.1.25"

//////////////////////////////////////////////////////////////////////////
//
// This is the structure of the socket level communcations between client
// and server

struct SPUGL_request {
	unsigned short command;
	union {
		struct {
			unsigned short major, minor, revision;
		} version;
		struct {
			unsigned long size;
		} alloc;
	};
};

#define SPUGLR_GET_VERSION		1
#define SPUGLR_NEGOTIATE_VERSION	2
#define SPUGLR_ALLOC_COMMAND_QUEUE	3
#define SPUGLR_ALLOC_BUFFER		4
#define SPUGLR_MAP_BUFFER		5
#define SPUGLR_FREE_BUFFER		6

struct SPUGL_reply {
	union {
		struct {
			unsigned short major, minor, revision;
		} version;
		struct {
			unsigned long id;
		} alloc;
	};
};
		
//////////////////////////////////////////////////////////////////////////
//
// This is the simple structure of a command queue

struct CommandQueue {
	unsigned long write_ptr;	// relative to &write_ptr
	unsigned long read_ptr;		// relative to &write_ptr
	unsigned long data[0];
};
