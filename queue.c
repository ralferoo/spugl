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

unsigned int ready_job_queues = 0;

void debug_queue(void)
{
	unsigned int mask = 1;
	int i;
	for (i=0; i<NUMBER_OF_QUEUE_JOBS; i++, mask<<=1) {
		Queue* q = &job_queue[i];
		if (!(free_job_queues&mask)) {
			printf("job %2d(%c): dispatcher %05lx DMA %08lx next %2d\n", 
				i, ready_job_queues&mask?'R':'S', q->handler, q->dmamask, q->next);
		}
	}
}

void dummy_handler(Queue* queue)
{
	QUEUE_JOB(queue,0);
}

void init_queue(void)
{
	int i;
	for (i=0; i<NUMBER_OF_QUEUE_JOBS; i++) {
		job_queue[i].id = i;
		job_queue[i].next = -1;
	}
	
	QUEUE_JOB(&job_queue[0],dummy_handler);
	READY_JOB(&job_queue[0]);
	debug_queue();
}

void process_queue(void)
{
//	debug_queue();
}
