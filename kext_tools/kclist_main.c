/*
 *  kclist_main.c
 *  kext_tools
 *
 *  Created by Nik Gervae on 2010 10 04.
 *  Copyright 2010-2016 Apple, Inc. All rights reserved.
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <System/libkern/OSKextLibPrivate.h>

#if __has_include(<prelink.h>)
/* take prelink.h from host side tools SDK */
#include <prelink.h>
#else
#include <System/libkern/prelink.h>
#endif
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

static const char * getSegmentCommandName(uint32_t theSegCommand);

static void createKCMap(const uint8_t *kernelcacheImage, Boolean verbose, struct kcmap **map);
static void printKernelCacheLayoutMap(KclistArgs *toolArgs, const char *prelinkTextBytes, uint64_t prelinkTextSourceAddress,
                                      uint64_t prelinkTextSourceSize,struct kcmap *kcmap, CFPropertyListRef kcInfoPlist,
                                      const UInt8 *kcImagePtr, CFIndex kcImageSize);
static void printJSON(KclistArgs *toolArgs, const char *prelinkTextBytes, uint64_t prelinkTextSourceAddress,
                      uint64_t prelinkTextSourceSize, struct kcmap *kcmap, CFPropertyListRef kcInfoPlist,
                      const UInt8 *kcImagePtr, CFIndex kcImageSize);
static void printJSONSegments(KclistArgs *toolArgs, void *mhpv, bool mh_is_64);
static off_t getKCFileOffset(struct kcmap *kcmap, uint8_t *kextStart, uint64_t va_ofst, uint64_t fileoff);

