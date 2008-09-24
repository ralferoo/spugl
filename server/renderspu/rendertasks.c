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
#include "../connection.h"
#include "../spu/spucontext.h"

#define DEBUG_VEC4(x) __debug_vec4(#x, (vec_uint4) x)
#define DEBUG_VEC8(x) __debug_vec8(#x, (vec_ushort8) x)
#define DEBUG_VECf(x) __debug_vecf(#x, (vec_float4) x)

void __debug_vec4(char* s, vec_uint4 x)
{
	printf("[%d] %-20s %08x   %08x   %08x   %08x\n", _SPUID, s,
		spu_extract(x, 0),
		spu_extract(x, 1),
		spu_extract(x, 2),
		spu_extract(x, 3) );
}

void __debug_vec8(char* s, vec_ushort8 x)
{
	printf("[%d] %-20s %04x %04x %04x %04x %04x %04x %04x %04x\n", _SPUID, s,
		spu_extract(x, 0),
		spu_extract(x, 1),
		spu_extract(x, 2),
		spu_extract(x, 3),
		spu_extract(x, 4),
		spu_extract(x, 5),
		spu_extract(x, 6),
		spu_extract(x, 7) );
}

void __debug_vecf(char* s, vec_float4 x)
{
	printf("[%d] %-20s %11.3f %11.3f %11.3f %11.3f\n", _SPUID, s,
		spu_extract(x, 0),
		spu_extract(x, 1),
		spu_extract(x, 2),
		spu_extract(x, 3) );
}


////////////////////////////////////////////////////////////////////////////////////////////////////


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

char __renderable_base_buffer[ 256 + sizeof(Renderable) ] __attribute__((__aligned__(128)));
unsigned long long currentRenderableBaseAddress = ~0ULL;
Renderable* currentRenderableBase;

