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
#include <mach/mach_port.h>     // mach_port_allocate()
#include <mach/mach_types.h>
#include <mach/machine/vm_param.h>
#include <mach/kmod.h>
#include <notify.h>
#include <servers/bootstrap.h>  // bootstrap mach ports
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
#include <IOKit/kext/kextmanager_types.h>
#include <IOKit/kext/kextmanager_mig.h>
#include <bootfiles.h>

#include <IOKit/pwr_mgt/IOPMLib.h>

#include "kextcache_main.h"
#include "bootcaches.h"
#include "update_boot.h"
#include "mkext1_file.h"
#include "compression.h"

// constants
#define MKEXT_PERMS             (0644)

/* The timeout we use when waiting for the system to get to a low load state.
 * We don't pick a round number in hopes of avoiding a collision of work with
 * other people waiting on this notification.
 */
#define SYSTEM_LOAD_TIMEOUT     ((8 * 60) + 27)

/*******************************************************************************
* Program Globals
*******************************************************************************/
const char * progname = "(unknown)";


/*******************************************************************************
* File-Globals
*******************************************************************************/
static mach_port_t sLockPort = MACH_PORT_NULL;
static uuid_t      s_vol_uuid;
static mach_port_t sKextdPort = MACH_PORT_NULL;

/*******************************************************************************
* Utility and callback functions.
*******************************************************************************/
// put/take helpers
static void waitForIOKitQuiescence(void);
static ExitStatus waitForLowSystemLoad(void);
static int takeVolumeForPath(const char *path);

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
    ExitStatus        result              = EX_SOFTWARE;
    Boolean           fatal               = false;
    KextcacheArgs     toolArgs;
    CFDataRef         prelinkedKernel     = NULL;  // must release
    CFDataRef         prelinkedKernelIn   = NULL;  // must release
    CFDictionaryRef   prelinkedSymbols    = NULL;  // must release

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
        if (toolArgs.prelinkedKernelURL) {
            result = waitForLowSystemLoad();
            if (result != EX_OK) goto finish;
        }
    }

   /* The whole point of this program is to update caches, so let's not
    * try to read any (we'll briefly turn this back on when checking them).
    */
    OSKextSetUsesCaches(false);

   /* If we're doing a boot!=root update, call updateBoots() and jump right
    * to the exit, we can't combine this with any other cache-building.
    */
    if (toolArgs.updateVolumeURL) {
        // xxx - updateBoots should return only sysexit-type values, not errno
        result = updateBoots(toolArgs.updateVolumeURL,
            toolArgs.forceUpdateFlag, toolArgs.expectUpToDate);
        goto finish;
    }

   /* If we're uncompressing the prelinked kernel, take care of that here
    * and exit.  We don't allow prelinked kernels to be compressed after they
    * are generated because the header for the compressed prelinked kernel
    * requires platform information that is not stored in the uncompressed
    * kernel.
    */
    if (toolArgs.prelinkedKernelURL && !CFArrayGetCount(toolArgs.argURLs) &&
        (toolArgs.compress || toolArgs.uncompress)) 
    {
        prelinkedKernelIn = readMachOFileWithURL(toolArgs.prelinkedKernelURL, NULL);
        if (!prelinkedKernelIn) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "Can't read prelinked kernel.");
            goto finish;
        }

        if (toolArgs.uncompress) {
            prelinkedKernel = uncompressPrelinkedKernel(prelinkedKernelIn);
            if (!prelinkedKernel) {
                goto finish;
            }
        } else if (toolArgs.compress) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "Prelinked kernels can't be compressed after they are created.");
            goto finish;
        }

        result = savePrelinkedKernel(&toolArgs, prelinkedKernel, NULL);
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

    // xxx - we are potentially overwriting error results here
    if (toolArgs.updateSystemCaches) {
        result = updateSystemDirectoryCaches(&toolArgs);
        // don't goto finish on error here, we might be able to create
        // the other caches
    }

    if (toolArgs.mkextURL) {
        result = createMkext(&toolArgs, &fatal);
        if (fatal) {
            goto finish;
        }
    }

    if (toolArgs.prelinkedKernelURL) {
        CFDictionaryRef * prelinkedSymbolsPtr = NULL;  // do not release
        PlatformInfo      platformInfo;

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

       /* This call updates prelinkedKernelURL if necessary from the
        * containing directory to the full path of the final file.
        */
        getPlatformInfo(&toolArgs, &platformInfo);

        if (toolArgs.generatePrelinkedSymbols) {
            prelinkedSymbolsPtr = &prelinkedSymbols;
        }

        result = createPrelinkedKernel(&toolArgs, &prelinkedKernel,
            prelinkedSymbolsPtr, toolArgs.archInfo[0],
            toolArgs.needLoadedKextInfo ? &platformInfo : NULL);
        if (result != EX_OK) {
            goto finish;
        }

        result = savePrelinkedKernel(&toolArgs, prelinkedKernel, 
            prelinkedSymbols);
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
    SAFE_RELEASE(toolArgs.mkextURL);
    SAFE_RELEASE(toolArgs.prelinkedKernelURL);
    SAFE_RELEASE(toolArgs.kernelURL);
    SAFE_RELEASE(toolArgs.kernelFile);
    SAFE_RELEASE(toolArgs.symbolDirURL);

    SAFE_RELEASE(prelinkedKernel);
    SAFE_RELEASE(prelinkedKernelIn);
    SAFE_RELEASE(prelinkedSymbols);

    return result;
}

