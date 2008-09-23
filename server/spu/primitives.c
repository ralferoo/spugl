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

//////////////////////////////////////////////////////////////////////////////

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
	vec_int4 x = spu_rlmaska(spu_convts(TRIx, 31-12), -1);
	vec_int4 y = spu_rlmaska(spu_convts(TRIy, 31-12), -1);

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

/*
	DEBUG_VEC4(total);
	DEBUG_VEC4(triA);
	DEBUG_VEC4(triAdx);
	DEBUG_VEC4(triAdy);
*/

	triangle->area = triA;
	triangle->area_dx = triAdx;
	triangle->area_dy = triAdy;

//////
//
	vec_float4 t_vx_cw = spu_shuffle(TRIx, TRIx, shuffle_tri_cw);
	vec_uint4 fcgt_x = spu_cmpgt(TRIx, t_vx_cw);	// all-ones if ax>bx, bx>cx, cx>ax, ???
	unsigned int fcgt_bits_x = spu_extract(spu_gather(fcgt_x), 0) & ~1;
	vec_uchar16 shuf_x = spu_slqwbyte(minimax_x, fcgt_bits_x);

	vec_float4 t_vy_cw = spu_shuffle(TRIy, TRIy, shuffle_tri_cw);
	vec_uint4 fcgt_y = spu_cmpgt(TRIy, t_vy_cw);	// all-ones if ay>by, by>cy, cy>ay, ???
	unsigned int fcgt_bits_y = spu_extract(spu_gather(fcgt_y), 0) & ~1;
	vec_uchar16 shuf_y = spu_slqwbyte(minimax_y, fcgt_bits_y);

	vec_uchar16 shuf_minmax_base = spu_shuffle(shuf_x, shuf_y, minimax_merge);
	vec_uchar16 shuf_minmax = spu_or(shuf_minmax_base, minimax_add);
	vec_float4 minmax_ = spu_shuffle(TRIx, TRIy, shuf_minmax);

	vec_float4 v_x_cw = spu_shuffle(TRIx, TRIx, shuffle_tri_cw);
	vec_float4 v_x_ccw = spu_shuffle(TRIx, TRIx, shuffle_tri_ccw);

	vec_float4 v_y_cw = spu_shuffle(TRIy, TRIy, shuffle_tri_cw);
	vec_float4 v_y_ccw = spu_shuffle(TRIy, TRIy, shuffle_tri_ccw);

	vec_float4 area_dx = spu_sub(v_y_ccw, v_y_cw); // cy -> by
	vec_float4 area_dy = spu_sub(v_x_cw, v_x_ccw); // bx -> cx

	// vec_float4 v_by_to_cy = spu_sub(v_y_ccw, v_y_cw);

	vec_float4 face_mul = spu_mul(TRIx, area_dx /*v_by_to_cy*/ );
	float face_sum = spu_extract(face_mul, 0) +
			 spu_extract(face_mul, 1) +
			 spu_extract(face_mul, 2);

	vec_float4 cast_zero = (vec_float4) {0.0f, 0.0f, 0.0f, 0.0f};
	vec_float4 base_area = spu_insert(face_sum, cast_zero, 0);
	vec_uint4 fcgt_area = spu_cmpgt(cast_zero, base_area);

	vec_float4 area_ofs = spu_madd(spu_splats(spu_extract(TRIx,0)),area_dx,
			spu_mul(spu_splats(spu_extract(TRIy,0)),area_dy));

	vec_int4   ia_dx = spu_convts(area_dx, 2);
	vec_int4   ia_dy = spu_convts(area_dy, 2);
	vec_int4   ia_ofs = spu_convts(area_ofs, 2);

//	printf("TRIx %8.4f area_dx %08.4f TRIy %8.4f area_dy %8.4f\n",
//			spu_extract(TRIx,0),area_dx,
//			spu_extract(TRIy,0),area_dy);

	vec_float4 start_A = spu_sub(base_area, area_ofs);

