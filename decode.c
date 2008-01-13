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
#include "primitives.h"

#include <GL/gl.h>

extern SPU_CONTROL control;

/*0*/void* imp_nop(void* p) {
//	printf("NOP\n");
	return p;
}

/*1*/void* imp_jump(u32* from) {
	u64 ea;
	__READ_EA(from)
//	printf("JMP %llx\n", ea);
	control.fifo_read = ea;
	return 0;
}

/*2*/void* impAddChild(u32* from) {
	u64 ea;
	__READ_EA(from)
	printf("add child %llx\n", ea);
	return from;
}

/*3*/void* impDeleteChild(u32* from) {
	u64 ea;
	__READ_EA(from)
	printf("delete child %llx\n", ea);
	return from;
}
	
_bitmap_image screen = { .address = 0};

/*4*/void* impScreenInfo(u32* from) {
	u64 ea;
	__READ_EA(from)
	screen.address = ea;
	screen.width = *from++;
	screen.height = *from++;
	screen.bytes_per_line = *from++;
	printf("screen at %llx, width %d, height %d, bpl %d\n",
		screen.address, screen.width, screen.height, screen.bytes_per_line);
	return from;
}
	
/*5*/void* impClearScreen(u32* from) {
	void* lb = malloc( (((screen.width<<2)+BYTE_ALIGNMENT)&~BYTE_ALIGNMENT) + BYTE_ALIGNMENT);
	u32* p = lb;
	int i;
	for (i=0; i<screen.width; i++)
		p[i] = 0;
	for (i=0; i<screen.height; i++) {
		u64 addr = screen.address + screen.bytes_per_line*i;
		mfc_putf(lb, addr, (((screen.width<<2)+BYTE_ALIGNMENT)&~BYTE_ALIGNMENT), 0, 0, 0);
		mfc_write_tag_mask(1);
		mfc_read_tag_status_any();
	}
	free(lb);
	return from;
}
	
#define SEL_A0 0,1,2,3,
#define SEL_A1 4,5,6,7,
#define SEL_A2 8,9,10,11,
#define SEL_A3 12,13,14,15,
#define SEL_B0 16,17,18,19,
#define SEL_B1 20,21,22,23,
#define SEL_B2 24,25,26,27,
#define SEL_B3 28,29,30,31,

/* A3 is preserved as the initial state if we need to loop */
static vec_uchar16 shuffles[] = {
{ SEL_B0 SEL_B0 SEL_B0 SEL_B0 }, /* 0 = fill all elements with new */
{ SEL_A1 SEL_B0 SEL_B0 SEL_A3 }, /* 1 = add line vertex */
{ SEL_A1 SEL_A3 SEL_B0 SEL_A3 }, /* 2 = line loop finished */
{ SEL_A1 SEL_A2 SEL_B0 SEL_A3 }, /* 3 = add triangle vertex */
{ SEL_A2 SEL_A1 SEL_B0 SEL_A3 }, /* 4 = add triangle strip vertex 4th */
{ SEL_A0 SEL_A2 SEL_B0 SEL_A3 }, /* 5 = add triangle fan vertex 4th */
{ SEL_A1 SEL_A2 SEL_A3 SEL_B0 }, /* 6 = add quad vertex */
{ SEL_A0 SEL_A2 SEL_B0 SEL_A3 }, /* 7 = add polygon vertex */
{ SEL_A0 SEL_A1 SEL_A3 SEL_A3 }, /* 8 = polygon vertex finished */

{ SEL_A0 SEL_A1 SEL_A2 SEL_A3 }, /* 9 = polygon finished */
{ SEL_A0 SEL_A2 SEL_A3 SEL_A3 }, /* 10 = quad finished */
{ SEL_A2 SEL_A1 SEL_A3 SEL_A3 }, /* 11 = quad strip finished */
};

