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
#include "utilities/SecFileLocations.h"
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
#include <os/trace.h>
#include <os/log_private.h>
#include <sqlite3.h>

const char *api_trace = "api_trace";


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
        return SECLOG_LEVEL_EMERG;
    else if (CFEqual(string, CFSTR(ASL_STRING_ALERT)))
        return SECLOG_LEVEL_ALERT;
    else if (CFEqual(string, CFSTR(ASL_STRING_CRIT)))
        return SECLOG_LEVEL_CRIT;
    else if (CFEqual(string, CFSTR(ASL_STRING_ERR)))
        return SECLOG_LEVEL_ERR;
    else if (CFEqual(string, CFSTR(ASL_STRING_WARNING)))
        return SECLOG_LEVEL_WARNING;
    else if (CFEqual(string, CFSTR(ASL_STRING_NOTICE)))
        return SECLOG_LEVEL_NOTICE;
    else if (CFEqual(string, CFSTR(ASL_STRING_INFO)))
        return SECLOG_LEVEL_INFO;
    else if (CFEqual(string, CFSTR(ASL_STRING_DEBUG)))
        return SECLOG_LEVEL_DEBUG;
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


/*
 * Instead of using CFPropertyListReadFromFile we use a
 * CFPropertyListCreateWithStream directly
 * here. CFPropertyListReadFromFile() uses
 * CFURLCopyResourcePropertyForKey() andCF pulls in CoreServices for
 * CFURLCopyResourcePropertyForKey() and that doesn't work in install
 * environment.
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

static void ApplyScopeByTypeForID(CFPropertyListRef scopes, SecDebugScopeID whichID) {
    if (isDictionary(scopes)) {
        ApplyScopeDictionaryForID(scopes, whichID);
    } else if (isString(scopes)) {
        ApplyScopeListForID(scopes, whichID);
    }
}

static void setup_config_settings() {
    CFStringRef logFileName;
#if TARGET_OS_IPHONE
    logFileName = CFSTR(".GlobalPreferences.plist");
#else
    logFileName = CFSTR("com.apple.security.logging.plist");
#endif
    CFURLRef prefURL = SecCopyURLForFileInManagedPreferencesDirectory(logFileName);
    if(prefURL) {
        CFPropertyListRef plist = CopyPlistFromFile(prefURL);
        if (plist) {
            ApplyScopeByTypeForID(CFDictionaryGetValue(plist, CFSTR("SecLogging")), kScopeIDConfig);
        }
        CFReleaseSafe(plist);
    }
    CFReleaseSafe(prefURL);
}

static void setup_defaults_settings() {
    CFPropertyListRef scopes_value = CFPreferencesCopyValue(CFSTR("Logging"), CFSTR("com.apple.security"), kCFPreferencesAnyUser, kCFPreferencesCurrentHost);

    ApplyScopeByTypeForID(scopes_value, kScopeIDDefaults);

    CFReleaseSafe(scopes_value);
}

static void setup_circle_defaults_settings() {
    CFPropertyListRef scopes_value = CFPreferencesCopyValue(CFSTR("Circle-Logging"), CFSTR("com.apple.security"), kCFPreferencesAnyUser, kCFPreferencesCurrentHost);

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
        setup_circle_defaults_settings();
    });
}




static char *copyScopeStr(CFStringRef scope, char *alternative) {
    char *scopeStr = NULL;
    if(scope) {
        scopeStr = CFStringToCString(scope);
    } else {
        scopeStr = strdup("noScope");
    }
    return scopeStr;
}

static os_log_t logObjForCFScope(CFStringRef scope) {
    static dispatch_once_t onceToken = 0;
    __block os_log_t retval = OS_LOG_DISABLED;
    static dispatch_queue_t logObjectQueue = NULL;
    static CFMutableDictionaryRef scopeMap = NULL;
    
    if(scope == NULL) scope = CFSTR("logging");
    
    dispatch_once(&onceToken, ^{
        logObjectQueue = dispatch_queue_create("logObjectQueue", DISPATCH_QUEUE_SERIAL);
        scopeMap = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFCopyStringDictionaryKeyCallBacks, NULL);
    });

    dispatch_sync(logObjectQueue, ^{
        retval = (os_log_t) CFDictionaryGetValue(scopeMap, scope);
        if (retval) return;
        
        CFStringPerformWithCString(scope, ^(const char *scopeStr) {
            CFDictionaryAddValue(scopeMap, scope, os_log_create("com.apple.securityd", scopeStr));
        });
        retval = (os_log_t) CFDictionaryGetValue(scopeMap, scope);
    });
    
    return retval;
}

static bool loggingEnabled = true;
static pthread_mutex_t loggingMutex = PTHREAD_MUTEX_INITIALIZER;

bool secLogEnabled(void) {
    bool r = false;
    pthread_mutex_lock(&loggingMutex);
    r = loggingEnabled;
    pthread_mutex_unlock(&loggingMutex);
    return r;
}
void secLogDisable(void) {
    pthread_mutex_lock(&loggingMutex);
    loggingEnabled = false;
    pthread_mutex_unlock(&loggingMutex);
}

void secLogEnable(void) {
    pthread_mutex_lock(&loggingMutex);
    loggingEnabled = true;
    pthread_mutex_unlock(&loggingMutex);
}

os_log_t logObjForScope(const char *scope)
{
    return secLogObjForScope(scope);
}

os_log_t secLogObjForScope(const char *scope) {
    if (!secLogEnabled())
        return OS_LOG_DISABLED;
    CFStringRef cfscope = NULL;
    if(scope) cfscope =  CFStringCreateWithCString(kCFAllocatorDefault, scope, kCFStringEncodingASCII);
    os_log_t retval = logObjForCFScope(cfscope);
    CFReleaseNull(cfscope);
    return retval;
}



CFStringRef SecLogAPICreate(bool apiIn, const char *api, CFStringRef format, ... ) {
    CFMutableStringRef outStr = CFStringCreateMutable(kCFAllocatorDefault, 0);

    char *direction = apiIn ? "ENTER" : "RETURN";
    va_list args;
    va_start(args, format);

    CFStringAppend(outStr, CFSTR("SecAPITrace "));
    CFStringAppendCString(outStr, api, kCFStringEncodingASCII);
    CFStringAppendCString(outStr, direction, kCFStringEncodingASCII);
    
    if (format) {
        CFStringRef message = CFStringCreateWithFormatAndArguments(kCFAllocatorDefault, NULL, format, args);
        CFStringAppend(outStr, message);
        CFReleaseSafe(message);
    }
    
    if (apiIn) {
        char caller_info[80];
        snprintf(caller_info, sizeof(caller_info), "C%p F%p", __builtin_return_address(1), __builtin_frame_address(2));
        CFStringAppend(outStr, CFSTR("CALLER "));
        CFStringAppendCString(outStr, caller_info, kCFStringEncodingASCII);
    }
    va_end(args);

    return outStr;
}

#if TARGET_OS_OSX
#ifdef NO_OS_LOG
// Functions for weak-linking os_log functions
#include <dlfcn.h>

#define weak_log_f(fname, newname, rettype, fallthrough) \
  rettype newname(log_args) { \
    static dispatch_once_t onceToken = 0; \
    static rettype (*newname)(log_args) = NULL; \
    \
    dispatch_once(&onceToken, ^{ \
        void* libtrace = dlopen("/usr/lib/system/libsystem_trace.dylib", RTLD_LAZY | RTLD_LOCAL); \
        if (libtrace) { \
            newname = (rettype(*)(log_args)) dlsym(libtrace, #fname); \
        } \
    }); \
    \
    if(newname) { \
        return newname(log_argnames); \
    } \
    fallthrough;\
}

#define log_args void *dso, os_log_t log, os_log_type_t type, const char *format, uint8_t *buf, unsigned int size
#define log_argnames dso, log, type, format, buf, size
weak_log_f(_os_log_impl, weak_os_log_impl, void, return);
#undef log_args
#undef log_argnames

#define log_args const char *subsystem, const char *category
#define log_argnames subsystem, category
weak_log_f(os_log_create, weak_os_log_create, os_log_t, return NULL);
#undef log_args
#undef log_argnames

#define log_args os_log_t oslog, os_log_type_t type
#define log_argnames oslog, type
weak_log_f(os_log_type_enabled, weak_os_log_type_enabled, bool, return false);
#undef log_args
#undef log_argnames

#undef weak_log_f

#endif // NO_OS_LOG
#endif // TARGET_OS_OSX

