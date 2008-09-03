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
#include "ppufuncs.h"

// this does the custom PPU/SPU locking
//
// it differs from standard mutex locking in that the SPU will pretty much always have an
// active lock, so the PPU signals that it wants the lock and when the SPU notices this,
// it will relinquish the lock and wait until the PPU has finished

void lock(LOCK* lock) {
	unsigned int result;
	typedef  struct {char a[4];} wordsize;
	wordsize *ptrp = (wordsize*)lock;
retry:
	__asm__ volatile ("lwarx %0,%y1" : "=r" (result) : "Z" (*ptrp));     

	unsigned int value;

	switch(result) {
		case LOCK_free:
		case LOCK_PPU_may_proceed:
			value = LOCK_PPU;
			__asm__ volatile ("stwcx. %2,%y1\n"
					"\tmfocrf %0,0x80" : "=r" (result), "=Z" (*ptrp) : "r" (value));
			if (result & 0x20000000)
				return;
			sched_yield();
			goto retry;

		case LOCK_SPU:
			value = LOCK_PPU_wait;
			__asm__ volatile ("stwcx. %0,%y1" : "=r" (value), "=Z" (*ptrp));
			sched_yield();
			goto retry;

		case LOCK_PPU_wait:
			sched_yield();
			goto retry;
	}
	__asm__ volatile ("isync" : : : "memory");
}

void unlock(LOCK* lock) {
	*lock = LOCK_free;
/*
	unsigned int result;
	typedef  struct {char a[4];} wordsize;
	wordsize *ptrp = (wordsize*)lock;
retry:
	__asm__ volatile ("lwarx %0,%y1" : "=r" (result) : "Z" (*ptrp));     

	unsigned int value;

	switch(result) {
		case LOCK_PPU:
			value = LOCK_free;
			__asm__ volatile ("stwcx. %2,%y1\n"
					"\tmfocrf %0,0x80" : "=r" (result), "=Z" (*ptrp) : "r" (value));
			if (result & 0x20000000)
				return;
			sched_yield();
			goto retry;
	}
*/
}