/*******************************************************************************
*******************************************************************************/
ExitStatus readArgs(
    int            * argc,
    char * const  ** argv,
    KextcacheArgs  * toolArgs)
{
    ExitStatus result = EX_USAGE;
    int          optchar;
    int          longindex       = -1;
    CFStringRef  scratchString   = NULL;  // must release
    CFNumberRef  scratchNumber   = NULL;  // must release
    CFURLRef     scratchURL      = NULL;  // must release
    uint32_t     i;

    bzero(toolArgs, sizeof(*toolArgs));
    
   /*****
    * Allocate collection objects.
    */
    if (!createCFMutableSet(&toolArgs->kextIDs, &kCFTypeSetCallBacks)             ||
        !createCFMutableArray(&toolArgs->argURLs, &kCFTypeArrayCallBacks)         ||
        !createCFMutableArray(&toolArgs->repositoryURLs, &kCFTypeArrayCallBacks)  ||
        !createCFMutableArray(&toolArgs->namedKextURLs, &kCFTypeArrayCallBacks)) {

        result = EX_OSERR;
        OSKextLogMemError();
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

        switch (optchar) {
  
            case kOptArch:
                if (toolArgs->numArches >= kMaxNumArches) {
                    OSKextLog(/* kext */ NULL,
                        kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                        "Maximum of %d architectures supported", kMaxNumArches);
                    goto finish;
                }
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
                result = readPrelinkedKernelArgs(toolArgs, *argc, *argv,
                    /* isLongopt */ longindex != -1);
                if (result != EX_OK) {
                    goto finish;
                }
                break;
  
            case kOptSystemMkext:
               /* Safe to ignore double spec of this flag. */
                if (toolArgs->updateSystemMkext) {
                    continue;
                }
                if (toolArgs->mkextURL && !toolArgs->updateSystemMkext) {
                    OSKextLog(/* kext */ NULL,
                        kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                        "-%s (-%c) can't be used when saving files by name",
                        kOptNameSystemMkext, kOptSystemMkext);
                    goto finish;
                }
                toolArgs->updateSystemMkext = true;
                scratchURL = CFURLCreateFromFileSystemRepresentation(
                    kCFAllocatorDefault, (const UInt8 *)_kOSKextStartupMkextPath,
                    strlen(_kOSKextStartupMkextPath), /* isDir */ true);
                if (!scratchURL) {
                    OSKextLogStringError(/* kext */ NULL);
                    result = EX_OSERR;
                    goto finish;
                }
                toolArgs->mkextURL = CFRetain(scratchURL);
                toolArgs->mkextVersion = 2;
                {
                    CFArrayRef sysExtensionsFolders =
                        OSKextGetSystemExtensionsFolderURLs();

                    CFArrayAppendArray(toolArgs->argURLs,
                        sysExtensionsFolders, RANGE_ALL(sysExtensionsFolders));
                    CFArrayAppendArray(toolArgs->repositoryURLs,
                        sysExtensionsFolders, RANGE_ALL(sysExtensionsFolders));
                }
                
                break;
    
            case kOptForce:
                toolArgs->forceUpdateFlag = true;
                break;
  
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
                if (toolArgs->kernelURL) {
                    OSKextLog(/* kext */ NULL,
                        kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
                        "Warning: kernel file already specified; using last.");
                    SAFE_RELEASE_NULL(toolArgs->kernelURL);
                }
                scratchURL = CFURLCreateFromFileSystemRepresentation(
                    kCFAllocatorDefault,
                    (const UInt8 *)optarg, strlen(optarg), true);
                if (!scratchURL) {
                    OSKextLogStringError(/* kext */ NULL);
                    result = EX_OSERR;
                    goto finish;
                }
                toolArgs->kernelURL = CFRetain(scratchURL);
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
                setNeededLoadedKextInfo(toolArgs);
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
  
            case kOptUpdate:
            case kOptCheckUpdate:
                if (toolArgs->updateVolumeURL) {
                    OSKextLog(/* kext */ NULL,
                        kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
                        "Warning: update volume already specified; using last.");
                    SAFE_RELEASE_NULL(toolArgs->updateVolumeURL);
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
          
            case kOptQuiet:
                beQuiet();
                break;

            case kOptVerbose:
                result = setLogFilterForOpt(*argc, *argv,
                    /* forceOnFlags */ kOSKextLogKextOrGlobalMask);
                break;

            case kOptNoAuthentication:
                toolArgs->skipAuthentication = true;
                break;

            case 0:
                switch (longopt) {
                    case kLongOptMkext1:
                    case kLongOptMkext2:
                    // note kLongOptMkext == latest supported version 
                        if (toolArgs->updateSystemMkext) {
                            OSKextLog(/* kext */ NULL,
                                kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                                "-%s (-%c) can't be used when saving files by name.",
                                kOptNameSystemMkext, kOptSystemMkext);
                            goto finish;
                        }
                        if (toolArgs->mkextURL) {
                            OSKextLog(/* kext */ NULL,
                                kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
                                "Warning: output mkext file already specified; using last.");
                            SAFE_RELEASE_NULL(toolArgs->mkextURL);
                        }
                        scratchURL = CFURLCreateFromFileSystemRepresentation(
                            kCFAllocatorDefault,
                            (const UInt8 *)optarg, strlen(optarg), true);
                        if (!scratchURL) {
                            OSKextLogStringError(/* kext */ NULL);
                            result = EX_OSERR;
                            goto finish;
                        }
                        toolArgs->mkextURL = CFRetain(scratchURL);
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
                        {
                            CFArrayRef sysExtensionsFolders =
                                 OSKextGetSystemExtensionsFolderURLs();

                            CFArrayAppendArray(toolArgs->argURLs,
                                sysExtensionsFolders, RANGE_ALL(sysExtensionsFolders));
                            CFArrayAppendArray(toolArgs->repositoryURLs,
                                sysExtensionsFolders, RANGE_ALL(sysExtensionsFolders));
                        }
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
                        result = setPrelinkedKernelArgs(toolArgs,
                            /* filename */ NULL);
                        if (result != EX_OK) {
                            goto finish;
                        }
                        setNeededLoadedKextInfo(toolArgs);
                        break;

                    case kLongOptAllPersonalities:
                        toolArgs->includeAllPersonalities = true;
                        break;

                    case kLongOptOmitLinkState:
                        toolArgs->omitLinkState = true;
                        break;

                    default:
                       /* getopt_long_only() prints an error message for us. */
                        goto finish;
                        break;
                }
                break;

            default:
               /* getopt_long_only() prints an error message for us. */
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
                result = EX_OSERR;
                OSKextLogMemError();
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
    ExitStatus   result      = EX_USAGE;
    CFURLRef     scratchURL  = NULL;  // must release

    if (toolArgs->prelinkedKernelURL) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
            "Warning: prelinked kernel already specified; using last.");
            SAFE_RELEASE_NULL(toolArgs->prelinkedKernelURL);
        toolArgs->prelinkedKernelErrorRequired = false;
    }

    if (!filename) {
        toolArgs->needDefaultPrelinkedKernelInfo = true;
        
        const char * prelinkedKernelDir =
            _kOSKextCachesRootFolder "/" _kOSKextStartupCachesSubfolder;

        scratchURL = CFURLCreateWithBytes(kCFAllocatorDefault,
            (UInt8 *)prelinkedKernelDir, strlen(prelinkedKernelDir),
            kCFStringEncodingMacRoman, NULL);
            
    } else {
        scratchURL = CFURLCreateFromFileSystemRepresentation(
            kCFAllocatorDefault,  (const UInt8 *)filename,
            strlen(filename), true);
        toolArgs->prelinkedKernelErrorRequired = true;
    }

    if (!scratchURL) {
        OSKextLogStringError(/* kext */ NULL);
        result = EX_OSERR;
        goto finish;
    }

    toolArgs->prelinkedKernelURL = CFRetain(scratchURL);
    
    result = EX_OK;

finish:

    SAFE_RELEASE(scratchURL);
    return result;
}

/*******************************************************************************
*******************************************************************************/
void setNeededLoadedKextInfo(KextcacheArgs * toolArgs)
{
    CFArrayRef sysExtensionsFolders = OSKextGetSystemExtensionsFolderURLs();

    CFArrayAppendArray(toolArgs->argURLs,
        sysExtensionsFolders, RANGE_ALL(sysExtensionsFolders));
    CFArrayAppendArray(toolArgs->repositoryURLs,
        sysExtensionsFolders, RANGE_ALL(sysExtensionsFolders));

    toolArgs->needLoadedKextInfo = true;

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
* Wait for the system to report that it's a good time to do work.  We define
* a good time to be when the IOSystemLoadAdvisory API returns a combined level
* of kIOSystemLoadAdvisoryLevelGreat, and we'll wait up to SYSTEM_LOAD_TIMEOUT
* seconds for the system to enter that state before we begin our work.
*******************************************************************************/
static ExitStatus 
waitForLowSystemLoad(void)
{
    ExitStatus result = EX_SOFTWARE;
    int notifyFileDescriptor = 0, notifyToken = 0, currentToken = 0;
    uint32_t notifyStatus = 0, usecs = 0;
    uint64_t notifyState = 0;
    struct timeval currenttime;
    struct timeval endtime;
    struct timeval timeout;
    fd_set readfds;
    fd_set tmpfds;

    bzero(&currenttime, sizeof(currenttime));
    bzero(&endtime, sizeof(endtime));
    bzero(&timeout, sizeof(timeout));

    OSKextLog(/* kext */ NULL,
        kOSKextLogProgressLevel | kOSKextLogGeneralFlag,
        "Waiting for low system load.");

    /* Register for SystemLoadAdvisory notifications */

    notifyStatus = notify_register_file_descriptor(
        kIOSystemLoadAdvisoryNotifyName, &notifyFileDescriptor, 
        /* flags */ 0, &notifyToken);
    if (notifyStatus != NOTIFY_STATUS_OK) {
        result = EX_OSERR;
        goto finish;
    }

    /* If it's not a good time, set up the select(2) timers */

    notifyStatus = notify_get_state(notifyToken, &notifyState);
    if (notifyStatus != NOTIFY_STATUS_OK) {
        result = EX_OSERR;
        goto finish;
    }

    if (notifyState != kIOSystemLoadAdvisoryLevelGreat) {
        notifyStatus = gettimeofday(&currenttime, NULL);
        if (notifyStatus < 0) {
            result = EX_OSERR;
            goto finish;
        }

        endtime = currenttime;
        endtime.tv_sec += SYSTEM_LOAD_TIMEOUT;
    }

    timeval_difference(&timeout, &endtime, &currenttime);
    usecs = usecs_from_timeval(&timeout);

    FD_ZERO(&readfds);
    FD_SET(notifyFileDescriptor, &readfds);

    /* Check SystemLoadAdvisory notifications until it's a great time to
     * do work or we hit the timeout.
     */  

    while (usecs) {
        /* Wait for notifications or the timeout */

        FD_COPY(&readfds, &tmpfds);
        notifyStatus = select(notifyFileDescriptor + 1, 
            &tmpfds, NULL, NULL, &timeout);
        if (notifyStatus < 0) {
            result = EX_OSERR;
            goto finish;
        }

        /* Set up the next timeout */

        notifyStatus = gettimeofday(&currenttime, NULL);
        if (notifyStatus < 0) {
            result = EX_OSERR;
            goto finish;
        }

        timeval_difference(&timeout, &endtime, &currenttime);
        usecs = usecs_from_timeval(&timeout);

        /* Check the system load state */

        if (!FD_ISSET(notifyFileDescriptor, &tmpfds)) continue;

        notifyStatus = read(notifyFileDescriptor, 
            &currentToken, sizeof(currentToken));
        if (notifyStatus < 0) {
            result = EX_OSERR;
            goto finish;
        }

        if (currentToken == notifyToken) {
            notifyStatus = notify_get_state(notifyToken, &notifyState);
            if (notifyStatus != NOTIFY_STATUS_OK) {
                result = EX_OSERR;
                goto finish;
            }

            /* If it's a great time, get started with work */
            if (notifyState == kIOSystemLoadAdvisoryLevelGreat) break;
        }
    }

    result = EX_OK;
finish:
    return result;
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

/*******************************************************************************
*******************************************************************************/
const NXArchInfo * addArchForName(
    KextcacheArgs * toolArgs,
    const char    * archname)
{
    const NXArchInfo * result = NULL;
    int i;
    
    result = NXGetArchInfoFromName(archname);
    if (!result) {
        goto finish;
    }
    for (i = 0; i < toolArgs->numArches; i++) {
        if (toolArgs->archInfo[i] == result) {
            goto finish;
        }
    }
    toolArgs->archInfo[toolArgs->numArches] = result;
    ++(toolArgs->numArches);

finish:
    return result;
}

/*******************************************************************************
 *******************************************************************************/
void addArch(
    KextcacheArgs     * toolArgs,
    const NXArchInfo  * arch)
{
    int i;
    
    for (i = 0; i < toolArgs->numArches; i++) {
        if (toolArgs->archInfo[i] == arch) {
            goto finish;
        }
    }
    toolArgs->archInfo[toolArgs->numArches] = arch;
    ++(toolArgs->numArches);
    
finish:
    return;
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
    ExitStatus result = EX_USAGE;

    if (!toolArgs->mkextURL && !toolArgs->prelinkedKernelURL &&
        !toolArgs->updateVolumeURL && !toolArgs->updateSystemCaches) {

        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "No work to do; check options and try again.");
        goto finish;
    }
    
    if (toolArgs->volumeRootURL && !toolArgs->mkextURL &&
        !toolArgs->prelinkedKernelURL) {

        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Use -%s only when creating an mkext archive or prelinked kernel.",
            kOptNameVolumeRoot);
        goto finish;
    }

    if (!toolArgs->updateVolumeURL && !CFArrayGetCount(toolArgs->argURLs) &&
        !toolArgs->compress && !toolArgs->uncompress) {
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
    
    if (toolArgs->forceUpdateFlag) {
        if (toolArgs->expectUpToDate || !toolArgs->updateVolumeURL) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                "-%s (-%c) is allowed only with -%s (-%c).",
                kOptNameForce, kOptForce, kOptNameUpdate, kOptUpdate);
            goto finish;
        }
    }

    if (toolArgs->updateVolumeURL) {
        if (toolArgs->mkextURL || toolArgs->prelinkedKernelURL) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                "Can't create mkext or prelinked kernel when updating volumes.");
        }
    }

   /* If no arches were explicitly specified, toss in the currently-supported
    * ones.
    * xxx - should find a reliable way to do this dynamically for any volume
    * with or without boot-root.
    */
    if (!toolArgs->explicitArch) {
        if (toolArgs->prelinkedKernelURL) {
            // default to host arch since we don't build universal prelinked kernels
            const NXArchInfo *hostArch;
            
            hostArch = OSKextGetRunningKernelArchitecture();
            if (!hostArch) {
                goto finish;
            }
            
            addArch(toolArgs, hostArch);
        } else {
            addArchForName(toolArgs, "i386");
            addArchForName(toolArgs, "x86_64");
        }
    } else {
        /* If they were specified, validate them */
        if (toolArgs->prelinkedKernelURL) {

            if (toolArgs->numArches != 1) {
                OSKextLog(/* kext */ NULL,
                    kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                    "Prelinked kernel requires a single architecture.");
                goto finish;
            }

            if (toolArgs->needLoadedKextInfo) {
                const NXArchInfo *hostArch;

                hostArch = OSKextGetRunningKernelArchitecture();
                if (!hostArch) {
                    goto finish;
                }
             
                if (toolArgs->archInfo[0]->cputype != hostArch->cputype) {
                    OSKextLog(/* kext */ NULL,
                        kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                        "Can't request loaded kexts for non-host architecture.");
                    goto finish;                        
                }
            }
        }
    }

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

    if (toolArgs->needDefaultPrelinkedKernelInfo && !toolArgs->kernelURL) {
        toolArgs->kernelURL = CFURLCreateFromFileSystemRepresentation(
            kCFAllocatorDefault, (UInt8 *) kDefaultKernelFile, 
            sizeof(kDefaultKernelFile) - 1, /* isDirectory */ false);
        if (!toolArgs->kernelURL) {
            OSKextLogMemError();
            result = EX_OSERR;
            goto finish;
        }
    }

    if (toolArgs->prelinkedKernelURL && !toolArgs->uncompress &&
        !toolArgs->kernelURL) {

        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "No kernel specified for prelinked kernel generation.");
        goto finish;
    }
    
    if (toolArgs->kernelURL) {
        result = statURL(toolArgs->kernelURL, &toolArgs->kernelStatBuffer);
        if (result != EX_OK) {
            goto finish;
        }
    }

    if (toolArgs->needDefaultPrelinkedKernelInfo ||
        toolArgs->updateSystemMkext ||
        toolArgs->updateSystemCaches) {
        
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
ExitStatus statURL(CFURLRef anURL, struct stat * statBuffer)
{
    ExitStatus result = EX_OSERR;
    char dirPath[PATH_MAX];

    if (!CFURLGetFileSystemRepresentation(anURL, /* resolveToBase */ true,
        (UInt8 *)dirPath, sizeof(dirPath))) {

        OSKextLogStringError(/* kext */ NULL);
        goto finish;
    }
    if (stat(dirPath, statBuffer)) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Can't stat %s - %s.", dirPath, strerror(errno));
        goto finish;
    }

    result = EX_OK;

finish:
    return result;
}

