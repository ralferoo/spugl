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

#include "GL/gl.h"

static void* _alloc_ptr = NULL;
static void* _alloc_ptr_alloc = NULL;

void alloc_init(int server) {
	_alloc_ptr_alloc = spuglAllocateBuffer(server, 24*1024*1024);
	_alloc_ptr = (void*) (( ((int)_alloc_ptr_alloc) + 127) &~ 127 );
	if (_alloc_ptr==NULL) { printf("Out of memory\n"); exit(1); }
}

void alloc_destroy(void)
{
	spuglFreeBuffer(_alloc_ptr_alloc);
}

// Returns a 128 byte-aligned buffer
void* alloc_aligned(unsigned int length) {
	void* result = _alloc_ptr;
	if (_alloc_ptr)
		_alloc_ptr += (length+127)&~127;

	return result;
}

void alloc_free(void* addr) {
	// NO-OP
}


