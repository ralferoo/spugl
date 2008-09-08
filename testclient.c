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
#include "GL/glspu.h"

int main(int argc, char* argv[]) {
	int server = SPUGL_connect();
	if (server<0) {
		printf("Cannot connect to spugl server\n");
		exit(1);
	}

	CommandQueue* queue = SPUGL_allocateCommandQueue(server, 2047*1024);
	if (queue==NULL) { printf("Out of memory\n"); exit(1); }

	for (int q=0; q<4; q++) {
		CommandQueue* queue = SPUGL_allocateCommandQueue(server, 2047*1024);
		if (queue==NULL) { printf("Out of memory on extra queue %d\n", q); exit(1); }
		SPUGL_currentContext(queue);
		FIFO_PROLOGUE(10);
		BEGIN_RING(FIFO_COMMAND_NOP,0);
		FIFO_EPILOGUE();
	}

	void* buffer2 = SPUGL_allocateBuffer(server, 1024);
	if (buffer2==NULL) { printf("Out of memory\n"); exit(1); }
	void* buffer = SPUGL_allocateBuffer(server, 204*1024*1024);
	if (buffer==NULL) { printf("Out of memory\n"); exit(1); }
	SPUGL_freeBuffer(buffer);	

	buffer = SPUGL_allocateBuffer(server, 1024*1024);
	if (buffer==NULL) { printf("Out of memory\n"); exit(1); }


	SPUGL_currentContext(queue);
	unsigned int start = glspuTarget();

	glLoadIdentity();
	glspuNop();

	for (int i=0; i<4; i++) {
		usleep(500000);
		glspuNop();
	}
	sleep(1);

	glspuNop();
	glspuJump(start);
	sleep(2);

	SPUGL_freeBuffer(buffer);	

	SPUGL_flush(queue);
	SPUGL_freeCommandQueue(queue);	

	SPUGL_invalidRequest(server);

//	SPUGL_freeBuffer(buffer2);	
	SPUGL_disconnect(server);

	exit(0);
}

