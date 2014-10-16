/* Copyright (c) 2012 Apple Inc. All Rights Reserved. */

#ifndef _SECURITY_AUTH_DEBUGGING_H_
#define _SECURITY_AUTH_DEBUGGING_H_

#if defined(__cplusplus)
extern "C" {
#endif

enum {
    AUTH_LOG_NORMAL,
    AUTH_LOG_VERBOSE,
    AUTH_LOG_ERROR
};
    
#define LOG(...) security_auth_log(AUTH_LOG_NORMAL, ##__VA_ARGS__)
#define LOGV(...) security_auth_log(AUTH_LOG_VERBOSE, ##__VA_ARGS__)
#define LOGE(...) security_auth_log(AUTH_LOG_ERROR, ##__VA_ARGS__)
#if DEBUG
#define LOGD(...) security_auth_log(AUTH_LOG_VERBOSE, ##__VA_ARGS__)
#else
#define LOGD(...) 
#endif
    
void security_auth_log(int,const char *,...) __printflike(2, 3);

#define CFReleaseSafe(CF) { CFTypeRef _cf = (CF); if (_cf) CFRelease(_cf); }
#define CFReleaseNull(CF) { CFTypeRef _cf = (CF); \
    if (_cf) { (CF) = NULL; CFRelease(_cf); } }
#define CFRetainSafe(CF) { CFTypeRef _cf = (CF); if (_cf) CFRetain(_cf); }
    
#define xpc_release_safe(obj)  if (obj) { xpc_release(obj); obj = NULL; }
#define free_safe(obj)  if (obj) { free(obj); obj = NULL; }

void _show_cf(CFTypeRef);
    
#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_AUTH_DEBUGGING_H_ */
