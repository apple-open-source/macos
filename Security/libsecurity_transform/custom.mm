//
//  custom.m
//  libsecurity_transform
//
//  Created by JOsborne on 2/18/10.
//  Copyright 2010 Apple. All rights reserved.
//


#import "SecTransform.h"
#import "SecCustomTransform.h"
#import "SecDigestTransform.h"
#import "SecEncryptTransform.h"
#import "SecEncodeTransform.h"
#import "SecDecodeTransform.h"
#import "SecSignVerifyTransform.h"
#import "SecNullTransform.h"
#import "SecExternalSourceTransform.h"
#import <Security/SecItem.h>
#import "misc.h"
#import "Utilities.h"
#import "SecNullTransform.h"
#include "regex.h"
#include <dispatch/dispatch.h>
#import "SecMaskGenerationFunctionTransform.h"
#import "SecTransformInternal.h"
#import "custom.h"
#include "SecTransformReadTransform.h"
#import "SecTransformValidator.h"
#include <sys/types.h>
#include <sys/sysctl.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <CommonCrypto/CommonCryptor.h>
#include <sys/stat.h>
#import "NSData+HexString.h"

// compatibility layer
struct SecTransformCreateBlockParameters {
	CFTypeRef (^send)(SecTransformStringOrAttributeRef attribute, SecTransformMetaAttributeType type, CFTypeRef value);
	CFTypeRef (^get)(SecTransformStringOrAttributeRef attribute, SecTransformMetaAttributeType type);
	CFTypeRef (^pushback)(SecTransformStringOrAttributeRef attribute, CFTypeRef value);
	CFErrorRef (^overrideTransform)(CFStringRef action, SecTransformActionBlock newAction);
	CFErrorRef (^overrideAttribute)(CFStringRef action, SecTransformStringOrAttributeRef attribute, SecTransformAttributeActionBlock newAction);
};

typedef void (^SecTransformCreateBlock)(CFStringRef name, SecTransformRef new_transform, const SecTransformCreateBlockParameters *params);

SecTransformCreateBlock global_create_block;

static SecTransformInstanceBlock block_for_custom_transform(CFStringRef name, SecTransformRef tr, SecTransformImplementationRef ir)
{	
	SecTransformInstanceBlock b = ^{
		// XXX: leak, need to override Finalize and clean up…   (and need to handle caller overriding finalize…)
		SecTransformCreateBlockParameters *params = static_cast<SecTransformCreateBlockParameters *>(malloc(sizeof(SecTransformCreateBlockParameters)));
		
		params->overrideAttribute = ^(CFStringRef action, SecTransformStringOrAttributeRef attribute, SecTransformAttributeActionBlock newAction) {
			// We don't need to special case ProcessData to call SecTransformSetDataAction as there are no longer any uses of it
			return SecTransformSetAttributeAction(ir, action, attribute, newAction);
		};
		
		params->overrideTransform = ^(CFStringRef action, SecTransformActionBlock newAction) {
			return SecTransformSetTransformAction(ir, action, newAction);
		};
		
		params->get = ^(SecTransformStringOrAttributeRef attribute, SecTransformMetaAttributeType type) {
			return SecTranformCustomGetAttribute(ir, attribute, type);
		};
		
		params->send = ^(SecTransformStringOrAttributeRef attribute, SecTransformMetaAttributeType type, CFTypeRef value) {
			return SecTransformCustomSetAttribute(ir, attribute, type, value);
		};
		
		params->pushback = ^(SecTransformStringOrAttributeRef attribute, CFTypeRef value) {
			return SecTransformPushbackAttribute(ir, attribute, value);
		};
		
		params->overrideAttribute = Block_copy(params->overrideAttribute);
		params->overrideTransform = Block_copy(params->overrideTransform);
		params->get = Block_copy(params->get);
		params->send = Block_copy(params->send);
		params->pushback = Block_copy(params->pushback);

		global_create_block(name, tr, params);
		
		return (CFErrorRef)NULL;
	};
	
	return Block_copy(b);
}

// Sort of a bridge from the old Custom SPI to the new API, but is also
// useful when you REALLY need to access stack locals as __block variables,
// but don't need multithreading, or generic internalizing.
SecTransformRef custom_transform(CFStringRef base_name, SecTransformCreateBlock cb)
{
	static int ct_cnt = 0;
	static dispatch_queue_t cnt_q = dispatch_queue_create("com.apple.security.custom_trasnform-cnt", 0);
	__block CFStringRef name = NULL;
	__block SecTransformRef ret = NULL;
	
	dispatch_sync(cnt_q, ^{
		CFErrorRef err = NULL;
		
		name = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@.%d"), base_name, ct_cnt++);
		global_create_block = cb;
		if (SecTransformRegister(name, block_for_custom_transform, &err)) {
			ret = SecTransformCreate(name, &err);
			if (err) {
				CFfprintf(stderr, "Error %@ creating %@\n", err, base_name);
				CFRelease(err);
			}
		} else {
			CFfprintf(stderr, "Error %@ registering %@\n", err, base_name);
			CFRelease(err);
		}
		global_create_block = NULL;
		CFRelease(name);
	});
	
	return ret;
}


#define STAssertErrorHas(err, rx, msg...) STAssertTrue(ErrorHas(err, rx), ##msg);

BOOL ErrorHas(NSError *error, NSString *rx) {
	if (!error) {
		return NO;
	}
	if (![error isKindOfClass:[NSError class]]) {
		return NO;
	}
	
	NSString *es = [error description];
	if (!es) {
		return NO;
	}
	return [es rangeOfString:rx options:NSRegularExpressionSearch].location != NSNotFound;
}


static SecTransformInstanceBlock DelayTransformBlock(CFStringRef name, 
							SecTransformRef newTransform, 
							SecTransformImplementationRef ref)
{
	SecTransformInstanceBlock instanceBlock = 
	^{
		CFErrorRef result = NULL;
				
								
		SecTransformSetDataAction(ref, kSecTransformActionProcessData,
			^(CFTypeRef value) 
			{
						
				if (NULL != value && CFNumberGetTypeID() == CFGetTypeID(value))
				{
					long long n;
					CFNumberGetValue((CFNumberRef)value, kCFNumberLongLongType, &n);
					usleep(n / NSEC_PER_USEC);
				}
				
				return value;
			});
		return result;
	};
	
	return Block_copy(instanceBlock);
}

SecTransformRef delay_transform(long long nsec) {
	CFStringRef name = CFSTR("com.apple.security.unit-test.delay");
	
	
	
	static dispatch_once_t once;
	__block Boolean ok = TRUE;
			
	dispatch_block_t aBlock = ^
	{
		ok = SecTransformRegister(name, &DelayTransformBlock, NULL);
	};
	
	dispatch_once(&once, aBlock);

	if (!ok) 
	{
		return NULL;
	}
	
	SecTransformRef ct = SecTransformCreate(name, NULL);
	CFNumberRef nr = CFNumberCreate(NULL, kCFNumberLongLongType, &nsec);
	SecTransformSetAttribute(ct, CFSTR("DELAY"), nr, NULL);
	CFRelease(nr);
	
	return ct;
}

@implementation custom

class BufferStream
{
protected:
	const char* mBuffer;
	size_t mLength;
	size_t mStringLength;
	size_t mPos;

	char *mCurrentString;

public:
	BufferStream(const char* buffer, size_t length) : mBuffer(buffer), mLength(length), mStringLength(0), mPos(0), mCurrentString(NULL) {}
	~BufferStream();
	
	const char* GetNextString();
	void SplitString(const char*& stringA, const char*& stringB);
};



BufferStream::~BufferStream()
{
	if (NULL != mCurrentString)
	{
		free(mCurrentString);
	}
}



const char* BufferStream::GetNextString()
{
	size_t p = mPos;
	if (p >= mLength)
	{
		return NULL; // eof
	}
	
	// run to either the end of the buffer or a return
	while (p < mLength && mBuffer[p] != '\n')
	{
		p += 1;
	}
	
	if (p != mLength)
	{
		// handle the end of the buffer specially, since it doesn't point
		// to valid space
		p -= 1;
	}
	
	// p now points to the last character in the string
	// allocate memory for our buffer
	mStringLength = p - mPos + 1;
	mCurrentString = (char*) realloc(mCurrentString, mStringLength + 1);
	memmove(mCurrentString, mBuffer + mPos, mStringLength);
	mCurrentString[mStringLength] = 0;
	mPos = p + 2;
	
	return mCurrentString;
}



void BufferStream::SplitString(const char*& a, const char*& b)
{
	// scan the buffer, looking for a ':'
	size_t p = 0;
	while (mCurrentString[p] != 0 && mCurrentString[p] != ':')
	{
		p += 1;
	}
	
	// the first string is always our buffer pointer
	a = mCurrentString;

	if (mCurrentString[p] == ':')
	{
		mCurrentString[p] = 0;
		
		// look for the beginning of the next string
		p += 1;
		while (p < mLength && isspace(mCurrentString[p]))
		{
			p += 1;
		}
		
		b = mCurrentString + p;
	}
	else
	{
		b = NULL;
	}
}



-(void)disabledtestzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz
{
	// open leaks and make a connection to it.
	char* name;

	const int kChunkSize = 16384;
	char buffer[kChunkSize];
	pid_t thePid = getpid();
	asprintf(&name, "/tmp/leaks%d.txt", thePid);
	sprintf(buffer, "/usr/bin/leaks %d >%s", thePid, name);
	system(buffer);
	
	struct stat st;
	stat(name, &st);
	
	char* rBuffer = (char*) malloc(st.st_size);
	FILE* f = fopen(name, "r");
	fread(rBuffer, 1, st.st_size, f);
	fclose(f);
	
	// set up our output parser
	BufferStream bStream(rBuffer, st.st_size);
	const char* s = bStream.GetNextString();
	
	bool didError = true;

	if (NULL != s)
	{
		// we have our string, split it and see what it means
		const char* key;
		const char* value;
		
		bStream.SplitString(key, value);
		if (strcmp(key, "leaks Report Version") != 0 || strcmp(value, "2.0") != 0)
		{
			didError = true;
		}
		else
		{
			didError = false;
		}
	}

	if (!didError)
	{
		const char* key;
		const char* value;

		// figure out what our target line will look like
		char* target;
		asprintf(&target, "Process %d", thePid);
		
		const char* nextString = bStream.GetNextString();
		while (nextString)
		{
			bStream.SplitString(key, value);
			if (strcmp(key, target) == 0) // we found our target!!!
			{
				// do it again
				bStream.GetNextString();
				bStream.SplitString(key, value);
				
				if (value[0] != '0') // we have a non-zero result... :(
				{
					didError = true;
				}
			}
			
			nextString = bStream.GetNextString();
		}
		
		free(target);
	}
	
	STAssertFalse(didError, @"You have leaks!");
	
	if (didError)
	{
		// dump to our output file
		// make a file name for the leaks output
		FILE* f = fopen(name, "w");
		fwrite(rBuffer, 1, st.st_size, f);
		fclose(f);
	}
	else
	{
		unlink(name);
	}
	
	free(name);
}



static const char* gHMACText = "The judicial Power shall extend to all Cases, in "
							   "Law and Equity, arising under this Constitution, "
							   "the Laws of the United States, and Treaties made, "
							   "or which shall be made, under their Authority;--to "
							   "all Cases affecting Ambassadors, other public "
							   "Ministers and Consuls;--to all Cases of admiralty "
							   "and maritime Jurisdiction;--to Controversies to "
							   "which the United States shall be a Party;--to "
							   "Controversies between two or more States;-- "
							   "between a State and Citizens of another State, "
							   "--between Citizens of different States,-- "
							   "between Citizens of the same State claiming Lands "
							   "under Grants of different States, and between a "
							   "State, or the Citizens thereof, and foreign "
							   "States, Citizens or Subjects";

const NSString* gAbortTransformName = (NSString*) kSecTransformAbortAttributeName;

static const char* gHMACKey = "No person shall be held to answer for a capital, or "
							  "otherwise infamous crime, unless on a presentment "
							  "or indictment of a Grand Jury, except in cases "
							  "arising in the land or naval forces, or in the "
							  "Militia, when in actual service in time of War or "
							  "public danger; nor shall any person be subject for "
							  "the same offence to be twice put in jeopardy of life "
							  "or limb; nor shall be compelled in any criminal case "
							  "to be a witness against himself, nor be deprived of "
							  "life, liberty, or property, without due process of "
							  "law; nor shall private property be taken for public "
							  "use, without just compensation.";

static const u_int8_t gSHA1HMAC[] = {0x2f, 0x68, 0x4b, 0x6b, 0x4f,
									 0xf7, 0x41, 0xc3, 0x76, 0x3d,
									 0x0b, 0xc3, 0x25, 0x02, 0x99,
									 0x03, 0xfa, 0xa5, 0xe9, 0xde};

static const u_int8_t gSHA256HMAC[] = {0xc2, 0x5c, 0x9a, 0x65, 0x08, 0x9e, 0x61, 0xb5,
									   0x03, 0xfe, 0xcb, 0x57, 0xb7, 0x55, 0x4f, 0x69,
									   0xdb, 0xef, 0xdb, 0xe7, 0x0d, 0xe2, 0x78, 0x2e,
									   0xf9, 0x48, 0xbd, 0xf6, 0x4f, 0x4b, 0x94, 0x0c};
-(void)testPaddings
{
    CFStringRef paddings[] = {kSecPaddingNoneKey, kSecPaddingPKCS7Key, kSecPaddingPKCS5Key, kSecPaddingPKCS1Key};

    for(int i = 0; i < sizeof(paddings) / sizeof(*paddings); i++) {
        CFErrorRef error = NULL;
        SecKeyRef cryptokey = NULL;
        SecTransformRef encrypt = NULL, decrypt = NULL;
        CFDataRef cfdatacryptokey = NULL, sourceData = NULL, encryptedData = NULL, decryptedData = NULL;
        const uint8_t rawcryptokey[16] = { 63, 17, 27, 99, 185, 231, 1, 191, 217, 74, 141, 16, 12, 99, 253, 41 }; // 128-bit AES key.
        const char *sourceCString = "All these worlds are yours except Europa.";  // I'm not so sure about that Earth one either

        CFMutableDictionaryRef parameters = CFDictionaryCreateMutable(
                                                                      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                                                      &kCFTypeDictionaryValueCallBacks);

        CFDictionarySetValue(parameters, kSecAttrKeyType, kSecAttrKeyTypeAES);

        cfdatacryptokey = CFDataCreate(kCFAllocatorDefault, rawcryptokey,
                                       sizeof(rawcryptokey));
        cryptokey = SecKeyCreateFromData(parameters,
                                         cfdatacryptokey, &error);
        STAssertNil((id)error, @"Unexpected SecKeyCreateFromData error: %@", error);

        size_t len = strlen(sourceCString) +1;
        if (paddings[i] == kSecPaddingNoneKey) {
            STAssertTrue(len >= kCCBlockSizeAES128, @"Had at least one block");
            // Get to an AES block multiple, discarding bytes wildly.
            len -= len % kCCBlockSizeAES128;
        }
        sourceData = (CFDataRef)[NSData dataWithBytes:sourceCString length:len];

        encrypt = SecEncryptTransformCreate(cryptokey, &error);
        STAssertNil((id)error, @"Unexpected error creating encrypt transform: %@", error);
        decrypt = SecDecryptTransformCreate(cryptokey, &error);
        STAssertNil((id)error, @"Unexpected error creating decrypt transform: %@", error);

        /* Set the padding on the transforms */
        SecTransformSetAttribute(encrypt, kSecPaddingKey, paddings[i], &error);
        STAssertNil((id)error, @"Couldn't set encrypt padding to %@: %@", paddings[i], error);
        SecTransformSetAttribute(decrypt, kSecPaddingKey, paddings[i], &error);
        STAssertNil((id)error, @"Couldn't set decrypt padding to %@: %@", paddings[i], error);

        SecTransformSetAttribute(encrypt, kSecTransformInputAttributeName, sourceData, &error);
        STAssertNil((id)error, @"Couldn't set encrypt transform input: %@", error);

        encryptedData = (CFDataRef)SecTransformExecute(encrypt, &error);
        STAssertNil((id)error, @"Couldn't execute encrypt: %@ (padding %@)", paddings[i], error);
        STAssertNotNil((id)encryptedData, @"Didn't get encrypted data");

        SecTransformSetAttribute(decrypt, kSecTransformInputAttributeName, encryptedData, &error);
        STAssertNil((id)error, @"Couldn't set decrypt transform input: %@", error);

        decryptedData = (CFDataRef)SecTransformExecute(decrypt, &error);
        STAssertNil((id)error, @"Couldn't execute decrypt: %@", error);
        STAssertNotNil((id)decryptedData, @"Didn't get decrypt data");

        STAssertEqualObjects((id)decryptedData, (id)sourceData, @"Decrypt output didn't match encrypt input for padding %@", paddings[i]);
    }
}

static SecTransformInstanceBlock nopInstance(CFStringRef name, SecTransformRef newTransform, SecTransformImplementationRef ref)
{
    SecTransformInstanceBlock instanceBlock = ^{
        return (CFErrorRef)NULL;
    };
    
    return Block_copy(instanceBlock);
}


-(void)test_manyregister
{
    dispatch_apply(4000, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, NULL), ^(size_t i) {
        NSString *name = [NSString stringWithFormat:@"many%dregister", i];
        CFErrorRef err = NULL;
        BOOL ok = SecTransformRegister((CFStringRef)name, nopInstance, &err);
        STAssertTrue(ok, @"register not ok");
        STAssertNil((id)err, @"register error: %@", err);
    });
}

-(void)test_emptyOAEP
{
    SecKeychainRef tmp_keychain = NULL;
    char *kcfname;
    asprintf(&kcfname, "%s-OAEP-XXXXXXXXXX", "/tmp/");
    const char *passwd = "sekret";
    // NOTE: "mktemp" isn't as safe as you might think...but this is test code and doesn't have to be, but
    // if you copy it elsewhere you may well need to rewrite it.  (use mkstemp)
    mktemp(kcfname);			
    OSStatus status = SecKeychainCreate(kcfname, strlen(passwd), passwd, NO, NULL, &tmp_keychain);
    STAssertTrue(status == 0, @"Expected to make keychain, but got error 0x%x", status);
    
    const char *pem_key_bytes[] = {
        // From the spec
        "-----BEGIN PUBLIC KEY-----\nMIGdMA0GCSqGSIb3DQEBAQUAA4GLADCBhwKBgQC7+C8JBoLOnCM4rCudqHH3No0H\n7tQQQ6RA1rbwdFT1H7jfuq8DXAKrYepIzutvzUh27VINYOHsRhlxnYpbi4B/r7jg\no9/HN3I+5rS32TolhO5qZJ0GCVN0iDSyRUWYOU7gqrEte2GlH1J6mkH2wWh/4lNy\nmMoqj1lG+OX9CR29ywIBEQ==\n-----END PUBLIC KEY-----\n-----BEGIN RSA PRIVATE KEY-----\nMIICWwIBAAKBgQC7+C8JBoLOnCM4rCudqHH3No0H7tQQQ6RA1rbwdFT1H7jfuq8D\nXAKrYepIzutvzUh27VINYOHsRhlxnYpbi4B/r7jgo9/HN3I+5rS32TolhO5qZJ0G\nCVN0iDSyRUWYOU7gqrEte2GlH1J6mkH2wWh/4lNymMoqj1lG+OX9CR29ywIBEQKB\ngQCl2vxTQfryicS5iNswwc34PzElHgZotCeEgTgBV5ZBspQQs8eZjWvEZXReXDkm\nadaHDaLAgqk543/cuC7JPtrJf/OtWVCsz7wRHHbxqVKUROVqr2jFbAks043DvvXS\nCpOZJu1PdKE+3fvhoc7MSJSvlCjCt7iIP+RGOkvIWxyzwQJBAO7ProGxubPJCIEL\nEKG1YAGZ659ErvT9pJO4Gp49hPYyEk7wI25dHjt+KPrnqgQKLVslIXZFnR85dUG6\nKlj7ZZkCQQDJf7HwJ/RT9jQSM+qq0dk1P2xC0IhmsdBaDyA1AoudhphAtBZmtC6S\n6g2jtDIEtc/OM1JSTQQWpaRB5wCvRhUDAkBUSUymProDN+TiQCP81ppa6wfd3AGD\npNCsm1SwUfKxPtlJCXXqt3QU/1nB92kumi4gKzj8kQpHQXStyTwfZ8mBAkBHHgKQ\n/wrwdQNRt/h4hkypYa29Oop+mRxcBVapTDFGp/mAP49viuNC6TH9iuR6Ig0bmaSV\nhJgH/jn5JFqYNto9AkEAsGxP2rtjARmNJlvbrpQjs4Dycfc0U4hQkwd/zTniEZ/J\nhjIVT1iDsWepZ79AK06eLg+WVuaY6jZm7fsleYA59w==\n-----END RSA PRIVATE KEY-----\n",
        NULL,
    };
    struct key_pair {
        SecKeyRef pubKey, privKey;
    };
    key_pair keys[1];
    
    int i;
    for(i = 0; i < sizeof(keys)/sizeof(key_pair); i++) {
        NSAssert(pem_key_bytes[i] != NULL, @"Expected a key");
        NSLog(@"Importing: %s", pem_key_bytes[i]);
        CFDataRef pem_data = CFDataCreate(NULL, (UInt8*)(pem_key_bytes[i]), strlen(pem_key_bytes[i]));
        SecKeyImportExportParameters import_params;
        bzero(&import_params, sizeof(import_params));
        
        import_params.version = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION;
        import_params.keyUsage = CSSM_KEYUSE_ANY;
        import_params.keyAttributes = CSSM_KEYATTR_PERMANENT|CSSM_KEYATTR_SENSITIVE;
        import_params.accessRef = NULL;
        import_params.passphrase = CFSTR("");
        import_params.alertPrompt = CFSTR("");
        
        CFArrayRef keypair = NULL;
        SecExternalFormat key_format = kSecFormatOpenSSL;
        SecExternalItemType itemType = kSecItemTypeUnknown;
        status = SecKeychainItemImport(pem_data, CFSTR(".pem"), &key_format, &itemType, 0, &import_params, tmp_keychain, &keypair);
        STAssertTrue(status == 0, @"Expected pubkey import to be ok, got err=0x%x", status);
        NSAssert(keypair != NULL, @"Expected to get some keys back");
        STAssertNotNil((id)keypair, @"Expected to get some keys back");
        STAssertTrue(CFArrayGetCount(keypair) == 2, @"Expected 2 keys, got %@", keypair);
        keys[i].pubKey = (SecKeyRef)CFArrayGetValueAtIndex(keypair, 0);
        keys[i].privKey = (SecKeyRef)CFArrayGetValueAtIndex(keypair, 1);
    }
    STAssertNil((id)pem_key_bytes[i], @"Expected to convert all pem keys, but found at least: %s", pem_key_bytes[i]);
    CFDataRef encoding_parameters = CFDataCreate(NULL, NULL, 0);
    
	
	CFErrorRef err = NULL;
	
	SecTransformRef encryptor = SecEncryptTransformCreate(keys[0].pubKey, &err);
	
	CFReadStreamRef empty_stream = CFReadStreamCreateWithBytesNoCopy(NULL, (UInt8*)"", 0, kCFAllocatorNull);
	SecTransformSetAttribute(encryptor, kSecTransformInputAttributeName, empty_stream, &err);
	SecTransformSetAttribute(encryptor, kSecPaddingKey, kSecPaddingOAEPKey, &err);
	SecTransformSetAttribute(encryptor, kSecOAEPEncodingParametersAttributeName, encoding_parameters, &err);
	
	CFTypeRef encryptedData = SecTransformExecute(encryptor, &err);
	STAssertNotNil((id)encryptedData, @"Expected to get encrypted data");
	STAssertNil((NSError*)err, @"Expected no error, got err=%@", err);
	// Can't support "seed" with commoncrypto, just check round trip.
	//STAssertEqualObjects((id)encryptedData, (id)tests[i].encryptedMessage, @"encrypted data should have matched test vector (%@) data", tests[i].label);
	CFRelease(encryptor);
	
	SecTransformRef decryptor = SecDecryptTransformCreate(keys[0].privKey, NULL);
	// XXX: totally round trip, not even partial KAT (KAT can't really be done on OAEP
	// without supporitng settign the seed externally)
	SecTransformSetAttribute(decryptor, kSecTransformInputAttributeName, encryptedData, NULL);
	SecTransformSetAttribute(decryptor, kSecPaddingKey, kSecPaddingOAEPKey, NULL);
	SecTransformSetAttribute(decryptor, kSecOAEPEncodingParametersAttributeName, encoding_parameters, NULL);
	CFTypeRef decryptedData = SecTransformExecute(decryptor, &err);
	STAssertNil((id)err, @"Expected no error, got: %@", err);
	STAssertNotNil((id)decryptedData, @"Expected to get decrypted data");
	// What do we expect an empty enc/dec to look like?   Mostly "not a crash"
	CFDataRef empty_data = CFDataCreate(NULL, (UInt8*)"", 0);
	STAssertEqualObjects((id)decryptedData, (id)empty_data, @"Expected decrypted data to match original message");
	CFRelease(decryptor);
    sleep(5);
	
    return;
}

-(void)testzzzzZZZZ
{
	// Give xcode a little time to parse all the output before the unit tests exit
	sleep(2);
}

