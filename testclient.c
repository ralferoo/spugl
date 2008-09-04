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

int main(int argc, char* argv[]) {
	int server = SPUGL_connect();
	if (server<0) {
		printf("Cannot connect to spugl server\n");
		exit(1);
	}

	CommandQueue* queue = SPUGL_allocateCommandQueue(server, 2047*1024);

	void* buffer2 = SPUGL_allocateBuffer(server, 1024);
	void* buffer = SPUGL_allocateBuffer(server, 2047*1024*1024);
	SPUGL_freeBuffer(buffer);	

	buffer = SPUGL_allocateBuffer(server, 1024*1024);

sleep(1);

	SPUGL_freeBuffer(buffer);	

	SPUGL_flush(queue);
	SPUGL_freeCommandQueue(queue);	

	SPUGL_invalidRequest(server);

	SPUGL_disconnect(server);

	exit(0);
}

