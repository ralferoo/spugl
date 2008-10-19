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

#define DEBUG_VEC4(x) __debug_vec4(#x, (vec_uint4) x)
#define DEBUG_VEC8(x) __debug_vec8(#x, (vec_ushort8) x)
#define DEBUG_VECf(x) __debug_vecf(#x, (vec_float4) x)

void __debug_vec4(char* s, vec_uint4 x)
{
	printf("%-20s %08x   %08x   %08x   %08x\n", s,
		spu_extract(x, 0),
		spu_extract(x, 1),
		spu_extract(x, 2),
		spu_extract(x, 3) );
}

void __debug_vec8(char* s, vec_ushort8 x)
{
	printf("%-20s %04x %04x %04x %04x %04x %04x %04x %04x\n", s,
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
	printf("%-20s %11.3f %11.3f %11.3f %11.3f\n", s,
		spu_extract(x, 0),
		spu_extract(x, 1),
		spu_extract(x, 2),
		spu_extract(x, 3) );
}


//#define CHECK_STATE_TABLE

extern float4 current_colour;
extern float4 current_texcoord;
extern unsigned int current_texture;

// extern TextureDefinition* currentTexture;

static void imp_point()
{
}

static void imp_line()
{
}

static const vec_uchar16 shuffle_tri_right_padded = {
	SEL_00 SEL_A0 SEL_A1 SEL_A2
};
static const vec_uchar16 shuffle_tri_normal = {
	SEL_A0 SEL_A1 SEL_A2 SEL_00
};
static const vec_uchar16 shuffle_tri_cw = {
	SEL_A1 SEL_A2 SEL_A0 SEL_00
};
static const vec_uchar16 shuffle_tri_ccw = {
	SEL_A2 SEL_A0 SEL_A1 SEL_00
};
static const vec_uchar16 shuffle_1st_word_pad = {
	SEL_A0 SEL_00 SEL_00 SEL_00
};
static const vec_uchar16 shuffle_2nd_word_pad = {
	SEL_A1 SEL_00 SEL_00 SEL_00
};
static const vec_uchar16 shuffle_3rd_word_pad = {
	SEL_A2 SEL_00 SEL_00 SEL_00
};

static const vec_uchar16 minimax_x = {
0xff,0xff, 0x00,0x08, 0x08,0x04, 0x00,0x04, 0x04,0x00, 0x04,0x08, 0x08,0x00, 0xff,0xff,
};
static const vec_uchar16 minimax_y = {
0xff,0xff, 0x10,0x18, 0x18,0x14, 0x10,0x14, 0x14,0x10, 0x14,0x18, 0x18,0x10, 0xff,0xff,
};
static const vec_uchar16 minimax_merge = {
0,0,0,0, 16,16,16,16, 1,1,1,1, 17,17,17,17};
static const vec_uchar16 minimax_add = {
0,1,2,3, 0,1,2,3, 0,1,2,3, 0,1,2,3};

//int last_triangle = -1;

//extern RenderFuncs _standard_texture_triangle;
//extern RenderFuncs _standard_simple_texture_triangle;
//extern RenderFuncs _standard_colour_triangle;

////////////////////////////////////////////////////////////////////////////////////////////////////

unsigned int DEBUG_SUBDIV(vec_int4 A, vec_int4 Adx, vec_int4 Ady, vec_short8 y, vec_short8 i,
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
				found  = DEBUG_SUBDIV(startA, dxA, dyA, spu_add(y,addA), i, spu_splats(spu_extract(newbase,0)), baseadd, type^0xf0, blockMax, found);
			}
			if (spu_extract(v_process, 1)) {
				found = DEBUG_SUBDIV(startB, dxB, dyB, spu_add(y,addB), i, spu_splats(spu_extract(newbase,1)), baseadd, type, blockMax, found);
			}
			if (spu_extract(v_process, 2)) {
				found = DEBUG_SUBDIV(startC, dxC, dyC, spu_add(y,addC), i, spu_splats(spu_extract(newbase,2)), baseadd, type, blockMax, found);
			}
			if (spu_extract(v_process, 3)) {
				found = DEBUG_SUBDIV(startD, dxD, dyD, spu_add(y,addD), i, spu_splats(spu_extract(newbase,3)), baseadd, type^0x0f, blockMax, found);
			}

		} else {
			int block = spu_extract(base,0);
			found++;
			printf("[%d] DEBUG coord %2d,%2d (block=%d) (#%d)\n", _SPUID, spu_extract(y,0), spu_extract(y,1), block, found );
		}
	}
	return found;
}

