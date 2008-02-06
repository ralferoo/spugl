/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

#include "queue.h"

Queue job_queue[NUMBER_OF_QUEUE_JOBS];

// this rather convuluted expression is just (1<<n)-1 but rearranged to 
// get rid of the warnings :(
unsigned int free_job_queues = (((1<<(NUMBER_OF_QUEUE_JOBS-2))-1)<<2)|3;
//unsigned int free_job_queues = (1<<NUMBER_OF_QUEUE_JOBS)-1;
