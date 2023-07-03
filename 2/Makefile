GCC_FLAGS = -Wextra -Werror -Wall -Wno-gnu-folding-constant -g

all: solution.c parser.c command.c
	gcc $(GCC_FLAGS) -O0 -g -s solution.c parser.c command.c
	./a.out 
clean:
	rm a.out
