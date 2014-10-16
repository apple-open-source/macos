/*
 * Copyright (c) 2010-2011 Apple Inc. All Rights Reserved.
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
 @header EncryptTransformUtilities
 This file contains utilities used by the SecEncryptTransform and the SecDecryptTransform
 
 */
#if !defined(___ENCRYPT_TRANSFORM_UTILITIES__)
#define ___ENCRYPT_TRANSFORM_UTILITIES__ 1

#include <CoreFoundation/CoreFoundation.h> 
#include <Security/cssmapi.h>
#include <Security/cssmapple.h>
#include <Security/cssmtype.h>

#ifdef __cplusplus
extern "C" {
#endif
	
	/*!
	 @function 			ConvertPaddingStringToEnum
	 @abstract			Given a string that represents a padding return the
	 CSSM_PADDING value
	 @param paddingStr	A CFStringRef that represents a padding string
	 @result				The corresponding CSSM_PADDING value or -1 if the
	 padding value could not be found
	 */
	uint32	ConvertPaddingStringToEnum(CFStringRef paddingStr);
	
	/*!
	 @function 			ConvertPaddingEnumToString
	 @abstract			Given a CSSM_PADDING value return the corresponding 
	 CFString representation.
	 @param paddingEnum	A CSSM_PADDING value.
	 @result				The corresponding CFStringRef or NULL if the the 
	 CSSM_PADDING value could not be found
	 */
	CFStringRef ConvertPaddingEnumToString(CSSM_PADDING paddingEnum);
	
	
	/*!
	 @function 			ConvertEncryptModeStringToEnum
	 @abstract			Given a string that represents an encryption mode return the
	 CSSM_ENCRYPT_MODE value
	 @param modeStr	A CFStringRef that represents an encryption mode
     @param hasPadding Specify if the mode should pad
		 @result				The corresponding CSSM_ENCRYPT_MODE value or -1 if the
	 encryptio mode value could not be found
	 */
	uint32	ConvertEncryptModeStringToEnum(CFStringRef modeStr, Boolean hasPadding);
	
	/*!
	 @function 			ConvertPaddingEnumToString
	 @abstract			Given a CSSM_ENCRYPT_MODE value return the corresponding 
	 CFString representation.
	 @param paddingEnum	A CSSM_ENCRYPT_MODE value.
	 @result				The corresponding CFStringRef or NULL if the the 
	 CSSM_ENCRYPT_MODE value could not be found
	 */
	CFStringRef ConvertEncryptModeEnumToString(CSSM_ENCRYPT_MODE paddingEnum);
	
#ifdef __cplusplus
}
#endif

#endif /* !___ENCRYPT_TRANSFORM_UTILITIES__ */
