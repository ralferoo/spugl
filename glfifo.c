/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> 
 *
 * This library may not be used or distributed without a licence, please
 * contact me for information if you wish to use it.
 *
 ****************************************************************************/

#include <stdlib.h>
#include <stdio.h>

#include "fifo.h"
#include "struct.h"
#include <GL/gl.h>

extern BitmapImage _getScreen(void);
extern void _closeScreen(void);
extern BitmapImage _flipScreen(void);

static DriverContext ctx = NULL;
static BitmapImage screen = NULL;

GLAPI GLenum GLAPIENTRY glGetError(void)
{
	return _3d_error(ctx);
}

GLAPI unsigned long GLAPIENTRY glspuCounter(void)
{
	return _3d_idle_count(ctx);
}

GLAPI unsigned long GLAPIENTRY glspuBlockedCounter(void)
{
	return _3d_blocked_count(ctx);
}

static void updateScreenPointer(void) 
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_SCREEN_INFO,1);
	OUT_RINGea(_FROM_EA(screen->address));
	OUT_RING(screen->width);
	OUT_RING(screen->height);
	OUT_RING(screen->bytes_per_line);
	FIFO_EPILOGUE();
}

GLAPI void GLAPIENTRY glspuSetup(void)
{
	ctx = _init_3d_driver(1);
	screen = _getScreen();
	updateScreenPointer();
}

GLAPI void GLAPIENTRY glspuDestroy(void)
{
	_exit_3d_driver(ctx);
	_closeScreen();
	ctx = NULL;
	screen = NULL;
}

GLAPI void GLAPIENTRY glspuFlip(void)
{
	screen = _flipScreen();
	updateScreenPointer();
}

GLAPI void GLAPIENTRY glspuClear(void)
{
	FIFO_PROLOGUE(ctx,2);
	BEGIN_RING(SPU_COMMAND_CLEAR_SCREEN,1);
	FIFO_EPILOGUE();
}

GLAPI void GLAPIENTRY glspuWait(void)
{
	_waitScreen();
}

GLAPI void GLAPIENTRY glFlush()
{
	FIFO_PROLOGUE(ctx,2);
	BEGIN_RING(SPU_COMMAND_SYNC,1);
	FIFO_EPILOGUE();
	_flush_3d_driver(ctx);
}

GLAPI void GLAPIENTRY glBegin(GLuint type)
{
	FIFO_PROLOGUE(ctx,2);
	BEGIN_RING(SPU_COMMAND_GL_BEGIN,1);
	OUT_RING(type);
	FIFO_EPILOGUE();
}

GLAPI void GLAPIENTRY glEnd()
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_GL_END,0);
	FIFO_EPILOGUE();
}

GLAPI void GLAPIENTRY glVertex3f (GLfloat x, GLfloat y, GLfloat z)
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_GL_VERTEX3,3);
	OUT_RINGf(x);
	OUT_RINGf(y);
	OUT_RINGf(z);
	FIFO_EPILOGUE();
}

GLAPI void GLAPIENTRY glColor3f (GLfloat r, GLfloat g, GLfloat b)
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_GL_COLOR3,3);
	OUT_RINGf(r);
	OUT_RINGf(g);
	OUT_RINGf(b);
	FIFO_EPILOGUE();
}

GLAPI void GLAPIENTRY glColor4f (GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_GL_COLOR4,3);
	OUT_RINGf(r);
	OUT_RINGf(g);
	OUT_RINGf(b);
	OUT_RINGf(a);
	FIFO_EPILOGUE();
}

GLAPI void GLAPIENTRY glColor3ub (GLubyte r, GLubyte g, GLubyte b)
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_GL_COLOR3,3);
	OUT_RINGf(r/255.0);
	OUT_RINGf(g/255.0);
	OUT_RINGf(b/255.0);
	FIFO_EPILOGUE();
}

GLAPI void GLAPIENTRY glColor4ub (GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_GL_COLOR4,3);
	OUT_RINGf(r/255.0);
	OUT_RINGf(g/255.0);
	OUT_RINGf(b/255.0);
	OUT_RINGf(a/255.0);
	FIFO_EPILOGUE();
}

GLAPI void GLAPIENTRY glTexCoord2f (GLfloat s, GLfloat t)
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_GL_TEX_COORD2,2);
	OUT_RINGf(s);
	OUT_RINGf(t);
	FIFO_EPILOGUE();
}

GLAPI void GLAPIENTRY glTexCoord3f (GLfloat s, GLfloat t, GLfloat u)
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_GL_TEX_COORD3,3);
	OUT_RINGf(s);
	OUT_RINGf(t);
	OUT_RINGf(u);
	FIFO_EPILOGUE();
}

GLAPI void GLAPIENTRY glTexCoord4f (GLfloat s, GLfloat t, GLfloat u, GLfloat v)
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_GL_TEX_COORD4,4);
	OUT_RINGf(s);
	OUT_RINGf(t);
	OUT_RINGf(u);
	OUT_RINGf(v);
	FIFO_EPILOGUE();
}

GLAPI void GLAPIENTRY glBindTexture(GLenum target, GLuint texture)
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_GL_BIND_TEXTURE,2);
	OUT_RING(target);
	OUT_RING(texture);
	FIFO_EPILOGUE();
}