static void printMachOHeader(const UInt8 *imagePtr, CFIndex imageSize);

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
    const NXArchInfo *   nextArchInfo       = NULL;
    CFMutableArrayRef    archInfoArray      = NULL;
    Boolean              userProvidedArch   = false;

    uint64_t             kextDataGap = 0;
    uint64_t             xnuTextStart = 0;

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

    // If arch passed in we will use that else we will print out info for all
    // archs (which is typically thinned to one anyway)
    if (toolArgs.archInfo) {
        nextArchInfo = toolArgs.archInfo;
        userProvidedArch = true;
    }
    else {
        fat_arch = getFirstFatArch(fat_header);
        if (fat_arch) {
            nextArchInfo = NXGetArchInfoFromCpuType(fat_arch->cputype,
                                                    fat_arch->cpusubtype);
        }
    }
    
    while (true) {
        // may not be any arch info, so pass through at least once
        SAFE_RELEASE_NULL(prelinkInfoPlist);
        SAFE_RELEASE_NULL(kernelcacheImage);
        SAFE_RELEASE_NULL(rawKernelcache);
        
        rawKernelcache = readMachOSliceForArch(toolArgs.kernelcachePath,
                                               nextArchInfo,
                                               /* checkArch */ FALSE);
        if (!rawKernelcache) {
            OSKextLog(/* kext */ NULL,
                      kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                      "Can't read arch %s from %s.",
                      nextArchInfo ? nextArchInfo->name : "NONE",
                      toolArgs.kernelcachePath);
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
        
        if (toolArgs.printMachO)
            printMachOHeader(kernelcacheStart, CFDataGetLength(kernelcacheImage));

        if (ISMACHO64(MAGIC32(kernelcacheStart))) {
            prelinkInfoSect = (void *)
            macho_get_section_by_name_64((struct mach_header_64 *)kernelcacheStart,
                                         kPrelinkInfoSegment,
                                         kPrelinkInfoSection);
            prelinkTextSect = (void *)
            macho_get_section_by_name_64((struct mach_header_64 *)kernelcacheStart,
                                         kPrelinkTextSegment,
                                         kPrelinkTextSection);
        } else {
            prelinkInfoSect = (void *)
            macho_get_section_by_name((struct mach_header *)kernelcacheStart,
                                      kPrelinkInfoSegment,
                                      kPrelinkInfoSection);
            prelinkTextSect = (void *)
            macho_get_section_by_name((struct mach_header *)kernelcacheStart,
                                      kPrelinkTextSegment,
                                      kPrelinkTextSection);
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
        
        prelinkInfoPlist = (CFPropertyListRef)
        IOCFUnserialize(prelinkInfoBytes,
                        kCFAllocatorDefault, /* options */ 0,
                        /* errorString */ NULL);
        if (!prelinkInfoPlist) {
            OSKextLog(/* kext */ NULL,
                      kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                      "Can't unserialize prelink info.");
            goto finish;
        }

        struct kcmap *kcmap = NULL;
        if (toolArgs.extractedKextPath != NULL)
            createKCMap(kernelcacheStart, toolArgs.verbose, &kcmap);

        if (toolArgs.printMap) {
            printKernelCacheLayoutMap(&toolArgs, prelinkTextBytes,
                                      prelinkTextSourceAddress,
                                      prelinkTextSourceSize, kcmap,
                                      prelinkInfoPlist,
                                      kernelcacheStart,
                                      CFDataGetLength(kernelcacheImage));
        } else if (toolArgs.printJSON) {
            printJSON(&toolArgs, prelinkTextBytes,
                      prelinkTextSourceAddress,
                      prelinkTextSourceSize, kcmap,
                      prelinkInfoPlist,
                      kernelcacheStart,
                      CFDataGetLength(kernelcacheImage));
        } else {
            listPrelinkedKexts(&toolArgs, prelinkInfoPlist, prelinkTextBytes,
                           prelinkTextSourceAddress, prelinkTextSourceSize,
                           kcmap, nextArchInfo);
        }

        if (kcmap)
            free(kcmap);

        // process next arch or done if user specified an architecture,
        // fat_arch will be NULL if user passed in an arch via "-arch XXX"
        nextArchInfo = NULL;
        if (fat_arch) {
            fat_arch = getNextFatArch(fat_header, fat_arch);
            if (fat_arch) {
                nextArchInfo = NXGetArchInfoFromCpuType(fat_arch->cputype,
                                                        fat_arch->cpusubtype);
            }
        }
        if (userProvidedArch || nextArchInfo == NULL) {
            break;
        }
    } // while true
    
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

            case kOptMachO:
                toolArgs->printMachO = true;
                break;

            case kOptUUID:
                toolArgs->printUUIDs = true;
                break;

            case kOptSaveKext:
                toolArgs->extractedKextPath = optarg;
                break;

            case kOptVerbose:
                toolArgs->verbose = true;
                break;

            case kOptLayoutMap:
                toolArgs->printMap = true;
                break;

            case kOptJSON:
                toolArgs->printJSON = true;
                break;

            default:
                OSKextLog(/* kext */ NULL,
                    kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                    "unrecognized option %s", (*argv)[optind-1]);
                goto finish;
                break;

        }

        if (toolArgs->printJSON && toolArgs->printMap) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                "cannot print map as human-readable and JSON simultaneously "
                "(-%c and -%c are mutually exclusive)", kOptJSON,
                kOptLayoutMap);
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

    if (toolArgs->extractedKextPath != NULL && CFSetGetCount(toolArgs->kextIDs) <= 0) {
        OSKextLog(/* kext */ NULL,
                  kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                  "when extracting kexts (-%c), specific bundle IDs must be specified", kOptSaveKext);
        result = EX_USAGE;
        goto finish;
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
void listPrelinkedKexts(KclistArgs * toolArgs,
                        CFPropertyListRef kcInfoPlist,
                        const char *prelinkTextBytes,
                        uint64_t prelinkTextSourceAddress,
                        uint64_t prelinkTextSourceSize,
                        struct kcmap *kcmap,
                        const NXArchInfo * archInfo)
{
    CFIndex i, count, printed = 0;
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

    if (archInfo) {
        printf("Listing info for architecture %s\n", archInfo->name);
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
        printKextInfo(toolArgs, kextPlist, kextTextBytes, kcmap);
        printed++;
    }

    if (haveIDs && printed != CFSetGetCount(toolArgs->kextIDs)) {
        OSKextLog(NULL,
                  kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                  "[WARNING] you specified %ld kexts, but I only found %ld\n",
                  CFSetGetCount(toolArgs->kextIDs), printed);
    }

finish:
    return;
}


/**
 *
 * Extract a 64-bit (possibly split) kext from the kernel cache
 *
 */
static void extractKext(struct kcmap *kcmap,
                        const char *kextName, const char *kextTextBytes,
                        void *mh, Boolean is64Bit,
                        KclistArgs *toolArgs, Boolean isSplitKext)
{
    int         fd = -1;
    mode_t      mode = 0644;
    char        tmpPath[PATH_MAX];
    uint64_t    kextMHSize = 0;
    struct mach_header_64 *kextMH64 = NULL;
    struct mach_header_64 *mhp64 = (struct mach_header_64 *)mh;
    struct mach_header    *kextMH32 = NULL;
    struct mach_header    *mhp32 = (struct mach_header *)mh;
    struct load_command *lcp;
    uint32_t ncmds = 0;
    Boolean beVerbose = toolArgs->verbose;

    if (is64Bit) {
        ncmds = mhp64->ncmds;
        kextMHSize = sizeof(*kextMH64) + mhp64->sizeofcmds;
        kextMH64 = (struct mach_header_64 *)calloc(1, (size_t)kextMHSize);
        memcpy(kextMH64, mhp64, (size_t)kextMHSize);
        lcp = (struct load_command *)(void *)(kextMH64 + 1);
    } else {
        ncmds = mhp32->ncmds;
        kextMHSize = sizeof(*kextMH32) + mhp32->sizeofcmds;
        kextMH32 = (struct mach_header *)calloc(1, (size_t)kextMHSize);
        memcpy(kextMH32, mhp32, (size_t)kextMHSize);
        lcp = (struct load_command *)(void *)(kextMH32 + 1);
    }

    size_t copied = strlcpy(tmpPath, toolArgs->extractedKextPath, PATH_MAX);
    strlcat(tmpPath, kextName, PATH_MAX - copied);

    printf("extracting%s%skext to: '%s'\n", is64Bit ? " 64bit" : " 32bit", isSplitKext ? " split " : " ", tmpPath);

    fd = open(tmpPath, O_WRONLY|O_CREAT|O_TRUNC, mode);
    if (fd == -1) {
        printf("[ERROR] could not open file (errno:%d)\n", errno);
        goto finish;
    }

    ExitStatus my_err = EX_OK;
    Boolean has_dysymtab = false, has_symtab = false;
    off_t fileoff = 0;
    off_t linkedit_ofst_new = 0;
    off_t linkedit_ofst_kc  = 0;
    struct load_command *dysymtab_lc = NULL, *symtab_lc = NULL;

    if (beVerbose)
        printf("\tPass 1/2 (%d load commands)\n", ncmds);
    for (uint32_t cmd_i = 0; cmd_i < ncmds; cmd_i++) {
        struct segment_command_64 *seg_cmd64 = (struct segment_command_64 *)lcp;
        struct segment_command    *seg_cmd32 = (struct segment_command *)lcp;

        /* 64-bit segments */
        if (lcp->cmd == LC_SEGMENT_64 && seg_cmd64->filesize > 0) {
            ssize_t wsize;
            off_t kcfileoff = (off_t)(seg_cmd64->fileoff);

            /* calculate the KC file offset */
            kcfileoff = getKCFileOffset(kcmap, (uint8_t *)kextTextBytes,
                                        (uint64_t)seg_cmd64->vmaddr, (uint64_t)seg_cmd64->fileoff);
            if (beVerbose) {
                printf("\tcmd[%d]:%16s @%p + %lld (%lld) [%lld bytes] -> %lld\n",
                       cmd_i, seg_cmd64->segname[0] ? seg_cmd64->segname : "none",
                       kextTextBytes, (uint64_t)kcfileoff, (uint64_t)seg_cmd64->fileoff,
                       (uint64_t)seg_cmd64->filesize, (uint64_t)fileoff);
            }

            /* write out the segment to disk */
            wsize = pwrite(fd, kextTextBytes + kcfileoff, (size_t)(seg_cmd64->filesize), fileoff);
            if (wsize < (ssize_t)(seg_cmd64->filesize)) {
                OSKextLog(/* kext */ NULL,
                          kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                          "[ERROR] unable to write cmd[%d]:%16s (fileoff:%lld = %p) to ofst:%lld in %s: %d (errno=%d)",
                          cmd_i, seg_cmd64->segname[0] ? seg_cmd64->segname : "none",
                          (uint64_t)kcfileoff, kextTextBytes + kcfileoff, fileoff, tmpPath, (int)wsize, errno);
                my_err = EX_OSERR;
                break;
            }

            /* reset the segment+section file offsets to the new output file */
            struct section_64 *sect = (struct section_64 *)(&seg_cmd64[1]);
            for (u_int j = 0; j < seg_cmd64->nsects; ++j, ++sect) {
                if (sect->offset > 0)
                    sect->offset = (uint32_t)(fileoff + (sect->offset - seg_cmd64->fileoff));
            }
            if (strcmp(seg_cmd64->segname, "__LINKEDIT") == 0) {
                linkedit_ofst_kc  = seg_cmd64->fileoff;
                linkedit_ofst_new = fileoff;
            }
            seg_cmd64->fileoff = fileoff;
            fileoff += seg_cmd64->filesize;

        /* 32-bit segments */
        } else if (lcp->cmd == LC_SEGMENT && seg_cmd32->filesize > 0) {
            ssize_t wsize;
            off_t kcfileoff = (off_t)(seg_cmd32->fileoff);

            /* calculate the KC file offset */
            kcfileoff = getKCFileOffset(kcmap, (uint8_t *)kextTextBytes,
                                        (uint64_t)seg_cmd32->vmaddr, (uint64_t)seg_cmd32->fileoff);
            if (beVerbose) {
                printf("\tcmd[%d]:%16s @%p + %lld (%lld) [%lld bytes] -> %lld\n",
                       cmd_i, seg_cmd32->segname[0] ? seg_cmd32->segname : "none",
                       kextTextBytes, (uint64_t)kcfileoff, (uint64_t)seg_cmd32->fileoff,
                       (uint64_t)seg_cmd32->filesize, (uint64_t)fileoff);
            }

            /* write out the segment to disk */
            wsize = pwrite(fd, kextTextBytes + kcfileoff, (size_t)(seg_cmd32->filesize), fileoff);
            if (wsize < (ssize_t)(seg_cmd32->filesize)) {
                OSKextLog(/* kext */ NULL,
                          kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                          "[ERROR] unable to write cmd[%d]:%16s (fileoff:%lld = %p) to ofst:%lld in %s: %d (errno=%d)",
                          cmd_i, seg_cmd32->segname[0] ? seg_cmd32->segname : "none",
                          (uint64_t)kcfileoff, kextTextBytes + kcfileoff, fileoff, tmpPath, (int)wsize, errno);
                my_err = EX_OSERR;
                break;
            }

            /* reset the segment+section file offsets to the new output file */
            struct section_64 *sect = (struct section_64 *)(&seg_cmd32[1]);
            for (u_int j = 0; j < seg_cmd32->nsects; ++j, ++sect) {
                if (sect->offset > 0)
                    sect->offset = (uint32_t)(fileoff + (sect->offset - seg_cmd32->fileoff));
            }
            if (strcmp(seg_cmd32->segname, "__LINKEDIT") == 0) {
                linkedit_ofst_kc  = seg_cmd32->fileoff;
                linkedit_ofst_new = fileoff;
            }
            seg_cmd32->fileoff = (uint32_t)fileoff;
            fileoff += seg_cmd32->filesize;
        }
        lcp = (struct load_command *)((uintptr_t)lcp + lcp->cmdsize);
    } /* for each load command */


    /* helper macro to adjust LINKEDIT commands */
    #define __adj_linkedit(__cmd,__nm) \
            if (__cmd->__nm > 0) { \
                uint32_t __tmp = (uint32_t)(linkedit_ofst_new + (__cmd->__nm - linkedit_ofst_kc)); \
                if (beVerbose) \
                    printf("\t  `-> moving %s {%lld -> %lld}\n", \
                           #__nm, (long long)(__cmd->__nm), (long long)(__tmp)); \
                __cmd->__nm = __tmp; \
            }

    if (is64Bit) {
        lcp = (struct load_command *)(void *)(kextMH64 + 1);
    } else {
        lcp = (struct load_command *)(void *)(kextMH32 + 1);
    }

    if (beVerbose)
        printf("\tPass 2/2 (%d load commands)\n", ncmds);

    /* adjust load commands that point into segments */
    for (uint32_t cmd_i = 0; cmd_i < ncmds; cmd_i++) {
        struct segment_command_64 *seg_cmd64;
        struct section_64         *sect64;
        struct segment_command    *seg_cmd32;
        struct section            *sect32;
        switch (lcp->cmd) {
        case LC_SEGMENT_64:
            seg_cmd64 = (struct segment_command_64 *)lcp;
            sect64 = (struct section_64 *)(&seg_cmd64[1]);
            if (beVerbose)
                printf("\t-> adjusting cmd[%d]:%24s LINKEDIT data: {%lld,%lld}\n",
                       cmd_i, seg_cmd64->segname, (uint64_t)linkedit_ofst_kc, (uint64_t)linkedit_ofst_new);
            for (uint32_t j = 0; j < seg_cmd64->nsects; ++j, ++sect64) {
                __adj_linkedit(sect64, reloff);
            }
            break;
        case LC_SEGMENT:
            seg_cmd32 = (struct segment_command *)lcp;
            sect32 = (struct section *)(&seg_cmd32[1]);
            if (beVerbose)
                printf("\t-> adjusting cmd[%d]:%24s LINKEDIT data: {%lld,%lld}\n",
                       cmd_i, seg_cmd32->segname, (uint64_t)linkedit_ofst_kc, (uint64_t)linkedit_ofst_new);
            for (uint32_t j = 0; j < seg_cmd32->nsects; ++j, ++sect32) {
                __adj_linkedit(sect32, reloff);
            }
            break;
        case LC_DYSYMTAB:
            dysymtab_lc = lcp;
            if (linkedit_ofst_new) {
                if (beVerbose)
                    printf("\t-> adjusting cmd[%d]:%24s LINKEDIT data: {%lld,%lld}\n",
                           cmd_i, getSegmentCommandName(lcp->cmd), (uint64_t)linkedit_ofst_kc, (uint64_t)linkedit_ofst_new);
                struct dysymtab_command * dsymtab_cmd = (struct dysymtab_command *)lcp;
                __adj_linkedit(dsymtab_cmd, tocoff);
                __adj_linkedit(dsymtab_cmd, modtaboff);
                __adj_linkedit(dsymtab_cmd, extrefsymoff);
                __adj_linkedit(dsymtab_cmd, indirectsymoff);
                __adj_linkedit(dsymtab_cmd, extreloff);
                __adj_linkedit(dsymtab_cmd, locreloff);
            } else {
                if (beVerbose)
                    printf("\t->   forcing cmd[%d]:%s to type 0\n",
                           cmd_i, getSegmentCommandName(lcp->cmd));
                lcp->cmd = 0;
            }
            break;
        case LC_SYMTAB:
            symtab_lc = lcp;
            if (linkedit_ofst_new) {
                if (beVerbose)
                    printf("\t-> adjusting cmd[%d]:%24s LINKEDIT data: {%lld,%lld}\n",
                           cmd_i, getSegmentCommandName(lcp->cmd), (uint64_t)linkedit_ofst_kc, (uint64_t)linkedit_ofst_new);
                struct symtab_command * symtab_cmd = (struct symtab_command *)lcp;
                __adj_linkedit(symtab_cmd, symoff);
                __adj_linkedit(symtab_cmd, stroff);
            } else {
                if (beVerbose)
                    printf("\t->   forcing cmd[%d]:%s to type 0\n",
                           cmd_i, getSegmentCommandName(lcp->cmd));
                lcp->cmd = 0;
            }
            break;
        case LC_SEGMENT_SPLIT_INFO:
            if (linkedit_ofst_new) {
                if (beVerbose)
                    printf("\t-> adjusting cmd[%d]:%24s LINKEDIT data: {%lld,%lld}\n",
                           cmd_i, getSegmentCommandName(lcp->cmd), (uint64_t)linkedit_ofst_kc, (uint64_t)linkedit_ofst_new);
                struct linkedit_data_command * lc = (struct linkedit_data_command *)lcp;
                __adj_linkedit(lc, dataoff);
            } else {
                if (beVerbose)
                    printf("\t-> forcing cmd[%d]:%s to type 0\n",
                           cmd_i, getSegmentCommandName(lcp->cmd));
                lcp->cmd = 0;
            }
            break;
        default:
            if (beVerbose)
                printf("\t-> skipping cmd[%d]:%d:%22s\n", cmd_i, lcp->cmd, getSegmentCommandName(lcp->cmd));
            break;
        }
        lcp = (struct load_command *)((uintptr_t)lcp + lcp->cmdsize);
    } /* for each load command */

    /*
     * The LC_SYMTAB command is considered a prerequisite of the LC_DYSYMTAB command
     * (even though current kext processing comits the former and not the latter).
     * If we found the LC_DYSYMTAB command, but not LC_SYMTAB; force the LC_DYSYMTAB
     * to an invalid command type to avoid tool failures while parsing the extracted kext.
     */
    if (dysymtab_lc) {
        if (!symtab_lc) {
            if (beVerbose)
                printf("\t-> forcing %s to type 0 (missing LC_SYMTAB)\n", getSegmentCommandName(dysymtab_lc->cmd));
            dysymtab_lc->cmd = 0;
        }
    }

    /* re-write the header with updated file offsets */
    if (my_err == EX_OK) {
        ssize_t wsize;
        void *kextMH = is64Bit ? (void *)kextMH64 : (void *)kextMH32;
        wsize = pwrite(fd, kextMH, (size_t)kextMHSize, 0);
        if (wsize < (ssize_t)kextMHSize) {
            OSKextLog(NULL,
                      kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                      "[ERROR] unable to re-write %lld bytes of macho header@%p to ofst:0 in %s: wsize=%lld (errno=%d)",
                      kextMHSize, kextMH, tmpPath, (uint64_t)wsize, errno);
            my_err = EX_OSERR;
        }
    }

    if (my_err != EX_OK) {
        OSKextLog(NULL, kOSKextLogErrorLevel,
                  "[ERROR] write failed for '%s' (errno:%d)", tmpPath, errno);
    } else if (beVerbose) {
        printf("\t-> created file '%s'\n", tmpPath);
    }
    close(fd);

finish:
    if (kextMH64)
        free(kextMH64);
    if (kextMH32)
        free(kextMH32);
}

#define KCLAYOUTFORMATSTR   "%-12s 0x%-16x 0x%-16x %-38s %s\n"
#define KCLAYOUTFORMATSTR64 "%-12s 0x%-16llx 0x%-16llx %-38s %s\n"

static void printKernelCacheLayoutMap(KclistArgs *toolArgs,
                                      const char *prelinkTextBytes,
                                      uint64_t prelinkTextSourceAddress,
                                      uint64_t prelinkTextSourceSize,
                                      struct kcmap *kcmap,
                                      CFPropertyListRef kcInfoPlist,
                                      const UInt8 *kcImagePtr,
                                      CFIndex kcImageSize)
{
    CFIndex i, count = 0;
    CFArrayRef kextPlistArray = NULL;
    CFDataRef  kcID = NULL;
    uint8_t    *kcID_val = NULL;
    struct mach_header_64 *mhp64 = NULL;
    struct mach_header *mhp = NULL;
    struct load_command *lcp;
    uint32_t ncmds, cmd_i;
    uuid_string_t uuid_string = {'\0'};

    if (CFDictionaryGetTypeID() == CFGetTypeID(kcInfoPlist)){
        kextPlistArray = (CFArrayRef)CFDictionaryGetValue(kcInfoPlist,
                                                          CFSTR(kPrelinkInfoDictionaryKey));
    } else {
        OSKextLog(/* kext */ NULL,
                  kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                  "Unrecognized kernelcache plist data -- type is %lu.",
                  CFGetTypeID(kcInfoPlist));
        return;
    }

    kcID = (CFDataRef)CFDictionaryGetValue(kcInfoPlist, CFSTR(kPrelinkInfoKCIDKey));
    if (kcID) {
        kcID_val = (uint8_t*)CFDataGetBytePtr(kcID);
        printf("KernelCache ID: ");
        for (i = 0; i < CFDataGetLength(kcID); i++) {
            printf("%02X", kcID_val[i]);
        }
        printf("\n");
    } else {
        printf("No KernelCache ID found\n");
    }

    /* Print out the KEXT map */
    count = CFArrayGetCount(kextPlistArray);
    for (i = 0; i < count; i++) {
        CFDictionaryRef kextPlist = (CFDictionaryRef)CFArrayGetValueAtIndex(kextPlistArray, i);
        CFStringRef kextIdentifier = (CFStringRef)CFDictionaryGetValue(kextPlist, kCFBundleIdentifierKey);
        CFNumberRef kextSourceAddress = (CFNumberRef)CFDictionaryGetValue(kextPlist, CFSTR(kPrelinkExecutableSourceKey));
        CFNumberRef kextSourceSize = (CFNumberRef)CFDictionaryGetValue(kextPlist, CFSTR(kPrelinkExecutableSizeKey));
        const char *kextTextBytes = NULL;

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

        printKextInfo(toolArgs, kextPlist, kextTextBytes, kcmap);
    }

    /* Print out the map of kernel segments */

    /* First look for the kernel UUID */
    if (ISMACHO64(MAGIC32(kcImagePtr))) {
        mhp64 = (struct mach_header_64 *)kcImagePtr;
        ncmds = mhp64->ncmds;
        lcp = (struct load_command *)(void *)(mhp64 + 1);
    } else {
        mhp = (struct mach_header *)kcImagePtr;
        ncmds = mhp->ncmds;
        lcp = (struct load_command *)(void *)(mhp + 1);
    }

    for (cmd_i = 0; cmd_i < ncmds; cmd_i++) {
        if (lcp->cmd == LC_UUID) {
            uuid_unparse(((struct uuid_command *)lcp)->uuid, uuid_string);
            break;
        }

        lcp = (struct load_command *)((uintptr_t)lcp + lcp->cmdsize);
    }

    /*
     * Next walk the load commands and print out all except __PLK* and __PRELINK* sections
     * since these are covered by the KEXT map above
     */
    if (ISMACHO64(MAGIC32(kcImagePtr))) {
        struct mach_header_64 *     kext_header;
        struct segment_command_64 * seg_cmd;
        uintptr_t last_cmd;

        kext_header = (struct mach_header_64 *)kcImagePtr;
        seg_cmd = (struct segment_command_64 *) ((uintptr_t)kext_header + sizeof(*kext_header));

        last_cmd = (uintptr_t)seg_cmd + (kext_header->ncmds * sizeof(*seg_cmd));
        if (last_cmd > (uintptr_t)kcImagePtr + kcImageSize) {
            fprintf(stderr, "[macho] ERROR: header points off the end of the image!\n");
            fprintf(stderr, "[macho] header@%p, sz:%d, ncmds:%d, last_cmd@%p\n",
                    kext_header, (int)kcImageSize, kext_header->ncmds, (void *)last_cmd);
            return;
        }

        for (cmd_i = 0; cmd_i < kext_header->ncmds; cmd_i++) {
            if (seg_cmd->cmd == LC_SEGMENT_64 && (!seg_cmd->segname[0] || (!strstr(seg_cmd->segname, "__PLK")
                                                                           && !strstr(seg_cmd->segname, "__PRELINK")))) {
                printf(KCLAYOUTFORMATSTR64, seg_cmd->segname,
                       seg_cmd->vmaddr, seg_cmd->vmsize,
                       uuid_string[0] ? uuid_string : "",
                       "xnu");
            }

            seg_cmd = (struct segment_command_64 *) ((uintptr_t)seg_cmd + seg_cmd->cmdsize);
        }
    } else {
        struct mach_header *kext_header;
        struct segment_command *seg_cmd;
        uintptr_t last_cmd;

        kext_header = (struct mach_header *)kcImagePtr;
        seg_cmd = (struct segment_command *)((uintptr_t)kext_header + sizeof(*kext_header));

        last_cmd = (uintptr_t)seg_cmd + (kext_header->ncmds * sizeof(*seg_cmd));
        if (last_cmd > (uintptr_t)kcImagePtr + kcImageSize) {
            fprintf(stderr, "[macho] ERROR: header points off the end of the image!\n");
            fprintf(stderr, "[macho] header@%p, sz:%d, ncmds:%d, last_cmd@%p\n",
                    kext_header, (int)kcImageSize, kext_header->ncmds, (void *)last_cmd);
            return;
        }

        for (cmd_i = 0; cmd_i < kext_header->ncmds; cmd_i++){
            if (seg_cmd->cmd == LC_SEGMENT && (!seg_cmd->segname[0] || (!strstr(seg_cmd->segname, "__PLK")
                                                                        && !strstr(seg_cmd->segname, "__PRELINK")))) {
                printf(KCLAYOUTFORMATSTR, seg_cmd->segname,
                       seg_cmd->vmaddr, seg_cmd->vmsize,
                       uuid_string[0] ? uuid_string : "",
                       "xnu");
            }

            seg_cmd = (struct segment_command *)((uintptr_t)seg_cmd + seg_cmd->cmdsize);
        }
    }

    return;
}

static void printJSONSegments(KclistArgs *toolArgs, void *mhpv, bool mh_is_64)
{
    struct mach_header_64 *mhp64 = NULL;
    struct mach_header *mhp = NULL;
    struct load_command *lcp;
    uint32_t ncmds, cmd_i;
    uuid_string_t uuid_string = {};

    if (mh_is_64) {
        struct segment_command_64 * seg_cmd;
        bool printed_seg = false;

        mhp64 = (struct mach_header_64 *)mhpv;
        ncmds = mhp64->ncmds;
        lcp = (struct load_command *)(void *)(mhp64 + 1);

        seg_cmd = (struct segment_command_64 *) ((uintptr_t)mhp64 + sizeof(*mhp64));

        for (cmd_i = 0; cmd_i < mhp64->ncmds; cmd_i++) {
            if (seg_cmd->cmd == LC_SEGMENT_64) {
                if (printed_seg) {
                    printf(", ");
                } else {
                    printed_seg = true;
                }
                printf("{\"name\": \"%s\", ", seg_cmd->segname);
                if (toolArgs->verbose) {
                    printf("\"commands_size\": %d, ", seg_cmd->cmdsize);
                }
                printf("\"size\": %llu, ", seg_cmd->vmsize);

                struct section_64 *sect = NULL;
                u_int j = 0;
                sect = (struct section_64 *) (&seg_cmd[1]);
                printf("\"sections\": [");
                for (j = 0; j < seg_cmd->nsects; ++j, ++sect) {
                    if (j != 0) {
                        printf(", ");
                    }
                    printf("{\"name\": \"%s\", ", sect->sectname);
                    printf("\"size\": %llu, ", sect->size);
                    printf("\"offset\": %u}", sect->offset);
                }
                printf("]}");
            }
            seg_cmd = (struct segment_command_64 *) ((uintptr_t)seg_cmd + seg_cmd->cmdsize);
        }
    } else {
        struct segment_command *    seg_cmd;
        bool printed_seg = false;

        mhp = (struct mach_header *)mhpv;
        ncmds = mhp->ncmds;
        lcp = (struct load_command *)(void *)(mhp + 1);

        seg_cmd = (struct segment_command *) ((uintptr_t)mhp + sizeof(*mhp));

        for (cmd_i = 0; cmd_i < mhp->ncmds; cmd_i++) {
            if (seg_cmd->cmd == LC_SEGMENT) {
                if (printed_seg) {
                    printf(", ");
                } else {
                    printed_seg = true;
                }
                printf("{\"name\": \"%s\", ", seg_cmd->segname);
                printf("\"commands_size\": %d, ", seg_cmd->cmdsize);
                printf("\"size\": %u, ", seg_cmd->vmsize);

                struct section *sect = NULL;
                u_int j = 0;
                sect = (struct section *) (&seg_cmd[1]);
                printf("\"sections\": [");
                for (j = 0; j < seg_cmd->nsects; ++j, ++sect) {
                    if (j != 0) {
                        printf(", ");
                    }
                    printf("{\"name\": \"%s\", ", sect->sectname);
                    printf("\"size\": %u, ", sect->size);
                    printf("\"offset\": %u}", sect->offset);
                }
                printf("]}");
            }
            seg_cmd = (struct segment_command *) ((uintptr_t)seg_cmd + seg_cmd->cmdsize);
        }
    }
}

static void printJSON(KclistArgs *toolArgs,
                      const char *prelinkTextBytes,
                      uint64_t prelinkTextSourceAddress,
                      uint64_t prelinkTextSourceSize,
                      struct kcmap *kcmap,
                      CFPropertyListRef kcInfoPlist,
                      const UInt8 *kcImagePtr,
                      CFIndex kcImageSize)
{
    CFIndex i, count = 0;
    CFArrayRef kextPlistArray = NULL;
    struct mach_header_64 *mhp64 = NULL;
    struct mach_header *mhp = NULL;
    struct load_command *lcp;
    uint32_t ncmds, cmd_i;
    uuid_string_t uuid_string = {};
    (void)kcImageSize;

    if (CFDictionaryGetTypeID() == CFGetTypeID(kcInfoPlist)){
        kextPlistArray = (CFArrayRef)CFDictionaryGetValue(kcInfoPlist,
                                                          CFSTR(kPrelinkInfoDictionaryKey));
    } else {
        OSKextLog(/* kext */ NULL,
                  kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                  "Unrecognized kernelcache plist data -- type is %lu.",
                  CFGetTypeID(kcInfoPlist));
        return;
    }
    printf("[");

    /* Print out the KEXT map */
    count = CFArrayGetCount(kextPlistArray);
    for (i = 0; i < count; i++) {
        CFDictionaryRef kextPlist = (CFDictionaryRef)CFArrayGetValueAtIndex(kextPlistArray, i);
        CFStringRef kextIdentifier = (CFStringRef)CFDictionaryGetValue(kextPlist, kCFBundleIdentifierKey);
        CFNumberRef kextSourceAddress = (CFNumberRef)CFDictionaryGetValue(kextPlist, CFSTR(kPrelinkExecutableSourceKey));
        CFNumberRef kextSourceSize = (CFNumberRef)CFDictionaryGetValue(kextPlist, CFSTR(kPrelinkExecutableSizeKey));
        const char *kextTextBytes = NULL;

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

        if (i != 0) {
            printf(", ");
        }
        printKextInfo(toolArgs, kextPlist, kextTextBytes, kcmap);
    }

    /* First look for the kernel UUID */
    if (ISMACHO64(MAGIC32(kcImagePtr))) {
        mhp64 = (struct mach_header_64 *)kcImagePtr;
        ncmds = mhp64->ncmds;
        lcp = (struct load_command *)(void *)(mhp64 + 1);
    } else {
        mhp = (struct mach_header *)kcImagePtr;
        ncmds = mhp->ncmds;
        lcp = (struct load_command *)(void *)(mhp + 1);
    }

    printf(", {\"name\": \"xnu\", ");

    for (cmd_i = 0; cmd_i < ncmds; cmd_i++) {
        if (lcp->cmd == LC_UUID) {
            uuid_unparse(((struct uuid_command *)lcp)->uuid, uuid_string);
            printf("\"uuid\": \"%s\", ", uuid_string);
            break;
        }

        lcp = (struct load_command *)((uintptr_t)lcp + lcp->cmdsize);
    }

    printf("\"segments\": [");
    printJSONSegments(toolArgs, (void *)kcImagePtr, ISMACHO64(MAGIC32(kcImagePtr)));
    printf("]}");
    printf("]");
}

/*******************************************************************************
*******************************************************************************/
void printKextInfo(KclistArgs *toolArgs,
                   CFDictionaryRef kextPlist,
                   const char *kextTextBytes,
                   struct kcmap *kcmap)
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

    struct mach_header_64 *mhp64 = NULL;
    struct mach_header *mhp = NULL;
    struct load_command *lcp;
    struct uuid_command *uuid_cmd = NULL;
    uint32_t ncmds, cmd_i;
    Boolean isSplitKext = false;

    Boolean beVerbose = toolArgs->verbose;
    Boolean printUUIDs = toolArgs->printUUIDs;
    Boolean layoutMap  = toolArgs->printMap;
    Boolean json = toolArgs->printJSON;
    Boolean shouldExtractKext = toolArgs->extractedKextPath != NULL;

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
            mhp64 = (struct mach_header_64 *)kextTextBytes;
            ncmds = mhp64->ncmds;
            lcp = (struct load_command *)(void *)(mhp64 + 1);
        } else {
            mhp = (struct mach_header *)kextTextBytes;
            ncmds = mhp->ncmds;
            lcp = (struct load_command *)(void *)(mhp + 1);
        }

        for (cmd_i = 0; cmd_i < ncmds; cmd_i++) {
            if (lcp->cmd == LC_UUID) {
                uuid_cmd = (struct uuid_command *)lcp;
            }
            if (lcp->cmd == LC_SEGMENT_SPLIT_INFO) {
                isSplitKext = true;
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
    } else if (json) {
        printf("{\"name\": \"%s\", ", idBuffer);
        printf("\"version\": \"%s\", ", versionBuffer);
        if (uuid_cmd) {
            uuid_string_t uuidBuffer;
            uuid_unparse(*(uuid_t *)uuid_cmd->uuid, uuidBuffer);
            printf("\"uuid\": \"%s\", ", uuidBuffer);
        }
        printf("\"path\": \"%s\", ", pathBuffer);
        printf("\"load_address\": \"%#llx\"", kextLoadAddress);
    } else if (!layoutMap) {
        printf("%s\t%s\t%s\n", idBuffer, versionBuffer, pathBuffer);
    }

    if (layoutMap) {
        uuid_string_t uuid_string;

        if (uuid_cmd) {
            uuid_unparse(*(uuid_t *)uuid_cmd->uuid, uuid_string);
        }

        if (mhp64) {
            struct segment_command_64 * seg_cmd;
            seg_cmd = (struct segment_command_64 *) ((uintptr_t)mhp64 + sizeof(*mhp64));

            for (cmd_i = 0; cmd_i < mhp64->ncmds; cmd_i++) {
                if (seg_cmd->cmd == LC_SEGMENT_64) {
                    printf(KCLAYOUTFORMATSTR64, seg_cmd->segname,
                           seg_cmd->vmaddr, seg_cmd->vmsize,
                           uuid_cmd ? uuid_string : "",
                           idBuffer);
                }
                seg_cmd = (struct segment_command_64 *) ((uintptr_t)seg_cmd + seg_cmd->cmdsize);
            }
        } else if (mhp) {
            struct segment_command *    seg_cmd;
            seg_cmd = (struct segment_command *) ((uintptr_t)mhp + sizeof(*mhp));

            for (cmd_i = 0; cmd_i < mhp->ncmds; cmd_i++) {
                if (seg_cmd->cmd == LC_SEGMENT) {
                    printf(KCLAYOUTFORMATSTR, seg_cmd->segname,
                           seg_cmd->vmaddr, seg_cmd->vmsize,
                           uuid_cmd ? uuid_string : "", idBuffer);
                }

                seg_cmd = (struct segment_command *) ((uintptr_t)seg_cmd + seg_cmd->cmdsize);
            }
        }
    } else if (json) {
        if (mhp64) {
            printf(", \"segments\": [");
            printJSONSegments(toolArgs, mhp64, true);
            printf("]");
        } else if (mhp) {
            printf(", \"segments\": [");
            printJSONSegments(toolArgs, mhp, false);
            printf("]");
        }
        printf("}");
    } else if (beVerbose) {
        printf("\t-> load address:   0x%0.8llx, "
               "size              = 0x%0.8llx,\n"
               "\t-> source address: 0x%0.8llx, "
               "kmod_info address = 0x%0.8llx\n",
               kextLoadAddress, kextExecutableSize, kextSourceAddress, kextKmodInfoAddress);

        if (mhp64) {
            struct segment_command_64 * seg_cmd;
            seg_cmd = (struct segment_command_64 *) ((uintptr_t)mhp64 + sizeof(*mhp64));

            printf("\t-> macho header: "
                   "magic = 0x%0.4x, "
                   "ncmds = %d, "
                   "sizeofcmds = %d\n",
                   mhp64->magic, mhp64->ncmds, mhp64->sizeofcmds);
            
            for (cmd_i = 0; cmd_i < mhp64->ncmds; cmd_i++) {
                if (seg_cmd->cmd == LC_SEGMENT_64) {
                    printf("\t\t-> %s: "
                           "segname = %s, "
                           "cmdsize = %d, "
                           "nsects = %d, "
                           "vmaddr = 0x%0.8llx, "
                           "vmsize = 0x%0.8llx (%llu), "
                           "fileoff = 0x%0.8llx (%llu), "
                           "filesize = 0x%0.8llx (%llu) \n",
                           getSegmentCommandName(seg_cmd->cmd),
                           seg_cmd->segname,
                           seg_cmd->cmdsize,
                           seg_cmd->nsects,
                           seg_cmd->vmaddr,
                           seg_cmd->vmsize,
                           seg_cmd->vmsize,
                           seg_cmd->fileoff,
                           seg_cmd->fileoff,
                           seg_cmd->filesize,
                           seg_cmd->filesize);
                    
                    struct section_64 *sect = NULL;
                    u_int j = 0;
                    sect = (struct section_64 *) (&seg_cmd[1]);
                    for (j = 0; j < seg_cmd->nsects; ++j, ++sect) {
                        printf("\t\t\t-> sectname = %s: "
                               "addr = 0x%0.8llx, "
                               "size = 0x%0.8llx (%llu), "
                               "offset = 0x%0.4x (%u), "
                               "reloff = 0x%0.4x (%u), "
                               "nreloc = 0x%0.4x (%u)\n",
                               sect->sectname,
                               sect->addr,
                               sect->size,
                               sect->size,
                               sect->offset,
                               sect->offset,
                               sect->reloff,
                               sect->reloff,
                               sect->nreloc,
                               sect->nreloc);
                    }
                }
                else {
                    printf("\t\t-> %s: "
                           "cmd = %d, "
                           "cmdsize = %d \n",
                           getSegmentCommandName(seg_cmd->cmd),
                           seg_cmd->cmd,
                           seg_cmd->cmdsize);
                }
                seg_cmd = (struct segment_command_64 *) ((uintptr_t)seg_cmd + seg_cmd->cmdsize);
            } // for loop...
        }
        else if (mhp) {
            struct segment_command *    seg_cmd;
            seg_cmd = (struct segment_command *) ((uintptr_t)mhp + sizeof(*mhp));

            printf("\t-> macho header: "
                   "magic = 0x%0.4x, "
                   "ncmds = %d, "
                   "sizeofcmds = %d\n",
                   mhp->magic, mhp->ncmds, mhp->sizeofcmds);

            for (cmd_i = 0; cmd_i < mhp->ncmds; cmd_i++) {
                if (seg_cmd->cmd == LC_SEGMENT) {
                    printf("\t\t-> %s: "
                           "segname = %s, "
                           "cmdsize = %d, "
                           "nsects = %d, "
                           "vmaddr = 0x%0.4x, "
                           "vmsize = 0x%0.4x (%u), "
                           "fileoff = 0x%0.4x (%u), "
                           "filesize = 0x%0.4x (%u) \n",
                           getSegmentCommandName(seg_cmd->cmd),
                           seg_cmd->segname,
                           seg_cmd->cmdsize,
                           seg_cmd->nsects,
                           seg_cmd->vmaddr,
                           seg_cmd->vmsize,
                           seg_cmd->vmsize,
                           seg_cmd->fileoff,
                           seg_cmd->fileoff,
                           seg_cmd->filesize,
                           seg_cmd->filesize);

                    struct section *sect = NULL;
                    u_int j = 0;
                    sect = (struct section *) (&seg_cmd[1]);
                    for (j = 0; j < seg_cmd->nsects; ++j, ++sect) {
                        printf("\t\t\t-> sectname = %s: "
                               "addr = 0x%0.4x, "
                               "size = 0x%0.4x (%u), "
                               "offset = 0x%0.4x (%u), "
                               "reloff = 0x%0.4x (%u), "
                               "nreloc = 0x%0.4x (%u) \n",
                               sect->sectname,
                               sect->addr,
                               sect->size,
                               sect->size,
                               sect->offset,
                               sect->offset,
                               sect->reloff,
                               sect->reloff,
                               sect->nreloc,
                               sect->nreloc);
                    }
                }
                else {
                    printf("\t\t-> %s: "
                           "cmd = %d, "
                           "cmdsize = %d \n",
                           getSegmentCommandName(seg_cmd->cmd),
                           seg_cmd->cmd,
                           seg_cmd->cmdsize);
                }
                seg_cmd = (struct segment_command *) ((uintptr_t)seg_cmd + seg_cmd->cmdsize);
            } // for loop...
        }
    }

    /* extract the kext from the kernel cache */
    if (shouldExtractKext && kextTextBytes && kextExecutableSize != 0) {
        extractKext(kcmap, idBuffer, kextTextBytes,
                    (mhp64 ? (void *)mhp64 : (void *)mhp),
                    (mhp64 ? true : false), toolArgs, isSplitKext);
    }

finish:
    return;
}


/*******************************************************************************
 *******************************************************************************/
static const char * getSegmentCommandName(uint32_t theSegCommand)
{
    const char * theResult;
    
    switch(theSegCommand) {
        case LC_SYMTAB:
            theResult = "LC_SYMTAB";
            break;
        case LC_DYSYMTAB:
            theResult = "LC_DYSYMTAB";
            break;
        case LC_SEGMENT_64:
            theResult = "LC_SEGMENT_64";
            break;
        case LC_SEGMENT:
            theResult = "LC_SEGMENT";
            break;
        case LC_UUID:
            theResult = "LC_UUID";
            break;
        case LC_SOURCE_VERSION:
            theResult = "LC_SOURCE_VERSION";
            break;
        case LC_CODE_SIGNATURE:
            theResult = "LC_CODE_SIGNATURE";
            break;
        case LC_SEGMENT_SPLIT_INFO:
            theResult = "LC_SEGMENT_SPLIT_INFO";
            break;
        default:
            theResult = "Unknown";
            break;
    }
    return(theResult);
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
      "usage: %1$s [-arch archname] [-j] [-l] [-M] [-u] [-v] [-x prefix] [--] kernelcache [bundle-id ...]\n"
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
        "        print kernel cache Mach-O header\n",
            kOptNameMachO, kOptMachO);
    fprintf(stderr, "-%s (-%c):\n"
        "        print kext load addresses and UUIDs\n",
            kOptNameUUID, kOptUUID);
    fprintf(stderr, "-%s (-%c) <prefix>:\n"
        "        extract named kexts from the cache into files prefixed with <prefix>\n",
            kOptNameSaveKext, kOptSaveKext);
    fprintf(stderr, "-%s (-%c):\n"
        "        emit additional information about kext load addresses and sizes\n",
            kOptNameVerbose, kOptVerbose);
    fprintf(stderr, "-%s (-%c):\n"
        "        print the kernelcache layout map in a human-readable format\n",
            kOptNameLayoutMap, kOptLayoutMap);
    fprintf(stderr, "-%s (-%c):\n"
        "        print the kernelcache layout map as JSON\n",
            kOptNameJSON, kOptJSON);
    fprintf(stderr, "\n");

    fprintf(stderr, "-%s (-%c): print this message and exit\n",
        kOptNameHelp, kOptHelp);

    return;
}

