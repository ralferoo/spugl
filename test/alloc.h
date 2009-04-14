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

// this is just a very simple alloc-only buffer management strategy
// TEST use only!

void alloc_init(int server);
void alloc_destroy(void);
void* alloc_aligned(unsigned int length);
void alloc_free(void* addr);
