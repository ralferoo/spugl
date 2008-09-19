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

	spuglSetCurrentContext(queue);
	unsigned int start = spuglTarget();

	glLoadIdentity();
	spuglNop();

	spuglJump(start);

	for (int i=0; i<120; i++) {
		spuglFlush(queue);
		spuglWait(queue);
		spuglFlip(queue);
	}

	//sleep(1);

	spuglFreeBuffer(buffer);
	spuglFreeCommandQueue(queue);
	spuglDisconnect(server);

	exit(0);
}

