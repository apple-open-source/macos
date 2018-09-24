//
//  tls_cache.h
//  coretls
//

#ifndef _TLS_CACHE_H_
#define _TLS_CACHE_H_ 1

#include <tls_types.h>

typedef struct _tls_cache_s *tls_cache_t;

CORETLS_EXTERN tls_cache_t
tls_cache_create(void);

CORETLS_EXTERN void
tls_cache_destroy(tls_cache_t cache);

CORETLS_EXTERN void
tls_cache_set_default_ttls(tls_cache_t cache, time_t default_ttl, time_t max_ttl);

CORETLS_EXTERN void
tls_cache_empty(tls_cache_t cache);

CORETLS_EXTERN void
tls_cache_cleanup(tls_cache_t cache);

/* main interface to coretls handshake layer */
CORETLS_EXTERN int
tls_cache_save_session_data(tls_cache_t cache, const tls_buffer *sessionKey, const tls_buffer *sessionData, time_t ttl);

CORETLS_EXTERN int
tls_cache_load_session_data(tls_cache_t cache, const tls_buffer *sessionKey, tls_buffer *sessionData);

CORETLS_EXTERN int
tls_cache_delete_session_data(tls_cache_t cache, const tls_buffer *sessionKey);




#endif /* _TLS_CACHE_H_ */
