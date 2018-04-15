#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "../b_plus_tree.h"

#define BPT_ORDER 4
// #define SILENT
#define ENTRY_CNT 5000
#define SAMPLE_MAX 1000
// #define PRINT_EVERY_INS
#define PRINT_EVERY_DEL
#define UPDATE_RANDSEED

void print_bpt(struct bpt_stat *bstat);
void check_bpt(struct bpt_stat *bstat);

int cmp_int(bpt_t a, bpt_t b)
{
  return (int)a.ptr - (int)b.ptr;
}


int main(void)
{
  struct bpt_entry entry;
  struct bpt_stat bstat;
  struct gen_stk stk, kstk;
  int i, t;
#ifdef UPDATE_RANDSEED
  FILE *fp;
#endif
  int ins_cnt = 0, rst;
#ifdef UPDATE_RANDSEED
  time_t time_val = time(NULL);

  setbuf(stdout, NULL);
  if ((fp = fopen("random_seed", "a")) == NULL) {
    perror("fopen");
    exit(1);
  }
  srand(time_val);
  fprintf(fp, "Random seed: %lu\n", (unsigned long)time_val);
  fflush(fp);
  fclose(fp);
#else
  srand(1523786504);
#endif
  if (bpt_init(&bstat, BPT_ORDER) == -1)
    return 1;
  if (gen_stk_init(&stk, BPT_STK_CAP_INIT, sizeof (struct bpt_frm)) == -1)
    return 1;
  if (gen_stk_init(&kstk, ENTRY_CNT, sizeof (int)) == -1)
    return 1;
  for (i = 0; i < ENTRY_CNT; i++) {
    entry.key.ptr = (void *)(rand() % SAMPLE_MAX);
    entry.val.ptr = (void *)(rand() % SAMPLE_MAX);
    if ((rst = bpt_insert(entry, cmp_int, bpt_pred_0,
        &stk, 1, &bstat)) == BPT_ERROR)
      return 1;
    if (rst == BPT_NEXIST) {
      t = (int)entry.key.ptr;
      gen_stk_push(&kstk, &t);
      ins_cnt++;
    }
#ifdef PRINT_EVERY_INS
    print_bpt(&bstat);
#endif
  }
  print_bpt(&bstat);
  while (!gen_stk_empty(&kstk)) {
    gen_stk_pop(&kstk, &t);
    entry.key.ptr = (void *)t;
    if (rand() < RAND_MAX / 2) {
#ifndef SILENT
      printf("delete: %d\n\n", (int)entry.key.ptr);
#endif
      rst = bpt_delete(entry, cmp_int, bpt_pred_1, &stk, 1, &bstat);
      assert(rst == BPT_PRED_SUCCESS);
#ifdef PRINT_EVERY_DEL
      print_bpt(&bstat);
#endif
      check_bpt(&bstat);
    }
  }

  return 0;
}


