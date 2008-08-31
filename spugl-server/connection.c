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

#include <syslog.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "connection.h"
#include "commandqueue.h"
#include "../spugl-client/daemon.h"

char SPUGL_VERSION[] = VERSION_STRING;

void handleConnect(struct Connection* connection) {
	connection->firstAllocation = NULL;

//#ifdef DEBUG
	char buffer[512];
	sprintf(buffer, "got new connection on fd %d, address %x\n", connection->fd, connection);
	syslog(LOG_INFO, buffer);
//#endif
}

void handleDisconnect(struct Connection* connection) {
//#ifdef DEBUG
	char buffer[512];
	sprintf(buffer, "lost connection on fd %d, address %x\n", connection->fd, connection);
	syslog(LOG_INFO, buffer);
//#endif

	struct Allocation* toFree = connection->firstAllocation;
	while (toFree) {
		struct Allocation* del = toFree;
		toFree = toFree->nextAllocation;

#ifdef DEBUG
		sprintf(buffer, "freeing buffer %d on exit at %x, size %d on fd %d conn %d", del->id, del->buffer, del->size, del->fd, del->conn_fd);
		syslog(LOG_INFO, buffer);
#endif

		munmap(del->buffer, del->size);
		close(del->fd);
		free(del);
	}
	connection->firstAllocation = NULL;
}

void freeBuffer(struct Connection* connection, struct SPUGL_request* request) {
	struct Allocation** ptr = &(connection->firstAllocation);
	while (*ptr) {
		struct Allocation* del = *ptr;
		if (del->conn_fd == connection->fd && del->id == request->free.id) {
#ifdef DEBUG
			char buffer[512];
			sprintf(buffer, "freeing buffer %d at %x, size %d on fd %d conn %d", del->id, del->buffer, del->size, del->fd, del->conn_fd);
			syslog(LOG_INFO, buffer);
#endif

			*ptr = del->nextAllocation;

			munmap(del->buffer, del->size);
			close(del->fd);
			free(del);
			return;
		}
		ptr = &(del->nextAllocation);
	}
#ifdef DEBUG
	char buffer[512];
	sprintf(buffer, "request to free buffer %d conn %d but not found", request->free.id, connection->fd);
	syslog(LOG_INFO, buffer);
#endif
}

static int alloc_id = 0;
static int name_id = 0;

