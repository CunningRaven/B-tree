#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "syscall_fail.h"
#include "b_plus_tree.h"

static void update_index(bpt_t new_key, struct gen_stk *stk);
static int internal_insert(struct bpt_node left_node, struct bpt_node right_node,
    struct gen_stk *stk, struct bpt_stat *bstat);
static int leaf_insert(struct bpt_entry new_entry, int (*cmp)(bpt_t, bpt_t), int (*pred)(bpt_t, bpt_t),
    struct bpt_node leaf, struct gen_stk *stk, struct bpt_stat *bstat);
static int bpt_delete_ientry(struct gen_stk *stk, struct bpt_stat *bstat);

struct bpt_node bpt_null_node = { .entries = NULL };

/**
 * bpt_node_new: allocate a new B+ tree node
 * @order: order of this B+ tree
 * @prv: previous node of this new one
 * @nxt: next node of this new one
 *
 * return bpt_null_node on system call failure
 */
struct bpt_node bpt_node_new(int order, struct bpt_node prv, struct bpt_node nxt)
{
  struct bpt_node new_node;

  if ((new_node.entries = malloc((order + 2) * sizeof(struct bpt_entry))) == NULL)
  {
    syscall_fail("malloc");
    return new_node;
  }
  bpt_node_set_nkey(new_node, order, 0);
  bpt_node_set_prv(new_node, order, prv);
  bpt_node_set_nxt(new_node, order, nxt);

  return new_node;
}

/**
 * bpt_init: allocte a new B+ tree and initialize the struct stating it
 * @bstat: pointer to that struct
 * @order: the order of B+ tree
 *
 * Returns 0 if OK, -1 on system call failure.
 */
int bpt_init(struct bpt_stat *bstat, int order)
{
  bstat->root_node = bpt_node_new(order, bpt_null_node, bpt_null_node);
  if (bstat->root_node.entries == NULL) 
    return -1;
  bstat->order = order;
  bstat->height = 0;

  bstat->old_leaf_nkey = order/2 + 1;
  bstat->new_leaf_nkey = order + 1 - bstat->old_leaf_nkey;
  bstat->old_inter_nkey = order - order / 2;
  bstat->new_inter_nkey = order - bstat->old_inter_nkey;

  return 0;
}

/**
 * bpt_search: search a B+ tree for an entry with specified key.
 * @search_for: the specified key
 * @cmp: pointer to a function comparing two keys. It returns an int greater than, equal to, or less than 0 to indicate
 *       that the first argument is, repectively, larger than, same as, or smaller than the second argument.
 * @bstat: pointer to the struct that states the B+ tree.
 * @leafp: the node containing matched entry will be written to this address if such an entry really exists.
 *
 * Returns either the offset of the matched entry in its node whose value will be assigned to *@leafp, 
 * or -1 to indicate the absence of an entry with the key @search_for.
 */
int bpt_search(bpt_t search_for, int (*cmp)(bpt_t, bpt_t), struct bpt_stat *bstat, struct bpt_node *leafp)
{
  int i, m;
  struct bpt_node node = bstat->root_node;
  int h = bstat->height, order = bstat->order;

  while (h) {
    for (m = bpt_node_nkey(node, order), i = 0; i < m; i++) {
      if (cmp(search_for, node.entries[i].key) < 0)
        break;
    }
    node.entries = node.entries[i].val.ptr;
    h--;
  } 
  for (m = bpt_node_nkey(node, order), i = 0; i < m; i++) {
    if (cmp(search_for, node.entries[i].key) == 0) {
      *leafp = node;
      return i;
    }
  }
  return -1;
}

/**
 * bpt_searchr: search a B+ tree for an entry with specified key and record the traversal journal.
 * @search_for: the specified key
 * @cmp: pointer to a function comparing two keys. It returns an int greater than, equal to, or less than 0 to indicate
 *       that the first argument is, repectively, larger than, same as, or smaller than the second argument.
 * @stk: the stack recording traversal journal.
 * @has_stk_init: if @stk is already initialized.
 * @bstat: pointer to the struct that states the B+ tree.
 * @leafp: the node containing matched entry would be written to this address if such an entry really exists.
 *
 * Returns either the offset of the matched entry in its node whose value will be assigned to *@leafp, 
 * or -1 to indicate a system call failure or the absence of an entry with the key @search_for.
 * @leafp->entries will be either set as NULL if the -1 return value is because of a failed system call,
 * or as the leaf node if that is because of no such a key.
 */
