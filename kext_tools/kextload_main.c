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
#include <servers/bootstrap.h>
#include <sysexits.h>

#ifdef LOG_32BIT_KEXT_LOAD_INFO_8980953
#if !TARGET_OS_EMBEDDED
#include <asl.h>
#endif /* !TARGET_OS_EMBEDDED */
#endif 

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
const char      * progname     = "(unknown)";
static Boolean    sKextdActive = FALSE;

#ifdef LOG_32BIT_KEXT_LOAD_INFO_8980953
#if !TARGET_OS_EMBEDDED
static aslclient	asl = NULL;
static void log32BitOnlyKexts(OSKextRef theKext, char * thePath);
#endif /* !TARGET_OS_EMBEDDED */
#endif 


#pragma mark Main Routine
/*******************************************************************************
* Global variables.
*******************************************************************************/
ExitStatus
main(int argc, char * const * argv)
{
    ExitStatus   result = EX_SOFTWARE;
    KextloadArgs toolArgs;

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

    result = checkAccess();
    if (result != EX_OK) {
        goto finish;
    }

#if 0 // force verbose logging for testing
    OSKextSetLogFilter(0xfff, /* kernel? */ false);
    OSKextSetLogFilter(0xfff, /* kernel? */ true);
#endif 
	
   /*****
    * Assemble the list of URLs to scan, in this order (the OSKext lib inverts it
    * for last-opened-wins semantics):
    * 1. System repository directories (if not asking kextd to load).
    * 2. Named kexts (always given after -repository & -dependency on command line).
    * 3. Named repository directories (-repository/-r).
    * 4. Named dependencies get priority (-dependency/-d).
    *
    * #2 is necessary since one might try to run kextload on two kexts,
    * one of which depends on the other.
    */
    if (!sKextdActive) {
        CFArrayRef sysExtFolders = OSKextGetSystemExtensionsFolderURLs();
        if (!sysExtFolders) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                "Can't get system extensions folders.");
            result = EX_OSERR;
            goto finish;
        }
        CFArrayAppendArray(toolArgs.scanURLs,
            sysExtFolders, RANGE_ALL(sysExtFolders));
    }
    CFArrayAppendArray(toolArgs.scanURLs, toolArgs.kextURLs,
        RANGE_ALL(toolArgs.kextURLs));
    CFArrayAppendArray(toolArgs.scanURLs, toolArgs.repositoryURLs,
        RANGE_ALL(toolArgs.repositoryURLs));
    CFArrayAppendArray(toolArgs.scanURLs, toolArgs.dependencyURLs,
        RANGE_ALL(toolArgs.dependencyURLs));

    if (sKextdActive) {
        result = loadKextsViaKextd(&toolArgs);
    } else {
        result = loadKextsIntoKernel(&toolArgs);
    }

finish:

#ifdef LOG_32BIT_KEXT_LOAD_INFO_8980953
#if !TARGET_OS_EMBEDDED
	if (asl) {
		asl_close(asl);
	}
#endif /* !TARGET_OS_EMBEDDED */
#endif 
	
   /* We're actually not going to free anything else because we're exiting!
    */
    exit(result);

    SAFE_RELEASE(toolArgs.kextIDs);
    SAFE_RELEASE(toolArgs.dependencyURLs);
    SAFE_RELEASE(toolArgs.repositoryURLs);
    SAFE_RELEASE(toolArgs.kextURLs);
    SAFE_RELEASE(toolArgs.scanURLs);
    SAFE_RELEASE(toolArgs.allKexts);

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
    * Allocate collection objects needed for reading args.
    */
    if (!createCFMutableArray(&toolArgs->kextIDs, &kCFTypeArrayCallBacks)         ||
        !createCFMutableArray(&toolArgs->dependencyURLs, &kCFTypeArrayCallBacks)  ||
        !createCFMutableArray(&toolArgs->repositoryURLs, &kCFTypeArrayCallBacks)  ||
        !createCFMutableArray(&toolArgs->kextURLs, &kCFTypeArrayCallBacks)        ||
        !createCFMutableArray(&toolArgs->scanURLs, &kCFTypeArrayCallBacks)) {

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
                CFArrayAppendValue((optchar == kOptDependency) ?
                    toolArgs->dependencyURLs : toolArgs->repositoryURLs,
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
ExitStatus checkAccess(void)
{
    ExitStatus    result         = EX_OK;
#if !TARGET_OS_EMBEDDED
    kern_return_t kern_result    = kOSReturnError;
    mach_port_t   kextd_port     = MACH_PORT_NULL;

    kern_result = bootstrap_look_up(bootstrap_port,
        (char *)KEXTD_SERVER_NAME, &kextd_port);

    if (kern_result == kOSReturnSuccess) {
        sKextdActive = TRUE;
    } else {
        if (geteuid() == 0) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogBasicLevel | kOSKextLogGeneralFlag |
                kOSKextLogLoadFlag | kOSKextLogIPCFlag,
                "Can't contact kextd; attempting to load directly into kernel.");
        } else {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogGeneralFlag |
                kOSKextLogLoadFlag | kOSKextLogIPCFlag,
                "Can't contact kextd; must run as root to load kexts.");
            result = EX_NOPERM;
            goto finish;
        }
    }

