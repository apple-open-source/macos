/*-
 * Copyright (c) 2013 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#import <AssertMacros.h>
#import <Security/Security.h>
#import <Security/SecRandom.h>
#import <CommonCrypto/CommonCryptor.h>
#import <CommonCrypto/CommonCryptorSPI.h>
#import "HeimCredCoder.h"
#import "common.h"

@implementation HeimCredDecoder

static uuid_t boolean_true =  { 0xED, 0x43, 0xE0, 0xE6, 0x20, 0x15, 0x40, 0x43, 0xA9, 0x0C, 0xE7, 0x30, 0x6C, 0x9D, 0xED, 0x6B };
static uuid_t boolean_false = {  0x48, 0xD3, 0x95, 0xCD, 0x37, 0xC7, 0x41, 0x76, 0xA0, 0x97, 0x9B, 0x44, 0x9E, 0x2C, 0x10, 0x75 };


+ (CFTypeRef)copyNS2CF:(id)ns
{
    if (!ns)
	return NULL;
    
    if ([ns isKindOfClass:[NSString class]] || [ns isKindOfClass:[NSData class]] || [ns isKindOfClass:[NSNumber class]]) {
	return (CFTypeRef)[ns copy];
    }
    
    if ([ns isKindOfClass:[NSUUID class]]) {
        union {
	    CFUUIDBytes bytes;
	    uuid_t uuid;
	} u;
	
	[ns getUUIDBytes:u.uuid];
	
	if (uuid_compare(u.uuid, boolean_false) == 0)
	    return kCFBooleanFalse;
	else if (uuid_compare(u.uuid, boolean_true) == 0)
	    return kCFBooleanTrue;
	
	return CFUUIDCreateFromUUIDBytes(NULL, u.bytes);
    }
    
    if ([ns isKindOfClass:[NSArray class]]) {
	CFIndex n, len = [ns count];
	CFMutableArrayRef array = CFArrayCreateMutable(NULL, len, &kCFTypeArrayCallBacks);
	if (!array) return NULL;
	
	for (n = 0; n < len; n++) {
	    CFTypeRef obj = [HeimCredDecoder copyNS2CF:[ns objectAtIndex:n]];
	    if (obj) CFArrayAppendValue(array, obj);
	    CFRELEASE_NULL(obj);
	}
        return array;
    }
    if ([ns isKindOfClass:[NSDictionary class]]) {
        CFIndex len = [ns count];
	
	CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, len, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (!dict) return NULL;
	
	[ns enumerateKeysAndObjectsUsingBlock:^void(id key, id value, BOOL *stop) {
	    CFTypeRef cfkey = [HeimCredDecoder copyNS2CF:key];
	    CFTypeRef cfvalue = [HeimCredDecoder copyNS2CF:value];
	    if (cfkey && cfvalue)
		CFDictionarySetValue(dict, cfkey, cfvalue);
	    CFRELEASE_NULL(cfkey);
	    CFRELEASE_NULL(cfvalue);
	}];
	
	return dict;
    }
    return NULL;
}

static void
convertDict(const void *key, const void *value, void *context)
{
    NSMutableDictionary *dict = context;
    id nskey = [HeimCredDecoder copyCF2NS:key];
    id nsvalue = [HeimCredDecoder copyCF2NS:value];
    if (nskey && nsvalue)
	[dict setObject:nsvalue forKey:nskey];
    [nskey release];
    [nsvalue release];
}

+ (id)copyCF2NS:(CFTypeRef)cf
{
    if (!cf)
	return nil;
    
    CFTypeID type = CFGetTypeID(cf);
    
    if (type == CFBooleanGetTypeID()) {
	if (CFBooleanGetValue(cf))
	    return [[NSUUID alloc] initWithUUIDBytes:boolean_true];
	else
	    return [[NSUUID alloc] initWithUUIDBytes:boolean_false];
    }
    
    if (type == CFStringGetTypeID())
	return (id)CFStringCreateCopy(NULL, cf);
    if (type == CFDataGetTypeID())
	return (id)CFDataCreateCopy(NULL, cf);
    if (type == CFNumberGetTypeID())
	return (id)CFRetain(cf);
    
    if (type == CFUUIDGetTypeID()) {
        union {
	    CFUUIDBytes bytes;
	    uuid_t uuid;
	} u;
	
	u.bytes = CFUUIDGetUUIDBytes(cf);
	
	return [[NSUUID alloc] initWithUUIDBytes:u.uuid];
    }
    if (type == CFArrayGetTypeID()) {
	CFIndex n, len = CFArrayGetCount(cf);
	NSMutableArray *array = [[NSMutableArray alloc] initWithCapacity:len];
	if (!array) return NULL;
	
	for (n = 0; n < len; n++) {
	    id obj = [HeimCredDecoder copyCF2NS:CFArrayGetValueAtIndex(cf, n)];
	    if (obj) [array addObject:obj];
	    [obj release];
	}
        return array;
    }
    if (type == CFDictionaryGetTypeID()) {
        CFIndex len = CFDictionaryGetCount(cf);
	
        NSMutableDictionary *dict = [[NSMutableDictionary alloc] initWithCapacity:len];
        if (!dict) return NULL;
	
	CFDictionaryApplyFunction(cf, convertDict, dict);
	return dict;
    }
    return NULL;
}

static NSData *
ksEncryptData(NSData *plainText)
{
    NSMutableData *blob = NULL;
    
    const uint32_t bulkKeySize = 32; /* Use 256 bit AES key for bulkKey. */
    const uint32_t maxKeyWrapOverHead = 8 + 32;
    uint8_t bulkKey[bulkKeySize];
    uint8_t bulkKeyWrapped[bulkKeySize + maxKeyWrapOverHead];
    size_t bulkKeyWrappedSize = sizeof(bulkKeyWrapped);
    uint32_t key_wrapped_size;
    CCCryptorStatus ccerr;

    
    if (![plainText isKindOfClass:[NSData class]]) abort();
    
    size_t ctLen = [plainText length];
    size_t tagLen = 16;
    
    if (SecRandomCopyBytes(kSecRandomDefault, bulkKeySize, bulkKey))
        abort();