int bpt_searchr(bpt_t search_for, int (*cmp)(bpt_t, bpt_t),
    struct gen_stk *stk, int has_stk_init,
    struct bpt_stat *bstat, struct bpt_node *leafp)
{
  int m;
  struct bpt_frm frm = { .node = bstat->root_node };
  int h = bstat->height, order = bstat->order;

  if (leafp != NULL)
    leafp->entries = NULL;
  if (!has_stk_init) {
    if (gen_stk_init(stk, BPT_STK_CAP_INIT, sizeof (struct bpt_frm)) == -1)
      return -1;
  } else
    stk->cnt = 0;

  while (h) {
    for (m = bpt_node_nkey(frm.node, order), frm.offset = 0; frm.offset < m; frm.offset++) {
      if (cmp(search_for, frm.node.entries[frm.offset].key) < 0)
        break;
    }
    if (gen_stk_push(stk, &frm) == -1)
      return -1;
    frm.node.entries = frm.node.entries[frm.offset].val.ptr;
    h--;
  } 
  if (leafp != NULL)
    *leafp = frm.node;
  for (m = bpt_node_nkey(frm.node, order), frm.offset = 0; frm.offset < m; frm.offset++) {
    if (cmp(search_for, frm.node.entries[frm.offset].key) == 0) 
      return frm.offset;
  }
  return -1;
}

int bpt_pred_1(bpt_t a, bpt_t b)
{
  return 1;
}

int bpt_pred_0(bpt_t a, bpt_t b)
{
  return 0;
}

/*
 * bpt_insert: insert a new entry to a B+ tree.
 * @new_entry: the entry to be inserted.
 * @cmp: pointer to a function comparing two keys. It returns an int greater than, equal to, or less than 0 to indicate
 *       that the first argument is, repectively, larger than, same as, or smaller than the second argument.
 * @pred: substitution will be performed if an existing entry with duplicate key is found as well as
 *           an invocation to @pred, with the first argument as the value of @new_entry and the second as the value of that
 *           existing entry, returns a non-zero.
 * @stk: the stack recording traversal journal.
 * @has_stk_init: if @stk is already initialized.
 * @bstat: pointer to struct stating the B+ tree.
 *           
 * Returns:
 *  BPT_NEXIST if no entry with duplicate key existed and the new entry was inserted successfully;
 *  BPT_PRED_FAIL if there's an entry with duplicate key and the call to @pred returned zero;
 *  BPT_PRED_SUCCESS if there's an entry with duplicate key, the call to @pred returned non-zero,
 *                       and the replacement succeeded;
 *  BPT_ERROR if any system call failure occurs;
 */
int bpt_insert(struct bpt_entry new_entry, int (*cmp)(bpt_t, bpt_t), int (*pred)(bpt_t, bpt_t),
    struct gen_stk *stk, int has_stk_init,
    struct bpt_stat *bstat)
{
  int m;
  int h = bstat->height;
  struct bpt_frm frm = { .node = bstat->root_node };

  if (!has_stk_init) {
    if (gen_stk_init(stk, BPT_STK_CAP_INIT, sizeof (struct bpt_frm)) == -1)
      return -1;
  } else
    stk->cnt = 0;

  while (h) {
    for (m = bpt_node_nkey(frm.node, bstat->order), frm.offset = 0; frm.offset < m; frm.offset++) {
      if (cmp(new_entry.key, frm.node.entries[frm.offset].key) < 0) 
          break;
    }
    if (gen_stk_push(stk, &frm) == -1)
      return -1;
    frm.node.entries = frm.node.entries[frm.offset].val.ptr;
    h--;
  }
  return leaf_insert(new_entry, cmp, pred, frm.node, stk, bstat);
}

static void update_index(bpt_t new_key, struct gen_stk *stk)
{
  struct bpt_frm frm;
  while (!gen_stk_empty(stk)) {
    gen_stk_pop(stk, &frm);
    if (frm.offset != 0) {
      frm.node.entries[frm.offset-1].key = new_key;
      return;
    }
  }
}
      
static int mid_between_prv(struct gen_stk *stk, struct bpt_node *mid_node)
{
  struct bpt_frm frm;
  while (!gen_stk_empty(stk)) {
    gen_stk_pop(stk, &frm);
    if (frm.offset != 0) {
      *mid_node = frm.node;
      return frm.offset - 1;
    }
  }
  return -1;
}

