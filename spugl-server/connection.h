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
// PPU/SPU locking system

enum LOCK {
	LOCK_free,
	LOCK_SPU,
	LOCK_PPU_wait,
	LOCK_PPU_may_proceed,
	LOCK_PPU
};

void lock(enum LOCK* lock);
void unlock(enum LOCK* lock);

//////////////////////////////////////////////////////////////////////////////
//
// List of client connections in the system

struct Connection {
	// populated by main.c
	struct Connection* nextConnection;
	int fd;
	
	// populated by connection.c
	struct Allocation* firstAllocation;
	enum LOCK lock;
};

// does any post connection initialisation that might be required
void handleConnect(struct Connection* connection);

// does any post disconnection tear down that might be required
void handleDisconnect(struct Connection* connection);

// handle a request from client
// return non-zero if we should disconnect the client
int handleConnectionData(struct Connection* connection, char* mountname);

// send any outstanding messages
void processOutstandingRequests(struct Connection* connection);

//////////////////////////////////////////////////////////////////////////////
//
// List of client's allocations

struct Allocation {
	struct Allocation* nextAllocation;
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
