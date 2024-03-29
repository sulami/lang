/*
 * Copyright (c) 2013-2014 Jean Niklas L'orange. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define RRB_MALLOC malloc
#define RRB_REALLOC realloc
#define RRB_MALLOC_ATOMIC malloc

#define RRB_BITS 5
#define RRB_MAX_HEIGHT 32

#define RRB_BRANCHING (1 << RRB_BITS)
#define RRB_MASK (RRB_BRANCHING - 1)

#define RRB_INVARIANT 1
#define RRB_EXTRAS 2

typedef struct RRB_ RRB;

typedef pthread_t RRBThread;

#define RRB_THREAD_ID pthread_self
#define RRB_THREAD_EQUALS(a, b) pthread_equal(a, b)

const RRB* rrb_create(void);

uint32_t rrb_count(const RRB *rrb);
void* rrb_nth(const RRB *rrb, uint32_t index);
const RRB* rrb_pop(const RRB *rrb);
void* rrb_peek(const RRB *rrb);
const RRB* rrb_push(const RRB *restrict rrb, const void *restrict elt);
const RRB* rrb_update(const RRB *restrict rrb, uint32_t index, const void *restrict elt);

const RRB* rrb_concat(const RRB *left, const RRB *right);
const RRB* rrb_slice(const RRB *rrb, uint32_t from, uint32_t to);

// Transients

typedef struct TransientRRB_ TransientRRB;

TransientRRB* rrb_to_transient(const RRB *rrb);
const RRB* transient_to_rrb(TransientRRB *trrb);

uint32_t transient_rrb_count(const TransientRRB *trrb);
void* transient_rrb_nth(const TransientRRB *trrb, uint32_t index);
TransientRRB* transient_rrb_pop(TransientRRB *trrb);
void* transient_rrb_peek(const TransientRRB *trrb);
TransientRRB* transient_rrb_push(TransientRRB *restrict trrb, const void *restrict elt);
TransientRRB* transient_rrb_update(TransientRRB *restrict trrb, uint32_t index, const void *restrict elt);
TransientRRB* transient_rrb_slice(TransientRRB *trrb, uint32_t from, uint32_t to);

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

// Typical stuff
#define RRB_SHIFT(rrb) (rrb->shift)
#define INC_SHIFT(shift) (shift + (uint32_t) RRB_BITS)
#define DEC_SHIFT(shift) (shift - (uint32_t) RRB_BITS)
#define LEAF_NODE_SHIFT ((uint32_t) 0)

// Abusing allocated pointers being unique to create GUIDs: using a single
// malloc to create a guid.
#define GUID_DECLARATION const void *guid;

typedef enum {LEAF_NODE, INTERNAL_NODE} NodeType;

typedef struct TreeNode {
  NodeType type;
  uint32_t len;
  GUID_DECLARATION
} TreeNode;

typedef struct LeafNode {
  NodeType type;
  uint32_t len;
  GUID_DECLARATION
  const void *child[];
} LeafNode;

typedef struct RRBSizeTable {
  GUID_DECLARATION
  uint32_t size[];
} RRBSizeTable;

typedef struct InternalNode {
  NodeType type;
  uint32_t len;
  GUID_DECLARATION
  RRBSizeTable *size_table;
  struct InternalNode *child[];
} InternalNode;

struct RRB_ {
  uint32_t cnt;
  uint32_t shift;
  uint32_t tail_len;
  LeafNode *tail;
  TreeNode *root;
};

static LeafNode EMPTY_LEAF = {.type = LEAF_NODE, .len = 0};
static const RRB EMPTY_RRB = {.cnt = 0, .shift = 0, .root = NULL,
                              .tail_len = 0, .tail = &EMPTY_LEAF};

static RRBSizeTable* size_table_create(uint32_t len);
static RRBSizeTable* size_table_clone(const RRBSizeTable* original, uint32_t len);
static RRBSizeTable* size_table_inc(const RRBSizeTable *original, uint32_t len);

static InternalNode* concat_sub_tree(TreeNode *left_node, uint32_t left_shift,
                                     TreeNode *right_node, uint32_t right_shift,
                                     char is_top);
static InternalNode* rebalance(InternalNode *left, InternalNode *centre,
                               InternalNode *right, uint32_t shift,
                               char is_top);
static uint32_t* create_concat_plan(InternalNode *all, uint32_t *top_len);
static InternalNode* execute_concat_plan(InternalNode *all, uint32_t *node_sizes,
                                         uint32_t slen, uint32_t shift);
static uint32_t find_shift(TreeNode *node);
static InternalNode* set_sizes(InternalNode *node, uint32_t shift);
static uint32_t size_sub_trie(TreeNode *node, uint32_t parent_shift);
static uint32_t sized_pos(const InternalNode *node, uint32_t *index,
                          uint32_t sp);
static const InternalNode* sized(const InternalNode *node, uint32_t *index,
                                 uint32_t sp);

static LeafNode* leaf_node_clone(const LeafNode *original);
static LeafNode* leaf_node_inc(const LeafNode *original);
static LeafNode* leaf_node_dec(const LeafNode *original);
static LeafNode* leaf_node_create(uint32_t size);
static LeafNode* leaf_node_merge(LeafNode *left_leaf, LeafNode *right_leaf);

static InternalNode* internal_node_create(uint32_t len);
static InternalNode* internal_node_clone(const InternalNode *original);
static InternalNode* internal_node_inc(const InternalNode *original);
static InternalNode* internal_node_dec(const InternalNode *original);
static InternalNode* internal_node_merge(InternalNode *left, InternalNode *centre,
                                         InternalNode *right);
static InternalNode* internal_node_copy(InternalNode *original, uint32_t start,
                                        uint32_t len);
static InternalNode* internal_node_new_above1(InternalNode *child);
static InternalNode* internal_node_new_above(InternalNode *left, InternalNode *right);

static RRB* slice_right(const RRB *rrb, const uint32_t right);
static TreeNode* slice_right_rec(uint32_t *total_shift, const TreeNode *root,
                                  uint32_t right, uint32_t shift,
                                  char has_left);
static const RRB* slice_left(RRB *rrb, uint32_t left);
static TreeNode* slice_left_rec(uint32_t *total_shift, const TreeNode *root,
                                uint32_t left, uint32_t shift,
                                char has_right);

static RRB* rrb_head_clone(const RRB *original);

static RRB* push_down_tail(const RRB *restrict rrb, RRB *restrict new_rrb,
                           const LeafNode *restrict new_tail);
static void promote_rightmost_leaf(RRB *new_rrb);



static RRBSizeTable* size_table_create(uint32_t size) {
  RRBSizeTable *table = RRB_MALLOC(sizeof(RRBSizeTable)
                                  + size * sizeof(uint32_t));
  return table;
}

static RRBSizeTable* size_table_clone(const RRBSizeTable *original,
                                      uint32_t len) {
  RRBSizeTable *clone = RRB_MALLOC(sizeof(RRBSizeTable)
                                   + len * sizeof(uint32_t));
  memcpy(&clone->size, &original->size, sizeof(uint32_t) * len);
  return clone;
}

static inline RRBSizeTable* size_table_inc(const RRBSizeTable *original,
                                           uint32_t len) {
  RRBSizeTable *incr = RRB_MALLOC(sizeof(RRBSizeTable) +
                                  (len + 1) * sizeof(uint32_t));
  memcpy(&incr->size, &original->size, sizeof(uint32_t) * len);
  return incr;
}

static RRB* rrb_head_clone(const RRB* original) {
  RRB *clone = RRB_MALLOC(sizeof(RRB));
  memcpy(clone, original, sizeof(RRB));
  return clone;
}

const RRB* rrb_create() {
  return &EMPTY_RRB;
}

static RRB* rrb_mutable_create() {
  RRB *rrb = RRB_MALLOC(sizeof(RRB));
  return rrb;
}

const RRB* rrb_concat(const RRB *left, const RRB *right) {
  if (left->cnt == 0) {
    return right;
  }
  else if (right->cnt == 0) {
    return left;
  }
  else {
    if (right->root == NULL) {
      // merge left and right tail, if possible
      RRB *new_rrb = rrb_head_clone(left);
      new_rrb->cnt += right->cnt;

      // skip merging if left tail is full.
      if (left->tail_len == RRB_BRANCHING) {
        new_rrb->tail_len = right->tail_len;
        return push_down_tail(left, new_rrb, right->tail);
      }
      // We can merge both tails into a single tail.
      else if (left->tail_len + right->tail_len <= RRB_BRANCHING) {
        const uint32_t new_tail_len = left->tail_len + right->tail_len;
        LeafNode *new_tail = leaf_node_merge(left->tail, right->tail);
        new_rrb->tail = new_tail;
        new_rrb->tail_len = new_tail_len;
        return new_rrb;
      }
      else { // must push down something, and will have elements remaining in
             // the right tail
        LeafNode *push_down = leaf_node_create(RRB_BRANCHING);
        memcpy(&push_down->child[0], &left->tail->child[0],
               left->tail_len * sizeof(void *));
        const uint32_t right_cut = RRB_BRANCHING - left->tail_len;
        memcpy(&push_down->child[left->tail_len], &right->tail->child[0],
               right_cut * sizeof(void *));

        // this will be strictly positive.
        const uint32_t new_tail_len = right->tail_len - right_cut;
        LeafNode *new_tail = leaf_node_create(new_tail_len);

        memcpy(&new_tail->child[0], &right->tail->child[right_cut],
               new_tail_len * sizeof(void *));

        new_rrb->tail = push_down;
        new_rrb->tail_len = new_tail_len;

        // This is an imitation, so that push_down_tail works as we intend it
        // to: Whenever the height has to be increased, it calculates the size
        // table based upon the old rrb's size, minus the old tail. However,
        // since we manipulate the old tail to be longer than it actually was,
        // we have to reflect those changes in the cnt variable.
        RRB left_imitation;
        memcpy(&left_imitation, left, sizeof(RRB));
        left_imitation.cnt = new_rrb->cnt - new_tail_len;

        return push_down_tail(&left_imitation, new_rrb, new_tail);
      }
    }
    left = push_down_tail(left, rrb_head_clone(left), NULL);
    RRB *new_rrb = rrb_mutable_create();
    new_rrb->cnt = left->cnt + right->cnt;

    InternalNode *root_candidate = concat_sub_tree(left->root, RRB_SHIFT(left),
                                                   right->root, RRB_SHIFT(right),
                                                   true);

    new_rrb->shift = find_shift((TreeNode *) root_candidate);
    // must be done before we set sizes.
    new_rrb->root = (TreeNode *) set_sizes(root_candidate,
                                           RRB_SHIFT(new_rrb));
    new_rrb->tail = right->tail;
    new_rrb->tail_len = right->tail_len;
    return new_rrb;
  }
}

static InternalNode* concat_sub_tree(TreeNode *left_node, uint32_t left_shift,
                                     TreeNode *right_node, uint32_t right_shift,
                                     char is_top) {
  if (left_shift > right_shift) {
    // Left tree is higher than right tree
    InternalNode *left_internal = (InternalNode *) left_node;
    InternalNode *centre_node =
      concat_sub_tree((TreeNode *) left_internal->child[left_internal->len - 1],
                      DEC_SHIFT(left_shift),
                      right_node, right_shift,
                      false);
    return rebalance(left_internal, centre_node, NULL, left_shift, is_top);
  }
  else if (left_shift < right_shift) {
    InternalNode *right_internal = (InternalNode *) right_node;
    InternalNode *centre_node =
      concat_sub_tree(left_node, left_shift,
                      (TreeNode *) right_internal->child[0],
                      DEC_SHIFT(right_shift),
                      false);
    return rebalance(NULL, centre_node, right_internal, right_shift, is_top);
  }
  else { // we have same height
    if (left_shift == LEAF_NODE_SHIFT) { // We're dealing with leaf nodes
      LeafNode *left_leaf = (LeafNode *) left_node;
      LeafNode *right_leaf = (LeafNode *) right_node;
      // We don't do this if we're not at top, as we'd have to zip stuff above
      // as well.
      if (is_top && (left_leaf->len + right_leaf->len) <= RRB_BRANCHING) {
        // Can put them in a single node
        LeafNode *merged = leaf_node_merge(left_leaf, right_leaf);
        return internal_node_new_above1((InternalNode *) merged);
      }
      else {
        InternalNode *left_internal = (InternalNode *) left_node;
        InternalNode *right_internal = (InternalNode *) right_node;
        return internal_node_new_above(left_internal, right_internal);
      }
    }

    else { // two internal nodes with same height. Move both down
      InternalNode *left_internal = (InternalNode *) left_node;
      InternalNode *right_internal = (InternalNode *) right_node;
      InternalNode *centre_node =
        concat_sub_tree((TreeNode *) left_internal->child[left_internal->len - 1],
                        DEC_SHIFT(left_shift),
                        (TreeNode *) right_internal->child[0],
                        DEC_SHIFT(right_shift),
                        false);
      // can be optimised: since left_shift == right_shift, we'll end up in this
      // block again.
      return rebalance(left_internal, centre_node, right_internal, left_shift,
                       is_top);
    }
  }
}

static LeafNode* leaf_node_clone(const LeafNode *original) {
  size_t size = sizeof(LeafNode) + original->len * sizeof(void *);
  LeafNode *clone = RRB_MALLOC(size);
  memcpy(clone, original, size);
  return clone;
}

static LeafNode* leaf_node_inc(const LeafNode *original) {
  size_t size = sizeof(LeafNode) + original->len * sizeof(void *);
  LeafNode *inc = RRB_MALLOC(size + sizeof(void *));
  memcpy(inc, original, size);
  inc->len++;
  return inc;
}

static LeafNode* leaf_node_dec(const LeafNode *original) {
  size_t size = sizeof(LeafNode) + (original->len - 1) * sizeof(void *);
  LeafNode *dec = RRB_MALLOC(size); // assumes size > 1
  memcpy(dec, original, size);
  dec->len--;
  return dec;
}


static LeafNode* leaf_node_create(uint32_t len) {
  LeafNode *node = RRB_MALLOC(sizeof(LeafNode) + len * sizeof(void *));
  node->type = LEAF_NODE;
  node->len = len;
  return node;
}

static LeafNode* leaf_node_merge(LeafNode *left, LeafNode *right) {
  LeafNode *merged = leaf_node_create(left->len + right->len);

  memcpy(&merged->child[0], left->child, left->len * sizeof(void *));
  memcpy(&merged->child[left->len], right->child, right->len * sizeof(void *));
  return merged;
}

static InternalNode* internal_node_create(uint32_t len) {
  InternalNode *node = RRB_MALLOC(sizeof(InternalNode)
                              + len * sizeof(InternalNode *));
  node->type = INTERNAL_NODE;
  node->len = len;
  node->size_table = NULL;
  return node;
}

static InternalNode* internal_node_new_above1(InternalNode *child) {
  InternalNode *above = internal_node_create(1);
  above->child[0] = child;
  return above;
}

static InternalNode* internal_node_new_above(InternalNode *left, InternalNode *right) {
  InternalNode *above = internal_node_create(2);
  above->child[0] = left;
  above->child[1] = right;
  return above;
}

static InternalNode* internal_node_merge(InternalNode *left, InternalNode *centre,
                                         InternalNode *right) {
  // If internal node is NULL, its size is zero.
  uint32_t left_len = (left == NULL) ? 0 : left->len - 1;
  uint32_t centre_len = (centre == NULL) ? 0 : centre->len;
  uint32_t right_len = (right == NULL) ? 0 : right->len - 1;

  InternalNode *merged = internal_node_create(left_len + centre_len + right_len);
  if (left_len != 0) { // memcpy'ing zero elements from/to NULL is undefined.
    memcpy(&merged->child[0], left->child,
           left_len * sizeof(InternalNode *));
  }
  if (centre_len != 0) { // same goes here
    memcpy(&merged->child[left_len], centre->child,
           centre_len * sizeof(InternalNode *));
  }
  if (right_len != 0) { // and here
    memcpy(&merged->child[left_len + centre_len], &right->child[1],
           right_len * sizeof(InternalNode *));
  }

  return merged;
}

static InternalNode* internal_node_clone(const InternalNode *original) {
  size_t size = sizeof(InternalNode) + original->len * sizeof(InternalNode *);
  InternalNode *clone = RRB_MALLOC(size);
  memcpy(clone, original, size);
  return clone;
}

static InternalNode* internal_node_copy(InternalNode *original, uint32_t start,
                                        uint32_t len){
  InternalNode *copy = internal_node_create(len);
  memcpy(copy->child, &original->child[start], len * sizeof(InternalNode *));
  return copy;
}

static InternalNode* internal_node_inc(const InternalNode *original) {
  size_t size = sizeof(InternalNode) + original->len * sizeof(InternalNode *);
  InternalNode *incr = RRB_MALLOC(size + sizeof(InternalNode *));
  memcpy(incr, original, size);
  // update length
  if (incr->size_table != NULL) {
    incr->size_table = size_table_inc(incr->size_table, incr->len);
  }
  incr->len++;
  return incr;
}

static InternalNode* internal_node_dec(const InternalNode *original) {
  size_t size = sizeof(InternalNode) + (original->len - 1) * sizeof(InternalNode *);
  InternalNode *clone = RRB_MALLOC(size);
  memcpy(clone, original, size);
  // update length
  clone->len--;
  // Leaks the size table, but it's okay: Would cost more to actually make a
  // smaller one as its size would be roughly the same size.
  return clone;
}


static InternalNode* rebalance(InternalNode *left, InternalNode *centre,
                               InternalNode *right, uint32_t shift,
                               char is_top) {
  InternalNode *all = internal_node_merge(left, centre, right);
  // top_len is children count of the internal node returned.
  uint32_t top_len; // populated through pointer manipulation.

  uint32_t *node_count = create_concat_plan(all, &top_len);

  InternalNode *new_all = execute_concat_plan(all, node_count, top_len, shift);
  if (top_len <= RRB_BRANCHING) {
    if (is_top == false) {
      return internal_node_new_above1(set_sizes(new_all, shift));
    }
    else {
      return new_all;
    }
  }
  else {
    InternalNode *new_left = internal_node_copy(new_all, 0, RRB_BRANCHING);
    InternalNode *new_right = internal_node_copy(new_all, RRB_BRANCHING,
                                                 top_len - RRB_BRANCHING);
    return internal_node_new_above(set_sizes(new_left, shift),
                                   set_sizes(new_right, shift));
  }
}

/**
 * create_concat_plan takes in the large concatenated internal node and a
 * pointer to an uint32_t, which will contain the reduced size of the rebalanced
 * node. It returns a plan as an array of uint32_t's, and modifies the input
 * pointer to contain the length of said array.
 */

