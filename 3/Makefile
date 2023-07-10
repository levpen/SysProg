GCC_FLAGS = -Wextra -Werror -Wall -Wno-gnu-folding-constant -O0 -g -ggdb3

all: test.o userfs.o
	gcc $(GCC_FLAGS) test.o userfs.o

test.o: test.c
	gcc $(GCC_FLAGS) -c test.c -o test.o -I ../utils

userfs.o: userfs.c
	gcc $(GCC_FLAGS) -c userfs.c -o userfs.o
