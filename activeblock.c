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

#include <spu_mfcio.h>
#include "fifo.h"
#include "struct.h"
#include "queue.h"

//#define DEBUG_1
//#define DEBUG_2

extern _bitmap_image screen;

//////////////////////////////////////////////////////////////////////////////

void activeBlockInit(ActiveBlock* active)
{
	printf("init active block %x\n", active);
	active->new_dma = &active->dma1[0];
	active->current_dma = &active->dma2[0];
	active->current_length = 0;
	active->eah = 0;
}

void activeBlockFlush(ActiveBlock* active, int tag)
{
	unsigned long len = active->current_length;

	if (len) {
		unsigned long eah = active->eah;
		unsigned long eal = (unsigned long) ((void*)active->current_dma);
		spu_mfcdma64(&active->pixels[0], eah, eal, len, tag, MFC_PUTLF_CMD);
		active->current_length = 0;
//		printf("flush_screen_block: block=%lx ea=%lx:%lx len=%d                 \n",
//			active, eah, eal, len);
	}
}

//////////////////////////////////////////////////////////////////////////////

static inline void build_blit_list(
		vec_uint4* dma_list_buffer,
		unsigned long eal, unsigned long stride)
{
#ifdef DEBUG_1
	printf("build_blit_list: eal=%lx stride=%d\n", eal, stride);
#endif

	unsigned long eal1 = eal + stride;
	unsigned long stride2 = 2 * stride;
	unsigned long stride8 = 8 * stride;
	vec_uint4 block0 = { 128, eal, 128, eal1 };
	vec_uint4 step2 = { 0, stride2, 0, stride2};
	vec_uint4 step4 = spu_add(step2, step2);
	vec_uint4 step6 = spu_add(step4, step2);
	vec_uint4 step8 = { 0, stride8, 0, stride8};
	vec_uint4 step16 = spu_add(step8, step8);
	vec_uint4 block2 = spu_add(block0, step2);
	vec_uint4 block4 = spu_add(block0, step4);
	vec_uint4 block6 = spu_add(block0, step6);
	vec_uint4 block8 = spu_add(block0, step8);
	vec_uint4 block10 = spu_add(block8, step2);
	vec_uint4 block12 = spu_add(block8, step4);
	vec_uint4 block14 = spu_add(block8, step6);
	vec_uint4 block16 = spu_add(block8, step8);
	vec_uint4 block18 = spu_add(step16, block2);
	vec_uint4 block20 = spu_add(step16, block4);
	vec_uint4 block22 = spu_add(step16, block6);
	vec_uint4 block24 = spu_add(step16, block8);
	vec_uint4 block26 = spu_add(step16, block10);
	vec_uint4 block28 = spu_add(step16, block12);
	vec_uint4 block30 = spu_add(step16, block14);

	dma_list_buffer[0] = block0;
	dma_list_buffer[1] = block2;
	dma_list_buffer[2] = block4;
	dma_list_buffer[3] = block6;
	dma_list_buffer[4] = block8;
	dma_list_buffer[5] = block10;
	dma_list_buffer[6] = block12;
	dma_list_buffer[7] = block14;

	dma_list_buffer[8] = block16;
	dma_list_buffer[9] = block18;
	dma_list_buffer[10] = block20;
	dma_list_buffer[11] = block22;
	dma_list_buffer[12] = block24;
	dma_list_buffer[13] = block26;
	dma_list_buffer[14] = block28;
	dma_list_buffer[15] = block30;

#ifdef DEBUG_1
	int i,j;
	for (i=0;i<16;i++) {
		printf("%lx(%x): ", &dma_list_buffer[i], i);
		for (j=0; j<4; j++) {
			printf("%lx ", spu_extract(dma_list_buffer[i], j));
		}
		printf("\n");
	}
#endif
}

//////////////////////////////////////////////////////////////////////////////

