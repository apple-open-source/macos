/*
 * Copyright (c) 2003-2009,2012,2014 Apple Inc. All Rights Reserved.
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
#include "access_utils.h"
#include "security_tool.h"

#include <utilities/fileIo.h>

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <Security/SecImportExport.h>
#include <Security/SecIdentity.h>
#include <Security/SecKey.h>
#include <Security/SecCertificate.h>
#include <Security/SecKeychainItemExtendedAttributes.h>
#include <CoreFoundation/CoreFoundation.h>

// SecTrustedApplicationCreateApplicationGroup
#include <Security/SecTrustedApplicationPriv.h>

#define KC_IMPORT_KEY_PASSWORD_MESSAGE      CFSTR("Enter the password for \"%1$@\":")
#define KC_IMPORT_KEY_PASSWORD_RETRYMESSAGE CFSTR("Sorry, you entered an invalid password.\n\nEnter the password for \"%1$@\":")

static int do_keychain_import(
	SecKeychainRef		kcRef,
	CFDataRef			inData,
	SecExternalFormat   externFormat,
	SecExternalItemType itemType,
	SecAccessRef		access,
	Boolean				nonExtractable,
	const char			*passphrase,
	const char			*fileName,
	char				**attrNames,
	char				**attrValues,
	unsigned			numExtendedAttributes)
{
	SecKeyImportExportParameters	keyParams;
	OSStatus		ortn;
	CFStringRef		fileStr;
	CFArrayRef		outArray = NULL;
	int				result = 0;
	int				numCerts = 0;
	int				numKeys = 0;
	int				numIdentities = 0;
	int				tryCount = 0;
	CFIndex			dex;
	CFIndex			numItems = 0;
	CFStringRef		passStr = NULL;
	CFStringRef		promptStr = NULL;
	CFStringRef		retryStr = NULL;

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
	if(nonExtractable) {
        // explicitly set the key attributes, omitting the CSSM_KEYATTR_EXTRACTABLE bit
        keyParams.keyAttributes = CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_SENSITIVE;
    }
	keyParams.accessRef = access;

	fileStr = CFStringCreateWithCString(NULL, fileName, kCFStringEncodingUTF8);
	if (fileStr) {
		CFURLRef fileURL = CFURLCreateWithFileSystemPath(NULL, fileStr, kCFURLPOSIXPathStyle, FALSE);
		if (fileURL) {
			CFStringRef nameStr = CFURLCopyLastPathComponent(fileURL);
			if (nameStr) {
				safe_CFRelease(&fileStr);
				fileStr = nameStr;
			}
			safe_CFRelease(&fileURL);
		}
	}
	promptStr = CFStringCreateWithFormat(NULL, NULL, KC_IMPORT_KEY_PASSWORD_MESSAGE, fileStr);
	retryStr = CFStringCreateWithFormat(NULL, NULL, KC_IMPORT_KEY_PASSWORD_RETRYMESSAGE, fileStr);

	while (TRUE)
	{
		keyParams.alertPrompt = (tryCount == 0) ? promptStr : retryStr;

		ortn = SecKeychainItemImport(inData,
									 fileStr,
									 &externFormat,
									 &itemType,
									 0,		/* flags not used (yet) */
									 &keyParams,
									 kcRef,
									 &outArray);

		if(ortn) {
			if (ortn == errSecPkcs12VerifyFailure && ++tryCount < 3) {
				continue;
			}
			sec_perror("SecKeychainItemImport", ortn);
			result = 1;
			goto cleanup;
		}
		break;
	}

	/*
	 * Parse returned items & report to user
	 */
	if(outArray == NULL) {
		sec_error("No keychain items found");
		result = 1;
		goto cleanup;
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
			goto cleanup;
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

	/* optionally apply extended attributes */
	if(numExtendedAttributes) {
		unsigned attrDex;
		for(attrDex=0; attrDex<numExtendedAttributes; attrDex++) {
			CFStringRef attrNameStr = CFStringCreateWithCString(NULL, attrNames[attrDex],
				kCFStringEncodingASCII);
			CFDataRef attrValueData = CFDataCreate(NULL, (const UInt8 *)attrValues[attrDex],
				strlen(attrValues[attrDex]));
			for(dex=0; dex<numItems; dex++) {
				SecKeychainItemRef itemRef =
					(SecKeychainItemRef)CFArrayGetValueAtIndex(outArray, dex);
				ortn = SecKeychainItemSetExtendedAttribute(itemRef, attrNameStr, attrValueData);
				if(ortn) {
					cssmPerror("SecKeychainItemSetExtendedAttribute", ortn);
					result = 1;
					break;
				}
			}	/* for each imported item */
			CFRelease(attrNameStr);
			CFRelease(attrValueData);
			if(result) {
				break;
			}
		}	/* for each extended attribute */
	}

cleanup:
	safe_CFRelease(&fileStr);
	safe_CFRelease(&outArray);
	safe_CFRelease(&passStr);
	safe_CFRelease(&promptStr);
	safe_CFRelease(&retryStr);

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
	Boolean wrapped = FALSE;
	Boolean nonExtractable = FALSE;
	const char *passphrase = NULL;
	unsigned char *inFileData = NULL;
	size_t inFileLen = 0;
	CFDataRef inData = NULL;
	unsigned numExtendedAttributes = 0;
	char **attrNames = NULL;
	char **attrValues = NULL;
	Boolean access_specified = FALSE;
	Boolean always_allow = FALSE;
	SecAccessRef access = NULL;
	CFMutableArrayRef trusted_list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	if(argc < 2) {
		result = 2; /* @@@ Return 2 triggers usage message. */
		goto cleanup;
	}
	inFile = argv[1];
	if((argc == 2) && (inFile[0] == '-')) {
		result = 2;
		goto cleanup;
	}
	optind = 2;

    while ((ch = getopt(argc, argv, "k:t:f:P:wxa:hAT:")) != -1)
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
				result = 2; /* @@@ Return 2 triggers usage message. */
				goto cleanup;
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
				result = 2; /* @@@ Return 2 triggers usage message. */
				goto cleanup;
			}
			break;
		case 'w':
			wrapped = TRUE;
			break;
		case 'x':
			nonExtractable = TRUE;
			break;
		case 'P':
			passphrase = optarg;
			break;
		case 'a':
			/* this takes an additional argument */
			if(optind > (argc - 1)) {
				result = 2; /* @@@ Return 2 triggers usage message. */
				goto cleanup;
			}
			attrNames  = (char **)realloc(attrNames, (numExtendedAttributes + 1) * sizeof(char *));
			attrValues = (char **)realloc(attrValues, (numExtendedAttributes + 1) * sizeof(char *));
			attrNames[numExtendedAttributes]  = optarg;
			attrValues[numExtendedAttributes] = argv[optind];
			numExtendedAttributes++;
			optind++;
			break;
		case 'A':
			always_allow = TRUE;
			access_specified = TRUE;
			break;
		case 'T':
			if (optarg[0])
			{
				SecTrustedApplicationRef app = NULL;
				OSStatus status = noErr;
				/* check whether the argument specifies an application group */
				const char *groupPrefix = "group://";
				size_t prefixLen = strlen(groupPrefix);
				if (strlen(optarg) > prefixLen && !memcmp(optarg, groupPrefix, prefixLen)) {
					const char *groupName = &optarg[prefixLen];
					if ((status = SecTrustedApplicationCreateApplicationGroup(groupName, NULL, &app)) != noErr) {
						sec_error("SecTrustedApplicationCreateApplicationGroup %s: %s", optarg, sec_errstr(status));
					}
				} else {
					if ((status = SecTrustedApplicationCreateFromPath(optarg, &app)) != noErr) {
						sec_error("SecTrustedApplicationCreateFromPath %s: %s", optarg, sec_errstr(status));
					}
				}

				if (status) {
					result = 1;
					goto cleanup;
				}

				CFArrayAppendValue(trusted_list, app);
				CFRelease(app);
			}
			access_specified = TRUE;
			break;
		case '?':
		default:
			result = 2; /* @@@ Return 2 triggers usage message. */
			goto cleanup;
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
				fprintf(stderr, "Don't know how to wrap in specified format/type\n");
				result = 2; /* @@@ Return 2 triggers usage message. */
				goto cleanup;
		}
	}

	if(kcName) {
		kcRef = keychain_open(kcName);
		if(kcRef == NULL) {
			return 1;
		}
	} else {
		OSStatus status = SecKeychainCopyDefault(&kcRef);
		if (status != noErr || kcRef == NULL) {
			return 1;
		}
	}
	if(readFileSizet(inFile, &inFileData, &inFileLen)) {
		sec_error("Error reading infile %s: %s", inFile, strerror(errno));
		result = 1;
		goto cleanup;
	}
	inData = CFDataCreate(NULL, inFileData, inFileLen);
	if(inData == NULL) {
		result = 1;
		goto cleanup;
	}
	free(inFileData);

	if(access_specified)
	{
		char *accessName = NULL;
		CFStringRef fileStr = CFStringCreateWithCString(NULL, inFile, kCFStringEncodingUTF8);
		if (fileStr) {
			CFURLRef fileURL = CFURLCreateWithFileSystemPath(NULL, fileStr, kCFURLPOSIXPathStyle, FALSE);
			if (fileURL) {
				CFStringRef nameStr = CFURLCopyLastPathComponent(fileURL);
				if (nameStr) {
					CFIndex nameLen = CFStringGetLength(nameStr);
					CFIndex bufLen = 1 + CFStringGetMaximumSizeForEncoding(nameLen, kCFStringEncodingUTF8);
					accessName = (char *)malloc(bufLen);
					if (!CFStringGetCString(nameStr, accessName, bufLen-1, kCFStringEncodingUTF8))
						accessName[0]=0;
					nameLen = strlen(accessName);
					char *p = &accessName[nameLen]; // initially points to terminating null
					while (--nameLen > 0) {
						if (*p == '.') { // strip extension, if any
							*p = '\0';
							break;
						}
						p--;
					}
					safe_CFRelease(&nameStr);
				}
				safe_CFRelease(&fileURL);
			}
			safe_CFRelease(&fileStr);
		}

		result = create_access(accessName, always_allow, trusted_list, &access);

		if (accessName) {
			free(accessName);
		}
		if (result != 0) {
			goto cleanup;
		}
	}

	result = do_keychain_import(kcRef, inData, externFormat, itemType, access,
								nonExtractable, passphrase, inFile, attrNames,
								attrValues, numExtendedAttributes);

cleanup:
	safe_CFRelease(&trusted_list);
	safe_CFRelease(&access);
	safe_CFRelease(&kcRef);
	safe_CFRelease(&inData);

    if(attrNames) {
        free(attrNames);
        attrNames = NULL;
    }
    if(attrValues) {
        free(attrValues);
        attrValues = NULL;
    }

	return result;
}
