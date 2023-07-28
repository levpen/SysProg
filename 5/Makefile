GCC_FLAGS = -Wextra -Werror -Wall -Wno-gnu-folding-constant -O0 -g -ggdb3

all: lib exe test

lib: chat.c chat_client.c chat_server.c buffer.c
	gcc $(GCC_FLAGS) -c chat.c -o chat.o
	gcc $(GCC_FLAGS) -c chat_client.c -o chat_client.o
	gcc $(GCC_FLAGS) -c chat_server.c -o chat_server.o
	gcc $(GCC_FLAGS) -c buffer.c -o buffer.o

exe: lib chat_client_exe.c chat_server_exe.c
	gcc $(GCC_FLAGS) chat_client_exe.c chat.o chat_client.o buffer.o -o client
	gcc $(GCC_FLAGS) chat_server_exe.c chat.o chat_server.o buffer.o -o server

test: lib
	gcc $(GCC_FLAGS) test.c chat.o chat_client.o chat_server.o buffer.o -o test 	\
		-I ../utils -lpthread

clean:
	rm *.o
	rm client server test
