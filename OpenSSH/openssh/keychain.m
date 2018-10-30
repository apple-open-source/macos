/*
 * Copyright (c) 2007-2016 Apple Inc. All rights reserved.
 *
 * @APPLE_BSD_LICENSE_HEADER_START@
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @APPLE_BSD_LICENSE_HEADER_END@
 */

#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import <Security/SecItemPriv.h>
#include <sys/stat.h>
#include <stdio.h>

#include "xmalloc.h"
#include "sshkey.h"
#include "ssherr.h"
#include "authfile.h"
#include "openbsd-compat/openbsd-compat.h"
#include "log.h"

char *keychain_read_passphrase(const char *filename)
{
	OSStatus	ret = errSecSuccess;
	NSString	*accountString = [NSString stringWithUTF8String: filename];
	NSData		*passphraseData = NULL;

	if (accountString == nil) {
		debug2("Cannot retrieve identity passphrase from the keychain since the path is not UTF8.");
		return NULL;
	}

	NSDictionary	*searchQuery = @{
			       (id)kSecClass: (id)kSecClassGenericPassword,
			       (id)kSecAttrAccount: accountString,
			       (id)kSecAttrLabel: [NSString stringWithFormat: @"SSH: %@", accountString],
			       (id)kSecAttrService: @"OpenSSH",
			       (id)kSecAttrNoLegacy: @YES,
			       (id)kSecAttrAccessGroup: @"com.apple.ssh.passphrases",
			       (id)kSecReturnData: @YES,
			       (id)kSecUseAuthenticationUI: (id)kSecUseAuthenticationUIFail};
	debug3("Search for item with query: %s", [[searchQuery description] UTF8String]);
	ret = SecItemCopyMatching((CFDictionaryRef)searchQuery, (CFTypeRef *)&passphraseData);
	if (ret == errSecItemNotFound) {
		debug2("Passphrase not found in the keychain.");
		return NULL;
	} else if (ret != errSecSuccess) {
		NSString *errorString = (NSString *)SecCopyErrorMessageString(ret, NULL);
		debug2("Unexpected keychain error while searching for an item: %s", [errorString UTF8String]);
		[errorString release];
		[passphraseData release];
		return NULL;
	}

	if (![passphraseData isKindOfClass: [NSData class]]) {
		debug2("Malformed result returned from the keychain");
		[passphraseData release];
		return NULL;
	}

	char *passphrase = xcalloc([passphraseData length] + 1, sizeof(char));
	[passphraseData getBytes: passphrase length: [passphraseData length]];
	[passphraseData release];

	// Try to load the key first and only return the passphrase if we know it's the right one
	struct sshkey *private = NULL;
	int r = sshkey_load_private_type(KEY_UNSPEC, filename, passphrase, &private, NULL, NULL);
	if (r != SSH_ERR_SUCCESS) {
		debug2("Could not unlock key with the passphrase retrieved from the keychain.");
		freezero(passphrase, strlen(passphrase));
		return NULL;
	}
	sshkey_free(private);

	return passphrase;
}

