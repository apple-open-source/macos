/*
 * Copyright (c) 2003-2010 Apple Inc. All Rights Reserved.
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
 * keychain_find.c
 */

#include "keychain_find.h"

#include "keychain_utilities.h"
#include "readline.h"
#include "security.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libkern/OSByteOrder.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecKeychainSearch.h>
#include <Security/SecCertificate.h>
#include <CoreFoundation/CFString.h>

// SecDigestGetData, SecKeychainSearchCreateForCertificateByEmail, SecCertificateFindByEmail
#include <Security/SecCertificatePriv.h>

Boolean	gDeleteIt = 0;

//	find_first_generic_password
//
//	Returns a SecKeychainItemRef for the first item
//	which matches the specified attributes. Caller is
//	responsible for releasing the item (with CFRelease).
//
SecKeychainItemRef
find_first_generic_password(CFTypeRef keychainOrArray,
							FourCharCode itemCreator,
							FourCharCode itemType,
							const char *kind,
							const char *value,
							const char *comment,
							const char *label,
							const char *serviceName,
							const char *accountName)
{
	OSStatus status = noErr;
	SecKeychainSearchRef searchRef = NULL;
	SecKeychainItemRef itemRef = NULL;

	SecKeychainAttribute attrs[8]; // maximum number of searchable attributes
	SecKeychainAttributeList attrList = { 0, attrs };
	
	// the primary key for a generic password item (i.e. the combination of
	// attributes which determine whether the item is unique) consists of:
	// { kSecAccountItemAttr, kSecServiceItemAttr }
	//
	// if we have a primary key, we don't need to search on other attributes
	// (and we don't want to, if non-primary attributes are being updated)
	Boolean primaryKey = (accountName && serviceName);
	
	// build the attribute list for searching
	if ((UInt32)itemCreator != 0 && !primaryKey) {
		attrs[attrList.count].tag = kSecCreatorItemAttr;
		attrs[attrList.count].length = sizeof(FourCharCode);
		attrs[attrList.count].data = (FourCharCode *)&itemCreator;
		attrList.count++;
	}
	if ((UInt32)itemType != 0 && !primaryKey) {
		attrs[attrList.count].tag = kSecTypeItemAttr;
		attrs[attrList.count].length = sizeof(FourCharCode);
		attrs[attrList.count].data = (FourCharCode *)&itemType;
		attrList.count++;
	}
	if (kind != NULL && !primaryKey) {
		attrs[attrList.count].tag = kSecDescriptionItemAttr;
		attrs[attrList.count].length = strlen(kind);
		attrs[attrList.count].data = (void*)kind;
		attrList.count++;
	}
	if (value != NULL && !primaryKey) {
		attrs[attrList.count].tag = kSecGenericItemAttr;
		attrs[attrList.count].length = strlen(value);
		attrs[attrList.count].data = (void*)value;
		attrList.count++;
	}
	if (comment != NULL && !primaryKey) {
		attrs[attrList.count].tag = kSecCommentItemAttr;
		attrs[attrList.count].length = strlen(comment);
		attrs[attrList.count].data = (void*)comment;
		attrList.count++;
	}
	if (label != NULL && !primaryKey) {
		attrs[attrList.count].tag = kSecLabelItemAttr;
		attrs[attrList.count].length = strlen(label);
		attrs[attrList.count].data = (void*)label;
		attrList.count++;
	}
	if (serviceName != NULL) {
		attrs[attrList.count].tag = kSecServiceItemAttr;
		attrs[attrList.count].length = strlen(serviceName);
		attrs[attrList.count].data = (void*)serviceName;
		attrList.count++;
	}
	if (accountName != NULL) {
		attrs[attrList.count].tag = kSecAccountItemAttr;
		attrs[attrList.count].length = strlen(accountName);
		attrs[attrList.count].data = (void*)accountName;
		attrList.count++;
	}
	
	status = SecKeychainSearchCreateFromAttributes(keychainOrArray, kSecGenericPasswordItemClass, &attrList, &searchRef);
	if (status) {
		sec_perror("SecKeychainSearchCreateFromAttributes", status);
		goto cleanup;
	}
	// we're only interested in the first match, if there is a match at all
	status = SecKeychainSearchCopyNext(searchRef, &itemRef);
	if (status) {
		itemRef = NULL;
	}

cleanup:
	if (searchRef)
		CFRelease(searchRef);
	
	return itemRef;
}