static uint32_t* create_concat_plan(InternalNode *all, uint32_t *top_len) {
  uint32_t *node_count = RRB_MALLOC_ATOMIC(all->len * sizeof(uint32_t));

  uint32_t total_nodes = 0;
  for (uint32_t i = 0; i < all->len; i++) {
    const uint32_t size = all->child[i]->len;
    node_count[i] = size;
    total_nodes += size;
  }

  const uint32_t optimal_slots = ((total_nodes-1) / RRB_BRANCHING) + 1;

  uint32_t shuffled_len = all->len;
  uint32_t i = 0;
  while (optimal_slots + RRB_EXTRAS < shuffled_len) {

    // Skip over all nodes satisfying the invariant.
    while (node_count[i] > RRB_BRANCHING - RRB_INVARIANT) {
      i++;
    }

    // Found short node, so redistribute over the next nodes
    uint32_t remaining_nodes = node_count[i];
    do {
      const uint32_t min_size = MIN(remaining_nodes + node_count[i+1], RRB_BRANCHING);
      node_count[i] = min_size;
      remaining_nodes = remaining_nodes + node_count[i+1] - min_size;
      i++;
    } while (remaining_nodes > 0);

    // Shuffle up remaining node sizes
    for (uint32_t j = i; j < shuffled_len - 1; j++) {
      node_count[j] = node_count[j+1]; // Could use memmove here I guess
    }
    shuffled_len--;
    i--;
  }

  *top_len = shuffled_len;
  return node_count;
}

