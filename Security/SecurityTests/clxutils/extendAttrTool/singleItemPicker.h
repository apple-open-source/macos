/*
 * Copyright (c) 2004,2006 Apple Computer, Inc. All Rights Reserved.
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
 * singleItemPicker.h - select a key or cert from a keychain
 */
 
#ifndef	_SINGLE_ITEM_PICKER_H_
#define _SINGLE_ITEM_PICKER_H_

#include <Security/Security.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	KPI_PrivateKey,
	KPI_PublicKey,
	KPI_Cert
} KP_ItemType;

OSStatus singleItemPicker(
	SecKeychainRef		kcRef,		// NULL means the default list
	KP_ItemType			itemType,		
	bool				takeFirst,	// take first key found
	SecKeychainItemRef	*keyRef);	// RETURNED


#ifdef __cplusplus
}
#endif

#endif	/* _SINGLE_ITEM_PICKER_H_ */

