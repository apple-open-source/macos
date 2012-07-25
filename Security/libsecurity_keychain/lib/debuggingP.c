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
#include <security_utilities/debugging.h>
#include <CoreFoundation/CFSet.h>
#include <CoreFoundation/CFString.h>

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <asl.h>

#if !defined(NDEBUG)
#define MAX_SCOPE_LENGTH  12

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

void __security_debug(const char *scope, const char *function,
    const char *file, int line, const char *format, ...)
{
#if !defined(NDEBUG)
	pthread_once(&__security_debug_once, __security_debug_init);

	CFStringRef scopeName = NULL;
	/* Scope NULL is always enabled. */
	if (scope) {
		/* Check if the scope is enabled. */
		if (scopeSet) {
			scopeName = copyScopeName(scope, strlen(scope));
			if (negate == CFSetContainsValue(scopeSet, scopeName)) {
				CFRelease(scopeName);
				return;
			}
		} else if (!negate) {
			return;
		}
	}

	CFStringRef formatStr = CFStringCreateWithCString(kCFAllocatorDefault,
		format, kCFStringEncodingUTF8);
	va_list args;
	va_start(args, format);
	CFStringRef message = CFStringCreateWithFormatAndArguments(
		kCFAllocatorDefault, NULL, formatStr, args);
	va_end(args);
	time_t now = time(NULL);
	char *date = ctime(&now);
	date[19] = '\0';
	CFStringRef logStr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
		CFSTR("%s %-*s %s %@\n"), date + 4, MAX_SCOPE_LENGTH - 1,
        scope ? scope : "", function, message);
	CFShow(logStr);
    char logMsg[4096];
    if (CFStringGetCString(logStr, logMsg, sizeof(logMsg), kCFStringEncodingUTF8)) {
#if 0
        asl_log(NULL, NULL, ASL_LEVEL_INFO, logMsg);
#else
        aslmsg msg = asl_new(ASL_TYPE_MSG);
        if (scope) {
            asl_set(msg, ASL_KEY_FACILITY, scope);
        }
        asl_set(msg, ASL_KEY_LEVEL, ASL_STRING_INFO);
        asl_set(msg, ASL_KEY_MSG, logMsg);
        asl_send(NULL, msg);
        asl_free(msg);
#endif
    }
	CFRelease(logStr);
	CFRelease(message);
	CFRelease(formatStr);
	if (scopeName)
		CFRelease(scopeName);
#endif
}
