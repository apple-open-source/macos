/*
 *  kextcache_main.h
 *  kext_tools
 *
 *  Created by nik on 5/20/08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 *
 */
#ifndef _KEXTCACHE_MAIN_H
#define _KEXTCACHE_MAIN_H

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/kext/OSKext.h>

#include <getopt.h>
#include <sysexits.h>

#include <IOKit/kext/OSKext.h>

#include "kext_tools_util.h"
#include "kernelcache.h"

#pragma mark Basic Types & Constants
/*******************************************************************************
* Constants
*******************************************************************************/

enum {
    kKextcacheExitOK          = EX_OK,
    kKextcacheExitNotFound,
    kKextcacheExitArchNotFound,
    kKextcacheExitKextBad,
    kKextcacheExitStale,

    // don't think we use it
    kKextcacheExitUnspecified = 11,

    // don't actually exit with this, it's just a sentinel value
    kKextcacheExitHelp        = 33,
    kKextcacheExitNoStart
};

#pragma mark Command-line Option Definitions
/*******************************************************************************
* Command-line options. This data is used by getopt_long_only().
*
* Options common to all kext tools are in kext_tools_util.h.
*******************************************************************************/

/* Mkext-generation flags.
 */
// kOptNameMkext always represents most recent format supported
#define kOptNameMkext                   "mkext"
#define kOptNameMkext1                  "mkext1"
#define kOptNameMkext2                  "mkext2"
#define kOptNameSystemMkext             "system-mkext"
#define kOptNameVolumeRoot              "volume-root"

// kOptNameBundleIdentifier in kext_tools_util.h
// kOptNameSystemExtensions in kext_tools_util.h

#define kOptNameLocalRoot               "local-root"
#define kOptNameLocalRootAll            "local-root-all"
#define kOptNameNetworkRoot             "network-root"
#define kOptNameNetworkRootAll          "network-root-all"
#define kOptNameSafeBoot                "safe-boot"
#define kOptNameSafeBootAll             "safe-boot-all"

/* Prelinked-kernel-generation flags.
 */
#define kOptNamePrelinkedKernel         "prelinked-kernel"
#define kOptNameSystemPrelinkedKernel   "system-prelinked-kernel"
#define kOptNameKernel                  "kernel"
#define kOptNameAllLoaded               "all-loaded"
#define kOptNameSymbols                 "symbols"

/* Embedded prelinked-kernel-generation flags.
 */
#define kOptNameAllPersonalities        "all-personalities"
#define kOptNameNoLinkFailures          "no-link-failures"
#define kOptNameStripSymbols            "strip-symbols"

/* Misc. cache update flags.
 */
#define kOptNameSystemCaches            "system-caches"

/* Boot!=root flags.
 */
#define kOptNameUpdate                  "update-volume"
#define kOptNameForce                   "force"
#define kOptNameInstaller               "Installer"
#define kOptNameCachesOnly              "caches-only"

/* Misc flags.
 */
#define kOptNameNoAuthentication        "no-authentication"
#define kOptNameTests                   "print-diagnostics"
#define kOptNameCompressed              "compressed"
#define kOptNameUncompressed            "uncompressed"

#define kOptArch                  'a'
// 'b' in kext_tools_util.h
#define kOptPrelinkedKernel       'c'
#define kOptSystemMkext           'e'
#if !NO_BOOT_ROOT
#define kOptForce                 'f'
#endif

// xxx - do we want a longopt for this?
#define kOptLowPriorityFork       'F'
// 'h' in kext_tools_util.h
#define kOptRepositoryCaches      'k'
#define kOptKernel                'K'
#define kOptLocalRoot             'l'
#define kOptLocalRootAll          'L'
// kOptMkext always represents most recent format supported
#define kOptMkext                 'm'
#define kOptNetworkRoot           'n'
#define kOptNetworkRootAll        'N'
// 'q' in kext_tools_util.h
#define kOptAllLoaded             'r'
#define kOptSafeBoot              's'
#define kOptSafeBootAll           'S'
#define kOptTests                 't'
#if !NO_BOOT_ROOT
#define kOptUpdate                'u'
#define kOptCheckUpdate           'U'
#endif
// 'v' in kext_tools_util.h
#define kOptNoAuthentication      'z'