/*******************************************************************************
*******************************************************************************/
ExitStatus updateSystemDirectoryCaches(
    KextcacheArgs * toolArgs)
{
    ExitStatus result               = EX_OSERR;
    ExitStatus directoryResult      = EX_OK;  // flipped to error as needed
    CFArrayRef sysExtensionsFolders = NULL;  // do not release
    CFURLRef   folderURL            = NULL;  // do not release
    char       folderPath[PATH_MAX] = "";
    CFIndex    count, i;

    sysExtensionsFolders = OSKextGetSystemExtensionsFolderURLs();
    if (!sysExtensionsFolders) {
        OSKextLogMemError();
        goto finish;
    }

    count = CFArrayGetCount(sysExtensionsFolders);
    for (i = 0; i < count; i++) {

        folderURL = CFArrayGetValueAtIndex(sysExtensionsFolders, i);

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
    return result;
}

/*******************************************************************************
*******************************************************************************/
ExitStatus updateDirectoryCaches(
    KextcacheArgs * toolArgs,
    CFURLRef        folderURL)
{
    ExitStatus  result                   = EX_OK;  // optimistic!
    CFArrayRef  kexts                    = NULL;   // must release
    CFArrayRef  directoryValues          = NULL;   // must release
    CFIndex     i;

    kexts = OSKextCreateKextsFromURL(kCFAllocatorDefault, folderURL);
    if (!kexts) {
        result = EX_OSERR;
        goto finish;
    }

    if (!_OSKextWriteIdentifierCacheForKextsInDirectory(
        kexts, folderURL, /* force? */ true)) {
    
        result = EX_OSERR;
    }

    for (i = 0; i < toolArgs->numArches; i++) {
        if (kOSReturnSuccess != writePersonalitiesCache(kexts,
            toolArgs->archInfo[i], folderURL)) {

            result = EX_OSERR;
        }

        if (!readKextPropertyValuesForDirectory(folderURL,
            CFSTR(kOSBundleHelperKey), toolArgs->archInfo[i],
            /* forceUpdate? */ true, /* values */ NULL)) {
            
            result = EX_OSERR;
        }
    }

    result = EX_OK;
finish:
    SAFE_RELEASE(kexts);
    SAFE_RELEASE(directoryValues);
    return result;
}

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
    ExitStatus        result         = EX_SOFTWARE;
    CFMutableArrayRef archiveKexts   = NULL;  // must release
    CFMutableArrayRef mkexts         = NULL;  // must release
    CFDataRef         mkext          = NULL;  // must release
    struct fat_header fatHeader;
    uint32_t          fatOffset      = 0;
    CFURLRef          systemMkextFolderURL = NULL;  // must release
    char              mkextPath[PATH_MAX];
    char              tmpPath[PATH_MAX];
    int               fileDescriptor = -1;    // must close
    mode_t            real_umask;
    struct timeval    cacheFileTimes[2];
    Boolean           updateModTime  = true;
    int               i;

   /* Try a lock on the volume for the mkext being updated.
    * xxx - why is this done even for non-system use?
    */
    if (!getenv("_com_apple_kextd_skiplocks")) {
        // xxx - updateBoots * related should return only sysexit-type values, not errno
        result = takeVolumeForPath(mkextPath);
        if (result != EX_OK) {
            goto finish;
        }
    }

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

    if (toolArgs->updateSystemMkext) {
        systemMkextFolderURL = CFURLCreateFromFileSystemRepresentation(
            kCFAllocatorDefault, (UInt8 *)_kOSKextStartupMkextFolderPath,
            strlen(_kOSKextStartupMkextFolderPath), /* isDir */ true);
        if (!systemMkextFolderURL) {
            OSKextLogMemError();
            result = EX_OSERR;
            goto finish;
        }

        if (!_OSKextCreateFolderForCacheURL(toolArgs->mkextURL)) {
            result = EX_OSERR;
            goto finish;
        }
    }

    for (i = 0; i < toolArgs->numArches; i++) {

        SAFE_RELEASE_NULL(mkext);

        if (!OSKextSetArchitecture(toolArgs->archInfo[i])) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                "Can't set architecture %s to create mkext.",
                toolArgs->archInfo[i]->name);
            result = EX_OSERR;
            goto finish;
        }

       /*****
        * Figure out which kexts we're actually archiving.
        * This uses toolArgs->allKexts, which must already be created.
        */
        result = filterKextsForMkext(toolArgs, archiveKexts,
            toolArgs->archInfo[i], fatalOut);
        if (result != EX_OK || *fatalOut) {
            goto finish;
        }

        if (!CFArrayGetCount(archiveKexts)) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogWarningLevel | kOSKextLogArchiveFlag,
                "No kexts found for architecture %s; skipping architecture.",
                toolArgs->archInfo[i]->name);
            continue;
        }

        if (toolArgs->mkextVersion == 2) {
            mkext = OSKextCreateMkext(kCFAllocatorDefault, archiveKexts,
                toolArgs->volumeRootURL,
                kOSKextOSBundleRequiredNone, toolArgs->compress);
        } else if (toolArgs->mkextVersion == 1) {
            mkext = createMkext1ForArch(toolArgs->archInfo[i], archiveKexts,
                toolArgs->compress);
        }
        if (!mkext) {
            // OSKextCreateMkext() logs an error
            result = EX_OSERR;
            goto finish;
        }
        if (toolArgs->archInfo[i] == NXGetArchInfoFromName("ppc")) {
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

    if (!CFURLGetFileSystemRepresentation(toolArgs->mkextURL,
        /* resolveToBase */ true, (UInt8 *)mkextPath, sizeof(mkextPath))) {

        OSKextLogStringError(/* kext */ NULL);
        goto finish;
    }
    
   /* If it looks like we're updating the system mkext, try to create
    * the folder to contain it. Don't log or bail here, we'll do that
    * when mkstemp() fails below.
    */
    if (!strncmp(mkextPath, _kOSKextStartupMkextFolderPath,
        sizeof(_kOSKextStartupMkextFolderPath) - 1)) {
        
        _OSKextCreateFolderForCacheURL(toolArgs->mkextURL);
    }

    strlcpy(tmpPath, mkextPath, sizeof(tmpPath));
    if (strlcat(tmpPath, ".XXXX", sizeof(tmpPath)) >= sizeof(tmpPath)) {
        OSKextLogStringError(/* kext */ NULL);
        goto finish;
    }

    fileDescriptor = mkstemp(tmpPath);
    if (-1 == fileDescriptor) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Can't create %s - %s.",
            tmpPath, strerror(errno));
        goto finish;
    }

   /* Set the umask to get it, then set it back to iself. Wish there were a
    * better way to query it.
    */
    real_umask = umask(0);
    umask(real_umask);

    if (-1 == fchmod(fileDescriptor, MKEXT_PERMS & ~real_umask)) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Can't set permissions on %s - %s.",
            tmpPath, strerror(errno));
    }

    OSKextLog(/* kext */ NULL,
        kOSKextLogDebugLevel | kOSKextLogFileAccessFlag,
        "Saving temp mkext file %s.",
        tmpPath);

    fatHeader.magic = OSSwapHostToBigInt32(FAT_MAGIC);
    fatHeader.nfat_arch = OSSwapHostToBigInt32(toolArgs->numArches);

    result = writeToFile(fileDescriptor, (const UInt8 *)&fatHeader,
        sizeof(fatHeader));
    if (result != EX_OK) {
        goto finish;
    }

   /* Set the fatOffset off to the byte just past the fat header and all
    * the fat_arch structs. It gets incremented by the size of each mkext
    * added to the fat mkext.
    */
    fatOffset = sizeof(struct fat_header) +
        (sizeof(struct fat_arch) * toolArgs->numArches);

    for (i = 0; i < toolArgs->numArches; i++) {
        CFDataRef       mkext       = (CFDataRef)CFArrayGetValueAtIndex(mkexts, i);
        CFIndex         mkextLength = CFDataGetLength(mkext);
        struct fat_arch fatArch;

        fatArch.cputype = OSSwapHostToBigInt32(toolArgs->archInfo[i]->cputype);
        fatArch.cpusubtype =
            OSSwapHostToBigInt32(toolArgs->archInfo[i]->cpusubtype);
        fatArch.offset = OSSwapHostToBigInt32(fatOffset);
        fatArch.size = OSSwapHostToBigInt32(mkextLength);
        fatArch.align = OSSwapHostToBigInt32(0);

        result = writeToFile(fileDescriptor, (UInt8 *)&fatArch, sizeof(fatArch));
        if (result != EX_OK) {
            goto finish;
        }
        
        fatOffset += mkextLength;
    }

    for (i = 0; i < toolArgs->numArches; i++) {
        CFDataRef     mkext       = (CFDataRef)CFArrayGetValueAtIndex(mkexts, i);
        const UInt8 * mkextPtr    = CFDataGetBytePtr(mkext);
        CFIndex       mkextLength = CFDataGetLength(mkext);
        
        result = writeToFile(fileDescriptor, mkextPtr, mkextLength);
        if (result != EX_OK) {
            goto finish;
        }
    }

    if (toolArgs->haveFolderMtime && CFArrayGetCount(toolArgs->repositoryURLs)) {
        CFURLRef firstURL = (CFURLRef)CFArrayGetValueAtIndex(
            toolArgs->repositoryURLs, 0);
        struct stat     newStatBuffer;
        struct timespec newModTime;
        struct timespec origModTime =
            toolArgs->firstFolderStatBuffer.st_mtimespec;
        
        result = statURL(firstURL, &newStatBuffer);
        if (result != EX_OK) {
            goto finish;
        }
        newModTime = newStatBuffer.st_mtimespec;
        if ((newModTime.tv_sec != origModTime.tv_sec) ||
            (newModTime.tv_nsec != origModTime.tv_nsec)) {

            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogGeneralFlag | kOSKextLogFileAccessFlag,
                "Source directory has changed since starting; "
                "not saving cache file %s.",
                mkextPath);
            result = kKextcacheExitStale;
            goto finish;
        }

        TIMESPEC_TO_TIMEVAL(&cacheFileTimes[0], &newStatBuffer.st_atimespec);
        TIMESPEC_TO_TIMEVAL(&cacheFileTimes[1], &newStatBuffer.st_mtimespec);
        cacheFileTimes[1].tv_sec++;
        updateModTime = true;
    }

    OSKextLog(/* kext */ NULL,
        kOSKextLogDebugLevel | kOSKextLogFileAccessFlag,
        "Renaming temp mkext file to %s.",
        mkextPath);

    if (rename(tmpPath, mkextPath) != 0) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Can't rename temp mkext file - %s.",
            strerror(errno));
        result = EX_OSERR;
        goto finish;
    }

   /* Update the mod time of the resulting mkext file. On error, print
    * a message, but don't unlink the file. The mod time being out of
    * whack should be sufficient to prevent the file from being used.
    */
    /* XX we might want to add an F_FULLFSYNC before updating the mod time
     * see also 6157625 ... which we'd like to reproduce and then see
     * if a F_FULLFSYNC here would help.
     */
    if (updateModTime) {
        if (utimes(mkextPath, cacheFileTimes)) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "Can't update mod time of %s - %s.", mkextPath, strerror(errno));
        }
    }

    result = EX_OK;

    OSKextLog(/* kext */ NULL,
        kOSKextLogBasicLevel | kOSKextLogGeneralFlag | kOSKextLogArchiveFlag,
        "Created mkext archive %s.", mkextPath);

