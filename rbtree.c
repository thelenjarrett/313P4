/*
 * rbtree.c -- CS 313 P4: red-black tree (CLRS 4th edition, chapter 13).
 *
 * Everything above the line "YOUR CODE BELOW" is provided and works.
 * Read it: rb_insert and rb_delete show exactly when your fixups run.
 * You write the seven functions at the bottom of the file: four that keep
 * the tree balanced, and three for an inorder iterator.
 */
#include "rbtree.h"

#include <stdio.h>
#include <stdlib.h>

/* ================= Provided code ================= */

rb_tree *rb_create(void)
{
    rb_tree *T = malloc(sizeof *T);
    if (T == NULL)
        return NULL;
    T->nil = malloc(sizeof *T->nil);
    if (T->nil == NULL) {
        free(T);
        return NULL;
    }
    T->nil->key = 0;
    T->nil->color = RB_BLACK;
    T->nil->left = T->nil;
    T->nil->right = T->nil;
    T->nil->p = T->nil;
    T->root = T->nil;
    T->size = 0;
    T->version = 0;
    return T;
}

static void free_subtree(rb_tree *T, rb_node *x)
{
    if (x == T->nil)
        return;
    free_subtree(T, x->left);
    free_subtree(T, x->right);
    free(x);
}

void rb_destroy(rb_tree *T)
{
    if (T == NULL)
        return;
    free_subtree(T, T->root);
    free(T->nil);
    free(T);
}

size_t rb_size(const rb_tree *T)
{
    return T->size;
}

bool rb_iter_changed(const rb_iter *it)
{
    return it->version != it->T->version;
}

/* CLRS ITERATIVE-TREE-SEARCH */
rb_node *rb_search(const rb_tree *T, int key)
{
    rb_node *x = T->root;
    while (x != T->nil && key != x->key) {
        if (key < x->key)
            x = x->left;
        else
            x = x->right;
    }
    return x;
}

bool rb_contains(const rb_tree *T, int key)
{
    return rb_search(T, key) != T->nil;
}

/* CLRS TREE-MINIMUM on the subtree rooted at x (x != T->nil) */
static rb_node *tree_minimum(const rb_tree *T, rb_node *x)
{
    while (x->left != T->nil)
        x = x->left;
    return x;
}

bool rb_minimum(const rb_tree *T, int *out)
{
    if (T->root == T->nil)
        return false;
    *out = tree_minimum(T, T->root)->key;
    return true;
}

/* The BST part of CLRS RB-INSERT: make a red node z and link it in.
 * Equal keys go right. Returns z, or NULL if out of memory. */
static rb_node *bst_insert_node(rb_tree *T, int key)
{
    rb_node *z = malloc(sizeof *z);
    if (z == NULL)
        return NULL;
    z->key = key;
    rb_node *x = T->root;       /* node being compared with z */
    rb_node *y = T->nil;        /* y will be the parent of z */
    while (x != T->nil) {
        y = x;
        if (z->key < x->key)
            x = x->left;
        else
            x = x->right;
    }
    z->p = y;
    if (y == T->nil)
        T->root = z;            /* tree was empty */
    else if (z->key < y->key)
        y->left = z;
    else
        y->right = z;
    z->left = T->nil;
    z->right = T->nil;
    z->color = RB_RED;
    T->size++;
    T->version++;
    return z;
}

/* CLRS RB-INSERT */
bool rb_insert(rb_tree *T, int key)
{
    rb_node *z = bst_insert_node(T, key);
    if (z == NULL)
        return false;
    rb_insert_fixup(T, z);
    return true;
}

/* Plain BST insert with no fixup. The new node is red and the tree may
 * break red-black rules. Use it only to build shapes for rotation tests. */
bool rb_bst_insert(rb_tree *T, int key)
{
    return bst_insert_node(T, key) != NULL;
}

/* CLRS RB-TRANSPLANT: replace the subtree rooted at u with the one rooted
 * at v. Sets v->p even when v == T->nil; rb_delete_fixup relies on that. */
static void rb_transplant(rb_tree *T, rb_node *u, rb_node *v)
{
    if (u->p == T->nil)
        T->root = v;
    else if (u == u->p->left)
        u->p->left = v;
    else
        u->p->right = v;
    v->p = u->p;
}

