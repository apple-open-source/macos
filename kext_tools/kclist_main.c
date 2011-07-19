/*
 *  kclist_main.c
 *  kext_tools
 *
 *  Created by Nik Gervae on 2010 10 04.
 *  Copyright 2010 Apple Computer, Inc. All rights reserved.
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <System/libkern/OSKextLibPrivate.h>
#include <System/libkern/prelink.h>
#include <IOKit/kext/OSKext.h>
#include <IOKit/kext/OSKextPrivate.h>
#include <IOKit/IOCFUnserialize.h>
#include <IOKit/kext/macho_util.h>

#include <architecture/byte_order.h>
#include <errno.h>
#include <libc.h>
#include <mach-o/fat.h>
#include <sys/mman.h>

#include "kclist_main.h"
#include "compression.h"

/*******************************************************************************
*******************************************************************************/
extern void printPList_new(FILE * stream, CFPropertyListRef plist, int style);

/*******************************************************************************
* Program Globals
*******************************************************************************/
const char * progname = "(unknown)";

/*******************************************************************************
*******************************************************************************/
int main(int argc, char * const argv[])
{
    ExitStatus           result             = EX_SOFTWARE;
    KclistArgs           toolArgs;
    int                  kernelcache_fd     = -1;  // must close()
    void               * fat_header         = NULL;  // must unmapFatHeaderPage()
    struct fat_arch    * fat_arch           = NULL;
    CFDataRef            rawKernelcache     = NULL;  // must release
    CFDataRef            kernelcacheImage   = NULL;  // must release
    const UInt8        * kernelcacheStart   = NULL;

    void               * prelinkInfoSect = NULL;

    const char         * prelinkInfoBytes = NULL;
    CFPropertyListRef    prelinkInfoPlist = NULL;  // must release
    
    bzero(&toolArgs, sizeof(toolArgs));

   /*****
    * Find out what the program was invoked as.
    */
    progname = rindex(argv[0], '/');
    if (progname) {
        progname++;   // go past the '/'
    } else {
        progname = (char *)argv[0];
    }

   /* Set the OSKext log callback right away.
    */
    OSKextSetLogOutputFunction(&tool_log);

   /*****
    * Process args & check for permission to load.
    */
    result = readArgs(&argc, &argv, &toolArgs);
    if (result != EX_OK) {
        if (result == kKclistExitHelp) {
            result = EX_OK;
        }
        goto finish;
    }

    result = checkArgs(&toolArgs);
    if (result != EX_OK) {
        if (result == kKclistExitHelp) {
            result = EX_OK;
        }
        goto finish;
    }

    kernelcache_fd = open(toolArgs.kernelcachePath, O_RDONLY);
    if (kernelcache_fd == -1) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Can't open %s: %s.", toolArgs.kernelcachePath, strerror(errno));
        result = EX_OSERR;
        goto finish;
    }
    fat_header = mapAndSwapFatHeaderPage(kernelcache_fd);
    if (!fat_header) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Can't map %s: %s.", toolArgs.kernelcachePath, strerror(errno));
        result = EX_OSERR;
        goto finish;
    }

    fat_arch = getFirstFatArch(fat_header);
    if (fat_arch && !toolArgs.archInfo) {
        toolArgs.archInfo = NXGetArchInfoFromCpuType(fat_arch->cputype, fat_arch->cpusubtype);
    }
    
    rawKernelcache = readMachOSliceForArch(toolArgs.kernelcachePath, toolArgs.archInfo,
        /* checkArch */ FALSE);
    if (!rawKernelcache) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Can't read arch %s from %s.", toolArgs.archInfo->name, toolArgs.kernelcachePath);
        goto finish;
    }

    if (MAGIC32(CFDataGetBytePtr(rawKernelcache)) == OSSwapHostToBigInt32('comp')) {
        kernelcacheImage = uncompressPrelinkedSlice(rawKernelcache);
        if (!kernelcacheImage) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                "Can't uncompress kernelcache slice.");
            goto finish;
        }
    } else {
        kernelcacheImage = CFRetain(rawKernelcache);
    }

    kernelcacheStart = CFDataGetBytePtr(kernelcacheImage);
    
    if (ISMACHO64(MAGIC32(kernelcacheStart))) {
        prelinkInfoSect = (void *)macho_get_section_by_name_64(
            (struct mach_header_64 *)kernelcacheStart,
            kPrelinkInfoSegment, kPrelinkInfoSection);

    } else {
        prelinkInfoSect = (void *)macho_get_section_by_name(
            (struct mach_header *)kernelcacheStart,
            kPrelinkInfoSegment, kPrelinkInfoSection);
    }

    if (!prelinkInfoSect) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Can't find prelink info section.");
        goto finish;
    }

    if (ISMACHO64(MAGIC32(kernelcacheStart))) {
        prelinkInfoBytes = ((char *)kernelcacheStart) +
            ((struct section_64 *)prelinkInfoSect)->offset;
    } else {
        prelinkInfoBytes = ((char *)kernelcacheStart) +
            ((struct section *)prelinkInfoSect)->offset;
    }

    prelinkInfoPlist = (CFPropertyListRef)IOCFUnserialize(prelinkInfoBytes,
        kCFAllocatorDefault, /* options */ 0, /* errorString */ NULL);
    if (!prelinkInfoPlist) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Can't unserialize prelink info.");
        goto finish;
    }

    listPrelinkedKexts(&toolArgs, prelinkInfoPlist);

    result = EX_OK;

