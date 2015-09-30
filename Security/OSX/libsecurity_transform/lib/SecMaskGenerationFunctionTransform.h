/*
 * Copyright (c) 2011 Apple Inc. All Rights Reserved.
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

#ifndef libsecurity_transform_SecMaskGenerationFunctionTransform_h
#define libsecurity_transform_SecMaskGenerationFunctionTransform_h

#include "SecTransform.h"

#ifdef __cplusplus
extern "C" {
#endif
    
/*!
 @function SecMaskGenerationFunctionTransformCreate
 @abstract			Creates a MGF computation object.
 @param maskGenerationFunctionType  The MGF algorigh to use, currently only kSecMaskGenerationFunctionMGF1
 @param digestType	The type of digest to compute the MGF with.  You may pass NULL
 for this parameter, in which case an appropriate
 algorithm will be chosen for you (SHA1 for MGF1).
 @param digestLength	The desired digest length.  Note that certain
 algorithms may only support certain sizes. You may
 pass 0 for this parameter, in which case an
 appropriate length will be chosen for you.
 @param maskLength	The desired mask length.
 @param error		A pointer to a CFErrorRef.  This pointer will be set
 if an error occurred.  This value may be NULL if you
 do not want an error returned.
 @result				A pointer to a SecTransformRef object.  This object must
 be released with CFRelease when you are done with
 it.  This function will return NULL if an error
 occurred.
 @discussion			This function creates a transform which computes a
 fixed length (maskLength) deterministic pseudorandom output.
 */
    
    
SecTransformRef SecCreateMaskGenerationFunctionTransform(CFStringRef hashType, int length, CFErrorRef *error)
	/* __OSX_AVAILABLE_STARTING(__MAC_10_8,__IPHONE_NA) */;
    
#ifdef __cplusplus
}
#endif

#endif