static InternalNode* execute_concat_plan(InternalNode *all, uint32_t *node_size,
                                         uint32_t slen, uint32_t shift) {
  // the all vector doesn't have sizes set yet.

  InternalNode *new_all = internal_node_create(slen);
  // Current old node index to copy from
  uint32_t idx = 0;

  // Offset is how long into the current old node we've already copied from
  uint32_t offset = 0;

  if (shift == INC_SHIFT(LEAF_NODE_SHIFT)) { // handle leaf nodes here
    for (uint32_t i = 0; i < slen; i++) {
      const uint32_t new_size = node_size[i];
      LeafNode *old = (LeafNode *) all->child[idx];

      if (offset == 0 && new_size == old->len) {
        // just pointer copy the node if there is no offset and both have same
        // size
        idx++;
        new_all->child[i] = (InternalNode *) old;
      }
      else {
        LeafNode *new_node = leaf_node_create(new_size);
        uint32_t cur_size = 0;
        // cur_size is the current size of the new node
        // (the amount of elements copied into it so far)

        while (cur_size < new_size /*&& idx < all->len*/) {
          // the commented out check is verified by create_concat_plan --
          // otherwise the implementation is erroneous!
          const LeafNode *old_node = (LeafNode *) all->child[idx];

          if (new_size - cur_size >= old_node->len - offset) {
            // if this node can contain all elements not copied in the old node,
            // copy all of them into this node
            memcpy(&new_node->child[cur_size], &old_node->child[offset],
                   (old_node->len - offset) * sizeof(void *));
            cur_size += old_node->len - offset;
            idx++;
            offset = 0;
          }
          else {
            // if this node can't contain all the elements not copied in the old
            // node, copy as many as we can and pass the old node over to the
            // new node after this one.
            memcpy(&new_node->child[cur_size], &old_node->child[offset],
                   (new_size - cur_size) * sizeof(void *));
            offset += new_size - cur_size;
            cur_size = new_size;
          }
        }

        new_all->child[i] = (InternalNode *) new_node;
      }
    }
  }
  else { // not at lowest non-leaf level
    // this is ALMOST equivalent with the leaf node copying, the only difference
    // is that this is with internal nodes and the fact that they have to create
    // their size tables.

    // As that's the only difference, I won't bother with comments here.
    for (uint32_t i = 0; i < slen; i++) {
      const uint32_t new_size = node_size[i];
      InternalNode *old = all->child[idx];

      if (offset == 0 && new_size == old->len) {
        idx++;
        new_all->child[i] = old;
      }
      else {
        InternalNode *new_node = internal_node_create(new_size);
        uint32_t cur_size = 0;
        while (cur_size < new_size) {
          const InternalNode *old_node = all->child[idx];

          if (new_size - cur_size >= old_node->len - offset) {
            memcpy(&new_node->child[cur_size], &old_node->child[offset],
                   (old_node->len - offset) * sizeof(InternalNode *));
            cur_size += old_node->len - offset;
            idx++;
            offset = 0;
          }
          else {
            memcpy(&new_node->child[cur_size], &old_node->child[offset],
                   (new_size - cur_size) * sizeof(InternalNode *));
            offset += new_size - cur_size;
            cur_size = new_size;
          }
        }
        set_sizes(new_node, DEC_SHIFT(shift)); // This is where we set sizes
        new_all->child[i] = new_node;
      }
    }
  }
  return new_all;
}

// optimize this away?
static uint32_t find_shift(TreeNode *node) {
  if (node->type == LEAF_NODE) {
    return 0;
  }
  else { // must be internal node
    InternalNode *inode = (InternalNode *) node;
    return RRB_BITS + find_shift((TreeNode *) inode->child[0]);
  }
}

static InternalNode* set_sizes(InternalNode *node, uint32_t shift) {
  uint32_t sum = 0;
  RRBSizeTable *table = size_table_create(node->len);
  const uint32_t child_shift = DEC_SHIFT(shift);

  for (uint32_t i = 0; i < node->len; i++) {
    sum += size_sub_trie((TreeNode *) node->child[i], child_shift);
    table->size[i] = sum;
  }
  node->size_table = table;
  return node;
}

static uint32_t size_sub_trie(TreeNode *node, uint32_t shift) {
  if (shift > LEAF_NODE_SHIFT) {
    InternalNode *internal = (InternalNode *) node;
    if (internal->size_table == NULL) {
      uint32_t len = internal->len;
      uint32_t child_shift = DEC_SHIFT(shift);
      // TODO: for loopify recursive calls
      /* We're not sure how many are in the last child, so look it up */
      uint32_t last_size =
        size_sub_trie((TreeNode *) internal->child[len - 1], child_shift);
      /* We know all but the last ones are filled, and they have child_shift
         elements in them. */
      return ((len - 1) << shift) + last_size;
    }
    else {
      return internal->size_table->size[internal->len - 1];
    }
  }
  else {
    LeafNode *leaf = (LeafNode *) node;
    return leaf->len;
  }
}

static inline RRB* rrb_tail_push(const RRB *restrict rrb, const void *restrict elt);

static inline RRB* rrb_tail_push(const RRB *restrict rrb, const void *restrict elt) {
  RRB* new_rrb = rrb_head_clone(rrb);
  LeafNode *new_tail = leaf_node_inc(rrb->tail);
  new_tail->child[new_rrb->tail_len] = elt;
  new_rrb->cnt++;
  new_rrb->tail_len++;
  new_rrb->tail = new_tail;
  return new_rrb;
}

static InternalNode** copy_first_k(const RRB *rrb, RRB *new_rrb, const uint32_t k,
                                   const uint32_t tail_size);

static InternalNode** append_empty(InternalNode **to_set,
                                   uint32_t empty_height);

