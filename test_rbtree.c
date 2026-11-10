/*
 * test_rbtree.c -- CS 313 P4 test harness. These are the autograder's tests.
 *
 *   ./test_rbtree            run every test (each in its own process, 10 s limit)
 *   ./test_rbtree NAME       run one test in this process (good for gdb/valgrind)
 *   ./test_rbtree --list     list the test names
 *   ./test_rbtree --inproc   run every test in this process (used for the leak check)
 *
 * Colors are written as strings such as "BBBR": one letter per node in
 * preorder, B = black, R = red.
 */
#define _POSIX_C_SOURCE 200809L   /* for fork, alarm, strsignal */
#include "rbtree.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define TIME_LIMIT_SECONDS 10
#define MAXN 5000

/* ---------------- helpers ---------------- */

static void fail(const char *fmt, const char *detail)
{
    printf("    FAILED: ");
    printf(fmt, detail);
    printf("\n");
}

static rb_tree *make_tree(void)
{
    rb_tree *T = rb_create();
    if (T == NULL) {
        printf("    FAILED: rb_create returned NULL\n");
        exit(1);
    }
    return T;
}

static void insert_all(rb_tree *T, const int *keys, size_t n)
{
    for (size_t i = 0; i < n; i++)
        rb_insert(T, keys[i]);
}

static void bst_insert_all(rb_tree *T, const int *keys, size_t n)
{
    for (size_t i = 0; i < n; i++)
        rb_bst_insert(T, keys[i]);
}

static void print_keys(const char *label, const int *keys, size_t n)
{
    printf("      %s [", label);
    for (size_t i = 0; i < n && i < 30; i++)
        printf("%s%d", i ? ", " : "", keys[i]);
    if (n > 30)
        printf(", ... (%zu keys in all)", n);
    printf("]\n");
}

static void print_colors(const char *label, const rb_color *c, size_t n)
{
    printf("      %s \"", label);
    for (size_t i = 0; i < n; i++)
        putchar(c[i] == RB_RED ? 'R' : c[i] == RB_BLACK ? 'B' : '?');
    printf("\"\n");
}

/* Draws the tree whose preorder is keys[lo..hi-1] (distinct keys) sideways,
 * the way rb_print does. colors may be NULL. */
static void print_expected(const int *keys, const char *colors, size_t lo, size_t hi, int depth)
{
    if (lo >= hi)
        return;
    size_t mid = lo + 1;            /* the right subtree starts at the first key > keys[lo] */
    while (mid < hi && keys[mid] < keys[lo])
        mid++;
    print_expected(keys, colors, mid, hi, depth + 1);
    if (colors != NULL)
        printf("%*s%d%c\n", 4 * depth, "", keys[lo], colors[lo]);
    else
        printf("%*s%d\n", 4 * depth, "", keys[lo]);
    print_expected(keys, colors, lo + 1, mid, depth + 1);
}

static bool all_distinct(const int *keys, size_t n)
{
    for (size_t i = 0; i < n; i++)
        for (size_t j = i + 1; j < n; j++)
            if (keys[i] == keys[j])
                return false;
    return true;
}

/* Compare the preorder keys (and colors, unless colors == NULL) with the
 * expected ones. Prints a diagnosis on mismatch. */
static bool expect_preorder(const rb_tree *T, const char *step,
                            const int *keys, const char *colors, size_t n)
{
    static int got_keys[MAXN];
    static rb_color got_colors[MAXN];
    static rb_color want_colors[MAXN];
    size_t sz = rb_size(T);
    if (sz > MAXN) {
        printf("    FAILED after %s: rb_size is %zu, which is too large\n", step, sz);
        return false;
    }
    size_t got = rb_preorder(T, got_keys, got_colors, MAXN);
    if (got > MAXN) {
        printf("    FAILED after %s: the tree has more nodes than rb_size says (a cycle?)\n", step);
        return false;
    }
    bool ok = (got == n) && memcmp(got_keys, keys, n * sizeof(int)) == 0;
    if (ok && colors != NULL) {
        for (size_t i = 0; i < n; i++) {
            want_colors[i] = colors[i] == 'R' ? RB_RED : RB_BLACK;
            if (got_colors[i] != want_colors[i])
                ok = false;
        }
    }
    if (!ok) {
        printf("    FAILED after %s: preorder %s differ\n", step,
               colors ? "keys or colors" : "keys");
        print_keys("expected keys:  ", keys, n);
        print_keys("got keys:       ", got_keys, got);
        if (colors != NULL) {
            printf("      expected colors: \"%s\"\n", colors);
            print_colors("got colors:     ", got_colors, got);
        }
        if (n <= 40 && all_distinct(keys, n)) {
            printf("      expected tree (sideways: root at the left, right subtrees above):\n");
            print_expected(keys, colors, 0, n, 0);
        }
        printf("      tree you built:\n");
        rb_print(T);
    }
    return ok;
}

/* Finds the first wrong parent pointer or BST-order break in the subtree at
 * x and describes it in msg. *count counts nodes, to catch pointer loops.
 * Used after rotations on trees built by rb_bst_insert; colors are not checked. */
