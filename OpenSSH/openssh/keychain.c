/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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

#include "includes.h"

#include <stdio.h>
#include <string.h>

#include "xmalloc.h"
#include "key.h"
#include "authfd.h"
#include "authfile.h"

#if defined(__APPLE_KEYCHAIN__)

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

/* Our Security/SecPassword.h is not yet API, so I will define the constants that I am using here. */
kSecPasswordGet     = 1<<0;  // Get password from keychain or user
kSecPasswordSet     = 1<<1;  // Set password (passed in if kSecPasswordGet not set, otherwise from user)
kSecPasswordFail    = 1<<2;  // Wrong password (ignore item in keychain and flag error)

#endif

/*
 * Platform-specific helper functions.
 */

#if defined(__APPLE_KEYCHAIN__)

static int get_boolean_preference(const char *key, int default_value,
    int foreground)
{
	int value = default_value;
	CFStringRef keyRef = NULL;
	CFPropertyListRef valueRef = NULL;

	keyRef = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
	if (keyRef != NULL)
		valueRef = CFPreferencesCopyAppValue(keyRef,
		    CFSTR("org.openbsd.openssh"));
	if (valueRef != NULL)
		if (CFGetTypeID(valueRef) == CFBooleanGetTypeID())
			value = CFBooleanGetValue(valueRef);
		else if (foreground)
			fprintf(stderr, "Ignoring nonboolean %s preference.\n", key);

	if (keyRef)
		CFRelease(keyRef);
	if (valueRef)
		CFRelease(valueRef);

	return value;
}

#endif

/*
 * Store the passphrase for a given identity in the keychain.
 */
