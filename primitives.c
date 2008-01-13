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

vertex_state pull_compat(int j)
{
	vec_uchar16 sel = TRIorder;
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

void imp_triangle()
{
//	debug_state("tri\t",3);

	vertex_state a = pull_compat(0);
	vertex_state b = pull_compat(1);
	vertex_state c = pull_compat(2);

	debug(a);
	debug(b);
	debug(c);
	
	TRIANGLE_SPAN_FUNCTION* func = triangleSpan;

	float abx = b.coords.x - a.coords.x;
	float aby = b.coords.y - a.coords.y;
	float bcx = c.coords.x - b.coords.x;
	float bcy = c.coords.y - b.coords.y;
	float cax = a.coords.x - c.coords.x;
	float cay = a.coords.y - c.coords.y;

	if (a.coords.x * bcy + b.coords.x * cay + c.coords.x * aby > 0) {
		// z is negative, so +ve means counter-clockwise and so not vis
		printf("Back-facing triangle...\n");
		return;
	}

	int qab = aby<0;
	int qbc = bcy<0;
	int qca = cay<0;

	int bc_a_ca = qbc && qca;
	int bc_o_ca = qbc || qca;

	int left;
	vertex_state temp;
	if (qab && qbc) {
		left = 1;
		temp = c; c = b; b = a; a = temp;
	}
	if (qab && qca) {
		left = 1;
		temp = a; a = b; b = c; c = temp;
	}
	if (qab && !bc_o_ca) {
		left = 0;
		temp = a; a = b; b = c; c = temp;
	}
	if (!(qab || qbc)) {
		left = 0;
	}
	if (!(qab || qca)) {
		left = 0;
		temp = c; c = b; b = a; a = temp;
	}
	if (!qab && bc_a_ca) {
		left = 1;
	}

	if (b.coords.y < a.coords.y || c.coords.y < a.coords.y) {
		printf (":ERROR: a not least\n");
		return;
	}
	if (left && b.coords.y < c.coords.y) {
		printf (":ERROR: left and b<c\n");
		return;
	}
	if (!left && c.coords.y < b.coords.y) {
		printf (":ERROR: right and c<b\n");
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