// BUGS: this only works if lines is a multiple of 2, and <=32
void load_screen_block(ActiveBlock* block, 
		unsigned long long ea, unsigned long stride, unsigned int lines, unsigned int tag)
{
#ifdef DEBUG_2
	printf("load_screen_block: block=%lx screen=%llx stride=%d lines=%d\n",
		block, ea, stride, lines);
#endif

	unsigned long eah = ea >> 32;
	unsigned long eal = ((unsigned long) (ea&0xffffffff));

	build_blit_list(block->new_dma, eal, stride);

	unsigned long old_size = block->current_length;
	unsigned long half_new_size = lines * 4;
	unsigned long new_size = half_new_size * 2;
	unsigned long store_new_size = new_size;

	unsigned long eal_old = (unsigned long) ((void*)block->current_dma);
	unsigned long eal_new = (unsigned long) ((void*)block->new_dma);

	// if this is an unused block, then we have no data to blit out
	// so to avoid branches, split the read block in half
//TODO: why do this fix work???
	unsigned long is_new = 0; //cmp_eq(old_size, 0);
	unsigned long cmd = if_then_else(is_new, MFC_GETL_CMD, MFC_PUTLF_CMD);
	eal_old = if_then_else(is_new, eal_new+half_new_size, eal_old);
	new_size = if_then_else(is_new, half_new_size, new_size);
	old_size = if_then_else(is_new, half_new_size, old_size);

#ifdef DEBUG_2
	printf("old_size %d, is_new %d store_new %d\n",
		block->current_length, is_new&1, store_new_size);
	printf("DMA[%02X]: ls=%lx eah=%lx list=%lx, size=%d, tag=%d\n",
		cmd, &block->pixels[0],eah,eal_old,old_size,tag);
	printf("DMA[%02X]: ls=%lx eah=%lx list=%lx, size=%d, tag=%d\n",
		MFC_GETLF_CMD, &block->pixels[0],eah,eal_new,new_size,tag);
#endif

	spu_mfcdma64(&block->pixels[0],eah,eal_old,old_size,tag, cmd);
	spu_mfcdma64(&block->pixels[0],eah,eal_new,new_size,tag, MFC_GETLF_CMD);

	// update the buffer pointers
	block->current_length = store_new_size;
	vec_uint4* t = block->current_dma; 
	block->current_dma = block->new_dma; 
	block->new_dma = t;
	block->eah = eah;
}

//////////////////////////////////////////////////////////////////////////////

void blockActivater(Block* block, ActiveBlock* active, int tag)
{
//	printf("activating block %x on %x (tag %d)\n", block, active, tag);

	unsigned int bx=block->bx, by=block->by;
	u64 scrbuf = screen.address + screen.bytes_per_line*by*32+bx*128;

//	printf("   -> screen address %llx width %d\n", scrbuf, screen.bytes_per_line);

	load_screen_block(active, scrbuf, screen.bytes_per_line, 32, tag);
/*
	screen_block* current_block = &buffer;
	wait_screen_block(current_block);
*/

	block->pixels = (vec_uint4*) ((void*)&active->pixels[0]);
	block->tex_temp = (char*) ((void*)&active->textemp[0]);
	block->tex_override = -1;
}




#ifdef __IGNORE_ALL_JUNK


// #define DEBUG_2		// corrupt each block we load in
// #define DEBUG_3

#define COUNT_BLOCKED_DMA
//#define TRY_TO_CULL_BLOCKS

int qs(int a) { return a>>5; }

unsigned int qu(unsigned int a) { return a>>5; }

float myrecip(float b) { return 1.0/b; }
float mydiv(float a,float b) { return a/b; }

//////////////////////////////////////////////////////////////////////////////

typedef struct {
	u32 pixels[32*32];
	char textemp[32*8];
	vec_uint4 dma1[16];
	vec_uint4 dma2[16];

	vec_uint4* current_dma;
	vec_uint4* new_dma;
	unsigned long current_length;
	unsigned long eah;
	unsigned long tagid;
	unsigned long pad;
} screen_block __attribute__((aligned(16)));

