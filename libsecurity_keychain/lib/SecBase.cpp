/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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

#include <Security/SecBasePriv.h>
#include <Security/SecKeychainPriv.h>
#include <security_utilities/threading.h>
#include "SecBridge.h"

static CFStringRef copyErrorMessageFromBundle(OSStatus status,CFStringRef tableName);

// caller MUST release the string, since it is gotten with "CFCopyLocalizedStringFromTableInBundle"
// intended use of reserved param is to pass in CFStringRef with name of the Table for lookup
// Will look by default in "SecErrorMessages.strings" in the resources of Security.framework.


CFStringRef
SecCopyErrorMessageString(OSStatus status, void *reserved)
{
	try
	{
		CFStringRef result = copyErrorMessageFromBundle(status,CFSTR("SecErrorMessages"));
		if (result == NULL)
			result = copyErrorMessageFromBundle(status,CFSTR("SecDebugErrorMessages"));
		
		if (result == NULL)
		{
			if (status >= errSecErrnoBase && status <= errSecErrnoLimit)
			{
				result = CFStringCreateWithFormat (NULL, NULL, CFSTR("UNIX[%s]"), strerror(status-errSecErrnoBase));
			}
			else
			{
				// no error message found, so format a faked-up error message from the status
				result = CFStringCreateWithFormat(NULL, NULL, CFSTR("OSStatus %d"), status);
			}
		}
		
		return result;
	}
	catch (...)
	{
		return NULL;
	}
}


void
cssmPerror(const char *how, CSSM_RETURN error)
{
	try
	{
		const char* errMsg = cssmErrorString(error);
		fprintf(stderr, "%s: %s\n", how ? how : "error", errMsg);
	}
	catch (...)
	{
		fprintf(stderr, "failed to print error: %lu\n", (unsigned long)error);
	}
}


const char *
cssmErrorString(CSSM_RETURN error)
{
	static ThreadNexus<string> lastError;
	
	try {
		string err;
		
		if (error >= errSecErrnoBase && error <= errSecErrnoLimit)
		{
			err = string ("UNIX[") + strerror(error - errSecErrnoBase) + "]";
		}
		else
		{
			CFStringRef result = copyErrorMessageFromBundle(error,CFSTR("SecErrorMessages"));
			if (result == NULL)
				result = copyErrorMessageFromBundle(error,CFSTR("SecDebugErrorMessages"));
			err = cfString(result, true);
		}
		
		if (err.empty())
		{
			char buf[200];
			snprintf(buf, sizeof(buf), "unknown error %ld=%lx", (long) error, (long) error);
			err = buf;
		}

		lastError() = err;
		return lastError().c_str();
	}
	catch (...)
	{
		char buf[256];
		snprintf (buf, sizeof (buf), "unknown error %ld=%lx", (long) error, (long) error);
		lastError() = buf;
		return lastError().c_str();
	}
}


static ModuleNexus<Mutex> gBundleLock;

CFStringRef
copyErrorMessageFromBundle(OSStatus status,CFStringRef tableName)
{
	StLock<Mutex> _lock(gBundleLock());

    CFStringRef errorString = nil;
    CFStringRef keyString = nil;
    CFBundleRef secBundle = NULL;

    // Make a bundle instance using the URLRef.
    secBundle = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.security"));
    if (!secBundle)
        goto xit;
	
    // Convert status to Int32 string representation, e.g. "-25924"
    keyString = CFStringCreateWithFormat (kCFAllocatorDefault,NULL,CFSTR("%d"),status);
    if (!keyString)
        goto xit;

	errorString = CFCopyLocalizedStringFromTableInBundle(keyString,tableName,secBundle,NULL);
    if (CFStringCompare(errorString, keyString, 0)==kCFCompareEqualTo)	// no real error message
	{
		if (errorString)
			CFRelease(errorString);	
		 errorString = nil;
	}
xit:
    if (keyString)
        CFRelease(keyString);	

    return errorString;
}

/* Convert a possible CSSM type osStatus error to a more Keychain friendly OSStatus. */
OSStatus SecKeychainErrFromOSStatus(OSStatus osStatus)
{
	if (CSSM_ERR_IS_CONVERTIBLE(osStatus))
	{
		switch (CSSM_ERRCODE(osStatus))
		{
			// CONVERTIBLE ERROR CODES.
			case CSSM_ERRCODE_SERVICE_NOT_AVAILABLE:
				return errSecNotAvailable;
			case CSSM_ERRCODE_USER_CANCELED:
				return userCanceledErr;
			case CSSM_ERRCODE_OPERATION_AUTH_DENIED:
				return errSecAuthFailed;
			case CSSM_ERRCODE_NO_USER_INTERACTION:
				return errSecInteractionNotAllowed;
			case CSSM_ERRCODE_OS_ACCESS_DENIED:
                return wrPermErr;
            default:
				return osStatus;
		}
	}
	else
	{
		switch (osStatus)
		{
			// DL SPECIFIC ERROR CODES
            case CSSMERR_DL_OS_ACCESS_DENIED:
                return wrPermErr;
			case CSSMERR_DL_RECORD_NOT_FOUND:
				return errSecItemNotFound;
			case CSSMERR_DL_INVALID_UNIQUE_INDEX_DATA:
				return errSecDuplicateItem;
			case CSSMERR_DL_DATABASE_CORRUPT:
				return errSecInvalidKeychain;
			case CSSMERR_DL_DATASTORE_DOESNOT_EXIST:
				return errSecNoSuchKeychain;
			case CSSMERR_DL_DATASTORE_ALREADY_EXISTS:
				return errSecDuplicateKeychain;
			case CSSMERR_DL_INVALID_FIELD_NAME:
				return errSecNoSuchAttr;
			default:
				return osStatus;
		}
	}
}
