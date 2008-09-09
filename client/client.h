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

typedef struct __CommandQueue* SPUGL_QUEUE;

//////////////////////////////////////////////////////////////////////////
//
// Function prototypes

// this returns a connection to the server, passed to all other routines.
// returns negative integer on error
int glspuConnect();

// disconnects the client from the server
void glspuDisconnect(int server);

// waits until command queue has finished processing
void glspuFlush(SPUGL_QUEUE buffer);

// allocates/frees an OpenGL FIFO
SPUGL_QUEUE glspuAllocateCommandQueue(int server, unsigned int size);
void glspuFreeCommandQueue(SPUGL_QUEUE buffer);

// allocates/frees a general buffer suitable for textures, pixel buffers, etc.
void* glspuAllocateBuffer(int server, unsigned int size);
void glspuFreeBuffer(void* buffer);

// sends the server an invalid request (for testing - should cause the connection to drop)
void glspuInvalidRequest(int server);

// changes the current OpenGL context
SPUGL_QUEUE glspuSetCurrentContext(SPUGL_QUEUE newContext);