//////////////////////////////////////////////////////////////////////////////

int DEBUG_TRI(Triangle* triangle, int chunkStart, int chunkEnd)
{

	int w = 64;
	vec_int4 INITIAL_BASE = spu_splats(0);
	vec_int4 INITIAL_BASE_ADD = { w*w, 2*w*w, 3*w*w, 4*w*w };
	vec_short8 ZEROS = spu_splats((short)0);
	vec_short8 INITIAL_i = spu_splats((short)w);
	
	vec_int4 A   = triangle->area;
	vec_int4 Adx = triangle->area_dx;
	vec_int4 Ady = triangle->area_dy;
	
	unsigned int blocksToProcess = DEBUG_SUBDIV(A, Adx, Ady,
		ZEROS, INITIAL_i, INITIAL_BASE, INITIAL_BASE_ADD, 0, spu_splats(4096), 0); 
}


////////////////////////////////////////////////////////////////////////////////////////////////////


static const vec_float4 muls4 = {4.0f, 4.0f, 4.0f, 4.0f};
static const vec_float4 muls32 = {32.0f, 32.0f, 32.0f, 32.0f};
static const vec_float4 mulsn28 = {-28.0f, -28.0f, -28.0f, -28.0f};

static const vec_float4 muls31x = {0.0f, 31.0f, 0.0f, 31.0f};
static const vec_float4 muls31y = {0.0f, 0.0f, 31.0f, 31.0f};

//////////////////////////////////////////////////////////////////////////////

/*
extern void* linearColourFill(void* self, Block* block, ActiveBlock* active, int tag);
extern void* textureMapFill(void* self, Block* block, ActiveBlock* active, int tag);
extern void* linearTextureMapFill(void* self, Block* block, ActiveBlock* active, int tag);
extern void* fastTextureMapFill(void* self, Block* block, ActiveBlock* active, int tag);
extern void* lessMulsLinearTextureMapFill(void* self, Block* block, ActiveBlock* active, int tag);

//////////////////////////////////////////////////////////////////////////////

*/

#define FIXED_PREC 2

