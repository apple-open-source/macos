//
//  SecCertificateTrace.c
//  utilities
//
//  Created on 10/18/13.
//  Copyright (c) 2013 Apple Inc. All rights reserved.
//

#include "SecCertificateTrace.h"
#include <TargetConditionals.h>
#include <inttypes.h>
#include "SecCFWrappers.h"
#include <sys/time.h>
#include <CoreFoundation/CoreFoundation.h>
#include "debugging.h"

#define SEC_CONST_DECL(k,v) CFTypeRef k = (CFTypeRef)(CFSTR(v));

SEC_CONST_DECL (kCertStatsPrefix, "com.apple.certstats");
SEC_CONST_DECL (kCertStatsCert, "cert");
SEC_CONST_DECL (kCertStatsPolicy, "id");
SEC_CONST_DECL (kCertStatsNotBefore, "nb");
SEC_CONST_DECL (kCertStatsNotAfter, "na");
SEC_CONST_DECL (kCertStatsSerialNumber, "sn");
SEC_CONST_DECL (kCertStatsSubjectSummary, "s");
SEC_CONST_DECL (kCertStatsIssuerSummary, "i");

static const CFStringRef kCertStatsFormat = CFSTR("%@/%@=%@/%@=%@/%@=%@/%@=%@/%@=%@/%@=%@");
static const CFStringRef kCertStatsDelimiters = CFSTR("/");

bool SetCertificateTraceValueForKey(CFStringRef key, int64_t value);
void* BeginCertificateLoggingTransaction(void);
bool AddKeyValuePairToCertificateLoggingTransaction(void* token, CFStringRef key, int64_t value);
void CloseCertificateLoggingTransaction(void* token);
CFStringRef SecCFStringCopyEncodingDelimiters(CFStringRef str, CFStringRef delimiters);

#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR))
#include <asl.h>

static const char* gTopLevelKeyForCertificateTracing = "com.apple.certstats";

static const char* gMessageTracerSetPrefix = "com.apple.message.";

static const char* gMessageTracerDomainField = "com.apple.message.domain";

/* --------------------------------------------------------------------------
	Function:		OSX_BeginCertificateLoggingTransaction

	Description:	For OSX, the message tracer back end wants its logging
					done in "bunches".  This function allows for beginning
					a 'transaction' of logging which will allow for putting
					all of the transaction's items into a single log making
					the message tracer folks happy.

					The work of this function is to create the aslmsg context
					and set the domain field and then return the aslmsg
					context as a void* result.
   -------------------------------------------------------------------------- */
static void* OSX_BeginCertificateLoggingTransaction()
{
	void* result = NULL;
	aslmsg mAsl = NULL;
	mAsl = asl_new(ASL_TYPE_MSG);
	if (NULL == mAsl)
	{
		return result;
	}

	asl_set(mAsl, gMessageTracerDomainField, gTopLevelKeyForCertificateTracing);

	result = (void *)mAsl;
	return result;
}

/* --------------------------------------------------------------------------
	Function:		OSX_AddKeyValuePairToCertificateLoggingTransaction

	Description:	Once a call to OSX_BeginCertificateLoggingTransaction
					is done, this call will allow for adding items to the
					"bunch" of items being logged.

					NOTE: The key should be a simple key such as
					"numberOfPeers".  This is because this function will
					append the required prefix of "com.apple.message."
   -------------------------------------------------------------------------- */
static bool OSX_AddKeyValuePairToCertificateLoggingTransaction(void* token, CFStringRef key, int64_t value)
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
	Function:		OSX_CloseCertificateLoggingTransaction

	Description:	Once a call to OSX_BeginCertificateLoggingTransaction
					is done, and all of the items that are to be in the
					"bunch" of items being logged, this function will do the
					real logging and free the aslmsg context.
   -------------------------------------------------------------------------- */
static void OSX_CloseCertificateLoggingTransaction(void* token)
{
	if (NULL != token)
	{
		aslmsg mAsl = (aslmsg)token;
		asl_log(NULL, mAsl, ASL_LEVEL_NOTICE, "");
		asl_free(mAsl);
	}
}

/* --------------------------------------------------------------------------
	Function:		OSX_SetCertificateTraceValueForKey

	Description:	If "bunching" of items either cannot be done or is not
					desired, then this 'single shot' function should be used.
					It will create the aslmsg context, register the domain,
					fix up the key and log the key value pair, and then
					do the real logging and free the aslmsg context.
   -------------------------------------------------------------------------- */
static bool OSX_SetCertificateTraceValueForKey(CFStringRef key, int64_t value)
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

	/* Domain */
	asl_set(mAsl, gMessageTracerDomainField, gTopLevelKeyForCertificateTracing);

	/* Our custom key (starts with com.apple.message.) and its value */
	asl_set(mAsl, key_buffer, value_buffer);

	/* Log this ASL record (note actual log message is empty) */
	asl_log(NULL, mAsl, ASL_LEVEL_NOTICE, "");
	asl_free(mAsl);
	return true;

}
#endif

