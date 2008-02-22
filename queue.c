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
#include <spu_mfcio.h>
#include <stdio.h>

Triangle triangles[NUMBER_OF_TRIS];
Block blocks[NUMBER_OF_QUEUED_BLOCKS];

unsigned int triangle_count = 0;
unsigned int triangle_next_read = 0;
unsigned int triangle_next_write = 0;

unsigned int free_blocks = ((((1<<(NUMBER_OF_QUEUED_BLOCKS-2))-1)<<2)|3);
unsigned int ready_blocks = 0;
unsigned int last_block_started = 0;
unsigned int last_block_added = 0;
vector unsigned short active_blocks = (vector unsigned short)(-1);

void process_queue(TriangleGenerator* generator)
{	
	mfc_write_tag_mask((1<<NUMBER_OF_ACTIVE_BLOCKS)-1);
	unsigned int completed = mfc_read_tag_status_immediate();

	vector unsigned short idle_blocks = spu_cmpeq(active_blocks,(vector unsigned short)(-1));

	unsigned int mask;
//	write(1,"!",1);
	for (int i=0, mask=1; i<NUMBER_OF_ACTIVE_BLOCKS; i++, mask<<=1) {
		if (completed&mask) {
			if (spu_extract(idle_blocks, i)==0) {
				unsigned short id = spu_extract(active_blocks,i);
//				printf("busy %d: %d\n", i, id);
				Block* block = &blocks[id];
				BlockHandler* next = block->process(block->process, block);
				if (next) {
					block->process = next;
				} else {
//					printf("finished %d: %d\n", i, id);
					block->process = next;
					free_blocks |= 1<<id;
					active_blocks = spu_insert( (unsigned short)-1,
								    active_blocks, i);
				}
			} else {
				if (ready_blocks) {
					unsigned int rest_mask = ((1<<last_block_started)-1);
					int bit1 = first_bit(ready_blocks);
					int bit2 = first_bit(ready_blocks & rest_mask);
					int next_bit = bit2<0 ? bit1 : bit2;
					int next_mask = 1<<next_bit;
					ready_blocks &= ~next_mask;
					last_block_started = next_bit;
					active_blocks = spu_insert( (unsigned short)next_bit,
								    active_blocks, i);
//					printf("queued %d: %d\n", i, next_bit);
				}
			}
		}
	}

	while (free_blocks && triangles[triangle_next_read].count) {
//		write(1,"@",1);
		unsigned int rest_mask = ((1<<last_block_added)-1);
		int bit1 = first_bit(free_blocks);
		int bit2 = first_bit(free_blocks & rest_mask);
		int next_bit = bit2<0 ? bit1 : bit2;
		int next_mask = 1<<next_bit;

		Triangle* tri = &triangles[triangle_next_read];
//		printf("calling triangle produce %08x on %d\n", tri->produce, next_bit);
		int next = tri->produce(tri, &blocks[next_bit]);
		if (next) {
			last_block_added = next_bit;
			ready_blocks |= next_mask;
			free_blocks &= ~next_mask;
		} else {
//			printf("finished producing on %d\n", triangle_next_read);
			triangle_next_read = (triangle_next_read+1)%NUMBER_OF_TRIS;
		}
	}

	while (triangles[triangle_next_write].count==0) {
//		write(1,"#",1);
		Triangle* tri = &triangles[triangle_next_write];
		if ( (*generator)(tri) ) {
//			printf("generated triangle on %d\n", triangle_next_write);
			triangle_next_write = (triangle_next_write+1)%NUMBER_OF_TRIS;
		} else {
			break;
		}
	}
}

void init_queue(void)
{
	for (int i=0; i<NUMBER_OF_TRIS; i++) {
		triangles[i].count = 0;
		triangles[i].produce = 0;
	}
}



/*
Queue job_queue[NUMBER_OF_QUEUE_JOBS];

unsigned int free_job_queues = ALL_QUEUE_JOBS;
unsigned int ready_job_queues = 0;
unsigned int dma_wait_job_queues = 0;

*/
int has_finished()
{
	return 1; //free_job_queues == ALL_QUEUE_JOBS;
}
/*

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
	unsigned int consider = ~0;

	if (ready_job_queues) {
#ifdef DEBUG_QUEUE 
		debug_queue();
#endif

		while (ready_job_queues&consider) {
			if (dma_wait_job_queues) {
				mfc_write_tag_mask(dma_wait_job_queues);
				unsigned long completed = mfc_read_tag_status_immediate();
//				printf("DMA completion %lx is %lx\n", dma_wait_job_queues, completed);
				dma_wait_job_queues &= ~completed;
				ready_job_queues |= completed;
				consider |= completed;
			}

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
*/

