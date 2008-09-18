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

#ifndef __FRAMEBUFFER_H
#define __FRAMEBUFFER_H

struct __SPUGL_framebuffer {
	unsigned int	width, height;
	unsigned int	stride;
	unsigned int	frame[2];		// offsets from base
	unsigned int	mode;
	void*		mmap_base;
	unsigned int	mmap_size;
	int		fd;
	int		hasUnblankedScreen;
};

extern int Screen_open(void);
extern void Screen_close(void);
extern int Screen_flip(int frame);
extern void Screen_wait(void);

extern struct __SPUGL_framebuffer* __SPUGL_SCREEN;

#endif // __FRAMEBUFFER_H

