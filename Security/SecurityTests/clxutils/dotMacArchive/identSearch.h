/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
 * identSearch.h - search for identity whose cert has specified email address
 */
 
#ifndef	_IDENT_SEARCH_H_
#define _IDENT_SEARCH_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <Security/Security.h>

/*
 * Find an identity whose cert has specified email address. 
 * Returns errSecItemNotFound if no matching identity found, noErr if 
 * we found one. 
 */
OSStatus findIdentity(
	const void			*emailAddress,		// UTF8 encoded email address
	unsigned			emailAddressLen,
	SecKeychainRef		kcRef,				// keychain to search, or NULL to search all
	SecIdentityRef		*idRef);			// RETURNED
	
#ifdef __cplusplus
}
#endif

#endif	/* _IDENT_SEARCH_H_ */

