//
//  PSCertKey.h
//  CertificateTool
//
//  Copyright (c) 2012-2013 Apple Inc. All Rights Reserved.
//

#import <Foundation/Foundation.h>


@interface PSCertKey : NSObject
{
@private
    NSString*             _key_hash;
}

@property (readonly) NSString* key_hash;

- (id)initWithCertFilePath:(NSString *)filePath;

@end