static bool find_link_problem(const rb_tree *T, const rb_node *x, size_t *count, size_t limit,
                              char *msg, size_t len)
{
    if (x == T->nil)
        return false;
    if (++(*count) > limit) {
        snprintf(msg, len, "the tree has more nodes than rb_size says (a pointer loop?)");
        return true;
    }
    for (int side = 0; side < 2; side++) {
        const rb_node *c = side == 0 ? x->left : x->right;
        const char *name = side == 0 ? "left" : "right";
        if (c == T->nil)
            continue;
        if (c->p != x) {
            if (c->p == T->nil)
                snprintf(msg, len, "%d is the %s child of %d, but its parent pointer is NIL",
                         c->key, name, x->key);
            else
                snprintf(msg, len, "%d is the %s child of %d, but its parent pointer is %d",
                         c->key, name, x->key, c->p->key);
            return true;
        }
        if (side == 0 ? c->key > x->key : c->key < x->key) {
            snprintf(msg, len, "%d is the %s child of %d, which breaks BST order", c->key, name, x->key);
            return true;
        }
    }
    return find_link_problem(T, x->left, count, limit, msg, len)
        || find_link_problem(T, x->right, count, limit, msg, len);
}

static bool expect_links(const rb_tree *T, const char *step)
{
    char msg[160];
    size_t count = 0;
    bool bad = true;
    if (T->root != T->nil && T->root->p != T->nil)
        snprintf(msg, sizeof msg, "the root %d has a parent pointer; T->root->p should be T->nil",
                 T->root->key);
    else if (find_link_problem(T, T->root, &count, T->size, msg, sizeof msg))
        ;
    else if (count != T->size)
        snprintf(msg, sizeof msg, "only %zu of the %zu nodes can be reached from the root (a lost node?)",
                 count, T->size);
    else
        bad = false;
    if (bad) {
        printf("    FAILED after %s: %s\n", step, msg);
        if (T->size <= 40) {
            printf("      tree you built (<-- marks a wrong parent pointer):\n");
            rb_print(T);
        }
    }
    return !bad;
}

static bool expect_valid(const rb_tree *T, const char *step)
{
    const char *why;
    if (!rb_is_valid(T, &why)) {
        printf("    FAILED after %s: rb_is_valid says: %s\n", step, why);
        if (rb_size(T) <= 20) {
            printf("      tree you built:\n");
            rb_print(T);
        }
        return false;
    }
    return true;
}

/* The keys in sorted order, read straight from the nodes rather than with
 * your iterator, so that only the iterator tests depend on the iterator.
 * Returns the number of nodes; writes at most cap keys. */
static void inorder_fill(const rb_tree *T, const rb_node *x, int out[], size_t cap, size_t *n)
{
    if (x == T->nil)
        return;
    inorder_fill(T, x->left, out, cap, n);
    if (*n < cap)
        out[*n] = x->key;
    (*n)++;
    inorder_fill(T, x->right, out, cap, n);
}

static size_t inorder_keys(const rb_tree *T, int out[], size_t cap)
{
    size_t n = 0;
    inorder_fill(T, T->root, out, cap, &n);
    return n;
}

/* Deterministic pseudo-random numbers, the same on every machine. */
static unsigned long rng_state;
static void rng_seed(unsigned long s) { rng_state = s; }
static unsigned long rng_next(void)
{
    rng_state = rng_state * 6364136223846793005UL + 1442695040888963407UL;
    return (rng_state >> 33);
}

static void shuffle(int *a, size_t n)
{
    for (size_t i = n; i > 1; i--) {
        size_t j = rng_next() % i;
        int t = a[i - 1];
        a[i - 1] = a[j];
        a[j] = t;
    }
}

/* The 9-key tree from the 24F lab, built by plain BST insert. */
static const int NINE[] = {7, 5, 9, 3, 6, 8, 10, 1, 2};

/* ---------------- core tests ---------------- */

static bool t_rotate_left(void)
{
    bool ok = true;
    rb_tree *T = make_tree();
    int a[] = {2, 1, 3};
    bst_insert_all(T, a, 3);
    ok = ok && rb_left_rotate(T, T->root);
    if (!ok) fail("%s", "rb_left_rotate(T, root) returned false on 2/(1,3)");
    int e1[] = {3, 2, 1};
    ok = ok && expect_preorder(T, "left rotate at root of 2/(1,3)", e1, NULL, 3);
    ok = ok && expect_links(T, "left rotate at root of 2/(1,3)");
    rb_destroy(T);
    if (!ok) return false;

    T = make_tree();
    bst_insert_all(T, NINE, 9);
    rb_left_rotate(T, T->root);
    int e2[] = {9, 7, 5, 3, 1, 2, 6, 8, 10};
    ok = expect_preorder(T, "left rotate at root 7 of the 9-key tree", e2, NULL, 9)
         && expect_links(T, "left rotate at root 7 of the 9-key tree");
    rb_destroy(T);
    if (!ok) return false;

    T = make_tree();
    bst_insert_all(T, NINE, 9);
    rb_left_rotate(T, rb_search(T, 9));
    int e3[] = {7, 5, 3, 1, 2, 6, 10, 9, 8};
    ok = expect_preorder(T, "left rotate at node 9 of the 9-key tree", e3, NULL, 9)
         && expect_links(T, "left rotate at node 9 of the 9-key tree");
    rb_destroy(T);
    return ok;
}