//	find_first_internet_password
//
//	Returns a SecKeychainItemRef for the first item
//	which matches the specified attributes. Caller is
//	responsible for releasing the item (with CFRelease).
//
SecKeychainItemRef
find_first_internet_password(CFTypeRef keychainOrArray,
	 FourCharCode itemCreator,
	 FourCharCode itemType,
	 const char *kind,
	 const char *comment,
	 const char *label,
	 const char *serverName,
	 const char *securityDomain,
	 const char *accountName,
	 const char *path,
	 UInt16 port,
	 SecProtocolType protocol,
	 SecAuthenticationType authenticationType)
{
	OSStatus status = noErr;
	SecKeychainSearchRef searchRef = NULL;
	SecKeychainItemRef itemRef = NULL;
	
	SecKeychainAttribute attrs[12]; // maximum number of searchable attributes
	SecKeychainAttributeList attrList = { 0, attrs };
			  
	// the primary key for an internet password item (i.e. the combination of
	// attributes which determine whether the item is unique) consists of:
	// { kSecAccountItemAttr, kSecSecurityDomainItemAttr, kSecServerItemAttr,
	//   kSecProtocolItemAttr, kSecAuthenticationTypeItemAttr,
	//   kSecPortItemAttr, kSecPathItemAttr }
	//
	// if we have a primary key, we don't need to search on other attributes.
	// (and we don't want to, if non-primary attributes are being updated)
	Boolean primaryKey = (accountName && securityDomain && serverName &&
						  protocol && authenticationType && port && path);
	
	// build the attribute list for searching
	if ((UInt32)itemCreator != 0 && !primaryKey) {
		attrs[attrList.count].tag = kSecCreatorItemAttr;
		attrs[attrList.count].length = sizeof(FourCharCode);
		attrs[attrList.count].data = (FourCharCode *)&itemCreator;
		attrList.count++;
	}
	if ((UInt32)itemType != 0 && !primaryKey) {
		attrs[attrList.count].tag = kSecTypeItemAttr;
		attrs[attrList.count].length = sizeof(FourCharCode);
		attrs[attrList.count].data = (FourCharCode *)&itemType;
		attrList.count++;
	}
	if (kind != NULL && !primaryKey) {
		attrs[attrList.count].tag = kSecDescriptionItemAttr;
		attrs[attrList.count].length = strlen(kind);
		attrs[attrList.count].data = (void*)kind;
		attrList.count++;
	}
	if (comment != NULL && !primaryKey) {
		attrs[attrList.count].tag = kSecCommentItemAttr;
		attrs[attrList.count].length = strlen(comment);
		attrs[attrList.count].data = (void*)comment;
		attrList.count++;
	}
	if (label != NULL && !primaryKey) {
		attrs[attrList.count].tag = kSecLabelItemAttr;
		attrs[attrList.count].length = strlen(label);
		attrs[attrList.count].data = (void*)label;
		attrList.count++;
	}
	if (serverName != NULL) {
		attrs[attrList.count].tag = kSecServerItemAttr;
		attrs[attrList.count].length = strlen(serverName);
		attrs[attrList.count].data = (void*)serverName;
		attrList.count++;
	}
	if (securityDomain != NULL) {
		attrs[attrList.count].tag = kSecSecurityDomainItemAttr;
		attrs[attrList.count].length = strlen(securityDomain);
		attrs[attrList.count].data = (void *)securityDomain;
		attrList.count++;
	}
	if (accountName != NULL) {
		attrs[attrList.count].tag = kSecAccountItemAttr;
		attrs[attrList.count].length = strlen(accountName);
		attrs[attrList.count].data = (void *)accountName;
		attrList.count++;
	}
	if (path != NULL) {
		attrs[attrList.count].tag = kSecPathItemAttr;
		attrs[attrList.count].length = strlen(path);
		attrs[attrList.count].data = (void *)path;
		attrList.count++;
	}
	if (port != 0) {
		attrs[attrList.count].tag = kSecPortItemAttr;
		attrs[attrList.count].length = sizeof(UInt16);
		attrs[attrList.count].data = (UInt16 *)&port;
		attrList.count++;
	}
	if ((UInt32)protocol != 0) {
		attrs[attrList.count].tag = kSecProtocolItemAttr;
		attrs[attrList.count].length = sizeof(SecProtocolType);
		attrs[attrList.count].data = (SecProtocolType *)&protocol;
		attrList.count++;
	}
	if ((UInt32)authenticationType != 0) {
		attrs[attrList.count].tag = kSecAuthenticationTypeItemAttr;
		attrs[attrList.count].length = sizeof(SecAuthenticationType);
		attrs[attrList.count].data = (SecAuthenticationType *)&authenticationType;
		attrList.count++;
	}
	
	status = SecKeychainSearchCreateFromAttributes(keychainOrArray, kSecInternetPasswordItemClass, &attrList, &searchRef);
	if (status) {
		sec_perror("SecKeychainSearchCreateFromAttributes", status);
		goto cleanup;
	}
	// we're only interested in the first match, if there is a match at all
	status = SecKeychainSearchCopyNext(searchRef, &itemRef);
	if (status) {
		itemRef = NULL;
	}
	
cleanup:
	if (searchRef)
		CFRelease(searchRef);
	
	return itemRef;
}

