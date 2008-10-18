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

// #define INFO

////////////////////////////////////////////////////////////////////////////////////////////////////

extern void maskRenderFunc(vec_uint4* pixelbuffer, vec_uint4* params, vec_int4 A, vec_int4 hdx, vec_int4 hdy);
extern void flatRenderFunc(vec_uint4* pixelbuffer, vec_uint4* params, vec_int4 A, vec_int4 hdx, vec_int4 hdy);

////////////////////////////////////////////////////////////////////////////////////////////////////

unsigned int subdivide(vec_int4 A, vec_int4 Adx, vec_int4 Ady, vec_short8 y, vec_short8 i,
		vec_int4 base, vec_int4 baseadd,
		int type, int blockStart, vec_int4 blockMax, unsigned int found);

////////////////////////////////////////////////////////////////////////////////////////////////////

struct {
	vec_int4 A  [NUMBER_OF_TILES_PER_CHUNK];

	unsigned int found;
	unsigned int coordArray[NUMBER_OF_TILES_PER_CHUNK];
} processTile;

////////////////////////////////////////////////////////////////////////////////////////////////////

vec_uint4 block_blit_list[16][16];
vec_uint4 block_buffer[16][4*32*32/16] __attribute__((aligned(128)));
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
	printf("[%d] Flush tiles %08x\n", _SPUID, processTile.found);
#ifdef INFO
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

void processTriangleChunks(Triangle* triangle, RenderableCacheLine* cache, int firstTile, int chunkEnd, unsigned int chunkTriangle, int ok)
{
	printf("[%d] Processing tiles %d to %d on tri %04x\n",
		_SPUID, firstTile, chunkEnd, chunkTriangle);
#ifdef INFO
#endif

	int w = 64;
	vec_int4 INITIAL_BASE = spu_splats(0); //-firstTile);
	vec_int4 INITIAL_BASE_ADD = { w*w, 2*w*w, 3*w*w, 4*w*w };
	vec_short8 ZEROS = spu_splats((short)0);
	vec_short8 INITIAL_i = spu_splats((short)w);
	
	vec_int4 A   = triangle->area;
	vec_int4 Adx = triangle->area_dx;
	vec_int4 Ady = triangle->area_dy;
	
	vec_int4 hdx = spu_rlmaska(Adx, -6);
	vec_int4 hdy = spu_rlmaska(Ady, -6);

	unsigned int blocksToProcess = subdivide(A, Adx, Ady,
		ZEROS, INITIAL_i, INITIAL_BASE, INITIAL_BASE_ADD, 0, firstTile, spu_splats(chunkEnd+1), 0); 
		// TODO: this +1 looks screwey

	printf("btp=%d\n", blocksToProcess);

	// if no blocks returned, short circuit
	if (!blocksToProcess && !ok) {
		printf("[%d] No blocks generated from tiles %d to %d on tri %04x %s\n",
			_SPUID, firstTile, chunkEnd, chunkTriangle, ok ? "OK" : "BAD");
/*
		DEBUG_VEC4( A );
		DEBUG_VEC4( Adx );
		DEBUG_VEC4( Ady );
*/
		return;
	}

	// mark the tiles as found
	unsigned int oldFound = processTile.found;
	processTile.found |= blocksToProcess;

	// create the DMA lists and read in the tiles to be changed
	unsigned int found=blocksToProcess;
	unsigned int wait = 0;
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

			// create the blit list (reading)
			vec_uint4* blit_list = block_blit_list[block];
			populate_blit_list(blit_list, mfc_ea2l(pixelbuffer_ea), cache->pixelLineDy);
			vec_uint4* pixelbuffer = (vec_uint4*) block_buffer[block];
			unsigned int eah = mfc_ea2h(pixelbuffer_ea);
			block_eah[block] = eah;

//#ifdef INFO
			printf("[%d] Block %x %08x (%2d,%2d) EA=%llx list=%05x, pix=%05x tri=%04x\n",
				_SPUID, block, coord, coordx, coordy, pixelbuffer_ea, blit_list, pixelbuffer, chunkTriangle);
//#endif

			for (int zzz=0; zzz<30; zzz++)
				for (int k=0; k<1000000; k++)
					__asm("nop");
/*
*/

			// wait for 2 slots to open up (one for pixel buffer one for Z-buffer)
			int a;
			do {
				a=spu_readchcnt(MFC_Cmd);
				printf("waiting, count=%d\n", a);
			}
			while (a<12);

			// read the existing pixel data
			spu_mfcdma64(pixelbuffer, eah, (unsigned int)blit_list, 8*32, block, MFC_GETL_CMD);
			wait |= mask;
//		mfc_write_tag_mask(1 << block);
//		mfc_read_tag_status_all();
		}
	}
	printf("rah\n");

	// now render each block
	found=blocksToProcess;
	while (found) {
		unsigned int block = spu_extract( spu_cntlz( spu_promote(found, 0) ), 0);
		unsigned int mask = ( 0x80000000>>block );
		found &= ~mask;

		printf("func on block %d, mask %x\n", block, mask);

		// ensure the block has finished DMA
		if (wait&mask) {
			mfc_write_tag_mask(1 << block);
			mfc_read_tag_status_all();
		}

		printf("waited on block %d, mask %x\n", block, mask);

		// do the actual rendering
		A=processTile.A[block];

//		maskRenderFunc(block_buffer[block], &triangle->shader_params[0], A, hdx, hdy);
		flatRenderFunc(block_buffer[block], &triangle->shader_params[0], A, hdx, hdy);

		printf("rendered block %d, mask %x\n", block, mask);
	}
	printf("dah\n");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

