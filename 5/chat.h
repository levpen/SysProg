#pragma once

#include <stddef.h>
#include "buffer.h"

#define NEED_AUTHOR 1
#define NEED_SERVER_FEED 1

enum chat_errcode {
	CHAT_ERR_INVALID_ARGUMENT = 1,
	CHAT_ERR_TIMEOUT,
	CHAT_ERR_PORT_BUSY,
	CHAT_ERR_NO_ADDR,
	CHAT_ERR_ALREADY_STARTED,
	CHAT_ERR_NOT_IMPLEMENTED,
	CHAT_ERR_NOT_STARTED,
	CHAT_ERR_SYS,
};

enum chat_events {
	CHAT_EVENT_INPUT = 1,
	CHAT_EVENT_OUTPUT = 2,
};

struct chat_message {
#if NEED_AUTHOR
	/** Author's name. */
	const char *author;
#endif
	/** 0-terminate text. */
	char *data;

	/* PUT HERE OTHER MEMBERS */
	struct chat_message *prev;
};

/** Create message with data. */
struct chat_message *chat_message_new(const char *author, const char *data);

/** Free message's memory. */
void
chat_message_delete(struct chat_message *msg);

/** Free message queue. */
void chat_message_freee_queue(struct chat_message *msg_front);

/** Push message to messages queue. */
void chat_message_push(struct chat_message **msg_front, struct chat_message **msg_back, struct chat_message *msg_new);

/** Push message to messages queue. */
// void
// chat_messages_push(struct chat_message **msg_front, struct chat_message **msg_back, struct buffer *buf);

/** Pop message from messages and delete it. */
struct chat_message *
chat_message_pop(struct chat_message **msg_front, struct chat_message **msg_back);

/** Convert chat_events mask to events suitable for poll(). */
int
chat_events_to_poll_events(int mask);
