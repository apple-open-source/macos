//
//  PSUtilities.h
//  CertificateTool
//
//  Copyright (c) 2012-2013 Apple Inc. All Rights Reserved.
//

#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#import <Security/Security.h>

@interface PSUtilities : NSObject

+ (NSString*)digestAndEncode:(CFDataRef)cfData useSHA1:(BOOL)useSHA1;

+ (NSData *)readFile:(NSString *)file_path;

+ (SecCertificateRef)getCertificateFromData:(CFDataRef)data;

+ (CFDataRef)getKeyDataFromCertificate:(SecCertificateRef)cert;

+ (NSString *)getCommonNameFromCertificate:(SecCertificateRef)cert;

+ (SecKeyRef)getPrivateKeyWithName:(NSString *)keyName;

+ (NSString *)signAndEncode:(CFDataRef)data usingKey:(SecKeyRef)key useSHA1:(BOOL)useSHA1;

@end
