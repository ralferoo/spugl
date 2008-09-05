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

#include "connection.h"

//void lock(LOCK* lock);
//void unlock(LOCK* lock);

//////////////////////////////////////////////////////////////////////////////
//
// CONNECTION ROUTINES

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
// BLOCK MANAGEMENT SYSTEM

// initialises the block management system
void		blockManagementInit();

// frees any memory used by the block management system, returns non-zero on success
int 		blockManagementDestroy();

// allocates a block from the system, storing the EA alongside it, returns block ID
// the initial usage count is set to 0
unsigned int	blockManagementAllocateBlock(void* ptr, int commandQueue);

// decrement the usage count of the block
void		blockManagementBlockCountDispose(unsigned int id);

// checks to see if a particular block ID can now be freed
int		blockManagementTryFree(unsigned int id);

//////////////////////////////////////////////////////////////////////////////

SPU_HANDLE _init_spu_thread(ConnectionList* list, int master);
int _exit_spu_thread(SPU_HANDLE context);