void init_screen_block(screen_block* block, unsigned long tagid)
{
	block->new_dma = &block->dma1[0];
	block->current_dma = &block->dma2[0];
	block->current_length = 0;
	block->tagid = tagid;
//	printf("new block %lx, dma lists at %lx and %lx\n",
//		block, block->current_dma, block->new_dma);
}

void flush_screen_block(screen_block* block)
{
}

extern SPU_CONTROL control;

unsigned long wait_for_dma(unsigned long mask)
{
#ifndef COUNT_BLOCKED_DMA
	mfc_write_tag_mask(mask);
	unsigned long mask = mfc_read_tag_status_any();
	return mask;
#else
	unsigned long done;
	do {
		mfc_write_tag_mask(mask);
		done = mfc_read_tag_status_any();
		control.block_count+=9;		// guesstimate how much longer this takes than nop
	} while (!(done&mask));
	return done&mask;
#endif
}
unsigned long wait_screen_block(screen_block* block)
{
	return wait_for_dma(1 << block->tagid);
}


//////////////////////////////////////////////////////////////////////////////

screen_block buffer __attribute__((aligned(128)));

void _init_buffers()
{
	init_screen_block(&buffer, 31);
}

//////////////////////////////////////////////////////////////////////////////

#define NUMBER_TEX_MAPS 16
#define NUMBER_TEX_PIXELS (32*32)

typedef struct {
	u32 textureBuffer[NUMBER_TEX_PIXELS] __attribute__((aligned(128)));
} TextureBlock;

TextureBlock textureCache[NUMBER_TEX_MAPS] __attribute__((aligned(128)));

unsigned int freeTextureMaps = 0;
unsigned int lastLoadedTextureMap = 0;
vec_ushort8 TEXblitting1,TEXblitting2;

static inline void updateTextureCache(int index,int value) {
	if (index&1)
		TEXcache1 = spu_insert(value, TEXcache1, index>>1);
	else
		TEXcache2 = spu_insert(value, TEXcache2, index>>1);
}

void _init_texture_cache()
{
	u32* texture = &textureCache[0].textureBuffer[0];

	int i;
	for (i=0; i<NUMBER_TEX_PIXELS*NUMBER_TEX_MAPS; i++) {
		texture[i] = (i<<4) | (i<<9) | (i<<19);
	}


	freeTextureMaps = (1<<NUMBER_TEX_MAPS)-1;
	TEXcache1 = TEXcache2 = spu_splats((unsigned short)-1);
	TEXblitting1 = TEXblitting2 = spu_splats((unsigned short)-1);

	TEXcache1 = spu_insert(32, TEXcache1, 0);	// 0
	TEXcache1 = spu_insert(33, TEXcache1, 1);	// 2
	TEXcache1 = spu_insert(34, TEXcache1, 2);
	TEXcache1 = spu_insert(35, TEXcache1, 3);
	TEXcache1 = spu_insert(36, TEXcache1, 4);
	TEXcache1 = spu_insert(37, TEXcache1, 5);
	TEXcache1 = spu_insert(38, TEXcache1, 6);
	TEXcache1 = spu_insert(39, TEXcache1, 7);	// 14

	TEXcache2 = spu_insert(40, TEXcache2, 0);	// 1
	TEXcache2 = spu_insert(41, TEXcache2, 1);	// 3
	TEXcache2 = spu_insert(42, TEXcache2, 2);	// 5
	TEXcache2 = spu_insert(43, TEXcache2, 3);
	TEXcache2 = spu_insert(44, TEXcache2, 4);
	TEXcache2 = spu_insert(45, TEXcache2, 5);
	TEXcache2 = spu_insert(46, TEXcache2, 6);
	TEXcache2 = spu_insert(47, TEXcache2, 7);	// 15
}

//////////////////////////////////////////////////////////////////////////////

