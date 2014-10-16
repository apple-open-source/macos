//
//  PSCert.m
//  CertificateTool
//
//  Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
//

#import "PSCert.h"
#import "PSUtilities.h"
#import "PSAssetConstants.h"
//#import <CommonCrypto/CommonDigest.h>
#import <corecrypto/ccsha1.h>
#import <corecrypto/ccdigest.h>
#import <Security/Security.h>

#import "DataConversion.h"

/* In OS X 10.9, the following symbols are exported:
_SecCertificateCopyIssuerSequence  (SecCertificateRef)  SecCertificatePriv.h
_SecCertificateCopySubjectSequence  (SecCertificateRef)  SecCertificatePriv.h
_SecCertificateGetNormalizedIssuerContent  (SecCertificateRefP)  SecCertificateInternal.h
_SecCertificateGetNormalizedSubjectContent  (SecCertificateRefP)  SecCertificateInternal.h
_SecCertificateCopyNormalizedIssuerContent  (SecCertificateRef)  SecCertificate.h
_SecCertificateCopyNormalizedSubjectContent  (SecCertificateRef)  SecCertificate.h
 */
// This is from the SecCertificatePriv.h file
CFDataRef SecCertificateCopyIssuerSequence(SecCertificateRef certificate);
CFDataRef SecCertificateCopySubjectSequence(SecCertificateRef certificate);

// This is from the SecCertificateInternal.h file
typedef struct __SecCertificate *SecCertificateRefP;
SecCertificateRefP SecCertificateCreateWithDataP(CFAllocatorRef allocator, CFDataRef der_certificate);
CFDataRef SecCertificateGetNormalizedIssuerContent(SecCertificateRefP certificate);
CFDataRef SecCertificateGetNormalizedSubjectContent(SecCertificateRefP certificate);


@interface PSCert (PrivateMethods)

- (NSData *)getNormalizedSubjectHash:(SecCertificateRef)cert_ref;
- (NSData *)getCertificateHash;
- (NSData *)getPublicKeyHash:(SecCertificateRef)cert_ref;
- (NSData *)createCertRecord;

@end

@implementation PSCert

@synthesize cert_data = _cert_data;
@synthesize normalized_subject_hash = _normalized_subject_hash;
@synthesize certificate_hash = _certificate_hash;
@synthesize public_key_hash = _public_key_hash;
@synthesize file_path = _file_path;
@synthesize flags = _flags;


- (id)initWithCertFilePath:(NSString *)filePath withFlags:(NSNumber*)flags
{
    if ((self = [super init]))
    {
        _file_path = filePath;
        _flags = flags;
        _cert_data = [PSUtilities readFile:filePath];
        if (NULL == _cert_data)
        {
            NSLog(@"PSCert: Unable to read data for file %@", filePath);
            return nil;
        }
        
        PSAssetFlags assetFlags = [_flags unsignedLongValue];
        
        
        SecCertificateRef certRef = SecCertificateCreateWithData(kCFAllocatorDefault, (__bridge CFDataRef)(_cert_data));
        if (NULL == certRef)
        {
            NSLog(@"Unable to create a SecCertificateRef from the cert data for file %@", filePath);
            return nil;
        }
        
        _certificate_hash = [self getCertificateHash];
        
        if (isAnchor & assetFlags)
        {
            _normalized_subject_hash = [self getNormalizedSubjectHash:certRef];
            if (NULL == _normalized_subject_hash)
            {
                NSLog(@"PSCert: Unable to get the normalized subject hash for file %@", filePath);
                return nil;
            }
        }
        else
        {
            _normalized_subject_hash = nil;
        }
        

        if ( (isGrayListed & assetFlags) || (isBlackListed & assetFlags) )
        {
            _public_key_hash = [self getPublicKeyHash:certRef];
            if (NULL == _public_key_hash)
            {
                NSLog(@"PSCert: Unable to get the public key hash for file %@", filePath);
                return nil;
            }
        }
        else
        {
            _public_key_hash = nil;
        }
		
        if (NULL != certRef)
        {
            CFRelease(certRef);
        }
    }
    return self;
}

- (NSData *)getNormalizedSubjectHash:(SecCertificateRef)cert_ref
{
    NSData* result = nil;
    
    if (NULL == cert_ref)
    {
        return result;
    }
    SecCertificateRefP iosCertRef = NULL;
    NSData* normalized_subject = NULL;
    CFDataRef cert_data = SecCertificateCopyData(cert_ref);
    if (NULL == cert_data)
    {
        NSLog(@"SecCertificateCopyData returned NULL");
        return result;
    }
    
    iosCertRef = SecCertificateCreateWithDataP(NULL, cert_data);
    CFRelease(cert_data);
    
    if (NULL != iosCertRef)
    {
        CFDataRef temp_data = SecCertificateGetNormalizedIssuerContent(iosCertRef);
        
        if (NULL == temp_data)
        {
            NSLog(@"SecCertificateGetNormalizedIssuerContent return NULL");
            CFRelease(iosCertRef);
            return result;
        }
        normalized_subject = [NSData dataWithBytes:CFDataGetBytePtr(temp_data) length:CFDataGetLength(temp_data)];
        CFRelease(iosCertRef);
    }
    
    if (NULL == normalized_subject)
    {
        
        NSLog(@"SecCertificateGetNormalizedIssuerContent returned NULL");
        return result;
    }
    
    unsigned char subject_digest[CCSHA1_OUTPUT_SIZE];
    memset(subject_digest, 0, CCSHA1_OUTPUT_SIZE);
    const struct ccdigest_info* digest_info = ccsha1_di();
    
    ccdigest(digest_info, (unsigned long)[normalized_subject length], [normalized_subject bytes], subject_digest);
    
    result = [NSData dataWithBytes:subject_digest length:CCSHA1_OUTPUT_SIZE];
    
    return result;
}

- (NSData *)getCertificateHash
{
    NSData* result = nil;
    unsigned char certificate_digest[CCSHA1_OUTPUT_SIZE];
    const struct ccdigest_info* digest_info = ccsha1_di();
    
    ccdigest(digest_info, (unsigned long)[_cert_data length], [_cert_data bytes], certificate_digest);
    //(void)CC_SHA1([_cert_data bytes], (CC_LONG)[_cert_data length], certificate_digest);
    
    result = [NSData dataWithBytes:certificate_digest length:CCSHA1_OUTPUT_SIZE];
    return result;
}

extern CFDataRef SecCertificateCopyPublicKeySHA1DigestFromCertificateData(CFAllocatorRef allocator, CFDataRef der_certificate);

- (NSData *)getPublicKeyHash:(SecCertificateRef)cert_ref;
{
    NSData* result = nil;
    
    CFDataRef derBits = SecCertificateCopyData(cert_ref);
    if (NULL == derBits)
    {
        return result;
    }
    
    CFDataRef hashBits = SecCertificateCopyPublicKeySHA1DigestFromCertificateData(kCFAllocatorDefault, derBits);
    CFRelease(derBits);
    if (NULL != hashBits)
    {
        result = CFBridgingRelease(hashBits);
    }
	
	return result;
}
@end
