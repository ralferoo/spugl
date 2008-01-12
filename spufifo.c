/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

#include <spu_mfcio.h>
#include "3d.h"

SPU_CONTROL control __CACHE_ALIGNED__;

int init_fifo(int fifo_size) {
	if (control.fifo_start)
		free(control.fifo_start);

	control.fifo_start = (u32) (fifo_size ? malloc(fifo_size) : 0UL);
	if (fifo_size>0 && control.fifo_start == 0UL)
		return 1;

//	printf("Allocated FIFO of %d bytes at %lx ctrl %lx\n",
//		fifo_size, control.fifo_start, &control);

	control.fifo_size = fifo_size;
	control.fifo_written = control.fifo_start;
	control.fifo_read = control.fifo_start;

	control.last_count = 0;
	control.idle_count = 0;

	return 0;
}

/* Process commands on the FIFO */
u32* process_fifo(u32* from, u32* to) {
	while (from != to) {
		u32* addr = from;
		u32 command = *from++;
		u32 eah, eal;
		switch (command) {
			case SPU_COMMAND_NOP:
				printf("%06lx: NOP\n", addr);
				break;
			case SPU_COMMAND_JMP:
				from = (u32*) *from++;
				printf("%06lx: JMP %06lx\n", addr, from);
				break;
			case SPU_COMMAND_DEL_CHILD:
				eah = *from++;
				eal = *from++;
				printf("%06lx: DEL CHILD %lx%08lx\n",
					addr, eah, eal);
				break;
			case SPU_COMMAND_ADD_CHILD:
				eah = *from++;
				eal = *from++;
				printf("%06lx: ADD CHILD %lx%08lx\n",
					addr, eah, eal);
				break;
			default:
				printf("%06lx: command %lx\n", addr, command);
		}
	}
	return from;
}

/* I'm deliberately going to ignore the arguments passed in as early versions
 * of libspe2 have the upper and lower 32 bits swapped.
 */
int main(unsigned long long spe_id, unsigned long long program_data_ea, unsigned long long env) {
//	printf("spumain running on spe %lx\n", spe_id);

	if (init_fifo(0)) {
		printf("couldn't allocate FIFO buffer...\n");
		exit(1);
	}

	int running = 1;
	while (running) {
		while (spu_stat_in_mbox() == 0) {
			control.idle_count ++;
			// check to see if there's any data waiting on FIFO
			u32 read = control.fifo_read;
			u32 written = control.fifo_written;
			if ((read-written)&0x3ffff) {
				u32* to = (u32*) written;
				u32* from = (u32*) read;
				u32* end = process_fifo(from, to);
//				printf("Processed FIFO from %lx to %lx "
//					"(ends %lx)\n", from, end, to);
				control.fifo_read = (u32) end;
			}
		}

		unsigned long msg = spu_read_in_mbox();
		printf("received message %ld\n", msg);
		switch (msg) {
			case SPU_MBOX_3D_TERMINATE:
				running = 0;
				continue;
			case SPU_MBOX_3D_FLUSH:
				spu_write_out_mbox(0);
				break; 
			case SPU_MBOX_3D_INITIALISE_MASTER:
				if (init_fifo(65536)) {
					printf("couldn't allocate FIFO\n");
					spu_write_out_mbox(0);
					running = 0;
				} else {
					spu_write_out_mbox((u32)&control);
				}
				break;
			case SPU_MBOX_3D_INITIALISE_NORMAL:
				if (init_fifo(1024)) {
					printf("couldn't allocate FIFO\n");
					spu_write_out_mbox(0);
					running = 0;
				} else {
					spu_write_out_mbox((u32)&control);
				}
				break;
		}
	}

	printf("spumain exiting\n");
}




























#ifdef __ignore_junk
typedef unsigned int u32;

#define BYTE_ALIGNMENT 127
#define PIXEL_ALIGNMENT (BYTE_ALIGNMENT>>2)

// the SPUs must be DMA bounded, as even the decision whether to blend
// with preblend or not takes longer than to do the multiply on every
// pixel

typedef void BLEND_FUNCTION(u32* screenBuffer, u32* source, int image_width, int step, int opacity);

extern BLEND_FUNCTION blendLine;
extern BLEND_FUNCTION blendLineWithPreBlend;

