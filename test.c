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
	glBegin(GL_TRIANGLES);
	glVertex3f( 0.0f, 1.0f, 0.0f);
	glVertex3f(-1.0f,-1.0f, 0.0f);
	glVertex3f( 1.0f,-1.0f, 0.0f);
	glEnd();	
	glspuDestroy();

	// quick hack so that SPU debugging messages have a chance to come out
	usleep(250);
	exit(0);
}
