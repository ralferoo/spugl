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
#include <stdio.h>
#include <spu_mfcio.h>

#include "spucontext.h"
#include "../connection.h"
#include "../renderspu/render.h"

Triangle*	_currentTriangle		= NULL;
Context*	_currentTriangleContext		= NULL;
char		_currentTriangleBuffer		[ 127 + TRIANGLE_MAX_SIZE ] __attribute__((aligned(128)));
unsigned int	_currentTriangleOffset;
unsigned int	_currentTriangleBufferExtra;
vec_ushort8 	_currentTriangleRewind;
unsigned int	_currentTriangleCacheEndTriangleEAL; 
unsigned int	_currentTriangleCacheEndTriangleEAH; 
unsigned long long _currentTriangleBufferEA; 

Triangle* getTriangleBuffer(Context* context)
{
	// if we've already allocated a triangle buffer (and we're in the same context)
	if (context == _currentTriangleContext && _currentTriangle)
		return _currentTriangle;

	// trash the default values
	_currentTriangleContext	= context;
	_currentTriangle	= NULL;

	// read the current renderable cache line to ensure there is room for the triangle data
	// in the cache line buffer; we do this by comparing against all 16 cache line blocks
	// to make sure that extending the write pointer wouldn't clobber the data

	unsigned long long cache_ea = context->renderableCacheLine;
	char cachebuffer[128+127];
	RenderableCacheLine* cache = (RenderableCacheLine*) ( ((unsigned int)cachebuffer+127) & ~127 );

	spu_mfcdma64(cache, mfc_ea2h(cache_ea), mfc_ea2l(cache_ea), 128, 0, MFC_GETLLAR_CMD);
	spu_readch(MFC_RdAtomicStat);

	// extendvalid = ( read<=write && test<end ) || ( read>write && test<read )
	// extendvalid = ( read>write && read>test ) || ( read<=write && end>test )
	// simplifies to	extendvalid = selb(end, read, read>write) > test
	// or			extendvalid = selb(end>test, read>test, read>write)
	// rewind = next >= end
	// rewindvalid = read != 0
	// valid = extendvalid && (!rewind || rewindvalid)
	// 	 = extendvalid && (!rewind || !rewindinvalid)
	// 	 = extendvalid && !(rewind && rewindinvalid)
	// invalid = ! (extendvalid && !(rewind && rewindinvalid))
	//         = (!extendvalid || (rewind && rewindinvalid))

	vec_ushort8 v_writeptr		= spu_splats( cache->endTriangle );
	vec_ushort8 v_readptr0		= cache->chunkTriangle[0];
	vec_ushort8 v_readptr1		= cache->chunkTriangle[1];
	vec_ushort8 v_testptr		= spu_add(v_writeptr,   TRIANGLE_MAX_SIZE);
	vec_ushort8 v_nextptr		= spu_add(v_writeptr, 2*TRIANGLE_MAX_SIZE);
	vec_ushort8 v_endptr		= spu_splats( (unsigned short)TRIANGLE_BUFFER_SIZE);

	vec_ushort8 v_zero		= spu_splats( (unsigned short) 0 );
	vec_uchar16 v_merger		= (vec_uchar16) { 1,3,5,7,9,11,13,15,17,19,21,23,25,27,29,31 };

	vec_ushort8 v_max0_test		= spu_sel( v_endptr, v_readptr0, spu_cmpgt( v_readptr0, v_writeptr ) );
	vec_ushort8 v_max1_test		= spu_sel( v_endptr, v_readptr1, spu_cmpgt( v_readptr1, v_writeptr ) );
	vec_ushort8 v_extend0_valid	= spu_cmpgt( v_max0_test, v_testptr );
	vec_ushort8 v_extend1_valid	= spu_cmpgt( v_max1_test, v_testptr );
	vec_ushort8 v_rewind0_invalid	= spu_cmpeq( v_readptr0, v_zero );
	vec_ushort8 v_rewind1_invalid	= spu_cmpeq( v_readptr1, v_zero );
	vec_ushort8 v_rewind8		= spu_cmpgt( v_nextptr, v_endptr );

	vec_uchar16 v_extend_valid	= (vec_uchar16) spu_shuffle( v_extend0_valid, v_extend1_valid, v_merger );
	vec_uchar16 v_rewind_invalid	= (vec_uchar16) spu_shuffle( v_rewind0_invalid, v_rewind1_invalid, v_merger );
	vec_uchar16 v_rewind		= (vec_uchar16) v_rewind8;

	vec_uchar16 v_valid_rhs		= spu_and( v_rewind_invalid, v_rewind );
	vec_uchar16 v_invalid		= spu_orc( v_valid_rhs, v_extend_valid );

	// check to see if the chunk is being processed
	vec_uint4 v_free = spu_gather(
		spu_cmpeq( spu_splats( (unsigned char) CHUNKNEXT_FREE_BLOCK ), cache->chunkNext ) );
	vec_uint4   v_invalid_bits	= spu_andc( spu_gather( v_invalid ), (vec_uint4) v_free );

/*
        DEBUG_VEC8( v_writeptr );
        DEBUG_VEC8( v_readptr0 );
        DEBUG_VEC8( v_readptr1 );
        DEBUG_VEC8( v_testptr );
        DEBUG_VEC8( v_nextptr );
        DEBUG_VEC8( v_endptr );
        DEBUG_VEC8( v_max0_test );
        DEBUG_VEC8( v_max1_test );
        DEBUG_VEC8( v_extend0_valid );
        DEBUG_VEC8( v_extend1_valid );
        DEBUG_VEC8( v_rewind0_invalid );
        DEBUG_VEC8( v_rewind1_invalid );
        DEBUG_VEC8( v_extend_valid );
        DEBUG_VEC8( v_rewind_invalid );
        DEBUG_VEC8( v_rewind );
        DEBUG_VEC8( v_valid_rhs );
        DEBUG_VEC8( v_invalid );
        DEBUG_VEC8( v_free );
        DEBUG_VEC8( v_invalid_bits );

      printf("\n");
*/

	// if any of the bits are invalid, then no can do
	if ( spu_extract(v_invalid_bits, 0) ) {
		return NULL;
	}

	// fetch in the data before this triangle in the cache buffer
	unsigned int offset = cache->endTriangle;
	_currentTriangleBufferExtra = offset & 127;
	unsigned long long trianglebuffer_ea = cache_ea + TRIANGLE_OFFSET_FROM_CACHE_LINE + (offset & ~127);
	if (_currentTriangleBufferExtra) {
		spu_mfcdma64(_currentTriangleBuffer, mfc_ea2h(trianglebuffer_ea), mfc_ea2l(trianglebuffer_ea), 128, 0, MFC_GET_CMD);

		// ensure DMA did actually complete
		mfc_write_tag_mask(1<<0);
		mfc_read_tag_status_all();
	}

	// final bit of initialisation
	_currentTriangle = (Triangle*) (_currentTriangleBuffer+_currentTriangleBufferExtra);
	_currentTriangleOffset = offset;
	_currentTriangleRewind = v_rewind8;
	_currentTriangleCacheEndTriangleEAL = mfc_ea2l(cache_ea) + (((char*)&cache->endTriangle) - ((char*)cache));
	_currentTriangleCacheEndTriangleEAH = mfc_ea2h(cache_ea); 
	_currentTriangleBufferEA = trianglebuffer_ea; 

	// and return the buffer ready to go
	return _currentTriangle;
}

