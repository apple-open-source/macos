//
//  PSCerts.h
//  CertificateTool
//
//  Copyright (c) 2012-2013 Apple Inc. All Rights Reserved.
//

#import <Foundation/Foundation.h>

@interface PSCerts : NSObject
{
    NSString*       _cert_dir_path;
    NSMutableArray* _certs;
    NSNumber*       _flags;
    
}

@property (readonly) NSArray* certs;

- (id)initWithCertFilePath:(NSString *)filePath withFlags:(NSNumber *)flags;

@end
