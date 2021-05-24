//
//  secstaticcode.cpp
//  secsecstaticcodeapitest
//
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "StaticCode.h"

#include <AssertMacros.h>
#include <sys/xattr.h>
#include <Security/SecCodePriv.h>
#include <Security/SecCode.h>
#include <Security/SecStaticCode.h>

#include "secstaticcode.h"

using namespace CodeSigning;

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
    fprintf(stdout, "[PASS] %s " fmt "\n", __FUNCTION__, ##__VA_ARGS__);    \
})

#define FAIL(fmt, ...)                                                      \
({                                                                          \
    fprintf(stdout, "[FAIL] %s " fmt "\n", __FUNCTION__, ##__VA_ARGS__);    \
})

#define SAFE_RELEASE(x)                                     \
({                                                          \
    if (x) {                                                \
        CFRelease(x);                                       \
        x = NULL;                                           \
    }                                                       \
})

#define kCommandRedirectOutputToDevNULL " >/dev/null 2>&1"
#define BUILD_COMMAND(x)                x kCommandRedirectOutputToDevNULL

#define kFAT32DiskImageFileDirectory    "/tmp"
#define kFAT32DiskImageFileName         "Security_SecStaticCodeAPITest.dmg"
#define kFAT32DiskImageFilePath         kFAT32DiskImageFileDirectory "/" kFAT32DiskImageFileName

#define kFAT32DiskImageVolumeDirectory  "/Volumes"
#define kFAT32DiskImageVolumeName       "SEC_TEST"
#define kFAT32DiskImageVolumePath        kFAT32DiskImageVolumeDirectory "/" kFAT32DiskImageVolumeName

#define kApplicationsPath               "/Applications"
#define kSafariBundleName               "Safari.app"
#define kSafariBundleOnSystemPath       kApplicationsPath "/" kSafariBundleName
#define kSafariBundleOnVolumePath       kFAT32DiskImageVolumePath "/" kSafariBundleName

static void
_cleanUpFAT32DiskImage(void)
{
    // Delete disk image.
    const char *command = BUILD_COMMAND("rm -rf " kFAT32DiskImageFilePath);
    system(command);

    // Detach volume.
    command = BUILD_COMMAND("hdiutil detach " kFAT32DiskImageVolumePath);
    system(command);
}

static int
_createFAT32DiskImage(void)
{
    const char *command = BUILD_COMMAND("hdiutil create -fs FAT32 -size 256m -volname " kFAT32DiskImageVolumeName " " kFAT32DiskImageFilePath);
    return system(command);
}

static int
_attachFAT32DiskImage(void)
{
    const char *command = BUILD_COMMAND("hdiutil attach " kFAT32DiskImageFilePath);
    return system(command);
}

static int
_copySafariBundleToVolume(void)
{
    const char *command = BUILD_COMMAND("cp -R " kSafariBundleOnSystemPath " " kSafariBundleOnVolumePath);
    return system(command);
}

static int
_confirmValidationPolicy(const char *path)
{
    int ret = -1;
    OSStatus status = 0;
    CFRef<CFStringRef> stringPath;
    CFRef<CFURLRef> url;
    CFRef<SecStaticCodeRef> codeRef;
    SecCSFlags createFlags = kSecCSDefaultFlags;
    SecCSFlags validateFlags = kSecCSDefaultFlags | kSecCSStrictValidateStructure;

    require(path, done);

    stringPath.take(CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)path, strlen(path), kCFStringEncodingASCII, false));
    require(stringPath, done);

    url.take(CFURLCreateWithFileSystemPath(kCFAllocatorDefault, stringPath, kCFURLPOSIXPathStyle, false));
    require(url, done);

    status = SecStaticCodeCreateWithPath(url, createFlags, &codeRef.aref());
    require_noerr(status, done);
    require(codeRef, done);

    // Validate binary without kSecCSSkipXattrFiles. Expectation is this should fail.
    status = SecStaticCodeCheckValidity(codeRef, validateFlags, NULL);
    if (!status) {
        INFO("%s validated without kSecCSSkipXattrFiles flag", path);
        goto done;
    }

    // Create codeRef again to clear state.
    status = SecStaticCodeCreateWithPath(url, createFlags, &codeRef.aref());
    require_noerr(status, done);
    require(codeRef, done);

    // Validate binary with kSecCSSkipXattrFiles. Expectation is this should pass.
    validateFlags |= kSecCSSkipXattrFiles;
    status = SecStaticCodeCheckValidity(codeRef, validateFlags, NULL);
    if (status) {
        INFO("%s is not valid even with kSecCSSkipXattrFiles flag", path);
        goto done;
    }

    // Complete.
    ret = 0;

