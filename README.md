	# P4: Red-black tree

CS 313, Fall 2026. Posted Tue 11/10.

- **First step (in lab Tue 11/10, or on your own):** fill in `rb_left_rotate` and `rb_right_rotate`. Submit `rbtree.c` to Gradescope, and check that the `rotate_left` and `rotate_right` tests pass. This makes sure your setup and submission work well before the deadline.
- **Due:** Fri 12/4, 11:59pm. Free extension to Sat 12/5, 11:59pm.
- **Language:** C. You may use AI tools as a tutor when you get stuck, but you need to understand your code: you will explain it in `REPORT.md`.
- **Set up first:** follow `SETUP.md`, as for P1 to P3.
- **Read first:** CLRS 4th edition, sections 13.2 (rotations), 13.3 (insertion), and 13.4 (deletion).

## What you write

You are given a working BST with colors. You write the four functions that keep it balanced, `rb_left_rotate`, `rb_right_rotate`, `rb_insert_fixup`, and `rb_delete_fixup`, and an iterator: three short functions that hand out the keys in sorted order, one key per call.

The code follows CLRS line by line. It uses one sentinel node, `T->nil`, for every NIL leaf and for the root's parent. `T->nil` is black. Always compare with `T->nil`, never with `NULL`. Colors are the enum values `RB_RED` and `RB_BLACK`.

## Files

| File | What to do |
|---|---|
| `rbtree.h` | The API and the rules. Read it. Do not edit it. |
| `rbtree.c` | Fill in the seven functions at the bottom. **Submit.** |
| `test_student.c` | Write three tests (below). **Submit.** |
| `REPORT.md` | Answer two short questions about your code. **Submit.** |
| `test_rbtree.c` | The autograder's tests, for local runs. Do not edit. |
| `check.h` | Tiny test framework for `test_student.c`. Do not edit. |
| `rbdemo.c` | A demo that draws your tree after each step (below). Do not submit. |
| `SETUP.md` | How to set up gcc, make, and valgrind. Same as P1 to P3. |
| `.devcontainer/` | Lets GitHub Codespaces set up everything for you (Option C in `SETUP.md`). |
| `Makefile` | `make`, `make check`, `make test`, `make rbdemo`, `make asan`, `make valgrind`, `make clean` |

## The API

Provided and working: `rb_create`, `rb_destroy`, `rb_size`, `rb_search`, `rb_contains`, `rb_minimum`, `rb_insert`, `rb_bst_insert`, `rb_delete`, `rb_preorder`, `rb_height`, `rb_is_valid`, and `rb_print`. See `rbtree.h` for one line on each.

You write:

- `rb_left_rotate(T, x)` and `rb_right_rotate(T, x)`: CLRS LEFT-ROTATE and its mirror. They return `false` and change nothing if `x` is `T->nil` or has no child to rotate up.
- `rb_insert_fixup(T, z)`: CLRS RB-INSERT-FIXUP. `rb_insert` calls it.
- `rb_delete_fixup(T, x)`: CLRS RB-DELETE-FIXUP. `rb_delete` calls it. `x` may be `T->nil`. In that case `rb_delete` has already set `T->nil->p`, exactly as in CLRS.
- `rb_iter_first`, `rb_iter_from`, and `rb_iter_next`: the iterator (next section).

**Rules.**

- Duplicate keys are allowed. As in CLRS, an equal key goes into the right subtree.
- Operations that can fail return a `bool`. `rb_delete` of a missing key and `rb_minimum` on an empty tree return `false` and change nothing. Nothing prints, exits, or crashes.
- `rb_search` returns `T->nil` when the key is absent.
- `rb_preorder(T, keys, colors, cap)` writes at most `cap` nodes into arrays you provide, and returns the number of nodes in the tree.

**Debugging aids.** Call `rb_is_valid(T, &why)` after any step. It checks BST order, parent pointers, and all five red-black properties, and `why` names the first problem. `rb_print(T)` draws the tree sideways with R and B after each key, and marks any node whose parent pointer is wrong. `rb_bst_insert` inserts with no fixup, so you can build any shape to test rotations.

## The iterator

An iterator hands out a tree's keys in sorted order, one key per call. The caller runs the loop and can stop at any time, and no array is needed:

```c
rb_iter it = rb_iter_first(T);
int key;
while (rb_iter_next(&it, &key))
    printf("%d\n", key);
```

An `rb_iter` holds the tree, one node (the node whose key comes next, or `T->nil` when none are left), and the tree's version when the iterator was made. You write three functions.

- `rb_iter_first(T)`: an iterator that starts at the smallest key.
- `rb_iter_from(T, lo)`: an iterator that starts at the first key ≥ `lo`. Find it in one walk down from the root, so it takes O(lg n) time. With duplicates, start at the first copy. A rotation can put a copy of a key in the left subtree of another copy, so do not stop at the first match.
- `rb_iter_next(&it, &key)`: if the tree has changed since `it` was made, return `false` and leave `key` alone (see below). Otherwise, if a key is left, put it in `key`, move `it` to the next node, and return `true`. Otherwise return `false` and leave `key` alone. Find the next node with the two cases of CLRS TREE-SUCCESSOR and the parent pointers, as in P3, with `T->nil` in place of NIL. The provided `tree_minimum` in `rbtree.c` may help.