typedef void TRIANGLE_SPAN_FUNCTION
(u32* screenBuffer, int start, int length, VERTEX* a, VERTEX* b, VERTEX* c, float Sa, float Sb, float Sc, float Da, float Db, float Dc, unsigned long long texture, unsigned int texture_stride, unsigned int texture_width, unsigned int texture_height)
;

extern TRIANGLE_SPAN_FUNCTION triangleSpan;

//extern void blendLine(u32* screenBuffer, u32* source, int image_width, int step, int opacity);
//extern void blendLineWithPreBlend(u32* screenBuffer, u32* source, int image_width, int step, int opacity);

extern void blitSolidLine(u32* screenBuffer, u32* source, int image_width, int step);
extern void doTest(void);

void doTriangle(SPU_TRIANGLE_COLOUR* cmd, u32 blitmask) {
	unsigned long long frame = (unsigned long long) (cmd->header.framebuffer);
	int fbWidth = cmd->header.fb_width;
	int fbClipWidth = cmd->header.fb_clip_width;
	int fbHeight = cmd->header.fb_height;
	TRIANGLE_SPAN_FUNCTION* func = triangleSpan;
	VERTEX *a=&cmd->vertex[0];
	VERTEX *b=&cmd->vertex[1];
	VERTEX *c=&cmd->vertex[2];

	unsigned long long tex = cmd->tex;
	unsigned int tex_stride = cmd->tex_stride;
	unsigned int tex_width = cmd->tex_width;
	unsigned int tex_height = cmd->tex_height;

//	printf("tritex %llx %d %d %d\n", tex, tex_stride, tex_width, tex_height);

	float abx = b->x - a->x;
	float aby = b->y - a->y;
	float bcx = c->x - b->x;
	float bcy = c->y - b->y;
	float cax = a->x - c->x;
	float cay = a->y - c->y;

//	printf("triangle (%3.2f,%3.2f),(%3.2f,%3.2f),(%3.2f,%3.2f)\n", 
//		a->x, a->y, b->x, b->y, c->x, c->y);

	if (a->x * bcy + b->x * cay + c->x * aby > 0) {
		// z is negative, so +ve means counter-clockwise and so not vis
		return;
	}

	int qab = aby<0;
	int qbc = bcy<0;
	int qca = cay<0;

	int bc_a_ca = qbc && qca;
	int bc_o_ca = qbc || qca;

	int left;
	VERTEX *temp;
	if (qab && qbc) {
		left = 1;
		temp = c; c = b; b = a; a = temp;
	}
	if (qab && qca) {
		left = 1;
		temp = a; a = b; b = c; c = temp;
	}
	if (qab && !bc_o_ca) {
		left = 0;
		temp = a; a = b; b = c; c = temp;
	}
	if (!(qab || qbc)) {
		left = 0;
	}
	if (!(qab || qca)) {
		left = 0;
		temp = c; c = b; b = a; a = temp;
	}
	if (!qab && bc_a_ca) {
		left = 1;
	}

	if (b->y < a->y || c->y < a->y) {
		printf (":ERROR: a not least\n");
		return;
	}
	if (left && b->y < c->y) {
		printf (":ERROR: left and b<c\n");
		return;
	}
	if (!left && c->y < b->y) {
		printf (":ERROR: right and c<b\n");
		return;
	}
		
	void* lb = malloc( (((fbWidth<<2)+BYTE_ALIGNMENT)&~BYTE_ALIGNMENT) + BYTE_ALIGNMENT);
	int tag_id = 1;
	int mask;

	float y = ((int)a->y) + 0.5;

	float lgrad = (c->x - a->x) / (c->y - a->y);
	float lx = ((int)a->x) + 0.5;

	float rgrad = (b->x - a->x) / (b->y - a->y);
	float rx = ((int)a->x) + 0.5;

	float mid, bottom;
	if (left) {
		mid = c->y; bottom = b->y;
	} else {
		mid = b->y; bottom = c->y;
	}

//	printf ("y=%.2f, lx=%.2f, lgrad=%.2f, rx=%.2f, rgrad=%.2f, mid=%.2f, bottom=%.2f\n",
//		y, lx, lgrad, rx, rgrad, mid, bottom);

	float tab = a->x * b->y - b->x * a->y;
	float tbc = b->x * c->y - c->x * b->y;
	float tca = c->x * a->y - a->x * c->y;

	float tap = a->x * y - a->y * lx;
	float tbp = b->x * y - b->y * lx;
	float tcp = c->x * y - c->y * lx;

	float dap = a->x - a->y * lgrad;
	float dbp = b->x - b->y * lgrad;
	float dcp = c->x - c->y * lgrad;

	float Sa = -tbc-tcp+tbp;
	float Sb = -tca-tap+tcp;
	float Sc = -tab-tbp+tap;

	float dSa = -dcp+dbp;
	float dSb = -dap+dcp;
	float dSc = -dbp+dap;

	float Da = c->y - b->y;
	float Db = a->y - c->y;
	float Dc = b->y - a->y;

	while (y < mid && y < fbHeight) {
		float l = lx>0.5 ? lx : 0.5;
		float r = rx<fbWidth-1 ? rx : fbWidth-0.5;
		if (l <= r) {
	    		int fbLine = (int)y;
			if ( blitmask & (1<<(fbLine&31)) ) {
			unsigned long long screen = (unsigned long) (frame + fbLine*(fbWidth<<2));	// framebuffer position
			mfc_getf(lb, screen, (((fbWidth<<2)+BYTE_ALIGNMENT)&~BYTE_ALIGNMENT), tag_id, 0, 0);
			mfc_write_tag_mask(1<<tag_id);
			mask = mfc_read_tag_status_any();

			int start = (int)l;
			int length = (int)(r-l);
			(*func)(lb, start, length, a, b, c, Sa, Sb, Sc, Da, Db, Dc, tex, tex_stride, tex_width, tex_height);
			mfc_putf(lb, screen, (((fbWidth<<2)+BYTE_ALIGNMENT)&~BYTE_ALIGNMENT), tag_id, 0, 0);
			mfc_write_tag_mask(1<<tag_id);
			mask = mfc_read_tag_status_any();
			}
		}
		y += 1.0;
		lx += lgrad;
		rx += rgrad;
		Sa += dSa;
		Sb += dSb;
		Sc += dSc;
	}

	if (left) {
		lgrad = (b->x - c->x) / (b->y - c->y);
		lx = ((int)c->x) + 0.5;

		tap = a->x * y - a->y * lx;
		tbp = b->x * y - b->y * lx;
		tcp = c->x * y - c->y * lx;

		dap = a->x - a->y * lgrad;
		dbp = b->x - b->y * lgrad;
		dcp = c->x - c->y * lgrad;
	
		Sa = -tbc-tcp+tbp;
		Sb = -tca-tap+tcp;
		Sc = -tab-tbp+tap;

		dSa = -dcp+dbp;
		dSb = -dap+dcp;
		dSc = -dbp+dap;
	} else {
		rgrad = (c->x - b->x) / (c->y - b->y);
		rx = ((int)b->x) + 0.5;
	}
		
//	printf ("cont y=%.2f, lx=%.2f, lgrad=%.2f, rx=%.2f, rgrad=%.2f, mid=%.2f, bottom=%.2f\n",
//		y, lx, lgrad, rx, rgrad, mid, bottom);

	while (y < bottom && y < fbHeight) {
		float l = lx>0.5 ? lx : 0.5;
		float r = rx<fbWidth-1 ? rx : fbWidth-0.5;
		if (l < r) {
	    		int fbLine = (int)y;
			if ( blitmask & (1<<(fbLine&31)) ) {
			unsigned long long screen = (unsigned long) (frame + fbLine*(fbWidth<<2));	// framebuffer position
			mfc_getf(lb, screen, (((fbWidth<<2)+BYTE_ALIGNMENT)&~BYTE_ALIGNMENT), tag_id, 0, 0);
			mfc_write_tag_mask(1<<tag_id);
			mask = mfc_read_tag_status_any();

			int start = (int)l;
			int length = (int)(r-l);
			(*func)(lb, start, length, a, b, c, Sa, Sb, Sc, Da, Db, Dc, tex, tex_stride, tex_width, tex_height);
			mfc_putf(lb, screen, (((fbWidth<<2)+BYTE_ALIGNMENT)&~BYTE_ALIGNMENT), tag_id, 0, 0);
			mfc_write_tag_mask(1<<tag_id);
			mask = mfc_read_tag_status_any();
			}
		}
		y += 1.0;
		lx += lgrad;
		rx += rgrad;
		Sa += dSa;
		Sb += dSb;
		Sc += dSc;
	}

	free(lb);
}

