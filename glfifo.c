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

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "fifo.h"
#include "struct.h"
#include <GL/gl.h>

extern BitmapImage _getScreen(char* dumpName);
extern void _closeScreen(void);
extern BitmapImage _flipScreen(void);

static DriverContext ctx = NULL;
static BitmapImage screen = NULL;

extern gimp_image berlin;
extern gimp_image oranges;
extern gimp_image mim;
extern gimp_image ralf;
extern gimp_image gate;
extern gimp_image space;
extern gimp_image tongariro;

Texture convertGimpTexture(gimp_image* source);
static Texture localTextures[10];

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

GLAPI unsigned long GLAPIENTRY glspuCacheMisses(void)
{
	return _3d_cache_misses(ctx);
}

GLAPI unsigned long GLAPIENTRY glspuBlocksProduced(void)
{
	return _3d_blocks_produced(ctx);
}

static void updateScreenPointer(void) 
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_SCREEN_INFO,3,1);
	OUT_RINGea(_FROM_EA(screen->address));
	OUT_RING(screen->width);
	OUT_RING(screen->height);
	OUT_RING(screen->bytes_per_line);
	FIFO_EPILOGUE();
}

GLAPI void GLAPIENTRY glspuSetup(char* dumpName)
{
	ctx = _init_3d_driver(1);
	screen = _getScreen(dumpName);
	updateScreenPointer();

	// TODO: this should not be here!
	localTextures[0] = convertGimpTexture(&berlin);
	localTextures[1] = convertGimpTexture(&oranges);
	localTextures[2] = convertGimpTexture(&ralf);
	localTextures[3] = convertGimpTexture(&gate);
	localTextures[4] = convertGimpTexture(&space);
	localTextures[5] = convertGimpTexture(&tongariro);
	localTextures[6] = convertGimpTexture(&mim);
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
	BEGIN_RING(SPU_COMMAND_CLEAR_SCREEN,1,0);
	FIFO_EPILOGUE();
}

GLAPI void GLAPIENTRY glspuSetFlag(u32* ptr, u32 value)
{
	FIFO_PROLOGUE(ctx,3);
	BEGIN_RING(SPU_COMMAND_SET_FLAG,1,1);
	OUT_RINGea(_FROM_EA(ptr));
	OUT_RING(value);
	FIFO_EPILOGUE();
}

GLAPI void GLAPIENTRY glspuWait(void)
{
	_waitScreen();
}

GLAPI void GLAPIENTRY glFlush()
{
	FIFO_PROLOGUE(ctx,2);
	BEGIN_RING(SPU_COMMAND_SYNC,0,0);
	FIFO_EPILOGUE();
	_flush_3d_driver(ctx);
}

GLAPI void GLAPIENTRY glBegin(GLuint type)
{
	FIFO_PROLOGUE(ctx,2);
	BEGIN_RING(SPU_COMMAND_GL_BEGIN,1,0);
	OUT_RING(type);
	FIFO_EPILOGUE();
}

GLAPI void GLAPIENTRY glEnd()
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_GL_END,0,0);
	FIFO_EPILOGUE();
}

GLAPI void GLAPIENTRY glVertex3f (GLfloat x, GLfloat y, GLfloat z)
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_GL_VERTEX3,3,0);
	OUT_RINGf(x);
	OUT_RINGf(y);
	OUT_RINGf(z);
	FIFO_EPILOGUE();
}

GLAPI void GLAPIENTRY glColor3f (GLfloat r, GLfloat g, GLfloat b)
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_GL_COLOR3,3,0);
	OUT_RINGf(r);
	OUT_RINGf(g);
	OUT_RINGf(b);
	FIFO_EPILOGUE();
}

GLAPI void GLAPIENTRY glColor4f (GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_GL_COLOR4,3,0);
	OUT_RINGf(r);
	OUT_RINGf(g);
	OUT_RINGf(b);
	OUT_RINGf(a);
	FIFO_EPILOGUE();
}

GLAPI void GLAPIENTRY glColor3ub (GLubyte r, GLubyte g, GLubyte b)
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_GL_COLOR3,3,0);
	OUT_RINGf(r/255.0);
	OUT_RINGf(g/255.0);
	OUT_RINGf(b/255.0);
	FIFO_EPILOGUE();
}

