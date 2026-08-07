#ifndef THREAD_SAFE_H_
#define THREAD_SAFE_H_

#include <pthread.h>

typedef struct
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int ready;
} thread_sync_t;

#define THREAD_SYNC_INIT {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0}

static inline void thread_mutex_lock(pthread_mutex_t *mutex)
{
    pthread_mutex_lock(mutex);
}

static inline void thread_mutex_unlock(pthread_mutex_t *mutex)
{
    pthread_mutex_unlock(mutex);
}

static inline void thread_sync_wait(thread_sync_t *sync)
{
    pthread_mutex_lock(&sync->mutex);
    while (!sync->ready)
    {
        pthread_cond_wait(&sync->cond, &sync->mutex);
    }
    sync->ready = 0;
    pthread_mutex_unlock(&sync->mutex);
}

static inline void thread_sync_signal(thread_sync_t *sync)
{
    pthread_mutex_lock(&sync->mutex);
    sync->ready = 1;
    pthread_cond_signal(&sync->cond);
    pthread_mutex_unlock(&sync->mutex);
}

#endif