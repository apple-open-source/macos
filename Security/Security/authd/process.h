/* Copyright (c) 2012-2013 Apple Inc. All Rights Reserved. */

#ifndef _SECURITY_AUTH_PROCESS_H_
#define _SECURITY_AUTH_PROCESS_H_

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCode.h>
#include <mach/mach.h>

#if defined(__cplusplus)
extern "C" {
#endif
    
AUTH_WARN_RESULT AUTH_MALLOC AUTH_NONNULL_ALL AUTH_RETURNS_RETAINED
process_t process_create(const audit_info_s*,session_t);

AUTH_NONNULL_ALL
const void * process_get_key(process_t);
    
AUTH_NONNULL_ALL
uid_t process_get_uid(process_t);

AUTH_NONNULL_ALL
pid_t process_get_pid(process_t);

AUTH_NONNULL_ALL
int32_t process_get_generation(process_t);
    
AUTH_NONNULL_ALL
session_id_t process_get_session_id(process_t);

AUTH_NONNULL_ALL
session_t process_get_session(process_t);

AUTH_NONNULL_ALL
uint32_t process_get_count(process_t);

AUTH_NONNULL_ALL
const audit_info_s * process_get_audit_info(process_t);
    
AUTH_NONNULL_ALL    
SecCodeRef process_get_code(process_t);
    
AUTH_NONNULL_ALL    
const char * process_get_code_url(process_t);
    
AUTH_NONNULL_ALL
CFIndex process_add_connection(process_t, connection_t);

AUTH_NONNULL_ALL
CFIndex process_remove_connection(process_t, connection_t);

AUTH_NONNULL_ALL
CFIndex process_get_connection_count(process_t);
    
AUTH_NONNULL_ALL
void process_add_auth_token(process_t,auth_token_t);

AUTH_NONNULL_ALL
void process_remove_auth_token(process_t,auth_token_t, uint32_t flags);

AUTH_NONNULL_ALL
auth_token_t process_find_copy_auth_token(process_t,const AuthorizationBlob*);

AUTH_NONNULL_ALL
CFIndex process_get_auth_token_count(process_t);

AUTH_NONNULL_ALL
CFTypeRef process_copy_entitlement_value(process_t, const char * entitlement);
    
AUTH_NONNULL_ALL
bool process_has_entitlement(process_t, const char * entitlement);
    
AUTH_NONNULL_ALL
bool process_has_entitlement_for_right(process_t, const char * right);

AUTH_NONNULL_ALL
const char * process_get_identifier(process_t);

AUTH_NONNULL_ALL
CFDataRef process_get_requirement_data(process_t);
    
AUTH_NONNULL_ALL
SecRequirementRef process_get_requirement(process_t);
    
AUTH_NONNULL_ALL
bool process_verify_requirment(process_t,SecRequirementRef);

AUTH_NONNULL_ALL
bool process_apple_signed(process_t proc);

AUTH_NONNULL_ALL
mach_port_t process_get_bootstrap(process_t);
    
AUTH_NONNULL_ALL
bool process_set_bootstrap(process_t, mach_port_t);
    
#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_AUTH_PROCESS_H_ */
