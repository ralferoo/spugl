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

////////////////////////////////////////////////////////////////////////////////////////////////////

extern void maskRenderFunc(vec_uint4* pixelbuffer, vec_uint4* params, vec_int4 A, vec_int4 hdx, vec_int4 hdy);
extern void flatRenderFunc(vec_uint4* pixelbuffer, vec_uint4* params, vec_int4 A, vec_int4 hdx, vec_int4 hdy);

////////////////////////////////////////////////////////////////////////////////////////////////////

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
		if (chunkNext != CHUNKNEXT_FREE_BLOCK) {
		// if (1 || cache->chunksWaiting & mask) {
		//if (error || ! (cache->chunksFree & mask) ) {

			unsigned int chunkStart    	= cache->chunkStartArray   [i];
			unsigned int chunkEnd		= cache->chunkStartArray   [chunkNext & CHUNKNEXT_MASK];
			//unsigned int chunkLength	= (chunkEnd-chunkStart) & (NUMBER_OF_TILES-1);
			unsigned int chunkLength	= 1 + ( (chunkEnd-1-chunkStart) & (NUMBER_OF_TILES-1) );

			if (chunkNext == CHUNKNEXT_RESERVED) {
			    printf("[%d] %s %2d - RESERVED\n",
				_SPUID, 
				error ? "ERROR" : "DEBUG",
				i );
			} else {
			    printf("[%d] %s %2d - [%c%c%c] Start %4d Length %4d End %5d Next %2d (%2x) Triangle %x\n",
				_SPUID, 
				error ? "ERROR" : "DEBUG",
				i,
				chunkNext & CHUNKNEXT_BUSY_BIT /*cache->chunksWaiting & mask*/ ? 'W': '-',
				chunkNext == CHUNKNEXT_FREE_BLOCK /*cache->chunksFree & mask*/ ? 'F': '-',
				chunkNext == CHUNKNEXT_RESERVED ? 'R': '-',
				chunkStart,
				chunkLength,
				chunkStart + chunkLength -1, //chunkEnd,
				chunkNext & CHUNKNEXT_MASK, chunkNext,
				cache->chunkTriangleArray[i]);
			}
		}
		mask>>=1;
	}
}

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

	for (;;) {
		vec_uchar16 nextnext = spu_shuffle(next, next, next);
		vec_uchar16 nextmask = spu_and(next, spu_splats((unsigned char)CHUNKNEXT_MASK));
	
		vec_ushort8 firstblock0 = spu_cmpeq( cache->chunkStart[0], 0);
		vec_ushort8 firstblock1 = spu_cmpeq( cache->chunkStart[1], 0);
		// change next to word offset, note we don't care what the low bit shifted in is
		vec_uchar16 firstshuf = (vec_uchar16) spu_sl( (vec_ushort8)nextmask, 1 );
		vec_uchar16 first = (vec_uchar16) spu_shuffle( firstblock0, firstblock1, firstshuf );
	
		vec_ushort8 tri0 = cache->chunkTriangle[0];
		vec_ushort8 tri1 = cache->chunkTriangle[1];
		vec_uchar16 trishufhi = spu_or ( firstshuf, spu_splats((unsigned char) 1));
		vec_uchar16 trishuflo = spu_and( firstshuf, spu_splats((unsigned char) 254));
	
		vec_ushort8 ntri0 = spu_shuffle( tri0, tri1, spu_shuffle( trishuflo, trishufhi, SHUF0 ) );
		vec_ushort8 ntri1 = spu_shuffle( tri0, tri1, spu_shuffle( trishuflo, trishufhi, SHUF1 ) );
	
		vec_ushort8 trieq0 = spu_cmpeq( tri0, ntri0 );
		vec_ushort8 trieq1 = spu_cmpeq( tri1, ntri1 );
	
		vec_uchar16 trieq = (vec_uchar16) spu_shuffle( trieq0, trieq1, MERGE );
		vec_uchar16 combi = spu_orc(first, trieq);
	
		vec_uchar16 canmerge = spu_cmpgt( spu_nor(spu_or(next, nextnext), combi), 256-CHUNKNEXT_BUSY_BIT );
	
		vec_uint4 gather = spu_gather( canmerge );
	
		vec_uint4 mergeid = spu_sub( spu_cntlz( gather ), spu_promote((unsigned int)16, 0));
	
		if( !spu_extract(gather, 0) ) {
			return;
		}
	
		//	unsigned int firstchunk = spu_extract(mergeid, 0);
		//	unsigned int nextchunk = cache->chunkNextArray[firstchunk];
		vec_uint4 v_chunkNext = (vec_uint4) si_rotqby( (qword) next, (qword) spu_add(mergeid,13) );
		vec_uint4 v_chunkNextNext = (vec_uint4) si_rotqby( (qword) next, (qword) spu_add(v_chunkNext,13) );

		// cache->chunkNextArray[firstchunk] = cache->chunkNextArray[nextchunk];
		next = spu_shuffle( (vec_uchar16) v_chunkNextNext, next, (vec_uchar16) si_cbd( (qword) mergeid, 0 ) );

		// cache->chunkNextArray[nextchunk] = CHUNKNEXT_FREE_BLOCK;
		next = spu_shuffle( spu_splats( (unsigned char) CHUNKNEXT_FREE_BLOCK), next, (vec_uchar16) si_cbd( (qword) v_chunkNext, 0 ) );

		// this is for debug use only, it's not really needed...
		// cache->chunkStartArray[nextchunk] = -1;
		cache->chunkStartArray[ spu_extract(v_chunkNext,0) & 255 ] = -1;

		cache->chunkNext = next;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

unsigned int subdivide(vec_int4 A, vec_int4 Adx, vec_int4 Ady, vec_short8 y, vec_short8 i,
		vec_int4 base, vec_int4 baseadd,
		int type, vec_int4 blockMax, unsigned int found);

////////////////////////////////////////////////////////////////////////////////////////////////////

struct {
//	union {
//		vec_uchar16 found;
//		unsigned char foundArray[16];
//	};

	vec_int4 A  [NUMBER_OF_TILES_PER_CHUNK];

	unsigned int found;
	unsigned int coordArray[NUMBER_OF_TILES_PER_CHUNK];
} processTile;

////////////////////////////////////////////////////////////////////////////////////////////////////

vec_uint4 block_blit_list[16][16];
vec_uint4 block_buffer[16][4*32*32/16];
unsigned int block_eah[16];

inline void populate_blit_list(vec_uint4* dma_list_buffer, unsigned int eal, unsigned int lineDy)
{
	unsigned int eal1 = eal + lineDy;
	unsigned int stride2 = lineDy << 1;
	unsigned int stride8 = lineDy << 3;

	vec_uint4 block0 = { 128, eal, 128, eal1 };
	vec_uint4 step2 = { 0, stride2, 0, stride2};
	vec_uint4 step4 = spu_add(step2, step2);
	vec_uint4 step6 = spu_add(step4, step2);
	vec_uint4 step8 = { 0, stride8, 0, stride8};
	vec_uint4 step16 = spu_add(step8, step8);
	vec_uint4 block2 = spu_add(block0, step2);
	vec_uint4 block4 = spu_add(block0, step4);
	vec_uint4 block6 = spu_add(block0, step6);
	vec_uint4 block8 = spu_add(block0, step8);
	vec_uint4 block10 = spu_add(block8, step2);
	vec_uint4 block12 = spu_add(block8, step4);
	vec_uint4 block14 = spu_add(block8, step6);
	vec_uint4 block16 = spu_add(block8, step8);
	vec_uint4 block18 = spu_add(step16, block2);
	vec_uint4 block20 = spu_add(step16, block4);
	vec_uint4 block22 = spu_add(step16, block6);
	vec_uint4 block24 = spu_add(step16, block8);
	vec_uint4 block26 = spu_add(step16, block10);
	vec_uint4 block28 = spu_add(step16, block12);
	vec_uint4 block30 = spu_add(step16, block14);

	dma_list_buffer[0] = block0;
	dma_list_buffer[1] = block2;
	dma_list_buffer[2] = block4;
	dma_list_buffer[3] = block6;
	dma_list_buffer[4] = block8;
	dma_list_buffer[5] = block10;
	dma_list_buffer[6] = block12;
	dma_list_buffer[7] = block14;

	dma_list_buffer[8] = block16;
	dma_list_buffer[9] = block18;
	dma_list_buffer[10] = block20;
	dma_list_buffer[11] = block22;
	dma_list_buffer[12] = block24;
	dma_list_buffer[13] = block26;
	dma_list_buffer[14] = block28;
	dma_list_buffer[15] = block30;
}

void initTileBuffers(unsigned int firstTile, unsigned int chunkEnd)
{
	processTile.found = 0;
}
void flushTileBuffers(unsigned int firstTile, unsigned int chunkEnd)
{
#ifdef INFO
	printf("[%d] Flush tiles %04x\n", _SPUID, processTile.found>>16);
#endif

	// flush the tiles out
	unsigned int found=processTile.found;
	unsigned int tags = 0;
	while (found) {
		unsigned int block = spu_extract( spu_cntlz( spu_promote(found, 0) ), 0);
		found &= ~( 0x80000000>>block );

		unsigned int coord = processTile.coordArray[block];
		vec_uint4* blit_list = block_blit_list[block];
		vec_uint4* pixelbuffer = (vec_uint4*) block_buffer[block];
		unsigned int eah = block_eah[block];

		// wait for 2 slots to open up (one for pixel buffer one for Z-buffer)
		do {} while (spu_readchcnt(MFC_Cmd)<2);

		// write the pixel data
		spu_mfcdma64(pixelbuffer, eah, (unsigned int)blit_list, 8*32, block, MFC_PUTL_CMD);
		tags |= (1<<block);

// command queue is 16 elements deep, so we should be fine here
//		// DEBUG, just in case
//		mfc_write_tag_mask(tags);
//		mfc_read_tag_status_all();
	}

//	printf("[%d] Waiting for tags %04x\n", _SPUID, tags);

	// wait for all blocks to flush
	mfc_write_tag_mask(tags);
	mfc_read_tag_status_all();
}

void processTriangleChunks(Triangle* triangle, RenderableCacheLine* cache, int firstTile, int chunkEnd, unsigned int chunkTriangle)
{
#ifdef INFO
	printf("[%d] Processing tiles %d to %d on tri %04x\n",
		_SPUID, firstTile, chunkEnd, chunkTriangle);
#endif

	int w = 64;
	vec_int4 INITIAL_BASE = spu_splats(-firstTile);
	vec_int4 INITIAL_BASE_ADD = { w*w, 2*w*w, 3*w*w, 4*w*w };
	vec_short8 ZEROS = spu_splats((short)0);
	vec_short8 INITIAL_i = spu_splats((short)w);
	
	vec_int4 A   = triangle->area;
	vec_int4 Adx = triangle->area_dx;
	vec_int4 Ady = triangle->area_dy;
	
	vec_int4 hdx = spu_rlmaska(Adx, -6);
	vec_int4 hdy = spu_rlmaska(Ady, -6);

	unsigned int blocksToProcess = subdivide(A, Adx, Ady,
		ZEROS, INITIAL_i, INITIAL_BASE, INITIAL_BASE_ADD, 0, spu_splats(chunkEnd-firstTile+1), 0); 
		// TODO: this +1 looks screwey

	// mark the tiles as found
	unsigned int oldFound = processTile.found;
	processTile.found |= blocksToProcess;

	// create the DMA lists and read in the tiles to be changed
	unsigned int found=blocksToProcess;
	while (found) {
		unsigned int block = spu_extract( spu_cntlz( spu_promote(found, 0) ), 0);
		unsigned int mask = ( 0x80000000>>block );
		found &= ~mask;

		if ( (~oldFound) & mask ) {
			unsigned int coord = processTile.coordArray[block];

			vec_ushort8 coord_vec = (vec_ushort8) spu_promote(coord, 0);
			unsigned short coordx = spu_extract(coord_vec,0);
			unsigned short coordy = spu_extract(coord_vec,1);

			unsigned int offsetx = coordx * cache->pixelTileDx;
			unsigned int offsety = coordy * cache->pixelTileDy;
			unsigned long long pixelbuffer_ea = cache->pixelBuffer + offsetx + offsety;

			vec_uint4* blit_list = block_blit_list[block];
			populate_blit_list(blit_list, mfc_ea2l(pixelbuffer_ea), cache->pixelLineDy);
			vec_uint4* pixelbuffer = (vec_uint4*) block_buffer[block];
			unsigned int eah = mfc_ea2h(pixelbuffer_ea);
			block_eah[block] = eah;

#ifdef INFO
			printf("[%d] Block %x %08x (%2d,%2d) EA=%llx list=%05x, pix=%05x tri=%04x\n",
				_SPUID, block, coord, coordx, coordy, pixelbuffer_ea, blit_list, pixelbuffer, chunkTriangle);
#endif

			// wait for 2 slots to open up (one for pixel buffer one for Z-buffer)
			do {} while (spu_readchcnt(MFC_Cmd)<2);

			// read the existing pixel data
			spu_mfcdma64(pixelbuffer, eah, (unsigned int)blit_list, 8*32, block, MFC_GETL_CMD);
		}
	}

	if (!blocksToProcess) {
		printf("[%d] No blocks generated from tiles %d to %d on tri %04x\n",
			_SPUID, firstTile, chunkEnd, chunkTriangle);
	}

	// now render each block
	found=blocksToProcess;
	while (found) {
		unsigned int block = spu_extract( spu_cntlz( spu_promote(found, 0) ), 0);
		found &= ~( 0x80000000>>block );

		// ensure the block has finished DMA
		mfc_write_tag_mask(1 << block);
		mfc_read_tag_status_all();

		// do the actual rendering
		A=processTile.A[block];

//		maskRenderFunc(block_buffer[block], &triangle->shader_params[0], A, hdx, hdy);
		flatRenderFunc(block_buffer[block], &triangle->shader_params[0], A, hdx, hdy);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

unsigned int subdivide(vec_int4 A, vec_int4 Adx, vec_int4 Ady, vec_short8 y, vec_short8 i,
		vec_int4 base, vec_int4 baseadd,
		int type, vec_int4 blockMax, unsigned int found)
{
	vec_int4 Ar = spu_add(A, Adx);
	vec_int4 Ab = spu_add(A, Ady);
	vec_int4 Abr = spu_add(Ar, Ady);
	unsigned int outside = spu_extract(spu_orx(spu_rlmaska(
					spu_nor( spu_or(A,Ar), spu_or(Ab,Abr) ), -31)),0);

	if (!outside) {
		i = spu_rlmask(i, -1);
		baseadd = spu_rlmask(baseadd, -2);
		if (spu_extract(i, 1)) {
			vec_int4 hdx = spu_rlmaska(Adx, -1);
			vec_int4 hdy = spu_rlmaska(Ady, -1);

			vec_int4 A_hdx	 	= spu_add(A,     hdx);
			vec_int4 A_hdy		= spu_add(A,     hdy);
			vec_int4 A_hdx_hdy	= spu_add(A_hdx, hdy);

			vec_int4 Adx_hdx	= spu_sub(Adx,hdx);
			vec_int4 Ady_hdy	= spu_sub(Ady,hdy);

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
			vec_short8 im   = i;

			// A
			vec_int4 startA	= spu_sel( A, A_hdx_hdy, bit1);
			vec_int4 dxA	= spu_sel( hdx, Adx_hdx, bit1);
			vec_int4 dyA	= spu_sel( hdy, Ady_hdy, bit1);
			vec_short8 addA	= spu_and( im, (vec_short8) bit1 );

			// B
			vec_int4 startB	= spu_sel( A_hdy, A_hdx, bit0);
			vec_int4 dxB	= spu_sel( hdx, Adx_hdx, bit0);
			vec_int4 dyB	= spu_sel( Ady_hdy, hdy, bit0);
			vec_short8 addB	= spu_andc( im, (vec_short8) andm );

			// C
			vec_int4 startC	= spu_sel( A_hdx_hdy, A, bit1);
			vec_int4 dxC	= spu_sel( Adx_hdx, hdx, bit1);
			vec_int4 dyC	= spu_sel( Ady_hdy, hdy, bit1);
			vec_short8 addC	= spu_andc( im, (vec_short8) bit1 );

			// D
			vec_int4 startD	= spu_sel( A_hdx, A_hdy, bit0);
			vec_int4 dxD	= spu_sel( Adx_hdx, hdx, bit0);
			vec_int4 dyD	= spu_sel( hdy, Ady_hdy, bit0);
			vec_short8 addD	= spu_and( im, (vec_short8) andm );

			vec_int4 newbase  = spu_add(base, spu_rlmaskqwbyte(baseadd, -4));
			vec_int4 baseend  = spu_add(base,baseadd);

			// chunkStart < baseEnd && chunkEnd > newBase
			// chunkStart >= baseEnd && chunkEnd > newBase

			//vec_uint4 v_process_1 = spu_rlmask(baseend, -31); //spu_cmpgt(baseend, spu_splats(0));
			vec_uint4 v_process_1 = spu_cmpgt(baseend, spu_splats(0));
			vec_uint4 v_process_2 = spu_cmpgt(blockMax, newbase);
			vec_uint4 v_process = v_process_2; //spu_and( v_process_2, v_process_1 );
		// TODO: this looks screwey

			if (spu_extract(v_process, 0)) {
				found  = subdivide(startA, dxA, dyA, spu_add(y,addA), i, spu_splats(spu_extract(newbase,0)), baseadd, type^0xf0, blockMax, found);
			}
			if (spu_extract(v_process, 1)) {
				found = subdivide(startB, dxB, dyB, spu_add(y,addB), i, spu_splats(spu_extract(newbase,1)), baseadd, type, blockMax, found);
			}
			if (spu_extract(v_process, 2)) {
				found = subdivide(startC, dxC, dyC, spu_add(y,addC), i, spu_splats(spu_extract(newbase,2)), baseadd, type, blockMax, found);
			}
			if (spu_extract(v_process, 3)) {
				found = subdivide(startD, dxD, dyD, spu_add(y,addD), i, spu_splats(spu_extract(newbase,3)), baseadd, type^0x0f, blockMax, found);
			}

		} else {
			int block = spu_extract(base,0);

			if (block>=0 && block<spu_extract(blockMax, 0)) {
				found |= 0x80000000>>block;
				processTile.coordArray[block] = spu_extract( (vec_uint4)y, 0);
				processTile.A[block] = A;
				// bottom bit of Adx and Ady may change, but I don't think we need to worry
				// DEBUG_VEC4(Adx);
				// DEBUG_VEC4(Ady);
			} else if (block>=0) {
				printf("[%d] Losing coord %2d,%2d (block=%d)\n", _SPUID, spu_extract(y,0), spu_extract(y,1), block );
			}
		}
	}
	return found;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

int findFirstTriangleTile(Triangle* triangle, int chunkStart, int chunkEnd);

////////////////////////////////////////////////////////////////////////////////////////////////////

int findFirstTile(vec_int4 A, vec_int4 Adx, vec_int4 Ady, vec_uint4 y, vec_ushort8 i,
		vec_uint4 base, vec_uint4 baseadd,
		int type, vec_uint4 v_start, vec_uint4 v_end)
{
	vec_int4 Ar = spu_add(A, Adx);
	vec_int4 Ab = spu_add(A, Ady);
	vec_int4 Abr = spu_add(Ar, Ady);
	unsigned int outside = spu_extract(spu_orx(spu_rlmaska(
					spu_nor( spu_or(A,Ar), spu_or(Ab,Abr) ), -31)),0);

	if (!outside) {
		i = spu_rlmask(i, -1);
		baseadd = spu_rlmask(baseadd, -2);
		if (spu_extract(i, 1)) {
			vec_int4 hdx = spu_rlmaska(Adx, -1);
			vec_int4 hdy = spu_rlmaska(Ady, -1);

			vec_int4 A_hdx 		= spu_add(A,     hdx);
			vec_int4 A_hdy		= spu_add(A,     hdy);
			vec_int4 A_hdx_hdy	= spu_add(A_hdx, hdy);

			vec_int4 Adx_hdx	= spu_sub(Adx,hdx);
			vec_int4 Ady_hdy	= spu_sub(Ady,hdy);

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
			vec_int4 startA	= spu_sel( A, A_hdx_hdy, bit1);
			vec_int4 dxA	= spu_sel( hdx, Adx_hdx, bit1);
			vec_int4 dyA	= spu_sel( hdy, Ady_hdy, bit1);
			vec_uint4 addA	= spu_and( im, bit1 );

			// B
			vec_int4 startB	= spu_sel( A_hdy, A_hdx, bit0);
			vec_int4 dxB	= spu_sel( hdx, Adx_hdx, bit0);
			vec_int4 dyB	= spu_sel( Ady_hdy, hdy, bit0);
			vec_uint4 addB	= spu_andc( im, andm );

			// C
			vec_int4 startC	= spu_sel( A_hdx_hdy, A, bit1);
			vec_int4 dxC	= spu_sel( Adx_hdx, hdx, bit1);
			vec_int4 dyC	= spu_sel( Ady_hdy, hdy, bit1);
			vec_uint4 addC	= spu_andc( im, bit1 );

			// D
			vec_int4 startD	= spu_sel( A_hdx, A_hdy, bit0);
			vec_int4 dxD	= spu_sel( Adx_hdx, hdx, bit0);
			vec_int4 dyD	= spu_sel( hdy, Ady_hdy, bit0);
			vec_uint4 addD	= spu_and( im, andm );

			vec_uint4 newbase  = spu_add(base, spu_rlmaskqwbyte(baseadd, -4));
			vec_uint4 baseend  = spu_add(base,baseadd);

			// chunkStart < baseEnd && chunkEnd > newBase
			// chunkStart >= baseEnd && chunkEnd > newBase

			vec_uint4 v_process_1 = spu_cmpgt(baseend, v_start);
			vec_uint4 v_process_2 = spu_cmpgt(v_end, newbase);
			vec_uint4 v_process = spu_and( v_process_1, v_process_2 );

			int first;
			if (spu_extract(v_process, 0)) {
				first = findFirstTile(startA, dxA, dyA, spu_add(y,addA), i, spu_splats(spu_extract(newbase,0)), baseadd, type^0xf0, v_start, v_end);
				//printf("[%d] first=%d\n", _SPUID, first);
				if (first>=0) return first;
			}
			if (spu_extract(v_process, 1)) {
				first = findFirstTile(startB, dxB, dyB, spu_add(y,addB), i, spu_splats(spu_extract(newbase,1)), baseadd, type, v_start, v_end);
				//printf("[%d] first=%d\n", _SPUID, first);
				if (first>=0) return first;
			}
			if (spu_extract(v_process, 2)) {
				first = findFirstTile(startC, dxC, dyC, spu_add(y,addC), i, spu_splats(spu_extract(newbase,2)), baseadd, type, v_start, v_end);
				//printf("[%d] first=%d\n", _SPUID, first);
				if (first>=0) return first;
			}
			if (spu_extract(v_process, 3)) {
				first = findFirstTile(startD, dxD, dyD, spu_add(y,addD), i, spu_splats(spu_extract(newbase,3)), baseadd, type^0x0f, v_start, v_end);
				//printf("[%d] first=%d\n", _SPUID, first);
				if (first>=0) return first;
			}
		} else {
			return spu_extract(base, 0);
		}
	}
	return -1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

int findFirstTriangleTile(Triangle* triangle, int chunkStart, int chunkEnd)
{
	int w = 64;
	vec_uint4 INITIAL_BASE_ADD = { w*w, 2*w*w, 3*w*w, 4*w*w };
	vec_uint4 ZEROS = spu_splats(0U);
	vec_ushort8 INITIAL_i = spu_splats((unsigned short)w);
	
	vec_int4 A   = triangle->area;
	vec_int4 Adx = triangle->area_dx;
	vec_int4 Ady = triangle->area_dy;
	
	return findFirstTile(A, Adx, Ady,
		ZEROS, INITIAL_i, ZEROS, INITIAL_BASE_ADD, 0,
		(vec_uint4) spu_splats( chunkStart ), (vec_uint4) spu_splats( chunkEnd ) );
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

	RenderableCacheLine* cache = (RenderableCacheLine*) aligned_sync_buffer;
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
			spu_cmpeq( spu_splats( (unsigned char) CHUNKNEXT_FREE_BLOCK ), cache->chunkNext ) );

		// check to see if the chunk is being processed
		vec_uint4 busyChunkGather = spu_gather(
			spu_cmpgt( cache->chunkNext, //spu_and(cache->chunkNext, CHUNKNEXT_MASK),
				   spu_splats( (unsigned char) (CHUNKNEXT_BUSY_BIT-1) ) ) );

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
trynextcacheline:
			cache_ea = cache->next;
			// __asm("stop 0x2111\n\t.word 0");
			continue;
		}
		
		unsigned int chunkStart    	= cache->chunkStartArray   [chunkToProcess];
		unsigned int chunkTriangle	= cache->chunkTriangleArray[chunkToProcess];
		unsigned int chunkNext		= cache->chunkNextArray	   [chunkToProcess] & CHUNKNEXT_MASK;
		unsigned int chunkEnd		= (cache->chunkStartArray  [chunkNext]-1) & (NUMBER_OF_TILES-1);
		unsigned int chunkLength	= 1 + chunkEnd-chunkStart;

		// only need an extra block if the block is especially long
		if (chunkLength <= NUMBER_OF_TILES_PER_CHUNK) {
			freeChunk = 32;
		}

		// mark this block as busy
		cache->chunkNextArray[chunkToProcess] |= CHUNKNEXT_BUSY_BIT;

		// if there's at least one free chunk, claim it
		if (freeChunk != 32) {
			cache->chunkNextArray[freeChunk] = CHUNKNEXT_RESERVED;
			cache->chunkTriangleArray[freeChunk] = chunkTriangle;
		}

		// write the cache line back
		spu_mfcdma64(cache, mfc_ea2h(cache_ea), mfc_ea2l(cache_ea), 128, 0, MFC_PUTLLC_CMD);
		if (spu_readch(MFC_RdAtomicStat) & MFC_PUTLLC_STATUS)
			continue;

#ifdef INFO
		printf("[%d] Claimed chunk %d (%d-%d len %d) at tri %x end %x with free chunk %d\n", _SPUID,
			chunkToProcess, chunkStart, chunkEnd, chunkLength, chunkTriangle, endTriangle,
			freeChunk!=32 ? freeChunk : -1 );
//		debug_render_tasks(cache);
#endif

		Triangle* triangle;
		int firstTile;
		do {
			// read the triangle data for the current triangle
			unsigned int extra = chunkTriangle & 127;
			unsigned long long trianglebuffer_ea = cache_ea + TRIANGLE_OFFSET_FROM_CACHE_LINE + (chunkTriangle & ~127);
			triangle = (Triangle*) (trianglebuffer+extra);
			unsigned int length = (extra + TRIANGLE_MAX_SIZE + 127) & ~127;
			spu_mfcdma64(trianglebuffer, mfc_ea2h(trianglebuffer_ea), mfc_ea2l(trianglebuffer_ea),
					length, 0, MFC_GET_CMD);
			mfc_write_tag_mask(1<<0);
			mfc_read_tag_status_all();

			// get the triangle deltas
			firstTile = findFirstTriangleTile(triangle, chunkStart, chunkEnd);

			if (firstTile>=0)
				break;

			// no match, try next triangle
			chunkTriangle = triangle->next_triangle;
		} while (chunkTriangle != endTriangle);

		// if we actually have something to process...
		if (firstTile>=0) {
			// the "normal" splitting will now become:
			// chunkStart .. (firstTile-1)	-> triangle->next_triangle
			// firstTile .. (firstTile+NUM-1) -> chunkTriangle (BUSY)
			// (firstTile+NUM) .. chunkEnd -> chunkTriangle (FREE)

			int tailChunk;
			int thisChunk;
			int nextBlockStart;
			int thisBlockStart;
			int realBlockStart;
			do {
retry:
				// read the cache line
				spu_mfcdma64(cache, mfc_ea2h(cache_ea), mfc_ea2l(cache_ea), 128, 0, MFC_GETLLAR_CMD);
				spu_readch(MFC_RdAtomicStat);
	
				// calculate start of next block
				nextBlockStart = firstTile + NUMBER_OF_TILES_PER_CHUNK;
				if (nextBlockStart > chunkEnd)
					nextBlockStart = chunkEnd+1;
	
				// calculate start of block to mark as busy
				thisBlockStart = nextBlockStart - NUMBER_OF_TILES_PER_CHUNK;
				if (thisBlockStart < chunkStart)
					thisBlockStart = chunkStart;
				realBlockStart = thisBlockStart;
	
				// allocate some more free chunks
				vec_uint4 freeChunkGather2 = spu_sl(spu_gather(spu_cmpeq(
					spu_splats((unsigned char)CHUNKNEXT_FREE_BLOCK), cache->chunkNext)), 16);
				unsigned int freeChunk2 = spu_extract(spu_cntlz(freeChunkGather2), 0);

				if (freeChunk == 32) {
					// if we didn't have one before, try again
					freeChunk = freeChunk2;

					// and try to get the second one
					freeChunkGather2 = spu_andc( freeChunkGather2, spu_promote(0x80000000>>freeChunk2, 0) );
					freeChunk2 = spu_extract(spu_cntlz(freeChunkGather2), 0);
				} else {
					// speculatively clear the free chunk just in case we don't need it
					cache->chunkNextArray[freeChunk] = CHUNKNEXT_FREE_BLOCK;
				}

#ifdef INFO
				printf("[%d] Free chunks %d and %d, cN=%d, nBS=%d, cE=%d, tBS=%d, cS=%d\n",
					_SPUID, freeChunk, freeChunk2, chunkNext, nextBlockStart, chunkEnd, thisBlockStart, chunkStart );
#endif

				// mark region after as available for processing if required
				if (nextBlockStart < chunkEnd) {
					if (freeChunk==32) {
						// if no free chunk, relinquish entire block and write back
						cache->chunkNextArray[chunkToProcess] = chunkNext;
						spu_mfcdma64(cache, mfc_ea2h(cache_ea), mfc_ea2l(cache_ea), 128, 0, MFC_PUTLLC_CMD);
						// if writeback failed, we *might* have a free block, retry
						if (spu_readch(MFC_RdAtomicStat) & MFC_PUTLLC_STATUS)
							goto retry;

						// otherwise give up and try the next cache line
						goto trynextcacheline;
					}
					cache->chunkStartArray[freeChunk] = nextBlockStart;
					cache->chunkNextArray[freeChunk] = chunkNext;
					cache->chunkTriangleArray[freeChunk] = chunkTriangle;
					cache->chunkNextArray[chunkToProcess] = freeChunk | CHUNKNEXT_BUSY_BIT;
					tailChunk = freeChunk;
#ifdef INFO
					printf("[%d] Insert tail, tailChunk=%d, chunkNext=%d, chunkToProcess=%d\n", _SPUID, tailChunk, chunkNext, chunkToProcess);
					debug_render_tasks(cache);
#endif
				} else {
					// we're gonna use freeChunk2 for the "in front" block, as we've not
					// used freeChunk, let's use it as it's more likely to have a free chunk
					freeChunk2 = freeChunk;
					tailChunk = chunkNext;
				}

				// mark region before as available if required and possible
				thisChunk = chunkToProcess;
				if (thisBlockStart > chunkStart) {
					if (freeChunk2 != 32) {
						// mark this region as busy
						cache->chunkStartArray[freeChunk2]=thisBlockStart;
						cache->chunkNextArray[freeChunk2]=tailChunk | CHUNKNEXT_BUSY_BIT;
						cache->chunkTriangleArray[freeChunk2]=chunkTriangle;

						// mark region before as available for processing
						cache->chunkNextArray[chunkToProcess]=freeChunk2;
						cache->chunkTriangleArray[chunkToProcess]=triangle->next_triangle;
						thisChunk = freeChunk2;
#ifdef INFO
						printf("[%d] Insert new head, tailChunk=%d, chunkNext=%d, thisChunk=%d\n", _SPUID, tailChunk, chunkNext, thisChunk);
						debug_render_tasks(cache);
#endif
					} else {
						// need to keep whole block, update info and mark bust
						cache->chunkTriangleArray[chunkToProcess]=chunkTriangle;
						cache->chunkNextArray[chunkToProcess]=tailChunk | CHUNKNEXT_BUSY_BIT;
						realBlockStart = chunkStart;
//#ifdef INFO
						printf("[%d] Keep whole block, tailChunk=%d, chunkNext=%d, thisChunk=%d\n", _SPUID, tailChunk, chunkNext, thisChunk);
						debug_render_tasks(cache);
//#endif
					}
				}

				// write the cache line back
				spu_mfcdma64(cache, mfc_ea2h(cache_ea), mfc_ea2l(cache_ea), 128, 0, MFC_PUTLLC_CMD);
			} while (spu_readch(MFC_RdAtomicStat) & MFC_PUTLLC_STATUS);
	
			// finally after the write succeeded, update the variables
			chunkNext = tailChunk;
			chunkToProcess = thisChunk;
			chunkStart = thisBlockStart;
			chunkLength = nextBlockStart - firstTile;
			chunkEnd = chunkStart + chunkLength - 1;
			freeChunk = 32;

			// now we can process the block up to endTriangle
			initTileBuffers(thisBlockStart, chunkEnd);

			while (chunkTriangle != endTriangle) {
#ifdef INFO
				printf("[%d] Processing chunk %d at %4d len %4d, triangle %04x first=%d tbs=%d\n",
					_SPUID, chunkToProcess, chunkStart, chunkLength,
					chunkTriangle, firstTile, thisBlockStart);
#endif
				// and actually process that triangle on these chunks
				processTriangleChunks(triangle, cache, thisBlockStart, chunkEnd, chunkTriangle);
#ifdef PAUSE
				__asm("stop 0x2111\n\t.word 0");
#endif
				// and advance to the next-triangle
				chunkTriangle = triangle->next_triangle;

				// this should only ever happen if we're running really low on cache line slots
				// basically, if we pick up a block with more than NUMBER_OF_TILES_PER_CHUNK and
				// there's no slot to store the pre-NUMBER_OF_TILES_PER_CHUNK tiles.
				// in this case, we process from thisBlockStart only (because we know that from
				// chunkStart to there has no result) and then we only process one triangle
				if (chunkStart != realBlockStart) {
					printf("[%d] chunkStart=%d != realBlockStart %d, chunkEnd=%d, "
						"firstTile=%d chunk=%d\n",
						_SPUID, chunkStart, realBlockStart, chunkEnd,
						firstTile, chunkToProcess);
					debug_render_tasks(cache);

					// abort the while loop
					break;
				}

				// read the next triangle
				unsigned int extra = chunkTriangle & 127;
				unsigned long long trianglebuffer_ea = cache_ea + TRIANGLE_OFFSET_FROM_CACHE_LINE + (chunkTriangle & ~127);
				triangle = (Triangle*) (trianglebuffer+extra);
				unsigned int length = (extra + TRIANGLE_MAX_SIZE + 127) & ~127;
				spu_mfcdma64(trianglebuffer, mfc_ea2h(trianglebuffer_ea),
						mfc_ea2l(trianglebuffer_ea), length, 0, MFC_GET_CMD);
				mfc_write_tag_mask(1<<0);
				mfc_read_tag_status_all();
			} // until chunkTriangle == endTriangle

			// flush any output buffers
			flushTileBuffers(thisBlockStart, chunkEnd);

		} // firstTile>=0

		// when finished, mark chunk as done and free reserved chunk (if still in use)
		do {
			// read the cache line
			spu_mfcdma64(cache, mfc_ea2h(cache_ea), mfc_ea2l(cache_ea), 128, 0, MFC_GETLLAR_CMD);
			spu_readch(MFC_RdAtomicStat);

			// free reserved chunk
			if (freeChunk != 32)
				cache->chunkNextArray[freeChunk] = CHUNKNEXT_FREE_BLOCK;

			// mark block as completed up to triangle
			cache->chunkNextArray[chunkToProcess] = chunkNext;
			cache->chunkTriangleArray[chunkToProcess] = chunkTriangle;

			// merge chunks
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

