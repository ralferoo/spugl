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
#include "client/fifo.h"

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
		unsigned int* ptr = (unsigned int*) ( ((void*)queue) + queue->write_ptr );
		*ptr++ = 0;
		__asm__("lwsync");
		queue->write_ptr = ((void*)ptr) - ((void*)queue);
		__asm__("lwsync");
	}

	void* buffer2 = SPUGL_allocateBuffer(server, 1024);
	if (buffer2==NULL) { printf("Out of memory\n"); exit(1); }
	void* buffer = SPUGL_allocateBuffer(server, 204*1024*1024);
	if (buffer==NULL) { printf("Out of memory\n"); exit(1); }
	SPUGL_freeBuffer(buffer);	

	buffer = SPUGL_allocateBuffer(server, 1024*1024);
	if (buffer==NULL) { printf("Out of memory\n"); exit(1); }

	SPUGL_currentContext(queue);

	unsigned int start = queue->write_ptr;
{
	FIFO_PROLOGUE(queue,10);
	BEGIN_RING(FIFO_COMMAND_NOP,0);
	FIFO_EPILOGUE();
}

	for (int i=0; i<4; i++) {
		usleep(500000);
		FIFO_PROLOGUE(queue,10);
		BEGIN_RING(FIFO_COMMAND_NOP,0);
		FIFO_EPILOGUE();
	}

	sleep(1);
{
	FIFO_PROLOGUE(queue,10);
	BEGIN_RING(FIFO_COMMAND_JUMP,1);
	OUT_RING(start);
	FIFO_EPILOGUE();
}
	sleep(2);

	SPUGL_freeBuffer(buffer);	

	SPUGL_flush(queue);
	SPUGL_freeCommandQueue(queue);	

	SPUGL_invalidRequest(server);

//	SPUGL_freeBuffer(buffer2);	
	SPUGL_disconnect(server);

	exit(0);
}

