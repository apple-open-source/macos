//
//  PSCertKey.m
//  ios_ota_cert_tool
//
//  Created by James Murphy on 12/12/12.
//  Copyright (c) 2012 James Murphy. All rights reserved.
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
        
        CFDataRef temp_cf_data = [PSUtilities readFile:filePath];
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
                CFRelease(temp_key_data);
            }
            CFRelease(aCert);
        }
    }
    return self;
}

@end
