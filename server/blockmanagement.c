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

// #define DEBUG

#include "connection.h"
#include <stdlib.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>

#include "renderspu/render.h"

static void*			_block_mgr_buffer		= NULL;
static signed char*		_block_mgr_lock_table		= NULL;
static unsigned long long*	_block_mgr_ea_table		= NULL;
static Renderable*		_block_mgr_renderables_table	= NULL;
static unsigned long long*	_block_mgr_render_tasks		= NULL;

unsigned long long* blockManagementGetRenderTasksPointer(void)
{
	return _block_mgr_render_tasks;
}

// TODO: there should be management around this, but currently it's only used for the framebuffer...
Renderable* blockManagementGetRenderable(int id)
{
	int rid = id & (MAX_RENDERABLES-1);
	if (rid>=MAX_RENDERABLES)
		return NULL;

	Renderable* result = _block_mgr_renderables_table + rid;
	if (result->id == id)
		return result;

	return NULL;
}

// TODO: there should be management around this, but currently it's only used for the framebuffer...
// TODO: also, this needs a corresponding delete function!
int blockManagementCreateRenderable(void* buffer, int width, int height, int stride)
{
	for (int id=0; id<MAX_RENDERABLES; id++) {
		Renderable* result = _block_mgr_renderables_table + id;
		if (result->ea == 0ULL) {
			id |= (rand() << 16) & (~MAX_RENDERABLES);
			result->ea = (unsigned long long) ( (unsigned long)buffer );
			result->id = id;
			result->width = width;
			result->height = height;
			result->stride = stride;
			result->format = 0;
			result->locks = 0;

			result->memoryBuffer = malloc(TRIANGLE_BUFFER_SIZE + 128 + 127);

			// calculate the cache line address
			RenderableCacheLine* cacheLine = (RenderableCacheLine*)
						( ((unsigned int)result->memoryBuffer+127)&~127);
			result->cacheLine = (unsigned long long) ( (unsigned long)cacheLine );

			// populate the cache line
			memset(cacheLine, 0, 128);
			cacheLine->chunkTriangleArray[0] = 0;
			cacheLine->chunkStartArray[0] = 0;
			memset(&cacheLine->chunkNextArray, CHUNKNEXT_FREE_BLOCK, 16);
			cacheLine->chunkNextArray[0] = 0;
			cacheLine->endTriangle = 0;
			cacheLine->pixelBuffer = (unsigned long long) ( (unsigned long)buffer );
			cacheLine->pixelTileDx = 4 * 32;
			cacheLine->pixelTileDy = stride * 32;
			cacheLine->pixelLineDy = stride;
			cacheLine->maxBlockX = (width>>5)-1;
			cacheLine->maxBlockY = (height>>5)-1;

			// chain in the cache line
			cacheLine->next = *_block_mgr_render_tasks;
			*_block_mgr_render_tasks = (unsigned long long) ( (unsigned long)cacheLine );

#ifdef INFO
			printf("Renderable id %x is %x to %x, cache at %x\n",
				id, buffer, buffer + stride*height, cacheLine);
#endif

			return id;
		}
	}
	return -1;
}

void blockManagementDebug()
{
#ifdef DEBUG
	char buffer[MAX_DATA_BUFFERS+4];
	char *last=buffer, *next=buffer;
	*next = '!';
	for (int i=0; i<MAX_DATA_BUFFERS; i++) {
		signed char v = _block_mgr_lock_table[i];
		char c = v+'0';
		if (i==MAX_COMMAND_BUFFERS) {
			*last++='.';
			*last++='.';
			*last++='.';
			next = last;
		}
		if (v<0) {
			if (v==-1) c='-';
			else if (v==-2) c='²';
			else if (v==-3) c='³';
			else c='*';
		} else {
			if (v>9) c='+';
			last = next;
		}
		*next++ = c;
	}
	*++last = 0;
	printf("DEBUG: %s\n", buffer);
#endif
}

// initialises the block management system
void *blockManagementInit() 
{
	_block_mgr_buffer = malloc(127 +
				   MAX_DATA_BUFFERS * ( sizeof(signed char)+sizeof(unsigned long long) ) +
				   MAX_RENDERABLES * sizeof(Renderable) +
				   sizeof(unsigned long long) );
	void* _aligned = (void*) ((((unsigned int)_block_mgr_buffer)+127)&~127);

	_block_mgr_lock_table = (signed char*) (_aligned + 0);
	_block_mgr_ea_table = (unsigned long long*) (_block_mgr_lock_table+MAX_DATA_BUFFERS);
	_block_mgr_renderables_table = (Renderable*) (_block_mgr_ea_table+MAX_DATA_BUFFERS);
	_block_mgr_render_tasks = (unsigned long long*) (_block_mgr_renderables_table+MAX_RENDERABLES);

	void* _end = (void*) (_block_mgr_render_tasks+1);

	// printf("Render tasks at %x, end buffer at %x\n", _block_mgr_render_tasks, _end);

	memset(_block_mgr_lock_table, -1, MAX_DATA_BUFFERS);
	memset(_block_mgr_ea_table, 0, MAX_DATA_BUFFERS*sizeof(long long));
	memset(_block_mgr_renderables_table, 0, MAX_RENDERABLES*sizeof(Renderable));
	*_block_mgr_render_tasks = 0ULL;

	blockManagementDebug();

	return _block_mgr_lock_table;
}

