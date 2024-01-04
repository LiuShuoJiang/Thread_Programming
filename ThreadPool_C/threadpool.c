#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "threadpool.h"

const int NUMBER = 2;

// task
typedef struct Task {
    // function pointer
    void (*function)(void* arg);
    void* arg;
} Task;

// thread pool
struct ThreadPool {
    Task* taskQ;                // task queue

    int queueCapacity;          // capacity
    int queueSize;              // current # of tasks
    int queueFront;             // front of queue: get data
    int queueRear;              // tail of queue: put data in

    pthread_t managerID;        // mamager thread ID
    pthread_t *threadIDs;       // working thread IDs

    int minNum;                 // min # of threads
    int maxNum;                 // max # of threads
    int busyNum;                // busy (working) # of threads
    int liveNum;                // surviving # of threads
    int exitNum;                // # of threads to kill

    pthread_mutex_t mutexPool;  // lock the whole thread pool
    pthread_mutex_t mutexBusy;  // loak busyNum

    pthread_cond_t notFull;     // is task queue full
    pthread_cond_t notEmpty;    // is task queue empty

    int shutdown;               // destroy thread pool? destroy: 1; not: 0
};

ThreadPool* threadPoolCreate(int min, int max, int queueSize) {
    ThreadPool* pool = (ThreadPool*) malloc(sizeof(ThreadPool));
    
    do {
        if (pool == NULL) {
            printf("malloc threadpool failed...\n");
            break;
        }

        pool->threadIDs = (pthread_t*) malloc(sizeof(pthread_t) * max);
        if (pool->threadIDs == NULL) {
            printf("malloc threadIDs failed...\n");
            break;
        }

        memset(pool->threadIDs, 0, sizeof(pthread_t) * max);
        pool->minNum = min;
        pool->maxNum = max;
        pool->busyNum = 0;
        pool->liveNum = min;  // euqal to min number
        pool->exitNum = 0;

        if (pthread_mutex_init(&pool->mutexPool, NULL) != 0
        || pthread_mutex_init(&pool->mutexBusy, NULL) != 0
        || pthread_cond_init(&pool->notEmpty, NULL) != 0
        || pthread_cond_init(&pool->notFull, NULL) != 0) {
            printf("mutex initialized failed\n");
            break;
        }

        // task queue
        pool->taskQ = (Task*) malloc(sizeof(Task) * queueSize);
        pool->queueCapacity = queueSize;
        pool->queueSize = 0;
        pool->queueFront = 0;
        pool->queueRear = 0;

        pool->shutdown = 0;

        // create threads
        pthread_create(&pool->managerID, NULL, manager, pool);
        for (int i = 0; i < min; i++) {
            pthread_create(&pool->threadIDs[i], NULL, worker, pool);
        }

        return pool;

    } while(0);

    // release resources
    if (pool && pool->threadIDs) free(pool->threadIDs);
    if (pool && pool->taskQ) free(pool->taskQ);
    if (pool) free(pool);
    
    return NULL;
}

void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void* arg) {
    pthread_mutex_lock(&pool->mutexPool);

    while (pool->queueSize == pool->queueCapacity && !pool->shutdown) {
        // block producer thread
        pthread_cond_wait(&pool->notFull, &pool->mutexPool);
    }

    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->mutexPool);
        return;
    }

    // add tasks
    pool->taskQ[pool->queueRear].function = func;
    pool->taskQ[pool->queueRear].arg = arg;
    pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
    pool->queueSize++;

    pthread_cond_signal(&pool->notEmpty);

    pthread_mutex_unlock(&pool->mutexPool);
}

int threadPoolBusyNum(ThreadPool* pool) {
    pthread_mutex_lock(&pool->mutexBusy);
    int busyNum = pool->busyNum;
    pthread_mutex_unlock(&pool->mutexBusy);
    return busyNum;
}

int threadPoolAliveNum(ThreadPool* pool) {
    pthread_mutex_lock(&pool->mutexPool);
    int aliveNum = pool->liveNum;
    pthread_mutex_unlock(&pool->mutexPool);
    return aliveNum;
}

