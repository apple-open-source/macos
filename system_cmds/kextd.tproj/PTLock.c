
#include <PTLock.h>
#include <stdlib.h>
#include <mach/message.h>

#define mutex_t                pthread_mutex_t
#define condition_t            pthread_cond_t

#define mutex_init(m)          pthread_mutex_init(m, NULL)
#define mutex_free(m)          pthread_mutex_destroy(m)
#define mutex_lock(m)          pthread_mutex_lock(m)
#define mutex_unlock(m)        pthread_mutex_unlock(m)

#define condition_init(c)      pthread_cond_init(c, NULL)
#define condition_free(c)      pthread_cond_destroy(c)
#define condition_wait(c, m)   pthread_cond_wait(c, m)
#define condition_signal(c)    pthread_cond_signal(c)
#define condition_broadcast(c) pthread_cond_broadcast(c)


typedef struct _PTLock {
    Boolean locked;
    pthread_mutex_t m;
    pthread_cond_t c;
} PTLock;


PTLockRef
PTLockCreate(void)
{
    PTLock * l;

    l = (PTLock *)malloc(sizeof(PTLock));
    if ( !l )
        return NULL;

    l->locked = false;
    mutex_init(&l->m);
    condition_init(&l->c);

    return (PTLockRef)l;
}

void
PTLockFree(PTLockRef lock)
{
    PTLock * l = (PTLock *)lock;
    
    if ( !lock )
        return;

    mutex_free(&l->m);
    condition_free(&l->c);
    free(lock);
}

void
PTLockTakeLock(PTLockRef lock)
{
    PTLock * l = (PTLock *)lock;

    mutex_lock(&l->m);
    while ( l->locked )
        condition_wait(&l->c, &l->m);
    l->locked = true;
    mutex_unlock(&l->m);
}

void
PTLockUnlock(PTLockRef lock)
{
    PTLock * l = (PTLock *)lock;

    mutex_lock(&l->m);
    l->locked = false;
    condition_signal(&l->c);
    mutex_unlock(&l->m);
}

Boolean
PTLockTryLock(PTLockRef lock)
{
    PTLock * l = (PTLock *)lock;

    Boolean ret;

    mutex_lock(&l->m);
    ret = !l->locked;
    if ( ret )
        l->locked = true;
    mutex_unlock(&l->m);

    return ret;
}