const RRB* rrb_push(const RRB *restrict rrb, const void *restrict elt) {
  if (rrb->tail_len < RRB_BRANCHING) {
    return rrb_tail_push(rrb, elt);
  }
  RRB *new_rrb = rrb_head_clone(rrb);
  new_rrb->cnt++;

  LeafNode *new_tail = leaf_node_create(1);
  new_tail->child[0] = elt;
  new_rrb->tail_len = 1;
  return push_down_tail(rrb, new_rrb, new_tail);
}


static RRB* push_down_tail(const RRB *restrict rrb, RRB *restrict new_rrb,
                           const LeafNode *restrict new_tail) {
  const LeafNode *old_tail = new_rrb->tail;
  new_rrb->tail = new_tail;
  if (rrb->cnt <= RRB_BRANCHING) {
    new_rrb->shift = LEAF_NODE_SHIFT;
    new_rrb->root = (TreeNode *) old_tail;
    return new_rrb;
  }
  // Copyable count starts here

  // TODO: Can find last rightmost jump in constant time for pvec subvecs:
  // use the fact that (index & large_mask) == 1 << (RRB_BITS * H) - 1 -> 0 etc.

  uint32_t index = rrb->cnt - 1;

  uint32_t nodes_to_copy = 0;
  uint32_t nodes_visited = 0;
  uint32_t pos = 0; // pos is the position we insert empty nodes in the bottom
                    // copyable node (or the element, if we can copy the leaf)
  const InternalNode *current = (const InternalNode *) rrb->root;

  uint32_t shift = RRB_SHIFT(rrb);

  // checking all non-leaf nodes (or if tail, all but the lowest two levels)
  while (shift > INC_SHIFT(LEAF_NODE_SHIFT)) {
    // calculate child index
    uint32_t child_index;
    if (current->size_table == NULL) {
      // some check here to ensure we're not overflowing the pvec subvec.
      // important to realise that this only needs to be done once in a better
      // impl, the same way the size_table check only has to be done until it's
      // false.
      const uint32_t prev_shift = shift + RRB_BITS;
      if (index >> prev_shift > 0) {
        nodes_visited++; // this could possibly be done earlier in the code.
        goto copyable_count_end;
      }
      child_index = (index >> shift) & RRB_MASK;
      // index filtering is not necessary when the check above is performed at
      // most once.
      index &= ~(RRB_MASK << shift);
    }
    else {
      // no need for sized_pos here, luckily.
      child_index = current->len - 1;
      // Decrement index
      if (child_index != 0) {
        index -= current->size_table->size[child_index-1];
      }
    }
    nodes_visited++;
    if (child_index < RRB_MASK) {
      nodes_to_copy = nodes_visited;
      pos = child_index;
    }

    current = current->child[child_index];
    // This will only happen in a pvec subtree
    if (current == NULL) {
      nodes_to_copy = nodes_visited;
      pos = child_index;

      // if next element we're looking at is null, we can copy all above. Good
      // times.
      goto copyable_count_end;
    }
    shift -= RRB_BITS;
  }
  // if we're here, we're at the leaf node (or lowest non-leaf), which is
  // `current`

  // no need to even use index here: We know it'll be placed at current->len,
  // if there's enough space. That check is easy.
  if (shift != 0) {
    nodes_visited++;
    if (current->len < RRB_BRANCHING) {
      nodes_to_copy = nodes_visited;
      pos = current->len;
    }
  }

 copyable_count_end:
  // GURRHH, nodes_visited is not yet handled nicely. for loop down to get
  // nodes_visited set straight.
  while (shift > INC_SHIFT(LEAF_NODE_SHIFT)) {
    nodes_visited++;
    shift -= RRB_BITS;
  }

  // Increasing height of tree.
  if (nodes_to_copy == 0) {
    InternalNode *new_root = internal_node_create(2);
    new_root->child[0] = (InternalNode *) rrb->root;
    new_rrb->root = (TreeNode *) new_root;
    new_rrb->shift = INC_SHIFT(RRB_SHIFT(new_rrb));

    // create size table if the original rrb root has a size table.
    if (rrb->root->type != LEAF_NODE &&
        ((const InternalNode *)rrb->root)->size_table != NULL) {
      RRBSizeTable *table = size_table_create(2);
      table->size[0] = rrb->cnt - old_tail->len;
      // If we insert the tail, the old size minus the old tail size will be the
      // amount of elements in the left branch. If there is no tail, the size is
      // just the old rrb-tree.

      table->size[1] = rrb->cnt;
      // If we insert the tail, the old size would include the tail.
      // Consequently, it has to be the old size. If we have no tail, we append
      // a single element to the old vector, therefore it has to be one more
      // than the original.

      new_root->size_table = table;
    }

    // nodes visited == original rrb tree height. Nodes visited > 0.
    InternalNode **to_set = append_empty(&((InternalNode *) new_rrb->root)->child[1],
                                         nodes_visited);
    *to_set = (InternalNode *) old_tail;
  }
  else {
    InternalNode **node = copy_first_k(rrb, new_rrb, nodes_to_copy, old_tail->len);
    InternalNode **to_set = append_empty(node, nodes_visited - nodes_to_copy);
    *to_set = (InternalNode *) old_tail;
  }

  return new_rrb;
}

// - Height should be shift or height, not max element size
// - copy_first_k returns a pointer to the next pointer to set
// - append_empty now returns a pointer to the *void we're supposed to set

static InternalNode** copy_first_k(const RRB *rrb, RRB *new_rrb, const uint32_t k,
                                   const uint32_t tail_size) {
  const InternalNode *current = (const InternalNode *) rrb->root;
  InternalNode **to_set = (InternalNode **) &new_rrb->root;
  uint32_t index = rrb->cnt - 1;
  uint32_t shift = RRB_SHIFT(rrb);

  // Copy all non-leaf nodes first. Happens when shift > RRB_BRANCHING
  uint32_t i = 1;
  while (i <= k && shift != 0) {
    // First off, copy current node and stick it in.
    InternalNode *new_current;
    if (i != k) {
      new_current = internal_node_clone(current);
      if (current->size_table != NULL) {
        new_current->size_table = size_table_clone(new_current->size_table, new_current->len);
        new_current->size_table->size[new_current->len-1] += tail_size;
      }
    }
    else { // increment size of last elt -- will only happen if we append empties
      new_current = internal_node_inc(current);
      if (current->size_table != NULL) {
        new_current->size_table->size[new_current->len-1] =
          new_current->size_table->size[new_current->len-2] + tail_size;
      }
    }
    *to_set = new_current;

    // calculate child index
    uint32_t child_index;
    if (current->size_table == NULL) {
      child_index = (index >> shift) & RRB_MASK;
    }
    else {
      // no need for sized_pos here, luckily.
      child_index = new_current->len - 1;
      // Decrement index
      if (child_index != 0) {
        index -= current->size_table->size[child_index-1];
      }
    }
    to_set = &new_current->child[child_index];
    current = current->child[child_index];

    i++;
    shift -= RRB_BITS;
  }

  return to_set;
}

static InternalNode** append_empty(InternalNode **to_set,
                                   uint32_t empty_height) {
  if (0 < empty_height) {
    InternalNode *leaf = internal_node_create(1);
    InternalNode *empty = (InternalNode *) leaf;
    for (uint32_t i = 1; i < empty_height; i++) {
      InternalNode *new_empty = internal_node_create(1);
      new_empty->child[0] = empty;
      empty = new_empty;
    }
    // this root node must be one larger, otherwise segfault
    *to_set = empty;
    return &leaf->child[0];
  }
  else {
    return to_set;
  }
}

static uint32_t sized_pos(const InternalNode *node, uint32_t *index,
                          uint32_t sp) {
  RRBSizeTable *table = node->size_table;
  uint32_t is = *index >> sp;
  while (table->size[is] <= *index) {
    is++;
  }
  if (is != 0) {
    *index -= table->size[is-1];
  }
  return is;
}

static const InternalNode* sized(const InternalNode *node, uint32_t *index,
                                 uint32_t sp) {
  uint32_t is = sized_pos(node, index, sp);
  return (InternalNode *) node->child[is];
}

void* rrb_nth(const RRB *rrb, uint32_t index) {
  if (index >= rrb->cnt) {
    return NULL;
  }
  const uint32_t tail_offset = rrb->cnt - rrb->tail_len;
  if (tail_offset <= index) {
    return rrb->tail->child[index - tail_offset];
  }
  else {
    const InternalNode *current = (const InternalNode *) rrb->root;
    for (uint32_t shift = RRB_SHIFT(rrb); shift > 0; shift -= RRB_BITS) {
      if (current->size_table == NULL) {
        const uint32_t subidx = (index >> shift) & RRB_MASK;
        current = current->child[subidx];
      }
      else {
        current = sized(current, &index, shift);
      }
    }
    return ((const LeafNode *)current)->child[index & RRB_MASK];
  }
}

