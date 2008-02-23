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

#include <spu_mfcio.h>
#include "fifo.h"
#include "struct.h"
#include "queue.h"

#define NUMBER_TEX_MAPS 16
#define NUMBER_TEX_PIXELS (32*32)

typedef struct {
	u32 textureBuffer[NUMBER_TEX_PIXELS] __attribute__((aligned(128)));
} TextureBlock;

TextureBlock textureCache[NUMBER_TEX_MAPS] __attribute__((aligned(128)));

unsigned int freeTextureMaps = 0;
unsigned int lastLoadedTextureMap = 0;
vec_ushort8 TEXblitting1,TEXblitting2;

static const vec_uchar16 dup_check0 = {SEL_FF SEL_A0 SEL_A0 SEL_A0};
static const vec_uchar16 dup_check1 = {SEL_FF SEL_FF SEL_A1 SEL_A1};
static const vec_uchar16 dup_check2 = {SEL_FF SEL_FF SEL_FF SEL_A2};

static const vec_uchar16 splats[] = {
	/* 0000 */ {0,1, 2,3,4,5, 6,7,8,9, 10,11,12,13, 14,15},
	/* 0001 */ {30,31,0,1, 2,3,4,5, 6,7,8,9, 10,11,12,13},
	/* 0010 */ {26,27,0,1, 2,3,4,5, 6,7,8,9, 10,11,12,13},
	/* 0011 */ {26,27,30,31, 0,1, 2,3,4,5, 6,7,8,9, 10,11},
	/* 0100 */ {22,23,0,1, 2,3,4,5, 6,7,8,9, 10,11,12,13},
	/* 0101 */ {22,23,30,31, 0,1, 2,3,4,5, 6,7,8,9, 10,11},
	/* 0110 */ {22,23,26,27, 0,1, 2,3,4,5, 6,7,8,9, 10,11},
	/* 0111 */ {22,23,26,27,30,31, 0,1, 2,3,4,5, 6,7,8,9},

	/* 1000 */ {18,19, 0,1, 2,3,4,5, 6,7,8,9, 10,11,12,13},
	/* 1001 */ {18,19, 30,31,0,1, 2,3,4,5, 6,7,8,9, 10,11},
	/* 1010 */ {18,19, 26,27,0,1, 2,3,4,5, 6,7,8,9, 10,11},
	/* 1011 */ {18,19, 26,27,30,31, 0,1, 2,3,4,5, 6,7,8,9},
	/* 1100 */ {18,19, 22,23,0,1, 2,3,4,5, 6,7,8,9, 10,11},
	/* 1101 */ {18,19, 22,23,30,31, 0,1, 2,3,4,5, 6,7,8,9},
	/* 1110 */ {18,19, 22,23,26,27, 0,1, 2,3,4,5, 6,7,8,9},
	/* 1111 */ {18,19, 22,23,26,27,30,31, 0,1, 2,3,4,5, 6,7},
};
	
void* finishTextureLoad(void* self, Block* block, int tag);

