#include "thread_pool.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

struct thread_task {
	thread_task_f function;
	void *arg;
	void *result;


	/* PUT HERE OTHER MEMBERS */
	
	enum {
		T_CREATED,
		T_RUNS,
		T_FINISHED,
	} status;

	pthread_mutex_t mutex_status;
	pthread_cond_t cond_status;

	struct thread_task *next;

	struct thread_pool *pool;
};

struct thread_pool {
	pthread_t *threads;
	int max_thread_count;
	int thread_count;

	/* PUT HERE OTHER MEMBERS */


	pthread_mutex_t mutex_queue;
	pthread_cond_t cond_queue;
	struct thread_task *queue_front;
	struct thread_task *queue_back;
	int tasks_count;

	int close;
};

int
thread_pool_new(int max_thread_count, struct thread_pool **pool)
{
	if(max_thread_count <= 0 || max_thread_count > TPOOL_MAX_THREADS) {
		return TPOOL_ERR_INVALID_ARGUMENT;
	}

	*pool = malloc(sizeof(struct thread_pool));
	(*pool)->threads = malloc(max_thread_count * sizeof(pthread_t));

	(*pool)->max_thread_count = max_thread_count;
	(*pool)->thread_count = 0;

	(*pool)->queue_front = NULL;
	(*pool)->queue_back = NULL;

	(*pool)->tasks_count = 0;
	(*pool)->close = 0;

	pthread_mutex_init(&(*pool)->mutex_queue, NULL);
	pthread_cond_init(&(*pool)->cond_queue, NULL);
	return 0;
	/* IMPLEMENT THIS FUNCTION */
	// (void)max_thread_count;
	// (void)pool;
	// return TPOOL_ERR_NOT_IMPLEMENTED;
}

int
thread_pool_thread_count(const struct thread_pool *pool)
{
	return pool->thread_count;
	/* IMPLEMENT THIS FUNCTION */
	// (void)pool;
	// return TPOOL_ERR_NOT_IMPLEMENTED;
}

int
thread_pool_delete(struct thread_pool *pool)
{
	if(__atomic_load_n(&pool->tasks_count, __ATOMIC_ACQUIRE) > 0) {
		return TPOOL_ERR_HAS_TASKS;
	}

	pthread_mutex_lock(&pool->mutex_queue);
    pool->close = 1;
    pthread_cond_broadcast(&pool->cond_queue);
    pthread_mutex_unlock(&pool->mutex_queue);

	for(int i = 0; i < pool->thread_count; ++i) {
		pthread_join(pool->threads[i], NULL);
	}

	pthread_mutex_destroy(&(pool->mutex_queue));
	pthread_cond_destroy(&(pool->cond_queue));

	free(pool->threads);
	free(pool);
	return 0;
	/* IMPLEMENT THIS FUNCTION */
	// (void)pool;
	// return TPOOL_ERR_NOT_IMPLEMENTED;
}

void* _thread_worker(void* arg_pool) {
	struct thread_pool * pool = arg_pool;
	while(1) {
		pthread_mutex_lock(&pool->mutex_queue);
		struct thread_task *cur_task = pool->queue_front;

		if(cur_task == NULL) {
			if (pool->close) {
                pthread_mutex_unlock(&pool->mutex_queue);
                break;
            }
			pthread_cond_wait(&pool->cond_queue, &pool->mutex_queue);
			pthread_mutex_unlock(&pool->mutex_queue);
			continue;
		}

		if(pool->queue_back == pool->queue_front) {
			pool->queue_front = NULL;
			pool->queue_back = NULL;
		}
		else {
			pool->queue_front = cur_task->next;
		}

		pthread_mutex_unlock(&pool->mutex_queue);

		__atomic_store_n(&cur_task->status, T_RUNS, __ATOMIC_RELAXED);
		//int tmp = __atomic_load_n(&pool->tasks_count, __ATOMIC_RELAXED);
		// printf("task started %d\n", tmp);
		cur_task->result = cur_task->function(cur_task->arg);

		pthread_mutex_lock(&cur_task->mutex_status);

		__atomic_store_n(&cur_task->status, T_FINISHED, __ATOMIC_RELAXED);
		// printf("task finished %d\n", tmp);

		pthread_cond_signal(&cur_task->cond_status);
		pthread_mutex_unlock(&cur_task->mutex_status);
	}
	return NULL;
}




