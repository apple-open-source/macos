#include <Security/Security.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecItem.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdlib.h>
#include <unistd.h>

#include "testmore.h"
#include "testenv.h"
#include "testcssm.h"
#include "testleaks.h"


void AddKeyCFTypePairToDictionary(CFMutableDictionaryRef mdr, CFTypeRef key, CFTypeRef value)
{
	CFDictionaryAddValue(mdr, key, value);
}



void AddKeyNumberPairToDictionary(CFMutableDictionaryRef mdr, CFTypeRef key, uint32 value)
{
	// make a CFNumber out of the value
	CFNumberRef number = CFNumberCreate(NULL, kCFNumberSInt32Type, &value);
	CFDictionaryAddValue(mdr, key, number);
	CFRelease(number);
}



void AddKeyStringPairToDictionary(CFMutableDictionaryRef mdr, CFTypeRef key, const char* string)
{
	// We add the string as a CFData
	CFDataRef data = CFDataCreate(NULL, (const UInt8*) string, strlen(string));
	CFDictionaryAddValue(mdr, key, data);
	CFRelease(data);
}



int SignVerifyTest()
{
	CFMutableDictionaryRef parameters;
	SecKeyRef publicKey, privateKey;
	
	// start out with an empty dictionary and see if it returns an error
	parameters = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	OSStatus result = SecKeyGeneratePair(parameters, &publicKey, &privateKey);
	
	ok(result != noErr, "result is noErr");
	
	// add the algorithm type
	AddKeyCFTypePairToDictionary(parameters, kSecAttrKeyType, kSecAttrKeyTypeRSA);
	
	// see if we can get a 2048 bit keypair
	AddKeyNumberPairToDictionary(parameters, kSecAttrKeySizeInBits, 2048);
	
	// put an application tag on the key
	AddKeyStringPairToDictionary(parameters, kSecAttrApplicationTag, "This is a test.");

	// try again
	result = SecKeyGeneratePair(parameters, &publicKey, &privateKey);
	ok_status(result, "SecKeyGeneratePair");
	if (result != noErr)
	{
		return 1;
	}
	
	// Make a chunk of data
	char data[] = "This is a test of some data.  Ain't it grand?";
		
	SecPadding paddings[] = {kSecPaddingNone, kSecPaddingPKCS1, kSecPaddingPKCS1MD2, kSecPaddingPKCS1MD5, kSecPaddingPKCS1SHA1};
	const int numberOfPaddings = sizeof(paddings) / sizeof (SecPadding);
	
	// test each padding mode
	int n;
	for (n = 0; n < numberOfPaddings; ++n)
	{
		// sign that data with the private key
		uint8 signature[512];
		size_t signatureLength = sizeof(signature);
		
		result = SecKeyRawSign(privateKey, paddings[n], (uint8_t*) data, strlen(data), signature, &signatureLength);
		ok_status(result, "SecKeyRawSign");

		// verify with the signature
		result = SecKeyRawVerify(publicKey, paddings[n], (uint8_t*) data, strlen(data), signature, signatureLength);
		ok_status(result, "SecKeyRawVerify");
	}
	
	// clean up
	SecKeychainItemDelete((SecKeychainItemRef) publicKey);
	SecKeychainItemDelete((SecKeychainItemRef) privateKey);
	
	CFRelease(publicKey);
	CFRelease(privateKey);
	CFRelease(parameters);
	
	return 0;
}

