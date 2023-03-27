//
//  secseccodesignerapitest.cpp
//  Security
//

#include <stdio.h>
#include <AssertMacros.h>
#include <Security/SecCode.h>
#include <Security/SecStaticCode.h>
#include <Security/SecCodeSigner.h>
#include <Security/SecCodeSignerRemote.h>
#include <Security/SecIdentity.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecCertificateRequest.h>
#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecAccessControl.h>
#include <security_utilities/cfutilities.h>
#include <CoreFoundation/CoreFoundation.h>
#include <TargetConditionals.h>

#define BEGIN()                                             \
({                                                          \
    fprintf(stdout, "[BEGIN] %s\n", __FUNCTION__);          \
})

#define INFO(fmt, ...)                                      \
({                                                          \
    fprintf(stdout, fmt "\n", ##__VA_ARGS__);               \
})

#define PASS(fmt, ...)                                                      \
({                                                                          \
    fprintf(stdout, "[PASS] %s " fmt "\n\n", __FUNCTION__, ##__VA_ARGS__);  \
})

#define FAIL(fmt, ...)                                                      \
({                                                                          \
    fprintf(stdout, "[FAIL] %s " fmt "\n\n", __FUNCTION__, ##__VA_ARGS__);  \
})

#define kCommandRedirectOutputToDevNULL     " >/dev/null 2>&1"

#define kTemporaryPath                      "/tmp"
#define kSystemBinariesPath                 "/bin"
#define kAppleInternalApplicationsPath      "/AppleInternal/Applications/"

#define k_ls_BinaryName                     "ls"
#define k_ls_BinaryPath                     kSystemBinariesPath "/" k_ls_BinaryName
#define k_ls_TemporaryBinaryPath            kTemporaryPath "/" k_ls_BinaryName

// Bundle exists on both macOS and iOS.
#define kNullBundleName                     "Null.app"
#define kNullBundlePath                     kAppleInternalApplicationsPath "/" kNullBundleName
#define kTemporaryNullBundlePath            kTemporaryPath "/" kNullBundleName

static int
_copyPath(const char *dst, const char *src)
{
    std::string command = std::string("cp -r ") + src + " " + dst + " " + kCommandRedirectOutputToDevNULL;
    return system(command.c_str());
}

static int
_deletePath(const char *path)
{
    string command = std::string("rm -rf ") + path + " " + kCommandRedirectOutputToDevNULL;
    return system(command.c_str());
}

#if TARGET_OS_OSX
static int
_runCommand(const char *format, ...) __attribute__((format(printf, 1, 2)));

static int
_runCommand(const char *format, ...)
{
    va_list args;

    // Figure out how big we need to make a buffer.
    va_start(args, format);
    int calculatedSize = vsnprintf(NULL, 0, format, args);
    if (calculatedSize <= 0) {
        return -1;
    }
    // Add one for trailing null to end the string.
    calculatedSize += 1;
    va_end(args);

    // Fill in the buffer and run the command.
    va_start(args, format);
    auto commandBuffer = std::make_unique<char[]>(calculatedSize);
    vsnprintf(commandBuffer.get(), calculatedSize, format, args);
    va_end(args);

    return system(commandBuffer.get());
}
#endif

static SecStaticCodeRef
_createStaticCode(const char *path)
{
    CFRef<CFStringRef> stringRef = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)path, strlen(path), kCFStringEncodingUTF8, false);
    CFRef<CFURLRef> pathRef = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, stringRef, kCFURLPOSIXPathStyle, false);
    if (!pathRef) {
        INFO("Unable to create pathRef");
        return NULL;
    }

    CFRef<SecStaticCodeRef> codeRef = NULL;
    OSStatus status = SecStaticCodeCreateWithPath(pathRef, kSecCSDefaultFlags, codeRef.take());
    if (status != errSecSuccess) {
        INFO("Unable to create SecStaticCode: %d", status);
        return NULL;
    }

    return codeRef.yield();
}

static CFArrayRef
_getCertificateChain(SecIdentityRef identity)
{
    CFRef<SecCertificateRef> primaryCert = NULL;
    OSStatus status = SecIdentityCopyCertificate(identity, primaryCert.take());
    if (status != errSecSuccess) {
        INFO("Unable to copy certificate from identity: %d", status);
        return NULL;
    }

    CFRef<CFArrayRef> certificateChain = NULL;
    CFRef<SecPolicyRef> policy = NULL;
    CFRef<SecTrustRef> trust = NULL;
    CFRef<CFMutableArrayRef> wrappedCert = NULL;

    policy = SecPolicyCreateBasicX509();
    if (!policy) {
        INFO("Unable to create basic x509 policy");
        return NULL;
    }

    wrappedCert.take(CFArrayCreateMutable(NULL, 3, NULL));
    CFArraySetValueAtIndex(wrappedCert, 0, primaryCert);

    if (SecTrustCreateWithCertificates(wrappedCert, policy, trust.take())) {
        INFO("Unable to create trust with certificates.");
        return NULL;
    }

    SecTrustResultType result;
    status = SecTrustEvaluate(trust, &result);
    if (status != errSecSuccess) {
        INFO("Unable to create trust with certificates.");
        return NULL;
    }

    certificateChain.take(SecTrustCopyCertificateChain(trust));
    return certificateChain.yield();
}

