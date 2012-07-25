/*
 * Copyright © 2010 Apple Inc. All Rights Reserved.
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

#ifndef _SEC_TRANSFORMVALIDATOR_H__
#define _SEC_TRANSFORMVALIDATOR_H__

CF_EXTERN_C_BEGIN


/*!
 @function			SecTransformCreateValidatorForCFtype
 
 @abstract			Create a validator that triggers an error when presented a CF type other then expected_type,
 or a NULL value if null_allowed is NO.
 
 @result				A SecTransformAttributeActionBlock suitable for passing to SecTransformSetAttributeAction
 with an actyion type of kSecTransformActionAttributeValidation.
 
 @discussion			If the validator is passed an incorrect CF type it will return a CFError including the
 type it was given, the value it was given, the type it expected, and if a NULL value is acceptable as well as
 what attribute the value was sent to.
 */
CF_EXPORT 
SecTransformAttributeActionBlock SecTransformCreateValidatorForCFtype(CFTypeID expected_type, Boolean null_allowed);

CF_EXTERN_C_END

#endif