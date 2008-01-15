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

void debug_state(char*s, int i)
{
	printf("\n%s:",s);
	vec_uchar16 sel = TRIorder;
	int j = 0;
	while (i--) {
		printf("v=(%.2f,%.2f,%.2f,%.2f)"
			" c=(%.2f,%.2f,%.2f,%.2f)"
			"%s",
			spu_extract(spu_shuffle(TRIx, TRIx, sel), j),
			spu_extract(spu_shuffle(TRIy, TRIx, sel), j),
			spu_extract(spu_shuffle(TRIz, TRIx, sel), j),
			spu_extract(spu_shuffle(TRIw, TRIx, sel), j),

			spu_extract(spu_shuffle(TRIr, TRIx, sel), j),
			spu_extract(spu_shuffle(TRIg, TRIx, sel), j),
			spu_extract(spu_shuffle(TRIb, TRIx, sel), j),
			spu_extract(spu_shuffle(TRIa, TRIx, sel), j),

			i ? ",\n\t" : "\n");
		j++;
	}
}

void imp_point()
{
	debug_state("point\t",1);
}

void imp_line()
{
	debug_state("line\t",2);
}

typedef void TRIANGLE_SPAN_FUNCTION
(u32* screenBuffer, int start, int length, vertex_state* a, vertex_state* b, vertex_state* c, float Sa, float Sb, float Sc, float Da, float Db, float Dc)
;

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

static void debug(vertex_state v)
{
//       printf("DBG v=(%.2f,%.2f,%.2f,%.2f)",
//               v.coords.x, v.coords.y, v.coords.z, v.coords.w);
//       printf(" c=(%.2f,%.2f,%.2f,%.2f)\n",
//               v.colour.x, v.colour.y, v.colour.z, v.colour.w);
}

vec_uchar16 shuffle_tri_cw = {
	SEL_A1 SEL_A2 SEL_A0 SEL_00
};
vec_uchar16 shuffle_tri_ccw = {
	SEL_A2 SEL_A0 SEL_A1 SEL_00
};

// this is tricky... 
// L/R is set by bit L=0x20, R=0x00
// none(0), cw(1), ccw(2) is encoded as 0x10, 0x0e, 0x0f respectively
// which is itself used as a shuffle mask on a shuffle delta mask
vec_uchar16 triangle_order_data = {
0,0,	/*  0 - unused <----- this is the primary word */
0x10,0x10,	/*  2 - 0 0 1 => 0 1 (R0) */
0x0c,0x0c,	/*  4 - 0 1 0 => 0 1 (R2) */
0x30,0x30,	/*  6 - 0 1 1 => 1 1 (L0) */
0x08,0x08,	/*  8 - 1 0 0 => 0 0 (R1) */
0x28,0x28,	/* 10 - 1 0 1 => 0 1 (L1) */
0x2c,0x2c,	/* 12 - 1 1 0 => 0 1 (L2) */
0,0	/* 14 - unused */
};
vec_uchar16 copy_order_data = {
1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1
};
vec_uchar16 copy_as_is = {
0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
};

//vec_uchar16 xb_yc ={
//	SEL_A2 SEL_A0 SEL_A1 SEL_00
//};

