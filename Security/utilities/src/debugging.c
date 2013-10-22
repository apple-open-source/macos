/*
 * Copyright (c) 2006-2010 Apple Inc. All Rights Reserved.
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
 * debugging.c - non-trivial debug support
 */
#include "utilities/debugging.h"
#include "utilities/SecCFWrappers.h"
#include <CoreFoundation/CFSet.h>
#include <CoreFoundation/CFString.h>

#include <dispatch/dispatch.h>

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <asl.h>

/** begin: For SimulateCrash **/
#include <dlfcn.h>
#include <mach/mach.h>
/// Type to represent a boolean value.
#if TARGET_OS_IPHONE  &&  __LP64__
typedef bool BOOL;
#else
typedef signed char BOOL;
// BOOL is explicitly signed so @encode(BOOL) == "c" rather than "C"
// even if -funsigned-char is used.
#endif
/** end: For SimulateCrash **/

#define MAX_SCOPE_LENGTH  12

#if !defined(NDEBUG)
static CFStringRef copyScopeName(const char *scope, CFIndex scopeLen) {
	if (scopeLen > MAX_SCOPE_LENGTH)
		scopeLen = MAX_SCOPE_LENGTH - 1;
	return CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)scope,
		scopeLen, kCFStringEncodingUTF8, false);
}

pthread_once_t __security_debug_once = PTHREAD_ONCE_INIT;
static const char *gDebugScope;
static CFMutableSetRef scopeSet;
static bool negate = false;

static void __security_debug_init(void) {
	const char *cur_scope = gDebugScope = getenv("DEBUGSCOPE");
	if (cur_scope) {
		if (!strcmp(cur_scope, "all")) {
			scopeSet = NULL;
			negate = true;
		} else if (!strcmp(cur_scope, "none")) {
			scopeSet = NULL;
			negate = false;
		} else {
			scopeSet = CFSetCreateMutable(kCFAllocatorDefault, 0,
				&kCFTypeSetCallBacks);
			if (cur_scope[0] == '-') {
				negate = true;
				cur_scope++;
			} else {
				negate = false;
			}

			const char *sep;
			while ((sep = strchr(cur_scope, ','))) {
				CFStringRef scopeName = copyScopeName(cur_scope,
					sep - cur_scope);
				CFSetAddValue(scopeSet, scopeName);
				CFRelease(scopeName);
				cur_scope = sep + 1;
			}

			CFStringRef scopeName = copyScopeName(cur_scope,
				strlen(cur_scope));
			CFSetAddValue(scopeSet, scopeName);
			CFRelease(scopeName);
		}
	} else {
		scopeSet = NULL;
		negate = false;
	}
}

#endif

static CFMutableArrayRef sSecurityLogHandlers;

static CFMutableArrayRef get_log_handlers()
{
    static dispatch_once_t handlers_once;
    
    dispatch_once(&handlers_once, ^{
        sSecurityLogHandlers = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        
        CFArrayAppendValue(sSecurityLogHandlers, ^(const char *level, CFStringRef scope, const char *function,
                                                   const char *file, int line, CFStringRef message){
            CFStringRef logStr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@ %s %@\n"), scope ? scope : CFSTR(""), function, message);
            CFStringPerformWithCString(logStr, ^(const char *logMsg) {
                aslmsg msg = asl_new(ASL_TYPE_MSG);
                if (scope) {
                    CFStringPerformWithCString(scope, ^(const char *scopeStr) {
                        asl_set(msg, ASL_KEY_FACILITY, scopeStr);
                    });
                }
                asl_set(msg, ASL_KEY_LEVEL, level);
                asl_set(msg, ASL_KEY_MSG, logMsg);
                asl_send(NULL, msg);
                asl_free(msg);
            });
            CFReleaseSafe(logStr);
        });
    });
    
    return sSecurityLogHandlers;
}

static void clean_aslclient(void *client)
{
    asl_close(client);
}

static aslclient get_aslclient()
{
    static dispatch_once_t once;
    static pthread_key_t asl_client_key;
    dispatch_once(&once, ^{
        pthread_key_create(&asl_client_key, clean_aslclient);
    });
    aslclient client = pthread_getspecific(asl_client_key);
    if (!client) {
        client = asl_open(NULL, "SecAPI", 0);
        asl_set_filter(client, ASL_FILTER_MASK_UPTO(ASL_LEVEL_DEBUG));
        pthread_setspecific(asl_client_key, client);
    }
    
    return client;
}

