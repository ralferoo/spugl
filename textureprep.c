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

#include <stdlib.h>
#include <stdio.h>

#include "struct.h"

u32* prepare_texture(gimp_image* source);


Texture convertGimpTexture(gimp_image* source) {
	Texture tex = malloc(sizeof(_texture));
	if (tex) {
		static unsigned int data[4] = {0};	// some weird alignment stuff here
		tex->tex_x_y_shift = 8-5;
		tex->tex_max_mipmap = 0;
		tex->tex_mipmap_shift = 8+8; // log2(w)+log2(h)

		tex->tex_id_base[0] = data[0]; data[0]+=64; //_next_id; // next_base; next_base += 64;	//texture<<(6+3); // +3 = 8 levels of mipmap
		tex->tex_data[0] = prepare_texture(source);
		tex->tex_t_mult[0] = (8*32+1)*32*4;
	}

	return tex;
}

u32* prepare_texture(gimp_image* source)
{
	int width = (source->width+31)&~31;
	int height = (source->height+31)&~31;
	int mainsize = width*(height+1)*4;

	int extrasize = (width/32)*height*4;

	void* buffer = malloc(mainsize+extrasize+127);
	u32* pixels = (u32*) ((((unsigned int)buffer)+127)&~127);

	int bx,by,x,y;

	u32* p = pixels;

	for (bx=0; bx<width; bx+=32) {
		for (by=0; by<height; by+=32) {
			for (y=0;y<32;y++) {
				for (x=0;x<32;x++) {
					u32* s = (u32*) (source->pixel_data + 
							((by+y)*source->width + (bx+x))*4);
					u32 rgba = *s;
					u32 argb = ((rgba&0xff)<<24) | ((rgba>>8)&0xffffff);
					*p++ = argb;
				}
			}
		}
		for (x=0;x<32;x++) {
			u32* s = (u32*) (source->pixel_data + 
					((source->height-1)*source->width + (bx+x))*4);	// clamp
//			u32* s = (u32*) (source->pixel_data + (bx+x)*4);	// repeat top
			u32 rgba = *s;
			u32 argb = ((rgba&0xff)<<24) | ((rgba>>8)&0xffffff);
			*p++ = argb;
		}
	}

	for (bx=0; bx<width; bx+=32) {
		for (by=0; by<height; by+=32) {
			for (y=0;y<32;y++) {
				u32* s = (u32*) (source->pixel_data + 
						((by+y)*source->width + (bx+32))*4);
				u32 rgba = *s;
				u32 argb = ((rgba&0xff)<<24) | ((rgba>>8)&0xffffff);
				*p++ = argb;
			}
		}
	}

	return pixels;
}
