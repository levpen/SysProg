#include "chat.h"
#include "chat_server.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <poll.h>

#include <unistd.h>

static int
port_from_str(const char *str, uint16_t *port)
{
	errno = 0;
	char *end = NULL;
	long res = strtol(str, &end, 10);
	if (res == 0 && errno != 0)
		return -1;
	if (*end != 0)
		return -1;
	if (res > UINT16_MAX || res < 0)
		return -1;
	*port = (uint16_t)res;
	return 0;
}

int
main(int argc, char **argv)
{
	if (argc < 2) {
		printf("Expected a port to listen on\n");
		return -1;
	}
	uint16_t port = 0;
	int rc = port_from_str(argv[1], &port);
	if (rc != 0) {
		printf("Invalid port\n");
		return -1;
	}
	struct chat_server *serv = chat_server_new();
	rc = chat_server_listen(serv, port);
	if (rc != 0) {
		printf("Couldn't listen: %d\n", rc);
		chat_server_delete(serv);
		return -1;
	}
#if NEED_SERVER_FEED
	/*
	 * If want +5 points, then do similarly to the client_exe - create 2
	 * pollfd objects, wait on STDIN_FILENO and on
	 * chat_server_get_descriptor(), process events, etc.
	 */
	struct pollfd poll_fds[2];
	memset(poll_fds, 0, sizeof(poll_fds));

	struct pollfd *poll_input = &poll_fds[0];
	poll_input->fd = STDIN_FILENO;
	poll_input->events = POLLIN;


	const int buf_size = 1024;
    char buf[buf_size];


	struct pollfd *poll_client = &poll_fds[1];
	poll_client->fd = chat_server_get_descriptor(serv);
	assert(poll_client->fd >= 0);
	while (true) {
		poll_client->events =
			chat_events_to_poll_events(chat_server_get_events(serv));

		int rc = poll(poll_fds, 2, -1);
		if (rc < 0) {
			printf("Poll error: %d\n", errno);
			break;
		}
		if (poll_input->revents != 0) {
			poll_input->revents = 0;
			rc = read(STDIN_FILENO, buf, buf_size - 1);
			if (rc == 0) {
				printf("EOF - exiting\n");
				break;
			}
			rc = chat_server_feed(serv, buf, rc);
			if (rc != 0) {
				printf("Feed error: %d\n", rc);
				break;
			}
		}
		if (poll_client->revents != 0) {
			poll_client->revents = 0;
			rc = chat_server_update(serv, 0);
			if (rc != 0 && rc != CHAT_ERR_TIMEOUT) {
				printf("Update error: %d\n", rc);
				break;
			}
		}
		/* Flush all the pending messages to the standard output. */
		struct chat_message *msg;
		while ((msg = chat_server_pop_next(serv)) != NULL) {
#if NEED_AUTHOR
			printf("%s: %s\n", msg->author, msg->data);
#else
			printf("%s\n", msg->data);
#endif
			chat_message_delete(msg);
		}
	}
#else
	/*
	 * The basic implementation without server messages. Just serving
	 * clients.
	 */
	while (true) {
		int rc = chat_server_update(serv, -1);
		if (rc != 0) {
			printf("Update error: %d\n", rc);
			break;
		}
		/* Flush all the pending messages to the standard output. */
		struct chat_message *msg;
		while ((msg = chat_server_pop_next(serv)) != NULL) {
#if NEED_AUTHOR
			printf("%s: %s\n", msg->author, msg->data);
#else
			printf("%s\n", msg->data);
#endif
			chat_message_delete(msg);
		}
	}
#endif
	chat_server_delete(serv);
	return 0;
}
