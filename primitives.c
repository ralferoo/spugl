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

static void debug(float4 v)
{
	printf("(%f,%f,%f,%f)", v.x,v.y,v.z,v.w);
}

void imp_point(float4 a)
{
	printf("point ");
	debug(a);printf("\n");
}

void imp_line(float4 a, float4 b)
{
	printf("line ");
	debug(a);printf(",");debug(b);printf("\n");
}

void imp_triangle(float4 a, float4 b, float4 c)
{
	printf("tri ");
	debug(a);printf(",");debug(b);printf(",");debug(c);printf("\n");
}


