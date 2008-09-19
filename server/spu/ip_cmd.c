/****************************************************************************
 *
 * SPU GL - 3d rasterisation library for the PS3
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net>
 *
 * This library may not be used or distributed without a licence, please
 * contact me for information if you wish to use it.
 *
 ****************************************************************************/

#include <stdio.h>

/*0*/int imp_nop(void* p) {
	printf("NOP\n");
	return 0;
}

/*1*/int imp_jump(unsigned int *from) {
	printf("JUMP %x\n", *from);
	return 1;
}

/*6*/int imp_draw_context(unsigned int* from) {
	printf("DRAW CTX %x\n", *from);
	return 0;
}
