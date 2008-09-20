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

#include <spu_mfcio.h>
#include <spu_intrinsics.h>
#include <stdio.h>

#include "render.h"

void debug_render_tasks(RenderableCacheLine* cache)
{
	int mask = 0x8000;
	for (int i=0; i<16; i++) {
		if (1 || cache->chunksWaiting & mask) {
			printf("[%d] DEBUG %2d - [%c%c] Start %4d Length %4d End %4d Triangle %x\n",
				_SPUID, i,
				cache->chunksWaiting & mask ? 'W': '-',
				cache->chunksFree & mask ? 'F': '-',
				cache->chunkStartArray[i],
				cache->chunkLengthArray[i],
				cache->chunkStartArray[i] + cache->chunkLengthArray[i],
				cache->chunkTriangleArray[i]);
		}
		mask>>=1;
	}
}

void process_render_tasks(unsigned long eah_render_tasks, unsigned long eal_render_tasks)
{
	const vec_uchar16 SHUFFLE_MERGE_BYTES = (vec_uchar16) {	// merge lo bytes from unsigned shorts (array)
		1,3,5,7,9,11,13,15,17,19,21,23,25,27,29,31 };

	const vec_uchar16 SHUFFLE_GET_BUSY_WITH_ONES = (vec_uchar16) {	// get busy flag with ones in unused bytes
		0xc0, 0xc0, 2, 3, 0xc0,0xc0,0xc0,0xc0, 0xc0,0xc0,0xc0,0xc0 };

	char	sync_buffer[128+127];
	void*	aligned_sync_buffer = (void*) ( ((unsigned long)sync_buffer+127) & ~127 );

	RenderableCacheLine*	cache = (RenderableCacheLine*) aligned_sync_buffer;
	unsigned long long cache_ea;

	spu_mfcdma64(&cache_ea, eah_render_tasks, eal_render_tasks, sizeof(cache_ea), 0, MFC_GET_CMD);
	mfc_write_tag_mask(1<<0);
	mfc_read_tag_status_all();

	while (cache_ea) {
		// terminate immediately if possible
		if (spu_stat_in_mbox())
			return;

		// read the cache line
		spu_mfcdma64(cache, mfc_ea2h(cache_ea), mfc_ea2l(cache_ea), 128, 0, MFC_GETLLAR_CMD);
		spu_readch(MFC_RdAtomicStat);

		unsigned int endTriangle = cache->endTriangle;
		vec_ushort8 testTriangle = spu_splats((unsigned short) endTriangle);

		vec_uint4 doneTriangleGather = spu_gather( (vec_uchar16) spu_shuffle(
			(vec_uchar16) spu_cmpeq(testTriangle, cache->chunkTriangle[0]),
			(vec_uchar16) spu_cmpeq(testTriangle, cache->chunkTriangle[1]),
			SHUFFLE_MERGE_BYTES) );
		// doneTriangleGather, rightmost 16 bits of word 0 set if triangle done
#ifdef TEST
printf("[%d] G1=%04x, G2=%04x, gather=%04x\n",
	_SPUID,
	spu_extract(spu_gather(spu_cmpeq(testTriangle, cache->chunkTriangle[0])),0),
	spu_extract(spu_gather(spu_cmpeq(testTriangle, cache->chunkTriangle[1])),0),
	spu_extract(doneTriangleGather,0) );
#endif // TEST
		vec_uint4 v_waiting = spu_splats( (unsigned int)cache->chunksWaiting );
		vec_uint4 v_free = spu_splats( (unsigned int)cache->chunksFree );

		vec_uint4 v_mayprocess = spu_andc(v_waiting, doneTriangleGather);
		// v_mayprocess bits are set if chunkWaiting bit set and triangle not complete

		unsigned int chunkToProcess = spu_extract( spu_cntlz(v_mayprocess), 0 )-16;
		unsigned int freeChunk = spu_extract( spu_cntlz(v_free), 0 )-16;
#ifdef TEST
printf("[%d] v_wait=%04x, v_may=%04x, chunkToProcess=%d, v_free = %04x, freeChunk=%d, g=%04x\n",
	_SPUID,
	spu_extract(v_waiting, 0),
	spu_extract(v_mayprocess, 0), chunkToProcess,
	spu_extract(v_free, 0), freeChunk,
	spu_extract(doneTriangleGather,0) );
#endif // TEST
		if (!spu_extract(v_mayprocess, 0)) {
			// nothing to process, try the next cache line in the rendering tasks list
#ifdef TEST
			debug_render_tasks(cache);
#endif // TEST
			cache_ea = cache->next;
			continue;
		}

		// at least one of the bits is set, chunkToProcess is a valid result
		unsigned int chunkStart    	= cache->chunkStartArray   [chunkToProcess];
		unsigned int chunkLength   	= cache->chunkLengthArray  [chunkToProcess];
		unsigned int chunkTriangle	= cache->chunkTriangleArray[chunkToProcess];

		if (spu_extract(v_free, 0) && chunkLength>NUMBER_OF_TILES_PER_CHUNK) {
			// there's one spare slot to move remainder into, so split the chunk up
			cache->chunkStartArray   [freeChunk]	  = chunkStart + NUMBER_OF_TILES_PER_CHUNK;
			cache->chunkLengthArray  [freeChunk]	  = chunkLength - NUMBER_OF_TILES_PER_CHUNK;
			cache->chunkTriangleArray[freeChunk]	  = chunkTriangle;
			cache->chunkLengthArray  [chunkToProcess] = NUMBER_OF_TILES_PER_CHUNK;
			cache->chunksWaiting	|=    0x8000>>freeChunk;
			cache->chunksFree	&= ~( 0x8000>>freeChunk );
#ifdef TEST
			printf("[%d] Split chunk %d at %d len %d, creating remainder chunk %d at %d len %d [%d]\n",
				_SPUID,
				chunkToProcess, chunkStart, chunkLength, freeChunk,
				cache->chunkStartArray [freeChunk], cache->chunkLengthArray[freeChunk],
				chunkTriangle);
#endif // TEST
			chunkLength = NUMBER_OF_TILES_PER_CHUNK;
		}
		else if (chunkLength>NUMBER_OF_TILES_PER_CHUNK) {
			printf("[%d] Unable to split chunk %d at %d len %d\n",
				_SPUID,
				chunkToProcess, chunkStart, chunkLength);
			debug_render_tasks(cache);
		}

		//cache->chunksBusy	|=    0x8000>>chunkToProcess;
		cache->chunksWaiting	&= ~( 0x8000>>chunkToProcess );

		// write out the updated cache line
		spu_mfcdma64(cache, mfc_ea2h(cache_ea), mfc_ea2l(cache_ea), 128, 0, MFC_PUTLLC_CMD);
		unsigned int status = spu_readch(MFC_RdAtomicStat) & MFC_PUTLLC_STATUS;
		if (status) {
			// cache is dirty and write failed, reload it and attempt the whole thing again again
			printf("[%d] Atomic write failed, retring...\n", _SPUID);
			continue;
		}

		// now, if we got here, then we have a successful lock on a chunk
		endTriangle = process_render_chunk(chunkStart, chunkLength, chunkTriangle, endTriangle,
					cache->triangleBase, cache->chunkBase);

		// now mark the chunk as complete...
		do {
#ifdef TEST
			printf("[%d] Trying to release chunk %d\n", _SPUID, chunkToProcess);
#endif // TEST

			spu_mfcdma64(cache, mfc_ea2h(cache_ea), mfc_ea2l(cache_ea), 128, 0, MFC_GETLLAR_CMD);
			spu_readch(MFC_RdAtomicStat);

			cache->chunkTriangleArray[chunkToProcess] = endTriangle;
			cache->chunksWaiting	|=    0x8000>>chunkToProcess;

			vec_ushort8 testTri = spu_splats( (unsigned short) endTriangle );
			unsigned short cStart = chunkStart;
			unsigned short cLength = chunkLength;
			unsigned int   cIndex = chunkToProcess;

			int repeat;
			do {
#ifdef TEST
				debug_render_tasks(cache);
#endif // TEST

				printf("[%d] cStart=%d, cLength=%d, cIndex=%d\n",
					_SPUID, cStart, cLength, cIndex);

				vec_ushort8 testStart = spu_splats( (unsigned short)(cStart+cLength) );
				vec_ushort8 testEnd = spu_splats( cStart );

				vec_uint4 testTriangleGather = spu_gather( (vec_uchar16) spu_shuffle(
					(vec_uchar16) spu_cmpeq(testTri, cache->chunkTriangle[0]),
					(vec_uchar16) spu_cmpeq(testTri, cache->chunkTriangle[1]),
					SHUFFLE_MERGE_BYTES) );

				vec_uint4 testStartGather = spu_gather( (vec_uchar16) spu_shuffle(
					(vec_uchar16) spu_cmpeq(testStart, cache->chunkStart[0]),
					(vec_uchar16) spu_cmpeq(testStart, cache->chunkStart[1]),
					SHUFFLE_MERGE_BYTES) );

				vec_uint4 testEndGather = spu_gather( (vec_uchar16) spu_shuffle(
					(vec_uchar16) spu_cmpeq(testEnd,
							spu_add(cache->chunkLength[0], cache->chunkStart[0])),
					(vec_uchar16) spu_cmpeq(testEnd,
							spu_add(cache->chunkLength[1], cache->chunkStart[1])),
					SHUFFLE_MERGE_BYTES) );

				vec_uint4 testWaiting = spu_splats( (unsigned int)cache->chunksWaiting );
				vec_uint4 matchMask = spu_and(testWaiting, testTriangleGather);

				vec_uint4 matchFollowing = spu_and( testStartGather, matchMask );
				vec_uint4 matchPreceding   = spu_and( testEndGather, matchMask );

#ifdef TEST
				printf("[%d] Join tests: W: %04x T: %04x S:%04x %c E:%04x %c tS:%d tE:%d\n",
					_SPUID,
					spu_extract(testWaiting, 0),
					spu_extract(testTriangleGather, 0),
					spu_extract(testStartGather, 0), spu_extract(matchFollowing,0) ? '*' : ' ',
					spu_extract(testEndGather, 0),   spu_extract(matchPreceding,0) ? '*' : ' ',
					spu_extract(testStart, 0),
					spu_extract(testEnd, 0));
#endif // TEST

				if (spu_extract(matchFollowing,0)) {
					unsigned int otherIndex = spu_extract( spu_cntlz(matchFollowing), 0 )-16;
#ifdef TEST
					printf("[%d] Merging %d with following %d, %d+%d and %d+%d\n",
						_SPUID,
						cIndex, otherIndex,
						cache->chunkStartArray[cIndex],
						cache->chunkLengthArray[cIndex],
						cache->chunkStartArray[otherIndex],
						cache->chunkLengthArray[otherIndex]);
#endif // TEST

					cache->chunksWaiting &= ~( 0x8000>>otherIndex );
					cache->chunksFree    |=    0x8000>>otherIndex;
					cache->chunkLengthArray[cIndex] += cache->chunkLengthArray[otherIndex];
					cLength = cache->chunkLengthArray[cIndex];

#ifdef TEST
					debug_render_tasks(cache);
#endif // TEST
				}

				if (spu_extract(matchPreceding,0)) {
					unsigned int otherIndex = spu_extract( spu_cntlz(matchPreceding), 0 )-16;
#ifdef TEST
					printf("[%d] Merging preceding %d with %d, %d+%d and %d+%d\n",
						_SPUID,
						otherIndex, cIndex,
						cache->chunkStartArray[otherIndex],
						cache->chunkLengthArray[otherIndex],
						cache->chunkStartArray[cIndex],
						cache->chunkLengthArray[cIndex]);
#endif // TEST

					cache->chunksWaiting &= ~( 0x8000>>cIndex );
					cache->chunksFree    |=    0x8000>>cIndex;
					cache->chunkLengthArray[otherIndex] += cache->chunkLengthArray[cIndex];
					cIndex = otherIndex;
					cStart = cache->chunkStartArray[cIndex];

#ifdef TEST
					debug_render_tasks(cache);
#endif // TEST
				}

				repeat = spu_extract( spu_or(matchFollowing, matchPreceding), 0);
			} while(repeat);

			// attempt the write
			spu_mfcdma64(cache, mfc_ea2h(cache_ea), mfc_ea2l(cache_ea), 128, 0, MFC_PUTLLC_CMD);
			status = spu_readch(MFC_RdAtomicStat) & MFC_PUTLLC_STATUS;
		} while (status);
	} // while (cache_ea) - process current cache line
}

unsigned short process_render_chunk(unsigned short chunkStart, unsigned short chunkLength,
				    unsigned short chunkTriangle, unsigned short endTriangle,
				    unsigned long long triangleBase, unsigned long long chunkBase)
{
	printf("[%d] Processing chunk at %d len %d, triangle %x\n",
		_SPUID,
		chunkStart, chunkLength, chunkTriangle );

//	__asm("stop 0x2110\n\t.word 0");

	return chunkTriangle+1;
	//return endTriangle;
}

