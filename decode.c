/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

#include <spu_mfcio.h>
#include "3d.h"

extern SPU_CONTROL control;

/*1*/void* imp_nop(void* p) {
	printf("NOP\n");
	return p;
}

/*2*/void* imp_jump(u32* from) {
	u64 ea;
	__READ_EA(from)
	printf("JMP %llx\n", ea);
	control.fifo_read = ea;
	return 0;
}

/*3*/void* impAddChild(u32* from) {
	u64 ea;
	__READ_EA(from)
	printf("add child %llx\n", ea);
	return from;
}

/*4*/void* impDeleteChild(u32* from) {
	u64 ea;
	__READ_EA(from)
	printf("delete child %llx\n", ea);
	return from;
}
	
#define STATE_NONE 0x16375923
static u32 begin_end_state = STATE_NONE;
static u32 vertex_count = 0;
void imp_vertex(float x, float y, float z, float w);

/*5*/void* imp_glBegin(u32* from) {
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
	
/*6*/void* imp_glEnd(u32* from) {
	if (begin_end_state != STATE_NONE) {
		begin_end_state = STATE_NONE;
		//printf("glEnd()\n");
	} else {
		printf("ERROR: glEnd() without glBegin()\n");
	}
	return from;
}
	
/*7*/void* imp_glVertex2(float* from) {
	imp_vertex(*from++,*from++,0.0,1.0);
	return from;
}
	
/*8*/void* imp_glVertex3(float* from) {
	imp_vertex(*from++,*from++,*from++,1.0);
	return from;
}
	
/*9*/void* imp_glVertex4(float* from) {
	imp_vertex(*from++,*from++,*from++,*from++);
	return from;
}

void imp_vertex(float x, float y, float z, float w)
{
	vertex_count++;
	printf("vertex %d (%f,%f,%f,%f)\n", vertex_count, x,y,z,w);
}