static bool t_rotate_right(void)
{
    bool ok = true;
    rb_tree *T = make_tree();
    int a[] = {2, 1, 3};
    bst_insert_all(T, a, 3);
    ok = ok && rb_right_rotate(T, T->root);
    if (!ok) fail("%s", "rb_right_rotate(T, root) returned false on 2/(1,3)");
    int e1[] = {1, 2, 3};
    ok = ok && expect_preorder(T, "right rotate at root of 2/(1,3)", e1, NULL, 3);
    ok = ok && expect_links(T, "right rotate at root of 2/(1,3)");
    rb_destroy(T);
    if (!ok) return false;

    T = make_tree();
    bst_insert_all(T, NINE, 9);
    rb_right_rotate(T, T->root);
    int e2[] = {5, 3, 1, 2, 7, 6, 9, 8, 10};
    ok = expect_preorder(T, "right rotate at root 7 of the 9-key tree", e2, NULL, 9)
         && expect_links(T, "right rotate at root 7 of the 9-key tree");
    rb_destroy(T);
    if (!ok) return false;

    T = make_tree();
    bst_insert_all(T, NINE, 9);
    rb_right_rotate(T, rb_search(T, 5));
    int e3[] = {7, 3, 1, 2, 5, 6, 9, 8, 10};
    ok = expect_preorder(T, "right rotate at node 5 of the 9-key tree", e3, NULL, 9)
         && expect_links(T, "right rotate at node 5 of the 9-key tree");
    rb_destroy(T);
    return ok;
}

static bool t_insert_one(void)
{
    rb_tree *T = make_tree();
    rb_insert(T, 5);
    int e[] = {5};
    bool ok = expect_preorder(T, "insert 5 into an empty tree", e, "B", 1)
              && expect_valid(T, "insert 5 into an empty tree");
    rb_destroy(T);
    return ok;
}

static bool t_insert_2134(void)
{
    rb_tree *T = make_tree();
    int a[] = {2, 1, 3, 4};
    insert_all(T, a, 4);
    int e[] = {2, 1, 3, 4};
    bool ok = expect_preorder(T, "inserting 2, 1, 3, 4", e, "BBBR", 4);
    rb_destroy(T);
    return ok;
}

static bool t_insert_1to7(void)
{
    rb_tree *T = make_tree();
    int a[] = {1, 2, 3, 4, 5, 6, 7};
    insert_all(T, a, 7);
    int e[] = {2, 1, 4, 3, 6, 5, 7};
    bool ok = expect_preorder(T, "inserting 1..7", e, "BBRBBRR", 7);
    rb_destroy(T);
    return ok;
}

static bool t_insert_nine(void)
{
    rb_tree *T = make_tree();
    insert_all(T, NINE, 9);
    int e[] = {7, 5, 2, 1, 3, 6, 9, 8, 10};
    bool ok = expect_preorder(T, "inserting 7, 5, 9, 3, 6, 8, 10, 1, 2", e, "BRBRRBBRR", 9);
    rb_destroy(T);
    return ok;
}

/* Valid after every single insert: ascending, descending, and 3 random orders. */
static bool t_insert_valid_1000(void)
{
    static int keys[1000];
    char step[80];
    for (int order = 0; order < 5; order++) {
        for (int i = 0; i < 1000; i++)
            keys[i] = (order == 1) ? 1000 - i : i + 1;
        if (order >= 2) {
            rng_seed(313 + order);
            shuffle(keys, 1000);
        }
        rb_tree *T = make_tree();
        for (int i = 0; i < 1000; i++) {
            rb_insert(T, keys[i]);
            snprintf(step, sizeof step, "insert #%d (key %d, %s order)", i + 1, keys[i],
                     order == 0 ? "ascending" : order == 1 ? "descending" : "random");
            if (!expect_valid(T, step)) {
                rb_destroy(T);
                return false;
            }
        }
        rb_destroy(T);
    }
    return true;
}

/* The case that broke the 24F provided code: x is T->nil. */
static bool t_delete_nil_x(void)
{
    rb_tree *T = make_tree();
    int a[] = {3, 2, 4, 1};
    insert_all(T, a, 4);
    int e1[] = {3, 2, 1, 4};
    bool ok = expect_preorder(T, "inserting 3, 2, 4, 1", e1, "BBRB", 4);
    if (ok) {
        rb_delete(T, 4);
        int e2[] = {2, 1, 3};
        ok = expect_preorder(T, "deleting 4 (x is T->nil, sibling 2 has red child 1)", e2, "BBB", 3)
             && expect_valid(T, "deleting 4");
    }
    rb_destroy(T);
    return ok;
}

static bool t_delete_small(void)
{
    rb_tree *T = make_tree();
    int a[] = {7, 5, 9, 6};
    insert_all(T, a, 4);
    int e1[] = {7, 5, 6, 9};
    bool ok = expect_preorder(T, "inserting 7, 5, 9, 6", e1, "BBRB", 4);
    if (ok) {
        rb_delete(T, 9);
        int e2[] = {6, 5, 7};
        ok = expect_preorder(T, "deleting 9", e2, "BBB", 3);
    }
    rb_destroy(T);
    if (!ok) return false;

    T = make_tree();
    int b[] = {1, 2, 3, 4, 5, 6, 7};
    insert_all(T, b, 7);
    rb_delete(T, 5);
    int e3[] = {2, 1, 4, 3, 6, 7};
    ok = expect_preorder(T, "inserting 1..7, then deleting 5", e3, "BBRBBR", 6);
    if (ok) {
        rb_delete(T, 4);
        int e4[] = {2, 1, 6, 3, 7};
        ok = expect_preorder(T, "then deleting 4", e4, "BBRBB", 5);
    }
    rb_destroy(T);
    return ok;
}