`rb_iter_from` makes range queries fast. This loop counts the keys in [lo, hi) in O(lg n + k) time, where k is the count:

```c
rb_iter it = rb_iter_from(T, lo);
int key, k = 0;
while (rb_iter_next(&it, &key) && key < hi)
    k++;
```

**When the tree changes.** An insert or a delete can free or move the node an iterator points to. Reading a freed node is a *use-after-free* bug, a common cause of crashes and security holes in C. So the tree keeps a version number, `T->version`, which goes up by 1 with every insert and delete. An iterator records the version when it is made, and the provided `rb_iter_changed(&it)` compares the two. Once the tree has changed, `rb_iter_next` must return `false` without reading `it->next`. To keep going, make a new iterator. Java's and C#'s collections use the same check (`TreeMap` and `SortedDictionary` are red-black trees), and they throw an exception. C has no exceptions, so this follows `fgetc`: it returns `EOF` both at the end and on an error, and `feof` and `ferror` tell the two apart. Here, `false` means "stop," and `rb_iter_changed` tells you whether the tree changed.

The iterator does not depend on your other four functions. Its tests build trees with `rb_bst_insert`, so you can write it first or last.

## Your three tests

In `test_student.c`, write three tests. Each one builds a tree and checks its preorder with `CHECK_TREE(T, "38B 19R 12B 8R 31B 41B")`. Work out the string **by hand** first, drawing the tree after each step. The string must be a literal, not computed.

The autograder runs your tests against the instructor's `rbtree.c`. A test passes if all its checks pass there and one `CHECK_TREE` has at least 4 nodes. So a wrong hand trace fails. Run `make test` to try them on your own code.

## The report

`REPORT.md` asks two short questions: how your `rb_insert_fixup` handles a red node with a red parent, and why walking the whole tree with your iterator takes O(n) time. Answer in your own words, about your own code. Replace every `____`.

## See your tree

When something is wrong, look at the tree. The demo runs your code and draws the tree after every step:

```
make rbdemo
./rbdemo 41 38 31 12 19 8          # insert each key; your fix-up runs
./rbdemo 41 38 31 -38              # a minus sign deletes; your delete fix-up runs
./rbdemo b60 b20 b80 b10 b40 L20   # b: plain BST insert; L or R: rotate at that key
```

The drawing is sideways: the root is at the left margin, right subtrees are above, and left subtrees are below. Tilt your head to the left. R is red and B is black, and `<--` marks a node whose parent pointer is wrong. Under each drawing, the demo says whether the red-black rules hold, and if not, which rule breaks first. Draw the same steps by hand and compare.

A failing test also draws the tree you built and, where it can, the tree it expected. Gradescope runs exactly the tests that `make check` runs, so debug here, where a run takes seconds, and submit when the tests pass.

## Test locally

- `make check` runs the autograder's tests on your `rbtree.c`. Each test runs in its own process with a 10-second limit. A failing test prints what it expected. Unless every test passes, `make` ends with an error line such as `make: *** [check] Error 1`.
- `./test_rbtree insert_2134` runs one test. Use this form under a debugger. `./test_rbtree --list` lists the tests.
- `make test` runs your three tests.
- `make asan` runs the tests with AddressSanitizer, which reports memory errors with a line number.
- `make valgrind` checks for leaks, if you have valgrind.

Write the rotations first. The tests `rotate_left` and `rotate_right` check them alone. Then write the insert fixup, then the delete fixup.

## Grading

P4 is worth 10 points, the same as each of the other programming assignments. Submit `rbtree.c`, `test_student.c`, and `REPORT.md`. The autograder uses its own copies of `rbtree.h`, `test_rbtree.c`, and `check.h`, so do not change them.

- **8 points from the autograder.** The points are split evenly across all the tests, and each passing test earns its share. The tests check rotations, insert sequences with exact keys and colors, validity after every insert of 1000 keys, delete scenarios with exact keys and colors, validity after every delete, empty-tree behavior, duplicates, the height bound, random mixed operations, the iterator (an empty tree, sorted order, many shapes, duplicates, starting keys, a changed tree, and a walk over 200,000 keys), no memory leaks (under `valgrind`), your three hand-traced tests, and that `REPORT.md` is filled in. Gradescope shows every test and its message, and you may resubmit as often as you like.
- **2 points graded by the course staff:** readable code (clear names, consistent indentation, comments where the logic is not obvious, no leftover debugging output), the required methods (the rotations and fixups follow CLRS, and the fixups use your rotations; `rb_iter_from` walks down from the root once; `rb_iter_next` uses the two CLRS successor cases with parent pointers; and no function copies the keys into an array or starts over from the root), and correct explanations in `REPORT.md`.

Working code is expected: if you are stuck, office hours, the Canvas discussion, and AI tools can all help you get there.

The autograder runs your code with AddressSanitizer. If a test says `heap-buffer-overflow` or `SEGV`, the lines below it name the function and line number.
