/*
 * Copyright (c) 2002-2004,2007-2008,2010,2012-2017 Apple Inc. All Rights Reserved.
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
 *  SecIdentity.c - CoreFoundation based object containing a
 *  private key, certificate tuple.
 */


#include <Security/SecIdentity.h>

#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFString.h>
#include <Security/SecCertificate.h>
#include <Security/SecKey.h>
#include <pthread.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecInternal.h>
#include <utilities/SecCFWrappers.h>

struct __SecIdentity {
    CFRuntimeBase		_base;
	SecCertificateRef	_certificate;
	SecKeyRef			_privateKey;
};

CFGiblisWithHashFor(SecIdentity)

/* Static functions. */
static CFStringRef SecIdentityCopyFormatDescription(CFTypeRef cf, CFDictionaryRef formatOptions) {
    SecIdentityRef identity = (SecIdentityRef)cf;
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
        CFSTR("<SecIdentityRef: %p>"), identity);
}

static void SecIdentityDestroy(CFTypeRef cf) {
    SecIdentityRef identity = (SecIdentityRef)cf;
	CFReleaseNull(identity->_certificate);
	CFReleaseNull(identity->_privateKey);
}

static Boolean SecIdentityCompare(CFTypeRef cf1, CFTypeRef cf2) {
    SecIdentityRef identity1 = (SecIdentityRef)cf1;
    SecIdentityRef identity2 = (SecIdentityRef)cf2;
    if (identity1 == identity2)
        return true;
    if (!identity2)
        return false;
    return CFEqual(identity1->_certificate, identity2->_certificate) &&
		CFEqual(identity1->_privateKey, identity2->_privateKey);
}

/* Hash of identity is hash of certificate plus hash of key. */
static CFHashCode SecIdentityHash(CFTypeRef cf) {
    SecIdentityRef identity = (SecIdentityRef)cf;
	return CFHash(identity->_certificate) + CFHash(identity->_privateKey);
}

OSStatus SecIdentityCopyCertificate(SecIdentityRef identity,
	SecCertificateRef *certificateRef) {
	*certificateRef = identity->_certificate;
	CFRetain(*certificateRef);
	return 0;
}

OSStatus SecIdentityCopyPrivateKey(SecIdentityRef identity,
	SecKeyRef *privateKeyRef) {
	*privateKeyRef = identity->_privateKey;
	CFRetain(*privateKeyRef);
	return 0;
}

SecIdentityRef SecIdentityCreate(CFAllocatorRef allocator,
	SecCertificateRef certificate, SecKeyRef privateKey) {
    if (!certificate || CFGetTypeID(certificate) != SecCertificateGetTypeID() ||
        !privateKey || CFGetTypeID(privateKey) != SecKeyGetTypeID()) {
        return NULL;
    }

    SecIdentityRef result = NULL;
    /* Compare the public keys to make sure we're making a coherent identity,
     * use the ExternalRepresentations so we don't fall into traps caused by different backing key types. */
    SecKeyRef publicKey = SecKeyCopyPublicKey(privateKey);
    if (!publicKey) {
        secwarning("SecIdentityCreate: failed to extract public key from private key");
        return NULL;
    }

    CFDataRef publicKeyData = SecKeyCopyExternalRepresentation(publicKey, NULL);
    SecKeyRef certKey = SecCertificateCopyKey(certificate);
    CFDataRef certKeyData = SecKeyCopyExternalRepresentation(certKey, NULL);
    if (CFEqualSafe(certKeyData, publicKeyData)) {
        CFIndex size = sizeof(struct __SecIdentity);
        result = (SecIdentityRef)_CFRuntimeCreateInstance(allocator, SecIdentityGetTypeID(), size - sizeof(CFRuntimeBase), 0);
        if (result) {
            CFRetain(certificate);
            CFRetain(privateKey);
            result->_certificate = certificate;
            result->_privateKey = privateKey;
        }
    } else {
        secwarning("Creating SecIdentity with mismatching public keys: %{mask.hash}@, %{mask.hash}@", certKeyData, publicKeyData);
        // TODO: rdar://152691063 (analytics data for SecIdentityCreate check key matches cert)
    }
    CFReleaseNull(publicKey);
    CFReleaseNull(publicKeyData);
    CFReleaseNull(certKey);
    CFReleaseNull(certKeyData);

    return result;
}