int threadPoolDestroy(ThreadPool* pool) {
    if (pool == NULL) return -1;

    // close thread pool
    pool->shutdown = 1;
    // block to recycle manager thread
    pthread_join(pool->managerID, NULL);
    // wake up blocked consumers
    for (int i = 0; i < pool->liveNum; i++) {
        pthread_cond_signal(&pool->notEmpty);
    }

    // free up spaces
    if (pool->taskQ) {
        free(pool->taskQ);
    }
    if (pool->threadIDs) {
        free(pool->threadIDs);
    }

    pthread_mutex_destroy(&pool->mutexPool);
    pthread_mutex_destroy(&pool->mutexBusy);
    pthread_cond_destroy(&pool->notEmpty);
    pthread_cond_destroy(&pool->notFull);

    free(pool);
    pool = NULL;

    return 0;
}

void* worker(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;

    while (1) {
        // --------------------------------------------------------------
        pthread_mutex_lock(&pool->mutexPool);
        
        // if current task queue is empty
        while (pool->queueSize == 0 && !pool->shutdown) {
            // block working thread
            pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);

            // check if we want to destroy the thread
            if (pool->exitNum > 0) {
                pool->exitNum--;
                if (pool->liveNum > pool->minNum) {
                    pool->liveNum--;
                    pthread_mutex_unlock(&pool->mutexPool);
                    threadExit(pool);  // self kill
                }
            }
        }

        // if current thread pool has been shut down
        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->mutexPool);
            threadExit(pool);
        }

        // get one task from task queue
        Task task;
        // get function from queue front
        task.function = pool->taskQ[pool->queueFront].function;
        task.arg = pool->taskQ[pool->queueFront].arg;

        // move head node  (circle queue)
        pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
        pool->queueSize--;

        // wake up producer
        pthread_cond_signal(&pool->notFull);

        pthread_mutex_unlock(&pool->mutexPool);
        // --------------------------------------------------------------

        printf("thread %ld starts working...\n", pthread_self());
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum++;
        pthread_mutex_unlock(&pool->mutexBusy);

        task.function(task.arg); // (*task.function)(task.arg);
        
        free(task.arg);
        task.arg = NULL;

        printf("thread %ld ends working...\n", pthread_self());
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum--;
        pthread_mutex_unlock(&pool->mutexBusy);
    }

    return NULL;
}

void* manager(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;
    
    while (!pool->shutdown) {
        // check every 3 seconds
        sleep(3);

        // get # of tasks and current # of threads from thread pool
        pthread_mutex_lock(&pool->mutexPool);
        int queueSize = pool->queueSize;
        int liveNum = pool->liveNum;
        pthread_mutex_unlock(&pool->mutexPool);

        // get # of busy threads
        pthread_mutex_lock(&pool->mutexBusy);
        int busyNum = pool->busyNum;
        pthread_mutex_unlock(&pool->mutexBusy);

        // add threads
        // tule: # of tasks > # of living threads 
        // && # of living threads < max # of threads
        if (queueSize > liveNum && liveNum < pool->maxNum) {
            pthread_mutex_lock(&pool->mutexPool);
            
            int counter = 0;
            for (int i = 0; i < pool->maxNum && counter < NUMBER
            && pool->liveNum < pool->maxNum; i++) {
                if (pool->threadIDs[i] == 0) {
                    pthread_create(&pool->threadIDs[i], NULL, worker, pool);
                    counter++;
                    pool->liveNum++;
                }
            }

            pthread_mutex_unlock(&pool->mutexPool);
        }

        // destroy threads
        // rule: # of busy threads * 2 < # of living threads 
        // && # of living threads > min # of threads
        if (busyNum * 2 < liveNum && liveNum > pool->minNum) {
            pthread_mutex_lock(&pool->mutexPool);
            pool->exitNum = NUMBER;
            pthread_mutex_unlock(&pool->mutexPool);
            
            // let the working threads kill thenselves
            for (int i = 0; i < NUMBER; i++) {
                pthread_cond_signal(&pool->notEmpty);
            }
        }
    }

    return NULL;
}

void threadExit(ThreadPool* pool) {
    pthread_t tid = pthread_self();
    
    for (int i = 0; i < pool->maxNum; i++) {
        if (pool->threadIDs[i] == tid) {
            pool->threadIDs[i] = 0;
            printf("threadExit() called, %ld exits...\n", tid);
            break;
        }
    }
    
    pthread_exit(NULL);
}
