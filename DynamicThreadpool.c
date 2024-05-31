// File: threadpool.c
// desc: crate the thraed pool model 
// working thread pool, manager thread

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>

#define DEFAULT_TIME 10 // manager thread will check task queue for every 10s
#define MIN_WAIT_TASK_NUM 5
#define DEFAULT_THREAD_VARY 10 //thread number for every create/destroy

typedef struct{
   void* (*function)(void *args); // callback function;
   void *arg; // param of the the callback function
} threadpool_task_t;

typedef struct 
{
   pthread_mutex_t lock; // used to lock this struct itself.
   pthread_mutex_t thread_counter; // used to lock the busy thread number --busy_thr_num
   
   pthread_cond_t queue_not_full; // if queue is full, adding thread will be blocked. will waiting this contidation var.
   pthread_cond_t queue_not_empty; // if queue is not empty, to notify the waiting threads

   pthread_t *threads; // And arrary, used to save every thread's id of the thread pool.
   pthread_t adjust_tid; // Manager thread's id
   threadpool_task_t *task_queue; // the task queue's add.(addr of the array)

   int min_thr_num; // the minimum thread num of the thread pool
   int max_thr_num; // the max thread num of the thread pool
   int live_thr_num; // the living thread num.
   int busy_thr_num; // the busy thread number.
   int wait_exit_thr_num; // num of thread to be destroyed

   int queue_front; // task queue's head index
   int queue_rear; // task queue's rear index
   int queue_size; // real task numer in task queue.
   int queue_max_size; // the capacity of the task queue;

   int shutdown; // flag: indicating thread using status: true/false
} threadpool_t;

void *adjust_thread(void* threadpool); //callback used by manager thread

int is_thread_alive(pthread_t tid){return tid == 0;}
int threadpool_free(threadpool_t *pool);

// worker in threadpool, to process task.
void *process(void *arg);

/// The working thread in the thread pool
/// @param threadpool 
/// @return 
void *threadpool_thread(void* threadpool)
{
   threadpool_t *pool = (threadpool_t*)threadpool;
   threadpool_task_t task;

   while(true)
   {
      // lock must be taken to wait on conditinal variable
      // the new created thread will be blocked until the queue has a task
      pthread_mutex_lock(&pool->lock);

      // queue_size == 0 means no task, so call cond_wait.
      while (pool->queue_size == 0 && !pool->shutdown)
      {
         printf("thread 0x%x is waiting\n", (unsigned long)pthread_self());
         pthread_cond_wait(&(pool->queue_not_empty), &(pool->lock));

         // remove idle threads
         if(pool->wait_exit_thr_num > 0)
         {
            pool->wait_exit_thr_num--;
            // can end current thread while thread num is more than min_thr_num
            if(pool->live_thr_num > pool->min_thr_num 
               && pool->busy_thr_num* 2 < pool->live_thr_num)
            {
               printf("thread 0x%x is existing, living threads count:%d\n", (unsigned long)pthread_self(), pool->live_thr_num);
               pool->live_thr_num--;
               pthread_mutex_unlock(&(pool->lock));
               
               pthread_exit(NULL); //close current thread
            }
         }
      }
      if(pool->shutdown)
      {
         pthread_mutex_unlock(&(pool->lock));
         printf("thread 0x%d is exiting\n", (unsigned long)pthread_self());
         pthread_detach(pthread_self());
         pthread_exit(NULL);
      }
      // get the task from the task queue and then execute the task.
      task.function = pool->task_queue[pool->queue_front].function;
      task.arg = pool->task_queue[pool->queue_front].arg;

      // out of the queue
      pool->queue_front = (pool->queue_front+1)%pool->queue_max_size; // circle queue.
      --pool->queue_size;

      //notify new task can be added now.
      pthread_cond_broadcast(&(pool->queue_not_full));
      // release the lock after the task was picked out
      pthread_mutex_unlock((&pool->lock));

      //execute the task
      printf("thread 0x%x start working\n", (unsigned long)pthread_self());
      pthread_mutex_lock(&(pool->thread_counter)); //busy thread counter lock
      ++pool->busy_thr_num; //busy thread number increase
      pthread_mutex_unlock(&(pool->thread_counter));
      
      (*(task.function))(task.arg);  // execute the callback function
      printf("thread 0x%x end working\n", (unsigned long)pthread_self());
      pthread_mutex_lock(&(pool->thread_counter)); //busy thread counter lock
      --pool->busy_thr_num; //busy thread number decrease after task finished.
      pthread_mutex_unlock(&(pool->thread_counter));    

   }
}

