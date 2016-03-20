//
//  PSCert.m
//  CertificateTool
//
//  Copyright (c) 2012-2015 Apple Inc. All Rights Reserved.
//

#import "PSCert.h"
#import "PSUtilities.h"
#import "PSAssetConstants.h"
#import <corecrypto/ccsha1.h>
#import <corecrypto/ccsha2.h>
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
//CFDataRef SecCertificateGetNormalizedIssuerContentP(SecCertificateRefP certificate);
//CFDataRef SecCertificateGetNormalizedSubjectContentP(SecCertificateRefP certificate);
CFDataRef SecCertificateGetAuthorityKeyID(SecCertificateRef certificate);

// rdar://22245471
// The size and layout of a __SecCertificateP differs from the __SecCertificate struct
// that is more recent; in particular, the normalized issuer and normalized subject are
// at different offsets. To address this bug without changing the Security framework,
// we need to avoid calling SecCertificateGetNormalizedIssuerContent on a cert we created
// with SecCertificateCreateWithDataP (since it expects a different non-P struct version),
// and find the content offset ourselves.

#include <CoreFoundation/CFRuntime.h>

#ifndef _LIB_DER_OIDS_H_
typedef uint8_t DERByte;
typedef size_t DERSize;

typedef struct {
    DERByte		*data;
    DERSize		length;
} DERItem;
#endif

typedef struct {
    DERItem		oid;			/* OID */
    DERItem		params;			/* ASN_ANY, optional, DER_DEC_SAVE_DER */
} DERAlgorithmId;

typedef struct {
    bool                present;
    bool                critical;
    bool                isCA;
    bool                pathLenConstraintPresent;
    uint32_t			pathLenConstraint;
} SecCEBasicConstraints;

typedef struct {
    bool                present;
    bool                critical;
    bool                requireExplicitPolicyPresent;
    uint32_t			requireExplicitPolicy;
    bool                inhibitPolicyMappingPresent;
    uint32_t			inhibitPolicyMapping;
} SecCEPolicyConstraints;

typedef struct {
    DERItem policyIdentifier;
    DERItem policyQualifiers;
} SecCEPolicyInformation;

typedef struct {
    bool                    present;
    bool                    critical;
    size_t                  numPolicies;			// size of *policies;
    SecCEPolicyInformation  *policies;
} SecCECertificatePolicies;

typedef struct SecCertificateExtension {
    DERItem extnID;
    bool critical;
    DERItem extnValue;
} SecCertificateExtension;

struct __SecCertificateP {
    char _block1[
        sizeof(CFRuntimeBase) +
        sizeof(DERItem) +
        sizeof(DERItem) +
        sizeof(DERAlgorithmId) +
        sizeof(DERItem) +
        sizeof(UInt8) +
        sizeof(DERItem) +
        sizeof(DERAlgorithmId) +
        sizeof(DERItem) +
        sizeof(CFAbsoluteTime) +
        sizeof(CFAbsoluteTime) +
        sizeof(DERItem) +
        sizeof(DERAlgorithmId) +
        sizeof(DERItem) +
        sizeof(DERItem) +
        sizeof(DERItem) +
        sizeof(bool) +
        sizeof(SecCEBasicConstraints) +
        sizeof(SecCEPolicyConstraints) +
        sizeof(CFDictionaryRef) +
        sizeof(SecCECertificatePolicies) +
        sizeof(uint32_t) +
        sizeof(uint32_t)
    ];

    DERItem				_subjectKeyIdentifier;
    DERItem				_authorityKeyIdentifier;

    char _block2[
        sizeof(DERItem) +
        sizeof(DERItem) +
        sizeof(SecCertificateExtension *) +
        sizeof(CFMutableArrayRef) +
        sizeof(CFMutableArrayRef) +
        sizeof(CFMutableArrayRef) +
        sizeof(CFIndex) +
        sizeof(SecCertificateExtension *) +
        sizeof(SecKeyRef) +
        sizeof(CFDataRef) +
        sizeof(CFArrayRef) +
        sizeof(CFDataRef)
    ];