-(void)test_multiOAEP
{
    SecKeychainRef tmp_keychain = NULL;
    char *kcfname;
    asprintf(&kcfname, "%s-OAEP-XXXXXXXXXX", "/tmp/");
    const char *passwd = "sekret";
    // NOTE: "mktemp" isn't as safe as you might think...but this is test code and doesn't have to be, but
    // if you copy it elsewhere you may well need to rewrite it.  (use mkstemp)
    mktemp(kcfname);			
    OSStatus status = SecKeychainCreate(kcfname, strlen(passwd), passwd, NO, NULL, &tmp_keychain);
    STAssertTrue(status == 0, @"Expected to make keychain, but got error 0x%x", status);
    
    const char *pem_key_bytes[] = {
        // From the spec
        "-----BEGIN PUBLIC KEY-----\nMIGdMA0GCSqGSIb3DQEBAQUAA4GLADCBhwKBgQC7+C8JBoLOnCM4rCudqHH3No0H\n7tQQQ6RA1rbwdFT1H7jfuq8DXAKrYepIzutvzUh27VINYOHsRhlxnYpbi4B/r7jg\no9/HN3I+5rS32TolhO5qZJ0GCVN0iDSyRUWYOU7gqrEte2GlH1J6mkH2wWh/4lNy\nmMoqj1lG+OX9CR29ywIBEQ==\n-----END PUBLIC KEY-----\n-----BEGIN RSA PRIVATE KEY-----\nMIICWwIBAAKBgQC7+C8JBoLOnCM4rCudqHH3No0H7tQQQ6RA1rbwdFT1H7jfuq8D\nXAKrYepIzutvzUh27VINYOHsRhlxnYpbi4B/r7jgo9/HN3I+5rS32TolhO5qZJ0G\nCVN0iDSyRUWYOU7gqrEte2GlH1J6mkH2wWh/4lNymMoqj1lG+OX9CR29ywIBEQKB\ngQCl2vxTQfryicS5iNswwc34PzElHgZotCeEgTgBV5ZBspQQs8eZjWvEZXReXDkm\nadaHDaLAgqk543/cuC7JPtrJf/OtWVCsz7wRHHbxqVKUROVqr2jFbAks043DvvXS\nCpOZJu1PdKE+3fvhoc7MSJSvlCjCt7iIP+RGOkvIWxyzwQJBAO7ProGxubPJCIEL\nEKG1YAGZ659ErvT9pJO4Gp49hPYyEk7wI25dHjt+KPrnqgQKLVslIXZFnR85dUG6\nKlj7ZZkCQQDJf7HwJ/RT9jQSM+qq0dk1P2xC0IhmsdBaDyA1AoudhphAtBZmtC6S\n6g2jtDIEtc/OM1JSTQQWpaRB5wCvRhUDAkBUSUymProDN+TiQCP81ppa6wfd3AGD\npNCsm1SwUfKxPtlJCXXqt3QU/1nB92kumi4gKzj8kQpHQXStyTwfZ8mBAkBHHgKQ\n/wrwdQNRt/h4hkypYa29Oop+mRxcBVapTDFGp/mAP49viuNC6TH9iuR6Ig0bmaSV\nhJgH/jn5JFqYNto9AkEAsGxP2rtjARmNJlvbrpQjs4Dycfc0U4hQkwd/zTniEZ/J\nhjIVT1iDsWepZ79AK06eLg+WVuaY6jZm7fsleYA59w==\n-----END RSA PRIVATE KEY-----\n",
        // The next 10 are from oaep-vect.txt (via a lot of OpenSSL higgerdy-jiggerdey)
        "-----BEGIN PUBLIC KEY-----\nMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQCos7KEr461CzhwNKhg8UbEkZ8x\nh2PNbFWYyK5IEaHgq8TH4LCC1pOl5/ztZ1z0ZoUSdywMvGSnQsbGMPUzyMxy9iro\nM8QL8lhC6YS7eL2/l8AQfVW9tmL1xOD6uYRctRSO9zkt06r/k64ea2Z7s9QkdhbU\n9boQ1M/SJt6I058W+wIDAQAB\n-----END PUBLIC KEY-----\n-----BEGIN RSA PRIVATE KEY-----\nMIICXAIBAAKBgQCos7KEr461CzhwNKhg8UbEkZ8xh2PNbFWYyK5IEaHgq8TH4LCC\n1pOl5/ztZ1z0ZoUSdywMvGSnQsbGMPUzyMxy9iroM8QL8lhC6YS7eL2/l8AQfVW9\ntmL1xOD6uYRctRSO9zkt06r/k64ea2Z7s9QkdhbU9boQ1M/SJt6I058W+wIDAQAB\nAoGAUzOc/befyEZqZVxzFqyoXFX9j23YmP2vEZUX709S6P2OJY35P+4YD6Dkqylp\nPNg7FSpVPUrE0YEri5+lrw5/Vf5zBN9BVwkm8zEfFcTWWnMsSDEW7j09LQrzVJrZ\nv3y/t4rYhPhNW+sEck3HNpsx3vN9DPU56c/N095lNynq1dECQQDTJzfnJn/+E0Gy\n1cDRUKgbWG+zEyvtL41SYoZKnLnzCvOL5EhZjUE6Fy77gCwhrPHBHFIMLyakcdyt\nIS6sfKOdAkEAzIhT0dVNpjD6wAT0cfKBx7iYLYIkpJDtvrM9Pj1cyTxHZXA9HdeR\nZC8fEWoN2FK+JBmyr3K/6aAw6GCwKItddwJADhK/FxjpzvVZm6HDiC/oBGqQh07v\nzo8szCDk8nQfsKM6OEiuyckwX77L0tdoGZZ9RnGsxkMeQDeWjbN4eOaVwQJBAJUp\new+Vovpn0AcH1gnf1PwFyJ2vwu9tbqVb7HceozNzTZJR55CC7NqGbv7xPEWeGmMT\nhrfjVMiZ9fESyoXXFYMCQE9FbFAkk73A7Sq3VqOm7U1nNSppfUIW6TISsSemPVQR\nzm+pjV2+/XMmPjcoFCdDgYFm7X3WNofdKoyh0vT72OE=\n-----END RSA PRIVATE KEY-----\n",
        "RSA key ok\n-----BEGIN PUBLIC KEY-----\nMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQGUfH/OkEJfRyeecIUfJdXmIxb+\nih3xk3Hj5ijiYFQ+SQHvYIH2jAuBQRkNKujaun0SUOxttjbpROw3Iod8fB0KZ/FL\nFpTF8DeUUaQ+SaMt3oNnC3PakaHJm8I7Q2pgBVxhDwuvmcGgeVZblaPxUmYy0dTa\nYPIO2iXmU8TwAnZvRQIDAQAB\n-----END PUBLIC KEY-----\n-----BEGIN RSA PRIVATE KEY-----\nMIICXAIBAAKBgQGUfH/OkEJfRyeecIUfJdXmIxb+ih3xk3Hj5ijiYFQ+SQHvYIH2\njAuBQRkNKujaun0SUOxttjbpROw3Iod8fB0KZ/FLFpTF8DeUUaQ+SaMt3oNnC3Pa\nkaHJm8I7Q2pgBVxhDwuvmcGgeVZblaPxUmYy0dTaYPIO2iXmU8TwAnZvRQIDAQAB\nAoGAaHJZomJX8Thzf5M4nNltSXcIKgRKRSY4w4ucRRBw0ICTslduV9bD5cWEjYTm\nCg0b3M3ur0ndFhFJGdedusRlzrJ3phMQcCvg8AygYOPN4gqYbIqz7xshfRxwQoGT\nGwFbOc4FQzlmlGna+VJDZ8sxykucXXKZh+wfN0vR7xXmj6UCQQFZ294Eoz7wb7YI\nuAsZD00+IrzBOsjkoIEDOr+kFu2wsziqCLVzCepaUkDn3G5UN4xpQUwx2X3bH0Bt\ns3acxBpDAkEBK2UvMEA7OLQJlf1v9BoazIracDcyNrcgLTmy7jDPtG2wlRH28wfM\nYcwhYGwYp1uKYvgi3wMboN8Nr9VQb1aL1wJAQ271CN5zZRnC2kxYDZjILLdFKj+1\n763Ducd4mhvGWE95Wt270yQ5x0aGVS7LbCwwek069/U57sFXJIx7MfGiVQJBASsV\nqJ89+ys5Bz5z8CvdDBp7N53UNfBc3eLv+eRilIt87GLukFDV4IFuB4WoVrSRCNy3\nXzaDh00cpjKaGQEwZv8CQAJw2xfVkUsBjXYRiyQ4mnNQ7INrAGOiFyEjb9jtttib\nUefuuHthG3Eyy36nNWwjFRwed1FQfHhtnuF5QXCoyOg=\n-----END RSA PRIVATE KEY-----\n",
        "RSA key ok\n-----BEGIN PUBLIC KEY-----\nMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQK1j+wDmoYHAKTXtkYvk+bN1JEW\nHd109OgQtA48FlIAalwneyd0wRMFpMurWnjvpX4XqG33o/o2/EsdIknyLsfC3WpG\nMjKszqkG1m6+gLVwSxBynab4MyNKu1791KKSy/rTO00z+noUuMOXtW46zSEgNCi3\nfN+jOm2nBrPYsPxD6QIDAQAB\n-----END PUBLIC KEY-----\n-----BEGIN RSA PRIVATE KEY-----\nMIICXQIBAAKBgQK1j+wDmoYHAKTXtkYvk+bN1JEWHd109OgQtA48FlIAalwneyd0\nwRMFpMurWnjvpX4XqG33o/o2/EsdIknyLsfC3WpGMjKszqkG1m6+gLVwSxBynab4\nMyNKu1791KKSy/rTO00z+noUuMOXtW46zSEgNCi3fN+jOm2nBrPYsPxD6QIDAQAB\nAoGAFbSKW1aDqUZw4jtXGPgU+g4T+FA49QcRGCy6YVEFgfPSLH4jLvk34i5VHWi4\nbi+MsarYvi5Ij13379J54/Vo1Orzb4DPcUGs5g/MkRP7bEqEH9ULvHxRL/y+/yFI\neqgR6zyoxiAFNGqG3oa/odipSP0/NIwi6q3zM8PObOEyCP0CQQG/AdIW1zWVzwJw\nwr63jUCg2ER9MdqRmpg/fup4G3fYX+Nxs+k3PntpIX0xUKAtiVjef62dVVFglYtE\nVBJ+Dn6vAkEBjTOZZYFm2zgpgW17KVQWdZ6ckZh/Wy2K7NY7BLSL17L88im7f4pt\nyIuhPdLjmtVbbRoGFgcI+XAL6AuP03RM5wJABsCiSdIKby7nXIi0lNU/aq6ZqkJ8\niMKLFjp2lEXl85DPQMJ0/W6mMppc58fOA6IVg5buKnhFeG4J4ohalyjk5QJBANHS\nfCn+3ZLYbDSO3QzL+sFPdG4FHOHRgR3zXWHy7hyX1L8oBIAvZCcYe6jpCor0QkO0\nB5sDRF5gLin6UZPmT+kCQQCMsvdWvYlBsdO3cOWtMe43Oyis2mn/m29A/leLnxr7\nhYNvlifTes/3PCd55jS7JgEcLI9/M2GuKp6mXtaJ42Oa\n-----END RSA PRIVATE KEY-----\n",
        "RSA key ok\n-----BEGIN PUBLIC KEY-----\nMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQUSQLbMAAT6SNATRnHAeMfI3sOz\n4vJbwlZEZzOds4hT0GuF7qWy3jU7/0KsLka8l/rmrJYY2pU3pcj1U8HjV2JZkdYQ\njc14hfs6JUE/U+/K2UjLNc2bmunBxnYm0RPVfd5MW+p2u1u33pbADQc3LpaFptdc\n+dI5+hSNcJMbXz+wOQIDAQAB\n-----END PUBLIC KEY-----\n-----BEGIN RSA PRIVATE KEY-----\nMIICXgIBAAKBgQUSQLbMAAT6SNATRnHAeMfI3sOz4vJbwlZEZzOds4hT0GuF7qWy\n3jU7/0KsLka8l/rmrJYY2pU3pcj1U8HjV2JZkdYQjc14hfs6JUE/U+/K2UjLNc2b\nmunBxnYm0RPVfd5MW+p2u1u33pbADQc3LpaFptdc+dI5+hSNcJMbXz+wOQIDAQAB\nAoGBAQs6yuW+80dZiYsOKwgFVIpiYEI86so8fGlkHNnPRLaL5jYRY4Yn+yk4Z87t\nT54uYnTs/ZBsHd7wfycQcI6NRC5hgVY5sbQKDJDJIHgDPvxewvmE+mgbRFo7v4RH\nGGacGivrZVhXdDMpOm3KyxRfToWUJIq6IhT0AeURYrezGJABAkECdFjBnsFjaRnn\nNsmvJdYJpRuPVh0Zxr9pQ90e4auKSj8jIQC9QLiN7Ma6I1VItu95KhHJ3oI9Cnki\nxwlbbrpXAQJBAhDumzOrYXFuJ9JRvUZfSzWhojLi2gCQHClL8iNQzkkNCZ9kK1N1\nYS22O6HyA4ZJK/BNNLPCK865CdE0QbU7UTkCQDn6AouCbojBEht1CoskL6mjXFtm\nvf0fpjfTzEioSk9FehlOdyfkn3vMblpaQSZX/EcMcyLrw3QW70WMMHqMCQECQQFd\nmahBlZQ5efqeG+LDwbafQy9G/QPkfVvvu7/WsdE3HYPvszCj4CCUKy/tEV5dAr4k\n/ZLJAZ0c7NbdTPHlTMiZAkEB8LcBUXCz9eQiI7owMBxBpth8u3DjDLfTxn0lRz2x\n9svwPj+RJuPpeWgnmoZbLCtCZSTPxSpoPTHtMOuYS+QSug==\n-----END RSA PRIVATE KEY-----\n",
        "-----BEGIN PUBLIC KEY-----\nMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQqt8/nBJeXYkfMaxEjpk97+WA+A\nK0X51/IrpQIenEdXa1oeaAMbqdtObavk2Wodbz0mcmjP9AgAXxGO/K25mIjRwjRG\ncWayorhJoFqInAYKwNoMX66LVfMJumLnA3QvoDJvLRCwEQIUif9Jd3AZDYlf059S\nKTw579c6aYvaufEO2QIDAQAB\n-----END PUBLIC KEY-----\n-----BEGIN RSA PRIVATE KEY-----\nMIICXwIBAAKBgQqt8/nBJeXYkfMaxEjpk97+WA+AK0X51/IrpQIenEdXa1oeaAMb\nqdtObavk2Wodbz0mcmjP9AgAXxGO/K25mIjRwjRGcWayorhJoFqInAYKwNoMX66L\nVfMJumLnA3QvoDJvLRCwEQIUif9Jd3AZDYlf059SKTw579c6aYvaufEO2QIDAQAB\nAoGBAlbrTLpwZ/LSvlQNzf9FgqNrfTHRyQmbshS3mEhGaiaPgPWKSawEwONkiTSg\nIGwEU3wZsjZkOmCCcyFE33X6IXWI95RoK+iRaCdtxybFwMvbhNMbvybQpDr0lXF/\nfVKKz+40FWH2/zyuBcV4+EcNloL5wNBy+fYGi1bViA9oK+LFAkEDsNOWL20XVJy/\nyhEpQ0jc8Ofjn4wrxoJPIWS2BtaHhg2uHmMjk8/t9RMigikGni9g5KzX5jOkNgY/\ngjhfSJk3BwJBAuTDLi9Rcmm3ByMJ8AwOMTZffOKLI2uCkS3yOavzlXLPDtYEsCmC\n5TVkxS1qBTl95cBSov3cFB73GJg2NGrrMx8CQQHoSxGdJRYfpnsAJWpb2bZF0rIy\n7LBbAVGAApqIYircPwmzrqzeYWGrfN4iwq0m53l99U4HLL07JnOACz5DONvVAkEA\n65CqGkATW0zqBxl87ciBm+Hny/8lR2YhFvRlpKn0h6sS87pP7xOCImWmUpfZi3ve\n2TcuP/6Bo4s+lgD+0FV1TwJBAS9/gTj5QEBi64WkKSRSCzj1u4hqAZb0i7jc6mD9\nkswCfxjngVijSlxdX4YKD2wEBxp9ATEsBlBi8etIt50cg8s=\n-----END RSA PRIVATE KEY-----\n",
        "-----BEGIN PUBLIC KEY-----\nMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgRKxf22tLs0Z/0bcE/eGDwng4M+2\nd7OKUlkjBc6vAiwWbbkNBKwp4z990S2fr2bggWu2Pq0mfMfUbBfDe+IUvKKiLXI6\nZOREB0Nrb8llcprvwlVPN2zV3OpoKTeApivznQApSFoWC7ueXcCXLSGlBPUuXuAo\nqkFjMvUQsunP9fcirwIDAQAB\n-----END PUBLIC KEY-----\n-----BEGIN RSA PRIVATE KEY-----\nMIICXwIBAAKBgRKxf22tLs0Z/0bcE/eGDwng4M+2d7OKUlkjBc6vAiwWbbkNBKwp\n4z990S2fr2bggWu2Pq0mfMfUbBfDe+IUvKKiLXI6ZOREB0Nrb8llcprvwlVPN2zV\n3OpoKTeApivznQApSFoWC7ueXcCXLSGlBPUuXuAoqkFjMvUQsunP9fcirwIDAQAB\nAoGBApXso1YGGDaVWc7NMDqpz9r8HZ8GlZ33X/75KaqJaWG80ZDcaZftp/WWPnJN\nB7TcEfMGXlrpfZaDURIoC5CEuxTyoh69ToidQbnEEy7BlW/KuLsv7QV1iEk2Uixf\n99MyYZBIJOfK3uTguzctJFfPeOK9EoYij/g/EHMc5jyQz/P5AkEEps6Lc1jfppvc\n90JhcAWvtThfXzpYok73SiKowFy3zDjr1Mydmp14mmLND2Dwy5QdNCPJaS76T+Ot\n/ykMR0mjiwJBBATJqAM3H+20xb4588ALAJ5eCKY74eQANc2spQEcxwHPfuvLmfD/\n4Xz9Ckv3vv0t1TaslG23l/28Sr6PKTSbke0CQQOWHI92CqK9UVTHqv13Ils7rNAT\nmue1lI6jMR/M2G+5XHWvp2coS5st5VlXLxXY0ETH64Ohvl+t8sw3fA2EdSlLAkEC\nIZfgZnQhlqq8A/ov7rTnCxXLeH1hes0xu3XHvCNK1wb3xI0hgtHw/5wijc9Blnts\nC6bSwK0RChuFeDHsJF4ssQJBBAHEwMU9RdvbXp2W0P7PQnXfCXS8Sgc2tKdMMmkF\nPvtoas4kBuIsngWN20rlQGJ64v2wgmHo5+S8vJlNqvowXEU=\n-----END RSA PRIVATE KEY-----\n",
        "-----BEGIN PUBLIC KEY-----\nMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgTERefC8/JudPKMV0A7zDXvdOiz6\n6ZEb/ty5SLOkeC0HMrarRKpL8DdBpkTcAb7D5psBoDPmddis18SSXGsa7DEZBR39\niXYtIV1FR1/8tZ+QgUhiPzcXcVb2robdenxfQ9weH5CCVAWKKEpfBsACF5OofxrF\n/v99yu5pxeUaN4njcwIDAQAB\n-----END PUBLIC KEY-----\n-----BEGIN RSA PRIVATE KEY-----\nMIICXgIBAAKBgTERefC8/JudPKMV0A7zDXvdOiz66ZEb/ty5SLOkeC0HMrarRKpL\n8DdBpkTcAb7D5psBoDPmddis18SSXGsa7DEZBR39iXYtIV1FR1/8tZ+QgUhiPzcX\ncVb2robdenxfQ9weH5CCVAWKKEpfBsACF5OofxrF/v99yu5pxeUaN4njcwIDAQAB\nAoGBDzqRUfoVnGZsj2ERtdIReUPr7lHhc7vwmaiXu8lr0u3M+4ykPwZag4vIgs6V\nbBN42triUblREfJy9PtH26X7cC0twt93fImTyuAcrKSNXKQIizI0XrhyB/9ewQv8\nkuI8dQugKhVIO+A1Ii2HPT7q9BC1DS9aYZ/PUzUQ68WM7b2BAkEHSSYsERzUcOwl\nZuazcy/AkylGmqGQcdO5wBkGUUxvHSa6oUvqsJcci35hGk95AJ1v6ndpKMolKFsN\n42Q9Gj+McQJBBrweUOlsAr9jbp7qi4mbvr92Ud533UdMPpvCO62BgrYZBMfZffvr\n+x4AEIh4tuZ+QVOR1nlCwrK/m0Q1+IsMsCMCQQO8fqfwqrFDq8bOi5cRhjajAXLk\nz+Asj6Ddo7e6r5D4CSmCmFUl9Ii9/LS9cm4iY5rGSjCSq3/8vx1TNM+lC1vxAkEC\nYqaqKcKjxn3FNGwGOBr9mHqjzJPPv+z1T92fnXh9f1mlI9OYl52hN6L2OB/pSAH3\nyU2iFRjcNMtAhwxGl5lK2QJAZJ1MF7buFyHnctA4mlWcPTzflVDUV8RrA3t0ZBsd\nUhZq+KITyDliBs37pEIvGNb2Hby10hTJcb9IKuuXanNwwg==\n-----END RSA PRIVATE KEY-----\n",
        "-----BEGIN PUBLIC KEY-----\nMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgVvfDjDTId2lFH+IJAj6aRlUgN+P\ngNP26L9YGFBPNkJ8qbH1VAucZaj2l0z4RHokTZKAIBu0n8u+Y3jRlEzSJ+Iw+W49\nEPgZ3O8nbGSgCypLZwHn0B3l+r3jsemg34L0YxNZzSJmlkf7sXFyRhNO17SXz/+9\nxCtZxzqW7ZAWYhLf9wIDAQAB\n-----END PUBLIC KEY-----\n-----BEGIN RSA PRIVATE KEY-----\nMIICXwIBAAKBgVvfDjDTId2lFH+IJAj6aRlUgN+PgNP26L9YGFBPNkJ8qbH1VAuc\nZaj2l0z4RHokTZKAIBu0n8u+Y3jRlEzSJ+Iw+W49EPgZ3O8nbGSgCypLZwHn0B3l\n+r3jsemg34L0YxNZzSJmlkf7sXFyRhNO17SXz/+9xCtZxzqW7ZAWYhLf9wIDAQAB\nAoGBD30enlqqJf0T5KBmOuFE4NFfXNGLzbCd8sx+ZOPF6RWtYmRTBBYdCYxxW7er\ni9AdB+rz/tfH7QivKopi70SrFrMg4Ur3Kkj5av4mKgrkz2XmNekQeQzU7lzqdopL\nJjn35vZ3s/C7a+MrdXR9iQkDbwJk9Y1AHNuhMXFhV6dez2MxAkEKAu+ESNn62LvQ\n0ATIwqqXUe+XIcGw0DI2pUsN+UfLrtWiVe6ejiDUkeoXI/4JRwSpdi6Ir9Fuu1mU\nQSypZtxPnwJBCS02Ln7ToL/Z6f0ObAMBtt8pFZz1DMg7mwz01u6nGmHgArRuCuny\n3mLSW110UtSYuByaxvxYWT1MP7T11y37sKkCQQfHFBCvEDli2zZ0BON66FC6pOnC\nndkhRYFSlKZ8fRxt7SY6oDCptjOuUDA+FANdGvAUEj66aHggMI2OvIW2lX19AkEA\nrix1OAwCwBatBYkbMwHeiB8orhFxGCtrLIO+p8UV7KnKKYx7HKtYF6WXBo/IUGDe\nTaigFjeKrkPH+We8w3kEuQJBBZjRBZ462k9jIHUsCdgF/30fGuDQF67u6c76DX3X\n/3deRLV4Mi9kBdYhHaGVGWZqqH/cTNjIj2tuPWfpYdy7o9A=\n-----END RSA PRIVATE KEY-----\n",
        "-----BEGIN PUBLIC KEY-----\nMIHfMA0GCSqGSIb3DQEBAQUAA4HNADCByQKBwQDPLNQeNMo6co6ly4r/ZMNtJ73v\nU2TjNv1o0xI8WhlqjChwE+hT1RVtWNFRlUUg+09texertoF3ZZCcV2EZZZ2QKxkG\n7YorEMFVwk0SRSjaue6uN5vqxm5KQReG3Lj9AGLrwDDeEhmgTCqMG33TEx5Na2yu\n4uMaXtQawVCbLvHuKrGDZL5WjKlBwl7MhP+dZDtewaquECog1z9Hm3gP1tqRB1IS\n2erAOgZ02JnrouQx9MRLYVtroiMr1LM7rtc9Yl0CAwEAAQ==\n-----END PUBLIC KEY-----\n-----BEGIN RSA PRIVATE KEY-----\nMIIDfgIBAAKBwQDPLNQeNMo6co6ly4r/ZMNtJ73vU2TjNv1o0xI8WhlqjChwE+hT\n1RVtWNFRlUUg+09texertoF3ZZCcV2EZZZ2QKxkG7YorEMFVwk0SRSjaue6uN5vq\nxm5KQReG3Lj9AGLrwDDeEhmgTCqMG33TEx5Na2yu4uMaXtQawVCbLvHuKrGDZL5W\njKlBwl7MhP+dZDtewaquECog1z9Hm3gP1tqRB1IS2erAOgZ02JnrouQx9MRLYVtr\noiMr1LM7rtc9Yl0CAwEAAQKBwQCBIn4tPdZ3zAQiT9caDiLKHSWE0cRm5FXcSwRo\n3fhNs4NZKO99oaozeFMwuQxX3I3LvhgpDh9w3rve15BMlkw6GsME0Hd5FH6OCAim\nRLmMbKzbpwnmszz3x870Xwxnlx7xZblxuoKHiq4tjuoOK2FETNi979bB1jGO0xrA\nd8Oap2AMKBju4OmNpRdzjTKaMVyFavjn7HKHZ2Pp2Y45K/X+hIv0Kx8xx7kkaix7\nyxQLIVKMPjoanViwHTxWls9mUQECYQD8jWwEvsTrmoGSynkAy+U24ui1Gd7PM7JF\nl5jGkJ308XbbfSMZD8criGWnGK+JXxvNkUUpgCdCO2BecKR89YOQqMPoj8jEjosy\n49ohDfvj6IHqVnS2o0jCHpP55V6mXv0CYQDSANReeIqs6mBqQB0EYPh91cECfhLc\nGg11huiTnZz3ibQPUawEQpYd59Icwh4FyDFVwfKqkZM4fP35VstI0VO6JwQG+bu6\nU31Jh9ni+ZQtehTL//6nT+zdqSjSPiWfXuECYQDbFoAveaLw1F81jWn9M+RLgfro\nKGIuk6VCU+mX0BsHQ3WdoOgStKpObIvqsjKNVDGVWkGKZ/8mqMXIB6XaNU4F7zHM\njPdY9GNzKVCwPiZXJvuU451qVyomJEqwjbdXUq0CYQCgoxfP598UI/h6be6EUfTi\ntKZ+VJfym08eToMLn63ZQBFnAm9VluWjnJeBfg9fFuJ+GeyZAuAdfqb7mqPHYK/u\nHjgbad5qycB1haBq2cS6AL91yK0vqJikeegK4pT+0qECYAsh8zXDUzQutEw6okRF\neAwtZVuUAXTK44x8ik5kk8C6n9MDdIJnsIO5p6bLYeQts2K4yYlttwZOAq1a5hWH\n2hW0ZJyQWUkJ/rN9vLZUvrcmjsgB5ai0qjkRvr2IVC8Fvg==\n-----END RSA PRIVATE KEY-----\n",
        "-----BEGIN PUBLIC KEY-----\nMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEArkXtVgHOxrjMBfgDk1xn\nTdvg11xMCf15UfxrDK7DE6jfOZcMUYv/ul7Wjz8NfyKkAp1BPxrgfk6+nkF3ziPn\n9UBLVp5O4b3PPB+wPvETgC1PhV65tRNLWnyAha3K5vovoUF+w3Y74XGwxit2Dt4j\nwSrZK5gIhMZB9aj6wmva1KAzgaIv4bdUiFCUyCUG1AGaU1ooav6ycbubpZLeGNz2\nAMKu6uVuAvfPefwUzzvcfNhP67v5UMqQMEsiGaeqBjrvosPBmA5WDNZK/neVhbYQ\ndle5V4V+/eYBCYirfeQX/IjY84TE5ucsP5Q+DDHAxKXMNvh52KOsnX1Zhg6q2muD\nuwIDAQAB\n-----END PUBLIC KEY-----\n-----BEGIN RSA PRIVATE KEY-----\nMIIEowIBAAKCAQEArkXtVgHOxrjMBfgDk1xnTdvg11xMCf15UfxrDK7DE6jfOZcM\nUYv/ul7Wjz8NfyKkAp1BPxrgfk6+nkF3ziPn9UBLVp5O4b3PPB+wPvETgC1PhV65\ntRNLWnyAha3K5vovoUF+w3Y74XGwxit2Dt4jwSrZK5gIhMZB9aj6wmva1KAzgaIv\n4bdUiFCUyCUG1AGaU1ooav6ycbubpZLeGNz2AMKu6uVuAvfPefwUzzvcfNhP67v5\nUMqQMEsiGaeqBjrvosPBmA5WDNZK/neVhbYQdle5V4V+/eYBCYirfeQX/IjY84TE\n5ucsP5Q+DDHAxKXMNvh52KOsnX1Zhg6q2muDuwIDAQABAoIBAFyN+sxwzVaxEnoh\nDBUZQCwTmMgH1sJ/gg1O17O2pRgt2dAGLp6okbpzX9RYElzxEtXompxfM9chDw+R\niYVLgIe6C8kG7rHpUsSFt97VvhuW9OLKOiq3ApAeC0vzzwz41o7379DzXD4RWWcF\n8f9XbvnKPehvKCcL/D/x7KuRCHlfcePoXxNqn5d8sTvh6/sn+8FRT63/A5FYxhQX\nMt8loVGw8ezKX5U98U/gvoSWCK6lJ4YEcBgdlIewIj0ueWehA7cLMzzPpVxtqp1J\nFEw1ruWhwGiIIPHEgj8tnyAq17lDs6I/Drx0MGJ9eWQNpn0RVRDluALBIuf5RjU1\ntCRJU+ECgYEA7PWuzR5VFf/6y9daKBbG6/SQGM37RjjhhdZqc5a2+AkPgBjH/ZXM\nNLhX3BfwzGUWuxNGq01YLK2te0EDNSOHtwM40IQEfJ2VObZJYgSz3W6kQkmSB77A\nH5ZCh/9jNsOYRlgzaEb1bkaGGIHBAjPSF2vxWl6W3ceAvIaKp30852kCgYEAvEbE\nZPxqxMp4Ow6wijyEG3cvfpsvKLq9WIroheGgxh5IWKD7JawpmZDzW+hRZMJZuhF1\nzdcZJwcTUYSZK2wpt0bdDSyr4UKDX30UjMFhUktKCZRtSLgoRz8c52tstohsNFwD\n4F9B1RtcOpCj8kBzx9dKT+JdnPIcdZYPP8OGMYMCgYEAxzVkVx0A+xXQij3plXpQ\nkV1xJulELaz0K8guhi5Wc/9qAI7U0uN0YX34nxehYLQ7f9qctra3QhhgmBX31Fyi\nY8FZqjLSctEn+vS8jKLXc3jorrGbCtfaPLPeCucxSYD2K21LCoddHfA8G645zNgz\n72zX4tlSi/CE0flp55Tp9sECgYAmWLN/bfnBAwvh22gRf6nYfjnqK2k7fm06L3CU\ndBPuxhQuGPuN/LasVF18hqCtSPhFcXDw77JrxIEmxT79HRaSAZjcKhEH3CgttqgM\n0wYjYLo/oT9w5DEv8abNa4/EzZxcPbF8bWpXIS9zrin2GTJ7rVmxU4WFhbpOKLYK\nYqReSQKBgG84Ums5JQhVNO8+QVqDbt6LhhWKLHy/7MsL2DQwT+xoO6jU9HnEM9Q0\nFuYyaWI86hAHdtha/0AdP/9hDuZUEc47E2PWOpcJ7t5CZHzqVhST1UVwqHnBhoLN\nl3ELliBewxEX1ztfNiI/rdboupDdfA7mHUThYyUeIMf2brMFEXy4\n-----END RSA PRIVATE KEY-----\n",
        NULL,
    };
    struct key_pair {
        SecKeyRef pubKey, privKey;
    };
    key_pair keys[11];
    
    int i;
    for(i = 0; i < sizeof(keys)/sizeof(key_pair); i++) {
        NSAssert(pem_key_bytes[i] != NULL, @"Expected a key");
        NSLog(@"Importing: %s", pem_key_bytes[i]);
        CFDataRef pem_data = CFDataCreate(NULL, (UInt8*)(pem_key_bytes[i]), strlen(pem_key_bytes[i]));
        SecKeyImportExportParameters import_params;
        bzero(&import_params, sizeof(import_params));
        
        import_params.version = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION;
        import_params.keyUsage = CSSM_KEYUSE_ANY;
        import_params.keyAttributes = CSSM_KEYATTR_PERMANENT|CSSM_KEYATTR_SENSITIVE;
        import_params.accessRef = NULL;
        import_params.passphrase = CFSTR("");
        import_params.alertPrompt = CFSTR("");
        
        CFArrayRef keypair = NULL;
        SecExternalFormat key_format = kSecFormatOpenSSL;
        SecExternalItemType itemType = kSecItemTypeUnknown;
        status = SecKeychainItemImport(pem_data, CFSTR(".pem"), &key_format, &itemType, 0, &import_params, tmp_keychain, &keypair);
        STAssertTrue(status == 0, @"Expected pubkey import to be ok, got err=0x%x", status);
        NSAssert(keypair != NULL, @"Expected to get some keys back");
        STAssertNotNil((id)keypair, @"Expected to get some keys back");
        STAssertTrue(CFArrayGetCount(keypair) == 2, @"Expected 2 keys, got %@", keypair);
        keys[i].pubKey = (SecKeyRef)CFArrayGetValueAtIndex(keypair, 0);
        keys[i].privKey = (SecKeyRef)CFArrayGetValueAtIndex(keypair, 1);
    }
    STAssertNil((id)pem_key_bytes[i], @"Expected to convert all pem keys, but found at least: %s", pem_key_bytes[i]);
    CFDataRef encoding_parameters = CFDataCreate(NULL, NULL, 0);
    
    struct KAT {
        NSData *message, *seed, *encryptedMessage;
        NSString *label;
        key_pair keys;
    };
    KAT tests[] = {
        // This first one is from the spec
        {
            .message = [NSData dataWithHexString:@"d436e99569fd32a7c8a05bbc90d32c49"],
            .seed = [NSData dataWithHexString:@"aafd12f659cae63489b479e5076ddec2f06cb58f"],
            .encryptedMessage = [NSData dataWithHexString:@"1253e04dc0a5397bb44a7ab87e9bf2a039a33d1e996fc82a94ccd30074c95df763722017069e5268da5d1c0b4f872cf653c11df82314a67968dfeae28def04bb6d84b1c31d654a1970e5783bd6eb96a024c2ca2f4a90fe9f2ef5c9c140e5bb48da9536ad8700c84fc9130adea74e558d51a74ddf85d8b50de96838d6063e0955"],
            .keys = keys[0],
            .label = @"From spec",
        },
        // The next 60 are from oaep-vect.txt
        {
            .message = [NSData dataWithHexString:@"6628194e12073db03ba94cda9ef9532397d50dba79b987004afefe34"],
            .seed = [NSData dataWithHexString:@"18b776ea21069d69776a33e96bad48e1dda0a5ef"],
            .encryptedMessage = [NSData dataWithHexString:@"354fe67b4a126d5d35fe36c777791a3f7ba13def484e2d3908aff722fad468fb21696de95d0be911c2d3174f8afcc201035f7b6d8e69402de5451618c21a535fa9d7bfc5b8dd9fc243f8cf927db31322d6e881eaa91a996170e657a05a266426d98c88003f8477c1227094a0d9fa1e8c4024309ce1ecccb5210035d47ac72e8a"],
            .keys = keys[1],
            .label = @"1-1",
        },
        {
            .message = [NSData dataWithHexString:@"750c4047f547e8e41411856523298ac9bae245efaf1397fbe56f9dd5"],
            .seed = [NSData dataWithHexString:@"0cc742ce4a9b7f32f951bcb251efd925fe4fe35f"],
            .encryptedMessage = [NSData dataWithHexString:@"640db1acc58e0568fe5407e5f9b701dff8c3c91e716c536fc7fcec6cb5b71c1165988d4a279e1577d730fc7a29932e3f00c81515236d8d8e31017a7a09df4352d904cdeb79aa583adcc31ea698a4c05283daba9089be5491f67c1a4ee48dc74bbbe6643aef846679b4cb395a352d5ed115912df696ffe0702932946d71492b44"],
            .keys = keys[1],
            .label = @"1-2",
        },
        {
            .message = [NSData dataWithHexString:@"d94ae0832e6445ce42331cb06d531a82b1db4baad30f746dc916df24d4e3c2451fff59a6423eb0e1d02d4fe646cf699dfd818c6e97b051"],
            .seed = [NSData dataWithHexString:@"2514df4695755a67b288eaf4905c36eec66fd2fd"],
            .encryptedMessage = [NSData dataWithHexString:@"423736ed035f6026af276c35c0b3741b365e5f76ca091b4e8c29e2f0befee603595aa8322d602d2e625e95eb81b2f1c9724e822eca76db8618cf09c5343503a4360835b5903bc637e3879fb05e0ef32685d5aec5067cd7cc96fe4b2670b6eac3066b1fcf5686b68589aafb7d629b02d8f8625ca3833624d4800fb081b1cf94eb"],
            .keys = keys[1],
            .label = @"1-3",
        },
        {
            .message = [NSData dataWithHexString:@"52e650d98e7f2a048b4f86852153b97e01dd316f346a19f67a85"],
            .seed = [NSData dataWithHexString:@"c4435a3e1a18a68b6820436290a37cefb85db3fb"],
            .encryptedMessage = [NSData dataWithHexString:@"45ead4ca551e662c9800f1aca8283b0525e6abae30be4b4aba762fa40fd3d38e22abefc69794f6ebbbc05ddbb11216247d2f412fd0fba87c6e3acd888813646fd0e48e785204f9c3f73d6d8239562722dddd8771fec48b83a31ee6f592c4cfd4bc88174f3b13a112aae3b9f7b80e0fc6f7255ba880dc7d8021e22ad6a85f0755"],
            .keys = keys[1],
            .label = @"1-4",
        },
        {
            .message = [NSData dataWithHexString:@"8da89fd9e5f974a29feffb462b49180f6cf9e802"],
            .seed = [NSData dataWithHexString:@"b318c42df3be0f83fea823f5a7b47ed5e425a3b5"],
            .encryptedMessage = [NSData dataWithHexString:@"36f6e34d94a8d34daacba33a2139d00ad85a9345a86051e73071620056b920e219005855a213a0f23897cdcd731b45257c777fe908202befdd0b58386b1244ea0cf539a05d5d10329da44e13030fd760dcd644cfef2094d1910d3f433e1c7c6dd18bc1f2df7f643d662fb9dd37ead9059190f4fa66ca39e869c4eb449cbdc439"],
            .keys = keys[1],
            .label = @"1-5",
        },
        {
            .message = [NSData dataWithHexString:@"26521050844271"],
            .seed = [NSData dataWithHexString:@"e4ec0982c2336f3a677f6a356174eb0ce887abc2"],
            .encryptedMessage = [NSData dataWithHexString:@"42cee2617b1ecea4db3f4829386fbd61dafbf038e180d837c96366df24c097b4ab0fac6bdf590d821c9f10642e681ad05b8d78b378c0f46ce2fad63f74e0ad3df06b075d7eb5f5636f8d403b9059ca761b5c62bb52aa45002ea70baace08ded243b9d8cbd62a68ade265832b56564e43a6fa42ed199a099769742df1539e8255"],
            .keys = keys[1],
            .label = @"1-6",
        },
        
        {
            .message = [NSData dataWithHexString:@"8ff00caa605c702830634d9a6c3d42c652b58cf1d92fec570beee7"],
            .seed = [NSData dataWithHexString:@"8c407b5ec2899e5099c53e8ce793bf94e71b1782"],
            .encryptedMessage = [NSData dataWithHexString:@"0181af8922b9fcb4d79d92ebe19815992fc0c1439d8bcd491398a0f4ad3a329a5bd9385560db532683c8b7da04e4b12aed6aacdf471c34c9cda891addcc2df3456653aa6382e9ae59b54455257eb099d562bbe10453f2b6d13c59c02e10f1f8abb5da0d0570932dacf2d0901db729d0fefcc054e70968ea540c81b04bcaefe720e"],
            .keys = keys[2],
            .label = @"2-1",
        },
        {
            .message = [NSData dataWithHexString:@"2d"],
            .seed = [NSData dataWithHexString:@"b600cf3c2e506d7f16778c910d3a8b003eee61d5"],
            .encryptedMessage = [NSData dataWithHexString:@"018759ff1df63b2792410562314416a8aeaf2ac634b46f940ab82d64dbf165eee33011da749d4bab6e2fcd18129c9e49277d8453112b429a222a8471b070993998e758861c4d3f6d749d91c4290d332c7a4ab3f7ea35ff3a07d497c955ff0ffc95006b62c6d296810d9bfab024196c7934012c2df978ef299aba239940cba10245"],
            .keys = keys[2],
            .label = @"2-2",
        },
        {
            .message = [NSData dataWithHexString:@"74fc88c51bc90f77af9d5e9a4a70133d4b4e0b34da3c37c7ef8e"],
            .seed = [NSData dataWithHexString:@"a73768aeeaa91f9d8c1ed6f9d2b63467f07ccae3"],
            .encryptedMessage = [NSData dataWithHexString:@"018802bab04c60325e81c4962311f2be7c2adce93041a00719c88f957575f2c79f1b7bc8ced115c706b311c08a2d986ca3b6a9336b147c29c6f229409ddec651bd1fdd5a0b7f610c9937fdb4a3a762364b8b3206b4ea485fd098d08f63d4aa8bb2697d027b750c32d7f74eaf5180d2e9b66b17cb2fa55523bc280da10d14be2053"],
            .keys = keys[2],
            .label = @"2-3",
        },
        {
            .message = [NSData dataWithHexString:@"a7eb2a5036931d27d4e891326d99692ffadda9bf7efd3e34e622c4adc085f721dfe885072c78a203b151739be540fa8c153a10f00a"],
            .seed = [NSData dataWithHexString:@"9a7b3b0e708bd96f8190ecab4fb9b2b3805a8156"],
            .encryptedMessage = [NSData dataWithHexString:@"00a4578cbc176318a638fba7d01df15746af44d4f6cd96d7e7c495cbf425b09c649d32bf886da48fbaf989a2117187cafb1fb580317690e3ccd446920b7af82b31db5804d87d01514acbfa9156e782f867f6bed9449e0e9a2c09bcecc6aa087636965e34b3ec766f2fe2e43018a2fddeb140616a0e9d82e5331024ee0652fc7641"],
            .keys = keys[2],
            .label = @"2-4",
        },
        {
            .message = [NSData dataWithHexString:@"2ef2b066f854c33f3bdcbb5994a435e73d6c6c"],
            .seed = [NSData dataWithHexString:@"eb3cebbc4adc16bb48e88c8aec0e34af7f427fd3"],
            .encryptedMessage = [NSData dataWithHexString:@"00ebc5f5fda77cfdad3c83641a9025e77d72d8a6fb33a810f5950f8d74c73e8d931e8634d86ab1246256ae07b6005b71b7f2fb98351218331ce69b8ffbdc9da08bbc9c704f876deb9df9fc2ec065cad87f9090b07acc17aa7f997b27aca48806e897f771d95141fe4526d8a5301b678627efab707fd40fbebd6e792a25613e7aec"],
            .keys = keys[2],
            .label = @"2-5",
        },
        {
            .message = [NSData dataWithHexString:@"8a7fb344c8b6cb2cf2ef1f643f9a3218f6e19bba89c0"],
            .seed = [NSData dataWithHexString:@"4c45cf4d57c98e3d6d2095adc51c489eb50dff84"],
            .encryptedMessage = [NSData dataWithHexString:@"010839ec20c27b9052e55befb9b77e6fc26e9075d7a54378c646abdf51e445bd5715de81789f56f1803d9170764a9e93cb78798694023ee7393ce04bc5d8f8c5a52c171d43837e3aca62f609eb0aa5ffb0960ef04198dd754f57f7fbe6abf765cf118b4ca443b23b5aab266f952326ac4581100644325f8b721acd5d04ff14ef3a"],
            .keys = keys[2],
            .label = @"2-6",
        },
        
        {
            .message = [NSData dataWithHexString:@"087820b569e8fa8d"],
            .seed = [NSData dataWithHexString:@"8ced6b196290805790e909074015e6a20b0c4894"],
            .encryptedMessage = [NSData dataWithHexString:@"026a0485d96aebd96b4382085099b962e6a2bdec3d90c8db625e14372de85e2d5b7baab65c8faf91bb5504fb495afce5c988b3f6a52e20e1d6cbd3566c5cd1f2b8318bb542cc0ea25c4aab9932afa20760eaddec784396a07ea0ef24d4e6f4d37e5052a7a31e146aa480a111bbe926401307e00f410033842b6d82fe5ce4dfae80"],
            .keys = keys[3],
            .label = @"3-1",
        },
        {
            .message = [NSData dataWithHexString:@"4653acaf171960b01f52a7be63a3ab21dc368ec43b50d82ec3781e04"],
            .seed = [NSData dataWithHexString:@"b4291d6567550848cc156967c809baab6ca507f0"],
            .encryptedMessage = [NSData dataWithHexString:@"024db89c7802989be0783847863084941bf209d761987e38f97cb5f6f1bc88da72a50b73ebaf11c879c4f95df37b850b8f65d7622e25b1b889e80fe80baca2069d6e0e1d829953fc459069de98ea9798b451e557e99abf8fe3d9ccf9096ebbf3e5255d3b4e1c6d2ecadf067a359eea86405acd47d5e165517ccafd47d6dbee4bf5"],
            .keys = keys[3],
            .label = @"3-2",
        },
        {
            .message = [NSData dataWithHexString:@"d94cd0e08fa404ed89"],
            .seed = [NSData dataWithHexString:@"ce8928f6059558254008badd9794fadcd2fd1f65"],
            .encryptedMessage = [NSData dataWithHexString:@"0239bce681032441528877d6d1c8bb28aa3bc97f1df584563618995797683844ca86664732f4bed7a0aab083aaabfb7238f582e30958c2024e44e57043b97950fd543da977c90cdde5337d618442f99e60d7783ab59ce6dd9d69c47ad1e962bec22d05895cff8d3f64ed5261d92b2678510393484990ba3f7f06818ae6ffce8a3a"],
            .keys = keys[3],
            .label = @"3-3",
        },
        {
            .message = [NSData dataWithHexString:@"6cc641b6b61e6f963974dad23a9013284ef1"],
            .seed = [NSData dataWithHexString:@"6e2979f52d6814a57d83b090054888f119a5b9a3"],
            .encryptedMessage = [NSData dataWithHexString:@"02994c62afd76f498ba1fd2cf642857fca81f4373cb08f1cbaee6f025c3b512b42c3e8779113476648039dbe0493f9246292fac28950600e7c0f32edf9c81b9dec45c3bde0cc8d8847590169907b7dc5991ceb29bb0714d613d96df0f12ec5d8d3507c8ee7ae78dd83f216fa61de100363aca48a7e914ae9f42ddfbe943b09d9a0"],
            .keys = keys[3],
            .label = @"3-4",
        },
        {
            .message = [NSData dataWithHexString:@"df5151832b61f4f25891fb4172f328d2eddf8371ffcfdbe997939295f30eca6918017cfda1153bf7a6af87593223"],
            .seed = [NSData dataWithHexString:@"2d760bfe38c59de34cdc8b8c78a38e66284a2d27"],
            .encryptedMessage = [NSData dataWithHexString:@"0162042ff6969592a6167031811a239834ce638abf54fec8b99478122afe2ee67f8c5b18b0339805bfdbc5a4e6720b37c59cfba942464c597ff532a119821545fd2e59b114e61daf71820529f5029cf524954327c34ec5e6f5ba7efcc4de943ab8ad4ed787b1454329f70db798a3a8f4d92f8274e2b2948ade627ce8ee33e43c60"],
            .keys = keys[3],
            .label = @"3-5",
        },
        {
            .message = [NSData dataWithHexString:@"3c3bad893c544a6d520ab022319188c8d504b7a788b850903b85972eaa18552e1134a7ad6098826254ff7ab672b3d8eb3158fac6d4cbaef1"],
            .seed = [NSData dataWithHexString:@"f174779c5fd3cfe007badcb7a36c9b55bfcfbf0e"],
            .encryptedMessage = [NSData dataWithHexString:@"00112051e75d064943bc4478075e43482fd59cee0679de6893eec3a943daa490b9691c93dfc0464b6623b9f3dbd3e70083264f034b374f74164e1a00763725e574744ba0b9db83434f31df96f6e2a26f6d8eba348bd4686c2238ac07c37aac3785d1c7eea2f819fd91491798ed8e9cef5e43b781b0e0276e37c43ff9492d005730"],
            .label = @"3-6",
            .keys = keys[3],
        },
        
        {
            .message = [NSData dataWithHexString:@"4a86609534ee434a6cbca3f7e962e76d455e3264c19f605f6e5ff6137c65c56d7fb344cd52bc93374f3d166c9f0c6f9c506bad19330972d2"],
            .seed = [NSData dataWithHexString:@"1cac19ce993def55f98203f6852896c95ccca1f3"],
            .encryptedMessage = [NSData dataWithHexString:@"04cce19614845e094152a3fe18e54e3330c44e5efbc64ae16886cb1869014cc5781b1f8f9e045384d0112a135ca0d12e9c88a8e4063416deaae3844f60d6e96fe155145f4525b9a34431ca3766180f70e15a5e5d8e8b1a516ff870609f13f896935ced188279a58ed13d07114277d75c6568607e0ab092fd803a223e4a8ee0b1a8"],
            .keys = keys[4],
            .label = @"4-1",
        },
        {
            .message = [NSData dataWithHexString:@"b0adc4f3fe11da59ce992773d9059943c03046497ee9d9f9a06df1166db46d98f58d27ec074c02eee6cbe2449c8b9fc5080c5c3f4433092512ec46aa793743c8"],
            .seed = [NSData dataWithHexString:@"f545d5897585e3db71aa0cb8da76c51d032ae963"],
            .encryptedMessage = [NSData dataWithHexString:@"0097b698c6165645b303486fbf5a2a4479c0ee85889b541a6f0b858d6b6597b13b854eb4f839af03399a80d79bda6578c841f90d645715b280d37143992dd186c80b949b775cae97370e4ec97443136c6da484e970ffdb1323a20847821d3b18381de13bb49aaea66530c4a4b8271f3eae172cd366e07e6636f1019d2a28aed15e"],
            .keys = keys[4],
            .label = @"4-2",
        },
        {
            .message = [NSData dataWithHexString:@"bf6d42e701707b1d0206b0c8b45a1c72641ff12889219a82bdea965b5e79a96b0d0163ed9d578ec9ada20f2fbcf1ea3c4089d83419ba81b0c60f3606da99"],
            .seed = [NSData dataWithHexString:@"ad997feef730d6ea7be60d0dc52e72eacbfdd275"],
            .encryptedMessage = [NSData dataWithHexString:@"0301f935e9c47abcb48acbbe09895d9f5971af14839da4ff95417ee453d1fd77319072bb7297e1b55d7561cd9d1bb24c1a9a37c619864308242804879d86ebd001dce5183975e1506989b70e5a83434154d5cbfd6a24787e60eb0c658d2ac193302d1192c6e622d4a12ad4b53923bca246df31c6395e37702c6a78ae081fb9d065"],
            .keys = keys[4],
            .label = @"4-3",
        },
        {
            .message = [NSData dataWithHexString:@"fb2ef112f5e766eb94019297934794f7be2f6fc1c58e"],
            .seed = [NSData dataWithHexString:@"136454df5730f73c807a7e40d8c1a312ac5b9dd3"],
            .encryptedMessage = [NSData dataWithHexString:@"02d110ad30afb727beb691dd0cf17d0af1a1e7fa0cc040ec1a4ba26a42c59d0a796a2e22c8f357ccc98b6519aceb682e945e62cb734614a529407cd452bee3e44fece8423cc19e55548b8b994b849c7ecde4933e76037e1d0ce44275b08710c68e430130b929730ed77e09b015642c5593f04e4ffb9410798102a8e96ffdfe11e4"],
            .keys = keys[4],
            .label = @"4-4",
        },
        {
            .message = [NSData dataWithHexString:@"28ccd447bb9e85166dabb9e5b7d1adadc4b9d39f204e96d5e440ce9ad928bc1c2284"],
            .seed = [NSData dataWithHexString:@"bca8057f824b2ea257f2861407eef63d33208681"],
            .encryptedMessage = [NSData dataWithHexString:@"00dbb8a7439d90efd919a377c54fae8fe11ec58c3b858362e23ad1b8a44310799066b99347aa525691d2adc58d9b06e34f288c170390c5f0e11c0aa3645959f18ee79e8f2be8d7ac5c23d061f18dd74b8c5f2a58fcb5eb0c54f99f01a83247568292536583340948d7a8c97c4acd1e98d1e29dc320e97a260532a8aa7a758a1ec2"],
            .keys = keys[4],
            .label = @"4-5",
        },
        {
            .message = [NSData dataWithHexString:@"f22242751ec6b1"],
            .seed = [NSData dataWithHexString:@"2e7e1e17f647b5ddd033e15472f90f6812f3ac4e"],
            .encryptedMessage = [NSData dataWithHexString:@"00a5ffa4768c8bbecaee2db77e8f2eec99595933545520835e5ba7db9493d3e17cddefe6a5f567624471908db4e2d83a0fbee60608fc84049503b2234a07dc83b27b22847ad8920ff42f674ef79b76280b00233d2b51b8cb2703a9d42bfbc8250c96ec32c051e57f1b4ba528db89c37e4c54e27e6e64ac69635ae887d9541619a9"],
            .keys = keys[4],
            .label = @"4-6",
        },
        
        {
            .message = [NSData dataWithHexString:@"af71a901e3a61d3132f0fc1fdb474f9ea6579257ffc24d164170145b3dbde8"],
            .seed = [NSData dataWithHexString:@"44c92e283f77b9499c603d963660c87d2f939461"],
            .encryptedMessage = [NSData dataWithHexString:@"036046a4a47d9ed3ba9a89139c105038eb7492b05a5d68bfd53accff4597f7a68651b47b4a4627d927e485eed7b4566420e8b409879e5d606eae251d22a5df799f7920bfc117b992572a53b1263146bcea03385cc5e853c9a101c8c3e1bda31a519807496c6cb5e5efb408823a352b8fa0661fb664efadd593deb99fff5ed000e5"],
            .keys = keys[5],
            .label = @"5-1",
        },
        {
            .message = [NSData dataWithHexString:@"a3b844a08239a8ac41605af17a6cfda4d350136585903a417a79268760519a4b4ac3303ec73f0f87cfb32399"],
            .seed = [NSData dataWithHexString:@"cb28f5860659fceee49c3eeafce625a70803bd32"],
            .encryptedMessage = [NSData dataWithHexString:@"03d6eb654edce615bc59f455265ed4e5a18223cbb9be4e4069b473804d5de96f54dcaaa603d049c5d94aa1470dfcd2254066b7c7b61ff1f6f6770e3215c51399fd4e34ec5082bc48f089840ad04354ae66dc0f1bd18e461a33cc1258b443a2837a6df26759aa2302334986f87380c9cc9d53be9f99605d2c9a97da7b0915a4a7ad"],
            .keys = keys[5],
            .label = @"5-2",
        },
        {
            .message = [NSData dataWithHexString:@"308b0ecbd2c76cb77fc6f70c5edd233fd2f20929d629f026953bb62a8f4a3a314bde195de85b5f816da2aab074d26cb6acddf323ae3b9c678ac3cf12fbdde7"],
            .seed = [NSData dataWithHexString:@"2285f40d770482f9a9efa2c72cb3ac55716dc0ca"],
            .encryptedMessage = [NSData dataWithHexString:@"0770952181649f9f9f07ff626ff3a22c35c462443d905d456a9fd0bff43cac2ca7a9f554e9478b9acc3ac838b02040ffd3e1847de2e4253929f9dd9ee4044325a9b05cabb808b2ee840d34e15d105a3f1f7b27695a1a07a2d73fe08ecaaa3c9c9d4d5a89ff890d54727d7ae40c0ec1a8dd86165d8ee2c6368141016a48b55b6967"],
            .keys = keys[5],
            .label = @"5-3",
        },
        {
            .message = [NSData dataWithHexString:@"15c5b9ee1185"],
            .seed = [NSData dataWithHexString:@"49fa45d3a78dd10dfd577399d1eb00af7eed5513"],
            .encryptedMessage = [NSData dataWithHexString:@"0812b76768ebcb642d040258e5f4441a018521bd96687e6c5e899fcd6c17588ff59a82cc8ae03a4b45b31299af1788c329f7dcd285f8cf4ced82606b97612671a45bedca133442144d1617d114f802857f0f9d739751c57a3f9ee400912c61e2e6992be031a43dd48fa6ba14eef7c422b5edc4e7afa04fdd38f402d1c8bb719abf"],
            .keys = keys[5],
            .label = @"5-4",
        },
        {
            .message = [NSData dataWithHexString:@"21026e6800c7fa728fcaaba0d196ae28d7a2ac4ffd8abce794f0985f60c8a6737277365d3fea11db8923a2029a"],
            .seed = [NSData dataWithHexString:@"f0287413234cc5034724a094c4586b87aff133fc"],
            .encryptedMessage = [NSData dataWithHexString:@"07b60e14ec954bfd29e60d0047e789f51d57186c63589903306793ced3f68241c743529aba6a6374f92e19e0163efa33697e196f7661dfaaa47aac6bde5e51deb507c72c589a2ca1693d96b1460381249b2cdb9eac44769f2489c5d3d2f99f0ee3c7ee5bf64a5ac79c42bd433f149be8cb59548361640595513c97af7bc2509723"],
            .keys = keys[5],
            .label = @"5-5",
        },
        {
            .message = [NSData dataWithHexString:@"541e37b68b6c8872b84c02"],
            .seed = [NSData dataWithHexString:@"d9fba45c96f21e6e26d29eb2cdcb6585be9cb341"],
            .encryptedMessage = [NSData dataWithHexString:@"08c36d4dda33423b2ed6830d85f6411ba1dcf470a1fae0ebefee7c089f256cef74cb96ea69c38f60f39abee44129bcb4c92de7f797623b20074e3d9c2899701ed9071e1efa0bdd84d4c3e5130302d8f0240baba4b84a71cc032f2235a5ff0fae277c3e8f9112bef44c9ae20d175fc9a4058bfc930ba31b02e2e4f444483710f24a"],
            .keys = keys[5],
            .label = @"5-6",
        },
        {
            .label = @"6-1",
            .keys = keys[6],
            .message = [NSData dataWithHexString:@"4046ca8baa3347ca27f49e0d81f9cc1d71be9ba517d4"],
            .seed = [NSData dataWithHexString:@"dd0f6cfe415e88e5a469a51fbba6dfd40adb4384"],
            .encryptedMessage = [NSData dataWithHexString:@"0630eebcd2856c24f798806e41f9e67345eda9ceda386acc9facaea1eeed06ace583709718d9d169fadf414d5c76f92996833ef305b75b1e4b95f662a20faedc3bae0c4827a8bf8a88edbd57ec203a27a841f02e43a615bab1a8cac0701de34debdef62a088089b55ec36ea7522fd3ec8d06b6a073e6df833153bc0aefd93bd1a3"],
        },
        {
            .label = @"6-2",
            .keys = keys[6],
            .message = [NSData dataWithHexString:@"5cc72c60231df03b3d40f9b57931bc31109f972527f28b19e7480c7288cb3c92b22512214e4be6c914792ddabdf57faa8aa7"],
            .seed = [NSData dataWithHexString:@"8d14bd946a1351148f5cae2ed9a0c653e85ebd85"],
            .encryptedMessage = [NSData dataWithHexString:@"0ebc37376173a4fd2f89cc55c2ca62b26b11d51c3c7ce49e8845f74e7607317c436bc8d23b9667dfeb9d087234b47bc6837175ae5c0559f6b81d7d22416d3e50f4ac533d8f0812f2db9e791fe9c775ac8b6ad0f535ad9ceb23a4a02014c58ab3f8d3161499a260f39348e714ae2a1d3443208fd8b722ccfdfb393e98011f99e63f"],
        },
        {
            .label = @"6-3",
            .keys = keys[6],
            .message = [NSData dataWithHexString:@"b20e651303092f4bccb43070c0f86d23049362ed96642fc5632c27db4a52e3d831f2ab068b23b149879c002f6bf3feee97591112562c"],
            .seed = [NSData dataWithHexString:@"6c075bc45520f165c0bf5ea4c5df191bc9ef0e44"],
            .encryptedMessage = [NSData dataWithHexString:@"0a98bf1093619394436cf68d8f38e2f158fde8ea54f3435f239b8d06b8321844202476aeed96009492480ce3a8d705498c4c8c68f01501dc81db608f60087350c8c3b0bd2e9ef6a81458b7c801b89f2e4fe99d4900ba6a4b5e5a96d865dc676c7755928794130d6280a8160a190f2df3ea7cf9aa0271d88e9e6905ecf1c5152d65"],
        },
        {
            .label = @"6-4",
            .keys = keys[6],
            .message = [NSData dataWithHexString:@"684e3038c5c041f7"],
            .seed = [NSData dataWithHexString:@"3bbc3bd6637dfe12846901029bf5b0c07103439c"],
            .encryptedMessage = [NSData dataWithHexString:@"008e7a67cacfb5c4e24bec7dee149117f19598ce8c45808fef88c608ff9cd6e695263b9a3c0ad4b8ba4c95238e96a8422b8535629c8d5382374479ad13fa39974b242f9a759eeaf9c83ad5a8ca18940a0162ba755876df263f4bd50c6525c56090267c1f0e09ce0899a0cf359e88120abd9bf893445b3cae77d3607359ae9a52f8"],
        },
        {
            .label = @"6-5",
            .keys = keys[6],
            .message = [NSData dataWithHexString:@"32488cb262d041d6e4dd35f987bf3ca696db1f06ac29a44693"],
            .seed = [NSData dataWithHexString:@"b46b41893e8bef326f6759383a83071dae7fcabc"],
            .encryptedMessage = [NSData dataWithHexString:@"00003474416c7b68bdf961c385737944d7f1f40cb395343c693cc0b4fe63b31fedf1eaeeac9ccc0678b31dc32e0977489514c4f09085f6298a9653f01aea4045ff582ee887be26ae575b73eef7f3774921e375a3d19adda0ca31aa1849887c1f42cac9677f7a2f4e923f6e5a868b38c084ef187594dc9f7f048fea2e02955384ab"],
        },
        {
            .label = @"6-6",
            .keys = keys[6],
            .message = [NSData dataWithHexString:@"50ba14be8462720279c306ba"],
            .seed = [NSData dataWithHexString:@"0a2403312a41e3d52f060fbc13a67de5cf7609a7"],
            .encryptedMessage = [NSData dataWithHexString:@"0a026dda5fc8785f7bd9bf75327b63e85e2c0fdee5dadb65ebdcac9ae1de95c92c672ab433aa7a8e69ce6a6d8897fac4ac4a54de841ae5e5bbce7687879d79634cea7a30684065c714d52409b928256bbf53eabcd5231eb7259504537399bd29164b726d33a46da701360a4168a091ccab72d44a62fed246c0ffea5b1348ab5470"],
        },
        {
            .label = @"7-1",
            .keys = keys[7],
            .message = [NSData dataWithHexString:@"47aae909"],
            .seed = [NSData dataWithHexString:@"43dd09a07ff4cac71caa4632ee5e1c1daee4cd8f"],
            .encryptedMessage = [NSData dataWithHexString:@"1688e4ce7794bba6cb7014169ecd559cede2a30b56a52b68d9fe18cf1973ef97b2a03153951c755f6294aa49adbdb55845ab6875fb3986c93ecf927962840d282f9e54ce8b690f7c0cb8bbd73440d9571d1b16cd9260f9eab4783cc482e5223dc60973871783ec27b0ae0fd47732cbc286a173fc92b00fb4ba6824647cd93c85c1"],
        },
        {
            .label = @"7-2",
            .keys = keys[7],
            .message = [NSData dataWithHexString:@"1d9b2e2223d9bc13bfb9f162ce735db48ba7c68f6822a0a1a7b6ae165834e7"],
            .seed = [NSData dataWithHexString:@"3a9c3cec7b84f9bd3adecbc673ec99d54b22bc9b"],
            .encryptedMessage = [NSData dataWithHexString:@"1052ed397b2e01e1d0ee1c50bf24363f95e504f4a03434a08fd822574ed6b9736edbb5f390db10321479a8a139350e2bd4977c3778ef331f3e78ae118b268451f20a2f01d471f5d53c566937171b2dbc2d4bde459a5799f0372d6574239b2323d245d0bb81c286b63c89a361017337e4902f88a467f4c7f244bfd5ab46437ff3b6"],
        },
        {
            .label = @"7-3",
            .keys = keys[7],
            .message = [NSData dataWithHexString:@"d976fc"],
            .seed = [NSData dataWithHexString:@"76a75e5b6157a556cf8884bb2e45c293dd545cf5"],
            .encryptedMessage = [NSData dataWithHexString:@"2155cd843ff24a4ee8badb7694260028a490813ba8b369a4cbf106ec148e5298707f5965be7d101c1049ea8584c24cd63455ad9c104d686282d3fb803a4c11c1c2e9b91c7178801d1b6640f003f5728df007b8a4ccc92bce05e41a27278d7c85018c52414313a5077789001d4f01910b72aad05d220aa14a58733a7489bc54556b"],
        },
        {
            .label = @"7-4",
            .keys = keys[7],
            .message = [NSData dataWithHexString:@"d4738623df223aa43843df8467534c41d013e0c803c624e263666b239bde40a5f29aeb8de79e3daa61dd0370f49bd4b013834b98212aef6b1c5ee373b3cb"],
            .seed = [NSData dataWithHexString:@"7866314a6ad6f2b250a35941db28f5864b585859"],
            .encryptedMessage = [NSData dataWithHexString:@"0ab14c373aeb7d4328d0aaad8c094d88b9eb098b95f21054a29082522be7c27a312878b637917e3d819e6c3c568db5d843802b06d51d9e98a2be0bf40c031423b00edfbff8320efb9171bd2044653a4cb9c5122f6c65e83cda2ec3c126027a9c1a56ba874d0fea23f380b82cf240b8cf540004758c4c77d934157a74f3fc12bfac"],
        },
        {
            .label = @"7-5",
            .keys = keys[7],
            .message = [NSData dataWithHexString:@"bb47231ca5ea1d3ad46c99345d9a8a61"],
            .seed = [NSData dataWithHexString:@"b2166ed472d58db10cab2c6b000cccf10a7dc509"],
            .encryptedMessage = [NSData dataWithHexString:@"028387a318277434798b4d97f460068df5298faba5041ba11761a1cb7316b24184114ec500257e2589ed3b607a1ebbe97a6cc2e02bf1b681f42312a33b7a77d8e7855c4a6de03e3c04643f786b91a264a0d6805e2cea91e68177eb7a64d9255e4f27e713b7ccec00dc200ebd21c2ea2bb890feae4942df941dc3f97890ed347478"],
        },
        {
            .label = @"7-6",
            .keys = keys[7],
            .message = [NSData dataWithHexString:@"2184827095d35c3f86f600e8e59754013296"],
            .seed = [NSData dataWithHexString:@"52673bde2ca166c2aa46131ac1dc808d67d7d3b1"],
            .encryptedMessage = [NSData dataWithHexString:@"14c678a94ad60525ef39e959b2f3ba5c097a94ff912b67dbace80535c187abd47d075420b1872152bba08f7fc31f313bbf9273c912fc4c0149a9b0cfb79807e346eb332069611bec0ff9bcd168f1f7c33e77313cea454b94e2549eecf002e2acf7f6f2d2845d4fe0aab2e5a92ddf68c480ae11247935d1f62574842216ae674115"],
        },
        {
            .label = @"8-1",
            .keys = keys[8],
            .message = [NSData dataWithHexString:@"050b755e5e6880f7b9e9d692a74c37aae449b31bfea6deff83747a897f6c2c825bb1adbf850a3c96994b5de5b33cbc7d4a17913a7967"],
            .seed = [NSData dataWithHexString:@"7706ffca1ecfb1ebee2a55e5c6e24cd2797a4125"],
            .encryptedMessage = [NSData dataWithHexString:@"09b3683d8a2eb0fb295b62ed1fb9290b714457b7825319f4647872af889b30409472020ad12912bf19b11d4819f49614824ffd84d09c0a17e7d17309d12919790410aa2995699f6a86dbe3242b5acc23af45691080d6b1ae810fb3e3057087f0970092ce00be9562ff4053b6262ce0caa93e13723d2e3a5ba075d45f0d61b54b61"],
        },
        {
            .label = @"8-2",
            .keys = keys[8],
            .message = [NSData dataWithHexString:@"4eb68dcd93ca9b19df111bd43608f557026fe4aa1d5cfac227a3eb5ab9548c18a06dded23f81825986b2fcd71109ecef7eff88873f075c2aa0c469f69c92bc"],
            .seed = [NSData dataWithHexString:@"a3717da143b4dcffbc742665a8fa950585548343"],
            .encryptedMessage = [NSData dataWithHexString:@"2ecf15c97c5a15b1476ae986b371b57a24284f4a162a8d0c8182e7905e792256f1812ba5f83f1f7a130e42dcc02232844edc14a31a68ee97ae564a383a3411656424c5f62ddb646093c367be1fcda426cf00a06d8acb7e57776fbbd855ac3df506fc16b1d7c3f2110f3d8068e91e186363831c8409680d8da9ecd8cf1fa20ee39d"],
        },
        {
            .label = @"8-3",
            .keys = keys[8],
            .message = [NSData dataWithHexString:@"8604ac56328c1ab5ad917861"],
            .seed = [NSData dataWithHexString:@"ee06209073cca026bb264e5185bf8c68b7739f86"],
            .encryptedMessage = [NSData dataWithHexString:@"4bc89130a5b2dabb7c2fcf90eb5d0eaf9e681b7146a38f3173a3d9cfec52ea9e0a41932e648a9d69344c50da763f51a03c95762131e8052254dcd2248cba40fd31667786ce05a2b7b531ac9dac9ed584a59b677c1a8aed8c5d15d68c05569e2be780bf7db638fd2bfd2a85ab276860f3777338fca989ffd743d13ee08e0ca9893f"],
        },
        {
            .label = @"8-4",
            .keys = keys[8],
            .message = [NSData dataWithHexString:@"fdda5fbf6ec361a9d9a4ac68af216a0686f438b1e0e5c36b955f74e107f39c0dddcc"],
            .seed = [NSData dataWithHexString:@"990ad573dc48a973235b6d82543618f2e955105d"],
            .encryptedMessage = [NSData dataWithHexString:@"2e456847d8fc36ff0147d6993594b9397227d577752c79d0f904fcb039d4d812fea605a7b574dd82ca786f93752348438ee9f5b5454985d5f0e1699e3e7ad175a32e15f03deb042ab9fe1dd9db1bb86f8c089ccb45e7ef0c5ee7ca9b7290ca6b15bed47039788a8a93ff83e0e8d6244c71006362deef69b6f416fb3c684383fbd0"],
        },
        {
            .label = @"8-5",
            .keys = keys[8],
            .message = [NSData dataWithHexString:@"4a5f4914bee25de3c69341de07"],
            .seed = [NSData dataWithHexString:@"ecc63b28f0756f22f52ac8e6ec1251a6ec304718"],
            .encryptedMessage = [NSData dataWithHexString:@"1fb9356fd5c4b1796db2ebf7d0d393cc810adf6145defc2fce714f79d93800d5e2ac211ea8bbecca4b654b94c3b18b30dd576ce34dc95436ef57a09415645923359a5d7b4171ef22c24670f1b229d3603e91f76671b7df97e7317c97734476d5f3d17d21cf82b5ba9f83df2e588d36984fd1b584468bd23b2e875f32f68953f7b2"],
        },
        {
            .label = @"8-6",
            .keys = keys[8],
            .message = [NSData dataWithHexString:@"8e07d66f7b880a72563abcd3f35092bc33409fb7f88f2472be"],
            .seed = [NSData dataWithHexString:@"3925c71b362d40a0a6de42145579ba1e7dd459fc"],
            .encryptedMessage = [NSData dataWithHexString:@"3afd9c6600147b21798d818c655a0f4c9212db26d0b0dfdc2a7594ccb3d22f5bf1d7c3e112cd73fc7d509c7a8bafdd3c274d1399009f9609ec4be6477e453f075aa33db382870c1c3409aef392d7386ae3a696b99a94b4da0589447e955d16c98b17602a59bd736279fcd8fb280c4462d590bfa9bf13fed570eafde97330a2c210"],
        },
        {
            .label = @"9-1",
            .keys = keys[9],
            .message = [NSData dataWithHexString:@"f735fd55ba92592c3b52b8f9c4f69aaa1cbef8fe88add095595412467f9cf4ec0b896c59eda16210e7549c8abb10cdbc21a12ec9b6b5b8fd2f10399eb6"],
            .seed = [NSData dataWithHexString:@"8ec965f134a3ec9931e92a1ca0dc8169d5ea705c"],
            .encryptedMessage = [NSData dataWithHexString:@"267bcd118acab1fc8ba81c85d73003cb8610fa55c1d97da8d48a7c7f06896a4db751aa284255b9d36ad65f37653d829f1b37f97b8001942545b2fc2c55a7376ca7a1be4b1760c8e05a33e5aa2526b8d98e317088e7834c755b2a59b12631a182c05d5d43ab1779264f8456f515ce57dfdf512d5493dab7b7338dc4b7d78db9c091ac3baf537a69fc7f549d979f0eff9a94fda4169bd4d1d19a69c99e33c3b55490d501b39b1edae118ff6793a153261584d3a5f39f6e682e3d17c8cd1261fa72"],
        },
        {
            .label = @"9-2",
            .keys = keys[9],
            .message = [NSData dataWithHexString:@"81b906605015a63aabe42ddf11e1978912f5404c7474b26dce3ed482bf961ecc818bf420c54659"],
            .seed = [NSData dataWithHexString:@"ecb1b8b25fa50cdab08e56042867f4af5826d16c"],
            .encryptedMessage = [NSData dataWithHexString:@"93ac9f0671ec29acbb444effc1a5741351d60fdb0e393fbf754acf0de49761a14841df7772e9bc82773966a1584c4d72baea00118f83f35cca6e537cbd4d811f5583b29783d8a6d94cd31be70d6f526c10ff09c6fa7ce069795a3fcd0511fd5fcb564bcc80ea9c78f38b80012539d8a4ddf6fe81e9cddb7f50dbbbbcc7e5d86097ccf4ec49189fb8bf318be6d5a0715d516b49af191258cd32dc833ce6eb4673c03a19bbace88cc54895f636cc0c1ec89096d11ce235a265ca1764232a689ae8"],
        },
        {
            .label = @"9-3",
            .keys = keys[9],
            .message = [NSData dataWithHexString:@"fd326429df9b890e09b54b18b8f34f1e24"],
            .seed = [NSData dataWithHexString:@"e89bb032c6ce622cbdb53bc9466014ea77f777c0"],
            .encryptedMessage = [NSData dataWithHexString:@"81ebdd95054b0c822ef9ad7693f5a87adfb4b4c4ce70df2df84ed49c04da58ba5fc20a19e1a6e8b7a3900b22796dc4e869ee6b42792d15a8eceb56c09c69914e813cea8f6931e4b8ed6f421af298d595c97f4789c7caa612c7ef360984c21b93edc5401068b5af4c78a8771b984d53b8ea8adf2f6a7d4a0ba76c75e1dd9f658f20ded4a46071d46d7791b56803d8fea7f0b0f8e41ae3f09383a6f9585fe7753eaaffd2bf94563108beecc207bbb535f5fcc705f0dde9f708c62f49a9c90371d3"],
        },
        {
            .label = @"9-4",
            .keys = keys[9],
            .message = [NSData dataWithHexString:@"f1459b5f0c92f01a0f723a2e5662484d8f8c0a20fc29dad6acd43bb5f3effdf4e1b63e07fdfe6628d0d74ca19bf2d69e4a0abf86d293925a796772f8088e"],
            .seed = [NSData dataWithHexString:@"606f3b99c0b9ccd771eaa29ea0e4c884f3189ccc"],
            .encryptedMessage = [NSData dataWithHexString:@"bcc35f94cde66cb1136625d625b94432a35b22f3d2fa11a613ff0fca5bd57f87b902ccdc1cd0aebcb0715ee869d1d1fe395f6793003f5eca465059c88660d446ff5f0818552022557e38c08a67ead991262254f10682975ec56397768537f4977af6d5f6aaceb7fb25dec5937230231fd8978af49119a29f29e424ab8272b47562792d5c94f774b8829d0b0d9f1a8c9eddf37574d5fa248eefa9c5271fc5ec2579c81bdd61b410fa61fe36e424221c113addb275664c801d34ca8c6351e4a858"],
        },
        {
            .label = @"9-5",
            .keys = keys[9],
            .message = [NSData dataWithHexString:@"53e6e8c729d6f9c319dd317e74b0db8e4ccca25f3c8305746e137ac63a63ef3739e7b595abb96e8d55e54f7bd41ab433378ffb911d"],
            .seed = [NSData dataWithHexString:@"fcbc421402e9ecabc6082afa40ba5f26522c840e"],
            .encryptedMessage = [NSData dataWithHexString:@"232afbc927fa08c2f6a27b87d4a5cb09c07dc26fae73d73a90558839f4fd66d281b87ec734bce237ba166698ed829106a7de6942cd6cdce78fed8d2e4d81428e66490d036264cef92af941d3e35055fe3981e14d29cbb9a4f67473063baec79a1179f5a17c9c1832f2838fd7d5e59bb9659d56dce8a019edef1bb3accc697cc6cc7a778f60a064c7f6f5d529c6210262e003de583e81e3167b89971fb8c0e15d44fffef89b53d8d64dd797d159b56d2b08ea5307ea12c241bd58d4ee278a1f2e"],
        },
        {
            .label = @"9-6",
            .keys = keys[9],
            .message = [NSData dataWithHexString:@"b6b28ea2198d0c1008bc64"],
            .seed = [NSData dataWithHexString:@"23aade0e1e08bb9b9a78d2302a52f9c21b2e1ba2"],
            .encryptedMessage = [NSData dataWithHexString:@"438cc7dc08a68da249e42505f8573ba60e2c2773d5b290f4cf9dff718e842081c383e67024a0f29594ea987b9d25e4b738f285970d195abb3a8c8054e3d79d6b9c9a8327ba596f1259e27126674766907d8d582ff3a8476154929adb1e6d1235b2ccb4ec8f663ba9cc670a92bebd853c8dbf69c6436d016f61add836e94732450434207f9fd4c43dec2a12a958efa01efe2669899b5e604c255c55fb7166de5589e369597bb09168c06dd5db177e06a1740eb2d5c82faeca6d92fcee9931ba9f"],
        },
        {
            .label = @"10-1",
            .keys = keys[10],
            .message = [NSData dataWithHexString:@"8bba6bf82a6c0f86d5f1756e97956870b08953b06b4eb205bc1694ee"],
            .seed = [NSData dataWithHexString:@"47e1ab7119fee56c95ee5eaad86f40d0aa63bd33"],
            .encryptedMessage = [NSData dataWithHexString:@"53ea5dc08cd260fb3b858567287fa91552c30b2febfba213f0ae87702d068d19bab07fe574523dfb42139d68c3c5afeee0bfe4cb7969cbf382b804d6e61396144e2d0e60741f8993c3014b58b9b1957a8babcd23af854f4c356fb1662aa72bfcc7e586559dc4280d160c126785a723ebeebeff71f11594440aaef87d10793a8774a239d4a04c87fe1467b9daf85208ec6c7255794a96cc29142f9a8bd418e3c1fd67344b0cd0829df3b2bec60253196293c6b34d3f75d32f213dd45c6273d505adf4cced1057cb758fc26aeefa441255ed4e64c199ee075e7f16646182fdb464739b68ab5daff0e63e9552016824f054bf4d3c8c90a97bb6b6553284eb429fcc"],
        },
        {
            .label = @"10-2",
            .keys = keys[10],
            .message = [NSData dataWithHexString:@"e6ad181f053b58a904f2457510373e57"],
            .seed = [NSData dataWithHexString:@"6d17f5b4c1ffac351d195bf7b09d09f09a4079cf"],
            .encryptedMessage = [NSData dataWithHexString:@"a2b1a430a9d657e2fa1c2bb5ed43ffb25c05a308fe9093c01031795f5874400110828ae58fb9b581ce9dddd3e549ae04a0985459bde6c626594e7b05dc4278b2a1465c1368408823c85e96dc66c3a30983c639664fc4569a37fe21e5a195b5776eed2df8d8d361af686e750229bbd663f161868a50615e0c337bec0ca35fec0bb19c36eb2e0bbcc0582fa1d93aacdb061063f59f2ce1ee43605e5d89eca183d2acdfe9f81011022ad3b43a3dd417dac94b4e11ea81b192966e966b182082e71964607b4f8002f36299844a11f2ae0faeac2eae70f8f4f98088acdcd0ac556e9fccc511521908fad26f04c64201450305778758b0538bf8b5bb144a828e629795"],
        },
        {
            .label = @"10-3",
            .keys = keys[10],
            .message = [NSData dataWithHexString:@"510a2cf60e866fa2340553c94ea39fbc256311e83e94454b4124"],
            .seed = [NSData dataWithHexString:@"385387514deccc7c740dd8cdf9daee49a1cbfd54"],
            .encryptedMessage = [NSData dataWithHexString:@"9886c3e6764a8b9a84e84148ebd8c3b1aa8050381a78f668714c16d9cfd2a6edc56979c535d9dee3b44b85c18be8928992371711472216d95dda98d2ee8347c9b14dffdff84aa48d25ac06f7d7e65398ac967b1ce90925f67dce049b7f812db0742997a74d44fe81dbe0e7a3feaf2e5c40af888d550ddbbe3bc20657a29543f8fc2913b9bd1a61b2ab2256ec409bbd7dc0d17717ea25c43f42ed27df8738bf4afc6766ff7aff0859555ee283920f4c8a63c4a7340cbafddc339ecdb4b0515002f96c932b5b79167af699c0ad3fccfdf0f44e85a70262bf2e18fe34b850589975e867ff969d48eabf212271546cdc05a69ecb526e52870c836f307bd798780ede"],
        },
        {
            .label = @"10-4",
            .keys = keys[10],
            .message = [NSData dataWithHexString:@"bcdd190da3b7d300df9a06e22caae2a75f10c91ff667b7c16bde8b53064a2649a94045c9"],
            .seed = [NSData dataWithHexString:@"5caca6a0f764161a9684f85d92b6e0ef37ca8b65"],
            .encryptedMessage = [NSData dataWithHexString:@"6318e9fb5c0d05e5307e1683436e903293ac4642358aaa223d7163013aba87e2dfda8e60c6860e29a1e92686163ea0b9175f329ca3b131a1edd3a77759a8b97bad6a4f8f4396f28cf6f39ca58112e48160d6e203daa5856f3aca5ffed577af499408e3dfd233e3e604dbe34a9c4c9082de65527cac6331d29dc80e0508a0fa7122e7f329f6cca5cfa34d4d1da417805457e008bec549e478ff9e12a763c477d15bbb78f5b69bd57830fc2c4ed686d79bc72a95d85f88134c6b0afe56a8ccfbc855828bb339bd17909cf1d70de3335ae07039093e606d655365de6550b872cd6de1d440ee031b61945f629ad8a353b0d40939e96a3c450d2a8d5eee9f678093c8"],
        },
        {
            .label = @"10-5",
            .keys = keys[10],
            .message = [NSData dataWithHexString:@"a7dd6c7dc24b46f9dd5f1e91ada4c3b3df947e877232a9"],
            .seed = [NSData dataWithHexString:@"95bca9e3859894b3dd869fa7ecd5bbc6401bf3e4"],
            .encryptedMessage = [NSData dataWithHexString:@"75290872ccfd4a4505660d651f56da6daa09ca1301d890632f6a992f3d565cee464afded40ed3b5be9356714ea5aa7655f4a1366c2f17c728f6f2c5a5d1f8e28429bc4e6f8f2cff8da8dc0e0a9808e45fd09ea2fa40cb2b6ce6ffff5c0e159d11b68d90a85f7b84e103b09e682666480c657505c0929259468a314786d74eab131573cf234bf57db7d9e66cc6748192e002dc0deea930585f0831fdcd9bc33d51f79ed2ffc16bcf4d59812fcebcaa3f9069b0e445686d644c25ccf63b456ee5fa6ffe96f19cdf751fed9eaf35957754dbf4bfea5216aa1844dc507cb2d080e722eba150308c2b5ff1193620f1766ecf4481bafb943bd292877f2136ca494aba0"],
        },
        {
            .label = @"10-6",
            .keys = keys[10],
            .message = [NSData dataWithHexString:@"eaf1a73a1b0c4609537de69cd9228bbcfb9a8ca8c6c3efaf056fe4a7f4634ed00b7c39ec6922d7b8ea2c04ebac"],
            .seed = [NSData dataWithHexString:@"9f47ddf42e97eea856a9bdbc714eb3ac22f6eb32"],
            .encryptedMessage = [NSData dataWithHexString:@"2d207a73432a8fb4c03051b3f73b28a61764098dfa34c47a20995f8115aa6816679b557e82dbee584908c6e69782d7deb34dbd65af063d57fca76a5fd069492fd6068d9984d209350565a62e5c77f23038c12cb10c6634709b547c46f6b4a709bd85ca122d74465ef97762c29763e06dbc7a9e738c78bfca0102dc5e79d65b973f28240caab2e161a78b57d262457ed8195d53e3c7ae9da021883c6db7c24afdd2322eac972ad3c354c5fcef1e146c3a0290fb67adf007066e00428d2cec18ce58f9328698defef4b2eb5ec76918fde1c198cbb38b7afc67626a9aefec4322bfd90d2563481c9a221f78c8272c82d1b62ab914e1c69f6af6ef30ca5260db4a46"],
        },
        
    };
    
	int max_cycles = 100;
	if (!getenv("SUBMISSION_TEST")) {
		max_cycles = 10;
		CFfprintf(stderr, "Running the far faster but far less reliable fast test.\nSet the SUBMISSION_TEST environment variable for full testing\n");
	}
	
    for (int j = 0; j < max_cycles; j++) {
        NSLog(@"Cycle %d", j);
        for (i = 0; i < sizeof(tests)/sizeof(KAT); i++) {
            NSLog(@"test#%d %@ L(IN)=%lu, L(OUT)=%lu", i, tests[i].label, [tests[i].message length], [tests[i].encryptedMessage length]);
            CFErrorRef err = NULL;
            
            SecTransformRef encryptor = SecEncryptTransformCreate(tests[i].keys.pubKey, &err);
            
            SecTransformSetAttribute(encryptor, kSecTransformInputAttributeName, tests[i].message, &err);
            SecTransformSetAttribute(encryptor, kSecPaddingKey, kSecPaddingOAEPKey, &err);
            SecTransformSetAttribute(encryptor, kSecOAEPEncodingParametersAttributeName, encoding_parameters, &err);
            SecTransformSetAttribute(encryptor, CFSTR("FixedSeedForOAEPTesting"), tests[i].seed, &err);
            
            CFTypeRef encryptedData = SecTransformExecute(encryptor, &err);
            STAssertNotNil((id)encryptedData, @"Expected to get encrypted data");
            STAssertNil((NSError*)err, @"Expected no error, got err=%@", err);
			// Can't support "seed" with commoncrypto, just check round trip.
            //STAssertEqualObjects((id)encryptedData, (id)tests[i].encryptedMessage, @"encrypted data should have matched test vector (%@) data", tests[i].label);
            CFRelease(encryptor);
            
            SecTransformRef decryptor = SecDecryptTransformCreate(tests[i].keys.privKey, NULL);
            SecTransformSetAttribute(decryptor, kSecTransformInputAttributeName, tests[i].encryptedMessage, NULL);
			// XXX: totally round trip, not even partial KAT (KAT can't really be done on OAEP
			// without supporitng settign the seed externally)
            SecTransformSetAttribute(decryptor, kSecTransformInputAttributeName, encryptedData, NULL);
            SecTransformSetAttribute(decryptor, kSecPaddingKey, kSecPaddingOAEPKey, NULL);
            SecTransformSetAttribute(decryptor, kSecOAEPEncodingParametersAttributeName, encoding_parameters, NULL);
            CFTypeRef decryptedData = SecTransformExecute(decryptor, &err);
            STAssertNil((id)err, @"Expected no error, got: %@", err);
            STAssertNotNil((id)decryptedData, @"Expected to get decrypted data");
            STAssertEqualObjects((id)decryptedData, tests[i].message, @"Expected decrypted data to match original message (%@)", tests[i].label);
            CFRelease(decryptor);
        }
    }
    
    return;
}

