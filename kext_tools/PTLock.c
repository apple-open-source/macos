#include "PTLock.h"
#include <stdlib.h>
#include <mach/message.h>


/*******************************************************************************
*
*******************************************************************************/
typedef struct __PTLock {
    Boolean locked;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
} PTLock;


/*******************************************************************************
*
*******************************************************************************/
PTLockRef PTLockCreate(void)
{
    PTLock * lock;

    lock = (PTLock *)malloc(sizeof(PTLock));
    if (!lock) {
        return NULL;
    }

    lock->locked = false;
    pthread_mutex_init(&lock->mutex, NULL);
    pthread_cond_init(&lock->condition, NULL);

    return (PTLockRef)lock;
}

/*******************************************************************************
*
*******************************************************************************/
void PTLockFree(PTLockRef lock)
{
    if (!lock) {
        return;
    }

    pthread_mutex_destroy(&lock->mutex);
    pthread_cond_destroy(&lock->condition);
    free(lock);
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void PTLockTakeLock(PTLockRef lock)
{
    pthread_mutex_lock(&lock->mutex);
    while (lock->locked) {
        pthread_cond_wait(&lock->condition, &lock->mutex);
    }
    lock->locked = true;
    pthread_mutex_unlock(&lock->mutex);
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void PTLockUnlock(PTLockRef lock)
{
    pthread_mutex_lock(&lock->mutex);
    lock->locked = false;
    pthread_cond_signal(&lock->condition);
    pthread_mutex_unlock(&lock->mutex);
    return;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean PTLockTryLock(PTLockRef lock)
{
    Boolean available = false;
    Boolean got_it = false;

    pthread_mutex_lock(&lock->mutex);
    available = !lock->locked;
    if (available) {
        lock->locked = true;
        got_it = true;
    }
    pthread_mutex_unlock(&lock->mutex);

    return got_it;
}

