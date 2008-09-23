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

#include <stdio.h>
#include <stdlib.h>

#include "client/daemon.h"
#include "client/client.h"
#include "client/fifodefs.h"

#include "GL/gl.h"
//#include "GL/spugl.h"

int main(int argc, char* argv[]) {
	int server = spuglConnect();
	if (server<0) {
		printf("Cannot connect to spugl server\n");
		exit(1);
	}

	int width, height;
	spuglScreenSize(server, &width, &height);
	printf("Connected to spugl, screen size is %dx%d\n", width, height);

	CommandQueue* queue = spuglAllocateCommandQueue(server, 2047*1024);
	if (queue==NULL) { printf("Out of memory\n"); exit(1); }

	void* buffer = spuglAllocateBuffer(server, 204*1024*1024);
	if (buffer==NULL) { printf("Out of memory\n"); exit(1); }

	unsigned int context = spuglFlip(queue);

	spuglSetCurrentContext(queue);
	unsigned int start = spuglTarget();

	glLoadIdentity();
	spuglNop();
	spuglDrawContext(context);

	// spuglJump(start);


	// NOTE THIS ISN'T A REAL OPENGL TRANSFORM!!!
	GLfloat projectionMatrix[] = {
		1.0,				0.0,				0.0,	0.0,
		0.0,				1.0,				0.0,	0.0,
		640.0f/420.0f,			360.0/420.0f,			1.0,	1.0/420.f,
		(640.0f*-282.0f)/420.0f,	(360.f*-282.0f)/420.0f,		0.0,	-282.0f/420.f,
	};
	glLoadMatrixf(projectionMatrix);


	for (int i=0; i<1200; i++) {
		spuglDrawContext(context);

		glBegin(GL_TRIANGLES);
				glTexCoord2f( 256, 256 );
				glColor3ub(255, 255, 0);
				glVertex3f(1100, 100, 100);

				glTexCoord2f( 0, 0 );
				glColor3ub(0, 0, 0);
				glVertex3f(0, 0, 100);

				glTexCoord2f( 256, 0 );
				glColor3ub(255, 0, 0);
				glVertex3f(100, 0, 100);
		glEnd();

		spuglFlush(queue);
		spuglWait(queue);
		context = spuglFlip(queue);
	}

	//sleep(1);

	spuglFreeBuffer(buffer);
	spuglFreeCommandQueue(queue);
	spuglDisconnect(server);

	exit(0);
}
