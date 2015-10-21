/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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


#include "iCloudKeychainTrace.h"
#include <TargetConditionals.h>
#include <inttypes.h>
#include "SecCFWrappers.h"
#include <sys/time.h>
#include <CoreFoundation/CoreFoundation.h>

const CFStringRef kNumberOfiCloudKeychainPeers = CFSTR("numberOfPeers");
const CFStringRef kNumberOfiCloudKeychainItemsBeingSynced = CFSTR("numberOfItemsBeingSynced");
const CFStringRef kCloudKeychainNumberOfSyncingConflicts = CFSTR("conflictsCount");
const CFStringRef kCloudKeychainNumberOfTimesSyncFailed = CFSTR("syncFailureCount");
const CFStringRef kCloudKeychainNumberOfConflictsResolved = CFSTR("conflictsResolved");
const CFStringRef kCloudKeychainNumberOfTimesSyncedWithPeers = CFSTR("syncedWithPeers");

#if !TARGET_IPHONE_SIMULATOR
#if TARGET_OS_IPHONE
static const char* gTopLevelKeyForiCloudKeychainTracing = "com.apple.icdp";
#else
static const char* gTopLevelKeyForiCloudKeychainTracing = "com.apple.icdp.KeychainStats";
#endif
#endif

#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR))
#include <asl.h>

static const char* gMessageTracerSetPrefix = "com.apple.message.";

static const char* gMessageTracerDomainField = "com.apple.message.domain";

/* --------------------------------------------------------------------------
	Function:		OSX_BeginCloudKeychainLoggingTransaction
	
	Description:	For OSX the message tracer back end wants its logging 
					done in "bunches".  This function allows for beginning
					a 'transaction' of logging which will allow for putting
					all of the transactions items into a single log making
					the message tracer folks happy.
					
					The work of this function is to create the aslmsg context
					and set the domain field and then return the aslmsg
					context as a void*
   -------------------------------------------------------------------------- */					
static void* OSX_BeginCloudKeychainLoggingTransaction()
{
	void* result = NULL;	
	aslmsg mAsl = NULL;
	mAsl = asl_new(ASL_TYPE_MSG);
	if (NULL == mAsl)
	{
		return result;
	}
	
	asl_set(mAsl, gMessageTracerDomainField, gTopLevelKeyForiCloudKeychainTracing);
	
	result = (void *)mAsl;
	return result;
}

/* --------------------------------------------------------------------------
	Function:		OSX_AddKeyValuePairToKeychainLoggingTransaction
	
	Description:	Once a call to OSX_BeginCloudKeychainLoggingTransaction 
					is done, this call all allow for adding items to the
					"bunch" of items being logged.
					
					NOTE: The key should be a simple key such as 
					"numberOfPeers".  This is because this function will 
					apptend the required prefix of "com.apple.message."
   -------------------------------------------------------------------------- */
static bool OSX_AddKeyValuePairToKeychainLoggingTransaction(void* token, CFStringRef key, int64_t value)
{
	if (NULL == token || NULL == key)
	{
		return false;
	}
		
	aslmsg mAsl = (aslmsg)token;
	
	// Fix up the key
	CFStringRef real_key = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%s%@"), gMessageTracerSetPrefix, key);
	if (NULL == real_key)
	{
		return false;
	}
	
	CFIndex key_length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(real_key), kCFStringEncodingUTF8);
    key_length += 1; // For null
    char key_buffer[key_length];
    memset(key_buffer, 0,key_length);
    if (!CFStringGetCString(real_key, key_buffer, key_length, kCFStringEncodingUTF8))
    {
        CFRelease(real_key);
        return false;
    }
	CFRelease(real_key);
	
	CFStringRef value_str = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%lld"), value);
    if (NULL == value_str)
    {
        return false;
    }

    CFIndex value_str_numBytes = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value_str), kCFStringEncodingUTF8);
    value_str_numBytes += 1; // For null
    char value_buffer[value_str_numBytes];
    memset(value_buffer, 0, value_str_numBytes);
    if (!CFStringGetCString(value_str, value_buffer, value_str_numBytes, kCFStringEncodingUTF8))
    {
        CFRelease(value_str);
        return false;
    }
    CFRelease(value_str);

	asl_set(mAsl, key_buffer, value_buffer);
	return true;	
}