finish:
    SAFE_RELEASE(archiveKexts);
    SAFE_RELEASE(mkexts);
    SAFE_RELEASE(mkext);
    SAFE_RELEASE(systemMkextFolderURL);

    if (fileDescriptor != -1) {
        close(fileDescriptor);
        if (result != EX_OK) {
            unlink(tmpPath);
        }
    }
    
    putVolumeForPath(mkextPath, result);

    return result;
}

/*******************************************************************************
*******************************************************************************/
typedef struct {
    KextcacheArgs     * toolArgs;
    CFMutableArrayRef   kextArray;
} FilterIDContext;

void filterKextID(const void * vValue, void * vContext)
{
    CFStringRef       kextID  = (CFStringRef)vValue;
    FilterIDContext * context = (FilterIDContext *)vContext;
    OSKextRef       theKext = OSKextGetKextWithIdentifier(kextID);
    
    if (!theKext) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Internal error filtering kexts by identifier.");
        goto finish;
    }
    
    if (OSKextMatchesRequiredFlags(theKext, context->toolArgs->requiredFlagsAll) &&
        !CFArrayContainsValue(context->kextArray,
            RANGE_ALL(context->kextArray), theKext)) {

        CFArrayAppendValue(context->kextArray, theKext);
    }
    
finish:
    return;    
}

/******************************************************************************/

ExitStatus filterKextsForMkext(
    KextcacheArgs     * toolArgs,
    CFMutableArrayRef   kextArray,
    const NXArchInfo  * arch,
    Boolean           * fatalOut)
{
    ExitStatus          result        = EX_SOFTWARE;
    OSKextRequiredFlags requiredFlags;
    CFIndex             count, i;

    CFArrayRemoveAllValues(kextArray);
    
    if (CFSetGetCount(toolArgs->kextIDs)) {
        FilterIDContext context;

        context.toolArgs = toolArgs;
        context.kextArray = kextArray;
        CFSetApplyFunction(toolArgs->kextIDs, filterKextID, &context);

        result = EX_OK;
        goto finish;
    }

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
        char kextPath[PATH_MAX];
        OSKextRef theKext = (OSKextRef)CFArrayGetValueAtIndex(
            toolArgs->repositoryKexts, i);

        if (!CFURLGetFileSystemRepresentation(OSKextGetURL(theKext),
            /* resolveToBase */ false, (UInt8 *)kextPath, sizeof(kextPath))) {
            
            strlcpy(kextPath, "(unknown)", sizeof(kextPath));
        }

       /* Skip kexts we have no interest in for the current arch.
        */
        if (!OSKextIsValid(theKext)) {
            // xxx - should also use kOSKextLogArchiveFlag?
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogArchiveFlag |
                kOSKextLogValidationFlag | kOSKextLogGeneralFlag, 
                "%s is not valid; omitting from mkext.", kextPath);
            if (toolArgs->printTestResults) {
                OSKextLogDiagnostics(theKext, kOSKextDiagnosticsFlagAll);
            }
            continue;
        }
        if (!OSKextSupportsArchitecture(theKext, arch)) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogStepLevel | kOSKextLogArchiveFlag,
                "%s doesn't support architecture '%s'; omitting from mkext.", kextPath,
                arch->name);
            continue;
        }
        if (!OSKextMatchesRequiredFlags(theKext, requiredFlags)) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogStepLevel | kOSKextLogArchiveFlag,
                "%s does not match OSBundleRequired conditions; omitting from mkext.",
                kextPath);
            continue;
        }
        if (!toolArgs->skipAuthentication && !OSKextIsAuthentic(theKext)) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogArchiveFlag |
                kOSKextLogAuthenticationFlag | kOSKextLogGeneralFlag, 
                "%s is not authentic; omitting from mkext.", kextPath);
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
        if (OSKextMatchesRequiredFlags(theKext, requiredFlags) &&
            !CFArrayContainsValue(kextArray, RANGE_ALL(kextArray), theKext)) {
            
            CFArrayAppendValue(kextArray, theKext);
        }
    }

    result = EX_OK;