static SecKeyRef
_createAsymmetricKey(const char *name, CFStringRef keyType, CFNumberRef keySize, CFBooleanRef makePermanent = kCFBooleanFalse)
{
    CFRef<CFStringRef> label = NULL;
    CFRef<CFMutableDictionaryRef> parameters = NULL;
    CFRef<SecKeyRef> privateKey = NULL;
    CFRef<SecAccessControlRef> accessControl = NULL;

    label.take(CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)name, strlen(name), kCFStringEncodingUTF8, false));
    require_string(label, exit, "Unable to create label string");

    accessControl.take(SecAccessControlCreateWithFlags(kCFAllocatorDefault,
                                                    kSecAttrAccessibleAfterFirstUnlock,
                                                    0,
                                                    NULL));
    require_string(accessControl, exit, "Unable to create accessControl reference");

    parameters.take(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    require_string(parameters, exit, "Unable to create parameters dictionary");

    // Required parameters for creating a key.
    CFDictionaryAddValue(parameters, kSecAttrKeyType, keyType);
    CFDictionaryAddValue(parameters, kSecAttrKeySizeInBits, keySize);

    // Optional parameters.
    CFDictionaryAddValue(parameters, kSecAttrLabel, label);
    CFDictionaryAddValue(parameters, kSecAttrIsPermanent, makePermanent);
    CFDictionaryAddValue(parameters, kSecAttrAccessControl, accessControl);
    CFDictionaryAddValue(parameters, kSecUseDataProtectionKeychain, kCFBooleanTrue);

    privateKey.take(SecKeyCreateRandomKey(parameters, NULL));
    require_string(privateKey, exit, "Unable to create SecKeyRef");

exit:
    return privateKey.yield();
}

static SecKeyRef
_createRSAKey(const char *name, CFBooleanRef makePermanent = kCFBooleanFalse)
{
    const int keySize = 2048;
    CFRef<CFNumberRef> keySizeInBits = NULL;
    CFRef<SecKeyRef> privateKey = NULL;

    keySizeInBits.take(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &keySize));
    require_string(keySizeInBits, exit, "Unable to create keySizeInBits number");

    privateKey.take(_createAsymmetricKey(name, kSecAttrKeyTypeRSA, keySizeInBits, makePermanent));
    require_string(privateKey, exit, "Unable to create kSecAttrKeyTypeRSA SecKeyRef");

exit:
    return privateKey.yield();
}

static SecKeyRef
_createECCKey(const char *name, CFBooleanRef makePermanent = kCFBooleanFalse)
{
    const int keySize = 384;
    CFRef<CFNumberRef> keySizeInBits = NULL;
    CFRef<SecKeyRef> privateKey = NULL;

    keySizeInBits.take(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &keySize));
    require_string(keySizeInBits, exit, "Unable to create keySizeInBits number");

    privateKey.take(_createAsymmetricKey(name, kSecAttrKeyTypeECSECPrimeRandom, keySizeInBits, makePermanent));
    require_string(privateKey, exit, "Unable to create kSecAttrKeyTypeECSECPrimeRandom SecKeyRef");

exit:
    return privateKey.yield();
}

static SecKeyRef
_findKey(const char *name)
{
    OSStatus status;
    CFRef<CFStringRef> label = NULL;
    CFRef<CFMutableDictionaryRef> query = NULL;
    CFRef<SecKeyRef> key = NULL;

    label.take(CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)name, strlen(name), kCFStringEncodingUTF8, false));
    require_string(label, exit, "Unable to create label CFString");

    query.take(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    require_string(query, exit, "Unable to create query CFDictionary");

    CFDictionaryAddValue(query, kSecClass, kSecClassKey);
    CFDictionaryAddValue(query, kSecReturnRef, kCFBooleanTrue);
    CFDictionaryAddValue(query, kSecAttrLabel, label);
    CFDictionaryAddValue(query, kSecMatchLimit, kSecMatchLimitOne);
    CFDictionaryAddValue(query, kSecUseDataProtectionKeychain, kCFBooleanTrue);

    // We're looking for the private key.
    CFDictionaryAddValue(query, kSecAttrCanSign, kCFBooleanTrue);

    status = SecItemCopyMatching(query, (CFTypeRef*)key.take());
    if (status != errSecSuccess) {
        INFO("Unable to query keychain for key (%s): %d", name, status);
        goto exit;
    }

exit:
    return key.yield();
}

