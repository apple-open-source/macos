//
//  tls_cache.h
//  coretls
//

#ifndef _TLS_CACHE_H_
#define _TLS_CACHE_H_ 1

#include <tls_types.h>

typedef struct _tls_cache_s *tls_cache_t;

tls_cache_t
tls_cache_create(void);

void
tls_cache_destroy(tls_cache_t cache);

void
tls_cache_set_default_ttls(tls_cache_t cache, time_t default_ttl, time_t max_ttl);

void
tls_cache_empty(tls_cache_t cache);

void
tls_cache_cleanup(tls_cache_t cache);

/* main interface to coretls handshake layer */
int
tls_cache_save_session_data(tls_cache_t cache, const tls_buffer *sessionKey, const tls_buffer *sessionData, time_t ttl);

int
tls_cache_load_session_data(tls_cache_t cache, const tls_buffer *sessionKey, tls_buffer *sessionData);

int
tls_cache_delete_session_data(tls_cache_t cache, const tls_buffer *sessionKey);




#endif /* _TLS_CACHE_H_ */