uint32_t rrb_count(const RRB *rrb) {
  return rrb->cnt;
}

void* rrb_peek(const RRB *rrb) {
  return (void *) rrb->tail->child[rrb->tail_len-1];
}

/**
 * Destructively replaces the rightmost leaf as the new tail, discarding the
 * old.
 */
// Note that this is very similar to the direct pop algorithm, which is
// described further down in this file.
static void promote_rightmost_leaf(RRB *new_rrb) {
  InternalNode *path[RRB_MAX_HEIGHT+1];
  path[0] = (InternalNode *) new_rrb->root;
  uint32_t i = 0, shift = LEAF_NODE_SHIFT;

  // populate path array
  for (i = 0, shift = LEAF_NODE_SHIFT; shift < RRB_SHIFT(new_rrb);
       i++, shift += RRB_BITS) {
    path[i+1] = path[i]->child[path[i]->len-1];
  }

  const uint32_t height = i;
  // Set the leaf node as tail.
  new_rrb->tail = (LeafNode *) path[height];
  new_rrb->tail_len = path[height]->len;
  const uint32_t tail_len = new_rrb->tail_len;

  // last element is now always null, in contrast to direct pop
  path[height] = NULL;

  while (i --> 0) {
    // TODO: First skip will always happen. Can we use that somehow?
    if (path[i+1] == NULL) {
      if (path[i]->len == 1) {
        path[i] = NULL;
      }
      else if (i == 0 && path[i]->len == 2) {
        path[i] = path[i]->child[0];
        new_rrb->shift -= RRB_BITS;
      }
      else {
        path[i] = internal_node_dec(path[i]);
      }
    }
    else {
      path[i] = internal_node_clone(path[i]);
      path[i]->child[path[i]->len-1] = path[i+1];
      if (path[i]->size_table != NULL) {
        path[i]->size_table = size_table_clone(path[i]->size_table, path[i]->len);
        // this line differs, as we remove `tail_len` elements from the trie,
        // instead of just 1 as in the direct pop algorithm.
        path[i]->size_table->size[path[i]->len-1] -= tail_len;
      }
    }
  }

  new_rrb->root = (TreeNode *) path[0];
}

static RRB* slice_right(const RRB *rrb, const uint32_t right) {
  if (right == 0) {
    return (RRB *) rrb_create();
  }
  else if (right < rrb->cnt) {
    const uint32_t tail_offset = rrb->cnt - rrb->tail_len;
    // Can just cut the tail slightly
    if (tail_offset < right) {
      RRB *new_rrb = rrb_head_clone(rrb);
      const uint32_t new_tail_len = right - tail_offset;
      LeafNode *new_tail = leaf_node_create(new_tail_len);
      memcpy(new_tail->child, rrb->tail->child, new_tail_len * sizeof(void *));
      new_rrb->cnt = right;
      new_rrb->tail = new_tail;
      new_rrb->tail_len = new_tail_len;
      return new_rrb;
    }

    RRB *new_rrb = rrb_mutable_create();
    TreeNode *root = slice_right_rec(&RRB_SHIFT(new_rrb), rrb->root, right - 1,
                                     RRB_SHIFT(rrb), false);
    new_rrb->cnt = right;
    new_rrb->root = root;

    // Not sure if this is necessary in this part of the program, due to issues
    // wrt. slice_left and roots without size tables.
    promote_rightmost_leaf(new_rrb);
    new_rrb->tail_len = new_rrb->tail->len;
    return new_rrb;
  }
  else {
    return (RRB *) rrb;
  }
}

static TreeNode* slice_right_rec(uint32_t *total_shift, const TreeNode *root,
                                 uint32_t right, uint32_t shift,
                                 char has_left) {
  const uint32_t subshift = DEC_SHIFT(shift);
  uint32_t subidx = right >> shift;
  if (shift > LEAF_NODE_SHIFT) {
    const InternalNode *internal_root = (InternalNode *) root;
    if (internal_root->size_table == NULL) {
      TreeNode *right_hand_node =
        slice_right_rec(total_shift,
                        (TreeNode *) internal_root->child[subidx],
                        right - (subidx << shift), subshift,
                        (subidx != 0) | has_left);
      if (subidx == 0) {
        if (has_left) {
          InternalNode *right_hand_parent = internal_node_create(1);
          right_hand_parent->child[0] = (InternalNode *) right_hand_node;
          *total_shift = shift;
          return (TreeNode *) right_hand_parent;
        }
        else { // if (!has_left)
          return right_hand_node;
        }
      }
      else { // if (subidx != 0)
        InternalNode *sliced_root = internal_node_create(subidx + 1);
        memcpy(sliced_root->child, internal_root->child,
               subidx * sizeof(InternalNode *));
        sliced_root->child[subidx] = (InternalNode *) right_hand_node;
        *total_shift = shift;
        return (TreeNode *) sliced_root;
      }
    }
    else { // if (internal_root->size_table != NULL)
      RRBSizeTable *table = internal_root->size_table;
      uint32_t idx = right;

      while (table->size[subidx] <= idx) {
        subidx++;
      }
      if (subidx != 0) {
        idx -= table->size[subidx-1];
      }

      const TreeNode *right_hand_node =
        slice_right_rec(total_shift, internal_root->child[subidx], idx,
                        subshift, (subidx != 0) | has_left);
      if (subidx == 0) {
        if (has_left) {
          // As there is one above us, must place the right hand node in a
          // one-node
          InternalNode *right_hand_parent = internal_node_create(1);
          RRBSizeTable *right_hand_table = size_table_create(1);

          right_hand_table->size[0] = right + 1;
          // TODO: Not set size_table if the underlying node doesn't have a
          // table as well.
          right_hand_parent->size_table = right_hand_table;
          right_hand_parent->child[0] = (InternalNode *) right_hand_node;

          *total_shift = shift;
          return (TreeNode *) right_hand_parent;
        }
        else { // if (!has_left)
          return (TreeNode *) right_hand_node;
        }
      }
      else { // if (subidx != 0)
        InternalNode *sliced_root = internal_node_create(subidx+1);
        RRBSizeTable *sliced_table = size_table_create(subidx+1);

        memcpy(sliced_table->size, table->size, subidx * sizeof(uint32_t));
        sliced_table->size[subidx] = right+1;

        memcpy(sliced_root->child, internal_root->child,
               subidx * sizeof(InternalNode *));
        sliced_root->size_table = sliced_table;
        sliced_root->child[subidx] = (InternalNode *) right_hand_node;

        *total_shift = shift;
        return (TreeNode *) sliced_root;
      }
    }
  }
  else { // if (shift <= RRB_BRANCHING)
    // Just pure copying into a new node
    const LeafNode *leaf_root = (LeafNode *) root;
    LeafNode *left_vals = leaf_node_create(subidx + 1);

    memcpy(left_vals->child, leaf_root->child, (subidx + 1) * sizeof(void *));
    *total_shift = shift;
    return (TreeNode *) left_vals;
  }
}

