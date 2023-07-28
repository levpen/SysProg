#include "chat.h"
#include "chat_client.h"
#include "buffer.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>

struct chat_client {
	/** Socket connected to the server. */
	int socket;
	/** Array of received messages. */
	struct chat_message *msgs_front;
	struct chat_message *msgs_back;
	/** Output buffer. */
	struct buffer *read_buf;
	struct buffer *send_buf;

	char *name;
};

struct chat_client *
chat_client_new(const char *name)
{
	/* Ignore 'name' param if don't want to support it for +5 points. */

	struct chat_client *client = calloc(1, sizeof(*client));
	client->name = strdup(name);
	
	client->msgs_front = NULL;
	client->msgs_back = NULL;

	client->read_buf = buffer_create();
	client->send_buf = buffer_create();
	client->socket = -1;

	return client;
}

void
chat_client_delete(struct chat_client *client)
{
	if (client->socket >= 0)
		close(client->socket);
	
	free(client->name);
	buffer_delete(client->read_buf);
	buffer_delete(client->send_buf);
	
	chat_message_freee_queue(client->msgs_front);

	free(client);
}

int
chat_client_connect(struct chat_client *client, const char *addr)
{
	/*
	 * 1) Use getaddrinfo() to resolve addr to struct sockaddr_in.
	 * 2) Create a client socket (function socket()).
	 * 3) Connect it by the found address (function connect()).
	 */
	if(client->socket != -1)
		return CHAT_ERR_ALREADY_STARTED;
	struct addrinfo *sock_addr = NULL;
	struct addrinfo filter;
	memset(&filter, 0, sizeof(filter));
	filter.ai_family = AF_INET;
	filter.ai_socktype = SOCK_STREAM;
	char *port = strdup(addr);
	char *node = strsep(&port, ":");

	int rc = getaddrinfo(node, port, &filter, &sock_addr);
	free(node);
	if(rc) {
		return CHAT_ERR_SYS;
	}
	if(sock_addr == NULL) {
		return CHAT_ERR_NO_ADDR;
	}
	client->socket = socket(sock_addr->ai_family, sock_addr->ai_socktype, sock_addr->ai_protocol);
	if(client->socket == -1) {
		return CHAT_ERR_SYS;
	}
	connect(client->socket, sock_addr->ai_addr, sock_addr->ai_addrlen);
	fcntl(client->socket, F_SETFL, fcntl(client->socket, F_GETFL, 0) | O_NONBLOCK);

	freeaddrinfo(sock_addr);

	chat_client_feed(client, client->name, strlen(client->name) + 1);
	return 0;
}

struct chat_message *
chat_client_pop_next(struct chat_client *client)
{	
	return chat_message_pop(&client->msgs_front, &client->msgs_back);
}


static int _chat_client_process_message(struct chat_client* client) {
	
    char* message = client->read_buf->buf + client->read_buf->start;
    client->read_buf->start += strlen(message) + 1;

    char* author = strsep(&message, "\n");
    if (message == NULL) {
        // bad message from server
        return 0;
    }

    chat_message_push(&client->msgs_front, &client->msgs_back, chat_message_new(author, message));
    return 0;
}



int
chat_client_update(struct chat_client *client, double timeout)
{
	/*
	 * The easiest way to wait for updates on a single socket with a timeout
	 * is to use poll(). Epoll is good for many sockets, poll is good for a
	 * few.
	 *
	 * You create one struct pollfd, fill it, call poll() on it, handle the
	 * events (do read/write).
	 */
	if(client->socket == -1) {
		return CHAT_ERR_NOT_STARTED;
	}

	struct pollfd poll_fd;
	poll_fd.fd = client->socket;
	poll_fd.events = chat_events_to_poll_events(chat_client_get_events(client));

	int code = poll(&poll_fd, 1, (int)(timeout * 1000.0));
	if(code == 0) {
		return CHAT_ERR_TIMEOUT;
	} else if(code == -1) {
		return CHAT_ERR_SYS;
	}

	if (poll_fd.revents & POLLIN) {
		// printf("wtf\n");
        int code = buffer_recv(client->read_buf, client->socket);
		if(code == -1)
			return CHAT_ERR_SYS;

		while(buffer_has_msg(client->read_buf) != 0)
			_chat_client_process_message(client);

		if (code == -2) {
			// disconnect
            close(client->socket);
            client->socket = -1;
        }
    }
	if ((poll_fd.revents & POLLOUT) != 0) {
        int code = buffer_send(client->send_buf, client->socket);
		if(code == -1)
			return CHAT_ERR_SYS;
    }
	
	return 0;
}

int
chat_client_get_descriptor(const struct chat_client *client)
{
	return client->socket;
}

int
chat_client_get_events(const struct chat_client *client)
{
	/*
	 * IMPLEMENT THIS FUNCTION - add OUTPUT event if has non-empty output
	 * buffer.
	 */
	if(client->socket == -1)
		return 0;
	return CHAT_EVENT_INPUT | (client->send_buf->sz > client->send_buf->start ? CHAT_EVENT_OUTPUT : 0);
}

int
chat_client_feed(struct chat_client *client, const char *msg, uint32_t msg_size)
{
	if(client->socket == -1) {
		return CHAT_ERR_NOT_STARTED;
	}
	buffer_push(client->send_buf, msg, msg_size);
	// printf("pushed to buf\n");
	for(size_t i = client->send_buf->start; i < client->send_buf->sz; ++i) {
		if(client->send_buf->buf[i] == '\n')
			client->send_buf->buf[i] = '\0';
	}
	// printf("pushed to buf: %s\n", client->send_buf->buf);
	return 0;
}
