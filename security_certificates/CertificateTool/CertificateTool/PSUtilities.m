//
//  PSUtilities.m
//  CertificateTool
//
//  Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
//

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
    CFErrorRef cfError  = NULL;
    NSString* result = nil;
    CFTypeRef digestType = kSecDigestSHA2;
    CFIndex digestLength = 256;
    
    if (useSHA1)
    {
        digestType = kSecDigestSHA1;
        digestLength = 0;
    }
    
    SecTransformRef digestXForm = SecDigestTransformCreate(digestType, digestLength, &cfError);
    if (NULL != cfError)
    {
        if (NULL != digestXForm)
        {
            CFRelease(digestXForm);
        }
        CFRelease(cfData);
		[PSUtilities outputError:@"Could not create the digesting transform." withError:cfError];
        return result;
    }
    
 	if (!SecTransformSetAttribute(digestXForm, kSecTransformInputAttributeName, cfData, &cfError))
    {
        CFRelease(cfData);
        CFRelease(digestXForm);
        [PSUtilities outputError:@"Could not set the input attribute" withError:cfError];
        return result;
    }
    
    SecTransformRef base64Xform = SecEncodeTransformCreate(kSecBase64Encoding, &cfError);
    
    if (NULL != cfError)
    {
            
        if (NULL != base64Xform)
        {
           CFRelease(base64Xform); 
        }
        CFRelease(cfData);
        CFRelease(digestXForm);
		[PSUtilities outputError:@"Could not create the encoding transform." withError:cfError];
        return result;
    }
    
    SecGroupTransformRef groupXForm = SecTransformCreateGroupTransform();
    if (NULL == groupXForm)
    {
        CFRelease(digestXForm);
        CFRelease(base64Xform);
        NSLog(@"Could not create the group transform");
        return result;
    }
    
    SecTransformConnectTransforms(digestXForm, kSecTransformOutputAttributeName,
                                  base64Xform, kSecTransformInputAttributeName,
                                  groupXForm, &cfError);
    CFRelease(digestXForm);
    CFRelease(base64Xform);
    
    if (NULL != cfError)
    {
        if (NULL != groupXForm)
        {
            CFRelease(groupXForm);
        }
		[PSUtilities outputError:@"Could not connect the transforms" withError:cfError];
        return result;
    }
    
    CFDataRef cfResult = (CFDataRef)SecTransformExecute(groupXForm, &cfError);
    CFRelease(groupXForm);
    if (NULL != cfError)
    {
        if (NULL != cfResult)
        {
            CFRelease(cfResult);
        }
        
        [PSUtilities outputError:@"Could not execute the transform." withError:cfError];
        return result;
    }
    const void* pPtr = (const void*)CFDataGetBytePtr(cfResult);
    NSUInteger len = (NSUInteger)CFDataGetLength(cfResult);
    
	NSData* temp_data = [[NSData alloc] initWithBytes:pPtr length:len];
    CFRelease(cfResult);
	result = [[NSString alloc] initWithData:temp_data encoding:NSUTF8StringEncoding];
    
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
    
    SecKeyRef aPublicKey = NULL;
    OSStatus err = SecCertificateCopyPublicKey(cert, &aPublicKey);
    if (errSecSuccess == err && NULL != aPublicKey)
    {
        err = SecItemExport(aPublicKey, kSecFormatBSAFE, 0, NULL, &result);
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
	if (NULL == data || NULL == key)
	{
		return result;
	}
	
    CFTypeRef digestType = kSecDigestHMACSHA2;
    CFIndex digestLength = 256;
    
    if (useSHA1)
    {
        digestType = kSecDigestSHA1;
        digestLength = 0;
    }
    
	CFErrorRef error = NULL;
	SecTransformRef signXForm =  SecSignTransformCreate(key, &error);
	if (NULL != error)
	{
        if (NULL != signXForm)
        {
            CFRelease(signXForm);
        }
		[PSUtilities outputError:@"Unable to create the signing transform" withError:error];
		return result;
	}
    
    if (!SecTransformSetAttribute(signXForm, kSecTransformInputAttributeName, data, &error))
    {
        CFRelease(signXForm);
		[PSUtilities outputError:@"Could not set the input attribute" withError:error];
        return result;
    }
	
	if (!SecTransformSetAttribute(signXForm, kSecDigestTypeAttribute, digestType, &error))
	{
		CFRelease(signXForm);
		[PSUtilities outputError:@"Unable to set the digest type attribute" withError:error];
		return result;
	}
	
    CFNumberRef digest_length_number  = CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType, &digestLength);
	if (!SecTransformSetAttribute(signXForm, kSecDigestLengthAttribute, digest_length_number, &error))
	{
		CFRelease(signXForm);
        CFRelease(digest_length_number);
		[PSUtilities outputError:@"Unable to set the digest length attribute" withError:error];
		return result;
	}
    CFRelease(digest_length_number);
	
	if (!SecTransformSetAttribute(signXForm, kSecInputIsAttributeName, kSecInputIsPlainText, &error))
	{
		CFRelease(signXForm);
		[PSUtilities outputError:@"Unable to set the is plain text attribute" withError:error];
		return result;
	}
	
	SecTransformRef base64Xform = SecEncodeTransformCreate(kSecBase64Encoding, &error);
    if (NULL != error)
    {
        if (NULL != base64Xform)
        {
            CFRelease(base64Xform);
        }
		CFRelease(signXForm);
		[PSUtilities outputError:@"Could not create the encoding transform." withError:error];
		return result;
    }
    
    SecGroupTransformRef groupXForm = SecTransformCreateGroupTransform();
    if (NULL == groupXForm)
    {
        CFRelease(signXForm);
        CFRelease(base64Xform);
        NSLog(@"Could not create the group transform");
        return result;
    }
    
    SecTransformConnectTransforms(signXForm, kSecTransformOutputAttributeName,
                                  base64Xform, kSecTransformInputAttributeName,
                                  groupXForm, &error);
    CFRelease(signXForm);
    CFRelease(base64Xform);
	if (NULL != error)
    {
        CFRelease(groupXForm);
		[PSUtilities outputError:@"Could connect the signing and encoding transforms." withError:error];
		return result;
    }
    
	CFDataRef cfResult = (CFDataRef)SecTransformExecute(groupXForm, &error);
    CFRelease(groupXForm);
    if (NULL != error)
    {
        if (NULL != cfResult)
        {
            CFRelease( cfResult);
        }
        [PSUtilities outputError:@"Could not execute the transform." withError:error];
        return result;
    }
    const void* pPtr = (const void*)CFDataGetBytePtr(cfResult);
    NSUInteger len = (NSUInteger)CFDataGetLength(cfResult);
    
    NSData* temp_data = [[NSData alloc] initWithBytes:pPtr length:len];
    CFRelease(cfResult);
    result = [[NSString alloc] initWithData:temp_data encoding:NSUTF8StringEncoding];
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