//	find_unique_certificate
//
//	Returns a SecKeychainItemRef for the certificate
//	in the specified keychain (or keychain list)
//	which is a unique match for either the specified name
//	or SHA-1 hash. If more than one match exists, the
//	certificate is not unique and none are returned. Caller is
//	responsible for releasing the item (with CFRelease).
//
SecKeychainItemRef
find_unique_certificate(CFTypeRef keychainOrArray,
	const char *name,
	const char *hash)
{
	OSStatus status = noErr;
	SecKeychainSearchRef searchRef = NULL;
	SecKeychainItemRef uniqueItemRef = NULL;
	
	status = SecKeychainSearchCreateFromAttributes(keychainOrArray, kSecCertificateItemClass, NULL, &searchRef);
	if (status) {
		return uniqueItemRef;
	}
	
	// check input hash string and convert to data
	CSSM_DATA hashData = { 0, NULL };
	if (hash) {
		CSSM_SIZE len = strlen(hash)/2;
		hashData.Length = len;
		hashData.Data = (uint8 *)malloc(hashData.Length);
		fromHex(hash, &hashData);
	}
	
	// filter candidates against the hash (or the name, if no hash provided)
	CFStringRef matchRef = (name) ? CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8) : NULL;
	Boolean exactMatch = FALSE;
	
	CSSM_DATA certData = { 0, NULL };
	SecKeychainItemRef candidate = NULL;
	
	while (SecKeychainSearchCopyNext(searchRef, &candidate) == noErr) {
		SecCertificateRef cert = (SecCertificateRef)candidate;
		if (SecCertificateGetData(cert, &certData) != noErr) {
			safe_CFRelease(&candidate);
			continue;
		}
		if (hash) {
			uint8 candidate_sha1_hash[20];
			CSSM_DATA digest;
			digest.Length = sizeof(candidate_sha1_hash);
			digest.Data = candidate_sha1_hash;
			if ((SecDigestGetData(CSSM_ALGID_SHA1, &digest, &certData) == CSSM_OK) &&
				(hashData.Length == digest.Length) &&
				(!memcmp(hashData.Data, digest.Data, digest.Length))) {
				exactMatch = TRUE;
				uniqueItemRef = candidate; // currently retained
				break; // we're done - can't get more exact than this
			}
		} else {
			// copy certificate name
			CFStringRef nameRef = NULL;
			if (SecCertificateCopyCommonName(cert, &nameRef) != noErr) {
				safe_CFRelease(&candidate);
				continue; // no name, so no match is possible
			}
			CFIndex nameLen = CFStringGetLength(nameRef);
			CFIndex bufLen = 1 + CFStringGetMaximumSizeForEncoding(nameLen, kCFStringEncodingUTF8);
			char *nameBuf = (char *)malloc(bufLen);
			if (!CFStringGetCString(nameRef, nameBuf, bufLen-1, kCFStringEncodingUTF8))
				nameBuf[0]=0;
			
			CFRange find = { kCFNotFound, 0 };
			if (nameRef && matchRef)
				find = CFStringFind(nameRef, matchRef, kCFCompareCaseInsensitive | kCFCompareNonliteral);
			Boolean isExact = (find.location == 0 && find.length == nameLen);
			if (find.location == kCFNotFound) {
				free(nameBuf);
				safe_CFRelease(&nameRef);
				safe_CFRelease(&candidate);
				continue; // no match
			}
			if (uniqueItemRef) {	// got two matches
				if (exactMatch && !isExact)	{	// prior is better; ignore this one
					free(nameBuf);
					safe_CFRelease(&nameRef);
					safe_CFRelease(&candidate);
					continue;
				}
				if (exactMatch == isExact) {	// same class of match
					if (CFEqual(uniqueItemRef, candidate)) {	// same certificate
						free(nameBuf);
						safe_CFRelease(&nameRef);
						safe_CFRelease(&candidate);
						continue;
					}
					// ambiguity - must fail
					sec_error("\"%s\" is ambiguous, matches more than one certificate", name);
					free(nameBuf);
					safe_CFRelease(&nameRef);
					safe_CFRelease(&candidate);
					safe_CFRelease(&uniqueItemRef);
					break;
				}
				safe_CFRelease(&uniqueItemRef); // about to replace with this one
			}
			uniqueItemRef = candidate; // currently retained
			exactMatch = isExact;
			free(nameBuf);
			safe_CFRelease(&nameRef);
		}
	}
	
	safe_CFRelease(&searchRef);
	safe_CFRelease(&matchRef);
	if (hashData.Data) {
		free(hashData.Data);
	}
	
	return uniqueItemRef;
}

static int
do_keychain_find_generic_password(CFTypeRef keychainOrArray,
	FourCharCode itemCreator,
	FourCharCode itemType,
	const char *kind,
	const char *value,
	const char *comment,
	const char *label,
	const char *serviceName,
	const char *accountName,
	Boolean get_password)
{
	OSStatus result = noErr;
    SecKeychainItemRef itemRef = NULL;
	void *passwordData = NULL;
    UInt32 passwordLength = 0;

	itemRef = find_first_generic_password(keychainOrArray,
										  itemCreator,
										  itemType,
										  kind,
										  value,
										  comment,
										  label,
										  serviceName,
										  accountName);
	if (!itemRef) {
		result = errSecItemNotFound;
		sec_perror("SecKeychainSearchCopyNext", result);
		goto cleanup;
	}

	if (get_password) {
		result = SecKeychainItemCopyContent(itemRef, NULL, NULL, &passwordLength, &passwordData);
	}

	print_keychain_item_attributes(stdout, itemRef, FALSE, FALSE, FALSE, FALSE);

	if (get_password)
	{
		fputs("password: ", stderr);
		print_buffer(stderr, passwordLength, passwordData); 
		fputc('\n', stderr);
	}

cleanup:
	if (passwordData)
		SecKeychainItemFreeContent(NULL, passwordData);
	if (itemRef)
		CFRelease(itemRef);

	return result;
}

