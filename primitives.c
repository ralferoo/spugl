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

void imp_point()
{
}

void imp_line()
{
}

typedef void TRIANGLE_SPAN_FUNCTION(u32* screenBuffer, int start, int length, vertex_state* a, vertex_state* b, vertex_state* c, float Sa, float Sb, float Sc, float Da, float Db, float Dc);

extern TRIANGLE_SPAN_FUNCTION triangleSpan;

vertex_state pull_compat(int j, vec_uchar16 sel)
{
	float4 s = {
			.x = spu_extract(spu_shuffle(TRIx, TRIx, sel), j),
			.y = spu_extract(spu_shuffle(TRIy, TRIx, sel), j),
			.z = spu_extract(spu_shuffle(TRIz, TRIx, sel), j),
			.w = spu_extract(spu_shuffle(TRIw, TRIx, sel), j),
	};
	float4 c = {
			.x = spu_extract(spu_shuffle(TRIr, TRIx, sel), j),
			.y = spu_extract(spu_shuffle(TRIg, TRIx, sel), j),
			.z = spu_extract(spu_shuffle(TRIb, TRIx, sel), j),
			.w = spu_extract(spu_shuffle(TRIa, TRIx, sel), j),
	};
	vertex_state v = {
		.coords = s,
		.colour = c,
	};
	return v;
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


void imp_triangle()
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
	unsigned char left = spu_extract(rep_swap_add,3) & 0x20;

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
	if (face_sum > 0)
		return;

//	vec_float4 v_y_cw = spu_shuffle(TRIy, TRIy, r_cw);
//	vec_float4 v_y_ccw = spu_shuffle(TRIy, TRIy, r_ccw);

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

	TRIANGLE_SPAN_FUNCTION* func = triangleSpan;

	vertex_state a = pull_compat(0, r_normal);
	vertex_state b = pull_compat(1, r_normal);
	vertex_state c = pull_compat(2, r_normal);

		
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

