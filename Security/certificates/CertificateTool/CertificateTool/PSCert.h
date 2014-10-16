//
//  PSCert.h
//  CertificateTool
//
//  Copyright (c) 2012-2013 Apple Inc. All Rights Reserved.
//

#import <Foundation/Foundation.h>

@interface PSCert : NSObject
{
@private
    NSData*             _cert_data;
    NSNumber*			_flags;
    NSData*             _normalized_subject_hash;
    NSData*             _certificate_hash;
	NSData*				_public_key_hash;
    NSString*           _file_path;
}

@property (readonly) NSData* cert_data;
@property (readonly) NSData* normalized_subject_hash;
@property (readonly) NSData* certificate_hash;
@property (readonly) NSData* public_key_hash;
@property (readonly) NSString* file_path;
@property (readonly) NSNumber* flags;

- (id)initWithCertFilePath:(NSString *)filePath withFlags:(NSNumber*)flags;

@end
