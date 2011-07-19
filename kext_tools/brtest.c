/*
 * FILE: brtest.c
 * AUTH: Soren Spies (sspies)
 * DATE: 10 March 2011 (Copyright Apple Inc.)
 * DESC: test libBootRoot
 *
 */

// CFLAGS: -lBootRoot

#include "bootroot.h"

#include <bootfiles.h>
#include <err.h>
#include <stdio.h>
#include <strings.h>
#include <sys/stat.h>
#include <sysexits.h>


#include <CoreFoundation/CoreFoundation.h>

void usage(int exval)
{
    fprintf(stderr,
            "Usage: brtest update <vol> -f\n"
            "       brtest listboots <vol>\n"
            "       brtest copyfiles <srcVol> <initRoot> <bootDev> /<rootDMG>\n"
            "              (/<rootDMG> is relative to <initRoot>)\n"
            "       brtest copyfiles <srcVol> <bootDev>\n"
            "       brtest erasefiles <srcVol> <bootDev>\n"

        //  how to prep for installation to a blank Boot!=Root volume?
        //  "       brtest disableHelperUpdates <[src]Vol> [<tgtVol>]\n"
            );
    
    exit(exval);
}

#define STR2URL(str)   CFURLCreateFromFileSystemRepresentation(nil, \
                                            (UInt8*)str, strlen(str, true);


int
update(CFURLRef volURL, int argc, char *argv[])
{
    int result;
    Boolean force = false;
    
    if (argc == 2) {
        if (argv[1][0] == '-' && argv[1][1] == 'f') {
            force = true;
            argv++;
        } else {
            return EINVAL;
        }
    }
    
    result = BRUpdateBootFiles(volURL, force);
    
    return result;
}

#define DEVMAXPATHSIZE  128
int
listboots(char *volpath, CFURLRef volURL)
{
    int result;
    CFArrayRef boots;
    CFIndex i, bcount = 0;
    
    boots = BRCopyActiveBootPartitions(volURL);
    
    if (!boots) {
        printf("%s: no boot partitions\n", volpath);
        result = 0;
        goto finish;
    }

    printf("boot%s for %s:\n", CFArrayGetCount(boots)==1 ? "":"s", volpath);
    bcount = CFArrayGetCount(boots);
    for (i = 0; i < bcount; i++) {
        CFShow(CFArrayGetValueAtIndex(boots, i));  // sufficient?
    }

    result = 0;

finish:
    if (boots)      CFRelease(boots);

    return result;
}

int
copyfilesdmg(CFURLRef srcVol, char *argv[])
{
    int result;
    char *hostpath, *helperName, *dmgpath, path[PATH_MAX];
    struct stat sb;
    CFArrayRef helpers = NULL;
    CFURLRef hostVol = NULL;
    CFStringRef bootDev = NULL;
    CFURLRef rootDMG = NULL;
    CFStringRef rootDMGURLStr;     // no need to release?
    CFStringRef bootArgs = NULL;
    CFMutableDictionaryRef plistOverrides = NULL;

    // could check to make sure hostVol/rootDMG was correct
    hostpath = argv[1];
    helperName = argv[2];
    dmgpath = argv[3];
    strlcpy(path, hostpath, PATH_MAX);
    strlcat(path, dmgpath, PATH_MAX);
    if (stat(path, &sb)) {
        err(EX_NOINPUT, "%s", path);
    }
    
    // build non-URL args
    hostVol = CFURLCreateFromFileSystemRepresentation(nil, (UInt8*)hostpath,
                                                      strlen(hostpath), true);
    if (!hostVol)           goto finish;
    bootDev = CFStringCreateWithFileSystemRepresentation(nil, helperName);
    if (!bootDev)           goto finish;

    if ((helpers = BRCopyActiveBootPartitions(hostVol))) {
        CFRange searchRange = { 0, CFArrayGetCount(helpers) }; 
        if (!CFArrayContainsValue(helpers, searchRange, bootDev)) {
            fprintf(stderr,"!!: %s doesn't support %s; CSFDE will fail !!\n",
                  helperName, hostpath);
        }
        CFRelease(helpers);
    }

/* !! from CFURL.h !!
 * Note that, strictly speaking any leading '/' is not considered part
 * of the URL's path, although its presence or absence determines
 * whether the path is absolute. */
    // an absolute URL appears a critical to get root-dmg=file:///foo.dmg
    if (argv[3][0] != '/')      usage(EX_USAGE);
    rootDMG = CFURLCreateFromFileSystemRepresentation(nil, (UInt8*)dmgpath,
                                                      strlen(dmgpath), false);
    if (!rootDMG)           goto finish;

    // Soren does not understand CFRURL.  An absolute URL isn't enough,
    // it also has to have CFURLGetString() called on it.   Awesome(?)ly,
    // this value could be ignored as it apparently changes the
    // description of rootDMG(!?).  To keep the code clear, we use
    // rootDMGURLStr when creating bootArgs.
    rootDMGURLStr = CFURLGetString(rootDMG);
    if (!rootDMGURLStr)    goto finish;

    // pass arguments to the kernel so it roots off the specified DMG
    // root-dmg must be a URL, not a path, with or w/o /localhost/
    bootArgs = CFStringCreateWithFormat(nil, nil, CFSTR("root-dmg=%@"),
                                        rootDMGURLStr);
    fputc('\t', stderr); CFShow(bootArgs);
    // container-dmg=%@ allows another layer of disk image :P
    if (!bootArgs)          goto finish;
    plistOverrides = CFDictionaryCreateMutable(nil, 1,
                                     &kCFTypeDictionaryKeyCallBacks,
                                     &kCFTypeDictionaryValueCallBacks);
    if (!plistOverrides)    goto finish;
    CFDictionarySetValue(plistOverrides, CFSTR(kKernelFlagsKey), bootArgs);

    
    result = BRCopyBootFiles(srcVol, hostVol, bootDev, plistOverrides);
    
finish:
    if (plistOverrides) CFRelease(plistOverrides);
    if (bootArgs)       CFRelease(bootArgs);
    if (rootDMG)        CFRelease(rootDMG);
    if (bootDev)        CFRelease(bootDev);
    if (hostVol)        CFRelease(hostVol);

    return result;
}

