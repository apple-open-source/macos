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
 * keychain_import.c
 */

#include "keychain_import.h"
#include "keychain_utilities.h"
#include "security.h"

#include <errno.h>
#include <unistd.h>
#include <Security/SecImportExport.h>
#include <Security/SecIdentity.h>
#include <Security/SecKey.h>
#include <Security/SecCertificate.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>

static int do_keychain_import(
	SecKeychainRef		kcRef,
	CFDataRef			inData,
	SecExternalFormat   externFormat,
	SecExternalItemType itemType,
	const char			*passphrase,
	const char			*fileName)
{
	SecKeyImportExportParameters	keyParams;
	OSStatus		ortn;
	CFStringRef		fileStr;
	CFArrayRef		outArray = NULL;
	int				result = 0;
	int				numCerts = 0;
	int				numKeys = 0;
	int				numIdentities = 0;
	CFIndex			dex;
	CFIndex			numItems = 0;
	CFStringRef		passStr = NULL;
	
	fileStr = CFStringCreateWithCString(NULL, fileName, kCFStringEncodingASCII);
	
	/* 
	 * Specify some kind of passphrase in case caller doesn't know this 
	 * is a wrapped object
	 */
	memset(&keyParams, 0, sizeof(SecKeyImportExportParameters));
	keyParams.version = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION;
	if(passphrase != NULL) {
		passStr = CFStringCreateWithCString(NULL, passphrase, kCFStringEncodingASCII);
		keyParams.passphrase = passStr;
	}
	else {
		keyParams.flags = kSecKeySecurePassphrase;
	}
	ortn = SecKeychainItemImport(inData, fileStr, &externFormat, &itemType,
		0,		/* flags not used (yet) */
		&keyParams,
		kcRef,
		&outArray);
	if(ortn) {
		sec_perror("SecKeychainItemImport", ortn);
		result = 1;
		goto loser;
	}
	
	/*
	 * Parse returned items & report to user
	 */
	if(outArray == NULL) {
		sec_error("No keychain items found");
		result = 1;
		goto loser;
	}
	numItems = CFArrayGetCount(outArray);
	for(dex=0; dex<numItems; dex++) {   
		CFTypeRef item = CFArrayGetValueAtIndex(outArray, dex);
		CFTypeID itemType = CFGetTypeID(item);
		if(itemType == SecIdentityGetTypeID()) {
			numIdentities++;
		}
		else if(itemType == SecCertificateGetTypeID()) {
			numCerts++;
		}
		else if(itemType == SecKeyGetTypeID()) {
			numKeys++;
		}
		else {
			sec_error("Unexpected item type returned from SecKeychainItemImport");
			result = 1;
			goto loser;
		}
	}
	if(numIdentities) {
		char *str;
		if(numIdentities > 1) {
			str = "identities";
		}
		else {
			str = "identity";
		}
		fprintf(stdout, "%d %s imported.\n", numIdentities, str);
	}
	if(numKeys) {
		char *str;
		if(numKeys > 1) {
			str = "keys";
		}
		else {
			str = "key";
		}
		fprintf(stdout, "%d %s imported.\n", numKeys, str);
	}
	if(numCerts) {
		char *str;
		if(numCerts > 1) {
			str = "certificates";
		}
		else {
			str = "certificate";
		}
		fprintf(stdout, "%d %s imported.\n", numCerts, str);
	}
	
loser:
	CFRelease(fileStr);
	if(outArray) {
		CFRelease(outArray);
	}
	if(passStr) {
		CFRelease(passStr);
	}
	return result;
}
	
int
keychain_import(int argc, char * const *argv)
{
	int ch, result = 0;
	
	char *inFile = NULL;
	char *kcName = NULL;
	SecKeychainRef kcRef = NULL;
	SecExternalFormat externFormat = kSecFormatUnknown;
	SecExternalItemType itemType = kSecItemTypeUnknown;
	int wrapped = 0;
	const char *passphrase = NULL;
	unsigned char *inFileData = NULL;
	unsigned inFileLen = 0;
	CFDataRef inData = NULL;
	
	if(argc < 2) {
		return 2; /* @@@ Return 2 triggers usage message. */
	}
	inFile = argv[1];
	if((argc == 2) && (inFile[0] == '-')) {
		return 2;
	}
	optind = 2;
	
    while ((ch = getopt(argc, argv, "k:t:f:P:wh")) != -1)
	{
		switch  (ch)
		{
		case 'k':
			kcName = optarg;
			break;
		case 't':
			if(!strcmp("pub", optarg)) {
				itemType = kSecItemTypePublicKey;
			}
			else if(!strcmp("priv", optarg)) {
				itemType = kSecItemTypePrivateKey;
			}
			else if(!strcmp("session", optarg)) {
				itemType = kSecItemTypeSessionKey;
			}
			else if(!strcmp("cert", optarg)) {
				itemType = kSecItemTypeCertificate;
			}
			else if(!strcmp("agg", optarg)) {
				itemType = kSecItemTypeAggregate;
			}
			else {
				return 2; /* @@@ Return 2 triggers usage message. */
			}
			break;
		case 'f':
			if(!strcmp("openssl", optarg)) {
				externFormat = kSecFormatOpenSSL;
			}
			else if(!strcmp("openssh", optarg)) {
				externFormat = kSecFormatSSH;
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
			case kSecFormatWrappedPKCS8:
				/* proceed */
				break;
			default:
				fprintf(stderr, "Don't know how to wrap in specified format/type\n");
				return 2; /* @@@ Return 2 triggers usage message. */
		}
	}

	if(kcName) {
		kcRef = keychain_open(kcName);
		if(kcRef == NULL) {
			return 1;
		}
	}
	if(readFile(inFile, &inFileData, &inFileLen)) {
		sec_error("Error reading infile %s: %s", inFile, strerror(errno));
		goto loser;
	}   
	inData = CFDataCreate(NULL, inFileData, inFileLen);
	if(inData == NULL) {
		goto loser;
	}
	free(inFileData);
	result = do_keychain_import(kcRef, inData, externFormat, itemType,
		passphrase, inFile);
	
loser:
	if(kcRef) {
		CFRelease(kcRef);
	}
	if(inData) {
		CFRelease(inData);
	}
	return result;
}
