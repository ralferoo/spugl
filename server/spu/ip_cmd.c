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
#include "spucontext.h"
#include "../connection.h"

/*0*/int imp_nop(void* p) {
	printf("NOP\n");
	return 0;
}

/*1*/int imp_jump(unsigned int *from) {
	printf("JUMP %x\n", *from);
	return 1;
}

/*6*/int imp_draw_context(unsigned int* from) {
	unsigned int id = *from;

	unsigned int eal = eal_renderables_table + sizeof(Renderable) * ( id&(MAX_RENDERABLES-1) );
	unsigned int eah = eah_buffer_tables;

	printf("DRAW CTX %x -> %x:%08x\n", id, eah, eal);

	char buffer[256+128];
	char* tbuffer = (char*) ( ( ((unsigned int)buffer)+127 ) & ~127 );

	Renderable* renderable = (Renderable*) (tbuffer + (eal&127));
	unsigned int base_eal = eal & ~127;

	spu_mfcdma64(tbuffer, eah, base_eal, 256, 0, MFC_GET_CMD);		// read data
	mfc_write_tag_mask(1<<0);						// tag 0
	mfc_read_tag_status_all();						// wait for read

	printf("Screen address: %llx, id %x, locks %d, size %dx%d, stride %d, format %d\n",
		renderable->ea, renderable->id, renderable->locks,
		renderable->width, renderable->height, renderable->stride, renderable->format);

	return 0;
}
