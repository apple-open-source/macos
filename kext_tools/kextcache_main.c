/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFBundlePriv.h>
#include <errno.h>
#include <libc.h>
#include <libgen.h>     // dirname()
#include <Kernel/libkern/mkext.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fts.h>
#include <paths.h>
#include <mach-o/arch.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/swap.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_types.h>
#include <mach/machine/vm_param.h>
#include <mach/kmod.h>
#include <notify.h>
#include <stdlib.h>
#include <unistd.h>             // sleep(3)
#include <sys/types.h>
#include <sys/stat.h>

#include <DiskArbitration/DiskArbitrationPrivate.h>
#include <IOKit/IOTypes.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitServer.h>
#include <IOKit/IOCFUnserialize.h>
#include <IOKit/IOCFSerialize.h>
#include <libkern/OSByteOrder.h>

#include <IOKit/kext/OSKext.h>
#include <IOKit/kext/OSKextPrivate.h>
#include <bootfiles.h>

#include <IOKit/pwr_mgt/IOPMLib.h>

#include "kextcache_main.h"
#if !NO_BOOT_ROOT
#include "bootcaches.h"
#include "bootroot_internal.h"
#endif /* !NO_BOOT_ROOT */
#include "mkext1_file.h"
#include "compression.h"

// constants
#define MKEXT_PERMS             (0644)

/* The timeout we use when waiting for the system to get to a low load state.
 * We're shooting for about 10 minutes, but we don't want to collide with
 * everyone else who wants to do work 10 minutes after boot, so we just pick
 * a number in that ballpark.
 */
#define kOSKextSystemLoadTimeout        (8 * 60)
#define kOSKextSystemLoadPauseTime      (30)

/*******************************************************************************
* Program Globals
*******************************************************************************/
const char * progname = "(unknown)";


/*******************************************************************************
* Utility and callback functions.
*******************************************************************************/
// put/take helpers
static void waitForIOKitQuiescence(void);
static void waitForGreatSystemLoad(void);

#define kMaxArchs 64
#define kRootPathLen 256

static u_int usecs_from_timeval(struct timeval *t);
static void timeval_from_usecs(struct timeval *t, u_int usecs);
static void timeval_difference(struct timeval *dst, 
    struct timeval *a, struct timeval *b);

/*******************************************************************************
*******************************************************************************/
int main(int argc, char * const * argv)
{
    KextcacheArgs       toolArgs;
    ExitStatus          result          = EX_SOFTWARE;
    Boolean             fatal           = false;

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
    * Check if we were spawned by kextd, set up straightaway
    * for service log filtering, and hook up to ASL.
    */
    if (getenv("KEXTD_SPAWNED")) {
        OSKextSetLogFilter(kDefaultServiceLogFilter | kOSKextLogKextOrGlobalMask,
            /* kernel? */ false);
        OSKextSetLogFilter(kDefaultServiceLogFilter | kOSKextLogKextOrGlobalMask,
            /* kernel? */ true);
        tool_openlog("com.apple.kextcache");
    }

   /*****
    * Process args & check for permission to load.
    */
    result = readArgs(&argc, &argv, &toolArgs);
    if (result != EX_OK) {
        if (result == kKextcacheExitHelp) {
            result = EX_OK;
        }
        goto finish;
    }

   /*****
    * Now that we have a custom verbose level set by options,
    * check the filter kextd passed in and combine them.
    */
    checkKextdSpawnedFilter(/* kernel? */ false);
    checkKextdSpawnedFilter(/* kernel? */ true);
    
    result = checkArgs(&toolArgs);
    if (result != EX_OK) {
        goto finish;
    }

   /* From here on out the default exit status is ok.
    */
    result = EX_OK;

    /* Reduce our priority and throttle I/O, then wait for a good time to run.
     */
    if (toolArgs.lowPriorityFlag) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogDetailLevel | kOSKextLogGeneralFlag,
            "Running in low-priority background mode.");

        setpriority(PRIO_PROCESS, getpid(), 20); // run at really low priority
        setiopolicy_np(IOPOL_TYPE_DISK, IOPOL_SCOPE_PROCESS, IOPOL_THROTTLE);

        /* When building the prelinked kernel, we try to wait for a good time
         * to do work.  We can't do this for an mkext yet because we don't
         * have a way to know if we're blocking reboot.
         */
        if (toolArgs.prelinkedKernelPath) {
            waitForGreatSystemLoad();
        }
    }

   /* The whole point of this program is to update caches, so let's not
    * try to read any (we'll briefly turn this back on when checking them).
    */
    OSKextSetUsesCaches(false);

#if !NO_BOOT_ROOT
   /* If it's a Boot!=root update, call checkUpdateCachesAndBoots() and
    * jump right to exit; B!=R doesn't combine with any other cache building.
    */
    if (toolArgs.updateVolumeURL) {
        // xxx - updateBoots should return only sysexit-type values, not errno
        result = checkUpdateCachesAndBoots(toolArgs.updateVolumeURL,
                                           toolArgs.forceUpdateFlag, 
                                           toolArgs.expectUpToDate, 
                            toolArgs.installerCalled || toolArgs.cachesOnly);
        goto finish;
    }
#endif /* !NO_BOOT_ROOT */

   /* If we're uncompressing the prelinked kernel, take care of that here
    * and exit.
    */
    if (toolArgs.prelinkedKernelPath && !CFArrayGetCount(toolArgs.argURLs) &&
        (toolArgs.compress || toolArgs.uncompress)) 
    {
        result = compressPrelinkedKernel(toolArgs.prelinkedKernelPath,
            /* compress */ toolArgs.compress);
        goto finish;
    }

   /*****
    * Read the kexts we'll be working with; first the set of all kexts, then
    * the repository and named kexts for use with mkext-creation flags.
    */
    if (toolArgs.printTestResults) {
        OSKextSetRecordsDiagnostics(kOSKextDiagnosticsFlagAll);
    }
    toolArgs.allKexts = OSKextCreateKextsFromURLs(kCFAllocatorDefault, toolArgs.argURLs);
    if (!toolArgs.allKexts || !CFArrayGetCount(toolArgs.allKexts)) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "No kernel extensions found.");
        result = EX_SOFTWARE;
        goto finish;
    }

    toolArgs.repositoryKexts = OSKextCreateKextsFromURLs(kCFAllocatorDefault,
        toolArgs.repositoryURLs);
    toolArgs.namedKexts = OSKextCreateKextsFromURLs(kCFAllocatorDefault,
        toolArgs.namedKextURLs);
    if (!toolArgs.repositoryKexts || !toolArgs.namedKexts) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Error reading extensions.");
        result = EX_SOFTWARE;
        goto finish;
    }

    if (result != EX_OK) {
        goto finish;
    }

    if (toolArgs.needLoadedKextInfo) {
        result = getLoadedKextInfo(&toolArgs);
        if (result != EX_OK) {
            goto finish;
        }
    }

    // xxx - we are potentially overwriting error results here
    if (toolArgs.updateSystemCaches) {
        result = updateSystemPlistCaches(&toolArgs);
        // don't goto finish on error here, we might be able to create
        // the other caches
    }

    if (toolArgs.mkextPath) {
        result = createMkext(&toolArgs, &fatal);
        if (fatal) {
            goto finish;
        }
    }

    if (toolArgs.prelinkedKernelPath) {
       /* If we're updating the system prelinked kernel, make sure we aren't
        * Safe Boot, or dire consequences shall result.
        */
        if (toolArgs.needDefaultPrelinkedKernelInfo &&
            OSKextGetActualSafeBoot()) {

            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogGeneralFlag, 
                "Can't update the system prelinked kernel during safe boot.");
            result = EX_OSERR;
            goto finish;
        }

       /* Create/update the prelinked kernel as explicitly requested, or
        * for the running kernel.
        */
        result = createPrelinkedKernel(&toolArgs);
        if (result != EX_OK) {
            goto finish;
        }

    }

finish:

   /* We're actually not going to free anything else because we're exiting!
    */
    exit(result);

    SAFE_RELEASE(toolArgs.kextIDs);
    SAFE_RELEASE(toolArgs.argURLs);
    SAFE_RELEASE(toolArgs.repositoryURLs);
    SAFE_RELEASE(toolArgs.namedKextURLs);
    SAFE_RELEASE(toolArgs.allKexts);
    SAFE_RELEASE(toolArgs.repositoryKexts);
    SAFE_RELEASE(toolArgs.namedKexts);
    SAFE_RELEASE(toolArgs.loadedKexts);
    SAFE_RELEASE(toolArgs.kernelFile);
    SAFE_RELEASE(toolArgs.symbolDirURL);
    SAFE_FREE(toolArgs.mkextPath);
    SAFE_FREE(toolArgs.prelinkedKernelPath);
    SAFE_FREE(toolArgs.kernelPath);

    return result;
}

