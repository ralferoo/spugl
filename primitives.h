/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

#ifndef __struct_h
#define __struct_h

#include "struct.h"

extern void imp_point(vertex_state a);
extern void imp_line(vertex_state a, vertex_state b);
extern void imp_triangle(vertex_state a, vertex_state b, vertex_state c);

#endif // __struct_h
