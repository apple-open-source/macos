/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 *  keychain_find.c
 *  security
 *
 *  Created by Michael Brouwer on Thu June 5 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#include "keychain_find.h"

#include "keychain_utilities.h"
#include "readline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecKeychainSearch.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <CoreFoundation/CFString.h>

static int
do_keychain_find_generic_password(CFTypeRef keychainOrArray, const char *serverName, const char *accountName, Boolean get_password)
 {
	OSStatus result;
    SecKeychainItemRef itemRef = NULL;
	void *passwordData = NULL;
    UInt32 passwordLength = 0;
  
	result = SecKeychainFindGenericPassword(keychainOrArray,
		serverName ? strlen(serverName) : 0,
		serverName,
		accountName ? strlen(accountName) : 0,
		accountName, 
		get_password ? &passwordLength : 0,
		get_password ? &passwordData : NULL,
		&itemRef);
	if (result)
	{
		fprintf(stderr, "SecKeychainFindGenericPassword returned %ld(0x%lx)\n", result, result);
		goto loser;
	}

	print_keychain_item_attributes(stderr, itemRef, FALSE, FALSE);

	if (get_password)
	{
		fputs("password: ", stderr);
		print_buffer(stderr, passwordLength, passwordData); 
		fputc('\n', stderr);
	}

loser:
	if (passwordData)
		SecKeychainItemFreeContent(NULL, passwordData);
	if (itemRef)
		CFRelease(itemRef);

	return result;
}

static int
do_keychain_find_internet_password(CFTypeRef keychainOrArray, const char *serverName, const char *securityDomain, const char *accountName, const char *path, UInt16 port, SecProtocolType protocol,SecAuthenticationType authenticationType, Boolean get_password)
 {
	OSStatus result;
    SecKeychainItemRef itemRef = NULL;
	void *passwordData = NULL;
    UInt32 passwordLength = 0;
  
	result = SecKeychainFindInternetPassword(keychainOrArray,
		serverName ? strlen(serverName) : 0,
		serverName, securityDomain ? strlen(securityDomain) : 0,
		securityDomain,
		accountName ? strlen(accountName) : 0,
		accountName, path ? strlen(path) : 0,
		path,
		port,
		protocol,
		authenticationType,
		get_password ? &passwordLength : 0,
		get_password ? &passwordData : NULL,
		&itemRef);
	if (result)
	{
		fprintf(stderr, "SecKeychainFindInternetPassword returned %ld(0x%lx)\n", result, result);
		goto loser;
	}

	print_keychain_item_attributes(stderr, itemRef, FALSE, FALSE);

	if (get_password)
	{
		fputs("password: ", stderr);
		print_buffer(stderr, passwordLength, passwordData); 
		fputc('\n', stderr);
	}

loser:
	if (passwordData)
		SecKeychainItemFreeContent(NULL, passwordData);
	if (itemRef)
		CFRelease(itemRef);

	return result;
}

static int
do_keychain_find_certificate(CFTypeRef keychainOrArray, const char *emailAddress, Boolean output_pem, Boolean find_all, Boolean print_email)
 {
	OSStatus result;
    SecCertificateRef certificateRef = NULL;
	SecKeychainSearchRef searchRef = NULL;

	if (find_all)
	{
		result = SecKeychainSearchCreateForCertificateByEmail(keychainOrArray, emailAddress, &searchRef);
		if (result)
		{
			fprintf(stderr, "SecKeychainSearchCreateForCertificateByEmail returned %ld(0x%lx)\n", result, result);
			goto loser;
		}
	}

	do
	{
		if (find_all)
		{
			SecKeychainItemRef itemRef = NULL;
			result = SecKeychainSearchCopyNext(searchRef, &itemRef);
			if (result == errSecItemNotFound)
			{
				result = 0;
				break;
			}
			else if (result)
			{
				fprintf(stderr, "SecKeychainSearchCopyNext returned %ld(0x%lx)\n", result, result);
				goto loser;
			}

			if (certificateRef)
				CFRelease(certificateRef);
			certificateRef = (SecCertificateRef) itemRef;
		}
		else
		{
			result = SecCertificateFindByEmail(keychainOrArray, emailAddress, &certificateRef);
			if (result)
			{
				fprintf(stderr, "SecCertificateFindByEmail returned %ld(0x%lx)\n", result, result);
				goto loser;
			}
		}

		if (print_email)
		{
			CFArrayRef emailAddresses = NULL;
			CFIndex ix, count;
			result = SecCertificateCopyEmailAddresses(certificateRef, &emailAddresses);
			if (result)
			{
				fprintf(stderr, "SecCertificateCopyEmailAddresses returned %ld(0x%lx)\n", result, result);
				goto loser;
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
				fprintf(stderr, "SecCertificateGetData returned %ld(0x%lx)\n", result, result);
				goto loser;
			}
	
			print_buffer_pem(stdout, "CERTIFICATE", certData.Length, certData.Data);
		}
		else
		{
			print_keychain_item_attributes(stderr, (SecKeychainItemRef)certificateRef, FALSE, FALSE);
		}
	} while (find_all);

loser:
	if (searchRef)
		CFRelease(searchRef);
	if (certificateRef)
		CFRelease(certificateRef);

	return result;
}