/*******************************************************************************
*******************************************************************************/
ExitStatus readArgs(
    int            * argc,
    char * const  ** argv,
    KextcacheArgs  * toolArgs)
{
    ExitStatus   result         = EX_USAGE;
    ExitStatus   scratchResult  = EX_USAGE;
    CFStringRef  scratchString  = NULL;  // must release
    CFNumberRef  scratchNumber  = NULL;  // must release
    CFURLRef     scratchURL     = NULL;  // must release
    size_t       len            = 0;
    uint32_t     i              = 0;
    int          optchar        = 0;
    int          longindex      = -1;
    struct stat  sb;

    bzero(toolArgs, sizeof(*toolArgs));
    
   /*****
    * Allocate collection objects.
    */
    if (!createCFMutableSet(&toolArgs->kextIDs, &kCFTypeSetCallBacks)             ||
        !createCFMutableArray(&toolArgs->argURLs, &kCFTypeArrayCallBacks)         ||
        !createCFMutableArray(&toolArgs->repositoryURLs, &kCFTypeArrayCallBacks)  ||
        !createCFMutableArray(&toolArgs->namedKextURLs, &kCFTypeArrayCallBacks)   ||
        !createCFMutableArray(&toolArgs->targetArchs, NULL)) {

        OSKextLogMemError();
        result = EX_OSERR;
        exit(result);
    }

    /*****
    * Process command line arguments.
    */
    while ((optchar = getopt_long_only(*argc, *argv,
        kOptChars, sOptInfo, &longindex)) != -1) {

        SAFE_RELEASE_NULL(scratchString);
        SAFE_RELEASE_NULL(scratchNumber);
        SAFE_RELEASE_NULL(scratchURL);

        /* When processing short (single-char) options, there is no way to
         * express optional arguments.  Instead, we suppress missing option
         * argument errors by adding a leading ':' to the option string.
         * When getopt detects a missing argument, it will return a ':' so that
         * we can screen for options that are not required to have an argument.
         */
        if (optchar == ':') {
            switch (optopt) {
                case kOptPrelinkedKernel:
                    optchar = optopt;
                    break;
                default:
                    OSKextLog(/* kext */ NULL,
                        kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                        "%s: option requires an argument -- -%c.", 
                        progname, optopt);
                    break;
            }
        }

       /* Catch a -m before the switch and redirect it to the latest supported
        * mkext version, so we don't have to duplicate the code block.
        */
        if (optchar == kOptMkext) {
            optchar = 0;
            longopt = kLongOptMkext;
        }

       /* Catch a -e/-system-mkext and redirect to -system-prelinked-kernel.
        */
        if (optchar == kOptSystemMkext) {
            optchar = 0;
            longopt = kLongOptSystemPrelinkedKernel;
        }

        switch (optchar) {
  
            case kOptArch:
                if (!addArchForName(toolArgs, optarg)) {
                    OSKextLog(/* kext */ NULL,
                        kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                        "Unknown architecture %s.", optarg);
                    goto finish;
                }
                toolArgs->explicitArch = true;
                break;
  
            case kOptBundleIdentifier:
                scratchString = CFStringCreateWithCString(kCFAllocatorDefault,
                   optarg, kCFStringEncodingUTF8);
                if (!scratchString) {
                    OSKextLogMemError();
                    result = EX_OSERR;
                    goto finish;
                }
                CFSetAddValue(toolArgs->kextIDs, scratchString);
                break;
  
            case kOptPrelinkedKernel:
                scratchResult = readPrelinkedKernelArgs(toolArgs, *argc, *argv,
                    /* isLongopt */ longindex != -1);
                if (scratchResult != EX_OK) {
                    result = scratchResult;
                    goto finish;
                }
                break;
  
   
#if !NO_BOOT_ROOT
            case kOptForce:
                toolArgs->forceUpdateFlag = true;
                break;
#endif /* !NO_BOOT_ROOT */
  
            case kOptLowPriorityFork:
                toolArgs->lowPriorityFlag = true;
                break;
    
            case kOptHelp:
                usage(kUsageLevelFull);
                result = kKextcacheExitHelp;
                goto finish;
    
            case kOptRepositoryCaches:
                OSKextLog(/* kext */ NULL,
                    kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
                    "-%c is no longer used; ignoring.",
                    kOptRepositoryCaches);
                break;
    
            case kOptKernel:
                if (toolArgs->kernelPath) {
                    OSKextLog(/* kext */ NULL,
                        kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
                        "Warning: kernel file already specified; using last.");
                } else {
                    toolArgs->kernelPath = malloc(PATH_MAX);
                    if (!toolArgs->kernelPath) {
                        OSKextLogMemError();
                        result = EX_OSERR;
                        goto finish;
                    }
                }

                len = strlcpy(toolArgs->kernelPath, optarg, PATH_MAX);
                if (len >= PATH_MAX) {
                    OSKextLog(/* kext */ NULL,
                        kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                        "Error: kernel filename length exceeds PATH_MAX");
                    goto finish;
                }
                break;
    
            case kOptLocalRoot:
                toolArgs->requiredFlagsRepositoriesOnly |=
                    kOSKextOSBundleRequiredLocalRootFlag;
                break;
    
            case kOptLocalRootAll:
                toolArgs->requiredFlagsAll |=
                    kOSKextOSBundleRequiredLocalRootFlag;
                break;
    
            case kOptNetworkRoot:
                toolArgs->requiredFlagsRepositoriesOnly |=
                    kOSKextOSBundleRequiredNetworkRootFlag;
                break;
  
            case kOptNetworkRootAll:
                toolArgs->requiredFlagsAll |=
                    kOSKextOSBundleRequiredNetworkRootFlag;
                break;
  
            case kOptAllLoaded:
                toolArgs->needLoadedKextInfo = true;
                break;
  
            case kOptSafeBoot:
                toolArgs->requiredFlagsRepositoriesOnly |=
                    kOSKextOSBundleRequiredSafeBootFlag;
                break;
  
            case kOptSafeBootAll:
                toolArgs->requiredFlagsAll |=
                    kOSKextOSBundleRequiredSafeBootFlag;
                break;
  
            case kOptTests:
                toolArgs->printTestResults = true;
                break;
  
#if !NO_BOOT_ROOT
            case kOptUpdate:
            case kOptCheckUpdate:
                if (toolArgs->updateVolumeURL) {
                    OSKextLog(/* kext */ NULL,
                        kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
                        "Warning: update volume already specified; using last.");
                    SAFE_RELEASE_NULL(toolArgs->updateVolumeURL);
                }
                // sanity check that the volume exists
                if (stat(optarg, &sb)) {
                    OSKextLog(NULL,kOSKextLogWarningLevel|kOSKextLogFileAccessFlag,
                              "%s - %s.", optarg, strerror(errno));
                    result = EX_NOINPUT;
                    goto finish;
                }

                scratchURL = CFURLCreateFromFileSystemRepresentation(
                    kCFAllocatorDefault,
                    (const UInt8 *)optarg, strlen(optarg), true);
                if (!scratchURL) {
                    OSKextLogStringError(/* kext */ NULL);
                    result = EX_OSERR;
                    goto finish;
                }
                toolArgs->updateVolumeURL = CFRetain(scratchURL);
                if (optchar == kOptCheckUpdate) {
                    toolArgs->expectUpToDate = true;
                }
                break;
#endif /* !NO_BOOT_ROOT */
          
            case kOptQuiet:
                beQuiet();
                break;

            case kOptVerbose:
                scratchResult = setLogFilterForOpt(*argc, *argv,
                    /* forceOnFlags */ kOSKextLogKextOrGlobalMask);
                if (scratchResult != EX_OK) {
                    result = scratchResult;
                    goto finish;
                }
                break;

            case kOptNoAuthentication:
                toolArgs->skipAuthentication = true;
                break;

            case 0:
                switch (longopt) {
                    case kLongOptMkext1:
                    case kLongOptMkext2:
                    // note kLongOptMkext == latest supported version 
                        if (toolArgs->mkextPath) {
                            OSKextLog(/* kext */ NULL,
                                kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
                                "Warning: output mkext file already specified; using last.");
                        } else {
                            toolArgs->mkextPath = malloc(PATH_MAX);
                            if (!toolArgs->mkextPath) {
                                OSKextLogMemError();
                                result = EX_OSERR;
                                goto finish;
                            }
                        }

                        len = strlcpy(toolArgs->mkextPath, optarg, PATH_MAX);
                        if (len >= PATH_MAX) {
                            OSKextLog(/* kext */ NULL,
                                kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                                "Error: mkext filename length exceeds PATH_MAX");
                            goto finish;
                        }
                        
                        if (longopt == kLongOptMkext1) {
                            toolArgs->mkextVersion = 1;
                        } else if (longopt == kLongOptMkext2) {
                            toolArgs->mkextVersion = 2;
                        } else {
                            OSKextLog(/* kext */ NULL,
                                kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                                "Intenral error.");
                        }
                        break;

                    case kLongOptVolumeRoot:
                        if (toolArgs->volumeRootURL) {
                            OSKextLog(/* kext */ NULL,
                                kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
                                "Warning: volume root already specified; using last.");
                            SAFE_RELEASE_NULL(toolArgs->volumeRootURL);
                        }

                        scratchURL = CFURLCreateFromFileSystemRepresentation(
                            kCFAllocatorDefault,
                            (const UInt8 *)optarg, strlen(optarg), true);
                        if (!scratchURL) {
                            OSKextLogStringError(/* kext */ NULL);
                            result = EX_OSERR;
                            goto finish;
                        }

                        toolArgs->volumeRootURL = CFRetain(scratchURL);
                        break;


                    case kLongOptSystemCaches:
                        toolArgs->updateSystemCaches = true;
                        setSystemExtensionsFolders(toolArgs);
                        break;

                    case kLongOptCompressed:
                        toolArgs->compress = true;
                        break;

                    case kLongOptUncompressed:
                        toolArgs->uncompress = true;
                        break;

                    case kLongOptSymbols:
                        if (toolArgs->symbolDirURL) {
                            OSKextLog(/* kext */ NULL,
                                kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
                                "Warning: symbol directory already specified; using last.");
                            SAFE_RELEASE_NULL(toolArgs->symbolDirURL);
                        }

                        scratchURL = CFURLCreateFromFileSystemRepresentation(
                            kCFAllocatorDefault,
                            (const UInt8 *)optarg, strlen(optarg), true);
                        if (!scratchURL) {
                            OSKextLogStringError(/* kext */ NULL);
                            result = EX_OSERR;
                            goto finish;
                        }

                        toolArgs->symbolDirURL = CFRetain(scratchURL);
                        toolArgs->generatePrelinkedSymbols = true;
                        break;

                    case kLongOptSystemPrelinkedKernel:
                        scratchResult = setPrelinkedKernelArgs(toolArgs,
                            /* filename */ NULL);
                        if (scratchResult != EX_OK) {
                            result = scratchResult;
                            goto finish;
                        }
                        toolArgs->needLoadedKextInfo = true;
                        toolArgs->requiredFlagsRepositoriesOnly |=
                            kOSKextOSBundleRequiredLocalRootFlag;
                        break;

                    case kLongOptAllPersonalities:
                        toolArgs->includeAllPersonalities = true;
                        break;

                    case kLongOptNoLinkFailures:
                        toolArgs->noLinkFailures = true;
                        break;

                    case kLongOptStripSymbols:
                        toolArgs->stripSymbols = true;
                        break;

#if !NO_BOOT_ROOT
                    case kLongOptInstaller:
                        toolArgs->installerCalled = true;
                        break;
                    case kLongOptCachesOnly:
                        toolArgs->cachesOnly = true;
                        break;
#endif /* !NO_BOOT_ROOT */

                    default:
                       /* Because we use ':', getopt_long doesn't print an error message.
                        */
                        OSKextLog(/* kext */ NULL,
                            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                            "unrecognized option %s", (*argv)[optind-1]);
                        goto finish;
                        break;
                }
                break;

            default:
               /* Because we use ':', getopt_long doesn't print an error message.
                */
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

   /* Update the argc & argv seen by main() so that boot<>root calls
    * handle remaining args.
    */
    *argc -= optind;
    *argv += optind;

   /*****
    * If we aren't doing a boot<>root update, record the kext & directory names
    * from the command line. (If we are doing a boot<>root update, remaining
    * command line args are processed later.)
    */
    if (!toolArgs->updateVolumeURL) {
        for (i = 0; i < *argc; i++) {
            SAFE_RELEASE_NULL(scratchURL);
            SAFE_RELEASE_NULL(scratchString);

            scratchURL = CFURLCreateFromFileSystemRepresentation(
                kCFAllocatorDefault,
                (const UInt8 *)(*argv)[i], strlen((*argv)[i]), true);
            if (!scratchURL) {
                OSKextLogMemError();
                result = EX_OSERR;
                goto finish;
            }
            CFArrayAppendValue(toolArgs->argURLs, scratchURL);

            scratchString = CFURLCopyPathExtension(scratchURL);
            if (scratchString && CFEqual(scratchString, CFSTR("kext"))) {
                CFArrayAppendValue(toolArgs->namedKextURLs, scratchURL);
            } else {
                CFArrayAppendValue(toolArgs->repositoryURLs, scratchURL);
            }
        }
    }

    result = EX_OK;

finish:
    SAFE_RELEASE(scratchString);
    SAFE_RELEASE(scratchNumber);
    SAFE_RELEASE(scratchURL);

    if (result == EX_USAGE) {
        usage(kUsageLevelBrief);
    }
    return result;
}

/*******************************************************************************
*******************************************************************************/
ExitStatus readPrelinkedKernelArgs(
    KextcacheArgs * toolArgs,
    int             argc,
    char * const  * argv,
    Boolean         isLongopt)
{
    char * filename = NULL;  // do not free

    if (optarg) {
        filename = optarg;
    } else if (isLongopt && optind < argc) {
        filename = argv[optind];
        optind++;
    }
    
    if (filename && !filename[0]) {
        filename = NULL;
    }

    return setPrelinkedKernelArgs(toolArgs, filename);
}

/*******************************************************************************
*******************************************************************************/
ExitStatus setPrelinkedKernelArgs(
    KextcacheArgs * toolArgs,
    char          * filename)
{
    ExitStatus          result          = EX_USAGE;

    if (toolArgs->prelinkedKernelPath) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
            "Warning: prelinked kernel already specified; using last.");
    } else {
        toolArgs->prelinkedKernelPath = malloc(PATH_MAX);
        if (!toolArgs->prelinkedKernelPath) {
            OSKextLogMemError();
            result = EX_OSERR;
            goto finish;
        }
    }

   /* If we don't have a filename we construct a default one, automatically
    * add the system extensions folders, and note that we're using default
    * info.
    */
    if (!filename) {
#if NO_BOOT_ROOT
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Error: prelinked kernel filename required");
        goto finish;
#else
        if (!setDefaultPrelinkedKernel(toolArgs)) {
            goto finish;
        }
        toolArgs->needDefaultPrelinkedKernelInfo = true;
        setSystemExtensionsFolders(toolArgs);
#endif /* NO_BOOT_ROOT */
    } else {
        size_t len = strlcpy(toolArgs->prelinkedKernelPath, filename, PATH_MAX);
        if (len >= PATH_MAX) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                "Error: prelinked kernel filename length exceeds PATH_MAX");
            goto finish;
        }
    }

    result = EX_OK;
