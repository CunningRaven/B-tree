#ifndef GEN_STK_H
#define GEN_STK_H

struct gen_stk {
  size_t init_cap;
  size_t cap;
  size_t cnt;
  size_t uni_siz;
  void *addr;
};

#define gen_stk_empty(self) (!(self)->cnt)

static inline void gen_stk_delete(struct gen_stk *self)
{
  free(self->addr);
}

static inline void gen_stk_dump(struct gen_stk *dst, struct gen_stk *src)
{
  *dst = *src;
}

int gen_stk_init(struct gen_stk *self, size_t init_cap, size_t uni_siz);
int gen_stk_push(struct gen_stk *self, void *elem);
void gen_stk_pop(struct gen_stk *self, void *buff);
int gen_stk_trunc(struct gen_stk *self, size_t new_cap);
int gen_stk_copy(struct gen_stk *dst, struct gen_stk *src);

#endif
