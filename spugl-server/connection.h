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

extern char SPUGL_VERSION[];

struct Allocation;

//////////////////////////////////////////////////////////////////////////////
//
// List of client connections in the system

struct Connection {
	// populated by main.c
	struct Connection* nextConnection;
	int fd;
	
	// populated by connection.c
	struct Allocation* firstAllocation;
};

// does any post connection initialisation that might be required
void handleConnect(struct Connection* connection);

// does any post disconnection tear down that might be required
void handleDisconnect(struct Connection* connection);

// handle a request from client
// return non-zero if we should disconnect the client
int handleConnectionData(struct Connection* connection);


//////////////////////////////////////////////////////////////////////////////
//
// List of client's allocations

struct Allocation {
	struct Allocation* nextAllocation;
	int fd;
	void* buffer;
	unsigned long size;
	unsigned long id;
};