-(void)testNoSignKeyMakesError
{
    NSData *data = [NSData dataWithBytes:"" length:1];
    
    struct test_case {
        NSString *name;
        CFErrorRef createError;
        SecTransformRef transform;
    } test_cases[] = {
        {
            .name = @"Sign",
            .createError = NULL,
            .transform = SecSignTransformCreate(NULL, &(test_cases[0].createError))
        },
        {
            .name = @"Verify",
            .createError = NULL,
            .transform = SecVerifyTransformCreate(NULL, (CFDataRef)data, &(test_cases[1].createError))
        }
    };
    
    for(int i = 0; i < sizeof(test_cases) / sizeof(test_case); i++) {
        struct test_case *test = test_cases + i;
        STAssertNil((id)test->createError, @"Testing %@, unexpected error: %@", test->name, test->createError);
        STAssertNotNil((id)test->transform, @"Didn't manage to create transform for %@", test->name);
        if (!test->transform) {
            continue;
        }
        
        __block CFErrorRef err = NULL;
        SecTransformSetAttribute(test->transform, kSecTransformInputAttributeName, data, &err);
        STAssertNil((id)err, @"Error setting input for %@: %@", test->name, err);
        
        dispatch_group_t execute_done = dispatch_group_create();
        dispatch_group_enter(execute_done);
        
        SecTransformExecuteAsync(test->transform, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, NULL), ^(CFTypeRef message, CFErrorRef error, Boolean isFinal) {
            if (error) {
                err = error;
            }
            if (isFinal) {
                dispatch_group_leave(execute_done);
            }
        });
        
        STAssertFalse(dispatch_group_wait(execute_done, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 5)), @"Timeout waiting for %@ transform", test->name);
        STAssertErrorHas((id)err, @"missing required attributes?:.*KEY", @"Unexpected error during %@ test, expected one about missing keys: %@", test->name, err);
        dispatch_group_notify(execute_done, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, NULL), ^(void) {
            dispatch_release(execute_done);
        });
    }
}

