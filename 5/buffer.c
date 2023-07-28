#include "buffer.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>


struct buffer *buffer_create() {
    struct buffer *buf = malloc(sizeof(struct buffer));
    buf->cap = 1000;
    buf->buf = malloc(buf->cap);
    buf->sz = 0;
    buf->start = 0;
    return buf;
}

static void _buffer_add_space(struct buffer *buf, size_t new_sz) {
    while(new_sz >= buf->cap) {
        buf->cap += 1000;
        // printf("realloc cap: %ld\n", buf->cap);
        // printf("buf start: %ld\n", buf->start);
        buf->buf = realloc(buf->buf, buf->cap);
    }
    if (buf->start > 0) {
        memmove(buf->buf, buf->buf + buf->start, buf->sz - buf->start);
        buf->sz -= buf->start;
        buf->start = 0;
    }
    
}

void buffer_push(struct buffer *buf, const char *str, size_t sz) {
    // printf("BUF_PUSH\n");
    _buffer_add_space(buf, buf->sz+sz);

    memcpy(buf->buf+buf->sz, str, sz);
    // for(size_t i = buf->sz; i < sz+buf->sz; ++i) {
    //     buf->buf[i] = str[i-buf->sz];
    // }
    buf->sz += sz;
}

int buffer_has_msg(struct buffer *buf) {
    for (size_t i = buf->start; i < buf->sz; ++i) {
        if (buf->buf[i] == '\0')
            return 1;
    }
    return 0;
}

void buffer_delete(struct buffer *buf) {
    // printf("buffer delete");
    free(buf->buf);
    free(buf);
}

static void _buffer_clear(struct buffer *buf) {
    buf->cap = 1000;
    buf->sz = 0;
    buf->start = 0;
}

int buffer_recv(struct buffer *buf, int socket) {
    // _buffer_clear(buf);
    size_t recvd = 0; 
    // printf("buf_recv\n");
    while(1) {
        _buffer_add_space(buf, buf->sz+1);
        ssize_t size = read(socket, buf->buf+buf->sz,  buf->cap - buf->sz);
        recvd += size;
        // printf("%ld\n", size);
        // printf("%s\n", buf->buf);
        if(size == -1) {
                // printf("recv %ld\n", buf->sz);
            if(errno == EAGAIN) {
                // printf("recv %ld\n", buf->sz);
                return 0;
            }
            if(errno == ECONNRESET) {
                return -2;
            }
            
            return -1;
        }
        if(size == 0) {
            // printf("recv %ld\n", buf->sz);
            if(recvd > 0) return recvd;
            return -2;
        }

        buf->sz += size;
        
    }
}

int buffer_send(struct buffer *buf, int socket) {
    size_t sent = 0;
    

    while(sent < buf->sz) {
        ssize_t size = write(socket, buf->buf + buf->start,  buf->sz - buf->start);
        // printf("%ld\n", size);
        if(size == -1)
            return -1;
        sent += size;
        buf->start += size;
    }

    _buffer_clear(buf);
    // printf("send %ld\n", sent);
    return 0;
}