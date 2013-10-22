//
//  iOSforOSX.c
//  utilities
//
//  Created by J Osborne on 11/13/12.
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#include <TargetConditionals.h>

#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))

#include <CoreFoundation/CoreFoundation.h>
#include <AssertMacros.h>
#include <utilities/SecCFWrappers.h>

#include <sys/types.h>
#include <pwd.h>
#include <uuid/uuid.h>
#include "iOSforOSX.h"
#include <pwd.h>
#include <unistd.h>

#include ".././libsecurity_keychain/lib/SecBase64P.c"

CFURLRef SecCopyKeychainDirectoryFile(CFStringRef file)
{
    struct passwd *passwd = getpwuid(getuid());
    if (!passwd)
        return NULL;
    
    CFURLRef pathURL = NULL;
    CFURLRef fileURL = NULL;
    CFStringRef home = NULL;
    CFStringRef filePath = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%s/%@"), "Library/Keychains", file);
    require(filePath, xit);
    
    if (passwd->pw_dir)
        home = CFStringCreateWithCString(NULL, passwd->pw_dir, kCFStringEncodingUTF8);

    pathURL = CFURLCreateWithFileSystemPath(NULL, home?home:CFSTR("/"), kCFURLPOSIXPathStyle, true);
    if (pathURL)
        fileURL = CFURLCreateCopyAppendingPathComponent(kCFAllocatorDefault, pathURL, filePath, false);

xit:
    CFReleaseSafe(filePath);
    CFReleaseSafe(pathURL);
    CFReleaseSafe(home);
    return fileURL;
}

// XXX: do we still need this?  see securityd_files?
CFURLRef PortableCFCopyHomeDirectoryURL(void)
{
    char *path = getenv("HOME");
    if (!path) {
        struct passwd *pw = getpwuid(getuid());
        path = pw->pw_dir;
    }
    CFStringRef path_cf = CFStringCreateWithCStringNoCopy(NULL, path, kCFStringEncodingUTF8, kCFAllocatorNull);
    CFURLRef path_url = CFURLCreateWithFileSystemPath(NULL, path_cf, kCFURLPOSIXPathStyle, true);
    
    CFRelease(path_cf);
    return path_url;
}

#endif