-(void)testHMAC
{
	// make the data for the key and the data to be HMAC'd
	CFDataRef hmacData = CFDataCreate(NULL, (u_int8_t*) gHMACText, strlen(gHMACText));
	CFDataRef hmacKey = CFDataCreate(NULL, (u_int8_t*) gHMACKey, strlen(gHMACKey));
	SecTransformRef hmacRef;
	CFErrorRef error = NULL;
	CFDataRef result;
	CFDataRef rightAnswer;
	CFComparisonResult ok;

	// create the object
	hmacRef = SecDigestTransformCreate(kSecDigestHMACSHA1, 20, &error);
	STAssertNil((id) error, @"Unexpected error returned.");
	
	// set the key
	SecTransformSetAttribute(hmacRef, kSecDigestHMACKeyAttribute, hmacKey, &error);
	STAssertNil((id) error, @"Unexpected error returned.");
	
	// digest the data
	SecTransformSetAttribute(hmacRef, kSecTransformInputAttributeName, hmacData, &error);
	STAssertNil((id) error, @"Unexpected error returned.");
	
	result = (CFDataRef) SecTransformExecute(hmacRef, &error);
	if (error)
	{
		CFShow(error);
		STAssertNil((id) error, @"Unexpected error returned.");
		CFRelease(error);
	}
	
	STAssertNotNil((id) result, @"No data returned for SHA1");
	
	// check to make sure we got the right answer
	rightAnswer = CFDataCreate(NULL, gSHA1HMAC, sizeof(gSHA1HMAC));
	ok = CFEqual(rightAnswer, result);
	CFRelease(rightAnswer);
	CFRelease(hmacRef);
	CFRelease(result);
	
	if (error)
	{
		CFRelease(error);
	}
	
	STAssertTrue(ok, @"Digest returned incorrect HMACSHA1 result.");

	//+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+*+

	// create the object
	hmacRef = SecDigestTransformCreate(kSecDigestHMACSHA2, 256, &error);
	STAssertNil((id) error, @"Unexpected error returned.");
	
	// set the key
	SecTransformSetAttribute(hmacRef, kSecDigestHMACKeyAttribute, hmacKey, &error);
	STAssertNil((id) error, @"Unexpected error returned.");
	
	// digest the data
	SecTransformSetAttribute(hmacRef, kSecTransformInputAttributeName, hmacData, &error);
	STAssertNil((id) error, @"Unexpected error returned.");
	
	result = (CFDataRef) SecTransformExecute(hmacRef, &error);
	if (error != nil)
	{
		CFShow(error);
		STAssertNil((id) error, @"Unexpected error returned.");
		CFRelease(error);
	}
	
	STAssertNotNil((id) result, @"No data returned for SHA256");
	
	rightAnswer = CFDataCreate(NULL, gSHA256HMAC, sizeof(gSHA256HMAC));
	ok = CFEqual(result, rightAnswer);

	CFRelease(rightAnswer);
	CFRelease(hmacRef);

	CFRelease(hmacData);
	CFRelease(hmacKey);
	CFRelease(result);
	
	STAssertTrue(ok, @"Digest returned incorrect HMACSHA256 result.");
}


	
-(void)echoParams:(NSString*)p1 p2:(NSString*)p2
{
}

-(void)testReadStreamTransform
{
	// point to our test data
	CFURLRef url = CFURLCreateWithFileSystemPath(NULL, CFSTR("/usr/share/dict/words"), kCFURLPOSIXPathStyle, false);
    FSRef force_resolve;
    STAssertTrue(CFURLGetFSRef(url, &force_resolve), @"Expected to create FSRef from %@", url);
    CFURLRef resolved_url = CFURLCreateFromFSRef(NULL, &force_resolve);
	CFNumberRef size_on_disk = NULL;
    CFURLCopyResourcePropertyForKey(resolved_url, kCFURLFileSizeKey, &size_on_disk, NULL);
    STAssertNotNil((id)size_on_disk, @"Expected to fetch size");
    
	CFReadStreamRef readStreamRef = CFReadStreamCreateWithFile(NULL, url);
	SecTransformRef transform = SecTransformCreateReadTransformWithReadStream(readStreamRef);
	STAssertNotNil((id) transform, @"Returned transform should not be nil.");
	
	dispatch_semaphore_t waitSemaphore = dispatch_semaphore_create(0);
	dispatch_queue_t queue = dispatch_queue_create("ReadStream queue", NULL);
    __block ssize_t bytes_presented = 0;

	SecTransformExecuteAsync(transform, queue,
	^(CFTypeRef message, CFErrorRef error, Boolean isFinal)
	{
		if (message)
        {
            bytes_presented += CFDataGetLength((CFDataRef)message);
        }
        if (isFinal)
		{
			STAssertNil((id)error, @"Unexpected error!");
			dispatch_semaphore_signal(waitSemaphore);
		}
	});
	
	dispatch_semaphore_wait(waitSemaphore, DISPATCH_TIME_FOREVER);
	NSNumber *size_via_stream = [NSNumber numberWithLongLong:bytes_presented];
    STAssertEqualObjects(size_via_stream, (NSNumber*)size_on_disk, @"Expected sizes to match");
    CFRelease(size_on_disk);
    
	dispatch_release(queue);
	dispatch_release(waitSemaphore);
	CFRelease(transform);
	CFRelease(readStreamRef);
	CFRelease(url);
    CFRelease(resolved_url);
}

-(void)testMGF
{
    UInt8 raw_seed[] = {0xaa, 0xfd, 0x12, 0xf6, 0x59, 0xca, 0xe6, 0x34, 0x89, 0xb4, 0x79, 0xe5, 0x07, 0x6d, 0xde, 0xc2, 0xf0, 0x6c, 0xb5, 0x8f};
    UInt8 raw_mgf107[] = {0x06, 0xe1, 0xde, 0xb2, 0x36, 0x9a, 0xa5, 0xa5, 0xc7, 0x07, 0xd8, 0x2c, 0x8e, 0x4e, 0x93, 0x24, 0x8a, 0xc7, 0x83, 0xde, 0xe0, 0xb2, 0xc0, 0x46, 0x26, 0xf5, 0xaf, 0xf9, 0x3e, 0xdc, 0xfb, 0x25, 0xc9, 0xc2, 0xb3, 0xff, 0x8a, 0xe1, 0x0e, 0x83, 0x9a, 0x2d, 0xdb, 0x4c, 0xdc, 0xfe, 0x4f, 0xf4, 0x77, 0x28, 0xb4, 0xa1, 0xb7, 0xc1, 0x36, 0x2b, 0xaa, 0xd2, 0x9a, 0xb4, 0x8d, 0x28, 0x69, 0xd5, 0x02, 0x41, 0x21, 0x43, 0x58, 0x11, 0x59, 0x1b, 0xe3, 0x92, 0xf9, 0x82, 0xfb, 0x3e, 0x87, 0xd0, 0x95, 0xae, 0xb4, 0x04, 0x48, 0xdb, 0x97, 0x2f, 0x3a, 0xc1, 0x4e, 0xaf, 0xf4, 0x9c, 0x8c, 0x3b, 0x7c, 0xfc, 0x95, 0x1a, 0x51, 0xec, 0xd1, 0xdd, 0xe6, 0x12, 0x64};
    CFDataRef seed = CFDataCreate(NULL, raw_seed, sizeof(raw_seed));
    CFDataRef mgf107 = CFDataCreate(NULL, raw_mgf107, sizeof(raw_mgf107));
    CFErrorRef err = NULL;
    
    SecTransformRef mgfTransform = SecCreateMaskGenerationFunctionTransform(NULL, 107, &err);
    STAssertNotNil((id)mgfTransform, @"Expected to create a MGF transform e=%@", err);
    err = NULL;
    SecTransformSetAttribute(mgfTransform, kSecTransformInputAttributeName, seed, &err);
    STAssertNil((id)err, @"Expected no error setting MGF's input, got %@", err);
    err = NULL;
    CFDataRef mgfOutput = (CFDataRef)SecTransformExecute(mgfTransform, &err);
    STAssertNotNil((id)mgfOutput, @"Expected output from mgf, got error %@", err);
    STAssertEqualObjects((id)mgfOutput, (id)mgf107, @"Expected matching output");
    
    CFRelease(mgfTransform);
    // XXX: leak test??
    
    UInt8 raw_maskedDB[] = {0xdc, 0xd8, 0x7d, 0x5c, 0x68, 0xf1, 0xee, 0xa8, 0xf5, 0x52, 0x67, 0xc3, 0x1b, 0x2e, 0x8b, 0xb4, 0x25, 0x1f, 0x84, 0xd7, 0xe0, 0xb2, 0xc0, 0x46, 0x26, 0xf5, 0xaf, 0xf9, 0x3e, 0xdc, 0xfb, 0x25, 0xc9, 0xc2, 0xb3, 0xff, 0x8a, 0xe1, 0x0e, 0x83, 0x9a, 0x2d, 0xdb, 0x4c, 0xdc, 0xfe, 0x4f, 0xf4, 0x77, 0x28, 0xb4, 0xa1, 0xb7, 0xc1, 0x36, 0x2b, 0xaa, 0xd2, 0x9a, 0xb4, 0x8d, 0x28, 0x69, 0xd5, 0x02, 0x41, 0x21, 0x43, 0x58, 0x11, 0x59, 0x1b, 0xe3, 0x92, 0xf9, 0x82, 0xfb, 0x3e, 0x87, 0xd0, 0x95, 0xae, 0xb4, 0x04, 0x48, 0xdb, 0x97, 0x2f, 0x3a, 0xc1, 0x4f, 0x7b, 0xc2, 0x75, 0x19, 0x52, 0x81, 0xce, 0x32, 0xd2, 0xf1, 0xb7, 0x6d, 0x4d, 0x35, 0x3e, 0x2d};
    
    UInt8 raw_mgf20[] = {0x41, 0x87, 0x0b, 0x5a, 0xb0, 0x29, 0xe6, 0x57, 0xd9, 0x57, 0x50, 0xb5, 0x4c, 0x28, 0x3c, 0x08, 0x72, 0x5d, 0xbe, 0xa9};
    CFDataRef maskedDB = CFDataCreate(NULL, raw_maskedDB, sizeof(raw_maskedDB));
    CFDataRef mgf20 = CFDataCreate(NULL, raw_mgf20, sizeof(raw_mgf20));
    err = NULL;
    
    mgfTransform = SecCreateMaskGenerationFunctionTransform(kSecDigestSHA1, 20, &err);
    STAssertNotNil((id)mgfTransform, @"Expected to create a MGF transform e=%@", err);
    err = NULL;
    SecTransformSetAttribute(mgfTransform, kSecTransformInputAttributeName, maskedDB, &err);
    STAssertNil((id)err, @"Expected no error setting MGF's input, got %@", err);
    err = NULL;
    mgfOutput = (CFDataRef)SecTransformExecute(mgfTransform, &err);
    STAssertNotNil((id)mgfOutput, @"Expected output from mgf, got error %@", err);
    STAssertEqualObjects((id)mgfOutput, (id)mgf20, @"Expected matching output");
}

-(void)testAbortParams
{
	// make a simple transform
	SecTransformRef a = SecNullTransformCreate();
	
	// try to abort the transform
	CFErrorRef errorRef = NULL;
	STAssertFalse(SecTransformSetAttribute(a, CFSTR("ABORT"), NULL, &errorRef), @"SecTransformSetAttribute should have returned FALSE");
	STAssertNotNil((id) errorRef, @"SecTransformSetAttribute should have had an error.");
	if (errorRef != NULL)
	{
		CFRelease(errorRef);
	}
	
	CFRelease(a);
	
	// We have instant end of stream, it is wired directly to  null_abort's ABORT.   It is wired to the final drain via a delay and some other
	// things.   If the end of stream makes it to the final drain we get an empty CFData.   If the abort triggers then abort has invalidly
	// triggered off of a NULL value.
	SecGroupTransformRef test_null_abort_via_connection = SecTransformCreateGroupTransform();
	SecTransformRef pass_through = SecNullTransformCreate();
	SecTransformRef null_abort = SecNullTransformCreate();
	
	CFURLRef dev_null_url = CFURLCreateWithFileSystemPath(NULL, CFSTR("/dev/null"), kCFURLPOSIXPathStyle, NO);
	CFReadStreamRef dev_null_stream = CFReadStreamCreateWithFile(NULL, dev_null_url);
	CFReadStreamOpen(dev_null_stream);
	CFRelease(dev_null_url);
	
	SecTransformSetAttribute(pass_through, kSecTransformInputAttributeName, dev_null_stream, NULL);
	SecTransformConnectTransforms(pass_through, kSecTransformOutputAttributeName, null_abort, kSecTransformAbortAttributeName, test_null_abort_via_connection, NULL);
	
	SecTransformRef delay_null = delay_transform(NSEC_PER_SEC / 10);
	SecTransformConnectTransforms(pass_through, kSecTransformOutputAttributeName, delay_null, kSecTransformInputAttributeName, test_null_abort_via_connection, NULL);
	SecTransformConnectTransforms(delay_null, kSecTransformOutputAttributeName, null_abort, kSecTransformInputAttributeName, test_null_abort_via_connection, NULL);
	
	
	CFErrorRef err = NULL;
	CFTypeRef not_null = SecTransformExecute(test_null_abort_via_connection, &err);
	
	STAssertNotNil((id)not_null, @"aborted via a NULL from a connection?  err=%@", err);
	
	if (err)
	{
		CFRelease(err);
	}
	
	CFRelease(test_null_abort_via_connection);
	CFRelease(pass_through);
	CFRelease(null_abort);
	CFRelease(delay_null);
	
	CFReadStreamClose(dev_null_stream);
	CFRelease(dev_null_stream);
}


-(void)testDisconnect
{
	SecTransformRef a = SecNullTransformCreate();
	SecTransformRef b = SecNullTransformCreate();
	SecTransformRef c = SecNullTransformCreate();
	SecGroupTransformRef g = SecTransformCreateGroupTransform();
	
	SecTransformConnectTransforms(a, kSecTransformOutputAttributeName, b, kSecTransformInputAttributeName, g, NULL);
	SecTransformConnectTransforms(a, kSecTransformOutputAttributeName, c, kSecTransformInputAttributeName, g, NULL);
	
	SecTransformDisconnectTransforms(a, kSecTransformOutputAttributeName, b, kSecTransformInputAttributeName);
	STAssertTrue(SecGroupTransformHasMember(g, a), @"A should still be in the group, but isn't");

	SecTransformDisconnectTransforms(a, kSecTransformOutputAttributeName, c, kSecTransformInputAttributeName);
	STAssertFalse(SecGroupTransformHasMember(g, a), @"A should no longer be in the group, but is");
	
	CFRelease(g);
	CFRelease(c);
	CFRelease(b);
	CFRelease(a);
}


-(void)testAbort
{
	CFStringRef abort_test_name = CFSTR("com.apple.security.unit-test.abortTest");
	
	SecTransformCreateBlock setupBlock =
	^(CFStringRef name, SecTransformRef new_transform, const SecTransformCreateBlockParameters *params)
	{
		params->send(kSecTransformInputAttributeName, kSecTransformMetaAttributeDeferred, kCFBooleanTrue);
		
		params->overrideAttribute(kSecTransformActionAttributeNotification, CFSTR("PB"), ^(SecTransformAttributeRef ah, CFTypeRef value) {
			// Makes sure we can shut down (via ABORT) a transform that has a pending pushback
			params->pushback(ah, value);
			return (CFTypeRef)NULL;
		});
	};
	
		
	SecTransformRef a;
	SecTransformRef dt; 
	SecGroupTransformRef group = SecTransformCreateGroupTransform();

	// make two of these transforms and link them together
	a = custom_transform(abort_test_name, setupBlock);
	STAssertNotNil((id) a, @"SecCustomTransformCreate failed");
	
	dt = delay_transform(NSEC_PER_SEC / 10);
	STAssertNotNil((id) dt, @"SecCustomTransformCreate failed");

	// connect the two transforms
	CFErrorRef error;
	
	// hook the output up so that the abort automatically fires.
	SecTransformConnectTransforms(dt, kSecTransformOutputAttributeName, a, CFSTR("ABORT"), group, &error);
	STAssertNil((id) error, @"SecTransformConnectTransforms failed.");
	
	// also hook it up to the input because the input attribute is required on a null transform
	SecTransformConnectTransforms(dt, CFSTR("NOVALUES"), a, kSecTransformInputAttributeName, group, &error);
	STAssertNil((id) error, @"SecTransformConnectTransforms failed.");
	
	// pass a plain piece of data down the transform just for fun...
	const u_int8_t data[] = {3, 1, 4, 1, 5, 9, 2, 6, 5, 4};
	
	CFDataRef dataRef = CFDataCreateWithBytesNoCopy(NULL, data, sizeof(data), kCFAllocatorNull);
	SecTransformSetAttribute(dt, kSecTransformInputAttributeName, dataRef, NULL);
	
	CFStringRef str = CFStringCreateMutable(NULL, 0);
	SecTransformSetAttribute(a, CFSTR("PB"), str, NULL);

	CFTypeRef er = SecTransformExecute(a, &error);
	
	STAssertNil((id)er, @"Didn't expect an result from aborted transform");
	STAssertNotNil((id)error, @"Expected error from execute");
	
	if (error)
	{
		CFShow(error);
		
		// while we are at it, make sure that the user dictionary has the originating transform
		CFDictionaryRef userDictionary = CFErrorCopyUserInfo(error);
		STAssertNotNil((id) CFDictionaryGetValue(userDictionary, kSecTransformAbortOriginatorKey), @"Originating transform not listed.");
		CFRelease(error);
	}
	
	CFRelease(a);
	CFRelease(dt);
	CFRelease(group);
	CFRelease(dataRef);

/*
	// XXX: these should both be 1, not 3 or 4.    WTF?   Is this an abort issue, or a generic leak?
	// STAssertEquals(rc0, CFGetRetainCount(str), @"The value we sent to PB hasn't been released (value retained by pushback)");
	// STAssertEquals(rc0, CFGetRetainCount(dataRef), @"The value we sent to INPUT hasn't been released");
*/
}
                                  
-(void)testPreAbort {
	CFErrorRef error = NULL;
	SecTransformRef prebort = SecNullTransformCreate();
    SecTransformSetAttribute(prebort, kSecTransformInputAttributeName, CFSTR("quux"), NULL);
	SecTransformSetAttribute(prebort, CFSTR("ABORT"), CFSTR("OOPS"), NULL);
	CFTypeRef er = SecTransformExecute(prebort, &error);
	STAssertNil((id)er, @"Didn't expect an result from pre-aborted transform");
	STAssertNotNil((id)error, @"Expected error from execute of pre-aborted transform");
	CFRelease(error);
}

#ifdef DEBUG
-(void)testFireAndForget
{
	bool isGC = false;
	NSGarbageCollector* gc = [NSGarbageCollector defaultCollector];
	if (gc)
	{
		isGC = [gc isEnabled];
	}

	CFIndex retCount = 0;

	// make transforms
	SecNullTransformRef a = SecNullTransformCreate();
	SecNullTransformRef b = SecNullTransformCreate();
	SecGroupTransformRef group = SecTransformCreateGroupTransform();
	SecTransformConnectTransforms(a, kSecTransformOutputAttributeName, b, kSecTransformInputAttributeName, group, NULL);

	if (!isGC)
	{
		retCount = CFGetRetainCount(group);
	}
	
	// set up a blob of data to fire
	const u_int8_t data[] = {3, 1, 4, 1, 5, 9, 2, 6, 5, 4};
	CFDataRef dataRef = CFDataCreateWithBytesNoCopy(NULL, data, sizeof(data), kCFAllocatorNull);
	SecTransformSetAttribute(a, kSecTransformInputAttributeName, dataRef, NULL);
	CFRelease(dataRef);
	
	// make dispatch related stuff
	dispatch_queue_t queue = dispatch_queue_create("ffqueue", NULL);
	// semaphore0's job is to be signaled when we know ExecuteAsync is actually executing (so we don't sample the retain 
	// count too soone), semaphore signals when we are about done with ExecuteAsync (I'm not sure why we need to know),
	// and semaphore2 is signaled to let the execute block know we are done sampling retain counts.
	dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
	dispatch_semaphore_t semaphore2 = dispatch_semaphore_create(0);
	
	// launch the chain
	SecTransformExecuteAsync(group, queue,
						^(CFTypeRef message, CFErrorRef error, Boolean isFinal)
						{
							CFfprintf(stderr, "message %p, final %d\n", message, isFinal ? 1 : 0);
							STAssertEquals(queue, const_cast<const dispatch_queue_t>(dispatch_get_current_queue()), @"Expected to be executing on own queue, got %s", dispatch_queue_get_label(dispatch_get_current_queue()));
							if (isFinal)
							{
								fprintf(stderr, "Final message received.\n");
								dispatch_semaphore_wait(semaphore2, DISPATCH_TIME_FOREVER); // make sure that the other chain has released its material
								dispatch_semaphore_signal(semaphore);
							}
						});
	CFRelease(a);
	CFRelease(b);
	CFRelease(group);
	dispatch_semaphore_signal(semaphore2);
	dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
	
	// no crash?  Life is good.
}
#endif

-(void)testExternalSource
{
	CFErrorRef err = NULL;
	SecTransformRef xs = SecExternalSourceTransformCreate(&err);
	SecTransformRef tee = SecNullTransformCreate();
	SecTransformRef group = SecTransformCreateGroupTransform();
	
	SecTransformConnectTransforms(xs, kSecTransformOutputAttributeName, tee, kSecTransformInputAttributeName, group, &err);
	
	dispatch_queue_t q = dispatch_queue_create("com.apple.security.unit-tests.test-external-source", 0);
	dispatch_group_t dg = dispatch_group_create();
	dispatch_group_enter(dg);
	__block bool got_ping = false;
	
	SecTransformExecuteAsync(group, q, ^(CFTypeRef message, CFErrorRef error, Boolean isFinal) {
		CFfprintf(stderr, "B: message %@, e %p, f %d\n", message ? message : (CFTypeRef)CFSTR("(NULL)"), error, isFinal);
		
		if (message) {
			if (CFEqual(message, CFSTR("PING"))) {
				got_ping = true;
			} else {
				STFail(@"expected ping, got: %@", message);
			}
		}
		
		if (error) {
			STFail(@"unexpected error: %@", error);
		}
		
		if (isFinal) {
			if (!got_ping) {
				STFail(@"never got ping");
			}
			dispatch_group_leave(dg);
		}
	});
	
	SecExternalSourceSetValue(tee, CFSTR("PONG"), &err);
	STAssertNotNil((id)err, @"Expected error setting tee");
	STAssertErrorHas((id)err, @"ExternalSource", @"Error should note what should be passed in: %@", err);
	CFRelease(err);
	err = NULL;
	SecExternalSourceSetValue(xs, CFSTR("PING"), &err);
	STAssertNil((id)err, @"unexpected error setting xs: %@", err);
	SecExternalSourceSetValue(xs, NULL, &err);
	STAssertNil((id)err, @"unexpected error setting xs: %@", err);
	dispatch_group_wait(dg, DISPATCH_TIME_FOREVER);
	
	dispatch_release(dg);
	dispatch_release(q);
	CFRelease(xs);
	CFRelease(tee);
	CFRelease(group);
}

-(void)testFindLastAndMonitor
{
	SecNullTransformRef a = delay_transform(NSEC_PER_SEC / 10);
	SecNullTransformRef b = SecNullTransformCreate();
	
	SecGroupTransformRef groupRef = SecTransformCreateGroupTransform();
	CFErrorRef error = NULL;
	SecTransformConnectTransforms(a, kSecTransformOutputAttributeName, b, kSecTransformInputAttributeName, groupRef, &error);
	STAssertNil((id)error, @"An error was returned when none was expected.");
	SecTransformSetAttribute(a, kSecTransformInputAttributeName, kCFNull, &error);
	STAssertNil((id)error, @"An error was returned when none was expected.");

	
	// get the last transform in the chain (unexecuted).  It had better be b...
	SecTransformRef tr = SecGroupTransformFindLastTransform(groupRef);
	STAssertNotNil((id)tr, @"FindLastTransform returned NULL");
	STAssertTrue(tr == b, @"FindLastTransform returned incorrect result");
	STAssertFalse(tr == a, @"FindLastTransform returned the head of the chain");
	
	// execute the transform. This should attach an output monitor
	dispatch_queue_t queue = dispatch_queue_create("test delivery queue", NULL);
	dispatch_semaphore_t last_block_run = dispatch_semaphore_create(0L);
	dispatch_semaphore_t last_assert_run = dispatch_semaphore_create(0L);
	SecTransformExecuteAsync(groupRef, queue,
		^(CFTypeRef message, CFErrorRef error, Boolean isFinal)
		{ 
			if (isFinal)
			{
				dispatch_semaphore_signal(last_block_run);
				dispatch_semaphore_wait(last_assert_run, DISPATCH_TIME_FOREVER);
				dispatch_release(last_assert_run);
			}
		
		});
	
	dispatch_semaphore_wait(last_block_run, DISPATCH_TIME_FOREVER);
	
	// see if the returned transform is the same now
	tr = SecGroupTransformFindLastTransform(groupRef);
	STAssertNotNil((id) tr, @"FindLastTransform returned NULL");
	STAssertTrue(tr == b, @"FindLastTransform returned incorrect result");
	STAssertFalse(tr == a, @"FindLastTransform returned the head of the chain");
	
	// get the monitor, it had better not be a or b
	tr = SecGroupTransformFindMonitor(groupRef);
	STAssertNotNil((id) tr, @"FindMonitor returned NULL");
	STAssertFalse(tr == a, @"FindLastTransform returned the head of the chain");
	STAssertFalse(tr == b, @"FindLastTransform returned the head of the chain");
	
	dispatch_semaphore_signal(last_assert_run);
	dispatch_release(queue);
	dispatch_release(last_block_run);
	CFRelease(a);
	CFRelease(b);
	CFRelease(groupRef);
}


-(void)testConnectUnsetAttributes /* <rdar://problem/7769955> Can't connect transform attributes with no setting */
{
	SecNullTransformRef a = SecNullTransformCreate();
	SecNullTransformRef b = SecNullTransformCreate();
	
	SecGroupTransformRef group = SecTransformCreateGroupTransform();
	CFErrorRef error = NULL;
	SecTransformConnectTransforms(a, CFSTR("RANDOM_NAME"), b, CFSTR("RANDOM_DESTINATION"), group, &error);
	CFRelease(group);
	CFRelease(b);
	CFRelease(a);
	STAssertNil((id) error, @"An error was returned when none was expected.");
}

-(void)testNoDataFlowPriorToInit /* <rdar://problem/8163542> Monitor must be attached before the data flow becomes active */
{
    CFStringRef name = CFSTR("com.apple.security.unit-test.flow-check");
	SecTransformCreateBlock cb = ^(CFStringRef name, SecTransformRef new_transform, const SecTransformCreateBlockParameters *params) {
		__block bool inited = false;
        __block bool saw_x_start = false;
        __block bool saw_null = false;
		__block int post_send_left = 8;
		SecTransformAttributeRef out_ah = params->get(kSecTransformOutputAttributeName, kSecTransformMetaAttributeRef);
		params->send(out_ah, kSecTransformMetaAttributeValue, CFSTR("create"));
		
		params->overrideTransform(kSecTransformActionStartingExecution, ^{
			params->send(out_ah, kSecTransformMetaAttributeValue, CFSTR("x-start"));
			inited = true;
			return (CFTypeRef)NULL;
		});
		
		params->overrideTransform(kSecTransformActionCanExecute, ^{
			params->send(out_ah, kSecTransformMetaAttributeValue, CFSTR("can-x"));
			return (CFTypeRef)NULL;
		});

		params->overrideAttribute(kSecTransformActionAttributeNotification, kSecTransformInputAttributeName, ^(SecTransformAttributeRef ah, CFTypeRef value) {
			if (inited) {
				if (value) {
                    if (!saw_x_start) {
                        saw_x_start = CFStringHasPrefix((CFStringRef)value, CFSTR("x-start"));
                    }
                    if (saw_null) {
                        params->send(kSecTransformAbortAttributeName, kSecTransformMetaAttributeValue, CreateGenericErrorRef(name, 88, "saw %@ after NULL", value));
                    }
					if (post_send_left--) {
						params->send(out_ah, kSecTransformMetaAttributeValue, CFSTR("post-init"));
					}
				} else {
                    saw_null = true;
                    // The FIRST flow transform should not see x-start (it is the OUTPUT of the flow transform
                    // before you in the chain), but all the other transforms should see it.
                    if (params->get(CFSTR("FIRST"), kSecTransformMetaAttributeValue)) {
                        if (saw_x_start) {
                            params->send(kSecTransformAbortAttributeName, kSecTransformMetaAttributeValue, CreateGenericErrorRef(name, 42, "saw bogus x-start on FIRST flow transform"));
                        } else {
                            params->send(out_ah, kSecTransformMetaAttributeValue, value);
                        }
                    } else {
                        if (saw_x_start) {
                            params->send(out_ah, kSecTransformMetaAttributeValue, value);
                        } else {
                            params->send(kSecTransformAbortAttributeName, kSecTransformMetaAttributeValue, CreateGenericErrorRef(name, 42, "never saw x-start before EOS"));
                            return (CFTypeRef)kCFNull;
                        }
                    }
				}
			} else {
				// attempting to put the value in the error string sometimes blows up, so I've left it out.
				params->send(kSecTransformAbortAttributeName, kSecTransformMetaAttributeValue, CreateGenericErrorRef(name, 42, "got: value before init"));
                return (CFTypeRef)kCFNull;
			}
			return (CFTypeRef)value;
		});
	};
	
	// Reliably reproduces with 100000 transforms in our group, but
	// not at 1000...doesn't even seem to do it at 50000.
	// Likely a timing issue triggered by heavy swapping.
	int n_transforms = 100000;

	if (!getenv("SUBMISSION_TEST")) {
		n_transforms = 10000;
		CFfprintf(stderr, "Running the far faster but far less reliable fast test.\nSet the SUBMISSION_TEST environment variable for full testing\n");
	}
	
	int i;
	CFMutableArrayRef ta = CFArrayCreateMutable(NULL, n_transforms, &kCFTypeArrayCallBacks);
	CFErrorRef err = NULL;

	for(i = 0; i < n_transforms; ++i) {
		SecTransformRef tr = custom_transform(name, cb);
		STAssertNil((id)err, @"Failure %@ creating %@ transform", err, name);
		CFStringRef l = CFStringCreateWithFormat(NULL, NULL, CFSTR("flow-%d"), i);
		SecTransformSetAttribute(tr, kSecTransformTransformName, l, &err);
		if (0 == i % 1000) {
			CFfprintf(stderr, "Created %@ of %d\n", l, n_transforms);
		}
		CFRelease(l);
		STAssertNil((id)err, @"Can't set name %@", err);
		CFArrayAppendValue(ta, tr);
		assert(CFArrayGetCount(ta));
		assert(CFArrayGetCount(ta) == i+1);
	}

	SecTransformRef prev_tr = NULL;
	SecTransformRef group = SecTransformCreateGroupTransform();
	CFIndex cnt;

	while ((cnt = CFArrayGetCount(ta))) {
		CFIndex r = arc4random() % cnt;
		SecTransformRef tr = CFArrayGetValueAtIndex(ta, r);
		if (prev_tr) {
			SecTransformConnectTransforms(tr, kSecTransformOutputAttributeName, prev_tr, kSecTransformInputAttributeName, group, &err);
			STAssertNil((id)err, @"Can't connect %@ to %@", tr, prev_tr);
			STAssertNotNil((id)group, @"nil group after connect");
			CFRelease(prev_tr);
		}
		prev_tr = tr;
		CFArrayRemoveValueAtIndex(ta, r);
		
		if (0 == cnt % 1000) {
			CFfprintf(stderr, "%d left to hook up\n", cnt);
		}
	}
	
	CFTypeRef ptl = SecTransformGetAttribute(prev_tr, kSecTransformTransformName);
	CFfprintf(stderr, "Setting INPUT for %@\n", ptl);
	SecTransformSetAttribute(prev_tr, kSecTransformInputAttributeName, CFSTR("First!"), &err);
	STAssertNil((id)err, @"Can't set first's input?  %@", err);
	SecTransformSetAttribute(prev_tr, CFSTR("FIRST"), kCFBooleanTrue, &err);
	STAssertNil((id)err, @"Can't set FIRST?  %@", err);
	CFTypeRef r = SecTransformExecute(group, &err);
	STAssertNil((id)err, @"execution error: %@", err);
	STAssertNotNil((id)r, @"result expected from execute");
	CFRelease(group);
	CFRelease(prev_tr);
}