unsigned int subdivide(vec_int4 A, vec_int4 Adx, vec_int4 Ady, vec_short8 y, vec_short8 i,
		vec_int4 base, vec_int4 baseadd,
		int type, int blockStart, vec_int4 blockMax, unsigned int found)
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
				found  = subdivide(startA, dxA, dyA, spu_add(y,addA), i, spu_splats(spu_extract(newbase,0)), baseadd, type^0xf0, blockStart, blockMax, found);
			}
			if (spu_extract(v_process, 1)) {
				found = subdivide(startB, dxB, dyB, spu_add(y,addB), i, spu_splats(spu_extract(newbase,1)), baseadd, type, blockStart, blockMax, found);
			}
			if (spu_extract(v_process, 2)) {
				found = subdivide(startC, dxC, dyC, spu_add(y,addC), i, spu_splats(spu_extract(newbase,2)), baseadd, type, blockStart, blockMax, found);
			}
			if (spu_extract(v_process, 3)) {
				found = subdivide(startD, dxD, dyD, spu_add(y,addD), i, spu_splats(spu_extract(newbase,3)), baseadd, type^0x0f, blockStart, blockMax, found);
			}

		} else {
			int block = spu_extract(base,0) - blockStart;

			if (block>=0 && block<spu_extract(blockMax, 0)) {
				if (spu_extract(y,0) < 30 && spu_extract(y,1) < 10) {
				found |= 0x80000000>>block;
				processTile.coordArray[block] = spu_extract( (vec_uint4)y, 0);
				processTile.A[block] = A;
				// bottom bit of Adx and Ady may change, but I don't think we need to worry
				// DEBUG_VEC4(Adx);
				// DEBUG_VEC4(Ady);
#ifdef INFO
				printf("[%d] coord %2d,%2d block=%d\n", _SPUID, spu_extract(y,0), spu_extract(y,1), block + blockStart );
#endif
				} else {
					printf("[%d] coord out of range %2d,%2d block=%d\n", _SPUID, spu_extract(y,0), spu_extract(y,1), block + blockStart );
				}
			} else if (block>=0) {
				printf("[%d] coord %2d,%2d (block=%d) *** really %d\n", _SPUID, spu_extract(y,0), spu_extract(y,1), block+blockStart, block );
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
		(vec_uint4) spu_splats( chunkStart ), (vec_uint4) spu_splats( chunkEnd+1 ) );
}


////////////////////////////////////////////////////////////////////////////////////////////////////

