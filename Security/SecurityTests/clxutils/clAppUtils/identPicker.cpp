/*
 * Copyright (c) 2003-2008 Apple Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please 
 * obtain a copy of the License at http://www.apple.com/publicsource and 
 * read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 */

/*
 * identPicker.cp - Given a keychain, select from possible multiple
 * 				   	SecIdentityRefs via stdio UI, and cook up a 
 *				   	CFArray containing that identity and all certs needed
 *				   	for cert verification by an SSL peer. The resulting
 *				   	CFArrayRef is suitable for passing to SSLSetCertificate().
 */
 
#include "identPicker.h"
#include "sslAppUtils.h"
#include <sys/param.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* 
 * Safe gets().
 * -- guaranteed no buffer overflow
 * -- guaranteed NULL-terminated string
 * -- handles empty string (i.e., response is just CR) properly
 */
void getString(
	char *buf,
	unsigned bufSize)
{
	unsigned dex;
	char c;
	char *cp = buf;
	
	for(dex=0; dex<bufSize-1; dex++) {
		c = getchar();
		if(!isprint(c)) {
			break;
		}
		switch(c) {
			case '\n':
			case '\r':
				goto done;
			default:
				*cp++ = c;
		}
	}
done:
	*cp = '\0';
}

/*
 * Obtain the printable name of a SecKeychainItemRef as a C string.
 * Caller must free() the result.
 */
char *kcItemPrintableName(
	SecKeychainItemRef itemRef)
{
	char *crtn = NULL;

	/* just search for the one attr we want */
	UInt32 tag = kSecLabelItemAttr;
	SecKeychainAttributeInfo attrInfo;
	attrInfo.count = 1;
	attrInfo.tag = &tag;
	attrInfo.format = NULL;
	SecKeychainAttributeList *attrList = NULL;
	SecKeychainAttribute *attr = NULL;
	
	OSStatus ortn = SecKeychainItemCopyAttributesAndData(
		itemRef, 
		&attrInfo,
		NULL,			// itemClass
		&attrList, 
		NULL,			// length - don't need the data
		NULL);			// outData
	if(ortn) {
		cssmPerror("SecKeychainItemCopyAttributesAndData", ortn);
		/* may want to be a bit more robust here, but this should
		 * never happen */
		return strdup("Unnamed KeychainItem");
	}
	/* subsequent errors to errOut: */
	
	if((attrList == NULL) || (attrList->count != 1)) {
		printf("***Unexpected result fetching label attr\n");
		crtn = strdup("Unnamed KeychainItem");
		goto errOut;
	}
	/* We're assuming 8-bit ASCII attribute data here... */
	attr = attrList->attr;
	crtn = (char *)malloc(attr->length + 1);
	memmove(crtn, attr->data, attr->length);
	crtn[attr->length] = '\0';
	
errOut:
	SecKeychainItemFreeAttributesAndData(attrList, NULL);
	return crtn;
}

/*
 * Get the final term of a keychain's path as a C string. Caller must free() 
 * the result.
 */
char *kcFileName(
	SecKeychainRef kcRef)
{
	char fullPath[MAXPATHLEN + 1];
	OSStatus ortn;
	UInt32 pathLen = MAXPATHLEN;
	
	ortn = SecKeychainGetPath(kcRef,  &pathLen, fullPath);
	if(ortn) {
		cssmPerror("SecKeychainGetPath", ortn);
		return strdup("orphan keychain");
	}
	
	/* NULL terminate the path string and search for final '/' */
	fullPath[pathLen] = '\0';
	char *lastSlash = NULL;
	char *thisSlash = fullPath;
	do {
		thisSlash = strchr(thisSlash, '/');
		if(thisSlash == NULL) {
			/* done */
			break;
		}
		thisSlash++;
		lastSlash = thisSlash;
	} while(thisSlash != NULL);
	if(lastSlash == NULL) {
		/* no slashes, odd, but handle it */
		return strdup(fullPath);
	}
	else {
		return strdup(lastSlash);
	}
}

/* 
 * Obtain the final term of a keychain item's keychain path as a C string. 
 * Caller must free() the result.
 * May well return NULL indicating the item has no keychain (e.g. az floating cert).
 */
char *kcItemKcFileName(SecKeychainItemRef itemRef)
{
	OSStatus ortn;
	SecKeychainRef kcRef = NULL;
	
	ortn = SecKeychainItemCopyKeychain(itemRef, &kcRef);
	if(ortn) {
		return NULL;
	}
	char *rtnStr = kcFileName(kcRef);
	CFRelease(kcRef);
	return rtnStr;
}

/*
 * Given an array of SecIdentityRefs:
 *	-- display a printable name of each identity's cert;
 *  -- prompt user to select which one to use;
 *
 * Returns CFIndex of desired identity. A return of <0 indicates
 * "none - abort".
 */
