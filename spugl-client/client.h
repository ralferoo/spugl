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

// this returns a connection to the server, passed to all other routines.
// returns negative integer on error
int SPUGL_connect();

// disconnects the client from the server
void SPUGL_disconnect(int server);

// allocates/frees an OpenGL FIFO
struct CommandQueue* SPUGL_allocateCommandQueue(int server, unsigned long size);
void SPUGL_freeCommandQueue(int server, struct CommandQueue* buffer);

// allocates/frees a general buffer suitable for textures, pixel buffers, etc.
void* SPUGL_allocateBuffer(int server, unsigned long size);
void SPUGL_freeBuffer(int server, void* buffer);

// sends the server an invalid request (for testing - should cause the connection to drop)
void SPUGL_invalidRequest(int server);
