/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

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
	while (ready_job_queues) {
//		debug_queue();
		int id = FIRST_JOB(ready_job_queues);
//		printf("Job %d waiting...\n", id);
		Queue* q = &job_queue[id];
		void (*handler)(Queue*) = q->handler;
		unsigned int mask = 1<<id;
		//BLOCK_JOB(q);
		ready_job_queues &= ~mask;
		//QUEUE_JOB(q,0);
		q->handler = 0;
		q->name = "processed";
//		printf("calling %lx\n", handler);
		handler(q);
//		printf("returned from %lx\n", handler);
		if (!(q->handler)) {
			free_job_queues |= mask;
//			printf("freeing job %lx\n", mask);
		}
	}
//	printf("No more ready jobs...\n");
//	debug_queue();
//	printf("...\n");
	}
}