/* CLRS RB-DELETE. Removes one node with the given key. */
bool rb_delete(rb_tree *T, int key)
{
    rb_node *z = rb_search(T, key);
    if (z == T->nil)
        return false;

    rb_node *x;
    rb_node *y = z;
    rb_color y_original_color = y->color;
    if (z->left == T->nil) {
        x = z->right;
        rb_transplant(T, z, z->right);      /* replace z by its right child */
    } else if (z->right == T->nil) {
        x = z->left;
        rb_transplant(T, z, z->left);       /* replace z by its left child */
    } else {
        y = tree_minimum(T, z->right);      /* y is z's successor */
        y_original_color = y->color;
        x = y->right;
        if (y != z->right) {                /* is y farther down the tree? */
            rb_transplant(T, y, y->right);  /* replace y by its right child */
            y->right = z->right;            /* z's right child becomes */
            y->right->p = y;                /*   y's right child */
        } else {
            x->p = y;                       /* in case x is T->nil */
        }
        rb_transplant(T, z, y);             /* replace z by its successor y */
        y->left = z->left;                  /* and give z's left child to y, */
        y->left->p = y;                     /*   which had no left child */
        y->color = z->color;
    }
    free(z);
    T->size--;
    T->version++;
    if (y_original_color == RB_BLACK)       /* if any red-black violations occurred, */
        rb_delete_fixup(T, x);              /*   correct them (x may be T->nil) */
    return true;
}

/* *n counts every node visited; only the first cap are written. */
static void preorder_fill(const rb_tree *T, const rb_node *x,
                          int keys[], rb_color colors[], size_t cap, size_t *n)
{
    if (x == T->nil)
        return;
    if (*n < cap) {
        if (keys != NULL)
            keys[*n] = x->key;
        if (colors != NULL)
            colors[*n] = x->color;
    }
    (*n)++;
    preorder_fill(T, x->left, keys, colors, cap, n);
    preorder_fill(T, x->right, keys, colors, cap, n);
}

size_t rb_preorder(const rb_tree *T, int keys[], rb_color colors[], size_t cap)
{
    size_t n = 0;
    preorder_fill(T, T->root, keys, colors, cap, &n);
    return n;
}

static int height_of(const rb_tree *T, const rb_node *x)
{
    if (x == T->nil)
        return -1;
    int hl = height_of(T, x->left);
    int hr = height_of(T, x->right);
    return 1 + (hl > hr ? hl : hr);
}

int rb_height(const rb_tree *T)
{
    return height_of(T, T->root);
}

/* Checks the subtree at x. Returns its black-height (counting T->nil as 1),
 * or -1 after setting *why if something is wrong. *count counts nodes.
 * prev and have_prev track the last key seen in order, to check BST order. */
static int check_subtree(const rb_tree *T, const rb_node *x, size_t *count,
                         int *prev, bool *have_prev, const char **why)
{
    if (x == T->nil)
        return 1;
    if (++(*count) > T->size) {
        *why = "more nodes than T->size (or a cycle)";
        return -1;
    }
    if (x->color != RB_RED && x->color != RB_BLACK) {
        *why = "a node has a color that is neither RB_RED nor RB_BLACK";
        return -1;
    }
    if (x->left != T->nil && x->left->p != x) {
        *why = "a left child's parent pointer is wrong";
        return -1;
    }
    if (x->right != T->nil && x->right->p != x) {
        *why = "a right child's parent pointer is wrong";
        return -1;
    }
    if (x->color == RB_RED &&
        (x->left->color == RB_RED || x->right->color == RB_RED)) {
        *why = "property 4 fails: a red node has a red child";
        return -1;
    }
    int bl = check_subtree(T, x->left, count, prev, have_prev, why);
    if (bl < 0)
        return -1;
    if (*have_prev && x->key < *prev) {
        *why = "BST order fails: in-order keys are not sorted";
        return -1;
    }
    *prev = x->key;
    *have_prev = true;
    int br = check_subtree(T, x->right, count, prev, have_prev, why);
    if (br < 0)
        return -1;
    if (bl != br) {
        *why = "property 5 fails: black-heights differ";
        return -1;
    }
    return bl + (x->color == RB_BLACK ? 1 : 0);
}

bool rb_is_valid(const rb_tree *T, const char **why)
{
    const char *dummy;
    if (why == NULL)
        why = &dummy;
    *why = "ok";
    if (T == NULL || T->nil == NULL || T->root == NULL) {
        *why = "tree, root, or nil pointer is NULL";
        return false;
    }
    if (T->nil->color != RB_BLACK) {
        *why = "property 3 fails: T->nil is not black";
        return false;
    }
    if (T->root == T->nil) {
        if (T->size != 0) {
            *why = "empty tree but T->size != 0";
            return false;
        }
        return true;
    }
    if (T->root->color != RB_BLACK) {
        *why = "property 2 fails: the root is not black";
        return false;
    }
    if (T->root->p != T->nil) {
        *why = "the root's parent is not T->nil";
        return false;
    }
    size_t count = 0;
    int prev = 0;
    bool have_prev = false;
    if (check_subtree(T, T->root, &count, &prev, &have_prev, why) < 0)
        return false;
    if (count != T->size) {
        *why = "node count does not match T->size";
        return false;
    }
    return true;
}