Triangle* imp_triangle(Triangle* triangle, Context* context)
{
	// rework: the new hope... :)
	//
	// note, this is done assuming input coordinates +-4095 in both x and y axes
	// and for the total_dx, total_dy to be based on 2048 subdivisions

	// it might seem odd doing the convts and then a shift right, but the reason is
	// to get the "free" clamping done by convts. format: S_1_12.2_16 (i.e. sign,
	// 1 "spare" bit, 12 before point, 2 after point, 16 junk
	vec_int4 x = spu_add(spu_rlmaska(spu_convts(TRIx, 31-12), -1), 1<<(31-12-1-1));
	vec_int4 y = spu_add(spu_rlmaska(spu_convts(TRIy, 31-12), -1), 1<<(31-12-1-1));

	x = spu_andc(x, spu_splats(0xffff) );
	y = spu_andc(y, spu_splats(0xffff) );

	// calculate the gradients, format: S_13.2_16
	vec_int4 dy = spu_sub(	spu_shuffle(x,x, shuffle_tri_cw),
				spu_shuffle(x,x, shuffle_tri_ccw) ); // x: cw-ccw

	vec_int4 dx = spu_sub(	spu_shuffle(y,y, shuffle_tri_ccw),
				spu_shuffle(y,y, shuffle_tri_cw) ); // y: ccw-cw

	// calculate the total area of the triangle, format: 3 * S_2_25.4 -> S_27.4
	vec_int4 total_v = spu_mule( (vec_short8)x, (vec_short8)dx );

	vec_int4 total = spu_add( spu_add(
					spu_shuffle(total_v,total_v,shuffle_1st_word_pad),
					spu_shuffle(total_v,total_v,shuffle_2nd_word_pad)),
					spu_shuffle(total_v,total_v,shuffle_3rd_word_pad));

	// calculate initial offset, format: 2 * S_2_25.4 -> S_1_26.4
	vec_int4 offset = spu_add(
				spu_mule((vec_short8)spu_splats(spu_extract(x,0)), (vec_short8)dx),
				spu_mule((vec_short8)spu_splats(spu_extract(y,0)), (vec_short8)dy));

	// calculate base, format: 5 * S_2_25.4 -> S_28.4 (ERROR!)
	// TODO: There is a potential overflow here, should really (optionally check for it and )
	// TODO: shift total, offset, dx and dy by 1 to handle this case
	vec_int4 triA = spu_sub(total, offset);

	// calculate deltas, format: S_3_24.4
	vec_int4 triAdx = spu_rlmaska(dx, -16+2+11); // -16 remove crap, +2=match precision
	vec_int4 triAdy = spu_rlmaska(dy, -16+2+11); // +11=2048 subdiv

	// calculate whether we do in fact have a visible triangle
	vec_uint4 valid = (vec_uint4) spu_rlmaska(total, -31);
	if (!spu_extract(valid,0))
		return triangle;

	// remove requirement to do this in the hilbert calcs
	vec_int4 Amask = {0, 0, 0, -1};
	triangle->area = spu_or(triA,Amask);
	triangle->area_dx = triAdx;
	triangle->area_dy = triAdy;

/*
	DEBUG_VEC4( triA );
	DEBUG_VEC4( triAdx );
	DEBUG_VEC4( triAdy );
*/

//	DEBUG_TRI( triangle, 0, 4096 );

	vec_float4 *params = (vec_float4*) triangle->shader_params;
	params[ 0] = TRIx;
	params[ 1] = TRIy;
	params[ 2] = TRIz;
	params[ 3] = TRIw;

	params[ 4] = TRIr;
	params[ 5] = TRIg;
	params[ 6] = TRIb;
	params[ 7] = TRIa;

	params[ 8] = TRIs;
	params[ 9] = TRIt;
	params[10] = TRIu;
	params[11] = TRIv;

	return (Triangle*) (&params[12]);

// TODO: MUST PUT BACK TEXTURE STUFF

//	triangle->tex_id_base = currentTexture->tex_id_base;

//	triangle->init_block = &linearColourFill;


//	static int a=0;
//	if (a++&2048)
//		triangle->init_block = &textureMapFill;
//	else
//
//	triangle->init_block = &linearTextureMapFill;

//	triangle->init_block = &lessMulsLinearTextureMapFill;

//	triangle->init_block = &fastTextureMapFill;

// if the triangle is visible (i.e. area>0), then we increment the triangle
// out ptr to just past the triangle data we've just written to memory.
// if the triangle is invisible (i.e. area<0) then leave the pointer in the
// same place so that we re-use the triangle slot on the next triangle.

//	unsigned long triangle_is_visible_mask = spu_extract(fcgt_area, 0);
//	triangle->count = 1 & triangle_is_visible_mask;
//	triangle->produce = (void*)( ((unsigned int)&triangleProducer) & triangle_is_visible_mask );

//	printf("[%d] %f, %f -> %f\n", triangle->count, face_sum, tex_cover, tex_cover/face_sum);
}

//////////////////////////////////////////////////////////////////////////////

#define ADD_LINE	1
#define ADD_TRIANGLE	2
#define ADD_POINT	3
#define ADD_TRIANGLE2	4