static const vec_float4 muls = {0.0f, 1.0f, 2.0f, 3.0f};
static const vec_float4 muls4 = {4.0f, 4.0f, 4.0f, 4.0f};
static const vec_float4 muls32 = {32.0f, 32.0f, 32.0f, 32.0f};
static const vec_float4 mulsn28 = {-28.0f, -28.0f, -28.0f, -28.0f};

static const vec_float4 muls31x = {0.0f, 31.0f, 0.0f, 31.0f};
static const vec_float4 muls31y = {0.0f, 0.0f, 31.0f, 31.0f};

//////////////////////////////////////////////////////////////////////////////

void block_handler(Queue* queue);
void finish_texmap_blit_handler(Queue* queue);

void real_block_handler(Queue* queue)
{
	vec_ushort8 need_mask=spu_cmpeq(needs,spu_splats((unsigned short)-1));
	vec_uint4 need_bits = spu_gather(need_mask);

	vec_uint4 need_any = spu_cmpeq(need_bits,spu_splats((unsigned int)0xff));
	int finished = spu_extract((vec_int4)need_any,0);
	queue->block.triangle->triangle.count += finished;

	vec_ushort8 tex_id_base = spu_splats((unsigned short)tri->triangle.tex_id_base);
	vec_ushort8 needs_sub = spu_sub(needs,tex_id_base);

	if (!finished) {
		vec_ushort8 TEXmerge1 = spu_splats((unsigned short)-1);
		vec_ushort8 TEXmerge2 = spu_splats((unsigned short)-1);
		int texturesMask = 0;

		int m,i;
		int n = spu_extract(need_bits,0);
		for (i=0,m=0x80; m && freeTextureMaps; m>>=1,i++) {
			if (n&m) {			// as soon as we find a 1 bit we are done
				break;
			}
			unsigned short want = spu_extract(needs,i);
			unsigned short want_sub = spu_extract(needs_sub,i);
			int nextBit1 = FIRST_JOB(freeTextureMaps);
			int nextBit2 = FIRST_JOB(freeTextureMaps & ((1<<lastLoadedTextureMap)-1) );
			int nextIndex = nextBit2<0 ? nextBit1 : nextBit2;
			int nextMask = 1<<nextIndex;

			if (! (freeTextureMaps&nextMask) ) {
				printf("ERROR: freeTextureMaps=%lx, next=%d,%d->%d, mask=%lx\n",
					freeTextureMaps, nextBit1, nextBit2, nextIndex, nextMask);
				break;
			}

			vec_ushort8 copy_cmp_0 = spu_splats(want);
			vec_ushort8 matches1_0 = spu_cmpeq(TEXblitting1,copy_cmp_0);
			vec_ushort8 matches2_0 = spu_cmpeq(TEXblitting2,copy_cmp_0);
			vec_ushort8 matches = spu_or(matches1_0, matches2_0);
			vec_uint4 match_bits = spu_gather(matches);

//			if (spu_extract(match_bits,0)) {
//				printf("want %d but already in progress\n", want);
//				continue;
//			}

/*
	printf("cache = %04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x\n",
		spu_extract(TEXcache1,0), spu_extract(TEXcache2,0),
		spu_extract(TEXcache1,1), spu_extract(TEXcache2,1),
		spu_extract(TEXcache1,2), spu_extract(TEXcache2,2),
		spu_extract(TEXcache1,3), spu_extract(TEXcache2,3),
		spu_extract(TEXcache1,4), spu_extract(TEXcache2,4),
		spu_extract(TEXcache1,5), spu_extract(TEXcache2,5),
		spu_extract(TEXcache1,6), spu_extract(TEXcache2,6),
		spu_extract(TEXcache1,7), spu_extract(TEXcache2,7));
*/
			if (nextIndex&1) {
				TEXcache2 = spu_insert(-1,   TEXcache2, nextIndex>>1);
				TEXmerge2 = spu_insert(want, TEXmerge2, nextIndex>>1);
				TEXblitting2 = spu_insert(want, TEXblitting2, nextIndex>>1);
			} else {
				TEXcache1 = spu_insert(-1,   TEXcache1, nextIndex>>1);
				TEXmerge1 = spu_insert(want, TEXmerge1, nextIndex>>1);
				TEXblitting1 = spu_insert(want, TEXblitting1, nextIndex>>1);
			}

/*
	printf(" now -> %04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x\n",
		spu_extract(TEXcache1,0), spu_extract(TEXcache2,0),
		spu_extract(TEXcache1,1), spu_extract(TEXcache2,1),
		spu_extract(TEXcache1,2), spu_extract(TEXcache2,2),
		spu_extract(TEXcache1,3), spu_extract(TEXcache2,3),
		spu_extract(TEXcache1,4), spu_extract(TEXcache2,4),
		spu_extract(TEXcache1,5), spu_extract(TEXcache2,5),
		spu_extract(TEXcache1,6), spu_extract(TEXcache2,6),
		spu_extract(TEXcache1,7), spu_extract(TEXcache2,7));
*/
			unsigned int desired = spu_extract(needs_sub, i);
//			unsigned long long ea = control.texture_hack[0] + (desired<<(5+5+2));
			unsigned long long ea = tri->triangle.texture_base + (desired<<(5+5+2));
			unsigned long len = 32*32*4;
			
			unsigned long eah = 0; // TODO: fix this
			unsigned long eal = ea & ~127;
			u32* texture = &textureCache[nextIndex].textureBuffer[0];

//			printf("desired %d->%d, reading to %x from %x:%08x len %d tag %d\n",
//				desired, nextIndex, texture, eah, eal, len, queue->id);

			if (mfc_stat_cmd_queue() == 0) {
				printf("DMA queue full; bailing...\n");
				break;
			}

			spu_mfcdma64(texture, eah, eal, len, queue->id, MFC_GET_CMD);

			freeTextureMaps &= ~nextMask;
			lastLoadedTextureMap = nextIndex;
			texturesMask |= nextMask;
		}

		dma_wait_job_queues |= 1 << queue->id;
		queue->block.TEXmerge1 = TEXmerge1;
		queue->block.TEXmerge2 = TEXmerge2;
		queue->block.texturesMask = texturesMask;
		QUEUE_JOB(queue, finish_texmap_blit_handler);
//		printf("queued texmap_blit on %d\n", queue->id);

/*
	printf("block %2d @(%2d,%2d) ", queue->id, queue->block.bx, queue->block.by);
	printf("needs %02x -> %04x %04x %04x %04x %04x %04x %04x %04x\n",
		spu_extract(need_bits,0),
		spu_extract(needs,0), spu_extract(needs,1),
		spu_extract(needs,2), spu_extract(needs,3),
		spu_extract(needs,4), spu_extract(needs,5),
		spu_extract(needs,6), spu_extract(needs,7)); 
*/
	} else {
	}
}