static SecCertificateRef
_createSelfSignedCertificate(const char *name, SecKeyRef privateKey)
{
    const CFStringRef kSecOIDExtendedKeyUsage = CFSTR("2.5.29.37");
    const uint8_t EKUCodeSigingEncodedData[] = {0x30, 0x0a, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x03};
    const int keyUsage = kSecKeyUsageAll;

    CFRef<CFStringRef> commonName = NULL;
    CFRef<CFDataRef> EKUCodeSigningEncoded = NULL;
    CFRef<CFNumberRef> keyUsageNumber = NULL;
    CFRef<CFMutableDictionaryRef> parameters = NULL;
    CFRef<CFMutableDictionaryRef> extensions = NULL;
    CFRef<CFArrayRef> subject = NULL;
    CFRef<CFArrayRef> encapsulatedSubject = NULL;
    CFRef<CFArrayRef> subjectCommonName = NULL;
    CFRef<SecCertificateRef> certificate = NULL;
    CFRef<SecKeyRef> publicKey = NULL;

    commonName.take(CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)name, strlen(name), kCFStringEncodingUTF8, false));
    if (!commonName) {
        INFO("Unable to create commonName string");
        return NULL;
    }
    const void *commonNameArray[] = {kSecOidCommonName, commonName};

    keyUsageNumber.take(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &keyUsage));
    require_string(keyUsageNumber, exit, "Unable to create keyUsageNumber number");

    // Add code signing OID extension.
    EKUCodeSigningEncoded.take(CFDataCreate(kCFAllocatorDefault, EKUCodeSigingEncodedData, sizeof(EKUCodeSigingEncodedData)));
    require_string(EKUCodeSigningEncoded, exit, "Unable to create EKU code signing data");

    extensions.take(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    require_string(extensions, exit, "Unable to create extensions dictionary");
    CFDictionaryAddValue(extensions, kSecOIDExtendedKeyUsage, EKUCodeSigningEncoded);

    parameters.take(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    require_string(parameters, exit, "Unable to create parameters dictionary");
    CFDictionaryAddValue(parameters, kSecCertificateKeyUsage, keyUsageNumber);
    CFDictionaryAddValue(parameters, kSecCertificateExtensionsEncoded, extensions);

    subjectCommonName.take(CFArrayCreate(kCFAllocatorDefault, commonNameArray, 2, NULL));
    require(subjectCommonName, exit);
    encapsulatedSubject.take(CFArrayCreate(kCFAllocatorDefault, (const void **)&subjectCommonName, 1, NULL));
    require(encapsulatedSubject, exit);
    subject.take(CFArrayCreate(kCFAllocatorDefault, (const void **)&encapsulatedSubject, 1, NULL));
    require(subject, exit);

    publicKey.take(SecKeyCopyPublicKey(privateKey));
    certificate.take(SecGenerateSelfSignedCertificate(subject, parameters, publicKey, privateKey));
    require_string(certificate, exit, "Unable to create SecCertificateRef");

exit:
    return certificate.yield();
}

static int
_addSecItem(CFStringRef keyClass, CFTypeRef item, const char *labelPtr)
{
    int ret = -1;
    OSStatus status;
    CFRef<CFStringRef> label = NULL;
    CFRef<CFMutableDictionaryRef> attributes = NULL;

    label.take(CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)labelPtr, strlen(labelPtr), kCFStringEncodingUTF8, false));
    require_string(label, exit, "Unable to create label CFString");

    attributes.take(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    require_string(attributes, exit, "Unable to create attributes CFDictionary");

    CFDictionaryAddValue(attributes, kSecClass, keyClass);
    CFDictionaryAddValue(attributes, kSecValueRef, item);
    CFDictionaryAddValue(attributes, kSecAttrLabel, label);
    CFDictionaryAddValue(attributes, kSecUseDataProtectionKeychain, kCFBooleanTrue);

    status = SecItemAdd(attributes, NULL);
    if (status != errSecSuccess) {
        INFO("Unable to add item to keychain: %d", status);
        goto exit;
    }
    ret = 0;

exit:
    return ret;
}

static int
_deleteSecItem(CFStringRef keyClass, const char *labelPtr)
{
    int ret = -1;
    OSStatus status;
    CFRef<CFStringRef> label = NULL;
    CFRef<CFMutableDictionaryRef> query = NULL;

    label.take(CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)labelPtr, strlen(labelPtr), kCFStringEncodingUTF8, false));
    require_string(label, exit, "Unable to create label CFString");

    query.take(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    require_string(query, exit, "Unable to create query CFDictionary");

    CFDictionaryAddValue(query, kSecClass, keyClass);
    CFDictionaryAddValue(query, kSecAttrLabel, label);
    CFDictionaryAddValue(query, kSecUseDataProtectionKeychain, kCFBooleanTrue);

    status = SecItemDelete(query);
    if (status != errSecSuccess) {
        INFO("Unable to delete SecItem (%s): %d", labelPtr, status);
        goto exit;
    }
    ret = 0;

exit:
    return ret;
}

static int
_checkSignatureValidity(const char *path, SecCSFlags verificationFlags, bool allowUntrusted = false)
{
    int ret = -1;
    OSStatus status;
    CFRef<SecStaticCodeRef> codeRef = NULL;

    codeRef.take(_createStaticCode(path));
    require_string(codeRef, exit, "Unable to create codeRef SecStaticCode");

    status = SecStaticCodeCheckValidity(codeRef, verificationFlags, NULL);
    if (status != errSecSuccess) {
        if (status == errSecCSSignatureUntrusted && allowUntrusted) {
            ret = 0;
            goto exit;
        }

        INFO("Unable to verify signature through SecStaticCodeCheckValidity: %d", status);
        goto exit;
    }
    ret = 0;

exit:
    return ret;
}

