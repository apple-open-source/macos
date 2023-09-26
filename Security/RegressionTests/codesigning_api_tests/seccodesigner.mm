//
//  secseccodesignerapitest.cpp
//  Security
//

#include <Foundation/Foundation.h>
#undef verify
#undef require
#undef require_string

#include <Security/SecCode.h>
#include <Security/SecCodePriv.h>
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
#include <security_codesigning/codedirectory.h>
#include <security_utilities/cfutilities.h>
#include <security_utilities/cfmunge.h>
#include <sys/xattr.h>
#include <TargetConditionals.h>
#include <TLE/TLE.h>

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

#define require(cond, label) \
do { \
    if(!(cond)) { \
        goto label; \
    } \
} while (0)


#define require_string(cond, label, info) \
do { \
    if(!(cond)) { \
        INFO("%s", info); \
        goto label; \
    } \
} while (0)

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

// HFS disk image paths
#define kHFSDiskImageFileDirectory    "/tmp"
#define kHFSDiskImageFileName         "SecurityAPITestHFS.dmg"
#define kHFSDiskImageFilePath         kHFSDiskImageFileDirectory "/" kHFSDiskImageFileName

#define kHFSDiskImageVolumeDirectory  "/Volumes"
#define kHFSDiskImageVolumeName       "SEC_TEST_HFS"
#define kHFSDiskImageVolumePath       kHFSDiskImageVolumeDirectory "/" kHFSDiskImageVolumeName
#define kHFSVolumeNullBundlePath      kHFSDiskImageVolumePath "/" kNullBundleName

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

static void
_cleanUpHFSDiskImage(void)
{
    _runCommand("rm -rf " kHFSDiskImageFilePath);
    _runCommand("hdiutil detach " kHFSDiskImageVolumePath);
}

static int
_createHFSDiskImage(void)
{
    return _runCommand("hdiutil create -fs HFS+ -size 256m -volname " kHFSDiskImageVolumeName " " kHFSDiskImageFilePath);
}

static int
_attachHFSDiskImage(void)
{
    return _runCommand("hdiutil attach " kHFSDiskImageFilePath);
}
#endif

static SecStaticCodeRef
_createStaticCode(const char *path)
{
    NSURL* url = [NSURL fileURLWithFileSystemRepresentation:path isDirectory:NO relativeToURL:nil];

    SecStaticCodeRef codeRef = NULL;
    OSStatus status = SecStaticCodeCreateWithPath((__bridge CFURLRef)url, kSecCSDefaultFlags, &codeRef);
    if (status != errSecSuccess) {
        INFO("Unable to create SecStaticCode: %d", status);
        return NULL;
    }

    return codeRef;
}

