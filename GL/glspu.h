/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

#ifndef __3d_setup_h

#include <GL/gl.h>

GLAPI void GLAPIENTRY glspuSetup(void);
GLAPI void GLAPIENTRY glspuDestroy(void);
GLAPI void GLAPIENTRY glspuFlip(void);
GLAPI void GLAPIENTRY glspuWait(void);

#endif // __3d_setup_h
