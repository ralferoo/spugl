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

typedef struct __Allocation Allocation;
typedef struct __Connection Connection;
typedef struct __ConnectionList ConnectionList;
typedef enum __LOCK LOCK;

//////////////////////////////////////////////////////////////////////////////
//
// PPU/SPU locking system

enum __LOCK {
	LOCK_free,
	LOCK_SPU,
	LOCK_PPU_wait,
	LOCK_PPU_may_proceed,
	LOCK_PPU
};

void lock(LOCK* lock);
void unlock(LOCK* lock);

//////////////////////////////////////////////////////////////////////////////
//
// List of client connections in the system

struct __Connection {
	// populated by main.c
	Connection* nextConnection;
	int fd;
	
	// populated by connection.c
	Allocation* firstAllocation;
	LOCK lock;
};

// does any post connection initialisation that might be required
void handleConnect(Connection* connection);

// does any post disconnection tear down that might be required
void handleDisconnect(Connection* connection);

// handle a request from client
// return non-zero if we should disconnect the client
int handleConnectionData(Connection* connection, char* mountname);

// send any outstanding messages
void processOutstandingRequests(Connection* connection);

//////////////////////////////////////////////////////////////////////////////
//
// List of client's allocations

struct __Allocation {
	Allocation* nextAllocation;
	void* buffer;
	unsigned long size;
	unsigned long id;
	int fd;
	int flags;
	int locks;
};

#define ALLOCATION_FLAGS_ISCOMMANDQUEUE 1
#define ALLOCATION_FLAGS_FLUSHWAIT 2
#define ALLOCATION_FLAGS_FLUSHDONE 4
#define ALLOCATION_FLAGS_FREEWAIT 8
#define ALLOCATION_FLAGS_FREEDONE 16

//////////////////////////////////////////////////////////////////////////////
//
// Main list of connections on server

struct __ConnectionList {
	Connection* first;
	Connection* firstClosed;
	LOCK lock;
};