const RRB* slice_left(RRB *rrb, uint32_t left) {
  if (left >= rrb->cnt) {
    return rrb_create();
  }
  else if (left > 0) {
    const uint32_t remaining = rrb->cnt - left;

    // If we slice into the tail, we just need to modify the tail itself
    if (remaining <= rrb->tail_len) {
      LeafNode *new_tail = leaf_node_create(remaining);
      memcpy(new_tail->child, &rrb->tail->child[rrb->tail_len - remaining],
             remaining * sizeof(void *));

      RRB *new_rrb = rrb_mutable_create();
      new_rrb->cnt = remaining;
      new_rrb->tail_len = remaining;
      new_rrb->tail = new_tail;
      return new_rrb;
    }
    // Otherwise, we don't really have to take the tail into consideration.
    // Good!

    RRB *new_rrb = rrb_mutable_create();
    InternalNode *root = (InternalNode *)
      slice_left_rec(&RRB_SHIFT(new_rrb), rrb->root, left,
                     RRB_SHIFT(rrb), false);
    new_rrb->cnt = remaining;
    new_rrb->root = (TreeNode *) root;

    // Ensure last element in size table is correct size, if the root is an
    // internal node.
    if (new_rrb->shift != LEAF_NODE_SHIFT && root->size_table != NULL) {
      root->size_table->size[root->len-1] = new_rrb->cnt - rrb->tail_len;
    }
    new_rrb->tail = rrb->tail;
    new_rrb->tail_len = rrb->tail_len;
    rrb = new_rrb;
  }

  // TODO: I think the code below also applies to root nodes where size_table
  // == NULL and (cnt - tail_len) & 0xff != 0, but it may be that this is
  // resolved by slice_right itself. Perhaps not promote in the right slicing,
  // but here instead?

  // This case handles leaf nodes < RRB_BRANCHING size, by redistributing
  // values from the tail into the actual leaf node.
  if (RRB_SHIFT(rrb) == 0 && rrb->root != NULL) {
    // two cases to handle: cnt <= RRB_BRANCHING
    //     and (cnt - tail_len) < RRB_BRANCHING

    if (rrb->cnt <= RRB_BRANCHING) {
      // can put all into a new tail
      LeafNode *new_tail = leaf_node_create(rrb->cnt);

      memcpy(&new_tail->child[0], &((LeafNode *) rrb->root)->child[0],
             rrb->root->len * sizeof(void *));
      memcpy(&new_tail->child[rrb->root->len], &rrb->tail->child[0],
             rrb->tail_len * sizeof(void *));
      rrb->tail_len = rrb->cnt;
      rrb->root = NULL;
      rrb->tail = new_tail;
    }
    // no need for <= here, because if the root node is == rrb_branching, the
    // invariant is kept.
    else if (rrb->cnt - rrb->tail_len < RRB_BRANCHING) {
      // create both a new tail and a new root node
      const uint32_t tail_cut = RRB_BRANCHING - rrb->root->len;
      LeafNode *new_root = leaf_node_create(RRB_BRANCHING);
      LeafNode *new_tail = leaf_node_create(rrb->tail_len - tail_cut);

      memcpy(&new_root->child[0], &((LeafNode *) rrb->root)->child[0],
             rrb->root->len * sizeof(void *));
      memcpy(&new_root->child[rrb->root->len], &rrb->tail->child[0],
             tail_cut * sizeof(void *));
      memcpy(&new_tail->child[0], &rrb->tail->child[tail_cut],
             (rrb->tail_len - tail_cut) * sizeof(void *));

      rrb->tail_len = rrb->tail_len - tail_cut;
      rrb->tail = new_tail;
      rrb->root = (TreeNode *) new_root;
    }
  }
  return rrb;
}

static TreeNode* slice_left_rec(uint32_t *total_shift, const TreeNode *root,
                                uint32_t left, uint32_t shift,
                                char has_right) {
  const uint32_t subshift = DEC_SHIFT(shift);
  uint32_t subidx = left >> shift;
  if (shift > LEAF_NODE_SHIFT) {
    const InternalNode *internal_root = (InternalNode *) root;
    uint32_t idx = left;
    if (internal_root->size_table == NULL) {
      idx -= subidx << shift;
    }
    else { // if (internal_root->size_table != NULL)
      const RRBSizeTable *table = internal_root->size_table;

      while (table->size[subidx] <= idx) {
        subidx++;
      }
      if (subidx != 0) {
        idx -= table->size[subidx - 1];
      }
    }

    const uint32_t last_slot = internal_root->len - 1;
    const TreeNode *child = (TreeNode *) internal_root->child[subidx];
    TreeNode *left_hand_node =
      slice_left_rec(total_shift, child, idx, subshift,
                     (subidx != last_slot) | has_right);
    if (subidx == last_slot) { // No more slots left
      if (has_right) {
        InternalNode *left_hand_parent = internal_node_create(1);
        const InternalNode *internal_left_hand_node = (InternalNode *) left_hand_node;
        left_hand_parent->child[0] = internal_left_hand_node;

        if (subshift != LEAF_NODE_SHIFT && internal_left_hand_node->size_table != NULL) {
          RRBSizeTable *sliced_table = size_table_create(1);
          sliced_table->size[0] =
            internal_left_hand_node->size_table->size[internal_left_hand_node->len-1];
          left_hand_parent->size_table = sliced_table;
        }
        *total_shift = shift;
        return (TreeNode *) left_hand_parent;
      }
      else { // if (!has_right)
        return left_hand_node;
      }
    }
    else { // if (subidx != last_slot)

      const uint32_t sliced_len = internal_root->len - subidx;
      InternalNode *sliced_root = internal_node_create(sliced_len);

      // TODO: Can shrink size here if sliced_len == 2, using the ambidextrous
      // vector technique w. offset. Takes constant time.

      memcpy(&sliced_root->child[1], &internal_root->child[subidx + 1],
             (sliced_len - 1) * sizeof(InternalNode *));

      const RRBSizeTable *table = internal_root->size_table;

      // TODO: Can check if left is a power of the tree size. If so, all nodes
      // will be completely populated, and we can ignore the size table. Most
      // importantly, this will remove the need to alloc a size table, which
      // increases perf.
      RRBSizeTable *sliced_table = size_table_create(sliced_len);

      if (table == NULL) {
        for (uint32_t i = 0; i < sliced_len; i++) {
          // left is total amount sliced off. By adding in subidx, we get faster
          // computation later on.
          sliced_table->size[i] = (subidx + 1 + i) << shift;
          // NOTE: This doesn't really work properly for top root, as last node
          // may have a higher count than it *actually* has. To remedy for this,
          // the top function performs a check afterwards, which may insert the
          // correct value if there's a size table in the root.
        }
      }
      else { // if (table != NULL)
        memcpy(sliced_table->size, &table->size[subidx],
               sliced_len * sizeof(uint32_t));
      }

      for (uint32_t i = 0; i < sliced_len; i++) {
        sliced_table->size[i] -= left;
      }

      sliced_root->size_table = sliced_table;
      sliced_root->child[0] = (InternalNode *) left_hand_node;
      *total_shift = shift;
      return (TreeNode *) sliced_root;
    }
  }
  else { // if (shift <= RRB_BRANCHING)
    LeafNode *leaf_root = (LeafNode *) root;
    const uint32_t right_vals_len = leaf_root->len - subidx;
    LeafNode *right_vals = leaf_node_create(right_vals_len);

    memcpy(right_vals->child, &leaf_root->child[subidx],
           right_vals_len * sizeof(void *));
    *total_shift = shift;

    return (TreeNode *) right_vals;
  }
}

const RRB* rrb_slice(const RRB *rrb, uint32_t from, uint32_t to) {
  return slice_left(slice_right(rrb, to), from);
}

const RRB* rrb_update(const RRB *restrict rrb, uint32_t index, const void *restrict elt) {
  if (index < rrb->cnt) {
    RRB *new_rrb = rrb_head_clone(rrb);
    const uint32_t tail_offset = rrb->cnt - rrb->tail_len;
    if (tail_offset <= index) {
      LeafNode *new_tail = leaf_node_clone(rrb->tail);
      new_tail->child[index - tail_offset] = elt;
      new_rrb->tail = new_tail;
      return new_rrb;
    }
    InternalNode **previous_pointer = (InternalNode **) &new_rrb->root;
    InternalNode *current = (InternalNode *) rrb->root;
    for (uint32_t shift = RRB_SHIFT(rrb); shift > 0; shift -= RRB_BITS) {
      current = internal_node_clone(current);
      *previous_pointer = current;

      uint32_t child_index;
      if (current->size_table == NULL) {
        child_index = (index >> shift) & RRB_MASK;
      }
      else {
        child_index = sized_pos(current, &index, shift);
      }
      previous_pointer = &current->child[child_index];
      current = current->child[child_index];
    }

    LeafNode *leaf = (LeafNode *) current;
    leaf = leaf_node_clone(leaf);
    *previous_pointer = (InternalNode *) leaf;
    leaf->child[index & RRB_MASK] = elt;
    return new_rrb;
  }
  else {
    return NULL;
  }
}

// Also assume direct append
const RRB* rrb_pop(const RRB *rrb) {
  if (rrb->cnt == 1) {
    return rrb_create();
  }
  RRB* new_rrb = rrb_head_clone(rrb);
  new_rrb->cnt--;

  if (rrb->tail_len == 1) {
    promote_rightmost_leaf(new_rrb);
    return new_rrb;
  }
  else {
    LeafNode *new_tail = leaf_node_dec(rrb->tail);
    new_rrb->tail_len--;
    new_rrb->tail = new_tail;
    return new_rrb;
  }
}

struct TransientRRB_ {
  uint32_t cnt;
  uint32_t shift;
  uint32_t tail_len;
  LeafNode *tail;
  TreeNode *root;
  RRBThread owner;
  GUID_DECLARATION
};

static const void* rrb_guid_create(void);
static TransientRRB* transient_rrb_head_create(const RRB* rrb);
static void check_transience(const TransientRRB *trrb);

static RRBSizeTable* transient_size_table_create(void);
static InternalNode* transient_internal_node_create(void);
static LeafNode* transient_leaf_node_create(void);
static RRBSizeTable* transient_size_table_clone(const RRBSizeTable *table,
                                                uint32_t len, const void *guid);
static InternalNode* transient_internal_node_clone(const InternalNode *internal,
                                                   const void *guid);
