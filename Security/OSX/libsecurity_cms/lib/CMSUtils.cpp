/*
 * Copyright (c) 2006,2011,2013-2014 Apple Inc. All Rights Reserved.
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
 * CMSUtils.cpp - common utility routines for libCMS.
 */

#include "CMSUtils.h"
#include <stdlib.h>
#include <string.h>
#include <security_asn1/secerr.h>
#include <security_asn1/seccomon.h>
#include <Security/SecBase.h>

/*
 * Copy a CSSM_DATA, mallocing the result.
 */
void cmsCopyCmsData(
	const CSSM_DATA *src,
	CSSM_DATA *dst)
{
	dst->Data = (uint8 *)malloc(src->Length);
	memmove(dst->Data, src->Data, src->Length);
	dst->Length = src->Length;
}

/* 
 * Append a CF type, or the contents of an array, to another array.
 * destination array will be created if necessary.
 * If srcItemOrArray is not of the type specified in expectedType,
 * errSecParam will be returned. 
 */
OSStatus cmsAppendToArray(
	CFTypeRef srcItemOrArray,
	CFMutableArrayRef *dstArray,
	CFTypeID expectedType)	
{
	if(srcItemOrArray == NULL) {
		return errSecSuccess;
	}
	if(*dstArray == NULL) {
		*dstArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}
	CFTypeID inType = CFGetTypeID(srcItemOrArray);
	if(inType == CFArrayGetTypeID()) {
		CFArrayRef srcArray = (CFArrayRef)srcItemOrArray;
		CFRange srcRange = {0, CFArrayGetCount(srcArray)};
		CFArrayAppendArray(*dstArray, srcArray, srcRange);
	}
	else if(inType == expectedType) {
		CFArrayAppendValue(*dstArray, srcItemOrArray);
	}
	else {
		return errSecParam;
	}
	return errSecSuccess;
}

/* 
 * Munge an OSStatus returned from libsecurity_smime, which may well be an ASN.1 private
 * error code, to a real OSStatus.
 */
OSStatus cmsRtnToOSStatus(
	OSStatus smimeRtn,		// from libsecurity_smime
	OSStatus defaultRtn)	// use this if we can't map smimeRtn
{
	if(smimeRtn == SECFailure) {
		/* This is a SECStatus. Try to get detailed error info. */
		smimeRtn = PORT_GetError();
		if(smimeRtn == 0) {
			/* S/MIME just gave us generic error; no further info available; punt. */
			dprintf("cmsRtnToOSStatus: SECFailure, no status avilable\n");
			return defaultRtn ? defaultRtn : errSecInternalComponent;
		}
		/* else proceed to map smimeRtn to OSStatus */
	}
	if(!IS_SEC_ERROR(smimeRtn)) {
		/* isn't ASN.1 or S/MIME error; use as is. */
		return smimeRtn;
	}
	
	/* Convert SECErrorCodes to OSStatus */
	switch(smimeRtn) {
		case SEC_ERROR_BAD_DER:
		case SEC_ERROR_BAD_DATA:
			return errSecUnknownFormat;
		case SEC_ERROR_NO_MEMORY:
			return errSecAllocate;
		case SEC_ERROR_IO:
			return errSecIO;
		case SEC_ERROR_OUTPUT_LEN:
		case SEC_ERROR_INPUT_LEN:
		case SEC_ERROR_INVALID_ARGS:
		case SEC_ERROR_INVALID_ALGORITHM:
		case SEC_ERROR_INVALID_AVA:
		case SEC_ERROR_INVALID_TIME:
			return errSecParam;
		case SEC_ERROR_PKCS7_BAD_SIGNATURE:
		case SEC_ERROR_BAD_SIGNATURE:
			return CSSMERR_CSP_VERIFY_FAILED;
		case SEC_ERROR_EXPIRED_CERTIFICATE:
		case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
			return CSSMERR_TP_CERT_EXPIRED;
		case SEC_ERROR_REVOKED_CERTIFICATE:
			return CSSMERR_TP_CERT_REVOKED;
		case SEC_ERROR_UNKNOWN_ISSUER:
		case SEC_ERROR_UNTRUSTED_ISSUER:
		case SEC_ERROR_UNTRUSTED_CERT:
			return CSSMERR_TP_NOT_TRUSTED;
		case SEC_ERROR_CERT_USAGES_INVALID:
		case SEC_ERROR_INADEQUATE_KEY_USAGE:
			return CSSMERR_CSP_KEY_USAGE_INCORRECT;
		case SEC_INTERNAL_ONLY:
			return errSecInternalComponent;
		case SEC_ERROR_NO_USER_INTERACTION:
			return errSecInteractionNotAllowed;
		case SEC_ERROR_USER_CANCELLED:
			return errSecUserCanceled;
		default:
			dprintf("cmsRtnToOSStatus: smimeRtn 0x%x\n", smimeRtn);
			return defaultRtn ? defaultRtn : errSecInternalComponent;
	}
}