finish:
    return result;
}

#if !NO_BOOT_ROOT
/*******************************************************************************
*******************************************************************************/
Boolean setDefaultKernel(KextcacheArgs * toolArgs)
{
    Boolean      result = FALSE;
    size_t       length = 0;

    toolArgs->haveKernelMtime = FALSE;

    if (!toolArgs->kernelPath) {
        toolArgs->kernelPath = malloc(PATH_MAX);
        if (!toolArgs->kernelPath) {
            OSKextLogMemError();
            result = EX_OSERR;
            goto finish;
        }
    }
    length = strlcpy(toolArgs->kernelPath,
        kDefaultKernel,
        PATH_MAX);
    if (length >= PATH_MAX) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Error: kernel filename length exceeds PATH_MAX");
        goto finish;
    }

    if (EX_OK != statPath(toolArgs->kernelPath, &toolArgs->kernelStatBuffer)) {
        goto finish;
    }
    toolArgs->haveKernelMtime = TRUE;

    result = TRUE;

finish:

    return result;
}

/*******************************************************************************
*******************************************************************************/
Boolean setDefaultPrelinkedKernel(KextcacheArgs * toolArgs)
{
    Boolean      result              = FALSE;
    const char * prelinkedKernelFile = NULL;
    size_t       length              = 0;

    prelinkedKernelFile =
        _kOSKextCachesRootFolder "/" _kOSKextStartupCachesSubfolder "/" 
        _kOSKextPrelinkedKernelBasename;

    length = strlcpy(toolArgs->prelinkedKernelPath, 
        prelinkedKernelFile, PATH_MAX);
    if (length >= PATH_MAX) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Error: prelinked kernel filename length exceeds PATH_MAX");
        goto finish;
    }
    
    result = TRUE;

finish:
    return result;
}
#endif /* !NO_BOOT_ROOT */

/*******************************************************************************
*******************************************************************************/
void setSystemExtensionsFolders(KextcacheArgs * toolArgs)
{
    CFArrayRef sysExtensionsFolders = OSKextGetSystemExtensionsFolderURLs();

    CFArrayAppendArray(toolArgs->argURLs,
        sysExtensionsFolders, RANGE_ALL(sysExtensionsFolders));
    CFArrayAppendArray(toolArgs->repositoryURLs,
        sysExtensionsFolders, RANGE_ALL(sysExtensionsFolders));

    return;
}

/*******************************************************************************
*******************************************************************************/
static void
waitForIOKitQuiescence(void)
{
    kern_return_t    kern_result = 0;;
    mach_timespec_t  waitTime = { 40, 0 };

    OSKextLog(/* kext */ NULL,
        kOSKextLogProgressLevel | kOSKextLogIPCFlag,
        "Waiting for I/O Kit to quiesce.");

    kern_result = IOKitWaitQuiet(kIOMasterPortDefault, &waitTime);
    if (kern_result == kIOReturnTimeout) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogIPCFlag,
            "IOKitWaitQuiet() timed out.");
    } else if (kern_result != kOSReturnSuccess) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "IOKitWaitQuiet() failed - %s.",
            safe_mach_error_string(kern_result));
    }
}

/*******************************************************************************
* Wait for the system to report that it's a good time to do work.  We define a
* good time to be when the IOSystemLoadAdvisory API returns a combined level of 
* kIOSystemLoadAdvisoryLevelGreat, and we'll wait up to kOSKextSystemLoadTimeout
* seconds for the system to enter that state before we begin our work.  If there
* is an error in this function, we just return and get started with the work.
*******************************************************************************/
static void
waitForGreatSystemLoad(void)
{
    struct timeval currenttime;
    struct timeval endtime;
    struct timeval timeout;
    fd_set readfds;
    fd_set tmpfds;
    uint64_t systemLoadAdvisoryState            = 0;
    uint32_t notifyStatus                       = 0;
    uint32_t usecs                              = 0;
    int systemLoadAdvisoryFileDescriptor        = 0;    // closed by notify_cancel()
    int systemLoadAdvisoryToken                 = 0;    // must notify_cancel()
    int currentToken                            = 0;    // do not notify_cancel()

    bzero(&currenttime, sizeof(currenttime));
    bzero(&endtime, sizeof(endtime));
    bzero(&timeout, sizeof(timeout));

    OSKextLog(/* kext */ NULL,
        kOSKextLogProgressLevel | kOSKextLogGeneralFlag,
        "Waiting for low system load.");

    /* Register for SystemLoadAdvisory notifications */

    notifyStatus = notify_register_file_descriptor(kIOSystemLoadAdvisoryNotifyName, 
        &systemLoadAdvisoryFileDescriptor, 
        /* flags */ 0, &systemLoadAdvisoryToken);
    if (notifyStatus != NOTIFY_STATUS_OK) {
        goto finish;
    }

    OSKextLog(/* kext */ NULL,
        kOSKextLogDebugLevel | kOSKextLogGeneralFlag,
        "Received initial system load status %llu", systemLoadAdvisoryState);

    /* If it's a good time, we'll just return */

    notifyStatus = notify_get_state(systemLoadAdvisoryToken, &systemLoadAdvisoryState);
    if (notifyStatus != NOTIFY_STATUS_OK) {
        goto finish;
    }

    if (systemLoadAdvisoryState == kIOSystemLoadAdvisoryLevelGreat) {
        goto finish;
    }

    /* Set up the select timers */

    notifyStatus = gettimeofday(&currenttime, NULL);
    if (notifyStatus < 0) {
        goto finish;
    }

    endtime = currenttime;
    endtime.tv_sec += kOSKextSystemLoadTimeout;

    timeval_difference(&timeout, &endtime, &currenttime);
    usecs = usecs_from_timeval(&timeout);

    FD_ZERO(&readfds);
    FD_SET(systemLoadAdvisoryFileDescriptor, &readfds);

    /* Check SystemLoadAdvisory notifications until it's a great time to
     * do work or we hit the timeout.
     */  

    while (usecs) {
        /* Wait for notifications or the timeout */

        FD_COPY(&readfds, &tmpfds);
        notifyStatus = select(systemLoadAdvisoryFileDescriptor + 1, 
            &tmpfds, NULL, NULL, &timeout);
        if (notifyStatus < 0) {
            goto finish;
        }

        /* Set up the next timeout */

        notifyStatus = gettimeofday(&currenttime, NULL);
        if (notifyStatus < 0) {
            goto finish;
        }

        timeval_difference(&timeout, &endtime, &currenttime);
        usecs = usecs_from_timeval(&timeout);

        /* Check the system load state */

        if (!FD_ISSET(systemLoadAdvisoryFileDescriptor, &tmpfds)) {
            continue;
        }

        notifyStatus = read(systemLoadAdvisoryFileDescriptor, 
            &currentToken, sizeof(currentToken));
        if (notifyStatus < 0) {
            goto finish;
        }

        /* The token is written in network byte order. */
        currentToken = ntohl(currentToken);

        if (currentToken != systemLoadAdvisoryToken) {
            continue;
        }

        notifyStatus = notify_get_state(systemLoadAdvisoryToken, 
            &systemLoadAdvisoryState);
        if (notifyStatus != NOTIFY_STATUS_OK) {
            goto finish;
        }

        OSKextLog(/* kext */ NULL,
            kOSKextLogDebugLevel | kOSKextLogGeneralFlag,
            "Received updated system load status %llu", systemLoadAdvisoryState);

        if (systemLoadAdvisoryState == kIOSystemLoadAdvisoryLevelGreat) {
            break;
        }
    }

    OSKextLog(/* kext */ NULL,
        kOSKextLogDebugLevel | kOSKextLogGeneralFlag,
        "Pausing for another %d seconds to avoid work contention",
        kOSKextSystemLoadPauseTime);

    /* We'll wait a random amount longer to avoid colliding with 
     * other work that is waiting for a great time.
     */
    sleep(kOSKextSystemLoadPauseTime);

    OSKextLog(/* kext */ NULL,
        kOSKextLogDebugLevel | kOSKextLogGeneralFlag,
        "System load is low.  Proceeding.\n");
finish:
    if (systemLoadAdvisoryToken) {
        notify_cancel(systemLoadAdvisoryToken);
    }
    return;
}

/*******************************************************************************
*******************************************************************************/
static u_int
usecs_from_timeval(struct timeval *t)
{
    u_int usecs = 0;

    if (t) {
        usecs = (t->tv_sec * 1000) + t->tv_usec;
    }

    return usecs;
}

/*******************************************************************************
*******************************************************************************/
static void
timeval_from_usecs(struct timeval *t, u_int usecs)
{
    if (t) {
        if (usecs > 0) {
            t->tv_sec = usecs / 1000;
            t->tv_usec = usecs % 1000;
        } else {
            bzero(t, sizeof(*t));
        }
    }
}

/*******************************************************************************
* dst = a - b
*******************************************************************************/
static void
timeval_difference(struct timeval *dst, struct timeval *a, struct timeval *b)
{
    u_int ausec = 0, busec = 0, dstusec = 0;

    if (dst) {
        ausec = usecs_from_timeval(a);
        busec = usecs_from_timeval(b);

        if (ausec > busec) {
            dstusec = ausec - busec;
        }

        timeval_from_usecs(dst, dstusec);
    }
}

