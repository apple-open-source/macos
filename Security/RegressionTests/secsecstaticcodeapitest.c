//
//  secsecstaticcodeapitest.c
//  secsecstaticcodeapitest
//

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <AssertMacros.h>
#include <sys/xattr.h>
#include <Security/SecCodePriv.h>
#include <Security/SecCode.h>
#include <Security/SecStaticCode.h>

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
    SecStaticCodeRef codeRef = NULL;
    SecCSFlags createFlags = kSecCSDefaultFlags;
    SecCSFlags validateFlags = kSecCSDefaultFlags | kSecCSStrictValidateStructure;

    CFStringRef stringRef = NULL;
    CFURLRef pathRef = NULL;

    require(path, done);

    stringRef = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)path, strlen(path), kCFStringEncodingASCII, false);
    require(stringRef, done);

    pathRef = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, stringRef, kCFURLPOSIXPathStyle, false);
    require(pathRef, done);

    OSStatus status = SecStaticCodeCreateWithPath(pathRef, createFlags, &codeRef);
    require_noerr(status, done);
    require(codeRef, done);

    // Validate binary without kSecCSSkipXattrFiles. Expectation is this should fail.
    status = SecStaticCodeCheckValidity(codeRef, validateFlags, NULL);
    if (!status) {
        INFO("%s validated without kSecCSSkipXattrFiles flag", path);
        goto done;
    }
    SAFE_RELEASE(codeRef);

    // Create codeRef again to clear state.
    status = SecStaticCodeCreateWithPath(pathRef, createFlags, &codeRef);
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
    SAFE_RELEASE(codeRef);
    SAFE_RELEASE(stringRef);
    SAFE_RELEASE(pathRef);
    return ret;
}

static int
CheckCheckValidity_kSecCSSkipXattrFiles(void)
{
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
    const char *xattrName = "com.apple.dummy";
    const uint32_t xattrValue = 0;

    const char *safariBinary = kSafariBundleOnVolumePath "/Contents/MacOS/Safari";
    if (setxattr(safariBinary, xattrName, &xattrValue, sizeof(xattrValue), 0, 0)) {
        FAIL("%s setxattr error: %d [%s]", safariBinary, errno, strerror(errno));
        goto done;
    }
    INFO("Wrote xattr \'%s\' to %s", xattrName, safariBinary);

    const char *safariCodeResources = kSafariBundleOnVolumePath "/Contents/_CodeSignature/CodeResources";
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

int main(void)
{
    fprintf(stdout, "[TEST] secsecstaticcodeapitest\n");

    int i;
    int (*testList[])(void) = {
        CheckCheckValidity_kSecCSSkipXattrFiles
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
