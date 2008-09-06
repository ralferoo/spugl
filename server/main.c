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
#include <syslog.h>
#include <signal.h>
#define __USE_GNU
#include <poll.h>
#include <fcntl.h>

#include <sys/mount.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "connection.h"
#include "ppufuncs.h"

#ifndef MNT_DETACH
// not defined on my system for some reason :(
#define MNT_DETACH 2
#endif

static volatile sig_atomic_t terminated = 0;

static void sig_hup(int sig)
{
	// no-op, but will cause the ppoll to exit early
}

static void sig_hup_onexit(int sig)
{
	signal(getpid(), (void*) SIGKILL);
}

static void sig_term(int sig)
{
	// gracefully handle the exit signal
        terminated = 1;
}

int main(int argc, char* argv[]) {
	// location to create tmpfs mountpoint and place mmap'ed files
	char* mountname = "/tmp/virtmem";

	int server = socket(PF_UNIX, SOCK_STREAM, 0);

	openlog("spugl", LOG_NDELAY | LOG_PID, LOG_DAEMON);
	syslog(LOG_INFO, SPUGL_VERSION);

	struct sockaddr_un sock_addr = { AF_UNIX, "\0spugl-server" };
	if (bind(server, (struct sockaddr *) &sock_addr, sizeof sock_addr)<0) {
		syslog(LOG_ERR, "cannot bind to spugl-server socket");
		exit(1);
	}
	if (listen(server, 5)<0) {
		syslog(LOG_ERR, "cannot listen on spugl-server socket");
		exit(1);
	}

	mkdir(mountname, 0700);
	mount("spugl", mountname, "tmpfs",
			MS_NOATIME | MS_NODEV | MS_NODIRATIME | MS_NOEXEC | MS_NOSUID, "");

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_flags = SA_NOCLDSTOP;

	sa.sa_handler = sig_term;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);

	sa.sa_handler = sig_hup;
	sigaction(SIGHUP, &sa, NULL);

	sa.sa_handler = SIG_IGN;
	sigaction(SIGCHLD, &sa, NULL);
	sigaction(SIGPIPE, &sa, NULL);

	syslog(LOG_INFO, "accepting connections");

	int connectionCount = 0;
	ConnectionList list = {0};

	SPU_HANDLE thread = _init_spu_thread(blockManagementInit(), 1);

	while (!terminated) {
		struct pollfd p[connectionCount+1];
		sigset_t sigs;
		struct timespec timeout;

		sigfillset(&sigs);
		sigdelset(&sigs, SIGCHLD);
     		sigdelset(&sigs, SIGPIPE);
		sigdelset(&sigs, SIGTERM);
       		sigdelset(&sigs, SIGINT);
		sigdelset(&sigs, SIGHUP);

		p[0].fd = server;
		p[0].events = POLLIN | POLLERR | POLLHUP;
		p[0].revents = 0;

		int i;
		Connection* current = list.first;
		for (i=1; i<=connectionCount && current; i++) {
			p[i].fd = current->fd;
			p[i].events = POLLIN | POLLERR | POLLHUP;
			p[i].revents = 0;
			current = current->nextConnection;
		}
		timeout.tv_sec = 10;
		timeout.tv_nsec = 0;

		if (ppoll(p, i, &timeout, &sigs) >=0 ) {
			Connection** curr_ptr = &list.first;
			for (i=1; i<=connectionCount && *curr_ptr; i++) {
				Connection* connection = *curr_ptr;
				if (p[i].revents & POLLIN) {
					if (handleConnectionData(connection, mountname))
						goto disconnected;
				}
				if (p[i].revents & (POLLERR|POLLHUP)) {
disconnected:				handleDisconnect(connection);
					// unlink from connection list
					*curr_ptr = connection->nextConnection;
					connectionCount--;
					// hook into close connection list
					connection->nextConnection = list.firstClosed;
					list.firstClosed = connection;
					syslog(LOG_INFO, "moved connection to closed queue");
				} else {
					// move to next connection
					curr_ptr = &(connection->nextConnection);
				} 
			}
	
			if (p[0].revents & POLLIN) {
				struct sockaddr_un client_addr;
				socklen_t client_addr_len = sizeof(client_addr);
				int client_connection = accept(server,
                               			(struct sockaddr *) &client_addr,
                               			&client_addr_len);
	
				if (client_connection < 0) {
					syslog(LOG_INFO, "accept on incoming connection failed");
				} else {
					Connection* connection = malloc(sizeof(Connection));
					connection->fd = client_connection;
					connection->nextConnection = list.first;
					list.first = connection;
					connectionCount++;
					handleConnect(connection);
				}
			}
		}

		// process other events
		Connection* conn = list.first;
		while (conn) {
			processOutstandingRequests(conn);
			conn = conn->nextConnection;
		}
		Connection** closed = &list.firstClosed;
		while (*closed) {
			Connection* conn = *closed;
			processOutstandingRequests(conn);
			if (conn->firstAllocation == NULL) {
				syslog(LOG_INFO, "clearing stuff");
				*closed = conn->nextConnection;
				close(conn->fd);
				free(conn);
			} else {
				printf("connection %x still has allocation %x address %x\n", 
					conn, conn->firstAllocation, conn->firstAllocation->buffer);
				closed = &(conn->nextConnection);
			}
		}
	}

	blockManagementDestroy();
	close(server);
	syslog(LOG_INFO, "exiting");

	sa.sa_handler = sig_hup_onexit;
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);

	_exit_spu_thread(thread);

	umount2(mountname, MNT_FORCE | MNT_DETACH);
	rmdir(mountname);
}