int current_state = -1;
const static struct {
	unsigned char next;
	unsigned char insert;
	unsigned char end;
	unsigned char add;
} shuffle_map[] = {
/*0 GL_POINTS          */
	{ .insert = 0, .next = 0, .add = ADD_POINT },
/*1 GL_LINES           */
	{ .insert = 0, .next = 10 },
/*2 GL_LINE_LOOP       */
	{ .insert = 0, .next = 11 },
/*3 GL_LINE_STRIP      */
	{ .insert = 0, .next = 12 },
/*4 GL_TRIANGLES       */
	{ .insert = 0, .next = 13 },
/*5 GL_TRIANGLE_STRIP  */
	{ .insert = 0, .next = 14 },
/*6 GL_TRIANGLE_FAN    */
	{ .insert = 0, .next = 18 },
/*7 GL_QUADS           */
	{ .insert = 0, .next = 21 },
/*8 GL_QUAD_STRIP      */
	{ .insert = 0, .next = 24 },
/*9 GL_POLYGON         */
	{ .insert = 0, .next = 27 },

/* 10 line second point */
	{ .insert = 1, .next = 1, .add = ADD_LINE },
/* 11 line loop second point */
	{ .insert = 1, .next = 11, .add = ADD_LINE, .end = 2},
/* 12 line strip second point */
	{ .insert = 1, .next = 12, .add = ADD_LINE },

/* 13 triangle second point */
	{ .insert = 3, .next = 14 },
/* 14 triangle third point */
	{ .insert = 3, .next = 4, .add = ADD_TRIANGLE },

/* 15 triangle strip second point */
	{ .insert = 3, .next = 16 },
/* 16 triangle strip third point */
	{ .insert = 3, .next = 17, .add = ADD_TRIANGLE },
/* 17 triangle strip fourth point */
	{ .insert = 4, .next = 16, .add = ADD_TRIANGLE },

/* 18 triangle fan second point */
	{ .insert = 3, .next = 19 },
/* 19 triangle fan third point */
	{ .insert = 3, .next = 20, .add = ADD_TRIANGLE },
/* 20 triangle fan fourth point */
	{ .insert = 5, .next = 20, .add = ADD_TRIANGLE },

/* 21 quad second point */
	{ .insert = 6, .next = 22 },
/* 22 quad third point */
	{ .insert = 6, .next = 23 },
/* 23 quad fourth point */
	{ .insert = 6, .next = 30, .add = ADD_TRIANGLE2 },

/* 24 quad strip second point */
	{ .insert = 6, .next = 22 },
/* 25 quad strip third point */
	{ .insert = 6, .next = 25 },
/* 26 quad strip fourth point */
	{ .insert = 6, .next = 31, .add = ADD_TRIANGLE2 },

/* 27 polygon second point */
	{ .insert = 3, .next = 28 },
/* 28 polygon third point */
	{ .insert = 3, .next = 4, .add = ADD_TRIANGLE },
/* 29 polygon fourth point */
	{ .insert = 7, .next = 29, .add = ADD_TRIANGLE, .end = 8 },

/* 30 quad fake point, 2nd triangle */
	{ .insert = 10, .next = 7, .add = ADD_TRIANGLE },

/* 31 quad strip fake point */
	{ .insert = 9, .next = 25, .add = ADD_TRIANGLE },
};

/* for quad_strips, the first triangle seems to be backwards. of course
 * it could just be my test data...
 *
 * there's also the case that quads aren't needed by GLES so i should
 * probably just get rid of them entirely and only use this function for
 * tris, tristrips and trifans.
 */

/* A3 is preserved as the initial state if we need to loop */
const static vec_uchar16 shuffles[] = {
{ SEL_B0 SEL_B0 SEL_B0 SEL_B0 }, /* 0 = fill all elements with new */
{ SEL_A1 SEL_B0 SEL_B0 SEL_A3 }, /* 1 = add line vertex */
{ SEL_A1 SEL_A3 SEL_00 SEL_00 }, /* 2 = END line loop finished */
{ SEL_A1 SEL_A2 SEL_B0 SEL_A3 }, /* 3 = add triangle vertex */
{ SEL_A2 SEL_A1 SEL_B0 SEL_A3 }, /* 4 = add triangle strip vertex 4th */
{ SEL_A0 SEL_A2 SEL_B0 SEL_A3 }, /* 5 = add triangle fan vertex 4th */
{ SEL_A1 SEL_A2 SEL_A3 SEL_B0 }, /* 6 = add quad vertex */
{ SEL_A0 SEL_A2 SEL_B0 SEL_A3 }, /* 7 = add polygon vertex */
{ SEL_A0 SEL_A1 SEL_A3 SEL_00 }, /* 8 = END polygon vertex finished */
{ SEL_A2 SEL_A1 SEL_A3 SEL_A0 }, /* 9 = quad strip fake, do 1st tri second */
{ SEL_A2 SEL_A3 SEL_A0 SEL_A1 }, /* 10 = quad fake, do 1st tri second */
{ SEL_A2 SEL_A1 SEL_B0 SEL_A0 }, /* 11 = quad strip fourth, do 2nd tri first */
};

