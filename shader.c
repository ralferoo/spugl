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

typedef void TRIANGLE_SPAN_FUNCTION(u32* screenBuffer, int start, int length, vertex_state* a, vertex_state* b, vertex_state* c, float Sa, float Sb, float Sc, float Da, float Db, float Dc);

extern TRIANGLE_SPAN_FUNCTION triangleSpan;

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

	float y = a.coords.y; //((int)a.coords.y) + 0.5;

	float lgrad = (c.coords.x - a.coords.x) / (c.coords.y - a.coords.y);
	float lx = a.coords.x; //((int)a.coords.x) + 0.5;

	float rgrad = (b.coords.x - a.coords.x) / (b.coords.y - a.coords.y);
	float rx = a.coords.x; //((int)a.coords.x) + 0.5;

	float mid, bottom;
	if (left) {
		mid = c.coords.y; bottom = b.coords.y;
	} else {
		mid = b.coords.y; bottom = c.coords.y;
	}

	printf ("y=%.2f, lx=%.2f, lgrad=%.2f, rx=%.2f, rgrad=%.2f, mid=%.2f, bottom=%.2f\n",
		y, lx, lgrad, rx, rgrad, mid, bottom);

	float tab = a.coords.x * b.coords.y - b.coords.x * a.coords.y;
	float tbc = b.coords.x * c.coords.y - c.coords.x * b.coords.y;
	float tca = c.coords.x * a.coords.y - a.coords.x * c.coords.y;

	float tap = a.coords.x * y - a.coords.y * lx;
	float tbp = b.coords.x * y - b.coords.y * lx;
	float tcp = c.coords.x * y - c.coords.y * lx;

	float dap = a.coords.x - a.coords.y * lgrad;
	float dbp = b.coords.x - b.coords.y * lgrad;
	float dcp = c.coords.x - c.coords.y * lgrad;

//	float Sa = -tbc-tcp+tbp;
//	float Sb = -tca-tap+tcp;
//	float Sc = -tab-tbp+tap;

	float dSa = -dcp+dbp;
	float dSb = -dap+dcp;
	float dSc = -dbp+dap;

//	float Da = c.coords.y - b.coords.y;
//	float Db = a.coords.y - c.coords.y;
//	float Dc = b.coords.y - a.coords.y;

	float face_sum = tri->A;
	vec_float4 _dx = tri->dx; //spu_shuffle(tri->dx, tri->dx, tri->shuffle);
	vec_float4 _dy = tri->dy; //spu_shuffle(tri->dy, tri->dy, tri->shuffle);

//	printf("    Sa=%f, Sb=%f, Sc=%f\n",
//			Sa, Sb, Sc);

	printf("    dSa=%f, dSb=%f, dSc=%f\n",
			dSa, dSb, dSc);

//	printf("old Da=%f, Db=%f, Dc=%f, Db+Dc=%f\n",
//			Da, Db, Dc, Db+Dc);

	float Da = spu_extract(_dx, 0);
	float Db = spu_extract(_dx, 1);
	float Dc = spu_extract(_dx, 2);

//	dSa = spu_extract(_dy, 0);
//	dSb = spu_extract(_dy, 1);
//	dSc = spu_extract(_dy, 2);

	float Sa = face_sum;
	float Sb = 0.0f;
	float Sc = 0.0f;

//	printf("new Da=%f, Db=%f, Dc=%f, Db+Dc=%f\n",
//			Da, Db, Dc, Db+Dc);

	printf("    dSa=%f, dSb=%f, dSc=%f\n",
			dSa, dSb, dSc);

//	printf("face_sum %f, Sa=%f, Sb=%f, Sc=%f\n",
//			face_sum, Sa, Sb, Sc);
//	printf("dSa=%f, dSb=%f, dSc=%f, dSb+dSc=%f, diff=%f\n\n",
//			dSa, dSb, dSc, dSb+dSc, face_sum-Sa);
//	printf("Da=%f, Db=%f, Dc=%f, Db+Dc=%f\n\n",
//			Da, Db, Dc, Db+Dc);

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