#if !NO_BOOT_ROOT
/*******************************************************************************
*******************************************************************************/
void setDefaultArchesIfNeeded(KextcacheArgs * toolArgs)
{
   /* If no arches were explicitly specified, toss in the currently-supported
    * ones.
    * xxx - should find a reliable way to do this dynamically for any volume
    * with or without boot-root.
    */
    if (toolArgs->explicitArch) {
        return;
    }

    CFArrayRemoveAllValues(toolArgs->targetArchs);
    addArchForName(toolArgs, "x86_64");
    addArchForName(toolArgs, "i386");
    return;
}
#endif /* !NO_BOOT_ROOT */

/*******************************************************************************
********************************************************************************/
void addArch(
    KextcacheArgs * toolArgs,
    const NXArchInfo  * arch)
{
    if (CFArrayContainsValue(toolArgs->targetArchs, 
        RANGE_ALL(toolArgs->targetArchs), arch)) 
    {
        return;
    }

    CFArrayAppendValue(toolArgs->targetArchs, arch);
}

/*******************************************************************************
*******************************************************************************/
const NXArchInfo * addArchForName(
    KextcacheArgs     * toolArgs,
    const char    * archname)
{
    const NXArchInfo * result = NULL;
    
    result = NXGetArchInfoFromName(archname);
    if (!result) {
        goto finish;
    }

    addArch(toolArgs, result);
    
finish:
    return result;
}

/*******************************************************************************
*******************************************************************************/
void checkKextdSpawnedFilter(Boolean kernelFlag)
{
    const char * environmentVariable  = NULL;  // do not free
    char       * environmentLogFilterString = NULL;  // do not free

    if (kernelFlag) {
        environmentVariable = "KEXT_LOG_FILTER_KERNEL";
    } else {
        environmentVariable = "KEXT_LOG_FILTER_USER";
    }

    environmentLogFilterString = getenv(environmentVariable);

   /*****
    * If we have environment variables for a log spec, take the greater
    * of the log levels and OR together the flags from the environment's &
    * this process's command-line log specs. This way the most verbose setting
    * always applies.
    *
    * Otherwise, set the environment variable in case we spawn children.
    */
    if (environmentLogFilterString) {
        OSKextLogSpec toolLogSpec  = OSKextGetLogFilter(kernelFlag);
        OSKextLogSpec kextdLogSpec = strtoul(environmentLogFilterString, NULL, 16);
        
        OSKextLogSpec toolLogLevel  = toolLogSpec & kOSKextLogLevelMask;
        OSKextLogSpec kextdLogLevel = kextdLogSpec & kOSKextLogLevelMask;
        OSKextLogSpec comboLogLevel = MAX(toolLogLevel, kextdLogLevel);
        
        OSKextLogSpec toolLogFlags  = toolLogSpec & kOSKextLogFlagsMask;
        OSKextLogSpec kextdLogFlags = kextdLogSpec & kOSKextLogFlagsMask;
        OSKextLogSpec comboLogFlags = toolLogFlags | kextdLogFlags |
            kOSKextLogKextOrGlobalMask;
        
        OSKextSetLogFilter(comboLogLevel | comboLogFlags, kernelFlag);
    } else {
        char logSpecBuffer[16];  // enough for a 64-bit hex value

        snprintf(logSpecBuffer, sizeof(logSpecBuffer), "0x%x",
            OSKextGetLogFilter(kernelFlag));
        setenv(environmentVariable, logSpecBuffer, /* overwrite */ 1);
    }
    
    return;
}

/*******************************************************************************
*******************************************************************************/
ExitStatus checkArgs(KextcacheArgs * toolArgs)
{
    ExitStatus  result  = EX_USAGE;

    if (!toolArgs->mkextPath && !toolArgs->prelinkedKernelPath &&
        !toolArgs->updateVolumeURL && !toolArgs->updateSystemCaches) 
    {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "No work to do; check options and try again.");
        goto finish;
    }
    
    if (toolArgs->volumeRootURL && !toolArgs->mkextPath &&
        !toolArgs->prelinkedKernelPath) 
    {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Use -%s only when creating an mkext archive or prelinked kernel.",
            kOptNameVolumeRoot);
        goto finish;
    }

    if (!toolArgs->updateVolumeURL && !CFArrayGetCount(toolArgs->argURLs) &&
        !toolArgs->compress && !toolArgs->uncompress) 
    {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "No kexts or directories specified.");
        goto finish;
    }

    if (!toolArgs->compress && !toolArgs->uncompress) {
        toolArgs->compress = true;
    } else if (toolArgs->compress && toolArgs->uncompress) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Both -%s and -%s specified; using -%s.",
            kOptNameCompressed, kOptNameUncompressed, kOptNameCompressed);
        toolArgs->compress = true;
        toolArgs->uncompress = false;
    }
    
#if !NO_BOOT_ROOT
    if (toolArgs->forceUpdateFlag) {
        if (toolArgs->expectUpToDate || !toolArgs->updateVolumeURL) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                "-%s (-%c) is allowed only with -%s (-%c).",
                kOptNameForce, kOptForce, kOptNameUpdate, kOptUpdate);
            goto finish;
        }
    }
    if (toolArgs->installerCalled) {
        if (toolArgs->expectUpToDate || !toolArgs->updateVolumeURL) {
            OSKextLog(/* kext */ NULL,
                      kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                      "-%s is allowed only with -%s (-%c).",
                      kOptNameInstaller, kOptNameUpdate, kOptUpdate);
            goto finish;
        }
    }
    if (toolArgs->cachesOnly) {
        if (toolArgs->expectUpToDate || !toolArgs->updateVolumeURL) {
            OSKextLog(/* kext */ NULL,
                      kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                      "-%s is allowed only with -%s (-%c).",
                      kOptNameCachesOnly, kOptNameUpdate, kOptUpdate);
            goto finish;
        }
    }
#endif /* !NO_BOOT_ROOT */

    if (toolArgs->updateVolumeURL) {
        if (toolArgs->mkextPath || toolArgs->prelinkedKernelPath) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                "Can't create mkext or prelinked kernel when updating volumes.");
        }
    }

#if !NO_BOOT_ROOT
   /* This is so lame.
    */
    setDefaultArchesIfNeeded(toolArgs);
#endif /* !NO_BOOT_ROOT */

   /* xxx - Old kextcache behavior was to just check the time of the
    * first directory argument given. Ideally we'd check every single
    * folder & kext cached, but that's prohibitively complicated....
    */
    if (!toolArgs->haveFolderMtime && CFArrayGetCount(toolArgs->repositoryURLs)) {
        CFURLRef firstURL = (CFURLRef)CFArrayGetValueAtIndex(
            toolArgs->repositoryURLs, 0);

        if (firstURL) {
            result = statURL(firstURL, &toolArgs->firstFolderStatBuffer);
            if (result != EX_OK) {
                goto finish;
            }
            toolArgs->haveFolderMtime = true;
        }
    }

#if !NO_BOOT_ROOT
    if (toolArgs->needDefaultPrelinkedKernelInfo && !toolArgs->kernelPath) {       
        if (!setDefaultKernel(toolArgs)) {
            goto finish;
        }
    }
