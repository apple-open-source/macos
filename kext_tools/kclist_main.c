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
#include <mach-o/loader.h>
#include <sys/mman.h>
#include <uuid/uuid.h>

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

    void               * prelinkTextSect = NULL;
    const char         * prelinkTextBytes = NULL;
    uint64_t             prelinkTextSourceAddress = 0;
    uint64_t             prelinkTextSourceSize = 0;
    
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
        prelinkTextSect = (void *)macho_get_section_by_name_64(
            (struct mach_header_64 *)kernelcacheStart,
            kPrelinkTextSegment, kPrelinkTextSection);
    } else {
        prelinkInfoSect = (void *)macho_get_section_by_name(
            (struct mach_header *)kernelcacheStart,
            kPrelinkInfoSegment, kPrelinkInfoSection);
        prelinkTextSect = (void *)macho_get_section_by_name(
            (struct mach_header *)kernelcacheStart,
            kPrelinkTextSegment, kPrelinkTextSection);
    }

    if (!prelinkInfoSect) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Can't find prelink info section.");
        goto finish;
    }

    if (!prelinkTextSect) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Can't find prelink text section.");
        goto finish;
    }

    if (ISMACHO64(MAGIC32(kernelcacheStart))) {
        prelinkInfoBytes = ((char *)kernelcacheStart) +
            ((struct section_64 *)prelinkInfoSect)->offset;
        prelinkTextBytes = ((char *)kernelcacheStart) +
            ((struct section_64 *)prelinkTextSect)->offset;
        prelinkTextSourceAddress = ((struct section_64 *)prelinkTextSect)->addr;
        prelinkTextSourceSize = ((struct section_64 *)prelinkTextSect)->size;
    } else {
        prelinkInfoBytes = ((char *)kernelcacheStart) +
            ((struct section *)prelinkInfoSect)->offset;
        prelinkTextBytes = ((char *)kernelcacheStart) +
            ((struct section *)prelinkTextSect)->offset;
        prelinkTextSourceAddress = ((struct section *)prelinkTextSect)->addr;
        prelinkTextSourceSize = ((struct section *)prelinkTextSect)->size;
    }

    prelinkInfoPlist = (CFPropertyListRef)IOCFUnserialize(prelinkInfoBytes,
        kCFAllocatorDefault, /* options */ 0, /* errorString */ NULL);
    if (!prelinkInfoPlist) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Can't unserialize prelink info.");
        goto finish;
    }

    listPrelinkedKexts(&toolArgs, prelinkInfoPlist, prelinkTextBytes, prelinkTextSourceAddress, prelinkTextSourceSize);

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
    
            case kOptUUID:
                toolArgs->printUUIDs = true;
                break;
                
            case kOptVerbose:
                toolArgs->verbose = true;
                break;
                
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
void listPrelinkedKexts(KclistArgs * toolArgs, CFPropertyListRef kcInfoPlist, const char *prelinkTextBytes, uint64_t prelinkTextSourceAddress, uint64_t prelinkTextSourceSize)
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
        CFNumberRef kextSourceAddress = (CFNumberRef)CFDictionaryGetValue(kextPlist, CFSTR(kPrelinkExecutableSourceKey));
        CFNumberRef kextSourceSize = (CFNumberRef)CFDictionaryGetValue(kextPlist, CFSTR(kPrelinkExecutableSizeKey));
        const char *kextTextBytes = NULL;
        
        if (haveIDs && !CFSetContainsValue(toolArgs->kextIDs, kextIdentifier)) {
            continue;
        }

        if (kextSourceAddress && CFNumberGetTypeID() == CFGetTypeID(kextSourceAddress) &&
            kextSourceSize && CFNumberGetTypeID() == CFGetTypeID(kextSourceSize)) {
            uint64_t sourceAddress;
            uint64_t sourceSize;
  
            CFNumberGetValue(kextSourceAddress, kCFNumberSInt64Type, &sourceAddress);
            CFNumberGetValue(kextSourceSize, kCFNumberSInt64Type, &sourceSize);
            if ((sourceAddress >= prelinkTextSourceAddress) &&
                ((sourceAddress+sourceSize) <= (prelinkTextSourceAddress + prelinkTextSourceSize))) {
                kextTextBytes = prelinkTextBytes + (ptrdiff_t)(sourceAddress - prelinkTextSourceAddress);
            }
        }
        
        printKextInfo(kextPlist, toolArgs->verbose, toolArgs->printUUIDs, kextTextBytes);
    }

finish:
    return;
}

