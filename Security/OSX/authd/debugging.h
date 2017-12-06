/* Copyright (c) 2012 Apple Inc. All Rights Reserved. */

#ifndef _SECURITY_AUTH_DEBUGGING_H_
#define _SECURITY_AUTH_DEBUGGING_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include <os/log.h>
#include <os/activity.h>

#define AUTHD_DEFINE_LOG \
static os_log_t AUTHD_LOG_DEFAULT() { \
static dispatch_once_t once; \
static os_log_t log; \
dispatch_once(&once, ^{ log = os_log_create("com.apple.Authorization", "authd"); }); \
return log; \
};

#define AUTHD_LOG AUTHD_LOG_DEFAULT()

#define CFReleaseSafe(CF) { CFTypeRef _cf = (CF); if (_cf) CFRelease(_cf); }
#define CFReleaseNull(CF) { CFTypeRef _cf = (CF); \
    if (_cf) { (CF) = NULL; CFRelease(_cf); } }
#define CFRetainSafe(CF) { CFTypeRef _cf = (CF); if (_cf) CFRetain(_cf); }
#define CFAssignRetained(VAR,CF) ({ \
__typeof__(VAR) *const _pvar = &(VAR); \
__typeof__(CF) _cf = (CF); \
(*_pvar) = *_pvar ? (CFRelease(*_pvar), _cf) : _cf; \
})

#define xpc_release_safe(obj)  if (obj) { xpc_release(obj); obj = NULL; }
#define free_safe(obj)  if (obj) { free(obj); obj = NULL; }
    
#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_AUTH_DEBUGGING_H_ */