-(void)testNoDataDescription /* <rdar://problem/7791122> CFShow(SecCustomTransformNoData()) crashes */
{
	CFStringRef result = CFCopyDescription(SecTransformNoData()); // this is called under the hood in CFShow, and it doesn't dump output
	STAssertNotNil((id)result, @"SecTransformNoData can be formatted");
	CFRelease(result);
}

static SecTransformInstanceBlock KnownProblemPlumbing(CFStringRef name, 
							SecTransformRef newTransform, 
							SecTransformImplementationRef ref)
{
	SecTransformInstanceBlock instanceBlock = 
	^{
		CFErrorRef result = NULL;
		// At the moment fully disconnecting a transform from a group leaves it in the group, so it can't have any
		// kSecTransformMetaAttributeRequiresOutboundConnection=TRUE attributes (by default OUTPUT requires an outbound connection)
		SecTransformCustomSetAttribute(ref, kSecTransformOutputAttributeName, 
			kSecTransformMetaAttributeRequiresOutboundConnection, kCFBooleanFalse);
		
		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, kSecTransformInputAttributeName, 
			^(SecTransformAttributeRef ah, CFTypeRef value) 
			{
				return (CFTypeRef)CFErrorCreate(NULL, kCFErrorDomainPOSIX, EOPNOTSUPP, NULL);
			});
		return result;
	};
	
	return Block_copy(instanceBlock);
}
							
-(void)knownProblemPlumbing // note, this test has been disconnected!
{
	SecNullTransformRef a = SecNullTransformCreate();
	CFStringRef name = CFSTR("com.apple.security.unit-test.error+outputless");
	CFErrorRef err = NULL;
	
	SecTransformRegister(name, &KnownProblemPlumbing, &err); 
	
	SecTransformRef b = SecTransformCreate(name, NULL);
	SecGroupTransformRef group = SecTransformCreateGroupTransform();
	
	SecTransformConnectTransforms(a, kSecTransformOutputAttributeName, b, kSecTransformInputAttributeName, group, NULL);
	SecTransformDisconnectTransforms(a, kSecTransformOutputAttributeName, b, kSecTransformInputAttributeName);
	
	
	CFStringRef data = CFSTR("Test");
	
	SecTransformSetAttribute(a, kSecTransformInputAttributeName, data, NULL);
	CFTypeRef result = SecTransformExecute(a, &err);
	
	STAssertEqualObjects((id)data, (id)result, @"Plumbing disconnect failed.");
	STAssertNil((id)err, @"Unexpected error=%@", err);
	CFRelease(a);
	CFRelease(b);
	CFRelease(group);
}

-(void)testUnknownEncodeType {
	CFErrorRef err1 = NULL;
	CFStringRef invalid_encode_type = CFSTR("no such encoding type ☃✪");
	SecTransformRef not_created = SecEncodeTransformCreate(invalid_encode_type, &err1);
	STAssertNil((id)not_created, @"Created encode transform with bad type");
	STAssertErrorHas((NSError*)err1, @"☃✪", @"Error mentions bad encoding type: %@", err1);
	fprintf(stderr, "Err1 = %p\n", err1);
	if (err1) CFRelease(err1);
	err1 = NULL;
	
	CFErrorRef err2 = NULL;
	SecTransformRef no_type_at_create = SecEncodeTransformCreate(NULL, &err2);
	fprintf(stderr, "Err2 = %p\n", err2);
	STAssertNotNil((id)no_type_at_create, @"should be able to create encoder with unset type, but got error %@", err2);
	
	if (no_type_at_create) {
		CFErrorRef err3 = NULL;
        SecTransformSetAttribute(no_type_at_create, kSecTransformInputAttributeName, NULL, &err3);
        STAssertNil((id)err3, @"Can't set INPUT: %@", err3);
		if (err3) {
            CFRelease(err3);
            err3 = NULL;
        }
		STAssertTrue(SecTransformSetAttribute(no_type_at_create, kSecEncodeTypeAttribute, kSecBase32Encoding, &err3), @"Can't set encode to valid type error: %@", err3);
		STAssertFalse(SecTransformSetAttribute(no_type_at_create, kSecEncodeTypeAttribute, invalid_encode_type, &err3), @"Set encode to invalid type, no error signaled", err3);
		fprintf(stderr, "Err3 = %p\n", err3);
		if (err3) CFRelease(err3);

		CFErrorRef err4 = NULL;
		CFTypeRef no_result = SecTransformExecute(no_type_at_create, &err4);
		STAssertNil((id)no_result, @"Got result when none expected %@");
		STAssertNotNil((id)err4, @"No error when one expected");
		fprintf(stderr, "Err4 = %p\n", err4);
		if (err4) CFRelease(err4);
	} else {
		STFail(@"Unable to run some tests");
	}
	
	CFRelease(no_type_at_create);
	CFRelease(invalid_encode_type);
}

-(void)testNoUnderscores 
{
	SecTransformRef zt = SecEncodeTransformCreate(kSecZLibEncoding, NULL);
	CFErrorRef err = NULL;
	SecTransformSetAttribute(zt, CFSTR("_FAIL"), kCFBooleanTrue, &err);
	STAssertNotNil((id)err, @"Expeced an error setting _FAIL");
	STAssertErrorHas((id)err, @"_FAIL", @"Expected error to contain _FAIL");
	STAssertErrorHas((id)err, @"Encoder", @"Expecting error to name offending transform", err);
	CFTypeRef v = SecTransformGetAttribute(zt, CFSTR("_FAIL"));
	STAssertNil((id)v, @"Expected nil result, got v=%p", v);
	CFRelease(err);
	CFRelease(zt);
}

-(void)testCanFetchDigestSizes
{
    NSDictionary *digests = [NSDictionary dictionaryWithObjectsAndKeys:
                             [NSNumber numberWithInt:128/8], kSecDigestMD2,
                             [NSNumber numberWithInt:128/8], kSecDigestMD4,
                             [NSNumber numberWithInt:128/8], kSecDigestMD5,
                             [NSNumber numberWithInt:160/8], kSecDigestSHA1,
                             [NSNumber numberWithInt:512/8], kSecDigestSHA2,
                             nil];
    NSData *zero = [NSData dataWithBytes:"" length:1];
    
    for (NSString *digestType in digests) {
        CFErrorRef err = NULL;
        SecTransformRef digest = SecDigestTransformCreate(digestType, 0, &err);
        STAssertNotNil((id)digest, @"Expected to make digest (err=%@)", err);
        STAssertNil((id)err, @"Unexpected error: %@", err);
        NSNumber *actualLength = (NSNumber*)SecTransformGetAttribute(digest, kSecDigestLengthAttribute);
        STAssertTrue([actualLength intValue] != 0, @"Got zero length back");
        STAssertNotNil(actualLength, @"Expected to get a length");
        STAssertEqualObjects(actualLength, [digests objectForKey:digestType], @"Expected lengths to match for %@", digestType);
        
        SecTransformSetAttribute(digest, kSecTransformInputAttributeName, zero, &err);
        STAssertNil((id)err, @"Unexpected error: %@", err);
        
        NSData *output = (NSData *)SecTransformExecute(digest, &err);
        STAssertNil((id)err, @"Unexpected error: %@", err);
        STAssertNotNil((id)output, @"No output");
        
        STAssertEquals([actualLength intValue], (int)[output length], @"Actual output not expected length");

        [output release];
        CFRelease(digest);
    }
}

-(void)testBadTransformTypeNames
{
    CFErrorRef error = NULL;
    Boolean ok = SecTransformRegister(CFSTR("Not valid: has a col..co...double dot thing"), DelayTransformBlock, &error);
    STAssertFalse(ok, @"Register of name with : fails");
    STAssertErrorHas((id)error, @":", @"Error mentions invalid character (error=%@)", error);
    
    ok = SecTransformRegister(CFSTR("Not/valid has a slash"), DelayTransformBlock, &error);
    STAssertFalse(ok, @"Register of name with / fails");
    STAssertErrorHas((id)error, @"/", @"Error mentions invalid character (error=%@)", error);

    ok = SecTransformRegister(CFSTR("https://NOT/VALID"), DelayTransformBlock, &error);
    STAssertFalse(ok, @"Register of name with : and / fails");
    STAssertErrorHas((id)error, @"[:/]", @"Error mentions invalid character (error=%@)", error);
    
    ok = SecTransformRegister(CFSTR("_not valid at start"), DelayTransformBlock, &error);
    STAssertFalse(ok, @"Register of _name fails");
    STAssertErrorHas((id)error, @"_", @"Error mentions invalid character (error=%@)", error);

    ok = SecTransformRegister(CFSTR("it is ok to have a _ after start"), DelayTransformBlock, &error);
    STAssertTrue(ok, @"Register of _ IN should have worked (error=%@)", error);
}
                                  
-(void)testExecuteBlock {
	unsigned char *raw_data = (unsigned char *)"Just some bytes, you know";
	NSData *empty = [NSData dataWithBytes:NULL length:0];
	NSData *data = [NSData dataWithBytes:raw_data length:strlen((const char *)raw_data)];
	NSUInteger ecnt = [empty retainCount];
	
	SecTransformRef zt = SecEncodeTransformCreate(kSecZLibEncoding, NULL);
	SecTransformSetAttribute(zt, kSecTransformInputAttributeName, data, NULL);
	dispatch_queue_t q = dispatch_queue_create("com.apple.security.testingQ", NULL);
	dispatch_queue_t q_sync = dispatch_queue_create("com.apple.security.testingQ_sync", NULL);
	dispatch_suspend((dispatch_object_t)q_sync);
	__block BOOL ran_block = NO;
	
	SecTransformExecuteAsync(zt, q, ^(CFTypeRef message, CFErrorRef error, Boolean isFinal) {
		if ([empty length]) {
			NSLog(@"Empty data not so empty");
		}
		STAssertTrue(dispatch_get_current_queue() == q, @"block dispatched to proper queue");
		
		if (!ran_block) {
			usleep(200);
			ran_block = YES;
		}
		
		if (message == NULL) {
			dispatch_resume((dispatch_object_t)q_sync);
		}
	});
	
	STAssertTrue(ecnt < [empty retainCount], @"SecTransformExecute retained block");
	dispatch_sync(q_sync, ^{ });
	STAssertTrue(ran_block, @"Block executed");
	
	dispatch_release(q_sync);
	dispatch_release(q);
	CFRelease(zt);
	
	// test for 7735698
	// STAssertTrue(ecnt == [empty retainCount], @"SecTransformExecute released block");
}

static SecTransformInstanceBlock ConnectionCheck(CFStringRef name, 
							SecTransformRef newTransform, 
							SecTransformImplementationRef ref)
{
	SecTransformInstanceBlock instanceBlock = 
	^{
		CFErrorRef result = NULL;
		__block SecTransformMetaAttributeType dir = kSecTransformMetaAttributeValue;
		
		SecTransformAttributeRef out_ah = 
			SecTranformCustomGetAttribute(ref, CFSTR("OUTPUT"), kSecTransformMetaAttributeRef);
		
		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, CFSTR("DIRECTION"), 
			^(SecTransformAttributeRef ah, CFTypeRef value) 
			{
				if (CFEqual(value, CFSTR("<"))) 
				{
					dir = kSecTransformMetaAttributeHasInboundConnection;
				} 
				else if (CFEqual(value, CFSTR(">"))) 
				{
					dir = kSecTransformMetaAttributeHasOutboundConnections;
				} 
				else 
				{
					return (CFTypeRef)CreateSecTransformErrorRef(kSecTransformErrorInvalidInput, "Unsupported direction %@, expected < or >", value);
				}
				return value;
			});
		
		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, CFSTR("INPUT"), 
			^(SecTransformAttributeRef ah, CFTypeRef value) 
			{
				if (dir != kSecTransformMetaAttributeValue) 
				{
					if (value) 
					{
						SecTransformCustomSetAttribute(ref, out_ah, kSecTransformMetaAttributeValue, 
									SecTranformCustomGetAttribute(ref, value, dir));
					} 
					else 
					{
						SecTransformCustomSetAttribute(ref, out_ah, kSecTransformMetaAttributeValue, NULL);
					}
				} 
				else 
				{
					SecTransformPushbackAttribute(ref, ah, value);
				}
			
				return value;
			});
		
		return result;
	};
	
	return Block_copy(instanceBlock);	
}

-(void)testConnectionChecks {
	
	CFStringRef name = CFSTR("com.apple.security.unit-test.connection-checks");
	CFErrorRef error = NULL;
	SecTransformRegister(name, &*ConnectionCheck, &error); 
	
	struct test_case {
		NSString *attr, *dir;
		CFBooleanRef expect;
	} cases[] = {
		{@"INPUT", @"<", kCFBooleanTrue},
		{@"OUTPUT", @">", kCFBooleanTrue},
		{@"INPUT", @">", kCFBooleanFalse},
		{@"OUTPUT", @"<", kCFBooleanFalse},
		{@"DIRECTION", @"<", kCFBooleanFalse},
		{@"DIRECTION", @">", kCFBooleanFalse},
	};
	
	CFIndex i, n = sizeof(cases)/sizeof(test_case);
	for(i = 0; i < n; ++i) 
	{
		test_case *t = cases + i;
		
		SecTransformRef cct = SecTransformCreate(name, NULL);
		SecTransformRef tee0 = SecNullTransformCreate();
		SecTransformRef tee1 = SecNullTransformCreate();
		SecTransformRef group = SecTransformCreateGroupTransform();
		
		SecTransformSetAttribute(cct, CFSTR("DEBUG"), kCFBooleanTrue, NULL);
		SecTransformSetAttribute(tee0, CFSTR("DEBUG"), kCFBooleanTrue, NULL);
		SecTransformSetAttribute(tee1, CFSTR("DEBUG"), kCFBooleanTrue, NULL);
		
		SecTransformSetAttribute(tee0, CFSTR("NAME"), CFSTR("tee0"), NULL);
		SecTransformSetAttribute(tee1, CFSTR("NAME"), CFSTR("tee1"), NULL);
		
		SecTransformConnectTransforms(cct, CFSTR("OUTPUT"), tee1, CFSTR("INPUT"), group, NULL);
		SecTransformConnectTransforms(tee0, CFSTR("OUTPUT"), cct, CFSTR("INPUT"), group, NULL);
		
		SecTransformSetAttribute(cct, CFSTR("DIRECTION"), t->dir, NULL);
		SecTransformSetAttribute(tee0, CFSTR("INPUT"), t->attr, NULL);
		CFErrorRef err = NULL;
		CFTypeRef r = SecTransformExecute(group, &err);
		STAssertNil((id)err, @"Error=%@ for case#%d", err, i);
		STAssertNotNil((id)r, @"Nil result for case#%d", i);
		STAssertEqualObjects((id)(t->expect), (id)r, @"Expected result for case#%d %@%@", i, t->dir, t->attr);
		
		CFRelease(cct);
		CFRelease(tee0);
		CFRelease(tee1);
		CFRelease(group);
	}
	 
}

static SecTransformInstanceBlock PushBackTest(CFStringRef name, 
							SecTransformRef newTransform, 
							SecTransformImplementationRef ref)
{
	SecTransformInstanceBlock instanceBlock = 
	^{
		CFErrorRef result = NULL;
		__block CFStringRef input_d = NULL;
		__block CFStringRef data_d = NULL;
		
		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, CFSTR("DATA"), 
			^(SecTransformAttributeRef ah, CFTypeRef value) 
			{
				if (!input_d) 
				{
					SecTransformPushbackAttribute(ref, ah, value);
				} 
				else 
				{
					if (data_d) 
					{
						CFRelease(data_d);
					}
					data_d = (CFStringRef)CFRetain(value);
				}
				return value;
			});
	
		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, CFSTR("INPUT"), 
			^(SecTransformAttributeRef ah, CFTypeRef value) 
			{
				if (value) 
				{
					if (input_d) 
					{
						CFRelease(input_d);
					}
					input_d = (CFStringRef)CFRetain(value);
					if (!data_d) 
					{
						SecTransformPushbackAttribute(ref, ah, value);
						return value;
					}				
				} 
				else 
				{
					if (data_d) 
					{
						SecTransformPushbackAttribute(ref, ah, NULL);
						value = data_d;
						data_d = NULL;
					}
				}
				SecTransformCustomSetAttribute(ref, CFSTR("OUTPUT"), kSecTransformMetaAttributeValue, value);
				return value;
			});
			
		return result;
	};
	
	return Block_copy(instanceBlock);
}

-(void)testPushback {

	CFStringRef name = CFSTR("com.apple.security.unit-test.basic-pushback");
	CFStringRef one = CFSTR("1");
	CFStringRef two = CFSTR("2");
	CFStringRef expect = CFSTR("12");																														

	// This unit test makes pushback look very complex, but that is because we are abusing it for test purposes.
	// normally it is a simple "if I need X before I can go on, and X isn't here yet pushback".   Here we attempt
	// to carefully sequence 2 attributes to be the inverse of the normal order AND test pushback of NULL as well
	// as normal values.
	
	CFErrorRef error = NULL;
	SecTransformRegister(name, &PushBackTest, &error); 
	
	SecTransformRef pt = SecTransformCreate(name, NULL);

	SecTransformSetAttribute(pt, CFSTR("DATA"), two, NULL);
	SecTransformSetAttribute(pt, CFSTR("INPUT"), one, NULL);
	
	CFTypeRef result = SecTransformExecute(pt, NULL);
	
	STAssertEqualObjects((id)result, (id)expect, @"Testing pushback");
	
	CFRelease(pt);
	
	// NOTE: we want to test doing a double pushback, but that sets the Abort attribute which currently causes an abort not an orderly shutdown
 
}

static SecTransformInstanceBlock CustomExternalization(CFStringRef name, 
							SecTransformRef newTransform, 
							SecTransformImplementationRef ref)
{	
	SecTransformInstanceBlock instanceBlock = 
	^{
		CFErrorRef result = NULL;
		// Create a non-attribute 'instance' variable which will contain
		// the version number of this class
		
		__block float versionNumber = 1.0;
		
		// Register the custom externalize override
		SecTransformActionBlock ExternalizeExtraDataOverride = 
		^{
			CFStringRef key = CFSTR("VersionNumber");
			CFNumberRef value = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &versionNumber);
			
			CFDictionaryRef result = CFDictionaryCreate(kCFAllocatorDefault, 
				(const void **)&key, (const void **)&value, 
				1, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
				
			CFRelease(value);
			
			
			return (CFTypeRef)result;
			
		};
		SecTransformSetTransformAction(ref, kSecTransformActionExternalizeExtraData, ExternalizeExtraDataOverride);
		
		// Register the custom internalize override
		SecTransformDataBlock InternalizeExtraDataOverride = 
			^(CFTypeRef d)
		{
			CFTypeRef internalizeResult = NULL;
			id testObj = (id)d;
			
			//STAssertNotNil(testObj, @"Internalize did NOT get a dictionary!");
				
			if (CFDictionaryGetTypeID() == CFGetTypeID(d))
			{
				CFStringRef key = CFSTR("VersionNumber");
				CFDictionaryRef dict = (CFDictionaryRef)d;
				CFNumberRef varsNum = (CFNumberRef)CFDictionaryGetValue(dict, key);
			//	STAssertNotNil((NSNumber *)varsNum, 
			//		@"Unable to retrieve the dictionary when the internalized data override");
				if (NULL != varsNum)
				{
					Boolean numResult = CFNumberGetValue(varsNum, kCFNumberFloatType, &versionNumber);
			//		STAssertTrue(numResult, @"Could not get the version number from the CFNumberRef");
					if (numResult)
					{
						float knownVersion = 1.0;
			//			STAssertTrue(knownVersion == versionNumber, @"Versions do not Match!");						
					}
				}
			}
			return internalizeResult;
		};
		SecTransformSetDataAction(ref, kSecTransformActionInternalizeExtraData, 
			InternalizeExtraDataOverride);
		
		return result;
	};
		
	return Block_copy(instanceBlock);	
}

/* --------------------------------------------------------------------------
	method:			testCustomExternalization
	description:	Test the ability to write out custom external data
   -------------------------------------------------------------------------- */
- (void)testCustomExternalization
{
	NSString* ctName = @"com.apple.security.unit-test-customExternalization";
	NSError* error = nil;
			
	CFStringRef aName = (CFStringRef)ctName;
	CFErrorRef* anError = (CFErrorRef *)&error;
	
	Boolean registerResult = SecTransformRegister(aName, &CustomExternalization, anError);
	STAssertTrue(registerResult, @"Unable to register the custom externalization transform");
	
	SecTransformRef externalTrans = SecTransformCreate((CFStringRef)ctName, 
		(CFErrorRef *)&error);
		
	STAssertNotNil((id)externalTrans, @"Could not create the custom externalization transform");
	
	CFDictionaryRef externalData = SecTransformCopyExternalRepresentation(externalTrans);
	STAssertNotNil((NSDictionary *)externalData, @"Did not get a dictionary from SecTransformCopyExternalRepresentation");

	CFRelease(externalTrans);
	
	externalTrans = NULL;
	externalTrans = SecTransformCreateFromExternalRepresentation(externalData, (CFErrorRef *)&error);
	STAssertNotNil((id)externalTrans, @"Could not create the custom external representation");
	CFRelease(externalData);
	if (NULL != externalTrans)
	{
		CFRelease(externalTrans);
	}
}

static SecTransformInstanceBlock TestString(CFStringRef name, 
							SecTransformRef newTransform, 
							SecTransformImplementationRef ref)
{
	SecTransformInstanceBlock instanceBlock = 
	^{
		CFErrorRef result = NULL;
		SecTransformSetDataAction(ref, kSecTransformActionProcessData, 
			^(CFTypeRef value) 
			{
				CFDataRef d = (CFDataRef)value;
				if (d) {
					return (CFTypeRef)CFStringCreateWithBytes(NULL, CFDataGetBytePtr(d), CFDataGetLength(d), kCFStringEncodingMacRoman, FALSE);
				} else {
					return (CFTypeRef)d;
				}
			});
		return result;
	};
	
	return Block_copy(instanceBlock);
}

-(void)testStringResults {
	CFStringRef name = CFSTR("com.apple.security.unit-test.string-converter");
	CFErrorRef error = NULL;
	SecTransformRegister(name, &TestString, &error);
		
	SecTransformRef sr = SecTransformCreate(name, NULL);
	
	unsigned char *msg = (unsigned char *)"This is a test message, it isn't large, but it will get broken into parts by the encode/decode transforms...";
	CFDataRef data = CFDataCreate(NULL, msg, strlen((const char *)msg));
	NSString *ns_msg = [NSString stringWithCString:(const char *)msg encoding:NSMacOSRomanStringEncoding];
	
	SecTransformRef er = SecEncodeTransformCreate(kSecBase32Encoding, NULL);
	SecTransformRef dr = SecDecodeTransformCreate(kSecBase32Encoding, NULL);
	
	SecTransformSetAttribute(er, kSecTransformInputAttributeName, data, NULL);
	
	SecGroupTransformRef group = SecTransformCreateGroupTransform();
	SecTransformConnectTransforms(er, kSecTransformOutputAttributeName, dr, kSecTransformInputAttributeName, group, NULL);
	SecTransformConnectTransforms(dr, kSecTransformOutputAttributeName, sr, kSecTransformInputAttributeName, group, NULL);


	CFStringRef result = (CFStringRef)SecTransformExecute(sr, NULL);
	STAssertEqualObjects(ns_msg, (NSString *)result, @"string results");

	CFRelease(result);
	CFRelease(group);
	CFRelease(dr);
	CFRelease(er);
	CFRelease(sr);
	if (error)
	{
		CFRelease(error);
	}
}

CFNumberRef MakeNumber1(long n)
{
	return CFNumberCreate(NULL, kCFNumberLongType, &n);
}



static SecTransformInstanceBlock TestRegisterCreate(CFStringRef name, 
							SecTransformRef newTransform, 
							SecTransformImplementationRef ref)
{
	
	
	SecTransformInstanceBlock instanceBlock = 
	^{
		__block long count = 0;
		
		__block CFNumberRef countNum = MakeNumber1(count);;
		SecTransformCustomSetAttribute(ref, CFSTR("Count"), kSecTransformMetaAttributeValue, countNum);
		CFRelease(countNum);
		fprintf(stderr, "countNum = %p\n", countNum);

		CFErrorRef result = NULL;
		SecTransformSetDataAction(ref, kSecTransformActionProcessData,
			^(CFTypeRef value) 
			{
				CFDataRef d = (CFDataRef)value;
				if (d) 
				{
					count += CFDataGetLength(d);
					
					CFNumberRef countNum2 = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongType, &count);
					SecTransformCustomSetAttribute(ref, CFSTR("Count"), kSecTransformMetaAttributeValue, countNum2);
					CFRelease(countNum2);
					fprintf(stderr, "countNum = %p\n", countNum);
					
				} else 
				{
					SecTransformCustomSetAttribute(ref, CFSTR("Count"), kSecTransformMetaAttributeValue, NULL);
				}
				return value;
			});
			
		
		return result;
	};
	
	return Block_copy(instanceBlock);	
}

- (void)testRegisterCreate 
{
	CFStringRef name = CFSTR("com.apple.security.unit-test.novel-unique-at-least-unusual-name");
	long count = 0;
	CFNumberRef countNum = NULL;
	CFErrorRef error = NULL;
	Boolean ok = SecTransformRegister(name, &TestRegisterCreate, &error);
	STAssertTrue(ok, @"Successful register");
	
	SecTransformRef tr = SecTransformCreate(name, NULL);
	STAssertNotNil((NSObject *)tr, @"newly created custom transform");
	SecTransformSetAttribute(tr, CFSTR("DEBUG"), kCFBooleanTrue, NULL);
	
	char *data_bytes = (char *)"It was the best of transforms, it was the worst of transforms.";
	CFDataRef data = CFDataCreate(NULL, (const UInt8 *)data_bytes, strlen(data_bytes));
	
	SecTransformSetAttribute(tr, kSecTransformInputAttributeName, data, NULL);
	
	SecTransformRef nt = SecNullTransformCreate();
	SecTransformRef tg = SecTransformCreateGroupTransform();
	SecTransformConnectTransforms(tr, CFSTR("OUTPUT"), nt, CFSTR("DISCARD"), tg, &error);
	STAssertNil((id)error, @"Connected tr's output to nt's discard: %@", error);
	SecTransformConnectTransforms(tr, CFSTR("Count"), nt, CFSTR("INPUT"), tg, &error);
	STAssertNil((id)error, @"Connected tr's count to nt's input: %@", error);
	
	SecTransformSetAttribute(nt, CFSTR("DEBUG"), kCFBooleanTrue, NULL);
	
	usleep(100);
	countNum = (CFNumberRef)SecTransformGetAttribute(tr, CFSTR("Count"));
	CFNumberGetValue(countNum, kCFNumberLongType, &count);
	CFRelease(countNum);	
	STAssertTrue(count == 0, @"length unchanged before execute");
	
	countNum = (CFNumberRef)SecTransformExecute(tg, NULL);
	STAssertNotNil((id)countNum, @"Got result from execute");
	STAssertEquals(CFGetTypeID(countNum), CFNumberGetTypeID(), @"expected a number from execute");
	CFNumberGetValue(countNum, kCFNumberLongType, &count);
	CFRelease(countNum);

	STAssertTrue(count == CFDataGetLength(data), @"Wrong data length");
	
	CFRelease(tg);
	CFRelease(nt);
	CFRelease(tr);
	CFRelease(data);
}


static SecTransformInstanceBlock CountTransformTest(CFStringRef name, 
							SecTransformRef newTransform, 
							SecTransformImplementationRef ref)
{
	SecTransformInstanceBlock instanceBlock = 
	^{
		CFErrorRef result = NULL;
		SecTransformSetAttributeAction(ref,kSecTransformActionAttributeNotification, CFSTR("INPUT"), 
			^(SecTransformAttributeRef ah, CFTypeRef value) 
			{
				if (value) {
					if (CFGetTypeID(value) != CFNumberGetTypeID()) {
						SecTransformCustomSetAttribute(ref, CFSTR("ABORT"), kSecTransformMetaAttributeValue, CFSTR("Bad type"));
						return value;
					}
					CFNumberRef nr = (CFNumberRef)value;
					int max, i;
					CFNumberGetValue(nr, kCFNumberIntType, &max);
					for(i = 0; i < max; ++i) {
						nr = CFNumberCreate(NULL, kCFNumberIntType, &i);
						SecTransformCustomSetAttribute(ref, CFSTR("OUTPUT"), kSecTransformMetaAttributeValue, nr);
						CFRelease(nr);
					}
				} else {
					SecTransformCustomSetAttribute(ref, CFSTR("OUTPUT"), kSecTransformMetaAttributeValue, value);
				}

				return value;
			});

		return result;
	};
	
	return Block_copy(instanceBlock);
}
							
SecTransformRef count_transform(int n) {
	CFStringRef name = CFSTR("com.apple.security.unit-test.count");
	static dispatch_once_t once;

	dispatch_once(&once, 
	^{
		SecTransformRegister(name, &CountTransformTest, NULL);
	});
	
	SecTransformRef ct = SecTransformCreate(name, NULL);
	CFNumberRef num = CFNumberCreate(NULL, kCFNumberIntType, &n);
	SecTransformSetAttribute(ct, CFSTR("INPUT"), num, NULL);
	CFRelease(num);
	
	return ct;
}


static SecTransformInstanceBlock StallTest(CFStringRef name, 
							SecTransformRef newTransform, 
							SecTransformImplementationRef ref)
{
	SecTransformInstanceBlock instanceBlock = 
	^{
		CFErrorRef result = NULL;
		__block bool go = false;

		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, CFSTR("GO"), 
			^(SecTransformAttributeRef ah, CFTypeRef value) 
			{
				go = true;
				return value;
			});

		SecTransformAttributeRef in_ah = SecTransformCustomGetAttribute(ref, CFSTR("INPUT"), kSecTransformMetaAttributeRef);
		SecTransformAttributeRef out_ah = SecTransformCustomGetAttribute(ref, CFSTR("OUTPUT"), kSecTransformMetaAttributeRef);

		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, NULL, 
			^(SecTransformAttributeRef ah, CFTypeRef value) 
			{
				if (!go) {
					SecTransformPushbackAttribute(ref, ah, value);
				} else {
					if (ah == in_ah) {
						SecTransformCustomSetAttribute(ref, out_ah, kSecTransformMetaAttributeValue, value);
					}
				}
				return value;
			});

		return result;
	};
	
	return Block_copy(instanceBlock);
}
							
-(void)testStall {
	CFStringRef name = CFSTR("com.apple.security.unit-test.stall");
	
	(void)SecTransformRegister(name, &StallTest, NULL);

	SecTransformRef stall = SecTransformCreate(name, NULL);
	SecTransformRef seven = count_transform(7);
	SecTransformRef group = SecTransformCreateGroupTransform();
	SecTransformRef delay = delay_transform(NSEC_PER_SEC / 10);
	
	SecTransformConnectTransforms(seven, CFSTR("OUTPUT"), stall, CFSTR("FOO"), group, NULL);
	SecTransformConnectTransforms(seven, CFSTR("OUTPUT"), stall, CFSTR("BAR"), group, NULL);
	SecTransformConnectTransforms(seven, CFSTR("OUTPUT"), stall, CFSTR("BAZ"), group, NULL);
	SecTransformConnectTransforms(seven, CFSTR("OUTPUT"), stall, CFSTR("INPUT"), group, NULL);
	SecTransformConnectTransforms(delay, CFSTR("OUTPUT"), stall, CFSTR("GO"), group, NULL);
	
	SecTransformSetAttribute(delay, CFSTR("INPUT"), (CFNumberRef)[NSNumber numberWithInt:42], NULL);
	
	CFErrorRef err = NULL;
	CFTypeRef r = SecTransformExecute(group, &err);
	
	STAssertNotNil((id)r, @"Results from testStall");
	STAssertNil((id)err, @"Got %@ error from testStall", err);
	NSArray *array_seven = [NSArray arrayWithObjects:[NSNumber numberWithInt:0], [NSNumber numberWithInt:1], [NSNumber numberWithInt:2], [NSNumber numberWithInt:3], [NSNumber numberWithInt:4], [NSNumber numberWithInt:5], [NSNumber numberWithInt:6], NULL];
	STAssertEqualObjects((id)r, array_seven, @"Correct stall test results");
	
	CFRelease(delay);
	CFRelease(group);
	CFRelease(seven);
	CFRelease(stall);
}

-(void)testInappropriateExecution
{
	// We want to have more then enough work for all the CPUs to help force a race, so twice
	// the number of logical CPUs should do it.   NOTE: the completion blocks got to a low
	// priority concurrent queue to encourage them to finish out of order and put more
	// stress on the system we are testing.

	int logical_cpus = 1;
	size_t int_size = sizeof(logical_cpus);
	int return_code = sysctlbyname("hw.logicalcpu_max", &logical_cpus, &int_size, NULL, 0);
	int e = errno; // Save this value so it doesn't get trashed by any subsequent syscalls
	STAssertEquals(return_code, 0, @"sysctlbyname failed %s (%d), assuming 1 CPU", strerror(e), e);
	
	SecTransformRef count_a_bunch = count_transform(logical_cpus * 2);
	CFErrorRef err = NULL;
	dispatch_group_t wait_for_async_to_complete = dispatch_group_create();
	dispatch_group_t outstanding_executions = dispatch_group_create();
	SecTransformRef count_group = SecTransformCreateGroupTransform();
	
	SecTransformConnectTransforms(count_a_bunch, CFSTR("kludge1"), count_a_bunch, CFSTR("kludge2"), count_group, &err);
	STAssertNil((id)err, @"Error (%@) connecting count transform to itself", err);

	dispatch_group_enter(wait_for_async_to_complete);
	SecTransformExecuteAsync(count_a_bunch, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0), ^(CFTypeRef message, CFErrorRef error, Boolean isFinal)
	{
		dispatch_group_enter(outstanding_executions);
		
		CFErrorRef err = NULL;
		CFTypeRef no_result = SecTransformExecute(count_a_bunch, &err);
		STAssertNil((id)no_result, @"Attempting to execute an already executing transform should fail, not provide results: %@", no_result);
		STAssertNotNil((id)err, @"Attempting to execute an already executing transform should produce some sort of error");
		CFRelease(err);
		
		dispatch_group_leave(outstanding_executions);
		
		if (isFinal)
		{
			// Give any pending executions time to get to the group_enter
			usleep(100);
			dispatch_group_wait(outstanding_executions, DISPATCH_TIME_FOREVER);
			dispatch_release(outstanding_executions);
			dispatch_group_leave(wait_for_async_to_complete);
		}
	});
	
	// Before that SecTransformExecuteAsync completes, we do some more work at >low priority to help
	// keep the completion blocks landing out of order.    In particular we run a transform to
	// completion, and then confirm that we can't run it again.
	
	SecTransformRef no_work = SecNullTransformCreate();
	SecTransformRef no_work_group = SecTransformCreateGroupTransform();
	SecTransformConnectTransforms(no_work, CFSTR("kludge1"), no_work, CFSTR("kludge2"), no_work_group, &err);
	STAssertNil((id)err, @"Can't connect no_work to itself (to make no_work_group), err=%@", err);
	
	SecTransformSetAttribute(no_work, CFSTR("INPUT"), CFSTR("value"), NULL);
	CFTypeRef no_result = SecTransformExecute(no_work_group, &err);
	STAssertNil((id)err, @"First execute of Null Transform should be ok, got e=%@", err);
	STAssertNotNil((id)no_result, @"First execute of Null Transform should produce a value");
	
	no_result = SecTransformExecute(no_work_group, &err);
	
	STAssertNotNil((id)err, @"Second execute of Null Transform should fail!");
	STAssertNil((id)no_result, @"Second execute of Null Transform shouldn't produce a value, got r=%@", no_result);
	CFRelease(err);
	
	// Now we wait for that first batch of tests to finish, we don't want to call STFail after self goes away.
	
	dispatch_group_wait(wait_for_async_to_complete, DISPATCH_TIME_FOREVER);
	dispatch_release(wait_for_async_to_complete);
	
	if (no_result) CFRelease(no_result);
	if (no_work) CFRelease(no_work);
	if (no_work_group) CFRelease(no_work_group);
	if (count_group) CFRelease(count_group);
	if (count_a_bunch) CFRelease(count_a_bunch);
}
								  
static SecTransformInstanceBlock ConnectionReqTest(CFStringRef name, 
							SecTransformRef newTransform, 
							SecTransformImplementationRef ref)
{
	SecTransformInstanceBlock instanceBlock = 
	^{
		CFErrorRef result = NULL;
		SecTransformAttributeRef xah = 
			(SecTransformAttributeRef)SecTranformCustomGetAttribute(ref, CFSTR("XYZZY"), kSecTransformMetaAttributeRef);
		
		SecTransformCustomSetAttribute(ref, xah, kSecTransformMetaAttributeRequiresOutboundConnection, kCFBooleanTrue);
		SecTransformCustomSetAttribute(ref, CFSTR("OUTPUT"), kSecTransformMetaAttributeRequiresOutboundConnection, kCFBooleanFalse);
		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, CFSTR("INPUT"), 
			^(SecTransformAttributeRef ah, CFTypeRef value) 
			{
				SecTransformCustomSetAttribute(ref, xah, kSecTransformMetaAttributeValue, value);
				return value;
			});
		
		return result;
	};
	
	return Block_copy(instanceBlock);	
}