done:
    return ret;
}

static int
CheckCheckValidity_kSecCSSkipXattrFiles(void)
{
    const char *xattrName = NULL;
    uint32_t xattrValue = 0;
    const char *safariBinary = NULL;
    const char *safariCodeResources = NULL;

    BEGIN();

    int ret = -1;

    // Create FAT32 disk image.
    if (_createFAT32DiskImage()) {
        FAIL("_createFAT32DiskImage error");
        goto done;
    }
    INFO("Created " kFAT32DiskImageFilePath);

    // Attach disk image to the system.
    if (_attachFAT32DiskImage()) {
        FAIL("_attachFAT32DiskImage error");
        goto done;
    }
    INFO("Attached " kFAT32DiskImageFilePath " as " kFAT32DiskImageVolumePath);

    // Copy Safari.app to the attached volume.
    if (_copySafariBundleToVolume()) {
        FAIL("_copySafariBundleToVolume error");
        goto done;
    }
    INFO("Copied " kSafariBundleOnSystemPath " to " kSafariBundleOnVolumePath);

    // Write "com.apple.dummy" xattr to Safari.
    xattrName = "com.apple.dummy";
    xattrValue = 0;

    safariBinary = kSafariBundleOnVolumePath "/Contents/MacOS/Safari";
    if (setxattr(safariBinary, xattrName, &xattrValue, sizeof(xattrValue), 0, 0)) {
        FAIL("%s setxattr error: %d [%s]", safariBinary, errno, strerror(errno));
        goto done;
    }
    INFO("Wrote xattr \'%s\' to %s", xattrName, safariBinary);

    safariCodeResources = kSafariBundleOnVolumePath "/Contents/_CodeSignature/CodeResources";
    if (setxattr(safariCodeResources, xattrName, &xattrValue, sizeof(xattrValue), 0, 0)) {
        FAIL("%s setxattr error: %d [%s]", safariCodeResources, errno, strerror(errno));
        goto done;
    }
    INFO("Wrote xattr \'%s\' to %s", xattrName, safariCodeResources);

    // Validate Safari.app with and without kSecCSSkipXattrFiles flag.
    if (_confirmValidationPolicy(kSafariBundleOnVolumePath)) {
        FAIL("%s _confirmValidationPolicy error", kSafariBundleOnVolumePath);
        goto done;
    }
    INFO("Validation policy on %s confirmed", kSafariBundleOnVolumePath);

    PASS("Completed validation policy check with kSecCSSkipXattrFiles");
    ret = 0;

done:
    _cleanUpFAT32DiskImage();
    return ret;
}

static int
CheckAppleProcessNetworkDefault(void)
{
    int ret = -1;
    CFRef<CFURLRef> url;
    CFRef<SecStaticCodeRef> codeRef;
    OSStatus status = 0;
    SecPointer<SecStaticCode> code;

    BEGIN();

    url.take(CFURLCreateWithString(NULL, CFSTR("/Applications/Safari.app"), NULL));
    status = SecStaticCodeCreateWithPath(url, kSecCSDefaultFlags, &codeRef.aref());
    if (status) {
        FAIL("Failed to create SecStaticCode: %d", status);
        goto done;
    }

    code = SecStaticCode::requiredStatic(codeRef);
    if (code->validationCannotUseNetwork()) {
        PASS("apple process cannot use network by default");
        ret = 0;
    } else {
        FAIL("apple process can use network by default");
    }
done:
    return ret;
}

