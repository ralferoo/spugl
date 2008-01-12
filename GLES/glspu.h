/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

#ifndef __3d_setup_h

#include <GLES/gl.h>

GL_API void GL_APIENTRY glspuSetup(void);
GL_API void GL_APIENTRY glspuDestroy(void);

// these seem not to be in gles, only gl
GL_API void GL_APIENTRY glVertex3f(GLfloat x, GLfloat y, GLfloat z);
GL_API void GL_APIENTRY glBegin(GLuint type);
GL_API void GL_APIENTRY glEnd();
GL_API void GL_APIENTRY glFlush();

#endif // __3d_setup_h