static int
do_keychain_delete_generic_password(CFTypeRef keychainOrArray,
	FourCharCode itemCreator,
	FourCharCode itemType,
	const char *kind,
	const char *value,
	const char *comment,
	const char *label,
	const char *serviceName,
	const char *accountName)
{
	OSStatus result = noErr;
    SecKeychainItemRef itemRef = NULL;
	void *passwordData = NULL;
	
	itemRef = find_first_generic_password(keychainOrArray,
										  itemCreator,
										  itemType,
										  kind,
										  value,
										  comment,
										  label,
										  serviceName,
										  accountName);
	if (!itemRef) {
		result = errSecItemNotFound;
		sec_perror("SecKeychainSearchCopyNext", result);
		goto cleanup;
	}
	
	print_keychain_item_attributes(stdout, itemRef, FALSE, FALSE, FALSE, FALSE);

	result = SecKeychainItemDelete(itemRef);
	
	fputs("password has been deleted.\n", stderr);

cleanup:
	if (passwordData)
		SecKeychainItemFreeContent(NULL, passwordData);
	if (itemRef)
		CFRelease(itemRef);
	
	return result;
}

static int
do_keychain_find_internet_password(CFTypeRef keychainOrArray,
	FourCharCode itemCreator,
	FourCharCode itemType,
	const char *kind,
	const char *comment,
	const char *label,
	const char *serverName,
	const char *securityDomain,
	const char *accountName,
	const char *path,
	UInt16 port,
	SecProtocolType protocol,
	SecAuthenticationType authenticationType,
	Boolean get_password)
{
	OSStatus result = noErr;
    SecKeychainItemRef itemRef = NULL;
	void *passwordData = NULL;
    UInt32 passwordLength = 0;
	
	itemRef = find_first_internet_password(keychainOrArray,
										   itemCreator,
										   itemType,
										   kind,
										   comment,
										   label,
										   serverName,
										   securityDomain,
										   accountName,
										   path,
										   port,
										   protocol,
										   authenticationType);
	if (!itemRef) {
		result = errSecItemNotFound;
		sec_perror("SecKeychainSearchCopyNext", result);
		goto cleanup;
	}
	
	if (get_password) {
		result = SecKeychainItemCopyContent(itemRef, NULL, NULL, &passwordLength, &passwordData);
	}
	
	print_keychain_item_attributes(stdout, itemRef, FALSE, FALSE, FALSE, FALSE);
	
	if (get_password)
	{
		fputs("password: ", stderr);
		print_buffer(stderr, passwordLength, passwordData); 
		fputc('\n', stderr);
	}
	
cleanup:
	if (passwordData)
		SecKeychainItemFreeContent(NULL, passwordData);
	if (itemRef)
		CFRelease(itemRef);
	
	return result;
}

static int
do_keychain_delete_internet_password(CFTypeRef keychainOrArray,
	FourCharCode itemCreator,
	FourCharCode itemType,
	const char *kind,
	const char *comment,
	const char *label,
	const char *serverName,
	const char *securityDomain,
	const char *accountName,
	const char *path,
	UInt16 port,
	SecProtocolType protocol,
	SecAuthenticationType authenticationType)
{
	OSStatus result = noErr;
    SecKeychainItemRef itemRef = NULL;
	void *passwordData = NULL;

	itemRef = find_first_internet_password(keychainOrArray,
										   itemCreator,
										   itemType,
										   kind,
										   comment,
										   label,
										   serverName,
										   securityDomain,
										   accountName,
										   path,
										   port,
										   protocol,
										   authenticationType);
	if (!itemRef) {
		result = errSecItemNotFound;
		sec_perror("SecKeychainSearchCopyNext", result);
		goto cleanup;
	}
	
	print_keychain_item_attributes(stdout, itemRef, FALSE, FALSE, FALSE, FALSE);

	result = SecKeychainItemDelete(itemRef);

	fputs("password has been deleted.\n", stderr);

cleanup:
	if (passwordData)
		SecKeychainItemFreeContent(NULL, passwordData);
	if (itemRef)
		CFRelease(itemRef);

	return result;
}

