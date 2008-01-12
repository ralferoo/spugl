/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

#include <stdlib.h>
#include <stdio.h>

#include <GLES/gl.h>
#include <GLES/glspu.h>

int main(int argc, char* argv[]) {
	glspuSetup();
	glBegin(GL_TRIANGLES);
	glVertex3f( 0.0f, 1.0f, 0.0f);
	glVertex3f(-1.0f,-1.0f, 0.0f);
	glVertex3f( 1.0f,-1.0f, 0.0f);
	glEnd();	
	glspuDestroy();
	exit(0);
}