int SignVerifyWithAsyncTest()
{
	CFMutableDictionaryRef parameters;
	__block SecKeyRef publicKey, privateKey;
	
	// start out with an empty dictionary and see if it returns an error
	parameters = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	OSStatus result = SecKeyGeneratePair(parameters, &publicKey, &privateKey);
	dispatch_group_t everyone_called = dispatch_group_create();

	dispatch_group_enter(everyone_called);
	SecKeyGeneratePairAsync(parameters, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(SecKeyRef publicKey, SecKeyRef privateKey,  CFErrorRef error){
		ok(publicKey == NULL && privateKey == NULL, "keys are NULL");
		ok(error != NULL, "error set");
		dispatch_group_leave(everyone_called);
	});
	
	// add the algorithm type
	AddKeyCFTypePairToDictionary(parameters, kSecAttrKeyType, kSecAttrKeyTypeRSA);
	
	// see if we can get a 2048 bit keypair
	AddKeyNumberPairToDictionary(parameters, kSecAttrKeySizeInBits, 2048);
	
	// put an application tag on the key
	AddKeyStringPairToDictionary(parameters, kSecAttrApplicationTag, "This is a test.");

	// throw some sort of access thingie on it too
	SecAccessRef access = NULL;
	SecTrustedApplicationRef myself = NULL;
	ok_status(SecTrustedApplicationCreateFromPath(NULL, &myself), "create trusted app for self");
	CFArrayRef trustedApplications = CFArrayCreate(NULL, (const void **)&myself, 1, &kCFTypeArrayCallBacks);
	ok_status(SecAccessCreate(CFSTR("Trust self (test)"), trustedApplications, &access), "SecAccessCreate");
	CFRelease(trustedApplications);
	CFRelease(myself);
	AddKeyCFTypePairToDictionary(parameters, kSecAttrAccess, access);

	// try again
	dispatch_group_enter(everyone_called);
	SecKeyGeneratePairAsync(parameters, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(SecKeyRef pubKey, SecKeyRef privKey,  CFErrorRef error){
		ok(pubKey != NULL && privKey != NULL, "keys set");
		ok(error == NULL, "no error");
		publicKey = (SecKeyRef)CFRetain(pubKey);
		privateKey = (SecKeyRef)CFRetain(privKey);
		dispatch_group_leave(everyone_called);
	});

	dispatch_group_wait(everyone_called, DISPATCH_TIME_FOREVER);
	if (NULL == publicKey || NULL == privateKey)
	{
		return 1;
	}
	
	// Make a chunk of data
	char data[] = "This is a test of some data.  Ain't it grand?";
		
	SecPadding paddings[] = {kSecPaddingNone, kSecPaddingPKCS1, kSecPaddingPKCS1MD2, kSecPaddingPKCS1MD5, kSecPaddingPKCS1SHA1};
	const int numberOfPaddings = sizeof(paddings) / sizeof (SecPadding);
	
	// test each padding mode
	int n;
	for (n = 0; n < numberOfPaddings; ++n)
	{
		// sign that data with the private key
		uint8 signature[512];
		size_t signatureLength = sizeof(signature);
		
		result = SecKeyRawSign(privateKey, paddings[n], (uint8_t*) data, strlen(data), signature, &signatureLength);
		ok_status(result, "SecKeyRawSign");

		// verify with the signature
		result = SecKeyRawVerify(publicKey, paddings[n], (uint8_t*) data, strlen(data), signature, signatureLength);
		ok_status(result, "SecKeyRawVerify");
	}
	
	// clean up
	SecKeychainItemDelete((SecKeychainItemRef) publicKey);
	SecKeychainItemDelete((SecKeychainItemRef) privateKey);
	
	CFRelease(publicKey);
	CFRelease(privateKey);
	CFRelease(parameters);
	
	return 0;
}



