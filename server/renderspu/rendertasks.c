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

#define TEST

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
	printf("[%d] %-20s %04x %04x  %04x %04x  %04x %04x  %04x %04x\n", _SPUID, s,
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
		unsigned int chunkNext	= cache->chunkNextArray	   [i];
		int error = 0; //= ( (chunkNext==255) ? 1:0) ^ (cache->chunksFree&mask ? 1 : 0);
		if (chunkNext != CHUNK_NEXT_INVALID) {
		// if (1 || cache->chunksWaiting & mask) {
		//if (error || ! (cache->chunksFree & mask) ) {

			unsigned int chunkStart    	= cache->chunkStartArray   [i];
			unsigned int chunkEnd		= cache->chunkStartArray   [chunkNext & CHUNK_NEXT_MASK];
			//unsigned int chunkLength	= (chunkEnd-chunkStart) & (NUMBER_OF_TILES-1);
			unsigned int chunkLength	= 1 + ( (chunkEnd-1-chunkStart) & (NUMBER_OF_TILES-1) );

			if (chunkNext == CHUNK_NEXT_RESERVED) {
			    printf("[%d] %s %2d - RESERVED\n",
				_SPUID, 
				error ? "ERROR" : "DEBUG",
				i );
			} else {
			    printf("[%d] %s %2d - [%c%c%c] Start %4d Length %4d End %5d Next %2d (%2x) Triangle %x\n",
				_SPUID, 
				error ? "ERROR" : "DEBUG",
				i,
				chunkNext & CHUNK_NEXT_BUSY_BIT /*cache->chunksWaiting & mask*/ ? 'W': '-',
				chunkNext == CHUNK_NEXT_INVALID /*cache->chunksFree & mask*/ ? 'F': '-',
				chunkNext == CHUNK_NEXT_RESERVED ? 'R': '-',
				chunkStart,
				chunkLength,
				chunkEnd,
				chunkNext & CHUNK_NEXT_MASK, chunkNext,
				cache->chunkTriangleArray[i]);
			}
		}
		mask>>=1;
	}
}

char __renderable_base_buffer[ 256 + sizeof(Renderable) ] __attribute__((__aligned__(128)));
unsigned long long currentRenderableBaseAddress = ~0ULL;
Renderable* currentRenderableBase;

////////////////////////////////////////////////////////////////////////////////////////////////////