#endif /* !NO_BOOT_ROOT */

    if (toolArgs->prelinkedKernelPath && CFArrayGetCount(toolArgs->argURLs)) {
         if (!toolArgs->kernelPath) {
            OSKextLog(/* kext */ NULL,
                    kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                    "No kernel specified for prelinked kernel generation.");
            goto finish;
        } else {
            result = statPath(toolArgs->kernelPath, &toolArgs->kernelStatBuffer);
            if (result != EX_OK) {
                goto finish;
            }
            toolArgs->haveKernelMtime = true;
        }
    }

   /* Updating system caches requires no additional kexts or repositories,
    * and must run as root.
    */
    if (toolArgs->needDefaultPrelinkedKernelInfo ||
            toolArgs->updateSystemCaches) {

        if (CFArrayGetCount(toolArgs->namedKextURLs) || CFSetGetCount(toolArgs->kextIDs) ||
            !CFEqual(toolArgs->repositoryURLs, OSKextGetSystemExtensionsFolderURLs())) {
            
            OSKextLog(/* kext */ NULL,
                    kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                    "Custom kexts and repository directories are not allowed "
                    "when updating system kext caches.");
            result = EX_USAGE;
            goto finish;

        }

        if (geteuid() != 0) {
            OSKextLog(/* kext */ NULL,
                    kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                    "You must be running as root to update system kext caches.");
            result = EX_NOPERM;
            goto finish;
        }
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
ExitStatus 
statURL(CFURLRef anURL, struct stat * statBuffer)
{
    ExitStatus result = EX_OSERR;
    char path[PATH_MAX];

    if (!CFURLGetFileSystemRepresentation(anURL, /* resolveToBase */ true,
            (UInt8 *)path, sizeof(path))) 
    {
        OSKextLogStringError(/* kext */ NULL);
        goto finish;
    }

    result = statPath(path, statBuffer);
    if (!result) {
        goto finish;
    }

    result = EX_OK;
finish:
    return result;
}

/*******************************************************************************
*******************************************************************************/
ExitStatus
statPath(const char *path, struct stat *statBuffer)
{
    ExitStatus result = EX_OSERR;

    if (stat(path, statBuffer)) {
        OSKextLog(/* kext */ NULL,
                kOSKextLogDebugLevel | kOSKextLogGeneralFlag,
                "Can't stat %s - %s.", path, strerror(errno));
        goto finish;
    }

    result = EX_OK;

finish:
    return result;
}

/*******************************************************************************
*******************************************************************************/
ExitStatus
getLoadedKextInfo(
    KextcacheArgs *toolArgs)
{
    ExitStatus  result                  = EX_SOFTWARE;
    CFArrayRef  requestedIdentifiers    = NULL; // must release

    /* Let I/O Kit settle down before we poke at it.
     */
    
    (void) waitForIOKitQuiescence();

    /* Get the list of requested bundle IDs from the kernel and find all of
     * the associated kexts.
     */

    requestedIdentifiers = OSKextCopyAllRequestedIdentifiers();
    if (!requestedIdentifiers) {
        goto finish;
    }

    toolArgs->loadedKexts = OSKextCopyKextsWithIdentifiers(requestedIdentifiers);
    if (!toolArgs->loadedKexts) {
        goto finish;
    }

    result = EX_OK;

finish:
    SAFE_RELEASE(requestedIdentifiers);

    return result;
}

#pragma mark System Plist Caches

/*******************************************************************************
*******************************************************************************/
ExitStatus updateSystemPlistCaches(KextcacheArgs * toolArgs)
{
    ExitStatus         result               = EX_OSERR;
    ExitStatus         directoryResult      = EX_OK;  // flipped to error as needed
    CFArrayRef         systemExtensionsURLs = NULL;   // do not release
    CFArrayRef         kexts                = NULL;   // must release
    CFURLRef           folderURL            = NULL;   // do not release
    char               folderPath[PATH_MAX] = "";
    const NXArchInfo * startArch            = OSKextGetArchitecture();
    CFArrayRef         directoryValues      = NULL;   // must release
    CFArrayRef         personalities        = NULL;   // must release
    CFIndex            count, i;

   /* We only care about updating info for the system extensions folders.
    */
    systemExtensionsURLs = OSKextGetSystemExtensionsFolderURLs();
    if (!systemExtensionsURLs) {
        OSKextLogMemError();
        result = EX_OSERR;
        goto finish;
    }

    kexts = OSKextCreateKextsFromURLs(kCFAllocatorDefault, systemExtensionsURLs);
    if (!kexts) {
        goto finish;
    }

   /* Update the global personalities & property-value caches, each per arch.
    */
    for (i = 0; i < CFArrayGetCount(toolArgs->targetArchs); i++) {
        const NXArchInfo * targetArch = 
            CFArrayGetValueAtIndex(toolArgs->targetArchs, i);

        SAFE_RELEASE_NULL(personalities);

       /* Set the active architecture for scooping out personalities and such.
        */        
        if (!OSKextSetArchitecture(targetArch)) {
            goto finish;
        }

        personalities = OSKextCopyPersonalitiesOfKexts(kexts);
        if (!personalities) {
            goto finish;
        }

        if (!_OSKextWriteCache(systemExtensionsURLs, CFSTR(kIOKitPersonalitiesKey),
            targetArch, _kOSKextCacheFormatIOXML, personalities)) {

            goto finish;
        }

       /* Loginwindow asks us for this property so let's spare lots of I/O
        * by caching it. This read function call updates the caches for us;
        * we don't use the output.
        */
        if (!readSystemKextPropertyValues(CFSTR(kOSBundleHelperKey), targetArch,
                /* forceUpdate? */ true, /* values */ NULL)) {

            goto finish;
        }
    }

   /* Update per-directory caches. This is just KextIdentifiers any more.
    */
    count = CFArrayGetCount(systemExtensionsURLs);
    for (i = 0; i < count; i++) {

        folderURL = CFArrayGetValueAtIndex(systemExtensionsURLs, i);

        if (!CFURLGetFileSystemRepresentation(folderURL, /* resolveToBase */ true,
                    (UInt8 *)folderPath, sizeof(folderPath))) {

            OSKextLogStringError(/* kext */ NULL);
            goto finish;
        }
        if (EX_OK != updateDirectoryCaches(toolArgs, folderURL)) {
            directoryResult = EX_OSERR;
        } else {
            OSKextLog(/* kext */ NULL,
                kOSKextLogBasicLevel | kOSKextLogGeneralFlag,
                "Directory caches updated for %s.", folderPath);
        }
    }

    if (directoryResult == EX_OK) {
        result = EX_OK;
    }

finish:
    SAFE_RELEASE(kexts);
    SAFE_RELEASE(directoryValues);
    SAFE_RELEASE(personalities);

    OSKextSetArchitecture(startArch);

    return result;
}

/*******************************************************************************
*******************************************************************************/
ExitStatus updateDirectoryCaches(
        KextcacheArgs * toolArgs,
        CFURLRef        folderURL)
{
    ExitStatus         result           = EX_OK;  // optimistic!
    CFArrayRef         kexts            = NULL;   // must release

    kexts = OSKextCreateKextsFromURL(kCFAllocatorDefault, folderURL);
    if (!kexts) {
        result = EX_OSERR;
        goto finish;
    }

    if (!_OSKextWriteIdentifierCacheForKextsInDirectory(
                kexts, folderURL, /* force? */ true)) {
        result = EX_OSERR;
        goto finish;
    }

    result = EX_OK;

finish:
    SAFE_RELEASE(kexts);
    return result;
}

#pragma mark Misc Stuff

/*******************************************************************************
*******************************************************************************/
/*******************************************************************************
*******************************************************************************/
/* Open Firmware (PPC only) has an upper limit of 16MB on file transfers,
 * so we'll limit ourselves just beneath that.
 */
#define kOpenFirmwareMaxFileSize (16 * 1024 * 1024)

ExitStatus createMkext(
    KextcacheArgs * toolArgs,
    Boolean       * fatalOut)
{
    struct timeval    cacheFileTimes[2];
    ExitStatus        result         = EX_SOFTWARE;
    CFMutableArrayRef archiveKexts   = NULL;  // must release
    CFMutableArrayRef mkexts         = NULL;  // must release
    CFDataRef         mkext          = NULL;  // must release
    const NXArchInfo *targetArch     = NULL;  // do not free
    int               i;

#if !NO_BOOT_ROOT
    /* Try a lock on the volume for the mkext being updated.
     * The lock prevents kextd from starting up a competing kextcache.
     */
    if (!getenv("_com_apple_kextd_skiplocks")) {
        // xxx - updateBoots + related should return only sysexit-type values, not errno
        result = takeVolumeForPath(toolArgs->mkextPath);
        if (result != EX_OK) {
            goto finish;
        }
    }
#endif /* !NO_BOOT_ROOT */

    if (!createCFMutableArray(&mkexts, &kCFTypeArrayCallBacks)) {
        OSKextLogMemError();
        result = EX_OSERR;
        *fatalOut = true;
        goto finish;
    }

    if (!createCFMutableArray(&archiveKexts, &kCFTypeArrayCallBacks)) {
        OSKextLogMemError();
        result = EX_OSERR;
        goto finish;
    }

    for (i = 0; i < CFArrayGetCount(toolArgs->targetArchs); i++) {
        targetArch = CFArrayGetValueAtIndex(toolArgs->targetArchs, i);

        SAFE_RELEASE_NULL(mkext);

        if (!OSKextSetArchitecture(targetArch)) {
            OSKextLog(/* kext */ NULL,
                    kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                    "Can't set architecture %s to create mkext.",
                    targetArch->name);
            result = EX_OSERR;
            goto finish;
        }

       /*****
        * Figure out which kexts we're actually archiving.
        */
        result = filterKextsForCache(toolArgs, archiveKexts,
                targetArch, fatalOut);
        if (result != EX_OK || *fatalOut) {
            goto finish;
        }

        if (!CFArrayGetCount(archiveKexts)) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogWarningLevel | kOSKextLogArchiveFlag,
                "No kexts found for architecture %s; skipping architecture.",
                targetArch->name);
            continue;
        }

        if (toolArgs->mkextVersion == 2) {
            mkext = OSKextCreateMkext(kCFAllocatorDefault, archiveKexts,
                    toolArgs->volumeRootURL,
                    kOSKextOSBundleRequiredNone, toolArgs->compress);
        } else if (toolArgs->mkextVersion == 1) {
            mkext = createMkext1ForArch(targetArch, archiveKexts,
                    toolArgs->compress);
        }
        if (!mkext) {
            // OSKextCreateMkext() logs an error
            result = EX_OSERR;
            goto finish;
        }
        if (targetArch == NXGetArchInfoFromName("ppc")) {
            if (CFDataGetLength(mkext) > kOpenFirmwareMaxFileSize) {
                OSKextLog(/* kext */ NULL,
                        kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
                        "PPC archive is too large for Open Firmware; aborting.");
                result = EX_SOFTWARE;
                *fatalOut = true;
                goto finish;
            }
        }
        CFArrayAppendValue(mkexts, mkext);
    }

    if (!CFArrayGetCount(mkexts)) {
        OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
                "No mkext archives created.");
        goto finish;
    }

    if (toolArgs->haveFolderMtime) {
        CFURLRef firstURL = CFArrayGetValueAtIndex(toolArgs->repositoryURLs, 0);

        result = getFileURLModTimePlusOne(firstURL, 
            &toolArgs->firstFolderStatBuffer, cacheFileTimes);
        if (result != EX_OK) {
            goto finish;
        }
    }

    result = writeFatFile(toolArgs->mkextPath, mkexts, toolArgs->targetArchs,
        MKEXT_PERMS, (toolArgs->haveFolderMtime) ? cacheFileTimes : NULL);
    if (result != EX_OK) {
        goto finish;
    }

    result = EX_OK;
    OSKextLog(/* kext */ NULL,
        kOSKextLogBasicLevel | kOSKextLogGeneralFlag | kOSKextLogArchiveFlag,
        "Created mkext archive %s.", toolArgs->mkextPath);

finish:
    SAFE_RELEASE(archiveKexts);
    SAFE_RELEASE(mkexts);
    SAFE_RELEASE(mkext);

#if !NO_BOOT_ROOT
    putVolumeForPath(toolArgs->mkextPath, result);
#endif /* !NO_BOOT_ROOT */

    return result;
}

/*******************************************************************************
*******************************************************************************/
ExitStatus
getFilePathTimes(
    const char        * filePath,
    struct timeval      cacheFileTimes[2])
{
    struct stat         statBuffer;
    ExitStatus          result          = EX_SOFTWARE;

    result = statPath(filePath, &statBuffer);
    if (result != EX_OK) {
        goto finish;
    }

    TIMESPEC_TO_TIMEVAL(&cacheFileTimes[0], &statBuffer.st_atimespec);
    TIMESPEC_TO_TIMEVAL(&cacheFileTimes[1], &statBuffer.st_mtimespec);

    result = EX_OK;
finish:
    return result;
}

/*******************************************************************************
*******************************************************************************/
ExitStatus
getFileURLModTimePlusOne(
    CFURLRef            fileURL,
    struct stat       * origStatBuffer,
    struct timeval      cacheFileTimes[2])
{
    ExitStatus   result          = EX_SOFTWARE;
    char         path[PATH_MAX];

    if (!CFURLGetFileSystemRepresentation(fileURL, /* resolveToBase */ true,
            (UInt8 *)path, sizeof(path))) 
    {
        OSKextLogStringError(/* kext */ NULL);
        goto finish;
    }

    result = getFilePathModTimePlusOne(path, origStatBuffer, cacheFileTimes);

finish:
    return result;
}

/*******************************************************************************
*******************************************************************************/
ExitStatus
getFilePathModTimePlusOne(
    const char        * filePath,
    struct stat       * origStatBuffer,
    struct timeval      cacheFileTimes[2])
{
    struct stat         newStatBuffer;
    ExitStatus          result          = EX_SOFTWARE;

    result = statPath(filePath, &newStatBuffer);
    if (result != EX_OK) {
        goto finish;
    }

    result = getModTimePlusOne(filePath, origStatBuffer, &newStatBuffer,
        cacheFileTimes);
    if (result != EX_OK) {
        goto finish;
    }

    result = EX_OK;
finish:
    return result;
}

/*******************************************************************************
*******************************************************************************/
ExitStatus
getModTimePlusOne(
    const char     * path,
    struct stat    * origStatBuffer,
    struct stat    * newStatBuffer,
    struct timeval   cacheFileTimes[2])
{
    ExitStatus       result = EX_SOFTWARE;
    struct timespec  newModTime;
    struct timespec  origModTime;

    origModTime = origStatBuffer->st_mtimespec;
    newModTime = newStatBuffer->st_mtimespec;

    if ((newModTime.tv_sec != origModTime.tv_sec) ||
        (newModTime.tv_nsec != origModTime.tv_nsec)) {

        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag | kOSKextLogFileAccessFlag,
            "Source item %s has changed since starting; "
            "not saving cache file", path);
        result = kKextcacheExitStale;
        goto finish;
    }

    TIMESPEC_TO_TIMEVAL(&cacheFileTimes[0], &newStatBuffer->st_atimespec);
    TIMESPEC_TO_TIMEVAL(&cacheFileTimes[1], &newStatBuffer->st_mtimespec);

    cacheFileTimes[1].tv_sec++;
    result = EX_OK;

finish:
    return result;
}

