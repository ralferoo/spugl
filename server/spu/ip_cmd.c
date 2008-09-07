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

/*0*/void* imp_nop(void* p) {
	printf("NOP\n");
	return 0;
}

/*1*/void* imp_jump(unsigned int *from, unsigned int id, unsigned int *ip) {
	*ip = *from;
	return 1;
}