static inline void shuffle_in(vec_uchar16 inserter, float4 s, float4 col, float4 tex) {
	TRIx = spu_shuffle(TRIx, (vec_float4) spu_promote(s.x,0), inserter);
	TRIy = spu_shuffle(TRIy, (vec_float4) spu_promote(s.y,0), inserter);
	TRIz = spu_shuffle(TRIz, (vec_float4) spu_promote(s.z,0), inserter);
	TRIw = spu_shuffle(TRIw, (vec_float4) spu_promote(s.w,0), inserter);
	TRIr = spu_shuffle(TRIr, (vec_float4) spu_promote(col.x,0), inserter);
	TRIg = spu_shuffle(TRIg, (vec_float4) spu_promote(col.y,0), inserter);
	TRIb = spu_shuffle(TRIb, (vec_float4) spu_promote(col.z,0), inserter);
	TRIa = spu_shuffle(TRIa, (vec_float4) spu_promote(col.w,0), inserter);
	TRIs = spu_shuffle(TRIs, (vec_float4) spu_promote(tex.x,0), inserter);
	TRIt = spu_shuffle(TRIt, (vec_float4) spu_promote(tex.y,0), inserter);
	TRIu = spu_shuffle(TRIu, (vec_float4) spu_promote(tex.z,0), inserter);
	TRIv = spu_shuffle(TRIv, (vec_float4) spu_promote(tex.w,0), inserter);
}