static SecStaticCodeRef
_createStaticCodeForArchitecture(const char *path, cpu_type_t type, cpu_subtype_t subtype)
{
    NSURL* url = [NSURL fileURLWithFileSystemRepresentation:path isDirectory:NO relativeToURL:nil];
    NSDictionary* attributes = @{
        (__bridge NSString*)kSecCodeAttributeArchitecture:@(type),
        (__bridge NSString*)kSecCodeAttributeSubarchitecture:@(subtype)
    };

    SecStaticCodeRef codeRef = NULL;
    OSStatus status = SecStaticCodeCreateWithPathAndAttributes((__bridge CFURLRef) url, kSecCSDefaultFlags, (__bridge CFDictionaryRef)attributes, &codeRef);
    if (status != errSecSuccess) {
        INFO("Unable to create SecStaticCode: %d", status);
        return NULL;
    }

    return codeRef;
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
_createAsymmetricKey(const char *name, CFStringRef keyType, NSNumber* keySize, CFBooleanRef makePermanent = kCFBooleanFalse)
{
    NSString* label = [NSString stringWithUTF8String:name];
    CFRef<CFMutableDictionaryRef> parameters = NULL;
    SecKeyRef privateKey = NULL;
    CFRef<SecAccessControlRef> accessControl = NULL;

    accessControl.take(SecAccessControlCreateWithFlags(kCFAllocatorDefault,
                                                    kSecAttrAccessibleAfterFirstUnlock,
                                                    0,
                                                    NULL));
    require_string(accessControl, exit, "Unable to create accessControl reference");

    parameters.take(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    require_string(parameters, exit, "Unable to create parameters dictionary");

    // Required parameters for creating a key.
    CFDictionaryAddValue(parameters, kSecAttrKeyType, keyType);
    CFDictionaryAddValue(parameters, kSecAttrKeySizeInBits, (__bridge CFNumberRef)keySize);

    // Optional parameters.
    CFDictionaryAddValue(parameters, kSecAttrLabel, (__bridge CFStringRef)label);
    CFDictionaryAddValue(parameters, kSecAttrIsPermanent, makePermanent);
    CFDictionaryAddValue(parameters, kSecAttrAccessControl, accessControl);
    CFDictionaryAddValue(parameters, kSecUseDataProtectionKeychain, kCFBooleanTrue);

    privateKey = SecKeyCreateRandomKey(parameters, NULL);
    require_string(privateKey, exit, "Unable to create SecKeyRef");

exit:
    return privateKey;
}

static SecKeyRef
_createRSAKey(const char *name, CFBooleanRef makePermanent = kCFBooleanFalse)
{
    const int keySize = 2048;
    NSNumber* keySizeInBits = @(keySize);
    SecKeyRef privateKey = NULL;

    privateKey = _createAsymmetricKey(name, kSecAttrKeyTypeRSA, keySizeInBits, makePermanent);
    require_string(privateKey, exit, "Unable to create kSecAttrKeyTypeRSA SecKeyRef");

exit:
    return privateKey;
}

static SecKeyRef
_createECCKey(const char *name, CFBooleanRef makePermanent = kCFBooleanFalse)
{
    const int keySize = 384;
    NSNumber* keySizeInBits = @(keySize);
    SecKeyRef privateKey = NULL;

    privateKey = _createAsymmetricKey(name, kSecAttrKeyTypeECSECPrimeRandom, keySizeInBits, makePermanent);
    require_string(privateKey, exit, "Unable to create kSecAttrKeyTypeECSECPrimeRandom SecKeyRef");

exit:
    return privateKey;
}

static SecKeyRef
_findKey(const char *name)
{
    OSStatus status;
    NSString* label = [NSString stringWithUTF8String:name];
    //CFRef<CFMutableDictionaryRef> query = NULL;
    SecKeyRef key = NULL;

    NSDictionary* query = @{
        (__bridge NSString*)kSecClass:(__bridge NSString*)kSecClassKey,
        (__bridge NSString*)kSecReturnRef:@YES,
        (__bridge NSString*)kSecAttrLabel:label,
        (__bridge NSString*)kSecMatchLimit:(__bridge NSString*)kSecMatchLimitOne,
        (__bridge NSString*)kSecUseDataProtectionKeychain:@YES,
        (__bridge NSString*)kSecAttrCanSign:@YES,
    };
    status = SecItemCopyMatching((__bridge CFDictionaryRef)query, (CFTypeRef*)&key);
    if (status != errSecSuccess) {
        INFO("Unable to query keychain for key (%s): %d", name, status);
    }

   return key;
}

static SecCertificateRef
_createSelfSignedCertificate(const char *name, SecKeyRef privateKey)
{
#if TARGET_OS_IPHONE
    const CFStringRef kSecOIDExtendedKeyUsage = CFSTR("2.5.29.37");
#endif
    const uint8_t EKUCodeSigingEncodedData[] = {0x30, 0x0a, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x03};
    const int keyUsage = kSecKeyUsageAll;

    NSString* commonName = [NSString stringWithUTF8String:name];
    NSData* EKUCodeSigningEncoded = [NSData dataWithBytes:EKUCodeSigingEncodedData length:sizeof(EKUCodeSigingEncodedData)];
    NSNumber* keyUsageNumber = @(keyUsage);
    NSDictionary* extensions = @{
        (__bridge NSString*)kSecOIDExtendedKeyUsage:EKUCodeSigningEncoded
    };
    NSDictionary* parameters = @{
        (__bridge NSString*)kSecCertificateKeyUsage:keyUsageNumber,
        (__bridge NSString*)kSecCertificateExtensionsEncoded:extensions
    };
    NSArray* subject = @[@[@[
        (__bridge NSString*)kSecOidCommonName,
        commonName
    ]]];
    SecCertificateRef certificate = NULL;
    CFRef<SecKeyRef> publicKey = NULL;

    publicKey.take(SecKeyCopyPublicKey(privateKey));
    certificate = SecGenerateSelfSignedCertificate((__bridge CFArrayRef)subject, (__bridge CFDictionaryRef)parameters, publicKey, privateKey);
    require_string(certificate, exit, "Unable to create SecCertificateRef");

exit:
    return certificate;
}

static int
_addSecItem(CFStringRef keyClass, CFTypeRef item, const char *labelPtr)
{
    int ret = -1;
    OSStatus status;
    NSString* label = [NSString stringWithUTF8String:labelPtr];
    CFRef<CFMutableDictionaryRef> attributes = NULL;

    attributes.take(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    require_string(attributes, exit, "Unable to create attributes CFDictionary");

    CFDictionaryAddValue(attributes, kSecClass, keyClass);
    CFDictionaryAddValue(attributes, kSecValueRef, item);
    CFDictionaryAddValue(attributes, kSecAttrLabel, (__bridge CFStringRef) label);
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
_deleteSecItem(NSString* keyClass, const char *labelPtr)
{
    int ret = -1;
    OSStatus status;
    NSString* label = [NSString stringWithUTF8String:labelPtr];
    NSDictionary* query = @{
        (__bridge NSString*)kSecClass:keyClass,
        (__bridge NSString*)kSecAttrLabel:label,
        (__bridge NSString*)kSecUseDataProtectionKeychain:@YES
    };

    status = SecItemDelete((__bridge CFDictionaryRef) query);
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
_checkSignatureValidityWithSecStaticCode(SecStaticCodeRef codeRef, SecCSFlags verificationFlags, bool allowUntrusted = false)
{
    int ret = -1;
    OSStatus status;

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
_checkPageSizeForStaticCode(SecStaticCodeRef code, size_t expectedPageSize)
{
    int ret = -1;
    CFDictionaryRef cfinfo = NULL;
    NSDictionary* info = nil;
    NSData* codeDir = nil;
    Security::CodeSigning::CodeDirectory* cd = NULL;
    size_t foundPageSize = 0;
    OSStatus status = SecCodeCopySigningInformation(code, (SecCSFlags)kSecCSInternalInformation, &cfinfo);
    if (status != errSecSuccess) {
        INFO("Unable to copy signing information through SecCodeCopySigningInformation: %d", status);
        goto exit;
    }
    info = (__bridge_transfer NSDictionary*)cfinfo;
    codeDir = [info objectForKey:(__bridge NSString*)kSecCodeInfoCodeDirectory];
    cd = (Security::CodeSigning::CodeDirectory*)codeDir.bytes;
    if (cd->pageSize) {
        foundPageSize = 1 << cd->pageSize;
    }
    if (expectedPageSize != foundPageSize) {
        INFO("Found pagesize (%zu) did not match expected (%zu)", foundPageSize, expectedPageSize);
    } else {
        ret = 0;
    }
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
    NSDictionary* parameters = @{};
    CFRef<CFDictionaryRef> signingInfo = NULL;

    status = SecCodeSignerCreate((__bridge CFDictionaryRef)parameters, kSecCSRemoveSignature, signerRef.take());
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

static void
_addLwcrToParams(CFMutableDictionaryRef params, NSData* self_lwcr, NSData* parent_lwcr, NSData* responsible_lwcr, NSData* library_lwcr)
{
    if (self_lwcr) {
        BlobWrapper *wrap = BlobWrapper::alloc(self_lwcr.bytes, self_lwcr.length, kSecCodeMagicLaunchConstraint);
        CFDictionaryAddValue(params, kSecCodeSignerLaunchConstraintSelf, CFTempData(*(BlobCore*)wrap));
        ::free(wrap);
    }
    if (parent_lwcr) {
        BlobWrapper *wrap = BlobWrapper::alloc(parent_lwcr.bytes, parent_lwcr.length, kSecCodeMagicLaunchConstraint);
        CFDictionaryAddValue(params, kSecCodeSignerLaunchConstraintParent, CFTempData(*(BlobCore*)wrap));
        ::free(wrap);
    }
    if (responsible_lwcr) {
        BlobWrapper *wrap = BlobWrapper::alloc(responsible_lwcr.bytes, responsible_lwcr.length, kSecCodeMagicLaunchConstraint);
        CFDictionaryAddValue(params, kSecCodeSignerLaunchConstraintResponsible, CFTempData(*(BlobCore*)wrap));
        ::free(wrap);
    }
    if (library_lwcr) {
        BlobWrapper *wrap = BlobWrapper::alloc(library_lwcr.bytes, library_lwcr.length, kSecCodeMagicLaunchConstraint);
        CFDictionaryAddValue(params, kSecCodeSignerLibraryConstraint, CFTempData(*(BlobCore*)wrap));
        ::free(wrap);
    }
}

static int
_forceAddSignature(const char *path, const char *ident, CFMutableDictionaryRef params, SecCSFlags flags = kSecCSDefaultFlags)
{
    int ret = -1;
    OSStatus status;
    CFRef<SecCodeSignerRef> signerRef = NULL;
    CFRef<SecStaticCodeRef> codeRef = NULL;
    CFRef<CFMutableDictionaryRef> parameters = NULL;
    NSString* identifier = [NSString stringWithUTF8String:ident];
    CFRef<CFDictionaryRef> signingInfo = NULL;
    CFRef<CFStringRef> signatureIdentifierRef = NULL;
    CFRef<CFDictionaryRef> lwcrRef = NULL;

    parameters = params;

    // Force an identifier on this, so we can validate later.
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentifier, (__bridge CFStringRef) identifier);

    // If we didn't want to force, then we need to add kSecCSSignPreserveSignature.
    status = SecCodeSignerCreate(parameters, flags, signerRef.take());
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

    if (CFStringCompare(signatureIdentifierRef, (__bridge CFStringRef)identifier, 0) != kCFCompareEqualTo) {
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

    CFRef<CFMutableDictionaryRef> parameters(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    // This is how we do adhoc signing.
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, SecIdentityRef(kCFNull));

    int ret = -1;

    if (_copyPath(path, copyPath)) {
        FAIL("Unable to create temporary path (%s)", path);
        goto exit;
    }

    ret = _forceAddSignature(path, "MachOIdentity", parameters);
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
_CheckAddAdhocSignatureMachOAndPageSizeHelper(boolean_t unsignFirst)
{
    const char *path = k_ls_TemporaryBinaryPath;
    const char *copyPath = k_ls_BinaryPath;
    CFRef<SecStaticCodeRef> codeRef (NULL);

    CFRef<CFMutableDictionaryRef> parameters(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    // This is how we do adhoc signing.
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, SecIdentityRef(kCFNull));

    int ret = -1;
    int foundSlice = 0;

    if (_copyPath(path, copyPath)) {
        FAIL("Unable to create temporary path (%s)", path);
        goto exit;
    }

    if (unsignFirst) {
        ret = _removeSignature(path);
        if (ret) {
            FAIL("Unable to remove signature from %s", path);
            goto exit;
        }
    }

    ret = _forceAddSignature(path, "MachOIdentity", parameters);
    if (ret) {
        FAIL("Unable to add adhoc signature to %s", path);
        goto exit;
    }

    codeRef.take(_createStaticCodeForArchitecture(path, CPU_TYPE_ARM64, CPU_SUBTYPE_ANY));
    if (codeRef) {
        foundSlice ++;
#if TARGET_OS_OSX
        ret = _checkSignatureValidityWithSecStaticCode(codeRef, kSecCSDefaultFlags);
        if (ret != 0) {
            FAIL("Unable to verify signature for ARM64 slice: %s", path);
            goto exit;
        }
#endif
        ret = _checkPageSizeForStaticCode(codeRef, 4*1024);
        if (ret != 0) {
            FAIL("Unexpected page size for ARM64 slice: %s", path);
            goto exit;
        }
    } else {
        INFO("%s does not have an ARM64 slice", path);
    }

    codeRef.take(_createStaticCodeForArchitecture(path, CPU_TYPE_ARM64_32, CPU_SUBTYPE_ANY));
    if (codeRef) {
        foundSlice ++;
#if TARGET_OS_OSX
        ret = _checkSignatureValidityWithSecStaticCode(codeRef, kSecCSDefaultFlags);
        if (ret != 0) {
            FAIL("Unable to verify signature for ARM64_32 slice: %s", path);
            goto exit;
        }
#endif
        ret = _checkPageSizeForStaticCode(codeRef, 4*1024);
        if (ret != 0) {
            FAIL("Unexpected page size for ARM64_32 slice: %s", path);
            goto exit;
        }
    } else {
        INFO("%s does not have an ARM64_32 slice", path);
    }

    codeRef.take(_createStaticCodeForArchitecture(path, CPU_TYPE_ARM, CPU_SUBTYPE_ANY));
    if (codeRef) {
        foundSlice ++;
#if TARGET_OS_OSX
        ret = _checkSignatureValidityWithSecStaticCode(codeRef, kSecCSDefaultFlags);
        if (ret != 0) {
            FAIL("Unable to verify signature for ARM slice: %s", path);
            goto exit;
        }
#endif
        ret = _checkPageSizeForStaticCode(codeRef, 4*1024);
        if (ret != 0) {
            FAIL("Unexpected page size for ARM slice: %s", path);
            goto exit;
        }
    } else {
        INFO("%s does not have an ARM slice", path);
    }

    codeRef.take(_createStaticCodeForArchitecture(path, CPU_TYPE_X86_64, CPU_SUBTYPE_ANY));
    if (codeRef) {
        foundSlice ++;
        ret = _checkSignatureValidityWithSecStaticCode(codeRef, kSecCSDefaultFlags);
        if (ret != 0) {
            FAIL("Unable to verify signature for X86_64 slice: %s", path);
            goto exit;
        }
        ret = _checkPageSizeForStaticCode(codeRef, 4*1024);
        if (ret != 0) {
            FAIL("Unexpected page size for X86_64 slice: %s", path);
            goto exit;
        }
    } else {
        INFO("%s does not have an X86_64 slice", path);
    }

    if (foundSlice == 0) {
        FAIL("No slices found");
        ret = -1;
        goto exit;
    }

    PASS("Successfully added adhoc signature to %s", path);
    ret = 0;

exit:
    _deletePath(path);
    return ret;
}

static int
CheckAddAdhocSignatureMachOAndPageSizeHelperUnsigned()
{
    BEGIN();
    return _CheckAddAdhocSignatureMachOAndPageSizeHelper(true);
}

static int
CheckAddAdhocSignatureMachOAndPageSizeHelperResign()
{
    BEGIN();
    return _CheckAddAdhocSignatureMachOAndPageSizeHelper(false);
}

static NSData*
_convertLWCRInfoToBlob(uint8_t cc, NSDictionary* req)
{
    NSError* error = nil;
    LWCR* lwcr = [LWCR withVersion:kLWCRVersionOne
            withConstraintCategory:cc
                  withRequirements:req
                         withError:&error];
    if (lwcr == nil) {
        INFO("Error parsing light weight code requirement data: %s", error.description.UTF8String);
    }
    return [lwcr externalRepresentation];
}

static int
CheckAddAdhocSignatureMachOWithSelfConstraint()
{
    BEGIN();
    const char *path = k_ls_TemporaryBinaryPath;
    const char *copyPath = k_ls_BinaryPath;
    OSStatus status;
    CFRef<SecStaticCodeRef> codeRef (NULL);
    CFRef<CFDictionaryRef> signingInfo (NULL);
    CFDataRef lwcr_out = NULL;

    CFRef<CFMutableDictionaryRef> parameters(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    // This is how we do adhoc signing.
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, SecIdentityRef(kCFNull));

    NSData* lwcr_self =_convertLWCRInfoToBlob(0, @{
        @"validation-category":@1
    });
    _addLwcrToParams(parameters, lwcr_self, nil, nil, nil);

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

    ret = _forceAddSignature(path, "MachOIdentity", parameters);
    if (ret) {
        FAIL("Unable to add adhoc signature to %s", path);
        goto exit;
    }
    codeRef.take(_createStaticCode(path));
    ret = _checkSignatureValidityWithSecStaticCode(codeRef, kSecCSDefaultFlags, TARGET_OS_IPHONE);
    if (ret) {
        FAIL("Unable to verify signature for: %s", path);
        goto exit;
    }

    status = SecCodeCopySigningInformation(codeRef, kSecCSInternalInformation, signingInfo.take());
    if (status != errSecSuccess) {
        FAIL("Error on acquiring signing information through SecCodeCopySigningInformation: %d", status);
        ret = -1;
        goto exit;
    }

    lwcr_out = (CFDataRef)CFDictionaryGetValue(signingInfo, kSecCodeInfoLaunchConstraintsSelf);
    if (!lwcr_out) {
        FAIL("No kSecCodeInfoLaunchConstraintsSelf on %s", path);
        ret = -1;
        goto exit;
    }

    CFDictionaryRemoveAllValues(parameters);
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, SecIdentityRef(kCFNull));
    CFDictionaryAddValue(parameters, kSecCodeSignerPreserveMetadata, CFTempNumber(kSecCodeSignerPreserveIdentifier|kSecCodeSignerPreserveLaunchConstraints));

    ret = _forceAddSignature(path, "MachOIdentity", parameters);
    if (ret) {
        FAIL("Unable to add adhoc signature to %s", path);
        goto exit;
    }
    codeRef.take(_createStaticCode(path));
    ret = _checkSignatureValidityWithSecStaticCode(codeRef, kSecCSDefaultFlags, TARGET_OS_IPHONE);
    if (ret != 0) {
        FAIL("Unable to verify signature for: %s", path);
        goto exit;
    }

    status = SecCodeCopySigningInformation(codeRef, kSecCSInternalInformation, signingInfo.take());
    if (status != errSecSuccess) {
        FAIL("Error on acquiring signing information through SecCodeCopySigningInformation: %d", status);
        ret = -1;
        goto exit;
    }

    lwcr_out = (CFDataRef)CFDictionaryGetValue(signingInfo, kSecCodeInfoLaunchConstraintsSelf);
    if (!lwcr_out) {
        FAIL("No kSecCodeInfoLaunchConstraintsSelf on %s", path);
        ret = -1;
        goto exit;
    }

    PASS("Successfully signed and preserved self launch constraint");
    ret = 0;

exit:
    _deletePath(path);
    return ret;
}

static int
CheckAddAdhocSignatureMachOWithParentConstraint()
{
    BEGIN();
    const char *path = k_ls_TemporaryBinaryPath;
    const char *copyPath = k_ls_BinaryPath;
    OSStatus status;
    CFRef<SecStaticCodeRef> codeRef (NULL);
    CFRef<CFDictionaryRef> signingInfo (NULL);
    CFDataRef lwcr_out = NULL;

    CFRef<CFMutableDictionaryRef> parameters(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    // This is how we do adhoc signing.
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, SecIdentityRef(kCFNull));

    NSData* lwcr_parent =_convertLWCRInfoToBlob(0, @{
        @"validation-category":@2
    });
    _addLwcrToParams(parameters, nil, lwcr_parent, nil, nil);

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

    ret = _forceAddSignature(path, "MachOIdentity", parameters);
    if (ret) {
        FAIL("Unable to add adhoc signature to %s", path);
        goto exit;
    }
    codeRef.take(_createStaticCode(path));
    ret = _checkSignatureValidityWithSecStaticCode(codeRef, kSecCSDefaultFlags, TARGET_OS_IPHONE);
    if (ret) {
        FAIL("Unable to verify signature for: %s", path);
        goto exit;
    }

    status = SecCodeCopySigningInformation(codeRef, kSecCSInternalInformation, signingInfo.take());
    if (status != errSecSuccess) {
        FAIL("Error on acquiring signing information through SecCodeCopySigningInformation: %d", status);
        ret = -1;
        goto exit;
    }

    lwcr_out = (CFDataRef)CFDictionaryGetValue(signingInfo, kSecCodeInfoLaunchConstraintsParent);
    if (!lwcr_out) {
        FAIL("No kSecCodeInfoLaunchConstraintsParent on %s", path);
        ret = -1;
        goto exit;
    }

    CFDictionaryRemoveAllValues(parameters);
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, SecIdentityRef(kCFNull));
    CFDictionaryAddValue(parameters, kSecCodeSignerPreserveMetadata, CFTempNumber(kSecCodeSignerPreserveIdentifier|kSecCodeSignerPreserveLaunchConstraints));

    ret = _forceAddSignature(path, "MachOIdentity", parameters);
    if (ret) {
        FAIL("Unable to add adhoc signature to %s", path);
        goto exit;
    }
    codeRef.take(_createStaticCode(path));
    ret = _checkSignatureValidityWithSecStaticCode(codeRef, kSecCSDefaultFlags, TARGET_OS_IPHONE);
    if (ret) {
        FAIL("Unable to verify signature for: %s", path);
        goto exit;
    }

    status = SecCodeCopySigningInformation(codeRef, kSecCSInternalInformation, signingInfo.take());
    if (status != errSecSuccess) {
        FAIL("Error on acquiring signing information through SecCodeCopySigningInformation: %d", status);
        ret = -1;
        goto exit;
    }

    lwcr_out = (CFDataRef)CFDictionaryGetValue(signingInfo, kSecCodeInfoLaunchConstraintsParent);
    if (!lwcr_out) {
        FAIL("No kSecCodeInfoLaunchConstraintsParent on %s", path);
        ret = -1;
        goto exit;
    }

    PASS("Successfully signed and preserved parent launch constraint");
    ret = 0;

exit:
    _deletePath(path);
    return ret;
}

static int
CheckAddAdhocSignatureMachOWithResponsibleConstraint()
{
    BEGIN();
    const char *path = k_ls_TemporaryBinaryPath;
    const char *copyPath = k_ls_BinaryPath;
    OSStatus status;
    CFRef<SecStaticCodeRef> codeRef (NULL);
    CFRef<CFDictionaryRef> signingInfo (NULL);

    CFRef<CFMutableDictionaryRef> parameters(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    // This is how we do adhoc signing.
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, SecIdentityRef(kCFNull));

    CFDataRef lwcr_out = NULL;
    NSData* lwcr_responsible =_convertLWCRInfoToBlob(0, @{
        @"validation-category":@3
    });
    _addLwcrToParams(parameters, nil, nil, lwcr_responsible, nil);

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

    ret = _forceAddSignature(path, "MachOIdentity", parameters);
    if (ret) {
        FAIL("Unable to add adhoc signature to %s", path);
        goto exit;
    }
    codeRef.take(_createStaticCode(path));
    ret = _checkSignatureValidityWithSecStaticCode(codeRef, kSecCSDefaultFlags, TARGET_OS_IPHONE);
    if (ret != 0) {
        FAIL("Unable to verify signature for: %s", path);
        goto exit;
    }

    status = SecCodeCopySigningInformation(codeRef, kSecCSInternalInformation, signingInfo.take());
    if (status != errSecSuccess) {
        FAIL("Error on acquiring signing information through SecCodeCopySigningInformation: %d", status);
        ret = -1;
        goto exit;
    }

    lwcr_out = (CFDataRef)CFDictionaryGetValue(signingInfo, kSecCodeInfoLaunchConstraintsResponsible);
    if (!lwcr_out) {
        FAIL("No kSecCodeInfoLaunchConstraintsParent on %s", path);
        ret = -1;
        goto exit;
    }

    CFDictionaryRemoveAllValues(parameters);
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, SecIdentityRef(kCFNull));
    CFDictionaryAddValue(parameters, kSecCodeSignerPreserveMetadata, CFTempNumber(kSecCodeSignerPreserveIdentifier|kSecCodeSignerPreserveLaunchConstraints));

    ret = _forceAddSignature(path, "MachOIdentity", parameters);
    if (ret) {
        FAIL("Unable to add adhoc signature to %s", path);
        ret = -1;
        goto exit;
    }
    codeRef.take(_createStaticCode(path));
    ret = _checkSignatureValidityWithSecStaticCode(codeRef, kSecCSDefaultFlags, TARGET_OS_IPHONE);
    if (ret != 0) {
        FAIL("Unable to verify signature for: %s", path);
        goto exit;
    }

    status = SecCodeCopySigningInformation(codeRef, kSecCSInternalInformation, signingInfo.take());
    if (status != errSecSuccess) {
        FAIL("Error on acquiring signing information through SecCodeCopySigningInformation: %d", status);
        ret = -1;
        goto exit;
    }

    lwcr_out = (CFDataRef)CFDictionaryGetValue(signingInfo, kSecCodeInfoLaunchConstraintsResponsible);
    if (!lwcr_out) {
        FAIL("No kSecCodeInfoLaunchConstraintsResponsible on %s", path);
        ret = -1;
        goto exit;
    }

    PASS("Successfully signed and preserved responsible launch constraint");
    ret = 0;

exit:
    _deletePath(path);
    return ret;
}

static int
CheckAddAdhocSignatureMachOPreserveLaunchConstraints()
{
    BEGIN();
    const char *path = k_ls_TemporaryBinaryPath;
    const char *copyPath = k_ls_BinaryPath;
    OSStatus status;
    CFRef<SecStaticCodeRef> codeRef (NULL);
    CFRef<CFDictionaryRef> signingInfo (NULL);
    CFDataRef lwcr_out = NULL;

    CFRef<CFMutableDictionaryRef> parameters(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    // This is how we do adhoc signing.
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, SecIdentityRef(kCFNull));

    NSData* lwcr_self =_convertLWCRInfoToBlob(0, @{
        @"validation-category":@1
    });
    NSData* lwcr_parent =_convertLWCRInfoToBlob(0, @{
        @"validation-category":@2
    });
    NSData* lwcr_responsible =_convertLWCRInfoToBlob(0, @{
        @"validation-category":@3
    });
    _addLwcrToParams(parameters, lwcr_self, lwcr_parent, lwcr_responsible, nil);

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

    // Add constraints
    ret = _forceAddSignature(path, "MachOIdentity", parameters);
    if (ret) {
        FAIL("Unable to add adhoc signature to %s", path);
        goto exit;
    }

    codeRef.take(_createStaticCode(path));
    ret = _checkSignatureValidityWithSecStaticCode(codeRef, kSecCSDefaultFlags, TARGET_OS_IPHONE);
    if (ret != 0) {
        FAIL("Unable to verify signature for: %s", path);
        goto exit;
    }

    status = SecCodeCopySigningInformation(codeRef, kSecCSInternalInformation, signingInfo.take());
    if (status != errSecSuccess) {
        FAIL("Error on acquiring signing information through SecCodeCopySigningInformation: %d", status);
        ret = -1;
        goto exit;
    }

    lwcr_out = (CFDataRef)CFDictionaryGetValue(signingInfo, kSecCodeInfoLaunchConstraintsSelf);
    if (!lwcr_out) {
        FAIL("No kSecCodeInfoLaunchConstraintsSelf on %s", path);
        ret = -1;
        goto exit;
    }
    lwcr_out = (CFDataRef)CFDictionaryGetValue(signingInfo, kSecCodeInfoLaunchConstraintsParent);
    if (!lwcr_out) {
        FAIL("No kSecCodeInfoLaunchConstraintsParent on %s", path);
        ret = -1;
        goto exit;
    }
    lwcr_out = (CFDataRef)CFDictionaryGetValue(signingInfo, kSecCodeInfoLaunchConstraintsResponsible);
    if (!lwcr_out) {
        FAIL("No kSecCodeInfoLaunchConstraintsResponsible on %s", path);
        ret = -1;
        goto exit;
    }

    // Preserve Constraints
    CFDictionaryRemoveAllValues(parameters);
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, SecIdentityRef(kCFNull));
    CFDictionaryAddValue(parameters, kSecCodeSignerPreserveMetadata, CFTempNumber(kSecCodeSignerPreserveIdentifier|kSecCodeSignerPreserveLaunchConstraints));

    ret = _forceAddSignature(path, "MachOIdentity", parameters);
    if (ret) {
        FAIL("Unable to add adhoc signature to %s", path);
        goto exit;
    }
    codeRef.take(_createStaticCode(path));
    ret = _checkSignatureValidityWithSecStaticCode(codeRef, kSecCSDefaultFlags, TARGET_OS_IPHONE);
    if (ret != 0) {
        FAIL("Unable to verify signature for: %s", path);
        goto exit;
    }

    status = SecCodeCopySigningInformation(codeRef, kSecCSInternalInformation, signingInfo.take());
    if (status != errSecSuccess) {
        FAIL("Error on acquiring signing information through SecCodeCopySigningInformation: %d", status);
        ret = -1;
        goto exit;
    }

    lwcr_out = (CFDataRef)CFDictionaryGetValue(signingInfo, kSecCodeInfoLaunchConstraintsSelf);
    if (!lwcr_out) {
        FAIL("No kSecCodeInfoLaunchConstraintsSelf on %s", path);
        ret = -1;
        goto exit;
    }
    lwcr_out = (CFDataRef)CFDictionaryGetValue(signingInfo, kSecCodeInfoLaunchConstraintsParent);
    if (!lwcr_out) {
        FAIL("No kSecCodeInfoLaunchConstraintsParent on %s", path);
        ret = -1;
        goto exit;
    }
    lwcr_out = (CFDataRef)CFDictionaryGetValue(signingInfo, kSecCodeInfoLaunchConstraintsResponsible);
    if (!lwcr_out) {
        FAIL("No kSecCodeInfoLaunchConstraintsResponsible on %s", path);
        ret = -1;
        goto exit;
    }

    // Don't preserve
    CFDictionaryRemoveAllValues(parameters);
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, SecIdentityRef(kCFNull));

    ret = _forceAddSignature(path, "MachOIdentity", parameters);
    if (ret) {
        FAIL("Unable to add adhoc signature to %s", path);
        goto exit;
    }
    codeRef.take(_createStaticCode(path));
    ret = _checkSignatureValidityWithSecStaticCode(codeRef, kSecCSDefaultFlags, TARGET_OS_IPHONE);
    if (ret != 0) {
        FAIL("Unable to verify signature for: %s", path);
        goto exit;
    }

    status = SecCodeCopySigningInformation(codeRef, kSecCSInternalInformation, signingInfo.take());
    if (status != errSecSuccess) {
        FAIL("Error on acquiring signing information through SecCodeCopySigningInformation: %d", status);
        ret = -1;
        goto exit;
    }

    lwcr_out = (CFDataRef)CFDictionaryGetValue(signingInfo, kSecCodeInfoLaunchConstraintsSelf);
    if (lwcr_out) {
        FAIL("Unexpected kSecCodeInfoLaunchConstraintsSelf on %s", path);
        ret = -1;
        goto exit;
    }
    lwcr_out = (CFDataRef)CFDictionaryGetValue(signingInfo, kSecCodeInfoLaunchConstraintsParent);
    if (lwcr_out) {
        FAIL("Unexpected kSecCodeInfoLaunchConstraintsParent on %s", path);
        ret = -1;
        goto exit;
    }
    lwcr_out = (CFDataRef)CFDictionaryGetValue(signingInfo, kSecCodeInfoLaunchConstraintsResponsible);
    if (lwcr_out) {
        FAIL("Unexpected kSecCodeInfoLaunchConstraintsResponsible on %s", path);
        ret = -1;
        goto exit;
    }

    PASS("Successfully signed and preserved and removed launch constraints");
    ret = 0;

exit:
    _deletePath(path);
    return ret;
}

static int
CheckAddAdhocSignatureMachoLibraryConstraints()
{
    BEGIN();
    const char *path = k_ls_TemporaryBinaryPath;
    const char *copyPath = k_ls_BinaryPath;
    OSStatus status;
    CFRef<SecStaticCodeRef> codeRef (NULL);
    CFRef<CFDictionaryRef> signingInfo (NULL);
    CFDataRef lwcr_out = NULL;

    CFRef<CFMutableDictionaryRef> parameters(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    // This is how we do adhoc signing.
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, SecIdentityRef(kCFNull));

    NSData* lwcr_library =_convertLWCRInfoToBlob(0, @{
        @"validation-category":@4
    });
    _addLwcrToParams(parameters, nil, nil, nil, lwcr_library);
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

    ret = _forceAddSignature(path, "MachOIdentity", parameters);
    if (ret) {
        FAIL("Unable to add adhoc signature to %s", path);
        goto exit;
    }
    codeRef.take(_createStaticCode(path));
    ret = _checkSignatureValidityWithSecStaticCode(codeRef, kSecCSDefaultFlags, TARGET_OS_IPHONE);
    if (ret != 0) {
        FAIL("Unable to verify signature for: %s", path);
        goto exit;
    }

    status = SecCodeCopySigningInformation(codeRef, kSecCSInternalInformation, signingInfo.take());
    if (status != errSecSuccess) {
        FAIL("Error on acquiring signing information through SecCodeCopySigningInformation: %d", status);
        ret = -1;
        goto exit;
    }

    lwcr_out = (CFDataRef)CFDictionaryGetValue(signingInfo, kSecCodeInfoLibraryConstraints);
    if (!lwcr_out) {
        FAIL("No kSecCodeInfoLaunchConstraintsParent on %s", path);
        ret = -1;
        goto exit;
    }

    CFDictionaryRemoveAllValues(parameters);
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, SecIdentityRef(kCFNull));
    CFDictionaryAddValue(parameters, kSecCodeSignerPreserveMetadata, CFTempNumber(kSecCodeSignerPreserveIdentifier|kSecCodeSignerPreserveLibraryConstraints));

    ret = _forceAddSignature(path, "MachOIdentity", parameters);
    if (ret) {
        FAIL("Unable to add adhoc signature to %s", path);
        goto exit;
    }
    codeRef.take(_createStaticCode(path));
    ret = _checkSignatureValidityWithSecStaticCode(codeRef, kSecCSDefaultFlags, TARGET_OS_IPHONE);
    if (ret != 0) {
        FAIL("Unable to verify signature for: %s", path);
        goto exit;
    }

    status = SecCodeCopySigningInformation(codeRef, kSecCSInternalInformation, signingInfo.take());
    if (status != errSecSuccess) {
        FAIL("Error on acquiring signing information through SecCodeCopySigningInformation: %d", status);
        ret = -1;
        goto exit;
    }

    lwcr_out = (CFDataRef)CFDictionaryGetValue(signingInfo, kSecCodeInfoLibraryConstraints);
    if (!lwcr_out) {
        FAIL("No kSecCodeInfoLaunchConstraintsResponsible on %s", path);
        ret = -1;
        goto exit;
    }

    PASS("Successfully signed and preserved library constraint");
    ret = 0;

exit:
    _deletePath(path);
    return ret;
}

static int
CheckReSignPreserveMetadataRuntimeMachO(void)
{
    BEGIN();

    const char *path = k_ls_TemporaryBinaryPath;
    const char *copyPath = k_ls_BinaryPath;
    int ret = -1;
    OSStatus status;
    CFRef<SecStaticCodeRef> codeRef = NULL;
    CFRef<CFDictionaryRef> signingInfo = NULL;
    CFRef<CFNumberRef> runtimeVersionRef = NULL;

    // Sign with a runtime version first
    {
        CFRef<CFMutableDictionaryRef> parameters(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, SecIdentityRef(kCFNull));
        uint64_t flags = kSecCodeSignatureRuntime;
        CFDictionaryAddValue(parameters, kSecCodeSignerFlags, CFTempNumber(flags));
        CFDictionaryAddValue(parameters, kSecCodeSignerRuntimeVersion, CFSTR("13.0.0"));

        if (_copyPath(path, copyPath)) {
            FAIL("Unable to create temporary path (%s)", path);
            goto exit;
        }

        ret = _forceAddSignature(path, "MachOIdentity", parameters);
        if (ret) {
            FAIL("Unable to add adhoc signature to %s", path);
            goto exit;
        }
    }

    // Now re-sign in place, preserving the runtime version
    {
        CFRef<CFMutableDictionaryRef> parameters(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, SecIdentityRef(kCFNull));
        uint64_t preserveMetadata = kSecCodeSignerPreserveRuntime;
        CFDictionaryAddValue(parameters, kSecCodeSignerPreserveMetadata, CFTempNumber(preserveMetadata));

        ret = _forceAddSignature(path, "MachOIdentity", parameters);
        if (ret) {
            FAIL("Unable to add adhoc signature to %s", path);
            goto exit;
        }
    }

    // Check that the runtime version was preserved
    {
        codeRef.take(_createStaticCode(path));
        require_string(codeRef, exit, "Unable to create SecStaticCode");

        status = SecCodeCopySigningInformation(codeRef, kSecCSDefaultFlags, signingInfo.take());
        if (status != errSecSuccess) {
            INFO("Error on acquiring signing information through SecCodeCopySigningInformation: %d", status);
            goto exit;
        }

        // Don't use .take since we want we add a CFRetain on it.
        runtimeVersionRef = (CFNumberRef)CFDictionaryGetValue(signingInfo, kSecCodeInfoRuntimeVersion);
        if (!runtimeVersionRef) {
            FAIL("No kSecCodeInfoRuntimeVersion on %s", path);
            goto exit;
        }

        uint64_t expectedRuntimeVersion = 0x0d0000;
        CFRef<CFNumberRef> expectedRuntimeVersionRef = CFTempNumber(expectedRuntimeVersion);
        if (CFNumberCompare(runtimeVersionRef, expectedRuntimeVersionRef, 0) != kCFCompareEqualTo) {
            uint32_t n;
            CFNumberGetValue(runtimeVersionRef, kCFNumberSInt32Type, &n);
            INFO("Forced signature runtime version mismatch: %d", n);
            goto exit;
        }
    }

    PASS("Successfully replaced signature while preserving runtime metadata, to %s", path);
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

    CFRef<CFMutableDictionaryRef> parameters(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    // This is how we do adhoc signing.
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, SecIdentityRef(kCFNull));

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
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, selfSignedIdentity);

    if (_copyPath(path, copyPath)) {
        FAIL("Unable to create temporary path (%s)", path);
        goto exit;
    }

    ret = _forceAddSignature(path, "MachOIdentity", parameters);
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

    CFRef<CFMutableDictionaryRef> parameters(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    // This is how we do adhoc signing.
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, SecIdentityRef(kCFNull));

    if (_copyPath(path, copyPath)) {
        FAIL("Unable to create temporary path (%s)", path);
        goto exit;
    }

    ret = _forceAddSignature(path, "BundleIdentity", parameters);
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

    CFRef<CFMutableDictionaryRef> parameters(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

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
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, selfSignedIdentity);

    if (_copyPath(path, copyPath)) {
        FAIL("Unable to create temporary path (%s)", path);
        goto exit;
    }

    ret = _forceAddSignature(path, "BundleIdentity", parameters);
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

    CFRef<CFMutableDictionaryRef> parameters(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

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
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, selfSignedIdentity);

    if (_copyPath(path, copyPath)) {
        FAIL("Unable to create temporary path (%s)", path);
        goto exit;
    }

    ret = _forceAddSignature(path, "BundleIdentity", parameters);
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
    _deleteSecItem((__bridge NSString*)kSecClassCertificate, certName);
    _deleteSecItem((__bridge NSString*)kSecClassKey, keyName);
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

    CFRef<CFMutableDictionaryRef> parameters(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

   // This is how we do adhoc signing.
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, SecIdentityRef(kCFNull));

    // Create a test directory, and a simple content directory.
    _runCommand("mkdir -p %s", testContentRootPath);
    _runCommand("echo 'hello' > %s/hello.txt", testContentRootPath);

    // Create an encrypted disk image with a known password and then strip the
    // FinderInfo attribute that inevitably ends up on it.
    const char *const cmd = "hdiutil create -encryption 'AES-256' -passphrase %s -srcfolder %s %s";
    _runCommand(cmd, "helloworld", testContentRootPath, diskImagePath);
    _runCommand("xattr -c %s", diskImagePath);

    ret = _forceAddSignature(diskImagePath, "com.test.encrypted-disk-image", parameters);
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

static int
_addFinderXattr (const char *path)
{
    uint8_t finderInfo[32] = {0};
    finderInfo[10] = 0xff;
    finderInfo[11] = 0xff;
    finderInfo[12] = 0xff;
    finderInfo[13] = 0xff;

    return setxattr(path, "com.apple.FinderInfo", finderInfo, sizeof(finderInfo), 0, 0);
}

static int
CheckStripDisallowedXattrsOnBundleRoot (void)
{
    BEGIN();

    const char *path = kTemporaryNullBundlePath;
    const char *copyPath = kNullBundlePath;
    int ret = -1;

    CFRef<CFMutableDictionaryRef> parameters(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    // This is how we do adhoc signing.
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, SecIdentityRef(kCFNull));

    if (_copyPath(path, copyPath)) {
        FAIL("Unable to create temporary path (%s)", path);
        goto exit;
    }

    if (_addFinderXattr(path)) {
        FAIL("Unable to write xattr (%s)", path);
        goto exit;
    }

    ret = _forceAddSignature(path, "BundleIdentity", parameters, kSecCSSignStrictPreflight);
    if (ret == 0) {
        FAIL("FinderInfo xattr should have prevented signature from being added to %s", path);
        ret = -1;
        goto exit;
    }

    ret = _forceAddSignature(path, "BundleIdentity", parameters, kSecCSSignStrictPreflight | kSecCSStripDisallowedXattrs);
    if (ret) {
        FAIL("Unable to add adhoc signature to %s", path);
        goto exit;
    }

    PASS("Successfully validated bundle with disallowed xattr on bundle root for %s", path);
    ret = 0;

exit:
    _deletePath(path);
    return ret;
}

static int
CheckStripDisallowedXattrsOnBundleMainExecutable (void)
{
    BEGIN();

    const char *path = kTemporaryNullBundlePath;
    const char *copyPath = kNullBundlePath;
#if TARGET_OS_OSX
    const char *mainExecutablePath = kTemporaryNullBundlePath "/Contents/MacOS/Null";
#else
    const char *mainExecutablePath = kTemporaryNullBundlePath "/Null";
#endif
    int ret = -1;

    CFRef<CFMutableDictionaryRef> parameters(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    // This is how we do adhoc signing.
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, SecIdentityRef(kCFNull));

    if (_copyPath(path, copyPath)) {
        FAIL("Unable to create temporary path (%s)", path);
        goto exit;
    }

    if (_addFinderXattr(mainExecutablePath)) {
        FAIL("Unable to write xattr (%s)", mainExecutablePath);
        goto exit;
    }

    ret = _forceAddSignature(path, "BundleIdentity", parameters, kSecCSSignStrictPreflight);
    if (ret == 0) {
        FAIL("FinderInfo xattr should have prevented signature from being added to %s", path);
        ret = -1;
        goto exit;
    }

    ret = _forceAddSignature(path, "BundleIdentity", parameters, kSecCSSignStrictPreflight | kSecCSStripDisallowedXattrs);
    if (ret) {
        FAIL("Unable to add adhoc signature to %s", path);
        goto exit;
    }

    PASS("Successfully validated bundle with disallowed xattr on bundle main executable for %s", path);
    ret = 0;

exit:
    _deletePath(path);
    return ret;
}

static int
CheckStripDisallowedXattrsOnBundleResource (void)
{
    BEGIN();

    const char *path = kTemporaryNullBundlePath;
    const char *copyPath = kNullBundlePath;
#if TARGET_OS_OSX
    const char *resourcePath = kTemporaryNullBundlePath "/Contents/Resources/Base.lproj/MainMenu.nib";
#else
    const char *resourcePath = kTemporaryNullBundlePath "/Default@2x.png";
#endif
    int ret = -1;

    CFRef<CFMutableDictionaryRef> parameters(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    // This is how we do adhoc signing.
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, SecIdentityRef(kCFNull));

    if (_copyPath(path, copyPath)) {
        FAIL("Unable to create temporary path (%s)", path);
        goto exit;
    }

    if (_addFinderXattr(resourcePath)) {
        FAIL("Unable to write xattr (%s)", resourcePath);
        goto exit;
    }

    ret = _forceAddSignature(path, "BundleIdentity", parameters, kSecCSSignStrictPreflight);
    if (ret == 0) {
        FAIL("FinderInfo xattr should have prevented signature from being added to %s", path);
        ret = -1;
        goto exit;
    }

    ret = _forceAddSignature(path, "BundleIdentity", parameters, kSecCSSignStrictPreflight | kSecCSStripDisallowedXattrs);
    if (ret) {
        FAIL("Unable to add adhoc signature to %s", path);
        goto exit;
    }

    PASS("Successfully validated bundle with disallowed xattr on bundle resource for %s", path);
    ret = 0;

exit:
    _deletePath(path);
    return ret;
}

static int
CheckStripDisallowedXattrsOnMachO (void)
{
    BEGIN();

    const char *path = k_ls_TemporaryBinaryPath;
    const char *copyPath = k_ls_BinaryPath;
    int ret = -1;

    CFRef<CFMutableDictionaryRef> parameters(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    // This is how we do adhoc signing.
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, SecIdentityRef(kCFNull));

    if (_copyPath(path, copyPath)) {
        FAIL("Unable to create temporary path (%s)", path);
        goto exit;
    }

    if (_addFinderXattr(path)) {
        FAIL("Unable to write xattr (%s)", path);
        goto exit;
    }

    ret = _forceAddSignature(path, "MachOIdentity", parameters, kSecCSSignStrictPreflight);
    if (ret == 0) {
        FAIL("FinderInfo xattr should have prevented signature from being added to %s", path);
        ret = -1;
        goto exit;
    }

    ret = _forceAddSignature(path, "MachOIdentity", parameters, kSecCSSignStrictPreflight | kSecCSStripDisallowedXattrs);
    if (ret) {
        FAIL("Unable to add adhoc signature to %s", path);
        goto exit;
    }

    PASS("Successfully validated MachO with disallowed xattr for %s", path);
    ret = 0;

exit:
    _deletePath(path);
    return ret;
}

#if TARGET_OS_OSX
static int
CheckStripDisallowedXattrsSigningHFS(void)
{
    BEGIN();

    int ret = -1;
    const char *destinationPath = kHFSVolumeNullBundlePath;
    const char *sourcePath = kNullBundlePath;

    CFRef<CFMutableDictionaryRef> parameters(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    if (_createHFSDiskImage()) {
        FAIL("_createHFSDiskImage error");
        goto exit;
    }
    INFO("Created " kHFSDiskImageFilePath);

    if (_attachHFSDiskImage()) {
        FAIL("_attachHFSDiskImage error");
        goto exit;
    }
    INFO("Attached " kHFSDiskImageFilePath " as " kHFSDiskImageVolumePath);

    // This is how we do adhoc signing.
    CFDictionaryAddValue(parameters, kSecCodeSignerIdentity, SecIdentityRef(kCFNull));

    if (_copyPath(destinationPath, sourcePath)) {
        FAIL("Unable to create temporary path (%s)", destinationPath);
        goto exit;
    }

    ret = _forceAddSignature(destinationPath, "BundleIdentity", parameters, kSecCSSignStrictPreflight | kSecCSStripDisallowedXattrs);
    if (ret) {
        FAIL("Unable to add adhoc signature to %s", destinationPath);
        goto exit;
    }

    PASS("Successfully validated signing bundle on HFS volume %s", destinationPath);
    ret = 0;

exit:
    _cleanUpHFSDiskImage();
    return ret;
}
#endif // TARGET_OS_OSX

int main(int argc, char* argv[])
{
    fprintf(stdout, "[TEST] secseccodesignerapitest\n\n");

    int i;
    int (*testList[])(void) = {
        // MachO tests.
        CheckRemoveSignatureMachO,
        CheckAddAdhocSignatureMachO,
        CheckAddRSASignatureMachO,
        CheckAddAdhocSignatureMachOAndPageSizeHelperResign,
        CheckAddAdhocSignatureMachOAndPageSizeHelperUnsigned,
        CheckAddAdhocSignatureMachOWithSelfConstraint,
        CheckAddAdhocSignatureMachOWithParentConstraint,
        CheckAddAdhocSignatureMachOWithResponsibleConstraint,
        CheckAddAdhocSignatureMachOPreserveLaunchConstraints,
        CheckAddAdhocSignatureMachoLibraryConstraints,
        CheckReSignPreserveMetadataRuntimeMachO,
        CheckStripDisallowedXattrsOnMachO,

        // Bundle tests.
        CheckRemoveSignatureBundle,
        CheckAddAdhocSignatureBundle,
        CheckAddECCSignatureBundle,
        CheckECCKeychainAndSignatureValidationIntegrationBundle,
        CheckStripDisallowedXattrsOnBundleRoot,
        CheckStripDisallowedXattrsOnBundleMainExecutable,
        CheckStripDisallowedXattrsOnBundleResource,

        // Remote signing.
        CheckRemoteSignatureAPI,
        CheckAddSignatureRemoteMachO,
        CheckAddSignatureRemoteBundle,

        // Tests that are only supported on macOS...
#if TARGET_OS_OSX
        // Encrypted disk image tests
        CheckAddAdhocSignatureEncryptedDiskImage,

        // HFS volume extended attribute signing, which involves dmg mounting.
        CheckStripDisallowedXattrsSigningHFS,
#endif
    };
    const int numberOfTests = sizeof(testList) / sizeof(*testList);
    int testResults[numberOfTests] = {0};

    if (argc >= 2) {
        int testIndex = atoi(argv[1]);
        if (testIndex >= 0 && testIndex < numberOfTests) {
            int result = testList[testIndex]();
            printf("Test %d: %s\n", testIndex, result == 0 ? "Passed" : "Failed");
            return 0;
        }
    }

    for (i = 0; i < numberOfTests; i++) {
        testResults[i] = testList[i]();
    }

    fprintf(stdout, "[SUMMARY]\n");
    for (i = 0; i < numberOfTests; i++) {
        fprintf(stdout, "%d. %s\n", i+1, testResults[i] == 0 ? "Passed" : "Failed");
    }

    return 0;
}