void process_render_tasks(unsigned long eah_render_tasks, unsigned long eal_render_tasks)
{
	const vec_uchar16 SHUFFLE_MERGE_BYTES = (vec_uchar16) {	// merge lo bytes from unsigned shorts (array)
		1,3,5,7,9,11,13,15,17,19,21,23,25,27,29,31 };

	const vec_uchar16 SHUFFLE_GET_BUSY_WITH_ONES = (vec_uchar16) {	// get busy flag with ones in unused bytes
		0xc0, 0xc0, 2, 3, 0xc0,0xc0,0xc0,0xc0, 0xc0,0xc0,0xc0,0xc0 };

	const vec_uchar16 ZERO_BYTES = spu_splats(0);

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

		unsigned int numberOfWaitingChunks = spu_extract( (vec_uint4)
						spu_sumb(spu_cntb( (vec_uchar16) v_mayprocess ), ZERO_BYTES), 0);

#ifdef TEST
printf("[%d] wait=%04x, may=%04x, chunkToProcess=%d, free=%04x(%2d), freeChunk=%d, g=%04x\n",
	_SPUID,
	spu_extract(v_waiting, 0),
	spu_extract(v_mayprocess, 0), chunkToProcess,
	spu_extract(v_free, 0), numberOfFreeChunks, freeChunk,
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

		// read in the renderable's information
		unsigned long long renderableBase = cache->renderableBase;
		if (renderableBase != currentRenderableBaseAddress) {
			currentRenderableBaseAddress = renderableBase;
			int offset = renderableBase & 127;
			currentRenderableBase = (Renderable*) (__renderable_base_buffer + offset);
			unsigned long long end = (renderableBase + sizeof(Renderable) + 127) & ~127;
			renderableBase &= ~127;
			unsigned int length = end - renderableBase;

			spu_mfcdma64(__renderable_base_buffer, mfc_ea2h(renderableBase), mfc_ea2l(renderableBase),
				length, 1, MFC_GET_CMD);
		}


		// at least one of the bits is set, chunkToProcess is a valid result
		unsigned int chunkStart    	= cache->chunkStartArray   [chunkToProcess];
		unsigned int chunkLength   	= cache->chunkLengthArray  [chunkToProcess];
		unsigned int chunkTriangle	= cache->chunkTriangleArray[chunkToProcess];

		if (numberOfWaitingChunks < CHUNK_DIVIDE_THRESHOLD &&
		    chunkLength>NUMBER_OF_TILES_PER_CHUNK) {
			vec_uint4 v_free2 = spu_andc(v_free, spu_splats( (unsigned int) (0x8000>>freeChunk) ));

			unsigned int freeChunk2 = spu_extract( spu_cntlz(v_free2), 0 )-16;

			vec_uint4 chunkBitFiddle = spu_splats( chunkStart ^ (chunkStart+chunkLength-1) );
			unsigned int chunkBitShift = spu_extract(spu_cntlz(chunkBitFiddle), 0);
			unsigned int chunkSplitSize = ((unsigned int) (1UL<<31)   ) >> chunkBitShift;
			unsigned int chunkSplitMask = ((unsigned int)((1UL<<31)-1)) >> chunkBitShift;
			unsigned int chunkBoundary = (chunkStart &~ (chunkSplitMask) ) + chunkSplitSize;

			cache->chunkStartArray   [freeChunk2]	  = chunkBoundary;
			cache->chunkLengthArray  [freeChunk2]	  = chunkStart + chunkLength - chunkBoundary;
			cache->chunkTriangleArray[freeChunk2]	  = chunkTriangle;
			cache->chunkLengthArray  [chunkToProcess] = chunkBoundary - chunkStart;
			cache->chunksWaiting	|=    0x8000>>freeChunk2;
			cache->chunksFree	&= ~( 0x8000>>freeChunk2 );
#ifdef TEST
			printf("[%d] W=%d Divided chunk %d at %d len %d, remainder chunk %d at %d len %d [%d]\n",
				_SPUID, numberOfWaitingChunks,
				chunkToProcess, chunkStart, chunkLength, freeChunk2,
				cache->chunkStartArray [freeChunk2], cache->chunkLengthArray[freeChunk2],
				chunkTriangle);
			debug_render_tasks(cache);
#endif // TEST
			chunkLength = chunkBoundary - chunkStart;
		}

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
#ifdef TEST
			printf("[%d] Atomic write failed, retring...\n", _SPUID);
#endif // TEST
			continue;
		}

		// ensure that the currentRenderableBase structure has finished DMA (shouldn't be a problem
		// but always worth checking)
		mfc_write_tag_mask(1<<1);
		mfc_read_tag_status_all();

renderMoreTriangles:
		// now, if we got here, then we have a successful lock on a chunk
		endTriangle = process_render_chunk(chunkStart, chunkLength, chunkTriangle, endTriangle,
					cache->triangleBase, currentRenderableBase);

		// now mark the chunk as complete...
		do {
#ifdef TEST
			printf("[%d] Trying to release chunk %d\n", _SPUID, chunkToProcess);
#endif // TEST

			spu_mfcdma64(cache, mfc_ea2h(cache_ea), mfc_ea2l(cache_ea), 128, 0, MFC_GETLLAR_CMD);
			spu_readch(MFC_RdAtomicStat);

			if (endTriangle != cache->endTriangle) {
#ifdef TEST
				printf("[%d] Goalposts moved from %d to %d, currently %d\n", _SPUID,
					endTriangle, cache->endTriangle, chunkTriangle);
#endif // TEST
				chunkTriangle = endTriangle;
				endTriangle = cache->endTriangle;
				goto renderMoreTriangles;
			}

			cache->chunkTriangleArray[chunkToProcess] = endTriangle;
			cache->chunksWaiting	|=    0x8000>>chunkToProcess;

			vec_ushort8 testTri = spu_splats( (unsigned short) endTriangle );
			unsigned short cStart = chunkStart;
			unsigned short cLength = chunkLength;
			unsigned int   cIndex = chunkToProcess;

			// merge blocks for as long as possible
			int continueMergingBlocks;
			do {
#ifdef TEST
				debug_render_tasks(cache);

				printf("[%d] cStart=%d, cLength=%d, cIndex=%d\n",
					_SPUID, cStart, cLength, cIndex);
#endif // TEST

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

				continueMergingBlocks = spu_extract( spu_or(matchFollowing, matchPreceding), 0);
			} while(continueMergingBlocks);

			// attempt the write
			spu_mfcdma64(cache, mfc_ea2h(cache_ea), mfc_ea2l(cache_ea), 128, 0, MFC_PUTLLC_CMD);
			status = spu_readch(MFC_RdAtomicStat) & MFC_PUTLLC_STATUS;
		} while (status);
	} // while (cache_ea) - process current cache line
}

