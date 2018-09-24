//
//  tls_cache.c
//  coretls
//

#include <tls_cache.h>
#include <pthread.h>
#include <stdlib.h>
#include "appleSession.h"

struct _tls_cache_s
{
    struct SessionCache cache;
    pthread_mutex_t lock;
};

tls_cache_t
tls_cache_create(void)
{
    tls_cache_t cache;

    cache = (tls_cache_t)malloc(sizeof(struct _tls_cache_s));
    if(cache) {
        SessionCacheInit(&cache->cache);
        pthread_mutex_init(&cache->lock, NULL);
    }

    return cache;
}

void
tls_cache_destroy(tls_cache_t cache)
{
    SessionCacheEmpty(&cache->cache);
    pthread_mutex_destroy(&cache->lock);
}

void
tls_cache_empty(tls_cache_t cache)
{
    pthread_mutex_lock(&cache->lock);
    SessionCacheEmpty(&cache->cache);
    pthread_mutex_unlock(&cache->lock);
}

void
tls_cache_set_default_ttls(__unused tls_cache_t cache, __unused time_t default_ttl, __unused time_t max_ttl)
{
    // no-op
    return;
}

void
tls_cache_cleanup(tls_cache_t cache)
{
    pthread_mutex_lock(&cache->lock);
    SessionCacheCleanup(&cache->cache);
    pthread_mutex_unlock(&cache->lock);
}

int
tls_cache_save_session_data(tls_cache_t cache, const tls_buffer *sessionKey, const tls_buffer *sessionData, time_t ttl)
{
    int err;
    pthread_mutex_lock(&cache->lock);
    err = SessionCacheAddEntry(&cache->cache, sessionKey, sessionData, ttl);
    pthread_mutex_unlock(&cache->lock);
    return err;
}

int
tls_cache_load_session_data(tls_cache_t cache, const tls_buffer *sessionKey, tls_buffer *sessionData)
{
    int err;
    pthread_mutex_lock(&cache->lock);
    err = SessionCacheLookupEntry(&cache->cache, sessionKey, sessionData);
    pthread_mutex_unlock(&cache->lock);
    return err;
}

int
tls_cache_delete_session_data(tls_cache_t cache, const tls_buffer *sessionKey)
{
    int err;
    pthread_mutex_lock(&cache->lock);
    err = SessionCacheDeleteEntry(&cache->cache, sessionKey);
    pthread_mutex_unlock(&cache->lock);
    return err;
}

