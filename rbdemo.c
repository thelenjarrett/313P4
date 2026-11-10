/*
 * rbdemo.c -- CS 313 P4: watch your red-black tree change, one step at a time.
 *
 *   make rbdemo
 *   ./rbdemo 41 38 31 12 19 8          insert each key with rb_insert (your fix-up runs)
 *   ./rbdemo 41 38 31 -38              a minus sign deletes, with rb_delete (your fix-up runs)
 *   ./rbdemo b60 b20 b80 b10 b40 L20   b: plain BST insert, no fix-up
 *                                      L, R: rotate left or right at the node with that key
 *
 * After each step, it draws the tree sideways with rb_print: the root is at
 * the left margin, right subtrees are above, and left subtrees are below.
 * Tilt your head to the left to see it. R is red, B is black, and <-- marks
 * a wrong parent pointer. Then it checks the pointers and BST order, and
 * the red-black rules too unless you used a plain BST insert.
 * Keys here must be 0 or more. This file is a tool for you; do not submit it.
 */
#include "rbtree.h"

#include <stdio.h>
#include <stdlib.h>

static bool read_key(const char *s, int *key)
{
    char *end;
    long v = strtol(s, &end, 10);
    if (*s == '\0' || *end != '\0' || v < 0 || v > 1000000000)
        return false;
    *key = (int)v;
    return true;
}

/* Describes the first wrong parent pointer or BST-order break under x in
 * msg, and returns true; returns false if there is none. */
static bool link_problem(const rb_tree *T, const rb_node *x, size_t *count, char *msg, size_t len)
{
    if (x == T->nil)
        return false;
    if (++(*count) > T->size) {
        snprintf(msg, len, "more nodes than rb_size says: a pointer loop?");
        return true;
    }
    for (int side = 0; side < 2; side++) {
        const rb_node *c = side == 0 ? x->left : x->right;
        const char *name = side == 0 ? "left" : "right";
        if (c == T->nil)
            continue;
        if (c->p != x) {
            snprintf(msg, len, "%d is the %s child of %d, but its parent pointer is not %d",
                     c->key, name, x->key, x->key);
            return true;
        }
        if (side == 0 ? c->key > x->key : c->key < x->key) {
            snprintf(msg, len, "%d is the %s child of %d, which breaks BST order", c->key, name, x->key);
            return true;
        }
    }
    return link_problem(T, x->left, count, msg, len) || link_problem(T, x->right, count, msg, len);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s KEY ...   (41 inserts, -41 deletes, b41 BST-inserts, L41 or R41 rotates)\n",
                argv[0]);
        return 1;
    }
    rb_tree *T = rb_create();
    if (T == NULL) {
        fprintf(stderr, "rbdemo: out of memory\n");
        return 1;
    }
    bool plain = false;                 /* has a plain BST insert been used? */
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        char op = 'i';
        if (arg[0] == '-' || arg[0] == 'b' || arg[0] == 'L' || arg[0] == 'R')
            op = *arg++;
        int key;
        if (!read_key(arg, &key)) {
            fprintf(stderr, "rbdemo: cannot read \"%s\"\n", argv[i]);
            rb_destroy(T);
            return 1;
        }
        if (op == 'i') {
            printf("== insert %d\n", key);
            rb_insert(T, key);
        } else if (op == 'b') {
            printf("== BST insert %d (no fix-up)\n", key);
            rb_bst_insert(T, key);
            plain = true;
        } else if (op == '-') {
            printf("== delete %d%s\n", key, rb_delete(T, key) ? "" : ": not in the tree");
        } else {
            rb_node *x = rb_search(T, key);
            printf("== %s-rotate at %d", op == 'L' ? "left" : "right", key);
            if (x == T->nil)
                printf(": not in the tree");
            else if (!(op == 'L' ? rb_left_rotate(T, x) : rb_right_rotate(T, x)))
                printf(": the rotation returned false");
            printf("\n");
        }
        rb_print(T);
        char msg[160];
        size_t count = 0;
        const char *why;
        if (T->root != T->nil && T->root->p != T->nil)
            printf("   (pointer problem: the root's parent pointer should be T->nil)\n\n");
        else if (link_problem(T, T->root, &count, msg, sizeof msg))
            printf("   (pointer problem: %s)\n\n", msg);
        else if (count != T->size)
            printf("   (pointer problem: only %zu of the %zu nodes can be reached from the root)\n\n",
                   count, T->size);
        else if (plain)
            printf("   (pointers and BST order are fine; colors are not checked after a plain BST insert)\n\n");
        else if (rb_is_valid(T, &why))
            printf("   (a valid red-black tree)\n\n");
        else
            printf("   (not a valid red-black tree: %s)\n\n", why);
    }
    rb_destroy(T);
    return 0;
}
