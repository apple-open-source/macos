/* Copyright (c) 2012 Apple Inc. All Rights Reserved. */

#ifndef _SECURITY_AUTH_DEBUGGING_H_
#define _SECURITY_AUTH_DEBUGGING_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include <os/log.h>
#include <os/activity.h>

#define AUTHD_DEFINE_LOG \
static os_log_t AUTHD_LOG_DEFAULT(void) { \
static dispatch_once_t once; \
static os_log_t log; \
dispatch_once(&once, ^{ log = os_log_create("com.apple.Authorization", "authd"); }); \
return log; \
};

#define AUTHD_LOG AUTHD_LOG_DEFAULT()

/*
 * gDebugOrDefaultInRecovery - Log level for AIR (Authorization In Recovery) database operations.
 * Set once at startup in main(), read-only thereafter.
 * In normal boot: OS_LOG_TYPE_DEBUG (logs not captured by default)
 * In FVUnlock/Recovery: OS_LOG_TYPE_DEFAULT (logs captured for diagnostics)
 */
extern os_log_type_t gDebugOrDefaultInRecovery;

#define os_log_debug_air(format, ...) os_log_with_type(AUTHD_LOG, gDebugOrDefaultInRecovery, format, ##__VA_ARGS__)

#ifndef CFReleaseSafe
#define CFReleaseSafe(CF) { CFTypeRef _cf = (CF); if (_cf) CFRelease(_cf); }
#endif
#ifndef CFReleaseNull
#define CFReleaseNull(CF) { CFTypeRef _cf = (CF); \
    if (_cf) { (CF) = NULL; CFRelease(_cf); } }
#endif
#ifndef CFRetainSafe
#define CFRetainSafe(CF) { CFTypeRef _cf = (CF); if (_cf) CFRetain(_cf); }
#endif
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
