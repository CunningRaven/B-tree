#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../b_plus_tree.h"

#define BPT_ORDER 4
// #define SILENT
#define ENTRY_CNT 50
#define SAMPLE_MAX 10000
// #define PRINT_EVERY_INS
#define UPDATE_RANDSEED

void print_bpt(struct bpt_stat *bstat);

int cmp_int(bpt_t a, bpt_t b)
{
  return (int)a.ptr - (int)b.ptr;
}


int main(void)
{
  struct bpt_entry entry;
  struct bpt_stat bstat;
  struct gen_stk stk;
  int i;
  FILE *fp;
#ifndef SILENT
  int cnt = 0;
  int ins_cnt = 0, ins_rst;
#endif
#ifdef UPDATE_RANDSEED
  time_t time_val = time(NULL);

  if ((fp = fopen("random_seed", "w")) == NULL) {
    perror("fopen");
    exit(1);
  }
  srand(time_val);
  fprintf(fp, "Random seed: %lu\n", (unsigned long)time_val);
  fflush(fp);
  fclose(fp);
#else
  srand(9);
#endif
  if (bpt_init(&bstat, BPT_ORDER) == -1)
    return 1;
  if (gen_stk_init(&stk, BPT_STK_CAP_INIT, sizeof (struct bpt_frm)) == -1)
    return 1;
  for (i = 0; i < ENTRY_CNT; i++) {
    entry.key.ptr = (void *)(rand() % SAMPLE_MAX);
    entry.val.ptr = (void *)(rand() % SAMPLE_MAX);
#ifndef SILENT
    printf("No.%d insert %d:%d\n", ++cnt, (int)entry.key.ptr, (int)entry.val.ptr);
#endif
    if ((ins_rst = bpt_insert(entry, cmp_int, bpt_pred_1,
        &stk, 1, &bstat)) == BPT_ERROR)
      return 1;
    if (ins_rst == BPT_NEXIST)
      ins_cnt++;
#ifdef PRINT_EVERY_INS
    print_bpt(&bstat);
#endif
  }

#ifndef SILENT
  printf("\neffective insertions count: %d\n\n", ins_cnt);
#endif
  print_bpt(&bstat);
  return 0;
}


