#include <Security/SecKeychain.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecKeychainSearch.h>
#include <stdlib.h>
#include <unistd.h>

#include "keychain_regressions.h"
#include "kc-helpers.h"

static void tests()
{
    SecKeychainRef keychain = createNewKeychain("test", "test");

	/* Symmetric key tests. */

	ok_status(SecKeyGenerate(keychain, CSSM_ALGID_AES, 128,
		0 /* contextHandle */,
		CSSM_KEYUSE_DECRYPT | CSSM_KEYUSE_ENCRYPT,
		CSSM_KEYATTR_EXTRACTABLE,
		NULL, NULL), "SecKeyGenerate");

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

	if (item) CFRelease(item);
	is_status(SecKeychainSearchCopyNext(search, &item),
		errSecItemNotFound, "copy next returns no more keys");
	CFRelease(search);

	ok_status(SecKeychainSearchCreateFromAttributes(keychain,
		CSSM_DL_DB_RECORD_ANY, NULL, &search),
		"create any item search");
	item = NULL;

    ok_status(SecKeychainSearchCopyNext(search, &item), "get first key");

	if (item) CFRelease(item);

	is_status(SecKeychainSearchCopyNext(search, &item),
		errSecItemNotFound, "copy next returns no more keys");
	CFRelease(search);

	SecKeyRef aes_key2 = NULL;
	ok_status(SecKeyGenerate(keychain, CSSM_ALGID_AES, 128,
		0 /* contextHandle */,
		CSSM_KEYUSE_DECRYPT | CSSM_KEYUSE_ENCRYPT,
		CSSM_KEYATTR_EXTRACTABLE,
		NULL, &aes_key2), "SecKeyGenerate and get key");
		
	is(CFGetRetainCount(aes_key2), 1, "retain count is 1");
	CFRelease(aes_key2);
	

    ok_status(SecKeychainDelete(keychain), "%s: SecKeychainDelete", testName);
	CFRelease(keychain);
}

int kc_12_key_create_symmetric(int argc, char *const *argv)
{
	plan_tests(11);

	tests();

    deleteTestFiles();
	return 0;
}
