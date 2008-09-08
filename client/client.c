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
// #define INFO

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

typedef struct __SPUGL_Buffer SPUGL_Buffer;

struct __SPUGL_Buffer {
	SPUGL_Buffer* next;
	void* data;
	unsigned long id;
	unsigned long size;
	int fd;
	int server_fd;
};

static SPUGL_Buffer* firstBuffer;

static void* _allocate(int server, unsigned long size, unsigned short command);

CommandQueue* SPUGL_allocateCommandQueue(int server, unsigned long size) {
	return _allocate(server, size, SPUGLR_ALLOC_COMMAND_QUEUE);
}
	
void* SPUGL_allocateBuffer(int server, unsigned long size) {
	return (CommandQueue*) _allocate(server, size, SPUGLR_ALLOC_BUFFER);
}
	
static void _freeBuffer(void* buffer, unsigned short command);

void SPUGL_freeCommandQueue(CommandQueue* buffer) {
	_freeBuffer(buffer, SPUGLR_FREE_COMMAND_QUEUE);
}

void SPUGL_freeBuffer(void* buffer) {
	_freeBuffer(buffer, SPUGLR_FREE_COMMAND_QUEUE);
}

static void _freeBuffer(void* buffer, unsigned short command) {
	SPUGL_Buffer** ptr = &firstBuffer;
	while (*ptr) {
		SPUGL_Buffer* test = *ptr;
		if (test->data == buffer) {
#ifdef DEBUG
			printf(" FREE %x - %x %x %x size %d\n", test->id, buffer, test, test->data, test->size);
#endif

			// tell the server we've junked the buffer
			SPUGL_request request;
			SPUGL_reply reply;
			request.free.command = command;
			request.free.id = test->id;
			send(test->server_fd, &request, sizeof(request), 0);

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
#ifdef INFO
	printf("Cannot find memory buffer %x in allocation list\n", buffer);
#endif
}

static void* _allocate(int server, unsigned long size, unsigned short command) {
	SPUGL_request request;
	SPUGL_reply reply;
	request.alloc.command = command;
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
				CommandQueue* queue = (CommandQueue*) memory;
#ifdef DEBUG
				printf("ALLOC %x - fd %d, memory %x, size %d, write %x, read %x\n",
					reply.alloc.id, mem_fd, memory, request.alloc.size, 
					queue->write_ptr, queue->read_ptr);
#endif

				SPUGL_Buffer* header = malloc(sizeof(SPUGL_Buffer));
				if (header!=NULL) {
					header->next = firstBuffer;
					header->data = memory;
					header->id = reply.alloc.id;
					header->fd = mem_fd;
					header->size = request.alloc.size;
					header->server_fd = server;
					firstBuffer = header;
					return memory;
				} else {
					// no memory for header data
#ifdef INFO
					printf("Couldn't allocate memory header\n");
#endif
					munmap(memory, request.alloc.size);
					close(mem_fd);
					return NULL;
				}
			} else {
				// couldn't mmap file
#ifdef INFO
				printf("Couldn't mmap memory\n");
#endif
				close(mem_fd);
				return NULL;
			}
		} else {
			// nothing returned from server
#ifdef INFO
			printf("Couldn't allocate memory on server\n");
#endif
			return NULL;
		}
	} else {
		// short data returned from server
#ifdef INFO
		printf("Couldn't allocate memory - no response from server\n");
#endif
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

	SPUGL_request request;
	SPUGL_reply reply;
	request.header.command = SPUGLR_GET_VERSION;
	send(server, &request, sizeof(request), 0);
	recv(server, &reply, sizeof(reply), 0);
#ifdef INFO
	printf("Server version is %d.%d.%d\n", reply.version.major, reply.version.minor, reply.version.revision);
#endif

	request.version.command = SPUGLR_NEGOTIATE_VERSION;
	request.version.major = VERSION_MAJOR;
	request.version.minor = VERSION_MINOR;
	request.version.revision = VERSION_REVISION;
	send(server, &request, sizeof(request), 0);
	return server;
}

void SPUGL_disconnect(int server) {
	close(server);
}

void SPUGL_flush(CommandQueue* buffer) {
	SPUGL_Buffer* ptr = firstBuffer;
	while (ptr) {
		if (ptr->data == buffer) {
			// send flush message to server
			SPUGL_request request;
			request.flush.command = SPUGLR_FLUSH;
			request.flush.id = ptr->id;
			send(ptr->server_fd, &request, sizeof(request), 0);

			// wait for server to reply - means server has flushed queue
			SPUGL_reply reply;
			recv(ptr->server_fd, &reply, sizeof(reply), 0);
			return;
		}
		ptr = ptr->next;
	}
}

void SPUGL_invalidRequest(int server) {
	SPUGL_request request;
	request.header.command = 4242;
	send(server, &request, sizeof(request), 0);
}

///////////////////////////////////////////////////////////////////////////////
//
// This is a reference to the "current context". In reality, this is nothing
// more than a pointer to the appropriate FIFO buffer, although this may yet
// change.

CommandQueue* _SPUGL_fifo = NULL;

CommandQueue* SPUGL_currentContext(CommandQueue* newContext) {
	CommandQueue* old = _SPUGL_fifo;
	_SPUGL_fifo = newContext;
	return old;
}