#if 0
    /* Now that we're done using the bulkKey, in place encrypt it. */
    require_noerr_quiet(error = ks_crypt(kAppleKeyStoreKeyWrap, keybag, keyclass,
					 bulkKeySize, bulkKey, bulkKeyWrapped,
					 &bulkKeyWrappedSize), out);
#else
    bulkKeyWrappedSize = bulkKeySize;
    memcpy(bulkKeyWrapped, bulkKey, bulkKeySize);
#endif
    key_wrapped_size = (uint32_t)bulkKeyWrappedSize;
    
    size_t blobLen = sizeof(key_wrapped_size) + key_wrapped_size + ctLen + tagLen;
    
    blob = [NSMutableData dataWithLength:blobLen];
    if (blob == NULL)
	return NULL;

    UInt8 *cursor = [blob mutableBytes];
    
    *((uint32_t *)cursor) = key_wrapped_size;
    cursor += sizeof(key_wrapped_size);
    
    memcpy(cursor, bulkKeyWrapped, key_wrapped_size);
    cursor += key_wrapped_size;
    
    ccerr = CCCryptorGCM(kCCEncrypt, kCCAlgorithmAES128,
			 bulkKey, bulkKeySize,
			 NULL, 0,  /* iv */
			 NULL, 0,  /* auth data */
			 [plainText bytes], ctLen,
			 cursor,
			 cursor + ctLen, &tagLen);
    memset(bulkKey, 0, sizeof(bulkKey));
    if (ccerr || tagLen != 16) {
	[blob release];
	return NULL;
    }

    return blob;
}