int imp_vertex(float4 in, Context* context)
{
#ifdef CHECK_STATE_TABLE
	if (current_state < 0 ||
		  current_state >= sizeof(shuffle_map)/sizeof(shuffle_map[0])) {
		raise_error(ERROR_VERTEX_INVALID_STATE);
		return 0;
		}
#endif
	int ins = shuffle_map[current_state].insert;
#ifdef CHECK_STATE_TABLE
	if (ins >= sizeof(shuffles)/sizeof(shuffles[0])) {
		raise_error(ERROR_VERTEX_INVALID_SHUFFLE);
		return 0;
	}
#endif

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

//	printf("\n");
*/

	// if any of the bits are invalid, then no can do
	if ( spu_extract(v_invalid_bits, 0) ) {
//		printf("BUFFER FULL!!!\n\n");
		return 1;
	}

	// this will happen on every vertex... so obviously quite slow; will need to move out!
	char trianglebuffer[ 256 + TRIANGLE_MAX_SIZE ];
	unsigned int offset = cache->endTriangle;
	unsigned int extra = offset & 127;
	unsigned long long trianglebuffer_ea = cache_ea + TRIANGLE_OFFSET_FROM_CACHE_LINE + (offset & ~127);
	Triangle* triangle = (Triangle*) (trianglebuffer+extra);
	if (extra) {
		spu_mfcdma64(trianglebuffer, mfc_ea2h(trianglebuffer_ea), mfc_ea2l(trianglebuffer_ea), 128, 0, MFC_GET_CMD);
	}

	vec_uchar16 inserter = shuffles[ins];

	// just for testing, have hard-coded persective and screen
	// transformations here. they'll probably live here anyway, just
	// done with matrices.

	vec_float4 vin = {in.x, in.y, in.z, in.w }; // this should be parameter format!

	vec_float4 matres = spu_madd(spu_splats(in.x), context->modelview_matrix_x,
			    spu_madd(spu_splats(in.y), context->modelview_matrix_y,
			    spu_madd(spu_splats(in.z), context->modelview_matrix_z,
			    spu_mul (spu_splats(in.w), context->modelview_matrix_w))));

	float recip = 1.0f/spu_extract(matres,3);
	vec_float4 vrecip = spu_splats(recip);
	vec_float4 vresdiv = spu_mul(matres, vrecip);
	float4 sa = {.x = spu_extract(matres,0), .y = spu_extract(matres,1),
		    .z = spu_extract(matres,2), .w = spu_extract(matres,3)};
	float4 s = {.x = spu_extract(vresdiv,0), .y = spu_extract(vresdiv,1),
		    .z = spu_extract(vresdiv,2), .w = recip};

/*
	float recip_old = 420.0f / (in.z-282.0f);
	float4 s_old = {.x=in.x*recip_old+screen.width/2, .y = in.y*recip_old+screen.height/2, .z = in.z*recip_old, .w = recip_old};

	printf(" in: %8.4f %8.4f %8.4f %8.4f\n", in.x, in.y, in.z, in.w);
	printf("out: %8.4f %8.4f %8.4f %8.4f\n", sa.x, sa.y, sa.z, sa.w);
	printf("new: %8.4f %8.4f %8.4f %8.4f (%8.4f)\n", s.x, s.y, s.z, s.w, spu_extract(matres,3));
	printf("old: %8.4f %8.4f %8.4f %8.4f\n\n", s_old.x, s_old.y, s_old.z, s_old.w);
*/

/*
	printf(" in: %8.4f %8.4f %8.4f %8.4f\n", in.x, in.y, in.z, in.w);
	printf("out: %8.4f %8.4f %8.4f %8.4f\n", sa.x, sa.y, sa.z, sa.w);
*/
	float4 c= current_colour;
	float4 col = {.x=c.x*recip, .y = c.y*recip, .z = c.z*recip, .w = c.w*recip};
	float4 t = current_texcoord;
	float4 tex = {.x=t.x*recip, .y = t.y*recip, .z = t.z*recip, .w = t.w*recip};

//	printf("tran (%.2f,%.2f,%.2f,%.2f)\n", s.x, s.y, s.z, s.w);

//	float4 p = {.x=in.x - screen.width/2, .y = in.y - screen.height/2, .z = in.z, .w = 420/(in.z-420)};
//	float recip = 1.0/p.w;
//	float4 s = {.x=p.x*recip, .y = p.y*recip, .z = p.z*recip, .w = recip};

	shuffle_in(inserter, s, col, tex);

	// check to see if we need to draw
	switch (shuffle_map[current_state].add) {
		case ADD_LINE:
			imp_line();
			break;
		case ADD_TRIANGLE2:
			// TODO: fix code for quad if it's worthwhile, will need to do buffer stuff twice...
			// imp_triangle(context);

			current_state = shuffle_map[current_state].next;
			ins = shuffle_map[current_state].insert;
#ifdef CHECK_STATE_TABLE
			if (ins >= sizeof(shuffles)/sizeof(shuffles[0])) {
				raise_error(ERROR_VERTEX_INVALID_SHUFFLE);
				return from;
			}
#endif
			inserter = shuffles[ins];
			shuffle_in(inserter, s, col, tex);

			// fall through here
		case ADD_TRIANGLE:

			// ensure DMA did actually complete
			mfc_write_tag_mask(1<<0);
			mfc_read_tag_status_all();

			Triangle* endTriangle = imp_triangle(triangle, context);

			if (endTriangle != triangle) {
				int length = ( ((char*)endTriangle) - trianglebuffer + 127) & ~127;
				unsigned short endTriangleBase = (((char*)endTriangle) - ((char*)triangle)) + offset;
				vec_ushort8 v_new_end = spu_promote(endTriangleBase, 1);

				// calculate genuine next pointer ( rewind==0 -> next, rewind!=0 -> 0 )
				unsigned short next_pointer = spu_extract( spu_andc( v_new_end, v_rewind8 ), 1 );
				triangle->next_triangle = next_pointer;
/*
				printf("len %x, endTriBase %x, next_pointer %x, ea %x:%08x len %x\n",
					length, endTriangleBase, next_pointer,
					mfc_ea2h(trianglebuffer_ea), mfc_ea2l(trianglebuffer_ea), length );
*/
/*
	printf("\ntri=%04x\n", offset);
	DEBUG_VECf( TRIx );
	DEBUG_VECf( TRIy );
	DEBUG_VECf( TRIz );
*/
				// DMA the triangle data out
				spu_mfcdma64(trianglebuffer, mfc_ea2h(trianglebuffer_ea), mfc_ea2l(trianglebuffer_ea), length, 0, MFC_PUT_CMD);

//				for (int i=0; i<1000000; i++) __asm("nop");

//				mfc_sync(0);
				//mfc_eieio(0,0,0);

				// update the information in the cache line
				cache->endTriangle = next_pointer;
				//static short updatedEndTriangle;
				//updatedEndTriangle = next_pointer;
				unsigned int eal = mfc_ea2l(cache_ea) + (((char*)&cache->endTriangle) - ((char*)cache));
//				printf("%08x %08x\n", mfc_ea2l(cache_ea), eal);
				spu_mfcdma64(&cache->endTriangle, mfc_ea2h(cache_ea), eal, sizeof(short), 0, MFC_PUTB_CMD);

				mfc_write_tag_mask(1<<0);
				mfc_read_tag_status_all();
/*

				// update the information in the cache line
				for(;;) {
					cache->endTriangle = next_pointer;
					spu_mfcdma64(cache, mfc_ea2h(cache_ea), mfc_ea2l(cache_ea), 128, 0, MFC_PUTLLC_CMD);
					unsigned int status = spu_readch(MFC_RdAtomicStat) & MFC_PUTLLC_STATUS;
					if (!status)
						break;

					// cache is dirty and write failed, reload it and attempt the whole thing again again
					spu_mfcdma64(cache, mfc_ea2h(cache_ea), mfc_ea2l(cache_ea), 128, 0, MFC_GETLLAR_CMD);
					spu_readch(MFC_RdAtomicStat);
				}
*/
			}
//			printf("done triangle\n");

			break;
	}
	current_state = shuffle_map[current_state].next;

	// now write all the changed data

	return 0;
}

