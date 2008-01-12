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

void* impNOP(void* p) {
	printf("NOP\n");
	return p;
}

void* impJMP(u32* from) {
	u64 ea;
	__READ_EA(from)
	printf("JMP %llx\n", ea);
	control.fifo_read = ea;
	return 0;
}

void* impAddChild(u32* from) {
	u64 ea;
	__READ_EA(from)
	printf("add child %llx\n", ea);
	return from;
}

void* impDeleteChild(u32* from) {
	u64 ea;
	__READ_EA(from)
	printf("delete child %llx\n", ea);
	return from;
}
	
void* imp_glBegin(u32* from) {
	printf("glBegin(%d)\n", *from++);
	return from;
}
	
void* imp_glEnd(u32* from) {
	printf("glEnd()\n");
	return from;
}
	
void* imp_glVertex3f(u32* from) {
	float x,y,z;
	union __f2i temp;
	temp.i = *from++; x=temp.f;
	temp.i = *from++; y=temp.f;
	temp.i = *from++; z=temp.f;
	printf("glVertex3f(%f,%f,%f)\n", x,y,z);
	return from;
}
