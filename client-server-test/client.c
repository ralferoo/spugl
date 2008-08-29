#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

int main(int argc, char* argv[]) {
	int server = socket(PF_UNIX, SOCK_STREAM, 0);

	struct sockaddr_un sock_addr = { AF_UNIX, "\0spuglserver" };
	connect(server, (struct sockaddr *) &sock_addr, sizeof sock_addr);

	int fds[1] = { STDOUT_FILENO };
	char buffer[CMSG_SPACE(sizeof fds)];

	struct msghdr message = {
  		.msg_control = buffer,
  		.msg_controllen = sizeof buffer,
	};

	struct cmsghdr *cmessage = CMSG_FIRSTHDR(&message);
	cmessage->cmsg_level = SOL_SOCKET;
	cmessage->cmsg_type = SCM_RIGHTS;
	cmessage->cmsg_len = CMSG_LEN(sizeof fds);
	message.msg_controllen = cmessage->cmsg_len;

	memcpy(CMSG_DATA(cmessage), fds, sizeof fds);

	char ping = 23;
	struct iovec ping_vec = {
  		.iov_base = &ping,
  		.iov_len = sizeof ping,
	};

	message.msg_iov = &ping_vec,
	message.msg_iovlen = 1,

	sendmsg(server, &message, 0);

	char rbuffer[256];
	int i = read(server, rbuffer, 256);
	write(0, rbuffer, i);
	exit(0);
}

