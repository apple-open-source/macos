/* Included due to <rdar://problem/4063307> SecKeyPriv.h should
   include <Security/x509defs.h> */
#include <Security/x509defs.h>

#include <Security/SecKeyPriv.h>
#include <Security/SecKeychainSearch.h>
#include <stdlib.h>
#include <unistd.h>

#include "testenv.h"
#include "testleaks.h"
#include "testmore.h"
#include "testsecevent.h"

void tests(int dont_skip)
{
	SecKeychainRef keychain;
	ok_status(SecKeychainCreate("test", 4, "test", FALSE, NULL, &keychain),
		"create keychain");

	/* Symmetric key tests. */

#ifdef DEBUG
	ok_status(test_sec_event_register(kSecAddEventMask | kSecDeleteEventMask),
		"register for add events");
	SecKeychainItemRef aes_key = NULL;
#endif

	ok_status(SecKeyGenerate(keychain, CSSM_ALGID_AES, 128,
		0 /* contextHandle */,
		CSSM_KEYUSE_DECRYPT | CSSM_KEYUSE_ENCRYPT,
		CSSM_KEYATTR_EXTRACTABLE,
		NULL, NULL), "SecKeyGenerate");

#ifdef DEBUG
	/* Wait for the add notification to get the generated aes_key to work
	   around <rdar://problem/4063405> SecKeyGenerate CFReleases the
	   returned key before returning it.  */
	is_sec_event(kSecAddEvent, NULL, &aes_key, NULL,
		"got add event for key");
#endif

	uint32 btrue = 1;
	SecKeychainAttribute sym_attrs[] =
	{
		{ kSecKeyEncrypt, sizeof(btrue), &btrue }
	};
	SecKeychainAttributeList sym_attr_list =
	{ sizeof(sym_attrs) / sizeof(*sym_attrs), sym_attrs };
	SecKeychainSearchRef search = NULL;
	ok_status(SecKeychainSearchCreateFromAttributes(keychain,
		CSSM_DL_DB_RECORD_SYMMETRIC_KEY, &sym_attr_list, &search),
		"create symmetric encryption key search");
	SecKeychainItemRef item = NULL;
	ok_status(SecKeychainSearchCopyNext(search, &item), "get first key");

#ifdef DEBUG
	cmp_ok((intptr_t)aes_key, ==, (intptr_t)item, "is key found the right one?");
#endif

	if (item) CFRelease(item);
	is_status(SecKeychainSearchCopyNext(search, &item),
		errSecItemNotFound, "copy next returns no more keys");
	CFRelease(search);

	ok_status(SecKeychainSearchCreateFromAttributes(keychain,
		CSSM_DL_DB_RECORD_ANY, NULL, &search),
		"create any item search");
	item = NULL;
	TODO: {
		todo("<rdar://problem/3760340> Searching for CSSM_DL_DB_RECORD_ANY does not return "
			"user-added symmetric keys");

		ok_status(SecKeychainSearchCopyNext(search, &item), "get first key");
		
#ifdef DEBUG
		cmp_ok((intptr_t)aes_key, ==, (intptr_t)item, "is key found the right one?");
#endif

	}
	if (item) CFRelease(item);

	is_status(SecKeychainSearchCopyNext(search, &item),
		errSecItemNotFound, "copy next returns no more keys");
	CFRelease(search);

#ifdef DEBUG
	ok_status(SecKeychainItemDelete(aes_key), "delete key");
	is(CFGetRetainCount(aes_key), 2, "key retain count is 2");
#endif

#ifdef DEBUG
	SecKeychainItemRef deleted_item = NULL;
    is_sec_event(kSecDeleteEvent, NULL, &deleted_item, NULL, "got delete event for key");
	is((intptr_t)aes_key, (intptr_t)deleted_item, "key was the deleted item");
#endif


#ifdef DEBUG
	ok_status(test_sec_event_deregister(), "deregister for events");
#endif

	SecKeyRef aes_key2 = NULL;
	ok_status(SecKeyGenerate(keychain, CSSM_ALGID_AES, 128,
		0 /* contextHandle */,
		CSSM_KEYUSE_DECRYPT | CSSM_KEYUSE_ENCRYPT,
		CSSM_KEYATTR_EXTRACTABLE,
		NULL, &aes_key2), "SecKeyGenerate and get key");
		
	is(CFGetRetainCount(aes_key2), 1, "retain count is 1");
	CFRelease(aes_key2);
	

	CFRelease(keychain);

	ok(tests_end(1), "cleanup");
}

int main(int argc, char *const *argv)
{
	int dont_skip = argc > 1 && !strcmp(argv[1], "-s");
	// plan_tests(21);
	plan_tests(12);

	if (!tests_begin(argc, argv))
		BAIL_OUT("tests_begin failed");

	tests(dont_skip);

	ok_leaks("no leaks");

	return 0;
}