static CFIndex pickIdent(
	CFArrayRef idArray)
{
	CFIndex count = CFArrayGetCount(idArray);
	CFIndex dex;
	OSStatus ortn;
	
	if(count == 0) {
		printf("***sslIdentPicker screwup: no identities found\n");
		return -1;
	}
	for(dex=0; dex<count; dex++) {
		SecIdentityRef idRef = (SecIdentityRef)CFArrayGetValueAtIndex(idArray, dex);
		SecCertificateRef certRef;
		ortn = SecIdentityCopyCertificate(idRef, &certRef);
		if(ortn) {
			/* should never happen */
			cssmPerror("SecIdentityCopyCertificate", ortn);
			return -1;
		}
		
		/* get printable name of cert and the keychain it's in */
		char *certLabel = kcItemPrintableName((SecKeychainItemRef)certRef);
		SecKeychainRef kcRef;
		char *kcLabel;
		ortn = SecKeychainItemCopyKeychain((SecKeychainItemRef)certRef, &kcRef);
		if(ortn) {
			cssmPerror("SecKeychainItemCopyKeychain", ortn);
			kcLabel = (char *)"Unnamed keychain";
		}
		else {
			kcLabel = kcFileName(kcRef);
		}
		printf("[%ld] keychain : %s\n", dex, kcLabel);
		printf("    cert     : %s\n", certLabel);
		free(certLabel);
		if(ortn == noErr) {
			free(kcLabel);
		}
		CFRelease(certRef);
	}
	
	while(1) {
		fpurge(stdin);
		printf("\nEnter Certificate number or CR to quit : ");
		fflush(stdout);
		char resp[64];
		getString(resp, sizeof(resp));
		if(resp[0] == '\0') {
			return -1;
		}
		int ires = atoi(resp);
		if((ires >= 0) && (ires < count)) {
			return (CFIndex)ires;
		}
		printf("***Invalid entry. Type a number between 0 and %ld\n", 
			count-1);
	}
	return -1;
}

OSStatus sslSimpleIdentPicker(
	SecKeychainRef		kcRef,			// NULL means use default list
	SecIdentityRef		*ident)			// RETURNED
{
	OSStatus 			ortn;
	CFMutableArrayRef	idArray = NULL;			// holds all SecIdentityRefs found
	
	/* Search for all identities */
	*ident = NULL;
	SecIdentitySearchRef srchRef = nil;
	ortn = SecIdentitySearchCreate(kcRef, 
		0,				// keyUsage - any
		&srchRef);
	if(ortn) {
		cssmPerror("SecIdentitySearchCreate", (CSSM_RETURN)ortn);
		printf("Cannot find signing key in keychain.\n");
		return ortn;
	}
	
	/* get all identities, stuff them into idArray */
	SecIdentityRef identity = nil;
	idArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	do {
		ortn = SecIdentitySearchCopyNext(srchRef, &identity);
		if(ortn != noErr) {
			break;
		}
		CFArrayAppendValue(idArray, identity);
		
		/* the array has the retain count we need */
		CFRelease(identity);
	} while(ortn == noErr);
	CFRelease(srchRef);
	
	switch(ortn) {
		case errSecItemNotFound:
			if(CFArrayGetCount(idArray) == 0) {
				printf("No signing keys found in keychain.\n");
				return errSecItemNotFound;
			}
			else {
				/* found at least one; proceed */
				break;
			}
		default:
			cssmPerror("SecIdentitySearchCopyNext", (CSSM_RETURN)ortn);
			printf("Cannot find signing key in keychain.\n");
			return ortn;
	}
	
	/*
	 * If there is just one, use it without asking 
	 */
	CFIndex whichId;
	if(CFArrayGetCount(idArray) == 1) {
		whichId = 0;
	}
	else {
		whichId = pickIdent(idArray);
		if(whichId < 0) {
			return CSSMERR_CSSM_USER_CANCELED;
		}
	}
	
	/* keep this one, free the rest */
	identity = (SecIdentityRef)CFArrayGetValueAtIndex(idArray, whichId);
	CFRetain(identity);
	CFRelease(idArray);
	*ident = identity;
	return noErr;
}

OSStatus sslIdentPicker(
	SecKeychainRef		kcRef,			// NULL means use default list
	SecCertificateRef	trustedAnchor,	// optional additional trusted anchor
	bool				includeRoot,	// true --> root is appended to outArray
										// false --> root not included
	const CSSM_OID		*vfyPolicy,		// optional - if NULL, use SSL
	CFArrayRef			*outArray)		// created and RETURNED
{
	OSStatus 			ortn;
	SecIdentityRef		identity;
	
	ortn = sslSimpleIdentPicker(kcRef, &identity);
	if(ortn) {
		return ortn;
	}
	return sslCompleteCertChain(identity, trustedAnchor, includeRoot, 
		vfyPolicy, outArray);
}

