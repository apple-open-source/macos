/* Copyright (c) 2012 Apple Inc. All Rights Reserved. */

#ifndef _SECURITY_AUTH_SESSION_H_
#define _SECURITY_AUTH_SESSION_H_

#include "credential.h"
#include <Security/AuthSession.h>
#include <bsm/audit_session.h>

#if defined(__cplusplus)
extern "C" {
#endif
    
AUTH_WARN_RESULT AUTH_MALLOC AUTH_NONNULL_ALL AUTH_RETURNS_RETAINED
session_t session_create(session_id_t);

AUTH_NONNULL_ALL
bool session_update(session_t);

AUTH_NONNULL_ALL
uint64_t session_get_attributes(session_t);
    
AUTH_NONNULL_ALL
void session_set_attributes(session_t,uint64_t flags);

AUTH_NONNULL_ALL
void session_clear_attributes(session_t,uint64_t flags);
    
AUTH_NONNULL_ALL
const void * session_get_key(session_t);
    
AUTH_NONNULL_ALL
session_id_t session_get_id(session_t);
    
AUTH_NONNULL_ALL
uid_t session_get_uid(session_t);

AUTH_NONNULL_ALL
CFIndex session_add_process(session_t, process_t);

AUTH_NONNULL_ALL
CFIndex session_remove_process(session_t, process_t);

AUTH_NONNULL_ALL
CFIndex session_get_process_count(session_t);

AUTH_NONNULL_ALL
void session_set_credential(session_t,credential_t);

AUTH_NONNULL_ALL
void session_credentials_purge(session_t);
    
AUTH_NONNULL_ALL
bool session_credentials_iterate(session_t, credential_iterator_t iter);
    
#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_AUTH_SESSION_H_ */
