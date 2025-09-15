//
//  PSCert.h
//  CertificateTool
//
//  Copyright (c) 2012-2013 Apple Inc. All Rights Reserved.
//

#import <Foundation/Foundation.h>

extern const NSString* kSecAnchorTypeUndefined;
extern const NSString* kSecAnchorTypeSystem;
extern const NSString* kSecAnchorTypePlatform;
extern const NSString* kSecAnchorTypeCustom;
extern const NSString* kSecAnchorTypeSystemTEST;
extern const NSString* kSecAnchorTypePlatformTEST;

@interface PSCert : NSObject
{
@private
    NSData*             _cert_data;
    NSNumber*			_flags;
    NSData*             _normalized_subject_hash;
    NSData*             _certificate_hash;
    NSData*             _certificate_sha256_hash;
	NSData*				_public_key_hash;
    NSData*             _spki_hash;
    NSString*           _file_path;
    NSString*           _auth_key_id;
    NSString*           _subj_key_id;
    const NSString*     _anchor_type;
}

@property (readonly) NSData* cert_data;
@property (readonly) NSData* normalized_subject_hash;
@property (readonly) NSData* certificate_hash;
@property (readonly) NSData* certificate_sha256_hash;
@property (readonly) NSData* public_key_hash;
@property (readonly) NSData* spki_hash;
@property (readonly) NSString* file_path;
@property (readonly) NSString* auth_key_id;
@property (readonly) NSString* subj_key_id;
@property (readonly) NSNumber* flags;
@property (readonly) NSString* anchor_type;

- (id)initWithCertFilePath:(NSString *)filePath withFlags:(NSNumber*)flags;

@end