static int mid_between_nxt(struct gen_stk *stk, struct bpt_node *mid_node, int order)
{
  struct bpt_frm frm;
  while (!gen_stk_empty(stk)) {
    gen_stk_pop(stk, &frm);
    if (frm.offset != bpt_node_nkey(frm.node, order)) {
      *mid_node = frm.node;
      return frm.offset;
    }
  }
  return -1;
}

static void mid_between_prv_nxt(struct gen_stk *stk, struct bpt_node *prv_mid_node, struct bpt_node *nxt_mid_node,
    int *prv_mid_offset, int *nxt_mid_offset, int order)
{
  struct bpt_frm frm;
  int found = 0;
  *prv_mid_offset = -1;
  *nxt_mid_offset = -1;
  while (!gen_stk_empty(stk) && found != 0x03) {
    gen_stk_pop(stk, &frm);
    if (!(found & 0x01) && frm.offset != 0) {
      *prv_mid_node = frm.node;
      *prv_mid_offset = frm.offset - 1;
      found |= 0x01;
    }
    if (!(found & 0x02) && frm.offset != bpt_node_nkey(frm.node, order)) {
      *nxt_mid_node = frm.node;
      *nxt_mid_offset = frm.offset;
      found |= 0x02;
    }
  }
}


/**
 * @new_entry: the entry to be inserted.
 * @cmp: pointer to a function comparing two keys. It returns an int greater than, equal to, or less than 0 to indicate
 *       that the first argument is, repectively, larger than, same as, or smaller than the second argument.
 * @pred: substitution will be performed if an existing entry with duplicate key is found as well as
 *           an invocation to @pred, with the first argument as the value of @new_entry and the second as the value of that
 *           existing entry, returns a non-zero.
 * @leaf: the leaf node to which @new_entry will be inserted.
 * @poff: offset in parent node(which entry in parent node is the one pointing to @leaf)
 * @bstat: pointer to struct stating the B+ tree.
 *           
 * returns: identical to bpt_insert().
 */
static int leaf_insert(struct bpt_entry new_entry, int (*cmp)(bpt_t, bpt_t), int (*pred)(bpt_t, bpt_t),
    struct bpt_node leaf, struct gen_stk *stk, struct bpt_stat *bstat)
{
  int offset, order = bstat->order;
  int m, result, i;
  struct bpt_node nxt, prv;

  // insert new entry to leaf node
  for (m = bpt_node_nkey(leaf, order), offset = 0; offset < m; offset++) {
    result = cmp(new_entry.key, leaf.entries[offset].key);
    if (result < 0) 
      break;
    else if (result == 0) {
      if (pred(new_entry.val, leaf.entries[offset].val)) {
        leaf.entries[offset].val = new_entry.val;
        return BPT_PRED_SUCCESS;
      } else 
        return BPT_PRED_FAIL;
    }
  }
  if (m < order) {
    memmove(&leaf.entries[offset+1], &leaf.entries[offset], (m - offset) * sizeof (struct bpt_entry));
    leaf.entries[offset] = new_entry;
    bpt_node_set_nkey(leaf, order, m+1);
  } else { // m == order, leaf node is full
    nxt = bpt_node_nxt(leaf, order);
    prv = bpt_node_prv(leaf, order);

    if (prv.entries != NULL &&
        (i = bpt_node_nkey(prv, order)) != order) { // push the minimum entry to previous leaf node
      prv.entries[i] = leaf.entries[0];
      memmove(&leaf.entries[0], &leaf.entries[1], (offset - 1) * sizeof (struct bpt_entry));
      leaf.entries[offset - 1] = new_entry;
      update_index(leaf.entries[0].key, stk);
      bpt_node_set_nkey(prv, order, i+1);
      return BPT_NEXIST;
    } else if (nxt.entries != NULL &&
        (i = bpt_node_nkey(nxt, order)) != order) { // push the maximum entry to next leaf node
      memmove(&nxt.entries[1], &nxt.entries[0], i * sizeof (struct bpt_entry));
      if (offset == order) {
        nxt.entries[0] = new_entry;
      } else {
        nxt.entries[0] = leaf.entries[order-1];
        memmove(&leaf.entries[offset+1], &leaf.entries[offset], (order-offset-1) * sizeof (struct bpt_entry));
        leaf.entries[offset] = new_entry;
      }
      struct bpt_node mid_node;
      int mid_offset;
      mid_offset = mid_between_nxt(stk, &mid_node, order);
      assert(mid_offset != -1);
      mid_node.entries[mid_offset].key = nxt.entries[0].key;
      bpt_node_set_nkey(nxt, order, i+1);
      return BPT_NEXIST;
    } else { // split this leaf node 
      struct bpt_node new_node;

      new_node = bpt_node_new(order, leaf, nxt);
      if (new_node.entries == NULL) {
#ifndef NDEBUG
        fprintf(stderr, "\nBPT_ERROR 2\n");
#endif
        return BPT_ERROR;
      }
      bpt_node_set_nkey(leaf, order, bstat->old_leaf_nkey);
      bpt_node_set_nkey(new_node, order, bstat->new_leaf_nkey);
      bpt_node_set_nxt(leaf, order, new_node);
      if (nxt.entries != NULL) {
        bpt_node_set_prv(nxt, order, new_node);
      }
      if (offset >= bstat->old_leaf_nkey) {
        int ins_pos;
        ins_pos = offset - bstat->old_leaf_nkey;
        memcpy(&new_node.entries[0], &leaf.entries[bstat->old_leaf_nkey], ins_pos * sizeof (struct bpt_entry));
        new_node.entries[ins_pos] = new_entry;
        memcpy(&new_node.entries[ins_pos+1], &leaf.entries[offset], (order - offset) * sizeof (struct bpt_entry));
        return internal_insert(leaf, new_node, stk, bstat);
      } else {
        memcpy(&new_node.entries[0], &leaf.entries[bstat->old_leaf_nkey-1], bstat->new_leaf_nkey * sizeof (struct bpt_entry));
        memmove(&leaf.entries[offset+1], &leaf.entries[offset], (bstat->old_leaf_nkey - offset - 1) * sizeof (struct bpt_entry));
        leaf.entries[offset] = new_entry;
        return internal_insert(leaf, new_node, stk, bstat);
      }
    }
  }
  return BPT_NEXIST;
}

