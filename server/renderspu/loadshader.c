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
#include <spu_intrinsics.h>
#include <stdio.h>

#include "render.h"
#include "shader.h"

// buffer for shader
char		shader_buffer[65536] __attribute__((aligned(128)));
vec_uint4*	shader_scratch;
unsigned long	shader_ea_current = 0;
 
void null_render_func(vec_uint4* params, vec_uint4* scratch, vec_int4 A, vec_int4 hdx, vec_int4 hdy, vec_uint4* pixelbuffer) {}

void clearInitFunc(vec_uint4* params, vec_uint4* scratch, vec_int4 hdx, vec_int4 hdy);
void clearRenderFunc(vec_uint4* params, vec_uint4* scratch, vec_int4 A, vec_int4 hdx, vec_int4 hdy, vec_uint4* pixelbuffer);

PixelShaderRenderFunc load_shader(Triangle* triangle, vec_uint4* params, vec_int4 hdx, vec_int4 hdy)
{
	unsigned long long	shader_ea	= triangle->shader_ea;
	unsigned int		shader_length	= ( triangle->shader_length + 127) & ~127;

	if (shader_ea != shader_ea_current) {
		shader_scratch = (vec_uint4*) (shader_buffer+shader_length);
		shader_ea_current = shader_ea;
		if (shader_ea) {
			spu_mfcdma64(shader_buffer, mfc_ea2h(shader_ea), mfc_ea2l(shader_ea), shader_length, 0, MFC_GET_CMD);	// tag 0
			mfc_write_tag_mask(1<<0);							// tag 0
			mfc_read_tag_status_all();							// wait for read
		}
	}

	PixelShaderRenderFunc	shader_func;
 
	PixelShaderHeader* header = (PixelShaderHeader*) shader_buffer;
	if (shader_ea && header->magic == PixelShaderHeaderMagic) {
		if (header->initFunctionOffset) {
			PixelShaderInitFunc init = (PixelShaderInitFunc) (shader_buffer + header->initFunctionOffset);
			//printf("Calling init func %x %x %x\n", init, params, shader_scratch);
			init(params, shader_scratch, hdx, hdy);
		}
		shader_func = (PixelShaderRenderFunc) (shader_buffer + header->renderFunctionOffset);
	} else {
		if (shader_length) {
			shader_func = &clearRenderFunc;
			clearInitFunc(params, shader_scratch, hdx, hdy);
		} else {
			shader_func = &null_render_func;
		}
	}

	return shader_func;
}