void writeTriangleBuffer(Triangle* endTriangle)
{
	if (endTriangle != _currentTriangle) {
		int length = ( ((char*)endTriangle) - _currentTriangleBuffer + 127) & ~127;
		unsigned short endTriangleBase = (((char*)endTriangle) - ((char*)_currentTriangle)) + _currentTriangleOffset;
		vec_ushort8 v_new_end = spu_promote(endTriangleBase, 1);

		// calculate genuine next pointer ( rewind==0 -> next, rewind!=0 -> 0 )
		unsigned short next_pointer = spu_extract( spu_andc( v_new_end, _currentTriangleRewind ), 1 );
		_currentTriangle->next_triangle = next_pointer;
		
//		printf("current=0x%x, endTriBase=0x%x, next_pointer=0x%x\n", _currentTriangleOffset, endTriangleBase, next_pointer);

		// DMA the triangle data out
		spu_mfcdma64(_currentTriangleBuffer, mfc_ea2h(_currentTriangleBufferEA), mfc_ea2l(_currentTriangleBufferEA), length, 0, MFC_PUT_CMD);

		// update the information in the cache line
		_currentTriangleRewind = spu_splats(next_pointer);		// re-use this variable as we don't need it anymore
		char* dstart = ((char*)&_currentTriangleRewind) + (_currentTriangleCacheEndTriangleEAL & 15);
		spu_mfcdma64(dstart, _currentTriangleCacheEndTriangleEAH, _currentTriangleCacheEndTriangleEAL, sizeof(short), 0, MFC_PUTB_CMD);

//		printf("writing from %x to %x:%x\n", dstart, _currentTriangleCacheEndTriangleEAH, _currentTriangleCacheEndTriangleEAL);

		// finally invalidate the triangle info
		_currentTriangle = NULL;

		// and make sure the DMA completed
		mfc_write_tag_mask(1<<0);
		mfc_read_tag_status_all();
	}
}