static int
do_keychain_find_certificate(CFTypeRef keychainOrArray,
	const char *name,
	const char *emailAddress,
	Boolean print_hash,
	Boolean output_pem,
	Boolean find_all,
	Boolean print_email)
{
	OSStatus result = noErr;
    SecCertificateRef certificateRef = NULL;
	SecKeychainSearchRef searchRef = NULL;
	CFStringRef matchRef = (name) ? CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8) : NULL;

	if (find_all && emailAddress) {
		result = SecKeychainSearchCreateForCertificateByEmail(keychainOrArray, emailAddress, &searchRef);
		if (result) {
			sec_perror("SecKeychainSearchCreateForCertificateByEmail", result);
			goto cleanup;
		}
	} else {
		result = SecKeychainSearchCreateFromAttributes(keychainOrArray, kSecCertificateItemClass, NULL, &searchRef);
		if (result) {
			sec_perror("SecKeychainSearchCreateFromAttributes", result);
			goto cleanup;
		}
	}

	do
	{
		if (find_all) {
			SecKeychainItemRef itemRef = NULL;
			result = SecKeychainSearchCopyNext(searchRef, &itemRef);
			if (result == errSecItemNotFound) {
				result = 0;
				break;
			}
			else if (result) {
				sec_perror("SecKeychainSearchCopyNext", result);
				goto cleanup;
			}

			if (!emailAddress && name) {
				// match name in common name
				CFStringRef nameRef = NULL;
				if (SecCertificateCopyCommonName((SecCertificateRef)itemRef, &nameRef) != noErr) {
					safe_CFRelease(&itemRef);
					continue; // no name, so no match is possible
				}
				CFRange find = { kCFNotFound, 0 };
				if (nameRef && matchRef)
					find = CFStringFind(nameRef, matchRef, kCFCompareCaseInsensitive | kCFCompareNonliteral);
				if (find.location == kCFNotFound) {
					safe_CFRelease(&nameRef);
					safe_CFRelease(&itemRef);
					continue; // no match
				}					
				safe_CFRelease(&nameRef);
			}
			safe_CFRelease(&certificateRef);
			certificateRef = (SecCertificateRef) itemRef;
		}
		else { // only want the first match
			if (emailAddress) {
				result = SecCertificateFindByEmail(keychainOrArray, emailAddress, &certificateRef);
				if (result) {
					sec_perror("SecCertificateFindByEmail", result);
					goto cleanup;
				}
			} else {
				SecKeychainItemRef itemRef = NULL;
				while ((result = SecKeychainSearchCopyNext(searchRef, &itemRef)) != errSecItemNotFound) {
					if (name) {
						// match name in common name
						CFStringRef nameRef = NULL;
						if (SecCertificateCopyCommonName((SecCertificateRef)itemRef, &nameRef) != noErr) {
							safe_CFRelease(&itemRef);
							continue; // no name, so no match is possible
						}
						CFRange find = { kCFNotFound, 0 };
						if (nameRef && matchRef)
							find = CFStringFind(nameRef, matchRef, kCFCompareCaseInsensitive | kCFCompareNonliteral);
						if (find.location == kCFNotFound) {
							safe_CFRelease(&nameRef);
							safe_CFRelease(&itemRef);
							continue; // no match
						}					
						safe_CFRelease(&nameRef);
					}
					break; // we have a match!
				}
				if (result == errSecItemNotFound) {
					sec_perror("SecKeychainSearchCopyNext", result);
					goto cleanup;
				}
				certificateRef = (SecCertificateRef) itemRef;
			}
		}

		// process the found certificate

		if (print_hash) {
			uint8 sha1_hash[20];
			CSSM_DATA data;
			CSSM_DATA digest;
			digest.Length = sizeof(sha1_hash);
			digest.Data = sha1_hash;
			if ((SecCertificateGetData(certificateRef, &data) == noErr) &&
				(SecDigestGetData(CSSM_ALGID_SHA1, &digest, &data) == CSSM_OK)) {
				unsigned int i;
				uint32 len = digest.Length;
				uint8 *cp = digest.Data;
				fprintf(stdout, "SHA-1 hash: ");
				for(i=0; i<len; i++) {
					fprintf(stdout, "%02X", ((unsigned char *)cp)[i]);
				}
				fprintf(stdout, "\n");
			}
		}
		
		if (print_email)
		{
			CFArrayRef emailAddresses = NULL;
			CFIndex ix, count;
			result = SecCertificateCopyEmailAddresses(certificateRef, &emailAddresses);
			if (result)
			{
				sec_perror("SecCertificateCopyEmailAddresses", result);
				goto cleanup;
			}

			count = CFArrayGetCount(emailAddresses);
			fputs("email addresses: ", stdout);
			for (ix = 0; ix < count; ++ix)
			{
				CFStringRef emailAddress = (CFStringRef)CFArrayGetValueAtIndex(emailAddresses, ix);
				const char *addr;
				char buffer[256];

				if (ix)
					fputs(", ", stdout);

				addr = CFStringGetCStringPtr(emailAddress, kCFStringEncodingUTF8);
				if (!addr)
				{
					if (CFStringGetCString(emailAddress, buffer, sizeof(buffer), kCFStringEncodingUTF8))
						addr = buffer;
				}

				fprintf(stdout, "%s", addr);
			}
			fputc('\n', stdout);

			CFRelease(emailAddresses);
		}

		if (output_pem)
		{
			CSSM_DATA certData = {};
			result = SecCertificateGetData(certificateRef, &certData);
			if (result)
			{
				sec_perror("SecCertificateGetData", result);
				goto cleanup;
			}
	
			print_buffer_pem(stdout, "CERTIFICATE", certData.Length, certData.Data);
		}
		else
		{
			print_keychain_item_attributes(stdout, (SecKeychainItemRef)certificateRef, FALSE, FALSE, FALSE, FALSE);
		}
	} while (find_all);