static bool t_delete_nine(void)
{
    rb_tree *T = make_tree();
    insert_all(T, NINE, 9);
    rb_delete(T, 6);
    int e1[] = {7, 2, 1, 5, 3, 9, 8, 10};
    bool ok = expect_preorder(T, "inserting the 9 keys, then deleting 6", e1, "BRBBRBRR", 8);
    if (ok) {
        rb_delete(T, 7);
        int e2[] = {8, 2, 1, 5, 3, 9, 10};
        ok = expect_preorder(T, "then deleting 7", e2, "BRBBRBR", 7);
    }
    rb_destroy(T);
    if (!ok) return false;

    T = make_tree();
    insert_all(T, NINE, 9);
    rb_delete(T, 6);
    rb_insert(T, 4);
    int e3[] = {7, 2, 1, 4, 3, 5, 9, 8, 10};
    ok = expect_preorder(T, "inserting the 9 keys, deleting 6, inserting 4", e3, "BRBBRRBRR", 9);
    if (ok) {
        rb_delete(T, 7);
        int e4[] = {8, 2, 1, 4, 3, 5, 9, 10};
        ok = expect_preorder(T, "then deleting 7", e4, "BRBBRRBR", 8);
    }
    rb_destroy(T);
    return ok;
}

/* Insert 1..500, then delete every key, checking validity after each delete.
 * Orders: ascending, descending, and 3 random orders. */
static bool t_delete_all_valid(void)
{
    enum { N = 500 };
    static int ins[N], del[N];
    char step[96];
    for (int order = 0; order < 5; order++) {
        for (int i = 0; i < N; i++) {
            ins[i] = i + 1;
            del[i] = (order == 1) ? N - i : i + 1;
        }
        rng_seed(1000 + order);
        shuffle(ins, N);
        if (order >= 2)
            shuffle(del, N);
        rb_tree *T = make_tree();
        insert_all(T, ins, N);
        for (int i = 0; i < N; i++) {
            snprintf(step, sizeof step, "delete #%d (key %d, %s order)", i + 1, del[i],
                     order == 0 ? "ascending" : order == 1 ? "descending" : "random");
            if (!rb_delete(T, del[i])) {
                printf("    FAILED at %s: rb_delete returned false for a key in the tree\n", step);
                rb_destroy(T);
                return false;
            }
            if (!expect_valid(T, step)) {
                rb_destroy(T);
                return false;
            }
        }
        bool empty = T->root == T->nil && rb_size(T) == 0;
        rb_destroy(T);
        if (!empty) {
            printf("    FAILED: tree not empty after deleting every key\n");
            return false;
        }
    }
    return true;
}

/* ---------------- other tests ---------------- */

static bool t_rotate_refuses(void)
{
    rb_tree *T = make_tree();
    bool ok = true;
    if (rb_left_rotate(T, T->root) || rb_right_rotate(T, T->root)) {
        fail("%s", "rotating T->nil in an empty tree should return false");
        ok = false;
    }
    int a[] = {2, 1, 3};
    bst_insert_all(T, a, 3);
    if (ok && (rb_left_rotate(T, rb_search(T, 3)) || rb_right_rotate(T, rb_search(T, 1)))) {
        fail("%s", "left-rotating a node with no right child (or the mirror) should return false");
        ok = false;
    }
    int e[] = {2, 1, 3};
    ok = ok && expect_preorder(T, "refused rotations", e, NULL, 3);
    rb_destroy(T);
    return ok;
}

static bool t_rotate_keeps_colors(void)
{
    rb_tree *T = make_tree();
    insert_all(T, NINE, 9);     /* preorder 7 5 2 1 3 6 9 8 10, colors BRBRRBBRR */
    rb_left_rotate(T, rb_search(T, 5));
    rb_right_rotate(T, rb_search(T, 6));
    int e[] = {7, 5, 2, 1, 3, 6, 9, 8, 10};
    bool ok = expect_preorder(T, "left rotate at 5 then right rotate at 6", e, "BRBRRBBRR", 9)
              && expect_links(T, "two rotations");
    rb_destroy(T);
    return ok;
}

static bool t_empty_contract(void)
{
    rb_tree *T = make_tree();
    int out = 12345;
    bool ok = true;
    if (rb_size(T) != 0) { fail("%s", "rb_size of a new tree is not 0"); ok = false; }
    if (rb_minimum(T, &out) || out != 12345) { fail("%s", "rb_minimum on an empty tree should return false and leave *out alone"); ok = false; }
    if (rb_search(T, 1) != T->nil) { fail("%s", "rb_search on an empty tree should return T->nil"); ok = false; }
    if (rb_delete(T, 1)) { fail("%s", "rb_delete on an empty tree should return false"); ok = false; }
    if (rb_height(T) != -1) { fail("%s", "rb_height of an empty tree should be -1"); ok = false; }
    ok = ok && expect_valid(T, "creating an empty tree");
    rb_destroy(T);
    return ok;
}

static bool t_delete_missing(void)
{
    rb_tree *T = make_tree();
    insert_all(T, NINE, 9);
    bool ok = true;
    if (rb_delete(T, 4) || rb_delete(T, 0) || rb_delete(T, 11)) {
        fail("%s", "rb_delete of a missing key should return false");
        ok = false;
    }
    int e[] = {7, 5, 2, 1, 3, 6, 9, 8, 10};
    ok = ok && expect_preorder(T, "deleting missing keys 4, 0, 11", e, "BRBRRBBRR", 9);
    ok = ok && rb_size(T) == 9;
    rb_destroy(T);
    return ok;
}

