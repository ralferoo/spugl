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

#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/kd.h>
#include <linux/tiocl.h>
#include <sys/time.h>
#include <linux/fb.h>
#include <asm/ps3fb.h>
#include <time.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>

#include "../client/daemon.h"
#include "framebuffer.h"
#include "functions.h"

#define PS3FB_IOCTL_SETMODE          _IOW('r',  1, int)
#define PS3FB_IOCTL_GETMODE          _IOR('r',  2, int)

static struct __SPUGL_framebuffer screen = {
	.fd = -1
};
struct __SPUGL_framebuffer* __SPUGL_SCREEN = NULL;

static void switchMode(int gfxMode) {
	if (screen.console_fd >= 0) {
		if (gfxMode && screen.hasUnblankedScreen == 0) {
			ioctl(screen.console_fd, KDSETMODE, KD_GRAPHICS);
			ioctl(screen.console_fd, KDSETMODE, KD_TEXT);
			screen.hasUnblankedScreen = 1;
		}
		ioctl(screen.console_fd, KDSETMODE, gfxMode ? KD_GRAPHICS : KD_TEXT);
		ioctl(screen.fd, PS3FB_IOCTL_ON, 0);
	} else {
		printf("Warning: Cannot open /dev/console, so cannot change console mode.\n");
	}
}

int Screen_open(void)
{
	if (screen.fd >= 0)
		return 1;

	screen.fd = open("/dev/fb0", O_RDWR);
	if (screen.fd < 0)
		return 0;

	screen.console_fd = open("/dev/console", O_NONBLOCK);

	int vid = 0;

	if (ioctl(screen.fd, PS3FB_IOCTL_GETMODE, (unsigned long)&vid)<0)
		screen.mode = -1;
	else
		screen.mode = vid;

	struct ps3fb_ioctl_res res;
	if (ioctl(screen.fd, PS3FB_IOCTL_SCREENINFO, (unsigned long)(&res))==-1)	{
		close(screen.fd);
		screen.fd = -1;
		return 0;
	}

#ifdef INFO
	printf("Screen - %dx%d, border %dx%d, %d frames\n", res.xres, res.yres, res.xoff, res.yoff, res.num_frames);
#endif

	screen.width		= res.xres - 2*res.xoff;
	screen.height		= res.yres - 2*res.yoff;
	screen.stride		= res.xres * 4;
	screen.num_frames	= res.num_frames;
	screen.visible_frame	= (res.yoff * res.xres + res.xoff) * 4;
	screen.draw_frame	= screen.visible_frame + res.yres * res.xres * 4;
	screen.draw_size	= res.yres * res.xres * 4;
	screen.clear_size	= screen.height * screen.stride - 4*res.xoff;
	screen.mmap_size	= res.num_frames * screen.draw_size;
	screen.mmap_base	= mmap(0, screen.mmap_size, PROT_WRITE, MAP_SHARED, screen.fd, 0);
	screen.visible		= 0;
	screen.draw_address	= screen.mmap_base + screen.draw_frame;

	if (res.num_frames < 2) {
		screen.draw_frame = screen.visible_frame;
	}

	screen.renderable_id[1]	= blockManagementCreateRenderable(screen.mmap_base + screen.visible_frame,	screen.width, screen.height, screen.stride);
	screen.renderable_id[0]	= blockManagementCreateRenderable(screen.mmap_base + screen.draw_frame,	screen.width, screen.height, screen.stride);

	if (screen.mmap_base == NULL) {
		close(screen.fd);
		screen.fd = -1;
		return 0;
	}

#ifdef INFO
	printf("mmaped screen from %x to %x\n", screen.mmap_base, screen.mmap_base+screen.mmap_size);
#endif

	screen.hasUnblankedScreen = 0;
	switchMode(1);
	memset(screen.mmap_base, 0, screen.mmap_size);

	//for (int q=0; q<screen.mmap_size; q++)
	//	* ((char*)(screen.mmap_base+q)) = q * 0x10503;

	if (ioctl(screen.fd, PS3FB_IOCTL_ON, 0) < 0) {
		close(screen.fd);
		screen.fd = -1;
		return 0;
	}

	uint32_t showFrame = 0;
	ioctl(screen.fd, PS3FB_IOCTL_FSEL, (unsigned long)&showFrame);

	__SPUGL_SCREEN = &screen;
	return 1;
}

void Screen_close(void) {
	switchMode(0);

	if (screen.fd >= 0) {
		if (screen.mmap_base != NULL) {
			ioctl(screen.fd, PS3FB_IOCTL_OFF, 0);
			munmap(screen.mmap_base, screen.mmap_size);
		}
		close(screen.fd);
		screen.fd = -1;
	}
	if (screen.console_fd >= 0) {
		close(screen.console_fd);
		screen.console_fd = -1;
	}

	__SPUGL_SCREEN = NULL;
}

unsigned int Screen_swap(void)
{
	if (screen.fd < 0)
		return -1;


	uint32_t showFrame = screen.num_frames - screen.visible;
	screen.visible = showFrame;
	ioctl(screen.fd, PS3FB_IOCTL_FSEL, (unsigned long)&showFrame);

	unsigned int f = screen.draw_frame;
	screen.draw_frame = screen.visible_frame;
	screen.visible_frame = f;
	screen.draw_address = screen.mmap_base + screen.draw_frame;

//	memset(screen.mmap_base + screen.draw_frame, 0x40, screen.clear_size);

	return screen.renderable_id[showFrame];
}

void Screen_wait(void)
{
	if (screen.fd < 0)
		return;

	uint32_t crt = 0;
	ioctl(screen.fd, FBIO_WAITFORVSYNC, (unsigned long)&crt);
//	printf("vsync\n\n");
}