/*******************************************************************************
*******************************************************************************/
void printKextInfo(CFDictionaryRef kextPlist, Boolean beVerbose, Boolean printUUIDs, const char *kextTextBytes)
{
    CFStringRef kextIdentifier = (CFStringRef)CFDictionaryGetValue(kextPlist, kCFBundleIdentifierKey);
    CFStringRef kextVersion = (CFStringRef)CFDictionaryGetValue(kextPlist, kCFBundleVersionKey);
    CFStringRef kextPath = (CFStringRef)CFDictionaryGetValue(kextPlist, CFSTR("_PrelinkBundlePath"));
    char idBuffer[KMOD_MAX_NAME];
    char versionBuffer[KMOD_MAX_NAME];
    char pathBuffer[PATH_MAX];

    CFNumberRef cfNum;
    uint64_t  kextLoadAddress = 0x0;
    uint64_t  kextSourceAddress = 0x0;
    uint64_t  kextExecutableSize = 0;
    uint64_t  kextKmodInfoAddress = 0x0;

    struct load_command *lcp;
    struct uuid_command *uuid_cmd = NULL;
    uint32_t ncmds, cmd_i;
    
    if (!kextIdentifier || !kextVersion || !kextPath) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Missing identifier, version, or path.");
        goto finish;
    }
    CFStringGetCString(kextIdentifier, idBuffer, sizeof(idBuffer), kCFStringEncodingUTF8);
    CFStringGetCString(kextVersion, versionBuffer, sizeof(versionBuffer), kCFStringEncodingUTF8);
    CFStringGetCString(kextPath, pathBuffer, sizeof(pathBuffer), kCFStringEncodingUTF8);
        
    if (NULL != (cfNum = CFDictionaryGetValue(kextPlist, CFSTR(kPrelinkExecutableLoadKey))))
        CFNumberGetValue(cfNum, kCFNumberSInt64Type, &kextLoadAddress);
    if (NULL != (cfNum = CFDictionaryGetValue(kextPlist, CFSTR(kPrelinkExecutableSourceKey))))
        CFNumberGetValue(cfNum, kCFNumberSInt64Type, &kextSourceAddress);
    if (NULL != (cfNum = CFDictionaryGetValue(kextPlist, CFSTR(kPrelinkExecutableSizeKey))))
        CFNumberGetValue(cfNum, kCFNumberSInt64Type, &kextExecutableSize);
    if (NULL != (cfNum = CFDictionaryGetValue(kextPlist, CFSTR(kPrelinkKmodInfoKey))))
        CFNumberGetValue(cfNum, kCFNumberSInt64Type, &kextKmodInfoAddress);

    if (kextTextBytes) {
        if (ISMACHO64(MAGIC32(kextTextBytes))) {
            struct mach_header_64 *mhp64 = (struct mach_header_64 *)kextTextBytes;
            ncmds = mhp64->ncmds;
            lcp = (struct load_command *)(void *)(mhp64 + 1);
        } else {
            struct mach_header *mhp = (struct mach_header *)kextTextBytes;
            ncmds = mhp->ncmds;
            lcp = (struct load_command *)(void *)(mhp + 1);
        }

        for (cmd_i = 0; cmd_i < ncmds; cmd_i++) {
            if (lcp->cmd == LC_UUID) {
                uuid_cmd = (struct uuid_command *)lcp;
                break;
            }
            lcp = (struct load_command *)((uintptr_t)lcp + lcp->cmdsize);
        }
    }
    
    if (printUUIDs) {

        if (uuid_cmd) {
            uuid_string_t uuid_string;

            uuid_unparse(*(uuid_t *)uuid_cmd->uuid, uuid_string);
            printf("%s\t%s\t%s\t0x%llx\t0x%llx\t%s\n", idBuffer, versionBuffer, uuid_string, kextLoadAddress, kextExecutableSize, pathBuffer);
        } else {
            printf("%s\t%s\t\t\t\t%s\n", idBuffer, versionBuffer, pathBuffer);
        }
    } else {
        printf("%s\t%s\t%s\n", idBuffer, versionBuffer, pathBuffer);
    }

    if (beVerbose) {
        printf("\t-> load address:   0x%0.8llx, "
               "size              = 0x%0.8llx,\n"
               "\t-> source address: 0x%0.8llx, "
               "kmod_info address = 0x%0.8llx\n",
               kextLoadAddress, kextExecutableSize, kextSourceAddress, kextKmodInfoAddress);
    }

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
      "usage: %1$s [-arch archname] [-u] [-v] [--] kernelcache [bundle-id ...]\n"
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
    fprintf(stderr, "-%s (-%c):\n"
        "        print kext load addresses and UUIDs\n",
            kOptNameUUID, kOptUUID);
    fprintf(stderr, "-%s (-%c):\n"
        "        emit additional information about kext load addresses and sizes\n",
            kOptNameVerbose, kOptVerbose);
    fprintf(stderr, "\n");
   
    fprintf(stderr, "-%s (-%c): print this message and exit\n",
        kOptNameHelp, kOptHelp);

    return;
}
