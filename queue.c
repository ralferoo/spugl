/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> 
 *
 * This library may not be used or distributed without a licence, please
 * contact me for information if you wish to use it.
 *
 ****************************************************************************/

//#define DEBUG_QUEUE 

#include "spuregs.h"
#include "queue.h"

Queue job_queue[NUMBER_OF_QUEUE_JOBS];

unsigned int free_job_queues = ALL_QUEUE_JOBS;

unsigned int ready_job_queues = 0;

int has_finished()
{
	return free_job_queues == ALL_QUEUE_JOBS;
}

void debug_queue(void)
{
	unsigned int mask = 1;
	int i;
	for (i=0; i<NUMBER_OF_QUEUE_JOBS; i++, mask<<=1) {
		Queue* q = &job_queue[i];
		if (!(free_job_queues&mask)) {
			printf("job %2d(%c): dispatcher %05lx DMA %08lx next %2d \"%s\"\n", 
				i, ready_job_queues&mask?'R':'S', q->handler, q->dmamask, q->next,
				q->name);
		}
	}
}

void dummy_handler(Queue* queue)
{
	printf("Dummy handler invoked\n");
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
	if (ready_job_queues) {
#ifdef DEBUG_QUEUE 
		debug_queue();
#endif
		unsigned int consider = ~0;

		while (ready_job_queues&consider) {
			int id = FIRST_JOB(ready_job_queues&consider);
			consider = (1<<id)-1;
#ifdef DEBUG_QUEUE 
			printf("Job %d waiting...\n", id);
#endif
			Queue* q = &job_queue[id];
			void (*handler)(Queue*) = q->handler;
			unsigned int mask = 1<<id;
			//BLOCK_JOB(q);
			ready_job_queues &= ~mask;
			//QUEUE_JOB(q,0);
			q->handler = 0;
			q->name = "processed";
			handler(q);
			if (!(q->handler)) {
				free_job_queues |= mask;
				int next = job_queue[id].next;
				if (next>=0)
					ready_job_queues |= 1<<next;
			}
		}
	}
}
