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

#include "spuregs.h"
#include "queue.h"
#include <spu_mfcio.h>
#include <stdio.h>

static Triangle triangles[NUMBER_OF_TRIS];
static Block blocks[NUMBER_OF_QUEUED_BLOCKS];
static ActiveBlock active[NUMBER_OF_ACTIVE_BLOCKS];
static ActiveBlockFlush* block_flusher = 0;

static unsigned int triangle_count = 0;
static unsigned int triangle_next_read = 0;
static unsigned int triangle_next_write = 0;

static unsigned int free_blocks = ((((1<<(NUMBER_OF_QUEUED_BLOCKS-2))-1)<<2)|3);
static unsigned int ready_blocks = 0;
static unsigned int last_block_started = 0;
static unsigned int last_block_added = 0;
static vector unsigned short active_blocks = (vector unsigned short)(-1);

static inline void debug()
{
/*
	printf("%08x: ",ready_blocks);
	for (int q=0; q<NUMBER_OF_TRIS; q++) {
		printf("%3d%c",(signed short)triangles[q].count,triangles[q].produce?'P':' ');
	}
	printf("\n");
*/
}

void flush_queue()
{
	for (int i=0; i<NUMBER_OF_ACTIVE_BLOCKS; i++) {
		(*block_flusher)(&active[i],i);
	}
	mfc_write_tag_mask((1<<NUMBER_OF_ACTIVE_BLOCKS)-1);
	mfc_read_tag_status_all();
}

void process_queue(TriangleGenerator* generator, BlockActivater* activate)
{	
	mfc_write_tag_mask((1<<NUMBER_OF_ACTIVE_BLOCKS)-1);
	unsigned int completed = mfc_read_tag_status_immediate();

	vector unsigned short idle_blocks = spu_cmpeq(active_blocks,(vector unsigned short)(-1));

	unsigned int mask;
	for (int i=0, mask=1; i<NUMBER_OF_ACTIVE_BLOCKS; i++, mask<<=1) {
		if (completed&mask) {
			if (spu_extract(idle_blocks, i)==0) {
				unsigned short id = spu_extract(active_blocks,i);
//				printf("busy %d: %d\n", i, id);
				Block* block = &blocks[id];
				BlockHandler* next = block->process(block->process, block, i);
				if (next) {
					block->process = next;
				} else {
//					printf("finished %d: %d\n", i, id);
					block->process = next;
					free_blocks |= 1<<id;
					active_blocks = spu_insert( (unsigned short)-1,
								    active_blocks, i);
					goto queue_next;
				}
			} else {
queue_next:
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
					activate(&blocks[next_bit], &active[i], i);
//					printf("queued %d: %d\n", i, next_bit);
				}
			}
		}
	}

	debug();

	while (free_blocks && triangles[triangle_next_read].produce) {
		unsigned int rest_mask = ((1<<last_block_added)-1);
		int bit1 = first_bit(free_blocks);
		int bit2 = first_bit(free_blocks & rest_mask);
		int next_bit = bit2<0 ? bit1 : bit2;
		int next_mask = 1<<next_bit;

		Triangle* tri = &triangles[triangle_next_read];
//		printf("calling triangle produce on tri %d(%x) on block %d\n", triangle_next_read, tri, next_bit);
		int next = tri->produce(tri, &blocks[next_bit]);
		last_block_added = next_bit;
		ready_blocks |= next_mask;
		free_blocks &= ~next_mask;
		if (next) {
		} else {
//			printf("finished producing on %d\n", triangle_next_read);
			tri->produce = 0;
			triangle_next_read = (triangle_next_read+1)%NUMBER_OF_TRIS;
		}
	}

	if (triangles[triangle_next_write].count!=0) {
//		printf("t[%d].c=%d\n",triangle_next_write, triangles[triangle_next_write].count);
	}

	debug();

	while (triangles[triangle_next_write].count==0) {
		Triangle* tri = &triangles[triangle_next_write];
		if ( (*generator)(tri) ) {
//			printf("generated triangle on %d\n", triangle_next_write);
			triangle_next_write = (triangle_next_write+1)%NUMBER_OF_TRIS;
		} else {
			break;
		}
	}

	debug();
}

void init_queue(ActiveBlockInit* init, ActiveBlockFlush* flush)
{
	block_flusher = flush;
	for (int i=0; i<NUMBER_OF_TRIS; i++) {
		triangles[i].count = 0;
		triangles[i].produce = 0;
	}
	for (int j=0; j<NUMBER_OF_ACTIVE_BLOCKS; j++) {
		(*init)(&active[j]);
	}
}

int has_finished()
{
	return ready_blocks==0 && triangles[triangle_next_read].count==0;
}
