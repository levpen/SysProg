#pragma once

#include <stdbool.h>

struct coro;
typedef void* (*coro_f)(void *);

/** Make current context scheduler. */
void
coro_sched_init(void);

/**
 * Block until any coroutine has finished. It is returned. NULl,
 * if no coroutines.
 */
struct coro *
coro_sched_wait(void);

/** Currently working coroutine. */
struct coro *
coro_this(void);

/**
 * Create a new coroutine. It is not started, just added to the
 * scheduler.
 */
struct coro *
coro_new(coro_f func, void *func_arg);

/** Returns the work time of the coroutine. */
double
coro_work_time(const struct coro *c);

/** Return status of the coroutine. */
void*
coro_status(const struct coro *c);

/** Return switch count of the coroutine. */
long long
coro_switch_count(const struct coro *c);

/** Check if the coroutine has finished. */
bool
coro_is_finished(const struct coro *c);

/** Free coroutine stack and it itself. */
void
coro_delete(struct coro *c);

/** Switch to another not finished coroutine. */
void
coro_yield(void);
