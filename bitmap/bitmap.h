#ifndef BITMAP_H
#define BITMAP_H

#include <stdlib.h>

#define BITS_IN_INT (sizeof (unsigned) * 8)

struct bitmap {
  unsigned *dt;
};

int bitmap_init(struct bitmap *bm, unsigned nbit);
void bitmap_set(struct bitmap *bm, unsigned start, unsigned nbit);
void bitmap_reset(struct bitmap *bm, unsigned start, unsigned nbit);
int bitmap_resize(struct bitmap *bm, unsigned old_nbit, unsigned nbit);
int bitmap_first_0(struct bitmap *bm, unsigned nbit);
#endif