static int
CheckAppleProcessCanUseNetworkWithFlag(void)
{
    int ret = -1;
    CFRef<CFURLRef> url;
    CFRef<SecStaticCodeRef> codeRef;
    OSStatus status = 0;
    SecPointer<SecStaticCode> code;

    BEGIN();

    url.take(CFURLCreateWithString(NULL, CFSTR("/Applications/Safari.app"), NULL));
    status = SecStaticCodeCreateWithPath(url, kSecCSDefaultFlags, &codeRef.aref());
    if (status) {
        FAIL("Failed to create SecStaticCode: %d", status);
        goto done;
    }

    status = SecStaticCodeCheckValidity(codeRef, kSecCSAllowNetworkAccess | kSecCSBasicValidateOnly, NULL);
    if (status) {
        FAIL("Failed to perform basic validation: %d", status);
        goto done;
    }

    code = SecStaticCode::requiredStatic(codeRef);
    if (code->validationCannotUseNetwork()) {
        FAIL("apple process cannot use network with kSecCSAllowNetworkAccess flag");
    } else {
        PASS("apple process can use network with kSecCSAllowNetworkAccess flag");
        ret = 0;
    }

done:
    return ret;
}

static int
CheckAppleProcessCanDenyNetworkWithFlag(void)
{
    int ret = -1;
    CFRef<CFURLRef> url;
    CFRef<SecStaticCodeRef> codeRef;
    OSStatus status = 0;
    SecPointer<SecStaticCode> code;

    BEGIN();

    url.take(CFURLCreateWithString(NULL, CFSTR("/Applications/Safari.app"), NULL));
    status = SecStaticCodeCreateWithPath(url, kSecCSDefaultFlags, &codeRef.aref());
    if (status) {
        FAIL("Failed to create SecStaticCode: %d", status);
        goto done;
    }

    status = SecStaticCodeCheckValidity(codeRef, kSecCSNoNetworkAccess | kSecCSBasicValidateOnly, NULL);
    if (status) {
        FAIL("Failed to perform basic validation: %d", status);
        goto done;
    }

    code = SecStaticCode::requiredStatic(codeRef);
    if (code->validationCannotUseNetwork()) {
        PASS("apple process cannot use network with kSecCSNoNetworkAccess flag");
        ret = 0;
    } else {
        FAIL("apple process can use network with kSecCSNoNetworkAccess flag");
    }

done:
    return ret;
}

static int
Check_SecStaticCodeEnableOnlineNotarizationCheck(void)
{
    int ret = -1;
    CFRef<CFURLRef> url;
    CFRef<SecStaticCodeRef> codeRef;
    OSStatus status = 0;
    SecPointer<SecStaticCode> code;

    BEGIN();

    url.take(CFURLCreateWithString(NULL, CFSTR("/usr/libexec/syspolicyd"), NULL));
    status = SecStaticCodeCreateWithPath(url, kSecCSDefaultFlags, &codeRef.aref());
    if (status) {
        FAIL("Failed to create SecStaticCode: %d", status);
        goto done;
    }

    code = SecStaticCode::requiredStatic(codeRef);
    if (!code) {
        FAIL("Unable to get get SecStaticCode");
        goto done;
    }

    if (isFlagSet(code->getFlags(), kSecCSForceOnlineNotarizationCheck)) {
        FAIL("Default object has notarization flag without asking for it");
        goto done;
    }

    status = SecStaticCodeEnableOnlineNotarizationCheck(codeRef, true);
    if (status) {
        FAIL("Call to SecStaticCodeEnableOnlineNotarizationCheck(true) failed: %d", status);
        goto done;
    }

    if (!isFlagSet(code->getFlags(), kSecCSForceOnlineNotarizationCheck)) {
        FAIL("SecStaticCodeEnableOnlineNotarizationCheck did not set notarization flag");
        goto done;
    }

    status = SecStaticCodeEnableOnlineNotarizationCheck(codeRef, false);
    if (status) {
        FAIL("Call to SecStaticCodeEnableOnlineNotarizationCheck(false) failed: %d", status);
        goto done;
    }

    if (isFlagSet(code->getFlags(), kSecCSForceOnlineNotarizationCheck)) {
        FAIL("SecStaticCodeEnableOnlineNotarizationCheck did not clear notarization flag");
        goto done;
    }

    PASS("SecStaticCodeEnableOnlineNotarizationCheck can control notarization behavior");
    ret = 0;

done:
    return ret;
}

/// Modify the binary at the provided path by overwriting a few bytes in the executable pages (beyond the header).
static void
modifyBinaryPage(const char *path)
{
    UnixPlusPlus::AutoFileDesc fd = UnixPlusPlus::AutoFileDesc();
    fd.open(path, O_RDWR);

    Universal uv = Universal(fd);
    Universal::Architectures architectures;
    uv.architectures(architectures);

    for (Universal::Architectures::const_iterator arch = architectures.begin(); arch != architectures.end(); ++arch) {
        unique_ptr<MachO> slice(uv.architecture(*arch));
        // Skip ahead to about 3 pages into the slice to skip past the header.
        size_t location = slice->offset() + (3 * 0x400);
        INFO("modifying binary at offset: %lx", location);
        lseek(fd, location, SEEK_SET);
        const char *data = "ERROR";
        write(fd, data, strlen(data));
    }
}