// manager thread's callback
void *adjust_thread(void* threadpool)
{
   int i;
   threadpool_t *pool = (threadpool_t*)threadpool;
   while(!pool->shutdown)
   {
      sleep(DEFAULT_TIME); // timer to manage the thread pool

      pthread_mutex_lock(&(pool->lock));
      int queue_size = pool->queue_size; // task number
      int live_thr_num = pool->live_thr_num; //living thread number
      pthread_mutex_unlock(&(pool->lock));

      pthread_mutex_lock(&(pool->thread_counter));
      int busy_thr_num = pool->busy_thr_num; //busy thread number
      pthread_mutex_unlock(&(pool->thread_counter));

      // create new thread.
      // alg: taskNUM > min_thr_num and living_thr_num < max_thr_num
      if(queue_size >= MIN_WAIT_TASK_NUM && live_thr_num < pool->max_thr_num)
      {
         pthread_mutex_lock(&(pool->lock));
         int add = 0;
         // will add DEFAULT_THREAD thread everytime
         for(int i=0; i< pool->max_thr_num && add < DEFAULT_THREAD_VARY
         && pool->live_thr_num < pool->max_thr_num; i++)
         {
            //if(pool->threads[i] == 0 || !is_thread_alive(pool->threads[i]))
            if(pool->threads[i] == 0)
            {
               pthread_create(&(pool->threads[i]), NULL, threadpool_thread, (void*)pool);
               ++add;
               ++pool->live_thr_num;
            }
         }
         pthread_mutex_unlock(&(pool->lock));
         printf("Dynamically added %d threads, living threads:%d\n", add, pool->live_thr_num);
      }
      // destroy redundant idle threads
      // alg: busy_thr_num*2 < live_thr_num, and live_thr_num > min_thr_num
      if((busy_thr_num*2) < live_thr_num && live_thr_num > pool->min_thr_num)
      {
         printf("Will dynamically decrease 10 threads\n");
         // destroy DEFAULT_THREAD number threads.
         pthread_mutex_lock(&(pool->lock));
         pool->wait_exit_thr_num = DEFAULT_THREAD_VARY; // thread numb to be destroyed, default to 10
         pthread_mutex_unlock(&(pool->lock));

         for(i = 0; i< DEFAULT_THREAD_VARY; ++i)
         {
            // to notify the idle thread. they will exit by themselves.
            pthread_cond_signal(&pool->queue_not_empty); 
         }
      }

      // test code: also use this thread to create task
      int num[100];
      for(int i=0; i<50; ++i)
      {
         num[i] = i;
         printf("add task %d\n",i);
         /*int threadpool_add(threadpool_t *pool, void*(*function)(void *agr), void *agr)*/
         threadpool_add(pool, process, (void*)&num[i]); // add task into the thread pool.
      }
   }
   return NULL;
}

threadpool_t* threadpool_create(int min_thr_num, int max_thr_num, int queue_max_size)
{
   int i;
   threadpool_t *pool = NULL; // thread pool struct

   do
   {
      if((pool = (threadpool_t*)malloc(sizeof(threadpool_t))) == NULL)
      {
         printf("malloc threadpool fail.\n");
         break;
      }
      pool->min_thr_num = min_thr_num;
      pool->max_thr_num = max_thr_num;
      pool->busy_thr_num = 0;

      pool->live_thr_num = min_thr_num; // live num's init value = min_thr_num
      pool->wait_exit_thr_num = 0;

      pool->queue_size = 0; // there is 0 task.
      pool->queue_max_size = queue_max_size; // set the max capacity

      pool->queue_front = 0;
      pool->queue_rear = 0;
      pool->shutdown = false; //means don't close the thread pool

      // according the max thread num, allocate the memeory for working threads, and initialize to 0.
      pool->threads = (pthread_t *)malloc(sizeof(pthread_t)*max_thr_num);
      if(pool->threads == NULL){
         printf("malloc threads fail\n");
         break;
      } 
      memset(pool->threads, 0, sizeof(pthread_t)*max_thr_num); // init to 0
      
      // allocate memeory for task queue.
      pool->task_queue = (threadpool_task_t*)malloc(sizeof(threadpool_task_t)*queue_max_size);
      if(pool->task_queue == NULL){
         printf("mallock task_queue failed\n");
         break;
      }

      // init the mutex and condition variable
      if(pthread_mutex_init(&(pool->lock), NULL) !=0 
      || pthread_mutex_init(&(pool->thread_counter), NULL) !=0
      || pthread_cond_init(&(pool->queue_not_empty), NULL) != 0
      || pthread_cond_init(&(pool->queue_not_full), NULL) != 0)
      {
         printf("init the lock or condition failed\n");
         break;
      }

      // create/start min_thr_num work threads.
      for(i = 0; i < min_thr_num; ++i){
         pthread_create(&(pool->threads[i]), NULL, threadpool_thread, (void*)pool); // pool is the pointer of current thread.
         printf("start thread 0x%x...\n", (unsigned int)pool->threads[i]);
      }
      // create the manager thread
      pthread_create(&(pool->adjust_tid), NULL, adjust_thread, (void*)pool);
      
      return pool;
      
   } while(0);

   // if error happened:
   threadpool_free(pool); // release the pool memory.
   return NULL;
}