-(void)testConnectionReq {

	CFStringRef req_xyzzy_name = CFSTR("com.apple.security.unit-test.req_xyzzy");
	
	(void)SecTransformRegister(req_xyzzy_name, &ConnectionReqTest, NULL);

	SecTransformRef tr_req_xyzzy = SecTransformCreate(req_xyzzy_name, NULL);
	
	CFTypeRef in_value = (CFTypeRef)@"Fnord";
	SecTransformSetAttribute(tr_req_xyzzy, CFSTR("INPUT"), in_value, NULL);
	
	CFErrorRef err = NULL;
	CFTypeRef r = SecTransformExecute(tr_req_xyzzy, &err);
	
	STAssertNil((id)r, @"Execute of tr_req_xyzzy with no xyzzy r=%@", r);
	STAssertErrorHas((id)err, @"req_xyzzy", @"Error failed to refer to the transform by name (%@)", err);
	STAssertErrorHas((id)err, @"XYZZY", @"Error failed to refer to missing attribute by name (%@)", err);
	STAssertErrorHas((id)err, @"requires.*outbound connection", @"Error failed to diagnose invalid condition (%@)", err);
	
	CFRelease(err);
	CFRelease(tr_req_xyzzy);
	if (r) CFRelease(r);

	/*
	
	Note For Josh:
	
	To make this work we need Josh's fix for FindLastTransform!
	
	CFRelease(tr_req_xyzzy);
	tr_req_xyzzy = SecTransformCreate(req_xyzzy_name, NULL);
	SecTransformSetAttribute(tr_req_xyzzy, CFSTR("INPUT"), in_value, NULL);
	
	SecTransformRef group = SecTransformCreateGroupTransform();
	SecTransformRef tee = SecNullTransformCreate();
	SecTransformConnectTransforms(tr_req_xyzzy, CFSTR("XYZZY"), tee, kSecTransformInputAttributeName, group, &err);
	STAssertNil((id)err, @"err=%@ from connect", err);
	STAssertNotNil((id)group, @"No group after connect");
	r = SecTransformExecute(group, &err);
	STAssertNil((id)err, @"Execute err=%@");
	STAssertEqualObjects((id)in_value, (id)r, @"Execution Result");
	
	*/
}

static SecTransformInstanceBlock DeferredTest(CFStringRef name, 
							SecTransformRef newTransform, 
							SecTransformImplementationRef ref)
{
	SecTransformInstanceBlock instanceBlock = 
	^{
		CFErrorRef result = NULL;
		SecTransformCustomSetAttribute(ref, CFSTR("LATE"), kSecTransformMetaAttributeDeferred, kCFBooleanTrue);
		SecTransformCustomSetAttribute(ref, CFSTR("INPUT"), kSecTransformMetaAttributeDeferred, kCFBooleanFalse);
		
		__block CFTypeRef in_v = NULL, late_v = NULL;
		
		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, CFSTR("INPUT"), 
			^(SecTransformAttributeRef ah, CFTypeRef value) 
			{
				if (NULL != late_v) {
					SecTransformCustomSetAttribute(ref, kSecTransformAbortAttributeName, kSecTransformMetaAttributeValue, 
												   CreateGenericErrorRef(CFSTR("FAIL"), 1, "LATE (%@) should process after INPUT (%@)", late_v, value));
				}
				in_v = value;
				return value;
			});
		
		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, CFSTR("LATE"), 
			^(SecTransformAttributeRef ah, CFTypeRef value) 
			{
				if (NULL == in_v) {
					SecTransformCustomSetAttribute(ref, kSecTransformAbortAttributeName, kSecTransformMetaAttributeValue, 
												   CreateGenericErrorRef(CFSTR("FAIL"), 1, "INPUT (%@) should process before LATE (%@)", in_v, value));
				}
				
				late_v = value;
				SecTransformCustomSetAttribute(ref, CFSTR("OUTPUT"), kSecTransformMetaAttributeValue, NULL);
				return value;
			});
		
		return result;
	};
	
	return Block_copy(instanceBlock);	
}


-(void)testDeferred {
	CFStringRef deferred_name = CFSTR("com.apple.security.unit-test.deferred");
	
	(void)SecTransformRegister(deferred_name, &DeferredTest, NULL);
	
	SecTransformRef dt = SecTransformCreate(deferred_name, NULL);
	
	// these set attribute calls are failing, but we're ignoring the failures
	SecTransformSetAttribute(dt, CFSTR("INPUT"), (CFTypeRef)CFSTR("BLAH"), NULL);
	SecTransformSetAttribute(dt, CFSTR("LATE"), (CFTypeRef)CFSTR("QUUX"), NULL);
	CFErrorRef err = NULL;
	SecTransformExecute(dt, &err);
	STAssertNil((id)err, @"Error from execute err=%@", err);
	
	if (err) CFRelease(err);
	// CFRelease(dt);
}

static SecTransformInstanceBlock SaveRestoreTest(CFStringRef name, 
							SecTransformRef newTransform, 
							SecTransformImplementationRef ref)
{
	SecTransformInstanceBlock instanceBlock = 
	^{
		CFErrorRef result = NULL;
		SecTransformCustomSetAttribute(ref, CFSTR("Needed"), kSecTransformMetaAttributeRequired, kCFBooleanTrue);
		SecTransformCustomSetAttribute(ref, CFSTR("NoSaves"), kSecTransformMetaAttributeExternalize, kCFBooleanFalse);

		return result;
	};
	
	return Block_copy(instanceBlock);	
}

-(void)testSaveRestore 
{
	
	unsigned char raw_data[] = "Val-U-Sav, nw wth lss vwls!";
	CFDataRef data = CFDataCreate(NULL, (const UInt8*)&raw_data, sizeof(raw_data));
	CFErrorRef err = NULL;
	
	CFStringRef name = CFSTR("com.apple.security.unit-test.SaveRestoreTest");
	
	(void)SecTransformRegister(name, &SaveRestoreTest, NULL); 
	
	SecTransformRef tr1 = SecTransformCreate(name, NULL);
	
	SecTransformSetAttribute(tr1, CFSTR("Optional"), CFSTR("42"), NULL);
	SecTransformSetAttribute(tr1, CFSTR("Needed"), CFSTR("and provided"), NULL);
	SecTransformSetAttribute(tr1, CFSTR("NoSaves"), CFSTR("42"), NULL);
	
	CFDictionaryRef xr = SecTransformCopyExternalRepresentation(tr1);
	STAssertNotNil((NSDictionary *)xr, @"external rep");
	SecTransformRef tr2 = SecTransformCreateFromExternalRepresentation(xr, NULL);
	SecTransformSetAttribute(tr2, kSecTransformInputAttributeName, data, NULL);
	CFTypeRef none = SecTransformGetAttribute(tr2, CFSTR("NoSaves"));
	STAssertNil((id)none, @"Expected %@ to be nil", none);
	CFTypeRef forty_two = SecTransformGetAttribute(tr2, CFSTR("Optional"));
	STAssertEqualObjects((id)forty_two, @"42", @"restored incorrect value");
	
	CFDataRef d = (CFDataRef)SecTransformExecute((NSObject *)tr2,  &err);
	
	STAssertNotNil((NSData * )d, @"execute result (err=%@)", err);
	
	if (err) {
		CFRelease(err);
	}
	
	CFRelease(tr1);
	CFRelease(tr2);
	CFRelease(xr);
	
	tr1 = SecTransformCreate(name, NULL);
	SecTransformSetAttribute(tr1, CFSTR("Needed"), CFSTR("and provided"), NULL);
	SecTransformSetAttribute(tr1, CFSTR("NoSaves"), CFSTR("42"), NULL);
	xr = SecTransformCopyExternalRepresentation(tr1);
	tr2 = SecTransformCreateFromExternalRepresentation(xr, NULL);
	
	
	
	//tr2 = SecTransformCreateFromExternalRepresentation(xr, NULL);
	SecTransformRef tga = SecTransformCreateGroupTransform();
	SecTransformSetAttribute(tr1, kSecTransformInputAttributeName, data, NULL);
	
	// XXX did not swap
	SecTransformConnectTransforms(tr1, CFSTR("OUTPUT"), tr2, CFSTR("INPUT"), tga, NULL);
	CFStringRef has1 = CFSTR("I has one!");
	CFStringRef has2 = CFSTR("I has two of them!");
	SecTransformSetAttribute(tr1, CFSTR("Needed"), has1, NULL);
	SecTransformSetAttribute(tr2, CFSTR("Needed"), has2, NULL);
	xr = SecTransformCopyExternalRepresentation(tr1);
	STAssertNotNil((NSDictionary *)xr, @"external rep for 2");
	NSLog(@"xr=%@", xr);
	
	SecTransformRef tgb = SecTransformCreateFromExternalRepresentation(xr, &err);
	STAssertNil((id)tgb, @"made transform group with duplicate labels");
	STAssertErrorHas((id)err, (NSString*)name, @"Error failed to identify the transform (%@)", err);
	STAssertErrorHas((id)err, @"damage|duplicate", @"Error failed to diagnose the invalid condition (%@)", err);
	
	CFStringRef new_name2 = CFSTR("SaveRestoreTestThingie#2");
	CFStringRef fetched_name;
	int attempts;
	
	for(attempts = 0; attempts < 20; ++attempts) 
	{
		SecTransformSetAttribute(tr2, CFSTR("NAME"), new_name2, &err);
		fetched_name = (CFStringRef)SecTransformGetAttribute(tr2, CFSTR("NAME"));
		
		STAssertNil((id)err, @"Error from setting tr2's name: %@", err);
		STAssertEqualObjects((id)fetched_name, (id)new_name2, @"Set tr2's name, attempt %d", attempts);
		if (CFEqual(fetched_name, new_name2)) 
		{
			break;
		}
		if (attempts > 10) 
		{
			usleep(1000);
		}
	}
	
	xr = SecTransformCopyExternalRepresentation(tr1);
	STAssertNotNil((NSDictionary *)xr, @"external rep for 2, take 2");
	NSLog(@"xr=%@", xr);
	
	tgb = SecTransformCreateFromExternalRepresentation(xr, &err);
	STAssertNotNil((id)tgb, @"made transform group (take 2)");
	STAssertNil((id)err, @"error from make 2 take 2 (err=%@)", err);
	
	SecTransformRef tr1b = SecTransformFindByName(tgb, (CFStringRef)SecTransformGetAttribute(tr1, CFSTR("NAME")));
	STAssertNotNil((id)tr1b, @"Found tr1b");
	SecTransformRef tr2b = SecTransformFindByName(tgb, (CFStringRef)SecTransformGetAttribute(tr2, CFSTR("NAME")));
	STAssertNotNil((id)tr2b, @"Found tr2b");
	
	CFStringRef has1b = (CFStringRef)SecTransformGetAttribute(tr1b, CFSTR("Needed"));
	STAssertNotNil((id)tr1b, @"tr1b's name");
	CFStringRef has2b = (CFStringRef)SecTransformGetAttribute(tr2b, CFSTR("Needed"));
	STAssertNotNil((id)tr2b, @"tr1b's name");
	
	STAssertEqualObjects((id)has1, (id)has1b, @"has1 == has1b");
	STAssertEqualObjects((id)has2, (id)has2b, @"has2 == has2b");
	
}
								  
-(void)testRequiredAttributes 
{
	CFStringRef name = CFSTR("com.apple.security.unit-test.requiresStuffThings");
	CFErrorRef error;
	// In addition to testing required attributes, this also does a  partial "lifecycle" test, making sure we
	// pass through the stages, don't regress stages, and don't receive the wrong events in the wrong stages.
	typedef enum { S_INITED = 0, S_STARTED, S_RUN, S_EOS, S_GONE } state_t;
	
	__block state_t state = S_INITED;
	dispatch_group_t leave_on_finalize = dispatch_group_create();
	
	SecTransformCreateBlock required_attributes_create_block = ^(CFStringRef name, SecTransformRef new_transform, const SecTransformCreateBlockParameters *params) {
		params->overrideAttribute(kSecTransformActionAttributeNotification, kSecTransformInputAttributeName, ^(SecTransformAttributeRef attribute, CFTypeRef value) {
			// NOTE: this is for testing with a single data value, not a series.
			if (value)
			{
				STAssertTrue(state == S_STARTED, @"Init'ed for data (state=%d)", state);
				state = S_RUN;
			} else {
				STAssertTrue(state == S_RUN, @"In run state at EOS (state=%d)", state);
				state = S_EOS;
			}
			params->send(kSecTransformOutputAttributeName, kSecTransformMetaAttributeValue, value);
			return value;
		});
		
		params->send(CFSTR("Stuff"), kSecTransformMetaAttributeRequired, kCFBooleanTrue);
		params->send(CFSTR("Things"), kSecTransformMetaAttributeRequired, kCFBooleanTrue);
		
		params->overrideTransform(kSecTransformActionStartingExecution, ^{
			STAssertTrue(state == S_INITED, @"Inited (state=%d)");
			state = S_STARTED;
			return (CFTypeRef)NULL;
		});
		
		params->overrideTransform(kSecTransformActionFinalize, ^{
			state = S_GONE;
			dispatch_group_leave(leave_on_finalize);
			return (CFTypeRef)NULL;
		});
	};
	
	dispatch_group_enter(leave_on_finalize);
	SecTransformRef tr = custom_transform(name, required_attributes_create_block);
	STAssertNotNil((NSObject *)tr, @"newly created custom transform");
	
	char *data_bytes = (char *)"It was the best of transforms, it was the worst of transforms.";
	CFDataRef data = CFDataCreate(NULL, (const UInt8*)data_bytes, strlen(data_bytes));
	SecTransformSetAttribute(tr, kSecTransformInputAttributeName, data, NULL);
	usleep(100);
	STAssertTrue(state == S_INITED, @"not run yet");
	CFDataRef rdata = (CFDataRef)SecTransformExecute((NSObject *)tr, &error);
	
	STAssertTrue(rdata == NULL, @"Expected no result, but got: %@", rdata);
	STAssertErrorHas((id)error, @"missing required attributes?", @"Error describes condition (%@)", error);
	STAssertErrorHas((id)error, @" Things[ ,)]", @"Missing attributes named (%@)", error);
	STAssertErrorHas((id)error, @" Stuff[ ,)]", @"Missing attributes named (%@)", error);
	STAssertErrorHas((id)error, @"requiresStuffThings", @"Name of erroring Transform in message (%@)", error);
	
	if (error) {
		CFRelease(error);
	}
	CFRelease(tr);

	STAssertFalse(dispatch_group_wait(leave_on_finalize, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC)), @"group was ready");
	STAssertTrue(state == S_GONE, @"Transform should be gone, state=%d", state);
	
	dispatch_group_enter(leave_on_finalize);
	state = S_INITED;
	tr = custom_transform(name, required_attributes_create_block);
	STAssertNotNil((NSObject *)tr, @"newly created custom transform");
	
	error = NULL;
	SecTransformSetAttribute(tr, kSecTransformInputAttributeName, data, NULL);
	SecTransformSetAttribute(tr, CFSTR("Things"), CFSTR("grubby things"), NULL);
	SecTransformSetAttribute(tr, CFSTR("Stuff"), CFSTR("Cool stuff"), NULL);
	rdata = (CFDataRef)SecTransformExecute(tr,  &error);
	
	STAssertNotNil((NSData *)rdata, @"Got data back");
	STAssertEqualObjects((NSData *)rdata, (NSData *)data, @"Data unchanged");
	STAssertTrue(state == S_EOS, @"Transform hit EOS");
	STAssertTrue(error == NULL, @"Error not set");
	
	CFRelease(tr);
	STAssertFalse(dispatch_group_wait(leave_on_finalize, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC)), @"group was ready");
	STAssertTrue(state == S_GONE, @"Transform gone (state=%d)", state);
	dispatch_group_notify(leave_on_finalize, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0), ^{
		dispatch_release(leave_on_finalize);
	});
}

static SecTransformInstanceBlock AttributeNotificationTest(CFStringRef name, 
							SecTransformRef newTransform, 
							SecTransformImplementationRef ref)
{
	SecTransformInstanceBlock instanceBlock = 
	^{
		CFErrorRef result = NULL;
		
			
		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, NULL, 
			^(SecTransformStringOrAttributeRef ah, CFTypeRef value) 
			{
				SecTransformCustomSetAttribute(ref, CFSTR("Generic"), kSecTransformMetaAttributeValue, kCFBooleanTrue);
				return value;
			});

		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, CFSTR("Specific"), 
			^(SecTransformStringOrAttributeRef ah, CFTypeRef value) 
			{
				SecTransformCustomSetAttribute(ref, CFSTR("Specific"), kSecTransformMetaAttributeValue, kCFBooleanTrue);
				return value;
				
				});

		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, CFSTR("AlsoSpecific"), 
			^(SecTransformStringOrAttributeRef ah, CFTypeRef value) 
			{
				SecTransformCustomSetAttribute(ref, CFSTR("AlsoSpecific"), kSecTransformMetaAttributeValue, kCFBooleanTrue);
				
				return value;
			});

		return result;
	};
	
	return Block_copy(instanceBlock);
}

-(void)testAttributeNotifications
{
	NSString *name = @"com.apple.security.unit-test.testAttributeNotifications";
	Boolean generic_called = NO;
	Boolean specific_called = NO;
	Boolean also_specific_called = NO;
	
	Boolean ok = SecTransformRegister((CFStringRef)name, &AttributeNotificationTest, NULL);
	
	STAssertTrue(ok, @"Successful register");
	
	SecTransformRef tr = SecTransformCreate((CFStringRef)name, NULL);
	
	CFStringRef aNameStr = ((CFStringRef)name);
	SecTransformSetAttribute(tr, CFSTR("Generic"), aNameStr, NULL);
	SecTransformSetAttribute(tr, CFSTR("Specific"), aNameStr, NULL);
	SecTransformSetAttribute(tr, CFSTR("AlsoSpecific"), aNameStr, NULL);
	
	generic_called = (kCFBooleanTrue == (CFBooleanRef)SecTransformGetAttribute(tr, CFSTR("Generic")));
	specific_called = (kCFBooleanTrue == (CFBooleanRef)SecTransformGetAttribute(tr, CFSTR("Specific")));
	also_specific_called = (kCFBooleanTrue == (CFBooleanRef)SecTransformGetAttribute(tr, CFSTR("AlsoSpecific")));
	
	
	STAssertTrue(generic_called, @"generic called");
	STAssertTrue(specific_called, @"specific called");
	STAssertTrue(also_specific_called, @"also specific called");

	CFRelease(tr);
}

-(void)testEncryptAndDecryptTransforms
{
	NSAutoreleasePool *pool = [NSAutoreleasePool new];	
		
	// generate a symmetrical key for testing
	OSStatus err = errSecSuccess;
	
	NSString* algNames[] = 
	{
		@"AES 128",
		@"AES 192",
		@"AES 256",
		@"DES",
		@"3DES",
		@"CAST",
		@"RC4"
	};
	
	CSSM_ALGORITHMS symmetricalAlgos[] = 
	{
		CSSM_ALGID_AES,
		CSSM_ALGID_AES,
		CSSM_ALGID_AES,
		CSSM_ALGID_DES,
		CSSM_ALGID_3DES_3KEY_EDE,
		CSSM_ALGID_CAST,
		CSSM_ALGID_RC4
	};
	
	uint32 keySizes[] = 
	{
		128,
		192,
		256,
		64,
		192,
		40,
		8
	};
	
	CSSM_KEYUSE keyUse = CSSM_KEYUSE_ANY;
	CSSM_KEYATTR_FLAGS keyAttrFlags = CSSM_KEYATTR_RETURN_DEFAULT;
	SecAccessRef accessRef = NULL;
	CSSM_CC_HANDLE handle = ((CSSM_CC_HANDLE)0);
	
	NSString* dataStr = @"At the round earth's imagined corners blow\
	Your trumpets, angels, and arise, arise\
	From death, you numberless infinities\
	Of souls, and to your scattered bodies go,\
	All whom the flood did, and fire shall, overthrow,\
	All whom war, dearth, age, agues, tyrannies,\
	Despair, law, chance, hath slain, and you whose eyes\
	Shall behold God, and never taste death's woe.\
	But let them sleep, Lord, and me mourn a space,\
	For, if above all these my sins abound,\
	'Tis late to ask abundance of Thy grace,\
	When we are there. Here on this lowly ground\
	Teach me how to repent; for that's as good\
	As if Thou'dst sealed my pardon, with Thy blood.";
	
	NSData* testData = [dataStr dataUsingEncoding:NSUTF8StringEncoding];
	int numItems = (sizeof(symmetricalAlgos) / sizeof(CSSM_ALGORITHMS));
	int iCnt = 0;
	
	for (iCnt = 0; iCnt < numItems; iCnt++)
	{
		SecKeyRef testKey = NULL;
		CSSM_ALGORITHMS algoToUse = symmetricalAlgos[iCnt];
		uint32 keySizeInBits = keySizes[iCnt];
		
		err = SecKeyGenerate(NULL, algoToUse, keySizeInBits, handle, keyUse, keyAttrFlags, accessRef, &testKey);
		STAssertTrue(err == errSecSuccess, [NSString stringWithFormat:@"Unable to create a symmetrical key %@", algNames[iCnt]]);
		if (errSecSuccess != err)
		{
			continue;
		}
		__block CFErrorRef error = NULL;
		
		SecTransformRef encryptSymRef = NULL;
		encryptSymRef = SecEncryptTransformCreate(testKey, &error);
		if (NULL != error)
		{
			CFRelease(testKey);
			STAssertTrue(NO, [NSString stringWithFormat:@"Unable to create the encrypt transform for key %@", algNames[iCnt]]);
			continue;
		}
		
		SecTransformRef decryptSymRef = SecDecryptTransformCreate(testKey, &error);
		if (NULL != error)
		{
			CFRelease(testKey);
			CFRelease(encryptSymRef);
			STAssertTrue(NO, [NSString stringWithFormat:@"Unable to create the decrypt transform for key %@", algNames[iCnt]]);
			continue;
		}
		
		// connect the output of the encryption to the input of the decryption transform
		
		SecGroupTransformRef group = SecTransformCreateGroupTransform();
		(void)SecTransformConnectTransforms(encryptSymRef, kSecTransformOutputAttributeName,
						decryptSymRef, kSecTransformInputAttributeName, 
						group, &error);
		if (NULL != error)
		{
			CFRelease(testKey);
			CFRelease(encryptSymRef);
			CFRelease(decryptSymRef);
			STAssertTrue(NO, [NSString stringWithFormat:@"Unable to connect transforms for key %@", algNames[iCnt]]);
			continue;
		}
		
		
		NSInputStream* dataStream = [NSInputStream inputStreamWithData:testData];
		[dataStream open];
		
		SecTransformSetAttribute(encryptSymRef, kSecTransformInputAttributeName, (CFTypeRef)dataStream, &error);
		if (NULL != error)
		{
			CFRelease(testKey);
			CFRelease(encryptSymRef);
			CFRelease(decryptSymRef);
			STAssertTrue(NO, [NSString stringWithFormat:@"Unable to set the input for key %@", algNames[iCnt]]);
			continue;
		}
		
		CFTypeRef transformResult = SecTransformExecute(encryptSymRef, &error);
		CFRelease(group);
		CFRelease(encryptSymRef);
		CFRelease(decryptSymRef);
		
		if (NULL != error)
		{
			STAssertTrue(NO, [NSString stringWithFormat:@"returned an error for algo %@", algNames[iCnt]]);
			continue;
			CFRelease(error);
		}
		
		if (NULL == transformResult || 0 == [(NSData*)transformResult length])
		{
			STAssertTrue(NO, [NSString stringWithFormat:@"transformResult was NULL or empty for %@", algNames[iCnt]]);
			continue;
		}
		
		NSData* resultData = nil;
		if (CFGetTypeID(transformResult) == CFDataGetTypeID())
		{
			resultData = (NSData*)transformResult;			
			[resultData autorelease];
		}
		
		CFRelease(testKey);
				
		STAssertTrue([testData isEqualToData:resultData], @"The output of the decrypt transform does NOT match the original input!");		
	}
	
	
	SecKeyRef publicKey = NULL;
	SecKeyRef privateKey = NULL;
	
	keyAttrFlags = CSSM_KEYATTR_RETURN_REF;
	
	const uint32 publicKeyAttributes = CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_EXTRACTABLE | CSSM_KEYATTR_RETURN_REF;
	const uint32 privateKeyAttributes = CSSM_KEYATTR_SENSITIVE | CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_EXTRACTABLE;
	
	CSSM_KEYUSE pubKeyUse = CSSM_KEYUSE_VERIFY | CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_WRAP;
	CSSM_KEYUSE privKeyUse = CSSM_KEYUSE_SIGN | CSSM_KEYUSE_DECRYPT | CSSM_KEYUSE_UNWRAP;
	
	
	err = SecKeyCreatePair(NULL, CSSM_ALGID_RSA, 2048, ((CSSM_CC_HANDLE)0), 						   
						   pubKeyUse, publicKeyAttributes, 
						   privKeyUse, privateKeyAttributes,
						   NULL, &publicKey, &privateKey);
	
	STAssertTrue(errSecSuccess == err, @"Unable to create a key pair");
	if (errSecSuccess != err)
	{
		cssmPerror(NULL, err);
		return;
	}
	
	CFErrorRef error = NULL;
	SecTransformRef encryptSymRef = SecEncryptTransformCreate(publicKey , &error);
	if (NULL != error)
	{
		CFRelease(publicKey);
		CFRelease(privateKey);
		STAssertTrue(NO, [NSString stringWithFormat:@"Unable to create the encrypt transform for key RSA"]);
		return;
	}
	
	SecTransformRef decryptSymRef = SecDecryptTransformCreate(privateKey, &error);
	if (NULL != error)
	{
		CFRelease(publicKey);
		CFRelease(privateKey);
		CFRelease(encryptSymRef);
		STAssertTrue(NO, [NSString stringWithFormat:@"Unable to create the decrypt transform for key RSA"]);
		return;
	}
	
	// connect the output of the encryption to the input of the decryption transform
	
	SecGroupTransformRef group = SecTransformCreateGroupTransform();
	(void)SecTransformConnectTransforms( encryptSymRef, kSecTransformOutputAttributeName,
							decryptSymRef, kSecTransformInputAttributeName, 
							group, &error);
	if (NULL != error)
	{
		CFRelease(publicKey);
		CFRelease(privateKey);
		CFRelease(encryptSymRef);
		CFRelease(decryptSymRef);
		STAssertTrue(NO, [NSString stringWithFormat:@"Unable to connect transforms for key RSA"]);
		return;
	}
	
	NSInputStream* dataStream = [NSInputStream inputStreamWithData:testData];
	[dataStream open];
	
	SecTransformSetAttribute(encryptSymRef, kSecTransformInputAttributeName, (CFTypeRef)dataStream, &error);
	if (NULL != error)
	{
		CFRelease(publicKey);
		CFRelease(privateKey);
		CFRelease(encryptSymRef);
		CFRelease(decryptSymRef);
		STAssertTrue(NO, [NSString stringWithFormat:@"Unable to set the input for key RSA"]);
		return;	
	}
	CFTypeRef transformResult = SecTransformExecute(encryptSymRef, &error);
	if (NULL != error)
	{
		STAssertTrue(NO, [NSString stringWithFormat:@"returned an error for RSA"]);
		CFRelease(error);
		return;
	}
	
	if (NULL == transformResult || 0 == [(NSData*)transformResult length])
	{
		STAssertTrue(NO, [NSString stringWithFormat:@"transformResult was NULL or empty for RSA"]);
		return;
	}
	
	NSData* resultData = nil;
	if (CFGetTypeID(transformResult) == CFDataGetTypeID())
	{
		resultData = (NSData*)transformResult;		
		[resultData autorelease];
	}
	
	CFRelease(publicKey);
	CFRelease(privateKey);
	CFRelease(encryptSymRef);
	CFRelease(decryptSymRef);
		
	STAssertTrue([testData isEqualToData:resultData], @"(RSA)The output of the decrypt transform does NOT match the original input!");	
	
	[pool drain];
}

// NOTE: this test is largely the same as testEncryptAndDecryptTransforms, but
// we make a single key and use it from many threads at once.   This uncovered
// some locking issues, so makes a good regression test.
-(void)testMultiEncryptWithSameKey {
	// generate a symmetrical key for testing
	CSSM_KEYUSE keyUse = CSSM_KEYUSE_ANY;
	CSSM_KEYATTR_FLAGS keyAttrFlags = CSSM_KEYATTR_RETURN_DEFAULT;
	SecAccessRef accessRef = NULL;
	CSSM_CC_HANDLE handle = ((CSSM_CC_HANDLE)0);
	
	NSString* dataStr = @"Reduce, reuse, recycle.   No crashes please.";
	NSData* testData = [dataStr dataUsingEncoding:NSUTF8StringEncoding];

	SecKeyRef testKey = NULL;
	{
		OSStatus err;
		err = SecKeyGenerate(NULL, CSSM_ALGID_AES, 256, handle, keyUse, keyAttrFlags, accessRef, &testKey);
		STAssertTrue(err == errSecSuccess, @"Unable to create a symmetrical key err=%x", err);
	}
	
	// The number of iterations is somewhat arbitrary.   When we use to have failures they were
	// within 2*#logicalCPUs iterations, but nothing says we won't have a regression that happens
	// outside that window.
	dispatch_apply(128, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(size_t i) {
		__block CFErrorRef error = NULL;
		
		SecTransformRef encryptSymRef = NULL;
		encryptSymRef = SecEncryptTransformCreate(testKey, &error);
		if (NULL != error)
		{
			STFail(@"Unable to create the encrypt transform iteration#%d error=%@", i, error);
			return;
		}
		if (NULL == encryptSymRef) {
			STFail(@"Unable to create the encrypt transform iteration#%d, error=NULL", i);
			return;
		}
		
		SecTransformRef decryptSymRef = SecDecryptTransformCreate(testKey, &error);
		if (NULL != error)
		{
			CFRelease(encryptSymRef);
			STFail(@"Unable to create the decrypt transform iteration#%d error=%@", i, error);
			return;
		}
		if (NULL == decryptSymRef) {
			CFRelease(encryptSymRef);
			STFail(@"Unable to create the decrypt transform iteration#%d, error=NULL", i);
			return;
		}
		
		// connect the output of the encryption to the input of the decryption transform
		
		SecGroupTransformRef group = SecTransformCreateGroupTransform();
		(void)SecTransformConnectTransforms(encryptSymRef, kSecTransformOutputAttributeName,
											decryptSymRef, kSecTransformInputAttributeName, 
											group, &error);
		if (NULL != error)
		{
			CFRelease(encryptSymRef);
			CFRelease(decryptSymRef);
			STFail(@"Unable to connect transforms on iteration %d error=%@", i, error);
			return;
		}
		
		
		NSInputStream* dataStream = [NSInputStream inputStreamWithData:testData];
		[dataStream open];
		
		SecTransformSetAttribute(encryptSymRef, kSecTransformInputAttributeName, (CFTypeRef)dataStream, &error);
		if (NULL != error)
		{
			CFRelease(encryptSymRef);
			CFRelease(decryptSymRef);
			STFail(@"Unable to set the input on iteration %d error=%@", i, error);
			return;
		}
		
		CFTypeRef transformResult = SecTransformExecute(encryptSymRef, &error);
		CFRelease(group);
		
		if (NULL != error)
		{
			STFail(@"returned an error on iteration %d error=%@", i, error);
			CFRelease(error);
			return;
		}
		
		if (NULL == transformResult || 0 == [(NSData*)transformResult length])
		{
			STFail(@"transformResult was NULL or empty for iteration %d", i);
			return;
		}
		
		NSData* resultData = nil;
		if (CFGetTypeID(transformResult) == CFDataGetTypeID())
		{
			resultData = (NSData*)transformResult;			
			[resultData autorelease];
		}
		
		CFRelease(encryptSymRef);
		CFRelease(decryptSymRef);
		
		STAssertEqualObjects(testData, resultData, @"The output of the decrypt transform does NOT match the original input iteration %d", i);		
	});
	
	CFRelease(testKey);
}
								  
static SecTransformInstanceBlock RoundTripCheck(CFStringRef name, 
							SecTransformRef newTransform, 
							SecTransformImplementationRef ref)
{
	SecTransformInstanceBlock instanceBlock = 
	^{
		CFErrorRef result = NULL;
		__block CFDataRef remainder = NULL;
		__block SecTransformStringOrAttributeRef ahead = NULL;
		__block int eof_count = 0;
		__block bool drain = false;

		SecTransformCustomSetAttribute(ref, CFSTR("INPUT2"), kSecTransformMetaAttributeDeferred, kCFBooleanTrue);
		SecTransformCustomSetAttribute(ref, CFSTR("INPUT2"), kSecTransformMetaAttributeStream, kCFBooleanTrue);

		dispatch_block_t not_equal = 
		^{
			// not equal
			SecTransformCustomSetAttribute(ref, kSecTransformOutputAttributeName, kSecTransformMetaAttributeValue, kCFBooleanFalse);
			SecTransformCustomSetAttribute(ref, kSecTransformOutputAttributeName, kSecTransformMetaAttributeValue, NULL);
			drain = true;
		};

		SecTransformAttributeActionBlock action = 
		^(SecTransformAttributeRef ah, CFTypeRef value) 
		{
            if (drain) 
			{
				return (CFTypeRef)NULL;
			} 

			if (ahead == ah) 
			{
				SecTransformPushbackAttribute(ref, ah, value);
			} 
			else if (value) 
			{
				CFDataRef d = (CFDataRef)value;
				if (remainder) 
				{
					CFIndex compare_length;
					CFIndex remainder_length = CFDataGetLength(remainder);
					CFIndex d_length = CFDataGetLength(d);
					CFDataRef new_remainder = NULL;
					SecTransformAttributeRef new_ahead = NULL;

					if (remainder_length == d_length) 
					{
						compare_length = d_length;
					} 
					else if (remainder_length < d_length) 
					{
						new_remainder = CFDataCreate(NULL, CFDataGetBytePtr(d) + remainder_length, d_length - remainder_length);
						compare_length = remainder_length;
						new_ahead = ah;
					} else 
					{
						new_remainder = CFDataCreate(NULL, CFDataGetBytePtr(remainder) + d_length, remainder_length - d_length);
						compare_length = d_length;
						new_ahead = ahead;
					}

					if (bcmp(CFDataGetBytePtr(d), CFDataGetBytePtr(remainder), compare_length)) {
						not_equal();
					} else 
					{
						// same, keep going
						CFRelease(remainder);
						remainder = new_remainder;
						ahead = new_ahead;
					}
				} 
				else 
				{
					if (!eof_count) 
					{
						ahead = ah;
						remainder = CFDataCreateCopy(NULL, d);
					} 
					else 
					{
						if (CFDataGetLength(d)) 
						{
							not_equal();
						}
					}
				}
			} 
			else 
			{ // EOF case
				ahead = NULL;
				if (++eof_count == 2) 
				{
					if (remainder) 
					{
						SecTransformCustomSetAttribute(ref, kSecTransformOutputAttributeName, 
								kSecTransformMetaAttributeValue, 
								CFDataGetLength(remainder) ? kCFBooleanFalse : kCFBooleanTrue);
						
						CFRelease(remainder);
					} 
					else 
					{
						SecTransformCustomSetAttribute(ref, kSecTransformOutputAttributeName, 
									kSecTransformMetaAttributeValue, kCFBooleanTrue);
					}
					
					SecTransformCustomSetAttribute(ref, kSecTransformOutputAttributeName, 
									kSecTransformMetaAttributeValue, NULL);
				}
			}

			return (CFTypeRef)NULL;
		};

		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, CFSTR("INPUT2"), action);
		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, kSecTransformInputAttributeName, action);

		return result;
	};
	
	return Block_copy(instanceBlock);
}

BOOL RoundTrip(CFStringRef fname, SecTransformRef in, SecTransformRef out, BOOL share) 
{
	static dispatch_once_t once;
	CFStringRef name = CFSTR("com.apple.examples.cmp");
	// concepts: pushback, SecTransformSetAttributeAction vs. ProcessData, ah==, & send value to output

	dispatch_once(&once,
		^{
			SecTransformRegister(name, &RoundTripCheck, NULL);
		});
	
	SecTransformRef cmp = SecTransformCreate(name, NULL);
	SecTransformRef group = SecTransformCreateGroupTransform();
	CFErrorRef err = NULL;
	SecTransformConnectTransforms(in, kSecTransformOutputAttributeName, out, kSecTransformInputAttributeName, group, NULL);
	SecTransformConnectTransforms(out, kSecTransformOutputAttributeName, cmp, kSecTransformInputAttributeName, group, NULL);
	NSInputStream *is = [NSInputStream inputStreamWithFileAtPath:(NSString *)fname];
	// XXX: failure to do this seem to crash SecTransformExecute when it releases the error, track down & fix or file radar
	[is open];	
	
	NSInputStream *is2 = nil;
	
	if (share) 
	{
		SecTransformRef tee = SecNullTransformCreate();
		SecTransformConnectTransforms(tee, kSecTransformOutputAttributeName, in, kSecTransformInputAttributeName, group, NULL);
		SecTransformConnectTransforms(tee, kSecTransformOutputAttributeName, cmp, CFSTR("INPUT2"), group, NULL);
		SecTransformSetAttribute(tee, kSecTransformInputAttributeName, (CFTypeRef)is, NULL);
		CFRelease(tee);
	} else {
		is2 = [NSInputStream inputStreamWithFileAtPath:(NSString *)fname];
		[is2 open];	
		SecTransformSetAttribute(in, kSecTransformInputAttributeName, (CFTypeRef)is, &err);
		SecTransformSetAttribute(cmp, CFSTR("INPUT2"), (CFTypeRef)is2, &err);
	}
	
	assert(err == NULL);
	CFTypeRef r = SecTransformExecute(group, &err);
	
	if (err)
	{
		CFRelease(err);
	}
	
	CFRelease(group);
	CFRelease(cmp);
	
	if (is2)
	{
		[is2 close];
	}
	
	if (is)
	{
		[is close];
	}
	
	if (r) 
	{
		return r == kCFBooleanTrue;
	} 
	else 
	{
		CFfprintf(stderr, "round trip error: %@", err);
		return NO;
	}
}