// frees any memory used by the block management system, returns non-zero on success
int blockManagementDestroy()
{
	free(_block_mgr_buffer);
	_block_mgr_buffer = NULL;
	_block_mgr_lock_table = NULL;
	_block_mgr_ea_table = NULL;
}

// allocates a block from the system, storing the EA alongside it, returns block ID
// the initial usage count is set to 0
unsigned int blockManagementAllocateBlock(void* ptr, int commandQueue)
{
	unsigned long long ea = (unsigned long) ptr;

	int start,end;
	if (commandQueue) {
		start = 0;
		end = MAX_COMMAND_BUFFERS;
	} else {
		start = MAX_COMMAND_BUFFERS;
		end = MAX_DATA_BUFFERS;
	}
	signed char* lock_ptr = _block_mgr_lock_table + start;

	for (int i=start; i<end; i+=4, lock_ptr+=4) {
		typedef  struct {char a[4];} wordsize;
		wordsize *ptrp = (wordsize*)lock_ptr;
		unsigned int result;
	retry:
		__asm__ volatile ("lwarx %0,%y1" : "=r" (result) : "Z" (*ptrp));     
		if (result & 0x80808080) {
			union { signed char a[4]; unsigned int b; } conv;
			conv.b = result;
			for (int j=0; j<4; j++) {
				signed char v = conv.a[j];
				if (v < 0) {
					_block_mgr_ea_table[i+j] = ea;
					__asm__ volatile ("lwsync");
					conv.a[j] = 1;
					unsigned int value = conv.b;
					__asm__ volatile ("stwcx. %2,%y1\n"
							"\tmfocrf %0,0x80" : "=r" (result), "=Z" (*ptrp) : "r" (value));
					if (result & 0x20000000) {
						blockManagementDebug();
						return (i+j) | (rand()<<16);
					}
					sched_yield();
					goto retry;
				}
			}
		}
	}
	blockManagementDebug();
	return OUT_OF_BUFFERS;
}


// decrement the usage count of the block
void blockManagementBlockCountDispose(unsigned int id)
{
	signed char* lock_ptr = _block_mgr_lock_table + (id & BLOCK_ID_MASK & ~3);
	typedef  struct {char a[4];} wordsize;
	wordsize *ptrp = (wordsize*)lock_ptr;
	unsigned int result;
retry:
	__asm__ volatile ("lwarx %0,%y1" : "=r" (result) : "Z" (*ptrp));     
//	__asm__ volatile ("\n.retryDispose:\n\tlwarx %0,%y1" : "=r" (result) : "Z" (*ptrp));     

	union { signed char a[4]; unsigned int b; } conv;
	conv.b = result;
	conv.a[id&3]--;
	unsigned int value = conv.b;

	__asm__ volatile ("stwcx. %2,%y1\n"
			"\tmfocrf %0,0x80" : "=r" (result), "=Z" (*ptrp) : "r" (value));
	if (!(result & 0x20000000)) {
		sched_yield();
		goto retry;
	}
//	__asm__ volatile ("stwcx. %1,%y0\n\tbne- retryDispose" : "=Z" (*ptrp) : "r" (value));
	blockManagementDebug();
}

// checks to see if a particular block ID can now be freed
int blockManagementTryFree(unsigned int id)
{
	signed char* lock_ptr = _block_mgr_lock_table + (id & BLOCK_ID_MASK & ~3);
	typedef  struct {char a[4];} wordsize;
	wordsize *ptrp = (wordsize*)lock_ptr;
	unsigned int result;
retry:
	__asm__ volatile ("lwarx %0,%y1" : "=r" (result) : "Z" (*ptrp));     

	union { signed char a[4]; unsigned int b; } conv;
	conv.b = result;
	if (conv.a[id&3] > 0) {
		blockManagementDebug();
		return 0;
	}
#ifdef DEBUG
	if (conv.a[id&3] < 0)
		printf("try free already -ve for %x\n", id);
#endif
	conv.a[id&3]=-1;
	unsigned int value = conv.b;

	__asm__ volatile ("stwcx. %2,%y1\n"
			"\tmfocrf %0,0x80" : "=r" (result), "=Z" (*ptrp) : "r" (value));
	if (!(result & 0x20000000)) {
		sched_yield();
		goto retry;
	}
	blockManagementDebug();
	return 1;
}
