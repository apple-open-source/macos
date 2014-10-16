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
 * identPicker.cpp - Given a keychain, select from possible multiple
 *		     SecIdentityRefs via stdio UI, and cook up a 
 *		     CFArray containing that identity and all certs needed
 *		     for cert verification by an SSL peer. The resulting
 *		     CFArrayRef is suitable for passing to SSLSetCertificate().
 */
 
#include "identPicker.h"
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
static void getString(
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
static char *kcItemPrintableName(
    SecKeychainItemRef certRef)
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
	(SecKeychainItemRef)certRef, 
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
static char *kcFileName(
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
 * Determine if specified SecCertificateRef is a self-signed cert.
 * We do this by comparing the subject and issuerr names; no cryptographic
 * verification is performed.
 *
 * Returns true if the cert appears to be a root. 
 */
static bool isCertRefRoot(
	SecCertificateRef certRef)
{
    /* just search for the two attrs we want */
    UInt32 tags[2] = {kSecSubjectItemAttr, kSecIssuerItemAttr};
    SecKeychainAttributeInfo attrInfo;
    attrInfo.count = 2;
    attrInfo.tag = tags;
    attrInfo.format = NULL;
    SecKeychainAttributeList *attrList = NULL;
    SecKeychainAttribute *attr1 = NULL;
    SecKeychainAttribute *attr2 = NULL;
    bool brtn = false;
    
    OSStatus ortn = SecKeychainItemCopyAttributesAndData(
	(SecKeychainItemRef)certRef, 
	&attrInfo,
	NULL,			// itemClass
	&attrList, 
	NULL,			// length - don't need the data
	NULL);			// outData
    if(ortn) {
	cssmPerror("SecKeychainItemCopyAttributesAndData", ortn);
	/* may want to be a bit more robust here, but this should
	 * never happen */
	return false;
    }
    /* subsequent errors to errOut: */
    
    if((attrList == NULL) || (attrList->count != 2)) {
	printf("***Unexpected result fetching label attr\n");
	goto errOut;
    }
    
    /* rootness is just byte-for-byte compare of the two names */ 
    attr1 = &attrList->attr[0];
    attr2 = &attrList->attr[1];
    if(attr1->length == attr2->length) {
	if(memcmp(attr1->data, attr2->data, attr1->length) == 0) {
	    brtn = true;
	}
    }
errOut:
    SecKeychainItemFreeAttributesAndData(attrList, NULL);
    return brtn;
}


/*
 * Given a SecIdentityRef, do our best to construct a complete, ordered, and 
 * verified cert chain, returning the result in a CFArrayRef. The result is 
 * suitable for use when calling SSLSetCertificate().
 */
static OSStatus completeCertChain(
	SecIdentityRef 		identity, 
	SecCertificateRef	trustedAnchor,	// optional additional trusted anchor
	bool			includeRoot, 	// include the root in outArray
	CFArrayRef		*outArray)	// created and RETURNED
{
    CFMutableArrayRef 			certArray;
    SecTrustRef					secTrust = NULL;
    SecPolicyRef				policy = NULL;
    SecPolicySearchRef			policySearch = NULL;
    SecTrustResultType			secTrustResult;
    CSSM_TP_APPLE_EVIDENCE_INFO *dummyEv;			// not used
    CFArrayRef					certChain = NULL;   // constructed chain
    CFIndex 					numResCerts;
    
    certArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(certArray, identity);
    
    /*
     * Case 1: identity is a root; we're done. Note that this case
     * overrides the includeRoot argument.
     */
    SecCertificateRef certRef;
    OSStatus ortn = SecIdentityCopyCertificate(identity, &certRef);
    if(ortn) {
	/* should never happen */
	cssmPerror("SecIdentityCopyCertificate", ortn);
	return ortn;
    }
    bool isRoot = isCertRefRoot(certRef);
    if(isRoot) {
	*outArray = certArray;
	CFRelease(certRef);
	return noErr;
    }
    
    /* 
     * Now use SecTrust to get a complete cert chain, using all of the 
     * user's keychains to look for intermediate certs.
     * NOTE this does NOT handle root certs which are not in the system
     * root cert DB. (The above case, where the identity is a root cert, does.)
     */
    CFMutableArrayRef subjCerts = CFArrayCreateMutable(NULL, 1, &kCFTypeArrayCallBacks);
    CFArraySetValueAtIndex(subjCerts, 0, certRef);
		    
    /* the array owns the subject cert ref now */
    CFRelease(certRef);
    
    /* Get a SecPolicyRef for generic X509 cert chain verification */
    ortn = SecPolicySearchCreate(CSSM_CERT_X_509v3,
	    &CSSMOID_APPLE_X509_BASIC,
	    NULL,				// value
	    &policySearch);
    if(ortn) {
	cssmPerror("SecPolicySearchCreate", ortn);
	goto errOut;
    }
    ortn = SecPolicySearchCopyNext(policySearch, &policy);
    if(ortn) {
	cssmPerror("SecPolicySearchCopyNext", ortn);
	goto errOut;
    }

    /* build a SecTrustRef for specified policy and certs */
    ortn = SecTrustCreateWithCertificates(subjCerts,
	    policy, &secTrust);
    if(ortn) {
	cssmPerror("SecTrustCreateWithCertificates", ortn);
	goto errOut;
    }
    
    if(trustedAnchor) {
	/* 
	 * Tell SecTrust to trust this one in addition to the current
	 * trusted system-wide anchors.
	 */
	CFMutableArrayRef newAnchors;
	CFArrayRef currAnchors;
	
	ortn = SecTrustCopyAnchorCertificates(&currAnchors);
	if(ortn) {
	    /* should never happen */
	    cssmPerror("SecTrustCopyAnchorCertificates", ortn);
	    goto errOut;
	}
	newAnchors = CFArrayCreateMutableCopy(NULL,
	    CFArrayGetCount(currAnchors) + 1,
	    currAnchors);
	CFRelease(currAnchors);
	CFArrayAppendValue(newAnchors, trustedAnchor);
	ortn = SecTrustSetAnchorCertificates(secTrust, newAnchors);
	CFRelease(newAnchors);
	if(ortn) {
	    cssmPerror("SecTrustSetAnchorCertificates", ortn);
	    goto errOut;
	}
    }
    /* evaluate: GO */
    ortn = SecTrustEvaluate(secTrust, &secTrustResult);
    if(ortn) {
	cssmPerror("SecTrustEvaluate", ortn);
	goto errOut;
    }
    switch(secTrustResult) {
	case kSecTrustResultUnspecified:
	    /* cert chain valid, no special UserTrust assignments */
	case kSecTrustResultProceed:
	    /* cert chain valid AND user explicitly trusts this */
	    break;
	default:
	    /*
	     * Cert chain construction failed. 
	     * Just go with the single subject cert we were given.
	     */
	    printf("***Warning: could not construct completed cert chain\n");
	    ortn = noErr;
	    goto errOut;
    }

    /* get resulting constructed cert chain */
    ortn = SecTrustGetResult(secTrust, &secTrustResult, &certChain, &dummyEv);
    if(ortn) {
	cssmPerror("SecTrustEvaluate", ortn);
	goto errOut;
    }
    
    /*
     * Copy certs from constructed chain to our result array, skipping 
     * the leaf (which is already there, as a SecIdentityRef) and possibly
     * a root.
     */
    numResCerts = CFArrayGetCount(certChain);
    if(numResCerts < 2) {
	/*
	 * Can't happen: if subject was a root, we'd already have returned. 
	 * If chain doesn't verify to a root, we'd have bailed after
	 * SecTrustEvaluate().
	 */
	printf("***sslCompleteCertChain screwup: numResCerts %d\n", 
		(int)numResCerts);
	ortn = noErr;
	goto errOut;
    }
    if(!includeRoot) {
	/* skip the last (root) cert) */
	numResCerts--;
    }
    for(CFIndex dex=1; dex<numResCerts; dex++) {
	certRef = (SecCertificateRef)CFArrayGetValueAtIndex(certChain, dex);
	CFArrayAppendValue(certArray, certRef);
    }
errOut:
    /* clean up */
    if(secTrust) {
	CFRelease(secTrust);
    }
    if(subjCerts) {
	CFRelease(subjCerts);
    }
    if(policy) {
	CFRelease(policy);
    }
    if(policySearch) {
	CFRelease(policySearch);
    }
    *outArray = certArray;
    return ortn;
}


/*
 * Given an array of SecIdentityRefs:
 *  -- display a printable name of each identity's cert;
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
	    kcLabel = "Unnamed keychain";
	}
	else {
	    kcLabel = kcFileName(kcRef);
	}
	printf("[%d] keychain : %s\n", (int)dex, kcLabel);
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
	printf("***Invalid entry. Type a number between 0 and %d\n", 
	    (int)(count-1));
    }
    return -1;
}

OSStatus simpleIdentPicker(
    SecKeychainRef	kcRef,			// NULL means use default list
    SecIdentityRef	*ident)			// RETURNED
{
    OSStatus 		ortn;
    CFMutableArrayRef	idArray = NULL;		// holds all SecIdentityRefs found
    
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

OSStatus identPicker(
    SecKeychainRef	kcRef,		// NULL means use default list
    SecCertificateRef	trustedAnchor,	// optional additional trusted anchor
    bool		includeRoot,	// true --> root is appended to outArray
					// false --> root not included
    CFArrayRef		*outArray)	// created and RETURNED
{
    OSStatus 			ortn;
    SecIdentityRef		identity;
    
    ortn = simpleIdentPicker(kcRef, &identity);
    if(ortn) {
	return ortn;
    }
    return completeCertChain(identity, trustedAnchor, includeRoot, outArray);
}

