//
//  PSCert.m
//  CertificateTool
//
//  Copyright (c) 2012-2017 Apple Inc. All Rights Reserved.
//

#import "PSCert.h"
#import "PSUtilities.h"
#import "PSAssetConstants.h"
#import <corecrypto/ccsha1.h>
#import <corecrypto/ccsha2.h>
#import <corecrypto/ccdigest.h>
#import <Security/Security.h>

#import "DataConversion.h"

// SecCertificateInternal.h declararations
CFDataRef SecCertificateGetAuthorityKeyID(SecCertificateRef certificate);

// SecCertificatePriv.h declarations
// (note we cannot simply include SecCertificatePriv.h, since some of
// these functions were exported but not declared prior to 10.12.2,
// and we need to build on earlier versions.)
CFDataRef SecCertificateGetNormalizedIssuerContent(SecCertificateRef certificate);
CFDataRef SecCertificateGetSubjectKeyID(SecCertificateRef certificate);


static CFDataRef GetNormalizedIssuerContent(SecCertificateRef cert)
{
    return SecCertificateGetNormalizedIssuerContent(cert);
}

static CFDataRef GetAuthorityKeyID(SecCertificateRef cert)
{
    return SecCertificateGetAuthorityKeyID(cert);
}

static CFDataRef GetSubjectKeyID(SecCertificateRef cert)
{
    return SecCertificateGetSubjectKeyID(cert);
}


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
@synthesize certificate_sha256_hash = _certificate_sha256_hash;
@synthesize public_key_hash = _public_key_hash;
@synthesize file_path = _file_path;
@synthesize auth_key_id = _auth_key_id;
@synthesize subj_key_id = _subj_key_id;
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

        _normalized_subject_hash = nil;
        _public_key_hash = nil;
        _auth_key_id = nil;
        _subj_key_id = nil;
        _certificate_sha256_hash = nil;

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

        if ( (isGrayListed & assetFlags) || (isBlocked & assetFlags) )
        {
            _public_key_hash = [self getPublicKeyHash:certRef];
            if (NULL == _public_key_hash)
            {
                NSLog(@"PSCert: Unable to get the public key hash for file %@", filePath);
                return nil;
            }
        }

        if (isAllowListed & assetFlags)
        {
            _certificate_sha256_hash = [self getCertificateSHA256Hash];
            if (isAnchor & assetFlags)
            {
                _auth_key_id = [self getKeyIDString:certRef forAuthKey:YES];
                _subj_key_id = [self getKeyIDString:certRef forAuthKey:NO];
            }
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
    SecCertificateRef iosCertRef = NULL;
    NSData* normalized_subject = nil;
    CFDataRef cert_data = SecCertificateCopyData(cert_ref);
    if (NULL == cert_data)
    {
        NSLog(@"SecCertificateCopyData returned NULL");
        return result;
    }

    iosCertRef = SecCertificateCreateWithData(NULL, cert_data);
    CFRelease(cert_data);

    if (NULL != iosCertRef)
    {
        CFDataRef temp_data = GetNormalizedIssuerContent(iosCertRef);

        if (NULL == temp_data)
        {
            CFStringRef name = SecCertificateCopySubjectSummary(cert_ref);
            NSLog(@"GetNormalizedIssuerContent returned NULL for %@", name);
            if (name) { CFRelease(name); }
            CFRelease(iosCertRef);
            return result;
        }

        if (CFGetTypeID(temp_data) != CFDataGetTypeID())
        {
            CFStringRef name = SecCertificateCopySubjectSummary(cert_ref);
            NSLog(@"GetNormalizedIssuerContent returned non-CFDataRef type for %@", name);
            if (name) { CFRelease(name); }
            CFRelease(iosCertRef);
            return result;
        }

        normalized_subject = [NSData dataWithBytes:CFDataGetBytePtr(temp_data) length:CFDataGetLength(temp_data)];
        CFRelease(iosCertRef);
    }

    if (NULL == normalized_subject)
    {
        NSLog(@"normalized_subject is nil!");
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

- (NSData *)getCertificateSHA256Hash
{
    NSData* result = nil;
    unsigned char certificate_digest[CCSHA256_OUTPUT_SIZE];
    const struct ccdigest_info* digest_info = ccsha256_di();

    ccdigest(digest_info, (unsigned long)[_cert_data length], [_cert_data bytes], certificate_digest);

    result = [NSData dataWithBytes:certificate_digest length:CCSHA256_OUTPUT_SIZE];
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

- (NSString *)getKeyIDString:(SecCertificateRef)cert_ref forAuthKey:(BOOL)auth_key
{
    NSString *result = nil;

    SecCertificateRef iosCertRef = NULL;
    NSData* key_data = nil;
    CFDataRef cert_data = SecCertificateCopyData(cert_ref);
    if (NULL == cert_data)
    {
        NSLog(@"SecCertificateCopyData returned NULL");
        return result;
    }

    iosCertRef = SecCertificateCreateWithData(NULL, cert_data);
    CFRelease(cert_data);

    if (NULL != iosCertRef)
    {
        CFDataRef temp_data = (auth_key) ? GetAuthorityKeyID(iosCertRef) : GetSubjectKeyID(iosCertRef);
        NSString *keyid_str = (auth_key) ? @"GetAuthorityKeyID" : @"GetSubjectKeyID";

        if (NULL == temp_data)
        {
            CFStringRef name = SecCertificateCopySubjectSummary(cert_ref);
            NSLog(@"%@ returned NULL for %@", keyid_str, name);
            if (name) { CFRelease(name); }
            CFRelease(iosCertRef);
            return result;
        }

        if (CFGetTypeID(temp_data) != CFDataGetTypeID())
        {
            CFStringRef name = SecCertificateCopySubjectSummary(cert_ref);
            NSLog(@"%@ returned non-CFDataRef type for %@", keyid_str, name);
            if (name) { CFRelease(name); }
            CFRelease(iosCertRef);
            return result;
        }

        key_data = [NSData dataWithBytes:CFDataGetBytePtr(temp_data) length:CFDataGetLength(temp_data)];
#if DEBUG
            NSString *str = [[key_data toHexString] uppercaseString];
            CFStringRef name = SecCertificateCopySubjectSummary(cert_ref);
            NSLog(@"AuthKeyID for %@ is %@", name, str);
            if (name) { CFRelease(name); }
        }
#endif //DEBUG
        CFRelease(iosCertRef);
    }

    if (key_data) {
        return [[key_data toHexString] uppercaseString];
    } else {
        return nil;
    }
}

@end
