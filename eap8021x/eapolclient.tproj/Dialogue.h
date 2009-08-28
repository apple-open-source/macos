
/*
 * Copyright (c) 2001-2008 Apple Inc. All rights reserved.
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
 * Modification History
 *
 * November 8, 2001	Dieter Siegmund
 * - created
 */
 
#ifndef _S_DIALOGUE_H
#define _S_DIALOGUE_H

#include <sys/types.h>
#include <CoreFoundation/CFString.h>

typedef struct {
    CFStringRef		username;
    CFStringRef		password;
    Boolean		user_cancelled;
} UserPasswordDialogueResponse, *UserPasswordDialogueResponseRef;

typedef void 
(*UserPasswordDialogueResponseCallBack)(const void * arg1, 
					const void * arg2, 
					UserPasswordDialogueResponseRef data);

typedef struct UserPasswordDialogue_s UserPasswordDialogue, 
    *UserPasswordDialogueRef;

UserPasswordDialogueRef
UserPasswordDialogue_create(UserPasswordDialogueResponseCallBack func,
			    const void * arg1, const void * arg2, 
			    CFStringRef icon,
			    CFStringRef title, CFStringRef message, 
			    CFStringRef username, CFStringRef password);

void
UserPasswordDialogue_free(UserPasswordDialogueRef * dialogue_p_p);

typedef struct {
    Boolean		proceed;
} TrustDialogueResponse, *TrustDialogueResponseRef;

typedef void 
(*TrustDialogueResponseCallBack)(const void * arg1, 
				 const void * arg2, 
				 TrustDialogueResponseRef data);

typedef struct TrustDialogue_s TrustDialogue, *TrustDialogueRef;

TrustDialogueRef
TrustDialogue_create(TrustDialogueResponseCallBack func,
		     const void * arg1, const void * arg2,
		     CFDictionaryRef trust_info, 
		     CFStringRef icon, CFStringRef title);

CFDictionaryRef
TrustDialogue_trust_info(TrustDialogueRef dialogue);

void
TrustDialogue_free(TrustDialogueRef * dialogue_p_p);

#endif _S_DIALOGUE_H