/**
 * internal_insert: insert an entry to an internal node.
 * @left_node: the node that has been splitted and is adjacently in front of the new node, @right_node.
 * @right_node: the new node made by splitting @left_node.
 * @stk: pointer to a stack recording the traversal journal through the B+ tree.
 * @bstat: pointer to struct stating the B+ tree.
 *           
 * returns: identical to bpt_insert().
 */
static int internal_insert(struct bpt_node left_node, struct bpt_node right_node,
    struct gen_stk *stk, struct bpt_stat *bstat)
{
  int order;
  bpt_t mid;
  struct bpt_frm frm;

  order = bstat->order;
  mid = right_node.entries[0].key;

  while (1) {
    if (gen_stk_empty(stk)) { // root node has been splitted
      struct bpt_node new_root;
      new_root = bpt_node_new(bstat->order, bpt_null_node, bpt_null_node);
      if (new_root.entries == NULL) {
#ifndef NDEBUG
        fprintf(stderr, "\nBPT_ERROR 3\n");
#endif
        return BPT_ERROR;
      }
      bpt_node_set_nkey(new_root, order, 1);
      new_root.entries[0].key = mid;
      new_root.entries[0].val.ptr = left_node.entries;
      new_root.entries[1].val.ptr = right_node.entries;
      bstat->root_node = new_root;
      bstat->height++;
      break;
    } else {
      int m;

      gen_stk_pop(stk, &frm);
      m = bpt_node_nkey(frm.node, order);
      if (m < order) {
        memmove(&frm.node.entries[frm.offset+1], &frm.node.entries[frm.offset], (m + 1 - frm.offset) * sizeof (struct bpt_entry));
        frm.node.entries[frm.offset].key = mid;
        frm.node.entries[frm.offset+1].val.ptr = right_node.entries;
        bpt_node_set_nkey(frm.node, order, m+1);
        break;
      } else { // split internal node
        bpt_t new_mid;
        struct bpt_node new_node, nxt;

        nxt = bpt_node_nxt(frm.node, order);
        new_node = bpt_node_new(order, frm.node, nxt);
        if (new_node.entries == NULL) {
#ifndef NDEBUG
          fprintf(stderr, "\nBPT_ERROR 4\n");
#endif
          return BPT_ERROR;
        }
        bpt_node_set_nxt(frm.node, order, new_node);
        if (nxt.entries != NULL) {
          bpt_node_set_prv(nxt, order, new_node);
        }

        if (frm.offset < bstat->old_inter_nkey) {
          new_mid = frm.node.entries[bstat->old_inter_nkey-1].key;
          memcpy(&new_node.entries[0], &frm.node.entries[bstat->old_inter_nkey], (bstat->new_inter_nkey + 1) * sizeof (struct bpt_entry));
          memmove(&frm.node.entries[frm.offset+1], &frm.node.entries[frm.offset], 
              (bstat->old_inter_nkey - frm.offset) * sizeof (struct bpt_entry));
          frm.node.entries[frm.offset].key = mid;
          frm.node.entries[frm.offset+1].val.ptr = right_node.entries;
        } else if (frm.offset > bstat->old_inter_nkey) {
          int front;
          new_mid = frm.node.entries[bstat->old_inter_nkey].key;
          front = frm.offset - bstat->old_inter_nkey;
          memcpy(&new_node.entries[0], &frm.node.entries[bstat->old_inter_nkey+1], front * sizeof (struct bpt_entry));
          memcpy(&new_node.entries[front], &frm.node.entries[frm.offset], (bstat->new_inter_nkey + 1 - front) * sizeof (struct bpt_entry));
          new_node.entries[front-1].key = mid;
          new_node.entries[front].val.ptr = right_node.entries;
        } else { // if (frm.offset + 1 == bstat->old_inter_nkey)
          new_mid = mid;
          memcpy(&new_node.entries[0], &frm.node.entries[bstat->old_inter_nkey], (bstat->new_inter_nkey + 1) * sizeof (struct bpt_entry));
          new_node.entries[0].val.ptr = right_node.entries;
        }

        bpt_node_set_nkey(frm.node, order, bstat->old_inter_nkey);
        bpt_node_set_nkey(new_node, order, bstat->new_inter_nkey);
        left_node = frm.node;
        right_node = new_node;
        mid = new_mid;
      }
    }
  }
  return BPT_NEXIST;
}

