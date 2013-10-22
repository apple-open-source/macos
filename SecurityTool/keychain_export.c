/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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
 *
 * keychain_export.c
 */

#include "keychain_export.h"
#include "keychain_utilities.h"
#include "security.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <Security/SecImportExport.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecKeychainSearch.h>
#include <Security/SecIdentitySearch.h>
#include <Security/SecKey.h>
#include <Security/SecCertificate.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>

typedef enum {
	IS_Certs,
	IS_AllKeys,
	IS_PubKeys,
	IS_PrivKeys,
	IS_Identities,
	IS_All
} ItemSpec;

/*
 * Add all itmes of specified class from a keychain to an array.
 * Item class are things like kSecCertificateItemClass, and
 * CSSM_DL_DB_RECORD_PRIVATE_KEY. Identities are searched separately.
 */
static OSStatus addKcItems(
	SecKeychainRef kcRef,
	SecItemClass itemClass,		// kSecCertificateItemClass
	CFMutableArrayRef outArray,
	unsigned *numItems)			// UPDATED on return
{
	OSStatus ortn;
	SecKeychainSearchRef srchRef;

	ortn = SecKeychainSearchCreateFromAttributes(kcRef,
		itemClass,
		NULL,		// no attrs
		&srchRef);
	if(ortn) {
		sec_perror("SecKeychainSearchCreateFromAttributes", ortn);
		return ortn;
	}
	for(;;) {
		SecKeychainItemRef itemRef;
		ortn = SecKeychainSearchCopyNext(srchRef, &itemRef);
		if(ortn) {
			if(ortn == errSecItemNotFound) {
				/* normal search end */
				ortn = noErr;
			}
			else {
				sec_perror("SecIdentitySearchCopyNext", ortn);
			}
			break;
		}
		CFArrayAppendValue(outArray, itemRef);
		CFRelease(itemRef);		// array owns the item
		(*numItems)++;
	}
	CFRelease(srchRef);
	return ortn;
}

/*
 * Add all SecIdentityRefs from a keychain into an array.
 */
static OSStatus addIdentities(
	SecKeychainRef kcRef,
	CFMutableArrayRef outArray,
	unsigned *numItems)			// UPDATED on return
{
	/* Search for all identities */
	SecIdentitySearchRef srchRef;
	OSStatus ortn = SecIdentitySearchCreate(kcRef,
		0,				// keyUsage - any
		&srchRef);
	if(ortn) {
		sec_perror("SecIdentitySearchCreate", ortn);
		return ortn;
	}

	do {
		SecIdentityRef identity;
		ortn = SecIdentitySearchCopyNext(srchRef, &identity);
		if(ortn) {
			if(ortn == errSecItemNotFound) {
				/* normal search end */
				ortn = noErr;
			}
			else {
				sec_perror("SecIdentitySearchCopyNext", ortn);
			}
			break;
		}
		CFArrayAppendValue(outArray, identity);

		/* the array has the retain count we need */
		CFRelease(identity);
		(*numItems)++;
	} while(ortn == noErr);
	CFRelease(srchRef);
	return ortn;
}