#if (TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR)

static const char* gTopLevelKeyForCertificateTracing = "com.apple.certstats";

typedef void (*type_ADClientClearScalarKey)(CFStringRef key);
typedef void (*type_ADClientAddValueForScalarKey)(CFStringRef key, int64_t value);

static type_ADClientClearScalarKey gADClientClearScalarKey = NULL;
static type_ADClientAddValueForScalarKey gADClientAddValueForScalarKey = NULL;

static dispatch_once_t gADFunctionPointersSet = 0;
static CFBundleRef gAggdBundleRef = NULL;
static bool gFunctionPointersAreLoaded = false;

/* --------------------------------------------------------------------------
	Function:		InitializeADFunctionPointers

	Description:	Linking to the Aggregate library causes a build cycle,
					so this function dynamically loads the needed function
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

                  gADClientAddValueForScalarKey = (type_ADClientAddValueForScalarKey)
                    CFBundleGetFunctionPointerForName(gAggdBundleRef, CFSTR("ADClientAddValueForScalarKey"));
              }
              CFRelease(aggd_url);
          }
      });

    gFunctionPointersAreLoaded = ((NULL != gADClientClearScalarKey) && (NULL != gADClientAddValueForScalarKey));
    return gFunctionPointersAreLoaded;
}

/* --------------------------------------------------------------------------
	Function:		Internal_ADClientClearScalarKey

	Description:	This function is a wrapper around calling the
					ADClientClearScalarKey function.

					NOTE: The key should be a simple key such as
					"numberOfPeers".  This is because this function will
					append the required prefix of "com.apple.certstats"
   -------------------------------------------------------------------------- */
static void Internal_ADClientClearScalarKey(CFStringRef key)
{
    if (InitializeADFunctionPointers())
    {
		CFStringRef real_key = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%s.%@"), gTopLevelKeyForCertificateTracing, key);
		if (NULL == real_key)
		{
			return;
		}

        gADClientClearScalarKey(real_key);
		CFRelease(real_key);
    }
}

/* --------------------------------------------------------------------------
	Function:		Internal_ADClientAddValueForScalarKey

	Description:	This function is a wrapper around calling the
					ADClientAddValueForScalarKey function.

					NOTE: The key should be a simple key such as
					"numberOfPeers".  This is because this function will
					append the required prefix of "com.apple.certstats"
   -------------------------------------------------------------------------- */
static void Internal_ADClientAddValueForScalarKey(CFStringRef key, int64_t value)
{
    if (InitializeADFunctionPointers())
    {
		CFStringRef real_key = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%s.%@"), gTopLevelKeyForCertificateTracing, key);
		if (NULL == real_key)
		{
			return;
		}

        gADClientAddValueForScalarKey(real_key, value);
		CFRelease(real_key);
    }
}


/* --------------------------------------------------------------------------
	Function:		iOS_SetCertificateTraceValueForKey

	Description:	This function is a wrapper around calling either
					ADClientAddValueForScalarKey or ADClientClearScalarKey,
					depending on whether the value is 0.

					NOTE: The key should be a simple key such as
					"numberOfPeers".  This is because this function will
					append the required prefix of "com.apple.certstats"
   -------------------------------------------------------------------------- */
static bool iOS_SetCertificateTraceValueForKey(CFStringRef key, int64_t value)
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

/* --------------------------------------------------------------------------
	Function:		iOS_AddKeyValuePairToCertificateLoggingTransaction

	Description:	For iOS there is no "bunching". This function will simply
					call iOS_SetCloudKeychainTraceValueForKey to log the
					key-value pair.
   -------------------------------------------------------------------------- */
static bool iOS_AddKeyValuePairToCertificateLoggingTransaction(void* token, CFStringRef key, int64_t value)
{
#pragma unused(token)
	return iOS_SetCertificateTraceValueForKey(key, value);
}
#endif

/* --------------------------------------------------------------------------
	Function:		SetCertificateTraceValueForKey

	Description:	SPI to log a single key-value pair with the logging system
   -------------------------------------------------------------------------- */
bool SetCertificateTraceValueForKey(CFStringRef key, int64_t value)
{
#if (TARGET_IPHONE_SIMULATOR)
	return false;
#endif

#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
	return OSX_SetCertificateTraceValueForKey(key, value);
#endif

#if (TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR)
	return iOS_SetCertificateTraceValueForKey(key, value);
#endif
}

/* --------------------------------------------------------------------------
	Function:		BeginCertificateLoggingTransaction

	Description:	SPI to begin a logging transaction
   -------------------------------------------------------------------------- */