finish:
    return result;
}

/*******************************************************************************
*******************************************************************************/
ExitStatus writeToFile(
    int           fileDescriptor,
    const UInt8 * data,
    CFIndex       length)
{
    ExitStatus result = EX_OSERR;
    int bytesWritten = 0;
    int totalBytesWritten = 0;
    
    while (totalBytesWritten < length) {
        bytesWritten = write(fileDescriptor, data + totalBytesWritten,
            length - totalBytesWritten);
        if (bytesWritten < 0) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "Write failed - %s", strerror(errno));
            goto finish;
        }
        totalBytesWritten += bytesWritten;
    }

    result = EX_OK;
finish:
    return result;
}

/*******************************************************************************
 *******************************************************************************/
int getFileOffsetAndSizeForArch(
    int                 fileDescriptor,
    const NXArchInfo  * archInfo,
    off_t             * sliceOffsetOut,
    size_t            * sliceSizeOut)
{
    int result = -1;
    void *headerPage = NULL;
    struct fat_header *fatHeader = NULL;
    struct fat_arch *fatArch = NULL;
    off_t sliceOffset = 0;
    size_t sliceSize = 0;
    
   /* Map the first page to read the fat headers.
    */
    headerPage = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_FILE | MAP_PRIVATE, 
        fileDescriptor, 0);
    if (!headerPage) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Failed to map file header page.");
        goto finish;
    }
    
    /* If the file is fat, find the appropriate fat slice.  Otherwise, ensure it is
     * compatible with the host architecture.
     */
    
    /* Make sure that the fat header, if any, is swapped to the host's byte order */
    
    fatHeader = (struct fat_header *) headerPage;
    fatArch = (struct fat_arch *) (&fatHeader[1]);
    
    if (fatHeader->magic == FAT_CIGAM) {
        swap_fat_header(fatHeader, NXHostByteOrder());
        swap_fat_arch(fatArch, fatHeader->nfat_arch, NXHostByteOrder());
    }
    
    /* If we have a fat file, find the host architecture's slice.  If not, record
     * the whole file's size.
     */
    
    if (fatHeader->magic == FAT_MAGIC) {
        fatArch = NXFindBestFatArch(archInfo->cputype, archInfo->cpusubtype, 
                                    fatArch, fatHeader->nfat_arch);
        if (!fatArch) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogGeneralFlag, 
                "Fat file does not contain requested architecture %s.",
                archInfo->name);
            goto finish;
        }
        
        sliceOffset = fatArch->offset;
        sliceSize = fatArch->size;
    } else {
        struct stat fileInfo;
        
        if (fstat(fileDescriptor, &fileInfo)) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "Could not stat file.");  // xxx - which file is that?
            goto finish;
        }
        
        sliceSize = fileInfo.st_size;
    }
    
    /* Return the offset and size info */
    
    *sliceOffsetOut = sliceOffset;
    *sliceSizeOut = sliceSize;
    result = 0;
    
finish:
    return result;
}

/*******************************************************************************
 *******************************************************************************/
int readFileAtOffset(
    int         fileDescriptor,
    off_t       fileOffset,
    size_t      fileSize,
    u_char    * buf)
{
    int result = -1;
    off_t seekedBytes = 0;
    ssize_t readBytes = 0;
    size_t totalReadBytes = 0;
    
    /* Seek to the specified file offset */
    
    seekedBytes = lseek(fileDescriptor, fileOffset, SEEK_SET);
    if (seekedBytes != fileOffset) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Failed to seek in file.");  // xxx - which file is that?
        goto finish;
    }
    
    /* Read the file's bytes into the provided buffer */
    
    while (totalReadBytes < fileSize) {
        readBytes = read(fileDescriptor, buf + totalReadBytes,
                         fileSize - totalReadBytes);
        if (readBytes < 0) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "Failed to read file.");
            goto finish;
        }
        
        totalReadBytes += (size_t) readBytes;
    }
    
    result = 0;
finish:
    return result;
}

/*******************************************************************************
 *******************************************************************************/
int verifyMachOIsArch(
    u_char           * fileBuf, 
    size_t             size,
    const NXArchInfo * archInfo)
{
    int result = -1;
    cpu_type_t cputype = 0;
    struct mach_header *checkHeader = (struct mach_header *)fileBuf;

    if (!archInfo) {
        result = 0;
        goto finish;
    }
    
    /* Get the cputype from the mach header */
    if (size < sizeof(uint32_t)) {
        goto finish;
    }
    
    if (checkHeader->magic == MH_MAGIC_64 || checkHeader->magic == MH_CIGAM_64) {
        struct mach_header_64 *machHeader = 
        (struct mach_header_64 *) fileBuf;
        
        if (size < sizeof(*machHeader)) {
            goto finish;
        }
        
        cputype = machHeader->cputype;
        if (checkHeader->magic == MH_CIGAM_64) cputype = OSSwapInt32(cputype);
    } else if (checkHeader->magic == MH_MAGIC || checkHeader->magic == MH_CIGAM) {
        struct mach_header *machHeader = 
        (struct mach_header *) fileBuf;
        
        if (size < sizeof(*machHeader)) {
            goto finish;
        }
        
        cputype = machHeader->cputype;
        if (checkHeader->magic == MH_CIGAM) cputype = OSSwapInt32(cputype);
    }
    
   /* Make sure the file's cputype matches the host's.
    */
    if (cputype != archInfo->cputype) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "File is not of expected architecture %s.", archInfo->name);
        goto finish;
    }
    
    result = 0;
finish:
    return result;
}

/*******************************************************************************
 *******************************************************************************/
CFDataRef readMachOFileWithURL(CFURLRef fileURL, const NXArchInfo * archInfo)
{   
    char        filePath[MAXPATHLEN];
    int         fileDescriptor = 0;
    off_t       fileSliceOffset    = 0;
    size_t      fileSliceSize      = 0;
    u_char    * fileBuf        = NULL;  // must free
    CFDataRef   fileImage          = NULL;
    struct stat statBuf;
    
    /* Get the file system representation of the file URL */
    
    if (!CFURLGetFileSystemRepresentation(fileURL, /* resolveToBase */ true, 
            (UInt8 *)filePath, sizeof(filePath))) 
    {
        goto finish;
    }
    
    /* Open the file */
    
    fileDescriptor = open(filePath, O_RDONLY);
    if (fileDescriptor < 0) {
        goto finish;
    }

    if (fstat(fileDescriptor, &statBuf)) {
        goto finish;
    }
    
    /* Find the slice for the running architecture */
    
    if (archInfo) {
        if (getFileOffsetAndSizeForArch(fileDescriptor, archInfo, 
            &fileSliceOffset, &fileSliceSize))  
        {
            goto finish;
        }
    } else {
        fileSliceOffset = 0;
        fileSliceSize = statBuf.st_size;
    }
            
    /* Allocate a buffer for the file */
    
    fileBuf = malloc(fileSliceSize);
    if (!fileBuf) {
        OSKextLogMemError();
        goto finish;
    }
    
    /* Read the file */    

    if (readFileAtOffset(fileDescriptor, fileSliceOffset, 
        fileSliceSize, fileBuf)) 
    {
        goto finish;
    }
    
    /* Verify that the file is of the right architecture */
    
    if (verifyMachOIsArch(fileBuf, fileSliceSize, archInfo)) {
        goto finish;
    }
    
    /* Wrap the file in a CFData object */
    
    fileImage = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, 
        (UInt8 *) fileBuf, fileSliceSize, kCFAllocatorMalloc);
    if (!fileImage) {
        goto finish;
    }
    fileBuf = NULL;
    
finish:
    if (fileBuf) free(fileBuf);
    
    return fileImage;
}

