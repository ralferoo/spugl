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

struct Connection {
	struct Connection* nextConnection;
	int fd;
};

void handleConnect(struct Connection* connection);
void handleDisconnect(struct Connection* connection);
void handleConnectionData(struct Connection* connection);
