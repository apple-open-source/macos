/*
 * Copyright (c) 2006-2007,2009-2010,2012-2014 Apple Inc. All Rights Reserved.
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

/*
 * debugging.h - non-trivial debug support
 */
#ifndef _SECURITY_UTILITIES_DEBUGGING_H_
#define _SECURITY_UTILITIES_DEBUGGING_H_

#ifdef KERNEL
        #include <libkern/libkern.h>
        #define secalert(format, ...) printf((format), ## __VA_ARGS__)
        #define secemergency(format, ...) printf((format), ## __VA_ARGS__)
        #define seccritical(format, ...) printf((format), ## __VA_ARGS__)
        #define secerror(format, ...) printf((format), ## __VA_ARGS__)
        #define secwarning(format, ...) printf((format), ## __VA_ARGS__)
        #define secnotice(scope, format, ...) printf((format), ## __VA_ARGS__)
        #define secnoticeq(scope, format, ...) printf((format), ## __VA_ARGS__)
        #define secinfo(scope, format, ...) printf((format), ## __VA_ARGS__)
    #if !defined(NDEBUG)
        #define secdebug(scope, format, ...) printf((format), ## __VA_ARGS__)
    #else // NDEBUG
        #define secdebug(scope, format, ...) 	/* nothing */
    #endif // NDEBUG
#else // !KERNEL

#include <TargetConditionals.h>
#include <CoreFoundation/CFString.h>
#include <asl.h>

__BEGIN_DECLS

extern void __security_trace_enter_api(const char *api, CFStringRef format, ...) CF_FORMAT_FUNCTION(2, 3);
extern void __security_trace_return_api(const char *api, CFStringRef format, ...) CF_FORMAT_FUNCTION(2, 3);

extern void __security_debug(CFStringRef scope,
                             const char *function, const char *file, int line,
                             CFStringRef format, ...) CF_FORMAT_FUNCTION(5,6);

extern void __security_log(int level, CFStringRef scope,
                           const char *function, const char *file, int line,
                           CFStringRef format, ...) CF_FORMAT_FUNCTION(6,7);

#define sec_trace_enter_api(format...) __security_trace_enter_api(__FUNCTION__, format)
#define sec_trace_return_api(rtype, body, format...) { rtype _r = body(); __security_trace_return_api(__FUNCTION__, format, _r); return _r; }
#define sec_trace_return_bool_api(body, format...) { bool _r = body(); typeof(format) _fmt = format; __security_trace_return_api(__FUNCTION__, _fmt ? _fmt : CFSTR("return=%d"), (int)_r); return _r; }

#define secemergency(format, ...)	__security_log(ASL_LEVEL_EMERG, NULL, \
    __FUNCTION__, __FILE__, __LINE__, \
    CFSTR(format), ## __VA_ARGS__)

#define secalert(format, ...)	__security_log(ASL_LEVEL_ALERT, NULL, \
    __FUNCTION__, __FILE__, __LINE__, \
    CFSTR(format), ## __VA_ARGS__)

#define seccritical(format, ...)	__security_log(ASL_LEVEL_CRIT, NULL, \
    __FUNCTION__, __FILE__, __LINE__, \
    CFSTR(format), ## __VA_ARGS__)

#define secerror(format, ...)	__security_log(ASL_LEVEL_ERR, NULL, \
    __FUNCTION__, __FILE__, __LINE__, \
    CFSTR(format), ## __VA_ARGS__)

#define secerrorq(format, ...)	__security_log(ASL_LEVEL_ERR, NULL, \
    "", "", 0, \
    CFSTR(format), ## __VA_ARGS__)

#define secwarning(format, ...)	__security_log(ASL_LEVEL_WARNING, NULL, \
    __FUNCTION__, __FILE__, __LINE__, \
    CFSTR(format), ## __VA_ARGS__)

#define secnotice(scope, format, ...)	__security_log(ASL_LEVEL_NOTICE, CFSTR(scope), \
    __FUNCTION__, __FILE__, __LINE__, \
    CFSTR(format), ## __VA_ARGS__)

#define secnoticeq(scope, format, ...)	__security_log(ASL_LEVEL_NOTICE, CFSTR(scope), \
    "", "", 0, \
    CFSTR(format), ## __VA_ARGS__)

#define secinfo(scope, format, ...)	__security_log(ASL_LEVEL_INFO, CFSTR(scope), \
    __FUNCTION__, __FILE__, __LINE__, \
    CFSTR(format), ## __VA_ARGS__)

#if !defined(NDEBUG)

# define secdebug(scope,format, ...)	__security_debug(CFSTR(scope), \
    __FUNCTION__, __FILE__, __LINE__, \
    CFSTR(format), ## __VA_ARGS__)
#else
# define secdebug(scope,...)	/* nothing */
#endif

typedef void (^security_log_handler)(int level, CFStringRef scope, const char *function,
                                     const char *file, int line, CFStringRef message);

void add_security_log_handler(security_log_handler handler);
void remove_security_log_handler(security_log_handler handler);

/* To simulate a process crash in some conditions */
void __security_simulatecrash(CFStringRef reason, uint32_t code);

/* predefined simulate crash exception codes */
#define __sec_exception_code(x) (0x53c00000+x)
#define __sec_exception_code_CorruptDb(db,rc)       __sec_exception_code(1|((db)<<8)|((rc)<<16))
#define __sec_exception_code_CorruptItem            __sec_exception_code(2)
#define __sec_exception_code_OTRError               __sec_exception_code(3)
#define __sec_exception_code_DbItemDescribe         __sec_exception_code(4)
#define __sec_exception_code_TwiceCorruptDb(db)     __sec_exception_code(5|((db)<<8))

/* Logging control functions */

typedef enum {
    kScopeIDEnvironment = 0,
    kScopeIDDefaults = 1,
    kScopeIDConfig = 2,
    kScopeIDXPC = 3,
    kScopeIDMax = 3,
} SecDebugScopeID;

void ApplyScopeListForID(CFStringRef scopeList, SecDebugScopeID whichID);
void ApplyScopeDictionaryForID(CFDictionaryRef scopeList, SecDebugScopeID whichID);
CFPropertyListRef CopyCurrentScopePlist(void);

__END_DECLS

#endif // !KERNEL

#endif /* _SECURITY_UTILITIES_DEBUGGING_H_ */
