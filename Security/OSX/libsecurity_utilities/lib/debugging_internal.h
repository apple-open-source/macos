/*
 * Copyright (c) 2012,2014 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


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
