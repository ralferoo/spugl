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
	_OUT_EA(fifo, &fifo[5]);
	_OUTu(fifo, 42);
	_OUTu(fifo, 43);
	_OUTu(fifo, 44);
	_OUTu(fifo, 45);
	_OUTu(fifo, 46);
	_OUTu(fifo, 47);
	_OUTu(fifo, 48);
	_OUTf(fifo, 1.0);
	_OUTf(fifo, 2.0);
	_OUTf(fifo, 3.14);
	_OUTf(fifo, -3.14);
	*fifo++ = SPU_COMMAND_NOP;
	*fifo++ = SPU_COMMAND_NOP;
	_end_fifo(ctx,fifo);
	printf("idle_count values: %d %d\n",
		_3d_idle_count(ctx), _3d_idle_count(ctx2));

	sleep(1);

	fifo = _begin_fifo(ctx2);
	*fifo++ = SPU_COMMAND_NOP;
	*fifo++ = SPU_COMMAND_NOP;
	_end_fifo(ctx2,fifo);
	printf("idle_count values: %d %d\n",
		_3d_idle_count(ctx), _3d_idle_count(ctx2));

	sleep(1);

	fifo = _begin_fifo(ctx2);
	*fifo++ = SPU_COMMAND_NOP;
	_end_fifo(ctx2,fifo);
	printf("idle_count values: %d %d\n",
		_3d_idle_count(ctx), _3d_idle_count(ctx2));

	sleep(1);

	_bind_child(ctx, ctx2, 0);

	sleep(1);

	_exit_3d_driver(ctx);
	_exit_3d_driver(ctx2);
//	printf("main end\n");
	exit(0);
}
