/*
 * Copyright (c) 2006,2011-2012,2014 Apple Inc. All Rights Reserved.
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

/*!
	@header SecStaticCode
	SecStaticCodePriv is the private counter-part to SecStaticCode. Its contents are not
	official API, and are subject to change without notice.
*/
#ifndef _H_SECSTATICCODEPRIV
#define _H_SECSTATICCODEPRIV

#include <Security/SecStaticCode.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 Private SecStaticCodeCreate* SecCS flags.
 */
CF_ENUM(uint32_t) {
	kSecCSForceOnlineNotarizationCheck = 1 << 0,
};


/*
	@function SecCodeSetCallback
	For a given Code or StaticCode object, specify a block that is invoked at certain
	stages of a validation operation. The block is only invoked for validations of this
	particular object. Note that validation outcomes are cached in the API object, and
	repeated validations will not generally result in the same set of callbacks.
	Only one callback can be active for each API object. A new call to SecCodeSetCallback
	replaces the previous callback.
	
	@param code A Code or StaticCode object whose validation should be monitored.
	@param flags Optional flags. Pass kSecCSDefaultFlags for standard behavior.
	@param old A pointer to a block pointer that receives any previously registered callback.
		Pass NULL if you are not interested in any previous value.
	@param callback A block to be synchronously invoked at certain stages of API operation.
		Pass NULL to disable callbacks for this code object. The block must be available to
		be invoked, possibly repeatedly, for as long as the code object exists or it is superseded
		by another call to this API, whichever happens earlier.
		From your block, return NULL to continue normal operation. Return a CFTypeRef object of
		suitable value for the reported stage to intervene.
 */
OSStatus SecStaticCodeSetCallback(SecStaticCodeRef code, SecCSFlags flag, SecCodeCallback *olds, SecCodeCallback callback);

	
/*
 	@function SecStaticCodeSetValidationConditions
 	Set various parameters that modify the evaluation of a signature.
 	This is an internal affordance used by Gatekeeper to implement checkfix evaluation.
 	It is not meant to be a generally useful mechanism.
 
 	@param code A Code or StaticCode object whose validation should be modified.
 	@param conditions A dictionary containing one or more validation conditions. Must not be NULL.
 */
OSStatus SecStaticCodeSetValidationConditions(SecStaticCodeRef code, CFDictionaryRef conditions);
	
	
/*
	@function SecStaticCodeCancelValidation
 	Ask for an ongoing static validation using this (static) code object to be canceled as soon as feasible.
 	if no validation is pending, this does nothing.
 	Since validation is synchronous, this call must be made from another thread.
 	This call will return immediately. If a validation operation is terminated due to it,
 	it will fail with the errSecCSVetoed error.
 
	@param code A Code or StaticCode object whose validation should be modified.
	@param flags Optional flags. Pass kSecCSDefaultFlags for standard behavior.
 */
OSStatus SecStaticCodeCancelValidation(SecStaticCodeRef code, SecCSFlags flags);

/*
 @function SecStaticCodeEnableOnlineNotarizationCheck
 Sets a flag on the object to allow an online notarization check once for the lifetime of the object during
 the next validation.

 @param code A StaticCode object whose validation should be modified.
 @param enable Whether to enable or disable online notarization checks.
 */
OSStatus SecStaticCodeEnableOnlineNotarizationCheck(SecStaticCodeRef code, Boolean enable) __SPI_AVAILABLE(macos(11.3));

/*
    @function SecStaticCodeValidateResource
    For a SecStaticCodeRef, check that the resource at the provided path is part of the signature and unaltered.
    This call will fail if the file is not in the bundle, missing from the bundle, optional, or signed
    into the bundle in a way that it cannot be fully verified.

    @param code A SecStaticCode object for the outer bundle.
    @param resourcePath A CFStringRef containing the absolute path to a sealed resource file.
        This path will be checked and must be to something within the code object's base path.
    @param flags Flags to use during validation, see SecStaticCodeCheckValidity
    @param errors An optional pointer to a CFErrorRef variable. If the call fails
        (something other than errSecSuccess is returned), and this argument is non-NULL,
        a CFErrorRef is stored there further describing the nature and circumstances
        of the failure. The caller must CFRelease() this error object when done with it.

    @result noErr if the file at resourcePath validates as a resource of the bundle represented by code. Can return a
        variety of errors from CSCommon.h or other Security framework headers, but notable errors are:

        errSecParam if the resource is not within the code object.
        errSecCSResourcesNotFound if the resources in the bundle could not be loaded.
        errSecCSResourcesNotSealed if the requested resource was found but cannot be verified.
        errSecCSBadResource if the resource couldn't be found or was altered.
        errSecCSSignatureFailed if the executable resource was found and was altered.
 */
OSStatus SecStaticCodeValidateResourceWithErrors(SecStaticCodeRef code, CFURLRef resourcePath, SecCSFlags flags, CFErrorRef *errors) __SPI_AVAILABLE(macos(11.3));

#ifdef __cplusplus
}
#endif

#endif //_H_SECSTATICCODEPRIV