void
store_in_keychain(const char *filename, const char *passphrase)
{

#if defined(__APPLE_KEYCHAIN__)

	/*
	 * store_in_keychain
	 * Mac OS X implementation
	 */

	CFStringRef cfstr_relative_filename = NULL;
	CFURLRef cfurl_relative_filename = NULL, cfurl_filename = NULL;
	CFStringRef cfstr_filename = NULL;
	CFDataRef cfdata_filename = NULL;
	CFIndex filename_len;
	UInt8 *label = NULL;
	UInt8 *utf8_filename;
	OSStatus rv;
	SecKeychainItemRef itemRef = NULL;
	SecTrustedApplicationRef apps[] = {NULL, NULL, NULL};
	CFArrayRef trustedlist = NULL;
	SecAccessRef initialAccess = NULL;

	/* Bail out if KeychainIntegration preference is -bool NO */
	if (get_boolean_preference("KeychainIntegration", 1, 1) == 0) {
		fprintf(stderr, "Keychain integration is disabled.\n");
		goto err;
	}

	/* Interpret filename with the correct encoding. */
	if ((cfstr_relative_filename =
	    CFStringCreateWithFileSystemRepresentation(NULL, filename)) == NULL)
	    {
		fprintf(stderr, "CFStringCreateWithFileSystemRepresentation failed\n");
		goto err;
	}
	if ((cfurl_relative_filename = CFURLCreateWithFileSystemPath(NULL,
	    cfstr_relative_filename, kCFURLPOSIXPathStyle, false)) == NULL) {
		fprintf(stderr, "CFURLCreateWithFileSystemPath failed\n");
		goto err;
	}
	if ((cfurl_filename = CFURLCopyAbsoluteURL(cfurl_relative_filename)) ==
	    NULL) {
		fprintf(stderr, "CFURLCopyAbsoluteURL failed\n");
		goto err;
	}
	if ((cfstr_filename = CFURLCopyFileSystemPath(cfurl_filename,
	    kCFURLPOSIXPathStyle)) == NULL) {
		fprintf(stderr, "CFURLCopyFileSystemPath failed\n");
		goto err;
	}
	if ((cfdata_filename = CFStringCreateExternalRepresentation(NULL,
	    cfstr_filename, kCFStringEncodingUTF8, 0)) == NULL) {
		fprintf(stderr, "CFStringCreateExternalRepresentation failed\n");
		goto err;
	}
	filename_len = CFDataGetLength(cfdata_filename);
	if ((label = xmalloc(filename_len + 5)) == NULL) {
		fprintf(stderr, "xmalloc failed\n");
		goto err;
	}
	memcpy(label, "SSH: ", 5);
	utf8_filename = label + 5;
	CFDataGetBytes(cfdata_filename, CFRangeMake(0, filename_len),
	    utf8_filename);

	/* Check if we already have this passphrase. */
	rv = SecKeychainFindGenericPassword(NULL, 3, "SSH", filename_len,
	    (char *)utf8_filename, NULL, NULL, &itemRef);
	if (rv == errSecItemNotFound) {
		/* Add a new keychain item. */
		SecKeychainAttribute attrs[] = {
			{kSecLabelItemAttr, filename_len + 5, label},
			{kSecServiceItemAttr, 3, "SSH"},
			{kSecAccountItemAttr, filename_len, utf8_filename}
		};
		SecKeychainAttributeList attrList =
		    {sizeof(attrs) / sizeof(attrs[0]), attrs};
		if (SecTrustedApplicationCreateFromPath("/usr/bin/ssh-agent",
		    &apps[0]) != noErr ||
		    SecTrustedApplicationCreateFromPath("/usr/bin/ssh-add",
		    &apps[1]) != noErr ||
		    SecTrustedApplicationCreateFromPath("/usr/bin/ssh",
		    &apps[2]) != noErr) {
			fprintf(stderr, "SecTrustedApplicationCreateFromPath failed\n");
			goto err;
		}
		if ((trustedlist = CFArrayCreate(NULL, (const void **)apps,
		    sizeof(apps) / sizeof(apps[0]), &kCFTypeArrayCallBacks)) ==
		    NULL) {
			fprintf(stderr, "CFArrayCreate failed\n");
			goto err;
		}
		if (SecAccessCreate(cfstr_filename, trustedlist,
		    &initialAccess) != noErr) {
			fprintf(stderr, "SecAccessCreate failed\n");
			goto err;
		}
		if (SecKeychainItemCreateFromContent(
		    kSecGenericPasswordItemClass, &attrList, strlen(passphrase),
		    passphrase, NULL, initialAccess, NULL) == noErr)
			fprintf(stderr, "Passphrase stored in keychain: %s\n", filename);
		else
			fprintf(stderr, "Could not create keychain item\n");
	} else if (rv == noErr) {
		/* Update an existing keychain item. */
		if (SecKeychainItemModifyAttributesAndData(itemRef, NULL,
		    strlen(passphrase), passphrase) == noErr)
			fprintf(stderr, "Passphrase updated in keychain: %s\n", filename);
		else
			fprintf(stderr, "Could not modify keychain item\n");
	} else
		fprintf(stderr, "Could not access keychain\n");

err:	/* Clean up. */
	if (cfstr_relative_filename)
		CFRelease(cfstr_relative_filename);
	if (cfurl_relative_filename)
		CFRelease(cfurl_relative_filename);
	if (cfurl_filename)
		CFRelease(cfurl_filename);
	if (cfstr_filename)
		CFRelease(cfstr_filename);
	if (cfdata_filename)
		CFRelease(cfdata_filename);
	if (label)
		xfree(label);
	if (itemRef)
		CFRelease(itemRef);
	if (apps[0])
		CFRelease(apps[0]);
	if (apps[1])
		CFRelease(apps[1]);
	if (apps[2])
		CFRelease(apps[2]);
	if (trustedlist)
		CFRelease(trustedlist);
	if (initialAccess)
		CFRelease(initialAccess);

#else

	/*
	 * store_in_keychain
	 * no keychain implementation
	 */

	fprintf(stderr, "Keychain is not available on this system\n");

#endif

}

/*
 * Remove the passphrase for a given identity from the keychain.
 */
