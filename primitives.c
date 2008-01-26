/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

#include <spu_mfcio.h>
#include "fifo.h"
#include "struct.h"

//#define CHECK_STATE_TABLE

extern void _draw_imp_triangle(triangle* tri);

static void imp_point()
{
}

static void imp_line()
{
}

// this is tricky... 
// basically, this is a table representing vertex y orders and how to
// translate them to the correct order.
// bit 0 represents ya>yb, bit 1 repr. yc>yb and bit 2 rep. ya>yc
// the "unused" entries are because its impossible for all 3 conditions to be
// true or false... ;)
// L/R is set by bit L=0x20, R=0x00 - this determines which is the lumpy side.
// none(0), cw(1), ccw(2) is encoded as 0 word, -2 word, -1 word respectively
// which is itself used as a shuffle mask on a shuffle delta mask.
// sadly, most of the working out of this is on paper rather than computer. :(
vec_uchar16 triangle_order_data = {
	0,0,		/*  0 - impossible */
	0x10,0x10,	/*  2 - 0 0 1 => 0 1 (R0) */
	0x0c,0x0c,	/*  4 - 0 1 0 => 0 1 (R2) */
	0x30,0x30,	/*  6 - 0 1 1 => 1 1 (L0) */
	0x08,0x08,	/*  8 - 1 0 0 => 0 0 (R1) */
	0x28,0x28,	/* 10 - 1 0 1 => 0 1 (L1) */
	0x2c,0x2c,	/* 12 - 1 1 0 => 0 1 (L2) */
	0,0		/* 14 - impossible */
};
vec_uchar16 copy_order_data = {
	1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1
};
vec_uchar16 copy_as_is = {
	SEL_A0 SEL_A1 SEL_A2 SEL_A3
};
static vec_uchar16 make_rhs_0 = {
	0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 
	0x00, 0x00, 0x00, 0x00,
};

vec_uchar16 shuffle_tri_right_padded = {
	SEL_00 SEL_A0 SEL_A1 SEL_A2
};
vec_uchar16 shuffle_tri_normal = {
	SEL_A0 SEL_A1 SEL_A2 SEL_00
};
vec_uchar16 shuffle_tri_cw = {
	SEL_A1 SEL_A2 SEL_A0 SEL_00
};
vec_uchar16 shuffle_tri_ccw = {
	SEL_A2 SEL_A0 SEL_A1 SEL_00
};