#else

    if (geteuid() != 0) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag |
            kOSKextLogLoadFlag | kOSKextLogIPCFlag,
            "You must be running as root to load kexts.");
        result = EX_NOPERM;
        goto finish;
    }
    
#endif /* !TARGET_OS_EMBEDDED */

finish:
    
#if !TARGET_OS_EMBEDDED
    if (kextd_port != MACH_PORT_NULL) {
        mach_port_destroy(mach_task_self(), kextd_port);
    }
#endif /* !TARGET_OS_EMBEDDED */

    return result;
}

/*******************************************************************************
*******************************************************************************/
ExitStatus loadKextsViaKextd(KextloadArgs * toolArgs)
{
    ExitStatus result     = EX_OK;
    OSReturn   loadResult = kOSReturnError;
    char       scratchCString[PATH_MAX];
    CFIndex    count, index;

    count = CFArrayGetCount(toolArgs->kextIDs);
    for (index = 0; index < count; index++) {
        CFStringRef kextID  = CFArrayGetValueAtIndex(toolArgs->kextIDs, index);
            
        if (!CFStringGetCString(kextID, scratchCString, sizeof(scratchCString),
            kCFStringEncodingUTF8)) {

            strlcpy(scratchCString, "unknown", sizeof(scratchCString));
        }
        
        OSKextLog(/* kext */ NULL,
            kOSKextLogBasicLevel | kOSKextLogGeneralFlag |
            kOSKextLogLoadFlag | kOSKextLogIPCFlag,
            "Requesting load of %s.",
            scratchCString);

        loadResult = KextManagerLoadKextWithIdentifier(kextID,
            toolArgs->scanURLs);
        if (loadResult != kOSReturnSuccess) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
                "%s failed to load - %s; "
                "check the system/kernel logs for errors or try kextutil(8).",
                scratchCString, safe_mach_error_string(loadResult));
            if (result == EX_OK) {
                result = exitStatusForOSReturn(loadResult);
                // keep trying other kexts though
            }
        } else {
            OSKextLog(/* kext */ NULL,
                kOSKextLogBasicLevel | kOSKextLogGeneralFlag | kOSKextLogLoadFlag,
                "%s loaded successfully (or already loaded).",
                scratchCString);
        }
#ifdef LOG_32BIT_KEXT_LOAD_INFO_8980953
#if !TARGET_OS_EMBEDDED

		OSKextRef theKext = NULL; // must release
		theKext = OSKextGetKextWithIdentifier(kextID);
		
		if (theKext) {
			log32BitOnlyKexts(theKext, scratchCString);
			SAFE_RELEASE(theKext);
		}
