#include <OpenDirectory/OpenDirectory.h>
#include <Security/Security.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecTrustPriv.h>
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CommonCrypto/CommonDigest.h>

#include "scmatch_evaluation.h"

CFDataRef copyRandomData(size_t size)
{
    if (!size)
        return NULL;
    
    unsigned char buffer[size];
    int fd = open("/dev/random", O_RDONLY, 0);
    if (fd < 0)
        return NULL;
    
    size_t readlength = read(fd, buffer, size);
    close (fd);
    if (readlength == size)
        return CFDataCreate(kCFAllocatorDefault, buffer, size);
    
    return NULL;
}

OSStatus validateCertificate(SecCertificateRef certificate, SecKeychainRef keychain)
{
    OSStatus result = errSecInternalError;
    SecPolicyRef policy = SecPolicyCreateBasicX509();
    CFArrayRef certificates = NULL;
    CFDataRef hash = NULL;
    CFStringRef commonName = NULL;
    
    
    OSStatus status = SecCertificateCopyCommonName(certificate, &commonName);
    if (status != errSecSuccess)
        goto cleanup;
    
    // make the query as detailed as possible to avoid trust and revocation checking of multiple certificates
    const void *keys[] = { kSecClass, kSecReturnRef, kSecMatchLimit, kSecMatchValidOnDate, kSecMatchPolicy, kSecMatchTrustedOnly, kSecMatchValidOnDate, kSecMatchSubjectWholeString};
    const void *vals[] = { kSecClassCertificate, kCFBooleanTrue, kSecMatchLimitAll, kCFNull, policy, kCFBooleanTrue, kCFNull, commonName };
    
    CFDictionaryRef query = CFDictionaryCreate(kCFAllocatorDefault, keys, vals, sizeof(keys) / sizeof(keys[0]), NULL, NULL);
    if (!query)
        goto cleanup;
    
    // policy is used so additional trust test is performed in Security
    status = SecItemCopyMatching(query, (CFTypeRef*)&certificates);
    CFRelease(query);
    
    if (status != errSecSuccess)
    {
        result = status;
        goto cleanup;
    }
    
    if (CFGetTypeID(certificates) != CFArrayGetTypeID())
        goto cleanup;

    // now check if our wanted certificate is returned in the array of valid ones
    hash = SecCertificateCopyPublicKeySHA1Digest(certificate);
    for (CFIndex i = 0; i < CFArrayGetCount(certificates); ++i)
    {
        SecCertificateRef cert = (SecCertificateRef)CFArrayGetValueAtIndex(certificates, i);
        if (CFGetTypeID(cert) != SecCertificateGetTypeID())
            continue;
        CFDataRef checkedHash = SecCertificateCopyPublicKeySHA1Digest(cert);
        bool found = CFEqual(hash, checkedHash);
        CFReleaseSafe(checkedHash);
        if (found)
        {
            result = errSecSuccess;
            break;
        }
    }
    
cleanup:
    openpam_log(PAM_LOG_DEBUG, "validateCertificate completed with: %d", (int)result);
    CFReleaseSafe(commonName);
    CFReleaseSafe(policy);
    CFReleaseSafe(certificates);
    CFReleaseSafe(hash);
    
    return result;
}

OSStatus verifySmartCardSigning(SecKeyRef publicKey, SecKeyRef privateKey)
{
    if (!publicKey || !privateKey)
        return false;
    
    OSStatus result = errSecAuthFailed;
    CFDataRef randomData = NULL;
    size_t blockSize = SecKeyGetBlockSize(privateKey);
    unsigned char signature[blockSize];
    
    randomData = copyRandomData(CC_SHA1_DIGEST_LENGTH);
    size_t signature_len = blockSize;

    if (randomData != NULL)
    {
        result = SecKeyRawSign(privateKey, kSecPaddingPKCS1, CFDataGetBytePtr(randomData), CC_SHA1_DIGEST_LENGTH, signature, &signature_len);
        if (result == errSecSuccess && signature_len == blockSize)
        {
            result = SecKeyRawVerify(publicKey, kSecPaddingPKCS1, CFDataGetBytePtr(randomData), CC_SHA1_DIGEST_LENGTH, signature, blockSize);
        }
        CFRelease(randomData);
    }
    return result;
}

CFArrayRef copyCardIdentities()
{
    CFArrayRef smartCardKeychains  = NULL;
    CFTypeRef certificates = NULL;
    
    OSStatus status = SecKeychainCopyDomainSearchList(kSecPreferencesDomainDynamic, &smartCardKeychains);
    if (status != errSecSuccess)
        goto cleanup;
    
    const void *keys[] = { kSecClass, kSecReturnRef, kSecMatchLimit, kSecMatchSearchList };
    const void *vals[] = { kSecClassIdentity, kCFBooleanTrue, kSecMatchLimitAll, smartCardKeychains };
    CFDictionaryRef query = CFDictionaryCreate(kCFAllocatorDefault, keys, vals, sizeof(keys) / sizeof(keys[0]), NULL, NULL);
    if (!query)
        goto cleanup;
    
    status = SecItemCopyMatching(query, &certificates);
    CFRelease(query);
    
    if (status != errSecSuccess)
        goto cleanup;
    
cleanup:
    if (smartCardKeychains)
        CFRelease(smartCardKeychains);
    
    if(certificates == NULL)
        return NULL;
    
    if (CFGetTypeID(certificates) == CFArrayGetTypeID())
        return certificates;
    
    CFRelease(certificates);
    return NULL;
}

SecKeychainRef copySmartCardKeychainForUser(ODRecordRef odRecord, const char* username, SecIdentityRef* copiedIdentity)
{
    if (odRecord == NULL || username == NULL)
        return NULL;
    
    CFArrayRef identities = copyCardIdentities();
    if (identities)
    {
        SecKeychainRef result = copyHashMatchedKeychain(odRecord, identities, copiedIdentity);
        if (result == NULL)
            result = copyAttributeMatchedKeychain(odRecord, identities, copiedIdentity);

        if (copiedIdentity && *copiedIdentity)
            CFRetain(*copiedIdentity);
        
        CFRelease(identities);
        return result;
    }
    
    return NULL;
}