cleanup:
	safe_CFRelease(&searchRef);
	safe_CFRelease(&certificateRef);
	safe_CFRelease(&matchRef);

	return result;
}

int
keychain_delete_internet_password(int argc, char * const *argv)
{
	char *serverName = NULL, *securityDomain = NULL, *accountName = NULL, *path = NULL;
	char *kind = NULL, *label = NULL, *comment = NULL;
	FourCharCode itemCreator = 0, itemType = 0;
    UInt16 port = 0;
    SecProtocolType protocol = 0;
    SecAuthenticationType authenticationType = 0;
	CFTypeRef keychainOrArray = NULL;
	int ch, result = 0;
	
	/*
	 *	"    -a  Match \"account\" string\n"
	 *	"    -c  Match \"creator\" (four-character code)\n"
	 *	"    -C  Match \"type\" (four-character code)\n"
	 *	"    -d  Match \"securityDomain\" string\n"
	 *	"    -D  Match \"kind\" string\n"
	 *	"    -j  Match \"comment\" string\n"
	 *	"    -l  Match \"label\" string\n"
	 *	"    -p  Match \"path\" string\n"
	 *	"    -P  Match port number\n"
	 *	"    -r  Match \"protocol\" (four-character code)\n"
	 *	"    -s  Match \"server\" string\n"
	 *	"    -t  Match \"authenticationType\" (four-character code)\n"
	 */
	
	while ((ch = getopt(argc, argv, "ha:c:C:d:D:hgj:l:p:P:r:s:t:")) != -1)
	{
		switch (ch)
		{
			case 'a':
				accountName = optarg;
				break;
			case 'c':
				result = parse_fourcharcode(optarg, &itemCreator);
				if (result) goto cleanup;
				break;
			case 'C':
				result = parse_fourcharcode(optarg, &itemType);
				if (result) goto cleanup;
				break;
			case 'd':
				securityDomain = optarg;
				break;
			case 'D':
				kind = optarg;
				break;
			case 'j':
				comment = optarg;
				break;
			case 'l':
				label = optarg;
				break;
			case 'p':
				path = optarg;
				break;
			case 'P':
				port = atoi(optarg);
				break;
			case 'r':
				result = parse_fourcharcode(optarg, &protocol);
				if (result) goto cleanup;
				break;
			case 's':
				serverName = optarg;
				break;
			case 't':
				result = parse_fourcharcode(optarg, &authenticationType);
				if (result) goto cleanup;
				break;
			case '?':
			default:
				result = 2; /* @@@ Return 2 triggers usage message. */
				goto cleanup;
		}
	}
	
	argc -= optind;
	argv += optind;
	
    keychainOrArray = keychain_create_array(argc, argv);
	
	result = do_keychain_delete_internet_password(keychainOrArray,
												itemCreator,
												itemType,
												kind,
												comment,
												label,
												serverName,
												securityDomain,
												accountName,
												path,
												port,
												protocol,
												authenticationType);
cleanup:
	if (keychainOrArray)
		CFRelease(keychainOrArray);
	
	return result;
}

