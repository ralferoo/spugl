/****************************************************************************
 *
 * SPU 3d rasterisation library
 *
 * (c) 2008 Ranulf Doswell <dev@ranulf.net> made available under LGPL
 *
 ****************************************************************************/

#ifndef __types_h
#define __types_h

typedef unsigned int u32;
typedef unsigned long long u64;

typedef struct {
  unsigned int   width;
  unsigned int   height;
  unsigned int   bytes_per_pixel; /* 3:RGB, 4:RGBA */ 
  unsigned char  pixel_data[1];
} gimp_image;

#endif // __types_h
