//
//  keybag_helpers.h
//  securityd_macos
//
//  Created by Fabrice Gautier on 7/26/23.
//

#ifndef keybag_helpers_h
#define keybag_helpers_h

#ifdef __cplusplus
extern "C" {
#endif

#include <bsm/audit.h>
#include <mach/message.h>
#include <libaks.h>

enum {
    KB_Success      = 0,
    KB_GeneralError,
    KB_BagNotFound,
    KB_BagError,
    KB_BagNotLoaded,
    KB_BagExists,
    KB_InvalidSession,
    KB_Unsupported,
};

typedef struct {
    au_asid_t s_id;
    uid_t s_uid;
    audit_token_t procToken;
    uint64_t kcv;
} service_context_t;

int kb_create(service_context_t *context, const void * secret, int secret_len);
int kb_load(service_context_t *context);
int kb_load_uid(uid_t uid);
int kb_unload(service_context_t *context);
int kb_save(service_context_t *context);
int kb_unlock(service_context_t *context, const void * secret, int secret_len);
int kb_lock(service_context_t *context);
int kb_change_secret(service_context_t *context, const void * secret, int secret_len, const void * new_secret, int new_secret_len);
int kb_is_locked(service_context_t *context, bool *locked, bool *no_pin);
int kb_reset(service_context_t *context, const void * secret, int secret_len);
int kb_wrap_key(service_context_t *context, const void *key, int key_size, keyclass_t key_class, void **wrapped_key, int *wrapped_key_size, keyclass_t *wrapped_key_class);
int kb_unwrap_key(service_context_t *context, const void *wrapped_key, int wrapped_key_size, keyclass_t wrapped_key_class, void **key, int *key_size);

int kb_stash_set_key(service_context_t *context, const void * key, int key_len);
int kb_stash_load_key(service_context_t *context, const void * key, int key_len);
int kb_stash_get_key(service_context_t *context, void ** key, size_t * key_len);

#ifdef __cplusplus
}
#endif


#endif /* keybag_helpers_h */
