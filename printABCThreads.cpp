//
// Target:
// Create three threads with name A, B, C respectively. Please output the thread name 10 times to the terminal.
// We want the thread names was printed by turns.
// Please let the three threads are all in running status and output(thread name ABCABCABC) by turns.
//
// Time: April,4 2018
// Author:
// Version :

#include <pthread.h>
#include <iostream>
#include <cstdio>

#define LOG 0
const int THREAD_NUMBER = 3;

// Thread mutex and condition variables.
pthread_mutex_t thread_mutex[THREAD_NUMBER];
pthread_cond_t thread_cond[THREAD_NUMBER];

// Is thread waiting..
bool thread_wait_flag[THREAD_NUMBER];

int iName[THREAD_NUMBER]; // Thread argument for buffering every threadName.

// Flag to indicates which thread gets the turn to output its id.
pthread_mutex_t mutex;
int thread_turn;

void *thread_func(void *arg);

int main(int argc, char **argv)
{
    pthread_t tids[THREAD_NUMBER];

    for (int i = 0; i < THREAD_NUMBER; ++i)
    {
        pthread_mutex_init(&thread_mutex[i], NULL);
        pthread_cond_init(&thread_cond[i], NULL);
    }

    pthread_mutex_init(&mutex, NULL);
    thread_turn = 0;

    for (int i = 0; i < THREAD_NUMBER; ++i)
        thread_wait_flag[i] = false;

    for (int i = 0; i < THREAD_NUMBER; ++i)
    {
        iName[i] = i; //The thread argument should be isolated between threeads.
        pthread_create(&tids[i], NULL, thread_func, (void *)&iName[i]);
    }

    for (int i = 0; i < THREAD_NUMBER; ++i)
    {
        pthread_join(tids[i], NULL);
    }
    printf("\n");
    return 0;
}

void *thread_func(void *arg)
{
    int *pid = (int*)arg;
    int id = *pid;
    char ch = 'A' + id;
    int count = 0;
#if LOG
    printf("Thread:%d starts running.\n", id);
#endif
    while (true)
    {
        if (id == thread_turn) // output thread id and send signal.
        {
#if LOG
            printf("Thread:%d starts Printing.\n", id);
#endif
            printf(" %c ", ch);
            ++count;
            if (id == THREAD_NUMBER-1 && count == 10) // exit if it's the third thread.
                break;
            else if (id == THREAD_NUMBER - 1)
                printf("\n");

            pthread_mutex_lock(&mutex);
#if LOG
            printf("Thread:%d get the global mutex.\n", id);
#endif
            thread_turn++;
            thread_turn %= THREAD_NUMBER;
            pthread_mutex_unlock(&mutex);
#if LOG
            printf("Thread:%d release the global mutex.\n", id);
#endif
            while (true)
            {
                pthread_mutex_lock(&thread_mutex[thread_turn]);
                if (true == thread_wait_flag[thread_turn])
                {
                    pthread_cond_signal(&thread_cond[thread_turn]);
                    pthread_mutex_unlock(&thread_mutex[thread_turn]);
                    break;
                }
                pthread_mutex_unlock(&thread_mutex[thread_turn]);
            }
            if (count == 10) // Thread will exit after send signal if the thread number is 1 or 2.
                break;
        }
        else // wait
        {
#if LOG
            printf("Thread:%d is waiting.\n", id);
#endif
            pthread_mutex_lock(&thread_mutex[id]);
            thread_wait_flag[id] = true;
            pthread_cond_wait(&thread_cond[id], &thread_mutex[id]);
            thread_wait_flag[id] = false;
            pthread_mutex_unlock(&thread_mutex[id]);
        }
    }
    return (void *)0;
}