void finish_texmap_blit_handler(Queue* queue)
{
//	printf("finish_texmap_blit on %d\n", queue->id);

/*
	printf("merge = %04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x\n",
		spu_extract(queue->block.TEXmerge1,0), spu_extract(queue->block.TEXmerge2,0),
		spu_extract(queue->block.TEXmerge1,1), spu_extract(queue->block.TEXmerge2,1),
		spu_extract(queue->block.TEXmerge1,2), spu_extract(queue->block.TEXmerge2,2),
		spu_extract(queue->block.TEXmerge1,3), spu_extract(queue->block.TEXmerge2,3),
		spu_extract(queue->block.TEXmerge1,4), spu_extract(queue->block.TEXmerge2,4),
		spu_extract(queue->block.TEXmerge1,5), spu_extract(queue->block.TEXmerge2,5),
		spu_extract(queue->block.TEXmerge1,6), spu_extract(queue->block.TEXmerge2,6),
		spu_extract(queue->block.TEXmerge1,7), spu_extract(queue->block.TEXmerge2,7));

	printf("cache = %04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x\n",
		spu_extract(TEXcache1,0), spu_extract(TEXcache2,0),
		spu_extract(TEXcache1,1), spu_extract(TEXcache2,1),
		spu_extract(TEXcache1,2), spu_extract(TEXcache2,2),
		spu_extract(TEXcache1,3), spu_extract(TEXcache2,3),
		spu_extract(TEXcache1,4), spu_extract(TEXcache2,4),
		spu_extract(TEXcache1,5), spu_extract(TEXcache2,5),
		spu_extract(TEXcache1,6), spu_extract(TEXcache2,6),
		spu_extract(TEXcache1,7), spu_extract(TEXcache2,7));
*/
	TEXcache1 &= queue->block.TEXmerge1;
	TEXcache2 &= queue->block.TEXmerge2;
	freeTextureMaps |= queue->block.texturesMask;
	
/*
	printf(" now -> %04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x\n",
		spu_extract(TEXcache1,0), spu_extract(TEXcache2,0),
		spu_extract(TEXcache1,1), spu_extract(TEXcache2,1),
		spu_extract(TEXcache1,2), spu_extract(TEXcache2,2),
		spu_extract(TEXcache1,3), spu_extract(TEXcache2,3),
		spu_extract(TEXcache1,4), spu_extract(TEXcache2,4),
		spu_extract(TEXcache1,5), spu_extract(TEXcache2,5),
		spu_extract(TEXcache1,6), spu_extract(TEXcache2,6),
		spu_extract(TEXcache1,7), spu_extract(TEXcache2,7));
*/

//	queue->block.triangle->triangle.count --;
	real_block_handler(queue);
}

