/* Copyright (c) 2013 Apple Inc. All rights reserved. */

#ifndef __SECURITYD_SERVICE_CLIENT_H
#define __SECURITYD_SERVICE_CLIENT_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <bsm/audit.h>
#include <mach/message.h>
#include <stdbool.h>
    
enum {
    KB_Success      = 0,
    KB_GeneralError,
    KB_BagNotFound,
    KB_BagError,
    KB_BagNotLoaded,
    KB_BagExists,
    KB_InvalidSession
};

typedef struct {
    au_asid_t s_id;
    uid_t s_uid;
    audit_token_t procToken;
} service_context_t;
    
int service_client_kb_create(service_context_t *context, const void * secret, int secret_len);
int service_client_kb_load(service_context_t *context);
int service_client_kb_unlock(service_context_t *context, const void * secret, int secret_len);
int service_client_kb_lock(service_context_t *context);
int service_client_kb_change_secret(service_context_t *context, const void * secret, int secret_len, const void * new_secret, int new_secret_len);
int service_client_kb_is_locked(service_context_t *context, bool *locked, bool *no_pin);
int service_client_kb_reset(service_context_t *context, const void * secret, int secret_len);

int service_client_stash_set_key(service_context_t *context, const void * key, int key_len);
int service_client_stash_get_key(service_context_t *context, void ** key, int * key_len);

#if defined(__cplusplus)
}
#endif

#endif // __SECURITYD_SERVICE_CLIENT_H
