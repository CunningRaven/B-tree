#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "bitmap.h"
#include "../syscall_fail.h"

int bitmap_init(struct bitmap *bm, unsigned nbit)
{
  unsigned nbyte;
  
  nbyte = ((nbit + (BITS_IN_INT-1)) & ~(BITS_IN_INT-1)) >> 3;
  if ((bm->dt = malloc(nbyte)) == NULL) {
    syscall_fail("malloc");
    return -1;
  }
  memset(bm->dt, 0, nbyte);
  return 0;
}

void bitmap_set(struct bitmap *bm, unsigned start, unsigned nbit)
{
  unsigned u, r;
  
  u = start / (sizeof (unsigned) * 8);
  start = start % (sizeof (unsigned) * 8);
  r = (sizeof (unsigned) * 8) - start;
  if (nbit <= r) {
    bm->dt[u] |= ((1 << nbit) - 1) << start;
  } else {
    bm->dt[u++] |= ((1 << r) - 1) << start;
    nbit -= r;
    while (nbit > sizeof (unsigned) * 8) {
      bm->dt[u++] = ~0;
      nbit -= sizeof (unsigned) * 8;
    }
    bm->dt[u] |= (1 << nbit) - 1;
  }
}

void bitmap_reset(struct bitmap *bm, unsigned start, unsigned nbit)
{
  unsigned u, r;
  
  u = start / (sizeof (unsigned) * 8);
  start = start % (sizeof (unsigned) * 8);
  r = (sizeof (unsigned) * 8) - start;
  if (nbit <= r) {
    bm->dt[u] &= ~(((1 << nbit) - 1) << start);
  } else {
    bm->dt[u++] &= ~(((1 << r) - 1) << start);
    nbit -= r;
    while (nbit > sizeof (unsigned) * 8) {
      bm->dt[u++] = 0;
      nbit -= sizeof (unsigned) * 8;
    }
    bm->dt[u] &= ~((1 << nbit) - 1);
  }
}

int bitmap_resize(struct bitmap *bm, unsigned old_nbit, unsigned nbit)
{
  void *new_reg;
  unsigned nbyte;
  
  nbyte = ((nbit + (BITS_IN_INT-1)) & ~(BITS_IN_INT-1)) >> 3;
  
  if ((new_reg = realloc(bm->dt, nbyte)) == NULL) {
    syscall_fail("realloc");
    return -1;
  }
  bm->dt = new_reg;
  if (old_nbit < nbit) {
    unsigned old_nbyte;
    old_nbyte = ((old_nbit + (BITS_IN_INT-1)) & ~(BITS_IN_INT-1)) >> 3;
    memset((char *)bm->dt[old_nbyte], 0, nbyte - old_nbyte);
  }
  return 0;
}

int bitmap_first_0(struct bitmap *bm, unsigned nbit)
{
  unsigned u = 0, v, w, x, mask;
  
  while (nbit > BITS_IN_INT) {
    v = bm->dt[u];
    if (v != ~0) {
      for (w = 1, x = 0; w != 0; w <<= 1, x++) {
        if (!(v & w))
          return u * BITS_IN_INT + x;
      }
    } else {
      u++;
      nbit -= BITS_IN_INT;
    }
  }
  mask = (1 << nbit) - 1;
  if ((v = (bm->dt[u] & mask)) == mask)
    return -1;
  mask++;
  for (w = 1, x = 0; w != mask; w <<= 1, x++) {
    if (!(v & w))
      return u * BITS_IN_INT + x;
  }
  return -1;
}