void* BeginCertificateLoggingTransaction(void)
{
#if (TARGET_IPHONE_SIMULATOR)
	return (void *)-1;
#endif

#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
	return OSX_BeginCertificateLoggingTransaction();
#endif

#if (TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR)
	return NULL;
#endif
}

/* --------------------------------------------------------------------------
	Function:		AddKeyValuePairToCertificateLoggingTransaction

	Description:	SPI to add a key-value pair to an outstanding logging
					transaction
   -------------------------------------------------------------------------- */
bool AddKeyValuePairToCertificateLoggingTransaction(void* token, CFStringRef key, int64_t value)
{
#if (TARGET_IPHONE_SIMULATOR)
	return false;
#endif

#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
	return OSX_AddKeyValuePairToCertificateLoggingTransaction(token, key, value);
#endif

#if (TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR)
	return iOS_AddKeyValuePairToCertificateLoggingTransaction(token, key, value);
#endif
}

/* --------------------------------------------------------------------------
	Function:		CloseCertificateLoggingTransaction

	Description:	SPI to complete a logging transaction and clean up the
					context
   -------------------------------------------------------------------------- */
void CloseCertificateLoggingTransaction(void* token)
{
#if (TARGET_IPHONE_SIMULATOR)
	; // nothing
#endif

#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
	OSX_CloseCertificateLoggingTransaction(token);
#endif

#if (TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR)
	; // nothing
#endif
}

/* --------------------------------------------------------------------------
 Function:		SecCFStringCopyEncodingDelimiters

 Description:	Utility to replace all characters in the given string
                which match characters in the delimiters string. This
                should be expanded later; for now we are assuming the
                delimiters string is a single character, and we replace
                it with an underscore instead of percent encoding.
 -------------------------------------------------------------------------- */

CFStringRef SecCFStringCopyEncodingDelimiters(CFStringRef str, CFStringRef delimiters)
{
    CFMutableStringRef newStr = (str) ? CFStringCreateMutableCopy(kCFAllocatorDefault, 0, str) : NULL;
    if (str && delimiters)
    {
        CFStringRef stringToFind = delimiters;
        CFStringRef replacementString = CFSTR("_");
        CFStringFindAndReplace(newStr, stringToFind, replacementString, CFRangeMake(0, CFStringGetLength(newStr)), 0);
    }
    return (CFStringRef) newStr;
}

/* --------------------------------------------------------------------------
	Function:		SecCertificateTraceAddRecord

	Description:	High-level SPI that logs a single certificate record,
					given a dictionary containing key-value pairs.
   -------------------------------------------------------------------------- */
bool SecCertificateTraceAddRecord(CFDictionaryRef certRecord)
{
	bool result = false;
	if (!certRecord)
		return result;

	/* encode delimiter characters in supplied strings */
    CFStringRef policy = SecCFStringCopyEncodingDelimiters(CFDictionaryGetValue(certRecord, kCertStatsPolicy), kCertStatsDelimiters);
    CFStringRef notBefore = SecCFStringCopyEncodingDelimiters(CFDictionaryGetValue(certRecord, kCertStatsNotBefore), kCertStatsDelimiters);
    CFStringRef notAfter = SecCFStringCopyEncodingDelimiters(CFDictionaryGetValue(certRecord, kCertStatsNotAfter), kCertStatsDelimiters);
    CFStringRef serial = SecCFStringCopyEncodingDelimiters(CFDictionaryGetValue(certRecord, kCertStatsSerialNumber), kCertStatsDelimiters);
    CFStringRef subject = SecCFStringCopyEncodingDelimiters(CFDictionaryGetValue(certRecord, kCertStatsSubjectSummary), kCertStatsDelimiters);
    CFStringRef issuer = SecCFStringCopyEncodingDelimiters(CFDictionaryGetValue(certRecord, kCertStatsIssuerSummary), kCertStatsDelimiters);

	/* this generates a key which includes delimited fields to identify the certificate */
	CFStringRef keyStr = CFStringCreateWithFormat(NULL, NULL,
		kCertStatsFormat, /* format string */
		kCertStatsCert, /* certificate key */
		kCertStatsPolicy, policy,
		kCertStatsNotBefore, notBefore,
		kCertStatsNotAfter, notAfter,
		kCertStatsSerialNumber, serial,
		kCertStatsSubjectSummary, subject,
		kCertStatsIssuerSummary, issuer
	);
    CFReleaseSafe(policy);
    CFReleaseSafe(notBefore);
    CFReleaseSafe(notAfter);
    CFReleaseSafe(serial);
    CFReleaseSafe(subject);
    CFReleaseSafe(issuer);

	result = SetCertificateTraceValueForKey(keyStr, 1);
#if DEBUG
	secerror("%@.%@ (%d)", kCertStatsPrefix, keyStr, (result) ? 1 : 0);
#endif
	CFReleaseSafe(keyStr);

	return result;
}