/* --------------------------------------------------------------------------
	Function:		OSX_CloseCloudKeychainLoggingTransaction
	
	Description:	Once a call to OSX_BeginCloudKeychainLoggingTransaction 
					is done, and all of the items that are to be in the 
					"bunch" of items being logged, this function will do the
					real logging and free the aslmsg context.
   -------------------------------------------------------------------------- */
static void OSX_CloseCloudKeychainLoggingTransaction(void* token)
{
	if (NULL != token)
	{
		aslmsg mAsl = (aslmsg)token;
		asl_log(NULL, mAsl, ASL_LEVEL_NOTICE, "");
		asl_free(mAsl);
	}
}

/* --------------------------------------------------------------------------
	Function:		OSX_SetCloudKeychainTraceValueForKey
	
	Description:	If "bunching" of items either cannot be done or is not
					desired, then this 'single shot' function shold be used.
					It will create the aslmsg context, register the domain
					fix up the key and log the key value pair and then 
					do the real logging and free the aslmsg context.
   -------------------------------------------------------------------------- */
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
	
	// Fix up the key
	CFStringRef real_key = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%s%@"), gMessageTracerSetPrefix, key);
	if (NULL == real_key)
	{
		return false;
	}
	
	CFIndex key_length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(real_key), kCFStringEncodingUTF8);
    key_length += 1; // For null
    char key_buffer[key_length];
    memset(key_buffer, 0,key_length);
    if (!CFStringGetCString(real_key, key_buffer, key_length, kCFStringEncodingUTF8))
    {
        CFRelease(real_key);
        return false;
    }
	CFRelease(real_key);
	
	
	CFStringRef value_str = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%lld"), value);
    if (NULL == value_str)
    {
        asl_free(mAsl);
        return result;
    }
   
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
	
    asl_set(mAsl, gMessageTracerDomainField, gTopLevelKeyForiCloudKeychainTracing);
	
	asl_set(mAsl, key_buffer, value_buffer);
	asl_log(NULL, mAsl, ASL_LEVEL_NOTICE, "%s is %lld", key_buffer, value);
	asl_free(mAsl);
	return true;
	
}
#endif

#if (TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR)

typedef void (*type_ADClientClearScalarKey)(CFStringRef key);
typedef void (*type_ADClientSetValueForScalarKey)(CFStringRef key, int64_t value);

static type_ADClientClearScalarKey gADClientClearScalarKey = NULL;
static type_ADClientSetValueForScalarKey gADClientSetValueForScalarKey = NULL;

static dispatch_once_t gADFunctionPointersSet = 0;
static CFBundleRef gAggdBundleRef = NULL;
static bool gFunctionPointersAreLoaded = false;

/* --------------------------------------------------------------------------
	Function:		InitializeADFunctionPointers
	
	Description:	Linking to the Aggregate library causes a build cycle so
					This function will dynamically load the needed function
					pointers.
   -------------------------------------------------------------------------- */
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
                  
                  gADClientSetValueForScalarKey = (type_ADClientSetValueForScalarKey)
                    CFBundleGetFunctionPointerForName(gAggdBundleRef, CFSTR("ADClientSetValueForScalarKey"));
              }
              CFRelease(aggd_url);
          }
      });
    
    gFunctionPointersAreLoaded = ((NULL != gADClientClearScalarKey) && (NULL != gADClientSetValueForScalarKey));
    return gFunctionPointersAreLoaded;
}

/* --------------------------------------------------------------------------
	Function:		Internal_ADClientClearScalarKey
	
	Description:	This fucntion is a wrapper around calling the  
					ADClientClearScalarKey function.  
					
					NOTE: The key should be a simple key such as 
					"numberOfPeers".  This is because this function will 
					apptend the required prefix of "com.apple.cloudkeychain"
   -------------------------------------------------------------------------- */
