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
#include "GL/spugl.h"

int main(int argc, char* argv[]) {
	int server = spuglConnect();
	if (server<0) {
		printf("Cannot connect to spugl server\n");
		exit(1);
	}

	CommandQueue* queue = spuglAllocateCommandQueue(server, 2047*1024);
	if (queue==NULL) { printf("Out of memory\n"); exit(1); }

	for (int q=0; q<4; q++) {
		CommandQueue* queue = spuglAllocateCommandQueue(server, 2047*1024);
		if (queue==NULL) { printf("Out of memory on extra queue %d\n", q); exit(1); }
		spuglSetCurrentContext(queue);
		FIFO_PROLOGUE(10);
		BEGIN_RING(FIFO_COMMAND_NOP,0);
		FIFO_EPILOGUE();
	}

	void* buffer2 = spuglAllocateBuffer(server, 1024);
	if (buffer2==NULL) { printf("Out of memory\n"); exit(1); }
	void* buffer = spuglAllocateBuffer(server, 204*1024*1024);
	if (buffer==NULL) { printf("Out of memory\n"); exit(1); }
	spuglFreeBuffer(buffer);

	buffer = spuglAllocateBuffer(server, 1024*1024);
	if (buffer==NULL) { printf("Out of memory\n"); exit(1); }


	spuglSetCurrentContext(queue);
	unsigned int start = spuglTarget();

	glLoadIdentity();
	spuglNop();

	for (int i=0; i<4; i++) {
		usleep(500000);
		spuglNop();
	}
	sleep(1);

	spuglNop();
	spuglJump(start);
	sleep(2);

	spuglFreeBuffer(buffer);

	spuglFlush(queue);
	spuglFreeCommandQueue(queue);

	spuglInvalidRequest(server);

//	spuglFreeBuffer(buffer2);
	spuglDisconnect(server);

	exit(0);
}

