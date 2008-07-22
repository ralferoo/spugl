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
#include "fifo.h"
#include "struct.h"
#include "primitives.h"
#include "queue.h"

#include <GL/gl.h>

void imp_close_segment(struct __TRIANGLE * triangle);

extern SPU_CONTROL control;
extern int current_state;
extern void* imp_vertex(void* from, float4 in, struct __TRIANGLE * triangle);
extern int imp_validate_state(int state);
extern int has_finished();

float4 current_colour = {.x=1.0,.y=1.0,.z=1.0,.w=1.0};
float4 current_texcoord = {.x=0.0,.y=0.0,.z=0.0,.w=1.0};
u32 current_texture = 0;

/*0*/void* imp_nop(void* p, struct __TRIANGLE * triangle) {
	return p;
}

/*1*/void* imp_jump(u32* from, struct __TRIANGLE * triangle) {
	u64 ea;
	__READ_EA(from)
	control.fifo_read = ea;
	return 0;
}

/*2*/void* impAddChild(u32* from, struct __TRIANGLE * triangle) {
	u64 ea;
	__READ_EA(from)
	printf("add child %llx\n", ea);
	return from;
}

/*3*/void* impDeleteChild(u32* from, struct __TRIANGLE * triangle) {
	u64 ea;
	__READ_EA(from)
	printf("delete child %llx\n", ea);
	return from;
}
	
/*15*/void* imp_glBegin(u32* from, struct __TRIANGLE * triangle) {
	u32 state = *from++;
	if (current_state >= 0) {
		raise_error(ERROR_NESTED_GLBEGIN);
	}
	if (!imp_validate_state(state)) {
		raise_error(ERROR_GLBEGIN_INVALID_STATE);
		return from;
	}
	current_state = state;
	return from;
}

// if we're only support GLES, the only important case is lines and i think
// they should be handled seperately anyway
/*16*/void* imp_glEnd(u32* from, struct __TRIANGLE * triangle) {
	if (current_state < 0) {
		raise_error(ERROR_GLEND_WITHOUT_GLBEGIN);
	} else {
		imp_close_segment(triangle);
	}
	current_state = -1;
	return from;
}

/*17*/void* imp_glVertex2(float* from, struct __TRIANGLE * triangle) {
	float4 a = {.x=from[0],.y=from[1],.z=0.0f,.w=1.0f};
	return imp_vertex(&from[2], a, triangle);
}
	
/*18*/void* imp_glVertex3(float* from, struct __TRIANGLE * triangle) {
	float4 a = {.x=from[0],.y=from[1],.z=from[2],.w=1.0f};
	return imp_vertex(&from[3], a, triangle);
}
	
/*19*/void* imp_glVertex4(float* from, struct __TRIANGLE * triangle) {
	float4 a = {.x=from[0],.y=from[1],.z=from[2],.w=from[3]};
	return imp_vertex(&from[4], a, triangle);
}

/*20*/void* imp_glColor3(float* from, struct __TRIANGLE * triangle) {
	float4 a = {.x=from[0],.y=from[1],.z=from[2],.w=1.0f};
	current_colour = a;
	return &from[3];
}
	
/*21*/void* imp_glColor4(float* from, struct __TRIANGLE * triangle) {
	float4 a = {.x=from[0],.y=from[1],.z=from[2],.w=from[3]};
	current_colour = a;
	return &from[4];
}

/*22*/void* imp_glTexCoord2(float* from, struct __TRIANGLE * triangle) {
	float4 a = {.x=from[0],.y=from[1],.z=0.0f,.w=1.0f};
	current_texcoord = a;
	return &from[2];
}
	
/*23*/void* imp_glTexCoord3(float* from, struct __TRIANGLE * triangle) {
	float4 a = {.x=from[0],.y=from[1],.z=from[2],.w=1.0f};
	current_texcoord = a;
	return &from[3];
}
	
/*24*/void* imp_glTexCoord4(float* from, struct __TRIANGLE * triangle) {
	float4 a = {.x=from[0],.y=from[1],.z=from[2],.w=from[3]};
	current_texcoord = a;
	return &from[4];
}

/*25*/void* imp_old_glBindTexture(u32* from, struct __TRIANGLE * triangle) {
	u32 target = *from++;
	u32 texture = *from++;
	current_texture = texture;
	return from;
}

extern void flush_queue();	

/*26*/void* imp_sync(u32* from, struct __TRIANGLE * triangle) {
	if (has_finished()) {
		flush_queue();		// force all blocks out
		return &from[0];
	} else {
		return 0;
	}
}

extern TextureDefinition textureDefinition[];
TextureDefinition* currentTexture = &textureDefinition[0];
int nextTextureDefinitionPtr = 0;

/*27*/void* imp_glBindTexture(u32* from, struct __TRIANGLE * triangle) {
	// this should probably be made into a cache style thing, but for now we just move onto the
	// next entry unless nobody has used the current bounding. this works as we always have
	// more texture slots than active triangles and handles multiple bindings between triangles.
	TextureDefinition* definition = currentTexture;
	if (definition->users) {
		definition = &textureDefinition[nextTextureDefinitionPtr];
		nextTextureDefinitionPtr = (nextTextureDefinitionPtr+1)%NUMBER_OF_TEXTURE_DEFINITIONS;
		currentTexture = definition;
	}
	definition->users = 0;

	vec_uchar16 lo = spu_splats((unsigned char)0);
	vec_uchar16 hi = spu_splats((unsigned char)0);

	u32 log2x = *from++;
	u32 log2y = *from++;

	definition->shifts = spu_splats((int)((log2x+(log2y<<16))-0x50005));
	definition->mipmapshifts = spu_splats((int)(log2x+log2y));

	u32 max_mipmap = *from++;
	definition->tex_max_mipmap = max_mipmap;

	for (int i=0; i<=max_mipmap; i++) {
		u64 ea;
		__READ_EA(from)
		definition->tex_pixel_base[i] = ea;
		definition->tex_t_blk_mult[i] = (unsigned short)(*from++);

		u32 tex_id_base = *from++;
		lo = spu_insert((unsigned char)tex_id_base, lo, i);
		hi = spu_insert((unsigned char)(tex_id_base>>8), hi, i);
	}
	definition->tex_base_lo = lo;
	definition->tex_base_hi = hi;

 	return from;
}
	
static u32 set_flag_value = 0;
/*28*/void* impSetFlag(u32* from, struct __TRIANGLE * triangle) {
	u64 ea;
	__READ_EA(from)
	set_flag_value = *from++;

	spu_mfcdma64(&set_flag_value, mfc_ea2h(ea), mfc_ea2l(ea), 4, FIFO_DMA_TAG, MFC_PUT_CMD);

	return from;
}