#endif /* !TARGET_OS_EMBEDDED */
#endif /* 8980953 */
	}		
	
    count = CFArrayGetCount(toolArgs->kextURLs);
    for (index = 0; index < count; index++) {
        CFURLRef kextURL = CFArrayGetValueAtIndex(toolArgs->kextURLs, index);
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
            toolArgs->scanURLs);
        if (loadResult != kOSReturnSuccess) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
                "%s failed to load - %s; "
                "check the system/kernel logs for errors or try kextutil(8).",
                scratchCString, safe_mach_error_string(loadResult));
            if (result == EX_OK) {
                result = exitStatusForOSReturn(loadResult);
                // keep trying other kexts though
            }
        } else {
            OSKextLog(/* kext */ NULL,
                kOSKextLogBasicLevel | kOSKextLogGeneralFlag | kOSKextLogLoadFlag,
                "%s loaded successfully (or already loaded).",
                scratchCString);
        }
#ifdef LOG_32BIT_KEXT_LOAD_INFO_8980953
#if !TARGET_OS_EMBEDDED
		
		OSKextRef theKext = NULL; // must release
		theKext = OSKextCreate(kCFAllocatorDefault, kextURL);
		if (theKext) {
			log32BitOnlyKexts(theKext, scratchCString);
			SAFE_RELEASE(theKext);
		}
#endif /* !TARGET_OS_EMBEDDED */
#endif /* 8980953 */
	}
	
    return result;
}

/*******************************************************************************
*******************************************************************************/
ExitStatus loadKextsIntoKernel(KextloadArgs * toolArgs)
{
    ExitStatus result     = EX_OK;
    OSReturn   loadResult = kOSReturnError;
    char       scratchCString[PATH_MAX];
    CFIndex    count, index;

    OSKextLog(/* kext */ NULL,
        kOSKextLogProgressLevel | kOSKextLogGeneralFlag,
        "Reading extensions.");
    toolArgs->allKexts = OSKextCreateKextsFromURLs(kCFAllocatorDefault,
        toolArgs->scanURLs);
    if (!toolArgs->allKexts) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Can't read kexts from disk.");
        result = EX_OSERR;
        goto finish;
    }

    count = CFArrayGetCount(toolArgs->kextIDs);
    for (index = 0; index < count; index++) {
        OSKextRef     theKext = NULL;  // do not release
        CFStringRef   kextID  = CFArrayGetValueAtIndex(
            toolArgs->kextIDs,
            index);
            
        if (!CFStringGetCString(kextID, scratchCString, sizeof(scratchCString),
            kCFStringEncodingUTF8)) {

            strlcpy(scratchCString, "unknown", sizeof(scratchCString));
        }
        
        OSKextLog(/* kext */ NULL,
            kOSKextLogBasicLevel | kOSKextLogGeneralFlag |
            kOSKextLogLoadFlag | kOSKextLogIPCFlag,
            "Loading %s.",
            scratchCString);

        theKext = OSKextGetKextWithIdentifier(kextID);
        if (!theKext) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
                "Error: Kext %s - not found/unable to create.", scratchCString);
            result = kOSKextReturnNotFound;
            goto finish;
        }

       /* The codepath from this function will do any error logging
        * and cleanup needed.
        */
        loadResult = OSKextLoadWithOptions(theKext,
            /* statExclusion */ kOSKextExcludeNone,
            /* addPersonalitiesExclusion */ kOSKextExcludeNone,
            /* personalityNames */ NULL,
            /* delayAutounloadFlag */ false);

        if (loadResult != kOSReturnSuccess) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
                "%s failed to load - %s.",
                scratchCString, safe_mach_error_string(loadResult));
            if (result == EX_OK) {
                result = exitStatusForOSReturn(loadResult);
                // keep trying other kexts though
            }
        } else {
            OSKextLog(/* kext */ NULL,
                kOSKextLogBasicLevel | kOSKextLogGeneralFlag | kOSKextLogLoadFlag,
                "%s loaded successfully (or already loaded).",
                scratchCString);
        }
    }

    count = CFArrayGetCount(toolArgs->kextURLs);
    for (index = 0; index < count; index++) {
        CFURLRef      kextURL        = CFArrayGetValueAtIndex(
            toolArgs->kextURLs,
            index);
        if (!CFURLGetFileSystemRepresentation(kextURL, /* resolveToBase */ true,
            (UInt8 *)scratchCString, sizeof(scratchCString))) {
            
            strlcpy(scratchCString, "unknown", sizeof(scratchCString));
        }

        OSKextLog(/* kext */ NULL,
            kOSKextLogBasicLevel | kOSKextLogGeneralFlag |
            kOSKextLogLoadFlag | kOSKextLogIPCFlag,
            "Loading %s.",
            scratchCString);

        OSKextRef theKext = NULL;  // do not release

       /* Use OSKextGetKextWithURL() to avoid double open error messages,
        * because we already tried to open all kexts above.
        * That means we don't log here if we don't find the kext.
        */ 
        theKext = OSKextGetKextWithURL(kextURL);
        if (!theKext) {
            loadResult = kOSKextReturnNotFound;
        } else {
           /* The codepath from this function will do any error logging
            * and cleanup needed.
            */
            loadResult = OSKextLoadWithOptions(theKext,
                /* statExclusion */ kOSKextExcludeNone,
                /* addPersonalitiesExclusion */ kOSKextExcludeNone,
                /* personalityNames */ NULL,
                /* delayAutounloadFlag */ false);
        }
        if (loadResult != kOSReturnSuccess) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
                "%s failed to load - %s.",
                scratchCString, safe_mach_error_string(loadResult));
            if (result == EX_OK) {
                result = exitStatusForOSReturn(loadResult);
                // keep trying other kexts though
            }
        } else {
            OSKextLog(/* kext */ NULL,
                kOSKextLogBasicLevel | kOSKextLogGeneralFlag | kOSKextLogLoadFlag,
                "%s loaded successfully (or already loaded).",
                scratchCString);
        }
    }

