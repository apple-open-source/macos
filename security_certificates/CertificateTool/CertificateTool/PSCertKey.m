//
//  PSCertKey.m
//  CertificateTool
//
//  Copyright (c) 2012-2013 Apple Inc. All Rights Reserved.
//

#import "PSCertKey.h"
#import <Security/Security.h>
#import "PSUtilities.h"

@implementation PSCertKey

@synthesize key_hash = _key_hash;


- (id)initWithCertFilePath:(NSString *)filePath
{
    if ((self = [super init]))
    {
        _key_hash = nil;
        
        CFDataRef temp_cf_data = CFBridgingRetain([PSUtilities readFile:filePath]);
        if (NULL == temp_cf_data)
        {
            NSLog(@"PSCertKey: Unable to read data for file %@", filePath);
            return nil;
        }
        
        SecCertificateRef aCert = [PSUtilities getCertificateFromData:temp_cf_data];
        CFRelease(temp_cf_data);
        if (NULL != aCert)
        {
            CFDataRef temp_key_data = [PSUtilities getKeyDataFromCertificate:aCert];
            if (NULL != temp_key_data)
            {
                _key_hash = [PSUtilities digestAndEncode:temp_key_data useSHA1:YES];
            }
            CFRelease(aCert);
        }
    }
    return self;
}

@end
