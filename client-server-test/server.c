#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

int main(int argc, char* argv[]) {
	int server = socket(PF_UNIX, SOCK_STREAM, 0);

	struct sockaddr_un sock_addr = { AF_UNIX, "\0spuglserver" };
	bind(server, (struct sockaddr *) &sock_addr, sizeof sock_addr);
	listen(server, 5);

	struct sockaddr_un client_addr;
	socklen_t client_addr_len = sizeof client_addr;
	int client_connection = accept(server,
                               (struct sockaddr *) &client_addr,
                               &client_addr_len);
	close(server);

	int fds[1];
	char buffer[CMSG_SPACE(sizeof fds)];

	char ping;
	struct iovec ping_vec = {
  		.iov_base = &ping,
  		.iov_len = sizeof ping,
	};

	struct msghdr message = {
  		.msg_control = buffer,
  		.msg_controllen = sizeof buffer,
  		.msg_iov = &ping_vec,
  		.msg_iovlen = 1,
	};

	int r;
	if ((r=recvmsg(client_connection, &message, 0)) > 0) {
		printf("recvmsg = %x\n", r);
		struct cmsghdr *cmessage = CMSG_FIRSTHDR(&message);
		memcpy(fds, CMSG_DATA(cmessage), sizeof fds);
		write(fds[0], "HELLO\n", 6);
	}

	write(client_connection, "spugl-0.0.1\n", 13);

	sleep(1);
	exit(0);
}

