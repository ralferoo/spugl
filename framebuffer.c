/*
 * This file is part of the python games library for the PS3, 
 * (c) 2007 Ranulf Doswell and made available under the LGPL
 * license. Please see the file "LICENSE" for more information.
 *
 * Please visit https://sourceforge.net/projects/python-ps3/ 
 * for more information of the python-PS3 project
 */

#include <fcntl.h>
#include <stdint.h>
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

#define PS3FB_IOCTL_SETMODE          _IOW('r',  1, int)
#define PS3FB_IOCTL_GETMODE          _IOR('r',  2, int)

#include "fifo.h"
#include "struct.h"

typedef struct {
	int fd;
	struct ps3fb_ioctl_res res;
	int graphicsMode;

	unsigned long length;
	unsigned long lengthPerFrame;
	char* addr;
	int mode;
	int desiredMode;
	int hasUnblankedScreen;

	char* current;
	int drawFrame;

	_bitmap_image image;
} ScreenObject;

static void switchMode(ScreenObject* self, int gfxMode) {
	int fd = open("/dev/console", O_NONBLOCK);
	if (fd >= 0) {
//		printf("Switching to %s mode\n", gfxMode ? "graphics" : "text");
		if (gfxMode && self->hasUnblankedScreen == 0) {
//			this should work, but doesn't, so instead we toggle modes as the gfx->txt
//			transition is documented as unblanking the screen... <sigh>
//			int i =  TIOCL_UNBLANKSCREEN;
//			ioctl(fd, TIOCLINUX, &i);
			ioctl(fd, KDSETMODE, KD_GRAPHICS);
			ioctl(fd, KDSETMODE, KD_TEXT);
			self->hasUnblankedScreen = 1;
		}
		ioctl(fd, KDSETMODE, gfxMode ? KD_GRAPHICS : KD_TEXT);
		close(fd);
	} else {
		printf("Warning: Cannot open /dev/console, so cannot change console mode.\n");
	}
}

static void Screen_initResources(ScreenObject* self) {
	self->fd = -1;
	self->desiredMode = -1;
	self->graphicsMode = 0;
	self->hasUnblankedScreen = 0;
}

static void Screen_closeResources(ScreenObject* self) {
	if (self->fd >= 0) {
		if (self->addr != NULL) {
			ioctl(self->fd, PS3FB_IOCTL_OFF, 0);
			munmap(self->addr, self->length);
		}
		close(self->fd);
		self->fd = -1;
	}
	if (self->graphicsMode) {
		switchMode(self, 0);
		self->graphicsMode = 0;
	}
	self->desiredMode = -1;
}

static int Screen_fd(ScreenObject *self)
{
	if (self->fd >= 0)
		return 1;

	self->fd = open("/dev/fb0", O_RDWR);
	if (self->fd < 0)
		return 0;

	int vid = 0;
	int res = 0;

	if (self->desiredMode >= 0) {
//		printf("desired mode %d\n", self->desiredMode);
		vid = self->desiredMode;
		if ((res=ioctl(self->fd, PS3FB_IOCTL_SETMODE, (unsigned long)&vid))<0) 
			return 0;
		self->mode = self->desiredMode;
	} else {
		if ((res=ioctl(self->fd, PS3FB_IOCTL_GETMODE, (unsigned long)&vid))<0) 
			self->mode = -1;
		else
			self->mode = vid;
	}

	if (ioctl(self->fd, PS3FB_IOCTL_SCREENINFO, (unsigned long)(&self->res))==-1)	
		return 0;

	self->lengthPerFrame = self->res.xres * self->res.yres * 4;
	self->length = self->lengthPerFrame * self->res.num_frames;

	self->graphicsMode = 1;
	switchMode(self, 1);

	self->addr = mmap(0, self->length, PROT_WRITE, MAP_SHARED, self->fd, 0);
	if (self->addr == NULL)
		return 0;

	if (ioctl(self->fd, PS3FB_IOCTL_ON, 0) < 0)
		return 0;

	self->drawFrame = 1;
	self->current = self->addr;

	uint32_t showFrame = self->drawFrame ? 0 : 1;
    ioctl(self->fd, PS3FB_IOCTL_FSEL, (unsigned long)&showFrame);

	return 1;
}

static void
Screen_dealloc(ScreenObject *self)
{
	Screen_closeResources(self);
	free(self);
}

static int
Screen_flip(ScreenObject *self)
{
	if (!Screen_fd(self)) {
		return -1;
	}

	self->drawFrame = !self->drawFrame;

	uint32_t showFrame = self->drawFrame ? 0 : 1;
    ioctl(self->fd, PS3FB_IOCTL_FSEL, (unsigned long)&showFrame);

	self->current = self->drawFrame ? (self->addr + self->lengthPerFrame)
									: self->addr;
	return showFrame;
}

static void
Screen_wait(ScreenObject *self)
{
	if (!Screen_fd(self)) {
		return;
	}

	uint32_t crt = 0;
	ioctl(self->fd, FBIO_WAITFORVSYNC, (unsigned long)&crt);
}

static int Screen_setMode(ScreenObject* self, int desiredMode) {
	if (desiredMode<0) {
		return -1;
	}

	Screen_closeResources(self);
	self->desiredMode = desiredMode;

	if (!Screen_fd(self)) {
		return -1;
	}
	
	return 0;
}

static int _screen_initialised = 0;
static ScreenObject _screen;
BitmapImage _getScreen(void)
{
	if (_screen_initialised == 0) {
		_screen_initialised = 1;
		Screen_initResources(&_screen);
	}
	if (Screen_fd(&_screen)) {
		_screen.image.width = _screen.res.xres - _screen.res.xoff*2;
		_screen.image.height = _screen.res.yres - _screen.res.yoff*2;
		_screen.image.bytes_per_line = _screen.res.xres * 4;
		_screen.image.address = _MAKE_EA(_screen.current) +
		    4*(_screen.res.xres * _screen.res.xoff + _screen.res.yoff);
		return &(_screen.image);
	}
	return NULL;
}

BitmapImage _flipScreen(void)
{
	Screen_flip(&_screen);
	_screen.image.address = _MAKE_EA(_screen.current) +
	    4*(_screen.res.xres * _screen.res.xoff + _screen.res.yoff);
	return &(_screen.image);
}

void _waitScreen(void)
{
	Screen_wait(&_screen);
}

void _closeScreen(void)
{
	Screen_closeResources(&_screen);
}