void
remove_from_keychain(const char *filename)
{

#if defined(__APPLE_KEYCHAIN__)

	/*
	 * remove_from_keychain
	 * Mac OS X implementation
	 */

	CFStringRef cfstr_relative_filename = NULL;
	CFURLRef cfurl_relative_filename = NULL, cfurl_filename = NULL;
	CFStringRef cfstr_filename = NULL;
	CFDataRef cfdata_filename = NULL;
	CFIndex filename_len;
	const UInt8 *utf8_filename;
	OSStatus rv;
	SecKeychainItemRef itemRef = NULL;

	/* Bail out if KeychainIntegration preference is -bool NO */
	if (get_boolean_preference("KeychainIntegration", 1, 1) == 0) {
		fprintf(stderr, "Keychain integration is disabled.\n");
		goto err;
	}

	/* Interpret filename with the correct encoding. */
	if ((cfstr_relative_filename =
	    CFStringCreateWithFileSystemRepresentation(NULL, filename)) == NULL)
	    {
		fprintf(stderr, "CFStringCreateWithFileSystemRepresentation failed\n");
		goto err;
	}
	if ((cfurl_relative_filename = CFURLCreateWithFileSystemPath(NULL,
	    cfstr_relative_filename, kCFURLPOSIXPathStyle, false)) == NULL) {
		fprintf(stderr, "CFURLCreateWithFileSystemPath failed\n");
		goto err;
	}
	if ((cfurl_filename = CFURLCopyAbsoluteURL(cfurl_relative_filename)) ==
	    NULL) {
		fprintf(stderr, "CFURLCopyAbsoluteURL failed\n");
		goto err;
	}
	if ((cfstr_filename = CFURLCopyFileSystemPath(cfurl_filename,
	    kCFURLPOSIXPathStyle)) == NULL) {
		fprintf(stderr, "CFURLCopyFileSystemPath failed\n");
		goto err;
	}
	if ((cfdata_filename = CFStringCreateExternalRepresentation(NULL,
	    cfstr_filename, kCFStringEncodingUTF8, 0)) == NULL) {
		fprintf(stderr, "CFStringCreateExternalRepresentation failed\n");
		goto err;
	}
	filename_len = CFDataGetLength(cfdata_filename);
	utf8_filename = CFDataGetBytePtr(cfdata_filename);

	/* Check if we already have this passphrase. */
	rv = SecKeychainFindGenericPassword(NULL, 3, "SSH", filename_len,
	    (const char *)utf8_filename, NULL, NULL, &itemRef);
	if (rv == noErr) {
		/* Remove the passphrase from the keychain. */
		if (SecKeychainItemDelete(itemRef) == noErr)
			fprintf(stderr, "Passphrase removed from keychain: %s\n", filename);
		else
			fprintf(stderr, "Could not remove keychain item\n");
	} else if (rv != errSecItemNotFound)
		fprintf(stderr, "Could not access keychain\n");

err:	/* Clean up. */
	if (cfstr_relative_filename)
		CFRelease(cfstr_relative_filename);
	if (cfurl_relative_filename)
		CFRelease(cfurl_relative_filename);
	if (cfurl_filename)
		CFRelease(cfurl_filename);
	if (cfstr_filename)
		CFRelease(cfstr_filename);
	if (cfdata_filename)
		CFRelease(cfdata_filename);
	if (itemRef)
		CFRelease(itemRef);

#else

	/*
	 * remove_from_keychain
	 * no keychain implementation
	 */

	fprintf(stderr, "Keychain is not available on this system\n");

#endif

}

/*
 * Add identities to ssh-agent using passphrases stored in the keychain.
 * Returns zero on success and nonzero on failure.
 * add_identity is a callback into ssh-agent.  It takes a filename and a
 * passphrase, and attempts to add the identity to the agent.  It returns
 * zero on success and nonzero on failure.
 */