static void createKCMap(const uint8_t *kernelcacheImage, Boolean verbose, struct kcmap **kcmap)
{
    struct load_command *lcp = NULL;
    uint32_t ncmds = 0;

    if (kcmap == NULL)
        return;

    if (ISMACHO64(MAGIC32(kernelcacheImage))) {
        struct mach_header_64 *kext_header = (struct mach_header_64 *)((uintptr_t)kernelcacheImage);
        ncmds = kext_header->ncmds;
        lcp = (struct load_command *)((uintptr_t)kext_header + sizeof(*kext_header));
    } else {
        struct mach_header *kext_header = (struct mach_header *)((uintptr_t)kernelcacheImage);
        ncmds = kext_header->ncmds;
        lcp = (struct load_command *)((uintptr_t)kext_header + sizeof(*kext_header));
    }

    *kcmap = calloc(1, sizeof(*kcmap) + (ncmds * sizeof(struct kcmap_entry)));
    if (*kcmap == NULL)
        return;

    if (verbose)
        printf("Building KC Map from %d load commands...\n", ncmds);

    (*kcmap)->va_start = UINT64_MAX;
    (*kcmap)->cache_start = kernelcacheImage;
    struct kcmap_entry *entry = &((*kcmap)->entries[0]);

    for (uint32_t i = 0; i < ncmds; i++) {
        if (lcp->cmd == LC_SEGMENT_64) {
            struct segment_command_64 *seg_cmd;
            seg_cmd = (struct segment_command_64 *)((uintptr_t)lcp);
            entry->kc_lcp = lcp;
            entry->kc_start = seg_cmd->fileoff;
            entry->kc_end = entry->kc_start + seg_cmd->filesize;
            entry->va_start = seg_cmd->vmaddr;
            entry->va_end = entry->va_start + seg_cmd->vmsize;
            if (entry->va_start < (*kcmap)->va_start)
                (*kcmap)->va_start = entry->va_start;
            if (verbose)
                printf("\t`-> %16s [0x%llx,0x%llx] => [%lld,%lld]\n",
                       seg_cmd->segname[0] ? seg_cmd->segname : "none",
                       entry->va_start, entry->va_end,
                       entry->kc_start, entry->kc_end);
            entry++;
            (*kcmap)->nentries++;
        } else if (lcp->cmd == LC_SEGMENT) {
            struct segment_command *seg_cmd;
            seg_cmd = (struct segment_command *)((uintptr_t)lcp);
            entry->kc_lcp = lcp;
            entry->kc_start = seg_cmd->fileoff;
            entry->kc_end = entry->kc_start + seg_cmd->filesize;
            entry->va_start = seg_cmd->vmaddr;
            entry->va_end = entry->va_start + seg_cmd->vmsize;
            if (entry->va_start < (*kcmap)->va_start)
                (*kcmap)->va_start = entry->va_start;
            if (verbose)
                printf("\t`-> %16s [0x%llx,0x%llx] => [%lld,%lld]\n",
                       seg_cmd->segname[0] ? seg_cmd->segname : "none",
                       entry->va_start, entry->va_end,
                       entry->kc_start, entry->kc_end);
            entry++;
            (*kcmap)->nentries++;
        }
        lcp = (struct load_command *)((uintptr_t)lcp + lcp->cmdsize);
    }

    if (verbose)
        printf("\tDONE: found %d segments, va_start:0x%llx\n",
               (*kcmap)->nentries, (*kcmap)->va_start);
}