static int
CheckSingleResourceValidationAPI(void)
{
    int ret = -1;
    CFRef<CFURLRef> url;
    CFRef<SecStaticCodeRef> codeRef;
    OSStatus status = 0;
    SecPointer<SecStaticCode> code;
    CFErrorRef error = NULL;

    BEGIN();

    url.take(CFURLCreateWithString(NULL, CFSTR("/Applications/Safari.app"), NULL));
    status = SecStaticCodeCreateWithPath(url, kSecCSDefaultFlags, &codeRef.aref());
    if (status) {
        FAIL("Failed to create SecStaticCode: %d", status);
        goto done;
    }

    // Check it returns an error for a non-nested item.
    status = SecStaticCodeValidateResourceWithErrors(codeRef, CFTempURL("/Applications/test"), kSecCSDefaultFlags, &error);
    if (status != errSecParam) {
        FAIL("Failed to reject non-nested path with errSecParam: %d", status);
        goto done;
    }

    // Check it returns an error for a perfect prefix match.
    status = SecStaticCodeValidateResourceWithErrors(codeRef, CFTempURL("/Applications/Safari.app"), kSecCSDefaultFlags, &error);
    if (status != errSecParam) {
        FAIL("Failed to reject exact path match with errSecParam: %d", status);
        goto done;
    }

    // Check it can validate a basic file as a resource.
    status = SecStaticCodeValidateResourceWithErrors(codeRef, CFTempURL("/Applications/Safari.app/Contents/Resources/BuiltInBookmarks_ca.plist"), kSecCSDefaultFlags, &error);
    if (status != errSecSuccess) {
        FAIL("Failed to succeed validation for a basic file: %d", status);
        goto done;
    }

    // Check it can validate the main executable file.
    status = SecStaticCodeValidateResourceWithErrors(codeRef, CFTempURL("/Applications/Safari.app/Contents/MacOS/Safari"), kSecCSDefaultFlags, &error);
    if (status != errSecSuccess) {
        FAIL("Failed to succeed validation for the main executable: %d", status);
        goto done;
    }

    // Check that it fails to validate a non-resource file.
    status = SecStaticCodeValidateResourceWithErrors(codeRef, CFTempURL("/Applications/Safari.app/Contents/Info.plist"), kSecCSDefaultFlags, &error);
    if (status != errSecSuccess) {
        FAIL("Failed to validate Info.plist: %d", status);
        goto done;
    }

    // Check that it can validate a specific sub-bundle directly.
    status = SecStaticCodeValidateResourceWithErrors(codeRef, CFTempURL("/Applications/Safari.app/Contents/XPCServices/com.apple.WebKit.WebContent.Safari.xpc"), kSecCSDefaultFlags, &error);
    if (status != errSecSuccess) {
        FAIL("Failed to succeed validation of nested code: %d", status);
        goto done;
    }

    // Check that it can validate a specific resource within a sub-bundle.
    status = SecStaticCodeValidateResourceWithErrors(codeRef, CFTempURL("/Applications/Safari.app/Contents/XPCServices/com.apple.WebKit.WebContent.Safari.xpc/Contents/Resources/WebContentProcess.nib"), kSecCSDefaultFlags, &error);
    if (status != errSecSuccess) {
        FAIL("Failed to succeed validation of nested code: %d", status);
        goto done;
    }

    // Check it can validate the main executable file of a nested bundle.
    status = SecStaticCodeValidateResourceWithErrors(codeRef, CFTempURL("/Applications/Safari.app/Contents/XPCServices/com.apple.WebKit.WebContent.Safari.xpc/Contents/MacOS/com.apple.WebKit.WebContent.Safari"), kSecCSDefaultFlags, &error);
    if (status != errSecSuccess) {
        FAIL("Failed to succeed validation for the main executable: %d", status);
        goto done;
    }

    system("rm -rf  /tmp/Safari.app");
    system("cp -R /Applications/Safari.app /tmp/");

    // Create new SecStaticCode object referencing app in temp location.
    url.take(CFURLCreateWithString(NULL, CFSTR("/tmp/Safari.app"), NULL));
    status = SecStaticCodeCreateWithPath(url, kSecCSDefaultFlags, &codeRef.aref());
    if (status) {
        FAIL("Failed to create SecStaticCode on temporary app: %d", status);
        goto done;
    }

    // Check it cannot verify a file within an omission hole in the main bundle itself.
    system("cp /usr/bin/ditto /tmp/Safari.app/Contents/.DS_Store");
    status = SecStaticCodeValidateResourceWithErrors(codeRef, CFTempURL("/tmp/Safari.app/Contents/.DS_Store"), kSecCSDefaultFlags, &error);
    if (status != errSecCSResourcesNotSealed) {
        FAIL("Failed to reject binary within an omission hole: %d", status);
        goto done;
    }
    system("rm /Applications/Safari.app/Contents/.DS_Store");

    // Verify that if a file is modified, the resource is no longer valid.
    system("echo 'hello' >> /tmp/Safari.app/Contents/Resources/BuiltInBookmarks_ca.plist");
    status = SecStaticCodeValidateResourceWithErrors(codeRef, CFTempURL("/tmp/Safari.app/Contents/Resources/BuiltInBookmarks_ca.plist"), kSecCSDefaultFlags, &error);
    if (status != errSecCSBadResource) {
        FAIL("Failed to reject validation of a modified resource file: %d", status);
        goto done;
    }
    system("cp /Applications/Safari.app/Contents/Resources/BuiltInBookmarks_ca.plist /tmp/Safari.app/Contents/Resources/BuiltInBookmarks_ca.plist");

    // Verify that if a nested bundle doesn't pass basic validation, its resources cannot be valid.
    system("echo 'hello' >> /tmp/Safari.app/Contents/XPCServices/com.apple.WebKit.WebContent.Safari.xpc/Contents/Info.plist");
    status = SecStaticCodeValidateResourceWithErrors(codeRef, CFTempURL("/tmp/Safari.app/Contents/XPCServices/com.apple.WebKit.WebContent.Safari.xpc/Contents/Resources/WebContentProcess.nib"), kSecCSDefaultFlags, &error);
    if (status != errSecCSInfoPlistFailed) {
        FAIL("Failed to succeed validation of nested code: %d", status);
        goto done;
    }
    system("cp /Applications/Safari.app/Contents/XPCServices/com.apple.WebKit.WebContent.Safari.xpc/Contents/Info.plist /tmp/Safari.app/Contents/XPCServices/com.apple.WebKit.WebContent.Safari.xpc/Contents/Info.plist");

    // Verify that if a nested bundle has its resource modified, its noticed during default validation.
    modifyBinaryPage("/tmp/Safari.app/Contents/XPCServices/com.apple.WebKit.WebContent.Safari.xpc/Contents/MacOS/com.apple.WebKit.WebContent.Safari");
    status = SecStaticCodeValidateResourceWithErrors(codeRef, CFTempURL("/tmp/Safari.app/Contents/XPCServices/com.apple.WebKit.WebContent.Safari.xpc/Contents/MacOS/com.apple.WebKit.WebContent.Safari"), kSecCSDefaultFlags, &error);
    if (status != errSecCSSignatureFailed) {
        FAIL("Failed to detect tampering in main executable: %d", status);
        goto done;
    }

    // And confirm that validating it by the bundle itself behaves the same.
    status = SecStaticCodeValidateResourceWithErrors(codeRef, CFTempURL("/tmp/Safari.app/Contents/XPCServices/com.apple.WebKit.WebContent.Safari.xpc"), kSecCSDefaultFlags, &error);
    if (status != errSecCSSignatureFailed) {
        FAIL("Failed to detect tampering in main executable (bundle): %d", status);
        goto done;
    }

    // Verify that if a nested bundle has its resource modified and the 'fast validation' flag is used, its not noticed during resource validation.
    status = SecStaticCodeValidateResourceWithErrors(codeRef, CFTempURL("/tmp/Safari.app/Contents/XPCServices/com.apple.WebKit.WebContent.Safari.xpc/Contents/MacOS/com.apple.WebKit.WebContent.Safari"), kSecCSFastExecutableValidation, &error);
    if (status != errSecSuccess) {
        FAIL("Failed to allow tampered executable with fast validation: %d", status);
        goto done;
    }

    // And confirm that validating it by the bundle itself behaves the same.
    status = SecStaticCodeValidateResourceWithErrors(codeRef, CFTempURL("/tmp/Safari.app/Contents/XPCServices/com.apple.WebKit.WebContent.Safari.xpc"), kSecCSFastExecutableValidation, &error);
    if (status != errSecSuccess) {
        FAIL("Failed to allow tampered executable with fast validation (bundle): %d", status);
        goto done;
    }
    system("cp /Applications/Safari.app/Contents/XPCServices/com.apple.WebKit.WebContent.Safari.xpc/Contents/MacOS/com.apple.WebKit.WebContent.Safari /tmp/Safari.app/Contents/XPCServices/com.apple.WebKit.WebContent.Safari.xpc/Contents/MacOS/com.apple.WebKit.WebContent.Safari");

    PASS("All SecStaticCodeValidateResourceWithErrors passed");
    ret = 0;

done:
    system("rm -rf  /tmp/Safari.app");
    return ret;
}