static SecTransformInstanceBlock LineLengthCheck(CFStringRef name, SecTransformRef newTransform, SecTransformImplementationRef ref)
{
	SecTransformInstanceBlock instanceBlock = ^{
		CFErrorRef result = NULL;
		__block int bytesPastLastEOL = 0;
		__block int max = 0;
		
		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, CFSTR("MAX"), ^(SecTransformAttributeRef ah, CFTypeRef value) {
			CFNumberGetValue((CFNumberRef)value, kCFNumberIntType, &max);
			return value;
		});
		
		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, kSecTransformInputAttributeName, ^(SecTransformAttributeRef ah, CFTypeRef value) {
			if (NULL != value) {
				CFDataRef d = (CFDataRef)value;
				CFIndex len = CFDataGetLength(d);
				const UInt8 *bytes = CFDataGetBytePtr(d);
				
				for(int i = 0; i < len; i++) {
					if (bytes[i] == '\n') {
						bytesPastLastEOL = 0;
					} else {
						bytesPastLastEOL++;
						if (bytesPastLastEOL > max) {
							SecTransformCustomSetAttribute(ref, kSecTransformAbortAttributeName, kSecTransformMetaAttributeValue, CreateSecTransformErrorRef(kSecTransformErrorInvalidInput, "MAX line length of %d exceeded", max));
							break;
						}
					}
				}
			}
			SecTransformCustomSetAttribute(ref, kSecTransformOutputAttributeName, kSecTransformMetaAttributeValue, value);
			return value;
		});

		return result;
	};

	return Block_copy(instanceBlock);
}
								  
-(void)testLargeChunkEncode 
{
	NSError *err = NULL;
	NSData *d = [NSData dataWithContentsOfFile:@"/usr/share/dict/web2a" options:NSDataReadingMapped error: &err];
	STAssertNil(err, @"dataWithContentsOfFile %@", err);
	CFStringRef types[] = {kSecZLibEncoding, kSecBase64Encoding, kSecBase32Encoding, NULL};
	
	dispatch_group_t dg = dispatch_group_create();
	
	CFStringRef lengthCheckName = CFSTR("com.apple.security.unit-test.lineLengthCheck");
	SecTransformRegister(lengthCheckName, LineLengthCheck, (CFErrorRef *)&err);
	STAssertNil(err, @"Expected to register %@", lengthCheckName);
	
	for(int i = 0; types[i]; i++) {
		int max_j = 80;
		CFStringRef etype = types[i];

		void (^trial)(NSString *testName, id lineLength, int maxLineLength) = ^(NSString *testName, id lineLength, int maxLineLength) {
			SecGroupTransformRef group = SecTransformCreateGroupTransform();
			
			SecTransformRef et = SecEncodeTransformCreate(etype, (CFErrorRef *)&err);
			SecTransformRef dt = SecDecodeTransformCreate(etype, (CFErrorRef *)&err);

			SecTransformRef lineLengthChecker = (etype == kSecZLibEncoding) ? SecNullTransformCreate() : SecTransformCreate(lengthCheckName, NULL);
			STAssertNotNil((id)lineLengthChecker, @"Expected to create line length checker");
			SecTransformSetAttribute(lineLengthChecker, CFSTR("MAX"), [NSNumber numberWithInt:maxLineLength], NULL);
			
			SecTransformConnectTransforms(et, kSecTransformOutputAttributeName, lineLengthChecker, kSecTransformInputAttributeName, group, (CFErrorRef *)&err);
			SecTransformConnectTransforms(lineLengthChecker, kSecTransformOutputAttributeName, dt, kSecTransformInputAttributeName, group, (CFErrorRef *)&err);
			
			SecTransformSetAttribute(et, kSecTransformInputAttributeName, (CFDataRef)d, (CFErrorRef *)&err);
			SecTransformSetAttribute(et, kSecEncodeLineLengthAttribute, lineLength, (CFErrorRef *)&err);
			SecTransformSetAttribute(et, CFSTR("NAME"), (CFStringRef)testName, (CFErrorRef *)&err);
			
			dispatch_group_async(dg, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
				CFDataRef result = (CFDataRef)SecTransformExecute(group, (CFErrorRef *)&err);
				
				STAssertNil(err, @"execute for %@ got %@", testName, err);
				STAssertNotNil((id)result, @"No result from execute of %@", testName);
				
				if (result) {
					STAssertEqualObjects(d, (id)result, @"test %@ failed", testName);
				}
			});
		};

		for(int j = max_j; j > 70; --j) {
			if (etype == kSecZLibEncoding && j != max_j) {
				break;
			}
			trial([NSString stringWithFormat:@"%@-%d", etype, j], [NSNumber numberWithInt:j], j);
		}
		
		if (etype != kSecZLibEncoding) {
			trial([NSString stringWithFormat:@"%@-LL64", etype], (id)kSecLineLength64, 64);
			trial([NSString stringWithFormat:@"%@-LL76", etype], (id)kSecLineLength76, 76);
		}
	}
	
	dispatch_group_wait(dg, DISPATCH_TIME_FOREVER);
}

-(void)testZLib {
	SecTransformRef et = SecEncodeTransformCreate(kSecZLibEncoding, NULL);
	SecTransformRef dt = SecDecodeTransformCreate(kSecZLibEncoding, NULL);
	
	// using a tee would require >10 buffered items (we need to buffer about 64K), so we pass share=NO
	STAssertTrue(RoundTrip((CFStringRef)@"/usr/share/dict/web2a", et, dt, NO), @"Roundtrip /usr/share/dict/web2a");
	
	CFRelease(et);
	CFRelease(dt);
	
	/*
	If we want this we need a 'new' custom transform that will get receive the ratio data and be able to 
	query that data.
	
	CFNumberRef r1 = (CFNumberRef)SecTransformGetAttribute(et, kSecCompressionRatio);
	CFNumberRef r2 = (CFNumberRef)SecTransformGetAttribute(dt, kSecCompressionRatio);
	
	STAssertNotNil((NSNumber *)r1, @"encode ratio");
	STAssertNotNil((NSNumber *)r2, @"decode ratio");
	STAssertEqualObjects((NSNumber *)r1, (NSNumber *)r2, @"same ratios");
	*/
}

static SecTransformInstanceBlock CycleCheckTest(CFStringRef name, 
							SecTransformRef newTransform, 
							SecTransformImplementationRef ref)
{
	SecTransformInstanceBlock instanceBlock = 
	^{
		CFErrorRef result = NULL;
		int zero = 0;
		__block CFNumberRef feedback = CFNumberCreate(NULL, kCFNumberIntType, &zero);
		
		SecTransformCustomSetAttribute(ref, CFSTR("FEEDBACK"), kSecTransformMetaAttributeCanCycle, kCFBooleanTrue);
		
		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, CFSTR("INPUT"), 
			^(SecTransformAttributeRef ah, CFTypeRef value) 
			{
				if (value == NULL) {
					SecTransformCustomSetAttribute(ref, CFSTR("OUTPUT"), kSecTransformMetaAttributeValue, value);
					return value;
				}
				if (feedback == NULL) {
					SecTransformPushbackAttribute(ref, ah, value);
					return (CFTypeRef)NULL;
				}
			
				int x, y;
				CFNumberGetValue((CFNumberRef)value, kCFNumberIntType, &x);
				CFNumberGetValue(feedback, kCFNumberIntType, &y);
				x ^= y;
				CFNumberRef res = CFNumberCreate(NULL, kCFNumberIntType, &x);
				SecTransformCustomSetAttribute(ref, CFSTR("OUTPUT"), kSecTransformMetaAttributeValue, res);
				CFRelease(res);
				CFRelease(feedback);
				feedback = NULL;
				return (CFTypeRef)NULL;
			});
		
		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, CFSTR("FEEDBACK"), 
			^(SecTransformAttributeRef ah, CFTypeRef value) 
			{
				if (value) {
					if (feedback) {
						SecTransformPushbackAttribute(ref, ah, value);
					} else {
						feedback = (CFNumberRef)CFRetain(value);
					}
				}
			
				return (CFTypeRef)NULL;
			});
	
		return result;

	};
	
	return Block_copy(instanceBlock);
}

-(void)testCycleCheck {

	SecTransformRef cat = SecNullTransformCreate();
	SecTransformRef group = SecTransformCreateGroupTransform();
	CFErrorRef err = NULL;
	
	CFStringRef name = CFSTR("com.apple.examples.unit-test.loop-test");
	
	SecTransformRegister(name, &CycleCheckTest, NULL);
	
	SecTransformRef twenty = count_transform(20);
	
	// this is getting an internal error, but it's being ignored.
	SecTransformRef xxor = SecTransformCreate(name, &err);
	
	SecTransformConnectTransforms(xxor, CFSTR("OUTPUT"), cat, CFSTR("INPUT"), group, &err);
	STAssertNil((id)err, @"xor->cat");
	SecTransformConnectTransforms(xxor, CFSTR("OUTPUT"), xxor, CFSTR("FEEDBACK"), group, &err);
	STAssertNil((id)err, @"xor->xor");
	SecTransformConnectTransforms(twenty, CFSTR("OUTPUT"), xxor, CFSTR("INPUT"), group, &err);
	STAssertNil((id)err, @"twenty->xor");
	
	//SecTransformSetAttribute(xxor, CFSTR("DEBUG"), kCFBooleanTrue, &err);

	CFTypeRef r = SecTransformExecute(group, &err);
	STAssertNil((id)err, @"execute err=%@", err);
	STAssertNotNil((id)r, @"no results from execute");

	if (r) {
		CFNumberRef z = (CFNumberRef)[NSNumber numberWithInt:0];
		int n = CFArrayGetCountOfValue((CFArrayRef)r, CFRangeMake(0, CFArrayGetCount((CFArrayRef)r)), z);
		// There should be six zeros in the xor->feedback chain from 0 to 19.
		STAssertEquals(n, 6, @"There should be six zeros in %@", r);
	}

	CFRelease(r);
	CFRelease(group);
	CFRelease(twenty);
	CFRelease(xxor);
	CFRelease(cat);
}

-(void)testValidate {
	SecTransformRef group = SecTransformCreateGroupTransform();
	CFErrorRef err = NULL;
	
	CFStringRef data_or_null_name = CFSTR("com.apple.examples.unit-test.data-or-null");
	SecTransformCreateBlock data_or_null = ^(CFStringRef name, SecTransformRef new_transform, const SecTransformCreateBlockParameters *params) {
		params->overrideAttribute(kSecTransformActionAttributeValidation, CFSTR("INPUT"), SecTransformCreateValidatorForCFtype(CFDataGetTypeID(), YES));
	};
	
	
	SecTransformRef makes_numbers = count_transform(20);
	SecTransformRef wants_data = custom_transform(data_or_null_name, data_or_null);
	
	SecTransformConnectTransforms(makes_numbers, CFSTR("OUTPUT"), wants_data, CFSTR("INPUT"), group, NULL);
	STAssertNil((id)err, @"unexpected connect error: %@", err);
	CFTypeRef r = SecTransformExecute(group, &err);
	STAssertNil((id)r, @"Got non-null result (%@) when expecting null!", r);
	STAssertNotNil((id)err, @"Expected an error!", err);
	STAssertErrorHas((id)err, @"/INPUT", @"Error indicated attribute that was set incorrectly");
	STAssertErrorHas((id)err, @" type CFNumber", @"Error indicated provided type");
	STAssertErrorHas((id)err, @" a CFData", @"Error indicated required type");
	
	if (err) {
		CFRelease(err);
	}
	err = NULL;
	CFRelease(wants_data);
	
	wants_data = custom_transform(data_or_null_name, data_or_null);
	
	char raw_data[] = "`Twas brillig, and the slithy toves / Did gyre and gimble in the wabe: / All mimsy were the borogoves, / And the mome raths outgrabe.";
	CFDataRef the_data = CFDataCreate(NULL, (UInt8*)raw_data, strlen(raw_data));
	SecTransformSetAttribute(wants_data, kSecTransformInputAttributeName, the_data, &err);
	CFRelease(the_data);
	
	STAssertNil((id)err, @"unexpected set error: %@", err);
	r = SecTransformExecute(wants_data, &err);
	STAssertNotNil((id)r, @"Expected a result, got error: %@", err);
	if (r) {
		STAssertEqualObjects((id)the_data, (id)r, @"Invalid result");
	}
	
	CFStringRef numbers_only_name = CFSTR("com.apple.examples.unit-test.numbers-only");
	SecTransformCreateBlock numbers_only = ^(CFStringRef name, SecTransformRef new_transform, const SecTransformCreateBlockParameters *params) {
		params->overrideAttribute(kSecTransformActionAttributeValidation, CFSTR("INPUT"), SecTransformCreateValidatorForCFtype(CFNumberGetTypeID(), NO));
	};
	
	CFRelease(group);
	CFRelease(makes_numbers);
	CFRelease(wants_data);
	
	group = SecTransformCreateGroupTransform();
	makes_numbers = count_transform(20);
	SecTransformRef wants_numbers = custom_transform(numbers_only_name, numbers_only);
	
	SecTransformConnectTransforms(makes_numbers, CFSTR("OUTPUT"), wants_numbers, CFSTR("INPUT"), group, NULL);
	STAssertNil((id)err, @"unexpected connect error: %@", err);
	r = SecTransformExecute(group, &err);
	CFfprintf(stderr, "r=%@; err=%@\n", r, err);
	STAssertNil((id)r, @"Got non-null result (%@) when expecting null!", r);
	STAssertNotNil((id)err, @"Expected an error!", err);
	STAssertErrorHas((id)err, @"/INPUT", @"Error indicated attribute that was set incorrectly");
	STAssertErrorHas((id)err, @"received NULL value", @"Error indicated provided value is NULL");
	STAssertErrorHas((id)err, @" a CFNumber", @"Error indicated required type");
	
	CFRelease(err);
	CFRelease(group);
	CFRelease(makes_numbers);
	CFRelease(wants_numbers);
}

-(void)testCodeBase32 {
	struct base32_test_vector {
		const char *plain_text;
		const char *base32_rfc4648;
		const char *base32_fde;
	};
	
	// RFC 4648 test vectors
	static base32_test_vector base32_test_vectors[] = {
		{"", "", ""},
		{"f", "MY======", "MY======"},
		{"fo", "MZXQ====", "MZXQ===="},
		{"foo", "MZXW6===", "MZXW6==="},
		{"foob", "MZXW6YQ=", "MZXW6YQ="},
		{"fooba", "MZXW6YTB", "MZXW6YTB"},
		{"foobar", "MZXW6YTBOI======", "MZXW6YTBO8======"}};
	
	void (^test)(NSString *test_name, SecTransformRef transform, const char *input, const char *expected_output, NSString *error_format) =
	^(NSString *test_name, SecTransformRef transform, const char *input, const char *expected_output, NSString *error_format)
	{
		if (!transform) {
			STFail(@"No transform for %@", test_name);
			return;
		}
		
		CFErrorRef err = NULL;
		NSData *input_data = [NSData dataWithBytes:input length:strlen(input)];
		NSData *expected_output_data = [NSData dataWithBytes:expected_output length:strlen(expected_output)];
		SecTransformSetAttribute(transform, kSecTransformInputAttributeName, input_data, &err);
		STAssertNil((NSError *)err, @"unexpected error %@ from SecTransformSetAttribute for %@", err, test_name);
		NSData *output_data = (NSData *)SecTransformExecute(transform, &err);
		[output_data autorelease];
		STAssertNil((NSError *)err, @"Error from %@ execute (in=%s, err=%s)", test_name, input, err);
		STAssertNotNil(output_data, @"Unexpected nil output from %@ execute (in=%s)", test_name, input);
		if (output_data) {
			NSString *output_string = [NSString alloc];
			output_string = [output_string initWithBytes:[output_data bytes] length:[output_data length] encoding:NSMacOSRomanStringEncoding];
			[output_string autorelease];
			NSString *msg = [NSString stringWithFormat:error_format, input, expected_output, output_string];
			STAssertEqualObjects(expected_output_data, output_data, @"%@ %@", test_name, msg);					
		}
		CFRelease(transform);
	};
	
	for(int idx = 0; idx < sizeof(base32_test_vectors)/sizeof(*base32_test_vectors); idx++)
	{
		SecTransformRef base32encoder = SecEncodeTransformCreate(kSecBase32Encoding, NULL);
		test(@"base32 encode", base32encoder, base32_test_vectors[idx].plain_text, base32_test_vectors[idx].base32_rfc4648, @"B32(\"%1$s\") should be \"%2$s\", got \"%3$@\"");
		
		SecTransformRef base32decoder = SecDecodeTransformCreate(kSecBase32Encoding, NULL);
		test(@"base32 decode", base32decoder, base32_test_vectors[idx].base32_rfc4648, base32_test_vectors[idx].plain_text, @"B32dec(\"%1$s\") should be \"%2$s\", got \"%3$@\"");
		
		SecTransformRef base32FDEencoder = SecEncodeTransformCreate(CFSTR("base32FDE"), NULL);
		test(@"base32FDE encode", base32FDEencoder, base32_test_vectors[idx].plain_text, base32_test_vectors[idx].base32_fde, @"B32(\"%1$s\") should be \"%2$s\", got \"%3$@\"");
		
		SecTransformRef base32FDEdecoder = SecDecodeTransformCreate(CFSTR("base32FDE"), NULL);
		test(@"base32FDE decode", base32FDEdecoder, base32_test_vectors[idx].base32_fde, base32_test_vectors[idx].plain_text, @"B32dec(\"%1$s\") should be \"%2$s\", got \"%3$@\"");
	}
	
	SecTransformRef bet = SecEncodeTransformCreate(kSecBase32Encoding, NULL);
	STAssertNotNil((id)bet, @"got bulk base 32 encoder");
	SecTransformRef bdt = SecDecodeTransformCreate(kSecBase32Encoding, NULL);
	STAssertNotNil((id)bdt, @"got bulk base 32 decoder");
	STAssertTrue(RoundTrip((CFStringRef)@"/usr/share/dict/words", bet, bdt, YES), @"Roundtrip base32 /usr/share/dict/words");
	
	CFRelease(bet);
	CFRelease(bdt);
	
	// FDE uses a modified base32 alphabet, we want to test it here.
	SecTransformRef FDE_encode_transform = SecEncodeTransformCreate(@"base32FDE", NULL);
	STAssertNotNil((id)FDE_encode_transform, @"got FDE encoder");
	SecTransformRef FDE_decode_transform = SecDecodeTransformCreate(@"base32FDE", NULL);
	STAssertNotNil((id)FDE_decode_transform, @"got bulk base 32 decoder");
	STAssertTrue(RoundTrip((CFStringRef)@"/usr/share/dict/words", FDE_encode_transform, FDE_decode_transform, YES), @"Roundtrip base32FDE /usr/share/dict/words");
	
	CFRelease(FDE_encode_transform);
	CFRelease(FDE_decode_transform);
}

-(void)testCodeBase64 {
	CFErrorRef error = NULL;
	
#if 0
	SecTransformRef tr = SecDecodeTransformCreate(@"Not a real encoding", &error);
	// XXX: known failure in Transform::SetAttribute 7707822 -- I would fix on this branch, but I think that code has diverged
	STAssertTrue(tr == NULL, @"Checks for invalid encodings");
	NSLog(@"Error: %@", error);
#endif
	
	SecTransformRef dt = SecDecodeTransformCreate(kSecBase64Encoding, NULL);
	STAssertNotNil((id)dt, @"Got decoder");
	
	const char raw_data0[] = "Tm90IHV1ZW5jb2RlZAo=";
	const char raw_data1[] = "Not uuencoded\n";
	CFDataRef data0 = CFDataCreate(NULL, (const UInt8*)raw_data0, strlen(raw_data0));
	CFDataRef data1 = CFDataCreate(NULL, (const UInt8*)raw_data1, strlen(raw_data1));
	SecTransformSetAttribute(dt, kSecTransformInputAttributeName, data0, NULL);
	
	CFDataRef decoded_data = (CFDataRef)SecTransformExecute(dt, &error);
	STAssertNotNil((NSData *)decoded_data, @"Got a decode result");
	STAssertEqualObjects((NSData *)decoded_data, (NSData *)data1, @"Proper decode results");
	
	SecTransformRef et = SecEncodeTransformCreate(kSecBase64Encoding, NULL);
	STAssertNotNil((id)et, @"Got encoder");
	
	SecTransformSetAttribute(et, kSecTransformInputAttributeName, data1, NULL);

	CFDataRef encoded_data = (CFDataRef)SecTransformExecute(et, NULL);
	STAssertNotNil((NSData *)encoded_data, @"Got an encode result");
		
	STAssertEqualObjects((NSData *)encoded_data, (NSData *)data0, @"Proper encode results");
	
	// XXX also for general testing we want a "RandomChunkSizer" that copies INPUT to OUTPUT, but makes random size chunks (incl 0) as it goes.
	
	SecTransformRef dt2 = SecDecodeTransformCreate(kSecBase64Encoding, NULL);
	SecTransformRef et2 = SecEncodeTransformCreate(kSecBase64Encoding, NULL);
	int ll = 75;
	SecTransformSetAttribute(et2, kSecEncodeLineLengthAttribute, CFNumberCreate(NULL, kCFNumberIntType, &ll), NULL);

	STAssertTrue(RoundTrip((CFStringRef)@"/usr/share/dict/words", et2, dt2, YES), @"Roundtrip base64 /usr/share/dict/words");
	
	CFRelease(et2);
	CFRelease(dt2);
}

static SecTransformInstanceBlock ErrorResultsTest(CFStringRef name, 
							SecTransformRef newTransform, 
							SecTransformImplementationRef ref)
{
	SecTransformInstanceBlock instanceBlock = 
	^{
		CFErrorRef result = NULL;
		SecTransformSetDataAction(ref, kSecTransformActionProcessData, 
		^(CFTypeRef value) 
		{
			if (value != NULL)
			{
				return (CFTypeRef)CFErrorCreate(NULL, CFSTR("expected error"), 42, NULL);
			}
			else
			{
				return SecTransformNoData();
			}
		});

		return result;
	};
	
	return Block_copy(instanceBlock);	
}


-(void)testErrorResults {
	CFStringRef name = CFSTR("com.apple.security.unit-test.error-results");
	SecTransformRegister(name, &ErrorResultsTest, NULL);
	
	SecTransformRef tr = SecTransformCreate(name, NULL);
	CFDataRef data = CFDataCreate(NULL, NULL, 0);
	SecTransformSetAttribute(tr, kSecTransformInputAttributeName, data, NULL);
	
	CFErrorRef err = NULL;
	CFTypeRef no_result = SecTransformExecute(tr, &err);
	
	STAssertErrorHas((id)err, @"expected error", @"Signaled error has original string");
	STAssertErrorHas((id)err, @"42", @"Signaled error has original error code");
	STAssertNil((id)no_result, @"No result from erroring transform");
	CFRelease(data);
	CFRelease(tr);
	CFRelease(err);
}

-(void)testErrorExecutesInRightQueue {
	// testExecuteBlock checks to see if blocks are generally executed on the proper queue, this specifically checks
	// for an error while starting (which was originally improperly coded).
	
	SecTransformRef unassigned_input = SecNullTransformCreate();
	dispatch_queue_t q = dispatch_queue_create("com.apple.unit-test.ErrorExecutesInRightQueue", NULL);
	dispatch_group_t got_final = dispatch_group_create();
	dispatch_group_enter(got_final);
	__block bool saw_data = false;
	__block bool saw_error = false;
	
	SecTransformExecuteAsync(unassigned_input, q, ^(CFTypeRef message, CFErrorRef error, Boolean isFinal) {
		STAssertEquals(q, const_cast<const dispatch_queue_t>(dispatch_get_current_queue()), @"Should be running on %p, but is running on %p", q, dispatch_get_current_queue());
		saw_data = saw_data || (message != NULL);
		saw_error = saw_error || (error != NULL);
		if (isFinal) {
			dispatch_group_leave(got_final);
		}
	});
	
	STAssertFalse(dispatch_group_wait(got_final, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC)), @"Execute completed");
	STAssertFalse(saw_data, @"Should have seen no data (but did)");
	STAssertTrue(saw_error, @"Should have seen error (but didn't)");
	
	CFRelease(unassigned_input);
	dispatch_group_notify(got_final, q, ^{
		dispatch_release(got_final);
		dispatch_release(q);
	});
	
}