/*********************************************************************
* After this function is called, toolArgs->prelinkedKernelURL names
* the actual file rather than its containing directory.
*********************************************************************/
void getPlatformInfo(
    KextcacheArgs * toolArgs,
    PlatformInfo  * platformInfo)
{
    io_registry_entry_t entry                   = 0;     // must IOObjectRelease()
    CFURLRef            parentURL               = NULL;  // must release
    CFStringRef         prelinkedKernelFilename = NULL;  // must release

    bzero(platformInfo, sizeof(*platformInfo));

    entry = IORegistryEntryFromPath(kIOMasterPortDefault, kIOServicePlane ":/");
    if (entry) {
        if (KERN_SUCCESS !=
            IORegistryEntryGetName(entry, platformInfo->platformName)) {

            platformInfo->platformName[0] = 0;
        }
        IOObjectRelease(entry);
    }

    entry = IORegistryEntryFromPath(kIOMasterPortDefault,
        kIODeviceTreePlane ":/chosen");
    if (entry) {
        CFTypeRef obj = 0;
        obj = IORegistryEntryCreateCFProperty(entry, CFSTR("rootpath"), 
                kCFAllocatorDefault, kNilOptions);
        if (obj && (CFGetTypeID(obj) == CFDataGetTypeID())) {
            CFIndex len = CFDataGetLength((CFDataRef) obj);
            strlcpy(platformInfo->rootPath, (char *) CFDataGetBytePtr((CFDataRef) obj), 
                sizeof(platformInfo->rootPath));
            platformInfo->rootPath[len] = 0;
        } else {
            const char *data;
            char *ptr = platformInfo->rootPath;
            u_long len = 0;
            // Construct entry from UUID of boot volume and kernel name.
            obj = 0;
            do {
                obj = IORegistryEntryCreateCFProperty(entry, CFSTR("boot-device-path"), 
                    kCFAllocatorDefault, kNilOptions);
                if (!obj) {
                    break;
                }

                if (CFGetTypeID(obj) == CFDataGetTypeID()) {
                    data = (char *)CFDataGetBytePtr((CFDataRef) obj);
                    len = CFDataGetLength((CFDataRef) obj);
                } else if (CFGetTypeID(obj) == CFStringGetTypeID()) {
                    data = CFStringGetCStringPtr((CFStringRef) obj, kCFStringEncodingUTF8);
                    if (!data) {
                        break;
                    }
                    len = strlen(data) + 1; // include trailing null
                } else {
                    break;
                }
                if (len > sizeof(platformInfo->rootPath)) {
                    len = sizeof(platformInfo->rootPath);
                }
                memcpy(ptr, data, len);
                ptr += len;

                SAFE_RELEASE_NULL(obj);

                obj = IORegistryEntryCreateCFProperty(entry, CFSTR("boot-file"), 
                    kCFAllocatorDefault, kNilOptions);
                if (!obj) {
                    break;
                }

                if (CFGetTypeID(obj) == CFDataGetTypeID()) {
                    data = (char *)CFDataGetBytePtr((CFDataRef) obj);
                    len = CFDataGetLength((CFDataRef) obj);
                } else if (CFGetTypeID(obj) == CFStringGetTypeID()) {
                    data = CFStringGetCStringPtr((CFStringRef) obj, kCFStringEncodingUTF8);
                    if (!data) {
                        break;
                    }
                    len = strlen(data);
                } else {
                    break;
                }
                if ((ptr - platformInfo->rootPath + len) >=
                    sizeof(platformInfo->rootPath)) {

                    len = sizeof(platformInfo->rootPath) -
                        (ptr - platformInfo->rootPath);
                }
                memcpy(ptr, data, len);
            } while (0);
        }
        SAFE_RELEASE(obj);
        IOObjectRelease(entry);
    }
    if (!platformInfo->platformName[0] || !platformInfo->rootPath[0]) {
        platformInfo->platformName[0] = platformInfo->rootPath[0] = 0;
    }

   /* Update the prelinkedKernelURL to name the file that will be saved,
    * by appending the checksum of the hardware info.
    */
    if (toolArgs->needDefaultPrelinkedKernelInfo) {
        uint32_t adler32 = 0;

        parentURL = toolArgs->prelinkedKernelURL;
        adler32 = OSSwapHostToBigInt32(local_adler32(
            (u_int8_t *)platformInfo, sizeof(*platformInfo)));

        prelinkedKernelFilename = CFStringCreateWithFormat(
            kCFAllocatorDefault, /* options */ 0, CFSTR("%s_%s.%08X"), 
            _kOSKextPrelinkedKernelBasename, toolArgs->archInfo[0]->name,
            adler32);
        if (!prelinkedKernelFilename) {
            OSKextLogMemError();
            goto finish;
        }

        toolArgs->prelinkedKernelURL = CFURLCreateCopyAppendingPathComponent(
            kCFAllocatorDefault, parentURL, prelinkedKernelFilename,
            /* isDir */ false);
        if (!toolArgs->prelinkedKernelURL) {
            OSKextLogMemError();
            goto finish;
        }
    }

finish:
    SAFE_RELEASE(parentURL);
    SAFE_RELEASE(prelinkedKernelFilename);
    return;
}

/*********************************************************************
*********************************************************************/
CFDataRef uncompressPrelinkedKernel(
    CFDataRef      prelinkImage)
{
    CFDataRef                     result              = NULL;
    CFMutableDataRef              uncompressedImage   = NULL;  // must release
    const PrelinkedKernelHeader * prelinkHeader       = NULL;  // do not free
    unsigned char               * buf                 = NULL;  // do not free
    vm_size_t                     bufsize             = 0;
    vm_size_t                     uncompsize          = 0;
    uint32_t                      adler32             = 0;
   
    prelinkHeader = (PrelinkedKernelHeader *) CFDataGetBytePtr(prelinkImage);

   /* Verify the header information.
    */
    if (prelinkHeader->signature != OSSwapHostToBigInt32('comp')) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
            "Compressed prelinked kernel has invalid signature: 0x%x.", 
            prelinkHeader->signature);
        goto finish;
    }

    if (prelinkHeader->compressType != OSSwapHostToBigInt32('lzss')) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
            "Compressed prelinked kernel has invalid compressType: 0x%x.", 
            prelinkHeader->compressType);
        goto finish;
    }


   /* Create a buffer to hold the uncompressed kernel.
    */
    bufsize = OSSwapBigToHostInt32(prelinkHeader->uncompressedSize);
    uncompressedImage = CFDataCreateMutable(kCFAllocatorDefault, bufsize);
    if (!uncompressedImage) {
        goto finish;
    }

   /* We have to call CFDataSetLength explicitly to get CFData to allocate
    * its internal buffer.
    */
    CFDataSetLength(uncompressedImage, bufsize);
    buf = CFDataGetMutableBytePtr(uncompressedImage);
    if (!buf) {
        OSKextLogMemError();
        goto finish;
    }

   /* Uncompress the kernel.
    */
    uncompsize = decompress_lzss(buf, bufsize,
        ((u_int8_t *)(CFDataGetBytePtr(prelinkImage))) + sizeof(*prelinkHeader),
        CFDataGetLength(prelinkImage) - sizeof(*prelinkHeader));
    if (uncompsize != bufsize) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
            "Compressed prelinked kernel uncompressed to an unexpected size: %u.",
            (unsigned)uncompsize);
        goto finish;
    }

   /* Verify the adler32.
    */
    adler32 = local_adler32((u_int8_t *) buf, bufsize);
    if (prelinkHeader->adler32 != OSSwapHostToBigInt32(adler32)) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
            "Checksum error for compressed prelinked kernel.");
        goto finish;
    }

    result = CFRetain(uncompressedImage);
    
finish:
    SAFE_RELEASE(uncompressedImage);
    return result;
}
/*********************************************************************
*********************************************************************/
CFDataRef compressPrelinkedKernel(
    CFDataRef      prelinkImage,
    PlatformInfo * platformInfo)
{
    CFDataRef               result          = NULL;
    CFMutableDataRef        compressedImage = NULL;  // must release
    PrelinkedKernelHeader * kernelHeader    = NULL;  // do not free
    const PrelinkedKernelHeader * kernelHeaderIn = NULL; // do not free
    unsigned char         * buf             = NULL;  // do not free
    unsigned char         * bufend          = NULL;  // do not free
    u_long                  offset          = 0;
    vm_size_t               bufsize         = 0;
    vm_size_t               compsize        = 0;
    uint32_t                adler32         = 0;
   
    /* Check that the kernel is not already compressed */

    kernelHeaderIn = (const PrelinkedKernelHeader *) 
        CFDataGetBytePtr(prelinkImage);
    if (kernelHeaderIn->signature == OSSwapHostToBigInt('comp')) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
            "Prelinked kernel is already compressed.");
        goto finish;
    }

    /* Create a buffer to hold the compressed kernel */

    offset = sizeof(*kernelHeader);
    bufsize = CFDataGetLength(prelinkImage) + offset;
    compressedImage = CFDataCreateMutable(kCFAllocatorDefault, bufsize);
    if (!compressedImage) {
        goto finish;
    }

    /* We have to call CFDataSetLength explicitly to get CFData to allocate
     * its internal buffer.
     */
    CFDataSetLength(compressedImage, bufsize);
    buf = CFDataGetMutableBytePtr(compressedImage);
    if (!buf) {
        OSKextLogMemError();
        goto finish;
    }

    kernelHeader = (PrelinkedKernelHeader *) buf;
    bzero(kernelHeader, sizeof(*kernelHeader));

    /* Fill in the compression information */

    kernelHeader->signature = OSSwapHostToBigInt32('comp');
    kernelHeader->compressType = OSSwapHostToBigInt32('lzss');
    adler32 = local_adler32((u_int8_t *)CFDataGetBytePtr(prelinkImage),
        CFDataGetLength(prelinkImage));
    kernelHeader->adler32 = OSSwapHostToBigInt32(adler32);
    kernelHeader->uncompressedSize = 
        OSSwapHostToBigInt32(CFDataGetLength(prelinkImage));
    
    /* Copy over the machine-identifying info */
    if (platformInfo) {
        strlcpy(kernelHeader->platformName, platformInfo->platformName, 
            sizeof(kernelHeader->platformName));
        memcpy(kernelHeader->rootPath, platformInfo->rootPath,
            sizeof(kernelHeader->rootPath));
    }
    
    /* Compress the kernel */

    bufend = compress_lzss(buf + offset, bufsize, 
        (u_int8_t *)CFDataGetBytePtr(prelinkImage),
        CFDataGetLength(prelinkImage));
    if (!bufend) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
            "Failed to compress prelinked kernel.");
        goto finish;
    }

    compsize = bufend - (buf + offset);
    kernelHeader->compressedSize = OSSwapHostToBigInt32(compsize);
    CFDataSetLength(compressedImage, bufend - buf);

    result = CFRetain(compressedImage);
    
finish:
    SAFE_RELEASE(compressedImage);
    return result;
}

