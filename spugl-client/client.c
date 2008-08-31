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
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "daemon.h"

void allocate(int server, unsigned long size) {
	struct SPUGL_request request;
	struct SPUGL_reply reply;
	request.command = SPUGLR_ALLOC_COMMAND_QUEUE;
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
				sprintf(memory, "HELLO! Updated contents is fd %d at address %x\n", mem_fd, memory);
				msync(memory, 65536, MS_SYNC);
			}

			printf("fd %d, memory %x, size %d, id %d\n", mem_fd, memory, request.alloc.size, reply.alloc.id);
		} else {
			printf("Couldn't allocate memory\n");
		}
	}
}

int main(int argc, char* argv[]) {
	struct sockaddr_un sock_addr = { AF_UNIX, "\0spugl-server" };
	int server = socket(PF_UNIX, SOCK_STREAM, 0);
	if (server<0) {
		printf("Cannot create communication socket\n");
		exit(1);
	}
	
	if (connect(server, (struct sockaddr *) &sock_addr, sizeof sock_addr)<0) {
		printf("Cannot connect to spugl server\n");
		exit(1);
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

	allocate(server, 2047*1024);
	allocate(server, 2047*1024*1024);

	request.command = 42;
	send(server, &request, sizeof(request), 0);

	close(server);

	exit(0);
}

