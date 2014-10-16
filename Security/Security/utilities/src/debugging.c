/*
 * Copyright (c) 2006-2010,2012-2014 Apple Inc. All Rights Reserved.
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
#include "utilities/debugging_test.h"
#include "utilities/SecCFWrappers.h"
#include <CoreFoundation/CFSet.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFPreferences.h>

#include <dispatch/dispatch.h>

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <asl.h>


const CFStringRef kStringNegate = CFSTR("-");
const CFStringRef kStringAll = CFSTR("all");

const CFStringRef kAPIScope = CFSTR("api");

static CFMutableArrayRef sLogSettings = NULL; /* Either sets or dictionaries of level => set. */

static dispatch_queue_t GetDispatchControlQueue(void) {
    static dispatch_queue_t sLoggingScopeControlQueue;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sLoggingScopeControlQueue = dispatch_queue_create("security scope control", DISPATCH_QUEUE_CONCURRENT);
    });
    return sLoggingScopeControlQueue;
}

static void with_scopes_read(dispatch_block_t action) {
    dispatch_sync(GetDispatchControlQueue(), action);
}

static void with_scopes_write(dispatch_block_t action) {
    dispatch_barrier_sync(GetDispatchControlQueue(), action);
}

bool IsScopeActive(int level, CFStringRef scope)
{
    if (scope == NULL)
        return true;

    CFNumberRef level_number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &level);

    __block bool isActive = false;
    with_scopes_read(^{
        if (sLogSettings) {
            CFArrayForEach(sLogSettings, ^(const void *value) {
                CFSetRef setToCheck = NULL;

                if (isSet(value)) {
                    setToCheck = (CFSetRef) value;
                } else if (isDictionary(value)) {
                    CFDictionaryRef levels = (CFDictionaryRef) value;

                    setToCheck = CFDictionaryGetValue(levels, level_number);

                    if (!isSet(setToCheck))
                        setToCheck = NULL;
                }

                if (setToCheck != NULL && !isActive) {
                    bool negated = CFSetContainsValue(setToCheck, kStringNegate);
                    bool inSet = CFSetContainsValue(setToCheck, scope);

                    isActive = negated ^ inSet;
                }
            });
        }
    });

    CFReleaseNull(level_number);

    return isActive;
}

bool IsScopeActiveC(int level, const char *scope)
{
    CFStringRef scopeString = CFStringCreateWithBytes(kCFAllocatorDefault, (const uint8_t*)scope, strlen(scope), kCFStringEncodingUTF8, false);
    bool isActive = IsScopeActive(level, scopeString);
    CFReleaseNull(scopeString);

    return isActive;
}



static CFStringRef copyScopeName(const char *scope, CFIndex scopeLen) {
	return CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)scope,
		scopeLen, kCFStringEncodingUTF8, false);
}

static CFMutableSetRef CopyScopesFromScopeList(CFStringRef scopes) {
    CFMutableSetRef resultSet = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);

    CFStringRef allocated_scope_list = NULL;
    CFStringRef clean_scope_list = scopes;
    bool add_negate = false;

    if (CFStringHasPrefix(scopes, kStringNegate)) {
        allocated_scope_list = CFStringCreateWithSubstring(kCFAllocatorDefault, scopes, CFRangeMake(CFStringGetLength(kStringNegate), CFStringGetLength(scopes) - 1));
        clean_scope_list = allocated_scope_list;
        add_negate = true;
    }

    CFArrayRef commaArray = CFStringCreateArrayBySeparatingStrings(kCFAllocatorDefault, clean_scope_list, CFSTR(","));

    if (commaArray) {
        CFArrayForEach(commaArray, ^(const void *value) {
            if (isString(value)) {
                CFMutableStringRef copy = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, (CFStringRef) value);
                CFStringTrimWhitespace(copy);
                CFSetSetValue(resultSet, copy);
                CFReleaseNull(copy);
            }
        });
    }

    CFSetRemoveValue(resultSet, CFSTR("none"));
    CFSetRemoveValue(resultSet, CFSTR(""));

    if (CFSetContainsValue(resultSet, CFSTR("all"))) {
        CFSetRemoveAllValues(resultSet);
        add_negate = !add_negate;
    }

    if (add_negate)
        CFSetSetValue(resultSet, kStringNegate);

    CFReleaseNull(commaArray);
    CFReleaseNull(allocated_scope_list);

    return resultSet;
}