static LeafNode* transient_leaf_node_clone(const LeafNode *leaf, const void *guid);

static RRBSizeTable* ensure_size_table_editable(const RRBSizeTable *table,
                                                uint32_t len, const void *guid);
static InternalNode* ensure_internal_editable(InternalNode *internal, const void *guid);
static LeafNode* ensure_leaf_editable(LeafNode *leaf, const void *guid);

static void transient_promote_rightmost_leaf(TransientRRB* trrb);

static const void* rrb_guid_create() {
  return (const void *) RRB_MALLOC_ATOMIC(1);
}

static TransientRRB* transient_rrb_head_create(const RRB* rrb) {
  TransientRRB *trrb = RRB_MALLOC(sizeof(TransientRRB));
  memcpy(trrb, rrb, sizeof(RRB));
  trrb->owner = RRB_THREAD_ID();
  return trrb;
}

static void check_transience(const TransientRRB *trrb) {
  if (trrb->guid == NULL) {
    // Transient used after transient_to_persistent call
    exit(1);
  }
  if (!RRB_THREAD_EQUALS(trrb->owner, RRB_THREAD_ID())) {
    // Transient used by non-owner thread
    exit(1);
  }
}

static InternalNode* transient_internal_node_create() {
  InternalNode *node = RRB_MALLOC(sizeof(InternalNode)
                              + RRB_BRANCHING * sizeof(InternalNode *));
  node->type = INTERNAL_NODE;
  node->size_table = NULL;
  return node;
}

static RRBSizeTable* transient_size_table_create() {
  // this atomic allocation is, strictly speaking, NOT ok. Small chance of guid
  // being reallocated at same position if lost. Note when porting over to
  // different GC/Precise mode.
  RRBSizeTable *table = RRB_MALLOC_ATOMIC(sizeof(RRBSizeTable)
                                          + RRB_BRANCHING * sizeof(void *));
  return table;
}

static LeafNode* transient_leaf_node_create() {
  LeafNode *node = RRB_MALLOC(sizeof(LeafNode)
                              + RRB_BRANCHING * sizeof(void *));
  node->type = LEAF_NODE;
  return node;
}

static RRBSizeTable* transient_size_table_clone(const RRBSizeTable *table,
                                                uint32_t len, const void *guid) {
  RRBSizeTable *copy = transient_size_table_create();
  memcpy(copy, table, sizeof(RRBSizeTable) + len * sizeof(uint32_t));
  copy->guid = guid;
  return copy;
}


static InternalNode* transient_internal_node_clone(const InternalNode *internal,
                                                   const void *guid) {
  InternalNode *copy = transient_internal_node_create();
  memcpy(copy, internal,
         sizeof(InternalNode) + internal->len * sizeof(InternalNode *));
  copy->guid = guid;
  return copy;
}

static LeafNode* transient_leaf_node_clone(const LeafNode *leaf, const void *guid) {
  LeafNode *copy = transient_leaf_node_create();
  memcpy(copy, leaf, sizeof(LeafNode) + leaf->len * sizeof(void *));
  copy->guid = guid;
  return copy;
}

static RRBSizeTable* ensure_size_table_editable(const RRBSizeTable *table,
                                                uint32_t len, const void *guid) {
  if (table->guid == guid) {
    return table;
  }
  else {
    return transient_size_table_clone(table, len, guid);
  }
}

static InternalNode* ensure_internal_editable(InternalNode *internal, const void *guid) {
  if (internal->guid == guid) {
    return internal;
  }
  else {
    return transient_internal_node_clone(internal, guid);
  }
}

static LeafNode* ensure_leaf_editable(LeafNode *leaf, const void *guid) {
  if (leaf->guid == guid) {
    return leaf;
  }
  else {
    return transient_leaf_node_clone(leaf, guid);
  }
}

TransientRRB* rrb_to_transient(const RRB *rrb) {
  TransientRRB* trrb = transient_rrb_head_create(rrb);
  const void *guid = rrb_guid_create();
  trrb->guid = guid;
  trrb->tail = transient_leaf_node_clone(rrb->tail, guid);
  return trrb;
}

const RRB* transient_to_rrb(TransientRRB *trrb) {
  // Deny further modifications on the tree.
  trrb->guid = NULL;
  // reshrink tail
  // In case of optimisation where tail len is not modified (NOT yet tested!)
  // we have to handle it here first.
  trrb->tail = leaf_node_clone(trrb->tail);
  RRB* rrb = rrb_head_clone((const RRB *) trrb);
  return rrb;
}

uint32_t transient_rrb_count(const TransientRRB *trrb) {
  check_transience(trrb);
  return rrb_count((const RRB *) trrb);
}

void* transient_rrb_nth(const TransientRRB *trrb, uint32_t index) {
  check_transience(trrb);
  return rrb_nth((const RRB *) trrb, index);
}

void* transient_rrb_peek(const TransientRRB *trrb) {
  check_transience(trrb);
  return rrb_peek((const RRB *) trrb);
}

// rrb_push MUST use direct append techniques, otherwise it has to use
// concatenation. And, as mentioned in the report, concatenation modification is
// not evident.

static InternalNode** mutate_first_k(TransientRRB *trrb, const uint32_t k);

static InternalNode** new_editable_path(InternalNode **to_set,
                                        uint32_t empty_height, const void *guid);

TransientRRB* transient_rrb_push(TransientRRB *restrict trrb, const void *restrict elt) {
  check_transience(trrb);
  if (trrb->tail_len < RRB_BRANCHING) {
    trrb->tail->child[trrb->tail_len] = elt;
    trrb->cnt++;
    trrb->tail_len++;
    trrb->tail->len++;
    // ^ consider deferring incrementing this until insertion and/or persistentified.
    return trrb;
  }

  trrb->cnt++;
  const void *guid = trrb->guid;

  LeafNode *new_tail = transient_leaf_node_create();
  new_tail->guid = guid;
  new_tail->child[0] = elt;
  new_tail->len = 1;
  trrb->tail_len = 1;

  const LeafNode *old_tail = trrb->tail;
  trrb->tail = new_tail;

  if (trrb->root == NULL) { // If it's  null, we can't just mutate it down.
    trrb->shift = LEAF_NODE_SHIFT;
    trrb->root = (TreeNode *) old_tail;
    return trrb;
  }
  // mutable count starts here

  // TODO: Can find last rightmost jump in constant time for pvec subvecs:
  // use the fact that (index & large_mask) == 1 << (RRB_BITS * H) - 1 -> 0 etc.

  uint32_t index = trrb->cnt - 2;

  uint32_t nodes_to_mutate = 0;
  uint32_t nodes_visited = 0;
  uint32_t pos = 0; // pos is the position we insert empty nodes in the bottom
                    // mutable node (or the element, if we can mutate the leaf)
  const InternalNode *current = (const InternalNode *) trrb->root;

  uint32_t shift = RRB_SHIFT(trrb);

  // checking all non-leaf nodes (or if tail, all but the lowest two levels)
  while (shift > INC_SHIFT(LEAF_NODE_SHIFT)) {
    // calculate child index
    uint32_t child_index;
    if (current->size_table == NULL) {
      // some check here to ensure we're not overflowing the pvec subvec.
      // important to realise that this only needs to be done once in a better
      // impl, the same way the size_table check only has to be done until it's
      // false.
      const uint32_t prev_shift = shift + RRB_BITS;
      if (index >> prev_shift > 0) {
        nodes_visited++; // this could possibly be done earlier in the code.
        goto mutable_count_end;
      }
      child_index = (index >> shift) & RRB_MASK;
      // index filtering is not necessary when the check above is performed at
      // most once.
      index &= ~(RRB_MASK << shift);
    }
    else {
      // no need for sized_pos here, luckily.
      child_index = current->len - 1;
      // Decrement index
      if (child_index != 0) {
        index -= current->size_table->size[child_index-1];
      }
    }
    nodes_visited++;
    if (child_index < RRB_MASK) {
      nodes_to_mutate = nodes_visited;
      pos = child_index;
    }

    current = current->child[child_index];
    // This will only happen in a pvec subtree
    if (current == NULL) {
      nodes_to_mutate = nodes_visited;
      pos = child_index;

      // if next element we're looking at is null, we can mutate all above. Good
      // times.
      goto mutable_count_end;
    }
    shift -= RRB_BITS;
  }
  // if we're here, we're at the leaf node (or lowest non-leaf), which is
  // `current`

  // no need to even use index here: We know it'll be placed at current->len,
  // if there's enough space. That check is easy.
  if (shift != 0) {
    nodes_visited++;
    if (current->len < RRB_BRANCHING) {
      nodes_to_mutate = nodes_visited;
      pos = current->len;
    }
  }

 mutable_count_end:
  // GURRHH, nodes_visited is not yet handled nicely. loop down to get
  // nodes_visited set straight.
  while (shift > INC_SHIFT(LEAF_NODE_SHIFT)) {
    nodes_visited++;
    shift -= RRB_BITS;
  }

  // Increasing height of tree.
  if (nodes_to_mutate == 0) {
    const InternalNode *old_root = (const InternalNode *) trrb->root;
    InternalNode *new_root = transient_internal_node_create();
    new_root->guid = guid;
    new_root->len = 2;
    new_root->child[0] = (InternalNode *) trrb->root;
    trrb->root = (TreeNode *) new_root;
    trrb->shift = INC_SHIFT(RRB_SHIFT(trrb));

    // create size table if the original rrb root has a size table.
    if (old_root->type != LEAF_NODE &&
        ((const InternalNode *) old_root)->size_table != NULL) {
      RRBSizeTable *table = transient_size_table_create();
      table->guid = trrb->guid;
      table->size[0] = trrb->cnt - (old_tail->len + 1);
      // If we insert the tail, the old size minus (new size minus one) the old
      // tail size will be the amount of elements in the left branch. If there
      // is no tail, the size is just the old rrb-tree.

      table->size[1] = trrb->cnt - 1;
      // If we insert the tail, the old size would include the tail.
      // Consequently, it has to be the old size. If we have no tail, we append
      // a single element to the old vector, therefore it has to be one more
      // than the original (which means it is zero)

      new_root->size_table = table;
    }

    // nodes visited == original rrb tree height. Nodes visited > 0.
    InternalNode **to_set = new_editable_path(&((InternalNode *) trrb->root)->child[1],
                                              nodes_visited, guid);
    *to_set = (InternalNode *) old_tail;
  }
  else {
    InternalNode **node = mutate_first_k(trrb, nodes_to_mutate);
    InternalNode **to_set = new_editable_path(node, nodes_visited - nodes_to_mutate,
                                              guid);
    *to_set = (InternalNode *) old_tail;
  }

  return trrb;
}