int
keychain_find_internet_password(int argc, char * const *argv)
{
	char *serverName = NULL, *securityDomain = NULL, *accountName = NULL, *path = NULL;
	char *kind = NULL, *label = NULL, *comment = NULL;
	FourCharCode itemCreator = 0, itemType = 0;
    UInt16 port = 0;
    SecProtocolType protocol = 0;
    SecAuthenticationType authenticationType = 0;
	CFTypeRef keychainOrArray = NULL;
	int ch, result = 0;
	Boolean get_password = FALSE;

	/*
	 *	"    -a  Match \"account\" string\n"
	 *	"    -c  Match \"creator\" (four-character code)\n"
	 *	"    -C  Match \"type\" (four-character code)\n"
	 *	"    -d  Match \"securityDomain\" string\n"
	 *	"    -D  Match \"kind\" string\n"
	 *	"    -j  Match \"comment\" string\n"
	 *	"    -l  Match \"label\" string\n"
	 *	"    -p  Match \"path\" string\n"
	 *	"    -P  Match port number\n"
	 *	"    -r  Match \"protocol\" (four-character code)\n"
	 *	"    -s  Match \"server\" string\n"
	 *	"    -t  Match \"authenticationType\" (four-character code)\n"
	 *	"    -g  Display the password for the item found\n"
	 */

	while ((ch = getopt(argc, argv, "ha:c:C:d:D:hgj:l:p:P:r:s:t:")) != -1)
	{
		switch (ch)
		{
        case 'a':
            accountName = optarg;
            break;
		case 'c':
			result = parse_fourcharcode(optarg, &itemCreator);
			if (result) goto cleanup;
			break;
		case 'C':
			result = parse_fourcharcode(optarg, &itemType);
			if (result) goto cleanup;
			break;
		case 'd':
			securityDomain = optarg;
			break;
		case 'D':
			kind = optarg;
			break;
		case 'j':
			comment = optarg;
			break;
		case 'l':
			label = optarg;
			break;
		case 'g':
			get_password = TRUE;
			break;
        case 'p':
            path = optarg;
            break;
        case 'P':
            port = atoi(optarg);
            break;
        case 'r':
			result = parse_fourcharcode(optarg, &protocol);
			if (result) goto cleanup;
			break;
		case 's':
			serverName = optarg;
			break;
        case 't':
			result = parse_fourcharcode(optarg, &authenticationType);
			if (result) goto cleanup;
			break;
        case '?':
		default:
			result = 2; /* @@@ Return 2 triggers usage message. */
			goto cleanup;
		}
	}
  
	argc -= optind;
	argv += optind;

    keychainOrArray = keychain_create_array(argc, argv);

	result = do_keychain_find_internet_password(keychainOrArray,
												itemCreator,
												itemType,
												kind,
												comment,
												label,
												serverName,
												securityDomain,
												accountName,
												path,
												port,
												protocol,
												authenticationType,
												get_password);
cleanup:
	if (keychainOrArray)
		CFRelease(keychainOrArray);

	return result;
}

int
keychain_delete_generic_password(int argc, char * const *argv)
{
	char *serviceName = NULL, *accountName = NULL;
	char *kind = NULL, *label = NULL, *value = NULL, *comment = NULL;
	FourCharCode itemCreator = 0, itemType = 0;
	CFTypeRef keychainOrArray = nil;
	int ch, result = 0;
	
	/*
	 *	"    -a  Match \"account\" string\n"
	 *	"    -c  Match \"creator\" (four-character code)\n"
	 *	"    -C  Match \"type\" (four-character code)\n"
	 *	"    -D  Match \"kind\" string\n"
	 *	"    -G  Match \"value\" string (generic attribute)\n"
	 *	"    -j  Match \"comment\" string\n"
	 *	"    -l  Match \"label\" string\n"
	 *	"    -s  Match \"service\" string\n"
	 */
	
	while ((ch = getopt(argc, argv, "ha:c:C:D:G:j:l:s:g")) != -1)
	{
		switch  (ch)
		{
			case 'a':
				accountName = optarg;
				break;
			case 'c':
				result = parse_fourcharcode(optarg, &itemCreator);
				if (result) goto cleanup;
				break;
			case 'C':
				result = parse_fourcharcode(optarg, &itemType);
				if (result) goto cleanup;
				break;
			case 'D':
				kind = optarg;
				break;
			case 'G':
				value = optarg;
				break;
			case 'j':
				comment = optarg;
				break;
			case 'l':
				label = optarg;
				break;
			case 's':
				serviceName = optarg;
				break;
			case '?':
			default:
				result = 2; /* @@@ Return 2 triggers usage message. */
				goto cleanup;
		}
	}
	
	argc -= optind;
	argv += optind;
	
    keychainOrArray = keychain_create_array(argc, argv);
	
	result = do_keychain_delete_generic_password(keychainOrArray,
											   itemCreator,
											   itemType,
											   kind,
											   value,
											   comment,
											   label,
											   serviceName,
											   accountName);
cleanup:
	if (keychainOrArray)
		CFRelease(keychainOrArray);
	
	return result;
}

int
keychain_find_generic_password(int argc, char * const *argv)
{
	char *serviceName = NULL, *accountName = NULL;
	char *kind = NULL, *label = NULL, *value = NULL, *comment = NULL;
	FourCharCode itemCreator = 0, itemType = 0;
	CFTypeRef keychainOrArray = nil;
	int ch, result = 0;
	Boolean get_password = FALSE;

	/*
	 *	"    -a  Match \"account\" string\n"
	 *	"    -c  Match \"creator\" (four-character code)\n"
	 *	"    -C  Match \"type\" (four-character code)\n"
	 *	"    -D  Match \"kind\" string\n"
	 *	"    -G  Match \"value\" string (generic attribute)\n"
	 *	"    -j  Match \"comment\" string\n"
	 *	"    -l  Match \"label\" string\n"
	 *	"    -s  Match \"service\" string\n"
	 *	"    -g  Display the password for the item(s) found\n"
	 */

	while ((ch = getopt(argc, argv, "ha:c:C:D:G:j:l:s:g")) != -1)
	{
		switch  (ch)
		{
        case 'a':
            accountName = optarg;
            break;
		case 'c':
			result = parse_fourcharcode(optarg, &itemCreator);
			if (result) goto cleanup;
			break;
		case 'C':
			result = parse_fourcharcode(optarg, &itemType);
			if (result) goto cleanup;
			break;
		case 'D':
			kind = optarg;
			break;
		case 'G':
			value = optarg;
			break;
		case 'j':
			comment = optarg;
			break;
		case 'l':
			label = optarg;
			break;
		case 's':
			serviceName = optarg;
			break;
		case 'g':
			get_password = TRUE;
			break;
		case '?':
		default:
			result = 2; /* @@@ Return 2 triggers usage message. */
			goto cleanup;
		}
	}
  
	argc -= optind;
	argv += optind;

    keychainOrArray = keychain_create_array(argc, argv);

	result = do_keychain_find_generic_password(keychainOrArray,
											   itemCreator,
											   itemType,
											   kind,
											   value,
											   comment,
											   label,
											   serviceName,
											   accountName,
											   get_password);
cleanup:
	if (keychainOrArray)
		CFRelease(keychainOrArray);

	return result;
}


