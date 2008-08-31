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

// #define DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "daemon.h"
#include "client.h"

struct SPUGL_Buffer {
	struct SPUGL_Buffer* next;
	void* data;
	unsigned long id;
	unsigned long size;
	int fd;
};

static struct SPUGL_Buffer* firstBuffer;

static void* _allocate(int server, unsigned long size, unsigned short command);

struct CommandQueue* SPUGL_allocateCommandQueue(int server, unsigned long size) {
	return _allocate(server, size, SPUGLR_ALLOC_COMMAND_QUEUE);
}
	
void* SPUGL_allocateBuffer(int server, unsigned long size) {
	return (struct CommandQueue*) _allocate(server, size, SPUGLR_ALLOC_BUFFER);
}
	
static void _freeBuffer(int server, void* buffer, unsigned short command);

void SPUGL_freeCommandQueue(int server, struct CommandQueue* buffer) {
	_freeBuffer(server, buffer, SPUGLR_FREE_COMMAND_QUEUE);
}

void SPUGL_freeBuffer(int server, void* buffer) {
	_freeBuffer(server, buffer, SPUGLR_FREE_COMMAND_QUEUE);
}

static void _freeBuffer(int server, void* buffer, unsigned short command) {
	struct SPUGL_Buffer** ptr = &firstBuffer;
	while (*ptr) {
		struct SPUGL_Buffer* test = *ptr;
		if (test->data == buffer) {
#ifdef DEBUG
			printf("freeing %x %x %x size %d\n", buffer, test, test->data, test->size);
#endif

			// tell the server we've junked the buffer
			struct SPUGL_request request;
			struct SPUGL_reply reply;
			request.command = command;
			request.free.id = test->id;
			send(server, &request, sizeof(request), 0);

			// unmap the buffer and close the file
			munmap(buffer, test->size);
			close(test->fd);

			// unlink the header
			*ptr = test->next;
			free(test);
			return;
		}
		ptr = &(test->next);
	}
	printf("Cannot find memory buffer %x in allocation list\n", buffer);
}

static void* _allocate(int server, unsigned long size, unsigned short command) {
	struct SPUGL_request request;
	struct SPUGL_reply reply;
	request.command = command;
	request.alloc.size = size;
	send(server, &request, sizeof(request), 0);

	int fds[1];
	char buffer[CMSG_SPACE(sizeof(fds))];
	memset(buffer, -1, sizeof buffer);

	struct iovec reply_vec = {
  		.iov_base = &reply,
  		.iov_len = sizeof(reply),
	};

	struct msghdr message = {
  		.msg_control = buffer,
  		.msg_controllen = sizeof(buffer),
  		.msg_iov = &reply_vec,
  		.msg_iovlen = 1,
	};

	if (recvmsg(server, &message, 0) > 0) {
		if (message.msg_controllen == sizeof(buffer)) {
			struct cmsghdr *cmessage = CMSG_FIRSTHDR(&message);
			memcpy(fds, CMSG_DATA(cmessage), sizeof fds);

			int mem_fd = fds[0];
			void* memory = mmap(NULL, request.alloc.size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, 0);
			if (memory!=NULL) {
				struct CommandQueue* queue = (struct CommandQueue*) memory;
#ifdef DEBUG
				printf("fd %d, memory %x, size %d, id %d, write %x, read %x\n",
					mem_fd, memory, request.alloc.size, reply.alloc.id,
					queue->write_ptr, queue->read_ptr);
#endif

				struct SPUGL_Buffer* header = malloc(sizeof(struct SPUGL_Buffer));
				if (header!=NULL) {
					header->next = firstBuffer;
					header->data = memory;
					header->id = reply.alloc.id;
					header->fd = mem_fd;
					header->size = request.alloc.size;
					firstBuffer = header;
					return memory;
				} else {
					// no memory for header data
					printf("Couldn't allocate memory header\n");
					munmap(memory, request.alloc.size);
					close(mem_fd);
					return NULL;
				}
			} else {
				// couldn't mmap file
				printf("Couldn't mmap memory\n");
				close(mem_fd);
				return NULL;
			}
		} else {
			// nothing returned from server
			printf("Couldn't allocate memory on server\n");
			return NULL;
		}
	} else {
		// short data returned from server
		printf("Couldn't allocate memory - no response from server\n");
		return NULL;
	}
}

int SPUGL_connect() {
	struct sockaddr_un sock_addr = { AF_UNIX, "\0spugl-server" };
	int server = socket(PF_UNIX, SOCK_STREAM, 0);
	if (server<0) {
		//printf("Cannot create communication socket\n");
		return -1;
	}
	
	if (connect(server, (struct sockaddr *) &sock_addr, sizeof sock_addr)<0) {
		//printf("Cannot connect to spugl server\n");
		return -2;
	}

	struct SPUGL_request request;
	struct SPUGL_reply reply;
	request.command = SPUGLR_GET_VERSION;
	send(server, &request, sizeof(request), 0);
	recv(server, &reply, sizeof(reply), 0);
	printf("Server version is %d.%d.%d\n", reply.version.major, reply.version.minor, reply.version.revision);

	request.command = SPUGLR_NEGOTIATE_VERSION;
	request.version.major = VERSION_MAJOR;
	request.version.minor = VERSION_MINOR;
	request.version.revision = VERSION_REVISION;
	send(server, &request, sizeof(request), 0);
	return server;
}

void SPUGL_disconnect(int server) {
	close(server);
}

void SPUGL_invalidRequest(int server) {
	struct SPUGL_request request;
	request.command = 4242;
	send(server, &request, sizeof(request), 0);
}