static InternalNode** mutate_first_k(TransientRRB *trrb, const uint32_t k) {
  const void *guid = trrb->guid;
  InternalNode *current = (InternalNode *) trrb->root;
  InternalNode **to_set = (InternalNode **) &trrb->root;
  uint32_t index = trrb->cnt - 2;
  uint32_t shift = RRB_SHIFT(trrb);

  // mutate all non-leaf nodes first. Happens when shift > RRB_BRANCHING
  uint32_t i = 1;
  while (i <= k && shift != 0) {
    // First off, ensure current node is editable
    current = ensure_internal_editable(current, guid);
    *to_set = current;

    if (i == k) {
      // increase width of node
      current->len++;
    }

    if (current->size_table != NULL) {
      // Ensure size table is editable too
      RRBSizeTable *table = ensure_size_table_editable(current->size_table, current->len, guid);
      if (i != k) {
        // Tail will always be 32 long, otherwise we insert a single element only
        table->size[current->len-1] += RRB_BRANCHING;
      }
      else { // increment size of last elt -- will only happen if we append empties
        table->size[current->len-1] =
          table->size[current->len-2] + RRB_BRANCHING;
      }
      current->size_table = table;
    }

    // calculate child index
    uint32_t child_index;
    if (current->size_table == NULL) {
      child_index = (index >> shift) & RRB_MASK;
    }
    else {
      // no need for sized_pos here, luckily.
      child_index = current->len - 1;
      // Decrement index
      if (child_index != 0) {
        index -= current->size_table->size[child_index-1];
      }
    }
    to_set = &current->child[child_index];
    current = current->child[child_index];

    i++;
    shift -= RRB_BITS;
  }

  // check if we need to mutate the leaf node. Very likely to happen (31/32)
  if (i == k) {
    LeafNode *leaf = ensure_leaf_editable((LeafNode *) current, guid);
    leaf->len++;
    *to_set = (InternalNode *) leaf;
  }
  return to_set;
}

static InternalNode** new_editable_path(InternalNode **to_set, uint32_t empty_height,
                                        const void* guid) {
  if (0 < empty_height) {
    InternalNode *leaf = transient_internal_node_create();
    leaf->guid = guid;
    leaf->len = 1;

    InternalNode *empty = (InternalNode *) leaf;
    for (uint32_t i = 1; i < empty_height; i++) {
      InternalNode *new_empty = transient_internal_node_create();
      new_empty->guid = guid;
      new_empty->len = 1;
      new_empty->child[0] = empty;
      empty = new_empty;
    }
    *to_set = empty;
    return &leaf->child[0];
  }
  else {
    return to_set;
  }
}

// transient_rrb_update is effectively the same as rrb_update, but may mutate
// nodes if it's safe to do so (replacing clone calls with ensure_editable
// calls)
TransientRRB* transient_rrb_update(TransientRRB *restrict trrb, uint32_t index,
                                   const void *restrict elt) {
  check_transience(trrb);
  const void* guid = trrb->guid;
  if (index < trrb->cnt) {
    const uint32_t tail_offset = trrb->cnt - trrb->tail_len;
    if (tail_offset <= index) {
      trrb->tail->child[index - tail_offset] = elt;
      return trrb;
    }
    InternalNode **previous_pointer = (InternalNode **) &trrb->root;
    InternalNode *current = (InternalNode *) trrb->root;
    for (uint32_t shift = RRB_SHIFT(trrb); shift > 0; shift -= RRB_BITS) {
      current = ensure_internal_editable(current, guid);
      *previous_pointer = current;

      uint32_t child_index;
      if (current->size_table == NULL) {
        child_index = (index >> shift) & RRB_MASK;
      }
      else {
        child_index = sized_pos(current, &index, shift);
      }
      previous_pointer = &current->child[child_index];
      current = current->child[child_index];
    }
    
    LeafNode *leaf = (LeafNode *) current;
    leaf = ensure_leaf_editable((LeafNode *) leaf, guid);
    *previous_pointer = (InternalNode *) leaf;
    leaf->child[index & RRB_MASK] = elt;
    return trrb;
  }
  else {
    return NULL;
  }
}

TransientRRB* transient_rrb_pop(TransientRRB *trrb) {
  check_transience(trrb);
  if (trrb->cnt == 1) {
    trrb->cnt = 0;
    trrb->tail_len = 0;
    trrb->tail->child[0] = NULL;
    trrb->tail->len = 0;
    return trrb;
  }
  trrb->cnt--;

  if (trrb->tail_len == 1) {
    transient_promote_rightmost_leaf(trrb);
    return trrb;
  }
  else {
    trrb->tail->child[trrb->tail_len - 1] = NULL;
    trrb->tail_len--;
    trrb->tail->len--;

    return trrb;
  }
}

void transient_promote_rightmost_leaf(TransientRRB* trrb) {
  const void* guid = trrb->guid;

  InternalNode *path[RRB_MAX_HEIGHT+1];
  path[0] = (InternalNode *) trrb->root;
  uint32_t i = 0, shift = LEAF_NODE_SHIFT;

  // populate path array
  for (i = 0, shift = LEAF_NODE_SHIFT; shift < RRB_SHIFT(trrb);
       i++, shift += RRB_BITS) {
    path[i+1] = path[i]->child[path[i]->len-1];
  }

  const uint32_t height = i;

  // Set leaf node as tail.
  trrb->tail = (LeafNode *) path[height];
  trrb->tail_len = path[height]->len;
  const uint32_t tail_len = trrb->tail_len;

  path[height] = NULL;

  while (i --> 0) {
    if (path[i+1] == NULL && path[i]->len == 1) {
        path[i] = NULL;
    }
    else if (path[i+1] == NULL && i == 0 && path[0]->len == 2) {
      path[i] = path[i]->child[0];
      trrb->shift -= RRB_BITS;
    }
    else {
      path[i] = ensure_internal_editable(path[i], guid);
      path[i]->child[path[i]->len-1] = path[i+1];
      if (path[i+1] == NULL) {
        path[i]->len--;
      }
      if (path[i]->size_table != NULL) { // this is decrement-size-table*
        path[i]->size_table = ensure_size_table_editable(path[i]->size_table,
                                                         path[i]->len, guid);
        path[i]->size_table->size[path[i]->len-1] -= tail_len;
      }
    }
  }

  trrb->root = (TreeNode *) path[0];
}

// TODO: more efficient slicing algorithm for transients. Should in theory just
// require some size table magic and converting cloning over to ensure_editable.
TransientRRB* transient_rrb_slice(TransientRRB *trrb, uint32_t from, uint32_t to) {
  check_transience(trrb);
  const RRB* rrb = rrb_slice((const RRB*) trrb, from, to);
  memcpy(trrb, rrb, sizeof(RRB));
  return trrb;
}