GLAPI void GLAPIENTRY glColor4ub (GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_GL_COLOR4,4,0);
	OUT_RINGf(r/255.0);
	OUT_RINGf(g/255.0);
	OUT_RINGf(b/255.0);
	OUT_RINGf(a/255.0);
	FIFO_EPILOGUE();
}

GLAPI void GLAPIENTRY glTexCoord2f (GLfloat s, GLfloat t)
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_GL_TEX_COORD2,2,0);
	OUT_RINGf(s);
	OUT_RINGf(t);
	FIFO_EPILOGUE();
}

GLAPI void GLAPIENTRY glTexCoord3f (GLfloat s, GLfloat t, GLfloat u)
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_GL_TEX_COORD3,3,0);
	OUT_RINGf(s);
	OUT_RINGf(t);
	OUT_RINGf(u);
	FIFO_EPILOGUE();
}

GLAPI void GLAPIENTRY glTexCoord4f (GLfloat s, GLfloat t, GLfloat u, GLfloat v)
{
	FIFO_PROLOGUE(ctx,10);
	BEGIN_RING(SPU_COMMAND_GL_TEX_COORD4,4,0);
	OUT_RINGf(s);
	OUT_RINGf(t);
	OUT_RINGf(u);
	OUT_RINGf(v);
	FIFO_EPILOGUE();
}

GLAPI void GLAPIENTRY glBindTexture(GLenum target, GLuint texture)
{
	Texture tex = localTextures[texture];
	unsigned int tex_max_mipmap = tex->tex_max_mipmap;

	FIFO_PROLOGUE(ctx,10+4*(1+tex_max_mipmap));
	BEGIN_RING(SPU_COMMAND_GL_BIND_TEXTURE,3+2*(1+tex_max_mipmap),1+tex_max_mipmap);
	OUT_RING(tex->tex_log2_x);
	OUT_RING(tex->tex_log2_y);
	OUT_RING(tex_max_mipmap);

	for (int i=0; i<=tex_max_mipmap; i++) {
		OUT_RINGea(tex->tex_data[i]);
		OUT_RING(tex->tex_t_mult[i]);
		OUT_RING(tex->tex_id_base[i]);
	}
	FIFO_EPILOGUE();
}

GLAPI void GLAPIENTRY calculateMipmap(void* tl, void* tr, void* bl, void* br, void* o)
{
	FIFO_PROLOGUE(ctx,15);
	BEGIN_RING(SPU_COMMAND_GENERATE_MIP_MAP,0,5);
	OUT_RINGea(tl);
	OUT_RINGea(tr);
	OUT_RINGea(bl);
	OUT_RINGea(br);
	OUT_RINGea(o);
	FIFO_EPILOGUE();
}

/*
// http://publib.boulder.ibm.com/infocenter/systems/index.jsp?topic=/com.ibm.aix.opengl/doc/openglrf/gluPerspective.htm
GLAPI void GLAPIENTRY glFrustum( GLdouble left, GLdouble right,
                                   GLdouble bottom, GLdouble top,
                                   GLdouble near_val, GLdouble far_val )
{
	GLdouble A = (right+left)/(right-left);
	GLdouble B = (top+bottom)/(top-bottom);
	GLdouble C = (far+near)/(far-near);
	GLdouble D = (2*far*near)/(far-near);

	GLdouble mat[16] = {
		2.0*near/(right-left),	0.0,			0.0,	 0.0,
		0.0,			2.0*near/(top-0bottom),	0.0,	 0.0,
		A,			B,			C,	-1.0,
		0.0,			0.0,			D,	 0.0};

	glMultMatrixd(mat);
}

// http://publib.boulder.ibm.com/infocenter/systems/index.jsp?topic=/com.ibm.aix.opengl/doc/openglrf/gluPerspective.htm

void
gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar)
{
   GLdouble xmin, xmax, ymin, ymax;

   ymax = zNear * tan(fovy * M_PI / 360.0);
   ymin = -ymax;
   xmin = ymin * aspect;
   xmax = ymax * aspect;


   glFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
}
*/
