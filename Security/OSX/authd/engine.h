/* Copyright (c) 2012-2013 Apple Inc. All Rights Reserved. */

#ifndef _SECURITY_AUTH_ENGINE_H_
#define _SECURITY_AUTH_ENGINE_H_

#include "credential.h"
#include <Security/Authorization.h>

#if defined(__cplusplus)
extern "C" {
#endif

AUTH_WARN_RESULT AUTH_MALLOC AUTH_NONNULL_ALL AUTH_RETURNS_RETAINED
engine_t engine_create(connection_t, auth_token_t);

AUTH_NONNULL1 AUTH_NONNULL2
OSStatus engine_authorize(engine_t, auth_rights_t rights, auth_items_t environment, AuthorizationFlags);

AUTH_NONNULL1 AUTH_NONNULL2
OSStatus engine_get_right_properties(engine_t engine, const char *rightName, CFDictionaryRef *output);

AUTH_NONNULL_ALL
OSStatus engine_verify_modification(engine_t, rule_t, bool remove, bool force_modify);

AUTH_NONNULL_ALL
auth_rights_t engine_get_granted_rights(engine_t);

AUTH_NONNULL_ALL
auth_token_t engine_get_auth_token(engine_t);

AUTH_NONNULL_ALL
CFAbsoluteTime engine_get_time(engine_t);
    
AUTH_NONNULL_ALL
void engine_destroy_agents(engine_t);
    
AUTH_NONNULL_ALL
void engine_interrupt_agent(engine_t engine);

AUTH_NONNULL_ALL
bool engine_acquire_sheet_data(engine_t engine);

AUTH_NONNULL_ALL AUTH_RETURNS_RETAINED
CFTypeRef engine_copy_context(engine_t engine, auth_items_t source);

AUTH_NONNULL_ALL
bool engine_should_use_lw_session(engine_t engine);

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_AUTH_ENGINE_H_ */