/*
	DEBUG_VEC8( ia_dx );
	DEBUG_VEC8( ia_dy );
	DEBUG_VEC8( ia_ofs );

	DEBUG_VEC8( i_area_dx );
	DEBUG_VEC8( i_area_dy );
	DEBUG_VEC8( i_area_ofs );

	DEBUG_VECf( area_ofs );
	DEBUG_VECf( base_area );
*/
	DEBUG_VEC8( fcgt_area );
	DEBUG_VECf( start_A );
	DEBUG_VECf( area_dx );
	DEBUG_VECf( area_dy );

/*
	vec_float4 cv_total = spu_convtf(total, 4);
	vec_float4 cv_total_v = spu_convtf(total_v, 4);
	vec_float4 cv_offset = spu_convtf(offset, 4);
	DEBUG_VECf( cv_total_v );
	DEBUG_VECf( cv_total );
	DEBUG_VECf( cv_offset );
*/
	vec_float4 cv_A = spu_convtf(triA, 4);
	vec_float4 cv_Adx = spu_convtf(triAdx, 15);
	vec_float4 cv_Ady = spu_convtf(triAdy, 15);
	DEBUG_VECf( cv_A );
	DEBUG_VECf( cv_Adx );
	DEBUG_VECf( cv_Ady );

/*
*/

	triangle->x = TRIx;
	triangle->y = TRIy;
	triangle->z = TRIz;
	triangle->w = TRIw;

	triangle->r = TRIr;
	triangle->g = TRIg;
	triangle->b = TRIb;
	triangle->a = TRIa;

	triangle->s = TRIs;
	triangle->t = TRIt;
	triangle->u = TRIu;
	triangle->v = TRIv;

	return triangle+1;


//	triangle->A_dx = area_dx;
//	triangle->A_dy = area_dy;


	vec_float4 v_t_cw = spu_shuffle(TRIt, TRIt, shuffle_tri_cw);
	vec_float4 v_t_ccw = spu_shuffle(TRIt, TRIt, shuffle_tri_ccw);

	vec_float4 v_bt_to_ct = spu_sub(v_t_cw, v_t_ccw); // deliberately change sign...

	////////////////////////////////
	//
	// clip minmax to screen boundary

	vec_int4 minmax = spu_convts(minmax_,0);

	//vec_int4 mm_gt = { 0, 0, screen.width-32, screen.height-32 };
	vec_int4 mm_gt = { 0, 0, 640-32, 480-32 };

	vec_uint4 mm_cmp = spu_cmpgt(minmax,mm_gt);
	vec_uint4 mm_inv = { -1, -1, 0, 0 };
	vec_uint4 mm_sel = spu_xor(mm_cmp, mm_inv);
	minmax = spu_sel(minmax, mm_gt, mm_sel);

	vec_int4 minmax_block = spu_rlmaska(minmax,-5);
	vec_int4 minmax_block_mask = minmax & spu_splats(~31);
	vec_float4 minmax_block_topleft = spu_convtf(minmax_block_mask,0);
/*
	printf("minmax %d %d %d %d\n",
		spu_extract(minmax,0),
		spu_extract(minmax,1),
		spu_extract(minmax,2),
		spu_extract(minmax,3));
*/
	int block_left = spu_extract(minmax_block,0);
	int block_top = spu_extract(minmax_block,1);
	int block_right = spu_extract(minmax_block,2);
	int block_bottom = spu_extract(minmax_block,3);

//	triangle->step = triangle->step_start = block_right - block_left;
//	triangle->cur_x = block_left;
//	triangle->cur_y = block_top;
//	triangle->block_left = block_left;
//	triangle->left = (block_bottom+1-block_top)*(block_right+1-block_left);

//	DEBUG_VEC8( minmax_block_mask );

