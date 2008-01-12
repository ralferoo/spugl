/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

#include <stdlib.h>
#include <stdio.h>

#include <libspe.h>
#include "3d.h"

extern spe_program_handle_t spu_3d_handle;
spe_program_handle_t* spu_3d_program = &spu_3d_handle;

typedef struct {
        speid_t spe_id;
	void* local_store;
	SPU_CONTROL* control;
	int master;
} __DRIVER_CONTEXT;

typedef __DRIVER_CONTEXT* DriverContext;
static u32 spe_read(speid_t spe_id);

// initialise
DriverContext _init_3d_driver(int master)
{
	DriverContext context = (DriverContext) malloc(sizeof(__DRIVER_CONTEXT));
	if (context == NULL)
		return NULL;

	context->spe_id = spe_create_thread(0, spu_3d_program, NULL, NULL, -1, 0);
	context->master = master;

	if (context->spe_id==0) {
		free(context);
		return NULL;
	}

	context->local_store = spe_get_ls(context->spe_id);
	printf("Allocated spe %lx, local store at %lx\n",
		context->spe_id, context->local_store);

	spe_write_in_mbox(context->spe_id, 
		 master ? SPU_MBOX_3D_INITIALISE_MASTER
			: SPU_MBOX_3D_INITIALISE_NORMAL); 
	u32 addr = spe_read(context->spe_id);
	context->control = addr + context->local_store;
	context->control->my_local_address = (u64) context->local_store;
//	printf("Control structure at %lx\n", context->control);

//	context->fifo_write = context->local_store + context->control->fifo_start;
//	context->fifo_left = context->control->fifo_size;

	return context;
}

int _exit_3d_driver(DriverContext context)
{
	if (context == NULL)
		return 0;

	// send the SPU a quit command
	spe_write_in_mbox(context->spe_id, SPU_MBOX_3D_TERMINATE);

	// wait for completion and return exit code
	int status = 0;
	spe_wait(context->spe_id, &status, 0);
	return status;
}

u32* _begin_fifo(DriverContext context)
{
	return (u32*)(context->control->fifo_written + context->local_store);
}

void _end_fifo(DriverContext context, u32* fifo)
{
	SPU_CONTROL* control = context->control;
	control->fifo_written = ((void*)fifo) - context->local_store;

	if (control->fifo_written - control->fifo_start
			> control->fifo_size - SPU_MIN_FIFO) {
		*fifo++ = SPU_COMMAND_JMP;
		*fifo++ = control->fifo_start;
		control->fifo_written = ((void*)fifo) - context->local_store;
	}
/*
	int written = ((u32*)fifo) - ((u32*)context->fifo_write);
	context->fifo_left -= written;
	
	if (context->fifo_left < SPU_MIN_FIFO) {
		printf("Only %d bytes remaining; causing jump to %lx\n",
			context->fifo_left, control->fifo_start);
		*fifo++ = SPU_COMMAND_JMP;
		*fifo++ = control->fifo_start;

		control->fifo_written = control->fifo_start;
		context->fifo_left = control->fifo_size;

		control->fifo_written = context->fifo_write;
	}
*/
}

void _bind_child(DriverContext parent, DriverContext child, int assign)
{
	if (parent->master && !child->master) {
		u32* fifo = _begin_fifo(parent);
		*fifo++ = assign ?
			SPU_COMMAND_ADD_CHILD :
			SPU_COMMAND_DEL_CHILD;
		u64 ea = (u64) child->control;
		*fifo++ = (u32)(ea>>32);
		*fifo++ = (u32)(ea&0xffffffffUL);
		*fifo++ = SPU_COMMAND_NOP;
		_end_fifo(parent,fifo);
	}
}

static u32 spe_read(speid_t spe_id) {
	while(!spe_stat_out_mbox(spe_id))
		; //usleep(10);
	return spe_read_out_mbox(spe_id);
}