static off_t getKCFileOffset(struct kcmap *kcmap, uint8_t *kextStart, uint64_t va_ofst, uint64_t fileoff)
{
    struct kcmap_entry *entry = NULL;
    if (!kcmap)
        return fileoff;
    /*
     * The entries will most likely _not_ be sorted,
     * so we have to do a simple linear search.
     */
    for (int i = 0; i < kcmap->nentries; i++) {
        entry = &(kcmap->entries[i]);
        if (va_ofst >= entry->va_start && va_ofst < entry->va_end) {
            off_t kext_ofst = va_ofst - entry->va_start;
            if (entry->kc_start + kext_ofst >= entry->kc_end) {
                printf("[ERROR] mismatch in KC -> VA map: found 0x%llx in entry:\n"
                       "        {%s: [0x%llx,0x%llx] => [%lld,%lld]}\n"
                       "        but VA offset (%lld) doesn't fit in map KC range!\n",
                       va_ofst,
                       getSegmentCommandName(entry->kc_lcp->cmd),
                       entry->va_start, entry->va_end, entry->kc_start, entry->kc_end,
                       (uint64_t)kext_ofst);
                return fileoff;
            }
            /*
             * We want to return an offset from the start of the kext (which will be used in the macho),
             * so we need to subtract off the distance from the start of the kernel cache.
             */
            off_t seg_ofst = (uintptr_t)kextStart - (uintptr_t)(kcmap->cache_start);
            return entry->kc_start + kext_ofst - seg_ofst;
        }
    }

    return fileoff;
}