static bool t_duplicates(void)
{
    rb_tree *T = make_tree();
    int a[] = {5, 5, 5, 3, 5, 8, 5};
    insert_all(T, a, 7);
    bool ok = expect_valid(T, "inserting 5, 5, 5, 3, 5, 8, 5");
    int in[7], want[] = {3, 5, 5, 5, 5, 5, 8};
    if (ok && (inorder_keys(T, in, 7) != 7 || memcmp(in, want, sizeof want) != 0)) {
        fail("%s", "in-order keys with duplicates are wrong");
        ok = false;
    }
    for (int i = 0; ok && i < 5; i++) {
        ok = rb_delete(T, 5) && expect_valid(T, "deleting one copy of 5");
    }
    if (ok && (rb_contains(T, 5) || rb_size(T) != 2)) {
        fail("%s", "after deleting all five 5s the tree should hold only 3 and 8");
        ok = false;
    }
    rb_destroy(T);
    return ok;
}

/* CLRS Lemma 13.1: height (in nodes) <= 2 lg(n + 1). */
static bool t_height_bound(void)
{
    static int keys[4095];
    for (int order = 0; order < 3; order++) {
        for (int i = 0; i < 4095; i++)
            keys[i] = i;
        if (order == 2) {
            rng_seed(99);
            shuffle(keys, 4095);
        }
        rb_tree *T = make_tree();
        for (int n = 1; n <= 4095; n++) {
            rb_insert(T, order == 1 ? 4095 - keys[n - 1] : keys[n - 1]);
            int h = rb_height(T) + 1;       /* nodes on the longest path */
            /* h <= 2 lg(n+1)  is the same as  2^h <= (n+1)^2 */
            unsigned long long sq = (unsigned long long)(n + 1) * (unsigned long long)(n + 1);
            if (h >= 63 || (1ULL << h) > sq) {
                printf("    FAILED: height %d (in nodes) with n = %d exceeds 2 lg(n+1)\n", h, n);
                rb_destroy(T);
                return false;
            }
        }
        rb_destroy(T);
    }
    return true;
}

/* 20000 random inserts and deletes on keys 0..299, compared with a count table. */
static bool t_random_mixed(void)
{
    enum { K = 300, OPS = 20000 };
    static int count[K];
    memset(count, 0, sizeof count);
    rb_tree *T = make_tree();
    rng_seed(2026);
    size_t expected = 0;
    char step[64];
    for (int i = 0; i < OPS; i++) {
        int k = (int)(rng_next() % K);
        bool ins = rng_next() % 5 < 3;
        if (ins) {
            rb_insert(T, k);
            count[k]++;
            expected++;
        } else {
            bool r = rb_delete(T, k);
            if (r != (count[k] > 0)) {
                printf("    FAILED at op %d: rb_delete(%d) returned %s\n", i, k, r ? "true" : "false");
                rb_destroy(T);
                return false;
            }
            if (r) { count[k]--; expected--; }
        }
        if (i % 50 == 0 || i == OPS - 1) {
            snprintf(step, sizeof step, "random operation #%d", i);
            if (!expect_valid(T, step) || rb_size(T) != expected) {
                rb_destroy(T);
                return false;
            }
        }
    }
    for (int k = 0; k < K; k++) {
        if (rb_contains(T, k) != (count[k] > 0)) {
            printf("    FAILED: rb_contains(%d) is wrong after random operations\n", k);
            rb_destroy(T);
            return false;
        }
    }
    rb_destroy(T);
    return true;
}

static bool t_delete_root_repeatedly(void)
{
    rb_tree *T = make_tree();
    for (int i = 1; i <= 200; i++)
        rb_insert(T, (i * 37) % 211);
    bool ok = true;
    while (ok && T->root != T->nil) {
        ok = rb_delete(T, T->root->key) && expect_valid(T, "deleting the root");
    }
    rb_destroy(T);
    return ok;
}

static bool t_delete_then_reinsert(void)
{
    rb_tree *T = make_tree();
    bool ok = true;
    for (int round = 0; ok && round < 3; round++) {
        for (int i = 0; i < 100; i++)
            rb_insert(T, i);
        for (int i = 0; ok && i < 100; i += 2)
            ok = rb_delete(T, i);
        for (int i = 1; ok && i < 100; i += 2)
            ok = rb_delete(T, i);
        ok = ok && rb_size(T) == 0 && T->root == T->nil && expect_valid(T, "emptying the tree");
    }
    rb_destroy(T);
    return ok;
}

static bool t_min_search(void)
{
    rb_tree *T = make_tree();
    for (int i = 0; i < 300; i++)
        rb_insert(T, (i * 7919) % 1009);
    int m = -1;
    bool ok = rb_minimum(T, &m) && m == 0;
    if (!ok) fail("%s", "rb_minimum is wrong after 300 inserts");
    rb_node *x = rb_search(T, 7919 % 1009);
    if (ok && (x == T->nil || x->key != 7919 % 1009)) {
        fail("%s", "rb_search did not find an inserted key");
        ok = false;
    }
    ok = ok && expect_valid(T, "300 inserts");
    rb_destroy(T);
    return ok;
}

