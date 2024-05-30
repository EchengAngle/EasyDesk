#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define THREAD_POOL_SIZE 4
#define TASK_QUEUE_SIZE 10

typedef struct {
    void (*function)(void*);
    void* argument;
} Task;

typedef struct {
    Task taskQueue[TASK_QUEUE_SIZE];
    int taskCount;
    pthread_mutex_t lock;
    pthread_cond_t notify;
    pthread_t threads[THREAD_POOL_SIZE];
    int shutdown; //shutdown flag
} ThreadPool;

void* threadDoWork(void* threadPool);

void initThreadPool(ThreadPool* pool) {
    pool->taskCount = 0;
    pool->shutdown = 0;
    pthread_mutex_init(&(pool->lock), NULL);
    pthread_cond_init(&(pool->notify), NULL);
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_create(&(pool->threads[i]), NULL, threadDoWork, (void*)pool);
    }
}

void submitTask(ThreadPool* pool, void (*function)(void*), void* argument) {
    pthread_mutex_lock(&(pool->lock));
    Task task;
    task.function = function;
    task.argument = argument;
    pool->taskQueue[pool->taskCount++] = task;
    pthread_cond_signal(&(pool->notify));
    pthread_mutex_unlock(&(pool->lock));
}

void* threadDoWork(void* threadPool) {
    ThreadPool* pool = (ThreadPool*)threadPool;
    while (1) {
        pthread_mutex_lock(&(pool->lock));
        while (pool->taskCount == 0 && !pool->shutdown) {
            pthread_cond_wait(&(pool->notify), &(pool->lock));
        }
        if (pool->shutdown) {
            pthread_mutex_unlock(&(pool->lock));
            pthread_exit(NULL);
        }
        Task task = pool->taskQueue[--pool->taskCount];
        pthread_mutex_unlock(&(pool->lock));
        (*(task.function))(task.argument);
    }
}

void destroyThreadPool(ThreadPool* pool) {
    pthread_mutex_lock(&(pool->lock));
    pool->shutdown = 1;
    pthread_cond_broadcast(&(pool->notify));
    pthread_mutex_unlock(&(pool->lock));
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    pthread_mutex_destroy(&(pool->lock));
    pthread_cond_destroy(&(pool->notify));
}

void sampleTask(void* arg) {
    int* num = (int*)arg;
    printf("Thread %ld is processing task %d\n", pthread_self(), *num);
    sleep(1);
}

int main() {
    ThreadPool pool;
    initThreadPool(&pool);
    for (int i = 0; i < 20; i++) {
        int* arg = malloc(sizeof(*arg));
        *arg = i;
        submitTask(&pool, sampleTask, arg);
    }
    sleep(10);  // Let the tasks complete
    destroyThreadPool(&pool);
    return 0;
}
