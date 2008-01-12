/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

#include <stdlib.h>
#include <stdio.h>

#include <GL/gl.h>
#include <GL/glspu.h>

int main(int argc, char* argv[]) {
	glspuSetup();
	glBegin(GL_QUADS);
	glColor3f(1.0, 0.5, 0.25);
	glVertex3f(-100.0f, 100.0f,   0.0f);
	glColor3f(0.5, 1.0, 0.25);
	glVertex3f(-100.0f,-100.0f,   0.0f);
	glVertex3f( 100.0f,-100.0f,   0.0f);
	glColor3ub(127, 15, 192);
	glVertex3f( 100.0f, 100.0f,   0.0f);
	glEnd();	
	usleep(250000);
	glspuDestroy();

	// quick hack so that SPU debugging messages have a chance to come out
	usleep(150000);
	exit(0);
}