/* ---------------- iterator tests ----------------
 * These build trees with rb_bst_insert or by hand, so they do not depend on
 * your rotations or fixups. The iterator needs only BST order and parent
 * pointers. */

/* Link in a node by hand. Plain BST insert never puts a copy of a key in
 * the left subtree of another copy, but rotations do: rb_insert 5 three
 * times and see. */
static rb_node *hand_node(rb_tree *T, rb_node *parent, int key, bool left)
{
    rb_node *z = malloc(sizeof *z);
    if (z == NULL) {
        printf("    FAILED: out of memory\n");
        exit(1);
    }
    z->key = key;
    z->color = RB_BLACK;
    z->left = T->nil;
    z->right = T->nil;
    z->p = parent;
    if (parent == T->nil)
        T->root = z;
    else if (left)
        parent->left = z;
    else
        parent->right = z;
    T->size++;
    return z;
}

/* Run the iterator to the end and compare its keys with want[0..n-1]. Then
 * check that one more call returns false and leaves the key alone. */
static bool expect_iter(rb_iter it, const char *step, const int *want, size_t n)
{
    static int got[MAXN];
    size_t m = 0;
    int key;
    while (rb_iter_next(&it, &key)) {
        if (m == MAXN) {
            printf("    FAILED %s: rb_iter_next returned more than %d keys (does it stop?)\n", step, MAXN);
            return false;
        }
        got[m++] = key;
    }
    if (m != n || (n > 0 && memcmp(got, want, n * sizeof(int)) != 0)) {
        printf("    FAILED %s: the iterator's keys are wrong\n", step);
        print_keys("expected keys:", want, n);
        print_keys("got keys:     ", got, m);
        return false;
    }
    key = 12345;
    if (rb_iter_next(&it, &key) || key != 12345) {
        printf("    FAILED %s: after the last key, rb_iter_next should keep returning"
               " false and leave *key alone\n", step);
        return false;
    }
    return true;
}

static bool t_iter_empty(void)
{
    rb_tree *T = make_tree();
    bool ok = expect_iter(rb_iter_first(T), "with rb_iter_first on an empty tree", NULL, 0)
              && expect_iter(rb_iter_from(T, 0), "with rb_iter_from(T, 0) on an empty tree", NULL, 0);
    rb_destroy(T);
    return ok;
}

static bool t_iter_sorted(void)
{
    rb_tree *T = make_tree();
    bst_insert_all(T, NINE, 9);     /* 2 -> 3 and 6 -> 7 walk up two levels */
    int want[] = {1, 2, 3, 5, 6, 7, 8, 9, 10};
    bool ok = expect_iter(rb_iter_first(T), "with rb_iter_first on the 9-key tree", want, 9);
    rb_destroy(T);
    if (!ok) return false;

    T = make_tree();
    rb_bst_insert(T, 42);
    int one[] = {42};
    ok = expect_iter(rb_iter_first(T), "with rb_iter_first on a one-key tree", one, 1);
    rb_destroy(T);
    return ok;
}

/* Two paths, a zigzag, and three random trees, compared with an inorder walk. */
static bool t_iter_shapes(void)
{
    enum { N = 300 };
    static int keys[N], want[N];
    static const char *const names[] = {
        "50 ascending keys (a path of right children)",
        "50 descending keys (a path of left children)",
        "50 keys in zigzag order 50, 1, 48, 3, ...",
        "300 random keys", "300 random keys", "300 random keys",
    };
    char step[96];
    for (int shape = 0; shape < 6; shape++) {
        int n = shape < 3 ? 50 : N;
        rng_seed(500 + shape);
        for (int i = 0; i < n; i++) {
            if (shape == 0)
                keys[i] = i + 1;
            else if (shape == 1)
                keys[i] = n - i;
            else if (shape == 2)
                keys[i] = (i % 2) ? i : n - i;
            else
                keys[i] = (int)(rng_next() % 1000);     /* some duplicates */
        }
        rb_tree *T = make_tree();
        bst_insert_all(T, keys, n);
        size_t m = inorder_keys(T, want, N);
        snprintf(step, sizeof step, "with rb_iter_first on %s", names[shape]);
        bool ok = expect_iter(rb_iter_first(T), step, want, m);
        rb_destroy(T);
        if (!ok)
            return false;
    }
    return true;
}

static bool t_iter_duplicates(void)
{
    rb_tree *T = make_tree();
    int a[] = {5, 5, 5, 3, 5, 8, 5};
    bst_insert_all(T, a, 7);
    int all[] = {3, 5, 5, 5, 5, 5, 8};
    bool ok = expect_iter(rb_iter_first(T), "with rb_iter_first after inserting 5, 5, 5, 3, 5, 8, 5", all, 7)
              && expect_iter(rb_iter_from(T, 5), "with rb_iter_from(T, 5) on that tree", all + 1, 6)
              && expect_iter(rb_iter_from(T, 6), "with rb_iter_from(T, 6) on that tree", all + 6, 1);
    rb_destroy(T);
    if (!ok) return false;

    /* Root 5, with left child 5 (whose left child is 3) and right child 8. */
    T = make_tree();
    rb_node *r = hand_node(T, T->nil, 5, false);
    rb_node *l = hand_node(T, r, 5, true);
    hand_node(T, l, 3, true);
    hand_node(T, r, 8, false);
    int want[] = {3, 5, 5, 8};
    ok = expect_iter(rb_iter_first(T), "with rb_iter_first on the tree 5 (left 5 (left 3), right 8)", want, 4)
         && expect_iter(rb_iter_from(T, 5), "with rb_iter_from(T, 5) on the tree 5 (left 5 (left 3), right 8)", want + 1, 3)
         && expect_iter(rb_iter_from(T, 4), "with rb_iter_from(T, 4) on that tree", want + 1, 3);
    rb_destroy(T);
    return ok;
}

