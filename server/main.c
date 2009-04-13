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

#define NUMBER_OF_RENDER_SPU_THREADS 4

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#define __USE_GNU
#include <poll.h>
#include <fcntl.h>
#include <time.h>

#include <sys/mount.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "connection.h"
#include "functions.h"
#include "framebuffer.h"

#ifndef MNT_DETACH
// not defined on my system for some reason :(
#define MNT_DETACH 2
#endif

static volatile sig_atomic_t terminated = 0;

static void sig_hup(int sig)
{
	// no-op, but will cause the ppoll to exit early
//	write(1,"*",1);
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

ConnectionList list = {0};

void receivedFlush(int id) {
	Connection* current = list.first;
	while (current) {
		Allocation* ptr = current->firstAllocation;
		while (ptr) {
			if ( (ptr->id & BLOCK_ID_MASK) == id && (ptr->flags&ALLOCATION_FLAGS_ISCOMMANDQUEUE) ) {
				ptr->flags |= ALLOCATION_FLAGS_FLUSHDONE;
				kill(getpid(), SIGHUP);
				return;
			}
			ptr = ptr->nextAllocation;
		}
		current = current->nextConnection;
	}
	printf("flush %x not found \n", id);
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

	srand(time(NULL));
	void* blockManagementData = blockManagementInit();
	if (!Screen_open()) {
		syslog(LOG_ERR, "cannot open screen");
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

	SPU_HANDLE thread = _init_spu_thread(blockManagementData, 1);

	SPU_HANDLE renderThread[NUMBER_OF_RENDER_SPU_THREADS];
	for (int i=0; i<NUMBER_OF_RENDER_SPU_THREADS; i++)
		renderThread[i] = _init_spu_thread(blockManagementGetRenderTasksPointer(), 0);

	syslog(LOG_INFO, "accepting connections");

	int connectionCount = 0;

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
		timeout.tv_sec = 1;
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
			// printf("process %08x\n", conn);
			processOutstandingRequests(conn);
			conn = conn->nextConnection;
		}
		Connection** closed = &list.firstClosed;
		while (*closed) {
			Connection* conn = *closed;
			processOutstandingRequests(conn);
			if (conn->firstAllocation == NULL) {
				*closed = conn->nextConnection;
				close(conn->fd);
				free(conn);
			} else {
				//printf("connection %x still has allocation %x [%x] address %x\n", 
				//	conn, conn->firstAllocation->id, 
				//	conn->firstAllocation, conn->firstAllocation->buffer);
				closed = &(conn->nextConnection);
			}
		}
	}

	blockManagementDestroy();
	close(server);
	syslog(LOG_INFO, "exiting");

	Screen_close();

	sa.sa_handler = sig_hup_onexit;
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);

	printf("Killing SPU threads...\n");
	_exit_spu_thread(thread);

	for (int i=0; i<NUMBER_OF_RENDER_SPU_THREADS; i++)
		_exit_spu_thread( renderThread[i] );

	umount2(mountname, MNT_FORCE | MNT_DETACH);
	rmdir(mountname);
}