//add a task into the thread pool
int threadpool_add(threadpool_t *pool, void*(*function)(void* arg), void* arg)
{
   pthread_mutex_lock(&(pool->lock));

   // the task queue is full, need to wait.
   while((pool->queue_size == pool->queue_max_size) && (!pool->shutdown))
   {
      pthread_cond_wait(&(pool->queue_not_full), &(pool->lock));
   }
   if(pool->shutdown)
   {
      pthread_cond_broadcast(&(pool->queue_not_empty));
      pthread_mutex_unlock(&(pool->lock));
      return 0;
   }
   // clear the worker thread's callback's arg.
   if(pool->task_queue[pool->queue_rear].arg != NULL){
      pool->task_queue[pool->queue_rear].arg = NULL;
   }
   // add task into the queue
   pool->task_queue[pool->queue_rear].function = function;
   pool->task_queue[pool->queue_rear].arg = arg;
   // move the queue rear pointer in circle queue.
   pool->queue_rear = (pool->queue_rear + 1) % pool->queue_max_size; 
   ++pool->queue_size; // real task number 

   // after new task added, woke up the waiting worker thread
   pthread_cond_signal(&(pool->queue_not_empty));
   pthread_mutex_unlock(&(pool->lock));

   return 0;
}

// worker in threadpool, to process task.
void *process(void *arg)
{
   printf("thread 0x%x working on task %d\n",(unsigned int)pthread_self(), *(int*)arg);
   sleep(2); //sleep 1s
   printf("task %d is end\n", *(int*)arg);
   return NULL;
}

int threadpool_destroy(threadpool_t *pool)
{
   int i;
   if(pool == NULL)
      return -1;
   pool->shutdown = true;

   // destroy the manager thread first.
   pthread_join(pool->adjust_tid, NULL);

   // notify all the idle worker thread
   for(i = 0; i<pool->live_thr_num; ++i)
   {
      pthread_cond_broadcast(&(pool->queue_not_empty));
   }
   for(i = 0; i<pool->live_thr_num; ++i)
   {
      pthread_join(pool->threads[i], NULL);
   }
   threadpool_free(pool);

   return 0;
}

int threadpool_free(threadpool_t* pool)
{
   if(pool == NULL) return -1;

   if(pool->task_queue)
      free(pool->task_queue);

   if(pool->threads)
   {
      free(pool->threads);
      pthread_mutex_lock(&(pool->lock));
      pthread_mutex_destroy(&(pool->lock));
      
      pthread_mutex_lock(&(pool->thread_counter));
      pthread_mutex_destroy(&(pool->thread_counter));

      pthread_cond_destroy(&(pool->queue_not_empty));
      pthread_cond_destroy(&(pool->queue_not_full));
   }
   free(pool);
   pool=NULL;
   return 0;
}

int main(void)
{
    /*threadpool_t *threadpool_create(int min_thr_num, int max_thr_num, int queue_max_size)*/
    threadpool_t *thp = threadpool_create(3, 100, 100); /*create thread pool,池里最小3个线程,最大100, 队列最大100*/
    printf("pool inited\n");
    // int *num = (int*) malloc(sizeof(int)*20);
    int num[100], i;
    for(i=0; i<100; ++i)
    {
        num[i] = i;
        printf("add task %d\n",i);
        /*int threadpool_add(threadpool_t *pool, void*(*function)(void *agr), void *agr)*/
        threadpool_add(thp, process, (void*)&num[i]); // add task into the thread pool.
    } //!! also use the manager thread to dynamically generate tasks for test.
    while(1){
      sleep(10);   // waiting sub-threads to finish their tasks
    } 
    threadpool_destroy(thp);
    return 0;
}