/*
	triangle->A = ((
		      // spu_madd(spu_splats(spu_extract(minmax_block_topleft,0)),area_dx,
	              // spu_madd(spu_splats(spu_extract(minmax_block_topleft,1)),area_dy,
		      spu_sub(base_area, area_ofs)));

	triangle->blockA_dy = spu_madd(mulsn28,area_dx,area_dy);
	triangle->A_dx32 = spu_mul(muls32,area_dx);

	vec_float4 pixels_wide = spu_splats(spu_extract(minmax_block_topleft,2) -
				            spu_extract(minmax_block_topleft,0));
	triangle->A_dy32 = spu_nmsub(pixels_wide,area_dx,spu_mul(muls32,area_dy));
	triangle->A_dx4 = spu_mul(muls4,area_dx);
*/

/*
	printf("%8.2f | %8.2f,%8.2f | %8.2f,%8.2f | +%8.2f,%8.2f\n",
		spu_extract(base_area,0), spu_extract(area_dx,0), spu_extract(area_dy,0),
		spu_extract(area_dx,1), spu_extract(area_dy,1),
		spu_extract(area_dx,2), spu_extract(area_dy,2));
	printf("%8x | %8x,%8x | %8x,%8x | %8x,%8x\n\n",
		spu_extract(i_base_area,0), spu_extract(i_area_dx,0), spu_extract(i_area_dy,0),
		spu_extract(i_area_dx,1), spu_extract(i_area_dy,1),
		spu_extract(i_area_dx,2), spu_extract(i_area_dy,2));
*/

///////////////////////////////////////////

	return triangle + 1;

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

	unsigned long triangle_is_visible_mask = spu_extract(fcgt_area, 0);
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
	TRIx = spu_shuffle(TRIx, (vec_float4) s.x, inserter);
	TRIy = spu_shuffle(TRIy, (vec_float4) s.y, inserter);
	TRIz = spu_shuffle(TRIz, (vec_float4) s.z, inserter);
	TRIw = spu_shuffle(TRIw, (vec_float4) s.w, inserter);
	TRIr = spu_shuffle(TRIr, (vec_float4) col.x, inserter);
	TRIg = spu_shuffle(TRIg, (vec_float4) col.y, inserter);
	TRIb = spu_shuffle(TRIb, (vec_float4) col.z, inserter);
	TRIa = spu_shuffle(TRIa, (vec_float4) col.w, inserter);
	TRIs = spu_shuffle(TRIs, (vec_float4) tex.x, inserter);
	TRIt = spu_shuffle(TRIt, (vec_float4) tex.y, inserter);
	TRIu = spu_shuffle(TRIu, (vec_float4) tex.z, inserter);
	TRIv = spu_shuffle(TRIv, (vec_float4) tex.w, inserter);
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

	//vec_uchar16 v_valid_rhs		= spu_orc( v_rewind_valid, v_rewind );
	//vec_uchar16 v_invalid		= spu_nand( v_extend_valid, v_valid_rhs );
	vec_uchar16 v_valid_rhs		= spu_and( v_rewind_invalid, v_rewind );
	vec_uchar16 v_invalid		= spu_orc( v_valid_rhs, v_extend_valid );

	vec_ushort8 v_free		= spu_promote( cache->chunksFree,1 );
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
	unsigned long long trianglebuffer_ea = cache->triangleBase + (offset & ~127);
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

				printf("len %x, endTriBase %x, next_pointer %x, ea %x:%08x len %x\n",
					length, endTriangleBase, next_pointer,
					mfc_ea2h(trianglebuffer_ea), mfc_ea2l(trianglebuffer_ea), length );

				// DMA the triangle data out
				spu_mfcdma64(trianglebuffer, mfc_ea2h(trianglebuffer_ea), mfc_ea2l(trianglebuffer_ea), length, 0, MFC_PUT_CMD);
				mfc_write_tag_mask(1<<0);
				mfc_read_tag_status_all();

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

			}
			printf("done triangle\n");

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