    CFDataRef			_normalizedIssuer;
    CFDataRef			_normalizedSubject;
    CFDataRef			_authorityKeyID;
    CFDataRef			_subjectKeyID;

    char _block3[
        sizeof(CFDataRef) +
        sizeof(uint8_t)
    ];
};

static CFDataRef GetNormalizedIssuerContent(SecCertificateRefP cert)
{
    struct __SecCertificateP *certP = (struct __SecCertificateP *)cert;
    if (!certP) {
        return NULL;
    }
    return certP->_normalizedIssuer;
}

static CFDataRef GetNormalizedSubjectContent(SecCertificateRefP cert)
{
    struct __SecCertificateP *certP = (struct __SecCertificateP *)cert;
    if (!certP) {
        return NULL;
    }
    return certP->_normalizedSubject;
}

static CFDataRef GetAuthorityKeyID(SecCertificateRefP cert)
{
    struct __SecCertificateP *certP = (struct __SecCertificateP *)cert;
    if (!certP) {
        return NULL;
    }
    if (!certP->_authorityKeyID && certP->_authorityKeyIdentifier.length) {
        certP->_authorityKeyID = CFDataCreate(kCFAllocatorDefault,
            certP->_authorityKeyIdentifier.data,
            certP->_authorityKeyIdentifier.length);
    }
    return certP->_authorityKeyID;
}

static CFDataRef GetSubjectKeyID(SecCertificateRefP cert)
{
    struct __SecCertificateP *certP = (struct __SecCertificateP *)cert;
    if (!certP) {
        return NULL;
    }
    if (!certP->_subjectKeyID && certP->_subjectKeyIdentifier.length) {
        certP->_subjectKeyID = CFDataCreate(kCFAllocatorDefault,
            certP->_subjectKeyIdentifier.data,
            certP->_subjectKeyIdentifier.length);
    }
    return certP->_subjectKeyID;
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


        if ( (isGrayListed & assetFlags) || (isBlocked & assetFlags) )
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
        if (isAllowListed & assetFlags)
        {
            _certificate_sha256_hash = [self getCertificateSHA256Hash];
            _auth_key_id = nil;
            if (isAnchor & assetFlags)
            {
                _auth_key_id = [self getAuthKeyIDString:certRef];
            }
        }
        else
        {
            _certificate_sha256_hash = nil;
            _auth_key_id = nil;
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
        CFDataRef temp_data = GetNormalizedIssuerContent(iosCertRef);

        if (NULL == temp_data)
        {
            CFStringRef name = SecCertificateCopySubjectSummary(cert_ref);
            NSLog(@"SecCertificateGetNormalizedIssuerContent returned NULL for %@", name);
            if (name)
                CFRelease(name);
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

- (NSString *)getAuthKeyIDString:(SecCertificateRef)cert_ref
{
    NSString *result = nil;

    SecCertificateRefP iosCertRef = NULL;
    NSData* authKey = NULL;
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
        CFDataRef temp_data = GetAuthorityKeyID(iosCertRef);

        if (NULL == temp_data)
        {
            CFStringRef name = SecCertificateCopySubjectSummary(cert_ref);
            NSLog(@"SecCertificateGetAuthorityKeyID returned NULL for %@", name);
            if (name)
                CFRelease(name);
            CFRelease(iosCertRef);
            return result;
        }
        authKey = [NSData dataWithBytes:CFDataGetBytePtr(temp_data) length:CFDataGetLength(temp_data)];
        //%%% debug-only code to verify output
        if (false) {
            NSString *str = [[authKey toHexString] uppercaseString];
            CFStringRef name = SecCertificateCopySubjectSummary(cert_ref);
            NSLog(@"AuthKeyID for %@ is %@", name, str);
            if (name)
                CFRelease(name);
        }
        //%%%
        CFRelease(iosCertRef);
    }

    if (authKey) {
        return [[authKey toHexString] uppercaseString];
    } else {
        return NULL;
    }
}

@end
