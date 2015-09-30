/*
 * Copyright (c) 2006,2011-2014 Apple Inc. All Rights Reserved.
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
 * CMSUtils.h - common utility routines for libCMS.
 */
 
#ifndef	_CMS_UTILS_H_
#define _CMS_UTILS_H_

#include <Security/cssmtype.h>
#include <Security/cssmapple.h>		/* cssmPerror() */
#include <CoreFoundation/CoreFoundation.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Copy a CSSM_DATA, mallocing the result.
 */
void cmsCopyCmsData(
	const CSSM_DATA *src,
	CSSM_DATA *dst);
	
/* 
 * Append a CF type, or the contents of an array, to another array.
 * destination array will be created if necessary.
 * If srcItemOrArray is not of the type specified in expectedType,
 * errSecParam will be returned. 
 */
OSStatus cmsAppendToArray(
	CFTypeRef srcItemOrArray,
	CFMutableArrayRef *dstArray,
	CFTypeID expectedType);

/* 
 * Munge an OSStatus returned from libsecurity_smime, which may well be an ASN.1 private
 * error code, to a real OSStatus.
 */
OSStatus cmsRtnToOSStatus(
	OSStatus smimeRtn,			// from libsecurity_smime
	OSStatus defaultRtn = 0);	// use this if we can't map smimeRtn

#define CFRELEASE(cfr)	if(cfr != NULL) { CFRelease(cfr); }

#define CMS_DEBUG 0
#if	CMS_DEBUG
#define ASSERT(s)			assert(s)
#define CSSM_PERROR(s, r)	cssmPerror(s, r)
#define dprintf(args...)	printf(args)
#else
#define ASSERT(s)
#define CSSM_PERROR(s, r)
#define dprintf(args...)
#endif

#ifdef __cplusplus
}
#endif

#endif	/* _CMS_UTILS_H_ */