static int
parse_fourcharcode(const char *name, UInt32 *code)
{
	/* @@@ Check for errors. */
	char *p = (char *)code;
	strncpy(p, name, 4);
	return 0;
}

int
keychain_find_internet_password(int argc, char * const *argv)
{
	char *serverName = NULL, *securityDomain = NULL, *accountName = NULL, *path = NULL;
    UInt16 port = 0;
    SecProtocolType protocol = NULL;
    SecAuthenticationType authenticationType = NULL;
	int ch, result = 0;
	Boolean get_password = FALSE;

	while ((ch = getopt(argc, argv, "a:d:hgp:P:r:s:t:")) != -1)
	{
		switch  (ch)
		{
        case 'a':
            accountName = optarg;
            break;
        case 'd':
            securityDomain = optarg;
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
			if (result)
				goto loser;
			break;
		case 's':
			serverName = optarg;
			break;
        case 't':
			result = parse_fourcharcode(optarg, &authenticationType);
			if (result)
				goto loser;
			break;
        case '?':
		default:
			return 2; /* @@@ Return 2 triggers usage message. */
		}
	}
  
	argc -= optind;
	argv += optind;

    CFTypeRef keychainOrArray = keychain_create_array(argc, argv);

	result = do_keychain_find_internet_password(keychainOrArray, serverName, securityDomain, accountName, path, port, protocol,authenticationType, get_password);


loser:
	if (keychainOrArray)
		CFRelease(keychainOrArray);

	return result;
}

int
keychain_find_generic_password(int argc, char * const *argv)
{
	char *serviceName = NULL, *accountName = NULL;
	int ch, result = 0;
	Boolean get_password = FALSE;

	while ((ch = getopt(argc, argv, "a:s:g")) != -1)
	{
		switch  (ch)
		{
        case 'a':
            accountName = optarg;
            break;
        case 'g':
			get_password = TRUE;
			break;
		case 's':
			serviceName = optarg;
			break;
        case '?':
		default:
			return 2; /* @@@ Return 2 triggers usage message. */
		}
	}
  
	argc -= optind;
	argv += optind;

    CFTypeRef keychainOrArray = keychain_create_array(argc, argv);

	result = do_keychain_find_generic_password(keychainOrArray, serviceName,accountName, get_password);
/*
loser:
	if (keychainOrArray)
		CFRelease(keychainOrArray);
        */

	return result;
}


int
keychain_find_certificate(int argc, char * const *argv)
{
	char *emailAddress = NULL;
	int ch, result = 0;
	Boolean output_pem = FALSE;
	Boolean find_all = FALSE;
	Boolean print_email = FALSE;

	while ((ch = getopt(argc, argv, "ae:mp")) != -1)
	{
		switch  (ch)
		{
        case 'a':
            find_all = TRUE;
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
        case '?':
		default:
			return 2; /* @@@ Return 2 triggers usage message. */
		}
	}

	argc -= optind;
	argv += optind;

    CFTypeRef keychainOrArray = keychain_create_array(argc, argv);

	result = do_keychain_find_certificate(keychainOrArray, emailAddress, output_pem, find_all, print_email);
/*
loser:
	if (keychainOrArray)
		CFRelease(keychainOrArray);
        */

	return result;
}


static int
do_keychain_dump_class(CFTypeRef keychainOrArray, SecItemClass itemClass, Boolean show_data, Boolean show_raw_data)
{
	SecKeychainItemRef item;
	SecKeychainSearchRef search = NULL;
	int result = 0;
	OSStatus status;

	status = SecKeychainSearchCreateFromAttributes(keychainOrArray, itemClass, NULL, &search);
	if (status)
	{
		fprintf(stderr, "SecKeychainSearchCreateFromAttributes returned %ld(0x%lx)\n", status, status);
		result = 1;
		goto loser;
	}

	while ((status = SecKeychainSearchCopyNext(search, &item)) == 0)
	{
		print_keychain_item_attributes(stderr, item, show_data, show_raw_data);
		CFRelease(item);
	}

	if (status != errSecItemNotFound)
	{
		fprintf(stderr, "SecKeychainSearchCopyNext returned %ld(0x%lx)\n", status, status);
		result = 1;
		goto loser;
	}

loser:
	if (search)
		CFRelease(search);

	return result;
}

static int
do_keychain_dump(CFTypeRef keychainOrArray, Boolean show_data, Boolean show_raw_data)
{
	return do_keychain_dump_class(keychainOrArray, CSSM_DL_DB_RECORD_ANY, show_data, show_raw_data);
}

int
keychain_dump(int argc, char * const *argv)
{
	int ch, result = 0;
	Boolean show_data = FALSE, show_raw_data = FALSE;
	CFTypeRef keychainOrArray = NULL;

	while ((ch = getopt(argc, argv, "dhr")) != -1)
	{
		switch  (ch)
		{
		case 'd':
			show_data = TRUE;
			break;
		case 'r':
			show_raw_data = TRUE;
			break;
        case '?':
		default:
			return 2; /* @@@ Return 2 triggers usage message. */
		}
	}
  
	argc -= optind;
	argv += optind;

    keychainOrArray = keychain_create_array(argc, argv);

	result = do_keychain_dump(keychainOrArray, show_data, show_raw_data);

	if (keychainOrArray)
		CFRelease(keychainOrArray);

	return result;
}