static int
CheckSingleResourceValidationAPIPolicy(void)
{
    int ret = -1;
    CFRef<CFURLRef> url;
    CFRef<SecStaticCodeRef> codeRef;
    OSStatus status = 0;
    SecPointer<SecStaticCode> code;
    CFErrorRef error = NULL;

    BEGIN();

    url.take(CFURLCreateWithString(NULL, CFSTR("/System/Applications/Calculator.app"), NULL));
    status = SecStaticCodeCreateWithPath(url, kSecCSDefaultFlags, &codeRef.aref());
    if (status) {
        FAIL("Failed to create SecStaticCode: %d", status);
        goto done;
    }

    status = SecStaticCodeValidateResourceWithErrors(codeRef, CFTempURL("/System/Applications/Calculator.app/Contents/Resources/Speakable.plist"), kSecCSDefaultFlags, &error);
    if (status != errSecSuccess) {
        FAIL("Failed to validate app in ARV: %d", status);
        goto done;
    }

    status = SecStaticCodeValidateResourceWithErrors(codeRef, CFTempURL("/System/Applications/Calculator.app/Contents/Resources/Speakable.plist"), kSecCSSkipRootVolumeExceptions, &error);
    if (status != errSecCSResourcesNotSealed) {
        FAIL("Failed to reject app in ARV with kSecCSSkipRootVolumeExceptions: %d", status);
        goto done;
    }

    url.take(CFURLCreateWithString(NULL, CFSTR("/AppleInternal/Applications/CatalogInspector.app"), NULL));
    status = SecStaticCodeCreateWithPath(url, kSecCSDefaultFlags, &codeRef.aref());
    if (status) {
        FAIL("Failed to create apple internal SecStaticCode: %d", status);
        goto done;
    }

    status = SecStaticCodeValidateResourceWithErrors(codeRef, CFTempURL("/AppleInternal/Applications/CatalogInspector.app/Contents/Resources/pkg.tiff"), kSecCSDefaultFlags, &error);
    if (status != errSecSuccess) {
        FAIL("Failed to validate apple internal app resource: %d", status);
        goto done;
    }

    status = SecStaticCodeValidateResourceWithErrors(codeRef, CFTempURL("/AppleInternal/Applications/CatalogInspector.app/Contents/Resources/pkg.tiff"), kSecCSSkipRootVolumeExceptions, &error);
    if (status != errSecCSResourcesNotSealed) {
        FAIL("Failed to reject apple internal app with kSecCSSkipRootVolumeExceptions: %d", status);
        goto done;
    }

    PASS("All SecStaticCodeValidateResourceWithErrors passed");
    ret = 0;

done:
    return ret;
}

