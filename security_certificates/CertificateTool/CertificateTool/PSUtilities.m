//
//  PSUtilities.m
//  CertificateTool
//
//  Copyright (c) 2012-2014,2023 Apple Inc. All Rights Reserved.
//

#import <CommonCrypto/CommonCrypto.h>
#import "PSUtilities.h"

@implementation PSUtilities

+ (void)outputError:(NSString *)message withError:(CFErrorRef)error
{
	NSString* output = nil;
	if (nil != error)
	{
		CFStringRef aCFStr = CFErrorCopyDescription(error);
		if (NULL != aCFStr)
		{
			output = [NSString stringWithFormat:@"%@ error: %@", message, aCFStr];
			CFRelease(aCFStr);
		}
	}
	
	if (nil == output)
	{
		output = message;
	}
	
	NSLog(@"%@",output);
	if (NULL != error)
	{
		CFRelease(error);
	}
}

+ (NSString *)digestAndEncode:(CFDataRef)cfData useSHA1:(BOOL)useSHA1;
{
    NSData* data = (__bridge NSData*)cfData;
    NSData* digestData = nil;
    NSString* result = nil;
    uint8_t digest[CC_SHA256_DIGEST_LENGTH] = {0};
    if (nil == data) {
        [PSUtilities outputError:@"No data provided to digest" withError:NULL];
        return result;
    }
    if (useSHA1) {
        CC_SHA1([data bytes],  (CC_LONG)[data length], digest);
        digestData = [NSData dataWithBytes:digest length:CC_SHA1_DIGEST_LENGTH];
    } else {
        CC_SHA256([data bytes], (CC_LONG)[data length], digest);
        digestData = [NSData dataWithBytes:digest length:CC_SHA256_DIGEST_LENGTH];
    }
    if (nil == digestData) {
        [PSUtilities outputError:@"Failed to obtain digest of data" withError:NULL];
        return result;
    }
    result = [digestData base64EncodedStringWithOptions:0];
    return result;
}

+ (NSData *)readFile:(NSString *)file_path
{
    NSData* result = NULL;
    
    NSError* error = nil;
    
    if (nil == file_path)
    {
        NSLog(@"PSUtilities.readFile called with a nil file path");
        return result;
        
    }
    
    NSFileManager* fileManager = [NSFileManager defaultManager];
    BOOL isDir = NO;
    if (![fileManager fileExistsAtPath:file_path isDirectory:&isDir])
    {
        NSLog(@"PSUtilities.readFile %@ does not exist", file_path);
        return result;
    }
    
    if (isDir)
    {
        NSLog(@"PSUtilities.readFile %@ does exist but it is a directory", file_path);
        return result;
    }
    
    
    NSData* temp_data = [NSData dataWithContentsOfFile:file_path options:0 error:&error];
    if (nil != error)
    {
        NSLog(@"NSData dataWithContentsOfFile returned error %@", file_path);
        return result;
    }
    
    result = temp_data;
    
    return result;
}

+ (SecCertificateRef)getCertificateFromData:(CFDataRef)data
{
    SecCertificateRef result = NULL;
    SecExternalFormat inputFormat = kSecFormatUnknown;
    SecExternalItemType itemType = kSecItemTypeUnknown;
    SecItemImportExportFlags flags = 0;
    CFArrayRef outItems = NULL;
    
    OSStatus err = SecItemImport(data, NULL, &inputFormat, &itemType, flags,
                                         NULL, NULL, &outItems);
    if (errSecSuccess != err)
    {
        NSLog(@"Could not import data");
    }
    
    if (NULL != outItems)
    {
        CFIndex num_items = CFArrayGetCount(outItems);
        if (num_items > 0)
        {
            CFTypeRef anItem = (CFTypeRef)CFArrayGetValueAtIndex(outItems, 0);
            if (NULL != anItem && (CFGetTypeID(anItem) == SecCertificateGetTypeID()))
            {
                result = (SecCertificateRef)anItem;
            }
        }
        
        if (NULL != result)
        {
            CFRetain(result);
        }
        CFRelease(outItems);
    }
    return result;
}

+ (CFDataRef)getKeyDataFromCertificate:(SecCertificateRef)cert
{
    CFDataRef result = NULL;
    
    if (NULL == cert)
    {
        return result;
    }
    
    SecKeyRef aPublicKey = SecCertificateCopyKey(cert);
    if (NULL != aPublicKey)
    {
        OSStatus err = SecItemExport(aPublicKey, kSecFormatBSAFE, 0, NULL, &result);
        if (errSecSuccess != err)
        {
            result = NULL;
        }
        CFRelease(aPublicKey);
    }
    return result;
}

+ (SecKeyRef)getPrivateKeyWithName:(NSString *)keyName
{
	SecKeyRef result = NULL;
	NSArray* key_array = [NSArray arrayWithObjects:(id)kSecClass, kSecAttrLabel, kSecReturnRef, nil];
	NSArray* obj_array = [NSArray arrayWithObjects:(id)kSecClassKey, keyName, kCFBooleanTrue, nil];
	NSDictionary* query = [NSDictionary dictionaryWithObjects:obj_array forKeys:key_array];
	
	OSStatus err = SecItemCopyMatching((__bridge CFDictionaryRef)(query), (CFTypeRef *)&result);
	if (errSecSuccess != err)
	{
		NSLog(@"Unable to find the Private Key");
	}
	return result;
}

+ (NSString *)signAndEncode:(CFDataRef)data usingKey:(SecKeyRef)key useSHA1:(BOOL)useSHA1
{
    NSString* result = nil;
    NSLog(@"ERROR: signAndEncode is no longer implemented!");
    return result;
}

+ (NSString *)getCommonNameFromCertificate:(SecCertificateRef)cert
{
    NSString* result = nil;
    
    if (NULL == cert)
    {
        return result;
    }
    
    CFStringRef commonName = NULL;
    OSStatus err = SecCertificateCopyCommonName(cert, &commonName);
    if (errSecSuccess != err)
    {
        return result;
    }
    
    result = [NSString stringWithString:(__bridge NSString *)(commonName)];
    
    
    return result;
}

@end