/* Every starting key from -1 to 12 on the 9-key tree, then 200 random
 * starting keys on a tree of 300 random keys. */
static bool t_iter_from(void)
{
    enum { N = 300 };
    static int want[N];
    char step[80];
    rb_tree *T = make_tree();
    bst_insert_all(T, NINE, 9);
    size_t n = inorder_keys(T, want, N);
    for (int lo = -1; lo <= 12; lo++) {
        size_t i = 0;
        while (i < n && want[i] < lo)
            i++;
        snprintf(step, sizeof step, "with rb_iter_from(T, %d) on the 9-key tree", lo);
        if (!expect_iter(rb_iter_from(T, lo), step, want + i, n - i)) {
            rb_destroy(T);
            return false;
        }
    }
    rb_destroy(T);

    T = make_tree();
    rng_seed(77);
    for (int i = 0; i < N; i++)
        rb_bst_insert(T, (int)(rng_next() % 1000));
    n = inorder_keys(T, want, N);
    for (int t = 0; t < 200; t++) {
        int lo = (int)(rng_next() % 1010) - 5;
        size_t i = 0;
        while (i < n && want[i] < lo)
            i++;
        snprintf(step, sizeof step, "with rb_iter_from(T, %d) on a tree of 300 random keys", lo);
        if (!expect_iter(rb_iter_from(T, lo), step, want + i, n - i)) {
            rb_destroy(T);
            return false;
        }
    }
    rb_destroy(T);
    return true;
}

/* After an insert or a delete, an old iterator must stop: rb_iter_next
 * returns false without reading its node, which a delete may have freed,
 * and rb_iter_changed says why. Deleting a missing key changes nothing.
 * The tree is built with rb_bst_insert (all red), so rb_delete never calls
 * rb_delete_fixup here. */
