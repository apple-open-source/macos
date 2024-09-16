/*
 * Copyright (c) 2000-2004,2011,2013-2016,2021 Apple Inc. All Rights Reserved.
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

#ifndef _SECURITY_SECBRIDGE_H_
#define _SECURITY_SECBRIDGE_H_

#include <security_keychain/Globals.h>
#include <security_keychain/SecCFTypes.h>
#include <Security/SecBasePriv.h>
#include <Security/SecKeychainPriv.h>
#include <security_keychain/KCUtilities.h>
#include <security_cdsa_utilities/cssmbridge.h>
#include "LegacyAPICounts.h"

using namespace KeychainCore;

#define COUNTLEGACYAPI	static dispatch_once_t countToken; \
	countLegacyAPI(&countToken, __FUNCTION__);

//
// API boilerplate macros. These provide a frame for C++ code that is impermeable to exceptions.
// Usage:
//	BEGIN_API
//		... your C++ code here ...
//  END_API		// returns CSSM_RETURN on exception
//	END_API0	// returns nothing (void) on exception
//	END_API1(bad) // return (bad) on exception
//	END_API2(name) // like END_API, with API name as debug scope for printing function result
//	END_API3(name, bad) // like END_API1, with API name as debug scope for printing function result
//
#define BEGIN_SECAPI \
	OSStatus __secapiresult = errSecSuccess; \
	bool __countlegacyapi = countLegacyAPIEnabledForThread(); \
	static dispatch_once_t countToken; \
	countLegacyAPI(&countToken, __FUNCTION__); \
	setCountLegacyAPIEnabledForThread(false); \
	try {
#define END_SECAPI }\
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); } \
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); } \
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; } \
	catch (...) { __secapiresult=errSecInternalComponent; } \
	setCountLegacyAPIEnabledForThread(__countlegacyapi); \
	return __secapiresult;
#define END_SECAPI1(BAD_RETURN_VAL) }\
	catch (...) { __secapiresult=BAD_RETURN_VAL; } \
	setCountLegacyAPIEnabledForThread(__countlegacyapi); \
	return __secapiresult;
#define END_SECAPI0 }\
	catch (...) { setCountLegacyAPIEnabledForThread(__countlegacyapi); return; }


//
// BEGIN_SECKCITEMAPI
// Note: this macro assumes an input parameter named "itemRef"
//
#define BEGIN_SECKCITEMAPI \
	OSStatus __secapiresult=errSecSuccess; \
	bool __countlegacyapi = countLegacyAPIEnabledForThread(); \
	static dispatch_once_t countToken; \
	countLegacyAPI(&countToken, __FUNCTION__); \
	setCountLegacyAPIEnabledForThread(false); \
	SecKeychainItemRef __itemImplRef=NULL; \
	bool __is_certificate=(itemRef && (CFGetTypeID(itemRef) == SecCertificateGetTypeID())); \
	if (__is_certificate) { \
		if (SecCertificateIsItemImplInstance((SecCertificateRef)itemRef)) { \
			__itemImplRef=(SecKeychainItemRef)CFRetain(itemRef); \
		} else { \
			__itemImplRef=(SecKeychainItemRef)SecCertificateCopyKeychainItem((SecCertificateRef)itemRef); \
			if (!__itemImplRef) { \
				__itemImplRef=(SecKeychainItemRef)SecCertificateCreateItemImplInstance((SecCertificateRef)itemRef); \
				(void)SecCertificateSetKeychainItem((SecCertificateRef)itemRef,__itemImplRef); \
			} \
		} \
	} else { \
		bool __is_cdsa_key_item=(itemRef && SecKeyIsLegacyInstance((SecKeyRef)itemRef)); \
		__itemImplRef=(SecKeychainItemRef)((__is_cdsa_key_item) ? CFRetain(itemRef) : NULL); \
	} \
	try {

//
// END_SECKCITEMAPI
//
#define END_SECKCITEMAPI } \
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); } \
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); } \
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; } \
	catch (...) { __secapiresult=errSecInternalComponent; } \
	if (__itemImplRef) { CFRelease(__itemImplRef); } \
	setCountLegacyAPIEnabledForThread(__countlegacyapi); \
	return __secapiresult;


//
// BEGIN_SECCERTAPI
// Note: this macro assumes an input parameter named "certificate"
//
#define BEGIN_SECCERTAPI \
	OSStatus __secapiresult=errSecSuccess; \
	bool __countlegacyapi = countLegacyAPIEnabledForThread(); \
	static dispatch_once_t countToken; \
	countLegacyAPI(&countToken, __FUNCTION__); \
	setCountLegacyAPIEnabledForThread(false); \
	SecCertificateRef __itemImplRef=NULL; \
	if (SecCertificateIsItemImplInstance(certificate)) { __itemImplRef=(SecCertificateRef)CFRetain(certificate); } \
	if (!__itemImplRef && certificate) { __itemImplRef=(SecCertificateRef)SecCertificateCopyKeychainItem(certificate); } \
	if (!__itemImplRef && certificate) { __itemImplRef=SecCertificateCreateItemImplInstance(certificate); \
		(void)SecCertificateSetKeychainItem(certificate,__itemImplRef); } \
	try {

//
// END_SECCERTAPI
//
#define END_SECCERTAPI } \
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); } \
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); } \
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; } \
	catch (...) { __secapiresult=errSecInternalComponent; } \
	if (__itemImplRef) { CFRelease(__itemImplRef); } \
	setCountLegacyAPIEnabledForThread(__countlegacyapi); \
	return __secapiresult;


//
// BEGIN_SECKEYAPI
//
#define BEGIN_SECKEYAPI(resultType, resultInit) \
	resultType result = resultInit; \
	bool __countlegacyapi = countLegacyAPIEnabledForThread(); \
	static dispatch_once_t countToken; \
	countLegacyAPI(&countToken, __FUNCTION__); \
	setCountLegacyAPIEnabledForThread(false); \
	try {

//
// END_SECKEYAPI
//
extern "C" bool SecError(OSStatus status, CFErrorRef *error, CFStringRef format, ...);

#define END_SECKEYAPI }\
	catch (const MacOSError &err) { SecError(err.osStatus(), error, CFSTR("%s"), err.what()); result = NULL; } \
	catch (const CommonError &err) { \
		if (err.osStatus() != CSSMERR_CSP_INVALID_DIGEST_ALGORITHM) { \
			OSStatus status = SecKeychainErrFromOSStatus(err.osStatus()); \
			if (status == errSecInputLengthError) status = errSecParam; \
			SecError(status, error, CFSTR("%s"), err.what()); result = NULL; \
		} \
	} \
	catch (const std::bad_alloc &) { SecError(errSecAllocate, error, CFSTR("allocation failed")); result = NULL; } \
	catch (...) { SecError(errSecInternalComponent, error, CFSTR("internal error")); result = NULL; } \
	setCountLegacyAPIEnabledForThread(__countlegacyapi); \
	return result;


//
// BEGIN_SECPOLICYAPI
// Note: this does not use try/catch in order to allow early returns,
// for use with API implementations that only call other 'C' APIs.
//
#define BEGIN_SECPOLICYAPI \
	bool __countlegacyapi = countLegacyAPIEnabledForThread(); \
	static dispatch_once_t countToken; \
	countLegacyAPI(&countToken, __FUNCTION__); \
	setCountLegacyAPIEnabledForThread(false);

//
// END_SECPOLICYAPI
//
#define END_SECPOLICYAPI(RETURN_VAL) \
	setCountLegacyAPIEnabledForThread(__countlegacyapi); \
	return RETURN_VAL;


//
// BEGIN_SECTRUSTAPI
// Note: this does not use try/catch in order to allow early returns,
// for use with API implementations that only call other 'C' APIs.
//
#define BEGIN_SECTRUSTAPI \
	bool __countlegacyapi = countLegacyAPIEnabledForThread(); \
	static dispatch_once_t countToken; \
	countLegacyAPI(&countToken, __FUNCTION__); \
	setCountLegacyAPIEnabledForThread(false);

//
// END_SECTRUSTAPI
//
#define END_SECTRUSTAPI(RETURN_VAL) \
	setCountLegacyAPIEnabledForThread(__countlegacyapi); \
	return RETURN_VAL;


#endif /* !_SECURITY_SECBRIDGE_H_ */