void allocateBuffer(struct Connection* connection, struct SPUGL_request* request, struct SPUGL_reply* reply, int commandQueue, char* mountname) {
	char buffer[512];

	char filename[256];
	sprintf(filename, "%s/virtmem.%d.%d", mountname, getpid(), ++name_id);
	int mem_fd = open(filename, O_RDWR | O_CREAT, 0700);
	if (mem_fd<0) {
#ifdef DEBUG
		sprintf(buffer, "cannot create temporary file %s", filename);
		syslog(LOG_INFO, buffer);
#endif
		reply->alloc.id = 0;
		send(connection->fd, reply, sizeof(struct SPUGL_reply), 0);
		return;
	}
	unlink(filename);

	// ideally, we'd use a tmpfs backed system so we don't need msync

	if ((lseek(mem_fd, request->alloc.size-1, SEEK_SET) == -1) || (write(mem_fd, "", 1)<1)) {
		close(mem_fd);
#ifdef DEBUG
		sprintf(buffer, "cannot extend temporary file %s to size %u", filename, request->alloc.size);
		syslog(LOG_INFO, buffer);
#endif

		reply->alloc.id = 0;
		send(connection->fd, reply, sizeof(struct SPUGL_reply), 0);
		return;
	}

	void* memory = mmap(NULL, request->alloc.size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, 0);
	if (memory==NULL) {
		close(mem_fd);
#ifdef DEBUG
		sprintf(buffer, "cannot mmap temporary file %s", filename);
		syslog(LOG_INFO, buffer);
#endif

		reply->alloc.id = 0;
		send(connection->fd, reply, sizeof(struct SPUGL_reply), 0);
	} else {
		if (commandQueue) {
			// initialise queue pointers to first bit of free buffer
			struct CommandQueue* queue = (struct CommandQueue*) memory;
			queue->write_ptr = queue->read_ptr =
				((void*)(&queue->data[0])) - ((void*)&queue->write_ptr);
		}

#ifdef DEBUG
		sprintf(buffer, "Allocated buffer on fd %d at address %x, size %d, file %s\n", mem_fd, memory, request->alloc.size, filename);
		syslog(LOG_INFO, buffer);
#endif

		struct Allocation* n = malloc(sizeof(struct Allocation));
		n->nextAllocation = connection->firstAllocation;
		n->fd = mem_fd;
		n->conn_fd = connection->fd;
		n->buffer = memory;
		n->size = request->alloc.size;
		n->id = ++alloc_id;
		connection->firstAllocation = n;

		reply->alloc.id = n->id;

		int fds[1] = { mem_fd };
		char buffer[CMSG_SPACE(sizeof(fds))];

		struct msghdr message = {
  			.msg_control = buffer,
  			.msg_controllen = sizeof(buffer),
		};
	
		struct cmsghdr *cmessage = CMSG_FIRSTHDR(&message);
		cmessage->cmsg_level = SOL_SOCKET;
		cmessage->cmsg_type = SCM_RIGHTS;
		cmessage->cmsg_len = CMSG_LEN(sizeof(fds));
		message.msg_controllen = cmessage->cmsg_len;

		memcpy(CMSG_DATA(cmessage), fds, sizeof(fds));

		struct iovec reply_vec = {
  			.iov_base = reply,
  			.iov_len = sizeof(struct SPUGL_reply)
		};

		message.msg_iov = &reply_vec,
		message.msg_iovlen = 1,

		sendmsg(connection->fd, &message, 0);
	}
}

int handleConnectionData(struct Connection* connection, char* mountname) {
	struct SPUGL_request request;
	struct SPUGL_reply reply;
	char buffer[512];

	int len = recv(connection->fd, &request, sizeof(request), 0/*MSG_DONTWAIT*/);
//	if (len == -EAGAIN)
//		return 0;		// need to retry, go back to polling loop

	if (len != sizeof(request)) {	// if short packet returned, bail out
		sprintf(buffer, "short request length %d on fd %d", len, connection->fd);
		syslog(LOG_ERR, buffer);
		return 1;
	}

	switch (request.command) {
		case SPUGLR_GET_VERSION:
			reply.version.major = VERSION_MAJOR;
			reply.version.minor = VERSION_MINOR;
			reply.version.revision = VERSION_REVISION;
			send(connection->fd, &reply, sizeof(reply), 0);
			break;
		case SPUGLR_NEGOTIATE_VERSION:
			if (request.version.major > VERSION_MAJOR ||
				(request.version.major == VERSION_MAJOR  &&
				 request.version.minor > VERSION_MINOR)) {
				sprintf(buffer, "client version too high, requested %d.%d.%d on fd %d", request.version.major, request.version.minor, request.version.revision, connection->fd);
				syslog(LOG_ERR, buffer);
				return 1;
			}
			break;
		case SPUGLR_ALLOC_BUFFER:
			allocateBuffer(connection, &request, &reply, 0, mountname);
			break;

		case SPUGLR_ALLOC_COMMAND_QUEUE: 
			allocateBuffer(connection, &request, &reply, 1, mountname);
			break;

		case SPUGLR_FREE_COMMAND_QUEUE:
		case SPUGLR_FREE_BUFFER:
			freeBuffer(connection, &request);
			break;
			
		default: 
			sprintf(buffer, "invalid request command %d on fd %d", request.command, connection->fd);
			syslog(LOG_ERR, buffer);
			return 1;
	}
				
//	sprintf(buffer, "processed command %d on fd %d\n", request.command, connection->fd);
//	syslog(LOG_INFO, buffer);

	return 0;
}
