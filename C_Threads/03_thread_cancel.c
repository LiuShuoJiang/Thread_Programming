#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void *working(void *arg) {
    int j = 0;
    for (int i = 0; i < 20; ++i) {
        j++;
    }
    // call system function
    printf("Sub thread ID: %ld\n", pthread_self());
    sleep(1);
    for (int i = 0; i < 20; ++i) {
        printf(" child i: %d\n", i);
    }

    return NULL;
}

int main() {
    pthread_t tid;
    pthread_create(&tid, NULL, working, NULL);

    printf("Sub thread creation successful ID: %ld\n", tid);

    printf("Main thread ID: %ld\n", pthread_self());
    for (int i = 0; i < 2; ++i) {
        printf("i = %d\n", i);
    }

    pthread_cancel(tid);

    pthread_exit(NULL);

    return 0;
}
