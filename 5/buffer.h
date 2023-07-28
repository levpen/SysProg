#pragma once

#include <stddef.h>

struct buffer {
    char *buf;
    size_t sz;
    size_t cap;
    size_t start;
};

struct buffer *buffer_create();

void buffer_delete(struct buffer *buf);

void buffer_push(struct buffer *buf, const char *str, size_t sz);

int buffer_has_msg(struct buffer *buf);

int buffer_recv(struct buffer *buf, int socket);
int buffer_send(struct buffer *buf, int socket);