/* p is the node that x hangs from (T->nil for the root), so a wrong parent
 * pointer can be marked. A tree deeper than its number of nodes has a
 * pointer loop, so the drawing stops there. */
static void print_subtree(const rb_tree *T, const rb_node *x, const rb_node *p, int depth)
{
    if (x == T->nil)
        return;
    if ((size_t)depth > T->size) {
        printf("%*s... (deeper than the tree has nodes: a pointer loop?)\n", 4 * depth, "");
        return;
    }
    print_subtree(T, x->right, x, depth + 1);
    printf("%*s%d%c", 4 * depth, "", x->key, x->color == RB_RED ? 'R' : 'B');
    if (x->p != p) {
        if (x->p == T->nil)
            printf("   <-- parent pointer is NIL");
        else
            printf("   <-- parent pointer is %d", x->p->key);
        if (p == T->nil)
            printf(", but this node is the root");
        else
            printf(", but this node hangs from %d", p->key);
    }
    printf("\n");
    print_subtree(T, x->left, x, depth + 1);
}

/* Prints the tree rotated 90 degrees left: the root is at the left margin,
 * right subtrees are above, left subtrees below. R = red, B = black.
 * A node whose parent pointer is wrong is marked with <--. */
void rb_print(const rb_tree *T)
{
    if (T->root == T->nil) {
        printf("(empty tree)\n");
        return;
    }
    print_subtree(T, T->root, T->nil, 0);
}

/* ================= YOUR CODE BELOW =================
 * Follow CLRS 4th edition, sections 13.2 (rotations), 13.3 (insertion),
 * and 13.4 (deletion). Compare with T->nil, never with NULL.
 */

/* CLRS LEFT-ROTATE (section 13.2). If x == T->nil or x->right == T->nil,
 * change nothing and return false. Otherwise rotate and return true.
 * Six pointers change. When x->p == T->nil, y becomes T->root. */
bool rb_left_rotate(rb_tree *T, rb_node *x)
{
    /* TODO */
    (void)T;
    (void)x;
    return false;
}

/* CLRS RIGHT-ROTATE: the mirror image of rb_left_rotate. Swap every
 * "left" and "right". Return false if x == T->nil or x->left == T->nil. */
bool rb_right_rotate(rb_tree *T, rb_node *x)
{
    /* TODO */
    (void)T;
    (void)x;
    return false;
}

/* CLRS RB-INSERT-FIXUP (section 13.3). z is red and was just linked in.
 * Loop while z's parent is red; look at the uncle's color to pick the case.
 * Finish by making the root black. */
void rb_insert_fixup(rb_tree *T, rb_node *z)
{
    /* TODO */
    (void)T;
    (void)z;
}

/* CLRS RB-DELETE-FIXUP (section 13.4). x carries an "extra black".
 * x may be T->nil; rb_delete has set x->p, so x->p is safe to use.
 * Loop while x is not the root and x is black; look at the sibling w. */
void rb_delete_fixup(rb_tree *T, rb_node *x)
{
    /* TODO */
    (void)T;
    (void)x;
}

/* ---------- Inorder iterator ----------
 * it->next is the node whose key rb_iter_next hands out next, or T->nil
 * when none are left. The provided tree_minimum may help.
 */

/* Start at the smallest key (T->nil if the tree is empty). */
rb_iter rb_iter_first(const rb_tree *T)
{
    rb_iter it = {T, T->nil, T->version};
    /* TODO */
    return it;
}

/* Start at the first key >= lo in sorted order, in one walk down from the
 * root. If x->key >= lo, x might be the answer, but an earlier one might be
 * in x's left subtree. With duplicates, a rotation can put a copy of a key
 * in the left subtree of another copy, so do not stop at the first match. */
rb_iter rb_iter_from(const rb_tree *T, int lo)
{
    rb_iter it = {T, T->nil, T->version};
    /* TODO */
    (void)lo;
    return it;
}

/* If the tree has changed since the iterator was made, return false and
 * leave *key alone: it->next may have been freed, so do not read it.
 * If it->next is T->nil, return false and leave *key alone. Otherwise set
 * *key to it->next's key, move it->next to the following node with the two
 * cases of CLRS TREE-SUCCESSOR (section 12.2), and return true. */
bool rb_iter_next(rb_iter *it, int *key)
{
    /* TODO */
    (void)it;
    (void)key;
    return false;
}
