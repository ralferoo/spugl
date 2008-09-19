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

#include "spuglver.h"

typedef struct __SPUGL_request SPUGL_request;
typedef struct __SPUGL_reply SPUGL_reply;
 
//////////////////////////////////////////////////////////////////////////
//
// This is the structure of the socket level communcations between client
// and server

// TODO: this needs to be made compatible across PPC and PPC64; PPC64 seems to
// TODO: be doing some very wacky alignment stuff... :(

struct __SPUGL_request {
	union {
		struct {
			unsigned short command;
		} header;
		struct {
			unsigned short command;
			unsigned short major, minor, revision;
		} version;
		struct {
			unsigned short command;
			unsigned int size;
		} alloc;
		struct {
			unsigned short command;
			unsigned int id;
		} flush;
		struct {
			unsigned short command;
			unsigned int id;
		} free;
		//struct {
		//	unsigned char size[16];
		//} pad;
	};
};

#define SPUGLR_GET_VERSION		1
#define SPUGLR_NEGOTIATE_VERSION	2
#define SPUGLR_ALLOC_COMMAND_QUEUE	3
#define SPUGLR_ALLOC_BUFFER		4
#define SPUGLR_MAP_BUFFER		5
#define SPUGLR_FREE_COMMAND_QUEUE	6
#define SPUGLR_FREE_BUFFER		7
#define SPUGLR_FLUSH			8
#define SPUGLR_SCREEN_SIZE		9
#define SPUGLR_SYNC			10
#define SPUGLR_FLIP			11

struct __SPUGL_reply {
	union {
		struct {
			unsigned short major, minor, revision;
		} version;
		struct {
			unsigned int id;
		} alloc;
		struct {
			unsigned int width, height;
		} screensize;
		//struct {
		//	unsigned char size[16];
		//} pad;
	};
};