static inline triangle* _new_imp_triangle(triangle* triangle_out_ptr)
{
	// TRIorder holds the select mask for triangle as is
	// these two are select masks for clockwise and counter-clockwise

	//TRIorder = shuffle_tri_normal;

	vec_float4 t_vy = spu_shuffle(TRIy, TRIy, shuffle_tri_normal);
	vec_float4 t_vy_cw = spu_shuffle(t_vy, t_vy, shuffle_tri_cw);

	// all-ones if ay>by, by>cy, cy>ay, 0>0
	vec_uint4 fcgt_y = spu_cmpgt(t_vy, t_vy_cw);
	u32 fcgt_bits = spu_extract(spu_gather(fcgt_y), 0);
	//printf("fcgt_bits = %d\n", fcgt_bits);

	vec_uchar16 r_right_padded = shuffle_tri_right_padded; // spu_rlqwbyte(TRIorder, -4);
	vec_uchar16 swap_mask = spu_rlqwbyte(triangle_order_data, fcgt_bits);
	vec_uchar16 rep_swap_add = spu_shuffle(swap_mask, swap_mask, copy_order_data);
	vec_ushort8 q1 = (vec_ushort8)rep_swap_add;
	vec_ushort8 q2 = (vec_ushort8)copy_as_is;
	vec_uchar16 ns_mask = (vec_uchar16) spu_add(q1,q2);
	vec_uchar16 r_normal = spu_shuffle(r_right_padded,shuffle_tri_normal,ns_mask);
	//vec_uchar16 r_normal = spu_shuffle(r_right_padded,TRIorder,ns_mask);
	vec_uint4 cast_left = (vec_uint4) rep_swap_add;
	vec_uint4 cast_left_mask = spu_and(cast_left,0x20);
	vec_uint4 right_full = spu_cmpeq(cast_left_mask, (vec_uint4) 0);
	unsigned long right_and = spu_extract(right_full,0);
	
	// new r_normal should be the mask containing a,b,c in height order
	// and left_flag should be 0x20 if the bulge is on the left edge

//	vec_uchar16 r_cw = spu_shuffle(r_normal, r_normal, shuffle_tri_cw);
//	vec_uchar16 r_ccw = spu_shuffle(r_normal, r_normal, shuffle_tri_ccw);

	// we now have new shuffle normals again (yay!)

	vec_float4 vx = spu_shuffle(TRIx, TRIx, r_normal);
	vec_float4 vy = spu_shuffle(TRIy, TRIy, r_normal);

	vec_float4 v_x_cw = spu_shuffle(vx, vx, shuffle_tri_cw);
	vec_float4 v_x_ccw = spu_shuffle(vx, vx, shuffle_tri_ccw);

	vec_float4 v_y_cw = spu_shuffle(vy, vy, shuffle_tri_cw);
	vec_float4 v_y_ccw = spu_shuffle(vy, vy, shuffle_tri_ccw);

	vec_float4 v_by_to_cy = spu_sub(v_y_ccw, v_y_cw);

	vec_float4 face_mul = spu_mul(vx, v_by_to_cy);
	float face_sum = spu_extract(face_mul, 0) +
			 spu_extract(face_mul, 1) +
			 spu_extract(face_mul, 2);

	vec_float4 cast_zero = (vec_float4) {0.0f, 0.0f, 0.0f, 0.0f};
	vec_float4 base_area = spu_insert(face_sum, cast_zero, 0);
	vec_uint4 fcgt_area = spu_cmpgt(cast_zero, base_area);

	vec_float4 area_dx = spu_sub(v_y_cw, v_y_ccw); // cy -> by
	vec_float4 area_dy = spu_sub(v_x_ccw, v_x_cw); // bx -> cx

//	vec_float4 area_recip = div(spu_splats(1.0f), spu_splats(face_sum));
//	vec_float4 area_dx = spu_mul(area_recip, spu_sub(v_y_cw, v_y_ccw)); // cy -> by
//	vec_float4 area_dy = spu_mul(area_recip, spu_sub(v_x_ccw, v_x_cw)); // bx -> cx
//	vec_float4 area = (vec_float4) {1.0f, 0.0f, 0.0f, 0.0f};



	vec_float4 area_ofs = spu_madd(spu_splats(spu_extract(TRIx,0)),area_dx,
			spu_mul(spu_splats(spu_extract(TRIy,0)),area_dy));

	unsigned long advance_ptr_mask = spu_extract(fcgt_area, 0);
	triangle* next_triangle = (triangle*) ( ((void*)triangle_out_ptr) +
			(advance_ptr_mask&sizeof(triangle)) );

	triangle_out_ptr -> shuffle = r_normal;
	triangle_out_ptr -> x = TRIx;
	triangle_out_ptr -> y = TRIy;
	triangle_out_ptr -> z = TRIz;
	triangle_out_ptr -> w = TRIw;
	
	triangle_out_ptr -> r = TRIr;
	triangle_out_ptr -> g = TRIg;
	triangle_out_ptr -> b = TRIb;
	triangle_out_ptr -> a = TRIa;
	
	triangle_out_ptr -> s = TRIs;
	triangle_out_ptr -> t = TRIt;
	triangle_out_ptr -> u = TRIu;
	triangle_out_ptr -> v = TRIv;
	
	triangle_out_ptr -> A = spu_sub(base_area, area_ofs);
	triangle_out_ptr -> dAdx = area_dx;
	triangle_out_ptr -> dAdy = area_dy;
	
	triangle_out_ptr -> texture = 0;
	triangle_out_ptr -> shader = 0;
	triangle_out_ptr -> right = right_and;

//	vec_float4 v_x_cw = spu_shuffle(vx, vx, shuffle_tri_cw);
//	vec_float4 v_x_ccw = spu_shuffle(vx, vx, shuffle_tri_ccw);
//	vec_float4 v_bx_to_cx = spu_sub(v_x_ccw, v_x_cw);

//	triangle_out_ptr -> dx = v_by_to_cy;
//	triangle_out_ptr -> dy = v_bx_to_cx;

/*
	if (face_sum<0) {
	int i;
	for (i=0; i<3; i++) {
		printf("face_sum: %f, A(%c): %f, dAdx(%c)=%f dAdy(%c)=%f\n",
			face_sum,
			i+'a', spu_extract(triangle_out_ptr->A,i),
			i+'a', spu_extract(triangle_out_ptr->dAdx,i),
			i+'a', spu_extract(triangle_out_ptr->dAdy,i));
	}
	printf("\n");
	}
*/

///////////////////////////////////////////////////////////////////////////////
//
// this is a convenient break point. we have now satisfied ourselves that the
// triangle is visible (face_sum>0) and we have the correctly ordered shuffle
// mask as well as the flag to describe how to render the triangle. this is
// probably the official end of the transformation phase and should be passed
// on at this point.
//
// also... face_sum is -ve and the same as the ideal Sa and the ideal Sb and
// Sc are both 0 (except we actually calculate Sa,Sb,Sc from mid-pixel)

	return next_triangle;
}

