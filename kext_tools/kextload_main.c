/*
 *  kextload_main.c
 *  kext_tools
 *
 *  Created by Nik Gervae on 11/08/08.
 *  Copyright 2008 Apple Inc. All rights reserved.
 *
 */
#include "kextload_main.h"
#include "kext_tools_util.h"

#include <libc.h>
#include <sysexits.h>

#include <IOKit/kext/KextManager.h>
#include <IOKit/kext/KextManagerPriv.h>
#include <IOKit/kext/kextmanager_types.h>
#include <IOKit/kext/OSKextPrivate.h>

#pragma mark Constants
/*******************************************************************************
* Constants
*******************************************************************************/

#pragma mark Global/Static Variables
/*******************************************************************************
* Global/Static Variables
*******************************************************************************/
const char * progname = "(unknown)";

#pragma mark Main Routine
/*******************************************************************************
* Global variables.
*******************************************************************************/
ExitStatus
main(int argc, char * const * argv)
{
    ExitStatus    result         = EX_SOFTWARE;
    OSReturn      loadResult     = kOSReturnError;
    KextloadArgs  toolArgs;
    char          scratchCString[PATH_MAX];
    CFIndex       count, index;

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
    result = readArgs(argc, argv, &toolArgs);
    if (result != EX_OK) {
        if (result == kKextloadExitHelp) {
            result = EX_OK;
        }
        goto finish;
    }

    result = checkArgs(&toolArgs);
    if (result != EX_OK) {
        goto finish;
    }

   /* From here on out the default exit status is ok.
    */
    result = EX_OK;

    count = CFArrayGetCount(toolArgs.kextIDs);
    for (index = 0; index < count; index++) {
        CFStringRef kextID = CFArrayGetValueAtIndex(
            toolArgs.kextIDs,
            index);

        if (!CFStringGetCString(kextID, scratchCString, sizeof(scratchCString),
            kCFStringEncodingUTF8)) {

            strlcpy(scratchCString, "unknown", sizeof(scratchCString));
        }
        OSKextLog(/* kext */ NULL,
            kOSKextLogBasicLevel | kOSKextLogGeneralFlag |
            kOSKextLogLoadFlag | kOSKextLogIPCFlag,
            "Requesting load of %s.", scratchCString);
        loadResult = KextManagerLoadKextWithIdentifier(kextID,
            toolArgs.dependencyAndRepositoryURLs);
        if (loadResult != kOSReturnSuccess) {
            // xxx - add mach_error_string() when those are in a build
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
                "%s failed to load - %s; "
                "check the system/kernel logs for errors or try kextutil(8).",
                scratchCString, safe_mach_error_string(loadResult));
            if (result == EX_OK) {
                result = exitStatusForOSReturn(loadResult);
                // keep trying though
            }
        } else {
            OSKextLog(/* kext */ NULL,
                kOSKextLogBasicLevel | kOSKextLogGeneralFlag | kOSKextLogLoadFlag,
                "%s loaded successfully (or already loaded).",
                scratchCString);
        }
    }

    count = CFArrayGetCount(toolArgs.kextURLs);
    for (index = 0; index < count; index++) {
        CFURLRef kextURL = CFArrayGetValueAtIndex(
            toolArgs.kextURLs,
            index);
        if (!CFURLGetFileSystemRepresentation(kextURL, /* resolveToBase */ true,
            (UInt8 *)scratchCString, sizeof(scratchCString))) {
            
            strlcpy(scratchCString, "unknown", sizeof(scratchCString));
        }
        OSKextLog(/* kext */ NULL,
            kOSKextLogBasicLevel | kOSKextLogGeneralFlag |
            kOSKextLogLoadFlag | kOSKextLogIPCFlag,
            "Requesting load of %s.",
            scratchCString);
        loadResult = KextManagerLoadKextWithURL(kextURL,
            toolArgs.dependencyAndRepositoryURLs);
        if (loadResult != kOSReturnSuccess) {
            // xxx - add mach_error_string() when those are in a build
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
                "%s failed to load - %s; "
                "check the system/kernel logs for errors or try kextutil(8).",
                scratchCString, safe_mach_error_string(loadResult));
            if (result == EX_OK) {
                result = exitStatusForOSReturn(loadResult);
                // keep trying though
            }
        } else {
            OSKextLog(/* kext */ NULL,
                kOSKextLogBasicLevel | kOSKextLogGeneralFlag | kOSKextLogLoadFlag,
                "%s loaded successfully (or already loaded).",
                scratchCString);
        }
    }