void imageBlit(SPU_DATA_BLIT* cmd, u32 blitmask) {
	//printf("SPU_DATA_BLIT at address %08lx\n", cmd);

	unsigned long long frame = (unsigned long long) (cmd->header.framebuffer);
	int fbWidth = cmd->header.fb_width;
	int fbClipWidth = cmd->header.fb_clip_width;
	int fbHeight = cmd->header.fb_height;
	int x = cmd->x;
	int y = cmd->y;
	int image_width = cmd->image_width;
	int image_height = cmd->image_height;
	int image_bytesPerLine = cmd->image_bytesPerLine;
	int image_longsPerLine = image_bytesPerLine/4;
	int transparency = cmd->transparency;
	int opacity = cmd->opacity;
	unsigned long long pixels = (unsigned long long) (cmd->pixels);

	BLEND_FUNCTION* blender = (opacity==255) ? &blendLine : &blendLineWithPreBlend;

	// vertical clipping

	if (y < 0) {
		// clip image at top
		image_height += y;
		pixels -= y * image_bytesPerLine;
		y = 0;
	}

	if (y + image_height >= fbHeight) {
		// clip image at bottom
		image_height = fbHeight - y;
	}

	// horizontal clipping (a bit harder, as the source address must stay aligned with
	// the cache line boundary for DMA purposes)
	
	int pixelskip = 0;

	if (x < 0) {
		// clip image at left
		image_width += x;
		pixelskip = -x;
		x = 0;
	}

	if (x + image_width >= fbClipWidth) {
		// clip image at right
		image_width = fbClipWidth - x;
	}

	if (image_width <= 0 || image_height <= 0) {
		// completely off screen
		return;
	}

//	printf("blit (%d,%d) @ (%d,%d) step=%d, trans=%d, image=%08llx, fbWidth=%d\n", 
//	 	image_width, image_height, x, y, image_longsPerLine, transparency, pixels, fbWidth);

	unsigned long long draw = frame + ((x&~PIXEL_ALIGNMENT)<<2);
	int numPixels = ((x+image_width-1)|PIXEL_ALIGNMENT)-(x&~PIXEL_ALIGNMENT)+1;

	unsigned long allTags = 0;
	int tag_id = 1;
	int step = x&PIXEL_ALIGNMENT;

	void* sb_[32];
	void* lb_[32];

	u32* sb[32];
	u32* lb[32];
	unsigned long long sc[32];

	int i;
	for (i=0; i<32; i++) {
		sc[i] = 0;
		lb_[i] = malloc( ((cmd->image_bytesPerLine+BYTE_ALIGNMENT)&~BYTE_ALIGNMENT) + BYTE_ALIGNMENT);
		lb[i] = ( (u32*) ((((u32)lb_[i])+BYTE_ALIGNMENT)&~BYTE_ALIGNMENT) );

		sb_[i] = malloc( numPixels*4 + BYTE_ALIGNMENT);
		sb[i] = ( (u32*) ((((u32)sb_[i])+BYTE_ALIGNMENT)&~BYTE_ALIGNMENT) );
	}

	int line;
	int mask;
	for (line=0;line<image_height;line++) {
	    int fbLine = line+y;				// we might just want to do certain lines on certain SPUs here
	    	if ( blitmask & (1<<(fbLine&31)) ) {
		//	tag_id = (tag_id+1)%8;				// use the next tag_id
			tag_id = fbLine & 7;		// stop multiple blits of the same line

			unsigned long long screen = (unsigned long) (draw + fbLine*(fbWidth<<2));	// framebuffer position
			unsigned long long source = (unsigned long) (pixels + line*image_bytesPerLine);	// source image position

			// if we already had data in progress, wait for it to complete
			if (sc[tag_id]) {
				// wait until previous source data is available
				mfc_write_tag_mask(1<<tag_id);
				mask = mfc_read_tag_status_any();
			
				if (transparency)
					(*blender)(sb[tag_id], lb[tag_id]+pixelskip, image_width, step, opacity);
				else
					blitSolidLine(sb[tag_id], lb[tag_id]+pixelskip, image_width, step);
	
				// write back the changed screen buffer
				mfc_putf(sb[tag_id], sc[tag_id], numPixels*4, tag_id, 0, 0);
				allTags |= 1<<tag_id;
	
				sc[tag_id] = 0;
			}
	
			// read the image source data and the screen buffer
			mfc_get(lb[tag_id], source, ((cmd->image_bytesPerLine+BYTE_ALIGNMENT)&~BYTE_ALIGNMENT), tag_id, 0, 0);
			if (transparency) {
				mfc_getf(sb[tag_id], screen, numPixels*4, tag_id, 0, 0);
			} else {
				// no transparency, so only get the start and end of the line!
				mfc_getf(sb[tag_id], screen, numPixels*4, tag_id, 0, 0);
		//	except that this doesn't work yet!
		//		mfc_get(sb[tag_id], screen, (BYTE_ALIGNMENT+1), tag_id, 0, 0);
		//		unsigned int ofs=numPixels*4-(BYTE_ALIGNMENT+1);
		//		mfc_get(sb[tag_id]+ofs/4, screen+ofs, (BYTE_ALIGNMENT+1), tag_id, 0, 0);
			}
	
			// record the fact it was in progress
			sc[tag_id] = screen;
    	}
	}
//	printf("Done all lines\n");

	for (i=0; i<32; i++) {
		tag_id = i;
		if (sc[tag_id]) {
			// wait until previous source data is available
			mfc_write_tag_mask(1<<tag_id);
			mask = mfc_read_tag_status_any();
			if (transparency)
				(*blender)(sb[tag_id], lb[tag_id]+pixelskip, image_width, step, opacity);
			else
				blitSolidLine(sb[tag_id], lb[tag_id]+pixelskip, image_width, step);

			// write back the changed screen buffer
			mfc_putf(sb[tag_id], sc[tag_id], numPixels*4, tag_id, 0, 0);
			allTags |= 1<<tag_id;
		}

		free(lb_[i]);
		free(sb_[i]);
	}
	
	// wait for all outstanding DMA to finish
	mfc_write_tag_mask(allTags); //-1); //1<<tag_id);
	mfc_read_tag_status_all();
}



