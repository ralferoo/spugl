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
#include "fifo.h"
#include "struct.h"
#include "queue.h"

//#define CHECK_STATE_TABLE

typedef struct {
	vec_float4	x,y,z,w;	// coords
	vec_float4	r,g,b,a;	// primary colour
	vec_float4	s,t,u,v;	// primary texture
	vec_float4	A,dAdx,dAdy;	// weight information
	vec_float4	minmax;		// bounding box (xmin,ymin,xmax,ymax)

	u32 		texture;
	void *		shader;
	unsigned int dummy;
} triangle;

extern void _draw_imp_triangle(triangle* tri);

extern float4 current_colour;
extern float4 current_texcoord;
extern u32 current_texture;
extern _bitmap_image screen;

extern SPU_CONTROL control;
extern TextureDefinition* currentTexture;

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

extern void* linearColourFill(void* self, Block* block, ActiveBlock* active, int tag);
extern void* textureMapFill(void* self, Block* block, ActiveBlock* active, int tag);
extern void* linearTextureMapFill(void* self, Block* block, ActiveBlock* active, int tag);
extern void* fastTextureMapFill(void* self, Block* block, ActiveBlock* active, int tag);
extern void* lessMulsLinearTextureMapFill(void* self, Block* block, ActiveBlock* active, int tag);

int dummyProducer(Triangle* tri, Block* block)
{
	static int id = 0;
	static int a = 3;
	if (tri->left<0) {
		tri->left = //(a%47)+1;
		a+=19;
		printf("new dummy producer, initialising left to %d\n", tri->left);
	}
	if (tri->left>0) {
		tri->left--;
		tri->count++;
		printf("dummy producer t:%x b:%x c:%d id:%d\n", tri, block, tri->count, id);
		block->process = tri->init_block;
		block->triangle = tri;
		block->bx = id++;
		return 1;
	} else {
		tri->count--;
		printf("dummy finished producer t:%x b:%x c:%d\n", tri, block, tri->count);
		return 0;
	}
}


//////////////////////////////////////////////////////////////////////////////

int triangleProducer(Triangle* tri, Block* block)
{
	vec_float4 A = tri->A;
	vec_float4 A_dx32 = tri->A_dx32;
	vec_float4 A_dy32 = tri->A_dy32;
	vec_float4 blockA_dy = tri->blockA_dy;
	int bx = tri->cur_x, by = tri->cur_y, block_left=tri->block_left;
	vec_int4 step = spu_splats((int)tri->step);
	vec_int4 step_start = spu_splats((int)tri->step_start);

	block->process = tri->init_block;
	block->bx = bx;
	block->by = by;
	block->triangle = tri;
	block->A=A;
	block->left=32*8;
	tri->count++;

	vec_uint4 step_eq0 = spu_cmpeq(step,spu_splats(0));
	vec_float4 A_d32 = spu_sel(A_dx32,A_dy32,step_eq0);
	A += A_d32;

	bx = if_then_else(spu_extract(step_eq0,0), block_left, bx+1);
	by = if_then_else(spu_extract(step_eq0,0), by+1, by);
	vec_int4 t_step = spu_add(step, spu_splats(-1));
	step = spu_sel(t_step, step_start, step_eq0);

	unsigned int left = tri->left;
	left--;

	tri->cur_x = bx;
	tri->cur_y = by;
	tri->step = spu_extract(step,0);
	tri->left = left;
	tri->A = A;

	if (left==0) {
		tri->count--;
		tri->produce = 0;
	}

	control.blocks_produced_count++;

	return bx | (by<<8);
}