static struct {
	unsigned char next;
	unsigned char insert;
	unsigned char end;
	unsigned char add;
} shuffle_map[] = {
/*0 GL_POINTS          */
	{ .insert = 0, .next = 0 },
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
	{ .insert = 1, .next = 1, .add = 1 },
/* 11 line loop second point */
	{ .insert = 1, .next = 11, .add = 1, .end = 2},
/* 12 line strip second point */
	{ .insert = 1, .next = 12, .add = 1 },

/* 13 triangle second point */
	{ .insert = 3, .next = 14 },
/* 14 triangle third point */
	{ .insert = 3, .next = 4, .add = 2 },

/* 15 triangle strip second point */
	{ .insert = 3, .next = 16 },
/* 16 triangle strip third point */
	{ .insert = 3, .next = 17, .add = 2 },
/* 17 triangle strip fourth point */
	{ .insert = 4, .next = 16, .add = 2 },

/* 18 triangle fan second point */
	{ .insert = 3, .next = 19 },
/* 19 triangle fan third point */
	{ .insert = 3, .next = 20, .add = 2 },
/* 20 triangle fan fourth point */
	{ .insert = 5, .next = 20, .add = 2 },

/* 21 quad second point */
	{ .insert = 6, .next = 22 },
/* 22 quad third point */
	{ .insert = 6, .next = 23 },
/* 23 quad fourth point */
	{ .insert = 6, .next = 7, .add = 3 },

/* 24 quad strip second point */
	{ .insert = 6, .next = 22 },
/* 25 quad strip third point */
	{ .insert = 6, .next = 26 },
/* 26 quad strip fourth point */
	{ .insert = 6, .next = 27, .add = 4 },
/* 26 quad strip fifth point */
	{ .insert = 6, .next = 25 },

/* 27 polygon second point */
	{ .insert = 3, .next = 28 },
/* 28 polygon third point */
	{ .insert = 3, .next = 4, .add = 2 },
/* 29 polygon fourth point */
	{ .insert = 7, .next = 29, .add = 2, .end = 8 },
};

static int current_state = -1;
/*15*/void* imp_glBegin(u32* from) {
	u32 state = *from++;
	if (current_state >= 0) {
		printf("ERROR: nested glBegin()\n");
	}
//	printf("glBegin(%d)\n", state);
	current_state = state;
	return from;
}
	
/*16*/void* imp_glEnd(u32* from) {
	if (current_state < 0) {
		printf("ERROR: glEnd() without glBegin()\n");
	} else {
		int end = shuffle_map[current_state].end;
		if (end) {
			TRIorder = shuffles[end];
			switch (shuffle_map[current_state].add) {
				case 1:
					imp_line();
					break;
				case 2:
					imp_triangle();
					break;
			}
		}
	}
	current_state = -1;
	return from;
}

static float4 current_colour = {.x=1.0,.y=1.0,.z=1.0,.w=1.0};

