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

#include "connection.h"
#include <stdlib.h>
#include <sched.h>

static void*		_block_mgr_buffer;
static signed char*	_block_mgr_lock_table;
static long long*	_block_mgr_ea_table;

// initialises the block management system
void		blockManagementInit() 
{
	_block_mgr_buffer = malloc(127+MAX_DATA_BUFFERS*(sizeof(signed char)+sizeof(long long)));
	_block_mgr_lock_table = (signed char*) ((((unsigned int)_block_mgr_buffer)+127)&~127);
	_block_mgr_ea_table = (long long*) (_block_mgr_lock_table+MAX_DATA_BUFFERS);
}

// frees any memory used by the block management system, returns non-zero on success
int 		blockManagementDestroy()
{
	free(_block_mgr_buffer);
	_block_mgr_buffer = NULL;
	_block_mgr_lock_table = NULL;
	_block_mgr_ea_table = NULL;
}

// allocates a block from the system, storing the EA alongside it, returns block ID
// the initial usage count is set to 0
unsigned int	blockManagementAllocateBlock(long long ea)
{
	signed char* lock_ptr = _block_mgr_lock_table;

	for (int i=0; i<MAX_DATA_BUFFERS; i+=4, lock_ptr+=4) {
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
		//		signed char v = (signed char) (result>>(8*j)&255);
				if (v < 0) {
					_block_mgr_ea_table[i+j] = ea;
					conv.a[j] = 1;
					unsigned int value = conv.b;
		//			result = (result &~ (0xff<<(8*j))) | (1<<(8*j));
					__asm__ volatile ("stwcx. %2,%y1\n"
							"\tmfocrf %0,0x80" : "=r" (result), "=Z" (*ptrp) : "r" (value));
					if (result & 0x20000000) {
						return (i+j);
					}
					sched_yield();
					goto retry;
				}
			}
		}
	}
	return 0xffffffff;
}


// decrement the usage count of the block
void		blockManagementBlockCountDispose(unsigned int id)
{
	signed char* lock_ptr = _block_mgr_lock_table + (id&~3);

	typedef  struct {char a[4];} wordsize;
	wordsize *ptrp = (wordsize*)lock_ptr;
	unsigned int result;
//	unsigned int mask = 0xff<<(4*(id&3));
retry:
	__asm__ volatile ("lwarx %0,%y1" : "=r" (result) : "Z" (*ptrp));     

	union { signed char a[4]; unsigned int b; } conv;
	conv.b = result;
	conv.a[id&3]--;
	unsigned int value = conv.b;

//	result = ((result+mask)&mask) | (result&~mask);		// subtract 1 from a byte

	__asm__ volatile ("stwcx. %2,%y1\n"
			"\tmfocrf %0,0x80" : "=r" (result), "=Z" (*ptrp) : "r" (value));
	if (!(result & 0x20000000)) {
		sched_yield();
		goto retry;
	}
}

// checks to see if a particular block ID can now be freed
int		blockManagementTryFree(unsigned int id)
{
	signed char* lock_ptr = _block_mgr_lock_table + (id&~3);

	typedef  struct {char a[4];} wordsize;
	wordsize *ptrp = (wordsize*)lock_ptr;
	unsigned int result;
retry:
	__asm__ volatile ("lwarx %0,%y1" : "=r" (result) : "Z" (*ptrp));     

	union { signed char a[4]; unsigned int b; } conv;
	conv.b = result;
	if (conv.a[id&3])
		return 0;
	conv.a[id&3]=-1;
	unsigned int value = conv.b;

	__asm__ volatile ("stwcx. %2,%y1\n"
			"\tmfocrf %0,0x80" : "=r" (result), "=Z" (*ptrp) : "r" (value));
	if (!(result & 0x20000000)) {
		sched_yield();
		goto retry;
	}
	return 1;
}