finish:

    SAFE_RELEASE(prelinkInfoPlist);
    SAFE_RELEASE(kernelcacheImage);
    SAFE_RELEASE(rawKernelcache);

    if (fat_header) {
        unmapFatHeaderPage(fat_header);
    }

    if (kernelcache_fd != -1) {
        close(kernelcache_fd);
    }
    return result;
}

/*******************************************************************************
*******************************************************************************/
ExitStatus readArgs(
    int            * argc,
    char * const  ** argv,
    KclistArgs     * toolArgs)
{
    ExitStatus   result         = EX_USAGE;
    ExitStatus   scratchResult  = EX_USAGE;
    int          optchar        = 0;
    int          longindex      = -1;

    bzero(toolArgs, sizeof(*toolArgs));
    
   /*****
    * Allocate collection objects.
    */
    if (!createCFMutableSet(&toolArgs->kextIDs, &kCFTypeSetCallBacks)) {

        OSKextLogMemError();
        result = EX_OSERR;
        exit(result);
    }

    /*****
    * Process command line arguments.
    */
    while ((optchar = getopt_long_only(*argc, *argv,
        kOptChars, sOptInfo, &longindex)) != -1) {

        switch (optchar) {
  
            case kOptArch:
                toolArgs->archInfo = NXGetArchInfoFromName(optarg);
                if (!toolArgs->archInfo) {
                    OSKextLog(/* kext */ NULL,
                        kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                        "Unknown architecture %s.", optarg);
                    goto finish;
                }
                break;
  
            case kOptHelp:
                usage(kUsageLevelFull);
                result = kKclistExitHelp;
                goto finish;
    
            default:
                OSKextLog(/* kext */ NULL,
                    kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                    "unrecognized option %s", (*argv)[optind-1]);
                goto finish;
                break;

        }
        
       /* Reset longindex, because getopt_long_only() is stupid and doesn't.
        */
        longindex = -1;
    }

   /*****
    * Record remaining args from the command line.
    */
    for ( /* optind already set */ ; optind < *argc; optind++) {
        if (!toolArgs->kernelcachePath) {
            toolArgs->kernelcachePath = (*argv)[optind];
        } else {
            CFStringRef scratchString = CFStringCreateWithCString(kCFAllocatorDefault,
                (*argv)[optind], kCFStringEncodingUTF8);
            if (!scratchString) {
                result = EX_OSERR;
                OSKextLogMemError();
                goto finish;
            }
            CFSetAddValue(toolArgs->kextIDs, scratchString);
            CFRelease(scratchString);
        }
    }

   /* Update the argc & argv seen by main() so that boot<>root calls
    * handle remaining args.
    */
    *argc -= optind;
    *argv += optind;

    result = EX_OK;

finish:
    if (result == EX_USAGE) {
        usage(kUsageLevelBrief);
    }
    return result;
}

/*******************************************************************************
*******************************************************************************/
ExitStatus checkArgs(KclistArgs * toolArgs)
{
    ExitStatus result = EX_USAGE;

    if (!toolArgs->kernelcachePath) {

        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "No kernelcache file specified.");
        goto finish;
    }

    result = EX_OK;