void imp_triangle()
{
	// TRIorder holds the select mask for triangle as is
	// these two are select masks for clockwise and counter-clockwise
	vec_uchar16 r_cw = spu_shuffle(TRIorder, TRIorder, shuffle_tri_cw);
	vec_uchar16 r_ccw = spu_shuffle(TRIorder, TRIorder, shuffle_tri_ccw);

	vec_float4 vx = spu_shuffle(TRIx, TRIx, TRIorder);
	vec_float4 vx_cw = spu_shuffle(TRIx, TRIx, r_cw);
	vec_float4 vx_ccw = spu_shuffle(TRIx, TRIx, r_ccw);

	vec_float4 vy = spu_shuffle(TRIy, TRIy, TRIorder);
	vec_float4 vy_cw = spu_shuffle(TRIy, TRIy, r_cw);
	vec_float4 vy_ccw = spu_shuffle(TRIy, TRIy, r_ccw);

	// all-ones if ay>by, by>cy, cy>ay, 0>0
	vec_uint4 fcgt_y = spu_cmpgt(vy, vy_cw);
	u32 fcgt_bits = spu_extract(spu_gather(fcgt_y), 0);
	//printf("fcgt_bits = %d\n", fcgt_bits);

	vec_uchar16 r_right_padded = spu_rlqwbyte(TRIorder, -4);
	vec_uchar16 swap_mask = spu_rlqwbyte(triangle_order_data, fcgt_bits);
	vec_uchar16 rep_swap_add = spu_shuffle(swap_mask, swap_mask, copy_order_data);
	vec_ushort8 q1 = (vec_ushort8)rep_swap_add;
	vec_ushort8 q2 = (vec_ushort8)copy_as_is;
	vec_uchar16 ns_mask = (vec_uchar16) spu_add(q1,q2);
	//vec_uchar16 rep_ns_mask = spu_add(rep_swap_add,copy_as_is);
	vec_uchar16 new_select = spu_shuffle(r_right_padded,TRIorder,ns_mask);
	
	unsigned char left_flag = spu_extract(rep_swap_add,0); // & 0x20;

int i;
for (i=0; i<16; i++)
	printf("%02x ",spu_extract(TRIorder,i));
printf("\n");
for (i=0; i<16; i++)
	printf("%02x ",spu_extract(new_select,i));
printf("\n\n");

	// new new_select should be the mask containing a,b,c in height order
	// and left_flag should be 0x20 if the bulge is on the left edge

	vec_float4 vx_cw_sub_vx = spu_sub(vx_cw, vx);
	vec_float4 vx_ccw_sub_vx = spu_sub(vx_ccw, vx);

	vec_float4 vy_cw_sub_vy = spu_sub(vy_cw, vy);
	vec_float4 vy_ccw_sub_vy = spu_sub(vy_ccw, vy);

	

//	vec_float4 vx_mul_yv_cw = spu_mul(vx, vy_cw);
//	vec_float4 vx_mul_yv_cw = spu_mul(vx, vy_cw);

	vec_float4 face_mul = spu_mul(vx, vy_cw);

	float face_sum = spu_extract(face_mul, 0) +
			 spu_extract(face_mul, 1) +
			 spu_extract(face_mul, 2);

	printf("face_sum: %.2f (%.2f %.2f %.2f)\n", face_sum, 
			 spu_extract(face_mul, 0),
			 spu_extract(face_mul, 1),
			 spu_extract(face_mul, 2));


//	if (face_sum > 0)
//		return;

	

	vec_float4 x_r = spu_shuffle(TRIy, TRIy, TRIorder);
	vec_float4 y_r = spu_shuffle(TRIy, TRIy, TRIorder);

//			spu_extract(spu_shuffle(TRIx, TRIx, sel), j),

	TRIANGLE_SPAN_FUNCTION* func = triangleSpan;

	vertex_state a = pull_compat(0, new_select);
	vertex_state b = pull_compat(1, new_select);
	vertex_state c = pull_compat(2, new_select);

	float abx = b.coords.x - a.coords.x;
	float aby = b.coords.y - a.coords.y;
	float bcx = c.coords.x - b.coords.x;
	float bcy = c.coords.y - b.coords.y;
	float cax = a.coords.x - c.coords.x;
	float cay = a.coords.y - c.coords.y;

	float face_sm2 = a.coords.x * bcy + b.coords.x * cay + c.coords.x * aby;
	printf("face_sm2: %.2f (%.2f %.2f %.2f)\n\n", face_sm2, 
			 a.coords.x * bcy,
			 b.coords.x * cay,
			 c.coords.x * aby);

	if (face_sm2 > 0) {
		// z is negative, so +ve means counter-clockwise and so not vis
		printf("Back-facing triangle...\n");
		return;
	}
/*
	int qab = aby<0;
	int qbc = bcy<0;
	int qca = cay<0;

	int bc_a_ca = qbc && qca;
	int bc_o_ca = qbc || qca;

	int rot = 0;
	int left;
	vertex_state temp;
	if (qab && qbc) {
		left = 1;
		temp = c; c = b; b = a; a = temp;
		rot = 2;
	}
	if (qab && qca) {
		left = 1;
		temp = a; a = b; b = c; c = temp;
		rot = 1;
	}
	if (qab && !bc_o_ca) {
		left = 0;
		temp = a; a = b; b = c; c = temp;
		rot = 1;
	}
	if (!(qab || qbc)) {
		left = 0;
	}
	if (!(qab || qca)) {
		left = 0;
		temp = c; c = b; b = a; a = temp;
		rot = 2;
	}
	if (!qab && bc_a_ca) {
		left = 1;
	}

	printf("fcgt_bits = %d - %d %d %d => %d %d (%c%d) L=%02x\n", 
		fcgt_bits,
		qab?1:0, qbc?1:0, qca?1:0, bc_a_ca?1:0, bc_o_ca?1:0,
		left?'L':'R', rot, left_flag);
*/

	int left = left_flag & 0x20;




	if (b.coords.y < a.coords.y || c.coords.y < a.coords.y) {
		raise_error(ERROR_TRI_A_NOT_LEAST);
		return;
	}
	if (left && b.coords.y < c.coords.y) {
		raise_error(ERROR_TRI_LEFT_AND_B_C);
		return;
	}
	if (!left && c.coords.y < b.coords.y) {
		raise_error(ERROR_TRI_RIGHT_AND_C_B);
		return;
	}
		
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