//////////////////////////////////////////////////////////////////////////////

#define ADD_LINE	1
#define ADD_TRIANGLE	2
#define ADD_POINT	3
#define ADD_TRIANGLE2	4

int current_state = -1;
static struct {
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
static vec_uchar16 shuffles[] = {
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

static inline void shuffle_in(vec_uchar16 inserter, float4 s, float4 col) {
	TRIx = spu_shuffle(TRIx, (vec_float4) s.x, inserter);
	TRIy = spu_shuffle(TRIy, (vec_float4) s.y, inserter);
	TRIz = spu_shuffle(TRIz, (vec_float4) s.z, inserter);
	TRIw = spu_shuffle(TRIw, (vec_float4) s.w, inserter);
	TRIr = spu_shuffle(TRIr, (vec_float4) col.x, inserter);
	TRIg = spu_shuffle(TRIg, (vec_float4) col.y, inserter);
	TRIb = spu_shuffle(TRIb, (vec_float4) col.z, inserter);
	TRIa = spu_shuffle(TRIa, (vec_float4) col.w, inserter);
//	TRIs = spu_shuffle(TRIs, (vec_float4) in.s, inserter);
//	TRIt = spu_shuffle(TRIt, (vec_float4) in.t, inserter);
//	TRIu = spu_shuffle(TRIu, (vec_float4) in.u, inserter);
//	TRIv = spu_shuffle(TRIv, (vec_float4) in.v, inserter);
}

static triangle* imp_triangle()
{
	static triangle dummy[10];
	triangle* next = _new_imp_triangle(&dummy[0]);
	if (next!=&dummy[0])
		_draw_imp_triangle(&dummy[0]);
	return next;
}

extern float4 current_colour;
extern _bitmap_image screen;

void* imp_vertex(void* from, float4 in)
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

	float recip = 420.0f / (in.z-420.0f);
	float4 s = {.x=in.x*recip+screen.width/2, .y = in.y*recip+screen.height/2, .z = in.z*recip, .w = recip};

	float4 c= current_colour;
	float4 col = {.x=c.x*recip, .y = c.y*recip, .z = c.z*recip, .w = c.w*recip};

//	printf("tran (%.2f,%.2f,%.2f,%.2f)\n", s.x, s.y, s.z, s.w);

//	float4 p = {.x=in.x - screen.width/2, .y = in.y - screen.height/2, .z = in.z, .w = 420/(in.z-420)};
//	float recip = 1.0/p.w;
//	float4 s = {.x=p.x*recip, .y = p.y*recip, .z = p.z*recip, .w = recip};

	shuffle_in(inserter, s, col);

	// check to see if we need to draw
	switch (shuffle_map[current_state].add) {
		case ADD_LINE:
			imp_line();
			break;
		case ADD_TRIANGLE2:
			imp_triangle();

			current_state = shuffle_map[current_state].next;
			ins = shuffle_map[current_state].insert;
#ifdef CHECK_STATE_TABLE
			if (ins >= sizeof(shuffles)/sizeof(shuffles[0])) {
				raise_error(ERROR_VERTEX_INVALID_SHUFFLE);
				return from;
			}
#endif
			inserter = shuffles[ins];
			shuffle_in(inserter, s, col);

			// fall through here
		case ADD_TRIANGLE:
			imp_triangle();
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

void imp_close_segment()
{
	int end = shuffle_map[current_state].end;
	if (end) {
		float4 x;
		shuffle_in(shuffles[end], x, x);
		switch (shuffle_map[current_state].add) {
			case ADD_LINE:
				imp_line();
				break;
			case ADD_TRIANGLE:
				imp_triangle();
				break;
		}
	}
}
	