finish:
    if (result == EX_USAGE) {
        usage(kUsageLevelBrief);
    }
    return result;
}

/*******************************************************************************
*******************************************************************************/
void listPrelinkedKexts(KclistArgs * toolArgs, CFPropertyListRef kcInfoPlist)
{
    CFIndex i, count;
    Boolean haveIDs = CFSetGetCount(toolArgs->kextIDs) > 0 ? TRUE : FALSE;
    CFArrayRef kextPlistArray = NULL;
    
    if (CFArrayGetTypeID() == CFGetTypeID(kcInfoPlist)) {
        kextPlistArray = (CFArrayRef)kcInfoPlist;
    } else if (CFDictionaryGetTypeID() == CFGetTypeID(kcInfoPlist)){
        kextPlistArray = (CFArrayRef)CFDictionaryGetValue(kcInfoPlist,
            CFSTR("_PrelinkInfoDictionary"));
    } else {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Unrecognized kernelcache plist data.");
        goto finish;
    }
    
    count = CFArrayGetCount(kextPlistArray);
    for (i = 0; i < count; i++) {
        CFDictionaryRef kextPlist = (CFDictionaryRef)CFArrayGetValueAtIndex(kextPlistArray, i);
        CFStringRef kextIdentifier = (CFStringRef)CFDictionaryGetValue(kextPlist, kCFBundleIdentifierKey);
        
        if (haveIDs && !CFSetContainsValue(toolArgs->kextIDs, kextIdentifier)) {
            continue;
        }
        
        printKextInfo(kextPlist);
    }

finish:
    return;
}

/*******************************************************************************
*******************************************************************************/
void printKextInfo(CFDictionaryRef kextPlist)
{
    CFStringRef kextIdentifier = (CFStringRef)CFDictionaryGetValue(kextPlist, kCFBundleIdentifierKey);
    CFStringRef kextVersion = (CFStringRef)CFDictionaryGetValue(kextPlist, kCFBundleVersionKey);
    CFStringRef kextPath = (CFStringRef)CFDictionaryGetValue(kextPlist, CFSTR("_PrelinkBundlePath"));
    char idBuffer[KMOD_MAX_NAME];
    char versionBuffer[KMOD_MAX_NAME];
    char pathBuffer[PATH_MAX];
    
    if (!kextIdentifier || !kextVersion || !kextPath) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Missing identifier, version, or path.");
        goto finish;
    }
    CFStringGetCString(kextIdentifier, idBuffer, sizeof(idBuffer), kCFStringEncodingUTF8);
    CFStringGetCString(kextVersion, versionBuffer, sizeof(versionBuffer), kCFStringEncodingUTF8);
    CFStringGetCString(kextPath, pathBuffer, sizeof(pathBuffer), kCFStringEncodingUTF8);
    
    printf("%s\t%s\t%s\n", idBuffer, versionBuffer, pathBuffer);

finish:
    return;
}

/*******************************************************************************
*******************************************************************************/
CFComparisonResult compareIdentifiers(const void * val1, const void * val2, void * context __unused)
{
    CFDictionaryRef dict1 = (CFDictionaryRef)val1;
    CFDictionaryRef dict2 = (CFDictionaryRef)val2;
    
    CFStringRef id1 = CFDictionaryGetValue(dict1, kCFBundleIdentifierKey);
    CFStringRef id2 = CFDictionaryGetValue(dict2, kCFBundleIdentifierKey);
    
    return CFStringCompare(id1, id2, 0);
}

/*******************************************************************************
* usage()
*******************************************************************************/
void usage(UsageLevel usageLevel)
{
    fprintf(stderr,
      "usage: %1$s [-arch archname] [--] kernelcache [bundle-id ...]\n"
      "usage: %1$s -help\n"
      "\n",
      progname);

    if (usageLevel == kUsageLevelBrief) {
        fprintf(stderr, "use %s -%s for an explanation of each option\n",
            progname, kOptNameHelp);
    }

    if (usageLevel == kUsageLevelBrief) {
        return;
    }

    fprintf(stderr, "-%s <archname>:\n"
        "        list info for architecture <archname>\n",
        kOptNameArch);
    fprintf(stderr, "\n");
   
    fprintf(stderr, "-%s (-%c): print this message and exit\n",
        kOptNameHelp, kOptHelp);

    return;
}
