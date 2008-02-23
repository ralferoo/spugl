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

#include "types.h"

u32* prepare_texture(gimp_image* source)
{
	int width = (source->width+31)&~31;
	int height = (source->height+31)&~31;
	int size = width*height*4;

	void* buffer = malloc(size+127);
	u32* pixels = (u32*) ((((unsigned int)buffer)+127)&~127);

	u32* t=pixels;
	int i;
	for (i=0;i<size;i+=4)
		*t++ = i;

	int bx,by,x,y;

	u32* p = pixels;
	for (by=0; by<height; by+=32) {
		for (bx=0; bx<width; bx+=32) {
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
	return pixels;
}