int EncryptDecryptTest()
{
	CFMutableDictionaryRef parameters;
	SecKeyRef encryptionKey, decryptionKey;
	
	// start out with an empty dictionary and see if it returns an error
	parameters = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	
	// add the algorithm type
	AddKeyCFTypePairToDictionary(parameters, kSecAttrKeyType, kSecAttrKeyTypeRSA);
	
	// see if we can get a 2048 bit keypair
	AddKeyNumberPairToDictionary(parameters, kSecAttrKeySizeInBits, 2048);
	
	// put an application tag on the key
	AddKeyStringPairToDictionary(parameters, kSecAttrApplicationTag, "This is a test.");

	OSStatus result = SecKeyGeneratePair(parameters, &encryptionKey, &decryptionKey);
	ok_status(result, "EncryptDecryptTest");
	
	// Make a chunk of data
	char data[] = "I want to keep this data secure.";
		
	SecPadding paddings[] = {kSecPaddingPKCS1};
	const int numberOfPaddings = sizeof(paddings) / sizeof (SecPadding);
	
	// test each padding mode
	int n;
	for (n = 0; n < numberOfPaddings; ++n)
	{
		// encrypt that data with the public key
		uint8 encryptedData[2048];
		size_t encryptedDataLength = sizeof(encryptedData);
		memset(encryptedData, 0xFF, encryptedDataLength);
		
		result = SecKeyEncrypt(encryptionKey, paddings[n], (uint8_t*) data, sizeof(data), encryptedData, &encryptedDataLength);
		if (result != noErr)
		{
			fprintf(stderr, "Error in encryption.\n");
			cssmPerror(NULL, result);
			return 1;
		}
		
		uint8 decryptedData[2048];
		size_t decryptedDataLength = sizeof(decryptedData);

		// decrypt with the private key
		result = SecKeyDecrypt(decryptionKey, paddings[n], encryptedData, encryptedDataLength, decryptedData, &decryptedDataLength);
		ok_status(result, "SecKeyDecrypt");
		
		// what we got back had better equal what we put in
		if (memcmp(data, decryptedData, sizeof(data)) != 0)
		{
			fprintf(stderr, "Decrypted text != original plain text.\n");
			return 1;
		}
	}
	
	// clean up
	SecKeychainItemDelete((SecKeychainItemRef) encryptionKey);
	SecKeychainItemDelete((SecKeychainItemRef) decryptionKey);
	
	CFRelease(encryptionKey);
	CFRelease(decryptionKey);
	CFRelease(parameters);
	
	return 0;
}



#define TEST_ITEM_ACCOUNT CFSTR("SecItemTest_Account")
#define TEST_ITEM_SERVICE CFSTR("SecItemTest_Service")

int SecItemTest()
{
	// create a dictionary to hold the item
	CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	
	// create our item -- we are only testing generic passwords, since that is the scope of the bug
	CFDictionaryAddValue(dict, kSecClass, kSecClassGenericPassword);
	CFDictionaryAddValue(dict, kSecAttrAccount, TEST_ITEM_ACCOUNT);
	CFDictionaryAddValue(dict, kSecAttrService, TEST_ITEM_SERVICE);
	
	const char* data = "Shh!  It's a secret!!!";
	CFDataRef dataRef = CFDataCreateWithBytesNoCopy(NULL, (const UInt8*) data, strlen(data), NULL);
	CFDictionaryAddValue(dict, kSecValueData, dataRef);
	
	CFTypeRef itemRef;
	OSStatus result = SecItemAdd(dict, &itemRef);
	ok_status(result, "SecItemAdd");
	
	// cleanup
	CFRelease(dict);
	
	// search for the item
	dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	
	// create our item -- we are only testing generic passwords, since that is the scope of the bug
	CFDictionaryAddValue(dict, kSecClass, kSecClassGenericPassword);
	CFDictionaryAddValue(dict, kSecAttrAccount, TEST_ITEM_ACCOUNT);
	CFDictionaryAddValue(dict, kSecAttrService, TEST_ITEM_SERVICE);
	CFDictionaryAddValue(dict, kSecReturnAttributes, kCFBooleanTrue);
	
	result = SecItemCopyMatching(dict, &itemRef);
	ok_status(result, "SecItemCopyMatching");
	
	return 0;
}



void tests()
{
	SecKeychainRef keychain = NULL;
	ok_status(SecKeychainSetUserInteractionAllowed(FALSE), "SecKeychainSetUserInteractionAllowed(FALSE)");
	ok_status(SecKeychainCreate("test", 4, "test", FALSE, NULL, &keychain), "SecKeychainCreate");

	SignVerifyWithAsyncTest();
	SignVerifyTest();
	SecItemTest();
	
	if (keychain) CFRelease(keychain);
}



int main(int argc, char * const *argv)
{
	plan_tests(34);
	if (!tests_begin(argc, argv))
		BAIL_OUT("tests_begin failed");
	tests();
	ok(tests_end(1), "cleanup");
	ok_leaks("no leaks");
	return 0;
}