/*
 * bpt_delete: delete an entry with specified key from a B+ tree.
 * @pair: a key-value pair whose key is what the function searchs for. Its value part works with the argument @pred shown below.
 * @cmp: pointer to a function comparing two keys. It returns an int greater than, equal to, or less than 0 to indicate
 *       that the first argument is, repectively, larger than, same as, or smaller than the second argument.
 * @pred: deletion will be performed if an existing entry with duplicate key is found as well as
 *           an invocation to @pred, with the first argument as the value of @pair and the second as the value of that
 *           existing entry, returns a non-zero.
 * @stk: the stack recording traversal journal.
 * @has_stk_init: if @stk is already initialized.
 * @bstat: pointer to struct stating the B+ tree.
 *           
 * Returns:
 *  BPT_NEXIST if no entry with duplicate key existed.
 *  BPT_PRED_FAIL if there's an entry with duplicate key and the call to @pred returned zero;
 *  BPT_PRED_SUCCESS if there's an entry with duplicate key, the call to @pred returned non-zero,
 *                       and the deletion succeeded;
 *  BPT_ERROR if any system call failure occurs;
 */
int bpt_delete(struct bpt_entry pair, int (*cmp)(bpt_t, bpt_t), int (*pred)(bpt_t, bpt_t),
    struct gen_stk *stk, int has_stk_init,
    struct bpt_stat *bstat)
{
  struct bpt_node leaf;
  int offset;

  if ((offset = bpt_searchr(pair.key, cmp, stk, has_stk_init, bstat, &leaf)) == -1) {
    if (leaf.entries == NULL)
      return BPT_ERROR;
    else
      return BPT_NEXIST;
  }
  if (pred(pair.val, leaf.entries[offset].val))
    return bpt_delete_entry(leaf, offset, stk, bstat);
  else
    return BPT_PRED_FAIL;
}