void block_handler(Queue* queue)
{
	Queue* tri = queue->block.triangle;

	unsigned int bx=queue->block.bx, by=queue->block.by;
	u64 scrbuf = screen.address + screen.bytes_per_line*by*32+bx*128;

	screen_block* current_block = &buffer;
	load_screen_block(current_block, scrbuf, screen.bytes_per_line, 32);
	wait_screen_block(current_block);

	queue->block.pixels = (vec_uint4*) ((void*)&current_block->pixels[0]);
	queue->block.tex_temp = (char*) ((void*)&current_block->textemp[0]);
	queue->block.tex_override = -1;
	
	real_block_handler(queue);

	// this shouldn't be necessary... AND... it sometimes causes duff
	// stuff to be blitted to screen, but i haven't sorted out the block
	// cache stuff yet
	flush_screen_block(&buffer);
	wait_screen_block(&buffer);
}

//////////////////////////////////////////////////////////////////////////////

extern int last_triangle;

void finish_triangle_handler(Queue* tri)
{
	if (tri->triangle.count) {
		QUEUE_JOB(tri, finish_triangle_handler);
		READY_JOB(tri);
		return;
	}
}

void triangle_handler(Queue* tri)
{
	int slots = COUNT_ONES(free_job_queues);
	if (slots==0) {
		QUEUE_JOB(tri, triangle_handler);
		READY_JOB(tri);
		return;
	}

	vec_float4 minmax_ = tri->triangle.minmax;
	vec_int4 minmax = spu_convts(tri->triangle.minmax,0);
	vec_int4 minmax_block = spu_rlmaska(minmax,-5);
	vec_int4 minmax_block_mask = minmax & spu_splats(~31);
	vec_float4 minmax_block_topleft = spu_convtf(minmax_block_mask,0);

	int block_left = spu_extract(minmax_block,0);
	int block_top = spu_extract(minmax_block,1);
	int block_right = spu_extract(minmax_block,2);
	int block_bottom = spu_extract(minmax_block,3);

	vec_float4 pixels_wide = spu_splats(spu_extract(minmax_block_topleft,2)-spu_extract(minmax_block_topleft,0));

////////////////////////////////////////////

	int bx = tri->triangle.cur_x, by = tri->triangle.cur_y;
	int left = tri->triangle.left;
	vec_int4 step = spu_splats((int)tri->triangle.step);
	vec_float4 A = tri->triangle.A;

	vec_int4 step_start = spu_splats(block_right - block_left);
	if (left<0) {
		bx = block_left;
		by = block_top;
		step = step_start;
		left = (block_bottom+1-block_top)*(block_right+1-block_left);
		A = spu_madd(spu_splats(spu_extract(minmax_block_topleft,0)),tri->triangle.A_dx,
	            spu_madd(spu_splats(spu_extract(minmax_block_topleft,1)),tri->triangle.A_dy,A));
	}

////////////////////////////////////////////

	vec_float4 A_dx = tri->triangle.A_dx;
	vec_float4 A_dy = tri->triangle.A_dy;
	vec_float4 A_dx4 = spu_mul(muls4,A_dx);
	vec_float4 A_dx32 = spu_mul(muls32,A_dx);
	vec_float4 A_dy32 = spu_nmsub(pixels_wide,A_dx,spu_mul(muls32,A_dy));
	vec_float4 blockA_dy = spu_madd(mulsn28,A_dx,A_dy);

#ifdef TRY_TO_CULL_BLOCKS
	vec_float4 Aa_corners = spu_madd(muls31y,Aa_dy,
				spu_madd(muls31x,tri->triangle.A_dx,Aa));
	vec_float4 Ab_corners = spu_madd(muls31y,Ab_dy,
				spu_madd(muls31x,tri->triangle.A_dx,Ab));
	vec_float4 Ac_corners = spu_madd(muls31y,Ac_dy,
				spu_madd(muls31x,tri->triangle.A_dx,Ac));
#endif

	while (left) {
#ifdef TRY_TO_CULL_BLOCKS
		vec_uint4 uAa = (vec_uint4) tAa_corners;
		vec_uint4 uAb = (vec_uint4) tAb_corners;
		vec_uint4 uAc = (vec_uint4) tAc_corners;

		vec_uint4 allNeg = spu_or(spu_or(uAa,uAb),uAc);
		vec_uint4 pixel = spu_rlmaska(allNeg,-31);

		vec_uint4 bail = spu_orx(pixel);
		if (spu_extract(bail,0)) {
#endif
			int free_queue = FIRST_JOB(free_job_queues);
			if (free_queue<0) {
				tri->triangle.cur_x = bx;
				tri->triangle.cur_y = by;
				tri->triangle.step = spu_extract(step,0);
				tri->triangle.left = left;
				tri->triangle.A = A;
				QUEUE_JOB(tri, triangle_handler);
				READY_JOB(tri);
				return;
			}
			unsigned int free_queue_mask = 1<<free_queue;
			Queue* block = &job_queue[free_queue];

			block->block.bx = bx;
			block->block.by = by;
			block->block.triangle = tri;
			block->block.A=A;
			block->block.A_dx=A_dx;
			block->block.A_dy=blockA_dy;
			block->next = -1;
			ENQUEUE_JOB(block, tri->triangle.functions->init);
			READY_JOB(block);
			tri->triangle.count++;
#ifdef TRY_TO_CULL_BLOCKS
		}
#endif
		vec_uint4 step_eq0 = spu_cmpeq(step,spu_splats(0));
		vec_float4 A_d32 = spu_sel(A_dx32,A_dy32,step_eq0);
		A += A_d32;
#ifdef TRY_TO_CULL_BLOCKS
		Aa_corners += Aa_d32;
		Ab_corners += Ab_d32;
		Ac_corners += Ac_d32;
#endif
		bx = if_then_else(spu_extract(step_eq0,0), block_left, bx+1);
		by = if_then_else(spu_extract(step_eq0,0), by+1, by);
		vec_int4 t_step = spu_add(step, spu_splats(-1));
		step = spu_sel(t_step, step_start, step_eq0);
		left--;
	}

	QUEUE_JOB(tri, finish_triangle_handler);
	READY_JOB(tri);
	last_triangle = if_then_else(cmp_eq(last_triangle,tri->id), -1, last_triangle);
}


#endif // __IGNORE_ALL_JUNK
