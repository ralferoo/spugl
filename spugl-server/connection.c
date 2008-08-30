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

#include <syslog.h>
#include <stdio.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>

#include "connection.h"
#include "../spugl-client/daemon.h"

void handleConnect(struct Connection* connection) {
	char buffer[512];
	sprintf(buffer, "got new connection on fd %d, address %x\n", connection->fd, connection);
	syslog(LOG_INFO, buffer);
}

void handleDisconnect(struct Connection* connection) {
	char buffer[512];
	sprintf(buffer, "lost connection on fd %d, address %x\n", connection->fd, connection);
	syslog(LOG_INFO, buffer);
}

int handleConnectionData(struct Connection* connection) {
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
	printf("read len %d\n", len);

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
		default: 
			sprintf(buffer, "invalid request command %d on fd %d", request.command, connection->fd);
			syslog(LOG_ERR, buffer);
			return 1;
	}
				
	sprintf(buffer, "processed command %d on fd %d, address %x\n", request.command, connection->fd);
	syslog(LOG_INFO, buffer);

	return 0;
}