int
thread_pool_push_task(struct thread_pool *pool, struct thread_task *task)
{
	if(__atomic_load_n(&pool->tasks_count, __ATOMIC_RELAXED) == TPOOL_MAX_TASKS)
		return TPOOL_ERR_TOO_MANY_TASKS;

	int new_count = __atomic_add_fetch(&pool->tasks_count, 1, __ATOMIC_ACQ_REL);

	task->pool = pool;
	
	pthread_mutex_lock(&pool->mutex_queue);
	struct thread_task *tmp = pool->queue_back;
	pool->queue_back = task;
	if(tmp != NULL) {
		tmp->next = task;
	} else {
		pool->queue_front = task;
	}

	if(new_count > pool->thread_count && pool->thread_count != pool->max_thread_count) {
		pthread_create(&pool->threads[pool->thread_count++], NULL, _thread_worker, pool);
	}

	pthread_cond_signal(&pool->cond_queue);
	pthread_mutex_unlock(&pool->mutex_queue);
	return 0;
	/* IMPLEMENT THIS FUNCTION */
	// (void)pool;
	// (void)task;
	// return TPOOL_ERR_NOT_IMPLEMENTED;
}

int
thread_task_new(struct thread_task **task, thread_task_f function, void *arg)
{
	*task = malloc(sizeof(struct thread_task));

	pthread_mutex_init(&(*task)->mutex_status, NULL);
	pthread_cond_init(&(*task)->cond_status, NULL);
	
	(*task)->status = T_CREATED;
	(*task)->arg = arg;
	(*task)->function = function;
	(*task)->result = NULL;
	(*task)->next = NULL;
	(*task)->pool = NULL;
	return 0;
	/* IMPLEMENT THIS FUNCTION */
	// (void)task;
	// (void)function;
	// (void)arg;
	// return TPOOL_ERR_NOT_IMPLEMENTED;
}

bool
thread_task_is_finished(const struct thread_task *task)
{
	return __atomic_load_n(&task->status, __ATOMIC_RELAXED) == T_FINISHED;
	/* IMPLEMENT THIS FUNCTION */
	// (void)task;
	// return false;
}

bool
thread_task_is_running(const struct thread_task *task)
{
	return __atomic_load_n(&task->status, __ATOMIC_RELAXED) == T_RUNS;
	/* IMPLEMENT THIS FUNCTION */
	// (void)task;
	// return false;
}


int
thread_task_join(struct thread_task *task, void **result)
{
	if(task->pool == NULL) {
		return TPOOL_ERR_TASK_NOT_PUSHED;
	}
	
	pthread_mutex_lock(&task->mutex_status);
	while (true) {
        if (__atomic_load_n(&task->status, __ATOMIC_RELAXED) == T_FINISHED) {
            break;
        }
        pthread_cond_wait(&task->cond_status, &task->mutex_status);
    }
	pthread_mutex_unlock(&task->mutex_status);

	__atomic_sub_fetch(&task->pool->tasks_count, 1, __ATOMIC_ACQ_REL);

	task->pool = NULL;
	
	*result = task->result;

	return 0;
	/* IMPLEMENT THIS FUNCTION */
	// (void)task;
	// (void)result;
	// return TPOOL_ERR_NOT_IMPLEMENTED;
}

#ifdef NEED_TIMED_JOIN

int
thread_task_timed_join(struct thread_task *task, double timeout, void **result)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	(void)timeout;
	(void)result;
	return TPOOL_ERR_NOT_IMPLEMENTED;
}

#endif

int
thread_task_delete(struct thread_task *task)
{
	if(task->pool != NULL) {
		return TPOOL_ERR_TASK_IN_POOL;
	}

	pthread_mutex_destroy(&task->mutex_status);
	pthread_cond_destroy(&task->cond_status);
	free(task);
	return 0;
	/* IMPLEMENT THIS FUNCTION */
	// (void)task;
	// return TPOOL_ERR_NOT_IMPLEMENTED;
}

#ifdef NEED_DETACH

int
thread_task_detach(struct thread_task *task)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	return TPOOL_ERR_NOT_IMPLEMENTED;
}

#endif