int main(int argc, char* argv[]) {
//	printf("main entered\n");
	DriverContext ctx = _init_3d_driver(1);
	DriverContext ctx2 = _init_3d_driver(0);
	_bind_child(ctx, ctx2, 1);
//	printf("context = %lx\n", ctx);
//	printf("context2 = %lx\n", ctx2);

	sleep(1);
	u32* fifo = _begin_fifo(ctx);
	*fifo++ = SPU_COMMAND_NOP;
	*fifo++ = SPU_COMMAND_JMP;
	*fifo++ = (u32)((void*)(fifo+2) - ctx->local_store);
	*fifo++ = SPU_COMMAND_NOP;
	*fifo++ = SPU_COMMAND_NOP;
	*fifo++ = SPU_COMMAND_NOP;
	*fifo++ = SPU_COMMAND_NOP;
	_end_fifo(ctx,fifo);
	printf("idle_count values: %d %d\n",
		ctx->control->idle_count,
		ctx2->control->idle_count);
	sleep(1);
	fifo = _begin_fifo(ctx2);
	*fifo++ = SPU_COMMAND_NOP;
	*fifo++ = SPU_COMMAND_NOP;
	_end_fifo(ctx2,fifo);
	printf("idle_count values: %d %d\n",
		ctx->control->idle_count,
		ctx2->control->idle_count);
	sleep(1);
	fifo = _begin_fifo(ctx2);
	*fifo++ = SPU_COMMAND_NOP;
	_end_fifo(ctx2,fifo);
	printf("idle_count values: %d %d\n",
		ctx->control->idle_count,
		ctx2->control->idle_count);
	sleep(1);

	_bind_child(ctx, ctx2, 0);
	sleep(1);

	_exit_3d_driver(ctx);
	_exit_3d_driver(ctx2);
//	printf("main end\n");
	exit(0);
}

#ifdef _RANDOM_CRAP

#define _SPUTALK_C

#include "draw.h"
//#include "spu/protocol.h"


// in my current testing, 4 seems to be the sweet spot. 
// any more doesn't really make much difference.
#define NUM_SPUS 4

// how many commands (i.e. blits we) should allocate for the queue at once
#define SPU_COMMAND_ALLOC_SIZE 100

typedef struct {
        speid_t spe_id;
        unsigned long long command_buffer_head;
        unsigned long long command_buffer_last;
        unsigned long long command_buffer_recycled;
	void* memory_list;
	void* memory_next_free;
	int memory_number_free;
} SpuData;

extern spe_program_handle_t spu_handle;
spe_program_handle_t* program = &spu_handle;

SpuData* Spu_create(spe_program_handle_t* program) {
	int size = sizeof(SpuData);// + data_size + 15;
	SpuData* spu = (SpuData*) malloc(size);
	if (spu == NULL)
		return NULL;

	speid_t spe_id = spe_create_thread(0, program, NULL, NULL, -1, 0);
	if (spe_id==0) {
		free(spu);
		return NULL;
	}
	
	spu->spe_id = spe_id;
	spu->command_buffer_head = 0ULL;
	spu->command_buffer_last = 0ULL;
	spu->command_buffer_recycled = 0ULL;
	spu->memory_list = NULL;
	spu->memory_number_free = 0;

	return spu;
}

void Spu_recycle_head(SpuData* spu) {
	unsigned long long scan = spu->command_buffer_head;
	SPU_DATA_HEADER* packet = NULL;
	int i = 0;
	while (scan) {
		packet = (SPU_DATA_HEADER*) scan;
		scan = packet->next_command;
		i++;
	}
	//printf("Recycling %d slots\n", i);
	if (packet) {
		packet->next_command = spu->command_buffer_recycled;
		spu->command_buffer_recycled = spu->command_buffer_head;
		spu->command_buffer_head = 0ULL;
	}
}

inline void Spu_signalSPU(SpuData* spu, unsigned long long ea) {
	unsigned long msg1 = (unsigned long)  ea;
	unsigned long msg2 = (unsigned long) (ea >> 32);
	if (msg2) {
		spe_write_in_mbox(spu->spe_id, msg1 | 1);
		spe_write_in_mbox(spu->spe_id, msg2);
		//printf("Writing 64 bit EA %08lx:%08lx to spu %d\n", msg1, msg2, spu->spe_id);
	} else {
		spe_write_in_mbox(spu->spe_id, msg1);
		//printf("Writing 32 bit EA %08lx to spu %d\n", msg1, spu->spe_id);
	}
}

