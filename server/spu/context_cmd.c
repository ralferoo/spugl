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
#include <spu_mfcio.h>

#include "spucontext.h"
#include "../connection.h"

/*2*/int imp_load_modelview_matrix(float* from, Context* context) {
        vec_float4 x = { from[ 0], from[ 1], from[ 2], from[ 3] };
        vec_float4 y = { from[ 4], from[ 5], from[ 6], from[ 7] };
        vec_float4 z = { from[ 8], from[ 9], from[10], from[11] };
        vec_float4 w = { from[12], from[13], from[14], from[15] };

	context->modelview_matrix_x = x;
	context->modelview_matrix_y = y;
	context->modelview_matrix_z = z;
	context->modelview_matrix_w = w;

	return 0;
}

/*3*/int imp_load_projection_matrix(float* from, Context* context) {
        vec_float4 x = { from[ 0], from[ 1], from[ 2], from[ 3] };
        vec_float4 y = { from[ 4], from[ 5], from[ 6], from[ 7] };
        vec_float4 z = { from[ 8], from[ 9], from[10], from[11] };
        vec_float4 w = { from[12], from[13], from[14], from[15] };

	context->projection_matrix_x = x;
	context->projection_matrix_y = y;
	context->projection_matrix_z = z;
	context->projection_matrix_w = w;

	return 0;
}

/*4*/int imp_load_texture_matrix(float* from, Context* context) {
        vec_float4 x = { from[ 0], from[ 1], from[ 2], from[ 3] };
        vec_float4 y = { from[ 4], from[ 5], from[ 6], from[ 7] };
        vec_float4 z = { from[ 8], from[ 9], from[10], from[11] };
        vec_float4 w = { from[12], from[13], from[14], from[15] };

	context->texture_matrix_x = x;
	context->texture_matrix_y = y;
	context->texture_matrix_z = z;
	context->texture_matrix_w = w;

	return 0;
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

	printf("Screen address: %llx, id %x, locks %d, size %dx%d, stride 0x%x, format %d\n",
		renderable->ea, renderable->id, renderable->locks,
		renderable->width, renderable->height, renderable->stride, renderable->format);

	return 0;
}

/*7*/int imp_load_vertex_attrib_1(float* from, Context* context) {
	unsigned int which = *( (unsigned int*) from );
        vec_float4 x = { from[ 1], 0.0f, 0.0f, 1.0f };
	if (which<NUM_FRAGMENT_UNIFORM_VECTORS)
		context->fragment_uniform_vectors[which] = x;
	return 0;
}

/*8*/int imp_load_vertex_attrib_2(float* from, Context* context) {
	unsigned int which = *( (unsigned int*) from );
        vec_float4 x = { from[ 1], from[ 2], 0.0f, 1.0f };
	if (which<NUM_FRAGMENT_UNIFORM_VECTORS)
		context->fragment_uniform_vectors[which] = x;
	return 0;
}

/*9*/int imp_load_vertex_attrib_3(float* from, Context* context) {
	unsigned int which = *( (unsigned int*) from );
        vec_float4 x = { from[ 1], from[ 2], from[ 3], 1.0f };
	if (which<NUM_FRAGMENT_UNIFORM_VECTORS)
		context->fragment_uniform_vectors[which] = x;
	return 0;
}

/*10*/int imp_load_vertex_attrib_4(float* from, Context* context) {
	unsigned int which = *( (unsigned int*) from );
        vec_float4 x = { from[ 1], from[ 2], from[ 3], from[ 4] };
	if (which<NUM_FRAGMENT_UNIFORM_VECTORS)
		context->fragment_uniform_vectors[which] = x;
	return 0;
}