finish:

   /* We're actually not going to free anything else because we're exiting!
    */
    exit(result);

    SAFE_RELEASE(toolArgs.kextIDs);
    SAFE_RELEASE(toolArgs.dependencyAndRepositoryURLs);
    SAFE_RELEASE(toolArgs.kextURLs);

    return result;
}

#pragma mark Major Subroutines
/*******************************************************************************
* Major Subroutines
*******************************************************************************/
ExitStatus
readArgs(
    int            argc,
    char * const * argv,
    KextloadArgs * toolArgs)
{
    ExitStatus   result = EX_USAGE;
    int          optchar;
    int          longindex;
    CFStringRef  scratchString   = NULL;  // must release
    CFURLRef     scratchURL      = NULL;  // must release
    uint32_t     i;

   /* Set up default arg values.
    */
    bzero(toolArgs, sizeof(*toolArgs));
    
   /*****
    * Allocate collection objects.
    */
    if (!createCFMutableArray(&toolArgs->kextIDs, &kCFTypeArrayCallBacks) ||
        !createCFMutableArray(&toolArgs->dependencyAndRepositoryURLs,
            &kCFTypeArrayCallBacks)   ||
        !createCFMutableArray(&toolArgs->kextURLs, &kCFTypeArrayCallBacks)) {

        result = EX_OSERR;
        OSKextLogMemError();
        exit(result);
    }

    while ((optchar = getopt_long_only(argc, (char * const *)argv,
        kOptChars, sOptInfo, &longindex)) != -1) {

        SAFE_RELEASE_NULL(scratchString);
        SAFE_RELEASE_NULL(scratchURL);

        switch (optchar) {
            case kOptHelp:
                usage(kUsageLevelFull);
                result = kKextloadExitHelp;
                goto finish;
                break;

            case kOptBundleIdentifier:
                scratchString = CFStringCreateWithCString(kCFAllocatorDefault,
                    optarg, kCFStringEncodingUTF8);
                if (!scratchString) {
                    OSKextLogMemError();
                    result = EX_OSERR;
                    goto finish;
                }
                CFArrayAppendValue(toolArgs->kextIDs, scratchString);
                break;
                
            case kOptDependency:
            case kOptRepository:
                scratchURL = CFURLCreateFromFileSystemRepresentation(
                    kCFAllocatorDefault,
                    (const UInt8 *)optarg, strlen(optarg), true);
                if (!scratchURL) {
                    OSKextLogStringError(/* kext */ NULL);
                    result = EX_OSERR;
                    goto finish;
                }
                CFArrayAppendValue(toolArgs->dependencyAndRepositoryURLs,
                    scratchURL);
                break;
                
            case kOptQuiet:
                beQuiet();
                break;

            case kOptVerbose:
                result = setLogFilterForOpt(argc, argv, /* forceOnFlags */ 0);
                break;

            case kOptNoCaches:
                OSKextLog(/* kext */ NULL,
                    kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
                    "Notice: -%s (-%c) ignored; use kextutil(8) to test kexts.",
                    kOptNameNoCaches, kOptNoCaches);
                break;

            case kOptNoLoadedCheck:
                OSKextLog(/* kext */ NULL,
                    kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
                    "Notice: -%s (-%c) ignored.",
                    kOptNameNoLoadedCheck, kOptNoLoadedCheck);
                break;

            case kOptTests:
                OSKextLog(/* kext */ NULL,
                    kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
                    "Notice: -%s (-%c) ignored; use kextutil(8) to test kexts.",
                    kOptNameTests, kOptTests);
                break;

            case 0:
                switch (longopt) {
                   default:
                        OSKextLog(/* kext */ NULL,
                            kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
                            "Use kextutil(8) for development loading of kexts.");
                        goto finish;
                        break;
                }
                break;

            default:
                OSKextLog(/* kext */ NULL,
                    kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
                    "Use kextutil(8) for development loading of kexts.");
                goto finish;
                break;

        } /* switch (optchar) */
    } /* while (optchar = getopt_long_only(...) */

   /*****
    * Record the kext names from the command line.
    */
    for (i = optind; i < argc; i++) {
        SAFE_RELEASE_NULL(scratchURL);
        scratchURL = CFURLCreateFromFileSystemRepresentation(
            kCFAllocatorDefault,
            (const UInt8 *)argv[i], strlen(argv[i]), true);
        if (!scratchURL) {
            result = EX_OSERR;
            OSKextLogMemError();
            goto finish;
        }
        CFArrayAppendValue(toolArgs->kextURLs, scratchURL);
    }

    result = EX_OK;

finish:
    SAFE_RELEASE(scratchString);
    SAFE_RELEASE(scratchURL);

    if (result == EX_USAGE) {
        usage(kUsageLevelBrief);
    }
    return result;
}

