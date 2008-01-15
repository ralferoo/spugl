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

extern _bitmap_image screen;

static void imp_point()
{
}

static void imp_line()
{
}

typedef void TRIANGLE_SPAN_FUNCTION(u32* screenBuffer, int start, int length, vertex_state* a, vertex_state* b, vertex_state* c, float Sa, float Sb, float Sc, float Da, float Db, float Dc);

extern TRIANGLE_SPAN_FUNCTION triangleSpan;

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
	vec_float4 v_y_cw = spu_shuffle(vy, vy, shuffle_tri_cw);
	vec_float4 v_y_ccw = spu_shuffle(vy, vy, shuffle_tri_ccw);

	vec_float4 v_by_to_cy = spu_sub(v_y_ccw, v_y_cw);

	vec_float4 face_mul = spu_mul(vx, v_by_to_cy);
	float face_sum = spu_extract(face_mul, 0) +
			 spu_extract(face_mul, 1) +
			 spu_extract(face_mul, 2);
	vec_float4 cast_sum = (vec_float4) face_sum;
	vec_float4 cast_zero = (vec_float4) 0.0f;
	vec_uint4 fcgt_area = spu_cmpgt(cast_zero, cast_sum);
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
	
	triangle_out_ptr -> A = face_sum;
	triangle_out_ptr -> dAdx = 0.0f;
	triangle_out_ptr -> dAdy = 0.0f;
	
	triangle_out_ptr -> texture = 0;
	triangle_out_ptr -> shader = 0;
	triangle_out_ptr -> right = right_and;

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

static inline vertex_state pull_compat(int j, triangle* tri)
{
	vec_uchar16 sel = tri->shuffle;

	float4 s = {
			.x = spu_extract(spu_shuffle(tri->x, tri->x, sel), j),
			.y = spu_extract(spu_shuffle(tri->y, tri->y, sel), j),
			.z = spu_extract(spu_shuffle(tri->z, tri->z, sel), j),
			.w = spu_extract(spu_shuffle(tri->w, tri->w, sel), j),
	};
	float4 c = {
			.x = spu_extract(spu_shuffle(tri->r, tri->r, sel), j),
			.y = spu_extract(spu_shuffle(tri->g, tri->g, sel), j),
			.z = spu_extract(spu_shuffle(tri->b, tri->b, sel), j),
			.w = spu_extract(spu_shuffle(tri->a, tri->a, sel), j),
	};
	vertex_state v = {
		.coords = s,
		.colour = c,
	};
	return v;
}

