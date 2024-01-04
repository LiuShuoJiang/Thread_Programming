# ifndef _THREADPOOL_H
#define _THREADPOOL_H

typedef struct ThreadPool ThreadPool;

// create thread pool and initialize
ThreadPool* threadPoolCreate(int min, int max, int queueSize);

// destroy thread pool
int threadPoolDestroy(ThreadPool* pool);

// add tasks to thread pool
void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void* arg);

// get current # of working threads
int threadPoolBusyNum(ThreadPool* pool);

// get current # of living threads
int threadPoolAliveNum(ThreadPool* pool);

//////////////////////////////////////////////////////////////////

// worker threads function
void* worker(void* arg);

// manager thread function
void* manager(void* arg);

void threadExit(ThreadPool* pool);

#endif