/*******************************************************************************
*******************************************************************************/
typedef struct {
    KextcacheArgs     * toolArgs;
    CFMutableArrayRef   kextArray;
    Boolean             error;
} FilterIDContext;

void filterKextID(const void * vValue, void * vContext)
{
    CFStringRef       kextID  = (CFStringRef)vValue;
    FilterIDContext * context = (FilterIDContext *)vContext;
    OSKextRef       theKext = OSKextGetKextWithIdentifier(kextID);

   /* This should really be a fatal error but embedded counts on
    * having optional kexts specified by identifier.
    */
    if (!theKext) {
        char kextIDCString[KMOD_MAX_NAME];
        
        CFStringGetCString(kextID, kextIDCString, sizeof(kextIDCString),
            kCFStringEncodingUTF8);
        OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                "Can't find kext with optional identifier %s; skipping.", kextIDCString);
#if 0
        OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                "Error - can't find kext with identifier %s.", kextIDCString);
        context->error = TRUE;
#endif /* 0 */
        goto finish;
    }

    if (kextMatchesFilter(context->toolArgs, theKext, 
            context->toolArgs->requiredFlagsAll) &&
        !CFArrayContainsValue(context->kextArray,
            RANGE_ALL(context->kextArray), theKext)) 
    {
        CFArrayAppendValue(context->kextArray, theKext);
    }

finish:
    return;    
}


/*******************************************************************************
*******************************************************************************/
ExitStatus filterKextsForCache(
        KextcacheArgs     * toolArgs,
        CFMutableArrayRef   kextArray,
        const NXArchInfo  * arch,
        Boolean           * fatalOut)
{
    ExitStatus          result        = EX_SOFTWARE;
    CFMutableArrayRef   firstPassArray = NULL;
    OSKextRequiredFlags requiredFlags;
    CFIndex             count, i;

    if (!createCFMutableArray(&firstPassArray, &kCFTypeArrayCallBacks)) {
        OSKextLogMemError();
        goto finish;
    }
    
   /*****
    * Apply filters to select the kexts.
    *
    * If kexts have been specified by identifier, those are the only kexts we are going to use.
    * Otherwise run through the repository and named kexts and see which ones match the filter.
    */
    if (CFSetGetCount(toolArgs->kextIDs)) {
        FilterIDContext context;

        context.toolArgs = toolArgs;
        context.kextArray = firstPassArray;
        context.error = FALSE;
        CFSetApplyFunction(toolArgs->kextIDs, filterKextID, &context);

        if (context.error) {
            goto finish;
        }

    } else {

       /* Set up the required flags for repository kexts. If any are set from
        * the command line, toss in "Root" and "Console" too.
        */
        requiredFlags = toolArgs->requiredFlagsRepositoriesOnly |
            toolArgs->requiredFlagsAll;
        if (requiredFlags) {
            requiredFlags |= kOSKextOSBundleRequiredRootFlag |
                kOSKextOSBundleRequiredConsoleFlag;
        }

        count = CFArrayGetCount(toolArgs->repositoryKexts);
        for (i = 0; i < count; i++) {

            OSKextRef theKext = (OSKextRef)CFArrayGetValueAtIndex(
                    toolArgs->repositoryKexts, i);

            if (!kextMatchesFilter(toolArgs, theKext, requiredFlags)) {

                char kextPath[PATH_MAX];

                if (!CFURLGetFileSystemRepresentation(OSKextGetURL(theKext),
                    /* resolveToBase */ false, (UInt8 *)kextPath, sizeof(kextPath))) 
                {
                    strlcpy(kextPath, "(unknown)", sizeof(kextPath));
                }

                if (toolArgs->mkextPath) {
                    OSKextLog(/* kext */ NULL,
                        kOSKextLogStepLevel | kOSKextLogArchiveFlag,
                        "%s does not match OSBundleRequired conditions; omitting.",
                        kextPath);
                } else if (toolArgs->prelinkedKernelPath) {
                    OSKextLog(/* kext */ NULL,
                        kOSKextLogStepLevel | kOSKextLogArchiveFlag,
                        "%s is not demanded by OSBundleRequired conditions.",
                        kextPath);
                }
                continue;
            }

            if (!CFArrayContainsValue(firstPassArray, RANGE_ALL(firstPassArray), theKext)) {
                CFArrayAppendValue(firstPassArray, theKext);
            }
        }

       /* Set up the required flags for named kexts. If any are set from
        * the command line, toss in "Root" and "Console" too.
        */
        requiredFlags = toolArgs->requiredFlagsAll;
        if (requiredFlags) {
            requiredFlags |= kOSKextOSBundleRequiredRootFlag |
                kOSKextOSBundleRequiredConsoleFlag;
        }

        count = CFArrayGetCount(toolArgs->namedKexts);
        for (i = 0; i < count; i++) {
            OSKextRef theKext = (OSKextRef)CFArrayGetValueAtIndex(
                    toolArgs->namedKexts, i);

            if (!kextMatchesFilter(toolArgs, theKext, requiredFlags)) {

                char kextPath[PATH_MAX];

                if (!CFURLGetFileSystemRepresentation(OSKextGetURL(theKext),
                    /* resolveToBase */ false, (UInt8 *)kextPath, sizeof(kextPath))) 
                {
                    strlcpy(kextPath, "(unknown)", sizeof(kextPath));
                }

                if (toolArgs->mkextPath) {
                    OSKextLog(/* kext */ NULL,
                        kOSKextLogStepLevel | kOSKextLogArchiveFlag,
                        "%s does not match OSBundleRequired conditions; omitting.",
                        kextPath);
                } else if (toolArgs->prelinkedKernelPath) {
                    OSKextLog(/* kext */ NULL,
                        kOSKextLogStepLevel | kOSKextLogArchiveFlag,
                        "%s is not demanded by OSBundleRequired conditions.",
                        kextPath);
                }
                continue;
            }

            if (!CFArrayContainsValue(firstPassArray, RANGE_ALL(firstPassArray), theKext)) {
                CFArrayAppendValue(firstPassArray, theKext);
            }
        }
    }

   /*****
    * Take all the kexts that matched the filters above and check them for problems.
    */
    CFArrayRemoveAllValues(kextArray);

    count = CFArrayGetCount(firstPassArray);
    if (count) {
        for (i = count - 1; i >= 0; i--) {
            char kextPath[PATH_MAX];
            OSKextRef theKext = (OSKextRef)CFArrayGetValueAtIndex(
                    firstPassArray, i);

            if (!CFURLGetFileSystemRepresentation(OSKextGetURL(theKext),
                /* resolveToBase */ false, (UInt8 *)kextPath, sizeof(kextPath))) 
            {
                strlcpy(kextPath, "(unknown)", sizeof(kextPath));
            }

            /* Skip kexts we have no interest in for the current arch.
             */
            if (!OSKextSupportsArchitecture(theKext, arch)) {
                OSKextLog(/* kext */ NULL,
                    kOSKextLogStepLevel | kOSKextLogArchiveFlag,
                    "%s doesn't support architecture '%s'; skipping.", kextPath,
                    arch->name);
                continue;
            }

            if (!OSKextIsValid(theKext)) {
                // xxx - should also use kOSKextLogArchiveFlag?
                OSKextLog(/* kext */ NULL,
                    kOSKextLogErrorLevel | kOSKextLogArchiveFlag |
                    kOSKextLogValidationFlag | kOSKextLogGeneralFlag, 
                    "%s is not valid; omitting.", kextPath);
                if (toolArgs->printTestResults) {
                    OSKextLogDiagnostics(theKext, kOSKextDiagnosticsFlagAll);
                }
                continue;
            }

            if (!toolArgs->skipAuthentication && !OSKextIsAuthentic(theKext)) {
                OSKextLog(/* kext */ NULL,
                    kOSKextLogErrorLevel | kOSKextLogArchiveFlag |
                    kOSKextLogAuthenticationFlag | kOSKextLogGeneralFlag, 
                    "%s is not authentic; omitting.", kextPath);
                if (toolArgs->printTestResults) {
                    OSKextLogDiagnostics(theKext, kOSKextDiagnosticsFlagAll);
                }
                continue;
            }
            if (!OSKextResolveDependencies(theKext)) {
                OSKextLog(/* kext */ NULL,
                        kOSKextLogWarningLevel | kOSKextLogArchiveFlag |
                        kOSKextLogDependenciesFlag | kOSKextLogGeneralFlag, 
                        "%s is missing dependencies (including anyway; "
                        "dependencies may be available from elsewhere)", kextPath);
                if (toolArgs->printTestResults) {
                    OSKextLogDiagnostics(theKext, kOSKextDiagnosticsFlagAll);
                }
            }
            if (!CFArrayContainsValue(kextArray, RANGE_ALL(kextArray), theKext)) {
                CFArrayAppendValue(kextArray, theKext);
            }
        }
    }

    result = EX_OK;

finish:
    return result;
}

/*******************************************************************************
*******************************************************************************/
Boolean
kextMatchesFilter(
    KextcacheArgs             * toolArgs,
    OSKextRef                   theKext,
    OSKextRequiredFlags         requiredFlags)
{
    Boolean result = false;
    Boolean needLoadedKextInfo = toolArgs->needLoadedKextInfo &&
        (OSKextGetArchitecture() == OSKextGetRunningKernelArchitecture());

    if (needLoadedKextInfo) {
        result = (requiredFlags && OSKextMatchesRequiredFlags(theKext, requiredFlags)) ||
            CFArrayContainsValue(toolArgs->loadedKexts, RANGE_ALL(toolArgs->loadedKexts), theKext);
    } else {
        result = OSKextMatchesRequiredFlags(theKext, requiredFlags);
    }

    return result;
}

/*******************************************************************************
 * Creates a list of architectures to generate prelinked kernel slices for by
 * selecting the requested architectures for which the kernel has a slice.
 * Warns when a requested architecture does not have a corresponding kernel
 * slice.
 *******************************************************************************/
ExitStatus
createPrelinkedKernelArchs(
    KextcacheArgs     * toolArgs,
    CFMutableArrayRef * prelinkArchsOut)
{
    ExitStatus          result          = EX_OSERR;
    CFMutableArrayRef   kernelArchs     = NULL;  // must release
    CFMutableArrayRef   prelinkArchs    = NULL;  // must release
    const NXArchInfo  * targetArch      = NULL;  // do not free
    u_int               i               = 0;

    result = readFatFileArchsWithPath(toolArgs->kernelPath, &kernelArchs);
    if (result != EX_OK) {
        goto finish;
    }

    prelinkArchs = CFArrayCreateMutableCopy(kCFAllocatorDefault,
        /* capacity */ 0, toolArgs->targetArchs);
    if (!prelinkArchs) {
        OSKextLogMemError();
        result = EX_OSERR;
        goto finish;
    }

    for (i = 0; i < CFArrayGetCount(prelinkArchs); ++i) {
        targetArch = CFArrayGetValueAtIndex(prelinkArchs, i);
        if (!CFArrayContainsValue(kernelArchs, 
            RANGE_ALL(kernelArchs), targetArch)) 
        {
            OSKextLog(/* kext */ NULL,
                kOSKextLogWarningLevel | kOSKextLogArchiveFlag,
                "Kernel file %s does not contain requested arch: %s",
                toolArgs->kernelPath, targetArch->name);
            CFArrayRemoveValueAtIndex(prelinkArchs, i);
            i--;
            continue;
        }
    }

    *prelinkArchsOut = (CFMutableArrayRef) CFRetain(prelinkArchs);
    result = EX_OK;

finish:
    SAFE_RELEASE(kernelArchs);
    SAFE_RELEASE(prelinkArchs);

    return result;
}

