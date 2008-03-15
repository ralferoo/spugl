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

// #define DEBUG_QUEUE 

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

static vector unsigned short hash0 = (vector unsigned short)(-1);
static vector unsigned short hash1 = (vector unsigned short)(-1);
static vector unsigned short hash2 = (vector unsigned short)(-1);
static vector unsigned short hash3 = (vector unsigned short)(-1);

static signed char chained_block[33];
static int busy = 0;

static inline void clear_hash(int idx)
{
	vector unsigned short empty = spu_splats(-1);

	// remove old match (if any) from hash table and update hash table with new entry
	const static vector unsigned int shift = { 0x80000000,0x800000,0x8000,0x80 };
	vector unsigned int shifted = spu_rl(shift,32-idx);

	hash0 = spu_sel(hash0,empty,spu_maskh(spu_extract(shifted,0)));
	hash1 = spu_sel(hash1,empty,spu_maskh(spu_extract(shifted,1)));
	hash2 = spu_sel(hash2,empty,spu_maskh(spu_extract(shifted,2)));
	hash3 = spu_sel(hash3,empty,spu_maskh(spu_extract(shifted,3)));
}

static inline unsigned int chain_hash(unsigned short hash, int idx)
{
	vector unsigned short compare = spu_splats(hash);

	// mask* will be all-ones if match, all-zeros if not
	vector unsigned short mask0 = spu_cmpeq(compare, hash0);
	vector unsigned short mask1 = spu_cmpeq(compare, hash1);
	vector unsigned short mask2 = spu_cmpeq(compare, hash2);
	vector unsigned short mask3 = spu_cmpeq(compare, hash3);

	// remove old match (if any) from hash table and update hash table with new entry
	const static vector unsigned int shift = { 0x80000000,0x800000,0x8000,0x80 };
	vector unsigned int shifted = spu_rl(shift,32-idx);

	hash0 = spu_sel(spu_or(hash0,mask0),compare,spu_maskh(spu_extract(shifted,0)));
	hash1 = spu_sel(spu_or(hash1,mask1),compare,spu_maskh(spu_extract(shifted,1)));
	hash2 = spu_sel(spu_or(hash2,mask2),compare,spu_maskh(spu_extract(shifted,2)));
	hash3 = spu_sel(spu_or(hash3,mask3),compare,spu_maskh(spu_extract(shifted,3)));
	
	// determine what index previous held the hash
	const static vector unsigned char sel01 = { 128, 128, 19, 3 };
	const static vector unsigned char sel23 = { 19, 3, 128, 128 };
	vector unsigned int old_v = spu_cntlz(spu_or(
		spu_shuffle(spu_gather(mask0), spu_gather(mask1), sel01), 
		spu_shuffle(spu_gather(mask2), spu_gather(mask3), sel23)));
	unsigned int old = spu_extract(old_v,0);

//	if (1) return -1;

//	if (old!=32) printf("%08x: hash %04x held by %2d now held by %2d     \n", ready_blocks, hash, old, idx);

//		for (int c=0; c<33; c++) printf("%2d|",c); printf("\n");
//		for (int c=0; c<33; c++) printf("%2d ",chained_block[c]); printf("\n");

	chained_block[old] = idx;
	return spu_extract(spu_cmpeq(old_v,spu_splats((unsigned int)32)),0);
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
#ifdef DEBUG_QUEUE 
	printf("process_queue\n");
#endif
	mfc_write_tag_mask((1<<NUMBER_OF_ACTIVE_BLOCKS)-1);
	unsigned int completed = mfc_read_tag_status_immediate();

	vector unsigned short idle_blocks = spu_cmpeq(active_blocks,(vector unsigned short)(-1));

	unsigned int mask;
	int next_bit;
	int next_mask;
	for (int i=0, mask=1; i<NUMBER_OF_ACTIVE_BLOCKS; i++, mask<<=1) {
		if (completed&mask) {
			if (spu_extract(idle_blocks, i)==0) {
				unsigned short id = spu_extract(active_blocks,i);
				Block* block = &blocks[id];
#ifdef DEBUG_QUEUE 
				printf("calling process %x on block %x(%d) id=%d\n", block->process, block, i, id);
#endif
				BlockHandler* next = block->process(block->process, block, &active[i], i);
				if (next) {
					block->process = next;
#ifdef DEBUG_QUEUE 
					printf("stalled %d: %d\n", i, id);
#endif
				} else {
#ifdef DEBUG_QUEUE 
					printf("finished %d: %d\n", i, id);
#endif
					busy--;
					block->process = next;
					free_blocks |= 1<<id;
					active_blocks = spu_insert( (unsigned short)-1,
								    active_blocks, i);
					next_bit = chained_block[id];
					clear_hash(id);
					if (next_bit>=0) {
//						printf("from %d, next_bit %d\n", id, next_bit);
						chained_block[id] = -1;
						ready_blocks |= 1<<next_bit;
						goto queue_chained;
					}
					goto queue_next;
				}
			} else {
queue_next:
				if (ready_blocks) {
					unsigned int rest_mask = ((1<<last_block_started)-1);
					int bit1 = first_bit(ready_blocks);
					int bit2 = first_bit(ready_blocks & rest_mask);
					next_bit = bit2<0 ? bit1 : bit2;
queue_chained:
					next_mask = 1<<next_bit;
					ready_blocks &= ~next_mask;
					last_block_started = next_bit;
					active_blocks = spu_insert( (unsigned short)next_bit,
								    active_blocks, i);
					activate(&blocks[next_bit], &active[i], i);
					busy++;
#ifdef DEBUG_QUEUE 
					printf("queued %d: %d\n", i, next_bit);
#endif
				}
			}
		}
	}

	while (free_blocks && triangles[triangle_next_read].produce) {
		unsigned int rest_mask = ((1<<last_block_added)-1);
		int bit1 = first_bit(free_blocks);
		int bit2 = first_bit(free_blocks & rest_mask);
		int next_bit = bit2<0 ? bit1 : bit2;
		int next_mask = 1<<next_bit;

		Triangle* tri = &triangles[triangle_next_read];
#ifdef DEBUG_QUEUE 
		printf("calling triangle produce on tri %d(%x) on block %d\n", triangle_next_read, tri, next_bit);
#endif
		int hash = tri->produce(tri, &blocks[next_bit]);
//		blocks[next_bit].hash = hash;

		unsigned int chain = chain_hash(hash, next_bit);
		ready_blocks |= next_mask & chain;
		last_block_added = next_bit;
		free_blocks &= ~next_mask;

		if (tri->produce == 0) {
#ifdef DEBUG_QUEUE 
			printf("finished producing on %d\n", triangle_next_read);
#endif
			triangle_next_read = (triangle_next_read+1)%NUMBER_OF_TRIS;
			busy--;
		}
	}

	while (triangles[triangle_next_write].count==0) {
		Triangle* tri = &triangles[triangle_next_write];
#ifdef DEBUG_QUEUE 
			printf("calling generator on %d\n", triangle_next_write);
#endif
		if ( (*generator)(tri) ) {
#ifdef DEBUG_QUEUE 
			printf("generated triangle on %d\n", triangle_next_write);
#endif
			triangle_next_write = (triangle_next_write+1)%NUMBER_OF_TRIS;
			busy++;
		} else {
#ifdef DEBUG_QUEUE 
			printf("done generating\n");
#endif
			break;
		}
	}
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
	for (int i=0; i<sizeof(chained_block); i++)
		chained_block[i] = -1;
}

int has_finished()
{
	return busy==0;
}
