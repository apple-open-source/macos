/* Copyright (c) 2012 Apple Inc. All rights reserved. */

#ifndef _SECURITY_AUTH_AUTHTOKEN_H_
#define _SECURITY_AUTH_AUTHTOKEN_H_

#include "credential.h"
#include <CoreFoundation/CoreFoundation.h>

#if defined(__cplusplus)
extern "C" {
#endif

enum {
    auth_token_state_zombie     = 1 << 0,
    auth_token_state_registered = 1 << 1
};
typedef uint32_t auth_token_state_t;

extern const CFDictionaryKeyCallBacks kAuthTokenKeyCallBacks;
    
AUTH_WARN_RESULT AUTH_MALLOC AUTH_NONNULL_ALL AUTH_RETURNS_RETAINED
auth_token_t auth_token_create(process_t,bool operateAsLeastPrivileged);

AUTH_WARN_RESULT AUTH_MALLOC AUTH_NONNULL_ALL AUTH_RETURNS_RETAINED
auth_token_t auth_token_create_with_audit_info(const audit_info_s*,bool operateAsLeastPrivileged);
    
AUTH_NONNULL_ALL
bool auth_token_get_sandboxed(auth_token_t);
    
AUTH_NONNULL_ALL
const char * auth_token_get_code_url(auth_token_t);
    
AUTH_NONNULL_ALL
const void * auth_token_get_key(auth_token_t);

AUTH_NONNULL_ALL
auth_items_t auth_token_get_context(auth_token_t);

AUTH_NONNULL_ALL
bool auth_token_least_privileged(auth_token_t);
    
AUTH_NONNULL_ALL
uid_t auth_token_get_uid(auth_token_t);
    
AUTH_NONNULL_ALL
pid_t auth_token_get_pid(auth_token_t);
    
AUTH_NONNULL_ALL
session_t auth_token_get_session(auth_token_t);
    
AUTH_NONNULL_ALL
const AuthorizationBlob * auth_token_get_blob(auth_token_t);
    
AUTH_NONNULL_ALL
const audit_info_s * auth_token_get_audit_info(auth_token_t);

AUTH_NONNULL_ALL
mach_port_t auth_token_get_creator_bootstrap(auth_token_t auth);
    
AUTH_NONNULL_ALL
CFIndex auth_token_add_process(auth_token_t,process_t);

AUTH_NONNULL_ALL
CFIndex auth_token_remove_process(auth_token_t,process_t);

AUTH_NONNULL_ALL
CFIndex auth_token_get_process_count(auth_token_t);
    
AUTH_NONNULL_ALL
void auth_token_set_credential(auth_token_t,credential_t);
    
AUTH_NONNULL_ALL
bool auth_token_credentials_iterate(auth_token_t, credential_iterator_t iter);

AUTH_NONNULL_ALL
void auth_token_set_right(auth_token_t,credential_t);
    
AUTH_NONNULL_ALL
bool auth_token_rights_iterate(auth_token_t, credential_iterator_t iter);

AUTH_NONNULL_ALL
CFTypeRef auth_token_copy_entitlement_value(auth_token_t, const char * entitlement);
    
AUTH_NONNULL_ALL
bool auth_token_has_entitlement(auth_token_t, const char * entitlement);

AUTH_NONNULL_ALL
bool auth_token_has_entitlement_for_right(auth_token_t, const char * right);

AUTH_NONNULL_ALL
credential_t auth_token_get_credential(auth_token_t);

AUTH_NONNULL_ALL
bool auth_token_apple_signed(auth_token_t);

AUTH_NONNULL_ALL
bool auth_token_is_creator(auth_token_t,process_t);

AUTH_NONNULL_ALL
void auth_token_set_state(auth_token_t,auth_token_state_t);

AUTH_NONNULL_ALL
void auth_token_clear_state(auth_token_t,auth_token_state_t);

AUTH_WARN_RESULT AUTH_NONNULL_ALL
auth_token_state_t auth_token_get_state(auth_token_t);

AUTH_NONNULL_ALL
bool auth_token_check_state(auth_token_t, auth_token_state_t);

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_AUTH_AUTHTOKEN_H_ */
