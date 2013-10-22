/*
 * Copyright (c) 2003-2009 Apple Inc. All Rights Reserved.
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
 * keychain_delete.c
 */

#include "keychain_delete.h"
#include "keychain_find.h"

#include "keychain_utilities.h"
#include "security.h"
#include <unistd.h>
#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecTrustSettings.h>

static int
do_delete(CFTypeRef keychainOrArray)
{
	/* @@@ SecKeychainDelete should really take a CFTypeRef argument. */
	OSStatus result = SecKeychainDelete((SecKeychainRef)keychainOrArray);
	if (result)
	{
		/* @@@ Add printing of keychainOrArray. */
		sec_perror("SecKeychainDelete", result);
	}

	return result;
}

static int
do_delete_certificate(CFTypeRef keychainOrArray, const char *name, const char *hash, Boolean deleteTrust)
{
	OSStatus result = noErr;
	SecKeychainItemRef itemToDelete = NULL;
	if (!name && !hash) {
		return 2;
	}

	itemToDelete = find_unique_certificate(keychainOrArray, name, hash);
	if (itemToDelete) {
		if (deleteTrust) {
			result = SecTrustSettingsRemoveTrustSettings((SecCertificateRef)itemToDelete,
														 kSecTrustSettingsDomainUser);
			if (result && result != errSecItemNotFound) {
				sec_perror("SecTrustSettingsRemoveTrustSettings (user)", result);
			}
			if (geteuid() == 0) {
				result = SecTrustSettingsRemoveTrustSettings((SecCertificateRef)itemToDelete,
															 kSecTrustSettingsDomainAdmin);
				if (result && result != errSecItemNotFound) {
					sec_perror("SecTrustSettingsRemoveTrustSettings (admin)", result);
				}
			}
		}
		result = SecKeychainItemDelete(itemToDelete);
		if (result) {
			sec_perror("SecKeychainItemDelete", result);
			goto cleanup;
		}
	} else {
		result = 1;
		fprintf(stderr, "Unable to delete certificate matching \"%s\"",
				(name) ? name : (hash) ? hash : "");
	}

cleanup:
	safe_CFRelease(&itemToDelete);

	return result;
}

int
keychain_delete_certificate(int argc, char * const *argv)
{
	CFTypeRef keychainOrArray = NULL;
	char *name = NULL;
	char *hash = NULL;
	Boolean delete_trust = FALSE;
	int ch, result = 0;

    while ((ch = getopt(argc, argv, "hc:Z:t")) != -1)
	{
		switch  (ch)
		{
			case 'c':
				name = optarg;
				break;
			case 'Z':
				hash = optarg;
				break;
			case 't':
				delete_trust = TRUE;
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

	result = do_delete_certificate(keychainOrArray, name, hash, delete_trust);

cleanup:
	safe_CFRelease(&keychainOrArray);

	return result;
}

int
keychain_delete(int argc, char * const *argv)
{
	CFTypeRef keychainOrArray = NULL;
	int ch, result = 0;

    while ((ch = getopt(argc, argv, "h")) != -1)
	{
		switch  (ch)
		{
		case '?':
		default:
			return 2; /* @@@ Return 2 triggers usage message. */
		}
	}

	argc -= optind;
	argv += optind;

	keychainOrArray = keychain_create_array(argc, argv);

	result = do_delete(keychainOrArray);
	if (keychainOrArray)
		CFRelease(keychainOrArray);

	return result;
}
