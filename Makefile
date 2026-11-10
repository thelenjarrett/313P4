# CS 313 P4 Makefile
#   make           build test_rbtree, test_student, and rbdemo
#   make rbdemo    build the demo: ./rbdemo 41 38 31 draws your tree after each step
#   make check     run the autograder's tests on your rbtree.c
#   make test      build and run your three tests against your rbtree.c
#   make asan      run the autograder's tests with AddressSanitizer
#   make valgrind  run the leak scenario under valgrind (Linux; see SETUP.md)
#   make clean     remove built files

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -g

.PHONY: all check test asan valgrind clean

all: test_rbtree test_student rbdemo

test_rbtree: test_rbtree.c rbtree.c rbtree.h
	$(CC) $(CFLAGS) -o test_rbtree test_rbtree.c rbtree.c

test_student: test_student.c rbtree.c rbtree.h check.h
	$(CC) $(CFLAGS) -o test_student test_student.c rbtree.c

rbdemo: rbdemo.c rbtree.c rbtree.h
	$(CC) $(CFLAGS) -o rbdemo rbdemo.c rbtree.c

check: test_rbtree
	./test_rbtree

test: test_student
	./test_student

asan: test_rbtree.c rbtree.c rbtree.h
	$(CC) $(CFLAGS) -fsanitize=address,undefined -fno-sanitize-recover=undefined \
		-fno-omit-frame-pointer -o test_rbtree_asan test_rbtree.c rbtree.c
	./test_rbtree_asan

valgrind: test_rbtree
	valgrind --leak-check=full --error-exitcode=1 ./test_rbtree random_mixed

clean:
	rm -rf test_rbtree test_rbtree_asan test_student rbdemo *.dSYM