extern void debug_state(char*s, int i);
static void inline imp_update_vertex(float4 in, vec_uchar16 inserter)
{
	// just for testing, have hard-coded persective and screen
	// transformations here. they'll probably live here anyway, just
	// done with matrices.
//	float4 p = {.x=in.x - screen.width/2, .y = in.y - screen.height/2, .z = in.z, .w = 420/(in.z-420)};
//	float recip = 1.0/p.w;
//	float4 s = {.x=p.x*recip, .y = p.y*recip, .z = p.z*recip, .w = recip};

	float recip = 420.0 / (in.z-420.0);
	float4 s = {.x=in.x*recip+screen.width/2, .y = in.y*recip+screen.height/2, .z = in.z*recip, .w = recip};

	float4 c= current_colour;
	float4 col = {.x=c.x*recip, .y = c.y*recip, .z = c.z*recip, .w = c.w*recip};
/*
	vertex_state v = {
		.coords = s,
		.colour = col,
	};
	v.coords = s;
	imp_vertex_state(v);
*/
//	printf("tran (%.2f,%.2f,%.2f,%.2f)\n", s.x, s.y, s.z, s.w);
//	printf("col  (%.2f,%.2f,%.2f,%.2f)\n", col.x, col.y, col.z, col.w);

	TRIorder = shuffles[9];
//	debug_state("previous state", 4);

	TRIx = spu_shuffle(TRIx, (vec_float4) s.x, inserter);
	TRIy = spu_shuffle(TRIy, (vec_float4) s.y, inserter);
	TRIz = spu_shuffle(TRIz, (vec_float4) s.z, inserter);
	TRIw = spu_shuffle(TRIw, (vec_float4) s.w, inserter);
	TRIr = spu_shuffle(TRIr, (vec_float4) col.x, inserter);
	TRIg = spu_shuffle(TRIg, (vec_float4) col.y, inserter);
	TRIb = spu_shuffle(TRIb, (vec_float4) col.z, inserter);
	TRIa = spu_shuffle(TRIa, (vec_float4) col.w, inserter);
//	TRIu = spu_shuffle(TRIu, (vec_float4) in.u, inserter);
//	TRIv = spu_shuffle(TRIv, (vec_float4) in.v, inserter);

	TRIorder = shuffles[9];
//	debug_state("current state", 4);
}

static void* imp_vertex(void* from, float4 in)
{
//	printf("\nvert (%.2f,%.2f,%.2f,%.2f)\n", in.x, in.y, in.z, in.w);
	if (current_state < 0 ||
		  current_state >= sizeof(shuffle_map)/sizeof(shuffle_map[0])) {
		printf("imp_vertex with invalid current_state value %d\n",
			current_state);
		return from;
	}
	printf("state %2d: insert=%2d next=%2d add=%d end=%2d\n",
		current_state,
		shuffle_map[current_state].insert,
		shuffle_map[current_state].next,
		shuffle_map[current_state].add,
		shuffle_map[current_state].end);

	int ins = shuffle_map[current_state].insert;
	if (ins >= sizeof(shuffles)/sizeof(shuffles[0])) {
		printf("imp_vertex with invalid shuffle value %d\n", ins);
		return from;
	}
	vec_uchar16 inserter = shuffles[ins];

	imp_update_vertex(in, inserter);

	// check to see if we need to draw
	TRIorder = shuffles[9];
	switch (shuffle_map[current_state].add) {
		case 1:
			imp_line();
			break;
		case 2:
			imp_triangle();
			break;
		case 3:
			imp_triangle();
			TRIorder = shuffles[10];
			imp_triangle();
			break;
		case 4:
			imp_triangle();
			TRIorder = shuffles[11];
			imp_triangle();
			break;
	}
	current_state = shuffle_map[current_state].next;

	return from;
}
	
/*17*/void* imp_glVertex2(float* from) {
	float4 a = {.x=from[0],.y=from[1],.z=0.0f,.w=1.0f};
	return imp_vertex(&from[2], a);
}
	
/*18*/void* imp_glVertex3(float* from) {
	float4 a = {.x=from[0],.y=from[1],.z=from[2],.w=1.0f};
	return imp_vertex(&from[3], a);
}
	
/*19*/void* imp_glVertex4(float* from) {
	float4 a = {.x=from[0],.y=from[1],.z=from[2],.w=from[3]};
	return imp_vertex(&from[4], a);
}

/*20*/void* imp_glColor3(float* col) {
	float4 a = {.x=col[0],.y=col[1],.z=col[2],.w=1.0f};
	current_colour = a;
	return &col[3];
}
	
/*21*/void* imp_glColor4(float* col) {
	float4 a = {.x=col[0],.y=col[1],.z=col[2],.w=col[3]};
	current_colour = a;
	return &col[4];
}


// just testing some stuff with gcc...
// this is really cool, you have have global registers ;)

u32 getNext(void)
{
return ___foo++;
}


