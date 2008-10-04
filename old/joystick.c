/*
 * This file is part of the python games library for the PS3, 
 * (c) 2007 Ranulf Doswell
 *
 * This library may not be used or distributed without a licence, please
 * contact me for information if you wish to use it.
 * license. Please see the file "LICENSE" for more information.
 *
 * Please visit https://sourceforge.net/projects/python-ps3/ 
 * for more information of the python-PS3 project
 */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <linux/kd.h>
#include <linux/joystick.h>

struct JOYSTICK {
	int fd;
	int axis[4];
	int buttons;
};

struct JOYSTICK stick = { .fd = -1};

void stick_init() {
	stick.fd = open("/dev/input/js0", O_RDONLY);
	if (stick.fd>=0)
		fcntl(stick.fd, F_SETFL, O_NONBLOCK);

	int i;
	for (i=0; i<4; i++)
		stick.axis[i] = 0;
	stick.buttons = 0;
}

void stick_close() {
	if (stick.fd>=0) {
		close(stick.fd);
		stick.fd = -1;
	}
}

void stick_update() {
        struct js_event js;
        int r=0;
	if (stick.fd>=0) {
        	while ((r=read(stick.fd, &js, sizeof(struct js_event))) == sizeof(struct js_event)) {
                	if (js.type & JS_EVENT_BUTTON) {
                        	if (js.value)
                                	stick.buttons |= (1<<js.number);
                        	else
                                	stick.buttons &=~ (1<<js.number);
                	}
                	if (js.type & JS_EVENT_AXIS && js.number<4) {
                        	stick.axis[js.number] = js.value;
                	}
		}
        }
}

int stick_buttons() {
	stick_update();
	// printf("stick buttons %x\n", stick.buttons);
	return stick.buttons;
}

int stick_button(int b) {
	return (stick_buttons()>>b)&1;
}

int stick_axis(int a) {
	stick_update();
	return stick.axis[a];
}