static int
_removeSignature(const char *path)
{
    int ret = -1;
    OSStatus status;
    CFRef<SecCodeSignerRef> signerRef = NULL;
    CFRef<SecStaticCodeRef> codeRef = NULL;
    CFRef<CFMutableDictionaryRef> parameters = NULL;
    CFRef<CFDictionaryRef> signingInfo = NULL;

    parameters.take(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    status = SecCodeSignerCreate(parameters, kSecCSRemoveSignature, signerRef.take());
    if (status != errSecSuccess) {
        INFO("Unable to create SecCodeSigner: %d", status);
        goto exit;
    }

    codeRef.take(_createStaticCode(path));
    require_string(codeRef, exit, "Unable to create SecStaticCode");

    // Because of the flags used to setup signerRef, this will actually remove
    // the signature.
    status = SecCodeSignerAddSignature(signerRef, codeRef, kSecCSDefaultFlags);
    if (status != errSecSuccess) {
        INFO("Error on removing signature through SecCodeSignerAddSignature: %d", status);
        goto exit;
    }

    status = SecCodeCopySigningInformation(codeRef, kSecCSDefaultFlags, signingInfo.take());
    if (status != errSecSuccess) {
        INFO("Error on acquiring signing information through SecCodeCopySigningInformation: %d", status);
        goto exit;
    }

    if (CFDictionaryGetValue(signingInfo, kSecCodeInfoIdentifier)) {
        INFO("%s is still signed", path);
        goto exit;
    }
    ret = 0;

exit:
    return ret;
}

static int
_forceAddSignature(const char *path, const char *ident, SecIdentityRef identity)
{
    int ret = -1;
    OSStatus status;
    CFRef<SecCodeSignerRef> signerRef = NULL;
    CFRef<SecStaticCodeRef> codeRef = NULL;
    CFRef<CFMutableDictionaryRef> parameters = NULL;
    CFRef<CFStringRef> identifierRef = NULL;
    CFRef<CFDictionaryRef> signingInfo = NULL;
    CFRef<CFStringRef> signatureIdentifierRef = NULL;
    CFRef<CFDictionaryRef> lwcrRef = NULL;

    parameters.take(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    if (identity == NULL) {
        // This is how we do adhoc signing.
        CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, SecIdentityRef(kCFNull));
    } else {
        CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, identity);
    }

    // Force an identifier on this, so we can validate later.
    identifierRef.take(CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)ident, strlen(ident), kCFStringEncodingUTF8, false));
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentifier, identifierRef);

    // If we didn't want to force, then we need to add kSecCSSignPreserveSignature.
    status = SecCodeSignerCreate(parameters, kSecCSDefaultFlags, signerRef.take());
    if (status != errSecSuccess) {
        INFO("Unable to create SecCodeSigner: %d", status);
        return -1;
    }

    codeRef.take(_createStaticCode(path));
    require_string(codeRef, exit, "Unable to create SecStaticCode");

    status = SecCodeSignerAddSignature(signerRef, codeRef, kSecCSDefaultFlags);
    if (status != errSecSuccess) {
        INFO("Error on adding signature through SecCodeSignerAddSignature: %d", status);
        goto exit;
    }

    status = SecCodeCopySigningInformation(codeRef, kSecCSRequirementInformation, signingInfo.take());
    if (status != errSecSuccess) {
        INFO("Error on acquiring signing information through SecCodeCopySigningInformation: %d", status);
        goto exit;
    }

    // Don't use .take since we want we add a CFRetain on it.
    signatureIdentifierRef = (CFStringRef)CFDictionaryGetValue(signingInfo, kSecCodeInfoIdentifier);
    if (!signatureIdentifierRef) {
        INFO("No kSecCodeInfoIdentifier on %s", path);
        goto exit;
    }

    if (CFStringCompare(signatureIdentifierRef, identifierRef, 0) != kCFCompareEqualTo) {
        INFO("Forced signature identifier mismatch: %s", CFStringGetCStringPtr(signatureIdentifierRef, kCFStringEncodingUTF8));
        goto exit;
    }

    lwcrRef = (CFDictionaryRef)CFDictionaryGetValue(signingInfo, kSecCodeInfoDefaultDesignatedLightweightCodeRequirement);
    if (!lwcrRef) {
        INFO("No kSecCodeInfoDefaultDesignatedLightweightCodeRequirement on %s", path);
        goto exit;
    }
    ret = 0;

exit:
    return ret;
}

