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
void spuglFlip(SPUGL_QUEUE buffer);

// allocates/frees an OpenGL FIFO
SPUGL_QUEUE spuglAllocateCommandQueue(int server, unsigned int size);
void spuglFreeCommandQueue(SPUGL_QUEUE buffer);

// allocates/frees a general buffer suitable for textures, pixel buffers, etc.
void* spuglAllocateBuffer(int server, unsigned int size);
void spuglFreeBuffer(void* buffer);

// sends the server an invalid request (for testing - should cause the connection to drop)
void spuglInvalidRequest(int server);

// changes the current OpenGL context
SPUGL_QUEUE spuglSetCurrentContext(SPUGL_QUEUE newContext);

// gets the current screen width and height
void spuglScreenSize(int server, unsigned int* width, unsigned int* height);

#endif // __SPUGL_CLIENT_H
