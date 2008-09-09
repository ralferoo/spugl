/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

#ifndef __3d_setup_h

#include <GL/gl.h>

GLAPI void GLAPIENTRY spuglNop(void);
GLAPI unsigned int spuglTarget();
GLAPI void GLAPIENTRY spuglJump(unsigned int target);

GLAPI void GLAPIENTRY spuglSetup(char* dumpName);
GLAPI void GLAPIENTRY spuglDestroy(void);
GLAPI void GLAPIENTRY spuglFlip(void);
GLAPI void GLAPIENTRY spuglWait(void);
GLAPI unsigned long GLAPIENTRY spuglCounter(void);
GLAPI unsigned long GLAPIENTRY spuglBlocksProduced(void);
GLAPI unsigned long GLAPIENTRY spuglCacheMisses(void);

GLAPI void GLAPIENTRY calculateMipmap(void* tl, void* tr, void* bl, void* br, void* o);

#endif // __3d_setup_h
