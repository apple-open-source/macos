//
//  PSCerts.m
//  ios_ota_cert_tool
//
//  Created by James Murphy on 12/11/12.
//  Copyright (c) 2012 James Murphy. All rights reserved.
//

#import "PSCerts.h"
#import "PSCertKey.h"
#import "PSCert.h"

@interface PSCerts (PrivateMethods)

- (void)get_certs:(BOOL)forBadCerts;

@end

@implementation PSCerts

@synthesize certs = _certs;

- (void)get_certs:(BOOL)forBad
{
    if (nil != _cert_dir_path)
    {
        @autoreleasepool
        {
            NSFileManager* fileManager = [NSFileManager defaultManager];
            BOOL isDir = NO;
            if (![fileManager fileExistsAtPath:_cert_dir_path isDirectory:&isDir] || !isDir)
            {
                return;
            }
            
            NSDirectoryEnumerator* enumer = [fileManager enumeratorAtPath:_cert_dir_path];
            if (nil == enumer)
            {
                return;
            }
            
            for(NSString* cert_path_str in enumer)
            {
                if ([cert_path_str hasPrefix:@"."])
                {
                    continue;
                }
                
                NSLog(@"Processing file %@", cert_path_str);
                
                NSString* full_path = [_cert_dir_path stringByAppendingPathComponent:cert_path_str];
                
                if (forBad)
                {
                    PSCertKey* aCertKey = [[PSCertKey alloc] initWithCertFilePath:full_path];
                    if (nil != aCertKey)
                    {
                        [_certs addObject:aCertKey.key_hash];
                    }
                }
                else
                {
                    PSCert* aCert = [[PSCert alloc] initWithCertFilePath:full_path];
                    if (nil != aCert)
                    {
                        [_certs addObject:aCert.cert_hash];
                    }
                    
                }
            }
        }
    }
}

- (id)initWithCertFilePath:(NSString *)filePath forBadCerts:(BOOL)forBad
{
    if (self = [super init])
    {
        _cert_dir_path = filePath;
        _certs = [NSMutableArray array];
        [self get_certs:forBad];
    }
    return self;
}

@end