static bool t_iter_changed(void)
{
    static const int keys[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    static const int after[] = {1, 2, 4, 5, 6, 7, 8, 9, 10, 11};
    rb_tree *T = make_tree();
    bst_insert_all(T, keys, 10);
    bool ok = true;
    int key = 0;

    rb_iter it = rb_iter_first(T);
    if (!rb_iter_next(&it, &key) || key != 1) {
        fail("%s", "rb_iter_first, then rb_iter_next, should give 1");
        ok = false;
    }
    if (ok) {
        rb_delete(T, 99);
        if (rb_iter_changed(&it) || !rb_iter_next(&it, &key) || key != 2) {
            fail("%s", "deleting a missing key changes nothing, so the iterator should go on to 2");
            ok = false;
        }
    }
    if (ok) {
        rb_delete(T, 3);            /* frees the node the iterator points to */
        key = 12345;
        if (rb_iter_next(&it, &key) || key != 12345) {
            fail("%s", "after a delete, the old iterator's rb_iter_next should return false"
                       " and leave *key alone");
            ok = false;
        } else if (!rb_iter_changed(&it)) {
            fail("%s", "after a delete, rb_iter_changed should return true");
            ok = false;
        }
    }
    if (ok) {
        it = rb_iter_from(T, 5);
        rb_bst_insert(T, 11);
        key = 12345;
        if (rb_iter_next(&it, &key) || key != 12345) {
            fail("%s", "after an insert, an old iterator from rb_iter_from should return false"
                       " and leave *key alone");
            ok = false;
        }
    }
    if (ok)
        ok = expect_iter(rb_iter_first(T), "with a new iterator after the changes", after, 10);
    rb_destroy(T);
    return ok;
}

/* 200000 keys: one full walk, then 100000 calls to rb_iter_from. An
 * iterator that starts over from the root or scans from the smallest key
 * runs out of time. */
static bool t_iter_large(void)
{
    enum { N = 200000 };
    static int keys[N];
    for (int i = 0; i < N; i++)
        keys[i] = 2 * i;                        /* the even numbers 0 .. 2N - 2 */
    rng_seed(4242);
    shuffle(keys, N);
    rb_tree *T = make_tree();
    bst_insert_all(T, keys, N);

    rb_iter it = rb_iter_first(T);
    int key, want = 0;
    while (rb_iter_next(&it, &key)) {
        if (want >= 2 * N)
            printf("    FAILED walking %d keys: the iterator did not stop after the last key\n", N);
        else if (key != want)
            printf("    FAILED walking %d keys: key #%d is %d; expected %d\n", N, want / 2 + 1, key, want);
        if (want >= 2 * N || key != want) {
            rb_destroy(T);
            return false;
        }
        want += 2;
    }
    if (want != 2 * N) {
        printf("    FAILED walking %d keys: the iterator stopped after %d keys\n", N, want / 2);
        rb_destroy(T);
        return false;
    }

    for (int t = 0; t < 100000; t++) {
        int lo = (int)(rng_next() % (2 * N + 2)) - 1;   /* -1 .. 2N */
        int first = lo <= 0 ? 0 : lo + lo % 2;          /* the first even key >= lo */
        it = rb_iter_from(T, lo);
        bool got = rb_iter_next(&it, &key);
        if (first >= 2 * N ? got : (!got || key != first)) {
            if (first >= 2 * N)
                printf("    FAILED: rb_iter_from(T, %d) should have no keys; it started at %d\n", lo, key);
            else
                printf("    FAILED: rb_iter_from(T, %d) should start at %d\n", lo, first);
            rb_destroy(T);
            return false;
        }
    }
    rb_destroy(T);
    return true;
}

/* ---------------- registry and runner ---------------- */

typedef struct {
    const char *name;
    bool (*fn)(void);
    const char *what;
} test_case;

static const test_case TESTS[] = {
    {"rotate_left",          t_rotate_left,          "left rotation at the root and at an inner node"},
    {"rotate_right",         t_rotate_right,         "right rotation at the root and at an inner node"},
    {"insert_one",           t_insert_one,           "the first key becomes a black root"},
    {"insert_2134",          t_insert_2134,          "insert 2,1,3,4: exact keys and colors"},
    {"insert_1to7",          t_insert_1to7,          "insert 1..7: exact keys and colors"},
    {"insert_nine",          t_insert_nine,          "insert 7,5,9,3,6,8,10,1,2: exact keys and colors"},
    {"insert_valid_1000",    t_insert_valid_1000,    "valid after every insert of 1000 keys, 5 orders"},
    {"delete_nil_x",         t_delete_nil_x,         "delete where x is T->nil (the 24F bug case)"},
    {"delete_small",         t_delete_small,         "delete scenarios on small trees: exact keys and colors"},
    {"delete_nine",          t_delete_nine,          "deletes and an insert on the 9-key tree"},
    {"delete_all_valid",     t_delete_all_valid,     "valid after every delete of 500 keys, 5 orders"},
    {"rotate_refuses",       t_rotate_refuses,       "rotations return false when they cannot rotate"},
    {"rotate_keeps_colors",  t_rotate_keeps_colors,  "rotations do not change colors"},
    {"empty_contract",       t_empty_contract,       "empty-tree behavior"},
    {"delete_missing",       t_delete_missing,       "deleting a missing key returns false, tree unchanged"},
    {"duplicates",           t_duplicates,           "duplicate keys go right and delete one at a time"},
    {"height_bound",         t_height_bound,         "height <= 2 lg(n+1) for n up to 4095"},
    {"random_mixed",         t_random_mixed,         "20000 random inserts and deletes"},
    {"delete_root_repeat",   t_delete_root_repeatedly, "delete the root until the tree is empty"},
    {"delete_then_reinsert", t_delete_then_reinsert, "empty the tree and refill it, 3 rounds"},
    {"min_search",           t_min_search,           "rb_minimum and rb_search on a larger tree"},
    {"iter_empty",           t_iter_empty,           "the iterator on an empty tree"},
    {"iter_sorted",          t_iter_sorted,          "the iterator on the 9-key tree and a one-key tree"},
    {"iter_shapes",          t_iter_shapes,          "the iterator on paths, a zigzag, and random trees"},
    {"iter_duplicates",      t_iter_duplicates,      "the iterator and rb_iter_from with duplicate keys"},
    {"iter_from",            t_iter_from,            "rb_iter_from at many starting keys"},
    {"iter_changed",         t_iter_changed,         "an old iterator stops after an insert or delete"},
    {"iter_large",           t_iter_large,           "200000 keys: a full walk and 100000 rb_iter_from calls"},
};
#define NTESTS (sizeof TESTS / sizeof TESTS[0])

static int run_forked(const test_case *t)
{
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        alarm(TIME_LIMIT_SECONDS);
        exit(t->fn() ? 0 : 1);
    }
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
        return WEXITSTATUS(status) == 0;
    if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        if (sig == SIGALRM)
            printf("    FAILED: took longer than %d seconds (infinite loop?)\n", TIME_LIMIT_SECONDS);
        else
            printf("    FAILED: crashed with signal %d (%s)\n", sig, strsignal(sig));
    }
    return 0;
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc == 2 && strcmp(argv[1], "--list") == 0) {
        for (size_t i = 0; i < NTESTS; i++)
            printf("%-22s %s\n", TESTS[i].name, TESTS[i].what);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--inproc") == 0) {
        int failed = 0;
        for (size_t i = 0; i < NTESTS; i++)
            failed += !TESTS[i].fn();
        return failed != 0;
    }
    if (argc == 2) {
        for (size_t i = 0; i < NTESTS; i++)
            if (strcmp(argv[1], TESTS[i].name) == 0) {
                if (!TESTS[i].fn())
                    return 1;
                printf("passed\n");
                return 0;
            }
        fprintf(stderr, "no test named %s (try --list)\n", argv[1]);
        return 2;
    }
    int pass = 0;
    for (size_t i = 0; i < NTESTS; i++) {
        printf("%-22s %s\n", TESTS[i].name, TESTS[i].what);
        int ok = run_forked(&TESTS[i]);
        printf("    %s\n", ok ? "pass" : "FAIL");
        pass += ok;
    }
    printf("\nTests passed: %d/%zu\n", pass, NTESTS);
    printf("Gradescope also runs leak_check (valgrind), your three student tests,"
           " and a check that REPORT.md is filled in.\n");
    return pass == (int)NTESTS ? 0 : 1;
}