void* loadMissingTextures(void* self, Block* block, int tag,
			vec_float4 A, vec_uint4 left, vec_uint4* ptr, vec_uint4 tex_keep,
			vec_uint4 block_id, vec_uint4 cache_not_found, vec_uint4 pixel)
{
	block->A = A;
	block->left = spu_extract(left,0);
	block->pixels = ptr;

	unsigned int l = block->left;
//	vec_uint4 z = spu_splats((unsigned int)63);
//	while (l--)
//		*ptr++ = z;
	
//////////////////////////////////////////////////////////////////////

        vec_ushort8 needs = spu_splats((unsigned short)-1); 

/*
	printf("tex failed, tex_keep=%08x left=%d\n", spu_extract(tex_keep,0), l);

	printf("req: %04x%c %04x%c %04x%c %04x%c\n",
			spu_extract(block_id,0), spu_extract(cache_not_found,0)?'*':' ',
			spu_extract(block_id,1), spu_extract(cache_not_found,1)?'*':' ',
			spu_extract(block_id,2), spu_extract(cache_not_found,2)?'*':' ',
			spu_extract(block_id,3), spu_extract(cache_not_found,3)?'*':' ');
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
			vec_uchar16 shuf_cmp_0 = spu_splats((unsigned short)0x203);
			vec_ushort8 copy_cmp_0 = spu_shuffle(block_id,block_id,shuf_cmp_0);

			vec_uchar16 shuf_cmp_1 = spu_splats((unsigned short)0x607);
			vec_ushort8 copy_cmp_1 = spu_shuffle(block_id,block_id,shuf_cmp_1);

			vec_uchar16 shuf_cmp_2 = spu_splats((unsigned short)0xa0b);
			vec_ushort8 copy_cmp_2 = spu_shuffle(block_id,block_id,shuf_cmp_2);
		
			vec_uchar16 shuf_cmp_3 = spu_splats((unsigned short)0xe0f);
			vec_ushort8 copy_cmp_3 = spu_shuffle(block_id,block_id,shuf_cmp_3);
		
			// pixel is mask of 1's where we want to draw
			//
			// cache_not_found mask of 0's where we have valid data
			// tex_mask is mask of 1's where we need to draw
			pixel = spu_andc(pixel, cache_not_found);
			
			// now try to work out what textures were missing
			// copy_cmp_[0123] hold required texture ids splatted across all 8 halfwords
		
			vec_uint4 check_dups0 = spu_cmpeq(block_id,
						spu_shuffle(block_id,block_id,dup_check0));
			vec_uint4 check_dups1 = spu_cmpeq(block_id,
						spu_shuffle(block_id,block_id,dup_check1));
			vec_uint4 check_dups2 = spu_cmpeq(block_id,
						spu_shuffle(block_id,block_id,dup_check2));
			vec_uint4 is_duplicate = spu_or(spu_or(check_dups0,check_dups1), check_dups2);

			vec_uint4 gotneeds_0 = spu_gather(spu_cmpeq(needs,copy_cmp_0));
			vec_uint4 gotneeds_1 = spu_gather(spu_cmpeq(needs,copy_cmp_1));
			vec_uint4 gotneeds_2 = spu_gather(spu_cmpeq(needs,copy_cmp_2));
			vec_uint4 gotneeds_3 = spu_gather(spu_cmpeq(needs,copy_cmp_3));

			vec_uint4 gotneeds_all = {spu_extract(gotneeds_0,0),spu_extract(gotneeds_1,0),
				  		spu_extract(gotneeds_2,0), spu_extract(gotneeds_3,0)};
			vec_uint4 gotneeds_mask = spu_andc(spu_cmpeq(gotneeds_all,0),is_duplicate);
			
			vec_uint4 want_textures = spu_and(cache_not_found,gotneeds_mask);
			
			vec_uint4 want_gather = spu_gather(want_textures);
			needs = spu_shuffle(needs,(vec_ushort8)block_id,
							splats[spu_extract(want_gather,0)]);

//////////////////////////////////////////////////////////////////////

	vec_ushort8 need_mask=spu_cmpeq(needs,spu_splats((unsigned short)-1));
	vec_uint4 need_bits = spu_gather(need_mask);

	vec_uint4 need_any = spu_cmpeq(need_bits,spu_splats((unsigned int)0xff));

	vec_ushort8 tex_id_base = spu_splats((unsigned short)block->triangle->tex_id_base);
	vec_ushort8 needs_sub = spu_sub(needs,tex_id_base);

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
			int nextBit1 = first_bit(freeTextureMaps);
			int nextBit2 = first_bit(freeTextureMaps & ((1<<lastLoadedTextureMap)-1) );
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
			unsigned long long ea = block->triangle->texture_base + (desired<<(5+5+2));
			unsigned long len = 32*32*4;
			
			unsigned long eah = 0; // TODO: fix this
			unsigned long eal = ea & ~127;
			u32* texture = &textureCache[nextIndex].textureBuffer[0];

//			printf("desired %d->%d, reading to %x from %x:%08x len %d tag %d\n",
//				desired, nextIndex, texture, eah, eal, len, tag);

			if (mfc_stat_cmd_queue() == 0) {
				printf("DMA queue full; bailing...\n");
				break;
			}

			spu_mfcdma64(texture, eah, eal, len, tag, MFC_GET_CMD);

			freeTextureMaps &= ~nextMask;
			lastLoadedTextureMap = nextIndex;
			texturesMask |= nextMask;
		}

//		dma_wait_job_queues |= 1 << queue->id;
		block->TEXmerge1 = TEXmerge1;
		block->TEXmerge2 = TEXmerge2;
		block->texturesMask = texturesMask;
//		QUEUE_JOB(queue, finish_texmap_blit_handler);
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
				
//////////////////////////////////////////////////////////////////////

	block->tex_continue = self;
	return &finishTextureLoad;
}

extern void* textureMapFill(void* self, Block* block, int tag);

void* finishTextureLoad(void* self, Block* block, int tag)
{
	TEXcache1 &= block->TEXmerge1;
	TEXcache2 &= block->TEXmerge2;
	freeTextureMaps |= block->texturesMask;

/*
	printf(" load = %04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x\n",
		spu_extract(TEXcache1,0), spu_extract(TEXcache2,0),
		spu_extract(TEXcache1,1), spu_extract(TEXcache2,1),
		spu_extract(TEXcache1,2), spu_extract(TEXcache2,2),
		spu_extract(TEXcache1,3), spu_extract(TEXcache2,3),
		spu_extract(TEXcache1,4), spu_extract(TEXcache2,4),
		spu_extract(TEXcache1,5), spu_extract(TEXcache2,5),
		spu_extract(TEXcache1,6), spu_extract(TEXcache2,6),
		spu_extract(TEXcache1,7), spu_extract(TEXcache2,7));
*/

	return block->tex_continue;
//	return &textureMapFill;

//	block->triangle->count--;
//	return 0;
}

