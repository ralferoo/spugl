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

#define PS3FB_IOCTL_SETMODE          _IOW('r',  1, int)
#define PS3FB_IOCTL_GETMODE          _IOR('r',  2, int)

static struct __SPUGL_framebuffer screen = {
	.fd = -1
};
struct __SPUGL_framebuffer* __SPUGL_SCREEN = NULL;

static void switchMode(int gfxMode) {
	int fd = open("/dev/console", O_NONBLOCK);
	if (fd >= 0) {
		if (gfxMode && screen.hasUnblankedScreen == 0) {
			ioctl(fd, KDSETMODE, KD_GRAPHICS);
			ioctl(fd, KDSETMODE, KD_TEXT);
			screen.hasUnblankedScreen = 1;
		}
		ioctl(fd, KDSETMODE, gfxMode ? KD_GRAPHICS : KD_TEXT);
		ioctl(screen.fd, PS3FB_IOCTL_ON, 0);
		close(fd);
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

	screen.width	= res.xres - 2*res.xoff;
	screen.height	= res.yres - 2*res.yoff;
	screen.stride	= res.xres * 4;
	screen.frame[0]	= (res.yoff * res.yres + res.xoff) * 4;
	screen.frame[1] = screen.frame[0] + res.yoff * res.xoff * 4;
	screen.mmap_size= 1920 * 1080 * 4 * res.num_frames; //res.num_frames * res.yres * res.xres * 4;
	screen.mmap_base= mmap(0, screen.mmap_size, PROT_WRITE, MAP_SHARED, screen.fd, 0);

	if (screen.mmap_base == NULL) {
		close(screen.fd);
		screen.fd = -1;
		return 0;
	}
	memset(screen.mmap_base, 0, screen.mmap_size);

	screen.hasUnblankedScreen = 0;
	switchMode(1);

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
	if (screen.fd >= 0) {
		if (screen.mmap_base != NULL) {
			ioctl(screen.fd, PS3FB_IOCTL_OFF, 0);
			munmap(screen.mmap_base, screen.mmap_size);
		}
		close(screen.fd);
		screen.fd = -1;
	}

	switchMode(0);
	__SPUGL_SCREEN = NULL;
}

int Screen_flip(int frame)
{
	if (screen.fd < 0)
		return -1;

	uint32_t showFrame = frame ? 0 : 1;
	ioctl(screen.fd, PS3FB_IOCTL_FSEL, (unsigned long)&showFrame);

	return showFrame;
}

void Screen_wait(void)
{
	if (screen.fd < 0)
		return;

	uint32_t crt = 0;
	ioctl(screen.fd, FBIO_WAITFORVSYNC, (unsigned long)&crt);
}