finish:
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

#ifdef LOG_32BIT_KEXT_LOAD_INFO_8980953
#if !TARGET_OS_EMBEDDED
/*******************************************************************************
 * The purpose of this logging code is to let us know how many K32 only kexts  
 * are out there.  See radar 8980953 and related for details.  This logging is  
 * not needed for Barolo or beyond.
 *
 * Description:
 * 1) we bail if we find an "x86_64" architecture in the kext
 * 2) if we do NOT find an "x86_64" architecture we log it (since this kext 
 *    would not load in K64 only systems and that's really what we want to know)
 * 3) we only care about kexts loaded via kextload and when kextd is active
 *    (kexts loaded during single user mode, no kextd running, are not logged, 
 *    see radar 8860974 for details)
 * 4) we log whether the kext loads or not
 *******************************************************************************/
void log32BitOnlyKexts(OSKextRef theKext, char *thePath)
{
	const NXArchInfo ** arches = NULL;  // must free
	aslmsg				aslMsg = NULL;
	int					i, err;
	
	arches = OSKextCopyArchitectures(theKext);
	if (!arches || !arches[0]) {
		goto cleanupAndReturn;
	}
	
	for (i = 0; arches[i]; i++) {
		/* bail out if we find a 64-bit architecture for this kext */
		if (strcmp(arches[i]->name, "x86_64") == 0) {
			goto cleanupAndReturn;
		}
	}
	
	/* did not find a 64-bit architecture for this kext so log it */
	if (asl == NULL) {
		asl = asl_open("kextloadlogger", 
                       "com.apple.kextload.32bitkext.path", 
                       ASL_OPT_NO_DELAY);
		
		if (asl == NULL) {
			OSKextLog(/* kext */ NULL,
					  kOSKextLogErrorLevel | kOSKextLogLoadFlag | 
                      kOSKextLogIPCFlag,
					  "asl_open failed!");
			goto cleanupAndReturn;
		}
	}
	
	aslMsg = asl_new(ASL_TYPE_MSG);
	if (aslMsg == NULL) {
		OSKextLog(/* kext */ NULL,
				  kOSKextLogErrorLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
				  "asl_new failed!");
		goto cleanupAndReturn;
	}
	
	err = asl_log(asl, aslMsg, ASL_LEVEL_NOTICE, "%s", thePath);
	if (err != 0) {
		OSKextLog(/* kext */ NULL,
				  kOSKextLogErrorLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
				  "asl_log failed with error %d!", err);
	}
	
cleanupAndReturn:
	if (arches) {
		SAFE_FREE(arches);
	}
  	if (aslMsg) {
		asl_free(aslMsg);
	}
	return;
}
#endif /* !TARGET_OS_EMBEDDED */
#endif /* 8980953 */


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
