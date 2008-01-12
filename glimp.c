/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

#include <stdlib.h>
#include <stdio.h>

#include "3d.h"
#include <GLES/gl.h>

static DriverContext ctx;

GL_API void GL_APIENTRY glspuSetup(void)
{
	ctx = _init_3d_driver(1);
}

GL_API void GL_APIENTRY glspuDestroy(void)
{
	_exit_3d_driver(ctx);
}

GL_API void GL_APIENTRY glFlush()
{
	_flush_3d_driver(ctx);
}

GL_API void GL_APIENTRY glBegin(GLuint type)
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_BEGIN,1);
	OUT_RING(type);
	FIFO_EPILOGUE();
}

GL_API void GL_APIENTRY glEnd()
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_END,1);
	FIFO_EPILOGUE();
}

GL_API void GL_APIENTRY glVertex3f (GLfloat x, GLfloat y, GLfloat z)
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_VERTEX3f,3);
	OUT_RINGf(x);
	OUT_RINGf(y);
	OUT_RINGf(z);
	FIFO_EPILOGUE();
}

int xmain(int argc, char* argv[]) {
//	printf("main entered\n");
	DriverContext ctx = _init_3d_driver(1);
	DriverContext ctx2 = _init_3d_driver(0);
	_bind_child(ctx, ctx2, 1);
//	printf("context = %lx\n", ctx);
//	printf("context2 = %lx\n", ctx2);

	sleep(1);
	FIFO_PROLOGUE(ctx,100);
	BEGIN_RING(SPU_COMMAND_NOP,1);
	BEGIN_RING(SPU_COMMAND_JMP,3);
	OUT_RINGea(&__fifo_ptr[5]);
	OUT_RING(42);
	OUT_RING(43);
	OUT_RING(44);
	OUT_RING(45);
	OUT_RING(46);
	OUT_RING(47);
	OUT_RING(48);
	OUT_RINGf(1.0);
	OUT_RINGf(2.0);
	OUT_RINGf(3.14);
	OUT_RINGf(-3.14);
	BEGIN_RING(SPU_COMMAND_NOP,1);
	BEGIN_RING(SPU_COMMAND_NOP,1);
	FIFO_EPILOGUE();
	printf("idle_count values: %d %d\n",
		_3d_idle_count(ctx), _3d_idle_count(ctx2));

	sleep(1);

	u32* fifo = _begin_fifo(ctx2);
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