static inline void updateTextureCache(int index,int value) {
	if (index&1)
		TEXcache1 = spu_insert(value, TEXcache1, index>>1);
	else
		TEXcache2 = spu_insert(value, TEXcache2, index>>1);
}

void init_texture_cache()
{
	u32* texture = &textureCache[0].textureBuffer[0];

	int i;
	for (i=0; i<NUMBER_TEX_MAPS*32*32; i++) {
		texture[i] = (i<<4) | (i<<9) | (i<<19);
	}


	freeTextureMaps = (1<<NUMBER_TEX_MAPS)-1;
	TEXcache1 = TEXcache2 = spu_splats((unsigned short)-1);
	TEXblitting1 = TEXblitting2 = spu_splats((unsigned short)-1);

	TEXcache1 = spu_insert(20, TEXcache1, 0);	// 0
	TEXcache1 = spu_insert(21, TEXcache1, 1);	// 2
	TEXcache1 = spu_insert(22, TEXcache1, 2);
	TEXcache1 = spu_insert(23, TEXcache1, 3);
	TEXcache1 = spu_insert(36, TEXcache1, 4);
	TEXcache1 = spu_insert(37, TEXcache1, 5);
	TEXcache1 = spu_insert(38, TEXcache1, 6);
	TEXcache1 = spu_insert(39, TEXcache1, 7);	// 14

	TEXcache2 = spu_insert(40, TEXcache2, 0);	// 1
	TEXcache2 = spu_insert(41, TEXcache2, 1);	// 3
	TEXcache2 = spu_insert(42, TEXcache2, 2);	// 5
	TEXcache2 = spu_insert(43, TEXcache2, 3);
	TEXcache2 = spu_insert(48, TEXcache2, 4);
	TEXcache2 = spu_insert(49, TEXcache2, 5);
	TEXcache2 = spu_insert(50, TEXcache2, 6);
	TEXcache2 = spu_insert(51, TEXcache2, 7);	// 15
}