/*******************************************************************************
 * If the existing prelinked kernel has a valid timestamp, this reads the slices
 * out of that prelinked kernel so we don't have to regenerate them.
 *******************************************************************************/
ExitStatus
createExistingPrelinkedSlices(
    KextcacheArgs     * toolArgs,
    CFMutableArrayRef * existingSlicesOut,
    CFMutableArrayRef * existingArchsOut)
{
    struct timeval      existingFileTimes[2];
    struct timeval      prelinkFileTimes[2];
    ExitStatus          result  = EX_SOFTWARE;

   /* If we aren't updating the system prelinked kernel, then we don't want
    * to reuse any existing slices.
    */
    if (!toolArgs->needDefaultPrelinkedKernelInfo) {
        result = EX_OK;
        goto finish;
    }

    bzero(existingFileTimes, sizeof(existingFileTimes));
    bzero(prelinkFileTimes, sizeof(prelinkFileTimes));

    result = getFilePathTimes(toolArgs->prelinkedKernelPath,
        existingFileTimes);
    if (result != EX_OK) {
        goto finish;
    }

    result = getExpectedPrelinkedKernelModTime(toolArgs, 
        prelinkFileTimes, NULL);
    if (result != EX_OK) {
        goto finish;
    }

    /* We are testing that the existing prelinked kernel still has a valid
     * timestamp by comparing it to the timestamp we are going to use for
     * the new prelinked kernel. If they are equal, we can reuse slices
     * from the existing prelinked kernel.
     */
    if (!timevalcmp(&existingFileTimes[1], &prelinkFileTimes[1], ==)) {
        result = EX_SOFTWARE;
        goto finish;
    }

    result = readMachOSlices(toolArgs->prelinkedKernelPath, 
        existingSlicesOut, existingArchsOut, NULL, NULL);
    if (result != EX_OK) {
        existingSlicesOut = NULL;
        existingArchsOut = NULL;
        goto finish;
    }

    result = EX_OK;

finish:
    return result;
}

/*******************************************************************************
*******************************************************************************/
ExitStatus
createPrelinkedKernel(
    KextcacheArgs     * toolArgs)
{
    ExitStatus          result              = EX_OSERR;
    struct timeval      prelinkFileTimes[2];
    CFMutableArrayRef   generatedArchs      = NULL;  // must release
    CFMutableArrayRef   generatedSymbols    = NULL;  // must release
    CFMutableArrayRef   existingArchs       = NULL;  // must release
    CFMutableArrayRef   existingSlices      = NULL;  // must release
    CFMutableArrayRef   prelinkArchs        = NULL;  // must release
    CFMutableArrayRef   prelinkSlices       = NULL;  // must release
    CFDataRef           prelinkSlice        = NULL;  // must release
    CFDictionaryRef     sliceSymbols        = NULL;  // must release
    const NXArchInfo  * targetArch          = NULL;  // do not free
    Boolean             updateModTime       = false;
    u_int               numArchs            = 0;
    u_int               i                   = 0;
    int                 j                   = 0;

    bzero(prelinkFileTimes, sizeof(prelinkFileTimes));

#if !NO_BOOT_ROOT
    /* Try a lock on the volume for the prelinked kernel being updated.
     * The lock prevents kextd from starting up a competing kextcache.
     */
    if (!getenv("_com_apple_kextd_skiplocks")) {
        // xxx - updateBoots * related should return only sysexit-type values, not errno
        result = takeVolumeForPath(toolArgs->prelinkedKernelPath);
        if (result != EX_OK) {
            goto finish;
        }
    }
#endif /* !NO_BOOT_ROOT */

    result = createPrelinkedKernelArchs(toolArgs, &prelinkArchs);
    if (result != EX_OK) {
        goto finish;
    }
    numArchs = CFArrayGetCount(prelinkArchs);

    /* If we're generating symbols, we'll regenerate all slices.
     */
    if (!toolArgs->symbolDirURL) {
        result = createExistingPrelinkedSlices(toolArgs,
            &existingSlices, &existingArchs);
        if (result != EX_OK) {
            SAFE_RELEASE_NULL(existingSlices);
            SAFE_RELEASE_NULL(existingArchs);
        }
    }

    prelinkSlices = CFArrayCreateMutable(kCFAllocatorDefault,
        numArchs, &kCFTypeArrayCallBacks);
    generatedSymbols = CFArrayCreateMutable(kCFAllocatorDefault, 
        numArchs, &kCFTypeArrayCallBacks);
    generatedArchs = CFArrayCreateMutable(kCFAllocatorDefault, 
        numArchs, NULL);
    if (!prelinkSlices || !generatedSymbols || !generatedArchs) {
        OSKextLogMemError();
        result = EX_OSERR;
        goto finish;
    }

    for (i = 0; i < numArchs; i++) {
        targetArch = CFArrayGetValueAtIndex(prelinkArchs, i);

        SAFE_RELEASE_NULL(prelinkSlice);
        SAFE_RELEASE_NULL(sliceSymbols);

       /* We always create a new prelinked kernel for the current
        * running architecture if asked, but we'll reuse existing slices
        * for other architectures if possible.
        */
        if (existingArchs && 
            targetArch != OSKextGetRunningKernelArchitecture())
        {
            j = CFArrayGetFirstIndexOfValue(existingArchs, 
                RANGE_ALL(existingArchs), targetArch);
            if (j != -1) {
                prelinkSlice = CFArrayGetValueAtIndex(existingSlices, j);
                CFArrayAppendValue(prelinkSlices, prelinkSlice);
                prelinkSlice = NULL;
                OSKextLog(/* kext */ NULL,
                    kOSKextLogDebugLevel | kOSKextLogArchiveFlag,
                    "Using existing prelinked slice for arch %s",
                    targetArch->name);
                continue;
            }
        }

        OSKextLog(/* kext */ NULL,
            kOSKextLogDebugLevel | kOSKextLogArchiveFlag,
            "Generating a new prelinked slice for arch %s",
            targetArch->name);

        result = createPrelinkedKernelForArch(toolArgs, &prelinkSlice,
            &sliceSymbols, targetArch);
        if (result != EX_OK) {
            goto finish;
        }

        CFArrayAppendValue(prelinkSlices, prelinkSlice);
        CFArrayAppendValue(generatedSymbols, sliceSymbols);
        CFArrayAppendValue(generatedArchs, targetArch);
    }

    result = getExpectedPrelinkedKernelModTime(toolArgs, 
        prelinkFileTimes, &updateModTime);
    if (result != EX_OK) {
        goto finish;
    }

    result = writeFatFile(toolArgs->prelinkedKernelPath, prelinkSlices,
        prelinkArchs, MKEXT_PERMS, 
        (updateModTime) ? prelinkFileTimes : NULL);
    if (result != EX_OK) {
        goto finish;
    }
     
    if (toolArgs->symbolDirURL) {
        result = writePrelinkedSymbols(toolArgs->symbolDirURL, 
            generatedSymbols, generatedArchs);
        if (result != EX_OK) {
            goto finish;
        }
    }

    OSKextLog(/* kext */ NULL,
        kOSKextLogBasicLevel | kOSKextLogGeneralFlag | kOSKextLogArchiveFlag,
        "Created prelinked kernel %s.", 
        toolArgs->prelinkedKernelPath);

    result = EX_OK;

finish:
    SAFE_RELEASE(generatedArchs);
    SAFE_RELEASE(generatedSymbols);
    SAFE_RELEASE(existingArchs);
    SAFE_RELEASE(existingSlices);
    SAFE_RELEASE(prelinkArchs);
    SAFE_RELEASE(prelinkSlices);
    SAFE_RELEASE(prelinkSlice);
    SAFE_RELEASE(sliceSymbols);

#if !NO_BOOT_ROOT
    putVolumeForPath(toolArgs->prelinkedKernelPath, result);
#endif /* !NO_BOOT_ROOT */

    return result;
}

/*******************************************************************************
*******************************************************************************/
ExitStatus createPrelinkedKernelForArch(
    KextcacheArgs       * toolArgs,
    CFDataRef           * prelinkedKernelOut,
    CFDictionaryRef     * prelinkedSymbolsOut,
    const NXArchInfo    * archInfo)
{
    ExitStatus result = EX_OSERR;
    CFMutableArrayRef prelinkKexts = NULL;
    CFDataRef kernelImage = NULL;
    CFDataRef prelinkedKernel = NULL;
    uint32_t flags = 0;
    Boolean fatalOut = false;

    /* Retrieve the kernel image for the requested architecture.
     */
    kernelImage = readMachOSliceForArch(toolArgs->kernelPath, archInfo, /* checkArch */ TRUE);
    if (!kernelImage) {
        OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogArchiveFlag |  kOSKextLogFileAccessFlag,
                "Failed to read kernel file.");
        goto finish;
    }

    /* Set the architecture in the OSKext library */

    if (!OSKextSetArchitecture(archInfo)) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Can't set architecture %s to create prelinked kernel.",
            archInfo->name);
        result = EX_OSERR;
        goto finish;
    }

   /*****
    * Figure out which kexts we're actually archiving.
    * This uses toolArgs->allKexts, which must already be created.
    */
    prelinkKexts = CFArrayCreateMutable(kCFAllocatorDefault, 0, 
        &kCFTypeArrayCallBacks);
    if (!prelinkKexts) {
        OSKextLogMemError();
        result = EX_OSERR;
        goto finish;
    }

    result = filterKextsForCache(toolArgs, prelinkKexts,
            archInfo, &fatalOut);
    if (result != EX_OK || fatalOut) {
        goto finish;
    }

    result = EX_OSERR;

    if (!CFArrayGetCount(prelinkKexts)) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
            "No kexts found for architecture %s.",
            archInfo->name);
        goto finish;
    }

   /* Create the prelinked kernel from the given kernel and kexts */

    flags |= (toolArgs->noLinkFailures) ? kOSKextKernelcacheNeedAllFlag : 0;
    flags |= (toolArgs->skipAuthentication) ? kOSKextKernelcacheSkipAuthenticationFlag : 0;
    flags |= (toolArgs->printTestResults) ? kOSKextKernelcachePrintDiagnosticsFlag : 0;
    flags |= (toolArgs->includeAllPersonalities) ? kOSKextKernelcacheIncludeAllPersonalitiesFlag : 0;
    flags |= (toolArgs->stripSymbols) ? kOSKextKernelcacheStripSymbolsFlag : 0;
    
    prelinkedKernel = OSKextCreatePrelinkedKernel(kernelImage, prelinkKexts,
        toolArgs->volumeRootURL, flags, prelinkedSymbolsOut);
    if (!prelinkedKernel) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
            "Failed to generate prelinked kernel.");
        result = EX_OSERR;
        goto finish;
    }

   /* Compress the prelinked kernel if needed */

    if (toolArgs->compress) {
        *prelinkedKernelOut = compressPrelinkedSlice(prelinkedKernel);
    } else {
        *prelinkedKernelOut = CFRetain(prelinkedKernel);
    }

    if (!*prelinkedKernelOut) {
        goto finish;
    }

    result = EX_OK;