/*******************************************************************************
*******************************************************************************/
ExitStatus
checkArgs(KextloadArgs * toolArgs)
{
    ExitStatus         result         = EX_USAGE;

    if (!CFArrayGetCount(toolArgs->kextURLs) &&
        !CFArrayGetCount(toolArgs->kextIDs)) {

        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "No kernel extensions specified; name kernel extension bundles\n"
            "    following options, or use -%s (-%c).",
            kOptNameBundleIdentifier, kOptBundleIdentifier);
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
ExitStatus exitStatusForOSReturn(OSReturn osReturn)
{
    ExitStatus result = EX_OSERR;

    switch (osReturn) {
    case kOSKextReturnNotPrivileged:
        result = EX_NOPERM;
        break;
    default:
        result = EX_OSERR;
        break;
    }
    return result;
}

/*******************************************************************************
* usage()
*******************************************************************************/
void usage(UsageLevel usageLevel)
{
    fprintf(stderr, "usage: %s [options] [--] [kext] ...\n"
      "\n", progname);

    if (usageLevel == kUsageLevelBrief) {
        fprintf(stderr, "use %s -%s for an explanation of each option\n",
            progname, kOptNameHelp);
        return;
    }

    fprintf(stderr, "kext: a kext bundle to load or examine\n");
    fprintf(stderr, "\n");

    fprintf(stderr, "-%s <bundle_id> (-%c):\n"
        "        load/use the kext whose CFBundleIdentifier is <bundle_id>\n",
        kOptNameBundleIdentifier, kOptBundleIdentifier);
    fprintf(stderr, "-%s <kext> (-%c):\n"
        "        consider <kext> as a candidate dependency\n",
        kOptNameDependency, kOptDependency);
    fprintf(stderr, "-%s <directory> (-%c):\n"
        "        look in <directory> for kexts\n",
        kOptNameRepository, kOptRepository);
    fprintf(stderr, "\n");

    fprintf(stderr, "-%s (-%c):\n"
        "        quiet mode: print no informational or error messages\n",
        kOptNameQuiet, kOptQuiet);
    fprintf(stderr, "-%s [ 0-6 | 0x<flags> ] (-%c):\n"
        "        verbose mode; print info about analysis & loading\n",
        kOptNameVerbose, kOptVerbose);
    fprintf(stderr, "\n");

    fprintf(stderr, "-%s (-%c): print this message and exit\n",
        kOptNameHelp, kOptHelp);
    fprintf(stderr, "\n");

    fprintf(stderr, "--: end of options\n");
    return;
}
