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

#include "../client/queue.h"

// This contains a list of command queues and a chain to the next list
// (this is private to server and should move there!)

struct CommandQueueList {
	struct CommandQueueList* next;
	struct CommandQueue* queues[31];
};