static void Internal_ADClientClearScalarKey(CFStringRef key)
{
    if (InitializeADFunctionPointers())
    {
		CFStringRef real_key = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%s.%@"), gTopLevelKeyForiCloudKeychainTracing, key);
		if (NULL == real_key)
		{
			return;
		}
		
        gADClientClearScalarKey(real_key);
		CFRelease(real_key);
    }
}

/* --------------------------------------------------------------------------
	Function:		Internal_ADClientSetValueForScalarKey
	
	Description:	This fucntion is a wrapper around calling the  
					ADClientSetValueForScalarKey function.
					
					NOTE: The key should be a simple key such as 
					"numberOfPeers".  This is because this function will 
					apptend the required prefix of "com.apple.cloudkeychain"
   -------------------------------------------------------------------------- */
static void Internal_ADClientSetValueForScalarKey(CFStringRef key, int64_t value)
{
    if (InitializeADFunctionPointers())
    {
		CFStringRef real_key = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%s.%@"), gTopLevelKeyForiCloudKeychainTracing, key);
		if (NULL == real_key)
		{
			return;
		}
		
        gADClientSetValueForScalarKey(real_key, value);
		CFRelease(real_key);
    }
}


/* --------------------------------------------------------------------------
	Function:		iOS_SetCloudKeychainTraceValueForKey
	
	Description:	This fucntion is a wrapper around calling either   
					ADClientSetValueForScalarKey  or ADClientClearScalarKey
					depending on if the value is 0.
					
					NOTE: The key should be a simple key such as 
					"numberOfPeers".  This is because this function will 
					apptend the required prefix of "com.apple.cloudkeychain"
   -------------------------------------------------------------------------- */
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
        Internal_ADClientSetValueForScalarKey(key, value);
    }
	return true;
}

/* --------------------------------------------------------------------------
	Function:		iOS_AddKeyValuePairToKeychainLoggingTransaction
	
	Description:	For iOS the is no "bunching"  This function will simply
					call iOS_SetCloudKeychainTraceValueForKey to log the 
					key value pair
   -------------------------------------------------------------------------- */
static bool iOS_AddKeyValuePairToKeychainLoggingTransaction(void* token, CFStringRef key, int64_t value)
{
#pragma unused(token)
	return iOS_SetCloudKeychainTraceValueForKey(key, value);
}
#endif

/* --------------------------------------------------------------------------
	Function:		SetCloudKeychainTraceValueForKey
	
	Description:	SPI to log a single key value pair with the logging system
   -------------------------------------------------------------------------- */
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

/* --------------------------------------------------------------------------
	Function:		BeginCloudKeychainLoggingTransaction
	
	Description:	SPI to begin a logging transaction
   -------------------------------------------------------------------------- */
void* BeginCloudKeychainLoggingTransaction()
{
#if (TARGET_IPHONE_SIMULATOR)
	return (void *)-1;
#endif

#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
	return OSX_BeginCloudKeychainLoggingTransaction();
#endif

#if (TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR)
	return NULL;
#endif
}

/* --------------------------------------------------------------------------
	Function:		AddKeyValuePairToKeychainLoggingTransaction
	
	Description:	SPI to add a key value pair to an outstanding logging
					tansaction
   -------------------------------------------------------------------------- */
bool AddKeyValuePairToKeychainLoggingTransaction(void* token, CFStringRef key, int64_t value)
{
#if (TARGET_IPHONE_SIMULATOR)
	return false;
#endif

#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
	return OSX_AddKeyValuePairToKeychainLoggingTransaction(token, key, value);
#endif

#if (TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR)
	return iOS_AddKeyValuePairToKeychainLoggingTransaction(token, key, value);
#endif
}

/* --------------------------------------------------------------------------
	Function:		CloseCloudKeychainLoggingTransaction
	
	Description:	SPI to complete a logging transaction and clean up the
					context
   -------------------------------------------------------------------------- */
void CloseCloudKeychainLoggingTransaction(void* token)
{
#if (TARGET_IPHONE_SIMULATOR)
	; // nothing
#endif
	
#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
	OSX_CloseCloudKeychainLoggingTransaction(token);
#endif

#if (TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR)
	; // nothing
#endif
}

