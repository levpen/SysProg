#define _GNU_SOURCE
#include "chat.h"
#include "chat_server.h"
#include "buffer.h"

#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>


struct chat_peer {
	/** Client's socket. To read/write messages. */
	int socket;
	/** Output buffer. */
	struct buffer *read_buf;
	struct buffer *send_buf;

	char *name;
	struct epoll_event event;
};

struct chat_server {
	/** Listening socket. To accept new clients. */
	int socket;
	/** Array of peers. */
	struct chat_peer** peers;
	size_t peers_count;

	int epoll;
	/** Message queue. */
	struct chat_message *msgs_front;
	struct chat_message *msgs_back;

	struct buffer *server_buf;
};

struct chat_server *
chat_server_new(void)
{
	struct chat_server *server = calloc(1, sizeof(*server));
	server->socket = -1;
	server->peers = NULL;
	server->peers_count = 0;
	server->epoll = -1;

	server->msgs_front = NULL;
	server->msgs_back = NULL;

	server->server_buf = buffer_create();

	return server;
}

static void _chat_peer_delete(struct chat_peer *peer) {
	// printf("deleting peer\n");
	close(peer->socket);
	free(peer->name);
	buffer_delete(peer->read_buf);
	buffer_delete(peer->send_buf);
	free(peer);
}

void
chat_server_delete(struct chat_server *server)
{
	if (server->socket >= 0)
		close(server->socket);
	if(server->epoll >= 0)
		close(server->epoll);
	for(size_t i = 0; i < server->peers_count; ++i)
		_chat_peer_delete(server->peers[i]);
	free(server->peers);
	
	chat_message_freee_queue(server->msgs_front);
	
	buffer_delete(server->server_buf);

	free(server);
}

int
chat_server_listen(struct chat_server *server, uint16_t port)
{
	/*
	 * 1) Create a server socket (function socket()).
	 * 2) Bind the server socket to addr (function bind()).
	 * 3) Listen the server socket (function listen()).
	 * 4) Create epoll/kqueue if needed.
	 */
	if(server->socket != -1) {
		return CHAT_ERR_ALREADY_STARTED;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_port = htons(port);
	/* Listen on all IPs of this machine. */
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	
	server->socket = socket(AF_INET, SOCK_STREAM, 0);
	if(server->socket == -1) {
		return CHAT_ERR_SYS;
	}

	server->epoll = epoll_create(1);
	if(server->epoll == -1) {
		return CHAT_ERR_SYS;
	}

	int enable = 1;
	if(setsockopt(server->socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int))) {
		return CHAT_ERR_SYS;
	}

	if(bind(server->socket, (struct sockaddr *)&addr, sizeof(addr))){
		if(errno == EADDRINUSE) {
			return CHAT_ERR_PORT_BUSY;
		}
		return CHAT_ERR_SYS;
	}

	if(listen(server->socket, 30)) {
		return CHAT_ERR_SYS;
	}

	if(fcntl(server->socket, F_SETFL, fcntl(server->socket, F_GETFL, 0) | O_NONBLOCK)) {
		return CHAT_ERR_SYS;
	}

	struct epoll_event event;
	event.events = EPOLLIN;
	event.data.ptr = NULL;
	if(epoll_ctl(server->epoll, EPOLL_CTL_ADD, server->socket, &event)) {
		return CHAT_ERR_SYS;
	}

	return 0;
}

struct chat_message *
chat_server_pop_next(struct chat_server *server)
{
	return chat_message_pop(&server->msgs_front, &server->msgs_back);
}

static void _chat_peer_accept(struct chat_server *server) {
	// printf("accepting peer\n");
	int socket_fd = accept4(server->socket, NULL, NULL, SOCK_NONBLOCK);

	server->peers = realloc(server->peers, (server->peers_count + 1) * sizeof(struct chat_peer*));
	// printf("%ld\n", server->peers_count);

	server->peers[server->peers_count] = malloc(sizeof(struct chat_peer));
	server->peers[server->peers_count]->socket = socket_fd;
	server->peers[server->peers_count]->read_buf = buffer_create();
	server->peers[server->peers_count]->send_buf = buffer_create();
	server->peers[server->peers_count]->name = NULL;
	server->peers[server->peers_count]->event.events = EPOLLIN;
	server->peers[server->peers_count]->event.data.ptr = server->peers[server->peers_count];
	epoll_ctl(server->epoll, EPOLL_CTL_ADD, socket_fd, &server->peers[server->peers_count]->event);
	// fcntl(server->socket, F_SETFL, fcntl(server->socket, F_GETFL, 0) | O_NONBLOCK)
	server->peers_count++;
}

static void _chat_server_process_msg(struct chat_server *server, struct chat_peer *peer) {
	char* msg = peer->read_buf->buf + peer->read_buf->start;
    size_t msg_sz = strlen(msg) + 1;
	peer->read_buf->start += msg_sz;

	if (peer->name == NULL) {
        peer->name = strdup(msg);
        return;
    }

	chat_message_push(&server->msgs_front, &server->msgs_back, chat_message_new(peer->name, msg));

	for (size_t i = 0; i < server->peers_count; ++i) {
        struct chat_peer* cur_peer = server->peers[i];
        if (cur_peer == peer)
            continue;
		
		//printf()
        buffer_push(cur_peer->send_buf, peer->name, strlen(peer->name));
        buffer_push(cur_peer->send_buf, "\n", 1);
        buffer_push(cur_peer->send_buf, msg, msg_sz);
        if ((cur_peer->event.events & EPOLLOUT) == 0) {
			cur_peer->event.events |= EPOLLOUT;
			epoll_ctl(server->epoll, EPOLL_CTL_MOD, cur_peer->socket, &cur_peer->event);
		}
    }
}