finish:
    SAFE_RELEASE(kernelImage);
    SAFE_RELEASE(prelinkKexts);
    SAFE_RELEASE(prelinkedKernel);

    return result;
}

/*****************************************************************************
 * FIXME: This assumes that we only care about the first repository URL. We
 *        need to make this more general if we add support for multiple system
 *        extensions folders.
 *****************************************************************************/
ExitStatus
getExpectedPrelinkedKernelModTime(
    KextcacheArgs  * toolArgs,
    struct timeval   cacheFileTimes[2],
    Boolean        * updateModTimeOut)
{
    struct timeval  kextTimes[2];
    struct timeval  kernelTimes[2];
    ExitStatus      result          = EX_SOFTWARE;
    CFURLRef        firstURL        = NULL;
    Boolean         updateModTime   = false; 

    if (!toolArgs->haveFolderMtime || !toolArgs->haveKernelMtime) {
        result = EX_OK;
        goto finish;
    }

    firstURL = (CFURLRef)CFArrayGetValueAtIndex(toolArgs->repositoryURLs, 0);

    /* Check kext repository mod time */

    result = getFileURLModTimePlusOne(firstURL, 
        &toolArgs->firstFolderStatBuffer, kextTimes);
    if (result != EX_OK) {
        goto finish;
    }

    /* Check kernel mod time */

    result = getFilePathModTimePlusOne(toolArgs->kernelPath,
        &toolArgs->kernelStatBuffer, kernelTimes);
    if (result != EX_OK) {
        goto finish;
    }

    /* Get the access and mod times of the later modified of the kernel
     * and kext repository.
     */

    if (timercmp(&kextTimes[1], &kernelTimes[1], >)) {
        cacheFileTimes[0] = kextTimes[0];
        cacheFileTimes[1] = kextTimes[1];
    } else {
        cacheFileTimes[0] = kernelTimes[0];
        cacheFileTimes[1] = kernelTimes[1];
    }

    /* Set the mod time of the kernelcache relative to the kernel */

    updateModTime = true;
    result = EX_OK;

finish:
    if (updateModTimeOut) *updateModTimeOut = updateModTime;

    return result;
}

/*********************************************************************
 *********************************************************************/
ExitStatus
compressPrelinkedKernel(
    const char        * prelinkPath,
    Boolean             compress)
{
    ExitStatus          result          = EX_SOFTWARE;
    struct timeval      prelinkedKernelTimes[2];
    CFMutableArrayRef   prelinkedSlices = NULL; // must release
    CFMutableArrayRef   prelinkedArchs  = NULL; // must release
    CFDataRef           prelinkedSlice  = NULL; // must release
    const NXArchInfo  * archInfo        = NULL; // do not free
    const u_char      * sliceBytes      = NULL; // do not free
    mode_t              fileMode        = 0;
    int                 i               = 0;
    
    result = readMachOSlices(prelinkPath, &prelinkedSlices, 
        &prelinkedArchs, &fileMode, prelinkedKernelTimes);
    if (result != EX_OK) {
        goto finish;
    }

    /* Compress/uncompress each slice of the prelinked kernel.
     */

    for (i = 0; i < CFArrayGetCount(prelinkedSlices); ++i) {

        SAFE_RELEASE_NULL(prelinkedSlice);
        prelinkedSlice = CFArrayGetValueAtIndex(prelinkedSlices, i);

        if (compress) {
            prelinkedSlice = compressPrelinkedSlice(prelinkedSlice);
            if (!prelinkedSlice) {
                result = EX_DATAERR;
                goto finish;
            }
        } else {
            prelinkedSlice = uncompressPrelinkedSlice(prelinkedSlice);
            if (!prelinkedSlice) {
                result = EX_DATAERR;
                goto finish;
            }
        }

        CFArraySetValueAtIndex(prelinkedSlices, i, prelinkedSlice);
    }
    SAFE_RELEASE_NULL(prelinkedSlice);

    /* Snow Leopard prelinked kernels are not wrapped in a fat header, so we
     * have to decompress the prelinked kernel and look at the mach header
     * to get the architecture information.
     */

    if (!prelinkedArchs && CFArrayGetCount(prelinkedSlices) == 1) {
        if (!createCFMutableArray(&prelinkedArchs, NULL)) {
            OSKextLogMemError();
            result = EX_OSERR;
            goto finish;
        }

        sliceBytes = CFDataGetBytePtr(
            CFArrayGetValueAtIndex(prelinkedSlices, 0));

        archInfo = getThinHeaderPageArch(sliceBytes);
        if (archInfo) {
            CFArrayAppendValue(prelinkedArchs, archInfo);
        } else {
            SAFE_RELEASE_NULL(prelinkedArchs);
        }
    }

    /* If we still don't have architecture information, then something
     * definitely went wrong.
     */

    if (!prelinkedArchs) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
            "Couldn't determine prelinked kernel's architecture");
        result = EX_SOFTWARE;
        goto finish;
    }

    result = writeFatFile(prelinkPath, prelinkedSlices, 
        prelinkedArchs, fileMode, prelinkedKernelTimes);
    if (result != EX_OK) {
        goto finish;
    }

    result = EX_OK;

finish:
    SAFE_RELEASE(prelinkedSlices);
    SAFE_RELEASE(prelinkedArchs);
    SAFE_RELEASE(prelinkedSlice);

    return result;
}

#pragma mark Boot!=Root


/*******************************************************************************
* usage()
*******************************************************************************/
void usage(UsageLevel usageLevel)
{
    fprintf(stderr,
      "usage: %1$s <mkext_flag> [options] [--] [kext or directory] ...\n"
      "       %1$s -prelinked-kernel <filename> [options] [--] [kext or directory]\n"
      "       %1$s -system-prelinked-kernel\n"
      "       %1$s [options] -prelinked-kernel\n"
#if !NO_BOOT_ROOT
      "       %1$s -update-volume <volume> [options]\n"
#endif /* !NO_BOOT_ROOT */
      "       %1$s -system-caches [options]\n"
      "\n",
      progname);

    if (usageLevel == kUsageLevelBrief) {
        fprintf(stderr, "use %s -%s for an explanation of each option\n",
            progname, kOptNameHelp);
    }

    if (usageLevel == kUsageLevelBrief) {
        return;
    }

    fprintf(stderr, "-%s <filename>: create an mkext (latest supported version)\n",
        kOptNameMkext);
    fprintf(stderr, "-%s <filename>: create an mkext (version 2)\n",
        kOptNameMkext2);
    fprintf(stderr, "-%s <filename> (-%c): create an mkext (version 1)\n",
        kOptNameMkext1, kOptMkext);
    fprintf(stderr, "-%s [<filename>] (-%c):\n"
        "        create/update prelinked kernel (must be last if no filename given)\n",
        kOptNamePrelinkedKernel, kOptPrelinkedKernel);
    fprintf(stderr, "-%s:\n"
        "        create/update system prelinked kernel\n",
        kOptNameSystemPrelinkedKernel);
#if !NO_BOOT_ROOT
    fprintf(stderr, "-%s <volume> (-%c): update system kext caches for <volume>\n",
        kOptNameUpdate, kOptUpdate);
    fprintf(stderr, "-%s called us, modify behavior appropriately\n",
            kOptNameInstaller);
    fprintf(stderr, "-%s skips updating any helper partitions even if they appear out of date\n",
            kOptNameCachesOnly);
#endif /* !NO_BOOT_ROOT */
#if 0
// don't print this system-use option
    fprintf(stderr, "-%c <volume>:\n"
        "        check system kext caches for <volume> (nonzero exit if out of date)\n",
        kOptCheckUpdate);
#endif
    fprintf(stderr, "-%s: update system kext info caches for the root volume\n",
        kOptNameSystemCaches);
    fprintf(stderr, "\n");

    fprintf(stderr,
        "kext or directory: Consider kext or all kexts in directory for inclusion\n");
    fprintf(stderr, "-%s <bundle_id> (-%c):\n"
        "        include the kext whose CFBundleIdentifier is <bundle_id>\n",
        kOptNameBundleIdentifier, kOptBundleIdentifier);
    fprintf(stderr, "-%s <volume>:\n"
        "        Save kext paths in an mkext archive or prelinked kernel "
        " relative to <volume>\n",
        kOptNameVolumeRoot);
    fprintf(stderr, "-%s <kernel_filename> (-%c): Use kernel_filename for a prelinked kernel\n",
        kOptNameKernel, kOptKernel);
    fprintf(stderr, "-%s (-%c): Include all kexts ever loaded in prelinked kernel\n",
        kOptNameAllLoaded, kOptAllLoaded);
#if !NO_BOOT_ROOT
    fprintf(stderr, "-%s (-%c): Update volumes even if they look up to date\n",
        kOptNameForce, kOptForce);
    fprintf(stderr, "\n");
#endif /* !NO_BOOT_ROOT */

    fprintf(stderr, "-%s (-%c): Add 'Local-Root' kexts from directories to an mkext file\n",
        kOptNameLocalRoot, kOptLocalRoot);
    fprintf(stderr, "-%s (-%c): Add 'Local-Root' kexts to an mkext file\n",
        kOptNameLocalRootAll, kOptLocalRootAll);
    fprintf(stderr, "-%s (-%c): Add 'Network-Root' kexts from directories to an mkext file\n",
        kOptNameNetworkRoot, kOptNetworkRoot);
    fprintf(stderr, "-%s (-%c): Add 'Network-Root' kexts to an mkext file\n",
        kOptNameNetworkRootAll, kOptNetworkRootAll);
    fprintf(stderr, "-%s (-%c): Add 'Safe Boot' kexts from directories to an mkext file\n",
        kOptNameSafeBoot, kOptSafeBoot);
    fprintf(stderr, "-%s (-%c): Add 'Safe Boot' kexts to an mkext file\n",
        kOptNameSafeBootAll, kOptSafeBootAll);
    fprintf(stderr, "\n");

    fprintf(stderr, "-%s <archname>:\n"
        "        include architecture <archname> in created cache(s)\n",
        kOptNameArch);
    fprintf(stderr, "-%c: run at low priority\n",
        kOptLowPriorityFork);
    fprintf(stderr, "\n");

    fprintf(stderr, "-%s (-%c): quiet mode: print no informational or error messages\n",
        kOptNameQuiet, kOptQuiet);
    fprintf(stderr, "-%s [ 0-6 | 0x<flags> ] (-%c):\n"
        "        verbose mode; print info about analysis & loading\n",
        kOptNameVerbose, kOptVerbose);
    fprintf(stderr, "\n");

    fprintf(stderr, "-%s (-%c):\n"
        "        print diagnostics for kexts with problems\n",
        kOptNameTests, kOptTests);
    fprintf(stderr, "-%s (-%c): don't authenticate kexts (for use during development)\n",
        kOptNameNoAuthentication, kOptNoAuthentication);
    fprintf(stderr, "\n");
    
    fprintf(stderr, "-%s (-%c): print this message and exit\n",
        kOptNameHelp, kOptHelp);

    return;
}