/* I'm deliberately going to ignore the arguments passed in as early versions
 * of libspe2 have the upper and lower 32 bits swapped.
 */
int main(unsigned long long spe_id, unsigned long long program_data_ea, unsigned long long env) {
	SPU_DATA_RECEPTACLE data __SPU_ALIGNED__;

	u32 blitmask = 0;

	unsigned long long ea = 0ULL;
	for (;;) {
		if (!ea) {
			unsigned long msg = spu_read_in_mbox();
			ea  = msg & 0xfffffffe;
		
			if (msg == SPU_MBOX_TERMINATE) {
				break;
			}
			if (msg == SPU_MBOX_SYNC) {
				spu_write_out_mbox(0);
				continue;
			}
			if (msg&1) {
	//			printf("Bit 0 is set, treating as 64 bit...\n");
				unsigned long long msg2 = spu_read_in_mbox();
				ea |= (msg2 & 0xffffffff)<<32;
			}
		}

//		printf("Reading command from address %08llx\n", ea);
		int tag_id = 0;
		mfc_get(&data, ea, sizeof(data), tag_id, 0, 0);
		mfc_write_tag_mask(1<<tag_id);
		int mask = mfc_read_tag_status_any();
	
		SPU_DATA_HEADER* header = ((SPU_DATA_HEADER*)&data);
		int command = header->command;

		switch (command) {
			case SPU_CMD_SET_BLIT_MASK: {
					SPU_DATA_SET_BLIT_MASK* cmd = (SPU_DATA_SET_BLIT_MASK*)&data;
					blitmask = cmd->blitmask;
//					printf("New blit mask is %08x\n", blitmask);
				}
				break;
			case SPU_CMD_BLIT_IMAGE:
				imageBlit( ((SPU_DATA_BLIT*) &data), blitmask );
				break;
			case SPU_CMD_TRIANGLE :
				doTriangle( ((SPU_TRIANGLE_COLOUR*) &data), blitmask );
				break;
			default:
				printf("Unknown command %d\n", command);
				goto ignorerest;
		}

		if (header->next_command) {
			ea = header->next_command;
//			printf("Continuing on with %08lx on spu %d\n", (u32)ea, spe_id);
		} else {
ignorerest:
//			printf("Writing %08lx back from spu %d\n", (u32)ea, spe_id);
			spu_write_out_mbox((u32)ea);
			ea = 0ULL;
		}
	} // main loop
	
	return 0;
}
#endif
