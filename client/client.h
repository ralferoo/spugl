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

#ifndef __SPUGL_CLIENT_H
#define __SPUGL_CLIENT_H

typedef struct __CommandQueue* SPUGL_QUEUE;

//////////////////////////////////////////////////////////////////////////
//
// Function prototypes

// this returns a connection to the server, passed to all other routines.
// returns negative integer on error
int spuglConnect();

// disconnects the client from the server
void spuglDisconnect(int server);

// waits until command queue has finished processing
void spuglFlush(SPUGL_QUEUE buffer);

// waits until frame flyback
void spuglWait(SPUGL_QUEUE buffer);

// flips the screen
unsigned int spuglFlip(SPUGL_QUEUE buffer);

// allocates/frees an OpenGL FIFO
SPUGL_QUEUE spuglAllocateCommandQueue(int server, unsigned int size);
void spuglFreeCommandQueue(SPUGL_QUEUE buffer);

// allocates/frees a general buffer suitable for textures, pixel buffers, etc.
void* spuglAllocateBuffer(int server, unsigned int size);
void spuglFreeBuffer(void* buffer);

// gets the base buffer address for some random pointer
void* spuglBufferBaseAddress(void* buffer, int* id);

// sends the server an invalid request (for testing - should cause the connection to drop)
void spuglInvalidRequest(int server);

// changes the current OpenGL context
SPUGL_QUEUE spuglSetCurrentContext(SPUGL_QUEUE newContext);

// gets the current screen width and height
void spuglScreenSize(int server, unsigned int* width, unsigned int* height);

///////////////////////////////////
//
// defined in glfifo.c

void spuglDrawContext(unsigned int target);
void spuglJump(unsigned int target);
void spuglNop(void);
unsigned int spuglTarget();

// loads a shader which must be copied to one of the spugl buffers
//unsigned int spuglLoadShader(void* buffer, int length);

#endif // __SPUGL_CLIENT_H
