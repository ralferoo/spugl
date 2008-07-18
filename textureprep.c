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

static int log2(int in) {
	int i=0;
	int x=in;
	while (x) {
		i++;
		x>>=1;
	}
	if (in-(1<<i)) i++;
	return i;
}

static unsigned int data[4] = {0};	// some weird alignment stuff here

void generateMipMaps(Texture tex) {
	// fake later mipmaps
	
	int width = tex->tex_width;
	int height = tex->tex_height;

	for (int i=1; i<7; i++) {
		width = ((width>>1)+31)&~31;
		height = ((height>>1)+31)&~31;
		int mainsize = width*(height+1)*4;
		int extrasize = (width/32)*height*4;
		void* buffer = malloc(mainsize+extrasize+127);
		u32* pixels = (u32*) ((((unsigned int)buffer)+127)&~127);
	
		tex->tex_id_base[i] = data[0];
		data[0]+=(width>>5)*(height>>5);
		tex->tex_t_mult[i] = (height+1)*32*4;

		tex->tex_data[i] = pixels;
		tex->tex_max_mipmap = i;

		printf("generating mipmap %d for %dx%d: %x base %x mult %x\n", i, width, height, tex->tex_data[i], tex->tex_id_base[i], tex->tex_t_mult[i]);
	}
}

Texture convertGimpTexture(gimp_image* source) {
	Texture tex = malloc(sizeof(_texture));
	if (tex) {
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
	
		tex->tex_width = width;
		tex->tex_height = height;
		tex->tex_log2_x = log2(width);
		tex->tex_log2_y = log2(height);
		tex->tex_max_mipmap = 0;

		// generate next texture block base id
		tex->tex_id_base[0] = data[0];
		data[0]+=(width>>5)*(height>>5);

		tex->tex_t_mult[0] = (height+1)*32*4;
		tex->tex_data[0] = pixels;

		for (int i=1; i<7; i++) {
//			tex->tex_id_base[i] = 0;
//			tex->tex_data[i] = NULL;
//			tex->tex_t_mult[i] = 0;

			tex->tex_id_base[i] = tex->tex_id_base[0];
			tex->tex_data[i] = tex->tex_data[0];
			tex->tex_t_mult[i] = tex->tex_t_mult[0];
			tex->tex_max_mipmap = i;

		}

//		generateMipMaps(tex);
	} // tex!=NULL

	return tex;
}