int Spu_busy(SpuData* spu) {
	if (spu->command_buffer_last) {
		// outstanding commands on the SPU, see how things are going
		if (!spe_stat_out_mbox(spu->spe_id)) {
			//printf("Busy on spu %d\n", spu->spe_id);
			return 1;			// nothing waiting, signal busy
		}
		//printf("Idle on spu %d\n", spu->spe_id);
		unsigned long last_packet = spe_read_out_mbox(spu->spe_id);
		//printf("Received completion packet %08lx on spu %d\n", last_packet, spu->spe_id);
		unsigned long long scan = spu->command_buffer_head;
		while (scan) {
			//printf("Testing %08llx\n", scan);
			SPU_DATA_HEADER* packet = (SPU_DATA_HEADER*) scan;
			if (last_packet == ((unsigned long) scan) ) {
				// found the last packet processed
				if (packet -> next_command) {
					// but there's more in the queue, inform the SPU and return busy
					Spu_signalSPU(spu, packet->next_command);
					return 1;
				} else {
					// found the last packet processed and there are no more, signal done
					//printf("That was the last packet on spu %d\n", spu->spe_id);
					spu->command_buffer_last = 0ULL;
					Spu_recycle_head(spu);
					return 0;
				}
			}
			scan = packet -> next_command;		// try next packet
		} // while (scan)

		// last_packet not found in list. all we can do is junk all subsequent commands and say we're no longer busy
		printf("ERROR: SPU %d claims to have processed packet %08lx but we don't have it!\n", (int)spu->spe_id, last_packet);
		spu->command_buffer_last = 0ULL;
		Spu_recycle_head(spu);
		return 0;
	} else {
		//printf("Nothing happening on spu %d\n",spu->spe_id);
		// nothing in queue, just return that we're not busy
		return 0;
	}
}

SPU_DATA_HEADER* Spu_allocate(SpuData* spu) {
	if (spu->command_buffer_recycled) {
		SPU_DATA_HEADER* packet = (SPU_DATA_HEADER*) spu->command_buffer_recycled;
		spu->command_buffer_recycled = packet->next_command;
		packet->next_command = 0ULL;
		packet->command = 0;
		return packet;
	} else {
		if (!spu->memory_number_free) {
			int r = SPU_COMMAND_ALLOC_SIZE;
			int b = (sizeof(SPU_DATA_UNION_OF_STRUCTURES)+15)&~15;
			int s = r*b + 15 + 16;
	
			void* p = malloc(s);
			*((void**)p) = spu->memory_list;
			spu->memory_list = p;
			p += sizeof(void**);
			spu->memory_next_free = (void*) ((((int)p)+15) & ~15);
			spu->memory_number_free = r;
		}

		SPU_DATA_HEADER* packet = spu->memory_next_free;
		spu->memory_next_free += (sizeof(SPU_DATA_UNION_OF_STRUCTURES)+15)&~15;
		spu->memory_number_free --;

		packet->next_command = 0ULL;
		packet->command = 0;
		return packet;
	}
}

void Spu_queue(SpuData* spu, SPU_DATA_HEADER* packet) {
	packet->next_command = 0ULL;
	unsigned long long ea = (unsigned long long) packet;

	if (spu->command_buffer_last) {
		SPU_DATA_HEADER* last_packet = (SPU_DATA_HEADER*) spu->command_buffer_last;
		last_packet->next_command = ea;
		//printf("Appended packet %08llx, command %d to packet %08lx on SPU %d\n", ea, packet->command, last_packet, spu->spe_id);
		spu->command_buffer_last = (unsigned long long) packet;
		if (spe_stat_out_mbox(spu->spe_id)) {
//			printf("SPU finished, prodding...\n");
			Spu_busy(spu);
		}
	} else {
//		printf("Starting queue with packet %08llx, command %d on SPU %d\n", ea, packet->command, spu->spe_id);
		spu->command_buffer_head = ea;
		spu->command_buffer_last = ea;
		Spu_signalSPU(spu, ea);
	}
}

int Spu_destroy(SpuData* spu) {
	// if there are any outstanding commands on the SPU, wait for them to finish
	if (spu->command_buffer_last) {
		while (!spe_stat_out_mbox(spu->spe_id));
		Spu_recycle_head(spu);
	}

	// free anything that remains
	void* p = spu->memory_list;
	while (p) {
		void* p2 = p;
		p = *((void**)p);
		//printf("Freeing %08lx\n", p2);
		free(p2);
	}

	// send the SPU a quit command
	spe_write_in_mbox(spu->spe_id, SPU_MBOX_TERMINATE);

	// wait for completion and return exit code
	int status = 0;
	spe_wait(spu->spe_id, &status, 0);
	return status;
}