/* Options with no single-letter variant.  */
// Do not use -1, that's getopt() end-of-args return value
// and can cause confusion
#define kLongOptLongindexHack             (-2)
#define kLongOptMkext1                    (-3)
#define kLongOptMkext2                    (-4)
// kLongOptMkext always represents most recent format supported
#define kLongOptMkext                     kLongOptMkext2
#define kLongOptCompressed                (-5)
#define kLongOptUncompressed              (-6)
#define kLongOptSymbols                   (-7)
#define kLongOptSystemCaches              (-8)
#define kLongOptSystemPrelinkedKernel     (-9)
#define kLongOptVolumeRoot               (-10)
#define kLongOptAllPersonalities         (-11)
#define kLongOptNoLinkFailures           (-12)
#define kLongOptStripSymbols             (-13)
#define kLongOptInstaller                (-14)
#define kLongOptCachesOnly               (-15)

#if !NO_BOOT_ROOT
#define kOptChars                ":a:b:c:efFhkK:lLm:nNqrsStu:U:vz"
#else
#define kOptChars                ":a:b:c:eFhkK:lLm:nNqrsStvz"
#endif
/* Some options are now obsolete:
 *     -F (fork)
 *     -k (update plist cache)
 */

int longopt = 0;

struct option sOptInfo[] = {
    { kOptNameLongindexHack,         no_argument,        &longopt, kLongOptLongindexHack },

    { kOptNameHelp,                  no_argument,        NULL,     kOptHelp },
    { kOptNameQuiet,                 no_argument,        NULL,     kOptQuiet },
    { kOptNameVerbose,               optional_argument,  NULL,     kOptVerbose },
    { kOptNameCompressed,            no_argument,        &longopt, kLongOptCompressed },
    { kOptNameUncompressed,          no_argument,        &longopt, kLongOptUncompressed },

    { kOptNameArch,                  required_argument,  NULL,     kOptArch },

    { kOptNameMkext1,                required_argument,  &longopt, kLongOptMkext1 },
    { kOptNameMkext2,                required_argument,  &longopt, kLongOptMkext2 },
    { kOptNameMkext,                 required_argument,  NULL,     kOptMkext },
    { kOptNameSystemMkext,           no_argument,        NULL,     kOptSystemMkext },
    { kOptNameVolumeRoot,            required_argument,  &longopt, kLongOptVolumeRoot },

    { kOptNameSystemCaches,          no_argument,        &longopt, kLongOptSystemCaches },

    { kOptNameBundleIdentifier,      required_argument,  NULL,     kOptBundleIdentifier },

    { kOptNameLocalRoot,             no_argument,        NULL,     kOptLocalRoot },
    { kOptNameLocalRootAll,          no_argument,        NULL,     kOptLocalRootAll },
    { kOptNameNetworkRoot,           no_argument,        NULL,     kOptNetworkRoot },
    { kOptNameNetworkRootAll,        no_argument,        NULL,     kOptNetworkRootAll, },
    { kOptNameSafeBoot,              no_argument,        NULL,     kOptSafeBoot },
    { kOptNameSafeBootAll,           no_argument,        NULL,     kOptSafeBootAll },

    { kOptNamePrelinkedKernel,       optional_argument,  NULL,     kOptPrelinkedKernel },
    { kOptNameSystemPrelinkedKernel, no_argument,        &longopt, kLongOptSystemPrelinkedKernel },
    { kOptNameKernel,                required_argument,  NULL,     kOptKernel },
    { kOptNameAllLoaded,             no_argument,        NULL,     kOptAllLoaded },
    { kOptNameSymbols,               required_argument,  &longopt, kLongOptSymbols },

