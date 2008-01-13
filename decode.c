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

#include <GL/gl.h>

extern SPU_CONTROL control;

/*0*/void* imp_nop(void* p) {
	printf("NOP\n");
	return p;
}

/*1*/void* imp_jump(u32* from) {
	u64 ea;
	__READ_EA(from)
	printf("JMP %llx\n", ea);
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
	return from;
}
	
#define STATE_NONE 0x16375923
static u32 begin_end_state = STATE_NONE;
static u32 vertex_count = 0;

/*15*/void* imp_glBegin(u32* from) {
	u32 state = *from++;
	if (begin_end_state == STATE_NONE) {
		begin_end_state = state;
		vertex_count = 0;
		//printf("glBegin(%d)\n", state);
	} else {
		printf("ERROR: nested glBegin()\n");
	}
	return from;
}

static vertex_state current_state = {
	.colour = {.x=1.0,.y=1.0,.z=1.0,.w=1.0}
};
static vertex_state previous[4];

static void imp_vertex_state(vertex_state v)
{
	switch(begin_end_state) {
		case GL_POINTS:
			imp_point(v);
			break;
		case GL_LINES:
			if (vertex_count++) {
				imp_line(previous[0], v);
				vertex_count = 0;
			} else {
				previous[0] = v;
			}
			break;
		case GL_LINE_STRIP:
			if (vertex_count++) {
				imp_line(previous[0], v);
			}
			previous[0] = v;
			break;
		case GL_LINE_LOOP:
			if (vertex_count++) {
				imp_line(previous[1], v);
			} else {
				previous[0] = v;
			}
			previous[1] = v;
			break;
		case GL_TRIANGLES:
			if (vertex_count == 2) {
				imp_triangle(previous[0], previous[1], v);
				vertex_count = 0;
			} else {
				previous[vertex_count++] = v;
			}
			break;
		case GL_TRIANGLE_STRIP:
			if (vertex_count == 2) {
				imp_triangle(previous[0], previous[1], v);
				previous[0] = previous[2];
			} else {
				previous[vertex_count++] = v;
			}
			break;
		case GL_POLYGON:
		case GL_TRIANGLE_FAN:
			if (vertex_count == 2) {
				imp_triangle(previous[0], previous[1], v);
				previous[1] = previous[2];
			} else {
				previous[vertex_count++] = v;
			}
			break;
		case GL_QUADS:
			if (vertex_count == 3) {
				imp_triangle(previous[0], previous[1], v);
				imp_triangle(v, previous[1], previous[2]);
				vertex_count = 0;
			} else {
				previous[vertex_count++] = v;
			}
			break;
		case GL_QUAD_STRIP:
			if (vertex_count == 3) {
				imp_triangle(previous[0], previous[1], previous[3]);
				imp_triangle( previous[2], previous[1], v);
				previous[0] = previous[2];
				previous[1] = v;
				vertex_count = 2;
			} else {
				previous[vertex_count++] = v;
			}
			break;
		default:
			printf("ERROR: glVertex*() following invalid glBegin()\n");
			break;
	}
}
	
/*16*/void* imp_glEnd(u32* from) {
	switch(begin_end_state) {
		case STATE_NONE:
			printf("ERROR: glEnd() without glBegin()\n");
			break;
		case GL_POLYGON:
		case GL_LINE_LOOP:
			imp_vertex_state(previous[0]);
			break;
	}
	begin_end_state = STATE_NONE;
	return from;
}
static void imp_vertex(float4 in)
{
	// just for testing, have hard-coded persective and screen
	// transformations here. they'll probably live here anyway, just
	// done with matrices.
	float4 p = {.x=in.x - screen.width/2, .y = in.y - screen.height/2, .z = in.z, .w = 420/(in.z-420)};
	float recip = 1.0/p.w;
	float4 s = {.x=p.x*recip, .y = p.y*recip, .z = p.z*recip, .w = recip};

	float4 c= current_state.colour;
	float4 col = {.x=c.x*recip, .y = c.y*recip, .z = c.z*recip, .w = c.w*recip};
	vertex_state v = {
		.coords = s,
		.colour = col,
	};
	v.coords = s;
	imp_vertex_state(v);
}
	
/*17*/void* imp_glVertex2(float* from) {
	float4 a = {.x=from[0],.y=from[1],.z=0.0,.w=1.0};
	imp_vertex(a);
	return &from[2];
}
	
/*18*/void* imp_glVertex3(float* from) {
	float4 a = {.x=from[0],.y=from[1],.z=from[2],.w=1.0};
	imp_vertex(a);
	return &from[3];
}
	
/*19*/void* imp_glVertex4(float* from) {
	float4 a = {.x=from[0],.y=from[1],.z=from[2],.w=from[3]};
	imp_vertex(a);
	return &from[4];
}

/*20*/void* imp_glColor3(float* col) {
	float4 a = {.x=col[0],.y=col[1],.z=col[2],.w=1.0};
	current_state.colour = a;
	return &col[3];
}
	
/*21*/void* imp_glColor4(float* col) {
	float4 a = {.x=col[0],.y=col[1],.z=col[2],.w=col[3]};
	current_state.colour = a;
	return &col[4];
}


