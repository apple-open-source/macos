/*
 * Copyright (c) 2003-2013 Apple Inc. All Rights Reserved.
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

#include <Security/SecBase.h>
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
				result = CFStringCreateWithFormat(NULL, NULL, CFSTR("OSStatus %d"), (int)status);
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
    keyString = CFStringCreateWithFormat (kCFAllocatorDefault,NULL,CFSTR("%d"),(int)status);
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
				return errSecUserCanceled;
			case CSSM_ERRCODE_OPERATION_AUTH_DENIED:
				return errSecAuthFailed;
			case CSSM_ERRCODE_NO_USER_INTERACTION:
				return errSecInteractionNotAllowed;
			case CSSM_ERRCODE_IN_DARK_WAKE:
				return errSecInDarkWake;
			case CSSM_ERRCODE_OS_ACCESS_DENIED:
                return errSecWrPerm;
			case CSSM_ERRCODE_INSUFFICIENT_CLIENT_IDENTIFICATION:
				return errSecInsufficientClientID;
			case CSSM_ERRCODE_DEVICE_RESET:
				return errSecDeviceReset;
			case CSSM_ERRCODE_DEVICE_FAILED:
				return errSecDeviceFailed;
			case CSSM_ERRCODE_INTERNAL_ERROR:
				return errSecInternalError;
			case CSSM_ERRCODE_MEMORY_ERROR:
				return errSecMemoryError;
			case CSSM_ERRCODE_MDS_ERROR:
				return errSecMDSError;
			case CSSM_ERRCODE_INVALID_POINTER:
			case CSSM_ERRCODE_INVALID_INPUT_POINTER:
			case CSSM_ERRCODE_INVALID_OUTPUT_POINTER:
			case CSSM_ERRCODE_INVALID_CERTGROUP_POINTER:
			case CSSM_ERRCODE_INVALID_CERT_POINTER:
			case CSSM_ERRCODE_INVALID_CRL_POINTER:
			case CSSM_ERRCODE_INVALID_FIELD_POINTER:
			case CSSM_ERRCODE_INVALID_DB_LIST_POINTER:
				return errSecInvalidPointer;
			case CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED:
				return errSecUnimplemented;		
			case CSSM_ERRCODE_SELF_CHECK_FAILED:
			case CSSMERR_CL_SELF_CHECK_FAILED:
			case CSSMERR_DL_SELF_CHECK_FAILED:
				return errSecSelfCheckFailed;
			case CSSM_ERRCODE_FUNCTION_FAILED:
				return errSecFunctionFailed;
			case CSSM_ERRCODE_MODULE_MANIFEST_VERIFY_FAILED:
				return errSecModuleManifestVerifyFailed;
			case CSSM_ERRCODE_INVALID_GUID:
				return errSecInvalidGUID;
			case CSSM_ERRCODE_OBJECT_USE_AUTH_DENIED:
			case CSSM_ERRCODE_OBJECT_MANIP_AUTH_DENIED:
				return errAuthorizationDenied;
			case CSSM_ERRCODE_OBJECT_ACL_NOT_SUPPORTED:	
			case CSSM_ERRCODE_OBJECT_ACL_REQUIRED:
			case CSSM_ERRCODE_INVALID_ACL_SUBJECT_VALUE:
			case CSSM_ERRCODE_ACL_SUBJECT_TYPE_NOT_SUPPORTED:
			case CSSM_ERRCODE_INVALID_ACL_EDIT_MODE:
			case CSSM_ERRCODE_INVALID_NEW_ACL_ENTRY:
			case CSSM_ERRCODE_INVALID_NEW_ACL_OWNER:
				return errSecInvalidACL;
			case CSSM_ERRCODE_INVALID_ACCESS_CREDENTIALS:
				return errSecInvalidAccessCredentials;
			case CSSM_ERRCODE_INVALID_ACL_BASE_CERTS:
			case CSSM_ERRCODE_ACL_BASE_CERTS_NOT_SUPPORTED:
				return errSecInvalidCertificateGroup;
			case CSSM_ERRCODE_INVALID_SAMPLE_VALUE:
				return errSecInvalidSampleValue;
			case CSSM_ERRCODE_SAMPLE_VALUE_NOT_SUPPORTED:
				return errSecInvalidSampleValue;
			case CSSM_ERRCODE_INVALID_ACL_CHALLENGE_CALLBACK:
				return errSecInvalidCallback;
			case CSSM_ERRCODE_ACL_CHALLENGE_CALLBACK_FAILED:
				return errSecCallbackFailed;
			case CSSM_ERRCODE_INVALID_ACL_ENTRY_TAG:
			case CSSM_ERRCODE_UNKNOWN_TAG:
				return errSecUnknownTag;
			case CSSM_ERRCODE_ACL_ENTRY_TAG_NOT_FOUND:
				return errSecTagNotFound;
			case CSSM_ERRCODE_ACL_CHANGE_FAILED:
				return errSecACLChangeFailed;
			case CSSM_ERRCODE_ACL_DELETE_FAILED:
				return errSecACLDeleteFailed;
			case CSSM_ERRCODE_ACL_REPLACE_FAILED:
				return errSecACLReplaceFailed;
			case CSSM_ERRCODE_ACL_ADD_FAILED:
				return errSecACLAddFailed;
			case CSSM_ERRCODE_INVALID_CONTEXT_HANDLE:
			case CSSM_ERRCODE_INVALID_DB_HANDLE:
			case CSSM_ERRCODE_INVALID_CSP_HANDLE:
			case CSSM_ERRCODE_INVALID_DL_HANDLE:
			case CSSM_ERRCODE_INVALID_CL_HANDLE:
			case CSSM_ERRCODE_INVALID_TP_HANDLE:
			case CSSM_ERRCODE_INVALID_KR_HANDLE:
			case CSSM_ERRCODE_INVALID_AC_HANDLE:
				return errSecInvalidHandle;
			case CSSM_ERRCODE_INCOMPATIBLE_VERSION:
				return errSecIncompatibleVersion;
			case CSSM_ERRCODE_INVALID_DATA:
				return errSecInvalidData;
			case CSSM_ERRCODE_CRL_ALREADY_SIGNED:
				return errSecCRLAlreadySigned;
			case CSSM_ERRCODE_INVALID_NUMBER_OF_FIELDS:
				return errSecInvalidNumberOfFields;
			case CSSM_ERRCODE_VERIFICATION_FAILURE:
				return errSecVerificationFailure;
			case CSSM_ERRCODE_PRIVILEGE_NOT_GRANTED:
				return errSecPrivilegeNotGranted;
			case CSSM_ERRCODE_INVALID_DB_LIST:
				return errSecInvalidDBList;
			case CSSM_ERRCODE_UNKNOWN_FORMAT:
				return errSecUnknownFormat;
			case CSSM_ERRCODE_INVALID_PASSTHROUGH_ID:
				return errSecInvalidPassthroughID;
			case CSSM_ERRCODE_INVALID_NETWORK_ADDR:
				return errSecInvalidNetworkAddress;
			case CSSM_ERRCODE_INVALID_CRYPTO_DATA:
				return errSecInvalidData;
		}
	}
	switch (osStatus)
	{
		// Some CSSM errors mapped to OSStatus-type (SnowLeopard and earlier).
		//
		case CSSMERR_DL_RECORD_NOT_FOUND:
		case CSSMERR_APPLETP_CERT_NOT_FOUND_FROM_ISSUER:
		case CSSMERR_CSP_PRIVATE_KEY_NOT_FOUND:
			return errSecItemNotFound;
		case CSSMERR_DL_INVALID_UNIQUE_INDEX_DATA: 
		case CSSMERR_CSP_PRIVATE_KEY_ALREADY_EXISTS: 
		case CSSMERR_CSP_KEY_LABEL_ALREADY_EXISTS:
			return errSecDuplicateItem;
		case CSSMERR_DL_DATABASE_CORRUPT:
			return errSecInvalidKeychain;
		case CSSMERR_DL_DATASTORE_DOESNOT_EXIST:
			return errSecNoSuchKeychain;
		case CSSMERR_DL_DATASTORE_ALREADY_EXISTS:
			return errSecDuplicateKeychain;
		case CSSMERR_APPLEDL_DISK_FULL:
			return errSecDskFull;
		case CSSMERR_DL_INVALID_OPEN_PARAMETERS: 
		case CSSMERR_APPLEDL_INVALID_OPEN_PARAMETERS:
		case CSSMERR_APPLE_DOTMAC_REQ_SERVER_PARAM:
			return errSecParam;
		case CSSMERR_DL_INVALID_FIELD_NAME: 
		case CSSMERR_CSSM_INVALID_ATTRIBUTE:
			return errSecNoSuchAttr;
		case CSSMERR_DL_OS_ACCESS_DENIED: 
		case CSSMERR_CSP_OS_ACCESS_DENIED:
		case CSSMERR_TP_OS_ACCESS_DENIED: 
		case CSSMERR_AC_OS_ACCESS_DENIED: 
		case CSSMERR_CL_OS_ACCESS_DENIED:
			return errSecWrPerm;
		case CSSMERR_CSSM_BUFFER_TOO_SMALL:
			return errSecBufferTooSmall;
		case CSSMERR_CSSM_FUNCTION_NOT_IMPLEMENTED:
		case CSSMERR_CSP_FUNCTION_NOT_IMPLEMENTED:
		case CSSMERR_TP_FUNCTION_NOT_IMPLEMENTED:
		case CSSMERR_AC_FUNCTION_NOT_IMPLEMENTED:
		case CSSMERR_CL_FUNCTION_NOT_IMPLEMENTED:
		case CSSMERR_DL_FUNCTION_NOT_IMPLEMENTED:
		case CSSMERR_APPLE_DOTMAC_REQ_SERVER_UNIMPL:
			return errSecUnimplemented;
		case CSSMERR_CSSM_INTERNAL_ERROR:
		case CSSMERR_CSP_INTERNAL_ERROR:
		case CSSMERR_TP_INTERNAL_ERROR:
		case CSSMERR_AC_INTERNAL_ERROR: 
		case CSSMERR_CL_INTERNAL_ERROR:
		case CSSMERR_DL_INTERNAL_ERROR:
			return errSecInternalError;
		case CSSMERR_CSSM_MEMORY_ERROR:
		case CSSMERR_CSP_MEMORY_ERROR:
		case CSSMERR_TP_MEMORY_ERROR:
		case CSSMERR_AC_MEMORY_ERROR:
		case CSSMERR_CSP_DEVICE_MEMORY_ERROR: 
		case CSSMERR_CL_MEMORY_ERROR:
		case CSSMERR_DL_MEMORY_ERROR:
			return errSecMemoryError;
		case CSSMERR_CSSM_MDS_ERROR:
		case CSSMERR_CSP_MDS_ERROR:
		case CSSMERR_TP_MDS_ERROR:
		case CSSMERR_AC_MDS_ERROR:
		case CSSMERR_CL_MDS_ERROR:
		case CSSMERR_DL_MDS_ERROR:
			return errSecMDSError;
		case CSSMERR_CSSM_INVALID_POINTER:
		case CSSMERR_CSP_INVALID_POINTER:
		case CSSMERR_TP_INVALID_POINTER:
		case CSSMERR_AC_INVALID_POINTER:
		case CSSMERR_CL_INVALID_POINTER:
		case CSSMERR_DL_INVALID_POINTER:
		case CSSMERR_CSSM_INVALID_INPUT_POINTER:
		case CSSMERR_CSP_INVALID_INPUT_POINTER:
		case CSSMERR_TP_INVALID_INPUT_POINTER: 
		case CSSMERR_AC_INVALID_INPUT_POINTER:
		case CSSMERR_CL_INVALID_INPUT_POINTER:
		case CSSMERR_DL_INVALID_INPUT_POINTER:
		case CSSMERR_TP_INVALID_DB_LIST_POINTER:
		case CSSMERR_AC_INVALID_DB_LIST_POINTER:
		case CSSMERR_DL_INVALID_DB_LIST_POINTER:
		case CSSMERR_TP_INVALID_CERTGROUP_POINTER:
		case CSSMERR_TP_INVALID_CERT_POINTER:
		case CSSMERR_TP_INVALID_CRL_POINTER:
		case CSSMERR_TP_INVALID_FIELD_POINTER:
		case CSSMERR_CSP_INVALID_KEY_POINTER:
		case CSSMERR_TP_INVALID_CALLERAUTH_CONTEXT_POINTER:
		case CSSMERR_TP_INVALID_IDENTIFIER_POINTER:
		case CSSMERR_TP_INVALID_CRLGROUP_POINTER:
		case CSSMERR_TP_INVALID_TUPLEGROUP_POINTER:
		case CSSMERR_CL_INVALID_CERTGROUP_POINTER:
		case CSSMERR_CL_INVALID_CERT_POINTER:
		case CSSMERR_CL_INVALID_CRL_POINTER:
		case CSSMERR_CL_INVALID_FIELD_POINTER:
		case CSSMERR_CL_INVALID_BUNDLE_POINTER:
		case CSSMERR_CSSM_INVALID_OUTPUT_POINTER:
		case CSSMERR_CSP_INVALID_OUTPUT_POINTER:
		case CSSMERR_TP_INVALID_OUTPUT_POINTER: 
		case CSSMERR_AC_INVALID_OUTPUT_POINTER:
		case CSSMERR_CL_INVALID_OUTPUT_POINTER:
		case CSSMERR_DL_INVALID_OUTPUT_POINTER:
			return errSecInvalidPointer;
		case CSSMERR_CSSM_FUNCTION_FAILED:
		case CSSMERR_CSP_FUNCTION_FAILED:
		case CSSMERR_TP_FUNCTION_FAILED:
		case CSSMERR_AC_FUNCTION_FAILED:
		case CSSMERR_CL_FUNCTION_FAILED:
		case CSSMERR_DL_FUNCTION_FAILED:
			return errSecFunctionFailed;
		case CSSMERR_CSP_INVALID_DATA:
		case CSSMERR_TP_INVALID_DATA:
		case CSSMERR_AC_INVALID_DATA:
		case CSSMERR_CL_INVALID_DATA:
		case CSSMERR_CSP_INVALID_CRYPTO_DATA:
		case CSSMERR_CSP_INVALID_DATA_COUNT:
		case CSSMERR_TP_INVALID_ACTION_DATA:
			return errSecInvalidData;
		case CSSMERR_TP_INVALID_DB_LIST:
		case CSSMERR_AC_INVALID_DB_LIST:
			return errSecInvalidDBList;
		case CSSMERR_CSP_INVALID_PASSTHROUGH_ID:
		case CSSMERR_TP_INVALID_PASSTHROUGH_ID:
		case CSSMERR_AC_INVALID_PASSTHROUGH_ID: 
		case CSSMERR_CL_INVALID_PASSTHROUGH_ID: 
		case CSSMERR_DL_INVALID_PASSTHROUGH_ID:
			return errSecInvalidPassthroughID;
		case CSSMERR_TP_INVALID_CSP_HANDLE:
		case CSSMERR_TP_INVALID_CL_HANDLE:
		case CSSMERR_TP_INVALID_DL_HANDLE:
		case CSSMERR_AC_INVALID_TP_HANDLE:
		case CSSMERR_AC_INVALID_DL_HANDLE:
		case CSSMERR_DL_INVALID_DL_HANDLE:
		case CSSMERR_AC_INVALID_CL_HANDLE:
		case CSSMERR_DL_INVALID_CL_HANDLE:
		case CSSMERR_DL_INVALID_CSP_HANDLE:
		case CSSMERR_TP_INVALID_DB_HANDLE:
		case CSSMERR_CSSM_INVALID_ADDIN_HANDLE:
		case CSSMERR_CSSM_INVALID_CONTEXT_HANDLE:
		case CSSMERR_CL_INVALID_CACHE_HANDLE:
		case CSSMERR_CL_INVALID_RESULTS_HANDLE:
		case CSSMERR_DL_INVALID_RESULTS_HANDLE:
		case CSSMERR_TP_INVALID_KEYCACHE_HANDLE:
		case CSSMERR_CSP_INVALID_CONTEXT_HANDLE:
		case CSSMERR_TP_INVALID_CONTEXT_HANDLE:
		case CSSMERR_AC_INVALID_CONTEXT_HANDLE:
		case CSSMERR_CL_INVALID_CONTEXT_HANDLE:
			return errSecInvalidHandle;
		case CSSMERR_TP_CRL_ALREADY_SIGNED:
		case CSSMERR_CL_CRL_ALREADY_SIGNED:
			return errSecCRLAlreadySigned;
		case CSSMERR_TP_INVALID_NUMBER_OF_FIELDS:
		case CSSMERR_CL_INVALID_NUMBER_OF_FIELDS:
			return errSecInvalidNumberOfFields;
		case CSSMERR_TP_VERIFICATION_FAILURE:
		case CSSMERR_CL_VERIFICATION_FAILURE:
			return errSecVerificationFailure;
		case CSSMERR_TP_INVALID_NETWORK_ADDR:
		case CSSMERR_DL_INVALID_NETWORK_ADDR:
			return errSecInvalidNetworkAddress;
		case CSSMERR_TP_UNKNOWN_TAG:
		case CSSMERR_CL_UNKNOWN_TAG:
		case CSSMERR_CSP_INVALID_ACL_ENTRY_TAG:
		case CSSMERR_DL_INVALID_ACL_ENTRY_TAG:
		case CSSMERR_DL_INVALID_SELECTION_TAG:
			return errSecUnknownTag;
		case CSSMERR_CSP_INVALID_SIGNATURE:
		case CSSMERR_TP_INVALID_SIGNATURE:
			return errSecInvalidSignature;
		case CSSMERR_CSSM_USER_CANCELED:
		case CSSMERR_CSP_USER_CANCELED:
		case CSSMERR_TP_USER_CANCELED:
		case CSSMERR_AC_USER_CANCELED:
		case CSSMERR_CL_USER_CANCELED:
		case CSSMERR_DL_USER_CANCELED:
			return errSecUserCanceled;
		case CSSMERR_CSSM_NO_USER_INTERACTION:
		case CSSMERR_CSP_NO_USER_INTERACTION:
		case CSSMERR_TP_NO_USER_INTERACTION:
		case CSSMERR_AC_NO_USER_INTERACTION:
		case CSSMERR_CL_NO_USER_INTERACTION:
		case CSSMERR_DL_NO_USER_INTERACTION:
			return errSecInteractionNotAllowed;
		case CSSMERR_CSSM_IN_DARK_WAKE:
		case CSSMERR_CSP_IN_DARK_WAKE:
		case CSSMERR_TP_IN_DARK_WAKE:
		case CSSMERR_AC_IN_DARK_WAKE:
		case CSSMERR_CL_IN_DARK_WAKE:
		case CSSMERR_DL_IN_DARK_WAKE:
			return errSecInDarkWake;
		case CSSMERR_CSSM_SERVICE_NOT_AVAILABLE:
		case CSSMERR_CSP_SERVICE_NOT_AVAILABLE:
		case CSSMERR_TP_SERVICE_NOT_AVAILABLE:
		case CSSMERR_AC_SERVICE_NOT_AVAILABLE:
		case CSSMERR_CL_SERVICE_NOT_AVAILABLE:
		case CSSMERR_DL_SERVICE_NOT_AVAILABLE:
			return errSecServiceNotAvailable;
		case CSSMERR_CSSM_INSUFFICIENT_CLIENT_IDENTIFICATION:
		case CSSMERR_CSP_INSUFFICIENT_CLIENT_IDENTIFICATION:
		case CSSMERR_TP_INSUFFICIENT_CLIENT_IDENTIFICATION:
		case CSSMERR_AC_INSUFFICIENT_CLIENT_IDENTIFICATION:
		case CSSMERR_CL_INSUFFICIENT_CLIENT_IDENTIFICATION:
		case CSSMERR_DL_INSUFFICIENT_CLIENT_IDENTIFICATION:
			return errSecInsufficientClientID;
		case CSSMERR_CSSM_DEVICE_RESET:
		case CSSMERR_CSP_DEVICE_RESET:
		case CSSMERR_TP_DEVICE_RESET:
		case CSSMERR_AC_DEVICE_RESET:
		case CSSMERR_CL_DEVICE_RESET:
		case CSSMERR_DL_DEVICE_RESET:
			return errSecDeviceReset;
		case CSSMERR_CSSM_DEVICE_FAILED:
		case CSSMERR_CSP_DEVICE_FAILED:
		case CSSMERR_TP_DEVICE_FAILED:
		case CSSMERR_AC_DEVICE_FAILED:
		case CSSMERR_CL_DEVICE_FAILED:
		case CSSMERR_DL_DEVICE_FAILED:
			return errSecDeviceFailed;
		case CSSMERR_APPLE_DOTMAC_REQ_SERVER_AUTH:
		case CSSMERR_CSSM_EMM_AUTHENTICATE_FAILED:
		case CSSMERR_CSSM_ADDIN_AUTHENTICATE_FAILED:
		case CSSMERR_CSP_OPERATION_AUTH_DENIED:
		case CSSMERR_CSP_OBJECT_USE_AUTH_DENIED:
		case CSSMERR_CSP_OBJECT_MANIP_AUTH_DENIED:
		case CSSMERR_TP_AUTHENTICATION_FAILED:
		case CSSMERR_DL_OPERATION_AUTH_DENIED:
		case CSSMERR_DL_OBJECT_USE_AUTH_DENIED:
		case CSSMERR_DL_OBJECT_MANIP_AUTH_DENIED:
			return errAuthorizationDenied;
		case CSSMERR_CSSM_SCOPE_NOT_SUPPORTED:
		case CSSMERR_CL_SCOPE_NOT_SUPPORTED:
		case CSSMERR_CL_INVALID_SCOPE:
			return errSecInvalidScope;
		case CSSMERR_TP_INVALID_NAME:
		case CSSMERR_DL_INVALID_DB_NAME:
			return errSecInvalidName;
		case CSSMERR_APPLETP_BAD_CERT_FROM_ISSUER:
		case CSSMERR_TP_INVALID_CERTIFICATE:
		case CSSMERR_TP_INVALID_ANCHOR_CERT:
		case CSSMERR_APPLETP_CRL_INVALID_ANCHOR_CERT:
		case CSSMERR_APPLETP_OCSP_INVALID_ANCHOR_CERT:
			return errSecInvalidCertificateRef;
		case CSSMERR_CSP_ACL_ENTRY_TAG_NOT_FOUND:
		case CSSMERR_DL_ACL_ENTRY_TAG_NOT_FOUND:
			return errSecTagNotFound;
		case CSSMERR_DL_UNSUPPORTED_QUERY:
		case CSSMERR_DL_INVALID_QUERY:
			return errSecInvalidQuery;
		case CSSMERR_CSP_INVALID_ACL_CHALLENGE_CALLBACK:
		case CSSMERR_TP_INVALID_CALLBACK: 
		case CSSMERR_DL_INVALID_ACL_CHALLENGE_CALLBACK:
			return errSecInvalidCallback;
		case CSSMERR_CSP_ACL_CHALLENGE_CALLBACK_FAILED:
		case CSSMERR_CSP_CRYPTO_DATA_CALLBACK_FAILED:
		case CSSMERR_DL_ACL_CHALLENGE_CALLBACK_FAILED:
			return errSecCallbackFailed;
		case CSSMERR_TP_INVALID_CERTGROUP:
		case CSSMERR_TP_CERTGROUP_INCOMPLETE:
		case CSSMERR_DL_INVALID_ACL_BASE_CERTS:
		case CSSMERR_DL_ACL_BASE_CERTS_NOT_SUPPORTED:
		case CSSMERR_CSP_INVALID_ACL_BASE_CERTS:
			return errSecInvalidCertificateGroup;
		case CSSMERR_CSP_ACL_DELETE_FAILED:
		case CSSMERR_DL_ACL_DELETE_FAILED:
			return errSecACLDeleteFailed;
		case CSSMERR_CSP_ACL_REPLACE_FAILED:
		case CSSMERR_DL_ACL_REPLACE_FAILED:
			return errSecACLReplaceFailed;
		case CSSMERR_CSP_ACL_ADD_FAILED:
		case CSSMERR_DL_ACL_ADD_FAILED:
			return errSecACLAddFailed;
		case CSSMERR_DL_ACL_CHANGE_FAILED:
		case CSSMERR_CSP_ACL_CHANGE_FAILED:
			return errSecACLChangeFailed;
		case CSSMERR_CSSM_PRIVILEGE_NOT_GRANTED:
		case CSSMERR_CSP_PRIVILEGE_NOT_GRANTED:
			return errSecPrivilegeNotGranted;
		case CSSMERR_CSP_INVALID_ACCESS_CREDENTIALS:
		case CSSMERR_DL_INVALID_ACCESS_CREDENTIALS:
			return errSecInvalidAccessCredentials;
		case CSSMERR_DL_INVALID_RECORD_INDEX:
		case CSSMERR_DL_INVALID_RECORDTYPE:
		case CSSMERR_DL_UNSUPPORTED_RECORDTYPE:
		case CSSMERR_DL_INVALID_RECORD_UID:
		case CSSMERR_DL_STALE_UNIQUE_RECORD:
			return errSecInvalidRecord;
		case CSSMERR_CSP_INVALID_KEY:
		case CSSMERR_CSP_INVALID_KEY_REFERENCE:
		case CSSMERR_CSP_INVALID_KEY_CLASS:
			return errSecInvalidKeyRef;
		case CSSMERR_CSP_OBJECT_ACL_NOT_SUPPORTED:
		case CSSMERR_CSP_OBJECT_ACL_REQUIRED:
		case CSSMERR_CSP_ACL_BASE_CERTS_NOT_SUPPORTED:
		case CSSMERR_CSP_INVALID_ACL_SUBJECT_VALUE:
		case CSSMERR_CSP_ACL_SUBJECT_TYPE_NOT_SUPPORTED:
		case CSSMERR_DL_OBJECT_ACL_NOT_SUPPORTED:
		case CSSMERR_DL_OBJECT_ACL_REQUIRED:
		case CSSMERR_DL_INVALID_ACL_SUBJECT_VALUE:
		case CSSMERR_DL_ACL_SUBJECT_TYPE_NOT_SUPPORTED:
		case CSSMERR_DL_INVALID_NEW_ACL_ENTRY:
		case CSSMERR_DL_INVALID_NEW_ACL_OWNER:
		case CSSMERR_DL_INVALID_ACL_EDIT_MODE:
		case CSSMERR_CSP_INVALID_ACL_EDIT_MODE:
		case CSSMERR_CSP_INVALID_NEW_ACL_ENTRY:
		case CSSMERR_CSP_INVALID_NEW_ACL_OWNER:
			return errSecInvalidACL;
		case CSSMERR_CSP_INVALID_SAMPLE_VALUE:
		case CSSMERR_DL_INVALID_SAMPLE_VALUE:
		case CSSMERR_CSP_SAMPLE_VALUE_NOT_SUPPORTED: 
		case CSSMERR_DL_SAMPLE_VALUE_NOT_SUPPORTED:
			return errSecInvalidSampleValue;
		case CSSMERR_TP_UNKNOWN_FORMAT:
		case CSSMERR_CL_UNKNOWN_FORMAT:
			return errSecUnknownFormat;
		case CSSMERR_CSP_APPLE_ADD_APPLICATION_ACL_SUBJECT:
			return errSecAppleAddAppACLSubject;
		case CSSMERR_CSP_APPLE_PUBLIC_KEY_INCOMPLETE:
			return errSecApplePublicKeyIncomplete;
		case CSSMERR_CSP_APPLE_SIGNATURE_MISMATCH:
			return errSecAppleSignatureMismatch;
		case CSSMERR_CSP_APPLE_INVALID_KEY_START_DATE:
			return errSecAppleInvalidKeyStartDate;
		case CSSMERR_CSP_APPLE_INVALID_KEY_END_DATE:
			return errSecAppleInvalidKeyEndDate;
		case CSSMERR_CSPDL_APPLE_DL_CONVERSION_ERROR:
			return errSecConversionError;
		case CSSMERR_CSP_APPLE_SSLv2_ROLLBACK:
			return errSecAppleSSLv2Rollback;
		case CSSMERR_APPLEDL_QUOTA_EXCEEDED:
			return errSecQuotaExceeded;
		case CSSMERR_APPLEDL_FILE_TOO_BIG:
			return errSecFileTooBig;
		case CSSMERR_APPLEDL_INVALID_DATABASE_BLOB:
			return errSecInvalidDatabaseBlob;
		case CSSMERR_APPLEDL_INVALID_KEY_BLOB:
			return errSecInvalidKeyBlob;
		case CSSMERR_APPLEDL_INCOMPATIBLE_DATABASE_BLOB:
			return errSecIncompatibleDatabaseBlob;
		case CSSMERR_APPLEDL_INCOMPATIBLE_KEY_BLOB:
			return errSecIncompatibleKeyBlob;
		case CSSMERR_APPLETP_HOSTNAME_MISMATCH:
			return errSecHostNameMismatch;
		case CSSMERR_APPLETP_UNKNOWN_CRITICAL_EXTEN:
			return errSecUnknownCriticalExtensionFlag;
		case CSSMERR_APPLETP_NO_BASIC_CONSTRAINTS:
			return errSecNoBasicConstraints;
		case CSSMERR_APPLETP_INVALID_AUTHORITY_ID:
			return errSecInvalidAuthorityKeyID;
		case CSSMERR_APPLETP_INVALID_SUBJECT_ID:
			return errSecInvalidSubjectKeyID;
		case CSSMERR_APPLETP_INVALID_KEY_USAGE:
			return errSecInvalidKeyUsageForPolicy;
		case CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE:
			return errSecInvalidExtendedKeyUsage;
		case CSSMERR_APPLETP_INVALID_ID_LINKAGE:
			return errSecInvalidIDLinkage;
		case CSSMERR_APPLETP_PATH_LEN_CONSTRAINT:
			return errSecPathLengthConstraintExceeded;
		case CSSMERR_APPLETP_INVALID_ROOT:
			return errSecInvalidRoot;
		case CSSMERR_APPLETP_CRL_EXPIRED:
			return errSecCRLExpired;
		case CSSMERR_APPLETP_CRL_NOT_VALID_YET:
			return errSecCRLNotValidYet;
		case CSSMERR_APPLETP_CRL_NOT_FOUND:
			return errSecCRLNotFound;
		case CSSMERR_APPLETP_CRL_SERVER_DOWN:
			return errSecCRLServerDown;
		case CSSMERR_APPLETP_CRL_BAD_URI:
			return errSecCRLBadURI;
		case CSSMERR_APPLETP_UNKNOWN_CERT_EXTEN:
			return errSecUnknownCertExtension;
		case CSSMERR_APPLETP_UNKNOWN_CRL_EXTEN:
			return errSecUnknownCRLExtension;
		case CSSMERR_APPLETP_CRL_NOT_TRUSTED:
			return errSecCRLNotTrusted;
		case CSSMERR_APPLETP_CRL_POLICY_FAIL:
			return errSecCRLPolicyFailed;
		case CSSMERR_APPLETP_IDP_FAIL:
			return errSecIDPFailure;
		case CSSMERR_APPLETP_SMIME_EMAIL_ADDRS_NOT_FOUND:
			return errSecSMIMEEmailAddressesNotFound;
		case CSSMERR_APPLETP_SMIME_BAD_EXT_KEY_USE:
			return errSecSMIMEBadExtendedKeyUsage;
		case CSSMERR_APPLETP_SMIME_BAD_KEY_USE:
			return errSecSMIMEBadKeyUsage;
		case CSSMERR_APPLETP_SMIME_KEYUSAGE_NOT_CRITICAL:
			return errSecSMIMEKeyUsageNotCritical;
		case CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS:
			return errSecSMIMENoEmailAddress;
		case CSSMERR_APPLETP_SMIME_SUBJ_ALT_NAME_NOT_CRIT:
			return errSecSMIMESubjAltNameNotCritical;
		case CSSMERR_APPLETP_SSL_BAD_EXT_KEY_USE:
			return errSecSSLBadExtendedKeyUsage;
		case CSSMERR_APPLETP_OCSP_BAD_RESPONSE:
			return errSecOCSPBadResponse;
		case CSSMERR_APPLETP_OCSP_BAD_REQUEST:
			return errSecOCSPBadRequest;
		case CSSMERR_APPLETP_OCSP_UNAVAILABLE:
			return errSecOCSPUnavailable;
		case CSSMERR_APPLETP_OCSP_STATUS_UNRECOGNIZED:
			return errSecOCSPStatusUnrecognized;
		case CSSMERR_APPLETP_INCOMPLETE_REVOCATION_CHECK:
			return errSecIncompleteCertRevocationCheck;
		case CSSMERR_APPLETP_NETWORK_FAILURE:
			return errSecNetworkFailure;
		case CSSMERR_APPLETP_OCSP_NOT_TRUSTED:
			return errSecOCSPNotTrustedToAnchor;
		case CSSMERR_APPLETP_OCSP_SIG_ERROR:
			return errSecOCSPSignatureError;
		case CSSMERR_APPLETP_OCSP_NO_SIGNER:
			return errSecOCSPNoSigner;
		case CSSMERR_APPLETP_OCSP_RESP_MALFORMED_REQ:
			return errSecOCSPResponderMalformedReq;
		case CSSMERR_APPLETP_OCSP_RESP_INTERNAL_ERR:
			return errSecOCSPResponderInternalError;
		case CSSMERR_APPLETP_OCSP_RESP_TRY_LATER:
			return errSecOCSPResponderTryLater;
		case CSSMERR_APPLETP_OCSP_RESP_SIG_REQUIRED:
			return errSecOCSPResponderSignatureRequired;
		case CSSMERR_APPLETP_OCSP_RESP_UNAUTHORIZED:
			return errSecOCSPResponderUnauthorized;
		case CSSMERR_APPLETP_OCSP_NONCE_MISMATCH:
			return errSecOCSPResponseNonceMismatch;
		case CSSMERR_APPLETP_CS_BAD_CERT_CHAIN_LENGTH:
			return errSecCodeSigningBadCertChainLength;
		case CSSMERR_APPLETP_CS_NO_BASIC_CONSTRAINTS:
			return errSecCodeSigningNoBasicConstraints;
		case CSSMERR_APPLETP_CS_BAD_PATH_LENGTH:
			return errSecCodeSigningBadPathLengthConstraint;
		case CSSMERR_APPLETP_CS_NO_EXTENDED_KEY_USAGE:
			return errSecCodeSigningNoExtendedKeyUsage;
		case CSSMERR_APPLETP_CODE_SIGN_DEVELOPMENT:
			return errSecCodeSigningDevelopment;
		case CSSMERR_APPLETP_RS_BAD_CERT_CHAIN_LENGTH:
			return errSecResourceSignBadCertChainLength;
		case CSSMERR_APPLETP_RS_BAD_EXTENDED_KEY_USAGE:
			return errSecResourceSignBadExtKeyUsage;
		case CSSMERR_APPLETP_TRUST_SETTING_DENY:
			return errSecTrustSettingDeny;
		case CSSMERR_APPLETP_INVALID_EMPTY_SUBJECT:
			return errSecInvalidSubjectName;
		case CSSMERR_APPLETP_UNKNOWN_QUAL_CERT_STATEMENT:
			return errSecUnknownQualifiedCertStatement;
		case CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION:
			return errSecMissingRequiredExtension;
		case CSSMERR_APPLETP_EXT_KEYUSAGE_NOT_CRITICAL:
			return errSecExtendedKeyUsageNotCritical;
		case CSSMERR_APPLE_DOTMAC_REQ_QUEUED:
			return errSecMobileMeRequestQueued;
		case CSSMERR_APPLE_DOTMAC_REQ_REDIRECT:
			return errSecMobileMeRequestRedirected;
		case CSSMERR_APPLE_DOTMAC_REQ_SERVER_ERR:
			return errSecMobileMeServerError;
		case CSSMERR_APPLE_DOTMAC_REQ_SERVER_NOT_AVAIL:
			return errSecMobileMeServerNotAvailable;
		case CSSMERR_APPLE_DOTMAC_REQ_SERVER_ALREADY_EXIST:
			return errSecMobileMeServerAlreadyExists;
		case CSSMERR_APPLE_DOTMAC_REQ_SERVER_SERVICE_ERROR:
			return errSecMobileMeServerServiceErr;
		case CSSMERR_APPLE_DOTMAC_REQ_IS_PENDING:
			return errSecMobileMeRequestAlreadyPending;
		case CSSMERR_APPLE_DOTMAC_NO_REQ_PENDING:
			return errSecMobileMeNoRequestPending;
		case CSSMERR_APPLE_DOTMAC_CSR_VERIFY_FAIL:
			return errSecMobileMeCSRVerifyFailure;
		case CSSMERR_APPLE_DOTMAC_FAILED_CONSISTENCY_CHECK:
			return errSecMobileMeFailedConsistencyCheck;
		case CSSMERR_CSSM_NOT_INITIALIZED:
			return errSecNotInitialized;
		case CSSMERR_CSSM_INVALID_HANDLE_USAGE:
			return errSecInvalidHandleUsage;
		case CSSMERR_CSSM_PVC_REFERENT_NOT_FOUND:
			return errSecPVCReferentNotFound;
		case CSSMERR_CSSM_FUNCTION_INTEGRITY_FAIL:
			return errSecFunctionIntegrityFail;
		case CSSMERR_CSSM_SELF_CHECK_FAILED:
		case CSSMERR_CSP_SELF_CHECK_FAILED:
			return errSecSelfCheckFailed;
		case CSSMERR_CSSM_MODULE_MANIFEST_VERIFY_FAILED:
			return errSecModuleManifestVerifyFailed;
		case CSSMERR_CSSM_INVALID_GUID:
			return errSecInvalidGUID;
		case CSSMERR_CSSM_INCOMPATIBLE_VERSION:
			return errSecIncompatibleVersion;
		case CSSMERR_CSSM_PVC_ALREADY_CONFIGURED:
			return errSecPVCAlreadyConfigured;
		case CSSMERR_CSSM_INVALID_PVC:
			return errSecInvalidPVC;
		case CSSMERR_CSSM_EMM_LOAD_FAILED:
			return errSecEMMLoadFailed;
		case CSSMERR_CSSM_EMM_UNLOAD_FAILED:
			return errSecEMMUnloadFailed;
		case CSSMERR_CSSM_ADDIN_LOAD_FAILED:
			return errSecAddinLoadFailed;
		case CSSMERR_CSSM_INVALID_KEY_HIERARCHY:
			return errSecInvalidKeyHierarchy;
		case CSSMERR_CSSM_ADDIN_UNLOAD_FAILED:
			return errSecAddinUnloadFailed;
		case CSSMERR_CSSM_LIB_REF_NOT_FOUND:
			return errSecLibraryReferenceNotFound;
		case CSSMERR_CSSM_INVALID_ADDIN_FUNCTION_TABLE:
			return errSecInvalidAddinFunctionTable;
		case CSSMERR_CSSM_INVALID_SERVICE_MASK:
			return errSecInvalidServiceMask;
		case CSSMERR_CSSM_MODULE_NOT_LOADED:
			return errSecModuleNotLoaded;
		case CSSMERR_CSSM_INVALID_SUBSERVICEID:
			return errSecInvalidSubServiceID;
		case CSSMERR_CSSM_ATTRIBUTE_NOT_IN_CONTEXT:
			return errSecAttributeNotInContext;
		case CSSMERR_CSSM_MODULE_MANAGER_INITIALIZE_FAIL:
			return errSecModuleManagerInitializeFailed;
		case CSSMERR_CSSM_MODULE_MANAGER_NOT_FOUND:
			return errSecModuleManagerNotFound;
		case CSSMERR_CSSM_EVENT_NOTIFICATION_CALLBACK_NOT_FOUND:
			return errSecEventNotificationCallbackNotFound;
		case CSSMERR_CSP_INPUT_LENGTH_ERROR:
			return errSecInputLengthError;
		case CSSMERR_CSP_OUTPUT_LENGTH_ERROR:
			return errSecOutputLengthError;
		case CSSMERR_CSP_PRIVILEGE_NOT_SUPPORTED:
			return errSecPrivilegeNotSupported;
		case CSSMERR_CSP_DEVICE_ERROR:
			return errSecDeviceError;
		case CSSMERR_CSP_ATTACH_HANDLE_BUSY:
			return errSecAttachHandleBusy;
		case CSSMERR_CSP_NOT_LOGGED_IN:
			return errSecNotLoggedIn;
		case CSSMERR_CSP_ALGID_MISMATCH:
			return errSecAlgorithmMismatch;
		case CSSMERR_CSP_KEY_USAGE_INCORRECT:
			return errSecKeyUsageIncorrect;
		case CSSMERR_CSP_KEY_BLOB_TYPE_INCORRECT:
			return errSecKeyBlobTypeIncorrect;
		case CSSMERR_CSP_KEY_HEADER_INCONSISTENT:
			return errSecKeyHeaderInconsistent;
		case CSSMERR_CSP_UNSUPPORTED_KEY_FORMAT:
			return errSecUnsupportedKeyFormat;
		case CSSMERR_CSP_UNSUPPORTED_KEY_SIZE:
			return errSecUnsupportedKeySize;
		case CSSMERR_CSP_INVALID_KEYUSAGE_MASK:
			return errSecInvalidKeyUsageMask;
		case CSSMERR_CSP_UNSUPPORTED_KEYUSAGE_MASK:
			return errSecUnsupportedKeyUsageMask;
		case CSSMERR_CSP_INVALID_KEYATTR_MASK:
			return errSecInvalidKeyAttributeMask;
		case CSSMERR_CSP_UNSUPPORTED_KEYATTR_MASK:
			return errSecUnsupportedKeyAttributeMask;
		case CSSMERR_CSP_INVALID_KEY_LABEL:
			return errSecInvalidKeyLabel;
		case CSSMERR_CSP_UNSUPPORTED_KEY_LABEL:
			return errSecUnsupportedKeyLabel;
		case CSSMERR_CSP_INVALID_KEY_FORMAT:
			return errSecInvalidKeyFormat;
		case CSSMERR_CSP_VECTOR_OF_BUFS_UNSUPPORTED:
			return errSecUnsupportedVectorOfBuffers;
		case CSSMERR_CSP_INVALID_INPUT_VECTOR:
			return errSecInvalidInputVector;
		case CSSMERR_CSP_INVALID_OUTPUT_VECTOR:
			return errSecInvalidOutputVector;
		case CSSMERR_CSP_INVALID_CONTEXT:
			return errSecInvalidContext;
		case CSSMERR_CSP_INVALID_ALGORITHM:
			return errSecInvalidAlgorithm;
		case CSSMERR_CSP_INVALID_ATTR_KEY:
			return errSecInvalidAttributeKey;
		case CSSMERR_CSP_MISSING_ATTR_KEY:
			return errSecMissingAttributeKey;
		case CSSMERR_CSP_INVALID_ATTR_INIT_VECTOR:
			return errSecInvalidAttributeInitVector;
		case CSSMERR_CSP_MISSING_ATTR_INIT_VECTOR:
			return errSecMissingAttributeInitVector;
		case CSSMERR_CSP_INVALID_ATTR_SALT:
			return errSecInvalidAttributeSalt;
		case CSSMERR_CSP_MISSING_ATTR_SALT:
			return errSecMissingAttributeSalt;
		case CSSMERR_CSP_INVALID_ATTR_PADDING:
			return errSecInvalidAttributePadding;
		case CSSMERR_CSP_MISSING_ATTR_PADDING:
			return errSecMissingAttributePadding;
		case CSSMERR_CSP_INVALID_ATTR_RANDOM:
			return errSecInvalidAttributeRandom;
		case CSSMERR_CSP_MISSING_ATTR_RANDOM:
			return errSecMissingAttributeRandom;
		case CSSMERR_CSP_INVALID_ATTR_SEED:
			return errSecInvalidAttributeSeed;
		case CSSMERR_CSP_MISSING_ATTR_SEED:
			return errSecMissingAttributeSeed;
		case CSSMERR_CSP_INVALID_ATTR_PASSPHRASE:
			return errSecInvalidAttributePassphrase;
		case CSSMERR_CSP_MISSING_ATTR_PASSPHRASE:
			return errSecMissingAttributePassphrase;
		case CSSMERR_CSP_INVALID_ATTR_KEY_LENGTH:
			return errSecInvalidAttributeKeyLength;
		case CSSMERR_CSP_MISSING_ATTR_KEY_LENGTH:
			return errSecMissingAttributeKeyLength;
		case CSSMERR_CSP_INVALID_ATTR_BLOCK_SIZE:
			return errSecInvalidAttributeBlockSize;
		case CSSMERR_CSP_MISSING_ATTR_BLOCK_SIZE:
			return errSecMissingAttributeBlockSize;
		case CSSMERR_CSP_INVALID_ATTR_OUTPUT_SIZE:
			return errSecInvalidAttributeOutputSize;
		case CSSMERR_CSP_MISSING_ATTR_OUTPUT_SIZE:
			return errSecMissingAttributeOutputSize;
		case CSSMERR_CSP_INVALID_ATTR_ROUNDS:
			return errSecInvalidAttributeRounds;
		case CSSMERR_CSP_MISSING_ATTR_ROUNDS:
			return errSecMissingAttributeRounds;
		case CSSMERR_CSP_INVALID_ATTR_ALG_PARAMS:
			return errSecInvalidAlgorithmParms;
		case CSSMERR_CSP_MISSING_ATTR_ALG_PARAMS:
			return errSecMissingAlgorithmParms;
		case CSSMERR_CSP_INVALID_ATTR_LABEL:
			return errSecInvalidAttributeLabel;
		case CSSMERR_CSP_MISSING_ATTR_LABEL:
			return errSecMissingAttributeLabel;
		case CSSMERR_CSP_INVALID_ATTR_KEY_TYPE:
			return errSecInvalidAttributeKeyType;
		case CSSMERR_CSP_MISSING_ATTR_KEY_TYPE:
			return errSecMissingAttributeKeyType;
		case CSSMERR_CSP_INVALID_ATTR_MODE:
			return errSecInvalidAttributeMode;
		case CSSMERR_CSP_MISSING_ATTR_MODE:
			return errSecMissingAttributeMode;
		case CSSMERR_CSP_INVALID_ATTR_EFFECTIVE_BITS:
			return errSecInvalidAttributeEffectiveBits;
		case CSSMERR_CSP_MISSING_ATTR_EFFECTIVE_BITS:
			return errSecMissingAttributeEffectiveBits;
		case CSSMERR_CSP_INVALID_ATTR_START_DATE:
			return errSecInvalidAttributeStartDate;
		case CSSMERR_CSP_MISSING_ATTR_START_DATE:
			return errSecMissingAttributeStartDate;
		case CSSMERR_CSP_INVALID_ATTR_END_DATE:
			return errSecInvalidAttributeEndDate;
		case CSSMERR_CSP_MISSING_ATTR_END_DATE:
			return errSecMissingAttributeEndDate;
		case CSSMERR_CSP_INVALID_ATTR_VERSION:
			return errSecInvalidAttributeVersion;
		case CSSMERR_CSP_MISSING_ATTR_VERSION:
			return errSecMissingAttributeVersion;
		case CSSMERR_CSP_INVALID_ATTR_PRIME:
			return errSecInvalidAttributePrime;
		case CSSMERR_CSP_MISSING_ATTR_PRIME:
			return errSecMissingAttributePrime;
		case CSSMERR_CSP_INVALID_ATTR_BASE:
			return errSecInvalidAttributeBase;
		case CSSMERR_CSP_MISSING_ATTR_BASE:
			return errSecMissingAttributeBase;
		case CSSMERR_CSP_INVALID_ATTR_SUBPRIME:
			return errSecInvalidAttributeSubprime;
		case CSSMERR_CSP_MISSING_ATTR_SUBPRIME:
			return errSecMissingAttributeSubprime;
		case CSSMERR_CSP_INVALID_ATTR_ITERATION_COUNT:
			return errSecInvalidAttributeIterationCount;
		case CSSMERR_CSP_MISSING_ATTR_ITERATION_COUNT:
			return errSecMissingAttributeIterationCount;
		case CSSMERR_CSP_INVALID_ATTR_DL_DB_HANDLE:
			return errSecInvalidAttributeDLDBHandle;
		case CSSMERR_CSP_MISSING_ATTR_DL_DB_HANDLE:
			return errSecMissingAttributeDLDBHandle;
		case CSSMERR_CSP_INVALID_ATTR_ACCESS_CREDENTIALS:
			return errSecInvalidAttributeAccessCredentials;
		case CSSMERR_CSP_MISSING_ATTR_ACCESS_CREDENTIALS:
			return errSecMissingAttributeAccessCredentials;
		case CSSMERR_CSP_INVALID_ATTR_PUBLIC_KEY_FORMAT:
			return errSecInvalidAttributePublicKeyFormat;
		case CSSMERR_CSP_MISSING_ATTR_PUBLIC_KEY_FORMAT:
			return errSecMissingAttributePublicKeyFormat;
		case CSSMERR_CSP_INVALID_ATTR_PRIVATE_KEY_FORMAT:
			return errSecInvalidAttributePrivateKeyFormat;
		case CSSMERR_CSP_MISSING_ATTR_PRIVATE_KEY_FORMAT:
			return errSecMissingAttributePrivateKeyFormat;
		case CSSMERR_CSP_INVALID_ATTR_SYMMETRIC_KEY_FORMAT:
			return errSecInvalidAttributeSymmetricKeyFormat;
		case CSSMERR_CSP_MISSING_ATTR_SYMMETRIC_KEY_FORMAT:
			return errSecMissingAttributeSymmetricKeyFormat;
		case CSSMERR_CSP_INVALID_ATTR_WRAPPED_KEY_FORMAT:
			return errSecInvalidAttributeWrappedKeyFormat;
		case CSSMERR_CSP_MISSING_ATTR_WRAPPED_KEY_FORMAT:
			return errSecMissingAttributeWrappedKeyFormat;
		case CSSMERR_CSP_STAGED_OPERATION_IN_PROGRESS:
			return errSecStagedOperationInProgress;
		case CSSMERR_CSP_STAGED_OPERATION_NOT_STARTED:
			return errSecStagedOperationNotStarted;
		case CSSMERR_CSP_VERIFY_FAILED:
			return errSecVerifyFailed;
		case CSSMERR_CSP_QUERY_SIZE_UNKNOWN:
			return errSecQuerySizeUnknown;
		case CSSMERR_CSP_BLOCK_SIZE_MISMATCH:
			return errSecBlockSizeMismatch;
		case CSSMERR_CSP_PUBLIC_KEY_INCONSISTENT:
			return errSecPublicKeyInconsistent;
		case CSSMERR_CSP_DEVICE_VERIFY_FAILED:
			return errSecDeviceVerifyFailed;
		case CSSMERR_CSP_INVALID_LOGIN_NAME:
			return errSecInvalidLoginName;
		case CSSMERR_CSP_ALREADY_LOGGED_IN:
			return errSecAlreadyLoggedIn;
		case CSSMERR_CSP_INVALID_DIGEST_ALGORITHM:
			return errSecInvalidDigestAlgorithm;
		case CSSMERR_TP_INVALID_CRLGROUP:
			return errSecInvalidCRLGroup;
		case CSSMERR_TP_CERTIFICATE_CANT_OPERATE:
			return errSecCertificateCannotOperate;
		case CSSMERR_TP_CERT_EXPIRED:
			return errSecCertificateExpired;
		case CSSMERR_TP_CERT_NOT_VALID_YET:
			return errSecCertificateNotValidYet;
		case CSSMERR_TP_CERT_REVOKED:
			return errSecCertificateRevoked;
		case CSSMERR_TP_CERT_SUSPENDED:
			return errSecCertificateSuspended;
		case CSSMERR_TP_INSUFFICIENT_CREDENTIALS:
			return errSecInsufficientCredentials;
		case CSSMERR_TP_INVALID_ACTION:
			return errSecInvalidAction;
		case CSSMERR_TP_INVALID_AUTHORITY:
			return errSecInvalidAuthority;
		case CSSMERR_TP_VERIFY_ACTION_FAILED:
			return errSecVerifyActionFailed;
		case CSSMERR_TP_INVALID_CERT_AUTHORITY:
		case CSSMERR_APPLETP_INVALID_CA:
			return errSecInvalidCertAuthority;
		case CSSMERR_TP_INVALID_CRL_AUTHORITY:
			return errSecInvaldCRLAuthority;
		case CSSMERR_TP_INVALID_CRL_ENCODING:
			return errSecInvalidCRLEncoding;
		case CSSMERR_TP_INVALID_CRL_TYPE:
			return errSecInvalidCRLType;
		case CSSMERR_TP_INVALID_CRL:
			return errSecInvalidCRL;
		case CSSMERR_TP_INVALID_FORM_TYPE:
			return errSecInvalidFormType;
		case CSSMERR_TP_INVALID_ID:
			return errSecInvalidID;
		case CSSMERR_TP_INVALID_IDENTIFIER:
			return errSecInvalidIdentifier;
		case CSSMERR_TP_INVALID_INDEX:
			return errSecInvalidIndex;
		case CSSMERR_TP_INVALID_POLICY_IDENTIFIERS:
			return errSecInvalidPolicyIdentifiers;
		case CSSMERR_TP_INVALID_TIMESTRING:
			return errSecInvalidTimeString;
		case CSSMERR_TP_INVALID_REASON:
			return errSecInvalidReason;
		case CSSMERR_TP_INVALID_REQUEST_INPUTS:
			return errSecInvalidRequestInputs;
		case CSSMERR_TP_INVALID_RESPONSE_VECTOR:
			return errSecInvalidResponseVector;
		case CSSMERR_TP_INVALID_STOP_ON_POLICY:
			return errSecInvalidStopOnPolicy;
		case CSSMERR_TP_INVALID_TUPLE:
			return errSecInvalidTuple;
		case CSSMERR_TP_NOT_SIGNER:
			return errSecNotSigner;
		case CSSMERR_TP_NOT_TRUSTED:
			return errSecNotTrusted;
		case CSSMERR_TP_NO_DEFAULT_AUTHORITY:
			return errSecNoDefaultAuthority;
		case CSSMERR_TP_REJECTED_FORM:
			return errSecRejectedForm;
		case CSSMERR_TP_REQUEST_LOST:
			return errSecRequestLost;
		case CSSMERR_TP_REQUEST_REJECTED:
			return errSecRequestRejected;
		case CSSMERR_TP_UNSUPPORTED_ADDR_TYPE:
			return errSecUnsupportedAddressType;
		case CSSMERR_TP_UNSUPPORTED_SERVICE:
			return errSecUnsupportedService;
		case CSSMERR_TP_INVALID_TUPLEGROUP:
			return errSecInvalidTupleGroup;
		case CSSMERR_AC_INVALID_BASE_ACLS:
			return errSecInvalidBaseACLs;
		case CSSMERR_AC_INVALID_TUPLE_CREDENTIALS:
			return errSecInvalidTupleCredendtials;
		case CSSMERR_AC_INVALID_ENCODING:
			return errSecInvalidEncoding;
		case CSSMERR_AC_INVALID_VALIDITY_PERIOD:
			return errSecInvalidValidityPeriod;
		case CSSMERR_AC_INVALID_REQUESTOR:
			return errSecInvalidRequestor;
		case CSSMERR_AC_INVALID_REQUEST_DESCRIPTOR:
			return errSecRequestDescriptor;
		case CSSMERR_CL_INVALID_BUNDLE_INFO:
			return errSecInvalidBundleInfo;
		case CSSMERR_CL_INVALID_CRL_INDEX:
			return errSecInvalidCRLIndex;
		case CSSMERR_CL_NO_FIELD_VALUES:
			return errSecNoFieldValues;
		case CSSMERR_DL_UNSUPPORTED_FIELD_FORMAT:
			return errSecUnsupportedFieldFormat;
		case CSSMERR_DL_UNSUPPORTED_INDEX_INFO:
			return errSecUnsupportedIndexInfo;
		case CSSMERR_DL_UNSUPPORTED_LOCALITY:
			return errSecUnsupportedLocality;
		case CSSMERR_DL_UNSUPPORTED_NUM_ATTRIBUTES:
			return errSecUnsupportedNumAttributes;
		case CSSMERR_DL_UNSUPPORTED_NUM_INDEXES:
			return errSecUnsupportedNumIndexes;
		case CSSMERR_DL_UNSUPPORTED_NUM_RECORDTYPES:
			return errSecUnsupportedNumRecordTypes;
		case CSSMERR_DL_FIELD_SPECIFIED_MULTIPLE:
			return errSecFieldSpecifiedMultiple;
		case CSSMERR_DL_INCOMPATIBLE_FIELD_FORMAT:
			return errSecIncompatibleFieldFormat;
		case CSSMERR_DL_INVALID_PARSING_MODULE:
			return errSecInvalidParsingModule;
		case CSSMERR_DL_DB_LOCKED:
			return errSecDatabaseLocked;
		case CSSMERR_DL_DATASTORE_IS_OPEN:
			return errSecDatastoreIsOpen;
		case CSSMERR_DL_MISSING_VALUE:
			return errSecMissingValue;
		case CSSMERR_DL_UNSUPPORTED_QUERY_LIMITS:
			return errSecUnsupportedQueryLimits;
		case CSSMERR_DL_UNSUPPORTED_NUM_SELECTION_PREDS:
			return errSecUnsupportedNumSelectionPreds;
		case CSSMERR_DL_UNSUPPORTED_OPERATOR:
			return errSecUnsupportedOperator;
		case CSSMERR_DL_INVALID_DB_LOCATION:
			return errSecInvalidDBLocation;
		case CSSMERR_DL_INVALID_ACCESS_REQUEST:
			return errSecInvalidAccessRequest;
		case CSSMERR_DL_INVALID_INDEX_INFO:
			return errSecInvalidIndexInfo;
		case CSSMERR_DL_INVALID_NEW_OWNER:
			return errSecInvalidNewOwner;
		case CSSMERR_DL_INVALID_MODIFY_MODE:
			return errSecInvalidModifyMode;
		case CSSMERR_DL_RECORD_MODIFIED:
			return errSecRecordModified;
		case CSSMERR_DL_ENDOFDATA:
			return errSecEndOfData;
		case CSSMERR_DL_INVALID_VALUE:
			return errSecInvalidValue;
		case CSSMERR_DL_MULTIPLE_VALUES_UNSUPPORTED:
			return errSecMultipleValuesUnsupported;
		default:
			return osStatus;
	}
}
