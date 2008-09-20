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
	const vec_uchar16 SHUFFLE_MERGE_BYTES = (vec_uchar16) {	// merge high bytes from halfwords into bytes
		0,16, 2,18, 4,20, 6,22,		8,24, 10,26, 12,28, 14,30 };

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
		vec_ushort8 testTriangle = spu_splats(endTriangle);

		vec_uchar16 doneTriangle = spu_shuffle(
			(vec_uchar16) spu_cmpeq(testTriangle, cache->chunkTriangle[0]),
			(vec_uchar16) spu_cmpeq(testTriangle, cache->chunkTriangle[1]),
			SHUFFLE_MERGE_BYTES);
		vec_uint4 doneTriangleGather = spu_gather(doneTriangle);
		// doneTriangleGather, rightmost 16 bits of word 0 set if triangle done

		vec_uint4 v_waiting = spu_splats( (unsigned int)cache->chunksWaiting );
		vec_uint4 v_mayprocess = spu_andc(v_waiting, doneTriangleGather);
		// v_mayprocess bits are set if chunkWaiting bit set and triangle not complete

		unsigned int chunkToProcess = spu_extract( spu_cntlz(v_mayprocess), 0 )-16;
		if (!spu_extract(v_mayprocess, 0)) {
			// nothing to process, try the next cache line in the rendering tasks list
			cache_ea = cache->next;
			continue;
		}

		// at least one of the bits is set, chunkToProcess is a valid result
		unsigned int chunkStart    	= cache->chunkStartArray   [chunkToProcess];
		unsigned int chunkLength   	= cache->chunkLengthArray  [chunkToProcess];
		unsigned int chunkTriangle	= cache->chunkTriangleArray[chunkToProcess];

		// get the busy flags in case we need to split up this chunk...
		vec_ushort8 v_busy_temp = (vec_ushort8) cache->chunksBusy;
		vec_uint4 v_busy = spu_shuffle( v_busy_temp, v_busy_temp, SHUFFLE_GET_BUSY_WITH_ONES );
		vec_uint4 v_free = spu_nor(v_waiting,v_busy);
		// v_free bits are set if not waiting or busy

		unsigned int freeChunk = spu_extract( spu_cntlz(v_mayprocess), 0 )-16;
		if (spu_extract(v_free, 0) && chunkLength>NUMBER_OF_CHUNKS_TO_PROCESS) {
			// there's one spare slot to move remainder into, so split the chunk up
			cache->chunkStartArray   [freeChunk]	  = chunkStart + NUMBER_OF_CHUNKS_TO_PROCESS;
			cache->chunkLengthArray  [freeChunk]	  = chunkLength - NUMBER_OF_CHUNKS_TO_PROCESS;
			cache->chunkTriangleArray[freeChunk]	  = chunkTriangle;
			cache->chunkLengthArray  [chunkToProcess] = NUMBER_OF_CHUNKS_TO_PROCESS;
			cache->chunksWaiting	|=    1<<freeChunk;
			cache->chunksBusy	&= ~( 1<<freeChunk );
			printf("Split chunk %d at %d len %d, creating remainder chunk %d at %d len %d\n",
				chunkToProcess, chunkStart, chunkLength, freeChunk,
				cache->chunkStartArray [freeChunk], cache->chunkLengthArray[freeChunk]);
			chunkLength = NUMBER_OF_CHUNKS_TO_PROCESS;
		}
		cache->chunksBusy	|=    1<<chunkToProcess;
		cache->chunksWaiting	&= ~( 1<<chunkToProcess );

		// write out the updated cache line
		spu_mfcdma64(cache, mfc_ea2h(cache_ea), mfc_ea2l(cache_ea), 128, 0, MFC_PUTLLC_CMD);
		unsigned int status = spu_readch(MFC_RdAtomicStat) & MFC_PUTLLC_STATUS;
		if (status) {
			// cache is dirty and write failed, reload it and attempt the whole thing again again
			continue;
		}

		// now, if we got here, then we have a successful lock on a chunk
		printf("Processing chunk %d at %d len %d, triangle %x\n",
				chunkToProcess, chunkStart, chunkLength, chunkTriangle );

		__asm("stop 0x2110\n\t.word 0");

		// now mark the chunk as complete...
		do {
			spu_mfcdma64(cache, mfc_ea2h(cache_ea), mfc_ea2l(cache_ea), 128, 0, MFC_GETLLAR_CMD);
			spu_readch(MFC_RdAtomicStat);

			cache->chunkTriangleArray[chunkToProcess] = endTriangle;
			cache->chunksBusy	|=    1<<chunkToProcess;
			cache->chunksWaiting	&= ~( 1<<chunkToProcess );

			// attempt the write
			spu_mfcdma64(cache, mfc_ea2h(cache_ea), mfc_ea2l(cache_ea), 128, 0, MFC_PUTLLC_CMD);
			status = spu_readch(MFC_RdAtomicStat) & MFC_PUTLLC_STATUS;
		} while (status);
	} // while (cache_ea) - process current cache line
}

