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

#include <spu_mfcio.h>

#include "spucontext.h"

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