static int
CheckPathHelpers(void)
{
    int ret = -1;
    string remaining;
    bool stopped = false;
    __block int currentIndex = 0;
    __block bool failed = false;

    static const char *expectedResults[] = {
        "/Applications/Safari.app/Contents/XPCServices/com.apple.WebKit.WebContent.Safari.xpc/Contents/Resources",
        "/Applications/Safari.app/Contents/XPCServices/com.apple.WebKit.WebContent.Safari.xpc/Contents",
        "/Applications/Safari.app/Contents/XPCServices/com.apple.WebKit.WebContent.Safari.xpc",
        "/Applications/Safari.app/Contents/XPCServices",
        "/Applications/Safari.app/Contents",
        "/Applications/Safari.app",
        "/Applications",
    };

    BEGIN();

    if (isPathPrefix("", "")) {
        FAIL("invalid empty prefixes were not rejected");
        goto done;
    }

    if (isPathPrefix("/Applications", "/Applications")) {
        FAIL("matching prefix was not rejected");
        goto done;
    }

    if (isPathPrefix("/somewhere", "/something")) {
        FAIL("invalid prefix was not rejected");
        goto done;
    }

    if (isPathPrefix("/Applications/Safari.app", "/Applications/Safari.application")) {
        FAIL("prefix not on a directory boundary was not rejected");
        goto done;
    }

    if (!isPathPrefix("/Applications/Safari.app", "/Applications/Safari.app/Contents")) {
        FAIL("proper prefix was not detected");
        goto done;
    }

    if (!isPathPrefix("/Applications/Safari.app/", "/Applications/Safari.app/Contents")) {
        FAIL("proper prefix was not detected");
        goto done;
    }

    remaining = pathRemaining("", "");
    if (remaining != "") {
        FAIL("empty arguments are handled incorrectly: %s", remaining.c_str());
        goto done;
    }

    remaining = pathRemaining("/something", "/");
    if (remaining != "something") {
        FAIL("empty prefix path is handled incorrectly: %s", remaining.c_str());
        goto done;
    }

    remaining = pathRemaining("/Applications/Safari.app/Contents/Info.plist", "/Applications/Safari.app");
    if (remaining != "Contents/Info.plist") {
        FAIL("simple resource path remaining was wrong: %s", remaining.c_str());
        goto done;
    }

    remaining = pathRemaining("/Applications/Safari.app/Contents/Info.plist", "/Applications/Safari.app/");
    if (remaining != "Contents/Info.plist") {
        FAIL("prefix path with trailing slash is not handled properly: %s", remaining.c_str());
        goto done;
    }

    remaining = pathRemaining("Resources/", "Resources/");
    if (remaining != "") {
        FAIL("exact paths with trailing slash is not handled properly: %s", remaining.c_str());
        goto done;
    }

    remaining = pathRemaining("Resources/a", "Resources/a");
    if (remaining != "") {
        FAIL("exact paths don't produce empty remaining path: %s", remaining.c_str());
        goto done;
    }

    remaining = pathRemaining("full", "prefix-is-longer");
    if (remaining != "") {
        FAIL("shorter full path doesn't produce empty remaining path: %s", remaining.c_str());
        goto done;
    }

    stopped = iterateLargestSubpaths("/Applications/Safari.app/Contents/XPCServices/com.apple.WebKit.WebContent.Safari.xpc/Contents/Resources/WebContentProcess.nib", ^bool(string p) {
        if ((p != expectedResults[currentIndex]) || (currentIndex > 6)) {
            FAIL("unexpected result: %d, %s", currentIndex, p.c_str());
            failed = true;
            return false;
        } else {
            currentIndex += 1;
            return true;
        }
    });
    if (failed) {
        goto done;
    }
    failed = false;

    stopped = iterateLargestSubpaths("WebContentProcess.nib", ^bool(string p) {
        FAIL("unexpected call to block on empty iteration: %s", p.c_str());
        return true;
    });
    if (stopped) {
        FAIL("iterate reported stopped when it wasn't");
        goto done;
    } else if (failed) {
        goto done;
    }

    PASS("All path helper tests succeeded.");
    ret = 0;

done:
    return ret;
}

