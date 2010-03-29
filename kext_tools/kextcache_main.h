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
#define kOptNameOmitLinkState           "omit-link-state"

/* Misc. cache update flags.
 */
#define kOptNameSystemCaches            "system-caches"

/* Boot!=root flags.
 */
#define kOptNameUpdate                  "update-volume"
#define kOptNameForce                   "force"

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
#define kOptForce                 'f'

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
#define kOptUpdate                'u'
#define kOptCheckUpdate           'U'
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
#define kLongOptOmitLinkState            (-12)

#define kOptChars                ":a:b:c:efFhkK:lLm:nNqrsStu:U:vz"
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
    { kOptNameOmitLinkState,         no_argument,        &longopt, kLongOptOmitLinkState },

    { kOptNameUpdate,                required_argument,  NULL,     kOptUpdate },
    { kOptNameForce,                 no_argument,        NULL,     kOptForce },

    { kOptNameNoAuthentication,      no_argument,        NULL,     kOptNoAuthentication },
    { kOptNameTests,                 no_argument,        NULL,     kOptTests },

    { NULL,                          required_argument,  NULL,     kOptCheckUpdate },
    { NULL,                          no_argument,        NULL,     kOptLowPriorityFork },

    { NULL, 0, NULL, 0 }  // sentinel to terminate list
};


#define kMaxNumArches  (64)
#define kDefaultKernelFile  "/mach_kernel"

typedef struct {
    OSKextRequiredFlags requiredFlagsRepositoriesOnly;  // -l/-n/-s
    OSKextRequiredFlags requiredFlagsAll;              // -L/-N/-S

    Boolean   updateSystemMkext;    // -e
    Boolean   updateSystemCaches;   // -system-caches
    Boolean   lowPriorityFlag;      // -F
    Boolean   printTestResults;     // -t
    Boolean   skipAuthentication;   // -z

    CFURLRef  volumeRootURL;        // for mkext/prelinked kernel

    CFURLRef  mkextURL;             // mkext option arg
    int       mkextVersion;         // -mkext1/-mkext2  0 (no mkext, 1 (old format),
                                    // or 2 (new format)

    CFURLRef  prelinkedKernelURL;              // -c option; if 
    Boolean   needDefaultPrelinkedKernelInfo;  // -c option w/o arg; 
                                               // prelinkedKernelURL is parent
                                               // directory of final kernelcache
                                               // until we create the cache
    Boolean   needLoadedKextInfo;           // -r option
    Boolean   prelinkedKernelErrorRequired;
    Boolean   generatePrelinkedSymbols;     // -symbols option
    Boolean   includeAllPersonalities;      // --all-personalities option
    Boolean   omitLinkState;                // --omit-link-state option
    CFURLRef  compressedPrelinkedKernelURL; // -uncompress option

    CFURLRef  updateVolumeURL;  // -u/-U options
    Boolean   expectUpToDate;   // -U
    Boolean   forceUpdateFlag;  // -f

    CFURLRef  kernelURL;        // overriden by -k option
    CFDataRef kernelFile;       // contents of kernelURL
    CFURLRef  symbolDirURL;     // -s option;

    const NXArchInfo * archInfo[kMaxNumArches]; // -a/-arch
    uint32_t           numArches;
    Boolean            explicitArch;  // user-provided instead of inferred host arches

    CFMutableSetRef    kextIDs;          // -b; must release
    CFMutableArrayRef  argURLs;          // directories & kexts in order
    CFMutableArrayRef  repositoryURLs;   // just non-kext directories
    CFMutableArrayRef  namedKextURLs;

    CFArrayRef         allKexts;         // directories + named
    CFArrayRef         repositoryKexts;  // all from directories (may include named)
    CFArrayRef         namedKexts;
    
    struct stat kernelStatBuffer;
    struct stat firstFolderStatBuffer;
    Boolean     haveFolderMtime;
    Boolean     compress;
    Boolean     uncompress;
} KextcacheArgs;

#define PLATFORM_NAME_LEN 64
#define ROOT_PATH_LEN 256

typedef struct prelinked_kernel_header {
    uint32_t  signature;
    uint32_t  compressType;
    uint32_t  adler32;
    uint32_t  uncompressedSize;
    uint32_t  compressedSize;
    uint32_t  reserved[11];
    char      platformName[PLATFORM_NAME_LEN];
    char      rootPath[ROOT_PATH_LEN];
    char      data[0];
} PrelinkedKernelHeader;

typedef struct platform_info {
    char platformName[PLATFORM_NAME_LEN];
    char rootPath[ROOT_PATH_LEN];
} PlatformInfo;


#pragma mark Function Prototypes
/*******************************************************************************
* Function Prototypes
*******************************************************************************/
ExitStatus readArgs(
    int            * argc,
    char * const  ** argv,
    KextcacheArgs  * toolArgs);
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
void setNeededLoadedKextInfo(KextcacheArgs * toolArgs);

void checkKextdSpawnedFilter(Boolean kernelFlag);
ExitStatus checkArgs(KextcacheArgs * toolArgs);
ExitStatus statURL(CFURLRef anURL, struct stat * statBuffer);

ExitStatus updateSystemDirectoryCaches(
    KextcacheArgs * toolArgs);
ExitStatus updateDirectoryCaches(
    KextcacheArgs * toolArgs,
    CFURLRef folderURL);

ExitStatus createMkext(
    KextcacheArgs * toolArgs,
    Boolean       * fatalOut);
ExitStatus filterKextsForMkext(
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
ExitStatus writeToFile(
    int           fileDescriptor,
    const UInt8 * data,
    CFIndex       length);

int getFileOffsetAndSizeForArch(
    int                 fileDescriptor,
    const NXArchInfo  * archInfo,
    off_t             * sliceOffsetOut,
    size_t            * sliceSizeOut);
int readFileAtOffset(
    int             fileDescriptor,
    off_t           fileOffset,
    size_t          fileSize,
    u_char        * buf);
int verifyMachOIsArch(
    u_char            * fileBuf,
    size_t              size,
    const NXArchInfo * archInfo);
void getPlatformInfo(
    KextcacheArgs * toolArgs,
    PlatformInfo  * platformInfo);
CFDataRef readMachOFileWithURL(
    CFURLRef kernelURL, 
    const NXArchInfo * archInfo);
CFDataRef uncompressPrelinkedKernel(
    CFDataRef prelinkImage);
CFDataRef compressPrelinkedKernel(
    CFDataRef prelinkImage,
    PlatformInfo *platformInfo);
ExitStatus createPrelinkedKernel(
    KextcacheArgs       * toolArgs,
    CFDataRef           * prelinkedKernelOut,
    CFDictionaryRef      * prelinkedSymbolsOut,
    const NXArchInfo    * archInfo,
    PlatformInfo        * platformInfo);
ExitStatus savePrelinkedKernel(
    KextcacheArgs       * toolArgs,
    CFDataRef             prelinkedKernel,
    CFDictionaryRef       prelinkedSymbols);
void usage(UsageLevel usageLevel);

#endif /* _KEXTCACHE_MAIN_H */