static CFMutableArrayRef CFArrayCreateMutableForCFTypesFilledWithCFNull(CFAllocatorRef allocator, CFIndex capacity) {
    CFMutableArrayRef result = CFArrayCreateMutableForCFTypesWithCapacity(kCFAllocatorDefault, kScopeIDMax);

    for(int count = 0; count <= capacity; ++count)
        CFArrayAppendValue(result, kCFNull);

    return result;
}

static bool CFArrayIsAll(CFArrayRef array, const void *value)
{
    return CFArrayGetCountOfValue(array, CFRangeMake(0, CFArrayGetCount(array)), value) == CFArrayGetCount(array);
}

static void SetNthScopeSet(int nth, CFTypeRef collection)
{
    with_scopes_write(^{
        if (sLogSettings == NULL) {
            sLogSettings = CFArrayCreateMutableForCFTypesFilledWithCFNull(kCFAllocatorDefault, kScopeIDMax);
        }

        CFArraySetValueAtIndex(sLogSettings, nth, collection);

        if (CFArrayIsAll(sLogSettings, kCFNull)) {
            CFReleaseNull(sLogSettings);
        }
    });
}

static int string_to_log_level(CFStringRef string) {
    if (CFEqual(string, CFSTR(ASL_STRING_EMERG)))
        return ASL_LEVEL_EMERG;
    else if (CFEqual(string, CFSTR(ASL_STRING_ALERT)))
        return ASL_LEVEL_ALERT;
    else if (CFEqual(string, CFSTR(ASL_STRING_CRIT)))
        return ASL_LEVEL_CRIT;
    else if (CFEqual(string, CFSTR(ASL_STRING_ERR)))
        return ASL_LEVEL_ERR;
    else if (CFEqual(string, CFSTR(ASL_STRING_WARNING)))
        return ASL_LEVEL_WARNING;
    else if (CFEqual(string, CFSTR(ASL_STRING_NOTICE)))
        return ASL_LEVEL_NOTICE;
    else if (CFEqual(string, CFSTR(ASL_STRING_INFO)))
        return ASL_LEVEL_INFO;
    else if (CFEqual(string, CFSTR(ASL_STRING_DEBUG)))
        return ASL_LEVEL_DEBUG;
    else
        return -1;
}

static void CFSetAppendValues(CFSetRef set, CFMutableArrayRef appendTo)
{
    CFSetForEach(set, ^(const void *value) {
        CFArrayAppendValue(appendTo, value);
    });
}

static CFMutableArrayRef CFSetOfCFObjectsCopyValues(CFSetRef setOfCFs)
{
    CFMutableArrayRef result = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);

    CFSetForEach(setOfCFs, ^(const void *value) {
        CFArrayAppendValue(result, value);
    });

    return result;
}

CFPropertyListRef CopyCurrentScopePlist(void)
{
    CFMutableArrayRef result = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    with_scopes_read(^{
        CFArrayForEach(sLogSettings, ^(const void *value) {
            if (isSet(value)) {
                CFArrayRef values = CFSetOfCFObjectsCopyValues((CFSetRef) value);
                CFArrayAppendValue(result, values);
                CFReleaseNull(values);
            } else if (isDictionary(value)) {
                CFMutableDictionaryRef levels = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

                CFDictionaryForEach((CFDictionaryRef) value, ^(const void *key, const void *value) {
                    if (isSet(value)) {
                        CFArrayRef values = CFSetOfCFObjectsCopyValues((CFSetRef) value);
                        CFDictionaryAddValue(levels, key, values);
                        CFReleaseNull(values);
                    }
                });

                CFArrayAppendValue(result, levels);
            } else {
                CFArrayAppendValue(result, kCFNull);
            }
        });
    });
    return result;
}

void ApplyScopeDictionaryForID(CFDictionaryRef scopeDictionary, SecDebugScopeID whichID)
{
    CFMutableDictionaryRef dictionary_for_id = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    CFDictionaryForEach(scopeDictionary, ^(const void *key, const void *value) {
        CFSetRef scope_set = NULL;
        CFNumberRef key_number = NULL;
        if (isString(key)) {
            int level = string_to_log_level((CFStringRef) key);

            if (level >= 0)
                key_number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &level);
        } else if (isNumber(key)) {
            key_number = CFRetainSafe(key);
        }

        if (isString(value)) {
            scope_set = CopyScopesFromScopeList(value);
        }

        if (key_number && scope_set)
            CFDictionaryAddValue(dictionary_for_id, key_number, scope_set);

        CFReleaseNull(key_number);
        CFReleaseNull(scope_set);
    });

    if (CFDictionaryGetCount(dictionary_for_id) > 0) {
        SetNthScopeSet(whichID, dictionary_for_id);
    }

    CFReleaseNull(dictionary_for_id);
}

