/*
 * Copyright (c) 2003-2004,2006,2012,2014 Apple Inc. All Rights Reserved.
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
#include "security_tool.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <Security/SecImportExport.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecKeychainSearch.h>
#include <Security/SecIdentitySearch.h>
#include <Security/SecKey.h>
#include <Security/SecCertificate.h>
#include <Security/SecItem.h>
#include <Security/SecAccessControl.h>
#include <Security/SecAccessControlPriv.h>
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
	size_t len;

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
		int rtn = writeFileSizet(fileName, CFDataGetBytePtr(outData), len);
		if(rtn == 0) {
			if(!do_quiet) {
				fprintf(stderr, "...%lu bytes written to %s\n", len, fileName);
			}
		}
		else {
			sec_error("Error writing to %s: %s", fileName, strerror(errno));
			result = 1;
		}
	}
	else {
		size_t irtn = write(STDOUT_FILENO, CFDataGetBytePtr(outData), len);
		if(irtn != (size_t)len) {
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

typedef struct {
    CFMutableStringRef str;
} ctk_dict2str_context;


static void
ctk_obj_to_str(CFTypeRef obj, char *buf, int bufLen, Boolean key);

static void
ctk_dict2str(const void *key, const void *value, void *context)
{
    char keyBuf[64] = { 0 };
    ctk_obj_to_str(key, keyBuf, sizeof(keyBuf), true);

    char valueBuf[1024] = { 0 };
    ctk_obj_to_str(value, valueBuf, sizeof(valueBuf), false);

    CFStringRef str = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("\n\t\t\t%s : %s,"), keyBuf, valueBuf);
    CFStringAppend(((ctk_dict2str_context *)context)->str, str);
    CFRelease(str);
}

static void
ctk_obj_to_str(CFTypeRef obj, char *buf, int bufLen, Boolean key)
{
    CFStringRef str = NULL;

    if(CFGetTypeID(obj) == CFStringGetTypeID()) {
        // CFStringRef - print the string as is (for keys) or quoted (values)
        str = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, key ? CFSTR("%@") : CFSTR("\"%@\""), obj);
    } else if(CFGetTypeID(obj) == CFNumberGetTypeID()) {
        // CFNumber - print the value using current locale
        CFNumberRef num = (CFNumberRef)obj;

        CFLocaleRef locale = CFLocaleCopyCurrent();
        CFNumberFormatterRef fmt = CFNumberFormatterCreate(kCFAllocatorDefault, locale, kCFNumberFormatterDecimalStyle);
        CFRelease(locale);
        
        str = CFNumberFormatterCreateStringWithNumber(kCFAllocatorDefault, fmt, num);
        CFRelease(fmt);
    } else if(CFGetTypeID(obj) == CFDataGetTypeID()) {
        // CFData - print the data as <hex bytes>
        CFDataRef data = (CFDataRef)obj;

        CFMutableStringRef hexStr = CFStringCreateMutable(kCFAllocatorDefault, CFDataGetLength(data) * 3);
        
        for(int i = 0; i < CFDataGetLength(data); i++) {
            CFStringRef hexByte = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%02x "), *(CFDataGetBytePtr(data) + i));
            CFStringAppend(hexStr, hexByte);
            CFRelease(hexByte);
        }

        // Get rid of the last excessive space.
        if(CFDataGetLength(data)) {
            CFStringDelete(hexStr, CFRangeMake(CFStringGetLength(hexStr) - 1, 1));
        }

        str = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("<%@>"), hexStr);
        CFRelease(hexStr);
    } else if(CFGetTypeID(obj) == CFBooleanGetTypeID()) {
        // CFBoolean - print true/false
        CFBooleanRef cfbool = (CFBooleanRef)obj;

        str = CFStringCreateWithCString(kCFAllocatorDefault, CFBooleanGetValue(cfbool) ? "true" : "false", kCFStringEncodingUTF8);
    } else if(CFGetTypeID(obj) == SecAccessControlGetTypeID()) {
        // SecAccessControlRef - print the constraints dictionary
        SecAccessControlRef ac = (SecAccessControlRef)obj;

        CFDictionaryRef constraints = SecAccessControlGetConstraints(ac);
        CFMutableStringRef constraintsStr = CFStringCreateMutable(kCFAllocatorDefault, 1024);
        if(constraints && CFDictionaryGetCount(constraints)) {
            ctk_dict2str_context context;
            context.str = constraintsStr;
            CFDictionaryApplyFunction(constraints, ctk_dict2str, &context);
            CFStringReplace(constraintsStr, CFRangeMake(CFStringGetLength(constraintsStr) - 1, 1), CFSTR("\n\t\t"));
        }

        CFDictionaryRef protection = SecAccessControlGetProtection(ac);
        CFMutableStringRef protectionStr = CFStringCreateMutable(kCFAllocatorDefault, 512);
        if(protection && CFDictionaryGetCount(protection)) {
            ctk_dict2str_context context;
            context.str = protectionStr;
            CFDictionaryApplyFunction(protection, ctk_dict2str, &context);
            CFStringReplace(protectionStr, CFRangeMake(CFStringGetLength(protectionStr) - 1, 1), CFSTR("\n\t\t"));
        }

        str = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("constraints: {%@}\n\t\tprotection: {%@}"), constraintsStr, protectionStr);
        CFRelease(constraintsStr);
        CFRelease(protectionStr);
    }

    // Fill the provided buffer with the converted string.
    if(str) {
        Boolean success = CFStringGetCString(str, buf, bufLen, kCFStringEncodingUTF8);
        CFRelease(str);

        if(success) {
            return;
        }
    }

    // Use object description as fallback...
    CFStringRef description = CFCopyDescription(obj);
    if(!CFStringGetCString(description, buf, bufLen, kCFStringEncodingUTF8)) {
        // ...or else we don't know.
        strncpy(buf, "<?>", bufLen);
    }

    CFRelease(description);
}

typedef struct {
    int i;
    const char *name;
} ctk_print_context;


static void
ctk_print_dict(const void *key, const void *value, void *context)
{
    char keyBuf[64] = { 0 };
    ctk_obj_to_str(key, keyBuf, sizeof(keyBuf), true);

    char valueBuf[1024] = { 0 };
    ctk_obj_to_str(value, valueBuf, sizeof(valueBuf), false);

    printf("\t%s : %s\n", keyBuf, valueBuf);
}

static void
ctk_dump_item_header(ctk_print_context *ctx)
{
    printf("\n");
    printf("==== %s #%d\n", ctx->name, ctx->i);
}

static void
ctk_dump_item_footer(ctk_print_context *ctx)
{
    printf("====\n");
}

static OSStatus
ctk_dump_item(CFTypeRef item, ctk_print_context *ctx)
{
    OSStatus stat = errSecSuccess;

    CFTypeID tid = CFGetTypeID(item);
    if(tid == CFDictionaryGetTypeID()) {
        // We expect a dictionary containing item attributes.
        ctk_dump_item_header(ctx);
        CFDictionaryApplyFunction((CFDictionaryRef)item, ctk_print_dict, ctx);
        ctk_dump_item_footer(ctx);
    } else {
        stat = errSecInternalComponent;
        printf("Unexpected item type ID: %lu\n", tid);
    }

    return stat;
}

static OSStatus
ctk_dump_items(CFArrayRef items, CFTypeRef secClass, const char *name)
{
    OSStatus stat = errSecSuccess;

    ctk_print_context ctx = { 1, name };

    for(CFIndex i = 0; i < CFArrayGetCount(items); i++) {
        CFTypeRef item = CFArrayGetValueAtIndex(items, i);
        stat = ctk_dump_item(item, &ctx);
        ctx.i++;

        if(stat) {
            break;
        }
    }

    return stat;
}

static OSStatus
ctk_dump(CFTypeRef secClass, const char *name, const char *tid)
{
    OSStatus stat = errSecSuccess;
    CFDictionaryRef query = NULL;
    CFTypeRef result = NULL;

    const void *keys[] = {
        kSecClass,
        kSecMatchLimit,
        kSecAttrAccessGroup,
        kSecReturnAttributes,
    };

    const void *values[] = {
        secClass,
        kSecMatchLimitAll,
        kSecAttrAccessGroupToken,
        kCFBooleanTrue
    };

    // Query attributes of items of the requested secClass.
    query = CFDictionaryCreate(kCFAllocatorDefault,
                               keys,
                               values,
                               sizeof(values) / sizeof(values[0]),
                               &kCFTypeDictionaryKeyCallBacks,
                               &kCFTypeDictionaryValueCallBacks);

    if(tid) {
        CFMutableDictionaryRef updatedQuery = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(query) + 1, query);
        CFStringRef tidStr = CFStringCreateWithCString(kCFAllocatorDefault, tid, kCFStringEncodingUTF8);
        CFDictionaryAddValue(updatedQuery, kSecAttrTokenID, tidStr);
        CFRelease(tidStr);

        CFRelease(query);
        query = updatedQuery;
    }

    stat = SecItemCopyMatching(query, (CFTypeRef *)&result);
    if(stat) {
        if (stat == errSecItemNotFound) {
            fprintf(stderr, "No items found.\n");
        } else {
            sec_error("SecItemCopyMatching: %x (%d) - %s", stat, stat, sec_errstr(stat));
        }
        goto cleanup;
    }

    // We expect an array of dictionaries containing item attributes as result.
    if(CFGetTypeID(result) == CFArrayGetTypeID()) {
        stat = ctk_dump_items((CFArrayRef)result, secClass, name);
    } else {
        stat = errSecInternalComponent;
    }

cleanup:
    if(query) {
        CFRelease(query);
    }

    if(result) {
        CFRelease(result);
    }
    
    return stat;
}

int
ctk_export(int argc, char * const *argv)
{
    OSStatus stat = errSecSuccess;

    ItemSpec itemSpec = IS_All;
    const char *tid = NULL;
    int ch;

    while ((ch = getopt(argc, argv, "i:t:h")) != -1) {
        switch  (ch) {
            case 't':
                if(!strcmp("certs", optarg)) {
                    itemSpec = IS_Certs;
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
            case 'i':
                tid = optarg;
                break;

            case '?':
            default:
                return 2; /* @@@ Return 2 triggers usage message. */
        }
    }

    CFTypeRef classes[] = { kSecClassCertificate, kSecClassKey, kSecClassIdentity };
    const char* names[] = { "certificate", "private key", "identity" };
    ItemSpec specs[] = { IS_Certs, IS_PrivKeys, IS_Identities };

    for(size_t i = 0; i < sizeof(classes)/sizeof(classes[0]); i++) {
        if(specs[i] == itemSpec || itemSpec == IS_All) {
            stat = ctk_dump(classes[i], names[i], tid);
            if(stat) {
                break;
            }
        }
    }

    return stat;
}
