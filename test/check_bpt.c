#include <stdio.h>
#include <stdlib.h>
#include "../b_plus_tree.h"

#define SILENT

int find_minimal_key(struct bpt_node node, int height)
{
  while (height > 0) {
    node.entries = node.entries[0].val.ptr;
    height--;
  }
  return (int)node.entries[0].key.ptr;
}

void check_bpt(struct bpt_stat *bstat)
{
  int m;
  int order = bstat->order;
  struct bpt_node first = bstat->root_node, node, child;
  int prev, cur, i, height = bstat->height, mini;
  int key_cnt;

  while (height >= 0) {
    prev = -1;
    key_cnt = 0;
    for (node = first; node.entries != NULL; node = bpt_node_nxt(node, order)) {
      m = bpt_node_nkey(node, order);
      key_cnt += m;
      for (i = 0; i < m; i++) {
        cur = (int)node.entries[i].key.ptr;
        if (cur <= prev) {
          fprintf(stderr, "not sorted.\n");
          exit(1);
        }
        prev = cur;
        if (height > 0) {
          child.entries = node.entries[i+1].val.ptr;
          mini = find_minimal_key(child, height - 1);
          if (mini != cur) {
            fprintf(stderr, "Internal node mapping wrong: %d %d\n", cur, mini);
            exit(1);
          }
        }
      }
    }
#ifndef SILENT
    printf("Lv.%d key count: %d\n", height, key_cnt);
#endif
    height--;
    first.entries = first.entries[0].val.ptr;
  }
}