static int
_forceAddSignatureRemote(const char *path, const char *ident, SecIdentityRef identity)
{
    int ret = -1;
    OSStatus status;
    CFRef<SecCodeSignerRemoteRef> signerRef = NULL;
    CFRef<SecStaticCodeRef> codeRef = NULL;
    CFRef<CFMutableDictionaryRef> parameters = NULL;
    CFRef<CFStringRef> identifierRef = NULL;
    CFRef<CFDictionaryRef> signingInfo = NULL;
    CFRef<CFStringRef> signatureIdentifierRef = NULL;
    CFRef<CFDictionaryRef> lwcrRef = NULL;

    parameters.take(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    // Force an identifier on this, so we can validate later.
    identifierRef.take(CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)ident, strlen(ident), kCFStringEncodingUTF8, false));
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentifier, identifierRef);

    // If we didn't want to force, then we need to add kSecCSSignPreserveSignature.
    CFArrayRef certChain = _getCertificateChain(identity);
    status = SecCodeSignerRemoteCreate(parameters, certChain, kSecCSDefaultFlags, signerRef.take(), NULL);
    if (status != errSecSuccess) {
        INFO("Unable to create SecCodeSigner: %d", status);
        return -1;
    }

    codeRef.take(_createStaticCode(path));
    require_string(codeRef, exit, "Unable to create SecStaticCode");

    status = SecCodeSignerRemoteAddSignature(signerRef, codeRef, kSecCSDefaultFlags, ^CFDataRef(CFDataRef cmsDigestHash, SecCSDigestAlgorithm digestAlgo) {
        // We only support SHA256 digest algorithm so make sure thats right.
        if (digestAlgo != kSecCodeSignatureHashSHA256) {
            INFO("Called with incorrect digest algorithm type: %d", digestAlgo);
            return NULL;
        }

        // Perform local signing based on the identity passed in above.
        SecKeyRef privateKey = NULL;
        OSStatus localStatus = SecIdentityCopyPrivateKey(identity, &privateKey);
        if (localStatus != errSecSuccess) {
            INFO("Unable to copy private key from identity: %d", localStatus);
            return NULL;
        }
        CFErrorRef localCFError = NULL;
        CFDataRef sig = SecKeyCreateSignature(privateKey, kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256, cmsDigestHash, &localCFError);
        if (localCFError != NULL) {
            CFRef<CFStringRef> errorString;
            errorString.take(CFErrorCopyDescription(localCFError));
            INFO("Unable to create perform local signature: %s", CFStringGetCStringPtr(errorString.get(), kCFStringEncodingUTF8));
        }
        return sig;
    }, NULL);
    if (status != errSecSuccess) {
        INFO("Error on adding signature through SecCodeSignerAddSignature: %d", status);
        goto exit;
    }

    status = SecCodeCopySigningInformation(codeRef, kSecCSRequirementInformation, signingInfo.take());
    if (status != errSecSuccess) {
        INFO("Error on acquiring signing information through SecCodeCopySigningInformation: %d", status);
        goto exit;
    }

    // Don't use .take since we want we add a CFRetain on it.
    signatureIdentifierRef = (CFStringRef)CFDictionaryGetValue(signingInfo, kSecCodeInfoIdentifier);
    if (!signatureIdentifierRef) {
        INFO("No kSecCodeInfoIdentifier on %s", path);
        goto exit;
    }

    if (CFStringCompare(signatureIdentifierRef, identifierRef, 0) != kCFCompareEqualTo) {
        INFO("Forced signature identifier mismatch: %s", CFStringGetCStringPtr(signatureIdentifierRef, kCFStringEncodingUTF8));
        goto exit;
    }

    lwcrRef = (CFDictionaryRef)CFDictionaryGetValue(signingInfo, kSecCodeInfoDefaultDesignatedLightweightCodeRequirement);
    if (!lwcrRef) {
        INFO("No kSecCodeInfoDefaultDesignatedLightweightCodeRequirement on %s", path);
        goto exit;
    }
    ret = 0;

exit:
    return ret;
}

static int
CheckRemoveSignatureMachO(void)
{
    BEGIN();

    const char *path = k_ls_TemporaryBinaryPath;
    const char *copyPath = k_ls_BinaryPath;
    int ret = -1;

    if (_copyPath(path, copyPath)) {
        FAIL("Unable to create temporary path (%s)", path);
        goto exit;
    }

    ret = _removeSignature(path);
    if (ret) {
        FAIL("Unable to remove signature from %s", path);
        goto exit;
    }

    PASS("Successfully removed signature from %s", path);
    ret = 0;

exit:
    _deletePath(path);
    return ret;
}

static int
CheckAddAdhocSignatureMachO(void)
{
    BEGIN();

    const char *path = k_ls_TemporaryBinaryPath;
    const char *copyPath = k_ls_BinaryPath;
    int ret = -1;

    if (_copyPath(path, copyPath)) {
        FAIL("Unable to create temporary path (%s)", path);
        goto exit;
    }

    ret = _forceAddSignature(path, "MachOIdentity", NULL);
    if (ret) {
        FAIL("Unable to add adhoc signature to %s", path);
        goto exit;
    }

    PASS("Successfully added adhoc signature to %s", path);
    ret = 0;

exit:
    _deletePath(path);
    return ret;
}

static int
CheckAddRSASignatureMachO(void)
{
    BEGIN();

    const char *path = k_ls_TemporaryBinaryPath;
    const char *copyPath = k_ls_BinaryPath;

    int ret = -1;
    CFRef<SecKeyRef> privateKey = NULL;
    CFRef<SecCertificateRef> selfSignedCert = NULL;
    CFRef<SecIdentityRef> selfSignedIdentity = NULL;

    privateKey.take(_createRSAKey("Test RSA Key"));
    if (!privateKey) {
        FAIL("Unable to create an RSA key pair");
        goto exit;
    }

    selfSignedCert.take(_createSelfSignedCertificate("Test Self-Signed Certificate", privateKey));
    if (!selfSignedCert) {
        FAIL("Unable to create a self signed certificate");
        goto exit;
    }

    selfSignedIdentity.take(SecIdentityCreate(kCFAllocatorDefault, selfSignedCert, privateKey));
    if (!selfSignedIdentity) {
        FAIL("Unable to create a self signed identity");
        goto exit;
    }

    if (_copyPath(path, copyPath)) {
        FAIL("Unable to create temporary path (%s)", path);
        goto exit;
    }

    ret = _forceAddSignature(path, "MachOIdentity", selfSignedIdentity);
    if (ret) {
        FAIL("Unable to add RSA CMS signature to %s", path);
        goto exit;
    }

    PASS("Successfully added RSA CMS signature to %s", path);
    ret = 0;

exit:
    _deletePath(path);
    return ret;
}

