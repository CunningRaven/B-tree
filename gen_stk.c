#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "syscall_fail.h"
#include "gen_stk.h"

int gen_stk_init(struct gen_stk *self, size_t init_cap, size_t uni_siz)
{
  if ((self->addr = malloc(uni_siz * init_cap)) == NULL) {
    syscall_fail("malloc");
    return -1;
  }
  self->init_cap = self->cap = init_cap;
  self->uni_siz = uni_siz;
  self->cnt = 0;

  return 0;
}

int gen_stk_push(struct gen_stk *self, void *elem)
{
  void *new_addr;
  size_t new_cap;

  if (self->cnt >= self->cap) {
    new_cap = self->cap + self->init_cap;
    if ((new_addr = realloc(self->addr, new_cap)) == NULL) {
      syscall_fail("realloc");
      return -1;
    }
    self->addr = new_addr;
    self->cap = new_cap;
  }
  memcpy(self->addr + self->cnt++ * self->uni_siz, elem, self->uni_siz);
  return 0;
}

void gen_stk_pop(struct gen_stk *self, void *buff)
{
  memcpy(buff, self->addr + --self->cnt * self->uni_siz, self->uni_siz);
}

int gen_stk_trunc(struct gen_stk *self, size_t new_cap)
{
  void *new_addr;

  if ((new_addr = realloc(self->addr, new_cap)) == NULL) {
    syscall_fail("realloc");
    return -1;
  }
  self->addr = new_addr;
  self->cap = new_cap;
  return 0;
}

int gen_stk_copy(struct gen_stk *dst, struct gen_stk *src)
{
  *dst = *src;
  if ((dst->addr = malloc(src->cap * src->uni_siz)) == NULL) {
    syscall_fail("malloc");
    return -1;
  }
  memcpy(dst->addr, src->addr, src->cnt * src->uni_siz);
  return 0;
}


