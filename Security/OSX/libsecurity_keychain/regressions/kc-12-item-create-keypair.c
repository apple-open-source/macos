#include <Security/SecKey.h>
#include <Security/SecKeychain.h>
#include <Security/SecKeychainSearch.h>
#include <stdlib.h>
#include <unistd.h>

#include "keychain_regressions.h"
#include "kc-helpers.h"

static void tests(void)
{
    SecKeychainRef keychain = createNewKeychain("test", "test");
	SecKeyRef pub_crypt = NULL, prv_crypt = NULL;
	ok_status(SecKeyCreatePair(keychain, CSSM_ALGID_RSA, 256,
		0 /* contextHandle */,
		CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_WRAP,
		CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_EXTRACTABLE,
		CSSM_KEYUSE_DECRYPT | CSSM_KEYUSE_UNWRAP,
		CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_EXTRACTABLE |
			CSSM_KEYATTR_SENSITIVE,
		NULL /* initialAccess */, &pub_crypt, &prv_crypt),
		"generate encryption keypair");

	SecKeyRef pub_sign = NULL, prv_sign = NULL;
	ok_status(SecKeyCreatePair(keychain, CSSM_ALGID_RSA, 256,
		0 /* contextHandle */,
		CSSM_KEYUSE_VERIFY,
		CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_EXTRACTABLE,
		CSSM_KEYUSE_SIGN,
		CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_EXTRACTABLE |
			CSSM_KEYATTR_SENSITIVE,
		NULL /* initialAccess */, &pub_sign, &prv_sign),
		"generate signing keypair");

	uint32 btrue = 1;
	uint32 bfalse = 0;
	/* uint32 prv_class = CSSM_KEYCLASS_PRIVATE_KEY; */
	SecKeychainAttribute attrs[] = 
	{
		{ kSecKeyDecrypt, sizeof(uint32), &btrue },
		{ kSecKeyEncrypt, sizeof(uint32), &bfalse },
		/* { kSecKeyKeyClass, sizeof(uint32), &prv_class } */
	};
	SecKeychainAttributeList attrList = { sizeof(attrs) / sizeof(*attrs), attrs };
	SecKeychainSearchRef search;
	OSStatus result;
	SecKeychainItemRef item;
	
	ok_status((result = SecKeychainSearchCreateFromAttributes(keychain,
		CSSM_DL_DB_RECORD_PRIVATE_KEY, &attrList, &search)), "create key search");
	if (result == noErr)
	{
		ok_status(SecKeychainSearchCopyNext(search, &item), "get first key");
		cmp_ok((intptr_t)prv_crypt, ==, (intptr_t)item, "is key found the right one?");
		CFRelease(item);
		item = NULL;
		is_status(SecKeychainSearchCopyNext(search, &item),
			errSecItemNotFound, "get next key");
		is((intptr_t)item, 0, "no item returned");
		CFRelease(search);
	}
	
	SecKeychainAttribute attrs2[] = { { kSecKeySign, sizeof(btrue), &btrue } };
	SecKeychainAttributeList attrList2 = { sizeof(attrs2) / sizeof(*attrs2), attrs2 };
	ok_status((result = SecKeychainSearchCreateFromAttributes(keychain,
		CSSM_DL_DB_RECORD_PRIVATE_KEY, &attrList2, &search)), "create private signing key search");
	
	if (result == noErr)
	{
		ok_status(SecKeychainSearchCopyNext(search, &item), "get first key");
		cmp_ok((intptr_t)prv_sign, ==, (intptr_t)item, "is key found the right one?");
		CFRelease(item);
		is_status(SecKeychainSearchCopyNext(search, &item),
			errSecItemNotFound, "get next key");
		CFRelease(search);
	}
	
	CFRelease(pub_crypt);
	CFRelease(prv_crypt);
	CFRelease(pub_sign);
	CFRelease(prv_sign);

    ok_status(SecKeychainDelete(keychain), "%s: SecKeychainDelete", testName);
	CFRelease(keychain);
}

int kc_17_item_find_key(int argc, char *const *argv)
{
	plan_tests(13);

	tests();

    deleteTestFiles();
	return 0;
}
