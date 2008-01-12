/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

#include <spu_mfcio.h>
#include "fifo.h"
#include "struct.h"

static void debug(vertex_state v)
{
	printf("v=(%.2f,%.2f,%.2f,%.2f)", v.v.x, v.v.y, v.v.z, v.v.w);
	printf(" c=(%.2f,%.2f,%.2f,%.2f)",
		v.colour.x, v.colour.y, v.colour.z, v.colour.w);
}

void imp_point(vertex_state a)
{
	printf("point\t");
	debug(a);printf("\n\n");
}

void imp_line(vertex_state a, vertex_state b)
{
	printf("line\t");
	debug(a);printf(",\n\t");debug(b);printf("\n\n");
}

void imp_triangle(vertex_state a, vertex_state b, vertex_state c)
{
	printf("tri\t");
	debug(a);printf(",\n\t");debug(b);printf(",\n\t");debug(c);printf("\n\n");
}


