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

#include <stdio.h>
#include <spu_mfcio.h>
#include "fifo.h"
#include "struct.h"
#include "queue.h"

#define NUMBER_TEX_MAPS 16

// the following is 33*32 (normal) 33*4(x=32) 7*4(pad)
#define TEX_MAP_PIXELS (33*32+4*40)

TextureDefinition textureDefinition[NUMBER_OF_TEXTURE_DEFINITIONS];
extern SPU_CONTROL control;

typedef struct {
	u32 textureBuffer[TEX_MAP_PIXELS] __attribute__((aligned(128)));
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

void* finishTextureLoad(void* self, Block* block, ActiveBlock* active, int tag);

void* loadMissingTextures(void* self, Block* block, ActiveBlock* active, int tag,
			vec_float4 A, vec_uint4 left, vec_uint4* ptr, vec_uint4 tex_keep,
			vec_int4 mipmap_vec, vec_uint4 block_id, vec_uint4 s, vec_uint4 t,
			vec_uint4 cache_not_found, vec_uint4 pixel)
{
	block->A = A;
	block->left = spu_extract(left,0);
	block->pixels = ptr;

	unsigned int l = block->left;
	
//////////////////////////////////////////////////////////////////////

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

	vec_uint4 want_textures = cache_not_found;
	vec_uint4 want_gather = spu_gather(want_textures);

////////////////////////////////////////////////////////////////////

	TextureDefinition* textureDefinition = block->triangle->texture;

/*
	printf("cache miss for block %x %x %x %x\n", 
		spu_extract(block_id,0),
		spu_extract(block_id,1),
		spu_extract(block_id,2),
		spu_extract(block_id,3));
*/

	vec_ushort8 TEXmerge1 = spu_splats((unsigned short)-1);
	vec_ushort8 TEXmerge2 = spu_splats((unsigned short)-1);
	int texturesMask = 0;

	int m,i;
	int n = spu_extract(want_gather,0);
	for (i=0,m=0x8; m && freeTextureMaps; m>>=1,i++) {
		if (n&m) {			// as soon as we find a 1 bit we are done
			unsigned int want = spu_extract(block_id,i);
			unsigned int mipmap = spu_extract(mipmap_vec,i);
			int nextBit1 = first_bit(freeTextureMaps);
			int nextBit2 = first_bit(freeTextureMaps & ((1<<lastLoadedTextureMap)-1) );
			int nextIndex = nextBit2<0 ? nextBit1 : nextBit2;
			int nextMask = 1<<nextIndex;

			if (! (freeTextureMaps&nextMask) ) {
//				printf("ERROR: freeTextureMaps=%lx, next=%d,%d->%d, mask=%lx\n",
//					freeTextureMaps, nextBit1, nextBit2, nextIndex, nextMask);
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

			if (nextIndex&1) {
				TEXcache2 = spu_insert(-1,   TEXcache2, nextIndex>>1);
				TEXmerge2 = spu_insert(want, TEXmerge2, nextIndex>>1);
				TEXblitting2 = spu_insert(want, TEXblitting2, nextIndex>>1);
			} else {
				TEXcache1 = spu_insert(-1,   TEXcache1, nextIndex>>1);
				TEXmerge1 = spu_insert(want, TEXmerge1, nextIndex>>1);
				TEXblitting1 = spu_insert(want, TEXblitting1, nextIndex>>1);
			}

			unsigned int s_blk = spu_extract(s, i);
			unsigned int t_blk = spu_extract(t, i);
			unsigned int t_next = (t_blk+1)&7;		// TODO: blksize ref
			unsigned int s_next = (s_blk+1)&7;		// TODO: blksize ref

			unsigned short t_mult = textureDefinition->tex_t_blk_mult[mipmap];

			unsigned int s_ofs = s_blk*32*32*4;
			unsigned int t_ofs = t_blk*t_mult;

			unsigned int sn_ofs = s_next*32*32*4;
			unsigned int tn_ofs = t_next*t_mult;

			unsigned long long ea_ = textureDefinition->tex_pixel_base[mipmap];
			unsigned long eah = ea_ >> 32;
			unsigned long ea  = ea_ & 0xffffffff;
			unsigned long eal = ea + s_ofs + t_ofs;
			unsigned long eal_next = ea + s_ofs + tn_ofs;
			unsigned long eabl = ea + sn_ofs + t_ofs;
			unsigned long eabl_next = ea + sn_ofs + tn_ofs;

			u32* texture = &textureCache[nextIndex].textureBuffer[0];

			if (mfc_stat_cmd_queue() == 0) {
//				printf("DMA queue full; bailing...\n");
				break;
			}

			static vec_uint4 load_dma_list[NUMBER_TEX_MAPS][35];

			vec_uint4* dma_list = &load_dma_list[nextIndex][0];
			vec_uint4* list_ptr = dma_list;

			vec_uint4 block0 = { 32*32*4, eal, 32*4, eabl };
			*list_ptr++ = block0;

			vec_uint4 block = { 16, eal_next, 16, eal_next+32*4 };
			vec_uint4 step = { 0, 32*4*2, 0, 32*4*2 };
	
			int qq;
			for (qq=0; qq<16; qq++) {
				*list_ptr++ = block;
				block += step;
			}

			vec_uint4 block_l = { 16, eabl_next, 0, 0 };
			*list_ptr++ = block_l;

			unsigned int list_len = (((void*)list_ptr)-((void*)dma_list))-8;

//			printf("starting DMA... list len %d, list at %x\n", list_len, dma_list);
			spu_mfcdma64(texture, eah, dma_list, list_len, tag, MFC_GETL_CMD);

			active->temp = (unsigned int) texture;

			freeTextureMaps &= ~nextMask;
			lastLoadedTextureMap = nextIndex;
			texturesMask |= nextMask;

			control.cache_miss_count++;
		}
	}

	active->TEXmerge1 = TEXmerge1;
	active->TEXmerge2 = TEXmerge2;
	active->texturesMask = texturesMask;
				
//////////////////////////////////////////////////////////////////////

	active->tex_continue = self;
	return &finishTextureLoad;
}

void shrinkTexture(void* in, void* out);

void* finishTextureLoad(void* self, Block* block, ActiveBlock* active, int tag)
{
	TEXcache1 &= active->TEXmerge1;
	TEXcache2 &= active->TEXmerge2;
	freeTextureMaps |= active->texturesMask;

//	shrinkTexture((void*)active->temp, (void*)active->temp);
	
	// loaded some texture maps, chain on to original request
	return active->tex_continue(active->tex_continue, block, active, tag);
}

void init_texture_cache()
{
	freeTextureMaps = (1<<NUMBER_TEX_MAPS)-1;
	TEXcache1 = TEXcache2 = spu_splats((unsigned short)-1);
	TEXblitting1 = TEXblitting2 = spu_splats((unsigned short)-1);

	int i;
	for (i=0; i<NUMBER_OF_TEXTURE_DEFINITIONS; i++)
		textureDefinition[i].users = 0;
}