-(void)testSignVerify {
	unsigned char *raw_message = (unsigned char *)"Controlling complexity is the essence of computer programming. - Brian Kernigan";
	dispatch_group_t dg = dispatch_group_create();
	CFErrorRef err = NULL;
	CFDataRef message = CFDataCreate(NULL, raw_message, strlen((const char *)raw_message));
	__block SecKeyRef rsa_pub_key = NULL;
	__block SecKeyRef rsa_priv_key = NULL;
	__block SecKeyRef ecdsa_pub_key = NULL;
	__block SecKeyRef ecdsa_priv_key = NULL;
	__block SecKeyRef dsa_pub_key = NULL;
	__block SecKeyRef dsa_priv_key = NULL;
	
	char *tmp_dir;
	asprintf(&tmp_dir, "%s/sign-verify-test-keychain-", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
	
	unsigned char *raw_bad_message = (unsigned char *)"Standards are great, there are so many to choose from - Andrew S. Tanenbaum (maybe)";
	CFDataRef bad_message = CFDataCreate(NULL, raw_bad_message, strlen((const char *)raw_bad_message));

	// when safe replace with a concurrent queue
	dispatch_queue_t key_q = dispatch_queue_create(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
	
	dispatch_group_async(dg, key_q, 
	^{
		NSAutoreleasePool *pool = [NSAutoreleasePool new];
				
		// (note the key must be bigger then a SHA2-256 signature plus the DER.1 packing of the OID, so 1024 was chosen for that, not speed or safety)
		NSDictionary *key_opts = [NSDictionary dictionaryWithObjectsAndKeys:
								  (NSString *)kSecAttrKeyTypeRSA, (NSString *)kSecAttrKeyType,
								  [NSNumber numberWithInt:1024], (NSString *)kSecAttrKeySizeInBits,
								  @"RSA transform unit test key", (NSString *)kSecAttrLabel,
								  nil];
		OSStatus gp_status = SecKeyGeneratePair((CFDictionaryRef)key_opts, &rsa_pub_key, &rsa_priv_key);
		STAssertTrue(gp_status == 0, @"RSA (gp_status=0x%x)", gp_status);
		[pool drain];
	});

	dispatch_group_async(dg, key_q, 
	^{
		OSStatus gp_status;
#if 0
		// I don't know how "safe" a 512 bit ECDSA key is, but again this is just for testing, not for signing any real data
		NSDictionary *key_opts = [NSDictionary dictionaryWithObjectsAndKeys:
								  (NSString *)kSecAttrKeyTypeECDSA, (NSString *)kSecAttrKeyType,
								  [NSNumber numberWithInt:512], (NSString *)kSecAttrKeySizeInBits,
								  @"ECDSA transform unit test key", (NSString *)kSecAttrLabel,
								  nil];
		gp_status = SecKeyGeneratePair((CFDictionaryRef)key_opts, &ecdsa_pub_key, &ecdsa_priv_key);
#else
		{
			SecKeychainRef tmp_keychain = NULL;
			gp_status = SecKeyCreatePair(tmp_keychain, CSSM_ALGID_ECDSA, 256, NULL, 
				CSSM_KEYUSE_VERIFY, 
				CSSM_KEYATTR_EXTRACTABLE|CSSM_KEYATTR_PERMANENT, 
				CSSM_KEYUSE_SIGN, 
				CSSM_KEYATTR_EXTRACTABLE|CSSM_KEYATTR_PERMANENT, 
				NULL, &ecdsa_pub_key, &ecdsa_priv_key);
		}
#endif
		if (gp_status) 
		{
			STAssertTrue(gp_status == 0, @"ECDSA (gp_status=0x%x)", gp_status);
		}
	});

	dispatch_group_async(dg, key_q, 
	^{
		OSStatus gp_status;
#if 0
		// I don't know how "safe" a 1024 bit DSA key is, but again this is just for testing, not for signing any real data
		NSDictionary *key_opts = [NSDictionary dictionaryWithObjectsAndKeys:(NSString *)kSecAttrKeyTypeDSA, 
					(NSString *)kSecAttrKeyType, [NSNumber numberWithInt:512], (NSString *)kSecAttrKeySizeInBits, nil];
		gp_status = SecKeyGeneratePair((CFDictionaryRef)key_opts, &ecdsa_pub_key, &ecdsa_priv_key);
#else
		{
			const char *passwd = "this is not secret";
			SecKeychainRef tmp_keychain = NULL;
			char *kcfname;
			asprintf(&kcfname, "%s-DSA-XXXXXXXXXX", tmp_dir);
			// NOTE: "mktemp" isn't as safe as you might think...but this is test code and doesn't have to be, but
			// if you copy it elsewhere you may well need to rewrite it.  (use mkstemp)
			mktemp(kcfname);			
			gp_status = SecKeychainCreate(kcfname, strlen(passwd), passwd, NO, NULL, &tmp_keychain);
			STAssertTrue(gp_status == 0, @"SecKeychainCreate (gp_status=0x%x)", gp_status);
			gp_status = SecKeyCreatePair(tmp_keychain, CSSM_ALGID_DSA, 512, NULL, 
				CSSM_KEYUSE_VERIFY|CSSM_KEYUSE_ENCRYPT|CSSM_KEYUSE_WRAP, 
				CSSM_KEYATTR_EXTRACTABLE|CSSM_KEYATTR_PERMANENT|CSSM_KEYATTR_RETURN_REF, 
				CSSM_KEYUSE_SIGN|CSSM_KEYUSE_DECRYPT|CSSM_KEYUSE_UNWRAP, 
				CSSM_KEYATTR_EXTRACTABLE|CSSM_KEYATTR_PERMANENT|CSSM_KEYATTR_RETURN_REF, 
				NULL, &dsa_pub_key, &dsa_priv_key);
			free(kcfname);
		}
#endif
		STAssertTrue(gp_status == 0, @"DSA (gp_status=0x%x)", gp_status);
	});
	
	struct sv_test {
		NSString *name;
		SecKeyRef pub_key, priv_key;
		CFDataRef msg_sign, msg_verify;
		CFTypeRef dalgo_sign, dalgo_verify;
		int dlen_sign, dlen_verify;
		BOOL pass;
	};
	
	dispatch_group_wait(dg, DISPATCH_TIME_FOREVER);

	struct sv_test sv_tests[] = 
	{
		{@"Basic RSA", rsa_pub_key, rsa_priv_key, message, message, NULL, NULL, 0, 0, YES},
		{@"Basic RSA (tampered data)", rsa_pub_key, rsa_priv_key, message, bad_message, NULL, NULL, 0, 0, NO},
		{@"RSA, mismatched digest algos", rsa_pub_key, rsa_priv_key, message, message, kSecDigestSHA2, kSecDigestSHA1, 0, 0, NO},
		
		{@"RSA SHA1 MD5", rsa_pub_key, rsa_priv_key, message, message, kSecDigestSHA1, kSecDigestSHA1, 0, 0, YES},
		{@"RSA SHA1 MD5 (tampered data)", rsa_pub_key, rsa_priv_key, message, bad_message, kSecDigestSHA1, kSecDigestSHA1, 0, 0, NO},
		
		{@"RSA MD5", rsa_pub_key, rsa_priv_key, message, message, kSecDigestMD5, kSecDigestMD5, 0, 0, YES},
		{@"RSA MD5 (tampered data)", rsa_pub_key, rsa_priv_key, message, bad_message, kSecDigestMD5, kSecDigestMD5, 0, 0, NO},
		
		{@"RSA MD2", rsa_pub_key, rsa_priv_key, message, message, kSecDigestMD2, kSecDigestMD2, 0, 0, YES},
		{@"RSA MD2 (tampered data)", rsa_pub_key, rsa_priv_key, message, bad_message, kSecDigestMD2, kSecDigestMD2, 0, 0, NO},
		
		{@"RSA SHA2 512", rsa_pub_key, rsa_priv_key, message, message, kSecDigestSHA2, kSecDigestSHA2, 512, 512, YES},
		{@"RSA SHA2 512 (tampered data)", rsa_pub_key, rsa_priv_key, message, bad_message, kSecDigestSHA2, kSecDigestSHA2, 512, 512, NO},
		{@"RSA SHA2 512 vs. 384", rsa_pub_key, rsa_priv_key, message, message, kSecDigestSHA2, kSecDigestSHA2, 512, 384, NO},
		
		{@"RSA SHA2 384", rsa_pub_key, rsa_priv_key, message, message, kSecDigestSHA2, kSecDigestSHA2, 384, 384, YES},
		{@"RSA SHA2 384 (tampered data)", rsa_pub_key, rsa_priv_key, message, bad_message, kSecDigestSHA2, kSecDigestSHA2, 384, 384, NO},
		
		{@"RSA SHA2 256", rsa_pub_key, rsa_priv_key, message, message, kSecDigestSHA2, kSecDigestSHA2, 256, 256, YES},
		{@"RSA SHA2 256 (tampered data)", rsa_pub_key, rsa_priv_key, message, bad_message, kSecDigestSHA2, kSecDigestSHA2, 256, 256, NO},
		
		{@"RSA SHA2 224", rsa_pub_key, rsa_priv_key, message, message, kSecDigestSHA2, kSecDigestSHA2, 224, 224, YES},
		{@"RSA SHA2 224 (tampered data)", rsa_pub_key, rsa_priv_key, message, bad_message, kSecDigestSHA2, kSecDigestSHA2, 224, 224, NO},
		
		{@"RSA SHA2 0", rsa_pub_key, rsa_priv_key, message, message, kSecDigestSHA2, kSecDigestSHA2, 0, 0, YES},
		{@"RSA SHA2 0 (tampered data)", rsa_pub_key, rsa_priv_key, message, bad_message, kSecDigestSHA2, kSecDigestSHA2, 0, 0, NO},
		
		{@"Basic ECDSA", ecdsa_pub_key, ecdsa_priv_key, message, message, NULL, NULL, 0, 0, YES},
		{@"Basic ECDSA (tampered data)", ecdsa_pub_key, ecdsa_priv_key, message, bad_message, NULL, NULL, 0, 0, NO},
		{@"ECDSA (mismatched digest algos)", ecdsa_pub_key, ecdsa_priv_key, message, bad_message, kSecDigestSHA2, kSecDigestSHA1, 0, 0, NO},
		
		{@"ECDSA SHA1", ecdsa_pub_key, ecdsa_priv_key, message, message, kSecDigestSHA1, kSecDigestSHA1, 0, 0, YES},
		{@"ECDSA SHA1 (tampered data)", ecdsa_pub_key, ecdsa_priv_key, message, bad_message, kSecDigestSHA1, kSecDigestSHA1, 0, 0, NO},
		
		{@"ECDSA SHA2 224", ecdsa_pub_key, ecdsa_priv_key, message, message, kSecDigestSHA2, kSecDigestSHA2, 224, 224, YES},
		{@"ECDSA SHA2 224 (tampered data)", ecdsa_pub_key, ecdsa_priv_key, message, bad_message, kSecDigestSHA2, kSecDigestSHA2, 224, 224, NO},
		
		{@"ECDSA SHA2 256", ecdsa_pub_key, ecdsa_priv_key, message, message, kSecDigestSHA2, kSecDigestSHA2, 256, 256, YES},
		{@"ECDSA SHA2 256 (tampered data)", ecdsa_pub_key, ecdsa_priv_key, message, bad_message, kSecDigestSHA2, kSecDigestSHA2, 256, 256, NO},
		
		{@"ECDSA SHA2 384", ecdsa_pub_key, ecdsa_priv_key, message, message, kSecDigestSHA2, kSecDigestSHA2, 384, 384, YES},
		{@"ECDSA SHA2 384 (tampered data)", ecdsa_pub_key, ecdsa_priv_key, message, bad_message, kSecDigestSHA2, kSecDigestSHA2, 384, 384, NO},
		
		{@"ECDSA SHA2 512", ecdsa_pub_key, ecdsa_priv_key, message, message, kSecDigestSHA2, kSecDigestSHA2, 512, 512, YES},
		{@"ECDSA SHA2 512 (tampered data)", ecdsa_pub_key, ecdsa_priv_key, message, bad_message, kSecDigestSHA2, kSecDigestSHA2, 512, 512, NO},
		
		{@"ECDSA SHA2 0", ecdsa_pub_key, ecdsa_priv_key, message, message, kSecDigestSHA2, kSecDigestSHA2, 0, 0, YES},
		{@"ECDSA SHA2 0 (tampered data)", ecdsa_pub_key, ecdsa_priv_key, message, bad_message, kSecDigestSHA2, kSecDigestSHA2, 0, 0, NO},
		
		{@"Basic DSA", dsa_pub_key, dsa_priv_key, message, message, NULL, NULL, 0, 0, YES},
		{@"Basic DSA (tampered data)", dsa_pub_key, dsa_priv_key, message, bad_message, NULL, NULL, 0, 0, NO},
		// only SHA1 is supported, so no mismatched digest algo test is available
		
		{@"DSA SHA1", dsa_pub_key, dsa_priv_key, message, message, kSecDigestSHA1, kSecDigestSHA1, 0, 0, YES},
		{@"DSA SHA1 (tampered data)", dsa_pub_key, dsa_priv_key, message, bad_message, kSecDigestSHA1, kSecDigestSHA1, 0, 0, NO},
	};

	free(tmp_dir);

	int i;
	for(i = 0; i < sizeof(sv_tests)/sizeof(sv_test); i++) 
	{
		CFStringRef input_cases[] = {kSecInputIsPlainText, kSecInputIsDigest};
		//CFStringRef input_cases[] = {kSecInputIsPlainText, kSecInputIsDigest, kSecInputIsRaw};
		const int ilim = sizeof(input_cases)/sizeof(input_cases[0]);
		int ii = 0, ij = 0;
		for(; ii < ilim; ++ii) 
		{
			for(ij = 0; ij < ilim; ++ij) 
			{
				err = NULL;
				struct sv_test *tst = sv_tests + i;
				NSString *tname = [NSString stringWithFormat:@"%@ %@ %@", tst->name, input_cases[ii], input_cases[ij]];
				
				CFStringRef sign_input_is = input_cases[ii];
				CFStringRef verify_input_is = input_cases[ij];
				
				if (sign_input_is != kSecInputIsPlainText && tst->dalgo_sign == NULL) {
					continue;
				}
				if (verify_input_is != kSecInputIsPlainText && tst->dalgo_verify == NULL) {
					continue;
				}
				
				if ((sign_input_is == kSecInputIsRaw || verify_input_is == kSecInputIsRaw) && [tst->name rangeOfString:@"RSA"].location == NSNotFound) {
					// we can only synthesize these tests for RSA
					NSLog(@"No %@ test", tname);
					continue;
				}
				
				STAssertNotNil((id)tst->pub_key, @"Have pub_key for %@", tname);
				STAssertNotNil((id)tst->priv_key, @"Have priv_key for %@", tname);
				
				if (tst->pub_key == nil || tst->priv_key == nil) {
					continue;
				}
				
				SecTransformRef sign = SecSignTransformCreate(tst->priv_key, &err);
				STAssertNil((NSError *)err, @"creating sign for %@", tname);
				STAssertNotNil((id)sign, @"Creating sign for %@", tname);
				
				if (sign == NULL) {
					continue;
				}
				
				SecTransformRef verify = SecVerifyTransformCreate(tst->pub_key, NULL, &err);
				STAssertNotNil((id)verify, @"Creating verify for %@", tname);
				STAssertNil((NSError *)err, @"Creating verify for %@", tname);
				
				if (verify == NULL) {
					continue;
				}
				
				SecTransformRef sign_digest = NULL;
				SecTransformRef verify_digest = NULL;
				SecTransformRef sign2 = NULL;
				
				if (tst->dalgo_sign) 
				{
					SecTransformSetAttribute(sign, kSecDigestTypeAttribute, tst->dalgo_sign, &err);
					STAssertNil((NSError *)err, @"Setting sign's digest type for %@", tname);
					SecTransformSetAttribute(sign, kSecDigestLengthAttribute, [NSNumber numberWithInt:tst->dlen_sign], &err);
					STAssertNil((NSError *)err, @"Setting sign's digest length for %@", tname);
					
					if (sign_input_is == kSecInputIsDigest) 
					{
						sign_digest = SecDigestTransformCreate(tst->dalgo_sign, tst->dlen_sign, &err);
						STAssertNotNil((id)sign_digest, @"Create sign's %@-%d digest transform (for %@)", tst->dalgo_sign, tst->dlen_sign, tname);
						STAssertNil((NSError *)err, @"Making sign's digester (for %@) - err=%@", tname, err);
					
						SecTransformSetAttribute(sign, kSecInputIsAttributeName, sign_input_is, &err);
						STAssertNil((NSError *)err, @"Setting sign's InputIs (for %@) - err=%@", tname, err);
					}
				}
				
				if (tst->dalgo_verify) {
					SecTransformSetAttribute(verify, kSecDigestTypeAttribute, tst->dalgo_verify, &err);
					STAssertNil((NSError *)err, @"Setting verify's digest type for %@", tname);
					SecTransformSetAttribute(verify, kSecDigestLengthAttribute, [NSNumber numberWithInt:tst->dlen_verify], &err);
					STAssertNil((NSError *)err, @"Setting verify's digest length for %@", tname);
					
					if (verify_input_is == kSecInputIsDigest) {
						verify_digest = SecDigestTransformCreate(tst->dalgo_verify, tst->dlen_verify, &err);
						STAssertNotNil((id)verify_digest, @"Create verify's %@-%d digest transform (for %@)", tst->dalgo_verify, tst->dlen_verify, tname);
						STAssertNil((NSError *)err, @"Making verify's digester (for %@) - err=%@", tname, err);
						
						SecTransformSetAttribute(verify, kSecInputIsAttributeName, verify_input_is, &err);
						STAssertNil((NSError *)err, @"Setting verify's InputIs (for %@) - err=%@", tname, err);
					}
				}
				
				SecGroupTransformRef group = SecTransformCreateGroupTransform();
				SecTransformSetAttribute(sign_digest ? sign_digest : sign, kSecTransformInputAttributeName, tst->msg_sign, (CFErrorRef *)&err);
				if (sign_digest) {
					STAssertNil((NSError *)err, @"Setting sign's digest's input for %@", tname);
					SecTransformConnectTransforms(sign_digest, kSecTransformOutputAttributeName, 
									sign, kSecTransformInputAttributeName, group, NULL);
				} else {
					STAssertNil((NSError *)err, @"Setting sign's input for %@", tname);
				}
				
				
				SecTransformSetAttribute(verify_digest ? verify_digest : verify, kSecTransformInputAttributeName, tst->msg_verify, (CFErrorRef *)&err);
				if (verify_digest) {
					STAssertNil((NSError *)err, @"Setting verify's digest's input for %@", tname);
					SecTransformConnectTransforms(verify_digest, kSecTransformOutputAttributeName, 
							verify, kSecTransformInputAttributeName, group, NULL);
				} else {
					STAssertNil((NSError *)err, @"Setting verify's input for %@", tname);
				}
				
				SecTransformConnectTransforms(sign2 ? sign2 : sign, kSecTransformOutputAttributeName, verify, kSecSignatureAttributeName, group, NULL);
				
				dispatch_group_enter(dg);
				dispatch_queue_t temp_q = dispatch_queue_create(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
				SecTransformExecuteAsync(sign, temp_q, 
					^(CFTypeRef message, CFErrorRef error, Boolean isFinal) 
					{
						if (message) 
						{
							if (tst->pass) 
							{
								STAssertTrue(message == kCFBooleanTrue, @"Failed to verify proper signature %@; message = %@", tname, message);
							} else 
							{
								STAssertTrue(message == kCFBooleanFalse, @"Failed to detect tampering %@; message = %@", tname, message);
							}
						}
					
						STAssertNil((NSError *)err, @"Executed ok for %@ (err=%@)", tname, error);
					
						if (isFinal)
						{
							dispatch_group_leave(dg);
						}
					});

				CFRelease(sign);
				CFRelease(verify);
				CFRelease(group);
			}
		}
	}
	
	struct raw_test {
		SecKeyRef pub, priv;
		NSString *name;
	} raw_tests[] = {
		{rsa_pub_key, rsa_priv_key, @"RSA raw test"},
		{dsa_pub_key, dsa_priv_key, @"DSA raw test"},
		{ecdsa_pub_key, ecdsa_priv_key, @"ECDSA raw test"},
	};
	
	for(i = 0; i < sizeof(raw_tests)/sizeof(raw_tests[0]); ++i) {
		raw_test *t = raw_tests + i;
		SecTransformRef tee = SecNullTransformCreate();
		const char *raw_bytes = "some bytes";
		CFDataRef bytes = CFDataCreate(NULL, (UInt8*)raw_bytes, strlen(raw_bytes));
		CFErrorRef err = NULL;
		
		SecTransformRef sign = SecSignTransformCreate(t->priv, &err);
		STAssertNil((id)err, @"%@ test sign create err=%@", t->name, err);
		
		SecTransformRef verify = SecVerifyTransformCreate(t->pub, NULL, &err);
		STAssertNil((id)err, @"%@ test verify create err=%@", t->name, err);
		
		SecGroupTransformRef group = SecTransformCreateGroupTransform();
		SecTransformConnectTransforms(sign, kSecTransformOutputAttributeName, verify, kSecSignatureAttributeName, group, &err);
		SecTransformConnectTransforms(tee, kSecTransformOutputAttributeName, sign, kSecTransformInputAttributeName, group, &err);
		SecTransformConnectTransforms(tee, kSecTransformOutputAttributeName, verify, kSecTransformInputAttributeName, group, &err);
		SecTransformSetAttribute(tee, kSecTransformInputAttributeName, bytes, &err);
		STAssertNil((id)err, @"%@ setup error=%@", t->name, err);
		CFRetain(group);
		dispatch_group_async(dg, dispatch_queue_create(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
			CFErrorRef xerr = NULL;
			CFTypeRef result = SecTransformExecute(group, &xerr);
			CFRelease(group);
			
			if (result) {
				STAssertTrue(result == kCFBooleanTrue, @"%@ sign result=%@", t->name, result);
			} else {
				STFail(@"%@ no result", t->name);
			}
			STAssertNil((id)err, @"%@ execute error=%@", t->name, xerr);
		});
		CFRelease(group);
	}
	
	// Test some things we want to fail for:
	
	SecTransformRef tee = SecNullTransformCreate();
	SecTransformSetAttribute(tee, kSecTransformInputAttributeName, message, NULL);
	SecTransformRef vrfy = SecVerifyTransformCreate(ecdsa_pub_key, NULL, NULL);
	
	SecGroupTransformRef group = SecTransformCreateGroupTransform();
	SecTransformConnectTransforms(tee, kSecTransformOutputAttributeName, vrfy, kSecSignatureAttributeName, group, NULL);
	SecTransformSetAttribute(vrfy, kSecDigestTypeAttribute, CFSTR("No such type"), NULL);
	SecTransformSetAttribute(vrfy, kSecTransformInputAttributeName, message, NULL);
	err = NULL;
	CFTypeRef no_result = SecTransformExecute(group, (CFErrorRef*)&err);
	CFRelease(group);
	
	STAssertNil((id)no_result, @"No result from nonexistent digest");
	STAssertErrorHas((id)err, @"[Ii]nvalid digest algorithm", @"Error message describes nature of error (%@)", err);
	STAssertErrorHas((id)err, @"ECDSA signature", @"Error is not overly general (%@)", err);
	STAssertErrorHas((id)err, @"ECDSA signature", @"Error is not overly general (%@)", err);
	STAssertErrorHas((id)err, @"SHA1.*SHA2", @"Error describes valid algorithms (%@)", err);
	CFRelease(vrfy);
	
	// It would be awesome if we supported all the digests, and this test went away.
	vrfy = SecVerifyTransformCreate(dsa_pub_key, message, NULL);
	tee = SecNullTransformCreate();
	
	group = SecTransformCreateGroupTransform();
	SecTransformConnectTransforms(vrfy, kSecSignatureAttributeName, tee, kSecTransformOutputAttributeName, group, NULL);
	SecTransformConnectTransforms(tee, kSecTransformOutputAttributeName, vrfy, kSecTransformInputAttributeName, group, NULL);
	SecTransformSetAttribute(vrfy, kSecDigestTypeAttribute, kSecDigestSHA2, NULL);
	SecTransformSetAttribute(tee, kSecTransformInputAttributeName, message, NULL);
	err = NULL;
	no_result = SecTransformExecute(group, (CFErrorRef*)&err);
	CFRelease(group);
	
	STAssertNil((id)no_result, @"No result from invalid digest");
	STAssertErrorHas((id)err, @"[Ii]nvalid digest algorithm", @"Error message gives problem statement (%@)", err);
	STAssertErrorHas((id)err, @"[^A-Z]DSA signature", @"Error is not overly general (%@)", err);
	STAssertErrorHas((id)err, @"SHA1", @"Correct algorithm is named (%@)", err);
	
	dispatch_group_wait(dg, DISPATCH_TIME_FOREVER);
}

static BOOL keyWithBytes(CFDataRef keyData, SecKeyRef* key, CFTypeRef keyClass) {
	CFErrorRef errorRef=NULL;
	CFMutableDictionaryRef parameters;
	parameters = CFDictionaryCreateMutable(kCFAllocatorDefault, 10, NULL, NULL);
	
	/* 
	 kSecAttrKeyClass values:
	 kSecAttrKeyClassPublic
	 kSecAttrKeyClassPrivate
	 kSecAttrKeyClassSymmetric
	 */
	CFDictionaryAddValue(parameters, kSecAttrKeyClass, keyClass);
	CFDictionaryAddValue(parameters, kSecAttrIsPermanent, kCFBooleanFalse); /* also means we have raw bits */
	CFDictionaryAddValue(parameters, kSecAttrKeyType, kSecAttrKeyTypeRSA); /* also means we have raw bits */
	*key = SecKeyCreateFromData(parameters, keyData, &errorRef);
	CFRelease(parameters);
	return (key != NULL);
}

-(void)testVerifyWithKeyFromBytes  {
	static const uint8_t original_pubKeyData[] =
	{
		0x30, 0x48, 0x02, 0x41, 0x00, 0xd1, 0x4d, 0x1c, 0xe6, 0xbd, 0xd6, 0x8c, 0x4b, 0x77, 0x1e, 0x9f,
		0xbc, 0xe1, 0xf6, 0x96, 0xf2, 0x55, 0xa2, 0xdc, 0x28, 0x36, 0x39, 0xf4, 0xec, 0x5b, 0x85, 0x9b,
		0x3c, 0x7f, 0x98, 0xe0, 0xed, 0x49, 0xf5, 0x44, 0xb1, 0x87, 0xa8, 0xf6, 0x7f, 0x55, 0xc0, 0x39,
		0xf0, 0xe7, 0xcc, 0x9c, 0x84, 0xde, 0x7d, 0x9a, 0x87, 0x38, 0xf2, 0x4b, 0x11, 0x6f, 0x63, 0x90,
		0xfc, 0x72, 0x2c, 0x86, 0xa3, 0x02, 0x03, 0x01, 0x00, 0x01
	};
	
	// openssl genrsa -out /tmp/rsa512.pem
	// openssl rsa -inform PEM -in /tmp/rsa512.pem -outform DER -out /tmp/rsa512.der
	// hexdump -C /tmp/rsa512.der | cut -c10-58 | tr -s ' ' ' ' | sed -e 's/ /, 0x/g' -e 's/$/,/' | cut -c3-|pbcopy
	static const uint8_t pubKeyData[] = {
		0x30, 0x5c, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05,
		0x00, 0x03, 0x4b, 0x00, 0x30, 0x48, 0x02, 0x41, 0x00, 0xbf, 0xd5, 0xce, 0x43, 0x59, 0xd5, 0xf8,
		0x41, 0xb2, 0xe1, 0x16, 0x02, 0x2a, 0x16, 0xcb, 0xef, 0x49, 0xea, 0x98, 0x71, 0xf8, 0xfb, 0x94,
		0x23, 0x12, 0xf7, 0xbc, 0x80, 0xd0, 0x8b, 0xfd, 0x29, 0xb8, 0xfc, 0x2c, 0x3d, 0x13, 0x6f, 0x37,
		0xef, 0xa7, 0x1e, 0xf9, 0x4c, 0x3d, 0x38, 0x3a, 0x2f, 0x6b, 0xa8, 0x16, 0x00, 0x27, 0x5a, 0xbe,
		0x3d, 0x61, 0xdd, 0x18, 0x45, 0x22, 0xdb, 0x1a, 0xff, 0x02, 0x03, 0x01, 0x00, 0x01, 
	};
	static const uint8_t signatureData[] =
	{
		0xbc, 0x76, 0x2a, 0x50, 0x4e, 0x17, 0x0b, 0xa9, 0x31, 0x3b, 0xc5, 0xb0, 0x4d, 0x2a, 0x01, 0x9a,
		0xbb, 0x5e, 0x7b, 0x6e, 0x90, 0x2f, 0xaf, 0x3f, 0x40, 0xdb, 0xb0, 0xfc, 0x49, 0xcf, 0xbb, 0xb6,
		0x08, 0xf0, 0xbb, 0x04, 0x5f, 0x89, 0x0b, 0x10, 0x47, 0x06, 0x93, 0xb3, 0xb7, 0x0b, 0x4e, 0x17,
		0xe9, 0xb1, 0x55, 0x94, 0x63, 0x30, 0x0b, 0xa3, 0xb1, 0x28, 0xba, 0xe8, 0xef, 0xb4, 0xbd, 0xc5
	};
	
	const char *raw_data = "Data to verify";
	CFDataRef data = CFDataCreate(NULL, (UInt8*) raw_data, strlen(raw_data));

	SecKeyRef key;
	CFDataRef cfkeybytes;
	CFErrorRef error;
	cfkeybytes = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, pubKeyData, sizeof(pubKeyData),kCFAllocatorNull);
	
	if(keyWithBytes(cfkeybytes, &key, kSecAttrKeyClassPublic)){
		CFDataRef signature = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, signatureData, sizeof(signatureData),kCFAllocatorNull);
		SecTransformRef vt = SecVerifyTransformCreate(key,signature,&error);
		SecTransformSetAttribute(vt, kSecTransformDebugAttributeName, @"YES", NULL);
		SecTransformSetAttribute(vt, kSecTransformInputAttributeName, data, &error);
		CFBooleanRef signature_ok = (CFBooleanRef) SecTransformExecute(vt, &error);
		
		CFRelease(vt);
		CFRelease(key);
		CFRelease(signature);
		CFRelease(data);
		CFRelease(cfkeybytes);
		
		NSLog(@"STE result %@, err=%@", signature_ok, error);
		STAssertNil((id)error, @"Error from SecTransformExecute: %@", error);
	} else {
		STFail(@"Can't get SecKeyCreateFromData to work");
	}
}

-(void)testAESAndCastKeysFromBytes  {
	CFErrorRef err = NULL;
	struct tcase {
		const char *name;
		CFTypeRef key_type;
		NSData *key_data;
	};
	const char *aes_kbytes = "0123456789012345";
	const char *cast_kbytes = "01234567";

	struct tcase cases[] = {
		{"AES", kSecAttrKeyTypeAES, [NSData dataWithBytes:aes_kbytes length:strlen(aes_kbytes)]},
		{"CAST", kSecAttrKeyTypeCAST, [NSData dataWithBytes:cast_kbytes length:strlen(cast_kbytes)]},
	};
	
	int i;
	for(i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
		NSDictionary *parm = [NSDictionary dictionaryWithObjectsAndKeys:
							  (id)kSecAttrKeyClassSymmetric, kSecAttrKeyClass,
							  (id)cases[i].key_type, kSecAttrKeyType,
							  (id)kCFBooleanFalse, kSecAttrIsPermanent,
							  NULL];
		
		SecKeyRef k = SecKeyCreateFromData((CFDictionaryRef)parm, (CFDataRef)cases[i].key_data, (CFErrorRef *)&err);
		STAssertNotNil((id)k, @"%s SecKeyCreateFromData didn't", cases[i].name);
		STAssertNil((id)err, @"%s SecKeyCreateFromData err=%@", err);
		
		SecTransformRef et = SecEncryptTransformCreate(k, &err);
		STAssertNotNil((id)et, @"No %s EncryptTransform created", cases[i].name);
		STAssertNil((id)err, @"Error from %s SecEncryptTransformCreate err=%@", cases[i].name, err);
		
		SecTransformRef dt = SecDecryptTransformCreate(k, &err);
		STAssertNotNil((id)dt, @"No %s DecryptTransform created", cases[i].name);
		STAssertNil((id)err, @"Error from %s SecDecryptTransformCreate err=%@", cases[i].name, err);
		
		if (k) {
			BOOL rt_ok = RoundTrip(CFSTR("/usr/share/dict/propernames"), et, dt, YES);
			CFRelease(et);
			CFRelease(dt);
			STAssertTrue(rt_ok, @"%s's round trip", cases[i].name);
		}
	}
}

-(void)testDispatchAsumptions {
    // Failures here don't directly indicate we have a bug.  It would indicate that
    // either dispatch has one, or that we rely on something dispatch never promised
    // and has changed.
    
    dispatch_semaphore_t pre_sem = dispatch_semaphore_create(0);
    dispatch_semaphore_t post_sem = dispatch_semaphore_create(0);
    __block bool pre_wait_works = false;
    
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0.1 * NSEC_PER_SEC), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(void){
        STAssertTrue(0 == dispatch_semaphore_wait(pre_sem, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC)), @"semaphore signal prior to wait pre-wakes");
        pre_wait_works = true;
        dispatch_semaphore_signal(post_sem);
    });
    dispatch_semaphore_signal(pre_sem);
    STAssertTrue(0 == dispatch_semaphore_wait(post_sem, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC)), @"signal after wait wakes");
    STAssertTrue(pre_wait_works, @"pre-wait worked");
    
    
}

// Build a group containing 3 subgroups, G1 which has 2 encoders, G2 and G3 which have one
// decoder each.   Exports and attributes are hooked up so execution results in a CFData
// with the same contents as input_data.   "self" is used by the STAssert macros.
// The various transforms are assigned names: G1, G2, G3, E64, EZLIB, DZLIB, D64.
SecTransformRef build_nested_groups(id self, CFDataRef input_data) {
    SecGroupTransformRef outer = SecTransformCreateGroupTransform();
    SecGroupTransformRef g1 = SecTransformCreateGroupTransform();
    SecGroupTransformRef g2 = SecTransformCreateGroupTransform();
    SecGroupTransformRef g3 = SecTransformCreateGroupTransform();
    
    CFErrorRef err = NULL;
    
    SecTransformSetAttribute(outer, kSecTransformTransformName, CFSTR("OUTER"), &err);
    STAssertNil((id)err, @"Can't set outer's name: %@", err);
    SecTransformSetAttribute(g1, kSecTransformTransformName, CFSTR("G1"), &err);
    STAssertNil((id)err, @"Can't set g1's name: %@", err);
    SecTransformSetAttribute(g2, kSecTransformTransformName, CFSTR("G2"), &err);
    STAssertNil((id)err, @"Can't set g2's name: %@", err);
    SecTransformSetAttribute(g3, kSecTransformTransformName, CFSTR("G3"), &err);
    STAssertNil((id)err, @"Can't set g3's name: %@", err);
    
    SecTransformRef e64 = SecEncodeTransformCreate(kSecBase64Encoding, &err);
    STAssertNil((id)err, @"Expected err to be nil, got: %@", err);
    STAssertNotNil((id)e64, @"Could not make Encode64 transform");
    SecTransformSetAttribute(e64, kSecTransformTransformName, CFSTR("E64"), NULL);
    SecTransformRef ezlib = SecEncodeTransformCreate(kSecZLibEncoding, &err);
    STAssertNil((id)err, @"Expected err to be nil, got: %@", err);
    STAssertNotNil((id)ezlib, @"Could not make Encode ZLib transform");
    SecTransformSetAttribute(ezlib, kSecTransformTransformName, CFSTR("EZLIB"), NULL);
    
    SecTransformConnectTransforms(e64, kSecTransformOutputAttributeName, ezlib, kSecTransformInputAttributeName, g1, &err);
    STAssertNil((id)err, @"Can't connect e64 to ezlib: %@", err);
    SecTransformConnectTransforms(g1, kSecTransformInputAttributeName, e64, kSecTransformInputAttributeName, g1, &err);
    STAssertNil((id)err, @"Can't connect g1's input to e64's input: %@", err);
    SecTransformConnectTransforms(ezlib, kSecTransformOutputAttributeName, g1, kSecTransformOutputAttributeName, g1, &err);
    STAssertNil((id)err, @"Can't connect ezlib's output to g1's output: %@", err);
    
    SecTransformRef dzlib = SecDecodeTransformCreate(kSecZLibEncoding, &err);
    STAssertNil((id)err, @"Expected err to be nil, got: %@", err);
    STAssertNotNil((id)dzlib, @"Could not make Decode ZLib transform");
    SecTransformSetAttribute(dzlib, kSecTransformTransformName, CFSTR("dzlib"), NULL);
    SecTransformRef d64 = SecDecodeTransformCreate(kSecBase64Encoding, &err);
    STAssertNil((id)err, @"Expected err to be nil, got: %@", err);
    STAssertNotNil((id)d64, @"Could not make Decode64 transform");
    SecTransformSetAttribute(dzlib, kSecTransformTransformName, CFSTR("D64"), NULL);
    
    // putting just one transform in g2 and g3
    SecTransformConnectTransforms(g2, kSecTransformInputAttributeName, dzlib, kSecTransformInputAttributeName, g2, &err);
    STAssertNil((id)err, @"Can't connect g2's input to dzlib's input: %@", err);
    SecTransformConnectTransforms(dzlib, kSecTransformOutputAttributeName, g2, kSecTransformOutputAttributeName, g2, &err);
    STAssertNil((id)err, @"Can't connect dzlib's output to g2's output: %@", err);
    
    SecTransformConnectTransforms(g3, kSecTransformInputAttributeName, d64, kSecTransformInputAttributeName, g3, &err);
    STAssertNil((id)err, @"Can't connect g2's input to d64's input: %@", err);
    SecTransformConnectTransforms(d64, kSecTransformOutputAttributeName, g3, kSecTransformOutputAttributeName, g3, &err);
    STAssertNil((id)err, @"Can't connect d64's output to g2's output: %@", err);
    
    SecTransformConnectTransforms(g1, kSecTransformOutputAttributeName, g2, kSecTransformInputAttributeName, outer, &err);
    STAssertNil((id)err, @"Can't connect g1 to g2 (dzlib): %@", err);
    SecTransformConnectTransforms(g2, kSecTransformOutputAttributeName, g3, kSecTransformInputAttributeName, outer, &err);
    STAssertNil((id)err, @"Can't connect g2 (dzlib) to g3 (d64): %@", err);
    
    SecTransformSetAttribute(g1, kSecTransformInputAttributeName, input_data, &err);
    STAssertNil((id)err, @"Can't set g1's input: %@", err);
    
    
    CFRelease(e64);
    CFRelease(ezlib);
    CFRelease(dzlib);
    CFRelease(d64);
    CFRelease(g1);
    CFRelease(g2);
    CFRelease(g3);
    return outer;
}

-(void)testGroupsInGroups {
    UInt8 original_bytes[] = "'Twas brillig and the...was that smiley toads?   Something with chives?  Aw heck!";
    CFDataRef original = CFDataCreate(NULL, original_bytes, sizeof(original_bytes));

    // Test executing the top group, a sub group, and a non-group member.
    for (NSString *name in [NSArray arrayWithObjects:@"OUTER", @"G1", @"D64", nil]) {
        CFErrorRef err = NULL;
        SecGroupTransformRef outer = build_nested_groups(self, original);
        SecTransformRef start_at = SecTransformFindByName(outer, (CFStringRef)name);
        STAssertNotNil((id)start_at, @"Expected to find %@", name);
        
        CFDataRef output = (CFDataRef)SecTransformExecute(start_at, &err);
        STAssertNil((id)err, @"Can't execute directly created nested transform starting at %@: %@", start_at, err);
        STAssertEqualObjects((id)output, (id)original, @"Output and original should match (started at %@)", start_at);
        CFRelease(outer);
        if (err) {
            CFRelease(err);
        }
    }
    
    {
        SecGroupTransformRef bad_outer = build_nested_groups(self, original);
        SecTransformRef d64 = SecTransformFindByName(bad_outer, CFSTR("D64"));
        STAssertNotNil((id)d64, @"Expected to find d64");
        CFErrorRef err = NULL;
        // d64 is in a group in bad_outer, we set things up to fail
        // and later expect execute to fail because of it.
        SecTransformSetAttribute(d64, kSecDecodeTypeAttribute, CFSTR("NOT valid"), &err);
        if (err) {
            // It can fail right away
            ErrorHas((NSError*)err, @"Unsupported decode type");
        } else {
            // Or later (see below)
            STAssertNil((id)err, @"Expected to set decode type: %@", err);
        }
        
        SecTransformRef e64 = SecTransformFindByName(bad_outer, CFSTR("E64"));
        STAssertNotNil((id)e64, @"Expected to find e64");
        CFStringRef any = CFSTR("ANY");
        // e64 and d64 aren't in the same groups, but they are in outer.
        // There should be no way to (directly) connect them, so try all
        // 4 groups and make sure none work.
        for (NSString *group_name in [NSArray arrayWithObjects:@"OUTER", @"G1", @"G2", @"G3", nil]) {
            SecTransformRef connect_in = SecTransformFindByName(bad_outer, (CFStringRef)group_name);
            STAssertNotNil((id)connect_in, @"Expected to find %@", group_name);
            err = NULL;
            SecTransformConnectTransforms(d64, any, e64, any, bad_outer, &err);
            STAssertNotNil((id)err, @"Expected error on cross group connect (in %@)", group_name);
            if (err) {
                STAssertEquals(CFErrorGetCode(err), (CFIndex)kSecTransformErrorInvalidConnection, @"error code (in %@)", group_name);
                STAssertEqualObjects((id)CFErrorGetDomain(err), (id)kSecTransformErrorDomain, @"error domain (in %@)", group_name);
                CFRelease(err);
                err = NULL;
            }
            
            // While we are here, make sure we can't set a non-exported group attribute
            SecTransformSetAttribute((SecTransformRef)connect_in, CFSTR("nobody-exports-me"), CFSTR("VALUE"), &err);
            STAssertNotNil((id)err, @"Expected an error setting a non-exported attribute on %@", connect_in);
            // Make sure this is the error we expect, not something unrelated to our transgression
            ErrorHas((NSError*)err, @"non-exported attribute");
            // Error should have the name of the offending attribute
            ErrorHas((NSError*)err, @"nobody-exports-me");
            if (err) {
                CFRelease(err);
                err = NULL;
            }
        }
        
        CFTypeRef no_result = SecTransformExecute(bad_outer, &err);
        STAssertNotNil((id)err, @"Expected error");
        ErrorHas((NSError*)err, @"Unsupported decode type");
        STAssertNil((id)no_result, @"Expected no result, got: %@", no_result);
        CFRelease(bad_outer);
        
        // Make sure we can't connect to or from non-exported group attributes
        bad_outer = build_nested_groups(self, original);
        STAssertNotNil((id)bad_outer, @"Expected to build nested transform");
        SecTransformRef g1 = SecTransformFindByName(bad_outer, CFSTR("G1"));
        STAssertNotNil((id)g1, @"Expected to find g1");
        SecTransformRef appendix = SecNullTransformCreate();
        SecTransformConnectTransforms(appendix, kSecTransformOutputAttributeName, g1, CFSTR("NONE"), bad_outer, &err);
        STAssertNotNil((id)err, @"Expected to fail connecting appendix to g1, but didn't");
        ErrorHas((NSError*)err, @"non-exported attribute");
        if (err) {
            CFRelease(err);
            err = NULL;
        }
        SecTransformConnectTransforms(g1, CFSTR("DOES_NOT_EXIST"), appendix, kSecTransformInputAttributeName, bad_outer, &err);
        STAssertNotNil((id)err, @"Expected to fail connecting g1 to appendix, but didn't");
        ErrorHas((NSError*)err, @"non-exported attribute");
        if (err) {
            CFRelease(err);
            err = NULL;
        }
        
        CFRelease(bad_outer);
        CFRelease(appendix);
    }
}

// 10080968 covers this case.   It isn't a regression (it was impossible to create nested groups
// until recently), but it needs to be addressed before we ship.
-(void)disabledUntilPR_10080968_testExternalizeGroupsInGroups {
    CFErrorRef err = NULL;
    UInt8 original_bytes[] = "Sic Semper Tyrannosaurus!";
    CFDataRef original = CFDataCreate(NULL, original_bytes, sizeof(original_bytes));
    
    SecGroupTransformRef outer = build_nested_groups(self, original);
    NSLog(@"outer=%@", SecTransformDotForDebugging(outer));
    SecTransformRef d64 = SecTransformFindByName(outer, CFSTR("D64"));
    STAssertNotNil((id)d64, @"Expected to find d64");
    
    CFDictionaryRef freezeDriedNestedGroups = SecTransformCopyExternalRepresentation(d64);
    STAssertNotNil((id)freezeDriedNestedGroups, @"Expected to externalize group");
    
    SecTransformRef outer2 = SecTransformCreateFromExternalRepresentation(freezeDriedNestedGroups, &err);
    STAssertNil((id)err, @"Can't create nested group err: %@", err);
    STAssertNotNil((id)outer2, @"Expected transform fron xrep: %@", freezeDriedNestedGroups);
    NSLog(@"outer2=%@", SecTransformDotForDebugging(outer2));
    
    CFTypeRef output2 = SecTransformExecute(outer2, &err);
    STAssertNil((id)err, @"Can't execute outer2: %@", err);
    STAssertEqualObjects((id)output2, (id)original, @"Output2 and original should match");
}

NSString *CopyLeakLine()
{
    static char os_build[16];
    static dispatch_once_t get_os_build_once;
    static BOOL broken_leaks_command = NO;
    
    dispatch_once(&get_os_build_once, ^{
        int mib[] = { CTL_KERN, KERN_OSVERSION };
        size_t bufsz = sizeof(os_build);
        sysctl(mib, 2, os_build, &bufsz, NULL, 0);
        
        if (4 == sizeof(char*) && 0 == strcmp(os_build, "12A75")) {
            // 12A75's leaks command was badly broken for 32 bit.
            // Running it suspends otest, and it is too hard to
            // recover.
            broken_leaks_command = YES;
        }
    });

    if (broken_leaks_command) {
        return [NSString stringWithFormat:@"Leaks command is broken in %s", os_build];
    }
    
    NSRegularExpression *matchLeaksLine = [NSRegularExpression regularExpressionWithPattern:@"^Process \\d+: \\d+ leaks for \\d+ total leaked bytes.$" options:NSRegularExpressionAnchorsMatchLines error:NULL];
    
	char *leak_command = NULL;
	NSString *fname = [NSString stringWithFormat:@"/tmp/L%d-%d", getpid(), (int)arc4random()];
	asprintf(&leak_command, "(/usr/bin/leaks %d >%s || (echo OOPS; kill -CONT %d))", getpid(), [fname UTF8String], getpid());
	system(leak_command);
	free(leak_command);
	NSString *output = [NSString stringWithContentsOfFile:fname encoding:NSUTF8StringEncoding error:NULL];
	NSTextCheckingResult *result = [matchLeaksLine firstMatchInString:output options:0 range:NSMakeRange(0, [output length])];
	if (result.range.location == NSNotFound) {
		return NULL;
	}
	NSRange matchRange = result.range;
	return [output substringWithRange:matchRange];
}
								  
-(void)testAAASimpleLeakTest {
	NSString *starting_leaks = CopyLeakLine();
	STAssertNotNil(starting_leaks, @"Found initial leaks");
	for(int i = 0; i < 10; i++) {
		CFRelease(SecTransformCreateGroupTransform());
	}
	
	NSString *current_leaks = NULL;
	
	// Some of the destruction is async, so if they don't pan out the same, a little sleep and retry
	// can legitimately fix it.
	for(int i = 0; i < 10; i++) {
		current_leaks = CopyLeakLine();
		if ([current_leaks isEqualToString:starting_leaks]) {
			break;
		} else {
			sleep(1);
		}
	}
	
	STAssertNotNil(current_leaks, @"Found current leaks");
	STAssertEqualObjects(current_leaks, starting_leaks, @"Expected no new leaks");
}

-(void)testAAASimpleishLeakTest {
    NSLog(@"pid=%d", getpid());
	NSString *starting_leaks = CopyLeakLine();
	STAssertNotNil(starting_leaks, @"Found initial leaks");
    CFErrorRef err = NULL;
    
    // Derived from Matt Wright's 10242560 test.c
    int fd = open("/dev/random", O_RDONLY);
    SecTransformRef b64encode = SecEncodeTransformCreate(kSecBase64Encoding, NULL);
    const int buffer_size = 1024;
    void *buffer = malloc(buffer_size);
    // For this test, ignore short reads
    read(fd, buffer, buffer_size);
    CFDataRef data = CFDataCreateWithBytesNoCopy(NULL, (UInt8*)buffer, buffer_size, kCFAllocatorMalloc);
    SecTransformSetAttribute(b64encode, kSecTransformInputAttributeName, data, &err);
    STAssertNil((id)err, @"Expected no SecTransformSetAttribute error, got: %@", err);
    CFRelease(data);
    CFTypeRef output = SecTransformExecute(b64encode, &err);
    STAssertNotNil((id)output, @"Expected result");
    STAssertNil((id)err, @"Expected no execute error, got: %@", err);
    CFRelease(output);
    CFRelease(b64encode);
    
	NSString *current_leaks = NULL;
	
	// Some of the destruction is async, so if they don't pan out the same, a little sleep and retry
	// can legitimately fix it.
	for(int i = 0; i < 10; i++) {
		current_leaks = CopyLeakLine();
		if ([current_leaks isEqualToString:starting_leaks]) {
			break;
		} else {
			sleep(1);
		}
	}
	
	STAssertNotNil(current_leaks, @"Found current leaks");
	STAssertEqualObjects(current_leaks, starting_leaks, @"Expected no new leaks");
}

@end
