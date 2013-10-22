//
//  iCloudKeychainTrace.c
//  utilities
//
//  Created  on 7/17/13.
//  Copyright (c) 2013 Apple Inc. All rights reserved.
//

#include "iCloudKeychainTrace.h"
#include <TargetConditionals.h>
#include <inttypes.h>
#include "SecCFWrappers.h"

const CFStringRef kCloudKeychainNumbrerOfSyncingConflicts = CFSTR("com.apple.cloudkeychain.conflictsCount");
const CFStringRef kCloudKeychainNumberOfTimesSyncFailed = CFSTR("com.apple.cloudkeychain.syncFailureCount");
const CFStringRef kCloudKeychainNumberOfConflictsResolved = CFSTR("com.apple.cloudkeychain.conflictsResolved");
const CFStringRef kCloudKeychainNumberOfTimesSyncedWithPeers = CFSTR("com.apple.cloudkeychain.syncedWithPeers");

static const CFStringRef gMessageTracerPrefix = CFSTR("com.apple.message.");

#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR))
#include <asl.h>

static const char* gMessageTracerDomainField = "com.apple.message.domain";
//static const char* gTopLevelKeyForiCloudKeychainTracing = "com.apple.cloudkeychain";

static bool OSX_SetCloudKeychainTraceValueForKey(CFStringRef key, int64_t value)
{
	bool result = false;
	
	if (NULL == key)
	{
		return result;
	}
	
	aslmsg mAsl = NULL;
	mAsl = asl_new(ASL_TYPE_MSG);
	if (NULL == mAsl)
	{
		return result;
	}
    
    CFIndex key_length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(key), kCFStringEncodingUTF8);
    key_length += 1; // For null
    char base_key_buffer[key_length];
    memset(base_key_buffer, 0,key_length);
    if (!CFStringGetCString(key, base_key_buffer, key_length, kCFStringEncodingUTF8))
    {
        asl_free(mAsl);
         return result;
    }
    
    
    CFStringRef key_str = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@%@"), gMessageTracerPrefix, key);
    if (NULL == key_str)
    {
        asl_free(mAsl);
        return result;
    }
	
	CFStringRef value_str = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%lld"), value);
    if (NULL == value_str)
    {
        asl_free(mAsl);
        CFRelease(key_str);
        return result;
    }
    
    CFIndex key_str_numBytes = CFStringGetMaximumSizeForEncoding(CFStringGetLength(key_str), kCFStringEncodingUTF8);
    key_str_numBytes += 1; // For null
    char key_buffer[key_str_numBytes];
    memset(key_buffer, 0, key_str_numBytes);
    if (!CFStringGetCString(key_str, key_buffer, key_str_numBytes, kCFStringEncodingUTF8))
    {
        asl_free(mAsl);
        CFRelease(key_str);
        CFRelease(value_str);
        return result;
    }
    CFRelease(key_str);
    
    CFIndex value_str_numBytes = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value_str), kCFStringEncodingUTF8);
    value_str_numBytes += 1; // For null
    char value_buffer[value_str_numBytes];
    memset(value_buffer, 0, value_str_numBytes);
    if (!CFStringGetCString(value_str, value_buffer, value_str_numBytes, kCFStringEncodingUTF8))
    {
        asl_free(mAsl);
        CFRelease(value_str);
        return result;
    }
    CFRelease(value_str);
	
	asl_set(mAsl, gMessageTracerDomainField, base_key_buffer);
	
	asl_set(mAsl, key_buffer, value_buffer);
	asl_log(NULL, mAsl, ASL_LEVEL_NOTICE, "%s is %lld", key_buffer, value);
	asl_free(mAsl);
	return true;
}
#endif

#if (TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR)

typedef void (*type_ADClientClearScalarKey)(CFStringRef key);
typedef void (*type_ADClientAddValueForScalarKey)(CFStringRef key, int64_t value);

static type_ADClientClearScalarKey gADClientClearScalarKey = NULL;
static type_ADClientAddValueForScalarKey gADClientAddValueForScalarKey = NULL;

static dispatch_once_t gADFunctionPointersSet = 0;
static CFBundleRef gAggdBundleRef = NULL;
static bool gFunctionPointersAreLoaded = false;

static bool InitializeADFunctionPointers()
{    
	if (gFunctionPointersAreLoaded)
	{
		return gFunctionPointersAreLoaded;
	}
    
    dispatch_once(&gADFunctionPointersSet,
      ^{
          CFStringRef path_to_aggd_framework = CFSTR("/System/Library/PrivateFrameworks/AggregateDictionary.framework");
          
          CFURLRef aggd_url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, path_to_aggd_framework, kCFURLPOSIXPathStyle, true);
          
          if (NULL != aggd_url)
          {
              gAggdBundleRef = CFBundleCreate(kCFAllocatorDefault, aggd_url);
              if (NULL != gAggdBundleRef)
              {  
                  gADClientClearScalarKey = (type_ADClientClearScalarKey)
                    CFBundleGetFunctionPointerForName(gAggdBundleRef, CFSTR("ADClientClearScalarKey"));
                  
                  gADClientAddValueForScalarKey = (type_ADClientAddValueForScalarKey)
                    CFBundleGetFunctionPointerForName(gAggdBundleRef, CFSTR("ADClientAddValueForScalarKey"));
              }
              CFRelease(aggd_url);
          }
      });
    
    gFunctionPointersAreLoaded = ((NULL != gADClientClearScalarKey) && (NULL != gADClientAddValueForScalarKey));
    return gFunctionPointersAreLoaded;
}

static void Internal_ADClientClearScalarKey(CFStringRef key)
{
    if (InitializeADFunctionPointers())
    {
        gADClientClearScalarKey(key);
    }
}

static void Internal_ADClientAddValueForScalarKey(CFStringRef key, int64_t value)
{
    if (InitializeADFunctionPointers())
    {
        gADClientAddValueForScalarKey(key, value);
    }
}

static bool iOS_SetCloudKeychainTraceValueForKey(CFStringRef key, int64_t value)
{
	if (NULL == key)
	{
		return false;
	}
    
    if (0LL == value)
    {
        Internal_ADClientClearScalarKey(key);
    }
    else
    {
        Internal_ADClientAddValueForScalarKey(key, value);
    }
	return true;
}
#endif

bool SetCloudKeychainTraceValueForKey(CFStringRef key, int64_t value)
{
#if (TARGET_IPHONE_SIMULATOR)
	return false;
#endif 

#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
	return OSX_SetCloudKeychainTraceValueForKey(key, value);
#endif

#if (TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR)
	return iOS_SetCloudKeychainTraceValueForKey(key, value);
#endif
}
