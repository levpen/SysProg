#include "chat.h"

#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "buffer.h"

void chat_message_delete(struct chat_message *msg)
{
	free((void *)msg->author);
	free(msg->data);
	free(msg);
}

struct chat_message *chat_message_new(const char *author, const char *data) {
	struct chat_message *new_msg = malloc(sizeof(struct chat_message));
	// printf("msg new: %p\n", new_msg);
	// printf("!!!%s\n", data);
	new_msg->author = strdup(author);
	new_msg->data = strdup(data);
	new_msg->prev = NULL;
	return new_msg;
}

void chat_message_freee_queue(struct chat_message *msg_front) {
	while(msg_front != NULL){
		struct chat_message *tmp = msg_front->prev;
		chat_message_delete(msg_front);
		msg_front = tmp;
	}
}

void chat_message_push(struct chat_message **msg_front, struct chat_message **msg_back, struct chat_message *msg_new) {
	if(*msg_back == NULL) {
		*msg_front = msg_new;
		*msg_back = *msg_front;
	} else {
		(*msg_back)->prev = msg_new;
		*msg_back = (*msg_back)->prev;
	}
}

// void chat_messages_push(struct chat_message **msg_front, struct chat_message **msg_back, struct buffer *buf)
// {
// 	printf("push msgs\n");
// 	char *data = buf->buf+buf->start;
// 	size_t data_sz = buf->sz;
// 	// printf("push msg: %s\n", data);
// 	// printf("front: %p\n", *msg_front);
// 	// printf("back: %p\n", *msg_back);
// 	char *cur_data = NULL;
// 	size_t cur = 0;
// 	// printf("!!%ld\n", data_sz);
// 	while(cur < data_sz) {
// 		//char* tmp = strpbrk(&data[cur], "\n");
// 		size_t i = cur;
// 		for(; i < data_sz; ++i) {
// 			if(data[i] == '\n')
// 				break;
// 		}
		
// 		// if(tmp == NULL)
// 		// 	break;
// 		size_t sz = i-cur;
// 		// size_t sz = tmp-&data[cur];
// 		if(cur+sz >= data_sz)
// 			break;
// 		cur_data = malloc(sz+1);
// 		printf("PUSH MSG with SIZE %ld\n", sz);
// 		// cur_data = strdup(data+cur);
// 		memcpy(cur_data, &data[cur], sz);
// 		cur += sz+1;
// 		cur_data[sz] = 0;
// 		// printf("%s\n", cur_data);

// 		if(*msg_back == NULL) {
// 			*msg_front = chat_message_new(cur_data);
// 			*msg_back = *msg_front;
// 		} else {
// 			(*msg_back)->prev = chat_message_new(cur_data);
// 			*msg_back = (*msg_back)->prev;
// 		}
// 		free(cur_data);
// 	}
	
// 	buf->buf = memmove(buf->buf, &buf->buf[cur], buf->sz-cur);
// 	buf->sz = buf->sz-cur;
// 	buf->cap = buf->sz;
// 	buf->start = 0;
// }

struct chat_message *
chat_message_pop(struct chat_message **msg_front, struct chat_message **msg_back)
{
	// printf("pop msg\n");
	// printf("pop msg: %s\n", (*msg_front)->data);
	if(*msg_front == NULL){
		return NULL;
	}
	struct chat_message *tmp = *msg_front;
	*msg_front = (*msg_front)->prev;
	if(*msg_front == NULL)
		*msg_back = NULL;
	// printf("poped msg size: %d\n", tmp->data)
	return tmp;
}

int chat_events_to_poll_events(int mask)
{
	int res = 0;
	if ((mask & CHAT_EVENT_INPUT) != 0)
		res |= POLLIN;
	if ((mask & CHAT_EVENT_OUTPUT) != 0)
		res |= POLLOUT;
	return res;
}