void ApplyScopeListForID(CFStringRef scopeList, SecDebugScopeID whichID)
{
    CFMutableSetRef scopesToUse = CopyScopesFromScopeList(scopeList);

    SetNthScopeSet(whichID, scopesToUse);

    CFReleaseNull(scopesToUse);
}

void ApplyScopeListForIDC(const char *scopeList, SecDebugScopeID whichID) {
    CFStringRef scope_string = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, scopeList, kCFStringEncodingUTF8, kCFAllocatorNull);

    ApplyScopeListForID(scope_string, whichID);

    CFReleaseNull(scope_string);
}

#pragma mark - Log Handlers to catch log information

static CFMutableArrayRef sSecurityLogHandlers;

#if TARGET_OS_IPHONE

/*
 * Instead of using CFPropertyListReadFromFile we use a
 * CFPropertyListCreateWithStream directly
 * here. CFPropertyListReadFromFile() uses
 * CFURLCopyResourcePropertyForKey() andCF pulls in CoreServices for
 * CFURLCopyResourcePropertyForKey() and that doesn't work in install
 * enviroment.
 */

static CFPropertyListRef
CopyPlistFromFile(CFURLRef url)
{
    CFDictionaryRef d = NULL;
    CFReadStreamRef s = CFReadStreamCreateWithFile(kCFAllocatorDefault, url);
    if (s && CFReadStreamOpen(s)) {
	    d = (CFDictionaryRef)CFPropertyListCreateWithStream(kCFAllocatorDefault, s, 0, kCFPropertyListImmutable, NULL, NULL);
	}
    CFReleaseSafe(s);

    return d;
}
#endif

static void ApplyScopeByTypeForID(CFPropertyListRef scopes, SecDebugScopeID whichID) {
    if (isDictionary(scopes)) {
        ApplyScopeDictionaryForID(scopes, whichID);
    } else if (isString(scopes)) {
        ApplyScopeListForID(scopes, whichID);
    }
}

static void setup_config_settings() {
#if TARGET_OS_IPHONE
    CFURLRef prefURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, CFSTR("/Library/Managed Preferences/mobile/.GlobalPreferences.plist"), kCFURLPOSIXPathStyle, false);
    if(prefURL) {
        CFPropertyListRef plist = CopyPlistFromFile(prefURL);
        if (plist) {
            ApplyScopeByTypeForID(CFDictionaryGetValue(plist, CFSTR("SecLogging")), kScopeIDConfig);
        }
        CFReleaseSafe(plist);
    }
    CFReleaseSafe(prefURL);
#endif
}

static void setup_defaults_settings() {
    CFPropertyListRef scopes_value = CFPreferencesCopyValue(CFSTR("Logging"), CFSTR("com.apple.security"), kCFPreferencesAnyUser, kCFPreferencesCurrentHost);

    ApplyScopeByTypeForID(scopes_value, kScopeIDDefaults);

    CFReleaseSafe(scopes_value);
}

static void setup_environment_scopes() {
    const char *cur_scope = getenv("DEBUGSCOPE");
    if (cur_scope == NULL)
        cur_scope = "";

    ApplyScopeListForIDC(cur_scope, kScopeIDEnvironment);
}

void __security_debug_init(void) {
    static dispatch_once_t sdOnceToken;

    dispatch_once(&sdOnceToken, ^{
        setup_environment_scopes();
        setup_config_settings();
        setup_defaults_settings();
    });
}


// MARK: Log handler recording (e.g. grabbing security logging and sending it to test results).
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
        client = asl_open(NULL, "SecLogging", 0);
        asl_set_filter(client, ASL_FILTER_MASK_UPTO(ASL_LEVEL_DEBUG));
        pthread_setspecific(asl_client_key, client);
    }

    return client;
}