static int
CheckRemoteSignatureAPI(void)
{
    BEGIN();

    int ret = -1;
    OSStatus status = 0;
    const char *path = k_ls_TemporaryBinaryPath;
    const char *copyPath = k_ls_BinaryPath;

    CFRef<SecCodeSignerRemoteRef> signerRef = NULL;
    CFRef<SecStaticCodeRef> codeRef = NULL;
    CFRef<CFMutableDictionaryRef> parameters = NULL;
    CFRef<CFArrayRef> certChain = NULL;
    CFRef<CFMutableArrayRef> mutableCertChain = NULL;

    // Set up a binary to use.
    if (_copyPath(path, copyPath)) {
        FAIL("Unable to create temporary path (%s)", path);
        goto exit;
    }

    parameters.take(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    // Not able to create a remote signer with no cert chain.
    status = SecCodeSignerRemoteCreate(parameters, NULL, kSecCSDefaultFlags, signerRef.take(), NULL);
    if (status != errSecCSInvalidObjectRef) {
        INFO("SecCodeSignerRemoteCreate didn't fail with appropriate error with NULL cert chain: %d", status);
        goto exit;
    }

    // Not able to create a remote signer with an empty cert chain.
    certChain.take(CFArrayCreate(NULL, NULL, 0, NULL));
    status = SecCodeSignerRemoteCreate(parameters, certChain.get(), kSecCSDefaultFlags, signerRef.take(), NULL);
    if (status != errSecCSInvalidObjectRef) {
        INFO("SecCodeSignerRemoteCreate didn't fail with appropriate error with empty cert chain: %d", status);
        goto exit;
    }

    // Not able to create a remote signer with a cert chain array of non-SecCertificate objects.
    mutableCertChain.take(CFArrayCreateMutable(NULL, 1, NULL));
    CFArrayAppendValue(mutableCertChain.get(), CFSTR("a string"));
    status = SecCodeSignerRemoteCreate(parameters, mutableCertChain.get(), kSecCSDefaultFlags, signerRef.take(), NULL);
    if (status != errSecCSInvalidObjectRef) {
        INFO("SecCodeSignerRemoteCreate didn't fail with appropriate error with empty cert chain: %d", status);
        goto exit;
    }

    // Explicitly disallowed flags: kSecCSEditSignature, kSecCSRemoveSignature, kSecCSSignNestedCode
    status = SecCodeSignerRemoteCreate(parameters, certChain.get(), kSecCSEditSignature, signerRef.take(), NULL);
    if (status != errSecCSInvalidFlags) {
        INFO("SecCodeSignerRemoteCreate didn't fail with unexpected flag kSecCSEditSignature: %d", status);
        goto exit;
    }

    status = SecCodeSignerRemoteCreate(parameters, certChain.get(), kSecCSRemoveSignature, signerRef.take(), NULL);
    if (status != errSecCSInvalidFlags) {
        INFO("SecCodeSignerRemoteCreate didn't fail with unexpected flag kSecCSRemoveSignature: %d", status);
        goto exit;
    }

    status = SecCodeSignerRemoteCreate(parameters, certChain.get(), kSecCSSignNestedCode, signerRef.take(), NULL);
    if (status != errSecCSInvalidFlags) {
        INFO("SecCodeSignerRemoteCreate didn't fail with unexpected flag kSecCSSignNestedCode: %d", status);
        goto exit;
    }

    PASS("Successfully tested SecCodeSignerRemote API boundary");
    ret = 0;

exit:
    _deletePath(path);
    return ret;
}

static int
CheckAddSignatureRemoteMachO(void)
{
    BEGIN();

    const char *path = k_ls_TemporaryBinaryPath;
    const char *copyPath = k_ls_BinaryPath;

    int ret = -1;
    CFRef<SecKeyRef> privateKey = NULL;
    CFRef<SecCertificateRef> selfSignedCert = NULL;
    CFRef<SecIdentityRef> selfSignedIdentity = NULL;
    CFRef<SecCodeSignerRemoteRef> remoteSigner = NULL;

    privateKey.take(_createRSAKey("Test RSA Key"));
    if (!privateKey) {
        FAIL("Unable to create an RSA key pair");
        goto exit;
    }

    selfSignedCert.take(_createSelfSignedCertificate("Test Self-Signed Certificate", privateKey));
    if (!selfSignedCert) {
        FAIL("Unable to create a self signed certificate");
        goto exit;
    }

    selfSignedIdentity.take(SecIdentityCreate(kCFAllocatorDefault, selfSignedCert, privateKey));
    if (!selfSignedIdentity) {
        FAIL("Unable to create a self signed identity");
        goto exit;
    }

    if (_copyPath(path, copyPath)) {
        FAIL("Unable to create temporary path (%s)", path);
        goto exit;
    }

    ret = _forceAddSignatureRemote(path, "MachOIdentity", selfSignedIdentity);
    if (ret) {
        FAIL("Unable to add RSA CMS signature to %s", path);
        goto exit;
    }

    PASS("Successfully added RSA CMS signature to %s", path);
    ret = 0;

exit:
    _deletePath(path);
    return ret;
}

static int
CheckAddSignatureRemoteBundle(void)
{
    BEGIN();

    const char *path = kTemporaryNullBundlePath;
    const char *copyPath = kNullBundlePath;

    int ret = -1;
    CFRef<SecKeyRef> privateKey = NULL;
    CFRef<SecCertificateRef> selfSignedCert = NULL;
    CFRef<SecIdentityRef> selfSignedIdentity = NULL;
    CFRef<SecCodeSignerRemoteRef> remoteSigner = NULL;

    privateKey.take(_createRSAKey("Test RSA Key"));
    if (!privateKey) {
        FAIL("Unable to create an RSA key pair");
        goto exit;
    }

    selfSignedCert.take(_createSelfSignedCertificate("Test Self-Signed Certificate", privateKey));
    if (!selfSignedCert) {
        FAIL("Unable to create a self signed certificate");
        goto exit;
    }

    selfSignedIdentity.take(SecIdentityCreate(kCFAllocatorDefault, selfSignedCert, privateKey));
    if (!selfSignedIdentity) {
        FAIL("Unable to create a self signed identity");
        goto exit;
    }

    if (_copyPath(path, copyPath)) {
        FAIL("Unable to create temporary path (%s)", path);
        goto exit;
    }

    ret = _forceAddSignatureRemote(path, "BundleIdentity", selfSignedIdentity);
    if (ret) {
        FAIL("Unable to add RSA CMS signature to %s", path);
        goto exit;
    }

    PASS("Successfully added RSA CMS signature to %s", path);
    ret = 0;

exit:
    _deletePath(path);
    return ret;
}

static int
CheckRemoveSignatureBundle(void)
{
    BEGIN();

    const char *path = kTemporaryNullBundlePath;
    const char *copyPath = kNullBundlePath;
    int ret = -1;

    if (_copyPath(path, copyPath)) {
        FAIL("Unable to create temporary path (%s)", path);
        goto exit;
    }

    ret = _removeSignature(path);
    if (ret) {
        FAIL("Unable to remove signature from %s", path);
        goto exit;
    }

    PASS("Successfully removed signature from %s", path);
    ret = 0;

exit:
    _deletePath(path);
    return ret;
}

static int
CheckAddAdhocSignatureBundle(void)
{
    BEGIN();

    const char *path = kTemporaryNullBundlePath;
    const char *copyPath = kNullBundlePath;
    int ret = -1;

    if (_copyPath(path, copyPath)) {
        FAIL("Unable to create temporary path (%s)", path);
        goto exit;
    }

    ret = _forceAddSignature(path, "BundleIdentity", NULL);
    if (ret) {
        FAIL("Unable to add adhoc signature to %s", path);
        goto exit;
    }

    PASS("Successfully added adhoc signature to %s", path);
    ret = 0;

exit:
    _deletePath(path);
    return ret;
}

static int
CheckAddECCSignatureBundle(void)
{
    BEGIN();

    const char *path = kTemporaryNullBundlePath;
    const char *copyPath = kNullBundlePath;

    int ret = -1;
    CFRef<SecKeyRef> privateKey = NULL;
    CFRef<SecCertificateRef> selfSignedCert = NULL;
    CFRef<SecIdentityRef> selfSignedIdentity = NULL;

    privateKey.take(_createECCKey("Test ECC Key"));
    if (!privateKey) {
        FAIL("Unable to create an ECC key pair");
        goto exit;
    }

    selfSignedCert.take(_createSelfSignedCertificate("Test Self-Signed Certificate", privateKey));
    if (!selfSignedCert) {
        FAIL("Unable to create a self signed certificate");
        goto exit;
    }

    selfSignedIdentity.take(SecIdentityCreate(kCFAllocatorDefault, selfSignedCert, privateKey));
    if (!selfSignedIdentity) {
        FAIL("Unable to create a self signed identity");
        goto exit;
    }

    if (_copyPath(path, copyPath)) {
        FAIL("Unable to create temporary path (%s)", path);
        goto exit;
    }

    ret = _forceAddSignature(path, "BundleIdentity", selfSignedIdentity);
    if (ret) {
        FAIL("Unable to add ECC CMS signature to %s", path);
        goto exit;
    }

    PASS("Successfully added ECC CMS signature to %s", path);
    ret = 0;

exit:
    _deletePath(path);
    return ret;
}

/*
 * This is a similar test to the ECC bundle signing, but integrates more components
 * together.
 *
 * - We create a test ECC private key which we store in the keychain.
 * - We create a self signed certificate using this private key and store in the keychain.
 * - We create a SecIdentity using the keychain APIs, and then sign with it.
 * - We validate the signature through SecStaticCodeCheckValidity.
 */
static int
CheckECCKeychainAndSignatureValidationIntegrationBundle(void)
{
    BEGIN();

    const char *keyName = "Test ECC Key";
    const char *certName = "Test Self-Signed Certificate";
    const char *path = kTemporaryNullBundlePath;
    const char *copyPath = kNullBundlePath;

    int ret = -1;
    bool oniOS = false;
    CFRef<SecKeyRef> privateKey = NULL;
    CFRef<SecCertificateRef> selfSignedCert = NULL;
    CFRef<SecIdentityRef> selfSignedIdentity = NULL;

#if TARGET_OS_IOS
    oniOS = true;
#endif

    // Passing the kCFBooleanTrue makes the key permanent in the keychain.
    privateKey = _createECCKey(keyName, kCFBooleanTrue);
    if (!privateKey) {
        FAIL("Unable to create an ECC key pair");
        goto exit;
    }
    privateKey.release();

    // Search for key to confirm storage in the keychain.
    privateKey.take(_findKey(keyName));
    if (!privateKey) {
        FAIL("Unable to find ECC key");
        goto exit;
    }

    selfSignedCert.take(_createSelfSignedCertificate(certName, privateKey));
    if (!selfSignedCert) {
        FAIL("Unable to create a self signed certificate");
        goto exit;
    }

    if(_addSecItem(kSecClassCertificate, selfSignedCert, certName)) {
        FAIL("Unable to store self-signed certificate in the keychain");
        goto exit;
    }

    selfSignedIdentity.take(SecIdentityCreate(kCFAllocatorDefault, selfSignedCert, privateKey));
    if (!selfSignedIdentity) {
        FAIL("Unable to create a self-signed identity");
        goto exit;
    }

    if (_copyPath(path, copyPath)) {
        FAIL("Unable to create temporary path (%s)", path);
        goto exit;
    }

    ret = _forceAddSignature(path, "BundleIdentity", selfSignedIdentity);
    if (ret) {
        FAIL("Unable to add ECC CMS signature to %s", path);
        goto exit;
    }

    ret = _checkSignatureValidity(path, kSecCSDefaultFlags, oniOS? true: false);
    if (ret) {
        FAIL("Unable to confirm signature validity of: %s", path);
        goto exit;
    }

    PASS("Successfully confirmed keychain and signature validity integration of ECC key on: %s", path);
    ret = 0;

exit:
    _deleteSecItem(kSecClassCertificate, certName);
    _deleteSecItem(kSecClassKey, keyName);
    _deletePath(path);
    return ret;
}

#if TARGET_OS_OSX
static int
CheckAddAdhocSignatureEncryptedDiskImage(void)
{
    int ret = 0;
    const char *testRootPath = "/tmp/EDI";
    const char *testContentRootPath = "/tmp/EDI/TestImageContent";
    const char *diskImagePath = "/tmp/EDI/test.dmg";

    BEGIN();

    // Create a test directory, and a simple content directory.
    _runCommand("mkdir -p %s", testContentRootPath);
    _runCommand("echo 'hello' > %s/hello.txt", testContentRootPath);

    // Create an encrypted disk image with a known password and then strip the
    // FinderInfo attribute that inevitably ends up on it.
    const char *const cmd = "hdiutil create -encryption 'AES-256' -passphrase %s -srcfolder %s %s";
    _runCommand(cmd, "helloworld", testContentRootPath, diskImagePath);
    _runCommand("xattr -c %s", diskImagePath);

    ret = _forceAddSignature(diskImagePath, "com.test.encrypted-disk-image", NULL);
    if (ret) {
        FAIL("Unable to add adhoc signature to %s", diskImagePath);
        goto exit;
    }

    // To ensure the security framework didn't fall back to using an xattr-based
    // signature, just strip the xattrs here before validating.
    _runCommand("xattr -c %s", diskImagePath);

    ret = _checkSignatureValidity(diskImagePath, kSecCSDefaultFlags);
    if (ret) {
        FAIL("Unable to add adhoc signature to %s", diskImagePath);
        goto exit;
    }

    PASS("Successfully added adhoc signature to %s", diskImagePath);

exit:
    _deletePath(testRootPath);
    return ret;
}
#endif

int main(void)
{
    fprintf(stdout, "[TEST] secseccodesignerapitest\n\n");

    int i;
    int (*testList[])(void) = {
        // MachO tests.
        CheckRemoveSignatureMachO,
        CheckAddAdhocSignatureMachO,
        CheckAddRSASignatureMachO,

        // Bundle tests.
        CheckRemoveSignatureBundle,
        CheckAddAdhocSignatureBundle,
        CheckAddECCSignatureBundle,
        CheckECCKeychainAndSignatureValidationIntegrationBundle,

        // Remote signing.
        CheckRemoteSignatureAPI,
        CheckAddSignatureRemoteMachO,
        CheckAddSignatureRemoteBundle,

        // Encrypted disk image tests - only supported on macOS
#if TARGET_OS_OSX
        CheckAddAdhocSignatureEncryptedDiskImage,
#endif
    };
    const int numberOfTests = sizeof(testList) / sizeof(*testList);
    int testResults[numberOfTests] = {0};

    for (i = 0; i < numberOfTests; i++) {
        testResults[i] = testList[i]();
    }

    fprintf(stdout, "[SUMMARY]\n");
    for (i = 0; i < numberOfTests; i++) {
        fprintf(stdout, "%d. %s\n", i+1, testResults[i] == 0 ? "Passed" : "Failed");
    }

    return 0;
}