static void imp_triangle(struct __TRIANGLE * triangle)
{
	vec_float4 t_vx_cw = spu_shuffle(TRIx, TRIx, shuffle_tri_cw);
	vec_uint4 fcgt_x = spu_cmpgt(TRIx, t_vx_cw);	// all-ones if ax>bx, bx>cx, cx>ax, ???
	u32 fcgt_bits_x = spu_extract(spu_gather(fcgt_x), 0) & ~1;
	vec_uchar16 shuf_x = spu_slqwbyte(minimax_x, fcgt_bits_x);

	vec_float4 t_vy_cw = spu_shuffle(TRIy, TRIy, shuffle_tri_cw);
	vec_uint4 fcgt_y = spu_cmpgt(TRIy, t_vy_cw);	// all-ones if ay>by, by>cy, cy>ay, ???
	u32 fcgt_bits_y = spu_extract(spu_gather(fcgt_y), 0) & ~1;
	vec_uchar16 shuf_y = spu_slqwbyte(minimax_y, fcgt_bits_y);

	vec_uchar16 shuf_minmax_base = spu_shuffle(shuf_x, shuf_y, minimax_merge);
	vec_uchar16 shuf_minmax = spu_or(shuf_minmax_base, minimax_add);
	vec_float4 minmax_ = spu_shuffle(TRIx, TRIy, shuf_minmax);

	vec_float4 v_x_cw = spu_shuffle(TRIx, TRIx, shuffle_tri_cw);
	vec_float4 v_x_ccw = spu_shuffle(TRIx, TRIx, shuffle_tri_ccw);

	vec_float4 v_y_cw = spu_shuffle(TRIy, TRIy, shuffle_tri_cw);
	vec_float4 v_y_ccw = spu_shuffle(TRIy, TRIy, shuffle_tri_ccw);

	vec_float4 v_by_to_cy = spu_sub(v_y_ccw, v_y_cw);

	vec_float4 face_mul = spu_mul(TRIx, v_by_to_cy);
	float face_sum = spu_extract(face_mul, 0) +
			 spu_extract(face_mul, 1) +
			 spu_extract(face_mul, 2);

	vec_float4 cast_zero = (vec_float4) {0.0f, 0.0f, 0.0f, 0.0f};
	vec_float4 base_area = spu_insert(face_sum, cast_zero, 0);
	vec_uint4 fcgt_area = spu_cmpgt(cast_zero, base_area);

	vec_float4 area_dx = spu_sub(v_y_ccw, v_y_cw); // cy -> by
	vec_float4 area_dy = spu_sub(v_x_cw, v_x_ccw); // bx -> cx

	vec_float4 area_ofs = spu_madd(spu_splats(spu_extract(TRIx,0)),area_dx,
			spu_mul(spu_splats(spu_extract(TRIy,0)),area_dy));

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
	
	triangle->A_dx = area_dx;
	triangle->A_dy = area_dy;


	vec_float4 v_t_cw = spu_shuffle(TRIt, TRIt, shuffle_tri_cw);
	vec_float4 v_t_ccw = spu_shuffle(TRIt, TRIt, shuffle_tri_ccw);

	vec_float4 v_bt_to_ct = spu_sub(v_t_cw, v_t_ccw); // deliberately change sign...

	vec_float4 tex_cover_mul = spu_mul(TRIs, v_bt_to_ct);
	float tex_cover = spu_extract(tex_cover_mul, 0) +
			 spu_extract(tex_cover_mul, 1) +
			 spu_extract(tex_cover_mul, 2);
	triangle->tex_cover = spu_splats(tex_cover/face_sum);

	////////////////////////////////
	//
	// clip minmax to screen boundary

	vec_int4 minmax = spu_convts(minmax_,0);

	vec_int4 mm_gt = { 0, 0, screen.width-32, screen.height-32 };
	vec_uint4 mm_cmp = spu_cmpgt(minmax,mm_gt);
	vec_uint4 mm_inv = { -1, -1, 0, 0 };
	vec_uint4 mm_sel = spu_xor(mm_cmp, mm_inv);
	minmax = spu_sel(minmax, mm_gt, mm_sel);

	vec_int4 minmax_block = spu_rlmaska(minmax,-5);
	vec_int4 minmax_block_mask = minmax & spu_splats(~31);
	vec_float4 minmax_block_topleft = spu_convtf(minmax_block_mask,0);

	int block_left = spu_extract(minmax_block,0);
	int block_top = spu_extract(minmax_block,1);
	int block_right = spu_extract(minmax_block,2);
	int block_bottom = spu_extract(minmax_block,3);

	triangle->step = triangle->step_start = block_right - block_left;
	triangle->cur_x = block_left;
	triangle->cur_y = block_top;
	triangle->block_left = block_left;
	triangle->left = (block_bottom+1-block_top)*(block_right+1-block_left);

	triangle->A = spu_madd(spu_splats(spu_extract(minmax_block_topleft,0)),area_dx,
	              spu_madd(spu_splats(spu_extract(minmax_block_topleft,1)),area_dy,
		      spu_sub(base_area, area_ofs)));

	triangle->blockA_dy = spu_madd(mulsn28,area_dx,area_dy);
	triangle->A_dx32 = spu_mul(muls32,area_dx);

	vec_float4 pixels_wide = spu_splats(spu_extract(minmax_block_topleft,2) -
				            spu_extract(minmax_block_topleft,0));
	triangle->A_dy32 = spu_nmsub(pixels_wide,area_dx,spu_mul(muls32,area_dy));
	triangle->A_dx4 = spu_mul(muls4,area_dx);

///////////////////////////////////////////

/*
	printf("triangle using currentTexture %lx, id base %d, shift %x, mask %x, count %d\n",
		currentTexture, currentTexture->tex_id_base, currentTexture->tex_y_shift,
		currentTexture->tex_id_mask, currentTexture->users); 
*/

	triangle->texture = currentTexture;
	currentTexture->users++;
//	triangle->tex_id_base = currentTexture->tex_id_base;

//	triangle->init_block = &linearColourFill;


//	static int a=0;
//	if (a++&2048)
//		triangle->init_block = &textureMapFill;
//	else
//
//	triangle->init_block = &linearTextureMapFill;

	triangle->init_block = &lessMulsLinearTextureMapFill;

//	triangle->init_block = &fastTextureMapFill;

// if the triangle is visible (i.e. area>0), then we increment the triangle
// out ptr to just past the triangle data we've just written to memory.
// if the triangle is invisible (i.e. area<0) then leave the pointer in the
// same place so that we re-use the triangle slot on the next triangle.

	unsigned long triangle_is_visible_mask = spu_extract(fcgt_area, 0);
	triangle->count = 1 & triangle_is_visible_mask;
	triangle->produce = (void*)( ((u32)&triangleProducer) & triangle_is_visible_mask );

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

void* imp_vertex(void* from, float4 in, struct __TRIANGLE * triangle)
{
#ifdef CHECK_STATE_TABLE
	if (current_state < 0 ||
		  current_state >= sizeof(shuffle_map)/sizeof(shuffle_map[0])) {
		raise_error(ERROR_VERTEX_INVALID_STATE);
		return from;
	}
#endif
	int ins = shuffle_map[current_state].insert;
#ifdef CHECK_STATE_TABLE
	if (ins >= sizeof(shuffles)/sizeof(shuffles[0])) {
		raise_error(ERROR_VERTEX_INVALID_SHUFFLE);
		return from;
	}
#endif

	vec_uchar16 inserter = shuffles[ins];

	// just for testing, have hard-coded persective and screen
	// transformations here. they'll probably live here anyway, just
	// done with matrices.

	float recip = 420.0f / (in.z-282.0f);
//	float recip = 420.0f / (in.z-180.0f);
	float4 s = {.x=in.x*recip+screen.width/2, .y = in.y*recip+screen.height/2, .z = in.z*recip, .w = recip};

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
			imp_triangle(triangle);

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
			imp_triangle(triangle);
			break;
	}
	current_state = shuffle_map[current_state].next;

	return from;
}
	
int imp_validate_state(int state)
{
	return state >= 0 &&
		  state < sizeof(shuffle_map)/sizeof(shuffle_map[0]) &&
		  shuffle_map[state].insert == 0;
}

void imp_close_segment(struct __TRIANGLE * triangle)
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
				imp_triangle(triangle);
				break;
		}
	}
}