static int
CheckValidityWithRevocationTraversal(void)
{
    int ret = -1;
    CFRef<CFURLRef> url;
    CFRef<SecStaticCodeRef> codeRef;
    OSStatus status = 0;
    SecPointer<SecStaticCode> code;

    BEGIN();

    url.take(CFURLCreateWithString(NULL, CFSTR("/Applications/Safari.app"), NULL));
    status = SecStaticCodeCreateWithPath(url, kSecCSDefaultFlags, &codeRef.aref());
    if (status) {
        FAIL("Failed to create SecStaticCode: %d", status);
        goto done;
    }

    status = SecStaticCodeCheckValidity(codeRef, kSecCSEnforceRevocationChecks, NULL);
    if (status) {
        FAIL("Unable to validate Safari with kSecCSEnforceRevocationChecks: %d", status);
        goto done;
    }

    PASS("SecStaticCodeCheckValidity with kSecCSEnforceRevocationChecks works");
    ret = 0;

done:
    return ret;
}

static int
CheckUnsignedProcessNetworkByDefault(void)
{
    int ret = -1;
    CFRef<CFURLRef> url;
    CFRef<SecStaticCodeRef> codeRef;
    OSStatus status = 0;
    SecPointer<SecStaticCode> code;

    BEGIN();

    url.take(CFURLCreateWithString(NULL, CFSTR("/Applications/Safari.app"), NULL));
    status = SecStaticCodeCreateWithPath(url, kSecCSDefaultFlags, &codeRef.aref());
    if (status) {
        FAIL("Failed to create SecStaticCode: %d", status);
        goto done;
    }

    code = SecStaticCode::requiredStatic(codeRef);
    if (code->validationCannotUseNetwork()) {
        FAIL("unsigned process does not use network by default");
    } else {
        PASS("unsigned process uses network by default");
        ret = 0;
    }

done:
    return ret;
}