static int do_keychain_export(
	SecKeychainRef		kcRef,
	SecExternalFormat   externFormat,
	ItemSpec			itemSpec,
	const char			*passphrase,
	int					doPem,
	const char			*fileName)
{
	int result = 0;
	CFIndex numItems;
	unsigned numPrivKeys = 0;
	unsigned numPubKeys = 0;
	unsigned numCerts = 0;
	unsigned numIdents = 0;
	OSStatus ortn;
	uint32 expFlags = 0;		// SecItemImportExportFlags
	SecKeyImportExportParameters keyParams;
	CFStringRef	passStr = NULL;
	CFDataRef outData = NULL;
	unsigned len;

	/* gather items */
	CFMutableArrayRef exportItems = CFArrayCreateMutable(NULL, 0,
		&kCFTypeArrayCallBacks);
	switch(itemSpec) {
		case IS_Certs:
			ortn = addKcItems(kcRef, kSecCertificateItemClass, exportItems, &numCerts);
			if(ortn) {
				result = 1;
				goto loser;
			}
			break;

		case IS_PrivKeys:
			ortn = addKcItems(kcRef, CSSM_DL_DB_RECORD_PRIVATE_KEY, exportItems,
				&numPrivKeys);
			if(ortn) {
				result = 1;
				goto loser;
			}
			break;

		case IS_PubKeys:
			ortn = addKcItems(kcRef, CSSM_DL_DB_RECORD_PUBLIC_KEY, exportItems,
				&numPubKeys);
			if(ortn) {
				result = 1;
				goto loser;
			}
			break;

		case IS_AllKeys:
			ortn = addKcItems(kcRef, CSSM_DL_DB_RECORD_PRIVATE_KEY, exportItems,
				&numPrivKeys);
			if(ortn) {
				result = 1;
				goto loser;
			}
			ortn = addKcItems(kcRef, CSSM_DL_DB_RECORD_PUBLIC_KEY, exportItems,
				&numPubKeys);
			if(ortn) {
				result = 1;
				goto loser;
			}
			break;

		case IS_All:
			/* No public keys here - PKCS12 doesn't support them */
			ortn = addKcItems(kcRef, kSecCertificateItemClass, exportItems, &numCerts);
			if(ortn) {
				result = 1;
				goto loser;
			}
			ortn = addKcItems(kcRef, CSSM_DL_DB_RECORD_PRIVATE_KEY, exportItems,
				&numPrivKeys);
			if(ortn) {
				result = 1;
				goto loser;
			}
			break;

		case IS_Identities:
			ortn = addIdentities(kcRef, exportItems, &numIdents);
			if(ortn) {
				result = 1;
				goto loser;
			}
			if(numIdents) {
				numPrivKeys += numIdents;
				numCerts    += numIdents;
			}
			break;
		default:
			sec_error("Internal error parsing item_spec");
			result = 1;
			goto loser;
	}

	numItems = CFArrayGetCount(exportItems);
	if(externFormat == kSecFormatUnknown) {
		/* Use default export format per set of items */
		if(numItems > 1) {
			externFormat = kSecFormatPEMSequence;
		}
		else if(numCerts) {
			externFormat = kSecFormatX509Cert;
		}
		else {
			externFormat = kSecFormatOpenSSL;
		}
	}
	if(doPem) {
		expFlags |= kSecItemPemArmour;
	}

	/*
	 * Key related arguments, ignored if we're not exporting keys.
	 * Always specify some kind of passphrase - default is secure passkey.
	 */
	memset(&keyParams, 0, sizeof(keyParams));
	keyParams.version = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION;
	if(passphrase != NULL) {
		passStr = CFStringCreateWithCString(NULL, passphrase, kCFStringEncodingASCII);
		keyParams.passphrase = passStr;
	}
	else {
		keyParams.flags = kSecKeySecurePassphrase;
	}

	/* Go */
	ortn = SecKeychainItemExport(exportItems, externFormat, expFlags, &keyParams,
		&outData);
	if(ortn) {
		sec_perror("SecKeychainItemExport", ortn);
		result = 1;
		goto loser;
	}

	len = CFDataGetLength(outData);
	if(fileName) {
		int rtn = writeFile(fileName, CFDataGetBytePtr(outData), len);
		if(rtn == 0) {
			if(!do_quiet) {
				fprintf(stderr, "...%u bytes written to %s\n", len, fileName);
			}
		}
		else {
			sec_error("Error writing to %s: %s", fileName, strerror(errno));
			result = 1;
		}
	}
	else {
		int irtn = write(STDOUT_FILENO, CFDataGetBytePtr(outData), len);
		if(irtn != (int)len) {
			perror("write");
		}
	}
loser:
	if(exportItems) {
		CFRelease(exportItems);
	}
	if(passStr) {
		CFRelease(passStr);
	}
	if(outData) {
		CFRelease(outData);
	}
	return result;
}