void store_in_keychain(const char *filename, const char *passphrase)
{
	OSStatus	ret = errSecSuccess;
	BOOL		updateExistingItem = NO;
	NSString	*accountString = [NSString stringWithUTF8String: filename];

	if (accountString == nil) {
		debug2("Cannot store identity passphrase into the keychain since the path is not UTF8.");
		return;
	}

	NSDictionary	*defaultAttributes = @{
				(id)kSecClass: (id)kSecClassGenericPassword,
				(id)kSecAttrAccount: accountString,
				(id)kSecAttrLabel: [NSString stringWithFormat: @"SSH: %@", accountString],
				(id)kSecAttrService: @"OpenSSH",
				(id)kSecAttrNoLegacy: @YES,
				(id)kSecAttrAccessGroup: @"com.apple.ssh.passphrases",
				(id)kSecUseAuthenticationUI: (id)kSecUseAuthenticationUIFail};

	CFTypeRef searchResults = NULL;
	NSMutableDictionary *searchQuery = [@{(id)kSecReturnRef: @YES} mutableCopy];
	[searchQuery addEntriesFromDictionary: defaultAttributes];

	debug3("Search for existing item with query: %s", [[searchQuery description] UTF8String]);
	ret = SecItemCopyMatching((CFDictionaryRef)searchQuery, &searchResults);
	[searchQuery release];
	if (ret == errSecSuccess) {
		debug3("Item already exists in the keychain, updating.");
		updateExistingItem = YES;

	} else if (ret == errSecItemNotFound) {
		debug3("Item does not exist in the keychain, adding.");
	} else {
		NSString *errorString = (NSString *)SecCopyErrorMessageString(ret, NULL);
		debug3("Unexpected keychain error while searching for an item: %s", [errorString UTF8String]);
		[errorString release];
	}

	if (updateExistingItem) {
		NSDictionary *updateQuery = defaultAttributes;
		NSDictionary *changes = @{(id)kSecValueData: [NSData dataWithBytesNoCopy: (void *)passphrase length: strlen(passphrase) freeWhenDone: NO]};

		ret = SecItemUpdate((CFDictionaryRef)updateQuery, (CFDictionaryRef)changes);
		if (ret != errSecSuccess) {
			NSString *errorString = (NSString *)SecCopyErrorMessageString(ret, NULL);
			debug3("Unexpected keychain error while updating the item: %s", [errorString UTF8String]);
			[errorString release];
		}
	} else {
		NSMutableDictionary *addQuery = [@{(id)kSecValueData: [NSData dataWithBytesNoCopy: (void *)passphrase length: strlen(passphrase) freeWhenDone: NO]} mutableCopy];

		[addQuery addEntriesFromDictionary: defaultAttributes];
		ret = SecItemAdd((CFDictionaryRef)addQuery, NULL);
		[addQuery release];
		if (ret != errSecSuccess) {
			NSString *errorString = (NSString *)SecCopyErrorMessageString(ret, NULL);
			debug3("Unexpected keychain error while inserting the item: %s", [errorString UTF8String]);
			[errorString release];
		}
	}
}

/*
 * Remove the passphrase for a given identity from the keychain.
 */
void
remove_from_keychain(const char *filename)
{
	OSStatus	ret = errSecSuccess;
	NSString	*accountString = [NSString stringWithUTF8String: filename];

	if (accountString == nil) {
		debug2("Cannot delete identity passphrase from the keychain since the path is not UTF8.");
		return;
	}

	NSDictionary	*searchQuery = @{
			       (id)kSecClass: (id)kSecClassGenericPassword,
			       (id)kSecAttrAccount: accountString,
			       (id)kSecAttrService: @"OpenSSH",
			       (id)kSecAttrNoLegacy: @YES,
			       (id)kSecAttrAccessGroup: @"com.apple.ssh.passphrases",
			       (id)kSecUseAuthenticationUI: (id)kSecUseAuthenticationUIFail};

	ret = SecItemDelete((CFDictionaryRef)searchQuery);
	if (ret == errSecSuccess) {
		NSString *errorString = (NSString *)SecCopyErrorMessageString(ret, NULL);
		debug3("Unexpected keychain error while deleting the item: %s", [errorString UTF8String]);
		[errorString release];
	}
}


int
load_identities_from_keychain(int (^add_identity)(const char *identity))
{
	int 		ret = 0;
	OSStatus	err = errSecSuccess;

	NSArray		*searchResults = nil;
	NSDictionary	*searchQuery = @{
				(id)kSecClass: (id)kSecClassGenericPassword,
				(id)kSecAttrService: @"OpenSSH",
				(id)kSecAttrNoLegacy: @YES,
				(id)kSecAttrAccessGroup: @"com.apple.ssh.passphrases",
				(id)kSecReturnAttributes: @YES,
				(id)kSecMatchLimit: (id)kSecMatchLimitAll,
				(id)kSecUseAuthenticationUI: (id)kSecUseAuthenticationUIFail};

	err = SecItemCopyMatching((CFDictionaryRef)searchQuery, (CFTypeRef *)&searchResults);
	if (err == errSecItemNotFound) {
		fprintf(stderr, "No identity found in the keychain.\n");
		[searchResults release];
		return 0;
	} else if (err != errSecSuccess || ![searchResults isKindOfClass: [NSArray class]]) {
		return 1;
	}

	for (NSDictionary *itemAttributes in searchResults) {
		NSString	*accountString = itemAttributes[(id)kSecAttrAccount];
		struct stat	st;

		if (stat([accountString UTF8String], &st) < 0)
			continue;
		if (add_identity([accountString UTF8String]))
			ret = 1;
	}
	[searchResults release];

	return ret;
}
