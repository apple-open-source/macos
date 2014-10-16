//
//  PSUtilities.h
//  ios_ota_cert_tool
//
//  Created by James Murphy on 12/12/12.
//  Copyright (c) 2012 James Murphy. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#import <Security/Security.h>

@interface PSUtilities : NSObject

+ (NSString*)digestAndEncode:(CFDataRef)cfData useSHA1:(BOOL)useSHA1;

+ (CFDataRef)readFile:(NSString *)file_path;

+ (SecCertificateRef)getCertificateFromData:(CFDataRef)data;

+ (CFDataRef)getKeyDataFromCertificate:(SecCertificateRef)cert;

+ (SecKeyRef)getPrivateKeyWithName:(NSString *)keyName;

+ (NSString *)signAndEncode:(CFDataRef)data usingKey:(SecKeyRef)key useSHA1:(BOOL)useSHA1;


@end