int
add_identities_using_keychain(int (*add_identity)(const char *, const char *))
{

#if defined(__APPLE_KEYCHAIN__)

	/*
	 * add_identities_using_keychain
	 * Mac OS X implementation
	 */

	OSStatus rv;
	SecKeychainSearchRef searchRef;
	SecKeychainItemRef itemRef;
	UInt32 length;
	void *data;
	CFIndex maxsize;

	/* Bail out if KeychainIntegration preference is -bool NO */
	if (get_boolean_preference("KeychainIntegration", 1, 0) == 0)
		return 0;

	/* Search for SSH passphrases in the keychain */
	SecKeychainAttribute attrs[] = {
		{kSecServiceItemAttr, 3, "SSH"}
	};
	SecKeychainAttributeList attrList =
	    {sizeof(attrs) / sizeof(attrs[0]), attrs};
	if ((rv = SecKeychainSearchCreateFromAttributes(NULL,
	    kSecGenericPasswordItemClass, &attrList, &searchRef)) != noErr)
		return 0;

	/* Iterate through the search results. */
	while ((rv = SecKeychainSearchCopyNext(searchRef, &itemRef)) == noErr) {
		UInt32 tag = kSecAccountItemAttr;
		UInt32 format = kSecFormatUnknown;
		SecKeychainAttributeInfo info = {1, &tag, &format};
		SecKeychainAttributeList *itemAttrList = NULL;
		CFStringRef cfstr_filename = NULL;
		char *filename = NULL;
		char *passphrase = NULL;

		/* Retrieve filename and passphrase. */
		if ((rv = SecKeychainItemCopyAttributesAndData(itemRef, &info,
		    NULL, &itemAttrList, &length, &data)) != noErr)
			goto err;
		if (itemAttrList->count != 1)
			goto err;
		cfstr_filename = CFStringCreateWithBytes(NULL,
		    itemAttrList->attr->data, itemAttrList->attr->length,
		    kCFStringEncodingUTF8, true);
		maxsize = CFStringGetMaximumSizeOfFileSystemRepresentation(
		    cfstr_filename);
		if ((filename = xmalloc(maxsize)) == NULL)
			goto err;
		if (CFStringGetFileSystemRepresentation(cfstr_filename,
		    filename, maxsize) == false)
			goto err;
		if ((passphrase = xmalloc(length + 1)) == NULL)
			goto err;
		memcpy(passphrase, data, length);
		passphrase[length] = '\0';

		/* Add the identity. */
		add_identity(filename, passphrase);

err:		/* Clean up. */
		if (itemRef)
			CFRelease(itemRef);
		if (cfstr_filename)
			CFRelease(cfstr_filename);
		if (filename)
			xfree(filename);
		if (passphrase)
			xfree(passphrase);
		if (itemAttrList)
			SecKeychainItemFreeAttributesAndData(itemAttrList,
			    data);
	}

	CFRelease(searchRef);

	return 0;

#else

	/*
	 * add_identities_using_keychain
	 * no implementation
	 */

	return 1;

#endif

}

/*
 * Prompt the user for a key's passphrase.  The user will be offered the option
 * of storing the passphrase in their keychain.  Returns the passphrase
 * (which the caller is responsible for xfreeing), or NULL if this function
 * fails or is not implemented.  If this function is not implemented, ssh will
 * fall back on the standard read_passphrase function, and the user will need
 * to use ssh-add -K to add their keys to the keychain.
 */
