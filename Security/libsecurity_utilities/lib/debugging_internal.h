//
//  debugging_internal.h
//  libsecurity_utilities
//
//  Created by ohjelmoija on 11/27/12.
//
//

#ifndef libsecurity_utilities_debugging_internal_h
#define libsecurity_utilities_debugging_internal_h


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

//
// Include DTrace static probe definitions
//
typedef const void *DTException;

#include <security_utilities/utilities_dtrace.h>

//
// The debug-log macro is now unconditionally emitted as a DTrace static probe point.
//

void secdebug_internal(const char* scope, const char* format, ...);

#define secdebug(scope, format...) secdebug_internal(scope, format)
#define secdebugf(scope, __msg)	SECURITY_DEBUG_LOG((char *)(scope), (__msg))

//
// The old secdelay() macro is also emitted as a DTrace probe (use destructive actions to handle this).
// Secdelay() should be considered a legacy feature; just put a secdebug at the intended delay point.
//
#define secdelay(file)	SECURITY_DEBUG_DELAY((char *)(file))


#ifdef __cplusplus
};
#endif // __cplusplus

#endif