void subdivide(vec_uint4 A, vec_uint4 Adx, vec_uint4 Ady, vec_uint4 y, vec_ushort8 i,
		vec_uint4 b, vec_uint4 n,
		int type)
{
	vec_uint4 Ar = spu_add(A, Adx);
	vec_uint4 Ab = spu_add(A, Ady);
	vec_uint4 Abr = spu_add(Ar, Ady);
	unsigned int outside = spu_extract(spu_orx(spu_rlmaska(
					spu_nor( spu_or(A,Ar), spu_or(Ab,Abr) ), -31)),0);

	if (!outside) {
		i = spu_rlmask(i, -1);
		n = spu_rlmask(n, -2);
		if (spu_extract(i, 1)) {
			vec_uint4 hdx = spu_rlmaska(Adx, -1);
			vec_uint4 hdy = spu_rlmaska(Ady, -1);

			vec_uint4 A_hdx 	= spu_add(A,     hdx);
			vec_uint4 A_hdy		= spu_add(A,     hdy);
			vec_uint4 A_hdx_hdy	= spu_add(A_hdx, hdy);

			vec_uint4 Adx_hdx	= spu_sub(Adx,hdx);
			vec_uint4 Ady_hdy	= spu_sub(Ady,hdy);

			// type & 0xf0 = bit 0 of type (&1)
			// type & 0x0f = bit 1 of type (&2)
			//
			//		A	B	C	D
			//
			// type 0:	0	y	x+y	x
			// type 1:	0	x	x+y	y
			// type 2:	x+y	x	0	y
			// type 3:	x+y	y	0	x

			vec_uint4 bit1 = spu_maskw( type );
			vec_uint4 bit0 = spu_maskw( type>>4 );
			vec_uint4 bitp = spu_xor( bit0, bit1 );

			vec_uint4 andm = spu_xor( spu_promote(0xffff0000, 0), bitp);
			vec_uint4 im   = (vec_uint4) i;

			// A
			vec_uint4 startA = spu_sel( A, A_hdx_hdy, bit1);
			vec_uint4 dxA	 = spu_sel( hdx, Adx_hdx, bit1);
			vec_uint4 dyA	 = spu_sel( hdy, Ady_hdy, bit1);
			vec_uint4 addA	 = spu_and( im, bit1 );

			// C
			vec_uint4 startC = spu_sel( A_hdx_hdy, A, bit1);
			vec_uint4 dxC	 = spu_sel( Adx_hdx, hdx, bit1);
			vec_uint4 dyC	 = spu_sel( Ady_hdy, hdy, bit1);
			vec_uint4 addC	 = spu_andc( im, bit1 );

			// B
			vec_uint4 startB = spu_sel( A_hdy, A_hdx, bitp);
			vec_uint4 dxB	 = spu_sel( hdx, Adx_hdx, bitp);
			vec_uint4 dyB	 = spu_sel( Ady_hdy, hdy, bitp);
			vec_uint4 addB	 = spu_andc( im, andm );

			// D
			vec_uint4 startD = spu_sel( A_hdx, A_hdy, bitp);
			vec_uint4 dxD	 = spu_sel( Adx_hdx, hdx, bitp);
			vec_uint4 dyD	 = spu_sel( hdy, Ady_hdy, bitp);
			vec_uint4 addD	 = spu_and( im, andm );

			vec_uint4 newb  = spu_add(b,n);

			subdivide( startA, dxA, dyA, spu_add(y, addA), i, spu_splats(spu_extract(newb,0)), n, type^0xf0);
			subdivide( startB, dxB, dyB, spu_add(y, addB), i, spu_splats(spu_extract(newb,1)), n, type);
			subdivide( startC, dxC, dyC, spu_add(y, addC), i, spu_splats(spu_extract(newb,2)), n, type);
			subdivide( startD, dxD, dyD, spu_add(y, addD), i, spu_splats(spu_extract(newb,3)), n, type^0xff);

		} else {
			printf("block %4x %08x %d,%d\n",
				spu_extract(b,0),
				spu_extract(y, 0),
				spu_extract(y,0) >> 16,
				spu_extract(y,0) & 0xffff);
		}
	}
}

unsigned short process_render_chunk(unsigned short chunkStart, unsigned short chunkLength,
				    unsigned short chunkTriangle, unsigned short endTriangle,
				    unsigned long long triangleBase, Renderable* renderable)
{

	char trianglebuffer[ 256 + TRIANGLE_MAX_SIZE ];
	unsigned int extra = chunkTriangle & 127;
	unsigned long long trianglebuffer_ea = triangleBase + (chunkTriangle & ~127);
	Triangle* triangle = (Triangle*) (trianglebuffer+extra);
	unsigned int length = (extra + TRIANGLE_MAX_SIZE + 127) & ~127;
	spu_mfcdma64(trianglebuffer, mfc_ea2h(trianglebuffer_ea), mfc_ea2l(trianglebuffer_ea), length, 0, MFC_GET_CMD);

	mfc_write_tag_mask(1<<0);
	mfc_read_tag_status_all();

	printf("[%d] Read triangle %x, next is %x\n", _SPUID, chunkTriangle, triangle->next_triangle);


	vec_uint4 A   = triangle->area;
	vec_uint4 Adx = triangle->area_dx;
	vec_uint4 Ady = triangle->area_dy;

	int w = 32;
	vec_uint4 Amask = {0, 0, 0, -1};
	vec_uint4 bdelta = { 0, w*w, 2*w*w, 3*w*w };
	subdivide(spu_or(A,Amask), Adx, Ady, spu_splats(0), spu_splats((unsigned short)w), spu_splats(0), bdelta, 0);

/*
	printf("[%d] Screen address: %llx, id %x, locks %d, size %dx%d, stride 0x%x, format %d\n",
		_SPUID,
		renderable->ea, renderable->id, renderable->locks,
		renderable->width, renderable->height, renderable->stride, renderable->format);
*/

	printf("[%d] Processing chunk at %d len %d, triangle %x to renderable %x\n",
		_SPUID,
		chunkStart, chunkLength, chunkTriangle, renderable->id);

//	__asm("stop 0x2110\n\t.word 0");

	return triangle->next_triangle;
}

