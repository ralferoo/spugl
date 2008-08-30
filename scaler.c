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

// shrinks a 32x32 pixel source block to 16x16 block, written in the topleft
// of a 32x32 pixel block. this is intended to be called 4 times to generate
// a mipmap tile, but if data is missing for the right or bottom, then we
// probably only need to call it once.
void shrinkTexture(void* in, void* out) {
        vec_uint4 src = spu_splats((unsigned int)in);
        vec_uint4 dst = spu_splats((unsigned int)out);

	vec_uint4 src_dx = spu_splats(32);
	vec_uint4 src_dy = spu_splats(32+32*4);

	vec_uint4 dst_dx = spu_splats(16);
	vec_uint4 dst_dy = spu_splats(16+16*4);

	vec_uint4 mask_rb = spu_splats(0x00ff00ff);
	vec_uint4 mask_ag = spu_splats(0x3fc03fc0);

	vec_uint4 left = spu_splats(4*16);

	// a01: A1R1G1B1 A2R2G2B2 A3R3G3B3 A4R4G4B4
	// b01: A5R5G5B5 A6R6G6B6 A7R7G7B7 A8R8G8B8

	static const vec_uchar16 s_left = {
		0, 4, 16, 20,		// A
		1, 5, 17, 21,		// R
		2, 6, 18, 22,		// G
		3, 7, 19, 23};		// B

	static const vec_uchar16 s_right = {
		 8, 12, 24, 28,		// A
		 9, 13, 25, 29,		// R
		10, 14, 26, 30,		// G
		11, 15, 27, 31};	// B

	// sumbA -> A1A2A5A6 R1R2R5R6 G1G2G5G6 B1B2B5B6	
	// sumbB -> A3A4A7A8 R3R4R7R8 G3G4G7G8 B3B4B7B8

	// sumb -> x A3 x A1 x R3 x R1 x G3 x G1 x B3 x B1

	// -> x Al x Rl x Gl x Bl, x Ar x Rr x Gr x Br

	static const vec_uchar16 merge = {
		 1,  5,  9, 13,
		 3,  7, 11, 15,
		17, 21, 25, 29,
		19, 23, 27, 31};

	do {
		vec_uint4* sptr = (vec_uint4*) spu_extract(src, 0);
		vec_uint4* dptr = (vec_uint4*) spu_extract(dst, 0);

		vec_uint4 pixa_01 = sptr[0];
		vec_uint4 pixa_23 = sptr[1];
		vec_uint4 pixb_01 = sptr[0]; //32*4/sizeof(vec_uint4)];
		vec_uint4 pixb_23 = sptr[1]; //32*4/sizeof(vec_uint4)+1];

		// ARGB ARGB ARGB ARGB

		vec_ushort8 avg01=spu_rlmask(spu_sumb(
				(vec_uchar16) spu_shuffle(pixa_01, pixb_01, s_right),
				(vec_uchar16) spu_shuffle(pixa_01, pixb_01, s_left)), -2);

		vec_ushort8 avg23=spu_rlmask(spu_sumb(
				(vec_uchar16) spu_shuffle(pixa_23, pixb_23, s_right),
				(vec_uchar16) spu_shuffle(pixa_23, pixb_23, s_left)), -2);

		vec_uint4 avg = (vec_uint4) spu_shuffle(avg01, avg23, merge);

		*dptr = avg;

		vec_uint4 which = spu_and(left,spu_splats((unsigned int)3));
		vec_uint4 sel = spu_cmpeq(which,1);
		left -= spu_splats(1);
		dst += spu_sel(dst_dx,dst_dy,sel);
		src += spu_sel(src_dx,src_dy,sel);
	} while (spu_extract(left,0)>0);
}

