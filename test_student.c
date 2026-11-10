/*
 * test_student.c -- CS 313 P4: your three hand-traced tests.
 *
 * Name:
 *
 * Write three tests. Each one builds a tree, then checks its preorder with
 * CHECK_TREE(T, "..."), where the string is what YOU worked out by hand
 * before running anything. Draw the tree after every step and show the
 * key steps in a comment above the check.
 *
 * The autograder runs your tests against the instructor's rbtree.c. A test
 * passes if all its checks pass there and it has at least one CHECK_TREE
 * with 4 or more nodes. So a wrong hand trace fails the test.
 *
 * Suggested tests (do not reuse a scenario from test_rbtree.c):
 *   1. Insert at least 6 keys in an order that makes insert cases 1, 2,
 *      and 3 all happen at least once.
 *   2. Build a tree of at least 5 keys with rb_bst_insert, then rotate a
 *      node that is NOT the root (find it with rb_search). Keys only.
 *   3. Build a tree with rb_insert, then delete a BLACK node so that
 *      rb_delete_fixup does real work. Check before and after.
 *
 * Worked example (not one of your three):
 *
 *   rb_tree *T = rb_create();
 *   rb_insert(T, 10);          // 10B
 *   rb_insert(T, 20);          // 10B, right child 20R
 *   rb_insert(T, 30);          // red 30 under red 20, uncle is T->nil
 *                              // (black): case 3, rotate left at 10
 *   rb_insert(T, 40);          // uncle 10 is red: case 1, recolor;
 *                              // root 20 turns back to black
 *   CHECK_TREE(T, "20B 10B 30B 40R");
 *   rb_destroy(T);
 *
 * Build and run against your own rbtree.c:  make test
 */
#include "check.h"

void student_test_1(void)
{
    rb_tree *T = rb_create();
    /* TODO */
    rb_destroy(T);
}

void student_test_2(void)
{
    rb_tree *T = rb_create();
    /* TODO */
    rb_destroy(T);
}

void student_test_3(void)
{
    rb_tree *T = rb_create();
    /* TODO */
    rb_destroy(T);
}
