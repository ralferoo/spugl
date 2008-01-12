/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

#include <stdlib.h>
#include <stdio.h>

#include "fifo.h"
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
	BEGIN_RING(SPU_COMMAND_GL_BEGIN,1);
	OUT_RING(type);
	FIFO_EPILOGUE();
}

GL_API void GL_APIENTRY glEnd()
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_GL_END,1);
	FIFO_EPILOGUE();
}

GL_API void GL_APIENTRY glVertex3f (GLfloat x, GLfloat y, GLfloat z)
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_GL_VERTEX3,3);
	OUT_RINGf(x);
	OUT_RINGf(y);
	OUT_RINGf(z);
	FIFO_EPILOGUE();
}

GL_API void GL_APIENTRY glColor3f (GLfloat r, GLfloat g, GLfloat b)
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_GL_COLOR3,3);
	OUT_RINGf(r);
	OUT_RINGf(g);
	OUT_RINGf(b);
	FIFO_EPILOGUE();
}

GL_API void GL_APIENTRY glColor4f (GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_GL_COLOR4,3);
	OUT_RINGf(r);
	OUT_RINGf(g);
	OUT_RINGf(b);
	OUT_RINGf(a);
	FIFO_EPILOGUE();
}

GL_API void GL_APIENTRY glColor3ub (GLubyte r, GLubyte g, GLubyte b)
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_GL_COLOR3,3);
	OUT_RINGf(r/255.0);
	OUT_RINGf(g/255.0);
	OUT_RINGf(b/255.0);
	FIFO_EPILOGUE();
}

GL_API void GL_APIENTRY glColor4ub (GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_GL_COLOR4,3);
	OUT_RINGf(r/255.0);
	OUT_RINGf(g/255.0);
	OUT_RINGf(b/255.0);
	OUT_RINGf(a/255.0);
	FIFO_EPILOGUE();
}