inline void merge_cache_blocks(RenderableCacheLine* cache)
{
	vec_uchar16 next = cache->chunkNext;

	const vec_uchar16 SHUF0 = (vec_uchar16) {
		0,16,1,17, 2,18,3,19, 4,20,5,21, 6,22, 7,23 };
	const vec_uchar16 SHUF1 = (vec_uchar16) {
		8,24,9,25, 10,26,11,27, 12,28,13,29, 14,30,15,31 };
	const vec_uchar16 MERGE = (vec_uchar16) {
		0,2,4,6, 8,10,12,14, 16,18,20,22, 24,26,28,30 };

//	printf("[%d] Merging:\n", _SPUID);
//	debug_render_tasks(cache);
//	DEBUG_VEC8( next );

	for (;;) {
		vec_uchar16 nextnext = spu_shuffle(next, next, next);
		vec_uchar16 nextmask = spu_and(next, spu_splats((unsigned char)CHUNK_NEXT_MASK));
	
		vec_ushort8 firstblock0 = spu_cmpeq( cache->chunkStart[0], 0);
		vec_ushort8 firstblock1 = spu_cmpeq( cache->chunkStart[1], 0);
		// change next to word offset, note we don't care what the low bit shifted in is
		vec_uchar16 firstshuf = (vec_uchar16) spu_sl( (vec_ushort8)nextmask, 1 );
		vec_uchar16 first = spu_shuffle( firstblock0, firstblock1, firstshuf );
	
		vec_ushort8 tri0 = cache->chunkTriangle[0];
		vec_ushort8 tri1 = cache->chunkTriangle[1];
		vec_uchar16 trishufhi = spu_or ( firstshuf, spu_splats((unsigned char) 1));
		vec_uchar16 trishuflo = spu_and( firstshuf, spu_splats((unsigned char) 254));
	
		vec_ushort8 ntri0 = spu_shuffle( tri0, tri1, spu_shuffle( trishuflo, trishufhi, SHUF0 ) );
		vec_ushort8 ntri1 = spu_shuffle( tri0, tri1, spu_shuffle( trishuflo, trishufhi, SHUF1 ) );
	
		vec_ushort8 trieq0 = spu_cmpeq( tri0, ntri0 );
		vec_ushort8 trieq1 = spu_cmpeq( tri1, ntri1 );
	
		vec_uchar16 trieq = spu_shuffle( trieq0, trieq1, MERGE );
		vec_uchar16 combi = spu_orc(first, trieq);
	
		vec_uchar16 canmerge = spu_cmpgt( spu_nor(spu_or(next, nextnext), combi), 256-CHUNK_NEXT_BUSY_BIT );
	
		vec_uint4 gather = spu_gather( canmerge );
	
		vec_uint4 mergeid = spu_sub( spu_cntlz( gather ), spu_promote((unsigned int)16, 0));
	
		if( !spu_extract(gather, 0) ) {
			// debug_render_tasks(cache);
			return;
		}
	
		//	unsigned int firstchunk = spu_extract(mergeid, 0);
		//	unsigned int nextchunk = cache->chunkNextArray[firstchunk];
		vec_uint4 v_chunkNext = (vec_uint4) si_rotqby( next, spu_add(mergeid,13) );
		vec_uint4 v_chunkNextNext = (vec_uint4) si_rotqby( next, spu_add(v_chunkNext,13) );

		// cache->chunkNextArray[firstchunk] = cache->chunkNextArray[nextchunk];
		next = spu_shuffle( (vec_uchar16) v_chunkNextNext, next, (vec_uchar16) si_cbd( mergeid, 0 ) );

		// cache->chunkNextArray[nextchunk] = CHUNK_NEXT_INVALID;
		next = spu_shuffle( spu_splats( (unsigned char) CHUNK_NEXT_INVALID), next, (vec_uchar16) si_cbd( v_chunkNext, 0 ) );

		// this is for debug use only, it's not really needed...
		// cache->chunkStartArray[nextchunk] = -1;
		cache->chunkStartArray[ spu_extract(v_chunkNext,0) & 255 ] = -1;

		cache->chunkNext = next;
//		printf("[%d] ->\n", _SPUID);
//		DEBUG_VEC8( next );
//		debug_render_tasks(cache);
	}
/*	
*/
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void subdivide(vec_uint4 A, vec_uint4 Adx, vec_uint4 Ady, vec_uint4 y, vec_ushort8 i,
		vec_uint4 base, vec_uint4 baseadd,
		int type)
{
	vec_uint4 Ar = spu_add(A, Adx);
	vec_uint4 Ab = spu_add(A, Ady);
	vec_uint4 Abr = spu_add(Ar, Ady);
	unsigned int outside = spu_extract(spu_orx(spu_rlmaska(
					spu_nor( spu_or(A,Ar), spu_or(Ab,Abr) ), -31)),0);

	if (!outside) {
		i = spu_rlmask(i, -1);
		baseadd = spu_rlmask(baseadd, -2);
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
			// type 2:	x+y	y	0	x
			// type 3:	x+y	x	0	y

			vec_uint4 bit1 = spu_maskw( type );
			vec_uint4 bit0 = spu_maskw( type>>4 );

			vec_uint4 andm = spu_xor( spu_promote(0xffff0000, 0), bit0);
			vec_uint4 im   = (vec_uint4) i;

			// A
			vec_uint4 startA = spu_sel( A, A_hdx_hdy, bit1);
			vec_uint4 dxA	 = spu_sel( hdx, Adx_hdx, bit1);
			vec_uint4 dyA	 = spu_sel( hdy, Ady_hdy, bit1);
			vec_uint4 addA	 = spu_and( im, bit1 );

			// B
			vec_uint4 startB = spu_sel( A_hdy, A_hdx, bit0);
			vec_uint4 dxB	 = spu_sel( hdx, Adx_hdx, bit0);
			vec_uint4 dyB	 = spu_sel( Ady_hdy, hdy, bit0);
			vec_uint4 addB	 = spu_andc( im, andm );

			// C
			vec_uint4 startC = spu_sel( A_hdx_hdy, A, bit1);
			vec_uint4 dxC	 = spu_sel( Adx_hdx, hdx, bit1);
			vec_uint4 dyC	 = spu_sel( Ady_hdy, hdy, bit1);
			vec_uint4 addC	 = spu_andc( im, bit1 );

			// D
			vec_uint4 startD = spu_sel( A_hdx, A_hdy, bit0);
			vec_uint4 dxD	 = spu_sel( Adx_hdx, hdx, bit0);
			vec_uint4 dyD	 = spu_sel( hdy, Ady_hdy, bit0);
			vec_uint4 addD	 = spu_and( im, andm );

			vec_uint4 newbase  = spu_add(base, spu_rlmaskqwbyte(baseadd, -4));
			vec_uint4 baseend  = spu_add(base,baseadd);

			subdivide(startA, dxA, dyA, spu_add(y,addA), i, spu_splats(spu_extract(newbase,0)), baseadd, type^0xf0);
			subdivide(startB, dxB, dyB, spu_add(y,addB), i, spu_splats(spu_extract(newbase,1)), baseadd, type);
			subdivide(startC, dxC, dyC, spu_add(y,addC), i, spu_splats(spu_extract(newbase,2)), baseadd, type);
			subdivide(startD, dxD, dyD, spu_add(y,addD), i, spu_splats(spu_extract(newbase,3)), baseadd, type^0x0f);

		} else {
			printf("block %4x %08x %d,%d\n",
				spu_extract(base,0),
				spu_extract(y, 0),
				spu_extract(y,0) >> 16,
				spu_extract(y,0) & 0xffff);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////

int findFirstTile(vec_uint4 A, vec_uint4 Adx, vec_uint4 Ady, vec_uint4 y, vec_ushort8 i,
		vec_uint4 base, vec_uint4 baseadd,
		int type, unsigned int chunkStart, unsigned int chunkLength, unsigned int chunkEnd)
{
	return -1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void process_render_tasks(unsigned long eah_render_tasks, unsigned long eal_render_tasks)
{
	const vec_uchar16 SHUFFLE_MERGE_BYTES = (vec_uchar16) {	// merge lo bytes from unsigned shorts (array)
		1,3,5,7,9,11,13,15,17,19,21,23,25,27,29,31 };

	const vec_uchar16 SHUFFLE_GET_BUSY_WITH_ONES = (vec_uchar16) {	// get busy flag with ones in unused bytes
		0xc0, 0xc0, 2, 3, 0xc0,0xc0,0xc0,0xc0, 0xc0,0xc0,0xc0,0xc0 };

	const vec_uchar16 ZERO_BYTES = (vec_uchar16) spu_splats(0);

	char trianglebuffer[ 256 + TRIANGLE_MAX_SIZE ];

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

		// check to see if chunk is already at the last triangle
		vec_uint4 doneChunkGather = spu_gather( (vec_uchar16) spu_shuffle(
			(vec_uchar16) spu_cmpeq(testTriangle, cache->chunkTriangle[0]),
			(vec_uchar16) spu_cmpeq(testTriangle, cache->chunkTriangle[1]),
			SHUFFLE_MERGE_BYTES) );

		// check if the chunk is free
		vec_uint4 freeChunkGather = spu_gather(
			spu_cmpeq( spu_splats( (unsigned char) CHUNK_NEXT_INVALID ), cache->chunkNext ) );

		// check to see if the chunk is being processed
		vec_uint4 busyChunkGather = spu_gather(
			spu_cmpgt( cache->chunkNext, //spu_and(cache->chunkNext, CHUNK_NEXT_MASK),
				   spu_splats( (unsigned char) (CHUNK_NEXT_BUSY_BIT-1) ) ) );

		// doneChunkGather, freeChunkGather, busyChunkGather - rightmost 16 bits of word 0
		// note that if freeChunkGather is true then busyChunkGather must also be true

		// done=false, free=false, busy=false -> can process
		// free=false, busy=false -> can be merged

		// decide which chunk to process
		vec_uint4 mayProcessBits = spu_sl( spu_nor( doneChunkGather, busyChunkGather ), 16);
		unsigned int chunkToProcess = spu_extract( spu_cntlz( mayProcessBits ), 0);
		unsigned int freeChunk = spu_extract( spu_cntlz( spu_sl( freeChunkGather, 16 ) ), 0);

		// if there's nothing to process, try the next cache line in the rendering tasks list
		if (!spu_extract(mayProcessBits, 0)) {
			cache_ea = cache->next;
			// __asm("stop 0x2111\n\t.word 0");
			continue;
		}
		
		unsigned int chunkStart    	= cache->chunkStartArray   [chunkToProcess];
		unsigned int chunkTriangle	= cache->chunkTriangleArray[chunkToProcess];
		unsigned int chunkNext		= cache->chunkNextArray	   [chunkToProcess] & CHUNK_NEXT_MASK;
		unsigned int chunkEnd		= (cache->chunkStartArray  [chunkNext]-1) & (NUMBER_OF_TILES-1);
		unsigned int chunkLength	= 1 + chunkEnd-chunkStart;

		// only need an extra block if the block is especially long
		if (chunkLength <= NUMBER_OF_TILES_PER_CHUNK) {
			freeChunk = 32;
		}

		// mark this block as busy
		cache->chunkNextArray[chunkToProcess] |= CHUNK_NEXT_BUSY_BIT;

		// if there's at least one free chunk, claim it
		if (freeChunk != 32) {
			cache->chunkNextArray[freeChunk] = CHUNK_NEXT_RESERVED;
			// cache->chunkStartArray[freeChunk] = 12345;
		}

		// write the cache line back
		spu_mfcdma64(cache, mfc_ea2h(cache_ea), mfc_ea2l(cache_ea), 128, 0, MFC_PUTLLC_CMD);
		if (spu_readch(MFC_RdAtomicStat) & MFC_PUTLLC_STATUS)
			continue;

#ifdef INFO
		printf("[%d] Claimed chunk %d (%d-%d len %d) at tri %x end %x with free chunk %d\n", _SPUID,
			chunkToProcess, chunkStart, chunkEnd, chunkLength, chunkTriangle, endTriangle,
			freeChunk!=32 ? freeChunk : -1 );
#endif

		Triangle* triangle;
		do {
			// read the triangle data for the current triangle
			unsigned int extra = chunkTriangle & 127;
			unsigned long long triangleBase = cache->triangleBase;
			unsigned long long trianglebuffer_ea = triangleBase + (chunkTriangle & ~127);
			triangle = (Triangle*) (trianglebuffer+extra);
			unsigned int length = (extra + TRIANGLE_MAX_SIZE + 127) & ~127;
			spu_mfcdma64(trianglebuffer, mfc_ea2h(trianglebuffer_ea), mfc_ea2l(trianglebuffer_ea),
					length, 0, MFC_GET_CMD);
			mfc_write_tag_mask(1<<0);
			mfc_read_tag_status_all();

			// get the triangle deltas
			vec_uint4 A   = (vec_uint4) triangle->area;
			vec_uint4 Adx = (vec_uint4) triangle->area_dx;
			vec_uint4 Ady = (vec_uint4) triangle->area_dy;
	
			int w = 64;
			vec_uint4 Amask = {0, 0, 0, -1};
			vec_uint4 bdelta = { w*w, 2*w*w, 3*w*w, 4*w*w };
	
			int firstTile = findFirstTile(spu_or(A,Amask), Adx, Ady,
					spu_splats(0U), spu_splats((unsigned short)w), spu_splats(0U), bdelta, 0,
					chunkStart, chunkLength, chunkEnd);
	
			printf("[%d] Processing chunk at %4d len %4d, triangle %04x \n",
				_SPUID, chunkStart, chunkLength, chunkTriangle);
	//		DEBUG_VEC4( A );
	//		DEBUG_VEC4( Adx );
	//		DEBUG_VEC4( Ady );
		} while (0);

		// fake split up the chunk
		if (freeChunk != 32) {
			do {
				// read the cache line
				spu_mfcdma64(cache, mfc_ea2h(cache_ea), mfc_ea2l(cache_ea), 128, 0, MFC_GETLLAR_CMD);
				spu_readch(MFC_RdAtomicStat);

				// chain in the free chunk, keeping this one marked as busy
				cache->chunkStartArray[freeChunk] = chunkStart + NUMBER_OF_TILES_PER_CHUNK;
				cache->chunkNextArray[freeChunk] = chunkNext;
				cache->chunkNextArray[chunkToProcess] = freeChunk | CHUNK_NEXT_BUSY_BIT;
				cache->chunkTriangleArray[freeChunk] = chunkTriangle;

				// write the cache line back
				spu_mfcdma64(cache, mfc_ea2h(cache_ea), mfc_ea2l(cache_ea), 128, 0, MFC_PUTLLC_CMD);
			} while (spu_readch(MFC_RdAtomicStat) & MFC_PUTLLC_STATUS);

			// finally after the write succeeded, update the variables
			chunkNext = freeChunk;
			chunkLength = NUMBER_OF_TILES_PER_CHUNK;
			chunkEnd = chunkStart + chunkLength - 1;
			freeChunk = 32;
		}

#ifdef INFO
		printf("[%d] Processing chunk %d (%d-%d len %d) at tri %x end %x\n", _SPUID,
			chunkToProcess, chunkStart, chunkEnd, chunkLength, chunkTriangle, endTriangle);
#endif

//		endTriangle = process_render_chunk(chunkStart, chunkLength, chunkTriangle, endTriangle,
//					cache->triangleBase, currentRenderableBase);

		endTriangle = triangle->next_triangle;


		// process stuff
//		for (int p=0; p<1; p++)
//			__asm("stop 0x2111\n\t.word 0");

		// update the cache line again
		do {
			// read the cache line
			spu_mfcdma64(cache, mfc_ea2h(cache_ea), mfc_ea2l(cache_ea), 128, 0, MFC_GETLLAR_CMD);
			spu_readch(MFC_RdAtomicStat);

			// mark chunk as available for processing, update endTriangle
			cache->chunkNextArray[chunkToProcess] &= ~CHUNK_NEXT_BUSY_BIT;
			cache->chunkTriangleArray[chunkToProcess] = endTriangle;

			// free spare chunk if we used one
			if (freeChunk != 32) {
				cache->chunkNextArray[freeChunk] = CHUNK_NEXT_INVALID;
			}

			// merge blocks if possible
			merge_cache_blocks(cache);

			// write the cache line back
			spu_mfcdma64(cache, mfc_ea2h(cache_ea), mfc_ea2l(cache_ea), 128, 0, MFC_PUTLLC_CMD);
		} while (spu_readch(MFC_RdAtomicStat) & MFC_PUTLLC_STATUS);

#ifdef INFO
		printf("[%d] Finished chunk %d, now at %x\n", _SPUID, chunkToProcess, endTriangle);
		debug_render_tasks(cache);
#endif
	} // while (cache_ea) - process current cache line
}


////////////////////////////////////////////////////////////////////////////////////////////////////