int
chat_server_update(struct chat_server *server, double timeout)
{
	/*
	 * 1) Wait on epoll/kqueue/poll for update on any socket.
	 * 2) Handle the update.
	 * 2.1) If the update was on listen-socket, then you probably need to
	 *     call accept() on it - a new client wants to join.
	 * 2.2) If the update was on a client-socket, then you might want to
	 *     read/write on it.
	 */
	if (server->socket == -1) {
        return CHAT_ERR_NOT_STARTED;
    }


	struct epoll_event event;
	int events = epoll_wait(server->epoll, &event, 1, (int)(timeout*1000));

	if (events == 0) {
        return CHAT_ERR_TIMEOUT;
    }
    if (events == -1) {
        return CHAT_ERR_SYS;
    }
	
	struct chat_peer *peer = event.data.ptr;
	if(peer == NULL) {
		_chat_peer_accept(server);
		return 0;
	}

	if(event.events & EPOLLIN) {
		int code = buffer_recv(peer->read_buf, peer->socket);
		if(code == -1) return CHAT_ERR_SYS;

		while(buffer_has_msg(peer->read_buf))
			_chat_server_process_msg(server, peer);


		if(code == -2) {
			//client disconnected
			epoll_ctl(server->epoll, EPOLL_CTL_DEL, peer->socket, NULL);

			for(size_t i = 0; i < server->peers_count-1; ++i) {
				if (server->peers[i] == peer) {
                    server->peers[i] = server->peers[server->peers_count];
                    break;
                }
			}
			--server->peers_count;
			_chat_peer_delete(peer);
			return 0;
		}
	}
	if (event.events & EPOLLOUT) {
        int result = buffer_send(peer->send_buf, peer->socket);
        if (result == -1) {
            return CHAT_ERR_SYS;
        }
        if (result == 0) {
            peer->event.events &= ~EPOLLOUT;
            if (epoll_ctl(server->epoll, EPOLL_CTL_MOD, peer->socket,
                          &peer->event) == -1) {
                return CHAT_ERR_SYS;
            }
        }
    }

	return 0;
}

int
chat_server_get_descriptor(const struct chat_server *server)
{
#if NEED_SERVER_FEED
	/* IMPLEMENT THIS FUNCTION if want +5 points. */

	/*
	 * Server has multiple sockets - own and from connected clients. Hence
	 * you can't return a socket here. But if you are using epoll/kqueue,
	 * then you can return their descriptor. These descriptors can be polled
	 * just like sockets and will return an event when any of their owned
	 * descriptors has any events.
	 *
	 * For example, assume you created an epoll descriptor and added to
	 * there a listen-socket and a few client-sockets. Now if you will call
	 * poll() on the epoll's descriptor, then on return from poll() you can
	 * be sure epoll_wait() can return something useful for some of those
	 * sockets.
	 */
#endif
	return server->epoll;
}

int
chat_server_get_socket(const struct chat_server *server)
{
	return server->socket;
}

int
chat_server_get_events(const struct chat_server *server)
{
	/*
	 * IMPLEMENT THIS FUNCTION - add OUTPUT event if has non-empty output
	 * buffer in any of the client-sockets.
	 */
	if(server->socket == -1)
		return 0;
	int mask = CHAT_EVENT_INPUT;
	for (size_t i = 0; i < server->peers_count; ++i) {
        struct chat_peer* peer = server->peers[i];
        if (peer->send_buf->start < peer->send_buf->sz) {
            mask |= CHAT_EVENT_OUTPUT;
            break;
        }
    }
	return mask;
}

int
chat_server_feed(struct chat_server *server, const char *msg, uint32_t msg_size)
{
#if NEED_SERVER_FEED
	/* IMPLEMENT THIS FUNCTION if want +5 points. */
#endif
	if(server->socket == -1) {
		return CHAT_ERR_NOT_STARTED;
	}
	buffer_push(server->server_buf, msg, msg_size);
    for (size_t i = server->server_buf->start; i < server->server_buf->sz; ++i) {
        if(server->server_buf->buf[i] == '\n')
            server->server_buf->buf[i] = '\0';
    }

    while (buffer_has_msg(server->server_buf)) {
        char* msg = server->server_buf->buf + server->server_buf->start;
        size_t msg_size = strlen(msg) + 1;
        server->server_buf->start += msg_size;

        for (size_t i = 0; i < server->peers_count; ++i) {
            struct chat_peer* peer = server->peers[i];

            buffer_push(peer->send_buf, "server\n", 7);
            buffer_push(peer->send_buf, msg, msg_size);
            if ((peer->event.events & EPOLLOUT) == 0) {
				peer->event.events |= EPOLLOUT;
				epoll_ctl(server->epoll, EPOLL_CTL_MOD, peer->socket, &peer->event);
            }
        }
    }

    return 0;
}