int
keychain_find_certificate(int argc, char * const *argv)
{
	char *emailAddress = NULL;
	char *name = NULL;
	int ch, result = 0;
	CFTypeRef keychainOrArray = nil;
	Boolean output_pem = FALSE;
	Boolean find_all = FALSE;
	Boolean print_hash = FALSE;
	Boolean print_email = FALSE;

	while ((ch = getopt(argc, argv, "hac:e:mpZ")) != -1)
	{
		switch  (ch)
		{
        case 'a':
            find_all = TRUE;
            break;
		case 'c':
			name = optarg;
			break;
		case 'e':
            emailAddress = optarg;
            break;
        case 'm':
            print_email = TRUE;
            break;
        case 'p':
            output_pem = TRUE;
            break;
		case 'Z':
			print_hash = TRUE;
			break;
        case '?':
		default:
			result = 2; /* @@@ Return 2 triggers usage message. */
			goto cleanup;
		}
	}

	argc -= optind;
	argv += optind;

    keychainOrArray = keychain_create_array(argc, argv);

	result = do_keychain_find_certificate(keychainOrArray, name, emailAddress, print_hash, output_pem, find_all, print_email);

cleanup:
	if (keychainOrArray)
		CFRelease(keychainOrArray);

	return result;
}


static int
do_keychain_dump_class(FILE *stream, CFTypeRef keychainOrArray, SecItemClass itemClass, Boolean show_data, Boolean show_raw_data, Boolean show_acl, Boolean interactive)
{
	SecKeychainItemRef item;
	SecKeychainSearchRef search = NULL;
	int result = 0;
	OSStatus status;

	status = SecKeychainSearchCreateFromAttributes(keychainOrArray, itemClass, NULL, &search);
	if (status)
	{
		sec_perror("SecKeychainSearchCreateFromAttributes", status);
		result = 1;
		goto cleanup;
	}

	while ((status = SecKeychainSearchCopyNext(search, &item)) == 0)
	{
		print_keychain_item_attributes(stream, item, show_data, show_raw_data, show_acl, interactive);
		CFRelease(item);
	}

	if (status != errSecItemNotFound)
	{
		sec_perror("SecKeychainSearchCopyNext", status);
		result = 1;
		goto cleanup;
	}

cleanup:
	if (search)
		CFRelease(search);

	return result;
}

static int
do_keychain_dump(FILE *stream, CFTypeRef keychainOrArray, Boolean show_data, Boolean show_raw_data, Boolean show_acl, Boolean interactive)
{
	return do_keychain_dump_class(stream, keychainOrArray, CSSM_DL_DB_RECORD_ANY, show_data, show_raw_data, show_acl, interactive);
}

int
keychain_dump(int argc, char * const *argv)
{
	int ch, result = 0;
	Boolean show_data = FALSE, show_raw_data = FALSE, show_acl = FALSE, interactive = FALSE;
	CFTypeRef keychainOrArray = NULL;
	const char *outputFilename = NULL;
	FILE *output;

	while ((ch = getopt(argc, argv, "adhiro:")) != -1)
	{
		switch  (ch)
		{
		case 'a':
			show_acl = TRUE;
			break;
		case 'd':
			show_data = TRUE;
			break;
		case 'i':
			show_acl = TRUE;
			interactive = TRUE;
			break;
		case 'r':
			show_raw_data = TRUE;
			break;
		case 'o':
			outputFilename = optarg;
			break;
        case '?':
		default:
			return 2; /* @@@ Return 2 triggers usage message. */
		}
	}
  
	argc -= optind;
	argv += optind;

    keychainOrArray = keychain_create_array(argc, argv);

	if (outputFilename)
		output = fopen(outputFilename, "w");
	else
		output = stdout;

	result = do_keychain_dump(output, keychainOrArray, show_data, show_raw_data, show_acl, interactive);

	if (outputFilename)
		fclose(output);

	if (keychainOrArray)
		CFRelease(keychainOrArray);

	return result;
}