int
keychain_export(int argc, char * const *argv)
{
	int ch, result = 0;

	char *outFile = NULL;
	char *kcName = NULL;
	SecKeychainRef kcRef = NULL;
	SecExternalFormat externFormat = kSecFormatUnknown;
	ItemSpec itemSpec = IS_All;
	int wrapped = 0;
	int doPem = 0;
	const char *passphrase = NULL;

    while ((ch = getopt(argc, argv, "k:o:t:f:P:wph")) != -1)
	{
		switch  (ch)
		{
		case 'k':
			kcName = optarg;
			break;
		case 'o':
			outFile = optarg;
			break;
		case 't':
			if(!strcmp("certs", optarg)) {
				itemSpec = IS_Certs;
			}
			else if(!strcmp("allKeys", optarg)) {
				itemSpec = IS_AllKeys;
			}
			else if(!strcmp("pubKeys", optarg)) {
				itemSpec = IS_PubKeys;
			}
			else if(!strcmp("privKeys", optarg)) {
				itemSpec = IS_PrivKeys;
			}
			else if(!strcmp("identities", optarg)) {
				itemSpec = IS_Identities;
			}
			else if(!strcmp("all", optarg)) {
				itemSpec = IS_All;
			}
			else {
				return 2; /* @@@ Return 2 triggers usage message. */
			}
			break;
		case 'f':
			if(!strcmp("openssl", optarg)) {
				externFormat = kSecFormatOpenSSL;
			}
			else if(!strcmp("openssh1", optarg)) {
				externFormat = kSecFormatSSH;
			}
			else if(!strcmp("openssh2", optarg)) {
				externFormat = kSecFormatSSHv2;
			}
			else if(!strcmp("bsafe", optarg)) {
				externFormat = kSecFormatBSAFE;
			}
			else if(!strcmp("raw", optarg)) {
				externFormat = kSecFormatRawKey;
			}
			else if(!strcmp("pkcs7", optarg)) {
				externFormat = kSecFormatPKCS7;
			}
			else if(!strcmp("pkcs8", optarg)) {
				externFormat = kSecFormatWrappedPKCS8;
			}
			else if(!strcmp("pkcs12", optarg)) {
				externFormat = kSecFormatPKCS12;
			}
			else if(!strcmp("netscape", optarg)) {
				externFormat = kSecFormatNetscapeCertSequence;
			}
			else if(!strcmp("x509", optarg)) {
				externFormat = kSecFormatX509Cert;
			}
			else if(!strcmp("pemseq", optarg)) {
				externFormat = kSecFormatPEMSequence;
			}
			else {
				return 2; /* @@@ Return 2 triggers usage message. */
			}
			break;
		case 'w':
			wrapped = 1;
			break;
		case 'p':
			doPem = 1;
			break;
		case 'P':
			passphrase = optarg;
			break;
		case '?':
		default:
			return 2; /* @@@ Return 2 triggers usage message. */
		}
	}

	if(wrapped) {
		switch(externFormat) {
			case kSecFormatOpenSSL:
			case kSecFormatUnknown:		// i.e., use default
				externFormat = kSecFormatWrappedOpenSSL;
				break;
			case kSecFormatSSH:
				externFormat = kSecFormatWrappedSSH;
				break;
			case kSecFormatSSHv2:
				/* there is no wrappedSSHv2 */
				externFormat = kSecFormatWrappedOpenSSL;
				break;
			case kSecFormatWrappedPKCS8:
				/* proceed */
				break;
			default:
				sec_error("Don't know how to wrap in specified format/type");
				return 2; /* @@@ Return 2 triggers usage message. */
		}
	}

	if(kcName) {
		kcRef = keychain_open(kcName);
		if(kcRef == NULL) {
			return 1;
		}
	}
	result = do_keychain_export(kcRef, externFormat, itemSpec,
		passphrase, doPem, outFile);

	if(kcRef) {
		CFRelease(kcRef);
	}
	return result;
}
