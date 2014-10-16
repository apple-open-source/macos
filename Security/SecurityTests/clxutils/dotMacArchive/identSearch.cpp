/*
 * Copyright (c) 2004-2005 Apple Computer, Inc. All Rights Reserved.
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
 * identSearch.cpp - search for identity whose cert has specified email address
 */

#include "identSearch.h"
#include <Security/SecKeychainItemPriv.h>		/* for kSecAlias */

/* 
 * Does the specified identity's cert have the specified email address? Returns
 * true if so.
 */
bool idHasEmail(
	SecIdentityRef idRef,
	const void		*emailAddress,		// UTF8 encoded email address
	unsigned		emailAddressLen)
{
	SecCertificateRef certRef;
	OSStatus ortn;
	bool ourRtn = false;
	
	ortn = SecIdentityCopyCertificate(idRef, &certRef);
	if(ortn) {
		/* should never happen */
		cssmPerror("SecIdentityCopyCertificate", ortn);
		return ortn;
	}
	
	/* 
	 * Fetch one attribute - the alias (which is always the "best attempt" at 
	 * finding an email address within a cert).
	 */
	UInt32 oneTag = kSecAlias;
	SecKeychainAttributeInfo attrInfo;
	attrInfo.count = 1;
	attrInfo.tag = &oneTag;
	attrInfo.format = NULL;
	SecKeychainAttributeList *attrList = NULL;
	SecKeychainAttribute *attr = NULL;
	
	ortn = SecKeychainItemCopyAttributesAndData((SecKeychainItemRef)certRef, 
		&attrInfo,
		NULL,			// itemClass
		&attrList, 
		NULL,			// length - don't need the data
		NULL);			// outData
	if(ortn || (attrList == NULL) || (attrList->count != 1)) {
		/* I don't *think* this should ever happen... */
		cssmPerror("SecKeychainItemCopyAttributesAndData", ortn);
		goto errOut;
	}
	attr = attrList->attr;
	if(attr->length == emailAddressLen) {
		if(!memcmp(attr->data, emailAddress, emailAddressLen)) {
			ourRtn = true;
		}
	}
errOut:
	SecKeychainItemFreeAttributesAndData(attrList, NULL);
	CFRelease(certRef);
	return ourRtn;
}
	
/* public function */
OSStatus findIdentity(
	const void			*emailAddress,		// UTF8 encoded email address
	unsigned			emailAddressLen,
	SecKeychainRef		kcRef,				// keychain to search, or NULL to search all
	SecIdentityRef		*idRef)				// RETURNED
{
	OSStatus 			ortn;
	
	/* Search for all identities */
	SecIdentitySearchRef srchRef = nil;
	ortn = SecIdentitySearchCreate(kcRef, 
		0,				// keyUsage - any
		&srchRef);
	if(ortn) {
		/* should never happen */
		cssmPerror("SecIdentitySearchCreate", ortn);
		return ortn;
	}

	SecIdentityRef foundId = NULL;
	do {
		SecIdentityRef thisId;
		ortn = SecIdentitySearchCopyNext(srchRef, &thisId);
		if(ortn != noErr) {
			break;
		}
		/* email addres match? */
		if(idHasEmail(thisId, emailAddress, emailAddressLen)) {
			foundId = thisId;
			break;
		}
		else {
			/* we're done with thie identity */
			CFRelease(thisId);
		}
	} while(ortn == noErr);
	CFRelease(srchRef);
	if(foundId) {
		*idRef = foundId;
		return noErr;
	}
	else {
		return errSecItemNotFound;
	}
}

