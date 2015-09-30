#define __DARWIN_NON_CANCELABLE 0
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static pthread_once_t once = PTHREAD_ONCE_INIT;
static int x = 0;

void cancelled(void)
{
    printf("thread cancelled.\n");
}

void oncef(void)
{
    printf("in once handler: %p\n", pthread_self());
    sleep(5);
    x = 1;
}

void* a(void *ctx)
{
    printf("a started: %p\n", pthread_self());
    pthread_cleanup_push((void*)cancelled, NULL);
    pthread_once(&once, oncef);
    pthread_cleanup_pop(0);
    printf("a finished\n");
    return NULL;
}

void* b(void *ctx)
{
    sleep(1); // give enough time for a() to get into pthread_once
    printf("b started: %p\n", pthread_self());
    pthread_once(&once, oncef);
    printf("b finished\n");
    return NULL;
}

int main(void)
{
    pthread_t t1;
    if (pthread_create(&t1, NULL, a, NULL) != 0) {
        fprintf(stderr, "failed to create thread a.");
        exit(1);
    }

    pthread_t t2;
    if (pthread_create(&t2, NULL, b, NULL) != 0) {
        fprintf(stderr, "failed to create thread b.");
        exit(1);
    }

    sleep(2);
    pthread_cancel(t1);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    exit(0);
}