void* blit_create(void) {
	SpuData** spus = malloc(sizeof(SpuData*)*NUM_SPUS);
	if (spus == NULL)
		return NULL;

	int got = 0;
	int i;
	for (i=0; i<NUM_SPUS;i++) {
		spus[i] = Spu_create(program);
		if (spus[i] != NULL) got++;
	}

	if (got == 0) {
		printf("No SPUs are available for blitting; PPU fall back mode is much slower...\n");
		free(spus);
		return NULL;
	}

	SPU_DATA_SET_BLIT_MASK command __SPU_ALIGNED__;
	command.header.command = SPU_CMD_SET_BLIT_MASK;
	command.header.next_command = 0ULL;
	command.blitmask = 0;

	for (i=0; i<32; i++) {
		if ( (i%got) == 0)
			command.blitmask |= 1<<i;
	}
	for (i=0; i<NUM_SPUS;i++) {
		if (spus[i]) {
			while (Spu_busy(spus[i]));			// spinlock on busy spu
			spe_write_in_mbox(spus[i]->spe_id, (unsigned long) &command);
			spe_read(spus[i]->spe_id);
			command.blitmask <<= 1;
		}
	}

	return spus;
}

int blit_destroy(void* spu) {
	if (spu != NULL) {
		SpuData** spus = (SpuData**)spu;
		int i;
		for (i=0; i<NUM_SPUS;i++) {
			if (spus[i])
				Spu_destroy(spus[i]);
		}
		free(spu);
	}
	return 0;
}

int blit_sync(void* spu_) {
	if (spu_ == NULL)
		return 0;

	SpuData** spus = (SpuData**)spu_;

	int i;
	for (i=0; i<NUM_SPUS;i++) {
		if (spus[i]) {
			while (Spu_busy(spus[i])) {			// spinlock on busy spu
				// printf("zzzz on spu %d\n", spus[i]->spe_id);
				//usleep(1000);
			}
			spe_write_in_mbox(spus[i]->spe_id, SPU_MBOX_SYNC);
			spe_read(spus[i]->spe_id);
		}
	}
	return 0;
}


int blit_image(void* spu_, char* fb, int x, int y, int fbWidth, int fbHeight, int fbClipWidth, ImageData image, int opacity) {
	SpuData** spus = (SpuData**)spu_;

	if (opacity<0) {
		// no point even worth continuing
		return 0;
	}

	int i;
	for (i=0; i<NUM_SPUS;i++) {
		if (spus[i]) {
			SPU_DATA_BLIT* command = Spu_allocate(spus[i]);

			command->header.command = SPU_CMD_BLIT_IMAGE;
			command->header.next_command = 0ULL;
			command->header.framebuffer = (unsigned long long) fb;		// ignore warning on 32 bit systems!
			command->header.fb_width = fbWidth;
			command->header.fb_clip_width = fbClipWidth;
			command->header.fb_height = fbHeight;

			command->x = x;
			command->y = y;
			command->image_width = image->width;
			command->image_height = image->height;
			command->image_bytesPerLine = image->bytesPerLine;
			command->pixels = (unsigned long long) image->pixels;
	
			if (opacity>=256) {
				command->opacity = 256;
				command->transparency = image->transparency;
			} else {
				command->opacity = opacity;
				command->transparency = ImageData_ALPHA;
			}
	
			Spu_queue(spus[i], (SPU_DATA_HEADER*) command);
		}
	}
	return 0;
}


int blit_triangle(void* spu_, char* fb, int fbWidth, int fbHeight, int fbClipWidth, VERTEX *a, VERTEX* b, VERTEX *c, ImageData image) {
	SpuData** spus = (SpuData**)spu_;

	int i;
	for (i=0; i<NUM_SPUS;i++) {
		if (spus[i]) {
			SPU_TRIANGLE_COLOUR* command = Spu_allocate(spus[i]);

			command->header.command = SPU_CMD_TRIANGLE;
			command->header.next_command = 0ULL;
			command->header.framebuffer = (unsigned long long) fb;		// ignore warning on 32 bit systems!
			command->header.fb_width = fbWidth;
			command->header.fb_clip_width = fbClipWidth;
			command->header.fb_height = fbHeight;

			if (image) {
				command->tex = image->pixels;
				command->tex_stride = image->bytesPerLine;
				command->tex_width = image->width;
				command->tex_height = image->height;
			} else {
				command->tex = 0ULL;
				command->tex_stride = 0;
				command->tex_width = 0;
				command->tex_height = 0;
			}

			memcpy(&command->vertex[0], a, sizeof(VERTEX));
			memcpy(&command->vertex[1], b, sizeof(VERTEX));
			memcpy(&command->vertex[2], c, sizeof(VERTEX));
	
			Spu_queue(spus[i], (SPU_DATA_HEADER*) command);
		}
	}
	return 0;
}


int blit_tile(void* spu_, char* fb, int fbWidth, int fbHeight, int offx, int offy) {
	return 0;
}

#endif
