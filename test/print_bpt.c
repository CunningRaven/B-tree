#include <stdio.h>
#include <stdlib.h>
#include "../b_plus_tree.h"

#define PAD_SPACES 6

void _print_bpt(struct bpt_node node, int order, int pad, int height)
{
  int n = pad * PAD_SPACES;
  int m;
  struct bpt_node child;
  if (height < 0)
    return;
  m = bpt_node_nkey(node, order);
  child.entries = node.entries[m].val.ptr;
  _print_bpt(child, order, pad + 1, height - 1);
  for (int i = m - 1; i >= 0; i--) {
    for (int j = 0; j < n; j++)
      putc(' ', stdout);
    printf("%4d\n", (int)node.entries[i].key.ptr);
    child.entries = node.entries[i].val.ptr;
    _print_bpt(child, order, pad + 1, height - 1);
  }
}

void _print_bpt0(struct bpt_node node, int order, int pad, int height)
{
  int n = pad * PAD_SPACES;
  int m;
  struct bpt_node child;
  m = bpt_node_nkey(node, order);
  if (height == 0) {
    for (int i = 0; i < m; i++) {
      for (int j = 0; j < n; j++)
        putc(' ', stdout);
      printf("%4d\n", (int)node.entries[i].key.ptr);
    }
  } else {
    for (int i = 0; i < m; i++) {
      child.entries = node.entries[i].val.ptr;
      _print_bpt0(child, order, pad + 1, height - 1);
      for (int j = 0; j < n; j++)
        putc(' ', stdout);
      printf("%4d\n", (int)node.entries[i].key.ptr);
    }
    child.entries = node.entries[m].val.ptr;
    _print_bpt0(child, order, pad + 1, height - 1);
  }
}

void print_bpt(struct bpt_stat *bstat)
{
  _print_bpt(bstat->root_node, bstat->order, 0, bstat->height);
  printf("\n\n");
}

