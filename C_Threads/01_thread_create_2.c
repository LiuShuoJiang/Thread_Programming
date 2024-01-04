#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

struct Test {
    int num;
    int age;
};

struct Test t;

void* callback(void *arg) {
    for (int i = 0; i < 6; i++) {
        printf("sub thread: i = %d\n", i);
    }
    printf("sub thread id: %ld\n", pthread_self());

    t.num = 100;
    t.age = 66;

    pthread_exit(&t);

    return NULL;
}

int main() {
    pthread_t tid;
    pthread_create(&tid, NULL, callback, &t);

    for (int i = 0; i < 3; i++) {
        printf("main thread: i = %d\n", i);
    }
    printf("main thread id: %ld\n", pthread_self());

    // sleep(3);
    // pthread_exit(NULL);

    void *ptr;

    pthread_join(tid, &ptr);

    struct Test *pt = (struct Test*)ptr;
    printf("num: %d, age: %d\n", pt->num, pt->age);

    return 0;
}