/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

#ifndef __3d_setup_h

#include <GL/gl.h>

GLAPI void GLAPIENTRY glspuNop(void);
GLAPI unsigned int glspuTarget();
GLAPI void GLAPIENTRY glspuJump(unsigned int target);

GLAPI void GLAPIENTRY glspuSetup(char* dumpName);
GLAPI void GLAPIENTRY glspuDestroy(void);
GLAPI void GLAPIENTRY glspuFlip(void);
GLAPI void GLAPIENTRY glspuWait(void);
GLAPI unsigned long GLAPIENTRY glspuCounter(void);
GLAPI unsigned long GLAPIENTRY glspuBlocksProduced(void);
GLAPI unsigned long GLAPIENTRY glspuCacheMisses(void);

GLAPI void GLAPIENTRY calculateMipmap(void* tl, void* tr, void* bl, void* br, void* o);

#endif // __3d_setup_h