static void printMachOHeader(const UInt8 *imagePtr, CFIndex imageSize)
{
    uint64_t i;

    printf("[macho] kernelcache @%p + %d\n", imagePtr, (uint32_t)imageSize);

    if (ISMACHO64(MAGIC32(imagePtr))) {
        struct mach_header_64 *     kext_header;
        struct segment_command_64 * seg_cmd;
        uintptr_t last_cmd;

        kext_header = (struct mach_header_64 *)imagePtr;
        seg_cmd = (struct segment_command_64 *) ((uintptr_t)kext_header + sizeof(*kext_header));
        printf("[macho] header: "
               "magic = 0x%0.4x, "
               "ncmds = %d, "
               "sizeofcmds = %d\n",
               kext_header->magic, kext_header->ncmds, kext_header->sizeofcmds);

        last_cmd = (uintptr_t)seg_cmd + (kext_header->ncmds * sizeof(*seg_cmd));
        if (last_cmd > (uintptr_t)imagePtr + imageSize) {
            fprintf(stderr, "[macho] ERROR: header points off the end of the image!\n");
            fprintf(stderr, "[macho] header@%p, sz:%d, ncmds:%d, last_cmd@%p\n",
                    kext_header, (int)imageSize, kext_header->ncmds, (void *)last_cmd);
            return;
        }

        for (i = 0; i < kext_header->ncmds; i++) {
            if (seg_cmd->cmd == LC_SEGMENT_64) {
                printf("[macho] cmd 0x%02X '%s' segment '%s' vmaddr %p vmsize %llu fileoff %llu filesize %llu nsects %u\n",
                       seg_cmd->cmd,
                       getSegmentCommandName(seg_cmd->cmd),
                       seg_cmd->segname[0] ? seg_cmd->segname : "none",
                       (void *)seg_cmd->vmaddr,
                       seg_cmd->vmsize,
                       seg_cmd->fileoff,
                       seg_cmd->filesize,
                       seg_cmd->nsects);

                struct section_64 *sect = NULL;
                u_int j = 0;
                sect = (struct section_64 *) (&seg_cmd[1]);
                for (j = 0; j < seg_cmd->nsects; ++j, ++sect) {
                    printf("[macho]   `-> sectname '%s' addr %p size %llu offset %u reloff %u nreloc %u\n",
                           sect->sectname[0] ? sect->sectname : "none",
                           (void *)sect->addr,
                           sect->size,
                           sect->offset,
                           sect->reloff,
                           sect->nreloc);
                }
            } else if (seg_cmd->cmd == LC_SYMTAB) {
                struct symtab_command * symtab_cmd = (struct symtab_command *) seg_cmd;
                printf("[macho] cmd 0x%02X '%s' cmdsize %u symoff %u nsyms %u stroff %u strsize %u\n",
                       symtab_cmd->cmd,
                       getSegmentCommandName(symtab_cmd->cmd),
                       symtab_cmd->cmdsize,
                       symtab_cmd->symoff,
                       symtab_cmd->nsyms,
                       symtab_cmd->stroff,
                       symtab_cmd->strsize);
            } else if (seg_cmd->cmd == LC_DYSYMTAB) {
                struct dysymtab_command * dsymtab_cmd = (struct dysymtab_command *) seg_cmd;
                printf("[macho] cmd 0x%02X '%s' cmdsize %u ilocalsym %u nlocalsym %u iextdefsym %u nextdefsym %u\n",
                       dsymtab_cmd->cmd,
                       getSegmentCommandName(dsymtab_cmd->cmd),
                       dsymtab_cmd->cmdsize,
                       dsymtab_cmd->ilocalsym,
                       dsymtab_cmd->nlocalsym,
                       dsymtab_cmd->iextdefsym,
                       dsymtab_cmd->nextdefsym);
                printf("                 '%s' iundefsym %u nundefsym %u tocoff %u ntoc %u modtaboff %u nmodtab %u\n",
                       getSegmentCommandName(dsymtab_cmd->cmd),
                       dsymtab_cmd->iundefsym,
                       dsymtab_cmd->nundefsym,
                       dsymtab_cmd->tocoff,
                       dsymtab_cmd->ntoc,
                       dsymtab_cmd->modtaboff,
                       dsymtab_cmd->nmodtab);
                printf("                 '%s' extrefsymoff %u nextrefsyms %u indirectsymoff %u nindirectsyms %u\n",
                       getSegmentCommandName(dsymtab_cmd->cmd),
                       dsymtab_cmd->extrefsymoff,
                       dsymtab_cmd->nextrefsyms,
                       dsymtab_cmd->indirectsymoff,
                       dsymtab_cmd->nindirectsyms);
                printf("                 '%s' extreloff %u nextrel %u locreloff %u nlocrel %u\n",
                       getSegmentCommandName(dsymtab_cmd->cmd),
                       dsymtab_cmd->extreloff,
                       dsymtab_cmd->nextrel,
                       dsymtab_cmd->locreloff,
                       dsymtab_cmd->nlocrel);
            } else if (seg_cmd->cmd == LC_SEGMENT_SPLIT_INFO) {
                struct linkedit_data_command * lc = (struct linkedit_data_command *) seg_cmd;
                printf("[macho] cmd 0x%02X '%s' cmdsize %u dataoff %u datasize %u\n",
                       seg_cmd->cmd,
                       getSegmentCommandName(seg_cmd->cmd),
                       lc->cmdsize,
                       lc->dataoff,
                       lc->datasize);
            } else {
                printf("[macho] cmd 0x%02X '%s' cmdsize:%u vmaddr:0x%llx vmsize:0x%llx\n",
                       seg_cmd->cmd,
                       getSegmentCommandName(seg_cmd->cmd),
                       seg_cmd->cmdsize,
                       seg_cmd->vmaddr,
                       seg_cmd->vmsize);
            }
            seg_cmd = (struct segment_command_64 *) ((uintptr_t)seg_cmd + seg_cmd->cmdsize);
        } // for each macho command
    } else {
        struct mach_header *kext_header;
        struct load_command *lcp;

        kext_header = (struct mach_header *)imagePtr;
        lcp = (struct load_command *)((uintptr_t)kext_header + sizeof(*kext_header));

        printf("\t-> macho header: "
               "magic = 0x%0.4x, "
               "ncmds = %d, "
               "sizeofcmds = %d\n",
               kext_header->magic, kext_header->ncmds, kext_header->sizeofcmds);

        for (i = 0; i < kext_header->ncmds; i++){
            if (lcp->cmd == LC_SEGMENT) {
                struct segment_command *seg_cmd = (struct segment_command *)((uintptr_t)lcp);
                printf("[macho] cmd 0x%02X '%s' segment '%s' vmaddr %p vmsize %u fileoff %u filesize %u nsects %u\n",
                       seg_cmd->cmd,
                       getSegmentCommandName(seg_cmd->cmd),
                       seg_cmd->segname[0] ? seg_cmd->segname : "none",
                       (void *)(uintptr_t)seg_cmd->vmaddr,
                       seg_cmd->vmsize,
                       seg_cmd->fileoff,
                       seg_cmd->filesize,
                       seg_cmd->nsects);

                    struct section *sect = NULL;
                    sect = (struct section *) (&seg_cmd[1]);
                    for (u_int j = 0; j < seg_cmd->nsects; ++j, ++sect) {
                        printf("[macho]   `-> sectname '%s' addr %p size %u offset %u reloff %u nreloc %u\n",
                               sect->sectname,
                               (void *)(uintptr_t)sect->addr,
                               sect->size,
                               sect->offset,
                               sect->reloff,
                               sect->nreloc);
                    }
            } else if (lcp->cmd == LC_SYMTAB) {
                struct symtab_command * symtab_cmd = (struct symtab_command *)((uintptr_t)lcp);
                printf("[macho] cmd 0x%02X '%s' cmdsize %u symoff %u nsyms %u stroff %u strsize %u\n",
                       symtab_cmd->cmd,
                       getSegmentCommandName(symtab_cmd->cmd),
                       symtab_cmd->cmdsize,
                       symtab_cmd->symoff,
                       symtab_cmd->nsyms,
                       symtab_cmd->stroff,
                       symtab_cmd->strsize);
            } else if (lcp->cmd == LC_DYSYMTAB) {
                struct dysymtab_command * dsymtab_cmd = (struct dysymtab_command *)((uintptr_t)lcp);
                printf("[macho] cmd 0x%02X '%s' cmdsize %u ilocalsym %u nlocalsym %u iextdefsym %u nextdefsym %u\n",
                       dsymtab_cmd->cmd,
                       getSegmentCommandName(dsymtab_cmd->cmd),
                       dsymtab_cmd->cmdsize,
                       dsymtab_cmd->ilocalsym,
                       dsymtab_cmd->nlocalsym,
                       dsymtab_cmd->iextdefsym,
                       dsymtab_cmd->nextdefsym);
                printf("                 '%s' iundefsym %u nundefsym %u tocoff %u ntoc %u modtaboff %u nmodtab %u\n",
                       getSegmentCommandName(dsymtab_cmd->cmd),
                       dsymtab_cmd->iundefsym,
                       dsymtab_cmd->nundefsym,
                       dsymtab_cmd->tocoff,
                       dsymtab_cmd->ntoc,
                       dsymtab_cmd->modtaboff,
                       dsymtab_cmd->nmodtab);
                printf("                 '%s' extrefsymoff %u nextrefsyms %u indirectsymoff %u nindirectsyms %u\n",
                       getSegmentCommandName(dsymtab_cmd->cmd),
                       dsymtab_cmd->extrefsymoff,
                       dsymtab_cmd->nextrefsyms,
                       dsymtab_cmd->indirectsymoff,
                       dsymtab_cmd->nindirectsyms);
                printf("                 '%s' extreloff %u nextrel %u locreloff %u nlocrel %u\n",
                       getSegmentCommandName(dsymtab_cmd->cmd),
                       dsymtab_cmd->extreloff,
                       dsymtab_cmd->nextrel,
                       dsymtab_cmd->locreloff,
                       dsymtab_cmd->nlocrel);
            } else if (lcp->cmd == LC_SEGMENT_SPLIT_INFO) {
                struct linkedit_data_command *le = (struct linkedit_data_command *)((uintptr_t)lcp);
                printf("[macho] cmd 0x%02X '%s' cmdsize %u dataoff %u datasize %u\n",
                       lcp->cmd,
                       getSegmentCommandName(lcp->cmd),
                       le->cmdsize,
                       le->dataoff,
                       le->datasize);
            } else {
                printf("[macho] cmd 0x%02X '%s' cmdsize:%u {%02x %02x %02x %02x%s\n",
                       lcp->cmd,
                       getSegmentCommandName(lcp->cmd),
                       lcp->cmdsize,
                       lcp->cmdsize > 0 ? *(uint8_t *)((uintptr_t)lcp + sizeof(*lcp) + 0) : 0,
                       lcp->cmdsize > 1 ? *(uint8_t *)((uintptr_t)lcp + sizeof(*lcp) + 1) : 0,
                       lcp->cmdsize > 2 ? *(uint8_t *)((uintptr_t)lcp + sizeof(*lcp) + 2) : 0,
                       lcp->cmdsize > 3 ? *(uint8_t *)((uintptr_t)lcp + sizeof(*lcp) + 3) : 0,
                       lcp->cmdsize > 4 ? " ... }" : "}");
            }
            lcp = (struct load_command *)((uintptr_t)lcp + lcp->cmdsize);
        } // for loop
    }
    return;
}
