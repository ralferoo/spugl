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
#include <sys/mount.h>

#ifndef MNT_DETACH
// not defined on my system for some reason :(
#define MNT_DETACH 2
#endif

int main(int argc, char* argv[]) {
	int server = socket(PF_UNIX, SOCK_STREAM, 0);

	struct sockaddr_un sock_addr = { AF_UNIX, "\0spuglserver" };
	connect(server, (struct sockaddr *) &sock_addr, sizeof sock_addr);

	// it seems passing /dev/zero through SCM_RIGHTS doesn't work, so
	// we need to pass memory blocks used a file-backed fd :(

	//int mem_fd = open("/dev/zero", O_RDWR);

	char* mountname = "/tmp/virtmem";
	mkdir(mountname, 0700);
	mount("spugl", mountname, "tmpfs", MS_NOATIME | MS_NODEV |
			MS_NODIRATIME | MS_NOEXEC | MS_NOSUID, "");

	char filename[256];
	sprintf(filename, "%s/virtmem.%d", mountname, getpid());
	int mem_fd = open(filename, O_RDWR | O_CREAT, 0700);
	unlink(filename);
	printf("%s\n", filename);

	// ideally, we'd use a tmpfs backed system so we don't need msync

//	char tmp[65536];
//	memset(tmp,0,65536);
//	write(mem_fd,tmp,65536);

	lseek(mem_fd, 65535, SEEK_SET);
	write(mem_fd,"",1);

	void* memory = mmap(NULL, 65536,
		PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, 0);
	if (memory!=NULL) {
		printf("Mapped %d at %x\n", mem_fd, memory);
		sprintf(memory, "Initial contents is fd %d at address %x\n", mem_fd, memory);
		msync(memory, 65536, MS_SYNC);
		write(0, memory, strlen(memory));
	}

	int fds[2] = { STDOUT_FILENO, mem_fd };
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

	write(0, memory, strlen(memory));

	//sleep(5);

	umount2(mountname, MNT_FORCE | MNT_DETACH);

	exit(0);
}

