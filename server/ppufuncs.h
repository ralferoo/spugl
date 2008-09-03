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

void lock(LOCK* lock);
void unlock(LOCK* lock);

//////////////////////////////////////////////////////////////////////////////

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

SPU_HANDLE _init_spu_thread(ConnectionList* list, int master);
int _exit_spu_thread(SPU_HANDLE context);
