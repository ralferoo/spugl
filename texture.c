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

u32 textureCache[NUMBER_TEX_MAPS*32*32] __attribute__((aligned(128)));

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
	
void* loadMissingTextures(void* self, Block* block, int tag,
			vec_float4 A, vec_uint4 left, vec_uint4* ptr, vec_uint4 tex_keep,
			vec_uint4 block_id, vec_uint4 cache_not_found, vec_uint4 pixel)
{
	block->A = A;
	block->left = spu_extract(left,0);
	block->pixels = ptr;

	unsigned int l = block->left;
	printf("tex failed, tex_keep=%08x left=%d\n",
			spu_extract(tex_keep,0), l);
	vec_uint4 z = spu_splats((unsigned int)63);
	while (l--)
		*ptr++ = z;
				
	block->triangle->count--;
	return 0;
}
/*		
			// pixel is mask of 1's where we want to draw
			// cache_not_found mask of 0's where we have valid data
			// tex_mask is mask of 1's where we need to draw
//			pixel = spu_andc(pixel, cache_not_found);
//			tex_mask = spu_andc(tex_mask, pixel);		// remove pixels we can draw
			
			// now try to work out what textures were missing
			// copy_cmp_[0123] hold required texture ids splatted across all 8 halfwords
		
			vec_uint4 check_dups0 = spu_cmpeq(block_id,
						spu_shuffle(block_id,block_id,dup_check0));
			vec_uint4 check_dups1 = spu_cmpeq(block_id,
						spu_shuffle(block_id,block_id,dup_check1));
			vec_uint4 check_dups2 = spu_cmpeq(block_id,
						spu_shuffle(block_id,block_id,dup_check2));
			vec_uint4 is_duplicate = spu_or(spu_or(check_dups0,check_dups1), check_dups2);

			vec_uint4 gotneeds_0 = spu_gather(spu_cmpeq(need1,copy_cmp_0));
			vec_uint4 gotneeds_1 = spu_gather(spu_cmpeq(need1,copy_cmp_1));
			vec_uint4 gotneeds_2 = spu_gather(spu_cmpeq(need1,copy_cmp_2));
			vec_uint4 gotneeds_3 = spu_gather(spu_cmpeq(need1,copy_cmp_3));

			vec_uint4 gotneeds_all = {spu_extract(gotneeds_0,0),spu_extract(gotneeds_1,0),
				  		spu_extract(gotneeds_2,0), spu_extract(gotneeds_3,0)};
			vec_uint4 gotneeds_mask = spu_andc(spu_cmpeq(gotneeds_all,0),is_duplicate);
			
			vec_uint4 want_textures = spu_and(cache_not_found,gotneeds_mask);
			
			vec_uint4 want_gather = spu_gather(want_textures);
			need1 = spu_shuffle(need1,(vec_ushort8)block_id,
							splats[spu_extract(want_gather,0)]);
*/

unsigned int freeTextureMaps = 0;
unsigned int lastLoadedTextureMap = 0;
vec_ushort8 TEXblitting1,TEXblitting2;

static inline void updateTextureCache(int index,int value) {
	if (index&1)
		TEXcache1 = spu_insert(value, TEXcache1, index>>1);
	else
		TEXcache2 = spu_insert(value, TEXcache2, index>>1);
}

void init_texture_cache()
{
	u32* texture = &textureCache[0];

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

