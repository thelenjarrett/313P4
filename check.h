/*
 * check.h -- tiny test framework for test_student.c. Do not edit.
 * The autograder uses its own copy.
 *
 *   CHECK(condition)            fails the test if condition is false.
 *   CHECK_TREE(T, "expected")   fails the test unless T's PREORDER matches
 *                               the string literal you worked out by hand.
 *
 * The expected string lists the nodes in preorder, separated by spaces.
 * Each node is its key followed by its color: B for black, R for red.
 *     CHECK_TREE(T, "7B 5R 9R");
 * Leave off the colors to check keys only (for rotation tests):
 *     CHECK_TREE(T, "7 5 9");
 * Use "" for an empty tree.
 *
 * A test passes if every check passes AND it contains at least one
 * CHECK_TREE with 4 or more nodes.
 *
 * Run all three tests:  ./test_student
 * Run one test:         ./test_student 2
 */
#ifndef CHECK_H
#define CHECK_H

#include <stdio.h>
#include <stdlib.h>
#include "rbtree.h"

static int check_count = 0;
static int check_failures = 0;
static int check_tree_big = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        check_count++;                                                  \
        if (!(cond)) {                                                  \
            printf("    FAIL line %d: CHECK(%s)\n", __LINE__, #cond);   \
            check_failures++;                                           \
        }                                                               \
    } while (0)

#define CHECK_TREE(T, expected) \
    check_tree_impl((T), (expected), #expected, __LINE__)

static inline void check_tree_print_actual(const rb_tree *T, bool colors)
{
    size_t n = rb_size(T);
    int *keys = malloc((n + 1) * sizeof *keys);
    rb_color *c = malloc((n + 1) * sizeof *c);
    size_t m = rb_preorder(T, keys, c, n + 1);
    if (m > n + 1)
        m = n + 1;
    printf("\"");
    for (size_t i = 0; i < m; i++) {
        printf("%s%d", i ? " " : "", keys[i]);
        if (colors)
            printf("%c", c[i] == RB_RED ? 'R' : 'B');
    }
    printf("\"");
    free(keys);
    free(c);
}

static inline void check_tree_impl(const rb_tree *T, const char *expected,
                                   const char *text, int line)
{
    check_count++;
    if (text[0] != '"') {
        printf("    FAIL line %d: CHECK_TREE needs a string literal you worked"
               " out by hand, not %s\n", line, text);
        check_failures++;
        return;
    }
    if (T == NULL) {
        printf("    FAIL line %d: tree is NULL\n", line);
        check_failures++;
        return;
    }
    size_t n = rb_size(T);
    int *keys = malloc((n + 1) * sizeof *keys);
    rb_color *c = malloc((n + 1) * sizeof *c);
    size_t m = rb_preorder(T, keys, c, n + 1);
    if (m > n + 1)              /* more nodes than rb_size says: compare what fits */
        m = n + 1;

    const char *p = expected;
    size_t i = 0;
    bool ok = true, parse_error = false, colors = false;
    while (ok) {
        while (*p == ' ')
            p++;
        if (*p == '\0')
            break;
        char *end;
        long k = strtol(p, &end, 10);
        if (end == p) {
            parse_error = true;
            break;
        }
        p = end;
        int want = -1;
        if (*p == 'B' || *p == 'R') {
            want = (*p == 'R') ? RB_RED : RB_BLACK;
            colors = true;
            p++;
        }
        if (*p != ' ' && *p != '\0') {
            parse_error = true;
            break;
        }
        if (i >= m || keys[i] != k || (want >= 0 && (int)c[i] != want))
            ok = false;
        i++;
    }
    if (parse_error) {
        printf("    FAIL line %d: cannot read %s; write nodes like \"7B 5R 9R\"\n",
               line, text);
        check_failures++;
    } else if (!ok || i != m) {
        printf("    FAIL line %d: you expected %s, the tree's preorder is ", line, text);
        check_tree_print_actual(T, colors);
        printf("\n");
        check_failures++;
    } else if (i >= 4) {
        check_tree_big++;
    }
    free(keys);
    free(c);
}

void student_test_1(void);
void student_test_2(void);
void student_test_3(void);

int main(int argc, char **argv)
{
    void (*tests[3])(void) = {student_test_1, student_test_2, student_test_3};
    int first = 1, last = 3, all_pass = 1;

    if (argc > 1) {
        first = last = atoi(argv[1]);
        if (first < 1 || first > 3) {
            printf("usage: %s [1|2|3]\n", argv[0]);
            return 2;
        }
    }
    for (int t = first; t <= last; t++) {
        check_count = check_failures = check_tree_big = 0;
        printf("student_test_%d:\n", t);
        tests[t - 1]();
        int pass = check_failures == 0 && check_tree_big >= 1;
        if (check_failures == 0 && check_tree_big == 0)
            printf("    FAIL: no CHECK_TREE with 4 or more nodes passed\n");
        printf("    %s (%d checks)\n", pass ? "PASS" : "FAIL", check_count);
        fflush(stdout);
        all_pass = all_pass && pass;
    }
    return all_pass ? 0 : 1;
}

#endif
