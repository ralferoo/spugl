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

/*6*/int imp_draw_context(unsigned int* from, Context* context) {
	unsigned int id = *from;

	unsigned int eal = eal_renderables_table + sizeof(Renderable) * ( id&(MAX_RENDERABLES-1) );
	unsigned int eah = eah_buffer_tables;

	// printf("DRAW CTX %x -> %x:%08x\n", id, eah, eal);

	char buffer[256+128];
	char* tbuffer = (char*) ( ( ((unsigned int)buffer)+127 ) & ~127 );

	Renderable* renderable = (Renderable*) (tbuffer + (eal&127));
	unsigned int base_eal = eal & ~127;

	spu_mfcdma64(tbuffer, eah, base_eal, 256, 0, MFC_GET_CMD);		// read data
	mfc_write_tag_mask(1<<0);						// tag 0
	mfc_read_tag_status_all();						// wait for read
/*
	printf("Screen address: %llx, id %x, locks %d, size %dx%d, stride 0x%x, format %d, cache line %x:%08x\n",
		renderable->ea, renderable->id, renderable->locks,
		renderable->width, renderable->height, renderable->stride, renderable->format,
		mfc_ea2h(renderable->cacheLine), mfc_ea2l(renderable->cacheLine));
*/
	if (renderable->id == id ) {
		context->renderableCacheLine = renderable->cacheLine;
		context->width = renderable->width;
		context->height = renderable->height;
	} else {
		context->renderableCacheLine = 0;
		printf("renderable ID incorrect - %x not %x\n", id, renderable->id);
	}

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



//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Temporary stuff until i redo all the triangle generation stuff properly using AttribPointer etc
//

float4 current_colour = {.x=1.0,.y=1.0,.z=1.0,.w=1.0};
float4 current_texcoord = {.x=0.0,.y=0.0,.z=0.0,.w=1.0};
unsigned int current_texture = 0;

/*15*/int imp_glBegin(unsigned int* from) {
	unsigned int state = *from++;
	if (current_state >= 0) {
		// raise_error(ERROR_NESTED_GLBEGIN);
	}
	if (!imp_validate_state(state)) {
		// raise_error(ERROR_GLBEGIN_INVALID_STATE);
	} else {
		current_state = state;
	}
	return 0;
}

// if we're only support GLES, the only important case is lines and i think
// they should be handled seperately anyway
/*16*/int imp_glEnd(unsigned int* from, Context* context) {
	if (current_state < 0) {
		// raise_error(ERROR_GLEND_WITHOUT_GLBEGIN);
	} else {
		imp_close_segment(context);
	}
	current_state = -1;
	return 0;
}

/*17*/int imp_glVertex2(float* from, Context* context) {
	float4 a = {.x=from[0],.y=from[1],.z=0.0f,.w=1.0f};
	return imp_vertex(a, context);
}

/*18*/int imp_glVertex3(float* from, Context* context) {
	float4 a = {.x=from[0],.y=from[1],.z=from[2],.w=1.0f};
	return imp_vertex(a, context);
}

/*19*/int imp_glVertex4(float* from, Context* context) {
	float4 a = {.x=from[0],.y=from[1],.z=from[2],.w=from[3]};
	return imp_vertex(a, context);
}

/*20*/int imp_glColor3(float* from, Context* context) {
	float4 a = {.x=from[0],.y=from[1],.z=from[2],.w=1.0f};
	current_colour = a;
	return 0;
}

/*21*/int imp_glColor4(float* from, Context* context) {
	float4 a = {.x=from[0],.y=from[1],.z=from[2],.w=from[3]};
	current_colour = a;
	return 0;
}

/*22*/int imp_glTexCoord2(float* from, Context* context) {
	float4 a = {.x=from[0],.y=from[1],.z=0.0f,.w=1.0f};
	current_texcoord = a;
	return 0;
}

/*23*/int imp_glTexCoord3(float* from, Context* context) {
	float4 a = {.x=from[0],.y=from[1],.z=from[2],.w=1.0f};
	current_texcoord = a;
	return 0;
}

/*24*/int imp_glTexCoord4(float* from, Context* context) {
	float4 a = {.x=from[0],.y=from[1],.z=from[2],.w=from[3]};
	current_texcoord = a;
	return 0;
}

/*25*/int imp_old_glBindTexture(unsigned int* from, Context* context) {
	unsigned int target = *from++;
	unsigned int texture = *from++;
	current_texture = texture;
	return 0;
}


/*26*/int imp_glSync(float* from, Context* context) {
	return stillProcessingQueue(context);
}

/*27*/int imp_glFlush(float* from, Context* context, unsigned int id) {
	// construct the code fragment containing the stop isntruction
	vector unsigned int stopfunc = {
		0x00002112,				/* stop */
		(unsigned int) id,
		0x4020007f,				/* nop */
		0x35000000				/* bi $0 */
	};

	void (*f) (void) = (void *) &stopfunc;
	asm ("sync");
	f();
	return 0;
}

/*28*/int imp_ClearScreen(float* from, Context* context) {
	//printf("-----------\n");
	float4 a = {.x=from[0],.y=from[1],.z=from[2],.w=from[3]};
	return imp_clear_screen(a, context);
}

/*29*/int imp_SelectPixelShader(unsigned int* from, Context* context) {
	unsigned int bufid = *from++;
	unsigned int offset = *from++;
	unsigned int length = *from++;

	// reserve some memory
	volatile char buffer_[16+15];
	volatile char* buf_ptr=(volatile char*)( (((int)&buffer_)+15) & ~15 );

	// DMA the buffer start address
	unsigned id = bufid & BLOCK_ID_MASK;
	unsigned int eal_memptr = eal_buffer_memory_table + id*sizeof(long long);
	spu_mfcdma64(buf_ptr, eah_buffer_tables, eal_memptr & ~15, 16, 0, MFC_GET_CMD);	// tag 0
	mfc_write_tag_mask(1<<0);							// tag 0
	mfc_read_tag_status_all();							// wait for read

	// calculate the pointer to the EA and fetch it
	volatile long long* long_ptr = (volatile long long*) (buf_ptr + (eal_memptr&8) );
	long long ea = *long_ptr;

	// store the new data into the context
	context->pixel_shader_ea	= ea + offset;
	context->pixel_shader_length	= length;

#ifdef INFO
	printf("select shader %x [%llx] %x %x -> %llx/%d\n",
		bufid, ea, offset, length, context->pixel_shader_ea, context->pixel_shader_length);
#endif
	return 0;
}