int
erasefiles(char *volpath, CFURLRef srcVol, char *devname)
{
    int result;
    CFStringRef bsdName = NULL;
    CFArrayRef helpers = NULL;
    
    // build args
    bsdName = CFStringCreateWithFileSystemRepresentation(nil, devname);
    
    // BREraseBootFiles() allows erasing an active boot partition
    // because a boot partition can look active even though the on-
    // disk partition type has been reverted to Apple_HFS.
    helpers = BRCopyActiveBootPartitions(srcVol);
    if (helpers) {
        CFRange searchRange = { 0, CFArrayGetCount(helpers) };
        if (CFArrayContainsValue(helpers, searchRange, bsdName)) {
            fprintf(stderr, "%s currently required to boot %s!\n",
                devname, volpath);
            result = /* kPOSIXErrorBase +*/ EBUSY;
            goto finish;
        }
    }

    result = BREraseBootFiles(srcVol, bsdName);
    
finish:
    if (helpers)        CFRelease(helpers);
    if (bsdName)        CFRelease(bsdName);

    return result;
}

int
prepBoot(CFURLRef srcVol, char *devname)
{
    int result;
    CFStringRef bsdName = NULL;
    
    // build args
    bsdName = CFStringCreateWithFileSystemRepresentation(nil, devname);
    
    result = BRCopyBootFiles(srcVol, srcVol, bsdName, NULL);
    
finish:
    if (bsdName)        CFRelease(bsdName);

    return result;
}

int
main(int argc, char *argv[])
{
    int result, exval;
    char *verb, *volpath;
    CFURLRef volURL;       // we don't release this
    
    if (2 == argc && argv[1][0] == '-' && argv[1][1] == 'h')
        usage(EX_OK);
    if (argc < 3)
        usage(EX_USAGE);
    
    verb = argv[1];
    volpath = argv[2];
    volURL = CFURLCreateFromFileSystemRepresentation(nil, (UInt8*)volpath,
                                                     strlen(volpath), true);

    if (strcasecmp(verb, "update") == 0) {
        if (argc < 3 || argc > 4)
            usage(EX_USAGE);
        result = update(volURL, argc-2, argv+2);
    } else if (strcasecmp(verb, "listboots") == 0) {
        if (argc != 3)
            usage(EX_USAGE);
        result = listboots(volpath, volURL);
    } else if (strcasecmp(verb, "copyfiles") == 0) {
        if (argc == 6) {
            result = copyfilesdmg(volURL, argv+2);
        } else if (argc == 4) {
            result = prepBoot(volURL, argv[3]);
        } else {
             usage(EX_USAGE);
        }
    } else if (strcasecmp(verb, "erasefiles") == 0) {
        if (argc != 4)
            usage(EX_USAGE);
        result = erasefiles(argv[2], volURL, argv[3]);
    /* ... other verbs ... */
    } else {
        usage(EX_USAGE);
    }

    printf("brtest function result = %d", result);
    if (result == -1) {
        printf(": errno %d -> %s", errno, strerror(errno));
    } else if (result && result <= ELAST) {
        printf(": %s", strerror(result));
    }
    printf("\n");

    if (result == -1) {
        exval = EX_OSERR;
    } else if (result == EINVAL) {
        exval = EX_USAGE;
    } else if (result) {
        exval = EX_SOFTWARE;
    } else {
        exval = result;
    }

// fprintf(stderr, "check for leaks now\n");
// pause()
    return exval;
}
