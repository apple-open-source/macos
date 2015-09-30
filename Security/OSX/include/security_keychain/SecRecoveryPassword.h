/*
 * Copyright (c) 2010-2012 Apple Inc. All Rights Reserved.
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
 * generalize to Sec function
 */

#ifndef SEC_RECOVERYPASSWORD_H
#define SEC_RECOVERYPASSWORD_H

#include <stdint.h>
#include <stddef.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Availability.h>

#ifdef __cplusplus
extern "C" {
#endif

extern CFStringRef kSecRecVersionNumber,
				   kSecRecQuestions,
				   kSecRecLocale,
				   kSecRecIV,
				   kSecRecWrappedPassword;

/*!
     @function	SecWrapRecoveryPasswordWithAnswers
     @abstract	Wrap a password with a key derived from an array of answers to questions 
     
     @param		password	The password to wrap.
     
     @param		questions	An array containing the questions corresponding to the answers.
     
     @param		answers		An array of CFStringRefs for each of the answers.
      
     @result	A CFDictionary with values for each of the keys defined for a recovery reference:
     			
                kSecRecVersionNumber  - the version of recovery reference
                kSecRecQuestions	  - the questions
                kSecRecLocale		  - the locale of the system used to generate the answers
                kSecRecIV			  - the IV for the password wrapping (base64)
                kSecRecWrappedPassword - the wrapped password bytes (base64)
 */
    
CFDictionaryRef
SecWrapRecoveryPasswordWithAnswers(CFStringRef password, CFArrayRef questions, CFArrayRef answers)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA); 

/*!
     @function	SecUnwrapRecoveryPasswordWithAnswers
     @abstract	Unwrap a password with a key derived from an array of answers to questions 
     
     @param		recref	    A CFDictionary containing the recovery reference as defined above.
          
     @param		answers		An array of CFStringRefs for each of the answers.
     
     @result	The unwrapped password
     
*/

CFStringRef
SecUnwrapRecoveryPasswordWithAnswers(CFDictionaryRef recref, CFArrayRef answers)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA); 

/*!
     @function	SecCreateRecoveryPassword
     @abstract	This function creates a random password of the form:
     			T2YG-WEGQ-WVFX-A37A-I3OM-NQKV 
     
     @result	The password
     
*/
    
CFStringRef
SecCreateRecoveryPassword(void)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA); 




#ifdef __cplusplus
}
#endif
#endif /* SEC_RECOVERYPASSWORD_H */