void __security_trace_enter_api(const char *api, CFStringRef format, ...)
{
    aslmsg msg = asl_new(ASL_TYPE_MSG);
    asl_set(msg, ASL_KEY_LEVEL, ASL_STRING_DEBUG);
    asl_set(msg, "SecAPITrace", api);
    asl_set(msg, "ENTER", "");
	va_list args;
	va_start(args, format);
    if (format) {
        CFStringRef message = CFStringCreateWithFormatAndArguments(kCFAllocatorDefault, NULL, format, args);
        va_end(args);
        CFStringPerformWithCString(message, ^(const char *utf8Str) {
            asl_set(msg, ASL_KEY_MSG, utf8Str);
        });
        CFReleaseSafe(message);
    }
    
    {
        char stack_info[80];
        
        snprintf(stack_info, sizeof(stack_info), "C%p F%p", __builtin_return_address(1), __builtin_frame_address(2));
        asl_set(msg, "CALLER", stack_info);
    }
    
    asl_send(get_aslclient(), msg);
    asl_free(msg);
}

void __security_trace_return_api(const char *api, CFStringRef format, ...)
{
    aslmsg msg = asl_new(ASL_TYPE_MSG);
    asl_set(msg, ASL_KEY_LEVEL, ASL_STRING_DEBUG);
    asl_set(msg, "SecAPITrace", api);
    asl_set(msg, "RETURN", "");
	va_list args;
	va_start(args, format);
    if (format) {
        CFStringRef message = CFStringCreateWithFormatAndArguments(kCFAllocatorDefault, NULL, format, args);
        va_end(args);
        CFStringPerformWithCString(message, ^(const char *utf8Str) {
            asl_set(msg, ASL_KEY_MSG, utf8Str);
        });
        CFReleaseSafe(message);
    }
    asl_send(get_aslclient(), msg);
    asl_free(msg);
}


void add_security_log_hanlder(security_log_handler handler)
{
    CFArrayAppendValue(get_log_handlers(), handler);
}

static void __security_log_msg(const char *level, CFStringRef scope, const char *function,
                    const char *file, int line, CFStringRef message)
{
    
    CFArrayForEach(get_log_handlers(), ^(const void *value) {
        security_log_handler handler = (security_log_handler) value;
        
        handler(level, scope, function, file, line, message);
    });
}

void __security_debug(CFStringRef scope, const char *function,
                      const char *file, int line, CFStringRef format, ...)
{
#if !defined(NDEBUG)
	pthread_once(&__security_debug_once, __security_debug_init);

    /* Check if scope is enabled. */
    if (scope && ((scopeSet && negate == CFSetContainsValue(scopeSet, scope)) ||
                  (!scopeSet && !negate)))
        return;
#endif

	va_list args;
	va_start(args, format);
	CFStringRef message = CFStringCreateWithFormatAndArguments(
        kCFAllocatorDefault, NULL, format, args);
	va_end(args);

    /* DEBUG scopes are logged as notice when enabled. */
    __security_log_msg(ASL_STRING_NOTICE, scope, function, file, line, message);
    CFRelease(message);
}

void __security_log(const char *level, CFStringRef scope, const char *function,
    const char *file, int line, CFStringRef format, ...)
{
	va_list args;
	va_start(args, format);
	CFStringRef message = CFStringCreateWithFormatAndArguments(
		kCFAllocatorDefault, NULL, format, args);
	va_end(args);
    __security_log_msg(level, scope, function, file, line, message);
    CFRelease(message);
}

static void __security_simulatecrash_link(CFStringRef reason, uint32_t code)
{
#if !TARGET_IPHONE_SIMULATOR
    // Prototype defined in <CrashReporterSupport/CrashReporterSupport.h>, but objC only.
    // Soft linking here so we don't link unless we hit this.
    static BOOL (*__SimulateCrash)(pid_t pid, mach_exception_data_type_t exceptionCode, CFStringRef description);

    static dispatch_once_t once = 0;
    dispatch_once(&once, ^{
        void *image = dlopen("/System/Library/PrivateFrameworks/CrashReporterSupport.framework/CrashReporterSupport", RTLD_NOW);
        if (image)
            __SimulateCrash = dlsym(image, "SimulateCrash");
        else
            __SimulateCrash = NULL;
    });

    if (__SimulateCrash)
        __SimulateCrash(getpid(), code, reason);
    else
        secerror("SimulateCrash not available");
#else
    secerror("SimulateCrash not available in iOS simulator");
#endif
}


void __security_simulatecrash(CFStringRef reason, uint32_t code)
{
    secerror("Simulating crash, reason: %@, code=%08x", reason, code);
    __security_simulatecrash_link(reason, code);
}
