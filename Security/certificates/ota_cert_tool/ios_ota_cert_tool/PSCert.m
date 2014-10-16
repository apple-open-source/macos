//
//  PSCert.m
//  ios_ota_cert_tool
//
//  Created by James Murphy on 12/11/12.
//  Copyright (c) 2012 James Murphy. All rights reserved.
//

#import "PSCert.h"
#import "PSUtilities.h"

@implementation PSCert

@synthesize cert_hash = _cert_hash;


- (id)initWithCertFilePath:(NSString *)filePath
{
    if ((self = [super init]))
    {
        _cert_hash = nil;
        
        CFDataRef temp_cf_data = [PSUtilities readFile:filePath];
        if (NULL == temp_cf_data)
        {
            NSLog(@"PSCert: Unable to read data for file %@", filePath);
            return nil;
        }
        _cert_hash  = [PSUtilities digestAndEncode:temp_cf_data useSHA1:NO];
        CFRelease(temp_cf_data);
    }
    return self;
}



@end
