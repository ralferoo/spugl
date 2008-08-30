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
#include "connection.h"

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

void handleConnectionData(struct Connection* connection) {
	syslog(LOG_INFO, "recieved data");
}