/*******************************************************************************
*******************************************************************************/
ExitStatus createPrelinkedKernel(
    KextcacheArgs       * toolArgs,
    CFDataRef           * prelinkedKernelOut,
    CFDictionaryRef     * prelinkedSymbolsOut,
    const NXArchInfo    * archInfo,
    PlatformInfo        * platformInfo)
{
    ExitStatus result = EX_OSERR;
    CFArrayRef requestedIdentifiers = NULL;
    CFArrayRef kextArray = NULL;
    CFDataRef kernelImage = NULL;
    CFDataRef prelinkedKernel = NULL;

   /* Retrieve the kernel image for the requested architecture.
    */
    kernelImage = readMachOFileWithURL(toolArgs->kernelURL, archInfo);
    if (!kernelImage) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogArchiveFlag |  kOSKextLogFileAccessFlag,
            "Failed to read kernel file.");
        goto finish;
    }

    /* Set the architecture in the OSKext library */

    if (!OSKextSetArchitecture(archInfo)) {
        goto finish;
    }

    if (toolArgs->needLoadedKextInfo) {

        /* If we need to get the requested set of kexts out of the kernel,
         * wait for I/O Kit to quiesce.
         */
        
        (void) waitForIOKitQuiescence();

        /* Get the list of requested bundle IDs from the kernel and find all of
         * the associated kexts.
         */

        requestedIdentifiers = OSKextCopyAllRequestedIdentifiers();
        if (!requestedIdentifiers) {
            goto finish;
        }

        kextArray = OSKextCopyKextsWithIdentifiers(requestedIdentifiers);
        if (!kextArray) {
            goto finish;
        }
    } else {
        Boolean                 fatalOut = false;
        CFMutableArrayRef       kexts = NULL;
        
        if (!createCFMutableArray(&kexts, &kCFTypeArrayCallBacks)) {
            OSKextLogMemError();
            goto finish;
        }
        
        /*****
         * Figure out which kexts we're actually archiving.
         * This uses toolArgs->allKexts, which must already be created.
         */
        result = filterKextsForMkext(toolArgs, kexts, archInfo, &fatalOut);
        if (result != EX_OK || fatalOut) {
            SAFE_RELEASE(kexts);
            goto finish;
        }
        
        result = EX_OSERR;
        
        if (!CFArrayGetCount(kexts)) {
            SAFE_RELEASE(kexts);
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
                 "No kexts found for architecture %s.",
                 archInfo->name);
            goto finish;
        }

        kextArray = CFArrayCreateCopy(kCFAllocatorDefault, kexts);
        if (!kextArray) {
            SAFE_RELEASE(kexts);                
            OSKextLogMemError();
            goto finish;
        }
        SAFE_RELEASE(kexts);
    }
    
    /* Create the prelinked kernel from the given kernel and kexts */
    
    prelinkedKernel = OSKextCreatePrelinkedKernel(kernelImage, kextArray,
        toolArgs->volumeRootURL, toolArgs->prelinkedKernelErrorRequired,
        toolArgs->skipAuthentication, toolArgs->printTestResults,
        toolArgs->includeAllPersonalities, !toolArgs->omitLinkState,
        prelinkedSymbolsOut);
    if (!prelinkedKernel) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
            "Failed to generate prelinked kernel.");
        if (toolArgs->prelinkedKernelErrorRequired) {
            result = EX_OSERR;
        }
        goto finish;
    }

    /* Compress the prelinked kernel if needed */

    if (toolArgs->compress) {
        *prelinkedKernelOut = compressPrelinkedKernel(prelinkedKernel, platformInfo);
    } else {
        *prelinkedKernelOut = CFRetain(prelinkedKernel);
    }

    if (!*prelinkedKernelOut) {
        goto finish;
    }

    result = EX_OK;
    
finish:
    SAFE_RELEASE(kernelImage);
    SAFE_RELEASE(requestedIdentifiers);
    SAFE_RELEASE(kextArray);
    SAFE_RELEASE(prelinkedKernel);
    
    return result;
}

/*******************************************************************************
*******************************************************************************/
ExitStatus savePrelinkedKernel(
    KextcacheArgs       * toolArgs,
    CFDataRef             prelinkedKernel,
    CFDictionaryRef       prelinkedSymbols)
{
    ExitStatus result = EX_OSERR;
    int fileDescriptor = 0;
    mode_t real_umask = 0;
    char tmpPath[MAXPATHLEN];
    char finalPath[MAXPATHLEN];
    struct timeval cacheFileTimes[2];
    boolean_t updateModTime = false;
    SaveFileContext saveFileContext;

    if (toolArgs->needDefaultPrelinkedKernelInfo) {
        if (!_OSKextCreateFolderForCacheURL(toolArgs->prelinkedKernelURL)) {
            goto finish;
        }
    }

    if (!CFURLGetFileSystemRepresentation(toolArgs->prelinkedKernelURL,
        /* resolveToBase */ true, (UInt8 *)tmpPath, sizeof(finalPath))) {

        OSKextLogStringError(/* kext */ NULL);
        goto finish;
    }

    strlcpy(finalPath, tmpPath, sizeof(finalPath));
    
    if (strlcat(tmpPath, ".XXXX", sizeof(tmpPath)) >= sizeof(tmpPath)) {
        OSKextLogStringError(/* kext */ NULL);
        goto finish;
    }
    
    fileDescriptor = mkstemp(tmpPath);
    if (-1 == fileDescriptor) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Can't create %s - %s.",
            tmpPath, strerror(errno));
        goto finish;
    }

   /* Set the umask to get it, then set it back to iself. Wish there were a
    * better way to query it.
    */
    real_umask = umask(0);
    umask(real_umask);

    if (-1 == fchmod(fileDescriptor, MKEXT_PERMS & ~real_umask)) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
             "Can't set permissions on %s - %s.",
            tmpPath, strerror(errno));
    }

    result = writeToFile(fileDescriptor,
        CFDataGetBytePtr(prelinkedKernel),
        CFDataGetLength(prelinkedKernel));
    if (result != EX_OK) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Can't save prelinked kernel - %s.",
            strerror(errno));
        goto finish;
    }

    if (close(fileDescriptor)) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
            "Failed to close prelinked kernel file - %s.",
            strerror(errno));
        goto finish;
    }

    if (toolArgs->haveFolderMtime && CFArrayGetCount(toolArgs->repositoryURLs)) {
        CFURLRef firstURL = (CFURLRef)CFArrayGetValueAtIndex(
            toolArgs->repositoryURLs, 0);
        struct stat     newStatBuffer;
        struct timespec newModTime;
        struct timespec origModTime =
            toolArgs->firstFolderStatBuffer.st_mtimespec;
        struct timeval kernelTimeval;
        struct timeval kextTimeval;
        
        /* Check kext repository mod time */

        result = statURL(firstURL, &newStatBuffer);
        if (result != EX_OK) {
            goto finish;
        }
        newModTime = newStatBuffer.st_mtimespec;
        if ((newModTime.tv_sec != origModTime.tv_sec) ||
            (newModTime.tv_nsec != origModTime.tv_nsec)) {

            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "Source directory has changed since starting; "
                "not saving cache file %s.",
                finalPath);
            result = kKextcacheExitStale;
            goto finish;
        }

        /* Check kernel mod time */

        result = statURL(toolArgs->kernelURL, &newStatBuffer);
        if (result != EX_OK) {
            goto finish;
        }
        
        origModTime = toolArgs->kernelStatBuffer.st_mtimespec;
        newModTime = newStatBuffer.st_mtimespec;
        if ((newModTime.tv_sec != origModTime.tv_sec) ||
            (newModTime.tv_nsec != origModTime.tv_nsec)) {

            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "Source kernel has changed since starting; "
                "not saving cache file %s.",
                finalPath);
            result = kKextcacheExitStale;
            goto finish;
        }

        /* Get the access and mod times of the later modified of the kernel
         * and kext repository.
         */

        TIMESPEC_TO_TIMEVAL(&kextTimeval, &toolArgs->firstFolderStatBuffer.st_mtimespec);
        TIMESPEC_TO_TIMEVAL(&kernelTimeval, &toolArgs->kernelStatBuffer.st_mtimespec);

        if (timercmp(&kextTimeval, &kernelTimeval, >)) {
            newStatBuffer = toolArgs->firstFolderStatBuffer;
        }

        /* Set the mod time of the kernelcache relative to the kernel */

        TIMESPEC_TO_TIMEVAL(&cacheFileTimes[0], &newStatBuffer.st_atimespec);
        TIMESPEC_TO_TIMEVAL(&cacheFileTimes[1], &newStatBuffer.st_mtimespec);
        cacheFileTimes[1].tv_sec++;
        updateModTime = true;
    }

    if (rename(tmpPath, finalPath) != 0) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Can't rename tmp prelinked kernel file - %s.",
            strerror(errno));
        result = EX_OSERR;
        goto finish;
    }

   /* Update the mod time of the resulting kernelcache. On error, print
    * a message, but don't unlink the file. The mod time being out of
    * whack should be sufficient to prevent the file from being used.
    */
    if (updateModTime) {
        if (utimes(finalPath, cacheFileTimes)) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "Can't update mod time of %s - %s.",
                finalPath, strerror(errno));
        }
    }
    
    if (prelinkedSymbols) {
        saveFileContext.saveDirURL = toolArgs->symbolDirURL;
        saveFileContext.overwrite = true;
        saveFileContext.fatal = false;
        CFDictionaryApplyFunction(prelinkedSymbols, &saveFile, &saveFileContext);
        if (saveFileContext.fatal) {
            goto finish;
        }
    }

    result = EX_OK;

    OSKextLog(/* kext */ NULL,
        kOSKextLogBasicLevel | kOSKextLogGeneralFlag | kOSKextLogArchiveFlag,
        "Created prelinked kernel %s.", finalPath);

finish:
    return result;
}