int bpt_delete_entry(struct bpt_node leaf, int offset, struct gen_stk *stk, struct bpt_stat *bstat)
{
  int order = bstat->order;
  int m = bpt_node_nkey(leaf, order), minimal_leaf_nkey = bstat->new_leaf_nkey;
  if (m != minimal_leaf_nkey || gen_stk_empty(stk)) {
    memmove(&leaf.entries[offset], &leaf.entries[offset+1], (m - offset - 1) * sizeof (struct bpt_entry));
    if (offset == 0)
      update_index(leaf.entries[0].key, stk);
    bpt_node_set_nkey(leaf, order, m - 1);
    return BPT_PRED_SUCCESS;
  } else { // m == minimal_leaf_nkey && frm.node is not root node
    struct bpt_node prv = bpt_node_prv(leaf, order),
                    nxt = bpt_node_nxt(leaf, order);
    int prv_nkey, nxt_nkey, sum;
    int post_sz = minimal_leaf_nkey - 1 - offset;
    if (prv.entries != NULL && (prv_nkey = bpt_node_nkey(prv, order)) != minimal_leaf_nkey) {
      // grab some entries from prv to pad current node
      sum = (minimal_leaf_nkey-1) + prv_nkey;
      int right_nkey = sum / 2, left_nkey = sum - right_nkey;
      int grab = right_nkey - (minimal_leaf_nkey-1);
      memmove(&leaf.entries[grab+offset], &leaf.entries[offset+1], post_sz * sizeof (struct bpt_entry));
      memmove(&leaf.entries[grab], &leaf.entries[0], offset * sizeof (struct bpt_entry));
      memcpy(&leaf.entries[0], &prv.entries[left_nkey], grab * sizeof (struct bpt_entry));
      bpt_node_set_nkey(prv, order, left_nkey);
      bpt_node_set_nkey(leaf, order, right_nkey);
      update_index(leaf.entries[0].key, stk);
      return BPT_PRED_SUCCESS;
    } else if (nxt.entries != NULL && (nxt_nkey = bpt_node_nkey(nxt, order)) != minimal_leaf_nkey) {
      // grab some entries from nxt to pad current node
      sum = (minimal_leaf_nkey-1) + nxt_nkey;
      int left_nkey = sum / 2, right_nkey = sum - left_nkey;
      int grab = left_nkey - (minimal_leaf_nkey-1);
      memmove(&leaf.entries[offset], &leaf.entries[offset+1], post_sz * sizeof (struct bpt_entry));
      memcpy(&leaf.entries[(minimal_leaf_nkey-1)], &nxt.entries[0], grab * sizeof (struct bpt_entry));
      memmove(&nxt.entries[0], &nxt.entries[grab], right_nkey * sizeof (struct bpt_entry));
      bpt_node_set_nkey(leaf, order, left_nkey);
      bpt_node_set_nkey(nxt, order, right_nkey);
      struct bpt_node mid_node;
      int mid_offset;
      if (offset != 0 || prv.entries == NULL) {
        mid_offset = mid_between_nxt(stk, &mid_node, order);
        mid_node.entries[mid_offset].key = nxt.entries[0].key;
      } else {
        struct bpt_node prv_mid_node;
        int prv_mid_offset;
        mid_between_prv_nxt(stk, &prv_mid_node, &mid_node, &prv_mid_offset, &mid_offset, order);
        prv_mid_node.entries[prv_mid_offset].key = leaf.entries[0].key;
        mid_node.entries[mid_offset].key = nxt.entries[0].key;
      }
      return BPT_PRED_SUCCESS;
    } else { // Neither side exists enough entries, try to merge them
      if (prv.entries != NULL) { 
        // merge to previous
        memcpy(&prv.entries[minimal_leaf_nkey], &leaf.entries[0], offset * sizeof (struct bpt_entry));
        memcpy(&prv.entries[minimal_leaf_nkey+offset], &leaf.entries[offset+1], post_sz * sizeof (struct bpt_entry));
        bpt_node_set_nkey(prv, order, minimal_leaf_nkey + minimal_leaf_nkey - 1);
        bpt_node_set_nxt(prv, order, nxt);
        if (nxt.entries != NULL)
          bpt_node_set_prv(nxt, order, prv);
      } else { // nxt.entries != NULL
        // merge to next
        memmove(&nxt.entries[minimal_leaf_nkey-1], &nxt.entries[0], minimal_leaf_nkey * sizeof (struct bpt_entry));
        memcpy(&nxt.entries[0], &leaf.entries[0], offset * sizeof (struct bpt_entry));
        memcpy(&nxt.entries[offset], &leaf.entries[offset+1], post_sz * sizeof (struct bpt_entry));
        {
          struct bpt_node mid_node;
          int mid_offset;
          assert(!gen_stk_empty(stk));
          struct gen_stk tmpstk;
          if (gen_stk_copy(&tmpstk, stk) == -1)
            return -1;
          mid_offset = mid_between_nxt(&tmpstk, &mid_node, order);
          assert(mid_offset != -1);
          mid_node.entries[mid_offset].key = nxt.entries[0].key;
          gen_stk_delete(&tmpstk);
        }
        bpt_node_set_nkey(nxt, order, minimal_leaf_nkey + minimal_leaf_nkey - 1);
        bpt_node_set_prv(nxt, order, prv);
        if (prv.entries != NULL)
          bpt_node_set_nxt(prv, order, nxt);
      }
      bpt_node_delete(leaf);
      return bpt_delete_ientry(stk, bstat);
    }
  }
}