static int
CheckUnsignedProcessCanDenyNetworkWithFlag(void)
{
    int ret = -1;
    CFRef<CFURLRef> url;
    CFRef<SecStaticCodeRef> codeRef;
    OSStatus status = 0;
    SecPointer<SecStaticCode> code;

    BEGIN();

    url.take(CFURLCreateWithString(NULL, CFSTR("/Applications/Safari.app"), NULL));
    status = SecStaticCodeCreateWithPath(url, kSecCSDefaultFlags, &codeRef.aref());
    if (status) {
        FAIL("Failed to create SecStaticCode: %d", status);
        goto done;
    }

    status = SecStaticCodeCheckValidity(codeRef, kSecCSNoNetworkAccess | kSecCSBasicValidateOnly, NULL);
    if (status) {
        FAIL("Failed to perform basic validation: %d", status);
        goto done;
    }

    code = SecStaticCode::requiredStatic(codeRef);
    if (code->validationCannotUseNetwork()) {
        PASS("unsigned process can block network with kSecCSNoNetworkAccess flag");
        ret = 0;
    } else {
        FAIL("unsigned process can block network with kSecCSNoNetworkAccess flag");
    }

done:
    return ret;
}

static int runTests(int (*testList[])(void), int testCount)
{
    fprintf(stdout, "[TEST] secsecstaticcodeapitest\n");

    int *testResults = (int *)malloc(sizeof(int) * testCount);

    for (int i = 0; i < testCount; i++) {
        testResults[i] = testList[i]();
    }

    fprintf(stdout, "[SUMMARY]\n");
    for (int i = 0; i < testCount; i++) {
        fprintf(stdout, "%d. %s\n", i+1, testResults[i] == 0 ? "Passed" : "Failed");
    }

    free(testResults);
    return 0;
}

int main(int argc, const char *argv[])
{
    static int (*signedTestList[])(void) = {
        CheckCheckValidity_kSecCSSkipXattrFiles,
        CheckAppleProcessNetworkDefault,
        CheckAppleProcessCanUseNetworkWithFlag,
        CheckAppleProcessCanDenyNetworkWithFlag,
        Check_SecStaticCodeEnableOnlineNotarizationCheck,
        CheckSingleResourceValidationAPI,
        CheckSingleResourceValidationAPIPolicy,
        CheckPathHelpers,
        CheckValidityWithRevocationTraversal,
    };

    static int (*unsignedTestList[])(void) = {
        CheckUnsignedProcessNetworkByDefault,
        CheckUnsignedProcessCanDenyNetworkWithFlag,
    };

    const int numberOfSignedTests = sizeof(signedTestList) / sizeof(*signedTestList);
    const int numberOfUnsignedTests = sizeof(unsignedTestList) / sizeof(*unsignedTestList);

    bool runUnsignedTests = false;
    if (argc == 2) {
        if (strcmp(argv[1], "unsigned") == 0) {
            INFO("Running unsigned variant of tests");
            runUnsignedTests = true;
        } else {
            INFO("Running integration tests with content root: %s", argv[1]);
            return run_integration_tests(argv[1]);
        }
    }

    if (runUnsignedTests) {
        return runTests(unsignedTestList, numberOfUnsignedTests);
    } else {
        return runTests(signedTestList, numberOfSignedTests);
    }
}