void _draw_imp_triangle(triangle* tri)
{
//	if (triangle_out_ptr == next_triangle)
//		return next_triangle;

//	vec_float4 v_y_cw = spu_shuffle(TRIy, TRIy, r_cw);
//	vec_float4 v_y_ccw = spu_shuffle(TRIy, TRIy, r_ccw);

	TRIANGLE_SPAN_FUNCTION* func = triangleSpan;

	vertex_state a = pull_compat(0, tri);
	vertex_state b = pull_compat(1, tri);
	vertex_state c = pull_compat(2, tri);

	unsigned long left = ~ tri->right;
		
	void* lb = malloc( (((screen.width<<2)+BYTE_ALIGNMENT)&~BYTE_ALIGNMENT) + BYTE_ALIGNMENT);
	int tag_id = 1;
	int mask;

	float y = ((int)a.coords.y) + 0.5;

	float lgrad = (c.coords.x - a.coords.x) / (c.coords.y - a.coords.y);
	float lx = ((int)a.coords.x) + 0.5;

	float rgrad = (b.coords.x - a.coords.x) / (b.coords.y - a.coords.y);
	float rx = ((int)a.coords.x) + 0.5;

	float mid, bottom;
	if (left) {
		mid = c.coords.y; bottom = b.coords.y;
	} else {
		mid = b.coords.y; bottom = c.coords.y;
	}

//	printf ("y=%.2f, lx=%.2f, lgrad=%.2f, rx=%.2f, rgrad=%.2f, mid=%.2f, bottom=%.2f\n",
//		y, lx, lgrad, rx, rgrad, mid, bottom);

	float tab = a.coords.x * b.coords.y - b.coords.x * a.coords.y;
	float tbc = b.coords.x * c.coords.y - c.coords.x * b.coords.y;
	float tca = c.coords.x * a.coords.y - a.coords.x * c.coords.y;

	float tap = a.coords.x * y - a.coords.y * lx;
	float tbp = b.coords.x * y - b.coords.y * lx;
	float tcp = c.coords.x * y - c.coords.y * lx;

	float dap = a.coords.x - a.coords.y * lgrad;
	float dbp = b.coords.x - b.coords.y * lgrad;
	float dcp = c.coords.x - c.coords.y * lgrad;

	float Sa = -tbc-tcp+tbp;
	float Sb = -tca-tap+tcp;
	float Sc = -tab-tbp+tap;

	float dSa = -dcp+dbp;
	float dSb = -dap+dcp;
	float dSc = -dbp+dap;

	float Da = c.coords.y - b.coords.y;
	float Db = a.coords.y - c.coords.y;
	float Dc = b.coords.y - a.coords.y;
/*
	printf("face_sum %f, Sa=%f, Sb=%f, Sc=%f\n",
			face_sum, Sa, Sb, Sc);
	printf("dSa=%f, dSb=%f, dSc=%f, dSb+dSc=%f, diff=%f\n\n",
			dSa, dSb, dSc, dSb+dSc, face_sum-Sa);
*/
	while (y < mid && y < 0) {
		y += 1.0;
		lx += lgrad;
		rx += rgrad;
		Sa += dSa;
		Sb += dSb;
		Sc += dSc;
	}

	while (y < mid && y < screen.height) {
		float l = lx>0.5 ? lx : 0.5;
		float r = rx<screen.width-1 ? rx : screen.width-0.5;
		if (l <= r) {
	    		int fbLine = (int)y;
			u64 scrbuf = screen.address + screen.bytes_per_line*fbLine;
			mfc_getf(lb, scrbuf, (((screen.width<<2)+BYTE_ALIGNMENT)&~BYTE_ALIGNMENT), tag_id, 0, 0);
			mfc_write_tag_mask(1<<tag_id);
			mask = mfc_read_tag_status_any();

			int start = (int)l;
			int length = (int)(r-l);
			(*func)(lb, start, length, &a, &b, &c, Sa, Sb, Sc, Da, Db, Dc);
			mfc_putf(lb, scrbuf, (((screen.width<<2)+BYTE_ALIGNMENT)&~BYTE_ALIGNMENT), tag_id, 0, 0);
			mfc_write_tag_mask(1<<tag_id);
			mask = mfc_read_tag_status_any();
		}
		y += 1.0;
		lx += lgrad;
		rx += rgrad;
		Sa += dSa;
		Sb += dSb;
		Sc += dSc;
	}

	if (left) {
		lgrad = (b.coords.x - c.coords.x) / (b.coords.y - c.coords.y);
		lx = ((int)c.coords.x) + 0.5;

		tap = a.coords.x * y - a.coords.y * lx;
		tbp = b.coords.x * y - b.coords.y * lx;
		tcp = c.coords.x * y - c.coords.y * lx;

		dap = a.coords.x - a.coords.y * lgrad;
		dbp = b.coords.x - b.coords.y * lgrad;
		dcp = c.coords.x - c.coords.y * lgrad;
	
		Sa = -tbc-tcp+tbp;
		Sb = -tca-tap+tcp;
		Sc = -tab-tbp+tap;

		dSa = -dcp+dbp;
		dSb = -dap+dcp;
		dSc = -dbp+dap;
	} else {
		rgrad = (c.coords.x - b.coords.x) / (c.coords.y - b.coords.y);
		rx = ((int)b.coords.x) + 0.5;
	}
		
//	printf ("cont y=%.2f, lx=%.2f, lgrad=%.2f, rx=%.2f, rgrad=%.2f, mid=%.2f, bottom=%.2f\n",
//		y, lx, lgrad, rx, rgrad, mid, bottom);

	while (y < bottom && y < 0) {
		y += 1.0;
		lx += lgrad;
		rx += rgrad;
		Sa += dSa;
		Sb += dSb;
		Sc += dSc;
	}

	while (y < bottom && y < screen.height) {
		float l = lx>0.5 ? lx : 0.5;
		float r = rx<screen.width-1 ? rx : screen.width-0.5;
		if (l < r) {
	    		int fbLine = (int)y;
			u64 scrbuf = screen.address + screen.bytes_per_line*fbLine;
			mfc_getf(lb, scrbuf, (((screen.width<<2)+BYTE_ALIGNMENT)&~BYTE_ALIGNMENT), tag_id, 0, 0);
			mfc_write_tag_mask(1<<tag_id);
			mask = mfc_read_tag_status_any();

			int start = (int)l;
			int length = (int)(r-l);
			(*func)(lb, start, length, &a, &b, &c, Sa, Sb, Sc, Da, Db, Dc);
			mfc_putf(lb, scrbuf, (((screen.width<<2)+BYTE_ALIGNMENT)&~BYTE_ALIGNMENT), tag_id, 0, 0);
			mfc_write_tag_mask(1<<tag_id);
			mask = mfc_read_tag_status_any();
		}
		y += 1.0;
		lx += lgrad;
		rx += rgrad;
		Sa += dSa;
		Sb += dSb;
		Sc += dSc;
	}
	free(lb);
}