static int bpt_delete_ientry(struct gen_stk *stk, struct bpt_stat *bstat)
{
  struct bpt_frm frm;
  int m, order = bstat->order, minimal_inter_nkey = bstat->new_inter_nkey;
  struct bpt_node mid_node;
  int mid_offset;
  int left_nkey, right_nkey;
  int sum;
  while (1) {
    gen_stk_pop(stk, &frm);
    m = bpt_node_nkey(frm.node, order);
    if (gen_stk_empty(stk)) { // current node is root node
      if (m == 1) {
        if (frm.offset == 0)
          bstat->root_node.entries = frm.node.entries[1].val.ptr;
        else
          bstat->root_node.entries = frm.node.entries[0].val.ptr;
        bstat->height--;
        bpt_node_delete(frm.node);
      } else {
        if (frm.offset != 0) {
          bpt_t saved_val = frm.node.entries[frm.offset-1].val;
          memmove(&frm.node.entries[frm.offset-1], &frm.node.entries[frm.offset],
              (m + 1 - frm.offset) * sizeof (struct bpt_entry));
          frm.node.entries[frm.offset-1].val = saved_val;
        } else { // frm.offset == 0
          memmove(&frm.node.entries[0], &frm.node.entries[1], m * sizeof (struct bpt_entry));
        }
        bpt_node_set_nkey(frm.node, order, m - 1);
      }
      return BPT_PRED_SUCCESS;
    } else if (m > minimal_inter_nkey) {
      if (frm.offset != 0) {
        bpt_t saved_val = frm.node.entries[frm.offset-1].val;
        memmove(&frm.node.entries[frm.offset-1], &frm.node.entries[frm.offset],
            (m + 1 - frm.offset) * sizeof (struct bpt_entry));
        frm.node.entries[frm.offset-1].val = saved_val;
      } else { // frm.offset == 0
        bpt_t nxt_key = frm.node.entries[0].key;
        memmove(&frm.node.entries[0], &frm.node.entries[1], m * sizeof (struct bpt_entry));
        mid_offset = mid_between_prv(stk, &mid_node);
        if (mid_offset != -1) {
          mid_node.entries[mid_offset].key = nxt_key;
        }
      }
      bpt_node_set_nkey(frm.node, order, m - 1);
      return BPT_PRED_SUCCESS;
    } else { // m == minimal_inter_nkey
      struct bpt_node prv = bpt_node_prv(frm.node, order),
                      nxt = bpt_node_nxt(frm.node, order);
      int prv_nkey, nxt_nkey;
      int grab;
      if (prv.entries != NULL && (prv_nkey = bpt_node_nkey(prv, order)) != minimal_inter_nkey) {
        // grab some entries from prv to pad current node
        mid_offset = mid_between_prv(stk, &mid_node);
        assert(mid_offset!=-1);
        sum = prv_nkey + (minimal_inter_nkey - 1);
        right_nkey = sum / 2;
        left_nkey = sum - right_nkey;
        grab = (right_nkey + 1) - minimal_inter_nkey;
        bpt_t saved_key = frm.node.entries[frm.offset].key;
        memmove(&frm.node.entries[grab+frm.offset], &frm.node.entries[1+frm.offset], (minimal_inter_nkey - frm.offset) * sizeof (struct bpt_entry));
        memmove(&frm.node.entries[grab], &frm.node.entries[0], frm.offset * sizeof (struct bpt_entry));
        memcpy(&frm.node.entries[0], &prv.entries[left_nkey+1], grab * sizeof(struct bpt_entry));
        frm.node.entries[grab-1].key = mid_node.entries[mid_offset].key;
        frm.node.entries[grab+frm.offset-1].key = saved_key;
        mid_node.entries[mid_offset].key = prv.entries[left_nkey].key;
        bpt_node_set_nkey(prv, order, left_nkey);
        bpt_node_set_nkey(frm.node, order, right_nkey);
        return BPT_PRED_SUCCESS;
      } else if (nxt.entries != NULL && (nxt_nkey = bpt_node_nkey(nxt, order)) != minimal_inter_nkey) {
        // grab some entries from nxt to pad current node
        sum = nxt_nkey + (minimal_inter_nkey - 1);
        left_nkey = sum / 2;
        right_nkey = sum - left_nkey;
        grab = left_nkey + 1 - minimal_inter_nkey;
        if (frm.offset != 0) {
          mid_offset = mid_between_nxt(stk, &mid_node, order);
          assert(mid_offset != -1);
          bpt_t saved_val = frm.node.entries[frm.offset-1].val;
          memmove(&frm.node.entries[frm.offset-1], &frm.node.entries[frm.offset], (minimal_inter_nkey + 1 - frm.offset) * sizeof (struct bpt_entry));
          frm.node.entries[frm.offset-1].val = saved_val;
        } else {
          struct bpt_node prv_mid_node;
          int prv_mid_offset;
          mid_between_prv_nxt(stk, &prv_mid_node, &mid_node, &prv_mid_offset, &mid_offset, order);
          assert(mid_offset != -1);
          if (prv_mid_offset != -1) {
            prv_mid_node.entries[prv_mid_offset].key = frm.node.entries[0].key;
          }
          memmove(&frm.node.entries[0], &frm.node.entries[1], minimal_inter_nkey * sizeof (struct bpt_entry));
        }
        memcpy(&frm.node.entries[minimal_inter_nkey], &nxt.entries[0], grab * sizeof (struct bpt_entry));
        frm.node.entries[minimal_inter_nkey-1].key = mid_node.entries[mid_offset].key;
        mid_node.entries[mid_offset].key = nxt.entries[grab-1].key;
        memmove(&nxt.entries[0], &nxt.entries[grab], (right_nkey + 1) * sizeof (struct bpt_entry));
        bpt_node_set_nkey(frm.node, order, left_nkey);
        bpt_node_set_nkey(nxt, order, right_nkey);
        return BPT_PRED_SUCCESS;
      } else { // merge with an adjacent node
        struct bpt_frm parent;
        gen_stk_pop(stk, &parent); // take a peep
        if (parent.offset != 0) { // merge to previous node
          bpt_t saved_key = frm.node.entries[frm.offset].key;
          memcpy(&prv.entries[minimal_inter_nkey+1], &frm.node.entries[0], frm.offset * sizeof (struct bpt_entry));
          memcpy(&prv.entries[minimal_inter_nkey+1+frm.offset], &frm.node.entries[frm.offset+1], (minimal_inter_nkey - frm.offset) * sizeof (struct bpt_entry));
          prv.entries[minimal_inter_nkey].key = parent.node.entries[parent.offset-1].key;
          // prv.entries[minimal_inter_nkey+1+frm.offset-1].key = saved_key;
          prv.entries[minimal_inter_nkey+frm.offset].key = saved_key;
          bpt_node_set_nkey(prv, order, minimal_inter_nkey + minimal_inter_nkey);
          bpt_node_set_nxt(prv, order, nxt);
          if (nxt.entries != NULL)
            bpt_node_set_prv(nxt, order, prv);
          bpt_node_delete(frm.node);
          gen_stk_push(stk, &parent);
        } else { // merge next node to current
          if (frm.offset != 0)
            frm.node.entries[frm.offset-1].key = frm.node.entries[frm.offset].key;
          else if (prv.entries != NULL) {
            assert(!gen_stk_empty(stk));
            struct gen_stk tmpstk;
            if (gen_stk_copy(&tmpstk, stk) == -1)
              return -1;
            mid_offset = mid_between_prv(&tmpstk, &mid_node);
            assert(mid_offset != -1);
            mid_node.entries[mid_offset].key = frm.node.entries[0].key;
            gen_stk_delete(&tmpstk);
          }
          memmove(&frm.node.entries[frm.offset], &frm.node.entries[frm.offset+1], (minimal_inter_nkey - frm.offset) * sizeof (struct bpt_entry));
          memcpy(&frm.node.entries[minimal_inter_nkey], &nxt.entries[0], (minimal_inter_nkey + 1) * sizeof (struct bpt_entry));
          frm.node.entries[minimal_inter_nkey-1].key = parent.node.entries[parent.offset].key;
          bpt_node_set_nkey(frm.node, order, minimal_inter_nkey + minimal_inter_nkey);
          struct bpt_node nxt_nxt = bpt_node_nxt(nxt, order);
          bpt_node_set_nxt(frm.node, order, nxt_nxt);
          if (nxt_nxt.entries != NULL)
            bpt_node_set_prv(nxt_nxt, order, frm.node);
          bpt_node_delete(nxt);
          parent.offset++;
          gen_stk_push(stk, &parent);
        }
      }
    }
  }
}


          