static CFMutableArrayRef get_log_handlers()
{
    static dispatch_once_t handlers_once;
    
    dispatch_once(&handlers_once, ^{
        sSecurityLogHandlers = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        
        CFArrayAppendValue(sSecurityLogHandlers, ^(int level, CFStringRef scope, const char *function,
                                                   const char *file, int line, CFStringRef message){
            CFStringRef logStr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@ %s %@\n"), scope ? scope : CFSTR(""), function, message);
            CFStringPerformWithCString(logStr, ^(const char *logMsg) {
                aslmsg msg = asl_new(ASL_TYPE_MSG);
                if (scope) {
                    CFStringPerformWithCString(scope, ^(const char *scopeStr) {
                        asl_set(msg, ASL_KEY_FACILITY, scopeStr);
                    });
                }
                asl_log(get_aslclient(), msg, level, "%s", logMsg);
                asl_free(msg);
            });
            CFReleaseSafe(logStr);
        });
    });
    
    return sSecurityLogHandlers;
}

static void log_api_trace_v(const char *api, const char *caller_info, CFStringRef format, va_list args)
{
    aslmsg msg = asl_new(ASL_TYPE_MSG);
    asl_set(msg, ASL_KEY_LEVEL, ASL_STRING_DEBUG);
    CFStringPerformWithCString(kAPIScope, ^(const char *scopeStr) {
        asl_set(msg, ASL_KEY_FACILITY, scopeStr);
    });
    asl_set(msg, "SecAPITrace", api);
    asl_set(msg, caller_info ? "ENTER" : "RETURN", "");

    if (format) {
        CFStringRef message = CFStringCreateWithFormatAndArguments(kCFAllocatorDefault, NULL, format, args);
        CFStringPerformWithCString(message, ^(const char *utf8Str) {
            asl_set(msg, ASL_KEY_MSG, utf8Str);
        });
        CFReleaseSafe(message);
    }

    if (caller_info) {
        asl_set(msg, "CALLER", caller_info);
    }

    asl_send(get_aslclient(), msg);
    asl_free(msg);
}

void __security_trace_enter_api(const char *api, CFStringRef format, ...)
{
    if (!IsScopeActive(ASL_LEVEL_DEBUG, kAPIScope))
        return;

	va_list args;
	va_start(args, format);

    {
        char stack_info[80];
        
        snprintf(stack_info, sizeof(stack_info), "C%p F%p", __builtin_return_address(1), __builtin_frame_address(2));

        log_api_trace_v(api, stack_info, format, args);
    }

    va_end(args);
}

void __security_trace_return_api(const char *api, CFStringRef format, ...)
{
    if (!IsScopeActive(ASL_LEVEL_DEBUG, kAPIScope))
        return;

    va_list args;
    va_start(args, format);

    log_api_trace_v(api, NULL, format, args);

    va_end(args);
}


void add_security_log_handler(security_log_handler handler)
{
    CFArrayAppendValue(get_log_handlers(), handler);
}

void remove_security_log_handler(security_log_handler handler)
{
    CFArrayRemoveAllValue(get_log_handlers(), handler);
}

static void __security_post_msg(int level, CFStringRef scope, const char *function,
                    const char *file, int line, CFStringRef message)
{
    CFArrayForEach(get_log_handlers(), ^(const void *value) {
        security_log_handler handler = (security_log_handler) value;
        
        handler(level, scope, function, file, line, message);
    });
}

static void __security_log_msg_v(int level, CFStringRef scope, const char *function,
                                 const char *file, int line, CFStringRef format, va_list args)
{
    __security_debug_init();

    if (!IsScopeActive(level, scope))
        return;

    CFStringRef message = CFStringCreateWithFormatAndArguments(kCFAllocatorDefault, NULL, format, args);
    __security_post_msg(level, scope, function, file, line, message);
    CFRelease(message);

}

void __security_debug(CFStringRef scope, const char *function,
                      const char *file, int line, CFStringRef format, ...)
{
	va_list args;
	va_start(args, format);

    __security_log_msg_v(ASL_LEVEL_DEBUG, scope, function, file, line, format, args);

    va_end(args);
}

void __security_log(int level, CFStringRef scope, const char *function,
    const char *file, int line, CFStringRef format, ...)
{
    va_list args;
    va_start(args, format);

    __security_log_msg_v(level, scope, function, file, line, format, args);

    va_end(args);
}
