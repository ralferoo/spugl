/****************************************************************************
 *
 * SPU triangle span function for interpolated colours
 *
 * (c) 2007 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

#include <spu_mfcio.h>
#include <spu_intrinsics.h>
#include "struct.h"

u32 textureTemp[32] __attribute__((aligned(128)));

// This is starting to look very much like some kind of pixel shader fragment
// that could be generated by something else.
//
// Surely that's not what i was planning all along... ;)
//
// For maximum unrolling, we'd want to process 8 vectors of 4 pixels at a time
// making a block of 32, which is conveniently the same size as I had planned
// for the texture blocks too...
//
// We probably need a different shader for alpha blocks as we'd also want
// to handle z-buffering differently (when I get round to that)

// don't really want to do textures like this...
void triangleSpan
(u32* screenBuffer, int start, int length, vertex_state* a, vertex_state* b, vertex_state* c, float Sa, float Sb, float Sc, float Da, float Db, float Dc)
{
	screenBuffer += start;	// skip pixels on left

//	int tex_blocks_wide = (texture_width-1+31)>>5;

	do {
		// can do these by anding off the sign bit
		float Aa = fabs(Sa);
		float Ab = fabs(Sb);
		float Ac = fabs(Sc);
	
		// with these running in parallel (although if unrolled to
		// 8 x vector[4] we will have 0..7*D precalculated and this
		// doesn't actually change per line)
		Sa += Da;
		Sb += Db;
		Sc += Dc;

		float w = a->coords.w*Aa + b->coords.w*Ab + c->coords.w*Ac;
		float z = 1.0/w; // don't forget newton-rhapson here

		float Ba = Aa*z;
		float Bb = Ab*z;
		float Bc = Ac*z;

/*
		if (texture_stride) {
			float ru = (a->r * Ba + b->r * Bb + c->r * Bc);
			float rv = (a->g * Ba + b->g * Bb + c->g * Bc);

			int u,v;

			u32 result = 0;
#ifdef RUBBISH
			vec_float4 tru = spu_splats(ru);
			vec_uint4 tu = si_cflts((qword)ru, 13);
			u = spu_extract(tu, 0)>>(32-7);

			vec_float4 trv = spu_splats(rv);
			vec_uint4 tv = si_cfltu((qword)rv, 32);
			v = spu_extract(tv, 0)>>(32-7);

			printf("%f %f -> %f %08lx %d\n", ru, rv, 
				spu_extract(tru, 0),
				spu_extract(tu, 0), u);

#else
			u = (int)(texture_width*ru);
			v = (int)(texture_height*rv);

			if (u<0) u=0;
			if (u>texture_width-1) u=texture_width-1;

			if (v<0) v=0;
			if (v>texture_height-1) v=texture_height-1;

			int block = (v>>5)*tex_blocks_wide + (u>>5);

			u &= 31;
			v &= 31;

			result = (block<<16) | (u<<8) | v;

		printf ("%1.3f,%1.3f -> %4x, %x,%x -> %x\n", ru, rv, block, u, v, result);
#endif
/*
			unsigned long long texAddress = texture + texture_stride*v + (u<<2);
			unsigned long ta = texAddress&~127;

//			printf("Ba %f Bb %f Bc %f ar %f br %f cr %f\n ag %f bg %f cg %f\n",
//				Ba, Bb, Bc, a->r, b->r, c->r, a->g, b->g, c->g);

//			printf("tex=(%1.3f,%1.3f) (%d,%d) stride=%d size=(%d,%d) doing short blit from address %llx to %lx\n", ru, rv, u, v, texture_stride, texture_width, texture_height, texAddress, &textureTemp);
			int tag_id = 0;
			//mfc_get(&textureTemp, ta, 128, tag_id,0,0);
			mfc_get(&textureTemp[u&31], texAddress, 4, tag_id,0,0);
			mfc_write_tag_mask(1<<tag_id);
			int mask = mfc_read_tag_status_any();
//			printf("finished blit...\n");
			result = textureTemp[u&31];
			
			*screenBuffer++ = result;
		} else {
*/
			// cfltu and scale should nicely handle the clamping
			int Cr = (int)(255*(a->colour.x * Ba + b->colour.x * Bb + c->colour.x * Bc));
			int Cg = (int)(255*(a->colour.y * Ba + b->colour.y * Bb + c->colour.y * Bc));
			int Cb = (int)(255*(a->colour.z * Ba + b->colour.z * Bb + c->colour.z * Bc));

//	printf("rgb = %d %d %d\n", Cr, Cg, Cb);

			if (Cr<0) Cr=0; if (Cr>255) Cr=255;
			if (Cg<0) Cg=0; if (Cg>255) Cg=255;
			if (Cb<0) Cb=0; if (Cb>255) Cb=255;

			*screenBuffer++ = 0xff000000 | (Cr<<16) | (Cg<<8) | Cb;
//		}
	} while (length--); // length=0 means 1px
}