char *
keychain_read_passphrase(const char *filename, int oAskPassGUI)
{

#if defined(__APPLE_KEYCHAIN__)

	/*
	 * keychain_read_passphrase
	 * Mac OS X implementation
	 */

	CFStringRef cfstr_relative_filename = NULL;
	CFURLRef cfurl_relative_filename = NULL, cfurl_filename = NULL;
	CFStringRef cfstr_filename = NULL;
	CFDataRef cfdata_filename = NULL;
	CFIndex filename_len;
	UInt8 *label = NULL;
	UInt8 *utf8_filename;
	SecPasswordRef passRef = NULL;
	SecTrustedApplicationRef apps[] = {NULL, NULL, NULL};
	CFArrayRef trustedlist = NULL;
	SecAccessRef initialAccess = NULL;
	CFURLRef path = NULL;
	CFStringRef pathFinal = NULL;
	CFURLRef bundle_url = NULL;
	CFBundleRef bundle = NULL;
	CFStringRef promptTemplate = NULL, prompt = NULL;
	UInt32 length;
	const void *data;
	AuthenticationConnection *ac = NULL;
	char *result = NULL;

	/* Bail out if KeychainIntegration preference is -bool NO */
	if (get_boolean_preference("KeychainIntegration", 1, 1) == 0)
		goto err;

	/* Bail out if the user set AskPassGUI preference to -bool NO */
	if (get_boolean_preference("AskPassGUI", 1, 1) == 0 || oAskPassGUI == 0)
		goto err;

	/* Bail out if we can't communicate with ssh-agent */
	if ((ac = ssh_get_authentication_connection()) == NULL)
		goto err;

	/* Interpret filename with the correct encoding. */
	if ((cfstr_relative_filename =
	    CFStringCreateWithFileSystemRepresentation(NULL, filename)) == NULL)
	    {
		fprintf(stderr, "CFStringCreateWithFileSystemRepresentation failed\n");
		goto err;
	}
	if ((cfurl_relative_filename = CFURLCreateWithFileSystemPath(NULL,
	    cfstr_relative_filename, kCFURLPOSIXPathStyle, false)) == NULL) {
		fprintf(stderr, "CFURLCreateWithFileSystemPath failed\n");
		goto err;
	}
	if ((cfurl_filename = CFURLCopyAbsoluteURL(cfurl_relative_filename)) ==
	    NULL) {
		fprintf(stderr, "CFURLCopyAbsoluteURL failed\n");
		goto err;
	}
	if ((cfstr_filename = CFURLCopyFileSystemPath(cfurl_filename,
	    kCFURLPOSIXPathStyle)) == NULL) {
		fprintf(stderr, "CFURLCopyFileSystemPath failed\n");
		goto err;
	}
	if ((cfdata_filename = CFStringCreateExternalRepresentation(NULL,
	    cfstr_filename, kCFStringEncodingUTF8, 0)) == NULL) {
		fprintf(stderr, "CFStringCreateExternalRepresentation failed\n");
		goto err;
	}
	filename_len = CFDataGetLength(cfdata_filename);
	if ((label = xmalloc(filename_len + 5)) == NULL) {
		fprintf(stderr, "xmalloc failed\n");
		goto err;
	}
	memcpy(label, "SSH: ", 5);
	utf8_filename = label + 5;
	CFDataGetBytes(cfdata_filename, CFRangeMake(0, filename_len),
	    utf8_filename);

	/* Build a SecPasswordRef. */
	SecKeychainAttribute searchAttrs[] = {
		{kSecServiceItemAttr, 3, "SSH"},
		{kSecAccountItemAttr, filename_len, utf8_filename}
	};
	SecKeychainAttributeList searchAttrList =
	    {sizeof(searchAttrs) / sizeof(searchAttrs[0]), searchAttrs};
	SecKeychainAttribute attrs[] = {
		{kSecLabelItemAttr, filename_len + 5, label},
		{kSecServiceItemAttr, 3, "SSH"},
		{kSecAccountItemAttr, filename_len, utf8_filename}
	};
	SecKeychainAttributeList attrList =
	    {sizeof(attrs) / sizeof(attrs[0]), attrs};
	if (SecGenericPasswordCreate(&searchAttrList, &attrList, &passRef) !=
	    noErr) {
		fprintf(stderr, "SecGenericPasswordCreate failed\n");
		goto err;
	}
	if (SecTrustedApplicationCreateFromPath("/usr/bin/ssh-agent", &apps[0])
	    != noErr ||
	    SecTrustedApplicationCreateFromPath("/usr/bin/ssh-add", &apps[1])
	    != noErr ||
	    SecTrustedApplicationCreateFromPath("/usr/bin/ssh", &apps[2])
	    != noErr) {
		fprintf(stderr, "SecTrustedApplicationCreateFromPath failed\n");
		goto err;
	}
	if ((trustedlist = CFArrayCreate(NULL, (const void **)apps,
	    sizeof(apps) / sizeof(apps[0]), &kCFTypeArrayCallBacks)) == NULL) {
		fprintf(stderr, "CFArrayCreate failed\n");
		goto err;
	}
	if (SecAccessCreate(cfstr_filename, trustedlist, &initialAccess)
	    != noErr) {
		fprintf(stderr, "SecAccessCreate failed\n");
		goto err;
	}
	if (SecPasswordSetInitialAccess(passRef, initialAccess) != noErr) {
		fprintf(stderr, "SecPasswordSetInitialAccess failed\n");
		goto err;
	}

	/* Request the passphrase from the user. */
	if ((path = CFURLCreateFromFileSystemRepresentation(NULL,
	    (UInt8 *)filename, strlen(filename), false)) == NULL) {
		fprintf(stderr, "CFURLCreateFromFileSystemRepresentation failed\n");
		goto err;
	}
	if ((pathFinal = CFURLCopyLastPathComponent(path)) == NULL) {
		fprintf(stderr, "CFURLCopyLastPathComponent failed\n");
		goto err;
	}
	if (!((bundle_url = CFURLCreateWithFileSystemPath(NULL,
	    CFSTR("/System/Library/CoreServices/"), kCFURLPOSIXPathStyle, true))
	    != NULL && (bundle = CFBundleCreate(NULL, bundle_url)) != NULL &&
	    (promptTemplate = CFCopyLocalizedStringFromTableInBundle(
	    CFSTR("Enter your password for the SSH key \"%@\"."),
	    CFSTR("OpenSSH"), bundle, "Text of the dialog asking the user for"
	    "their passphrase.  The %@ will be replaced with the filename of a"
	    "specific key.")) != NULL) &&
	    (promptTemplate = CFStringCreateCopy(NULL,
	    CFSTR("Enter your password for the SSH key \"%@\"."))) == NULL) {
		fprintf(stderr, "CFStringCreateCopy failed\n");
		goto err;
	}
	if ((prompt = CFStringCreateWithFormat(NULL, NULL, promptTemplate,
	    pathFinal)) == NULL) {
		fprintf(stderr, "CFStringCreateWithFormat failed\n");
		goto err;
	}
	switch (SecPasswordAction(passRef, prompt,
	    kSecPasswordGet|kSecPasswordFail, &length, &data)) {
	case noErr:
		result = xmalloc(length + 1);
		memcpy(result, data, length);
		result[length] = '\0';

		/* Save password in keychain if requested. */
		if (noErr != SecPasswordAction(passRef, CFSTR(""), kSecPasswordSet, &length, &data))
			fprintf(stderr, "Saving password to keychain failed\n");

		/* Add password to agent. */
		char *comment = NULL;
		Key *private = key_load_private(filename, result, &comment);
		if (NULL == private)
			break;
		if (ssh_add_identity(ac, private, comment))
			fprintf(stderr, "Identity added: %s (%s)\n", filename, comment);
		else
			fprintf(stderr, "Could not add identity: %s\n", filename);
		xfree(comment);
		key_free(private);
		break;
	case errAuthorizationCanceled:
		result = xmalloc(1);
		*result = '\0';
		break;
	default:
		goto err;
	}

err:	/* Clean up. */
	if (cfstr_relative_filename)
		CFRelease(cfstr_relative_filename);
	if (cfurl_relative_filename)
		CFRelease(cfurl_relative_filename);
	if (cfurl_filename)
		CFRelease(cfurl_filename);
	if (cfstr_filename)
		CFRelease(cfstr_filename);
	if (cfdata_filename)
		CFRelease(cfdata_filename);
	if (label)
		xfree(label);
	if (passRef)
		CFRelease(passRef);
	if (apps[0])
		CFRelease(apps[0]);
	if (apps[1])
		CFRelease(apps[1]);
	if (apps[2])
		CFRelease(apps[2]);
	if (trustedlist)
		CFRelease(trustedlist);
	if (initialAccess)
		CFRelease(initialAccess);
	if (path)
		CFRelease(path);
	if (pathFinal)
		CFRelease(pathFinal);
	if (bundle_url)
		CFRelease(bundle_url);
	if (bundle)
		CFRelease(bundle);
	if (promptTemplate)
		CFRelease(promptTemplate);
	if (prompt)
		CFRelease(prompt);
	if (ac)
		ssh_close_authentication_connection(ac);

	return result;

#else

	/*
	 * keychain_read_passphrase
	 * no implementation
	 */

	return NULL;

#endif

}
