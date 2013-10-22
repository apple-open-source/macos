/* Copyright (c) 2012 Apple Inc. All rights reserved. */

#ifndef _SECURITY_AUTH_CREDENTIAL_H_
#define _SECURITY_AUTH_CREDENTIAL_H_

#if defined(__cplusplus)
extern "C" {
#endif

#ifdef __BLOCKS__
    typedef bool (^credential_iterator_t)(credential_t);
#endif /* __BLOCKS__ */
    
AUTH_WARN_RESULT AUTH_MALLOC AUTH_NONNULL_ALL AUTH_RETURNS_RETAINED
credential_t credential_create(uid_t);

AUTH_WARN_RESULT AUTH_MALLOC AUTH_NONNULL_ALL AUTH_RETURNS_RETAINED    
credential_t credential_create_with_credential(credential_t,bool);
    
AUTH_WARN_RESULT AUTH_MALLOC AUTH_NONNULL_ALL AUTH_RETURNS_RETAINED    
credential_t credential_create_with_right(const char *);

AUTH_NONNULL_ALL
uid_t credential_get_uid(credential_t);

AUTH_NONNULL_ALL
const char * credential_get_name(credential_t);

AUTH_NONNULL_ALL
const char * credential_get_realname(credential_t);
    
AUTH_NONNULL_ALL
CFAbsoluteTime credential_get_creation_time(credential_t);
    
AUTH_NONNULL_ALL
bool credential_get_valid(credential_t);

AUTH_NONNULL_ALL    
bool credential_get_shared(credential_t);
    
AUTH_NONNULL_ALL
bool credential_is_right(credential_t);

AUTH_NONNULL_ALL    
bool credential_check_membership(credential_t,const char*);
    
AUTH_NONNULL_ALL
void credential_invalidate(credential_t);
    
#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_AUTH_CREDENTIAL_H_ */