    { kOptNameAllPersonalities,      no_argument,        &longopt, kLongOptAllPersonalities },
    { kOptNameNoLinkFailures,        no_argument,        &longopt, kLongOptNoLinkFailures },
    { kOptNameStripSymbols,          no_argument,        &longopt, kLongOptStripSymbols },

#if !NO_BOOT_ROOT
    { kOptNameUpdate,                required_argument,  NULL,     kOptUpdate },
    { kOptNameForce,                 no_argument,        NULL,     kOptForce },
    { kOptNameInstaller,             no_argument,        &longopt, kLongOptInstaller },
    { kOptNameCachesOnly,            no_argument,        &longopt, kLongOptCachesOnly },
#endif

    { kOptNameNoAuthentication,      no_argument,        NULL,     kOptNoAuthentication },
    { kOptNameTests,                 no_argument,        NULL,     kOptTests },

#if !NO_BOOT_ROOT
    { NULL,                          required_argument,  NULL,     kOptCheckUpdate },
#endif
    { NULL,                          no_argument,        NULL,     kOptLowPriorityFork },

    { NULL, 0, NULL, 0 }  // sentinel to terminate list
};

typedef struct {
    OSKextRequiredFlags requiredFlagsRepositoriesOnly;  // -l/-n/-s
    OSKextRequiredFlags requiredFlagsAll;              // -L/-N/-S

    Boolean   updateSystemCaches;   // -system-caches
    Boolean   lowPriorityFlag;      // -F
    Boolean   printTestResults;     // -t
    Boolean   skipAuthentication;   // -z

    CFURLRef  volumeRootURL;        // for mkext/prelinked kernel

    char    * mkextPath;         // mkext option arg
    int       mkextVersion;      // -mkext1/-mkext2  0 (no mkext, 1 (old format),
                                    // or 2 (new format)

    char    * prelinkedKernelPath;            // -c option
    Boolean   needDefaultPrelinkedKernelInfo; // -c option w/o arg; 
                                              // prelinkedKernelURL is parent
                                              // directory of final kernelcache
                                              // until we create the cache

    Boolean   needLoadedKextInfo;           // -r option
    Boolean   generatePrelinkedSymbols;     // -symbols option
    Boolean   includeAllPersonalities;      // -all-personalities option
    Boolean   noLinkFailures;               // -no-link-failures option
    Boolean   stripSymbols;                 // -strip-symbols option
    CFURLRef  compressedPrelinkedKernelURL; // -uncompress option

    CFURLRef  updateVolumeURL;  // -u/-U options
    Boolean   expectUpToDate;   // -U
    Boolean   forceUpdateFlag;  // -f
    Boolean   installerCalled;  // -Installer
    Boolean   cachesOnly;   // -caches-only

    char    * kernelPath;    // overriden by -k option
    CFDataRef kernelFile;    // contents of kernelURL
    CFURLRef  symbolDirURL;  // -s option;

    CFMutableSetRef    kextIDs;          // -b; must release
    CFMutableArrayRef  argURLs;          // directories & kexts in order
    CFMutableArrayRef  repositoryURLs;   // just non-kext directories
    CFMutableArrayRef  namedKextURLs;
    CFMutableArrayRef  targetArchs;
    Boolean            explicitArch;  // user-provided instead of inferred host arches

    CFArrayRef         allKexts;         // directories + named
    CFArrayRef         repositoryKexts;  // all from directories (may include named)
    CFArrayRef         namedKexts;
    CFArrayRef         loadedKexts;
    
    struct stat kernelStatBuffer;
    struct stat firstFolderStatBuffer;
    Boolean     haveFolderMtime;
    Boolean     haveKernelMtime;
    Boolean     compress;
    Boolean     uncompress;
} KextcacheArgs;

#pragma mark Function Prototypes
/*******************************************************************************
* Function Prototypes
*******************************************************************************/
ExitStatus readArgs(
    int            * argc,
    char * const  ** argv,
    KextcacheArgs  * toolArgs);
