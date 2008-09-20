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
		// read the cache line
		spu_mfcdma64(cache, mfc_ea2h(cache_ea), mfc_ea2l(cache_ea), 128, 0, MFC_GETLLAR_CMD);
		spu_readch(MFC_RdAtomicStat);

		unsigned int endTriangle = cache->endTriangle;
		vec_ushort8 testTriangle = spu_splats((unsigned short) endTriangle);

		vec_uchar16 doneTriangle = spu_shuffle(
			(vec_uchar16) spu_cmpeq(testTriangle, cache->chunkTriangle[0]),
			(vec_uchar16) spu_cmpeq(testTriangle, cache->chunkTriangle[1]),
			SHUFFLE_MERGE_BYTES);
		vec_uint4 doneTriangleGather = spu_gather(doneTriangle);
		// doneTriangleGather, rightmost 16 bits of word 0 set if triangle done
#ifdef TEST
printf("G1=%04x, G2=%04x, gather=%04x\n",
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
printf("v_wait=%04x, v_may=%04x, chunkToProcess=%d, v_free = %04x, freeChunk=%d, g=%04x\n",
	spu_extract(v_waiting, 0),
	spu_extract(v_mayprocess, 0), chunkToProcess,
	spu_extract(v_free, 0), freeChunk,
	spu_extract(doneTriangleGather,0) );
#endif // TEST
		if (!spu_extract(v_mayprocess, 0)) {
			// nothing to process, try the next cache line in the rendering tasks list
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
			printf("Split chunk %d at %d len %d, creating remainder chunk %d at %d len %d [%d]\n",
				chunkToProcess, chunkStart, chunkLength, freeChunk,
				cache->chunkStartArray [freeChunk], cache->chunkLengthArray[freeChunk],
				chunkTriangle);
			chunkLength = NUMBER_OF_TILES_PER_CHUNK;
		}
		//cache->chunksBusy	|=    0x8000>>chunkToProcess;
		cache->chunksWaiting	&= ~( 0x8000>>chunkToProcess );

		// write out the updated cache line
		spu_mfcdma64(cache, mfc_ea2h(cache_ea), mfc_ea2l(cache_ea), 128, 0, MFC_PUTLLC_CMD);
		unsigned int status = spu_readch(MFC_RdAtomicStat) & MFC_PUTLLC_STATUS;
		if (status) {
			// cache is dirty and write failed, reload it and attempt the whole thing again again
			continue;
		}

		// now, if we got here, then we have a successful lock on a chunk
		endTriangle = process_render_chunk(chunkStart, chunkLength, chunkTriangle, endTriangle,
					cache->triangleBase, cache->chunkBase);

		// now mark the chunk as complete...
		do {
#ifdef TEST
			printf("Trying to release chunk %d\n", chunkToProcess);
#endif // TEST
			spu_mfcdma64(cache, mfc_ea2h(cache_ea), mfc_ea2l(cache_ea), 128, 0, MFC_GETLLAR_CMD);
			spu_readch(MFC_RdAtomicStat);

			cache->chunkTriangleArray[chunkToProcess] = endTriangle;
			cache->chunksWaiting	|=    0x8000>>chunkToProcess;

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
	printf("Processing chunk at %d len %d, triangle %x\n", chunkStart, chunkLength, chunkTriangle );

	__asm("stop 0x2110\n\t.word 0");

	return chunkTriangle+1;
	//return endTriangle;
}