static int
ks_decrypt_data(CFDataRef blob, CFDataRef *pPlainText) {
    const uint32_t bulkKeySize = 32; /* Use 256 bit AES key for bulkKey. */
    uint8_t bulkKey[bulkKeySize];
    int error = EINVAL;
    CCCryptorStatus ccerr;
    
    CFMutableDataRef plainText = NULL;
    
    if (CFGetTypeID(blob) != CFDataGetTypeID())
        return EINVAL;
    
    size_t blobLen = CFDataGetLength(blob);
    const uint8_t *cursor = CFDataGetBytePtr(blob);

    uint32_t wrapped_key_size;
    
    size_t ctLen = blobLen;
    

    size_t tagLen = 16;
    if (ctLen < tagLen)
	return EINVAL;
    
    ctLen -= tagLen;
    
    uint8_t tag[tagLen];
    
    if (ctLen < sizeof(wrapped_key_size))
	return EINVAL;

    wrapped_key_size = *((uint32_t *)cursor);
    cursor += sizeof(wrapped_key_size);
    ctLen -= sizeof(wrapped_key_size);
    
    /* Validate key wrap length against total length */
    if (ctLen < wrapped_key_size)
	return EINVAL;

#if 0
    size_t bulkKeyCapacity = sizeof(bulkKey);

    /* Now unwrap the bulk key using a key in the keybag. */
    require_noerr_quiet(error = ks_crypt(kAppleKeyStoreKeyUnwrap, keybag,
					 keyclass, wrapped_key_size, cursor, bulkKey, &bulkKeyCapacity), out);
#else
    if (bulkKeySize != wrapped_key_size) {
	error = EINVAL;
	goto out;
    }

    memcpy(bulkKey, cursor, wrapped_key_size);
#endif
    cursor += wrapped_key_size;
    ctLen -= wrapped_key_size;
    
    plainText = CFDataCreateMutable(NULL, ctLen);
    if (!plainText) {
	error = EINVAL;
        goto out;
    }
    CFDataSetLength(plainText, ctLen);
    
    /* Decrypt the cipherText with the bulkKey. */

    ccerr = CCCryptorGCM(kCCDecrypt, kCCAlgorithmAES128,
			 bulkKey, bulkKeySize,
			 NULL, 0,  /* iv */
			 NULL, 0,  /* auth data */
			 cursor, ctLen,
			 CFDataGetMutableBytePtr(plainText),
			 tag, &tagLen);
    if (ccerr) {
	error = EINVAL;
	goto out;
    }
    if (tagLen != 16) {
	error = EINVAL;
	goto out;
    }
    cursor += ctLen;
    if (memcmp(tag, cursor, tagLen)) {
	error = EINVAL;
	goto out;
    }

    error = 0;

out:
    memset(bulkKey, 0, bulkKeySize);
    if (error) {
	CFRELEASE_NULL(plainText);
    } else {
	*pPlainText = plainText;
    }

    return error;
}


+ (id) copyUnarchiveObjectWithFileSecureEncoding:(NSString *)path
{
    @autoreleasepool {
	CFDataRef clearText = NULL;
	NSData *data = [NSData dataWithContentsOfFile:path options:NSDataReadingMappedIfSafe error:NULL];
	if (data == nil)
	    return NULL;
	
	int error = ks_decrypt_data((CFDataRef)data, &clearText);
	if (error)
	    return NULL;
	
	HeimCredDecoder *decoder = [[HeimCredDecoder alloc] initForReadingWithData:(NSData *)clearText];
	if (decoder == nil) {
	    CFRelease(clearText);
	    return NULL;
	}
	
	[decoder setRequiresSecureCoding:true];
	
	id obj = [[decoder decodeObjectForKey:NSKeyedArchiveRootObjectKey] retain];
        [decoder finishDecoding];
        [decoder release];
	CFRelease(clearText);
	
	return obj;
    }
}

+ (void)archiveRootObject:(id)object toFile:(NSString *)archiveFile
{
    @autoreleasepool {
	NSData *data = [NSKeyedArchiver archivedDataWithRootObject:object];
	if (data == nil)
	    return;
    
	NSData *encText = ksEncryptData(data);
	if (encText == NULL)
	    return;
	
	[encText writeToFile:archiveFile atomically:NO];
    }
}


- (NSSet *)allowedClasses
{
    static dispatch_once_t onceToken;
    static NSSet *_set;
    dispatch_once(&onceToken, ^{
	_set = [[NSSet alloc] initWithObjects:[NSMutableDictionary class], [NSDictionary class],
		[NSMutableArray class], [NSArray class],
		[NSMutableString class], [NSString class],
		[NSNumber class],
		[NSUUID class],
		[NSMutableData class], [NSData class],
		nil];
    });
    return [[_set retain] autorelease];
}

@end