void setDefaultArchesIfNeeded(KextcacheArgs * toolArgs);
const NXArchInfo * addArchForName(
    KextcacheArgs * toolArgs,
    const char    * archname);
ExitStatus readPrelinkedKernelArgs(
    KextcacheArgs * toolArgs,
    int             argc,
    char * const  * argv,
    Boolean         isLongopt);
ExitStatus setPrelinkedKernelArgs(
    KextcacheArgs * toolArgs,
    char          * filename);
Boolean setDefaultKernel(KextcacheArgs * toolArgs);
Boolean setDefaultPrelinkedKernel(KextcacheArgs * toolArgs);
void setSystemExtensionsFolders(KextcacheArgs * toolArgs);

void checkKextdSpawnedFilter(Boolean kernelFlag);
ExitStatus checkArgs(KextcacheArgs * toolArgs);
ExitStatus statURL(CFURLRef anURL, struct stat * statBuffer);
ExitStatus statPath(const char *path, struct stat *statBuffer);

ExitStatus getLoadedKextInfo(KextcacheArgs *toolArgs);
ExitStatus updateSystemPlistCaches(KextcacheArgs * toolArgs);
ExitStatus updateDirectoryCaches(
    KextcacheArgs * toolArgs,
    CFURLRef folderURL);
ExitStatus createMkext(
    KextcacheArgs * toolArgs,
    Boolean       * fatalOut);
ExitStatus filterKextsForCache(
    KextcacheArgs     * toolArgs,
    CFMutableArrayRef   kextArray,
    const NXArchInfo  * arch,
    Boolean           * fatalOut);
Boolean checkKextForArchive(
    KextcacheArgs       toolArgs,
    OSKextRef           aKext,
    const char        * archiveTypeName,
    const NXArchInfo  * archInfo,
    OSKextRequiredFlags requiredFlags);
Boolean kextMatchesFilter(
    KextcacheArgs             * toolArgs,
    OSKextRef                   theKext,
    OSKextRequiredFlags         requiredFlags);
ExitStatus getFilePathTimes(
    const char        * filePath,
    struct timeval      cacheFileTimes[2]);
ExitStatus getFileURLModTimePlusOne(
    CFURLRef            fileURL,
    struct stat       * origStatBuffer,
    struct timeval      cacheFileTimes[2]);
ExitStatus getFilePathModTimePlusOne(
    const char        * filePath,
    struct stat       * origStatBuffer,
    struct timeval      cacheFileTimes[2]);
ExitStatus getModTimePlusOne(
    const char        * path,
    struct stat       * origStatBuffer,
    struct stat       * newStatBuffer,
    struct timeval      cacheFileTimes[2]);
Boolean kextMatchesLoadedKextInfo(
    KextcacheArgs     * toolArgs,
    OSKextRef           theKext);
ExitStatus createPrelinkedKernelArchs(
    KextcacheArgs     * toolArgs,
    CFMutableArrayRef * prelinkArchsOut);
ExitStatus createExistingPrelinkedSlices(
    KextcacheArgs     * toolArgs,
    CFMutableArrayRef * prelinkedSlicesOut,
    CFMutableArrayRef * prelinkedArchsOut);
ExitStatus createPrelinkedKernel(
    KextcacheArgs     * toolArgs);
CFArrayRef mergeArchs(
    CFArrayRef  archSet1,
    CFArrayRef  archSet2);
ExitStatus createPrelinkedKernelForArch(
    KextcacheArgs       * toolArgs,
    CFDataRef           * prelinkedKernelOut,
    CFDictionaryRef     * prelinkedSymbolsOut,
    const NXArchInfo    * archInfo);
ExitStatus getExpectedPrelinkedKernelModTime(
    KextcacheArgs  * toolArgs,
    struct timeval   cacheFileTimes[2],
    Boolean        * updateModTimeOut);
ExitStatus compressPrelinkedKernel(
    const char        * prelinkedKernelPath,
    Boolean             compress);

void usage(UsageLevel usageLevel);

#endif
