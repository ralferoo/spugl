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
#include <GL/glspu.h>

u32* prepare_texture(gimp_image* source);

static int log2(int in) {
       int i=-1;
       int x=in;
       while (x) {
               i++;
               x>>=1;
       }
       if (in-(1<<i)) i++;
       return i;
}


Texture convertGimpTexture(gimp_image* source) {
	Texture tex = malloc(sizeof(_texture));
	if (tex) {
		tex->tex_max_mipmap = 0;

		int width = (source->width+31)&~31;
		int height = (source->height+31)&~31;
		int mainsize = width*height*4;

		void* buffer = malloc(mainsize+127);
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
		}
	
		static unsigned int data[4] = {0};	// some weird alignment stuff here
		tex->tex_id_base[0] = data[0]; data[0]+=(width*height)<<10;

		tex->tex_data[0] = pixels;
		tex->tex_t_mult[0] = (height)*32*4;
		tex->tex_width = width;
		tex->tex_height = height;
		tex->tex_log2_x = log2(width);
		tex->tex_log2_y = log2(height);

		//calculateMipmap(pixels, pixels, pixels, pixels, pixels);
		for (int a=0; a<width/32*height/32; a++) {
			void* p = pixels + a*32*32;
			calculateMipmap(p, p, p, p, p);
		}
		// fake later mipmaps
		for (int i=1; i<7; i++) {
			width = (width/2 + 31)&~31; 	
			height = (width/2 + 31)&~31; 	
			tex->tex_id_base[i] = tex->tex_id_base[0];
			tex->tex_data[i] = tex->tex_data[0];
			tex->tex_t_mult[i] = tex->tex_t_mult[0];
			tex->tex_max_mipmap = i;
		}

	}

	return tex;
}