int imp_validate_state(int state)
{
	return state >= 0 &&
		  state < sizeof(shuffle_map)/sizeof(shuffle_map[0]) &&
		  shuffle_map[state].insert == 0;
}

void imp_close_segment(Context* context)
{
	int end = shuffle_map[current_state].end;
	if (end) {
		float4 x;
		shuffle_in(shuffles[end], x, x, x);
		switch (shuffle_map[current_state].add) {
			case ADD_LINE:
				imp_line();
				break;
			case ADD_TRIANGLE:
				// TODO: implement!
				// imp_triangle(context);
				break;
		}
	}
}

// returns 0 if finished processing queue, non-zero if still processing
int stillProcessingQueue(Context* context)
{
	// read the current renderable cache line

	unsigned long long cache_ea = context->renderableCacheLine;
	char cachebuffer[128+127];
	RenderableCacheLine* cache = (RenderableCacheLine*) ( ((unsigned int)cachebuffer+127) & ~127 );

	spu_mfcdma64(cache, mfc_ea2h(cache_ea), mfc_ea2l(cache_ea), 128, 0, MFC_GETLLAR_CMD);
	spu_readch(MFC_RdAtomicStat);

	// check whether any active cache line is still running
	
	vec_ushort8 v_writeptr		= spu_splats( cache->endTriangle );
	vec_ushort8 v_readptr0		= cache->chunkTriangle[0];
	vec_ushort8 v_readptr1		= cache->chunkTriangle[1];

	vec_ushort8 v_finished0		= spu_cmpeq( v_readptr0, v_writeptr );
	vec_ushort8 v_finished1		= spu_cmpeq( v_readptr1, v_writeptr );

	vec_uchar16 v_merger		= (vec_uchar16) { 1,3,5,7,9,11,13,15,17,19,21,23,25,27,29,31 };
	vec_uchar16 v_finished		= (vec_uchar16) spu_shuffle( v_finished0, v_finished1, v_merger );
	// each byte = 0xFF if finished, 0x00 if still waiting

	vec_uchar16 v_next		= cache->chunkNext;
	vec_uchar16 v_free_blocks	= spu_cmpeq( spu_splats( (unsigned char) CHUNKNEXT_FREE_BLOCK ), v_next );
	// each byte = 0xFF if free, 0x00 if busy or in a list

	vec_uchar16 v_busy_mask		= spu_splats( (unsigned char) CHUNKNEXT_MASK );
	vec_uchar16 v_busy_blocks	= spu_cmpeq( v_busy_mask, spu_and( v_busy_mask, v_next) );
	// each byte = 0xFF if busy, 0x00 if not busy

	// if free 	  == 1 -> not dirty
	// if busy	  == 1 -> dirty
	// if finished	  == 0 -> dirty

	//  finished	free	busy	dirty
	//	x	 1	  x	   0
	//	x	 0	  1	   1
	//	1	 0	  0	   0
	//	0	 0	  0	   1
	//
	//	dirty = ~free & ( busy | ~finished )
	//
	//	WRONG - dirty = ~finished | ( ~free & busy )

	//vec_uchar16 v_include		= spu_andc( v_busy_blocks, v_free_blocks );
	//vec_uchar16 v_dirty		= spu_orc( v_include, v_finished );

	vec_uchar16 v_dirty		= spu_andc( spu_orc( v_busy_blocks, v_finished ), v_free_blocks );

	// and return the dirty flag
	unsigned int dirty		= spu_extract( spu_gather( v_dirty ), 0 );

	return (int) dirty;
}

int imp_clear_screen(float4 in, Context* context)
{
	return 0;
}