/*******************************************************************************
* takeVolumeForPath turns the path into a volume UUID and locks with kextd
*******************************************************************************/
// upstat() stat()s "up" the path if a file doesn't exist
static int upstat(const char *path, struct stat *sb, struct statfs *sfs)
{
    int rval = ELAST+1;
    char buf[PATH_MAX], *tpath = buf;
    struct stat defaultsb;

    if (strlcpy(buf, path, PATH_MAX) > PATH_MAX)        goto finish;

    if (!sb)    sb = &defaultsb;
    while ((rval = stat(tpath, sb)) == -1 && errno == ENOENT) {
        // "." and "/" should always exist, but you never know
        if (tpath[0] == '.' && tpath[1] == '\0')  goto finish;
        if (tpath[0] == '/' && tpath[1] == '\0')  goto finish;
        tpath = dirname(tpath);     // Tiger's dirname() took const char*
    }

    // call statfs if the caller needed it
    if (sfs)
        rval = statfs(tpath, sfs);

finish:
    if (rval) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogWarningLevel | kOSKextLogFileAccessFlag,
                "Couldn't find volume for %s.", path);
    }

    return rval;
}

#define COMPILE_TIME_ASSERT(pred)   switch(0){case 0:case pred:;}
static int getVolumeUUID(const char *volPath, uuid_t vol_uuid)
{
    int rval = ENODEV;
    DADiskRef dadisk = NULL;
    CFDictionaryRef dadesc = NULL;
    CFUUIDRef volUUID;      // just a reference into the dict
    CFUUIDBytes uuidBytes;
COMPILE_TIME_ASSERT(sizeof(CFUUIDBytes) == sizeof(uuid_t));

    dadisk = createDiskForMount(NULL, volPath);
    if (!dadisk)        goto finish;
    dadesc = DADiskCopyDescription(dadisk);
    if (!dadesc)        goto finish;
    volUUID = CFDictionaryGetValue(dadesc, kDADiskDescriptionVolumeUUIDKey);
    if (!volUUID)       goto finish;
    uuidBytes = CFUUIDGetUUIDBytes(volUUID);
    memcpy(vol_uuid, &uuidBytes.byte0, sizeof(uuid_t));   // sizeof(vol_uuid)?

    rval = 0;

finish:
    if (dadesc)     CFRelease(dadesc);
    if (dadisk)     CFRelease(dadisk);

    return rval;
}


// takeVolumeForPaths ensures all paths are on the given volume, then locks
int takeVolumeForPaths(char *volPath)
{
    int bsderr, rval = ELAST + 1;
    struct stat volsb;

    bsderr = stat(volPath, &volsb);
    if (bsderr)  goto finish;

    rval = takeVolumeForPath(volPath);

finish:
    if (bsderr) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogWarningLevel | kOSKextLogFileAccessFlag,
                "Couldn't lock paths on volume %s.", volPath);
        rval = bsderr;
    } 

    return rval;
}

// can return success if a lock isn't needed
// can return failure if sLockPort is already in use
#define WAITFORLOCK 1
static int takeVolumeForPath(const char *path)
{
    int rval = ELAST + 1;
    kern_return_t macherr = KERN_SUCCESS;
    int lckres = 0;
    struct statfs sfs;
    const char *volPath;
    mach_port_t taskport = MACH_PORT_NULL;

    if (sLockPort) {
        return EALREADY;        // only support one lock at a time
    }

    if (geteuid() != 0) {
        // kextd shouldn't be watching anything you can touch
        // and ignores locking requests from non-root anyway
        rval = 0;
        goto finish;
    }

    // look up kextd port if not cached
    // XX if there's a way to know kextd isn't running, we could skip
    // unnecessarily bringing it up in the boot-time case (5108882?).
    if (!sKextdPort) {
        macherr=bootstrap_look_up(bootstrap_port,KEXTD_SERVER_NAME,&sKextdPort);
        if (macherr)  goto finish;
    }

    if ((rval = upstat(path, NULL, &sfs)))      goto finish;
    volPath = sfs.f_mntonname;

    // get the volume's UUID
    if ((rval = getVolumeUUID(volPath, s_vol_uuid))) {
        goto finish;
    }
    
    // allocate a port to pass (in case we die -- kernel cleans up on exit())
    taskport = mach_task_self();
    if (taskport == MACH_PORT_NULL)  goto finish;
    macherr = mach_port_allocate(taskport, MACH_PORT_RIGHT_RECEIVE, &sLockPort);
    if (macherr)  goto finish;

    // try to take the lock; warn if it's busy and then wait for it
    // X kextcache -U, if it is going to lock at all, needs only WAITFORLOCK
    macherr = kextmanager_lock_volume(sKextdPort, sLockPort, s_vol_uuid,
                                      !WAITFORLOCK, &lckres);
    if (macherr)        goto finish;

    // 5519500: sleep if kextd hasn't gotten w/diskarb yet
    while (lckres == EAGAIN) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
            "kextd wasn't ready; waiting 10 seconds and trying again");
        sleep(10);
        macherr = kextmanager_lock_volume(sKextdPort, sLockPort, s_vol_uuid,
                                          !WAITFORLOCK, &lckres);
        if (macherr)    goto finish;
    }

    // now, if it was busy, let's sleep until it's free
    if (lckres == EBUSY || lckres == EAGAIN) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
            "%s locked; waiting for lock.", volPath);
        macherr = kextmanager_lock_volume(sKextdPort, sLockPort, s_vol_uuid,
            WAITFORLOCK, &lckres);
        if (macherr)    goto finish;
        if (lckres == 0) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
                "Lock acquired; proceeding.");
        }
    }
    
    // kextd might not be watching this volume 
    // or might be telling us that it went away (not watching any more)
    // so we set our success to the existance of the volume's root
    if (lckres == ENOENT) {
        struct stat sb;
        rval = stat(volPath, &sb);
    } else {
        rval = lckres;
    }

finish: 
    if (sLockPort != MACH_PORT_NULL && (lckres != 0 || macherr)) {
        mach_port_mod_refs(taskport, sLockPort, MACH_PORT_RIGHT_RECEIVE, -1);
        sLockPort = MACH_PORT_NULL;
    }

    /* XX needs unraveling XX */
    // if kextd isn't competing with us, then we didn't need the lock
    if (macherr == BOOTSTRAP_UNKNOWN_SERVICE) {
        rval = 0;
    } else if (macherr) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
            "Couldn't lock %s: %s (%d).", path,
            safe_mach_error_string(macherr), macherr);
        rval = macherr;
    } else {
        // dump rval
        if (rval == -1) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogWarningLevel | kOSKextLogFileAccessFlag,
                "Couldn't lock %s.", path);
            rval = errno;
        } else if (rval) {
            // lckres == EAGAIN should get here
            OSKextLog(/* kext */ NULL,
                kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
                "Couldn't lock %s: %s", path, strerror(rval));
        }
    }

    return rval;
}


/*******************************************************************************
* putVolumeForPath will unlock the relevant volume, passing 'status' to
* inform kextd whether we succeded, failed, or just need more time
*******************************************************************************/
int putVolumeForPath(const char *path, int status)
{
    int rval = KERN_SUCCESS;

    // if not locked, don't sweat it
    if (sLockPort == MACH_PORT_NULL)
        goto finish;

    rval = kextmanager_unlock_volume(sKextdPort, sLockPort, s_vol_uuid, status);

    // tidy up; the server will clean up its stuff if we die prematurely
    mach_port_mod_refs(mach_task_self(),sLockPort,MACH_PORT_RIGHT_RECEIVE,-1);
    sLockPort = MACH_PORT_NULL;

finish:
    if (rval) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
            "Couldn't unlock volume for %s: %s (%d).",
            path, safe_mach_error_string(rval), rval);
    }

    return rval;
}

/*******************************************************************************
* usage()
*******************************************************************************/
void usage(UsageLevel usageLevel)
{
    fprintf(stderr,
      "usage: %1$s <mkext_flag> [options] [--] [kext or directory] ...\n"
      "       %1$s -system-mkext [options] [--] [kext or directory] ...\n"
      "       %1$s -prelinked-kernel <filename> [options] [--] [kext or directory]\n"
      "       %1$s -system-prelinked-kernel\n"
      "       %1$s [options] -prelinked-kernel\n"
      "       %1$s -update-volume <volume> [options]\n"
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
    fprintf(stderr, "-%s (-%c): update the startup mkext for the system\n",
        kOptNameSystemMkext, kOptSystemMkext);
    fprintf(stderr, "-%s [<filename>] (-%c):\n"
        "        create/update prelinked kernel (must be last if no filename given)\n",
        kOptNamePrelinkedKernel, kOptPrelinkedKernel);
    fprintf(stderr, "-%s:\n"
        "        create/update system prelinked kernel\n",
        kOptNameSystemPrelinkedKernel);
    fprintf(stderr, "-%s <volume> (-%c): update system kext caches for <volume>\n",
        kOptNameUpdate, kOptUpdate);
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
        "kext or directory: Add the kext or all kexts in directory to cache\n");
    fprintf(stderr, "-%s <bundle_id> (-%c):\n"
        "        add the kext whose CFBundleIdentifier is <bundle_id>\n",
        kOptNameBundleIdentifier, kOptBundleIdentifier);
    fprintf(stderr, "-%s <volume>:\n"
        "        Save kext paths in an mkext archive or prelinked kernel "
        " relative to <volume>\n",
        kOptNameVolumeRoot);
    fprintf(stderr, "-%s <kernel_filename> (-%c): Use kernel_filename for a prelinked kernel\n",
        kOptNameKernel, kOptKernel);
    fprintf(stderr, "-%s (-%c): Include all kexts ever loaded in prelinked kernel\n",
        kOptNameAllLoaded, kOptAllLoaded);
    fprintf(stderr, "-%s (-%c): Update volumes even if they look up to date\n",
        kOptNameForce, kOptForce);
    fprintf(stderr, "\n");

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