//////////////////////////////////////////////////////////////////////////////

extern _bitmap_image screen;

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
	
extern float4 current_colour;

void* imp_vertex(void* from, float4 in)
{
	if (current_state < 0 ||
		  current_state >= sizeof(shuffle_map)/sizeof(shuffle_map[0])) {
		raise_error(ERROR_VERTEX_INVALID_STATE);
		return from;
	}

	int ins = shuffle_map[current_state].insert;
	if (ins >= sizeof(shuffles)/sizeof(shuffles[0])) {
		raise_error(ERROR_VERTEX_INVALID_SHUFFLE);
		return from;
	}
	vec_uchar16 inserter = shuffles[ins];

//////////////////////////////////////////////////////////////////////////

	// just for testing, have hard-coded persective and screen
	// transformations here. they'll probably live here anyway, just
	// done with matrices.

	float recip = 420.0 / (in.z-420.0);
	float4 s = {.x=in.x*recip+screen.width/2, .y = in.y*recip+screen.height/2, .z = in.z*recip, .w = recip};

	float4 c= current_colour;
	float4 col = {.x=c.x*recip, .y = c.y*recip, .z = c.z*recip, .w = c.w*recip};

//	printf("tran (%.2f,%.2f,%.2f,%.2f)\n", s.x, s.y, s.z, s.w);

//	float4 p = {.x=in.x - screen.width/2, .y = in.y - screen.height/2, .z = in.z, .w = 420/(in.z-420)};
//	float recip = 1.0/p.w;
//	float4 s = {.x=p.x*recip, .y = p.y*recip, .z = p.z*recip, .w = recip};

	shuffle_in(inserter, s, col);

//////////////////////////////////////////////////////////////////////////

	// check to see if we need to draw
	switch (shuffle_map[current_state].add) {
		case ADD_LINE:
			imp_line();
			break;
		case ADD_TRIANGLE2:
			imp_triangle();

			current_state = shuffle_map[current_state].next;
			ins = shuffle_map[current_state].insert;
			if (ins >= sizeof(shuffles)/sizeof(shuffles[0])) {
				raise_error(ERROR_VERTEX_INVALID_SHUFFLE);
				return from;
			}
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
