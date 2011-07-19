/*
 * Copyright (c) 2008 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 * 
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFBundlePriv.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFSerialize.h>
#include <IOKit/IOCFUnserialize.h>
#include <IOKit/IOKitServer.h>

#ifndef IOKIT_EMBEDDED
#include <kxld.h>
#endif

#include <System/libkern/mkext.h>
#include <System/libkern/kext_request_keys.h>
#include <System/libkern/prelink.h>
#include <System/libkern/OSKextLibPrivate.h>
#include <Kernel/mach/vm_param.h>

#include <fcntl.h>
#include <libc.h>
#include <pthread.h>
#include <mach/host_priv.h>
#include <zlib.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <uuid/uuid.h>

#include "OSKext.h"
#include "OSKextPrivate.h"
#include "printPList_new.h"
#include "fat_util.h"
#include "macho_util.h"
#include "misc_util.h"

#define SHARED_EXECUTABLE 1

#pragma mark Notes
/*********************************************************************
**********************************************************************
Notes
======================================================================
Each kext instance will be stored in a non-retaining dict, sKextsByIdentifier:
  - bundle-id -> matching kext, if there is only one, else
                 array of matching kexts, in reverse version order
----------------------------------------------------------------------
Any time a kext is created, all kexts in the search path for the same bundle
ID will be read. This is for sanity, and we don't expect to see duplicates
much if at all, so there shouldn't be
**********************************************************************
*********************************************************************/

#pragma mark OSKext Data Structures
/*********************************************************************
* OSKext Data Structures
*********************************************************************/

typedef struct __OSKextLoadInfo {
   /* Used whenever a dependency graph is needed (generating an mkext,
    * prelinked kernel, or linking/loading).
    */
    CFMutableArrayRef dependencies;    // may have some missing

   /* These are used when checking the kernel for loaded kexts,
    * or when loading/generating symbols from user space.
    */
    CFDictionaryRef   kernelLoadInfo;   // for lazy eval, cleared when we check
    uint32_t          loadTag;
    uint64_t          loadAddress;      // 64-bit for max coverage
    uint64_t          sourceAddress;    // For prelinking: where it starts in memory
    size_t            headerSize;       // xxx - needed?
    size_t            loadSize;         // xxx - haxx; do we need wiredSize?

   /* These only exist while loading from user space.
    */
    CFURLRef          executableURL;
    CFDataRef         executable;
    CFDataRef         linkedExecutable;
    CFDataRef         prelinkedExecutable;
    kmod_info_t     * kmod_info;
    uint64_t          kmodInfoAddress;
    uint64_t          linkStateAddress;
    
    struct {
        unsigned int  hasRawKernelDependency:1;
        unsigned int  hasKernelDependency:1;
        unsigned int  hasKPIDependency:1;
        unsigned int  hasPrivateKPIDependency:1;

        unsigned int  hasAllDependencies:1;
        unsigned int  dependenciesValid:1;
        unsigned int  dependenciesAuthentic:1;

        unsigned int  isLoaded:1;
        unsigned int  isStarted:1;
        unsigned int  otherCFBundleVersionIsLoaded:1;
        unsigned int  otherUUIDIsLoaded:1; // otherVersion is also set if this is
    } flags;
} __OSKextLoadInfo;

typedef struct __OSKextMkextInfo {
    CFURLRef               mkextURL;
    CFDataRef              mkextData;  // the whole mkext file!
    CFDataRef              executable;
    CFMutableDictionaryRef resources;
} __OSKextMkextInfo;

/*****
 * Any failed diagnotic tests get their results put in one of these
 * dictionaries. See the header file for what keys and values
 * go in them. If the library does not perform full tests, then the
 * first failure encountered will cease testing and any of
 * these dictionaries will have exactly one entry. If the library
 * does perform full tests, then as many errors as are found will
 * be in each dictionary.
 */
typedef struct __OSKextDiagnostics {
    CFMutableDictionaryRef validationFailures;
    CFMutableDictionaryRef authenticationFailures;
    CFMutableDictionaryRef dependencyFailures; // whether direct or indirect!
    CFMutableDictionaryRef warnings;
    CFMutableDictionaryRef bootLevel;
} __OSKextDiagnostics;

typedef struct __OSKext {

   /* base CFType information. */
    CFRuntimeBase         cfBase;

   /* Read/retained at creation time. */
    CFURLRef              bundleURL;
    CFStringRef           bundleID;


   /* Read by __OSKextProcessInfoDictionary(). */
    OSKextVersion         version;
    OSKextVersion         compatibleVersion;

   /* May be flushed, may need to reload from disk.
    */
    CFDictionaryRef       infoDictionary;  // read with IOCFUnserialize()

   /* Allocated and maintained as necessary. */
    __OSKextDiagnostics * diagnostics;
    __OSKextLoadInfo    * loadInfo;
    __OSKextMkextInfo   * mkextInfo;

    struct {
        unsigned int      isPluginChecked:1;
        unsigned int      isPlugin:1;

        unsigned int      isFromIdentifierCache:1; // must __OSKextRealize on access
        unsigned int      isFromMkext:1;      // i.e. *not* to be updated from bundleURL
    } staticFlags;

    struct {
       /* Set by __OSKextProcessInfoDictionary() */
        unsigned int      isKernelComponent:1;
        unsigned int      isInterface:1;
        unsigned int      declaresExecutable:1;
        unsigned int      loggingEnabled:1;
        unsigned int      plistHasEnableLoggingSet:1;
        unsigned int      plistHasIOKitDebugFlags:1;
        unsigned int      isLoadableInSafeBoot:1;

       /* Set as determined or on demand. */
        unsigned int      validated:1;  // all possible checks done
        unsigned int      invalid:1;       // at least 1 failure, or fully validated
        unsigned int      valid:1;         // all possible checks done & passed

        unsigned int      authenticated:1; // all possible checks done
        unsigned int      inauthentic:1;   // at least 1 failure, or all ok
        unsigned int      authentic:1;     // should we ever cache this?

        unsigned int      hasIOKitDebugProperty:1;
        unsigned int      warnForMismatchedKmodInfo:1;
    } flags;

} __OSKext, * __OSKextRef;

#pragma mark Internal Constants and Enums
/*********************************************************************
* Internal Constants and Enums
*********************************************************************/

#define __sOSKextFullBundleExtension     ".kext/"
#define __kDSStoreFilename               CFSTR(".DS_Store")

#define __kOSKextKernelIdentifier        CFSTR("__kernel__")
#define __kOSKextUnknownIdentifier       "__unknown__"

#define __kOSKextApplePrefix             CFSTR("com.apple.")
#define __kOSKextKernelLibBundleID       CFSTR("com.apple.kernel")
#define __kOSKextKernelLibPrefix         CFSTR("com.apple.kernel.")
#define __kOSKextKPIPrefix               CFSTR("com.apple.kpi.")
#define __kOSKextCompatibilityBundleID   "com.apple.kernel.6.0"
#define __kOSKextPrivateKPI              CFSTR("com.apple.kpi.private")

/* Used when generating symbols.
 */
#define __kOSKextSymbolFileSuffix        "sym"

/* Used to validate a kext executable.
 */
#define __kOSKextKmodInfoSymbol         "_kmod_info"

#define __kStringUnknown                 "(unknown)"

#define __kOSKextMaxKextDisplacementLP64 (2 * 1024 * 1024 * 1024ULL)

/*********************************************************************
 * Kext Cache Stuff
*********************************************************************/
#define __kOSKextIdentifierCacheBasePathKey    "OSKextIdentifierCacheBasePath"
#define __kOSKextIdentifierCacheKextInfoKey    "OSKextIdentifierCacheKextInfo"
#define __kOSKextIdentifierCacheVersionKey     "OSKextIdentifierCacheVersion"
#define __kOSKextIdentifierCacheCurrentVersion (1)


#pragma mark Module Internal Variables
/*********************************************************************
* Module Internal Variables
*********************************************************************/

static pthread_once_t __sOSKextInitialized  = PTHREAD_ONCE_INIT;
static Boolean        __sOSKextInitializing = false;

/* Internal lookup collections.
 * Created the first time any kext is; all functions that access should
 * check for existence first!
 *
 * Values are NOT retained.
 */
static CFMutableArrayRef      __sOSAllKexts                = NULL;
static CFMutableDictionaryRef __sOSKextsByURL              = NULL;
static CFMutableDictionaryRef __sOSKextsByIdentifier       = NULL;

/* The default log flags result in errors and the special explicit
 * messages going out, and that's about it.
 */
static OSKextLogSpec          __sUserLogFilter             = kOSKextLogWarningLevel |
                                                             kOSKextLogVerboseFlagsMask;
static OSKextLogSpec          __sKernelLogFilter           = kOSKextLogWarningLevel |
                                                             kOSKextLogVerboseFlagsMask;

static const NXArchInfo       __sOSKextUnknownArchInfo         = {
    .name        = "unknown",
    .cputype     = CPU_TYPE_ANY,
    .cpusubtype  = CPU_SUBTYPE_MULTIPLE,
    .byteorder   = NX_UnknownByteOrder,
    .description = "unknown CPU architecture",
};
static const NXArchInfo     * __sOSKextArchInfo                     = &__sOSKextUnknownArchInfo;
static Boolean                __sOSKextSimulatedSafeBoot           = FALSE;
static Boolean                __sOSKextUsesCaches                  = TRUE;
static Boolean                __sOSKextStrictRecordingByLastOpened = FALSE;

static CFArrayRef             __sOSKextPackageTypeValues       = NULL;
static CFArrayRef             __sOSKextOSBundleRequiredValues  = NULL;

static OSKextDiagnosticsFlags __sOSKextRecordsDiagnositcs      = kOSKextDiagnosticsFlagNone;

// xxx - need a lock for thread safety

static OSKextVersion          __sOSNewKmodInfoKernelVersion = -1;

/* These are function protos but we need them ahead of their
 * references.
 */
void __sOSKextDefaultLogFunction(
    OSKextRef       aKext,
    OSKextLogSpec   msgLogSpec,
    const char     * format, ...);
void __OSKextLogKernelMessages(
    OSKextRef aKext,
    CFTypeRef kernelMessages);

void (*__sOSKextLogOutputFunction)(
    OSKextRef       aKext,
    OSKextLogSpec   msgLogSpec,
    const char    * format, ...) =
        &__sOSKextDefaultLogFunction;

static const char * safe_mach_error_string(mach_error_t error_code);

#pragma mark External Variables and Constants
/*********************************************************************
* External Constants and Enums
*********************************************************************/
static CFArrayRef        __sOSKextSystemExtensionsFolderURLs = NULL;
static CFArrayRef        __sOSKextInfoEssentialKeys          = NULL;
// xxx - This set is all except OSBundleExecutablePath, OSBundleMachOHeaders, and OSBundleClasses.

const char * kOSKextLoadNotification   = "com.apple.kext.load";
const char * kOSKextUnloadNotification = "com.apple.kext.unload";

#pragma mark -
/********************************************************************/
#pragma mark Diagnostic Keys and Values
/********************************************************************/
const CFStringRef kOSKextDiagnosticsValidationKey =
                  CFSTR("Validation Failures");
const CFStringRef kOSKextDiagnosticsAuthenticationKey =
                  CFSTR("Authentication Failures");
const CFStringRef kOSKextDiagnosticsDependenciesKey =
                  CFSTR("Dependency Resolution Failures");
const CFStringRef kOSKextDiagnosticsWarningsKey =
                  CFSTR("Warnings");
const CFStringRef kOSKextDiagnosticsBootLevelKey =
                  CFSTR("Boot Level Restrictions");

#pragma mark Combo validation/authentication diagnostic strings
/* Combo validation/authentication diagnostic strings.
 */
const CFStringRef kOSKextDiagnosticURLConversionKey =
                  CFSTR("Internal error converting URL");
const CFStringRef kOSKextDiagnosticFileNotFoundKey =
                  CFSTR("File not found");
const CFStringRef kOSKextDiagnosticStatFailureKey =
                  CFSTR("Failed to get file info (stat failed)");
const CFStringRef kOSKextDiagnosticFileAccessKey =
                  CFSTR("File access failure; can't open, or I/O error");

/* Validation diagnostic strings.
 */
const CFStringRef kOSKextDiagnosticNotABundleKey =
                  CFSTR("Failed to open CFBundle (unknown error).");
const CFStringRef kOSKextDiagnosticBadPropertyListXMLKey =
                  CFSTR("Can't parse info dictionary XML");
const CFStringRef kOSKextDiagnosticMissingPropertyKey =
                  CFSTR("Info dictionary missing required property/value");
const CFStringRef kOSKextDiagnosticBadSystemPropertyKey =
                  CFSTR("A system kext has a property set that it shouldn't");
const CFStringRef kOSKextDiagnosticPropertyIsIllegalTypeKey =
                  CFSTR("Info dictionary property value is of illegal type");
const CFStringRef kOSKextDiagnosticPropertyIsIllegalValueKey =
                  CFSTR("Info dictionary property value is illegal");
const CFStringRef kOSKextDiagnosticIdentifierOrVersionTooLongKey =
                  CFSTR("CFBundleIdentifier and CFBundleVersion must be < 64 characters.");
const CFStringRef kOSKextDiagnosticExecutableMissingKey =
                  CFSTR("Kext has a CFBundleExecutable property but the executable can't be found");
#if SHARED_EXECUTABLE
const CFStringRef kOSKextDiagnosticSharedExecutableKextMissingKey =
                  CFSTR("Kext claims a shared executable with named kext, "
                  "but that kext can't be found");
const CFStringRef kOSKextDiagnosticSharedExecutableAndExecutableKey =
                  CFSTR("Kext declares both CFBundleExecutable and "
                  "CFBundleSharedExecutableIdentifier; use only one.");
#endif /* SHARED_EXECUTABLE */
const CFStringRef kOSKextDiagnosticCompatibleVersionLaterThanVersionKey =
                  CFSTR("Compatible version must be lower than current version.");
const CFStringRef kOSKextDiagnosticExecutableBadKey =
                  CFSTR("Executable file doesn't contain kernel extension code "
                  "(no kmod_info symbol or bad Mach-O layout).");

/* Authentication diagnostic strings.
 */
const CFStringRef kOSKextDiagnosticNoFileKey =
                  CFSTR("Kext was not created from an URL and can't be authenticated");
const CFStringRef kOSKextDiagnosticOwnerPermissionKey =
                  CFSTR("File owner/permissions are incorrect "
                  "(must be root:wheel, nonwritable by group/other)");

/* Warning diagnostic strings.
 */
const CFStringRef kOSKextDiagnosticTypeWarningKey =
                  CFSTR("Info dictionary property value is of incorrect type");

const CFStringRef kOSKextDiagnosticKernelComponentNotInterfaceKey =
                  CFSTR("Kext is a kernel component but OSBundleIsInterface "
                  "is set to false; overriding");

const CFStringRef kOSKextDiagnosticExecutableArchNotFoundKey =
                  CFSTR("Executable does not contain code for architecture");

const CFStringRef kOSKextDiagnosticSymlinkKey =
                  CFSTR("The booter does not recognize symbolic links; "
                  "confirm these files/directories aren't needed for startup");

const CFStringRef kOSKextDiagnosticDeprecatedPropertyKey =
                  CFSTR("Deprecated property (ignored)");
const CFStringRef kOSKextDiagnosticPersonalityHasNoBundleIdentifierKey =
                  CFSTR("Personality has no CFBundleIdentifier; "
                  "the kext's identifier will be inserted when sending to the IOCatalogue");

const CFStringRef kOSKextDiagnosticPersonalityNamesUnknownKextKey =
                  CFSTR("Personality CFBundleIdentifier names a kext that "
                  "can't be found");
const CFStringRef kOSKextDiagnosticPersonalityNamesNonloadableKextKey =
                  CFSTR("Personality CFBundleIdentifier names a kext that "
                  "is not loadable (run kextutil(8) on it with -nt for more information)");
const CFStringRef kOSKextDiagnosticPersonalityNamesKextWithNoExecutableKey =
                  CFSTR("Personality CFBundleIdentifier names a kext that "
                  "doesn't declare an executable");
const CFStringRef kOSKextDiagnosticPersonalityHasDifferentBundleIdentifierKey =
                  CFSTR("Personality CFBundleIdentifier differs from "
                  "containing kext's (not necessarily a mistake, but rarely done)");
const CFStringRef kOSKextDiagnosticNonuniqueIOResourcesMatchKey =
                  CFSTR("Personality matches on IOResources "
                  "but IOMatchCategory is missing or not equal to its IOClass; "
                  "driver may be blocked from matching or may block others");

const CFStringRef kOSKextDiagnosticCodelessWithLibrariesKey =
                  CFSTR("Kext has no executable or compatible version, "
                  "so it should not declare any OSBundleLibraries.");
const CFStringRef kOSKextDiagnosticNoExplicitKernelDependencyKey =
                  CFSTR("Kext declares no kernel/KPI libraries; "
                  "if it references any kernel symbols, it may fail to link.");
const CFStringRef kOSKextDiagnosticDeclaresNoKPIsWarningKey =
                  CFSTR("Kext declares no com.apple.kpi.* libraries; "
                  "if it references any kernel symbols, it may fail to link.");
const CFStringRef kOSKextDiagnosticDeclaresBothKernelAndKPIDependenciesKey =
                  CFSTR("Kexts should declare dependencies on either "
                  "com.apple.kernel* or com.apple.kpi.* libraries, not both.");
const CFStringRef kOSKextDiagnosticBundleIdentifierMismatchKey =
                  CFSTR("Kexts with a kernel library < v6.0 must set MODULE_NAME "
                  "the same as CFBundleIdentifier to load on kernel < v6.0.");
const CFStringRef kOSKextDiagnosticBundleVersionMismatchKey =
                  CFSTR("Kexts with a kernel library < v6.0 must set MODULE_VERSION "
                  "the same as CFBundleVersion to load on kernel < v6.0.");

const CFStringRef kOSKextDiagnosticsDependencyNotOSBundleRequired =
                  CFSTR("Dependency lacks appropriate value for OSBundleRequired "
                  "and may not be availalble during early boot");

/* Dependency resolution diagnostic strings.
 */
const CFStringRef kOSKextDependencyUnavailable =
                  CFSTR("No kexts found for these libraries");
const CFStringRef kOSKextDependencyNoCompatibleVersion =
                  CFSTR("Only incompatible kexts found for these libraries");
const CFStringRef kOSKextDependencyCompatibleVersionUndeclared =
                  CFSTR("Kexts found for these libraries lack valid "
                  "OSBundleCompatibleVersion");
const CFStringRef kOSKextDependencyLoadedIsIncompatible =
                  CFSTR("Kexts already loaded for these libraries "
                  "are not compatible with the requested version");
const CFStringRef kOSKextDependencyLoadedCompatibleVersionUndeclared =
                  CFSTR("Kexts already loaded for these libraries "
                  "have no OSBundleCompatibleVersion");
const CFStringRef kOSKextDependencyIndirectDependencyUnresolvable =
                  CFSTR("Indirect dependencies can't be resolved");
const CFStringRef kOSKextDependencyMultipleVersionsDetected =
                  CFSTR("Multiple kexts for these libraries "
                  "occur in the dependency graph");
const CFStringRef kOSKextDependencyCircularReference =
                  CFSTR("Some dependencies are causing circular references");
const CFStringRef kOSKextDependencyRawAndComponentKernel =
                  CFSTR("Libraries declared for both "
                  "com.apple.kernel and a com.apple.kernel.* component.");
const CFStringRef kOSKextDependencyInvalid =
                  CFSTR("Dependencies have validation problems");
const CFStringRef kOSKextDependencyInauthentic =
                  CFSTR("Dependencies have incorrect owner/permissions");
const CFStringRef kOSKextDiagnosticDeclaresNonKPIDependenciesKey =
                  CFSTR("64-bit kexts must use com.apple.kpi.* libraries, "
                  "not com.apple.kernel* libraries.");
const CFStringRef kOSKextDiagnosticNonAppleKextDeclaresPrivateKPIDependencyKey =
                  CFSTR("Only Apple kexts may link against com.apple.kpi.private.");
const CFStringRef kOSKextDiagnosticRawKernelDependency =
                  CFSTR("Kexts may not link against com.apple.kernel; "
                  "use either com.apple.kpi.* libraries (recommended), "
                  "or com.apple.kernel.* (for compatiblity with older releases).");
const CFStringRef kOSKextDiagnosticsInterfaceDependencyCount =
                  CFSTR("Interface kext must have exactly one dependency.");

/* Boot-level (safe boot) diagnostic strings.
 */
const CFStringRef kOSKextDiagnosticIneligibleInSafeBoot =
                  CFSTR("Kext isn't loadable during safe boot.");
const CFStringRef kOSKextDependencyIneligibleInSafeBoot =
                  CFSTR("Dependencies aren't loadable during safe boot");

#pragma mark General Private Function Declarations
/*********************************************************************
* Private Function Declarations
*********************************************************************/
static void __OSKextInitialize(void);

static OSKextRef   __OSKextAlloc(
    CFAllocatorRef       allocator,
    CFAllocatorContext * context);
static void        __OSKextReleaseContents(CFTypeRef cf);
static CFStringRef __OSKextCopyDebugDescription(CFTypeRef cf);

Boolean __OSKextReadRegistryNumberProperty(
    io_registry_entry_t ioObject,
    CFStringRef         key,
    CFNumberType        numberType,
    void              * valuePtr);

static Boolean __OSKextIsArchitectureLP64(void);

static Boolean __OSKextInitWithURL(
    OSKextRef aKext,
    CFURLRef  anURL);
static Boolean __OSKextInitFromMkext(
    OSKextRef       aKext,
    CFDictionaryRef infoDict,
    CFURLRef        mkextURL,
    CFDataRef       mkextData);
static void    __OSKextReinit(OSKextRef aKext);

static Boolean __OSKextRecordKext(OSKextRef aKext);
static void __OSKextRemoveKext(OSKextRef aKext);
static Boolean __OSKextRecordKextInIdentifierDict(
    OSKextRef              aKext,
    CFMutableDictionaryRef identifierDict);
static void __OSKextRemoveKextFromIdentifierDict(
    OSKextRef              aKext,
    CFMutableDictionaryRef identifierDict);

static CFMutableArrayRef __OSKextCreateKextsFromURL(
    CFAllocatorRef allocator,
    CFURLRef       anURL,
    OSKextRef      aKext,  // if called by a kext looking for plugins
    Boolean        createPluginsFlag);
static CFMutableArrayRef __OSKextCreateKextsFromURLs(
    CFAllocatorRef allocator,
    CFArrayRef     arrayOfURLs,
    Boolean        createPluginsFlag);
    
OSKextRef __OSKextCreateFromIdentifierCacheDict(
    CFAllocatorRef  allocator,
    CFDictionaryRef cacheDict,
    CFStringRef     basePath,
    CFIndex         entryIndex);
void __OSKextRealize(const void * vKext, void * context __unused);
void __OSKextRealizeKextsWithIdentifier(CFStringRef kextIdentifier);

CFURLRef __OSKextCreateCacheFileURL(
    CFTypeRef            folderURLsOrURL,
    CFStringRef          cacheName,
    const NXArchInfo   * arch,
    _OSKextCacheFormat   format);
Boolean __OSKextCacheNeedsUpdate(
    CFURLRef  cacheURL,
    CFTypeRef folderURLsOrURL);
Boolean _OSKextCreateFolderForCacheURL(CFURLRef cacheURL);
Boolean __OSKextURLIsSystemFolder(CFURLRef absURL);
Boolean __OSKextStatURL(
    CFURLRef      anURL,
    Boolean     * missingOut,
    struct stat * statOut);
Boolean __OSKextStatURLsOrURL(
    CFTypeRef     folderURLsOrURL,
    Boolean     * missingOut,
    struct stat * latestStatOut);

void __OSKextRemoveIdentifierCacheForKext(OSKextRef aKext);
CFDictionaryRef __OSKextCreateIdentifierCacheDict(
    OSKextRef   aKext,
    CFStringRef basePath);

static Boolean __OSKextGetFileSystemPath(
    OSKextRef aKext,
    CFURLRef  anURL,
    Boolean   resolveToBase,
    char    * pathBuffer);
CFStringRef __OSKextCreateCompositeKey(
    CFStringRef  baseKey,
    const char * auxKey);
static CFTypeRef __CFDictionaryGetValueForCompositeKey(
    CFDictionaryRef aDict,
    CFStringRef     baseKey,
    const char *    auxKey);

static Boolean __OSKextReadExecutable(OSKextRef aKext);

static Boolean __OSKextHasAllDependencies(OSKextRef aKext);
static Boolean __OSKextResolveDependencies(
    OSKextRef         aKext,
    OSKextRef         rootKext,
    CFMutableSetRef   resolvedSet,
    CFMutableArrayRef loopStack);
static Boolean __OSKextAddLinkDependencies(
    OSKextRef         aKext,
    CFMutableArrayRef linkDependencies,
    Boolean           needAllFlag,
    Boolean           bleedthroughFlag);

static Boolean __OSKextReadSymbolReferences(
    OSKextRef              aKext,
    CFMutableDictionaryRef symbols);
static Boolean __OSKextIsSearchableForSymbols(
    OSKextRef aKext,
    Boolean   nonKPIFlag,
    Boolean   allowUnsupportedFlag);
static Boolean __OSKextFindSymbols(
    OSKextRef              aKext,
    CFMutableDictionaryRef undefSymbols,
    CFMutableDictionaryRef onedefSymbols,
    CFMutableDictionaryRef multdefSymbols,
    CFMutableArrayRef      multdefLibs);

static CFMutableArrayRef __OSKextCopyDependenciesList(
    OSKextRef aKext,
    Boolean   needAllFlag,
    uint32_t  minDepth);

CFMutableDictionaryRef __OSKextCreateKextRequest(
    CFStringRef              predicateIn,
    CFTypeRef                bundleIdentifierIn,
    CFMutableDictionaryRef * argumentsOut);
OSReturn __OSKextSendKextRequest(
    OSKextRef       aKext,
    CFDictionaryRef kextRequest,
    CFTypeRef     * cfResponseOut,
    char         ** rawResponseOut,
    uint32_t      * rawResponseLengthOut);
static OSReturn __OSKextSimpleKextRequest(
    OSKextRef     aKext,
    CFStringRef   predicate,
    CFTypeRef   * cfResponseOut);
OSReturn __OSKextProcessKextRequestResults(
    OSKextRef       aKext,
    kern_return_t   mig_result,
    kern_return_t   op_result,
    char          * logInfoBuffer,
    uint32_t        logInfoLength);

static OSReturn __OSKextLoadWithArgsDict(
    OSKextRef       aKext,
    CFDictionaryRef loadArgsDict);
#ifndef IOKIT_EMBEDDED
static Boolean __OSKextGenerateDebugSymbols(
    OSKextRef                aKext,
    CFDataRef                kernelImage,
    uint64_t                 kernelLoadAddress,
    KXLDContext            * kxldContext,
    CFMutableDictionaryRef   symbols);

typedef struct __OSKextKXLDCallbackContext {
    OSKextRef   kext;
    uint64_t    kernelLoadAddress;
} __OSKextKXLDCallbackContext;
static kxld_addr_t __OSKextLinkAddressCallback(
    u_long              size,
    KXLDAllocateFlags * flags,
    void              * user_data);
static void __OSKextLoggingCallback(
    KXLDLogSubsystem    subsystem,
    KXLDLogLevel        level, 
    const char        * format, 
    va_list             argList,
    void              * user_data);
#endif /* !IOKIT_EMBEDDED */

OSReturn __OSKextRemovePersonalities(
    OSKextRef   aKext,
    CFStringRef aBundleID);

static void __OSKextProcessLoadInfo(
    const void * vKey __unused,
    const void * vValue,
          void * vContext __unused);
static void __OSKextCheckLoaded(OSKextRef aKext);

Boolean __OSKextSetLoadAddress(OSKextRef aKext, uint64_t address);
static Boolean __OSKextCreateLoadInfo(OSKextRef aKext);
static Boolean __OSKextCreateMkextInfo(OSKextRef aKext);
#ifndef IOKIT_EMBEDDED
static CFDataRef __OSKextCopyRunningKernelImage();
#endif /* !IOKIT_EMBEDDED */

static Boolean __OSKextIsValid(OSKextRef aKext);
static Boolean __OSKextValidate(OSKextRef aKext, CFMutableArrayRef propPath);
static Boolean __OSKextValidateExecutable(OSKextRef aKext);
static Boolean __OSKextAuthenticateURLRecursively(
    OSKextRef aKext,
    CFURLRef  anURL,
    CFURLRef  pluginsURL);

static CFDictionaryRef __OSKextCopyDiagnosticsDict(
    OSKextRef              aKext,
    OSKextDiagnosticsFlags type);
static CFMutableDictionaryRef __OSKextGetDiagnostics(OSKextRef aKext,
    OSKextDiagnosticsFlags type);
static void __OSKextSetDiagnostic(OSKextRef aKext,
    OSKextDiagnosticsFlags type, CFStringRef key);
static void __OSKextAddDiagnostic(OSKextRef aKext,
    OSKextDiagnosticsFlags type,
    CFStringRef            key,
    CFTypeRef              value,
    CFTypeRef              note);

static Boolean __OSKextCheckProperty(
    OSKextRef       aKext,
    CFDictionaryRef aDict,
    CFTypeRef       propKey,
    CFTypeRef       diagnosticValue, /* string or array of strings */
    CFTypeID        expectedType,
    CFArrayRef      legalValues,     /* NULL if not relevant */
    Boolean         required,
    Boolean         typeRequired,
    Boolean         nonnilRequired,
    CFTypeRef     * valueOut,
    Boolean       * valueIsNonnil);

static Boolean __OSKextReadInfoDictionary(
    OSKextRef aKext, CFBundleRef kextBundle);
static Boolean __OSKextProcessInfoDictionary(
    OSKextRef aKext, CFBundleRef kextBundle);

static Boolean __OSKextAddCompressedFileToMkext(
    OSKextRef        aKext,
    CFMutableDataRef mkextData,
    CFDataRef        fileData,
    Boolean          plistFlag,
    Boolean        * compressed);
static Boolean __OSKextAddToMkext(
    OSKextRef         aKext,
    CFMutableDataRef  mkextData,
    CFMutableArrayRef mkextInfoDictArray,
    char            * volumePath,
    Boolean           compressFlag);
static CFDataRef __OSKextCreateMkext(
    CFAllocatorRef      allocator,
    CFArrayRef          kextArray,
    CFURLRef            volumeRootURL,
    OSKextRequiredFlags requiredFlags,
    Boolean             compressFlag,
    Boolean             skipLoadedFlag,
    CFDictionaryRef     loadArgsDict);
static CFDataRef __OSKextUncompressMkext2FileData(
    CFAllocatorRef   allocator,
    const UInt8    * buffer,
    uint32_t        compressedSize,
    uint32_t        fullSize);
static CFArrayRef __OSKextCreateKextsFromMkext(
    CFAllocatorRef allocator,
    CFDataRef mkextData,
    CFURLRef  mkextURL);
static CFDataRef __OSKextExtractMkext2FileEntry(
    OSKextRef   aKext,
    CFDataRef   mkextData,
    CFNumberRef offsetNum,
    CFStringRef filename);  // null for executable

#ifndef IOKIT_EMBEDDED
static boolean_t __OSKextSwapHeaders(
    CFDataRef kernelImage);
static boolean_t __OSKextUnswapHeaders(
    CFDataRef kernelImage);
static boolean_t __OSKextGetLastKernelLoadAddr(
    CFDataRef kernelImage, 
    uint64_t *lastLoadAddrOut);
static boolean_t __OSKextGetSegmentAddressAndOffset(
    CFDataRef kernelImage, 
    const char *segname, 
    uint32_t *fileOffsetOut, 
    uint64_t *loadAddrOut);
static boolean_t __OSKextGetSegmentFileAndVMSize(
    CFDataRef kernelImage, 
    const char *segname, 
    uint64_t *fileSizeOut, 
    uint64_t *VMSizeOut);
static boolean_t __OSKextSetSegmentAddress(
    CFDataRef kernelImage, 
    const char *segname, 
    uint64_t loadAddr);
static boolean_t __OSKextSetSegmentVMSize(
    CFDataRef kernelImage, 
    const char *segname, 
    uint64_t vmsize);
static boolean_t __OSKextSetSegmentOffset(
    CFDataRef kernelImage, 
    const char *segname, 
    uint64_t fileOffset);
static boolean_t __OSKextSetSegmentFilesize(
    CFDataRef kernelImage, 
    const char *segname, 
    uint64_t filesize);
static boolean_t __OSKextSetSectionAddress(
    CFDataRef kernelImage, 
    const char *segname, 
    const char *sectname, 
    uint64_t loadAddr);
static boolean_t __OSKextSetSectionSize(
    CFDataRef kernelImage, 
    const char *segname, 
    const char *sectname, 
    uint64_t size);
static boolean_t __OSKextSetSectionOffset(
    CFDataRef kernelImage, 
    const char *segname, 
    const char *sectname, 
    uint32_t fileOffset);
static uint64_t __OSKextGetFakeLoadAddress(CFDataRef kernelImage);
static Boolean __OSKextCheckForPrelinkedKernel(
    OSKextRef aKext,
    Boolean   needAllFlag,
    Boolean   skipAuthenticationFlag,
    Boolean   printDiagnosticsFlag);
static CFArrayRef __OSKextPrelinkKexts(
    CFArrayRef        kextArray,
    CFDataRef         kernelImage,
    uint64_t          loadAddrBase,
    uint64_t          sourceAddrBase,
    KXLDContext     * kxldContext,
    u_long          * loadSizeOut,
    Boolean           needAllFlag,
    Boolean           skipAuthenticationFlag,
    Boolean           printDiagnosticsFlag,
    Boolean           stripSymbolsFlag);
static CFDataRef __OSKextCreatePrelinkInfoDictionary(
    CFArrayRef loadList,
    CFURLRef   volumeRootURL,
    Boolean includeAllPersonalities);
static Boolean __OSKextRequiredAtEarlyBoot(
    OSKextRef   theKext);
static u_long __OSKextCopyPrelinkedKexts(
    CFMutableDataRef prelinkImage,
    CFArrayRef loadList,
    u_long fileOffsetBase,
    uint64_t sourceAddrBase);
static u_long __OSKextCopyPrelinkInfoDictionary(
    CFMutableDataRef prelinkImage,
    CFDataRef prelinkInfoData,
    u_long fileOffset,
    uint64_t sourceAddr);
#endif /* !IOKIT_EMBEDDED */

CFComparisonResult __OSKextCompareIdentifiers(
    const void * val1,
    const void * val2,
    void       * context);
char * __absPathOnVolume(
    const char * path,
    const char * volumePath);
CFStringRef __OSKextCopyExecutableRelativePath(OSKextRef aKext);

static bool __OSKextShouldLog(
    OSKextRef        aKext,
    OSKextLogSpec    msgLogSpec);

#pragma mark Function-Like Macros
/*********************************************************************
* Function-Like Macros
*********************************************************************/

#pragma mark Core Foundation Class Functions
/*********************************************************************
* Core Foundation Class Definition Stuff
*********************************************************************/

/* This gets set by __OSKextInitialize().
 */
static CFTypeID __kOSKextTypeID = _kCFRuntimeNotATypeID;

CFTypeID OSKextGetTypeID(void) {
    return __kOSKextTypeID;
}

/*********************************************************************
*********************************************************************/
static const CFRuntimeClass __OSKextClass = {
    0,                            // version
    "OSKext",                     // className
    NULL,                         // init
    NULL,                         // copy
    __OSKextReleaseContents,      // finalize
    NULL,                         // equal: pointer equality, baby.
    NULL,                         // hash
    NULL,                         // copyFormattingDesc
    __OSKextCopyDebugDescription,  // copyDebugDesc
    NULL,                         // xxx - need to set reclaim field for garbage collection
    NULL
};

/*********************************************************************
*********************************************************************/
static void __OSKextInitialize(void)
{
    CFTypeRef packageTypeValues[] = { CFSTR("KEXT") };
    CFTypeRef bundleRequiredValues[] = {
        CFSTR(kOSBundleRequiredRoot),
        CFSTR(kOSBundleRequiredLocalRoot),
        CFSTR(kOSBundleRequiredNetworkRoot),
        CFSTR(kOSBundleRequiredSafeBoot),
        CFSTR(kOSBundleRequiredConsole),
    };
    CFTypeRef essentialInfoKeys[] = {
        kCFBundleIdentifierKey,
        kCFBundleVersionKey,
        CFSTR(kOSBundleCompatibleVersionKey),
        CFSTR(kOSBundleIsInterfaceKey),
        CFSTR(kOSKernelResourceKey),

        CFSTR(kOSBundleCPUTypeKey),
        CFSTR(kOSBundleCPUSubtypeKey),
        CFSTR(kOSBundlePathKey),
        CFSTR(kOSBundleUUIDKey),
        CFSTR(kOSBundleStartedKey),
        CFSTR(kOSBundleLoadTagKey),
        CFSTR(kOSBundleLoadAddressKey),
        CFSTR(kOSBundleLoadSizeKey),
        CFSTR(kOSBundleWiredSizeKey),
        CFSTR(kOSBundlePrelinkedKey),
        CFSTR(kOSBundleDependenciesKey),
        CFSTR(kOSBundleRetainCountKey)
    };
    CFAllocatorContext nonrefcountAllocatorContext;
    CFAllocatorRef     nonrefcountAllocator = NULL;  // must release

    // must release each
    CFURLRef  systemFolders[_kOSKextNumSystemExtensionsFolders] = { 0, };

    int    numValues;

   /* Prevent deadlock when calling other functions that might think they
    * need to initialize.
    */
    __sOSKextInitializing = true;

    __kOSKextTypeID = _CFRuntimeRegisterClass(&__OSKextClass);

    CFAllocatorGetContext(kCFAllocatorDefault, &nonrefcountAllocatorContext);
    nonrefcountAllocatorContext.retain = NULL;
    nonrefcountAllocatorContext.release = NULL;
    nonrefcountAllocator = CFAllocatorCreate(kCFAllocatorDefault,
        &nonrefcountAllocatorContext);
    if (!nonrefcountAllocator) {
        OSKextLogMemError();
        goto finish;
    }
    systemFolders[0] = CFURLCreateFromFileSystemRepresentation(
        nonrefcountAllocator,
        (const UInt8 *)_kOSKextSystemLibraryExtensionsFolder,
        strlen(_kOSKextSystemLibraryExtensionsFolder),
        /* isDir */ true);
    __sOSKextSystemExtensionsFolderURLs = CFArrayCreate(nonrefcountAllocator,
        (const void **)systemFolders,
        _kOSKextNumSystemExtensionsFolders,
        &kCFTypeArrayCallBacks);
    if (!systemFolders[0] || !__sOSKextSystemExtensionsFolderURLs) {
        OSKextLogMemError();
        goto finish;
    }

   /* Set up the the static arrays we use internally.
    */
    numValues = sizeof(packageTypeValues) / sizeof(void *);
    __sOSKextPackageTypeValues = CFArrayCreate(kCFAllocatorDefault,
        packageTypeValues, numValues, &kCFTypeArrayCallBacks);
    numValues = sizeof(bundleRequiredValues) / sizeof(void *);
    __sOSKextOSBundleRequiredValues = CFArrayCreate(kCFAllocatorDefault,
        bundleRequiredValues, numValues, &kCFTypeArrayCallBacks);

    numValues = sizeof(essentialInfoKeys) / sizeof(void *);
    __sOSKextInfoEssentialKeys = CFArrayCreate(kCFAllocatorDefault,
        essentialInfoKeys, numValues, &kCFTypeArrayCallBacks);

   /* This module keeps track of all open kexts by both URL and bundle ID,
    * in dictionaries that do not retain/release so that we do cleanup on
    * final client release.
    */
    if (!__sOSAllKexts) {
        CFArrayCallBacks nonrefcountValueCallBacks =
            kCFTypeArrayCallBacks;

        nonrefcountValueCallBacks.retain = NULL;
        nonrefcountValueCallBacks.release = NULL;
        __sOSAllKexts = CFArrayCreateMutable(kCFAllocatorDefault,
            0, &nonrefcountValueCallBacks);
        if (!__sOSAllKexts) {
            OSKextLogMemError();
            // xxx - what can we do? the program *will* crash pretty soon
            // xxx - CFBundle doesn't even bother to check, it will just crash
            goto finish;
        }
    }

   /* This module keeps track of all open kexts by both URL and bundle ID,
    * in dictionaries that do not retain/release so that we do cleanup on
    * final client release.
    */
    if (!__sOSKextsByURL) {
        CFDictionaryValueCallBacks nonrefcountValueCallBacks =
            kCFTypeDictionaryValueCallBacks;

        nonrefcountValueCallBacks.retain = NULL;
        nonrefcountValueCallBacks.release = NULL;
        __sOSKextsByURL = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks, &nonrefcountValueCallBacks);
        __sOSKextsByIdentifier = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks, &nonrefcountValueCallBacks);
        if (!__sOSKextsByURL || !__sOSKextsByIdentifier) {

            OSKextLogMemError();
            // xxx - what can we do? the program *will* crash pretty soon
            // xxx - CFBundle doesn't even bother to check, it will just crash
            goto finish;
        }
    }

   /* As of kernel version 6.0, we started stamping the bundle ID & version
    * into the kmod_info struct rather than relying on them to match the
    * info dictionary.
    *
    * xxx - Need to update that version #? Compare with original KX code.
    */
    __sOSNewKmodInfoKernelVersion = OSKextParseVersionString("6.0");

   /* Default to running kernel's arch by setting with NULL.
    */
    OSKextSetArchitecture(NULL);

finish:
    SAFE_RELEASE(nonrefcountAllocator);

    __sOSKextInitializing = false;
    return;
}

/*********************************************************************
*********************************************************************/
static OSKextRef __OSKextAlloc(
    CFAllocatorRef       allocator,
    CFAllocatorContext * context __unused)
{
    OSKextRef   result  = NULL;
    char      * offset  = NULL;
    UInt32      size;

    size  = sizeof(__OSKext) - sizeof(CFRuntimeBase);
    result = (OSKextRef)_CFRuntimeCreateInstance(allocator,
        __kOSKextTypeID, size, NULL);
    if (!result) {
        OSKextLogMemError();
        goto finish;
    }
    offset = (char *)result;
    bzero(offset + sizeof(CFRuntimeBase), size);

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
static void __OSKextReleaseContents(CFTypeRef cfObject)
{
    OSKextRef aKext = (OSKextRef)cfObject;

   /* Remove the kext from bookkeeping tables *before* releasing contents.
    */
    __OSKextRemoveKext(aKext);

    OSKextFlushDiagnostics(aKext, kOSKextDiagnosticsFlagAll);
    OSKextFlushLoadInfo(aKext, /* flushDependencies */ true);

    if (aKext->mkextInfo) {
        SAFE_RELEASE_NULL(aKext->mkextInfo->mkextURL);
        SAFE_RELEASE_NULL(aKext->mkextInfo->mkextData);
        SAFE_RELEASE_NULL(aKext->mkextInfo->executable);
        SAFE_RELEASE_NULL(aKext->mkextInfo->resources);
        SAFE_FREE_NULL(aKext->mkextInfo);
    }

   /* Release these bits last, the URL & identifier especially might get
    * referenced by functions above.
    */
    SAFE_RELEASE_NULL(aKext->bundleURL);
    SAFE_RELEASE_NULL(aKext->bundleID);
    SAFE_RELEASE_NULL(aKext->infoDictionary);

    return;
}

/*********************************************************************
*********************************************************************/
static CFStringRef __OSKextCopyDebugDescription(CFTypeRef cfObject)
{
    CFMutableStringRef result;
    OSKextRef          aKext = (OSKextRef)cfObject;
    CFAllocatorRef     allocator = CFGetAllocator(cfObject);
    CFStringRef        bundleID = NULL;   // do not release
    CFURLRef           mkextURL = NULL;   // do not release

    bundleID = OSKextGetIdentifier(aKext);

    result = CFStringCreateMutable(allocator, 0);
    if (!result) {
        OSKextLogMemError();
        goto finish;
    }
    CFStringAppendFormat(result, NULL, CFSTR("<OSKext %p [%p]> { "),
        cfObject, allocator);

    if (OSKextIsFromMkext(aKext)) {
        mkextURL = aKext->mkextInfo->mkextURL;
        CFStringAppendFormat(result, NULL, CFSTR("mkext URL = \"%@\", "),
            mkextURL ?(CFTypeRef)mkextURL : (CFTypeRef)CFSTR(__kStringUnknown));
        if (aKext->bundleURL) {
            CFStringAppendFormat(result, NULL, CFSTR("original URL = \"%@\", "),
                aKext->bundleURL);
        }
    } else if (aKext->bundleURL) {
        CFStringAppendFormat(result, NULL, CFSTR("URL = \"%@\", "),
            aKext->bundleURL);
    }
    CFStringAppendFormat(result, NULL, CFSTR("ID = \"%@\""),
        bundleID ? bundleID : CFSTR(__kStringUnknown));
    CFStringAppendFormat(result, NULL, CFSTR(" }"));

finish:
    return result;
}

#pragma mark Module Configuration
/*********************************************************************
* Module Configuration
*********************************************************************/

/*********************************************************************
*********************************************************************/
Boolean __OSKextReadRegistryNumberProperty(
    io_registry_entry_t   ioObject,
    CFStringRef           key,
    CFNumberType          numberType,
    void                * valuePtr)
{
    Boolean      result      = false;
    CFTypeRef    regObj      = NULL;  // must release
    CFNumberRef  numberObj   = NULL;  // do not release

    regObj = IORegistryEntryCreateCFProperty(ioObject,
        key, kCFAllocatorDefault, kNilOptions);
    if (!regObj || CFGetTypeID(regObj) != CFNumberGetTypeID()) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel |
            kOSKextLogGeneralFlag | kOSKextLogIPCFlag,
            "Can't read kernel CPU info from IORegistry (absent or wrong type).");
        goto finish;
    }
    numberObj = (CFNumberRef)regObj;
    result = CFNumberGetValue(numberObj, numberType, valuePtr);

finish:
    SAFE_RELEASE(regObj);
    return result;
}

/*********************************************************************
*********************************************************************/
const NXArchInfo * OSKextGetRunningKernelArchitecture(void)
{
    static const NXArchInfo * result           = &__sOSKextUnknownArchInfo;
    io_registry_entry_t       ioRegRoot        = IO_OBJECT_NULL;  // must IOObjectRelease()
    cpu_type_t                kernelCPUType    = CPU_TYPE_ANY;
    cpu_subtype_t             kernelCPUSubtype = CPU_SUBTYPE_MULTIPLE;

    if (result != &__sOSKextUnknownArchInfo) {
        goto finish;
    }

    /* 
     * XXX: This needs to be a runtime check and do the right thing based on
     * XXX: current OS (maybe check registry and fall back to cputype)?
     */
#if (MAC_OS_X_VERSION_MIN_REQUIRED >= 1060) || (__IPHONE_OS_VERSION_MIN_REQUIRED >= 50000)
    ioRegRoot = IORegistryGetRootEntry(kIOMasterPortDefault);
    if (ioRegRoot == IO_OBJECT_NULL) {
        goto finish;
    }

    if (!__OSKextReadRegistryNumberProperty(ioRegRoot,
        CFSTR(kOSKernelCPUTypeKey),
        kCFNumberSInt32Type,
        &kernelCPUType)) {

        goto finish;
    }
    if (!__OSKextReadRegistryNumberProperty(ioRegRoot,
        CFSTR(kOSKernelCPUSubtypeKey),
        kCFNumberSInt32Type,
        &kernelCPUSubtype)) {

        goto finish;
    }
#else
#pragma unused(ioRegRoot)
    size_t size;
    
    size = sizeof(kernelCPUType);
    if (sysctlbyname("hw.cputype", &kernelCPUType, &size, NULL, 0) != 0) {
        goto finish;
    }
    size = sizeof(kernelCPUSubtype);
    if (sysctlbyname("hw.cpusubtype", &kernelCPUSubtype, &size, NULL, 0) != 0) {
        goto finish;
    }
#endif

    result = NXGetArchInfoFromCpuType(kernelCPUType, kernelCPUSubtype);
    if (result) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogProgressLevel | kOSKextLogKextBookkeepingFlag,
            "Running kernel architecture is %s.", result->name);
    }

finish:
#if defined(MAC_OS_X_VERSION_10_6) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_6
    if (ioRegRoot) {
        IOObjectRelease(ioRegRoot);
    }
#endif
    if (!result) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel |
            kOSKextLogGeneralFlag | kOSKextLogIPCFlag,
            "Can't read running kernel architecture.");
    }
    return result;
}

/*********************************************************************
*********************************************************************/
Boolean OSKextSetArchitecture(const NXArchInfo * archInfo)
{
    Boolean            result      = true;
    const NXArchInfo * oldArchInfo = __sOSKextArchInfo;

       /* Prevent deadlock on lib initialization.
        */
    if (!__sOSKextInitializing) {
        pthread_once(&__sOSKextInitialized, __OSKextInitialize);
    }

    if (oldArchInfo && (oldArchInfo == archInfo)) {
        goto finish;
    }

   /* Set internal arch to NULL before we try to figure out the new one.
    */
    __sOSKextArchInfo = NULL;
    if (archInfo) {
        __sOSKextArchInfo = NXGetArchInfoFromCpuType(archInfo->cputype,
            archInfo->cpusubtype);
        if (!__sOSKextArchInfo) {
            if (archInfo->name && archInfo->name[0]) {
                OSKextLog(/* kext */ NULL,
                    kOSKextLogDebugLevel |
                    kOSKextLogGeneralFlag,
                    "Architecture %s not found by CPU info (type %d, subtype %d), "
                    "trying by name.",
                    archInfo->name, archInfo->cputype, archInfo->cpusubtype);

                __sOSKextArchInfo = NXGetArchInfoFromName(archInfo->name);
                if (!__sOSKextArchInfo) {
                    OSKextLog(/* kext */ NULL,
                        kOSKextLogDebugLevel |
                        kOSKextLogGeneralFlag,
                        "Architecture %s not found by name, "
                        "using running kernel architecture %s.",
                        archInfo->name,
                        OSKextGetRunningKernelArchitecture()->name);
                    result = false;
                }
            } else {
                OSKextLog(/* kext */ NULL,
                    kOSKextLogDebugLevel,
                    "Unknown CPU info given (type %d, subtype %d), "
                    "using running kernel architecture %s.",
                    archInfo->cputype, archInfo->cpusubtype,
                    OSKextGetRunningKernelArchitecture()->name);
                result = false;
            }
        }
    }

   /* If we didn't get one from the arg, get the running kernel's arch.
    * Failing that, set it back to the unknown record.
    */
    if (!__sOSKextArchInfo) {
        __sOSKextArchInfo = OSKextGetRunningKernelArchitecture();
    }
    if (!__sOSKextArchInfo) {
        __sOSKextArchInfo = &__sOSKextUnknownArchInfo;
    }

finish:

   /* Dump all load info and reinit kexts based on new arch.
    */
    if (oldArchInfo == __sOSKextArchInfo) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogDebugLevel | kOSKextLogGeneralFlag,
            "Kext library architecture is %s (unchanged).",
            __sOSKextArchInfo->name ? __sOSKextArchInfo->name : __kStringUnknown);
    } else {
        const char * auxMsg = "";

        if (!__sOSKextInitializing && __sOSAllKexts &&
            CFArrayGetCount(__sOSAllKexts)) {

            auxMsg = "; reinitializing all kexts for new architecture";
        }

        OSKextLog(/* kext */ NULL,
            kOSKextLogDetailLevel |
            kOSKextLogGeneralFlag | kOSKextLogKextBookkeepingFlag,
            "Kext library architecture set to %s%s.",
            __sOSKextArchInfo->name ? __sOSKextArchInfo->name : __kStringUnknown,
            auxMsg);

       /* Prevent deadlock on lib initialization.
        */
        if (!__sOSKextInitializing) {
            __OSKextReinit(NULL);
            OSKextFlushLoadInfo(NULL, /* flushDependencies */ true);
        }
    }
    return result;
}

/*********************************************************************
*********************************************************************/
const NXArchInfo * OSKextGetArchitecture(void)
{
    pthread_once(&__sOSKextInitialized, __OSKextInitialize);

    return __sOSKextArchInfo;
}

/*********************************************************************
*********************************************************************/
static Boolean __OSKextIsArchitectureLP64(void)
{
    return ((OSKextGetArchitecture()->cputype & CPU_ARCH_ABI64) != 0);
}

/*********************************************************************
*********************************************************************/
void OSKextSetLogFilter(
    OSKextLogSpec logFilter,
    Boolean       kernelFlag)
{
    OSKextLogSpec oldLogFilter;

       /* Save the old flags and set the new ones.
        */
    if (kernelFlag) {
        oldLogFilter = __sKernelLogFilter;
        __sKernelLogFilter = logFilter;
    } else {
        oldLogFilter = __sUserLogFilter;
        __sUserLogFilter = logFilter;
    }

    if (oldLogFilter != logFilter) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogDebugLevel |
            kOSKextLogGeneralFlag,
            "Kext %s-space log filter changed from 0x%x to 0x%x.",
            kernelFlag ? "kernel" : "user",
            oldLogFilter, kernelFlag ? __sKernelLogFilter : __sUserLogFilter);
    }

    return;
}

/*********************************************************************
*********************************************************************/
OSKextLogSpec OSKextGetLogFilter(Boolean kernelFlag)
{
    return kernelFlag ? __sKernelLogFilter : __sUserLogFilter;
}

/*********************************************************************
*********************************************************************/
void OSKextSetLogOutputFunction(OSKextLogOutputFunction func)
{
   /* Well now, how could we log this?
    * The log function itself is being changed!
    */
    __sOSKextLogOutputFunction = func;
    return;
}

/*********************************************************************
*********************************************************************/
void OSKextSetSimulatedSafeBoot(Boolean flag)
{
    Boolean oldSafeBootValue = __sOSKextSimulatedSafeBoot;

    OSKextLog(/* kext */ NULL,
        kOSKextLogDetailLevel | kOSKextLogKextBookkeepingFlag,
        "Kext library setting simulated safe boot mode to %s.",
        flag ? "true" : "false");

    __sOSKextSimulatedSafeBoot = flag;

    if (oldSafeBootValue != __sOSKextSimulatedSafeBoot) {
        __OSKextReinit(/* kext */ NULL);
    }

    return;
}

/*********************************************************************
*********************************************************************/
Boolean OSKextGetSimulatedSafeBoot(void)
{
    return __sOSKextSimulatedSafeBoot;
}

/*********************************************************************
*********************************************************************/
#define SYSCTL_MIB_LENGTH   (2)

Boolean OSKextGetActualSafeBoot(void)
{
    static Boolean result         = false;
    static Boolean gotIt          = false;
    int            kern_safe_boot = 0;
    size_t         length         = 0;
    int            mib_name[SYSCTL_MIB_LENGTH] = { CTL_KERN, KERN_SAFEBOOT };

    if (gotIt) {
        goto finish;
    }

    /* First check the kernel sysctl. */
    length = sizeof(kern_safe_boot);
    if (!sysctl(mib_name, SYSCTL_MIB_LENGTH,
        &kern_safe_boot, &length, NULL, 0)) {

        result = kern_safe_boot ? true : false;
        gotIt = true;
    } else {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel |
            kOSKextLogGeneralFlag | kOSKextLogIPCFlag,
            "Can't determine actual safe boot mode - "
            "sysctl() failed for KERN_SAFEBOOT - %s.",
            strerror(errno));
    }

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
CFArrayRef OSKextGetSystemExtensionsFolderURLs(void)
{
    pthread_once(&__sOSKextInitialized, __OSKextInitialize);

    return __sOSKextSystemExtensionsFolderURLs;
}

/*********************************************************************
*********************************************************************/
void OSKextSetRecordsDiagnostics(OSKextDiagnosticsFlags flags)
{
    OSKextLog(/* kext */ NULL,
        kOSKextLogDetailLevel |
        kOSKextLogValidationFlag | kOSKextLogAuthenticationFlag |
        kOSKextLogDependenciesFlag | kOSKextLogLoadFlag |
        kOSKextLogLinkFlag | kOSKextLogPatchFlag | kOSKextLogKextBookkeepingFlag,
        "Kext library recording diagnostics%s%s%s%s%s.",
        flags == kOSKextDiagnosticsFlagNone          ? " off"            : " for:",
        flags & kOSKextDiagnosticsFlagValidation     ? " validation"     : "",
        flags & kOSKextDiagnosticsFlagAuthentication ? " authentication" : "",
        flags & kOSKextDiagnosticsFlagDependencies   ? " dependencies"   : "",
        flags & kOSKextDiagnosticsFlagWarnings       ? " warnings"       : "");

    __sOSKextRecordsDiagnositcs = flags;
    return;
}

/*********************************************************************
*********************************************************************/
OSKextDiagnosticsFlags OSKextGetRecordsDiagnostics(void)
{
    return __sOSKextRecordsDiagnositcs;
}

/*********************************************************************
*********************************************************************/
void OSKextSetUsesCaches(Boolean flag)
{
    OSKextLog(/* kext */ NULL,
        kOSKextLogDetailLevel | kOSKextLogKextBookkeepingFlag,
        "Kext library %s using caches.", flag ? "now" : "not");

    __sOSKextUsesCaches = flag;
}

/*********************************************************************
*********************************************************************/
Boolean OSKextGetUsesCaches(void)
{
    return __sOSKextUsesCaches;
}

/*********************************************************************
*********************************************************************/
void _OSKextSetStrictRecordingByLastOpened(Boolean flag)
{
    __sOSKextStrictRecordingByLastOpened = flag;
    return;
}



#pragma mark Instance Management
/*********************************************************************
* Instance Management
*********************************************************************/

/*********************************************************************
*********************************************************************/
static Boolean __OSKextInitWithURL(
    OSKextRef aKext,
    CFURLRef  anURL)
{
    Boolean     result     = false;
    CFBundleRef kextBundle = NULL;  // must release
    char        urlPath[PATH_MAX];

    __OSKextGetFileSystemPath(/* kext */ NULL, /* otherURL */ anURL,
        /* resolveToBase */ true, urlPath);

    OSKextLog(aKext,
        kOSKextLogDebugLevel | kOSKextLogFileAccessFlag,
        "Opening CFBundle for %s.", urlPath);

    kextBundle = CFBundleCreate(CFGetAllocator(aKext), anURL);
    if (!kextBundle) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Can't open CFBundle for %s.", urlPath);
        goto finish;
    }

   /* Save the URL only after we've confirmed we can open a bundle there.
    * See __OSKextRemoveKext().
    */
    aKext->bundleURL = CFRetain(anURL);

   /* If we can't get the info dictionary at all, we don't even
    * have an examinable broken kext.
    */
    if (!__OSKextReadInfoDictionary(aKext, kextBundle)) {
        goto finish;
    }

   /* Don't worry about the return value of this; we want to be
    * able to open bad kexts to do further diagnostics. It's up
    * to the client to close out unusable kexts.
    */
    __OSKextProcessInfoDictionary(aKext, kextBundle);

    result = __OSKextRecordKext(aKext);

finish:
    if (kextBundle) {
        OSKextLog(aKext,
            kOSKextLogDebugLevel | kOSKextLogFileAccessFlag,
            "Releasing CFBundle for %s",
            urlPath);
    }
    SAFE_RELEASE(kextBundle);
    return result;
}

/*********************************************************************
*********************************************************************/
static Boolean __OSKextInitFromMkext(
    OSKextRef       aKext,
    CFDictionaryRef infoDict,
    CFURLRef        mkextURL,
    CFDataRef       mkextData)
{
    Boolean     result     = false;
    CFStringRef bundlePath = NULL;   // do not release

    aKext->staticFlags.isFromMkext = 1;

    // xxx - we should remove the bundle path from the info dict once
    // xxx - it's been extracted
    bundlePath = CFDictionaryGetValue(infoDict, CFSTR(kMKEXTBundlePathKey));
    if (bundlePath) {
        aKext->bundleURL = CFURLCreateWithFileSystemPath(CFGetAllocator(aKext),
            bundlePath, kCFURLPOSIXPathStyle, true);
        if (!aKext->bundleURL) {
            OSKextLogMemError();
        }

        // xxx - must a kext always have a bundle URL?
        // xxx - if we support mkext1 format, they definitely won't!
        // xxx - goto finish; // ?
    }
    aKext->infoDictionary = CFRetain(infoDict);

    if (!__OSKextCreateMkextInfo(aKext)) {
        OSKextLogMemError();
        goto finish;
    }

    if (mkextURL) {
        aKext->mkextInfo->mkextURL = CFRetain(mkextURL);
    }
    aKext->mkextInfo->mkextData = CFRetain(mkextData);

    if (!__OSKextProcessInfoDictionary(aKext, NULL)) {
        goto finish; // skip file extraction
    }

    result = __OSKextRecordKext(aKext);

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
static void __OSKextReinitApplierFunction(
    const void * vKey __unused,
    const void * vValue,
          void * vContext __unused)
{
    OSKextRef aKext = (OSKextRef)vValue;

    __OSKextReinit(aKext);
    return;
}

void __OSKextReinit(OSKextRef aKext)
{
    if (aKext) {
        if (!aKext->staticFlags.isFromIdentifierCache) {
            SAFE_RELEASE_NULL(aKext->bundleID);
            OSKextFlushDiagnostics(aKext, kOSKextDiagnosticsFlagAll);
            bzero(&aKext->flags, sizeof(aKext->flags));
            __OSKextProcessInfoDictionary(aKext, /* bundle */ NULL);
        }
    } else if (__sOSKextsByURL) {
        CFDictionaryApplyFunction(__sOSKextsByURL,
            __OSKextReinitApplierFunction, NULL);
    }
    return;
}

/*********************************************************************
* The record/remove functions are the few functions we know will only
* be called after the bookkeeping data structures have been created,
* so we don't check them here.
*********************************************************************/
Boolean __OSKextRecordKext(OSKextRef aKext)
{
    Boolean      result        = false;
    CFStringRef  kextID        = NULL;  // do not release
    CFURLRef     kextURL       = NULL;  // do not release
    CFURLRef     canonicalURL  = NULL;  // must release
    char       * kextIDCString = NULL;  // must free
    char         versionCString[kOSKextVersionMaxLength];
    char         urlPath[PATH_MAX];

    kextID = OSKextGetIdentifier(aKext);
    if (kextID) {
        kextIDCString = createUTF8CStringForCFString(kextID);
    }

   /* Kexts are allowed not to have an URL. Specifically, kexts
    * from an old-format mkext won't have one.
    */
    kextURL = OSKextGetURL(aKext);
    if (kextURL) {
        canonicalURL = CFURLCopyAbsoluteURL(kextURL);
        if (!canonicalURL) {
            OSKextLogMemError();
            goto finish;
        }
    }

    __OSKextGetFileSystemPath(/* kext */ NULL,
        /* otherURL */ canonicalURL,
        /* resolveToBase */ true, urlPath);

   /* Record the kext in the main array, the URL dict, then the bundle ID dict.
    * Kexts created from an mkext do *not* get cached by URL.
    */
    if (CFArrayGetFirstIndexOfValue(__sOSAllKexts, RANGE_ALL(__sOSAllKexts),
        aKext) == kCFNotFound) {

        CFArrayAppendValue(__sOSAllKexts, aKext);
    }

    // xxx - what if somehow we actually create 2 kexts w/same URL?
    if (canonicalURL && !OSKextIsFromMkext(aKext)) {
        CFDictionarySetValue(__sOSKextsByURL, canonicalURL, aKext);
    }

    result = __OSKextRecordKextInIdentifierDict(aKext, __sOSKextsByIdentifier);
    if (result) {
        OSKextVersionGetString(OSKextGetVersion(aKext), versionCString,
            sizeof(versionCString));
        OSKextLog(aKext,
            kOSKextLogStepLevel | kOSKextLogKextBookkeepingFlag,
            "Recorded %s%s, id %s, version %s.",
            urlPath,
            OSKextIsFromMkext(aKext) ? " (from mkext)" : "",
            kextIDCString ? kextIDCString : __kStringUnknown,
            versionCString);
    }

finish:
    SAFE_RELEASE(canonicalURL);
    SAFE_FREE(kextIDCString);
    return result;
}

/*********************************************************************
*********************************************************************/
void __OSKextRemoveKext(OSKextRef aKext)
{
    CFTypeRef    foundEntry             = NULL;    // do not release
    CFURLRef     kextURL                = NULL;    // do not release
    CFURLRef     canonicalURL           = NULL;    // must release
    CFStringRef  kextIdentifier         = NULL;    // do not release
    char       * allocatedCString       = NULL;   // must free
    char       * kextIdentifierCString  = NULL;   // do not free

    char         urlPath[PATH_MAX] = __kStringUnknown;
    char         versionCString[kOSKextVersionMaxLength];
    CFIndex      count, i;

   /* Remove from the cache of all kexts. This must absolutely happen
    * regardless of any other problems with bundle IDs or URLs.
    */
    count = CFArrayGetCount(__sOSAllKexts);
    if (count) {
        for (i = count - 1; i >= 0; i--) {
            OSKextRef checkKext = (OSKextRef)CFArrayGetValueAtIndex(
                __sOSAllKexts, i);
            if (checkKext == aKext) {
                CFArrayRemoveValueAtIndex(__sOSAllKexts, i);
            }
        }
    }

    kextIdentifier = OSKextGetIdentifier(aKext);
    if (kextIdentifier) {
        allocatedCString = createUTF8CStringForCFString(kextIdentifier);
        kextIdentifierCString = allocatedCString;
    } else {
        kextIdentifierCString = __kOSKextUnknownIdentifier;
    }

    kextURL = OSKextGetURL(aKext);
    if (kextURL) {
        canonicalURL = CFURLCopyAbsoluteURL(kextURL);
        if (canonicalURL) {
            __OSKextGetFileSystemPath(/* kext */ NULL,
                /* otherURL */ canonicalURL,
                /* resolveToBase */ true, urlPath);

           /* Remove from the URL cache.
            */
            if (canonicalURL && !OSKextIsFromMkext(aKext)) {
                foundEntry = CFDictionaryGetValue(__sOSKextsByURL, canonicalURL);

               /* Remove from URL dictionary.
                * xxx - There really should only be the one; should we log an error
                * xxx - if they aren't the same??
                */
                if (foundEntry == aKext) {
                    CFDictionaryRemoveValue(__sOSKextsByURL, canonicalURL);
                }
            }
        }
    }

   /* Remove from bundle identifier lookup dictionary.
    */
    __OSKextRemoveKextFromIdentifierDict(aKext, __sOSKextsByIdentifier);

    OSKextVersionGetString(OSKextGetVersion(aKext), versionCString,
        sizeof(versionCString));
    OSKextLog(aKext, kOSKextLogStepLevel | kOSKextLogKextBookkeepingFlag,
        "Removed %s, id %s%s, version %s.",
        urlPath,
        OSKextIsFromMkext(aKext) ? " (from mkext)" : "",
        kextIdentifierCString,
        versionCString);

    SAFE_RELEASE(canonicalURL);
    SAFE_FREE(allocatedCString);
    return;
}

/*********************************************************************
*********************************************************************/
Boolean __OSKextRecordKextInIdentifierDict(
    OSKextRef              aKext,
    CFMutableDictionaryRef identifierDict)
{
    Boolean             result        = true;
    CFStringRef         kextID        = NULL;  // do not release
    CFTypeRef           foundEntry    = NULL;  // do not release
    CFMutableArrayRef   subsKexts     = NULL;  // DO NOT RELEASE
    char              * kextIDCString = NULL;  // must free
    int                 lookupIndex   = 0;     // default if no array

    kextID = OSKextGetIdentifier(aKext);
    if (!kextID) {
        OSKextLog(aKext,
            kOSKextLogErrorLevel | kOSKextLogKextBookkeepingFlag,
            "Can't record kext in identifier lookup dictionary; no identifier.");
        result = false;
        goto finish;
    }

   /*****
    * Look up the bundle ID.
    * If we don't find it, add the kext and we're done.
    * If we find the kext itself, we're done!
    * If we find another kext, make an array and put both in it.
    * If we find an array, add the new kext to it.
    */
    foundEntry = CFDictionaryGetValue(identifierDict, kextID);
    if (!foundEntry) {
        CFDictionarySetValue(identifierDict, kextID, aKext);
        kextIDCString = createUTF8CStringForCFString(kextID);
        goto finish;
    }
    if (foundEntry == aKext) {
        kextIDCString = createUTF8CStringForCFString(kextID);
        goto finish;
    }

   /* Replace a single different kext with an array of kexts for this
    * kextID.
    */
    if (OSKextGetTypeID() == CFGetTypeID(foundEntry)) {

        CFArrayCallBacks nonrefcountArrayCallBacks =
            kCFTypeArrayCallBacks;
        nonrefcountArrayCallBacks.retain = NULL;
        nonrefcountArrayCallBacks.release = NULL;
        subsKexts = CFArrayCreateMutable(kCFAllocatorDefault, 0,
            &nonrefcountArrayCallBacks);
        // xxx - CFBundle doesn't even bother to check, it will just crash
        if (!subsKexts) {
            OSKextLogMemError();
            result = false;
            goto finish;
            // xxx - what can we do? the program *will* crash pretty soon
            // xxx - CFBundle doesn't even bother to check, it will just crash
        }

       /* Put the existing kext into the array, then replace
        * the kext in the dictionary of kexts by ID with the array.
        * Do *not* release foundEntry, as it wasn't retained
        * in the dictionary and isn't retained in the array!
        */
        CFArrayAppendValue(subsKexts, foundEntry);
        CFDictionarySetValue(identifierDict, kextID, subsKexts);
        foundEntry = subsKexts;
        
       /* Note: The identifier dicts do not retain values added,
        * so DO NOT RELEASE subsKexts at the bottom of this function!
        */
    }

   /* FALL THROUGH from the last check into this one, to insert the
    * new kext into the array of kexts by bundle ID.
    */
    if (CFArrayGetTypeID() == CFGetTypeID(foundEntry)) {
    
       /* Embedded folks want lookup always by last opened, so just shove
        * the kext at the beginning of the array for them. Otherwise
        * insert the kext before the first other instance of a kext with
        * the same or lower version.
        */
        if (__sOSKextStrictRecordingByLastOpened) {
            CFMutableArrayRef kextsWithSameID = (CFMutableArrayRef)foundEntry;
            CFArrayInsertValueAtIndex(kextsWithSameID, 0, aKext);
        } else {

            CFMutableArrayRef kextsWithSameID      = (CFMutableArrayRef)foundEntry;
            OSKextVersion     addedKextVersion     = OSKextGetVersion(aKext);
            CFIndex           addedKextCreateOrder = kCFNotFound;
            CFIndex           count, i;

           /* See if we already have the kext in the array, and yank it so we
            * can reinsert it at a (possibly different) location.
            */
            i = CFArrayGetFirstIndexOfValue(kextsWithSameID,
                RANGE_ALL(kextsWithSameID), aKext);
            if (i != kCFNotFound) {
                CFArrayRemoveValueAtIndex(kextsWithSameID, i);
            }

           /* Find out where in the list of all kexts the one being added is,
            * in case we are re-adding after a reload from disk. We need to
            * preserve the ordering amongst kexts with the same version.
            */
            addedKextCreateOrder = CFArrayGetFirstIndexOfValue(__sOSAllKexts,
                RANGE_ALL(__sOSAllKexts), aKext);

            count = CFArrayGetCount(kextsWithSameID);
            for (i = 0; i < count; i++) {
                OSKextRef         existingKext = (OSKextRef)CFArrayGetValueAtIndex(
                                                 kextsWithSameID, i);
                OSKextVersion     existingKextVersion = OSKextGetVersion(existingKext);
                CFIndex           existingKextCreateOrder = kCFNotFound;

                existingKextCreateOrder = CFArrayGetFirstIndexOfValue(__sOSAllKexts,
                    RANGE_ALL(__sOSAllKexts), existingKext);

               /* When recording kexts with the same identifier,
                * we sort them in DESCENDING version *and* create order, (as when
                * re-adding a kext because it got re-read from disk).
                *
                * So we are scanning through the list, we ignore higher versions
                * until we see any with the same version. When the versions are the
                * same, we break as soon as we see an "existing" kext that has a
                * LOWER create order than the one being added.
                *
                * Then, we're past the higher/same versions, so we break as soon
                * as we see an "existing" kext that has a LOWER version. If we
                * run through the whole array we'll just add to the end.
                */
                if (addedKextVersion == existingKextVersion) {
                    if (addedKextCreateOrder > existingKextCreateOrder) {
                        break;
                    }
                }
                if (addedKextVersion > existingKextVersion) {
                    break;
                }
            }
            
           /* Insert the kext at the location we found for it.
            */
            CFArrayInsertValueAtIndex(kextsWithSameID, i, aKext);
            kextIDCString = createUTF8CStringForCFString(kextID);
            lookupIndex = i;
        }
    }

finish:
    if (result && kextIDCString) {
        char versionString[kOSKextVersionMaxLength];
        OSKextVersionGetString(OSKextGetVersion(aKext), versionString,
            sizeof(versionString));
        if (foundEntry == aKext) {
            OSKextLog(aKext, kOSKextLogStepLevel | kOSKextLogKextBookkeepingFlag,
                "%s, version %s is already "
                "in the identifier lookup dictionary at index %d.",
                kextIDCString, versionString, lookupIndex);
        } else {
            OSKextLog(aKext, kOSKextLogDebugLevel | kOSKextLogKextBookkeepingFlag,
                "%s, version %s recorded at index %d "
                "in the identifier lookup dictionary.",
                kextIDCString, versionString, lookupIndex);
        }
    }
    SAFE_FREE(kextIDCString);
    return result;
}

/*********************************************************************
*********************************************************************/
void __OSKextRemoveKextFromIdentifierDict(
    OSKextRef              aKext,
    CFMutableDictionaryRef identifierDict)
{
    CFStringRef  kextID        = NULL;   // do not release
    CFTypeRef    foundEntry    = NULL;   // do not release
    OSKextRef    foundKext     = NULL;  // do not release
    char       * kextIDCString = NULL;  // must free

   /* A kext with no identifier is going to cause us a world of hurt,
    * but there's nothing we can do about it now.
    */
    kextID = OSKextGetIdentifier(aKext);
    if (!kextID) {
        goto finish;
    }

    kextIDCString = createUTF8CStringForCFString(kextID);
    if (!kextIDCString) {
        OSKextLogMemError();
        goto finish;
    }

    foundEntry = CFDictionaryGetValue(identifierDict, kextID);
    if (!foundEntry) {
        goto finish;
    }

    if (foundEntry == aKext) {
        foundKext = (OSKextRef)foundEntry;
        CFDictionaryRemoveValue(identifierDict, kextID);
    } else if (CFArrayGetTypeID() == CFGetTypeID(foundEntry)) {
        CFMutableArrayRef kextsWithSameID = (CFMutableArrayRef)foundEntry;
        CFIndex           count, i;

        count = CFArrayGetCount(kextsWithSameID);
        for (i = 0; i < count; i++) {
            OSKextRef thisKext = (OSKextRef)CFArrayGetValueAtIndex(
                kextsWithSameID, i);
            if (thisKext == aKext) {
                foundKext = thisKext;
                CFArrayRemoveValueAtIndex(kextsWithSameID, i);
                break;
                // xxx - scan through the whole array?
            }
        }
        
       /* If we've emptied the array kextsWithSameID, remove it
        * from the identifier dictionary. Also, because that dictionary
        * doesn't retain values, release it explicitly.
        */
        if (!CFArrayGetCount(kextsWithSameID)) {
            CFDictionaryRemoveValue(identifierDict, kextID);
            CFRelease(kextsWithSameID);
        }
    }

finish:
    if (foundKext) {
        char versionCString[kOSKextVersionMaxLength];
        OSKextVersionGetString(OSKextGetVersion(aKext),
            versionCString, sizeof(versionCString));
        OSKextLog(aKext,
            kOSKextLogStepLevel | kOSKextLogKextBookkeepingFlag,
            "%s, version %s removed from identifier lookup dictionary.",
            kextIDCString, versionCString);
    } else if (kextIDCString) {
        char * auxMsg = NULL;
        
        if (!strncmp(kextIDCString, __kOSKextUnknownIdentifier,
            sizeof(__kOSKextUnknownIdentifier) - 1)) {

            auxMsg = " (expected while realizing)";
        }
        OSKextLog(aKext,
            kOSKextLogDebugLevel | kOSKextLogKextBookkeepingFlag,
            "%s not found in identifier lookup dictionary%s.",
            kextIDCString, auxMsg);
    }
    SAFE_FREE(kextIDCString);
    return;
}

/*********************************************************************
* I really need to have stuff initialized before any instances are
* created, I hope the constructor attribute is the right approach.
*********************************************************************/
OSKextRef OSKextCreate(
    CFAllocatorRef allocator,
    CFURLRef       anURL)
{
    OSKextRef   result       = NULL;
    CFStringRef pathExtension = NULL;  // must release
    char        relPath[PATH_MAX];

    pthread_once(&__sOSKextInitialized, __OSKextInitialize);

    pathExtension = CFURLCopyPathExtension(anURL);
    if (!pathExtension || !CFEqual(pathExtension, CFSTR(kOSKextBundleExtension))) {
        goto finish;
    }

    if (!__OSKextGetFileSystemPath(/* kext */ NULL, /* otherURL */ anURL,
            /* resolveToBase */ false, relPath)) {
            
        goto finish;
    }

    result = OSKextGetKextWithURL(anURL);
    if (result) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogDebugLevel |
            kOSKextLogKextBookkeepingFlag | kOSKextLogFileAccessFlag,
            "%s is already open; returning existing object.",
            relPath);
        CFRetain(result);
        goto finish;
    }

    OSKextLog(/* kext */ NULL,
        kOSKextLogDebugLevel |
        kOSKextLogKextBookkeepingFlag | kOSKextLogFileAccessFlag,
        "Creating %s.",
        relPath);

    result = __OSKextAlloc(allocator, /* context */ NULL);
    if (!result) {
        OSKextLogMemError();
        goto finish;
    }
    if (!__OSKextInitWithURL(result, anURL)) {
        SAFE_RELEASE_NULL(result);
        goto finish;
    }

finish:
    SAFE_RELEASE(pathExtension);
    return result;
}

/*********************************************************************
*********************************************************************/
CFMutableArrayRef __OSKextCreateKextsFromURL(
    CFAllocatorRef allocator,
    CFURLRef       anURL,
    OSKextRef      aKext,  // if called by a kext looking for plugins
    Boolean        createPluginsFlag)
{
    CFMutableArrayRef result          = NULL;
    CFStringRef       pathExtension   = NULL;  // must release
    OSKextRef         theKext         = NULL;  // must release
    CFArrayRef        plugins         = NULL;  // must release
    CFURLRef          absURL          = NULL;  // must release
    char              urlPath[PATH_MAX];
    CFBooleanRef      dirExists       = NULL;  // must release
    CFArrayRef        urlContents     = NULL;  // must release
    SInt32            error;
    CFArrayRef        kexts           = NULL;  // must release
    CFIndex           count, i;

   /* Check for a single kext, read it and its plugins.
    */
    pathExtension = CFURLCopyPathExtension(anURL);
    if (pathExtension && CFEqual(pathExtension,
        CFSTR(kOSKextBundleExtension))) {

        result = CFArrayCreateMutable(allocator, 0, &kCFTypeArrayCallBacks);
        if (!result) {
            OSKextLogMemError();
            goto finish;
        }

        theKext = OSKextCreate(allocator, anURL);
        if (theKext) {
            CFArrayAppendValue(result, theKext);
            if (createPluginsFlag) {
                plugins = OSKextCopyPlugins(theKext);
                if (plugins && CFArrayGetCount(plugins)) {
                    CFArrayAppendArray(result, plugins, RANGE_ALL(plugins));
                }
            }
        }
        goto finish;
    }

    absURL = CFURLCopyAbsoluteURL(anURL);
    if (!absURL) {
        OSKextLogMemError();
        goto finish;
    }

    __OSKextGetFileSystemPath(/* kext */ NULL, /* otherURL */ absURL,
        /* resolveToBase */ true, urlPath);

   /* If we're scanning for plugins, silently skip when there's no plugins
    * folder. Else complain.
    */
    dirExists = CFURLCreatePropertyFromResource(allocator, anURL,
        kCFURLFileExists, &error);
    if (!dirExists) {
        OSKextLog(aKext,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Failed to check path %s (CF error %ld).",
            urlPath, (long)error);
        goto finish;
    } else if (!CFBooleanGetValue(dirExists)) {
        if (!aKext) {
            OSKextLog(aKext,
                kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "%s - no such file or directory.",
                urlPath);
        }
        goto finish;
    }

   /*****
    * If anURL is not a kext bundle, then scan it as a directory
    * for any kexts, with their plugins.
    * xxx - should we check for a symlink loop? :-O
    */

   /* If we can read from an identifier cache, we are done!
    */
    if (_OSKextReadFromIdentifierCacheForFolder(absURL, &result)) {
        goto finish;
    }

    result = CFArrayCreateMutable(allocator, 0, &kCFTypeArrayCallBacks);
    if (!result) {
        OSKextLogMemError();
        goto finish;
    }

    OSKextLog(aKext,
        kOSKextLogProgressLevel | kOSKextLogDirectoryScanFlag,
        "Scanning %s for kexts.", urlPath);

    // do not use absURL here, allow kexts to have relative URLs
    urlContents = CFURLCreatePropertyFromResource(allocator, anURL,
        kCFURLFileDirectoryContents, &error);
    if (!urlContents || error) {
        if (error && error != kCFURLResourceNotFoundError) {
            OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "Failed to read contents of %s, CFURL error %d.",
                urlPath, (int)error);
        }
        goto finish;
    }
    count = CFArrayGetCount(urlContents);
    for (i = 0; i < count; i++) {
        CFURLRef thisURL = (CFURLRef)CFArrayGetValueAtIndex(urlContents, i);

        SAFE_RELEASE_NULL(pathExtension);
        SAFE_RELEASE_NULL(kexts);

        pathExtension = CFURLCopyPathExtension(thisURL);
        if (pathExtension && CFEqual(pathExtension,
            CFSTR(kOSKextBundleExtension))) {

            char kextURLPath[PATH_MAX];
            
            __OSKextGetFileSystemPath(/* kext */ NULL, thisURL,
                /* resolveToBase */ FALSE, kextURLPath);

            if (aKext) {
                OSKextLog(aKext,
                    kOSKextLogDetailLevel | kOSKextLogDirectoryScanFlag,
                    "Found plugin %s.",
                    kextURLPath);
            } else {
                OSKextLog(aKext,
                    kOSKextLogDetailLevel | kOSKextLogDirectoryScanFlag,
                    "Found %s.",
                    kextURLPath);
            }

           /* Create kexts with their immediate plugins from the
            * current URL.
            */
            kexts = OSKextCreateKextsFromURL(allocator, thisURL);
            if (kexts) {
                CFArrayAppendArray(result, kexts, RANGE_ALL(kexts));
            }
        }
    }

    (void)_OSKextWriteIdentifierCacheForKextsInDirectory(result, absURL,
        /* force? */ false);

finish:
    SAFE_RELEASE(pathExtension);
    SAFE_RELEASE(theKext);
    SAFE_RELEASE(plugins);
    SAFE_RELEASE(dirExists);
    SAFE_RELEASE(urlContents);
    SAFE_RELEASE(kexts);
    SAFE_RELEASE(absURL);

    return result;
}

/*********************************************************************
*********************************************************************/
CFArrayRef OSKextCreateKextsFromURL(
    CFAllocatorRef allocator,
    CFURLRef       anURL)
{
    pthread_once(&__sOSKextInitialized, __OSKextInitialize);

   /* Passing NULL for the kext means we'll scan for plugins.
    */
    return __OSKextCreateKextsFromURL(allocator, anURL,
        /* kext */ NULL, true);
}

/*********************************************************************
*********************************************************************/
CFMutableArrayRef __OSKextCreateKextsFromURLs(
    CFAllocatorRef allocator,
    CFArrayRef     arrayOfURLs,
    Boolean        createPluginsFlag)
{
    CFMutableArrayRef result = NULL;
    CFArrayRef        scannedKexts = NULL;  // must release
    CFIndex           count, i;

    result = CFArrayCreateMutable(allocator, 0, &kCFTypeArrayCallBacks);
    if (!result) {
        OSKextLogMemError();
        goto finish;
    }

    count = CFArrayGetCount(arrayOfURLs);
    for (i = 0; i < count; i++) {
        CFURLRef theURL = (CFURLRef)CFArrayGetValueAtIndex(arrayOfURLs, i);

        SAFE_RELEASE_NULL(scannedKexts);

        scannedKexts = __OSKextCreateKextsFromURL(allocator, theURL,
            /* kext */ NULL, createPluginsFlag);
        if (scannedKexts) {
            CFArrayAppendArray(result, scannedKexts, RANGE_ALL(scannedKexts));
        }
    }

finish:
    SAFE_RELEASE(scannedKexts);
    return result;
}

/*********************************************************************
*********************************************************************/
CFArrayRef OSKextCreateKextsFromURLs(
    CFAllocatorRef allocator,
    CFArrayRef arrayOfURLs)
{
    CFMutableArrayRef result = NULL;
    CFIndex           count, i, j;

    pthread_once(&__sOSKextInitialized, __OSKextInitialize);

    result = __OSKextCreateKextsFromURLs(allocator, arrayOfURLs, true);
    if (!result) {
        goto finish;
    }
    count = CFArrayGetCount(result);
    if (!count) {
        goto finish;
    }

   /* Remove duplicates from the result. Note we don't use cached array
    * counts here, as the array is changing!
    */
    for (i = 0; i < CFArrayGetCount(result); i++) {
        OSKextRef thisKext = (OSKextRef)CFArrayGetValueAtIndex(result, i);
        for (j = i + 1; j < CFArrayGetCount(result); /* see loop */) {
            OSKextRef thatKext = (OSKextRef)CFArrayGetValueAtIndex(result, j);

           /* If we have two entries for the same kext, remove the latter.
            * Otherwise bump the inner index.
            */
            if (thisKext == thatKext) {
                CFArrayRemoveValueAtIndex(result, j);
            } else {
                j++;
            }
        }
    }

finish:
    return result;
}

#pragma mark (Plist Caches)
/*********************************************************************
* Plist Caches
*********************************************************************/

/*********************************************************************
*********************************************************************/
#define GZIP_RATIO    (5)
#define MAX_REALLOCS (16)

Boolean _OSKextReadFromIdentifierCacheForFolder(
    CFURLRef            anURL,
    CFMutableArrayRef * kextsOut)
{
    Boolean            result                = false;
    CFMutableArrayRef  kexts                 = NULL;  // must release
    CFURLRef           absURL                = NULL;  // must release
    char               absPath[PATH_MAX]     = "";
    CFDictionaryRef    cacheDict             = NULL;  // must release
    CFStringRef        basePath              = NULL;  // do not release
    CFNumberRef        cacheVersion          = NULL;  // do not release
    SInt32             cacheVersionValue     = 0;
    CFArrayRef         kextInfoArray         = NULL;  // do not release
    OSKextRef          newKext               = NULL;  // must release
    CFIndex            count, i;

    if (!OSKextGetUsesCaches()) {
        goto finish;
    }

    if (!__OSKextGetFileSystemPath(/* kext */ NULL, anURL,
        /* resolveToBase */ true, absPath)) {

        OSKextLogStringError(/* kext */ NULL);
        goto finish;
    }

    if (!_OSKextReadCache(anURL, 
        CFSTR(_kOSKextIdentifierCacheBasename),
        /* arch */ NULL,
        _kOSKextCacheFormatCFBinary,
        /* parseXML? */ true,
        kextsOut ? (CFPropertyListRef *)&cacheDict : NULL)) {

        goto finish;
    }

   /* If we aren't asked to actually read out kexts, we were just checking
    * that the cache is up to date, so return success now.
    */
    if (!kextsOut) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogDebugLevel | kOSKextLogKextBookkeepingFlag,
            "Kext identifier->path cache for %s is up to date.",
            absPath);
        result = true;
        goto finish;
    }

    if (CFDictionaryGetTypeID() != CFGetTypeID(cacheDict)) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogKextBookkeepingFlag,
            "Kext identifier->path cache for %s - not a dictionary.",
            absPath);
        goto finish;
    }

    cacheVersion = CFDictionaryGetValue(cacheDict,
        CFSTR(__kOSKextIdentifierCacheVersionKey));
    if (!cacheVersion ||
        CFNumberGetTypeID() != CFGetTypeID(cacheVersion) ||
        !CFNumberGetValue(cacheVersion, kCFNumberSInt32Type, &cacheVersionValue)) {

        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogKextBookkeepingFlag,
            "Kext identifier->cache for %s - cache version missing/invalid.",
            absPath);
        goto finish;
    }

    if (cacheVersionValue != __kOSKextIdentifierCacheCurrentVersion) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogKextBookkeepingFlag,
            "Kext identifier->path cache for %s - version %d unsupported.",
            absPath, (int)cacheVersionValue);
        goto finish;
    }

    basePath = CFDictionaryGetValue(cacheDict,
        CFSTR(__kOSKextIdentifierCacheBasePathKey));
    if (!basePath ||
        CFStringGetTypeID() != CFGetTypeID(basePath)) {

       OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogKextBookkeepingFlag,
            "Kext identifier->path cache for %s - base path missing or invalid.",
            absPath);
        goto finish;
    }

    kextInfoArray = CFDictionaryGetValue(cacheDict,
        CFSTR(__kOSKextIdentifierCacheKextInfoKey));
    if (!kextInfoArray ||
        CFArrayGetTypeID() != CFGetTypeID(kextInfoArray)) {

        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogKextBookkeepingFlag,
            "Kext identifier->path cache - kext info is not an array.");
        goto finish;
    }

    OSKextLog(/* kext */ NULL,
        kOSKextLogProgressLevel | kOSKextLogKextBookkeepingFlag,
        "Creating kexts from identifier->path cache for %s.",
        absPath);

    kexts = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if (!kexts) {
        OSKextLogMemError();
        goto finish;
    }

    // create kexts from cache file
    count = CFArrayGetCount(kextInfoArray);
    for (i = 0; i < count; i++) {
        CFDictionaryRef cacheDict = (CFDictionaryRef)CFArrayGetValueAtIndex(
            kextInfoArray, i);
        if (CFDictionaryGetTypeID() != CFGetTypeID(cacheDict)) {
            OSKextLog(/* kext */ NULL, kOSKextLogErrorLevel,
                "Kext identifier->path cache for %s - kext entry not a dictionary.",
                absPath);
            goto finish;
        }
        SAFE_RELEASE_NULL(newKext);
        newKext = __OSKextCreateFromIdentifierCacheDict(CFGetAllocator(absURL),
            cacheDict, basePath, i);
        if (!newKext) {
           /* The create call will have logged an error.
            */
            goto finish;
        }
        if (kCFNotFound == CFArrayGetFirstIndexOfValue(kexts, RANGE_ALL(kexts),
            newKext)) {

            CFArrayAppendValue(kexts, newKext);
        }
    }

   /* Now we know we have them all, record them (or drop them if recording
    * fails, which is why we're going backwards through the array).
    */
    count = CFArrayGetCount(kexts);
    if (count) {
        for (i = count - 1; i >= 0; i--) {
            OSKextRef aKext = (OSKextRef)CFArrayGetValueAtIndex(
                kexts, i);
            if (!__OSKextRecordKext(aKext)) {
                CFArrayRemoveValueAtIndex(kexts, i);
            }
        }
    }

    OSKextLog(/* kext */ NULL,
        kOSKextLogProgressLevel |
        kOSKextLogKextBookkeepingFlag | kOSKextLogFileAccessFlag,
        "Finished reading identifier->path cache for %s.",
        absPath);
    result = true;
    if (kextsOut) {
        *kextsOut = (CFMutableArrayRef)CFRetain(kexts);
    }

finish:

    SAFE_RELEASE(kexts);
    SAFE_RELEASE(absURL);
    SAFE_RELEASE(cacheDict);
    SAFE_RELEASE(newKext);

    return result;
}

/*********************************************************************
*********************************************************************/
OSKextRef __OSKextCreateFromIdentifierCacheDict(
    CFAllocatorRef  allocator,
    CFDictionaryRef cacheDict,
    CFStringRef     basePath,
    CFIndex         entryIndex)
{
    OSKextRef     result             = NULL;
    OSKextRef     newKext            = NULL;  // must release
    OSKextRef     existingKext       = NULL;  // do not release
    CFStringRef   bundleID           = NULL;  // do not release
    CFStringRef   bundlePath         = NULL;  // do not release
    CFStringRef   bundleVersion      = NULL;  // do not release
    CFStringRef   fullPath           = NULL;  // must release
    CFURLRef      bundleURL          = NULL;  // must release
    OSKextVersion kextVersion        = -1;
    CFBooleanRef  scratchBool        = NULL;  // do not release
    char          kextPath[PATH_MAX];

    bundlePath = (CFStringRef)CFDictionaryGetValue(cacheDict,
        CFSTR("OSBundlePath"));
    if (!bundlePath || (CFGetTypeID(bundlePath) != CFStringGetTypeID())) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogKextBookkeepingFlag,
            "Can't create kext: missing or non-string path "
            "in identifier cache entry %d.",
            (int)entryIndex);
        goto finish;
    }
    
   /* Reject any non-.kext path.
    */
    if (!CFStringHasSuffix(bundlePath, CFSTR(kOSKextBundleExtension))) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogKextBookkeepingFlag,
            "Can't create kext: path in identifier cache entry %d "
            "doesn't name a kext.",
            (int)entryIndex);
        goto finish;
    }
    
    fullPath = CFStringCreateWithFormat(allocator,
        /* options */ 0, CFSTR("%@/%@"), basePath, bundlePath);
     if (!fullPath) {
        OSKextLogMemError();
        goto finish;
     }
 
    bundleURL = CFURLCreateWithFileSystemPath(allocator,
        fullPath, kCFURLPOSIXPathStyle, /* isDir */ true);
    if (!bundleURL) {
        OSKextLogMemError();
        goto finish;
    }
    
    __OSKextGetFileSystemPath(/* kext */ NULL, bundleURL,
        /* resolveToBase */ TRUE, kextPath);

   /* See if we already have an instance.
    */
    existingKext = (OSKextRef)CFDictionaryGetValue(__sOSKextsByURL, bundleURL);
    if (existingKext) {
        result = existingKext;
    } else {
        newKext = __OSKextAlloc(allocator, /* context */ NULL);
        if (!newKext) {
            OSKextLogMemError();
            goto finish;
        }

        newKext->staticFlags.isFromIdentifierCache = 1;
        newKext->bundleURL = CFRetain(bundleURL);
    }
    
    bundleID = (CFStringRef)CFDictionaryGetValue(cacheDict,
        kCFBundleIdentifierKey);
    if (!bundleID || (CFGetTypeID(bundleID) != CFStringGetTypeID())) {
        OSKextLog(existingKext,
            kOSKextLogErrorLevel | kOSKextLogKextBookkeepingFlag,
            "Can't create kext: missing or non-string CFBundleIdentifier "
            "in identifier cache entry %d.",
            (int)entryIndex);
        goto finish;
    }

    if (existingKext) {
        if (!CFEqual(bundleID, OSKextGetIdentifier(existingKext))) {
            OSKextLog(existingKext,
                kOSKextLogErrorLevel | kOSKextLogKextBookkeepingFlag,
                "Can't create kext from cache: %s is already open and "
                "has a different CFBundleIdentifier "
                "from identifier->path cache entry %d.",
                kextPath, (int)entryIndex);
            goto finish;
        }
    } else {
        newKext->bundleID = CFRetain(bundleID);
    }

    bundleVersion = CFDictionaryGetValue(cacheDict, kCFBundleVersionKey);
    if (!bundleVersion || (CFGetTypeID(bundleVersion) != CFStringGetTypeID())) {
        OSKextLog(existingKext,
            kOSKextLogErrorLevel | kOSKextLogKextBookkeepingFlag,
            "Can't create kext: missing or non-string version "
            "in identifier cache entry %d.",
            (int)entryIndex);
        goto finish;
    }
    kextVersion = OSKextParseVersionCFString(bundleVersion);
    if (kextVersion < 0) {
        OSKextLog(existingKext,
            kOSKextLogErrorLevel | kOSKextLogKextBookkeepingFlag,
            "Can't create kext: invalid CFBundleVersion "
            "in identifier cache entry entry %d.",
            (int)entryIndex);
        goto finish;
    }
    if (existingKext) {
        if (kextVersion != OSKextGetVersion(existingKext)) {
            OSKextLog(existingKext,
                kOSKextLogErrorLevel | kOSKextLogKextBookkeepingFlag,
                "Can't create kext from cache: %s is already open and "
                "has a different CFBundleVersion "
                "from identifier->path cache entry %d.",
                kextPath, (int)entryIndex);
            goto finish;
        }
    } else {
        newKext->version = kextVersion;
    }

   /* Log flags are stored optionally in the cache.
    * Do not check log spec against cache, as those can be changed any time.
    */
    if (newKext) {
        scratchBool = (CFBooleanRef)CFDictionaryGetValue(cacheDict,
            CFSTR(kOSBundleEnableKextLoggingKey));
        if (scratchBool) {
             if (CFGetTypeID(scratchBool) != CFBooleanGetTypeID()) {
                OSKextLog(existingKext,
                kOSKextLogErrorLevel | kOSKextLogKextBookkeepingFlag,
                    "Can't create kext from cache: non-boolean "
                    "OSKextEnableKextLogging in identifier cache entry %d.",
                    (int)entryIndex);
                goto finish;
            }
            newKext->flags.plistHasEnableLoggingSet = CFBooleanGetValue(scratchBool) ? 1 : 0;
            newKext->flags.loggingEnabled = CFBooleanGetValue(scratchBool) ? 1 : 0;
        }
    }

    if (!existingKext) {
        result = newKext;
    }

finish:

    if (result) {
        CFRetain(result);
    }

   /* Do not __OSKextRecordKext() in this function; we want to see if any fail
    * before we record the whole set.
    */
    SAFE_RELEASE(newKext);
    SAFE_RELEASE(fullPath);
    SAFE_RELEASE(bundleURL);
    return result;
}

/*********************************************************************
* __OSKextRealize() takes a kext from an identifier->URL cache and
* tries to read its info for real from the bundle. Any time a kext
* is created or referenced by URL or identifier, it must be realized
* immediately (for sanity, else we'd be realizing on every property
* access). Further any time a lookup is done by identifier, ALL kexts
* with that same identifier are realized, so version/compat./loaded
* checks all work with the currently open set of kexts.
*
* Mixing kexts opened from an identifier cache with those opened from
* an mkext is not supported at this time.
*
* This function's signature matches a CFArrayApplierFunction for
* convenience in use.
*********************************************************************/
void __OSKextRealize(const void * vKext, void * context __unused)
{
    OSKextRef     aKext          = (OSKextRef)vKext;
    CFStringRef   cachedBundleID = NULL;
    OSKextVersion cachedVersion  = -1;
    Boolean       removeCache    = false;
    char          kextPath[PATH_MAX];

    if (!aKext->staticFlags.isFromIdentifierCache) {
        goto finish;
    }
    
    __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL, 
        /* resolveToBase */ false, kextPath);

    OSKextLog(aKext,
        kOSKextLogDetailLevel | kOSKextLogKextBookkeepingFlag,
        "Realizing %s from identifier cache object.",
        kextPath);

   /* __OSKextProcessInfoDictionary() calls this, but we'll have wiped
    * the bundle ID so it won't work! Do it now, for safety.
    */
    __OSKextRemoveKextFromIdentifierDict(aKext, __sOSKextsByIdentifier);

   /* Save the current identifier & version, then wipe them from the kext
    * in case we get nothing from disk. We are basically doing an
    * __OSKextInitWithURL() from here on out but without the absolute URL
    * lookup, and with a different return semantic.
    *
    * Note that we release cachedBundleID in the finish: block, so no
    * goto finish from here on!
    */
    cachedBundleID = aKext->bundleID;
    cachedVersion  = aKext->version;
    aKext->version = -1;
    aKext->bundleID = CFSTR(__kOSKextUnknownIdentifier);

   /* Read the basic info that we lack from the info dictionary
    * of the bundle on disk. Ignore the return value, we are
    * instantiated and may have client refs, nothing we can do,
    * unless we implement exceptions....
    *
    * This call re-files the kext in the kextsByIdentifier dict
    * and restores bundleID even if one isn't found.
    */
    __OSKextProcessInfoDictionary(aKext, /* bundle */ NULL);


   /* Oh dear, we have a possibly-referenced instance but no real bundle ID.
    * __OSKextProcessInfoDictionary() has already set the invalid bit.
    * It's up to the client to close out
    * unusable kexts.
    */
    if (aKext->bundleID == CFSTR(__kOSKextUnknownIdentifier)) {
        removeCache = true;
    }

   /* If the cached version or bundle identifier have changed (one more likely
    * than the other), then we return false!
    */
    if (cachedVersion != aKext->version ||
        !CFEqual(cachedBundleID, aKext->bundleID)) {

        removeCache = true;
    }

finish:
   /* We have to clear the flag even if we fail or we'll always be hitting
    * this function.
    */
    aKext->staticFlags.isFromIdentifierCache = 0;
    
    if (removeCache) {
        __OSKextRemoveIdentifierCacheForKext(aKext);
    }
    SAFE_RELEASE(cachedBundleID);  // we got a new one from disk
    return;
}

/*********************************************************************
*********************************************************************/
void __OSKextRealizeKextsWithIdentifier(
    CFStringRef kextIdentifier)
{
    CFTypeRef foundEntry = NULL;  // do not release
    OSKextRef theKext    = NULL;  // do not release

   /* No need to init the library if there's nothing to get!
    */
    if (!__sOSKextsByIdentifier) {
        goto finish;
    }

    foundEntry = CFDictionaryGetValue(__sOSKextsByIdentifier, kextIdentifier);
    if (!foundEntry) {
         goto finish;
    }

    if (OSKextGetTypeID() == CFGetTypeID(foundEntry)) {
        theKext = (OSKextRef)foundEntry;
        
        __OSKextRealize(theKext, /* context */ NULL);
    } else if (CFArrayGetTypeID() == CFGetTypeID(foundEntry)) {
        CFMutableArrayRef kexts = (CFMutableArrayRef)foundEntry;

        if (!CFArrayGetCount(kexts)) {
            goto finish;
        }
        CFArrayApplyFunction(kexts, RANGE_ALL(kexts), 
            &__OSKextRealize, /* context */ NULL);
    }

finish:
    return;
}

/*********************************************************************
*********************************************************************/
CFURLRef __OSKextCreateCacheFileURL(
    CFTypeRef            folderURLsOrURL,
    CFStringRef          cacheName,
    const NXArchInfo   * arch,
    _OSKextCacheFormat   format)
{
    CFURLRef      result                  = NULL;
    Boolean       isStartup               = FALSE;
    CFStringRef   folderAbsPath           = NULL;   // must release
    CFStringRef   cacheFileString         = NULL;   // must release
    char        * suffix                  = "";     // do not free

    if (CFGetTypeID(folderURLsOrURL) == CFURLGetTypeID()) {
        CFURLRef folderURL = (CFURLRef)folderURLsOrURL;

        folderAbsPath = _CFURLCopyAbsolutePath(folderURL);
        if (!folderAbsPath) {
            OSKextLogMemError();
            goto finish;
        }

       /* We only do caches for system extensions folders. If the URL
        * given isn't on that list, bail.
        */
        if (!__OSKextURLIsSystemFolder(folderURL)) {
            OSKextLogCFString(/* kext */ NULL,
                kOSKextLogProgressLevel | kOSKextLogKextBookkeepingFlag,
                CFSTR("%@ is not a system extensions folder; not looking for a cache."),
                folderAbsPath);
            goto finish;
        }
    } else if (folderURLsOrURL == OSKextGetSystemExtensionsFolderURLs()) {
        // ??? - log something here?
        isStartup = TRUE;
    } else {
        // ??? - log something here?
        goto finish;
    }

    switch (format) {
    case _kOSKextCacheFormatRaw:
        // do nothing
        break;
    case _kOSKextCacheFormatCFXML: // fall through
    case _kOSKextCacheFormatCFBinary:
        suffix = ".plist.gz";
        break;
    case _kOSKextCacheFormatIOXML:
        suffix = ".ioplist.gz";
        break;
    }

   /* Compose the path. Start with the path to the kext system's caches folder.
    * Then add the Startup or Directories component as appropriate.
    * Then add a the folder abs path (which begins with a /) if it's for a single folder.
    * Then add the cache name, the arch if necessary, and the suffix!
    */
    cacheFileString = CFStringCreateWithFormat(kCFAllocatorDefault,
        /* options */ NULL, CFSTR("%s/%s%@/%@%s%s%s"),
        _kOSKextCachesRootFolder,
        isStartup ? _kOSKextStartupCachesSubfolder : _kOSKextDirectoryCachesSubfolder,
        isStartup ? CFSTR("") : folderAbsPath,
        cacheName,
        arch ? "_" : "",
        arch ? arch->name : "",
        suffix);
    if (!cacheFileString) {
        OSKextLogMemError();
        goto finish;
    }

    result = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
        cacheFileString, kCFURLPOSIXPathStyle, /* isDir */ false);
    if (!result) {
        OSKextLogMemError();
        goto finish;
    }

finish:
    SAFE_RELEASE(folderAbsPath);
    SAFE_RELEASE(cacheFileString);

    return result;
}

/*********************************************************************
*********************************************************************/
Boolean __OSKextCheckURL(CFURLRef anURL, Boolean writeCreateFlag)
{
    Boolean        result   = false;
    char           path[PATH_MAX] = "";
    char         * statPath = NULL;  // do not free
    char         * slashPtr = NULL;  // do not free
    struct stat    statBuffer;

    if (!__OSKextGetFileSystemPath(/* kext */ NULL, anURL,
        /* resolveToBase */ TRUE, path)) {
        
        goto finish;
    }

    if (path[0] != '/') {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Internal error, invalid argument to __OSKextCheckURL.");
        goto finish;
    }

    slashPtr = &path[0];
    while (1) {
        if (slashPtr == path) {
            statPath = "/";
        } else {
            statPath = path;
            if (slashPtr) {
                slashPtr[0] = '\0';
            }
        }
        
       /* If we are checking to write a file and possibly create
        * the containing directory, but the filesystem is read-only,
        * bail. But only bail if we know for sure it's read-only.
        */
        if (writeCreateFlag) {
            struct statfs  statfsBuffer;
            if (0 == statfs(statPath, &statfsBuffer)) {
                if (statfsBuffer.f_flags & MNT_RDONLY) {
                    OSKextLogCFString(/* kext */ NULL,
                        kOSKextLogProgressLevel | kOSKextLogFileAccessFlag,
                        CFSTR("Not saving %s - read-only filesystem."), path);
                    goto finish;
                }
            }
        }

        if (0 != stat(statPath, &statBuffer)) {
            if (errno == ENOENT) {
                if (writeCreateFlag) {
                    if (0 != mkdir(path, _kOSKextCacheFolderMode) &&
                        errno != EEXIST) {

                        OSKextLog(/* kext */ NULL,
                            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                            "Failed to create directory %s - %s.",
                            statPath, strerror(errno));
                        goto finish;
                    }
                } else {
                   /* Don't log anything if the file simply doesn't exist.
                    */
                    goto finish;
                }
            } else {
                OSKextLog(/* kext */ NULL,
                    kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                    "Can't stat path %s - %s.",
                    statPath, strerror(errno));
                goto finish;
            }
        }
        if (statBuffer.st_uid != 0) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "Can't create kext cache under %s - owner not root.",
                statPath);
            goto finish;
        }
       
       /* The root folder, /, is owned by group admin! Now what am I supposed
        * to do? We should be able to check folder groups as well.
        */
        if (!S_ISDIR(statBuffer.st_mode) && statBuffer.st_gid != 0) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "Can't create kext cache under %s - group not wheel.",
                statPath);
            goto finish;
        }

        if (!slashPtr) {
            break;
        } else if (slashPtr == path) {
            slashPtr = index(path + 1, '/');
            statPath = path;
        } else {
            slashPtr[0] = '/';
            slashPtr++;
            slashPtr = index(slashPtr, '/');
        }
    }

    result = true;

finish:
    if (slashPtr && slashPtr != path) {
        slashPtr[0] = '/';
    }
    return result;
}

/*********************************************************************
*********************************************************************/
Boolean _OSKextCreateFolderForCacheURL(CFURLRef cacheFileURL)
{
    Boolean  result    = false;
    CFURLRef folderURL = NULL;  // must release

    folderURL = CFURLCreateCopyDeletingLastPathComponent(kCFAllocatorDefault,
        cacheFileURL);
    if (!folderURL) {
        OSKextLogMemError();
        goto finish;
    }

    result = __OSKextCheckURL(folderURL, /* writeCreate? */ true);
finish:
    SAFE_RELEASE(folderURL);
    return result;
}

/*********************************************************************
*********************************************************************/
Boolean __OSKextURLIsSystemFolder(CFURLRef folderURL)
{
    Boolean  result       = false;
    CFURLRef folderAbsURL = NULL;  // must release

    folderAbsURL = CFURLCopyAbsoluteURL(folderURL);
    if (!folderAbsURL) {
        OSKextLogMemError();
        goto finish;
    }
    if (kCFNotFound == CFArrayGetFirstIndexOfValue(
        __sOSKextSystemExtensionsFolderURLs,
        RANGE_ALL(__sOSKextSystemExtensionsFolderURLs), folderAbsURL)) {

        goto finish;
    }
    result = true;
finish:
    SAFE_RELEASE(folderAbsURL);
    return result;
}

/*********************************************************************
*********************************************************************/
Boolean __OSKextStatURL(
    CFURLRef      anURL,
    Boolean     * missingOut,
    struct stat * statOut)
{
    Boolean     result = FALSE;
    char        path[PATH_MAX];
    struct stat statBuf;

    if (missingOut) {
        *missingOut = FALSE;
    }

    if (!__OSKextGetFileSystemPath(/* kext */ NULL, anURL,
        /* resolve */ TRUE, path)) {

            goto finish;
    }

    if (0 != stat(path, &statBuf)) {
    
       /* Mask ENOENT and ENOTDIR if the caller is handling.
        */
        if ((errno == ENOENT || errno == ENOTDIR) && missingOut) {
            *missingOut = TRUE;
        } else {
            OSKextLogCFString(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                CFSTR("Can't stat %s - %s."),
                path, strerror(errno));
        }
        goto finish;
    }
    
    result = TRUE;

finish:
    if (statOut) {
        *statOut = statBuf;
    }
    return result;
}

/*********************************************************************
* Get the stat buffer of the URL with the latest mod time of those
* provided.
*********************************************************************/
Boolean __OSKextStatURLsOrURL(
    CFTypeRef     folderURLsOrURL,
    Boolean     * missingOut,
    struct stat * latestStatOut)
{
    Boolean     result       = FALSE;
    Boolean     localMissing = FALSE;
    struct stat newStatBuf;
    struct stat latestStatBuf;
    
    if (CFGetTypeID(folderURLsOrURL) == CFURLGetTypeID()) {
        result = __OSKextStatURL((CFURLRef)folderURLsOrURL,
            missingOut, &latestStatBuf);
    } else if (CFGetTypeID(folderURLsOrURL) == CFArrayGetTypeID()) {
        CFArrayRef folderURLs = (CFArrayRef)folderURLsOrURL;
        CFIndex    count, i;
        CFIndex    successCount = 0;
        
        count = CFArrayGetCount(folderURLs);
        for (i = 0; i < count; i++) {
            result = __OSKextStatURL((CFURLRef)
                CFArrayGetValueAtIndex(folderURLs, i),
                missingOut ? &localMissing : NULL,
                &newStatBuf);
            if (!result) {
                continue;
            }
            successCount++;
            if (i == 0 || (newStatBuf.st_mtime > latestStatBuf.st_mtime)) {
                latestStatBuf = newStatBuf;
            }
        }

        if (successCount > 0) {
            result = TRUE;
        }
        if ((successCount < count) && missingOut) {
            *missingOut = TRUE;
        }
    }
    
// finish:
    if (latestStatOut) {
        *latestStatOut = latestStatBuf;
    }
    return result;
}

/*********************************************************************
*********************************************************************/
Boolean _OSKextWriteCache(
    CFTypeRef                 folderURLsOrURL,
    CFStringRef               cacheName,
    const NXArchInfo        * arch,
    _OSKextCacheFormat        format,
    CFPropertyListRef         plist)
{
    Boolean                  result              = false;
    CFURLRef                 folderAbsURL        = NULL;  // must release
    CFStringRef              folderAbsPath       = NULL;  // must release
    CFStringRef              cacheFileString     = NULL;  // must release
    CFURLRef                 cacheFileURL        = NULL;  // must release
    char                     tmpPath[PATH_MAX]   = "";
    char                     cachePath[PATH_MAX] = "";
    char                   * unlinkPath          = NULL;  // do not free
    int                      fileDescriptor      = -1;    // close on gz error
    mode_t                   realUmask           = 0;
    CFWriteStreamRef         plistStream         = NULL;  // must release
    CFDataRef                cacheData           = NULL;  // must release
    const UInt8            * cacheDataPtr        = NULL;  // don't free
    gzFile                   outputGZFile        = Z_NULL;  // must gzclose
    CFIndex                  cacheDataLength     = 0;
    CFIndex                  bytesWritten        = 0;
    struct stat              latestStat;

    if (CFGetTypeID(folderURLsOrURL) == CFURLGetTypeID()) {
        folderAbsURL = CFURLCopyAbsoluteURL((CFURLRef)folderURLsOrURL);
        if (!folderAbsURL) {
            OSKextLogMemError();
            goto finish;
        }

        folderAbsPath = _CFURLCopyAbsolutePath((CFURLRef)folderURLsOrURL);
        if (!folderAbsPath) {
            OSKextLogMemError();
            goto finish;
        }
    }
    
   /* __OSKextCreateCacheFileURL() checks if the URL is for a system extensions
    * folder and returns NULL if it isn't.
    */
    cacheFileURL = __OSKextCreateCacheFileURL(folderURLsOrURL,
        cacheName, arch, format);
    if (!cacheFileURL) {
        goto finish;
    }

    if (!__OSKextGetFileSystemPath(/* kext */ NULL, cacheFileURL,
        /* resolveToBase */ true, cachePath)) {

        OSKextLogStringError(/* kext */ NULL);
        goto finish;
    }

    OSKextLog(/* kext */ NULL,
        kOSKextLogProgressLevel | kOSKextLogKextBookkeepingFlag | kOSKextLogFileAccessFlag,
        "Saving cache file %s.",
        cachePath);

    if (!_OSKextCreateFolderForCacheURL(cacheFileURL)) {
        goto finish;
    }

    strlcpy(tmpPath, cachePath, sizeof(tmpPath));
    if (strlcat(tmpPath, ".XXXXXX", sizeof(tmpPath)) > sizeof(tmpPath)) {

        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Temp cache file name too long: %s.",
            tmpPath);
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

    unlinkPath = tmpPath;

   /* Set the umask to get it, then set it back to iself. Wish there were a
    * better way to query it.
    */
    realUmask = umask(0);
    umask(realUmask);

    if (-1 == fchmod(fileDescriptor, _kOSKextCacheFileMode & ~realUmask)) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Failed to set permissions for %s - %s.",
            tmpPath, strerror(errno));
        goto finish;
    }

    if (format == _kOSKextCacheFormatCFXML ||
        format == _kOSKextCacheFormatCFBinary) {

        CFPropertyListFormat cfPlistFormat;
        
        if (format == _kOSKextCacheFormatCFXML) {
            cfPlistFormat = kCFPropertyListBinaryFormat_v1_0;
        } else {
            cfPlistFormat = kCFPropertyListXMLFormat_v1_0;
        }

        plistStream = CFWriteStreamCreateWithAllocatedBuffers(
            kCFAllocatorDefault, kCFAllocatorDefault);
        if (!plistStream) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogKextBookkeepingFlag,
                "Can't create CFWriteStream to save cache %s.",
                cachePath);
            goto finish;
        }
        CFWriteStreamOpen(plistStream);
        CFPropertyListWriteToStream(plist, plistStream,
            cfPlistFormat, /* errorString */ NULL);
        CFWriteStreamClose(plistStream);

        cacheData = CFWriteStreamCopyProperty(plistStream,
            kCFStreamPropertyDataWritten);
    } else {
        cacheData = IOCFSerialize(plist, /* options */ 0);
    }
    if (!cacheData) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogKextBookkeepingFlag,
            "Failed to serialize data for cache %s.",
            cachePath);
        goto finish;
    }

    cacheDataPtr = CFDataGetBytePtr(cacheData);
    cacheDataLength = CFDataGetLength(cacheData);
    if (!cacheDataPtr) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Unable to get data to create cache file %s.",
            cachePath);
        goto finish;
    }
 
    errno = 0;
    outputGZFile = gzdopen(fileDescriptor, "w");
    if (!outputGZFile) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Failed to open compression stream for %s - %s.",
            cachePath, strerror(errno));
        goto finish;
    }

   /* outputGZFile owns the file descriptor now.
    */
    fileDescriptor = -1;

    bytesWritten = 0;   
    while (bytesWritten < cacheDataLength) {
        int bytesJustWritten = 0;

        errno = 0;
        bytesJustWritten = gzwrite(outputGZFile,
            cacheDataPtr + bytesWritten, cacheDataLength - bytesWritten);
        if (bytesJustWritten < 0) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "Compressed write error for cache file %s - %s.",
                cachePath, strerror(errno));
            goto finish;
        }

        bytesWritten += bytesJustWritten;
    }

   /* Need to close it before calling utimes.
    */
    errno = 0;
    if (gzclose(outputGZFile) != Z_OK) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Failed to close compression stream for %s - %s.",
            tmpPath, strerror(errno));
    }
    outputGZFile = Z_NULL;

    if (-1 == rename(tmpPath, cachePath)) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Can't rename temp cache file to %s - %s.",
            cachePath, strerror(errno));
        goto finish;
    }
    unlinkPath = cachePath;

   /* Give the cache file the mod time of the folder with the latest
    * mod date, +1 sec.
    */
    if (!__OSKextStatURLsOrURL(folderURLsOrURL,
        /* missingIsError */ FALSE, &latestStat)) {
        goto finish;
    } else {
        struct timeval cacheFileTimes[2];
        cacheFileTimes[0].tv_sec  = latestStat.st_mtime + 1;
        cacheFileTimes[0].tv_usec = 0;
        cacheFileTimes[1].tv_sec  = latestStat.st_mtime + 1;
        cacheFileTimes[1].tv_usec = 0;

        if (-1 == utimes(cachePath, cacheFileTimes)) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "Can't update mod time of cache file %s - %s.",
                cachePath, strerror(errno));
            if (errno == ENOENT) {
                unlinkPath = NULL;
            }
            goto finish;
        }
    }
    unlinkPath = NULL;

    result = true;
    OSKextLog(/* kext */ NULL,
        kOSKextLogProgressLevel | kOSKextLogKextBookkeepingFlag | kOSKextLogFileAccessFlag,
        "Saved cache file %s.", cachePath);

finish:
    if (-1 != fileDescriptor) close(fileDescriptor);

    if (Z_NULL != outputGZFile && gzclose(outputGZFile) != Z_OK) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Failed to close compression stream for %s - %s.",
            tmpPath, strerror(errno));
    }

    if (unlinkPath) {
        if (-1 == unlink(unlinkPath)) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "Failed to remove temp cache file %s - %s.",
                unlinkPath, strerror(errno));
        }
    }

    SAFE_RELEASE(folderAbsPath);
    SAFE_RELEASE(cacheFileString);
    SAFE_RELEASE(cacheFileURL);
    SAFE_RELEASE(cacheData);
    SAFE_RELEASE(plistStream);
    return result;
}

/*********************************************************************
*********************************************************************/
Boolean __OSKextCacheNeedsUpdate(
    CFURLRef  cacheURL,
    CFTypeRef folderURLsOrURL)
{
    Boolean     result    = TRUE;  // default is to need update
    Boolean     missing   = FALSE;
    CFStringRef cachePath = NULL;  // must release
    struct stat cacheFileStat;
    struct stat latestFolderStat;

    cachePath = _CFURLCopyAbsolutePath(cacheURL);
    if (!cachePath) {
        goto finish;
    }

    if (!__OSKextStatURL(cacheURL, &missing, &cacheFileStat)) {
        if (missing) {
             OSKextLogCFString(/* kext */ NULL,
                kOSKextLogDebugLevel |
                kOSKextLogKextBookkeepingFlag | kOSKextLogFileAccessFlag,
                CFSTR("Cache file %@ does not exist."),
                cachePath);
        }

        goto finish;
    }

   /*****
    * Various stats and checks.
    */
   /* Exists but isn't a regular file; we'll never use it.
    */
    if (!(cacheFileStat.st_mode & S_IFREG)) {
        OSKextLogCFString(/* kext */ NULL,
            kOSKextLogProgressLevel | kOSKextLogKextBookkeepingFlag | kOSKextLogFileAccessFlag,
            CFSTR("Cache file %@ is not a regular file; ignoring."),
            cachePath);
        goto finish;
    }

    if (!__OSKextCheckURL(cacheURL, /* writeCreate? */ false)) {
        goto finish;
    }

   /* Check if the cache file is ok to use.
    */
    if (cacheFileStat.st_uid != 0) {
        OSKextLogCFString(/* kext */ NULL,
            kOSKextLogWarningLevel |
            kOSKextLogKextBookkeepingFlag | kOSKextLogFileAccessFlag,
            CFSTR("Cache file %@ - owner not root; not using."),
            cachePath);
        goto finish;
    }

    if (cacheFileStat.st_gid != 0) {
        OSKextLogCFString(/* kext */ NULL,
            kOSKextLogWarningLevel |
            kOSKextLogKextBookkeepingFlag | kOSKextLogFileAccessFlag,
            CFSTR("Cache file %@ - group not wheel; not using."), cachePath);
        goto finish;
    }
    
    if ((cacheFileStat.st_mode & _kOSKextCacheFileModeMask) != _kOSKextCacheFileMode) {
        OSKextLogCFString(/* kext */ NULL,
            kOSKextLogWarningLevel |
            kOSKextLogKextBookkeepingFlag | kOSKextLogFileAccessFlag,
            CFSTR("Cache file %@ - wrong permissions (%#o, should be %#o); not using."),
            cachePath,
            cacheFileStat.st_mode, _kOSKextCacheFileMode);
        goto finish;
    }

    if (!__OSKextStatURLsOrURL(folderURLsOrURL,
        /* missing */ &missing, &latestFolderStat)) {

        OSKextLogCFString(/* kext */ NULL,
            kOSKextLogWarningLevel |
            kOSKextLogKextBookkeepingFlag | kOSKextLogFileAccessFlag,
            CFSTR("Can't stat source folders for cache file %@."),
            cachePath);
        goto finish;
    }

    if (cacheFileStat.st_mtime != (latestFolderStat.st_mtime + 1)) {
        OSKextLogCFString(/* kext */ NULL,
            kOSKextLogWarningLevel |
            kOSKextLogKextBookkeepingFlag | kOSKextLogFileAccessFlag,
            CFSTR("Cache file %@ is out of date; not using."),
            cachePath);
        goto finish;
    }

    result = FALSE;

finish:
    SAFE_RELEASE(cachePath);
    return result;
}

/*********************************************************************
*********************************************************************/
Boolean _OSKextReadCache(
    CFTypeRef                 folderURLsOrURL,
    CFStringRef               cacheName,
    const NXArchInfo        * arch,
    _OSKextCacheFormat        format,
    Boolean                   parseXMLFlag,
    CFPropertyListRef       * cacheContentsOut)
{
    Boolean           result                  = false;
    CFURLRef          cacheFileURL            = NULL;  // must release
    char              cachePath[PATH_MAX]     = "";
    CFDataRef         cacheData               = NULL;  // must release
    SInt32            error = 0;
    CFStringRef       errorString             = NULL;  // must release
    char            * errorCString            = NULL;  // must free
    CFDataRef         uncompressedCacheData   = NULL;  // must release
    ssize_t           uncompressedByteSize    = 0;
    u_char          * uncompressedBytes       = NULL;  // do not free
    z_stream          zstream;
    int               zlibResult              = Z_UNKNOWN;
    int               numReallocs;
    Boolean           inflateTried            = false;
    CFPropertyListRef cacheContents           = NULL;  // must release

   /* __OSKextCreateCacheFileURL() checks if the URL is for a system extensions
    * folder and returns NULL if it isn't.
    */
    cacheFileURL = __OSKextCreateCacheFileURL(folderURLsOrURL,
        cacheName, arch, format);
    if (!cacheFileURL) {
        goto finish;
    }

   /* Get the C string path for the cache.
    */
    if (!__OSKextGetFileSystemPath(/* kext */ NULL, cacheFileURL,
        /* resolveToBase */ TRUE, cachePath)) {

        goto finish;
    }

    if (__OSKextCacheNeedsUpdate(cacheFileURL, folderURLsOrURL)) {
        goto finish;
    }

   /* If we weren't given an out param, we're just checking that the cache
    * is up to date, and if we got this far, we are up to date.
    */
    if (!cacheContentsOut) {
        result = true;
        goto finish;
    }
    
    OSKextLog(/* kext */ NULL,
        kOSKextLogProgressLevel | kOSKextLogKextBookkeepingFlag | kOSKextLogFileAccessFlag,
        "Reading cache file %s.",
        cachePath);

    if (!CFURLCreateDataAndPropertiesFromResource(
        CFGetAllocator(cacheFileURL), cacheFileURL, &cacheData, /* props */ NULL,
        /* desiredProps */ NULL, &error)) {

        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Can't open cache file %s, CF error %d.",
            cachePath, (int)error);
        goto finish;
    }

    zstream.next_in   = (UInt8 *)CFDataGetBytePtr(cacheData);
    zstream.avail_in  = CFDataGetLength(cacheData);
    zstream.zalloc    = NULL;
    zstream.zfree     = NULL;
    zstream.opaque    = NULL;

    uncompressedByteSize = GZIP_RATIO * zstream.avail_in;
    uncompressedBytes = (u_char *)malloc(uncompressedByteSize);
    if (!uncompressedBytes) {
        OSKextLogMemError();
        goto finish;
    }

    zstream.next_out  = uncompressedBytes;
    zstream.avail_out = uncompressedByteSize;

   /* In order to read gzip data, we need to specify the default
    * bit window of 15, and add 32, per the zlib.h comments.
    */
    zlibResult = inflateInit2(&zstream, 15 + 32);
    if (zlibResult != Z_OK) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Error initializing zlib uncompression for %s.",
            cachePath);
        goto finish;
    }
    inflateTried = true;

    numReallocs = 0;
    while (numReallocs < MAX_REALLOCS && zlibResult == Z_OK) {
        zlibResult = inflate(&zstream, Z_NO_FLUSH);
        if (zlibResult == Z_STREAM_END) {
            // success! nothing do here, actually
            break;
        } else if ((zlibResult == Z_OK) || (zlibResult == Z_BUF_ERROR)) {
            numReallocs++;
            uncompressedByteSize *= 2;
            uncompressedBytes = realloc(uncompressedBytes, uncompressedByteSize);
            if (!uncompressedBytes) {
                OSKextLogMemError();
                goto finish;
            }
            zstream.next_out  = uncompressedBytes + zstream.total_out;
            zstream.avail_out = uncompressedByteSize - zstream.total_out;
            zlibResult = Z_OK;  // make it ok for the while loop
        } else {
            break;
        }
    }
    if (zlibResult != Z_STREAM_END) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Error uncompressing kext cache file %s - zlib returned %d - %s.",
            cachePath, zlibResult, zstream.msg ? zstream.msg : "(unknown)");
        goto finish;
    }

    uncompressedCacheData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
        (const UInt8 *)uncompressedBytes, zstream.total_out, kCFAllocatorMalloc);
    if (!uncompressedCacheData) {
        OSKextLogMemError();
        goto finish;
    }

    if (parseXMLFlag) {
        if (format == _kOSKextCacheFormatCFXML ||
            format == _kOSKextCacheFormatCFBinary) {

            cacheContents = CFPropertyListCreateFromXMLData(kCFAllocatorDefault,
                uncompressedCacheData, kCFPropertyListImmutable, &errorString);
        } else if (format == _kOSKextCacheFormatIOXML) {
            cacheContents = IOCFUnserialize((const char *)uncompressedBytes,
                kCFAllocatorDefault,
                /* options */ 0, &errorString);
        } else {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel |
                kOSKextLogKextBookkeepingFlag | kOSKextLogFileAccessFlag,
                "Invalid cache format %d specified.", format);
            goto finish;
        }
        if (!cacheContents) {
            errorCString = createUTF8CStringForCFString(errorString);
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel |
                kOSKextLogKextBookkeepingFlag | kOSKextLogFileAccessFlag,
                "Can't read plist from cache file %s - %s.",
                cachePath, errorCString ? errorCString : __kStringUnknown);
            goto finish;
        }
    } else {
        cacheContents = CFRetain(uncompressedCacheData);
    }

    result = true;

finish:
    OSKextLog(/* kext */ NULL,
        kOSKextLogProgressLevel |
        kOSKextLogKextBookkeepingFlag | kOSKextLogFileAccessFlag,
        "Finished reading cache file %s.",
        cachePath);

    if (result && cacheContentsOut && cacheContents) {
        *cacheContentsOut = CFRetain(cacheContents);
    }

    SAFE_RELEASE(cacheFileURL);
    SAFE_RELEASE(cacheContents);
    SAFE_RELEASE(cacheData);
    SAFE_RELEASE(errorString);
    SAFE_RELEASE(uncompressedCacheData);

    SAFE_FREE(errorCString);

    if (inflateTried) {
        inflateEnd(&zstream);
    }

    return result;
}

/*********************************************************************
* xxx - will need to optimize so we only check & remove once
*********************************************************************/
void __OSKextRemoveIdentifierCacheForKext(OSKextRef aKext)
{
    char         scratchPath[PATH_MAX];
    const char * delRoot = NULL;   // do not free

   /* We can't do it if we aren't root.
    */    
    if (geteuid() != 0) {
        return;
    }

    if (!__OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
        /* resolveToBase */ true, scratchPath)) {

        goto finish;
    }

   /* I'm sick of CF verbosity so we're going to do this C style.
    */
    if (!strncmp(scratchPath,
        _kOSKextSystemLibraryExtensionsFolder,
        strlen(_kOSKextSystemLibraryExtensionsFolder))) {
        
        delRoot = _kOSKextSystemLibraryExtensionsFolder;

    } else if (!strncmp(scratchPath,
        _kOSKextLibraryExtensionsFolder,
        strlen(_kOSKextLibraryExtensionsFolder))){

        delRoot = _kOSKextSystemLibraryExtensionsFolder;

    } else if (!strncmp(scratchPath,
        _kOSKextAppleInternalLibraryExtensionsFolder,
        strlen(_kOSKextAppleInternalLibraryExtensionsFolder))){

        delRoot = _kOSKextSystemLibraryExtensionsFolder;
    }
    
    if (!delRoot) {
        goto finish;
    }
    
    OSKextLog(/* kext */ NULL,
         kOSKextLogProgressLevel |
         kOSKextLogKextBookkeepingFlag | kOSKextLogFileAccessFlag,
        "Removing identifier->path cache %s.",
        scratchPath);

    scratchPath[0] = '\0';
    strlcpy(scratchPath, _kOSKextCachesRootFolder, sizeof(scratchPath));
    strlcat(scratchPath, delRoot, sizeof(scratchPath));
    strlcat(scratchPath, _kOSKextIdentifierCacheBasename, sizeof(scratchPath));
    if (unlink(scratchPath)) {
        if (errno != ENOENT && errno != ENOTDIR) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "Failed to remove identifier->path cache %s - %s.",
                scratchPath, strerror(errno));
        }
    }

finish:
    return;
}

/*********************************************************************
*********************************************************************/
Boolean _OSKextWriteIdentifierCacheForKextsInDirectory(
    CFArrayRef kextArray,
    CFURLRef   directoryURL,
    Boolean    forceFlag)
{
    Boolean                  result                = false;
    CFURLRef                 cacheFileURL          = NULL;  // must release
    CFStringRef              basePath              = NULL;  // must release
    CFNumberRef              cacheVersion          = NULL;  // must release
    SInt32                   cacheVersionValue     = 0;
    CFMutableDictionaryRef   cacheDict             = NULL;  // must release
    CFMutableArrayRef        kextInfoArray         = NULL;  // must release
    CFDictionaryRef          kextDict              = NULL;  // must release
    OSKextRef                aKext                 = NULL;  // do not release
    char                     origDirPath[PATH_MAX] = "";
    CFIndex                  count, i;

    if (!OSKextGetUsesCaches() && !forceFlag) {
        goto finish;
    }

   /* We can't do it if we aren't root.
    */    
    if (geteuid() != 0) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogProgressLevel | kOSKextLogKextBookkeepingFlag,
            "Not running as root; skipping save of identifier->path cache.");
        goto finish;
    }

   /* __OSKextCreateCacheFileURL() checks if the URL is for a system extensions
    * folders and returns NULL if it isn't. We don't actually use the URL in
    * this function; we're just using the convenience of the NULL return value
    * to avoid all the work of generating data we'll never save.
    */
    cacheFileURL = __OSKextCreateCacheFileURL(
        directoryURL,
        CFSTR(_kOSKextIdentifierCacheBasename),
        /* arch */ NULL,
        _kOSKextCacheFormatCFBinary);
    if (!cacheFileURL) {
        goto finish;
    }

    cacheDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!cacheDict) {
        OSKextLogMemError();
        goto finish;
    }

    if (!__OSKextGetFileSystemPath(/* kext */ NULL, directoryURL,
        /* resolveToBase */ true, origDirPath)) {

        goto finish;
    }

   /* Length passed in is w/o terminating nul character.
    */
    basePath = CFStringCreateWithBytes(kCFAllocatorDefault,
        (UInt8 *)origDirPath, strlen(origDirPath), kCFStringEncodingUTF8,
        /* isExtRep */ false);
    if (!basePath) {
        OSKextLogMemError();
        goto finish;
    }
    CFDictionarySetValue(cacheDict, CFSTR(__kOSKextIdentifierCacheBasePathKey),
        basePath);

    kextInfoArray = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!kextInfoArray) {
        OSKextLogMemError();
        goto finish;
    }
    CFDictionarySetValue(cacheDict, CFSTR(__kOSKextIdentifierCacheKextInfoKey),
        kextInfoArray);
        
    cacheVersionValue = __kOSKextIdentifierCacheCurrentVersion;
    cacheVersion = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type,
        &cacheVersionValue);
    if (!kextInfoArray) {
        OSKextLogMemError();
        goto finish;
    }
    CFDictionarySetValue(cacheDict, CFSTR(__kOSKextIdentifierCacheVersionKey),
        cacheVersion);

    count = CFArrayGetCount(kextArray);
    for (i = 0; i < count; i++) {
        SAFE_RELEASE(kextDict);
        aKext = (OSKextRef)CFArrayGetValueAtIndex(kextArray, i);
        kextDict = __OSKextCreateIdentifierCacheDict(aKext, basePath);
        if (!kextDict) {
            continue;
        }
        CFArrayAppendValue(kextInfoArray, kextDict);
    }

    result = _OSKextWriteCache(directoryURL,
        CFSTR(_kOSKextIdentifierCacheBasename),
        /* arch */ NULL, _kOSKextCacheFormatCFBinary,
        cacheDict);

finish:
    SAFE_RELEASE(cacheFileURL);
    SAFE_RELEASE(cacheDict);
    SAFE_RELEASE(kextInfoArray);
    SAFE_RELEASE(kextDict);
    SAFE_RELEASE(cacheVersion);
    SAFE_RELEASE(basePath);
    return result;
}

/*********************************************************************
*********************************************************************/
CFDictionaryRef __OSKextCreateIdentifierCacheDict(
    OSKextRef   aKext,
    CFStringRef basePath)
{
    CFMutableDictionaryRef result        = NULL;
    CFMutableDictionaryRef preResult     = NULL;
    CFURLRef               absURL        = NULL;  // must release
    CFStringRef            bundlePath    = NULL;  // must release
    CFStringRef            relativePath  = NULL;  // must release
    CFStringRef            scratchString = NULL;  // do not release
    char                   bundlePathCString[PATH_MAX];
    char                   basePathCString[PATH_MAX];
    CFIndex                baseLength, fullLength;
    
    preResult = CFDictionaryCreateMutable(CFGetAllocator(aKext), 0, 
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!preResult) {
        OSKextLogMemError();
        goto finish;
    }

    absURL = CFURLCopyAbsoluteURL(OSKextGetURL(aKext));
    if (!absURL) {
        OSKextLogMemError();
        goto finish;
    }
    bundlePath = CFURLCopyFileSystemPath(absURL, kCFURLPOSIXPathStyle);
    if (!bundlePath) {
        OSKextLogMemError();
        goto finish;
    }
    
    if (!CFStringHasPrefix(bundlePath, basePath)) {
        CFStringGetCString(bundlePath, bundlePathCString,
            sizeof(bundlePathCString), kCFStringEncodingUTF8);
        CFStringGetCString(basePath, basePathCString,
            sizeof(basePathCString), kCFStringEncodingUTF8);
            
        OSKextLog(aKext,
            kOSKextLogErrorLevel | kOSKextLogKextBookkeepingFlag,
            "%s not in base path %s for identifier->path cache.",
            bundlePathCString, basePathCString);
    }

    fullLength = CFStringGetLength(bundlePath);
    baseLength = 1 + CFStringGetLength(basePath);  // +1 for the final slash
    relativePath = CFStringCreateWithSubstring(CFGetAllocator(aKext),
        bundlePath,
        CFRangeMake(baseLength, fullLength - baseLength));
    if (!relativePath) {
        OSKextLogMemError();
        goto finish;
    }

    CFDictionarySetValue(preResult, CFSTR("OSBundlePath"), relativePath);

    scratchString = OSKextGetIdentifier(aKext);
    if (!scratchString) {
        goto finish;
    }
    CFDictionarySetValue(preResult, kCFBundleIdentifierKey, scratchString);

    scratchString = OSKextGetValueForInfoDictionaryKey(aKext,
        kCFBundleVersionKey);
    if (!scratchString) {
        goto finish;
    }
    CFDictionarySetValue(preResult, kCFBundleVersionKey, scratchString);

   /* Only save nonzero log flags, to save file space.
    */
    if (aKext->flags.loggingEnabled) {
        CFDictionarySetValue(preResult, CFSTR(kOSBundleEnableKextLoggingKey),
            kCFBooleanTrue);
    }

    result = preResult;
    preResult = NULL;
    
finish:
    SAFE_RELEASE(preResult);
    SAFE_RELEASE(absURL);
    SAFE_RELEASE(bundlePath);
    SAFE_RELEASE(relativePath);
    return result;
}

#pragma mark Instance Management (Continued)
/*********************************************************************
* Instance Management (Continued)
*********************************************************************/

/*********************************************************************
*********************************************************************/
OSKextRef OSKextCreateWithIdentifier(
    CFAllocatorRef allocator,
    CFStringRef    kextIdentifier)
{
    OSKextRef       result = NULL;

    CFArrayRef      kextIDs           = NULL;  // must release
    CFDictionaryRef loadedKextsInfo   = NULL;  // must release
    CFDictionaryRef loadedKextInfo    = NULL;  // do not release
    CFStringRef     kextPath          = NULL;  // do not release
    CFURLRef        createdKextURL    = NULL;  // must release
    OSKextRef       createdKext       = NULL;  // must release
    CFArrayRef      allSystemKexts    = NULL;  // must release
    OSKextRef       lookedUpKext      = NULL;  // do not release
    CFStringRef     kextVersionString = NULL;  // do not release
    OSKextVersion   kextVersion       = -1;
    char          * pathCString       = NULL;  // must free

   /* Check with the kernel first to ensure correct info
    * about a loaded kext is returned.
    */
    kextIDs = CFArrayCreate(kCFAllocatorDefault,
        (const void **)&kextIdentifier, /* numValues */ 1,
        &kCFTypeArrayCallBacks);
    if (!kextIDs) {
        OSKextLogMemError();
        goto finish;
    }
    
    do {
        loadedKextsInfo = OSKextCopyLoadedKextInfo(kextIDs, __sOSKextInfoEssentialKeys);
        if (!loadedKextsInfo || (CFGetTypeID(loadedKextsInfo) != CFDictionaryGetTypeID())) {
            break;
        }
            
        loadedKextInfo = (CFDictionaryRef)CFDictionaryGetValue(loadedKextsInfo, kextIdentifier);
        if (!loadedKextInfo) {
            break;
        }
        if (CFGetTypeID(loadedKextInfo) != CFDictionaryGetTypeID()) {
            break;
        }
        kextPath = (CFStringRef)CFDictionaryGetValue(loadedKextInfo,
            CFSTR(kOSBundlePathKey));
        if (!kextPath || CFGetTypeID(kextPath) != CFStringGetTypeID()) {
            kextPath = NULL;
        }
        kextVersionString = (CFStringRef)CFDictionaryGetValue(
            loadedKextInfo, kCFBundleVersionKey);
        if (!kextVersionString || CFGetTypeID(kextVersionString) != CFStringGetTypeID()) {
            kextVersionString = NULL;
        }
        kextVersion = OSKextParseVersionCFString(kextVersionString);

    } while (0);

   /* If we got a path for the kext from the kernel, confirm that we can
    * open up the bundle and that its identifier is indeed the one requested.
    */
    if (kextPath) {
        pathCString = createUTF8CStringForCFString(kextPath);
        OSKextLog(/* kext */ NULL,
            kOSKextLogDebugLevel | kOSKextLogKextBookkeepingFlag,
            "Creating kext with path %s.", pathCString);
        createdKextURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
            kextPath, kCFURLPOSIXPathStyle, /* isDir? */ true);
        if (!createdKextURL) {
            OSKextLogMemError();
            goto finish;
        }
        createdKext = OSKextCreate(allocator, createdKextURL);
        if (result && CFEqual(OSKextGetIdentifier(result), kextIdentifier)) {
            result = (OSKextRef)CFRetain(createdKext);  // we will be releasing it
        }
    }

    if (!result) {

       /* No luck finding the kext from the path in the kernel.
        * Try with the ID and version from the kernel.
        * Failing that, just try the ID.
        *
        * xxx - the API doesn't really say we need to check the version....
        */
        allSystemKexts = OSKextCreateKextsFromURLs(allocator,
            OSKextGetSystemExtensionsFolderURLs());
        
        if (kextVersion != -1) {
            lookedUpKext = OSKextGetKextWithIdentifierAndVersion(kextIdentifier,
                kextVersion);
        }
        if (!lookedUpKext) {
            lookedUpKext = OSKextGetKextWithIdentifier(kextIdentifier);
        }
        if (lookedUpKext) {
            result = (OSKextRef)CFRetain(lookedUpKext);
        }
    }

   /* xxx - This really shouldn't affect any kexts other than the one we are
    * returning, but it affects all w/same identifier.
    */
    if (result && loadedKextInfo) {
        __OSKextProcessLoadInfo(kextIdentifier, loadedKextInfo, /* context */ NULL);
    }

finish:
    SAFE_FREE(pathCString);

    SAFE_RELEASE(kextIDs);
    SAFE_RELEASE(loadedKextsInfo);
    SAFE_RELEASE(createdKextURL);
    SAFE_RELEASE(createdKext);
    SAFE_RELEASE(allSystemKexts);

    return result;
}

/*********************************************************************
*********************************************************************/
CFArrayRef OSKextGetAllKexts(void)
{
    pthread_once(&__sOSKextInitialized, __OSKextInitialize);

    if (__sOSAllKexts) {
        CFArrayApplyFunction(__sOSAllKexts, RANGE_ALL(__sOSAllKexts), 
            &__OSKextRealize, /* context */ NULL);
    }
    return __sOSAllKexts;
}

/*********************************************************************
*********************************************************************/
OSKextRef OSKextGetKextWithURL(
    CFURLRef anURL)
{
    OSKextRef   result        = NULL;
    CFURLRef    canonicalURL  = NULL;  // must release
    char        relPath[PATH_MAX];
    char        absPath[PATH_MAX];

    pthread_once(&__sOSKextInitialized, __OSKextInitialize);

   /* A bit of paranoia, perhaps.
    */
    if (!__OSKextGetFileSystemPath(/* kext */ NULL, /* otherURL */ anURL,
            /* resolveToBase */ false, relPath) ||
        !__OSKextGetFileSystemPath(/* kext */ NULL, /* otherURL */ anURL,
            /* resolveToBase */ true, absPath)) {

        goto finish;
    }

   /* Canonicalize the URL so we can use it for a dictionary lookup.
    */
    canonicalURL = CFURLCreateFromFileSystemRepresentation(CFGetAllocator(anURL),
        (uint8_t *)absPath, strlen(absPath), /* isDir */ true);
    if (!canonicalURL) {
        OSKextLogMemError();
        goto finish;
    }

   /* Check if we already have this URL.
    */
    if (__sOSKextsByURL) {
        result = (OSKextRef)CFDictionaryGetValue(__sOSKextsByURL, canonicalURL);
        if (result) {

           /* Realize it from the identifier cache as needed. We don't
            * care about the result if the realize fails, even if the bundle's
            * gone missing, because there's already retained instances out
            * there somewhere.
            */
            if (result->staticFlags.isFromIdentifierCache) {
                __OSKextRealize(result, /* context */ NULL);
            }
            goto finish;
        }
    }
finish:
    SAFE_RELEASE(canonicalURL);
    return result;
}

/*********************************************************************
*********************************************************************/
OSKextRef OSKextGetKextWithIdentifier(
    CFStringRef aBundleID)
{
    OSKextRef result     = NULL;
    CFTypeRef foundEntry = NULL;  // do not release
    OSKextRef theKext    = NULL;  // do not release

   /* No need to init the library if there's nothing to get!
    */
    if (!__sOSKextsByIdentifier) {
        goto finish;
    }

   /* Make sure the lookup dict only contains realized kexts with the
    * requested identifier.
    */
    __OSKextRealizeKextsWithIdentifier(aBundleID);

    foundEntry = CFDictionaryGetValue(__sOSKextsByIdentifier, aBundleID);
    if (!foundEntry) {
         goto finish;
    }

    if (OSKextGetTypeID() == CFGetTypeID(foundEntry)) {
        theKext = (OSKextRef)foundEntry;
        
        result = theKext;

    } else if (CFArrayGetTypeID() == CFGetTypeID(foundEntry)) {
        CFMutableArrayRef kexts = (CFMutableArrayRef)foundEntry;

        if (CFArrayGetCount(kexts)) {
            result = (OSKextRef)CFArrayGetValueAtIndex(kexts, 0);
        }
    }

finish:
    return result;
}

/*********************************************************************
* XXX - Should this look for a loaded kext amongst duplicates?
*********************************************************************/
OSKextRef OSKextGetKextWithIdentifierAndVersion(
    CFStringRef aBundleID, OSKextVersion aVersion)
{
    OSKextRef result     = NULL;
    CFTypeRef foundEntry = NULL;  // do not release
    OSKextRef theKext    = NULL;  // do not release

   /* No need to init the library if there's nothing to get!
    */
    if (!__sOSKextsByIdentifier) {
        goto finish;
    }

   /* Make sure the lookup dict only contains realized kexts with the
    * requested identifier.
    */
    __OSKextRealizeKextsWithIdentifier(aBundleID);

    foundEntry = CFDictionaryGetValue(__sOSKextsByIdentifier, aBundleID);
    if (!foundEntry) {
         goto finish;
    }

    if (OSKextGetTypeID() == CFGetTypeID(foundEntry)) {
        theKext = (OSKextRef)foundEntry;

        if (theKext->version == aVersion) {
            result = theKext;
        }

    } else if (CFArrayGetTypeID() == CFGetTypeID(foundEntry)) {
        CFMutableArrayRef kexts = (CFMutableArrayRef)foundEntry;
        CFIndex count, i;

        count = CFArrayGetCount(kexts);
        for (i = 0; i < count; i++) {
            theKext = (OSKextRef)CFArrayGetValueAtIndex(kexts, i);
            if (theKext->version == aVersion) {
                result = theKext;
                goto finish;
            }
        }
    }

finish:
    return result;
}

/*********************************************************************
* All loaded kexts for a given identifier are theoretically the same
* (version, UUID), but that's as much verification as we ever do.
*********************************************************************/
OSKextRef OSKextGetLoadedKextWithIdentifier(
    CFStringRef aBundleID)
{
    OSKextRef result     = NULL;
    CFTypeRef foundEntry = NULL;  // do not release
    OSKextRef theKext    = NULL;  // do not release

   /* Make sure the lookup dict only contains realized kexts with the
    * requested identifier.
    */
    __OSKextRealizeKextsWithIdentifier(aBundleID);

    foundEntry = CFDictionaryGetValue(__sOSKextsByIdentifier, aBundleID);
    if (!foundEntry) {
         goto finish;
    }

    if (OSKextGetTypeID() == CFGetTypeID(foundEntry)) {
        theKext = (OSKextRef)foundEntry;

        if (OSKextIsLoaded(theKext)) {
            result = theKext;
        }
    } else if (CFArrayGetTypeID() == CFGetTypeID(foundEntry)) {
        CFMutableArrayRef kexts = (CFMutableArrayRef)foundEntry;
        CFIndex           count, i;

        count = CFArrayGetCount(kexts);
        for (i = 0; i < count; i++) {
            theKext = (OSKextRef)CFArrayGetValueAtIndex(kexts, i);
            if (OSKextIsLoaded(theKext)) {
                result = theKext;
                goto finish;
            }
        }
    }

finish:
    return result;
}

/*********************************************************************
XXX - Should this check valid/authentic flags and skip failed kexts?
*********************************************************************/
OSKextRef OSKextGetCompatibleKextWithIdentifier(
    CFStringRef   aBundleID,
    OSKextVersion requestedVersion)
{
    OSKextRef result     = NULL;
    CFTypeRef foundEntry = NULL;  // do not release

   /* No need to init the library if there's nothing to get!
    */
    if (!__sOSKextsByIdentifier) {
        goto finish;
    }

   /* Make sure the lookup dict only contains realized kexts with the
    * requested identifier.
    */
    __OSKextRealizeKextsWithIdentifier(aBundleID);

    foundEntry = CFDictionaryGetValue(__sOSKextsByIdentifier, aBundleID);
    if (!foundEntry) {
         goto finish;
    }

    if (OSKextGetTypeID() == CFGetTypeID(foundEntry)) {
        OSKextRef theKext = (OSKextRef)foundEntry;

        if (OSKextIsCompatibleWithVersion(theKext, requestedVersion)) {
            result = theKext;
        }

    } else if (CFArrayGetTypeID() == CFGetTypeID(foundEntry)) {
        CFMutableArrayRef kexts = (CFMutableArrayRef)foundEntry;
        CFIndex count, i;

        count = CFArrayGetCount(kexts);
        for (i = 0; i < count; i++) {
            OSKextRef thisKext = (OSKextRef)CFArrayGetValueAtIndex(kexts, i);
            if (OSKextIsCompatibleWithVersion(thisKext, requestedVersion)) {
                result = thisKext;
                goto finish;
            }
        }
    }

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
CFArrayRef OSKextCopyKextsWithIdentifier(
    CFStringRef aBundleID)
{
    CFArrayRef result       = NULL;
    CFTypeRef  foundEntry   = NULL;  // do not release
    CFArrayRef realizeArray = NULL;  // must release

   /* Note that this function always returns an array, even if there
    * are no kexts, so this test is different from previous retrieval
    * functions.
    */
    pthread_once(&__sOSKextInitialized, __OSKextInitialize);

   /* Make sure the lookup dict only contains realized kexts with the
    * requested identifier.
    */
    __OSKextRealizeKextsWithIdentifier(aBundleID);

    if (__sOSKextsByIdentifier) {
        foundEntry = CFDictionaryGetValue(__sOSKextsByIdentifier, aBundleID);
    }

    if (!foundEntry) {
        goto finish;

    } else if (OSKextGetTypeID() == CFGetTypeID(foundEntry)) {
        OSKextRef theKext = (OSKextRef)foundEntry;

        result = CFArrayCreate(kCFAllocatorDefault, (const void **)&theKext, 1,
            &kCFTypeArrayCallBacks);

    } else if (CFArrayGetTypeID() == CFGetTypeID(foundEntry)) {
        CFMutableArrayRef kexts = (CFMutableArrayRef)foundEntry;

        result = CFArrayCreateCopy(kCFAllocatorDefault, kexts);
    }

finish:
    SAFE_RELEASE(realizeArray);
    if (!result) {
        result = CFArrayCreate(kCFAllocatorDefault, NULL, 0,
            &kCFTypeArrayCallBacks);
    }
    return result;
}

/*********************************************************************
 *********************************************************************/
CFComparisonResult __OSKextBundleIDCompare(const void *val1,
    const void *val2, void *context __unused)
{ 
    return CFStringCompare(val1, val2, 0);
}

/*********************************************************************
 *********************************************************************/
CFArrayRef OSKextCopyAllRequestedIdentifiers(void)
{
    CFMutableArrayRef      result = NULL;
    CFRange                resultRange;
    CFSetRef               requestedIdentifiers = NULL;  // must release
    OSReturn               op_result            = kOSReturnError;
    CFMutableDictionaryRef requestDict          = NULL;  // must release
    const void           ** values              = NULL;  // must free
    int                    i                    = 0;
    
    OSKextLog(/* kext */ NULL,
        kOSKextLogDebugLevel | kOSKextLogIPCFlag,
        "Reading list of all kexts requested by kernel since startup.");
    
    /* Create the kext request to get the bundle IDs of all load requests */
    
    requestDict = __OSKextCreateKextRequest(
        CFSTR(kKextRequestPredicateGetAllLoadRequests),
        /* bundleID */ NULL, /* argsOut */ NULL);
    
    /* Execute the load request and validate that we got a CFSet back */
    
    op_result = __OSKextSendKextRequest(/* kext */ NULL, requestDict,
        (CFTypeRef *)&requestedIdentifiers, /* rawResponseOut */ NULL, 
        /* rawResponseLengthOut */ NULL);
    if (op_result != kOSReturnSuccess) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogIPCFlag,
            "Failed to read kexts requested by kernel since startup - %s.",
            safe_mach_error_string(op_result));
        goto finish;
    }
    
    if (!requestedIdentifiers || 
        CFSetGetTypeID() != CFGetTypeID(requestedIdentifiers)) 
    {
        goto finish;
    }
    
    /* Create a temporary array that we'll use to copy the bundle IDs from
     * the CFSet to a CFArray.
     */
    
    values = malloc(CFSetGetCount(requestedIdentifiers) * sizeof(*values));
    if (!values) {
        OSKextLogMemError();
        goto finish;
    }
    
    /* Create a new CFArray to return the identifiers in */
    
    CFSetGetValues(requestedIdentifiers, values);
    
    result = CFArrayCreateMutable(kCFAllocatorDefault,
        CFSetGetCount(requestedIdentifiers), &kCFTypeArrayCallBacks);
    if (!result) {
        OSKextLogMemError();
        goto finish;
    }

    for (i = 0; i < CFSetGetCount(requestedIdentifiers); ++i) {
        CFArrayAppendValue(result, values[i]);
    }

    /* Sort the array to make the order reproducible */

    resultRange.location = 0;
    resultRange.length = CFArrayGetCount(result);
    CFArraySortValues(result, resultRange, &__OSKextBundleIDCompare, NULL);

finish:
    SAFE_RELEASE(requestDict);
    SAFE_RELEASE(requestedIdentifiers);
    SAFE_FREE(values);
        
    return result;
}

/*********************************************************************
 *********************************************************************/
CFMutableArrayRef OSKextCopyKextsWithIdentifiers(CFArrayRef kextIdentifiers)
{
    CFMutableArrayRef   result                  = NULL;
    CFMutableArrayRef   kexts                   = NULL;  // must release
    CFArrayRef          kextsWithIdentifier     = NULL;  // must release
    char              * kextIdentifierCString   = NULL;  // must free
    int                 count, i;
    
    /* Create an array to hold the kext objects */
    
    count = CFArrayGetCount(kextIdentifiers);
    kexts = CFArrayCreateMutable(kCFAllocatorDefault, 
        count, &kCFTypeArrayCallBacks);
    if (!kexts) {
        OSKextLogMemError();
        goto finish;
    }
    
    /* Find a kext for each of the bundle identifiers */
    
    for (i = 0; i < count; ++i) {
        CFStringRef kextIdentifier = NULL;

        SAFE_RELEASE_NULL(kextsWithIdentifier);
        SAFE_FREE_NULL(kextIdentifierCString);
        kextIdentifier = CFArrayGetValueAtIndex(kextIdentifiers, i);

        kextsWithIdentifier = OSKextCopyKextsWithIdentifier(kextIdentifier);
        if (kextsWithIdentifier) {
            CFArrayAppendArray(kexts, kextsWithIdentifier,
                RANGE_ALL(kextsWithIdentifier));
        } else {
            kextIdentifierCString = createUTF8CStringForCFString(kextIdentifier);
            OSKextLog(/* kext */ NULL,
                kOSKextLogDebugLevel | kOSKextLogKextBookkeepingFlag,
                "Note: OSKextCopyKextsWithIdentifiers() - identifier %s not found.",
                kextIdentifierCString);
        }
    }
    
    result = kexts;
    kexts = NULL;
    
finish:
    SAFE_RELEASE(kexts);
    SAFE_RELEASE(kextsWithIdentifier);
    SAFE_FREE(kextIdentifierCString);
    
    return result;
}

/*********************************************************************
 *********************************************************************/
CFMutableArrayRef OSKextCopyLoadListForKexts(
    CFArrayRef kexts,
    Boolean    needAllFlag)
{
    CFMutableArrayRef result         = NULL;
    CFMutableArrayRef globalLoadList = NULL;
    CFMutableSetRef   resolvedKexts  = NULL;
    CFArrayRef        loadList       = NULL;
    CFIndex           kextCount, loadListCount, i, j;
    
    /* Create a set to track the kexts whose dependencies have been resolved */
    
    resolvedKexts = CFSetCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeSetCallBacks);
    if (!resolvedKexts) {
        OSKextLogMemError();
        goto finish;
    }
    
    /* Create the global load list */
    
    globalLoadList = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!globalLoadList) {
        OSKextLogMemError();
        goto finish;
    }
    
    /* Generate the global load list */
    
    kextCount = CFArrayGetCount(kexts);
    for (i = 0; i < kextCount; ++i) {
        Boolean valid     = false;
        
        SAFE_RELEASE_NULL(loadList);
        
        OSKextRef theKext = (OSKextRef) CFArrayGetValueAtIndex(kexts, i);
        
       /* If we've already determined this kext's load order, skip it.
        */
        if (CFSetGetValue(resolvedKexts, theKext)) continue;

       /* Skip kexts that we can't possibly load.
        */
        valid = __OSKextIsValid(theKext);
        
        if (!valid) {
            char kextPath[PATH_MAX];
            OSKextLogSpec logLevel = needAllFlag ? kOSKextLogErrorLevel :
                kOSKextLogWarningLevel;
            
            __OSKextGetFileSystemPath(theKext, /* otherURL */ NULL,
                /* resolveToBase */ FALSE, kextPath);

            OSKextLog(theKext,
                logLevel | kOSKextLogGeneralFlag | kOSKextLogArchiveFlag,
                "%s is not valid.",
                kextPath);

            if (needAllFlag) {
                goto finish;
            } else {
                continue;
            }
        }

       /* Determine the dependency graph for this kext.
        */
        loadList = OSKextCopyLoadList(theKext, needAllFlag);
        if (!loadList) {
            goto finish;
        }

        loadListCount = CFArrayGetCount(loadList);
        for (j = 0; j < loadListCount; ++j) {
            
            OSKextRef listKext = (OSKextRef)CFArrayGetValueAtIndex(loadList, j);
            
           /* If we've already determined this kext's load order, skip it.
            */
            if (CFSetGetValue(resolvedKexts, listKext)) continue;
            
           /* Add the kext to the global load list and the set of resolved kexts.
            */
            CFArrayAppendValue(globalLoadList, listKext);
            CFSetSetValue(resolvedKexts, listKext);
        }
    }
    
    result = globalLoadList;
    globalLoadList = NULL;
    
finish:
    SAFE_RELEASE(resolvedKexts);
    SAFE_RELEASE(globalLoadList);
    SAFE_RELEASE(loadList);
    
    return result;
}

#pragma mark Basic Accessors
/*********************************************************************
* Basic Accessors
*********************************************************************/

/*********************************************************************
*********************************************************************/
CFURLRef OSKextGetURL(OSKextRef aKext)
{
    return aKext->bundleURL;
}

/*********************************************************************
* Get the C string path for a kext's bundleURL, or for an arbitrary
* URL passed in. Should only ever give one or the other, but if both
* are given, the URL wins.
*
* pathBuffer length assumed to be PATH_MAX
*********************************************************************/
Boolean __OSKextGetFileSystemPath(
    OSKextRef aKext,
    CFURLRef  anURL,
    Boolean   resolveToBase,
    char    * pathBuffer)
{
    Boolean  result   = false;
    CFURLRef urlToUse = NULL;  // do not release

    if (aKext) {
        if (aKext->bundleURL) {
            urlToUse = aKext->bundleURL;
        }
    } else {
        urlToUse = anURL;
    }
    if (!urlToUse) {
        goto finish;
    }
    result = CFURLGetFileSystemRepresentation(urlToUse,
        resolveToBase, (UInt8 *)pathBuffer, PATH_MAX);

finish:
    if (!result) {
        OSKextLogStringError(aKext);
        memcpy(pathBuffer, __kStringUnknown, sizeof(__kStringUnknown));
    }
    return result;
}

/*********************************************************************
*********************************************************************/
CFStringRef OSKextGetIdentifier(OSKextRef aKext)
{
    return aKext->bundleID;
}

/*********************************************************************
*********************************************************************/
#define COMPOSITE_KEY_SEPARATOR  "_"

CFStringRef __OSKextCreateCompositeKey(
    CFStringRef  baseKey,
    const char * auxKey)
{
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
        CFSTR("%@%s%s"), baseKey, COMPOSITE_KEY_SEPARATOR, auxKey);
}

/*********************************************************************
*********************************************************************/
CFTypeRef __CFDictionaryGetValueForCompositeKey(
    CFDictionaryRef aDict,
    CFStringRef     baseKey,
    const char *    auxKey)
{
    CFTypeRef   result = NULL;
    CFStringRef compositeKey = NULL;  // must release

    compositeKey = __OSKextCreateCompositeKey(baseKey, auxKey);
    if (!compositeKey) {
        OSKextLogMemError();
        goto finish;
    }

    result = CFDictionaryGetValue(aDict, compositeKey);
    if (!result) {
        result = CFDictionaryGetValue(aDict, baseKey);
    }

finish:
    SAFE_RELEASE(compositeKey);
    return result;
}

/*********************************************************************
*********************************************************************/
CFTypeRef OSKextGetValueForInfoDictionaryKey(
    OSKextRef   aKext,
    CFStringRef key)
{
    CFTypeRef result = NULL;

    if (!__OSKextReadInfoDictionary(aKext, NULL)) {
        goto finish;
    }

   /* We only do arch-specific properties for keys within the domain
    * of kexts/OSBundle/IOKit.
    */
    if (CFStringHasPrefix(key, CFSTR("OS")) ||
        CFStringHasPrefix(key, CFSTR("IO"))) {

       /* Only use the generic CPU type, not the subtype, for arch-specific
        * properties. (Do not free lookupArchInfo.)
        */
        const NXArchInfo * lookupArchInfo = NXGetArchInfoFromCpuType(
            OSKextGetArchitecture()->cputype, CPU_SUBTYPE_MULTIPLE);

        if (lookupArchInfo) {
            result = __CFDictionaryGetValueForCompositeKey(aKext->infoDictionary,
                key, lookupArchInfo->name);
        }

    }
    
    if (!result) {
        result = CFDictionaryGetValue(aKext->infoDictionary, key);
    }

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
CFMutableDictionaryRef OSKextCopyInfoDictionary(OSKextRef aKext)
{
    CFMutableDictionaryRef result = NULL;

    if (!aKext->infoDictionary) {
        if (!__OSKextReadInfoDictionary(aKext, NULL)) {
            goto finish;
        }
    }

    result = CFDictionaryCreateMutableCopy(CFGetAllocator(aKext), 0,
        aKext->infoDictionary);

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
static void __OSKextFlushInfoDictionaryApplierFunction(
    const void * vKey __unused,
    const void * vValue,
          void * vContext __unused)
{
    OSKextRef theKext = (OSKextRef)vValue;

    OSKextFlushInfoDictionary(theKext);
    return;
}

void OSKextFlushInfoDictionary(OSKextRef aKext)
{
    static Boolean flushingAll = false;
    char           kextPath[PATH_MAX];

    pthread_once(&__sOSKextInitialized, __OSKextInitialize);

    if (aKext) {
        if (!flushingAll) {
            if (OSKextGetURL(aKext)) {
                __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
                    /* resolveToBase */ false, kextPath);
            }
            OSKextLog(/* kext */ NULL,
                kOSKextLogDetailLevel | kOSKextLogKextBookkeepingFlag,
                "Flushing info dictionary for %s.",
                kextPath);
        }
        if (!OSKextIsFromMkext(aKext)) {
            SAFE_RELEASE_NULL(aKext->infoDictionary);

           /* The info dict could change by the time we read it again,
            * so clear all validation/authentication flags. Leave
            * diagnostics in place (a bit funky I suppose).
            */
            aKext->flags.valid = 0;
            aKext->flags.invalid = 0;
            aKext->flags.validated = 0;
            aKext->flags.authentic = 0;
            aKext->flags.inauthentic = 0;
            aKext->flags.authenticated = 0;
        }

    } else if (__sOSKextsByURL) {
        flushingAll = true;
        OSKextLog(/* kext */ NULL,
            kOSKextLogDetailLevel | kOSKextLogKextBookkeepingFlag,
            "Flushing info dictionaries for all kexts.");
        CFDictionaryApplyFunction(__sOSKextsByURL,
            __OSKextFlushInfoDictionaryApplierFunction, NULL);
        flushingAll = false;
    }
    return;
}

/*********************************************************************
*********************************************************************/
OSKextVersion OSKextGetVersion(OSKextRef aKext)
{
    return aKext->version;
}

/*********************************************************************
*********************************************************************/
OSKextVersion OSKextGetCompatibleVersion(OSKextRef aKext)
{
    return aKext->compatibleVersion;
}

/*********************************************************************
*********************************************************************/
struct _uuid_stuff {
    unsigned int   uuid_size;
    char         * uuid;
};

macho_seek_result __OSKextUUIDCallback(
    struct load_command * load_command,
    const void * file_end,
    uint8_t swap __unused,
    void * user_data)
{
    struct _uuid_stuff * uuid_stuff = (struct _uuid_stuff *)user_data;
    if (load_command->cmd == LC_UUID) {
        struct uuid_command * uuid_command = (struct uuid_command *)load_command;
        if (((void *)load_command + load_command->cmdsize) > file_end) {
            return macho_seek_result_error;
        }
        uuid_stuff->uuid_size = sizeof(uuid_command->uuid);
        uuid_stuff->uuid = (char *)uuid_command->uuid;
        return macho_seek_result_found;
    }
    return macho_seek_result_not_found;
}

CFDataRef OSKextCopyUUIDForArchitecture(
    OSKextRef          aKext,
    const NXArchInfo * arch)
{
    CFDataRef                  result = NULL;
    CFDataRef                  executable = NULL;  // must release
    const struct mach_header * mach_header = NULL;
    const void               * file_end;
    macho_seek_result          seek_result;
    struct _uuid_stuff         seek_uuid;
    int swap = 0;

    // xxx - would we want to cache this, is it going to be accessed a lot?

    if (!arch) {
        arch = OSKextGetArchitecture();
    }

    executable = OSKextCopyExecutableForArchitecture(aKext, arch);
    if (!executable) {
        goto finish;
    }

    mach_header = (const struct mach_header *)CFDataGetBytePtr(executable);
    file_end = (((const char *)mach_header) + CFDataGetLength(executable));

    if (ISSWAPPEDMACHO(MAGIC32(mach_header))) {
        swap = 1;
    }

    seek_result = macho_scan_load_commands(
        mach_header, file_end,
        __OSKextUUIDCallback, (const void **)&seek_uuid);
    if (seek_result == macho_seek_result_error) {
        __OSKextSetDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
            kOSKextDiagnosticExecutableBadKey);
        goto finish;
    } else if (seek_result != macho_seek_result_found) {
        // ok for there to not be a uuid
        goto finish;
    }

    result = CFDataCreate(CFGetAllocator(aKext), (u_char *)seek_uuid.uuid,
        CondSwapInt32(swap, seek_uuid.uuid_size));

finish:

   /* Advise the system that we no longer need the mmapped executable.
    */
    if (executable) {
        (void)posix_madvise((void *)CFDataGetBytePtr(executable),
            CFDataGetLength(executable),
            POSIX_MADV_DONTNEED);
    }

    SAFE_RELEASE(executable);
    return result;
}

/*********************************************************************
*********************************************************************/
Boolean  OSKextIsKernelComponent(OSKextRef aKext)
{
    return aKext->flags.isKernelComponent ? true : false;

}

/*********************************************************************
*********************************************************************/
Boolean  OSKextIsInterface(OSKextRef aKext)
{
    return aKext->flags.isInterface ? true : false;
}

/*********************************************************************
*********************************************************************/
Boolean  OSKextIsLibrary(OSKextRef aKext)
{
    return (aKext->compatibleVersion > 0) ? true : false;
}

/*********************************************************************
*********************************************************************/
Boolean  OSKextDeclaresExecutable(OSKextRef aKext)
{
    return aKext->flags.declaresExecutable ? true : false;
}

/*********************************************************************
*********************************************************************/
Boolean  OSKextHasLogOrDebugFlags(OSKextRef aKext)
{
    return aKext->flags.plistHasEnableLoggingSet ||
        aKext->flags.plistHasIOKitDebugFlags;
}

/*********************************************************************
*********************************************************************/
Boolean OSKextIsLoggingEnabled(OSKextRef aKext)
{
    return aKext->flags.loggingEnabled;
}

/*********************************************************************
*********************************************************************/
void OSKextSetLoggingEnabled(
    OSKextRef aKext,
    Boolean   flag)
{
    unsigned int oldValue = aKext->flags.loggingEnabled;

    aKext->flags.loggingEnabled = flag ? 1 : 0;

    if (oldValue != aKext->flags.loggingEnabled) {
        char kextPath[PATH_MAX];
        __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
            /* resolveToBase */ false, kextPath);
        OSKextLog(aKext,
            kOSKextLogDebugLevel | kOSKextLogKextBookkeepingFlag,
            "Kext logging %sabled for %s.",
            aKext->flags.loggingEnabled ? "en" : "dis",
            kextPath);
    }
    return;
}

/*********************************************************************
*********************************************************************/
Boolean OSKextIsLoadableInSafeBoot(OSKextRef aKext)
{
    return aKext->flags.isLoadableInSafeBoot ? true : false;
}

/*********************************************************************
*********************************************************************/
Boolean OSKextDependenciesAreLoadableInSafeBoot(OSKextRef aKext)
{
    Boolean     result = true;
    CFArrayRef  allDependencies = NULL;  // must release
    CFIndex     count, i;

    allDependencies = OSKextCopyAllDependencies(aKext,
        /* needAll */ true);
    if (!allDependencies) {
        result = false;
        goto finish;
    }

    count = CFArrayGetCount(allDependencies);
    for (i = 0; i < count; i++) {
        OSKextRef thisKext = (OSKextRef)CFArrayGetValueAtIndex(
            allDependencies, i);

        if ((OSKextGetActualSafeBoot() || OSKextGetSimulatedSafeBoot()) &&
            !OSKextIsLoadableInSafeBoot(thisKext)) {

            __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagBootLevel,
                kOSKextDependencyIneligibleInSafeBoot,
                OSKextGetIdentifier(thisKext), /* note */ NULL);
            result = false;
        }
    }

finish:
    SAFE_RELEASE(allDependencies);

    return result;
}

/*********************************************************************
*********************************************************************/
const NXArchInfo ** OSKextCopyArchitectures(OSKextRef aKext)
{
    const NXArchInfo  ** result        = NULL;
    const UInt8        * executable    = NULL;  // do not free
    const UInt8        * executableEnd = NULL;  // do not free
    fat_iterator         fatIterator   = NULL;  // must fat_iterator_close
    char                 urlPath[PATH_MAX];
    int                  numArches     = 0;
    int                  resultSize    = 0;
    struct mach_header * machHeader    = NULL;
    uint32_t             index;

    if (!OSKextDeclaresExecutable(aKext) || !__OSKextReadExecutable(aKext)) {
        goto finish;
    }

    executable = CFDataGetBytePtr(aKext->loadInfo->executable);
    executableEnd = executable + CFDataGetLength(aKext->loadInfo->executable);
    fatIterator = fat_iterator_for_data(executable, executableEnd,
        1 /* mach-o only */);

    if (!fatIterator) {
        __OSKextGetFileSystemPath(aKext,
        /* otherURL */ _CFBundleCopyExecutableURLInDirectory(OSKextGetURL(aKext)),
        /* resolveToBase */ false, urlPath);

        OSKextLog(aKext,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Can't read mach-o file %s.",
            urlPath);
        goto finish;
    }

    numArches = fat_iterator_num_arches(fatIterator);
    resultSize = (1 + numArches) * sizeof(NXArchInfo *);
    result = (const NXArchInfo **)malloc(resultSize);
    if (!result) {
        goto finish;
    }
    
    bzero(result, resultSize);

    index = 0;
    for (index = 0;
         (machHeader = (struct mach_header *)fat_iterator_next_arch(
            fatIterator, NULL));
         index++) {

        int           swap       = ISSWAPPEDMACHO(machHeader->magic);
        cpu_type_t    cputype    = CondSwapInt32(swap, machHeader->cputype);
        cpu_subtype_t cpusubtype = CondSwapInt32(swap, machHeader->cpusubtype);
        result[index] = NXGetArchInfoFromCpuType(cputype, cpusubtype);
    }

finish:
   /* Advise the system that we no longer need the mmapped executable.
    */
    if (executable) {
        (void)posix_madvise((void *)executable, executableEnd - executable,
            POSIX_MADV_DONTNEED);
    }
    if (fatIterator) {
        fat_iterator_close(fatIterator);
    }
    return result;
}

/*********************************************************************
*********************************************************************/
Boolean OSKextSupportsArchitecture(OSKextRef aKext,
    const NXArchInfo * archInfo)
{
    Boolean       result = false;
    CFDataRef     executable     = NULL;  // must release
    fat_iterator  fatIterator    = NULL;  // must fat_iterator_close()
    const UInt8 * exec           = NULL;  // do not free
    void        * thinExec;
    void        * thinExecEnd;

    if (!OSKextDeclaresExecutable(aKext)) {
        result = true;
        goto finish;
    }

    if (!archInfo) {
        archInfo = OSKextGetArchitecture();
    }

    if (!__OSKextReadExecutable(aKext)) {
        goto finish;
    }

    if (aKext->staticFlags.isFromMkext) {
        if (aKext->mkextInfo && aKext->mkextInfo->executable) {
            executable = CFRetain(aKext->mkextInfo->executable);
        }
    } else {
        if (aKext->loadInfo && aKext->loadInfo->executable) {
            executable = CFRetain(aKext->loadInfo->executable);
        }
    }

    if (!executable) {
        goto finish;
    }

    exec = CFDataGetBytePtr(executable);
    fatIterator = fat_iterator_for_data(exec,
        exec + CFDataGetLength(executable), 1 /* mach-o only */);
    if (!fatIterator) {
        goto finish;
    }
    thinExec = fat_iterator_find_arch(fatIterator,
        archInfo->cputype, archInfo->cpusubtype, &thinExecEnd);

    if (!thinExec || (thinExec == thinExecEnd)) {
        goto finish;
    }

    result = true;

finish:
   /* Advise the system that we no longer need the mmapped executable.
    */
    if (executable) {
        (void)posix_madvise((void *)CFDataGetBytePtr(executable),
            CFDataGetLength(executable),
            POSIX_MADV_DONTNEED);
    }

    SAFE_RELEASE(executable);
    if (fatIterator) fat_iterator_close(fatIterator);
    return result;
}

/*********************************************************************
*********************************************************************/
CFArrayRef OSKextCopyPlugins(OSKextRef aKext)
{
    CFArrayRef  result     = NULL;
    CFBundleRef kextBundle = NULL;  // must release
    CFURLRef    pluginsURL = NULL;  // must release
    CFArrayRef  pluginURLs = NULL;   // must release

   /* If aKext is a plugin, don't scan, and return an empty array.
    */
    if (OSKextIsPlugin(aKext)) {
        result = CFArrayCreate(CFGetAllocator(aKext), NULL, 0,
            &kCFTypeArrayCallBacks);
        goto finish;
    }

    kextBundle = CFBundleCreate(kCFAllocatorDefault, aKext->bundleURL);
    if (!kextBundle) {
        goto finish;
    }

    pluginsURL = CFBundleCopyBuiltInPlugInsURL(kextBundle);
    if (!pluginsURL) {
        result = CFArrayCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeArrayCallBacks);
        goto finish;
    }

    result = __OSKextCreateKextsFromURL(kCFAllocatorDefault, pluginsURL,
        aKext, /* createPlugins */ false);

finish:
    SAFE_RELEASE(kextBundle);
    SAFE_RELEASE(pluginsURL);
    SAFE_RELEASE(pluginURLs);
    return result;
}

/*********************************************************************
* OSKextIsPlugin uses a pretty naive algorithm: If any part of the
* path leading to the kext is ".kext/", the kext must be a plugin
* of some other kext.
*********************************************************************/
Boolean OSKextIsPlugin(OSKextRef aKext)
{
    Boolean     result     = false;
    CFURLRef    absURL     = NULL;  // must release
    CFURLRef    parentURL  = NULL;  // must release
    CFStringRef parentPath = NULL;  // must release
    CFRange     findRange;

    if (aKext->staticFlags.isPluginChecked) {
        result = aKext->staticFlags.isPlugin;
        goto finish;
    }

    if (!aKext->bundleURL) {
        OSKextLog(aKext,
            kOSKextLogErrorLevel |
            kOSKextLogGeneralFlag | kOSKextLogKextBookkeepingFlag,
            "Bundle URL unexpectedly NULL!");
        goto finish;
    }

    absURL = CFURLCopyAbsoluteURL(aKext->bundleURL);
    if (!absURL) {
        OSKextLogMemError();
        goto finish;
    }

    parentURL = CFURLCreateCopyDeletingLastPathComponent(
        kCFAllocatorDefault, absURL);
    if (!parentURL) {
        OSKextLogMemError();
        goto finish;
    }
    parentPath = CFURLCopyFileSystemPath(parentURL, kCFURLPOSIXPathStyle);
    if (!parentPath) {
        OSKextLogMemError();
    }
    findRange = CFStringFind(parentPath, CFSTR(__sOSKextFullBundleExtension),
        /* compareOptions */ 0);

    aKext->staticFlags.isPlugin = (findRange.location == kCFNotFound) ? 0 : 1;
    aKext->staticFlags.isPluginChecked = 1;

    result = aKext->staticFlags.isPlugin;

finish:
    SAFE_RELEASE(absURL);
    SAFE_RELEASE(parentURL);
    SAFE_RELEASE(parentPath);
    return result;
}

/*********************************************************************
*********************************************************************/
OSKextRef OSKextCopyContainerForPluginKext(OSKextRef aKext)
{
    OSKextRef   result             = NULL;
    CFURLRef    absURL             = NULL;  // must release
    CFURLRef    parentURL          = NULL;  // must release
    CFStringRef parentPath         = NULL;  // must release
    CFStringRef containerPath      = NULL;  // must release
    CFURLRef    containerURL       = NULL;  // must release
    CFRange     findRange;
    OSKextRef   potentialContainer = NULL;  // must release
    CFBundleRef pContainerBundle   = NULL;  // must release
    CFURLRef    pluginsURL         = NULL;  // must release
    CFURLRef    checkURL           = NULL;  // must release
    char        potentialContainerPath[PATH_MAX];
    char        canonicalPath[PATH_MAX];
    char        scratchPath[PATH_MAX];

    if (aKext->staticFlags.isPluginChecked && !aKext->staticFlags.isPlugin) {
        goto finish;
    }

    absURL = CFURLCopyAbsoluteURL(aKext->bundleURL);
    if (!absURL) {
        OSKextLogMemError();
        goto finish;
    }

    parentURL = CFURLCreateCopyDeletingLastPathComponent(
        kCFAllocatorDefault, absURL);
    if (!parentURL) {
        OSKextLogMemError();
        goto finish;
    }

    parentPath = CFURLCopyFileSystemPath(parentURL, kCFURLPOSIXPathStyle);
    if (!parentPath) {
        OSKextLogMemError();
        goto finish;
    }
    findRange = CFStringFind(parentPath, CFSTR(__sOSKextFullBundleExtension),
        kCFCompareBackwards);

    aKext->staticFlags.isPlugin = (findRange.location == kCFNotFound) ? 0 : 1;
    aKext->staticFlags.isPluginChecked = 1;

    if (!aKext->staticFlags.isPlugin) {
        goto finish;
    }

    containerPath = CFStringCreateWithSubstring(kCFAllocatorDefault,
        parentPath, CFRangeMake(0, findRange.location + findRange.length));
    if (!containerPath) {
        OSKextLogMemError();
        goto finish;
    }
    if (!CFStringGetCString(containerPath, scratchPath, PATH_MAX,
         kCFStringEncodingUTF8)) {

        OSKextLogStringError(aKext);
        goto finish;
    }

    containerURL = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
        (const UInt8 *)scratchPath, strlen(scratchPath), true);
    if (!containerURL) {
        OSKextLogMemError();
        goto finish;
    }
    potentialContainer = OSKextCreate(kCFAllocatorDefault, containerURL);
    if (!potentialContainer) {
        // this may fail, but should we log it?
        goto finish;
    }
    __OSKextGetFileSystemPath(potentialContainer, /* otherURL */ NULL,
        /* resolveToBase */ false, potentialContainerPath);
    OSKextLog(aKext, kOSKextLogDebugLevel | kOSKextLogFileAccessFlag,
        "Opening CFBundle for %s.", potentialContainerPath);
    pContainerBundle = CFBundleCreate(kCFAllocatorDefault,
        potentialContainer->bundleURL);
    if (!pContainerBundle) {
        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Failed to open CFBundle for %s.", potentialContainerPath);
        goto finish;
    }
    pluginsURL = CFBundleCopyBuiltInPlugInsURL(pContainerBundle);
    checkURL = CFURLCreateCopyAppendingPathComponent(kCFAllocatorDefault,
        pluginsURL, CFURLCopyLastPathComponent(aKext->bundleURL), true);
    if (!checkURL) {
        OSKextLogMemError();
        goto finish;
    }

   /* Sigh. CFURL is stupid with regard to CFEqual. We have to canonicalize
    * the thing first.
    * xxx - this might not be enough wrt .. and // and such
    */
    if (!__OSKextGetFileSystemPath(/* kext */ NULL, checkURL,
        /* resolveToBase */ true, canonicalPath)) {

        goto finish;
    }
    SAFE_RELEASE_NULL(checkURL);
    checkURL = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
        (uint8_t *)canonicalPath, strlen(canonicalPath), true);
    if (!checkURL) {
        OSKextLogMemError();
        goto finish;
    }

    if (CFEqual(absURL, checkURL)) {
        result = (OSKextRef)CFRetain(potentialContainer);
    }

finish:
    SAFE_RELEASE(absURL);
    SAFE_RELEASE(parentURL);
    SAFE_RELEASE(parentPath);
    SAFE_RELEASE(containerPath);
    SAFE_RELEASE(containerURL);
    SAFE_RELEASE(potentialContainer);
    if (pContainerBundle) {
        OSKextLog(aKext,
            kOSKextLogDebugLevel | kOSKextLogFileAccessFlag,
            "Releasing CFBundle for %s.",
            potentialContainerPath);
    }
    SAFE_RELEASE(pContainerBundle);
    SAFE_RELEASE(pluginsURL);
    SAFE_RELEASE(checkURL);
    return result;
}


/*********************************************************************
*********************************************************************/
typedef struct {
    OSKextRef         kext;
    CFMutableArrayRef personalities;
} __OSKextPersonalityBundleIdentifierContext;

static void __OSKextPersonalityBundleIdentifierApplierFunction(
    const void * vKey,
    const void * vValue,
          void * vContext)
{
    CFStringRef            personalityName     = (CFStringRef)vKey;
    CFMutableDictionaryRef personality         = (CFMutableDictionaryRef)vValue;
    __OSKextPersonalityBundleIdentifierContext  * context =
        (__OSKextPersonalityBundleIdentifierContext *)vContext;
    OSKextRef              aKext               = context->kext;
    CFMutableArrayRef      personalities       = context->personalities;
    CFStringRef            bundleID            = NULL;  // do not release

    CFMutableDictionaryRef personalityCopy     = NULL;  // must release
    CFStringRef            personalityBundleID = NULL;  // do not release
    char                   kextPath[PATH_MAX];
    char                 * bundleIDCString     = NULL;  // must free
    char                 * personalityCString  = NULL;  // must free

    bundleID = OSKextGetIdentifier(aKext);
    if (!bundleID) {
        goto finish; // xxx - not much we can do, maybe log an error?
    }

   /* Make a copy of the personality, we may be modifying it and we want
    * to leave the kext's copy alone.
    */
    personalityCopy = CFDictionaryCreateMutableCopy(CFGetAllocator(aKext),
        0, personality);
    if (!personalityCopy) {
        OSKextLogMemError();
        goto finish;
    }

   /* If the personality has no bundle identifier, insert that of the
    * containing bundle. If is has one but it's not the same as the
    * containing bundle's, insert that as the personality publisher.
    */
    personalityBundleID = CFDictionaryGetValue(personality,
        kCFBundleIdentifierKey);
    if (!personalityBundleID) {
        CFDictionarySetValue(personalityCopy, kCFBundleIdentifierKey, bundleID);
    } else if (!CFEqual(bundleID, personalityBundleID)) {
        CFDictionarySetValue(personalityCopy, CFSTR(kIOPersonalityPublisherKey),
            bundleID);
    }

   /* Spare the effort of creating the data when not logging by checking
    * before the OSKextLog() call.
    */
    if (__OSKextShouldLog(aKext, kOSKextLogDetailLevel | kOSKextLogLoadFlag)) {
        __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
            /* resolveToBase */ false, kextPath);
        bundleIDCString = createUTF8CStringForCFString(bundleID);
        personalityCString = createUTF8CStringForCFString(personalityName);
        if (!personalityBundleID) {
            OSKextLog(aKext,
                kOSKextLogDetailLevel | kOSKextLogLoadFlag,
                "Adding CFBundleIdentifier %s to %s personality %s.",
                bundleIDCString, kextPath, personalityCString);
        } else if (!CFEqual(bundleID, personalityBundleID)) {
            OSKextLog(aKext,
                kOSKextLogDetailLevel | kOSKextLogLoadFlag,
                "Adding IOBundlePublisher %s to %s personality %s.",
                bundleIDCString, kextPath, personalityCString);
        }
    }

    CFArrayAppendValue(personalities, personalityCopy);

finish:
    SAFE_FREE(bundleIDCString);
    SAFE_FREE(personalityCString);
    SAFE_RELEASE(personalityCopy);
    return;
}

/*********************************************************************
*********************************************************************/
CFArrayRef OSKextCopyPersonalitiesArray(OSKextRef aKext)
{
    CFMutableArrayRef        result            = NULL;
    CFDictionaryRef          personalities     = NULL; // do not release
    __OSKextPersonalityBundleIdentifierContext context;

    result = CFArrayCreateMutable(CFGetAllocator(aKext), 0,
        &kCFTypeArrayCallBacks);
    if (!result) {
        OSKextLogMemError();
        goto finish;
    }

    personalities = OSKextGetValueForInfoDictionaryKey(aKext,
        CFSTR(kIOKitPersonalitiesKey));
    if (!personalities || !CFDictionaryGetCount(personalities)) {
        goto finish;
    }

    context.kext = aKext;
    context.personalities = result;
    CFDictionaryApplyFunction(personalities,
        __OSKextPersonalityBundleIdentifierApplierFunction,
        &context);

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
CFArrayRef OSKextCopyPersonalitiesOfKexts(CFArrayRef kextArray)
{
    CFMutableArrayRef result              = NULL;
    CFDictionaryRef   kextPersonalities   = NULL; // do not release
    __OSKextPersonalityBundleIdentifierContext context;
    CFIndex           count, i;

    if (!kextArray) {
        pthread_once(&__sOSKextInitialized, __OSKextInitialize);
        kextArray = OSKextGetAllKexts();
    }

    result = CFArrayCreateMutable(CFGetAllocator(kextArray),
        0, &kCFTypeArrayCallBacks);
    if (!result) {
        OSKextLogMemError();
        goto finish;
    }

    context.personalities = result;

    count = CFArrayGetCount(kextArray);
    for (i = 0; i < count; i++) {
        OSKextRef thisKext = (OSKextRef)CFArrayGetValueAtIndex(kextArray, i);

        kextPersonalities = OSKextGetValueForInfoDictionaryKey(thisKext,
            CFSTR(kIOKitPersonalitiesKey));
        if (!kextPersonalities || !CFDictionaryGetCount(kextPersonalities)) {
            continue;
        }
        context.kext = thisKext;
        CFDictionaryApplyFunction(kextPersonalities,
            __OSKextPersonalityBundleIdentifierApplierFunction,
            &context);
    }

finish:
    return result;
}


/*********************************************************************
*********************************************************************/
typedef struct {
    size_t length;
} __OSKextMmapBufferInfo;

void __OSKextDeallocateMmapBuffer(void * pointer, void * vInfo)
{
    __OSKextMmapBufferInfo * info = (__OSKextMmapBufferInfo *)vInfo;
    munmap(pointer, info->length);
    // need to log munmap under kOSKextLogFileAccessFlag w/path of file,
    // store it in vInfo
    free(info);
    return;
}

/********************************************************************/
CFDataRef __OSKextMapExecutable(
    OSKextRef  aKext,
    off_t      offset,
    off_t      length);
CFDataRef __OSKextMapExecutable(
    OSKextRef  aKext,
    off_t      offset,
    off_t      length)
{
    CFDataRef                result                     = NULL;
    int                      localErrno                 = 0;
    char                     kextPath[PATH_MAX];
    CFStringRef              executableName             = NULL;  // do not release
    char                     executablePath[PATH_MAX];
    struct stat              executableStat;
    int                      executableFD               = -1;    // must close
    void                   * executableBuffer           = NULL;  // munmap on error
    __OSKextMmapBufferInfo * mmapAllocatorInfo          = NULL;  // free on error
    CFAllocatorContext       mmapAllocatorContext;
    CFAllocatorRef           mmapAllocator              = NULL;  // must release

    __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
        /* resolveToBase */ true, kextPath);

    if (!__OSKextCreateLoadInfo(aKext)) {
        goto finish;
    }

    if (!aKext->loadInfo->executableURL) {
        // xxx - this bit might warrant a kOSKextLogFileAccessFlag message
        OSKextLog(aKext,
            kOSKextLogDebugLevel | kOSKextLogFileAccessFlag,
            "Checking CFBundle of %s for executable URL.",
            kextPath);
        aKext->loadInfo->executableURL =
            _CFBundleCopyExecutableURLInDirectory(OSKextGetURL(aKext));
    }

   /* Did we fail to get an executable URL, even though the kext has a
    * CFBundleExecutable? Tag the kext with a diagnostic.
    */
    if (!aKext->loadInfo->executableURL) {
        executableName = OSKextGetValueForInfoDictionaryKey(aKext,
            kCFBundleExecutableKey);
            
        if (executableName) {
            __OSKextAddDiagnostic(aKext,
                kOSKextDiagnosticsFlagValidation,
                kOSKextDiagnosticExecutableMissingKey,
                executableName, /* note */ NULL);
            goto finish;
        }
    }

#if SHARED_EXECUTABLE
   /* No executable URL? Check for a shared executable and nab
    * a copy direct from the owning kext.
    */
    if (!aKext->loadInfo->executableURL) {
        CFStringRef sharedExecutableIdentifier = NULL;  // do not release
        sharedExecutableIdentifier = OSKextGetValueForInfoDictionaryKey(
            aKext, CFSTR(kOSBundleSharedExecutableIdentifierKey));
        if (sharedExecutableIdentifier) {

            // xxx - does not handle multiple versions/duplicates!
            OSKextRef sharedExecutableKext =
                OSKextGetKextWithIdentifier(sharedExecutableIdentifier);
            if (sharedExecutableKext) {
                aKext->loadInfo->executableURL =
                    _CFBundleCopyExecutableURLInDirectory(
                        OSKextGetURL(sharedExecutableKext));
            } else {
                __OSKextAddDiagnostic(aKext,
                    kOSKextDiagnosticsFlagValidation,
                    kOSKextDiagnosticSharedExecutableKextMissingKey,
                    sharedExecutableIdentifier, /* note */ NULL);
                goto finish;
            }
        }
    }
#endif

   /* We have no way of distinguishing whether the kext has no
    * CFBundleExecutable vs. whether the call above failed, so
    * just process when we do get an URL.
    */
    if (aKext->loadInfo->executableURL) {
        if (!__OSKextGetFileSystemPath(/* kext */ NULL,
            aKext->loadInfo->executableURL,
            /* resolveToBase */ true, executablePath)) {

            goto finish;
        }

        OSKextLog(aKext,
            kOSKextLogDebugLevel | kOSKextLogFileAccessFlag,
            "Statting %s for map.",
            executablePath);

        if (-1 == stat(executablePath, &executableStat)) {
            localErrno = errno;
            if (localErrno == ENOENT) {
                __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
                    kOSKextDiagnosticExecutableMissingKey,
                    executableName, /* note */ NULL);
                __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
                    kOSKextDiagnosticFileNotFoundKey,
                    aKext->loadInfo->executableURL, /* note */ NULL);

            } else {
                __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
                    kOSKextDiagnosticStatFailureKey,
                    aKext->loadInfo->executableURL, /* note */ NULL);
            }
            OSKextLog(aKext,
                kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "Stat failed for %s - %s.",
                executablePath, strerror(localErrno));
            goto finish;
        }

        if (!length) {
            length = executableStat.st_size;
        } else if ((offset + length) > executableStat.st_size) {
            OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "Internal error; overrun mapping executable file %s.",
                executablePath);
            goto finish;
        }

        OSKextLog(aKext,
            kOSKextLogDebugLevel | kOSKextLogFileAccessFlag,
            "Opening %s for map.",
            executablePath);
        executableFD = open(executablePath, O_RDONLY);
        if (executableFD == -1) {
            localErrno = errno;
            if (localErrno == ENOENT) {
                __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
                    kOSKextDiagnosticExecutableMissingKey, executableName,
                    /* note */ NULL);
                __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
                    kOSKextDiagnosticFileNotFoundKey,
                    aKext->loadInfo->executableURL, /* note */ NULL);
            } else {
                __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
                    kOSKextDiagnosticFileAccessKey,
                    aKext->loadInfo->executableURL, /* note */ NULL);
            }
            OSKextLog(aKext,
                kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "Open failed for %s - %s.",
                executablePath, strerror(localErrno));
            goto finish;
        }
        
       /* Must do MAP_PRIVATE here because the linker scribbles in the executable
        * when doing a link.
        * xxx - do we want MAP_NOCACHE here?
        */
        executableBuffer = mmap(/* addr */ NULL, length,
            PROT_READ|PROT_WRITE, MAP_FILE|MAP_PRIVATE, executableFD, offset);
        if (!executableBuffer) {
            localErrno = errno;
            
           /* Only if we are mapping the executable as a whole, flag its
            * absence as a validation error. It is never a validation failure
            * for a given arch to not be present (you should check with
            * OSKextSupportsArchitecture()).
            */
            if (length == 0) {
                __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
                    kOSKextDiagnosticFileAccessKey, aKext->loadInfo->executableURL,
                    /* note */ NULL);
                __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
                    kOSKextDiagnosticExecutableMissingKey, executableName,
                    /* note */ NULL);
                aKext->flags.invalid = 1;
                aKext->flags.valid = 0;
            }

            OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "Failed to map executable file %s (offset %lu, %lu bytes) - %s.",
                    executablePath, (unsigned long)offset, (unsigned long)length,
                    strerror(localErrno));

            goto finish;
        }

        OSKextLog(aKext, kOSKextLogDetailLevel | kOSKextLogFileAccessFlag,
            "Mapped executable file %s (offset %lu, %lu bytes).",
            executablePath, (unsigned long)offset, (unsigned long)length);

        CFAllocatorGetContext(kCFAllocatorDefault, &mmapAllocatorContext);
        mmapAllocatorInfo = (__OSKextMmapBufferInfo *)malloc(
            sizeof(__OSKextMmapBufferInfo));
        if (!mmapAllocatorInfo) {
            OSKextLogMemError();
            goto finish;
        }
        mmapAllocatorInfo->length = length;
        mmapAllocatorContext.info = mmapAllocatorInfo;
        mmapAllocatorContext.deallocate = &__OSKextDeallocateMmapBuffer;
        mmapAllocator = CFAllocatorCreate(kCFAllocatorDefault,
            &mmapAllocatorContext);
        if (!mmapAllocator) {
            OSKextLogMemError();
            goto finish;
        }
        result = CFDataCreateWithBytesNoCopy(
            CFGetAllocator(aKext), executableBuffer, length,
            /* bytesDeallocator */ mmapAllocator);
    }

finish:
    SAFE_RELEASE(mmapAllocator);
    if (executableFD != -1) {
        close(executableFD);
    }

    if (!result) {
        SAFE_FREE(mmapAllocatorInfo);
        if (executableBuffer) {
            OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "Error encountered, unmapping executable file %s (offset %lu, %lu bytes).",
                executablePath, (unsigned long)offset, (unsigned long)length);
            munmap(executableBuffer, length);
        }
    }
    return result;
}

/*********************************************************************
*********************************************************************/
Boolean __OSKextReadExecutable(OSKextRef aKext)
{
    Boolean                  result                     = false;

    if (!OSKextDeclaresExecutable(aKext)) {
        result = false;  // nothing to read
        goto finish;
    } else if (aKext->staticFlags.isFromMkext) {
        if (aKext->mkextInfo && aKext->mkextInfo->executable) {
            result = true;
            goto finish;
        } else {
            CFNumberRef executableOffsetNum = NULL;  // do not release

            if (!__OSKextCreateMkextInfo(aKext)) {
                goto finish;
            }

// xxx - log a msg on kOSKextLogArchiveFlag ?

           /* Do not use OSKextGetValueForInfoDictionaryKey() here,
            * this isn't an arch-spefic prop.
            */
            executableOffsetNum = CFDictionaryGetValue(aKext->infoDictionary,
                CFSTR(kMKEXTExecutableKey));
            if (executableOffsetNum) {
                aKext->mkextInfo->executable = __OSKextExtractMkext2FileEntry(
                    aKext,
                    aKext->mkextInfo->mkextData,
                    executableOffsetNum,
                    /* filename */ NULL);
                if (!aKext->mkextInfo->executable) {
                    __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
                        kOSKextDiagnosticExecutableMissingKey,
                        CFSTR("(executable from mkext)"), /* note */ NULL);
                    aKext->flags.invalid = 1;
                    aKext->flags.valid = 0;
                    goto finish;
                }
            }

            result = true;
            goto finish;
        }
    } else {
        if (aKext->loadInfo && aKext->loadInfo->executable) {
            result = true;
            goto finish;
        } else {
            if (!__OSKextCreateLoadInfo(aKext)) {
                goto finish;
            }

            aKext->loadInfo->executable = __OSKextMapExecutable(aKext,
                /* offset */ 0, /* length (0 => whole file) */ 0);
            if (!aKext->loadInfo->executable) {
                goto finish;
            }
        }
    }

    result = true;

finish:
   /* Most access to a Mach-O executable is going to be random, so advise
    * the system about that.
    */
    if (aKext->loadInfo && aKext->loadInfo->executable) {
        (void)posix_madvise(
            (void *)CFDataGetBytePtr(aKext->loadInfo->executable),
            CFDataGetLength(aKext->loadInfo->executable),
            POSIX_MADV_RANDOM);
    }

    return result;
}

/*********************************************************************
*********************************************************************/
CFDataRef OSKextCopyExecutableForArchitecture(
    OSKextRef          aKext,
    const NXArchInfo * archInfo)
{
    CFDataRef    result          = NULL;
    CFBundleRef  kextBundle      = NULL;  // must release
    CFURLRef     execURL         = NULL;  // must release
    CFDataRef    executable      = NULL;  // must release
    CFDataRef    thinExecutable  = NULL;  // must release
    CFStringRef  archName        = NULL;  // must release
    fat_iterator fatIterator     = NULL;  // must fat_iterator_close()
    char         kextPath[PATH_MAX];

    if (!__OSKextReadExecutable(aKext)) {
        goto finish;
    }

    if (aKext->staticFlags.isFromMkext) {
        if (aKext->mkextInfo && aKext->mkextInfo->executable) {
            executable = CFRetain(aKext->mkextInfo->executable);
        }
    } else {
        if (aKext->loadInfo && aKext->loadInfo->executable) {
            executable = CFRetain(aKext->loadInfo->executable);
        }
    }

    if (!executable) {
        goto finish;
    }

    if (!archInfo) {

        if (aKext->staticFlags.isFromMkext) {

           /* Do not use CFDataCreateCopy(), it might just retain the
            * data object. We need a distinct buffer. Because the linker
            * might scribble in it.
            */
            result = CFDataCreate(CFGetAllocator(executable),
                CFDataGetBytePtr(executable), CFDataGetLength(executable));
        } else {
           /* Map the executable separately from the main one in the kext.
            * Again, we need to guarantee the copy semantic for real.
            */
            result = __OSKextMapExecutable(aKext,
                /* offset */ 0,
                /* length (0 => whole file) */ 0);
        }
    } else {
        const UInt8 * exec        = CFDataGetBytePtr(executable);
            void    * thinExec    = NULL;  // do not free
            void    * thinExecEnd = NULL;  // do not free

        fatIterator = fat_iterator_for_data(exec,
            exec + CFDataGetLength(executable), 1 /* mach-o only */);
        if (!fatIterator) {
            __OSKextSetDiagnostic(aKext,
                kOSKextDiagnosticsFlagValidation,
                kOSKextDiagnosticExecutableBadKey);
            goto finish;
        }

       /* If not from an mkext, we are going to mmap the thin executable
        * if possible directly from the on-disk file, separately from
        * the main executable held by the kext. If we can't mmap it because
        * the alignment isn't at PAGE_SIZE, we'll FALL THROUGH to do it
        * by copying a CFDataRef just like for an mkext.
        */
        if (!aKext->staticFlags.isFromMkext) {
            struct fat_arch fatArchInfo;
            
            if (!fat_iterator_find_fat_arch(fatIterator,
                archInfo->cputype, archInfo->cpusubtype, &fatArchInfo)) {

                goto finish;
            }
            
            if ((1 << fatArchInfo.align) == PAGE_SIZE) {
                result = __OSKextMapExecutable(aKext,
                    /* offset */ fatArchInfo.offset,
                    /* length (0 => whole file) */ fatArchInfo.size);
                goto finish;
            }
            // otherwise we FALL THROUGH here
        }
    
        thinExec = fat_iterator_find_arch(fatIterator,
            archInfo->cputype, archInfo->cpusubtype, &thinExecEnd);
        if (thinExec) {
            result = CFDataCreate(CFGetAllocator(aKext), thinExec,
                thinExecEnd - thinExec);
        }
    }

    if (!result) {
        goto finish;
    }
    
finish:
    if (!result && archInfo && fatIterator) {

        __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
            /* resolveToBase */ false, kextPath);

        archName = CFStringCreateWithCString(
            CFGetAllocator(aKext), archInfo->name,
            kCFStringEncodingUTF8);
        if (archName) {
            __OSKextAddDiagnostic(aKext,
                kOSKextDiagnosticsFlagWarnings,
                kOSKextDiagnosticExecutableArchNotFoundKey,
                archName, /* note */ NULL);
        }
    }

    SAFE_RELEASE(archName);
    SAFE_RELEASE(execURL);
    SAFE_RELEASE(kextBundle);
    SAFE_RELEASE(executable);
    SAFE_RELEASE(thinExecutable);
    if (fatIterator) fat_iterator_close(fatIterator);
    return result;
}

/*********************************************************************
* xxx - note that for mkext kexts, this won't get a non-boot resource
* xxx - we don't want to go rooting around the system for it, do we?
*********************************************************************/
CFDataRef OSKextCopyResource(
    OSKextRef   aKext,
    CFStringRef resourceName,
    CFStringRef resourceType)
{
    CFDataRef       result               = NULL;
    CFDataRef       resource             = NULL;  // do not release
    CFStringRef     resourceNamePlusType = NULL;  // must release
    CFBundleRef     kextBundle           = NULL;  // must release
    CFURLRef        resourceURL          = NULL;  // must release
    SInt32          error;
    char          * resourceCString      = NULL;  // must free
    char            kextPath[PATH_MAX];
    char            resourcePath[PATH_MAX];

    if (!aKext->staticFlags.isFromMkext) {
        __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
            /* resolveToBase */ false, kextPath);
        OSKextLog(aKext, kOSKextLogDetailLevel | kOSKextLogFileAccessFlag,
            "Opening CFBundle for %s.", kextPath);

        kextBundle = CFBundleCreate(CFGetAllocator(aKext),
            aKext->bundleURL);
        if (!kextBundle) {
            OSKextLog(aKext, kOSKextLogProgressLevel | kOSKextLogFileAccessFlag,
                "Couldn't open CFBundle for %s.", kextPath);
            goto finish;
        }
        resourceURL = CFBundleCopyResourceURL(kextBundle,
            resourceName, resourceType, /* subdirName */ NULL);

        if (!resourceURL) {
            resourceCString = createUTF8CStringForCFString(resourceName);
            OSKextLog(aKext, kOSKextLogProgressLevel | kOSKextLogFileAccessFlag,
                "Couldn't read resource URL in %s for resource %s.",
                kextPath, resourceCString);
            goto finish;
        }

        __OSKextGetFileSystemPath(/* kext */ NULL, resourceURL,
            /* resolveToBase */ false, resourcePath);
        OSKextLog(aKext, kOSKextLogDetailLevel | kOSKextLogFileAccessFlag,
            "Reading resource %s.", resourcePath);

        if (!CFURLCreateDataAndPropertiesFromResource(CFGetAllocator(aKext),
            resourceURL, &resource, NULL, NULL, &error)) {
            // xxx - get error string from error
            OSKextLog(aKext, kOSKextLogProgressLevel | kOSKextLogFileAccessFlag,
                "Couldn't read resource file %s.",
                resourcePath);
            goto finish;
        }
    }

    result = resource;

finish:
    if (resourceURL) {
        OSKextLogSpec logLevel = kOSKextLogDebugLevel;
        if (!result) {
            logLevel = kOSKextLogProgressLevel;
        }
        OSKextLog(aKext, logLevel | kOSKextLogFileAccessFlag,
            "Reading resource file %s%s.",
            resourcePath,
            result ? "" : " failed");
    }

    SAFE_RELEASE(resourceNamePlusType);
    SAFE_RELEASE(resourceURL);
    SAFE_FREE(resourceCString);

    if (kextBundle) {
        OSKextLog(aKext,
            kOSKextLogDebugLevel | kOSKextLogFileAccessFlag,
            "Releasing CFBundle for %s.",
            kextPath);
    }
    SAFE_RELEASE(kextBundle);
    return result;
}

#pragma mark Dependency Resolution
/*********************************************************************
* Dependency Resolution
*********************************************************************/
Boolean __OSKextHasAllDependencies(OSKextRef aKext)
{
    if (aKext->flags.isKernelComponent ||
        (aKext->loadInfo && aKext->loadInfo->flags.hasAllDependencies)) {

        return true;
    }
    return false;
}

/*********************************************************************
*********************************************************************/
// return should be OSReturn
Boolean __OSKextResolveDependencies(
    OSKextRef         aKext,
    OSKextRef         rootKext,
    CFMutableSetRef   resolvedSet,
    CFMutableArrayRef loopStack)
{
    Boolean         result               = false;
    Boolean         error                = false;
    Boolean         addedToLoopStack     = false;
    CFDictionaryRef declaredDependencies = NULL;  // do not release
    CFStringRef   * libIDs               = NULL;  // must free
    CFStringRef   * libVersions          = NULL;  // must free
    char          * libIDCString         = NULL;  // must free
    char            kextPath[PATH_MAX];
    char            dependencyPath[PATH_MAX];
    CFIndex         count = 0, i = 0;

    __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
        /* resolveToBase */ false, kextPath);

    if (!__OSKextReadInfoDictionary(aKext, /* bundle */ NULL) ||
        !aKext->infoDictionary) {

        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogDependenciesFlag,
            "%s has no info dictionary; can't resolve dependencies.",
            kextPath);
        goto finish;
    }
    
   /* If the kext is invalid, it's best not to try to resolve personalities.
    * I suppose we could add a flag bit specifically covering whether the
    * CFBundleVersion, OSBundleCompatibleVersion, and OSBundleLibraries
    * properties are kosher, but really the developer should just fix the
    * validation problems before doing more.
    */
    if (!__OSKextIsValid(aKext)) {
        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogDependenciesFlag,
            "%s is invalid; can't resolve dependencies.",
            kextPath);
        goto finish;
    }

   /* If we've already done resolution for aKext on this run
    * through the graph, we shouldn't repeat the work.
    */
    if (CFSetContainsValue(resolvedSet, aKext)) {
        OSKextLog(aKext, kOSKextLogDebugLevel | kOSKextLogDependenciesFlag,
            "%s already has dependencies resolved.", kextPath);
        result = true;
        goto finish;
    }

    // xxx - this looks silly being printed for kernel components
    if (!OSKextIsKernelComponent(aKext)) {
        OSKextLog(aKext, kOSKextLogStepLevel | kOSKextLogDependenciesFlag,
            "Resolving dependencies for %s.", kextPath);
    }

    if (CFArrayGetCountOfValue(loopStack, RANGE_ALL(loopStack), aKext)) {
        __OSKextAddDiagnostic(rootKext, kOSKextDiagnosticsFlagDependencies,
            kOSKextDependencyCircularReference, aKext->bundleID, /* note */ NULL);
        __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagDependencies,
            kOSKextDependencyCircularReference, aKext->bundleID, /* note */ NULL);
        goto finish;
    }

    CFArrayAppendValue(loopStack, aKext);
    addedToLoopStack = true;

    OSKextFlushDependencies(aKext);
    if (!__OSKextCreateLoadInfo(aKext)) {
        goto finish;
    }

   /* Other code expects the array to exist, even for a kernel component.
    */
    aKext->loadInfo->dependencies = CFArrayCreateMutable(
        CFGetAllocator(aKext), 0, &kCFTypeArrayCallBacks);
    if (!aKext->loadInfo->dependencies) {
        OSKextLogMemError();
        goto finish;
    }

   /* If we got this far we've confirmed it's a dictionary.
    */
    declaredDependencies = (CFDictionaryRef)OSKextGetValueForInfoDictionaryKey(
        aKext, CFSTR(kOSBundleLibrariesKey));

    // xxx - I think we need to allow for no dependencies, and link direct
    // xxx - on the kernel?

   /* No more work to do for a kernel component!
    */
    if (OSKextIsKernelComponent(aKext)) {
        if (aKext == rootKext) {
            OSKextLog(aKext, kOSKextLogDependenciesFlag,
                "%s is a kernel component with no dependencies.", kextPath);
        }
        result = true;
        goto finish;
    }

    if (declaredDependencies) {
        count = CFDictionaryGetCount(declaredDependencies);
        if (count) {
            libIDs = (CFStringRef *)malloc(count * sizeof(CFStringRef));
            libVersions = (CFStringRef *)malloc(count * sizeof(CFStringRef));
            if (!libIDs || !libVersions) {
                OSKextLogMemError();
                goto finish;
            }
            CFDictionaryGetKeysAndValues(declaredDependencies,
                (const void **)libIDs, (const void **)libVersions);
        }
    }

    for (i = 0; i < count; i++) {
        CFStringRef   libID                        = libIDs[i];
        CFStringRef   libVersion                   = libVersions[i];
        OSKextVersion requestedVersion             = OSKextParseVersionCFString(libVersion);
        OSKextRef     dependency                   = NULL;
        Boolean       loaded                       = false;
        Boolean       compatible                   = false;

        // xxx - need to check types of libID & libVersion, log error, add diagnostic

        SAFE_FREE_NULL(libIDCString);
        libIDCString = createUTF8CStringForCFString(libID);

       /* Look first for a dependency that's loaded and see if it's compatible.
        * If we don't do that look up or don't find one, search in all kexts.
        *
        * If the client doesn't want to require resoution against loaded
        * kexts, they should not call OSKextReadLoadedKextInfo() before calling
        * in here, or they should call OSKextFlushLoadInfo(NULL) before calling
        * in here.
        */
        dependency = OSKextGetLoadedKextWithIdentifier(libID);
        if (dependency) {
            loaded = true;
            compatible = OSKextIsCompatibleWithVersion(dependency,
                requestedVersion);

            if (!compatible) {
                char requestedVersionCString[kOSKextVersionMaxLength];
                char actualVersionCString[kOSKextVersionMaxLength];
                
                OSKextVersionGetString(requestedVersion,
                    requestedVersionCString, sizeof(requestedVersionCString));
                OSKextVersionGetString(OSKextGetVersion(dependency),
                    actualVersionCString, sizeof(actualVersionCString));

                if (OSKextGetCompatibleVersion(dependency) > 0) {
                    OSKextLog(aKext,
                        kOSKextLogErrorLevel | kOSKextLogDependenciesFlag,
                        "%s - loaded dependency %s, v%s is "
                        "not compatible with requested version %s.",
                        kextPath, libIDCString,
                        actualVersionCString, requestedVersionCString);
                    __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagDependencies,
                        kOSKextDependencyLoadedIsIncompatible, libID, /* note */ NULL);
                } else {
                    OSKextLog(aKext,
                        kOSKextLogErrorLevel | kOSKextLogDependenciesFlag,
                        "%s - loaded dependency %s lacks valid OSBundleCompatibleVersion.",
                        kextPath, libIDCString);
                    __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagDependencies,
                        kOSKextDependencyLoadedCompatibleVersionUndeclared, libID, /* note */ NULL);
                }
                error = true;
                continue;
            }
        } else {
            dependency = OSKextGetCompatibleKextWithIdentifier(libID,
                requestedVersion);
            if (dependency) {
                compatible = true;
            }
        }

        if (CFEqual(libID, __kOSKextKernelLibBundleID)) {
            aKext->loadInfo->flags.hasRawKernelDependency = 1;
        } else if (CFStringHasPrefix(libID, __kOSKextKernelLibPrefix)) {
            aKext->loadInfo->flags.hasKernelDependency = 1;
        } else if (CFStringHasPrefix(libID, __kOSKextKPIPrefix)) {
            aKext->loadInfo->flags.hasKPIDependency = 1;
            if (CFEqual(libID, __kOSKextPrivateKPI)) {
                aKext->loadInfo->flags.hasPrivateKPIDependency = 1;
            }
        }

       /* If we got a usable dependency, add it to the list. Otherwise
        * dig a little more for a proper diagnostic.
        */
        if (dependency) {
            Boolean kernelComponent = OSKextIsKernelComponent(dependency);

            __OSKextGetFileSystemPath(dependency, /* otherURL */ NULL,
                /* resolveToBase */ false, dependencyPath);
            OSKextLog(aKext, kOSKextLogDetailLevel | kOSKextLogDependenciesFlag,
                "%s found %s%sdependency %s for %s%s.",
                kextPath,
                compatible ? "compatible " : "incompatible ",
                loaded ? "loaded " : "",
                dependencyPath, libIDCString,
                kernelComponent ? " (kernel component)" : "");

           CFArrayAppendValue(aKext->loadInfo->dependencies, dependency);
        } else {

            dependency = OSKextGetKextWithIdentifier(libID);
            if (dependency) {
                if (OSKextGetCompatibleVersion(dependency) > 0) {
                    OSKextLog(aKext,
                        kOSKextLogErrorLevel | kOSKextLogDependenciesFlag,
                        "%s - no compatible dependency found for %s.",
                        kextPath, libIDCString);
                    __OSKextAddDiagnostic(aKext,
                        kOSKextDiagnosticsFlagDependencies,
                        kOSKextDependencyNoCompatibleVersion, libID, /* note */ NULL);
                } else {
                    OSKextLog(aKext,
                        kOSKextLogErrorLevel | kOSKextLogDependenciesFlag,
                        "%s - dependency for %s lacks valid OSBundleCompatibleVersion.",
                        kextPath, libIDCString);
                    __OSKextAddDiagnostic(aKext,
                        kOSKextDiagnosticsFlagDependencies,
                        kOSKextDependencyCompatibleVersionUndeclared, libID, /* note */ NULL);
                }
            } else {
                OSKextLog(aKext,
                    kOSKextLogErrorLevel | kOSKextLogDependenciesFlag,
                    "%s - no dependency found for %s.",
                    kextPath, libIDCString);
                __OSKextAddDiagnostic(aKext,
                    kOSKextDiagnosticsFlagDependencies,
                    kOSKextDependencyUnavailable, libID, /* note */ NULL);
            }
            error = true;
            continue;
        }
    }

    if (aKext->loadInfo->flags.hasRawKernelDependency) {
        __OSKextSetDiagnostic(aKext, kOSKextDiagnosticsFlagDependencies,
            kOSKextDiagnosticRawKernelDependency);
        error = true;
    }

   /* Interface kexts must have exactly one dependency.
    */
    if (OSKextIsInterface(aKext) && 
        CFArrayGetCount(aKext->loadInfo->dependencies) != 1) 
    {
        OSKextLog(aKext,
            kOSKextLogErrorLevel | kOSKextLogDependenciesFlag,
            "%s - Interface kext must have exactly one dependency.",
            kextPath);
        __OSKextSetDiagnostic(aKext,
            kOSKextDiagnosticsFlagDependencies,
            kOSKextDiagnosticsInterfaceDependencyCount);
        error = true;
    }

   /* Now that we've resolved aKext's dependencies, go through and
    * resolve the dependencies of the dependencies recursively.
    * Keep this in post-order for sanity with the logging.
    */
    count = CFArrayGetCount(aKext->loadInfo->dependencies);
    for (i = 0; i < count; i++) {
        OSKextRef dependency = (OSKextRef)CFArrayGetValueAtIndex(
            aKext->loadInfo->dependencies, i);
        CFStringRef libID = OSKextGetIdentifier(dependency);

        if (!__OSKextResolveDependencies(dependency, rootKext, resolvedSet,
            loopStack)) {

            CFIndex stackCount, stackIndex;

            stackCount = CFArrayGetCount(loopStack);
            for (stackIndex = 0; stackIndex < stackCount; stackIndex++) {
                OSKextRef stackKext = (OSKextRef)CFArrayGetValueAtIndex(
                    loopStack, stackIndex);
                __OSKextAddDiagnostic(stackKext,
                    kOSKextDiagnosticsFlagDependencies,
                    kOSKextDependencyIndirectDependencyUnresolvable,
                    libID, /* note */ NULL);
            }
            error = true;
        }
    }

    /* On 64-bit, we require that a kext explicitly list its dependencies
     * through KPIs only. This means that while it is not an error not to link
     * against any kernel components, we also won't implicitly link the kext
     * against the kernel.
     *
     * On 32-bit, a kext (with an executable) that doesn't declare any kernel
     * dependencies is linked against the full kernel.
     */
    if (__OSKextIsArchitectureLP64()) {
        if (OSKextDeclaresExecutable(aKext) && OSKextSupportsArchitecture(aKext, OSKextGetArchitecture())) {
            if (aKext->loadInfo->flags.hasKernelDependency) {

                __OSKextSetDiagnostic(aKext, kOSKextDiagnosticsFlagDependencies,
                    kOSKextDiagnosticDeclaresNonKPIDependenciesKey);

                goto finish;
            }

            if (!aKext->loadInfo->flags.hasKPIDependency) {
                __OSKextSetDiagnostic(aKext, kOSKextDiagnosticsFlagWarnings,
                    kOSKextDiagnosticDeclaresNoKPIsWarningKey);
            }
        }
    } else {
        if (OSKextDeclaresExecutable(aKext) &&
            !aKext->loadInfo->flags.hasKernelDependency &&
            !aKext->loadInfo->flags.hasKPIDependency) {

            OSKextLog(aKext,
                kOSKextLogWarningLevel | kOSKextLogDependenciesFlag,
                "%s does not declare a kernel dependency; using %s.",
                kextPath, __kOSKextCompatibilityBundleID);

            OSKextRef kernel6 = OSKextGetKextWithIdentifier(
                CFSTR(__kOSKextCompatibilityBundleID));

            if (!kernel6) {
                OSKextLog(aKext,
                    kOSKextLogErrorLevel | kOSKextLogDependenciesFlag,
                    "%s - no dependency found for %s.",
                    kextPath, __kOSKextCompatibilityBundleID);
                goto finish;
            }

            CFArrayAppendValue(aKext->loadInfo->dependencies, kernel6);

            __OSKextSetDiagnostic(aKext, kOSKextDiagnosticsFlagWarnings,
                kOSKextDiagnosticNoExplicitKernelDependencyKey);
        }

        if (aKext->loadInfo->flags.hasKPIDependency &&
            aKext->loadInfo->flags.hasKernelDependency) {

            __OSKextSetDiagnostic(aKext, kOSKextDiagnosticsFlagWarnings,
                kOSKextDiagnosticDeclaresBothKernelAndKPIDependenciesKey);
        }
    }

#ifndef IOKIT_EMBEDDED
    /* If the kext links against the private KPI, we want to make an effort
     * to ensure they are Apple-internal kexts.  We do this by verifying that
     * the kext's bundle identifier begins with "com.apple.", and by making
     * sure the kext's info dictionary contains an Apple copyright string
     * in either CFBundleGetInfoString (obsolete) or NSHumanReadableCopyright.
     */

    if (aKext->loadInfo->flags.hasPrivateKPIDependency)
    {
        CFStringRef     infoString                  = NULL;  // do not release
        CFStringRef     readableString              = NULL;  // do not release
        Boolean         hasApplePrefix              = false;
        Boolean         infoCopyrightIsValid        = false;
        Boolean         readableCopyrightIsValid    = false;

        hasApplePrefix = CFStringHasPrefix(aKext->bundleID, __kOSKextApplePrefix);

        infoString = (CFStringRef) OSKextGetValueForInfoDictionaryKey(aKext,
            CFSTR("CFBundleGetInfoString"));
        if (infoString) {
            char *infoCString = createUTF8CStringForCFString(infoString);
            infoCopyrightIsValid = kxld_validate_copyright_string(infoCString);
            SAFE_FREE(infoCString);
        }

        readableString = (CFStringRef) OSKextGetValueForInfoDictionaryKey(aKext,
            CFSTR("NSHumanReadableCopyright"));
        if (readableString) {
            char *readableCString = createUTF8CStringForCFString(readableString);
            readableCopyrightIsValid = 
                kxld_validate_copyright_string(readableCString);
            SAFE_FREE(readableCString);
        }

        if (!hasApplePrefix || (!infoCopyrightIsValid && !readableCopyrightIsValid)) {

            OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogDependenciesFlag,
                "%s has an Apple prefix but no copyright.", kextPath);

            __OSKextSetDiagnostic(aKext, kOSKextDiagnosticsFlagDependencies,
                kOSKextDiagnosticNonAppleKextDeclaresPrivateKPIDependencyKey);
            result = false;
            goto finish;
        }
    }
#endif /* !IOKIT_EMBEDDED */

    if (!error) {
        result = true;
    }
    
finish:
    SAFE_FREE(libIDCString);
    SAFE_FREE(libIDs);
    SAFE_FREE(libVersions);

    if (result && aKext->loadInfo) {
        aKext->loadInfo->flags.hasAllDependencies = 1;
    }

   /* Whether successful or not, we've done resolution.
    */
    CFSetAddValue(resolvedSet, aKext);

    if (addedToLoopStack) {
        CFArrayRemoveValueAtIndex(loopStack, CFArrayGetCount(loopStack) - 1);
    }

    return result;
}

/*********************************************************************
*********************************************************************/
typedef struct {
    Boolean result;
} __OSKextResolveDependenciesContext;

static void __OSKextResolveDependenciesApplierFunction(
    const void * vKey __unused,
    const void * vValue,
          void * vContext)
{
    OSKextRef aKext    = (OSKextRef)vValue;
    Boolean   resolved = false;
    __OSKextResolveDependenciesContext * context =
        (__OSKextResolveDependenciesContext *)vContext;

    resolved = OSKextResolveDependencies(aKext);
    if (!resolved) {
       context->result = false;
    }
    return;
}

/*********************************************************************
*********************************************************************/
Boolean OSKextResolveDependencies(OSKextRef aKext)
{
    Boolean           result       = false;
    CFMutableSetRef   resolvedSet  = NULL;  // must release
    CFMutableArrayRef loopStack    = NULL;  // must release
    CFArrayRef        loadList     = NULL;  // must release
    CFStringRef       kextID       = NULL;  // do not release
    CFIndex           count, i, j;
    char              kextPath[PATH_MAX];

    resolvedSet = CFSetCreateMutable(
        CFGetAllocator(aKext), 0, &kCFTypeSetCallBacks);
    loopStack = CFArrayCreateMutable(
        CFGetAllocator(aKext), 0, &kCFTypeArrayCallBacks);
    if (!resolvedSet || !loopStack) {
        OSKextLogMemError();
        goto finish;
    }

    if (aKext) {
        if (__OSKextHasAllDependencies(aKext)) {
            __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
                /* resolveToBase */ false, kextPath);
            OSKextLog(aKext,
                kOSKextLogDebugLevel | kOSKextLogDependenciesFlag,
                "%s - dependencies already resolved.", kextPath);

            result = true;
            goto finish;
        }
        result = __OSKextResolveDependencies(aKext, aKext, resolvedSet,
            loopStack);

       /* If we got a full dependency graph, check it for multiple instances
        * of kexts with the same ID. No need to check version or UUID, if
        * it's two separate objects, that's bad. Note we only do this for the
        * root kext being resolved, it's probably too expensive to do for all.
        */
        if (result) {
            CFStringRef required                   = NULL;  // do not release
            Boolean     checkRootOrConsoleRequired = FALSE;
            Boolean     checkLocalRequired         = FALSE;
            Boolean     checkNetworkRequired       = FALSE;

            loadList = OSKextCopyLoadList(aKext, /* needAll */ true);
            if (!loadList) {
                result = false;
                goto finish;
            }

           /* If the kext we are resolving has OSBundleRequired set,
            * we are going to check all its dependencies to see
            * if they will be available during early boot.
            */
            required = OSKextGetValueForInfoDictionaryKey(aKext, CFSTR(kOSBundleRequiredKey));
            if (required) {
                if (CFEqual(required, CFSTR(kOSBundleRequiredRoot)) ||
                    CFEqual(required, CFSTR(kOSBundleRequiredConsole))) {
                    
                    checkRootOrConsoleRequired = TRUE;
                } else if (CFEqual(required, CFSTR(kOSBundleRequiredLocalRoot))) {
                    checkLocalRequired = TRUE;
                } else if (CFEqual(required, CFSTR(kOSBundleRequiredNetworkRoot))) {
                    checkNetworkRequired = TRUE;
                }
            }

            count = CFArrayGetCount(loadList);
            for (i = 0; i < count; i++) {
                OSKextRef thisKext = (OSKextRef)CFArrayGetValueAtIndex(
                    loadList, i);
                CFStringRef thisRequired = OSKextGetValueForInfoDictionaryKey(thisKext,
                    CFSTR(kOSBundleRequiredKey));

                kextID = OSKextGetIdentifier(thisKext);

               /* Root and Console are good to go for any OSBundleRequired except
                * Safe Boot. We can assume the kext is valid here and therefore
                * that its OSBundleRequired is a required value.
                */
                if (checkRootOrConsoleRequired) {
                    if (!thisRequired ||
                        CFEqual(CFSTR(kOSBundleRequiredSafeBoot), thisRequired)) {
                        
                            __OSKextAddDiagnostic(aKext,
                                kOSKextDiagnosticsFlagWarnings,
                                kOSKextDiagnosticsDependencyNotOSBundleRequired,
                                kextID,
                                /* note */ thisRequired ? thisRequired : CFSTR("OSBundleRequired not set"));
                        }
                }

                if (checkLocalRequired) {
                    if (!thisRequired ||
                        !(CFEqual(CFSTR(kOSBundleRequiredRoot), thisRequired) ||
                          CFEqual(CFSTR(kOSBundleRequiredLocalRoot), thisRequired) ||
                          CFEqual(CFSTR(kOSBundleRequiredConsole), thisRequired))) {
                        
                            __OSKextAddDiagnostic(aKext,
                                kOSKextDiagnosticsFlagWarnings,
                                kOSKextDiagnosticsDependencyNotOSBundleRequired,
                                kextID,
                                /* note */ thisRequired ? thisRequired : CFSTR("OSBundleRequired not set"));
                        }
                }

                if (checkNetworkRequired) {
                    if (!thisRequired ||
                        !(CFEqual(CFSTR(kOSBundleRequiredRoot), thisRequired) ||
                         CFEqual(CFSTR(kOSBundleRequiredNetworkRoot), thisRequired) ||
                         CFEqual(CFSTR(kOSBundleRequiredConsole), thisRequired))) {
                        
                            __OSKextAddDiagnostic(aKext,
                                kOSKextDiagnosticsFlagWarnings,
                                kOSKextDiagnosticsDependencyNotOSBundleRequired,
                                kextID,
                                /* note */ thisRequired ? thisRequired : CFSTR("OSBundleRequired not set"));
                        }
                }

                for (j = i+1; j < count; j++) {
                    OSKextRef thatKext = (OSKextRef)CFArrayGetValueAtIndex(
                        loadList, j);
                    if (CFEqual(kextID, OSKextGetIdentifier(thatKext))) {

                        __OSKextAddDiagnostic(aKext,
                            kOSKextDiagnosticsFlagDependencies,
                            kOSKextDependencyMultipleVersionsDetected,
                            kextID, /* note */ NULL);
                        result = false;
                       /* But keep going, get all diagnostics. */
                    }
                }
            }
        }
    } else if (__sOSKextsByURL) {
        __OSKextResolveDependenciesContext context;
        context.result = true;  // failed resolve sets to false
        CFDictionaryApplyFunction(__sOSKextsByURL,
            __OSKextResolveDependenciesApplierFunction, &context);
    }
finish:
    SAFE_RELEASE(resolvedSet);
    SAFE_RELEASE(loopStack);
    SAFE_RELEASE(loadList);
    return result;
}

/*********************************************************************
*********************************************************************/
static void __OSKextFlushDependenciesApplierFunction(
    const void * vKey __unused,
    const void * vValue,
          void * vContext __unused)
{
    OSKextRef theKext = (OSKextRef)vValue;

    OSKextFlushDependencies(theKext);
    return;
}

/*********************************************************************
*********************************************************************/
void __OSKextClearHasAllDependenciesOnKext(OSKextRef aKext)
{
    char      kextPath[PATH_MAX];
    CFIndex   count, i;

    count = CFArrayGetCount(__sOSAllKexts);
    for (i = 0; i < count; i++) {
        OSKextRef checkKext = (OSKextRef)CFArrayGetValueAtIndex(__sOSAllKexts, i);
        if (!checkKext->loadInfo || !checkKext->loadInfo->dependencies ||
            !__OSKextHasAllDependencies(checkKext)) {
            continue;
        }
        if (CFArrayContainsValue(checkKext->loadInfo->dependencies,
            RANGE_ALL(checkKext->loadInfo->dependencies), aKext)) {

            __OSKextGetFileSystemPath(checkKext, /* otherURL */ NULL,
                /* resolveToBase */ false, kextPath);
            OSKextLog(aKext,
                kOSKextLogDebugLevel | kOSKextLogKextBookkeepingFlag,
                "Clearing \"has all dependencies\" for %s.", kextPath);

            checkKext->loadInfo->flags.hasAllDependencies = 0;
            __OSKextClearHasAllDependenciesOnKext(checkKext);
        }
    }
    
    return;
}

/*********************************************************************
*********************************************************************/
void OSKextFlushDependencies(OSKextRef aKext)
{
    static Boolean flushingAll = false;
    char           kextPath[PATH_MAX];

    pthread_once(&__sOSKextInitialized, __OSKextInitialize);

    if (aKext) {
        if (!flushingAll) {
            if (OSKextGetURL(aKext)) {
                __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
                    /* resolveToBase */ false, kextPath);
            }
            OSKextLog(aKext,
                kOSKextLogDetailLevel | kOSKextLogKextBookkeepingFlag,
                "Flushing dependencies for %s.", kextPath);
        }

        if (aKext->loadInfo) {
            aKext->loadInfo->flags.hasRawKernelDependency = 0;
            aKext->loadInfo->flags.hasKernelDependency = 0;
            aKext->loadInfo->flags.hasKPIDependency = 0;

            if (aKext->loadInfo->dependencies) {
                SAFE_RELEASE_NULL(aKext->loadInfo->dependencies);
                aKext->loadInfo->flags.hasAllDependencies = 0;
                aKext->loadInfo->flags.dependenciesValid = 0;
                aKext->loadInfo->flags.dependenciesAuthentic = 0;

               /* If we've cleared any dependency info, we need to go up
                * all dependency graphs and clear the "has all dependencies"
                * bits on any dependents of this kext, direct or indirect.
                */
                __OSKextClearHasAllDependenciesOnKext(aKext);
            }

            OSKextFlushDiagnostics(aKext, kOSKextDiagnosticsFlagDependencies);
        }
    } else if (__sOSKextsByURL) {
        flushingAll = true;
        OSKextLog(/* kext */ NULL,
            kOSKextLogDetailLevel | kOSKextLogKextBookkeepingFlag,
            "Flushing dependencies for all kexts.");
        CFDictionaryApplyFunction(__sOSKextsByURL,
            __OSKextFlushDependenciesApplierFunction, NULL);
        flushingAll = false;
    }
    return;
}

/*********************************************************************
*********************************************************************/
Boolean OSKextValidateDependencies(OSKextRef aKext)
{
    Boolean     result = true;
    CFArrayRef  allDependencies = NULL;  // must release
    CFIndex     count, i;

    if (aKext->loadInfo && aKext->loadInfo->flags.dependenciesValid) {
        goto finish;
    }
    allDependencies = OSKextCopyAllDependencies(aKext,
        /* needAll */ true);
    if (!allDependencies) {
        result = false;
        goto finish;
    }

    count = CFArrayGetCount(allDependencies);
    for (i = 0; i < count; i++) {
        OSKextRef thisKext = (OSKextRef)CFArrayGetValueAtIndex(
            allDependencies, i);

        if (!__OSKextIsValid(thisKext)) {
            __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagDependencies,
                kOSKextDependencyInvalid, OSKextGetIdentifier(thisKext),
                /* note */ NULL);
            result = false;
            // run through them all, do not continue or goto finish
        }
    }

    if (result) {
       /* We might not have loadInfo yet!
        */
        if (!__OSKextCreateLoadInfo(aKext)) {
            result = false;
            goto finish;
        }
        aKext->loadInfo->flags.dependenciesValid = 1;
    }

finish:
    SAFE_RELEASE(allDependencies);

    return result;
}

/*********************************************************************
*********************************************************************/
Boolean OSKextAuthenticateDependencies(OSKextRef aKext)
{
    Boolean     result          = true;
    CFArrayRef  allDependencies = NULL;  // must release
    CFIndex     count, i;

    if (aKext->loadInfo && aKext->loadInfo->flags.dependenciesAuthentic) {
        goto finish;
    }

    allDependencies = OSKextCopyAllDependencies(aKext,
        /* needAll */ true);
    if (!allDependencies) {
        result = false;
        goto finish;
    }

    count = CFArrayGetCount(allDependencies);
    for (i = 0; i < count; i++) {
        OSKextRef thisKext = (OSKextRef)CFArrayGetValueAtIndex(
            allDependencies, i);
        if (!OSKextIsAuthentic(thisKext)) {
            __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagDependencies,
                kOSKextDependencyInauthentic,
                OSKextGetIdentifier(thisKext), /* note */ NULL);
            result = false;
            // run through them all, do not continue or goto finish
        }
    }

    if (result) {
        if (!__OSKextCreateLoadInfo(aKext)) {
            OSKextLogMemError();
            goto finish;
        }
        aKext->loadInfo->flags.dependenciesAuthentic = 1;
    }

finish:
    SAFE_RELEASE(allDependencies);
    return result;
}

/*********************************************************************
*********************************************************************/
CFArrayRef OSKextCopyDeclaredDependencies(
    OSKextRef aKext,
    Boolean   needAllFlag)
{
    CFArrayRef result = NULL;
    Boolean resolved = false;

    resolved = OSKextResolveDependencies(aKext);

    if (needAllFlag && !resolved) {
        goto finish;
    }

    if (aKext->loadInfo && aKext->loadInfo->dependencies) {
        result = CFArrayCreateCopy(CFGetAllocator(aKext),
            aKext->loadInfo->dependencies);
    } else {
        result = CFArrayCreate(CFGetAllocator(aKext), NULL, 0,
            &kCFTypeArrayCallBacks);
    }
finish:
    return result;
}

/*********************************************************************
* If a kext *doesn't* depend on com.apple.kernel or any com.apple.kpi.*
* component, it's a pretty old kext that might need the old linear linkage
* model, so we pull all indirect dependencies into the link list for the kext. 
*********************************************************************/
Boolean
__OSKextGetBleedthroughFlag(OSKextRef aKext)
{
    Boolean result = false;

    /* Bleedthrough is ignored on 64-bit.
     */
    if (__OSKextIsArchitectureLP64()) {
        result = false;
    } else {
        result = !aKext->loadInfo->flags.hasKPIDependency;
    }

    return result;
}


/*********************************************************************
* List must be built in postorder (link order)
* xxx - de we want any kind of kOSKextLogDependenciesFlag
* xxx - messages for these?
*********************************************************************/
Boolean
__OSKextAddLinkDependencies(
    OSKextRef         aKext,
    CFMutableArrayRef linkDependencies,
    Boolean           needAllFlag,
    Boolean           bleedthroughFlag)
{
    Boolean result = false;
    CFIndex count, i;

   /* A kernel component has no dependencies other than an implicit
    * one on the kernel, and such a kext never has an array of
    * dependencies.
    */
    if (OSKextIsKernelComponent(aKext)) {
        result = true;
        goto finish;
    }

   /* If the kext doesn't have loadInfo or dependencies, we've hit
    * an internal error.
    */
    if (!aKext->loadInfo || !aKext->loadInfo->dependencies) {
        result = !needAllFlag;
        goto finish;
    }

   /* See if the bleedthroughFlag bit flips with this kext.  This flag then
    * propagates down the recursive call stack.
    */
    bleedthroughFlag = bleedthroughFlag || __OSKextGetBleedthroughFlag(aKext);

    count = CFArrayGetCount(aKext->loadInfo->dependencies);
    for (i = 0; i < count; i++) {
        OSKextRef dependency = (OSKextRef)CFArrayGetValueAtIndex(
            aKext->loadInfo->dependencies, i);

       /* If bleeding through for old kexts, or for a redirect kext that
        * has no executable, call recursively.
        */
        if (bleedthroughFlag || !OSKextDeclaresExecutable(dependency)) {
            if (!__OSKextAddLinkDependencies(dependency,
                linkDependencies,
                needAllFlag,
                bleedthroughFlag)) 
            {
                goto finish;
            }
        }

       /* The easy part: a kext with an executable goes on the list if it isn't
        * already on!
        */
        if (OSKextDeclaresExecutable(dependency)) {
            if (kCFNotFound == CFArrayGetFirstIndexOfValue(linkDependencies,
                RANGE_ALL(linkDependencies), dependency)) {

                CFArrayAppendValue(linkDependencies, dependency);
            }
        }
    }

    result = true;

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
CFArrayRef OSKextCopyLinkDependencies(
    OSKextRef aKext,
    Boolean   needAllFlag)
{
    CFMutableArrayRef result = NULL;
    Boolean resolved = OSKextResolveDependencies(aKext);
    if (needAllFlag && !resolved) {
        goto finish;
    }

    result = CFArrayCreateMutable(CFGetAllocator(aKext), 0,
        &kCFTypeArrayCallBacks);
    if (!result) {
        OSKextLogMemError();
        goto finish;
    }

    if (!__OSKextAddLinkDependencies(aKext, result,
        needAllFlag, /* bleedthrough */ false)) {

        SAFE_RELEASE_NULL(result);
    }

finish:
    return result;

}

/*******************************************************************************
* Suck from a kext's executable all of its undefined symbol names and build
* an array of empty arrays keyed  by symbol. The arrays will be filled with
* lib kexts that export that symbol. Also note the cpu arch of the kext to
* match on that for the libs.
*
* xxx - do we want to log stuff for this?
*******************************************************************************/
Boolean __OSKextReadSymbolReferences(
    OSKextRef              aKext,
    CFMutableDictionaryRef symbols)
{
    Boolean                    result        = false;
    char                       kextPath[PATH_MAX];

    CFDataRef                  executable    = NULL;  // must release
    const struct mach_header * mach_header   = NULL;
    const void               * file_end      = NULL;

    Boolean                    sixtyfourbit  = false;
    uint8_t                    swap          = 0;

    macho_seek_result          symtab_result = macho_seek_result_not_found;
    struct symtab_command    * symtab        = NULL;
    char                     * syms_address  = NULL;
    const void               * string_list   = NULL;
    unsigned int               sym_offset    = 0;
    unsigned int               str_offset    = 0;
    unsigned int               num_syms      = 0;
    unsigned int               syms_bytes    = 0;
    unsigned int               sym_index     = 0;

    __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
        /* resolveToBase */ false, kextPath);

   /* A kernel component has no symbol references outside the kernel,
    * and a kext with no executable also plainly has none.
    */
    if (OSKextIsKernelComponent(aKext) || !OSKextDeclaresExecutable(aKext)) {
        result = true;
        goto finish;
    }

   /* Get the executable for the current arch, return false if it doesn't
    * have one.
    */
    executable = OSKextCopyExecutableForArchitecture(aKext, OSKextGetArchitecture());
    if (!executable) {
        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogLinkFlag,
            "%s has no executable for architecture %s.",
            kextPath, OSKextGetArchitecture()->name);
        goto finish;
    }

    mach_header = (const struct mach_header *)CFDataGetBytePtr(executable);
    file_end = (((const char *)mach_header) + CFDataGetLength(executable));

    if (ISMACHO64(MAGIC32(mach_header))) {
        sixtyfourbit = true;
    }
    if (ISSWAPPEDMACHO(MAGIC32(mach_header))) {
        swap = 1;
    }

    symtab_result = macho_find_symtab(mach_header, file_end, &symtab);
    if (symtab_result != macho_seek_result_found) {
        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogLinkFlag,
            "%s has no symtab in its executable (%s)",
                kextPath, OSKextGetArchitecture()->name);
        goto finish;
    }

    sym_offset = CondSwapInt32(swap, symtab->symoff);
    str_offset = CondSwapInt32(swap, symtab->stroff);
    num_syms   = CondSwapInt32(swap, symtab->nsyms);

    syms_address = (char *)mach_header + sym_offset;
    string_list = (char *)mach_header + str_offset;
    syms_bytes = num_syms * sizeof(struct nlist);

    if (syms_address + syms_bytes > (char *)file_end) {
        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogLinkFlag,
            "%s: internal overrun in executable file (%s).",
            kextPath, OSKextGetArchitecture()->name);
        goto finish;
    }

    for (sym_index = 0; sym_index < num_syms; sym_index++) {
        struct nlist    * seekptr;
        struct nlist_64 * seekptr_64;
        uint32_t          string_index;
        uint8_t           n_type;

        if (sixtyfourbit) {
            seekptr_64 = &((struct nlist_64 *)syms_address)[sym_index];
            string_index = CondSwapInt32(swap, seekptr_64->n_un.n_strx);
            n_type = seekptr_64->n_type;
        } else {
            seekptr = &((struct nlist *)syms_address)[sym_index];
            string_index = CondSwapInt32(swap, seekptr->n_un.n_strx);
            n_type = seekptr->n_type;
        }

        // no need to swap n_type (one-byte value)
        if (string_index == 0 || n_type & N_STAB) {
            continue;
        }

        if ((n_type & N_TYPE) == N_UNDF) {
            char * symbol_name;
            CFStringRef cfSymbolName; // must release

            symbol_name = (char *)(string_list + string_index);
            cfSymbolName = CFStringCreateWithCString(kCFAllocatorDefault,
                symbol_name, kCFStringEncodingASCII);
            if (!cfSymbolName) {
                OSKextLogMemError();
                goto finish;
            }
            CFDictionarySetValue(symbols, cfSymbolName, kCFBooleanTrue);
            CFRelease(cfSymbolName);
        }
    }

    result = true;
finish:

   /* Advise the system that we no longer need the mmapped executable.
    */
    if (executable) {
        (void)posix_madvise((void *)CFDataGetBytePtr(executable),
            CFDataGetLength(executable),
            POSIX_MADV_DONTNEED);
    }

    SAFE_RELEASE(executable);
    return result;
}

/*********************************************************************
*********************************************************************/
Boolean __OSKextIsSearchableForSymbols(
    OSKextRef aKext,
    Boolean   nonKPIFlag,
    Boolean   allowUnsupportedFlag)
{
    CFStringRef kextID = NULL;  // do not release

    if (!OSKextIsLibrary(aKext)) {
        return false;
    }
    if (!OSKextDeclaresExecutable(aKext)) {
        return false;
    }

    kextID = OSKextGetIdentifier(aKext);

   /* Check as appropriate for unsupported/private KPIs and disallow them.
    * Truly private KPIs will fail at link time.
    */
    if (!allowUnsupportedFlag) {
        if (CFEqual(kextID, CFSTR("com.apple.kernel.unsupported")) ||
            CFEqual(kextID, CFSTR("com.apple.kpi.unsupported")) ||
            CFEqual(kextID, CFSTR("com.apple.kpi.private")) ||
            CFEqual(kextID, CFSTR("com.apple.kpi.dsep"))) {

            return false;
        }
    }

    if (nonKPIFlag) {
        if (CFStringHasPrefix(kextID, __kOSKextKPIPrefix)) {
            return false;
        }
    } else {
        if (CFStringHasPrefix(kextID, __kOSKextKernelLibPrefix)) {
            return false;
        }
    }
    return true;
}

/*******************************************************************************
* Given a library kext, check all of its exported symbols against the running
* lists of undef (not yet found) symbols, symbols found once, and symbols
* found already in other library kexts. Adjust tallies appropriately.
*
* xxx - do we want to log stuff for this?
*******************************************************************************/
Boolean __OSKextFindSymbols(
    OSKextRef              aKext,
    CFMutableDictionaryRef undefSymbols,
    CFMutableDictionaryRef onedefSymbols,
    CFMutableDictionaryRef multdefSymbols,
    CFMutableArrayRef      multdefLibs)
{
    Boolean                    result        = false;
    char                       kextPath[PATH_MAX];

    CFDataRef                  executable    = NULL;  // must release
    const struct mach_header * mach_header   = NULL;
    const void               * file_end      = NULL;

    Boolean                    sixtyfourbit  = false;
    uint8_t                    swap          = 0;

    macho_seek_result          symtab_result = macho_seek_result_not_found;
    struct symtab_command    * symtab        = NULL;
    char                     * syms_address  = NULL;
    const void               * string_list   = NULL;
    unsigned int               sym_offset    = 0;
    unsigned int               str_offset    = 0;
    unsigned int               num_syms      = 0;
    unsigned int               syms_bytes    = 0;
    unsigned int               sym_index     = 0;
    CFMutableArrayRef          libsArray     = NULL;  // release if created
    OSKextRef                  libKext       = NULL;  // do not release
    char                     * symbol_name   = NULL;  // do not free
    Boolean                    eligible      = false;
    Boolean                    notedMultdef  = false;

    CFStringRef                cfSymbolName  = NULL;  // must release

    __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
        /* resolveToBase */ false, kextPath);

   /* Get the executable for the current arch, return false if it doesn't
    * have one.
    */
    executable = OSKextCopyExecutableForArchitecture(aKext, OSKextGetArchitecture());
    if (!executable) {
        goto finish;
    }

    mach_header = (const struct mach_header *)CFDataGetBytePtr(executable);
    file_end = (((const char *)mach_header) + CFDataGetLength(executable));

    if (ISMACHO64(MAGIC32(mach_header))) {
        sixtyfourbit = true;
    }
    if (ISSWAPPEDMACHO(MAGIC32(mach_header))) {
        swap = 1;
    }

    symtab_result = macho_find_symtab(mach_header, file_end, &symtab);
    if (symtab_result != macho_seek_result_found) {
        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogLinkFlag,
            "%s has no symtab in its executable (%s)",
                kextPath, OSKextGetArchitecture()->name);
        goto finish;
    }

    sym_offset = CondSwapInt32(swap, symtab->symoff);
    str_offset = CondSwapInt32(swap, symtab->stroff);
    num_syms   = CondSwapInt32(swap, symtab->nsyms);

    syms_address = (char *)mach_header + sym_offset;
    string_list = (char *)mach_header + str_offset;
    syms_bytes = num_syms * sizeof(struct nlist);

    if (syms_address + syms_bytes > (char *)file_end) {
        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogLinkFlag,
            "%s - internal overrun in executable file (%s).",
            kextPath, OSKextGetArchitecture()->name);
        goto finish;
    }

    for (sym_index = 0; sym_index < num_syms; sym_index++) {
        struct nlist    * seekptr;
        struct nlist_64 * seekptr_64;
        uint32_t          string_index;
        uint8_t           n_type;

        if (sixtyfourbit) {
            seekptr_64 = &((struct nlist_64 *)syms_address)[sym_index];
            string_index = CondSwapInt32(swap, seekptr_64->n_un.n_strx);
            n_type = seekptr_64->n_type;
        } else {
            seekptr = &((struct nlist *)syms_address)[sym_index];
            string_index = CondSwapInt32(swap, seekptr->n_un.n_strx);
            n_type = seekptr->n_type;
        }

        if (string_index == 0 || (n_type & N_STAB)) {
            continue;
        }

       /* Kernel component kexts are weird; they are just lists of indirect
        * and undefined symtab entries for the real data in the kernel.
        */

        // no need to swap n_type (one-byte value)
        switch (n_type & N_TYPE) {
          case N_UNDF:
            /* Fall through, only support indirects for KPI for now. */
          case N_INDR:
            eligible = OSKextIsKernelComponent(aKext) ? true : false;
            break;
          case N_SECT:
            eligible = OSKextIsKernelComponent(aKext) ? false : true;
            break;
          default:
            eligible = false;
            break;
        }

        if (eligible) {

            SAFE_RELEASE_NULL(cfSymbolName);

            symbol_name = (char *)(string_list + string_index);

            cfSymbolName = CFStringCreateWithCString(kCFAllocatorDefault,
                symbol_name, kCFStringEncodingASCII);
            if (!cfSymbolName) {
                OSKextLogMemError();
                result = false;
                goto finish;
            }

           /* Bubble library tallies from undef->onedef->multdef as symbols
            * are found. Also note any lib that has a duplicate match.
            */
            libsArray = (CFMutableArrayRef)CFDictionaryGetValue(
                multdefSymbols, cfSymbolName);
            if (libsArray) {

               /* The symbol is already multiply-defined, so just add this
                * kext to the list.
                */
                result = true;

                CFArrayAppendValue(libsArray, aKext);

            } else {
                libKext = (OSKextRef)CFDictionaryGetValue(
                    onedefSymbols, cfSymbolName);
                if (libKext) {

                   /* The symbol was found in one kext so far; now we have two.
                    * Create an array of those two kexts for the multdef dict,
                    * and remove the symbol from the onedef dict.
                    */
                    result = true;

                    libsArray = CFArrayCreateMutable(kCFAllocatorDefault,
                        /* capacity */ 0, &kCFTypeArrayCallBacks);
                    if (!libsArray) {
                        OSKextLogMemError();
                        goto finish;
                    }
                    CFArrayAppendValue(libsArray, libKext);
                    CFArrayAppendValue(libsArray, aKext);
                    CFDictionarySetValue(multdefSymbols, cfSymbolName, libsArray);
                    SAFE_RELEASE_NULL(libsArray);

                    if (!notedMultdef) {
                        if (kCFNotFound == CFArrayGetFirstIndexOfValue(
                            multdefLibs, RANGE_ALL(multdefLibs), aKext)) {
                        
                            CFArrayAppendValue(multdefLibs, aKext);
                        }
                        notedMultdef = true;
                    }
                    
                    CFDictionaryRemoveValue(onedefSymbols, cfSymbolName);

                } else {
                    if (CFDictionaryGetValue(undefSymbols, cfSymbolName)) {

                       /* The symbol just got found for the first time. Set
                        * this kext in the onedef dict, and remove the entry
                        * from the undef dict.
                        */
                        result = true;

                        CFDictionarySetValue(onedefSymbols, cfSymbolName, aKext);
                        CFDictionaryRemoveValue(undefSymbols, cfSymbolName);
                    }
                }
            }

        } /* if (eligible) */
    } /* for (...) */

finish:

   /* Advise the system that we no longer need the mmapped executable.
    */
    if (executable) {
        (void)posix_madvise((void *)CFDataGetBytePtr(executable),
            CFDataGetLength(executable),
            POSIX_MADV_DONTNEED);
    }
    SAFE_RELEASE(cfSymbolName);
    return result;
}

/*********************************************************************
*********************************************************************/
CFArrayRef OSKextFindLinkDependencies(
    OSKextRef         aKext,
    Boolean           nonKPIFlag,
    Boolean           allowUnsupportedFlag,
    CFDictionaryRef * undefinedSymbolsOut,
    CFDictionaryRef * onedefSymbolsOut,
    CFDictionaryRef * multiplyDefinedSymbolsOut,
    CFArrayRef      * multipleDefinitionLibraries)
{
    CFArrayRef             result         = NULL;
    CFArrayRef             allKexts       = NULL;  // do not release
    CFMutableArrayRef      libKexts       = NULL;  // must release
    CFMutableDictionaryRef undefSymbols   = NULL;  // must release
    CFMutableDictionaryRef onedefSymbols  = NULL;  // must release
    CFMutableDictionaryRef multdefSymbols = NULL;  // must release
    CFMutableArrayRef      multdefLibs    = NULL;  // must release
    char                   kextPath[PATH_MAX];
    char                   dependencyPath[PATH_MAX];
    CFIndex                kextCount, kextIndex;

   /* If this doesn't exist there's nothing we can do. No point
    * initializing it either, it'll be empty.
    */
    allKexts = OSKextGetAllKexts();
    if (!allKexts) {
        // xxx - log internal error?
        goto finish;
    }

    __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
        /* resolveToBase */ false, kextPath);
    OSKextLog(aKext,
        kOSKextLogStepLevel |
        kOSKextLogDependenciesFlag | kOSKextLogLinkFlag,
        "Searching for link dependencies of %s.",
        kextPath);

    libKexts = CFArrayCreateMutable(CFGetAllocator(aKext),
        0, &kCFTypeArrayCallBacks);
    undefSymbols = CFDictionaryCreateMutable(CFGetAllocator(aKext),
        0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    onedefSymbols = CFDictionaryCreateMutable(CFGetAllocator(aKext),
        0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    multdefSymbols = CFDictionaryCreateMutable(CFGetAllocator(aKext),
        0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    multdefLibs = CFArrayCreateMutable(CFGetAllocator(aKext),
        0, &kCFTypeArrayCallBacks);
    if (!libKexts || !undefSymbols || !onedefSymbols ||
        !multdefSymbols || !multdefLibs) {

        OSKextLogMemError();
        goto finish;
    }

    if (!__OSKextReadSymbolReferences(aKext, undefSymbols)) {
        goto finish;
    }

    kextCount = CFArrayGetCount(allKexts);
    for (kextIndex = 0; kextIndex < kextCount; kextIndex++) {
        OSKextRef thisKext = (OSKextRef)CFArrayGetValueAtIndex(
            allKexts, kextIndex);

        if (thisKext != aKext &&
            __OSKextIsSearchableForSymbols(thisKext, nonKPIFlag,
                allowUnsupportedFlag)) {

            if (__OSKextFindSymbols(thisKext, undefSymbols, onedefSymbols,
                multdefSymbols, multdefLibs)) {

                __OSKextGetFileSystemPath(thisKext, /* otherURL */ NULL,
                    /* resolveToBase */ false, dependencyPath);
                OSKextLog(aKext,
                    kOSKextLogDetailLevel | kOSKextLogDependenciesFlag | kOSKextLogLinkFlag,
                    "%s found link dependency %s.",
                    kextPath, dependencyPath);

                if (kCFNotFound == CFArrayGetFirstIndexOfValue(libKexts,
                    RANGE_ALL(libKexts), thisKext)) {

                    CFArrayAppendValue(libKexts, thisKext);
                }
            }
        }
    }

    CFArraySortValues(libKexts, RANGE_ALL(libKexts),
        &__OSKextCompareIdentifiers, /* context */ NULL);
    CFArraySortValues(multdefLibs, RANGE_ALL(multdefLibs),
        &__OSKextCompareIdentifiers, /* context */ NULL);
    result = CFRetain(libKexts);

finish:
    if (result) {
        CFIndex count;

        count = CFDictionaryGetCount(undefSymbols);
        if (count) {
            OSKextLog(aKext, kOSKextLogDetailLevel |
                kOSKextLogDependenciesFlag | kOSKextLogLinkFlag,
                "%s has %d remaining undefined symbol%s",
                kextPath, (int)count, count > 1 ? "s" : "");
        }

        count = CFDictionaryGetCount(multdefSymbols);
        if (count) {
            OSKextLog(aKext, kOSKextLogDetailLevel |
                kOSKextLogDependenciesFlag | kOSKextLogLinkFlag,
                "%s has multiply defined %ld symbol%s",
                kextPath, count, count > 1 ? "s" : "");
        }

        if (undefinedSymbolsOut) {
            *undefinedSymbolsOut = CFRetain(undefSymbols);
        }
        if (onedefSymbolsOut) {
            *onedefSymbolsOut = CFRetain(onedefSymbols);
        }
        if (multiplyDefinedSymbolsOut) {
            *multiplyDefinedSymbolsOut = CFRetain(multdefSymbols);
        }
        if (multipleDefinitionLibraries) {
            *multipleDefinitionLibraries = CFRetain(multdefLibs);
        }
    }

    SAFE_RELEASE(libKexts);
    SAFE_RELEASE(undefSymbols);
    SAFE_RELEASE(onedefSymbols);
    SAFE_RELEASE(multdefSymbols);
    SAFE_RELEASE(multdefLibs);
    return result;
}

/*********************************************************************
*********************************************************************/
typedef struct {
    CFMutableArrayRef array;
    uint32_t          minDepth;
    uint32_t          depth;
    Boolean           error;
} __OSKextAddDependenciesContext;

static void __OSKextAddDependenciesApplierFunction(
    const void * vKext,
          void * vContext)
{
    char kextPath[PATH_MAX];

    OSKextRef aKext = (OSKextRef)vKext;
    __OSKextAddDependenciesContext * context =
        (__OSKextAddDependenciesContext *)vContext;

   /* A kernel component has no dependencies other than an implicit
    * one on the kernel, and such a kext never has an array of
    * dependencies.
    */
    if (OSKextIsKernelComponent(aKext)) {
        goto finish;
    }

    if (!aKext->loadInfo || !aKext->loadInfo->dependencies) {
        __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL, /* resolve */ true,
            kextPath);
        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogDependenciesFlag,
            "%s - missing load info or dependencies array in applier function.",
            kextPath);
        context->error = true;
        goto finish;
    }

    context->depth++;
    CFArrayApplyFunction(aKext->loadInfo->dependencies,
        RANGE_ALL(aKext->loadInfo->dependencies),
        &__OSKextAddDependenciesApplierFunction, context);
    context->depth--;

finish:
    if (!context->error) {
        if (context->depth >= context->minDepth) {
            if (CFArrayGetFirstIndexOfValue(context->array,
                RANGE_ALL(context->array),
                aKext) == -1) {

                CFArrayAppendValue(context->array, aKext);
            }
        }
    }

    return;
}

/*********************************************************************
*********************************************************************/
CFMutableArrayRef __OSKextCopyDependenciesList(
    OSKextRef aKext,
    Boolean   needAllFlag,
    uint32_t  minDepth)
{
    CFMutableArrayRef result = NULL;
    Boolean resolved = false;
    __OSKextAddDependenciesContext context;

    resolved = OSKextResolveDependencies(aKext);
    if (needAllFlag && !resolved) {
        goto finish;
    }

    result = CFArrayCreateMutable(CFGetAllocator(aKext), 0,
        &kCFTypeArrayCallBacks);
    if (!result) {
        OSKextLogMemError();
        goto finish;
    }

    context.array = result;
    context.minDepth = minDepth;
    context.depth = 0;
    context.error = false;

    __OSKextAddDependenciesApplierFunction(aKext, &context);
    if (context.error) {
        SAFE_RELEASE_NULL(result);
    }

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
CFMutableArrayRef OSKextCopyLoadList(
    OSKextRef aKext,
    Boolean   needAllFlag)
{
   /* minDepth: 0 means include self
    */
    return __OSKextCopyDependenciesList(aKext, needAllFlag,
        /* minDepth */ 0);
}

/*********************************************************************
*********************************************************************/
CFMutableArrayRef OSKextCopyAllDependencies(
    OSKextRef aKext,
    Boolean needAllFlag)
{
   /* minDepth: 1 means skip self
    */
    return __OSKextCopyDependenciesList(aKext, needAllFlag,
        /* minDepth */ 1);
}

/*********************************************************************
*********************************************************************/
CFMutableArrayRef OSKextCopyIndirectDependencies(
    OSKextRef aKext,
    Boolean needAllFlag)
{
   /* minDepth: 2 means skip self & direct dependencies
    */
    return __OSKextCopyDependenciesList(aKext, needAllFlag,
        /* minDepth */ 2);
}

/*********************************************************************
*********************************************************************/
Boolean OSKextDependsOnKext(OSKextRef aKext,
    OSKextRef libraryKext,
    Boolean directFlag)
{
    Boolean result = false;
    CFIndex count, i;

    OSKextResolveDependencies(aKext);
    if (!aKext->loadInfo || !aKext->loadInfo->dependencies) {
        goto finish;
    }

    count = CFArrayGetCount(aKext->loadInfo->dependencies);
    for (i = 0; i < count; i++) {
        OSKextRef dependency = (OSKextRef)CFArrayGetValueAtIndex(
            aKext->loadInfo->dependencies, i);

        if ((dependency == libraryKext) ||
            (!directFlag && OSKextDependsOnKext(dependency, libraryKext,
                directFlag))) {

            result = true;
            goto finish;
        }
    }
finish:
    return result;
}

/*********************************************************************
*********************************************************************/
CFMutableArrayRef OSKextCopyDependents(OSKextRef aKext,
    Boolean directFlag)
{
    CFMutableArrayRef result = NULL;
    CFArrayRef        allKexts = NULL;   // do not release
    CFIndex           count, i;

   /* If this doesn't exist there's nothing we can do. No point
    * initializing it either, it'll be empty.
    */
    allKexts = OSKextGetAllKexts();
    if (!allKexts) {
        // xxx - log internal error?
        goto finish;
    }

    OSKextResolveDependencies(NULL);

    result = CFArrayCreateMutable(CFGetAllocator(aKext), 0,
        &kCFTypeArrayCallBacks);
    if (!result) {
        OSKextLogMemError();
        goto finish;
    }

    count = CFArrayGetCount(allKexts);
    for (i = 0; i < count; i++) {
        OSKextRef thisKext = (OSKextRef)CFArrayGetValueAtIndex(allKexts, i);
        if (OSKextDependsOnKext(thisKext, aKext, /* direct */ directFlag)) {
            CFArrayAppendValue(result, thisKext);
        }
    }

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
Boolean OSKextIsCompatibleWithVersion(
    OSKextRef aKext,
    OSKextVersion aVersion)
{
    Boolean result = false;

   /* Be sure to check that the kext *has* a valid compatibleVersion
    * (parsed > 0) as well as checking the range.
    */
    if (aKext->compatibleVersion > 0 &&
        aKext->compatibleVersion <= aVersion &&
        aKext->version >= aVersion) {

        result = true;
    }
    return result;
}

/*********************************************************************
*********************************************************************/
typedef struct __OSKextPrintDependenciesContext {
    UInt32  depth;
    Boolean bundleIDFlag;
    Boolean linkFlag;
} __OSKextPrintDependenciesContext;

#define _DEPENDENCY_DEPTH_INCREMENT  (4)

void __OSKextLogDependencyGraphApplierFunction(
    const void * vKext,
          void * vContext)
{
    OSKextRef aKext = (OSKextRef)vKext;
    __OSKextPrintDependenciesContext dContext =
        *(__OSKextPrintDependenciesContext *)vContext;

    char        * pad = NULL;  // must free
    CFStringRef   kextLabel = NULL;  // must release
    char        * kextName = NULL; // must free
    char          kextVersion[kOSKextVersionMaxLength];
    char        * note = ""; // do not free
    CFArrayRef    linkDependencies = NULL;  // must release
    CFArrayRef    dependencies = NULL;      // do not release

    if (!OSKextResolveDependencies(aKext)) {
        // xxx - log it?
        goto finish;
    }

    if (dContext.depth) {
        pad = (char *)malloc((1 + dContext.depth) * sizeof(char));
        if (!pad) {
            OSKextLogMemError();
            goto finish;
        }
        memset(pad, ' ', dContext.depth);
        pad[dContext.depth] = '\0';
    }

    if (dContext.bundleIDFlag) {
        kextLabel = OSKextGetIdentifier(aKext);
        CFRetain(kextLabel);
    } else {
        // xxx - relative or absolute?
        kextLabel = CFURLCopyLastPathComponent(aKext->bundleURL);
    }
    kextName = createUTF8CStringForCFString(kextLabel);
    if (!kextName) {
        goto finish;
    }

    OSKextVersionGetString(aKext->version, kextVersion, kOSKextVersionMaxLength);

   /* Mark kexts with final or missing dependencies.
    */
    if (!aKext->loadInfo || !aKext->loadInfo->dependencies) {
        if (__OSKextHasAllDependencies(aKext)) {
            note = ".";  // stops here (at the kernel, really)
        } else {
            note = " (dependencies not resolved).";
        }
    } else {
        if (__OSKextHasAllDependencies(aKext)) {
            note = " ->";
        } else {
            note = " (dependencies not fully resolved) ->";
        }
    }

    OSKextLog(/* kext */ NULL,
        kOSKextLogExplicitLevel | kOSKextLogDependenciesFlag,
        "%s%s (%s)%s", pad ? pad : "", kextName, kextVersion, note);

    dContext.depth += _DEPENDENCY_DEPTH_INCREMENT;
    if (dContext.linkFlag) {
        linkDependencies = OSKextCopyLinkDependencies(aKext,
            /* needAll */ false);
        dependencies = linkDependencies;
    } else if (aKext->loadInfo && aKext->loadInfo->dependencies) {
        dependencies = aKext->loadInfo->dependencies;
    }

    if (dependencies) {
        CFArrayApplyFunction(dependencies, RANGE_ALL(dependencies),
            __OSKextLogDependencyGraphApplierFunction, &dContext);
    }

finish:
    SAFE_FREE(pad);
    SAFE_FREE(kextName);
    SAFE_RELEASE(kextLabel);
    SAFE_RELEASE(linkDependencies);
    return;
}

void OSKextLogDependencyGraph(OSKextRef aKext,
    Boolean bundleIDFlag,
    Boolean linkFlag)
{
    __OSKextPrintDependenciesContext context;

    context.depth = 0;
    context.bundleIDFlag = bundleIDFlag;
    context.linkFlag = linkFlag;

    OSKextResolveDependencies(aKext);
    __OSKextLogDependencyGraphApplierFunction(aKext, &context);
}

#pragma mark Core Kernel IPC

/*********************************************************************
*********************************************************************/
CFMutableDictionaryRef __OSKextCreateKextRequest(
    CFStringRef              predicateIn,
    CFTypeRef                bundleIdentifierIn,
    CFMutableDictionaryRef * argumentsOut)
{
    CFMutableDictionaryRef result    = NULL;
    CFMutableDictionaryRef arguments = NULL;  // must release
    Boolean                error     = false;

    result = CFDictionaryCreateMutable(kCFAllocatorDefault,
        0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!result) {
        OSKextLogMemError();
        goto finish;
    }
    CFDictionarySetValue(result, CFSTR(kKextRequestPredicateKey), predicateIn);
    if (bundleIdentifierIn || argumentsOut) {
        arguments = CFDictionaryCreateMutable(kCFAllocatorDefault,
            0, &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
        if (!arguments) {
            OSKextLogMemError();
            error = true;
            goto finish;
        }

        CFDictionarySetValue(result, CFSTR(kKextRequestArgumentsKey),
            arguments);

        if (argumentsOut) {
            *argumentsOut = arguments;
        }

        if (bundleIdentifierIn) {
            CFDictionarySetValue(arguments,
                CFSTR(kKextRequestArgumentBundleIdentifierKey),
                bundleIdentifierIn);
        }
    }
finish:
    if (error) {
        SAFE_RELEASE_NULL(result);
    }
    SAFE_RELEASE(arguments);
    return result;
}

/*********************************************************************
*********************************************************************/
OSReturn __OSKextSendKextRequest(
    OSKextRef       aKext,
    CFDictionaryRef kextRequest,
    CFTypeRef     * cfResponseOut,
    char         ** rawResponseOut,
    uint32_t      * rawResponseLengthOut)
{
    OSReturn       result            = kOSReturnError;
    kern_return_t  mig_result        = KERN_FAILURE;
    OSReturn       op_result         = kOSReturnError;
    CFDataRef      requestData       = NULL;  // must release
    Boolean        deallocResponse   = true; 
    char         * responseBuffer    = NULL;  // must vm_dealloc per deallocResponse
    uint32_t       responseLength    = 0;
    char         * logInfoBuffer     = NULL;  // must vm_dealloc
    uint32_t       logInfoLength     = 0;
    host_priv_t    hostPriv          = HOST_PRIV_NULL; // xxx - need to clean up?
    CFStringRef    errorString       = NULL;  // must release
    char         * errorCString      = NULL;  // must free

   /* Technically not necessary since we are just getting info.
    */
    hostPriv = mach_host_self();

    requestData = IOCFSerialize(kextRequest, kNilOptions);
    if (!requestData) {
        result = kOSKextReturnSerialization;
        OSKextLog(aKext,
            kOSKextLogErrorLevel | kOSKextLogIPCFlag,
            "Failed to serialize kext request.");
        goto finish;
    }

   /* If we don't have a log function to process the messages,
    * don't bother sending any log flags to the kernel.
    */
    mig_result = kext_request(
        hostPriv,
        __sOSKextLogOutputFunction ? __sKernelLogFilter : kOSKextLogSilentFilter,
        (vm_offset_t)CFDataGetBytePtr(requestData),
        CFDataGetLength(requestData),
        (vm_offset_t *)&responseBuffer,
        &responseLength,
        (vm_offset_t *)&logInfoBuffer,
        &logInfoLength,
        &op_result);

    result = __OSKextProcessKextRequestResults(aKext,
        mig_result, op_result, 
        (char *)logInfoBuffer, logInfoLength);
    if (result != kOSReturnSuccess) {
        goto finish;
    }

    if (responseBuffer && responseLength) {
        if (cfResponseOut) {

            *cfResponseOut = IOCFUnserialize(responseBuffer, kCFAllocatorDefault,
                /* options */ 0, &errorString);
            if (!*cfResponseOut) {
                result = kOSKextReturnSerialization;
                if (errorString) {
                    errorCString = createUTF8CStringForCFString(errorString);
                }
                OSKextLog(aKext,
                    kOSKextLogErrorLevel | kOSKextLogIPCFlag,
                    "Can't unserialize kext request response: %s",
                    errorCString ? errorCString : "unknown error");
                SAFE_FREE_NULL(errorCString);
                SAFE_RELEASE_NULL(errorString);
                goto finish;
            }
        } else if (rawResponseOut && rawResponseLengthOut) {
            *rawResponseOut = responseBuffer;
            *rawResponseLengthOut = responseLength;
            deallocResponse = false;
        }
    }

    result = kOSReturnSuccess;

finish:
    SAFE_RELEASE(requestData);
    SAFE_RELEASE(errorString);
    SAFE_FREE(errorCString);

    if (deallocResponse && responseBuffer && responseLength) {
        vm_deallocate(mach_task_self(), (vm_address_t)responseBuffer,
            responseLength);
    }
    if (logInfoBuffer) {
        vm_deallocate(mach_task_self(), (vm_address_t)logInfoBuffer,
            logInfoLength);
    }

    if (hostPriv != HOST_PRIV_NULL) {
        mach_port_deallocate(mach_task_self(), hostPriv);
    }

    return result;
}

/*********************************************************************
*********************************************************************/
OSReturn __OSKextSimpleKextRequest(
    OSKextRef     aKext,
    CFStringRef   predicate,
    CFTypeRef   * cfResponseOut)
{
    kern_return_t   result        = kOSKextReturnInternalError;
    CFDictionaryRef kextRequest   = NULL;  // must release

    kextRequest = __OSKextCreateKextRequest(predicate,
        aKext ? OSKextGetIdentifier(aKext) : NULL, /* argsOut */ NULL);
    if (!kextRequest) {
        goto finish;
    }

    result = __OSKextSendKextRequest(aKext, kextRequest,
        cfResponseOut,
        /* rawResponseOut */ NULL, /* rawResponseLengthOut */ NULL);

finish:
    SAFE_RELEASE(kextRequest);
    return result;
}

/*********************************************************************
*********************************************************************/
OSReturn __OSKextProcessKextRequestResults(
    OSKextRef       aKext,
    kern_return_t   mig_result,
    kern_return_t   op_result,
    char          * logInfoBuffer,
    uint32_t        logInfoLength)
{
    OSReturn    result             = kOSReturnError;
    CFTypeRef   kernelLogMessages  = NULL;  // must release
    CFStringRef errorString        = NULL;  // must release
    char      * errorCString       = NULL;  // must free

    if (mig_result != KERN_SUCCESS) {
        OSKextLog(aKext,
            kOSKextLogErrorLevel | kOSKextLogIPCFlag,
             "Error communicating with kernel - %s.",
             safe_mach_error_string(mig_result));
        result = mig_result;
        goto finish;
    }

   /* Errors here are not to be considered problems with the request
    * itself. Log about the problem but continue as if things worked.
    */
    if (logInfoBuffer && logInfoLength) {
        kernelLogMessages = IOCFUnserialize((char *)logInfoBuffer,
            kCFAllocatorDefault,
            /* options */ 0, &errorString);
        if (kernelLogMessages) {
            __OSKextLogKernelMessages(aKext, kernelLogMessages);
        } else {
            errorCString = createUTF8CStringForCFString(errorString);
            OSKextLog(aKext,
                kOSKextLogErrorLevel | kOSKextLogIPCFlag,
                "Failed to parse kernel log messages: %s.",
                errorCString ? errorCString : "(unknown)");
        }
    }
    
   /* It's really the job of the caller to provide a specific error message.
    * We'll use this one for debugging.
    */
    if (op_result != KERN_SUCCESS) {
        OSKextLog(aKext, kOSKextLogDebugLevel | kOSKextLogIPCFlag,
            "Kernel error handling kext request - %s.",
            safe_mach_error_string(op_result));
        result = op_result;
        goto finish;
    }
    
    result = kOSReturnSuccess;
    
finish:

    SAFE_RELEASE(kernelLogMessages);
    SAFE_RELEASE(errorString);
    SAFE_FREE(errorCString);

    return result;
}

#pragma mark Linking and Loading; Other Kernel Operations

/*********************************************************************
*********************************************************************/
OSReturn __OSKextLoadWithArgsDict(
    OSKextRef       aKext,
    CFDictionaryRef loadArgsDict)
{
    OSReturn           result          = kOSReturnError;
    CFArrayRef         loadList        = NULL;           // must release
    CFMutableArrayRef  kextIdentifiers = NULL;           // must release
    kern_return_t      mig_result      = KERN_FAILURE;
    OSReturn           op_result       = kOSReturnError;
    host_priv_t        hostPriv        = HOST_PRIV_NULL; // xxx - need to clean up?
    CFDataRef          mkext           = NULL;           // must release
    const UInt8      * requestBuffer   = NULL;
    CFIndex            requestLength   = 0;
    vm_address_t       responseBuffer  = 0;              // must vm_deallocate
    uint32_t           responseLength  = 0;
    vm_address_t       logInfoBuffer   = 0;              // must vm_deallocate
    uint32_t           logInfoLength   = 0;
    CFStringRef        errorString     = NULL;           // must release
    char               kextPath[PATH_MAX];
    CFIndex            count = 0, i = 0;

   /* If we are privileged this will work.
    */
    hostPriv = mach_host_self();
    if (hostPriv == HOST_PRIV_NULL) {
        result = kOSKextReturnNotPrivileged;
        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogLoadFlag,
             "Process must be running as root to load kexts.");
        goto finish;
    }

    __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
        /* resolveToBase */ false, kextPath);

   /*****
    * Do all the checks we can before sending the kext down to the kernel.
    *****/

    if (!__OSKextIsValid(aKext)) {
        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogLoadFlag,
            "Can't load %s - validation problems.", kextPath);
        result = kOSKextReturnValidation;
        goto finish;
    }

    if (!OSKextSupportsArchitecture(aKext, OSKextGetArchitecture())) {
        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogLoadFlag,
            "Can't load %s - no code for running kernel's architecture.",
            kextPath);
        result = kOSKextReturnArchNotFound;
        goto finish;
    }

    if ((OSKextGetActualSafeBoot() || OSKextGetSimulatedSafeBoot()) &&
        !OSKextIsLoadableInSafeBoot(aKext)) {

        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogLoadFlag,
            "Can't load %s - ineligible during safe boot.", kextPath);
        result = kOSKextReturnBootLevel;
        goto finish;
    }

    if (!OSKextIsAuthentic(aKext)) {
        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogLoadFlag,
            "Can't load %s - authentication problems.", kextPath);
        result = kOSKextReturnAuthentication;
        goto finish;
    }

   /* First resolve dependencies without loaded kext info to build
    * a list of identifiers, so we don't read any more bundles off
    * disk than necessary when we do check loaded. If we have different
    * versions of libraries that have wildly different dependencies
    * amongst themselves, things may fail in the next resolve step,
    * but it's really unlikely innit.
    */
    OSKextFlushLoadInfo(/* kext */ NULL, /* flushDependencies? */ true);
    loadList = OSKextCopyLoadList(aKext, /* needAll? */ true);
    if (!loadList) {
        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogLoadFlag,
            "Can't load %s - failed to resolve dependencies.", kextPath);
        result = kOSKextReturnDependencies;
        goto finish;
    }

    if (!OSKextAuthenticateDependencies(aKext)) {
        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogLoadFlag,
            "Can't load %s - dependency authentication problems.", kextPath);
        result = kOSKextReturnAuthentication;
        goto finish;
    }

    if (!OSKextDependenciesAreLoadableInSafeBoot(aKext)) {
        CFDictionaryRef bootLevelDiagnostics         = NULL;  // do not release
        CFArrayRef      ineligibleDependencies       = NULL;  // must release
        CFStringRef     ineligibleDependenciesString = NULL;  // must release

        bootLevelDiagnostics = __OSKextGetDiagnostics(aKext,
            kOSKextDiagnosticsFlagBootLevel);
        if (bootLevelDiagnostics) {
            ineligibleDependencies = CFDictionaryGetValue(bootLevelDiagnostics,
                kOSKextDependencyIneligibleInSafeBoot);
        }
        if (ineligibleDependencies && CFArrayGetCount(ineligibleDependencies)) {
            ineligibleDependenciesString = createCFStringForPlist_new(
                ineligibleDependencies, kPListStyleDiagnostics);
        }

        if (ineligibleDependenciesString) {
            OSKextLogCFString(aKext, kOSKextLogErrorLevel | kOSKextLogLoadFlag,
                CFSTR("Can't load %s - dependencies not elibible during safe boot:\n%@"),
                kextPath, ineligibleDependenciesString);
        } else {
            OSKextLogCFString(aKext, kOSKextLogErrorLevel | kOSKextLogLoadFlag,
                CFSTR("Can't load %s - dependencies not elibible during safe boot."),
                kextPath);
        }
        
        SAFE_RELEASE_NULL(ineligibleDependencies);
        SAFE_RELEASE_NULL(ineligibleDependenciesString);

        result = kOSKextReturnBootLevel;
        goto finish;
    }

    count = CFArrayGetCount(loadList);
    kextIdentifiers = CFArrayCreateMutable(CFGetAllocator(aKext),
        count, &kCFTypeArrayCallBacks);
    if (!kextIdentifiers) {
        OSKextLogMemError();
        goto finish;
    }
    for (i = 0; i < count; i++) {
        OSKextRef thisKext = (OSKextRef)CFArrayGetValueAtIndex(loadList, i);
        CFArrayAppendValue(kextIdentifiers, OSKextGetIdentifier(thisKext));
    }
    SAFE_RELEASE_NULL(loadList);

   /* Now find out who's really loaded in the kernel, and flush dependency
    * graphs again so we can build them based on who's loaded.
    */
    result = OSKextReadLoadedKextInfo(kextIdentifiers,
        /* flushDependencies? */ true);
    if (result != kOSReturnSuccess) {
        // called function logs well enough
        goto finish;
    }

   /* Check before bothering further whether another version/UUID of
    * the kexts being loaded is already loaded, and bail if so.
    */
    if (!OSKextIsLoaded(aKext)) {
        Boolean      otherVersionLoaded = false;
        Boolean      otherUUIDLoaded    = false;
        const char * difference = NULL;
        
        otherVersionLoaded = OSKextOtherVersionIsLoaded(aKext, &otherUUIDLoaded);
        if (otherUUIDLoaded) {
            difference = "UUID";
        } else if (otherVersionLoaded) {
            difference = "version";
        }
        if (difference) {
            OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogLoadFlag,
                "Can't load %s - a different %s is already loaded.",
                kextPath, difference);
            result = kOSKextReturnLoadedVersionDiffers;
            goto finish;
        }
    }
    
   /* Ok, now resolve dependencies based on kexts having their load info.
    * Hopefully it'll still work.
    */
    loadList = OSKextCopyLoadList(aKext, /* needAll? */ true);
    if (!loadList) {
        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogLoadFlag,
            "Can't load %s - failed to resolve dependencies based on loaded kexts.",
            kextPath);
        result = kOSKextReturnDependencies;
        goto finish;
    }

    if (!OSKextAuthenticateDependencies(aKext)) {
        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogLoadFlag,
            "Can't load %s - dependency authentication problems.", kextPath);
        result = kOSKextReturnAuthentication;
        goto finish;
    }

    // construct mkext w/o compression & w/o loaded kexts in it
    mkext = __OSKextCreateMkext(CFGetAllocator(aKext), loadList,
        /* volumeRootURL */ NULL,
        /* requiredFlags */ 0,
        /* compress */ false,
        /* skipLoaded */ true,
        loadArgsDict);
    if (!mkext) {
        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogLoadFlag,
             "Can't create kernel load request for %s.", kextPath);
        goto finish;
    }

    requestBuffer = CFDataGetBytePtr(mkext);
    requestLength = CFDataGetLength(mkext);

    OSKextLog(aKext, kOSKextLogProgressLevel | kOSKextLogLoadFlag,
         "Loading %s.", kextPath);

   /* We don't actually expect a response, but try to tell MIG that
    * by passing a NULL pointer and it throws a hissy fit and crashes
    * your program. Fine, MIG; you has a bukkit for the response that
    * ain't coming. Are you happy now?
    *
    * Also, if we don't have a log function to process the messages,
    * don't bother sending any log flags to the kernel.
    */
    mig_result = kext_request(
        hostPriv,
        __sOSKextLogOutputFunction ? __sKernelLogFilter : kOSKextLogSilentFilter,
        (vm_offset_t)requestBuffer,
        (mach_msg_type_number_t)requestLength,
        &responseBuffer,
        &responseLength,
        (vm_offset_t *)&logInfoBuffer,
        &logInfoLength,
        &op_result);

    result = __OSKextProcessKextRequestResults(aKext,
        mig_result, op_result,
        (char *)logInfoBuffer, logInfoLength);
    if (result != kOSReturnSuccess) {
        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogLoadFlag,
            "Failed to load %s - %s.",
            kextPath, safe_mach_error_string(result));
        goto finish;
    }

finish:
    SAFE_RELEASE(kextIdentifiers);
    SAFE_RELEASE(loadList);
    SAFE_RELEASE(mkext);
    SAFE_RELEASE(errorString);
    if (responseBuffer) {
        vm_deallocate(mach_task_self(), responseBuffer, responseLength);
    }
    if (logInfoBuffer) {
        vm_deallocate(mach_task_self(), logInfoBuffer, logInfoLength);
    }

    if (result == kOSReturnSuccess) {
        OSKextLog(aKext, kOSKextLogProgressLevel | kOSKextLogLoadFlag,
            "Successfully loaded %s.", kextPath);
    } else {
        // error points have logged specific reason for failure
        OSKextRemoveKextPersonalitiesFromKernel(aKext);
    }

    return result;
}

/*********************************************************************
*********************************************************************/
OSReturn OSKextLoad(OSKextRef aKext)
{
    return OSKextLoadWithOptions(aKext,
        /* startExclusion */ kOSKextExcludeNone,
        /* addPersonalitiesExclusion */ kOSKextExcludeAll,
        /* personalityNames */ NULL,
        /* delayAutounload */ false);
}

/*********************************************************************
*********************************************************************/
OSReturn OSKextLoadWithOptions(
    OSKextRef           aKext,
    OSKextExcludeLevel  startExclusion,
    OSKextExcludeLevel  addPersonalitiesExclusion,
    CFArrayRef          personalityNames,
    Boolean             delayAutounloadFlag)
{
    OSReturn               result                       = kOSReturnError;
    CFMutableDictionaryRef loadArgs                     = NULL;  // must release
    CFNumberRef            startExclusionNum            = NULL;  // must release
    CFNumberRef            addPersonalitiesExclusionNum = NULL;  // must release

   /* loadArgs will be set in the mkext under the "arguments" key, containing:
    *     "bundle ID" = <bundle ID>
    *     "startKext" = bool
    *     "startMatching" = bool
    *     "disableAutounload" = bool
    */
    // construct load request dict (wow this is verbose)
    loadArgs = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!loadArgs) {
        OSKextLogMemError();
        goto finish;
    }

    CFDictionarySetValue(loadArgs,
        CFSTR(kKextRequestArgumentBundleIdentifierKey),
        OSKextGetIdentifier(aKext));

    startExclusionNum = CFNumberCreate(CFGetAllocator(aKext),
        kCFNumberSInt8Type,
        &startExclusion);
    if (!startExclusionNum) {
        OSKextLogMemError();
        goto finish;
    }
    CFDictionarySetValue(loadArgs, CFSTR(kKextRequestArgumentStartExcludeKey),
        startExclusionNum);

    addPersonalitiesExclusionNum = CFNumberCreate(CFGetAllocator(aKext),
        kCFNumberSInt8Type,
        &addPersonalitiesExclusion);
    if (!addPersonalitiesExclusionNum) {
        OSKextLogMemError();
        goto finish;
    }
    CFDictionarySetValue(loadArgs, CFSTR(kKextRequestArgumentStartMatchingExcludeKey),
        addPersonalitiesExclusionNum);

    if (personalityNames) {
        CFDictionarySetValue(loadArgs,
            CFSTR(kKextRequestArgumentPersonalityNamesKey),
            personalityNames);
    }

    if (delayAutounloadFlag) {
        CFDictionarySetValue(loadArgs, CFSTR(kKextRequestArgumentDelayAutounloadKey),
            kCFBooleanTrue);
    }

    result = __OSKextLoadWithArgsDict(aKext, loadArgs);

finish:
    SAFE_RELEASE(loadArgs);
    SAFE_RELEASE(startExclusionNum);
    SAFE_RELEASE(addPersonalitiesExclusionNum);
    return result;
}

#ifndef IOKIT_EMBEDDED
/*********************************************************************
*********************************************************************/
Boolean __OSKextInitKXLDDependency(
    KXLDDependency * dependency,
    OSKextRef        aKext,
    CFDataRef        kernelImage,
    Boolean          isDirect)
{
    Boolean result = FALSE;
    char    kextPath[PATH_MAX];

    if (!aKext->loadInfo->linkedExecutable) {
        __OSKextGetFileSystemPath(aKext,
            /* otherURL */ NULL,
            /* resolveToBase */ false, kextPath);
        OSKextLog(aKext, kOSKextLogErrorLevel,
            "Can't use %s - not linked.", kextPath);
        goto finish;
    }

    if (OSKextIsInterface(aKext)) {
        CFDataRef   interfaceTarget     = NULL;
        CFStringRef interfaceTargetName = NULL;

        if (OSKextIsKernelComponent(aKext)) {
            interfaceTarget = kernelImage;
            interfaceTargetName = __kOSKextKernelIdentifier;
        } else {
            OSKextRef interfaceTargetKext = (OSKextRef) 
                CFArrayGetValueAtIndex(aKext->loadInfo->dependencies, 0);

            if (!interfaceTargetKext->loadInfo->linkedExecutable) {
                __OSKextGetFileSystemPath(interfaceTargetKext,
                    /* otherURL */ NULL,
                    /* resolveToBase */ false, kextPath);

                OSKextLog(aKext, kOSKextLogErrorLevel,
                    "Can't use %s - not linked.", kextPath);
                goto finish;
            }
            interfaceTarget = 
                interfaceTargetKext->loadInfo->linkedExecutable;
            interfaceTargetName = interfaceTargetKext->bundleID;
        }

        dependency->kext = (u_char *) 
            CFDataGetBytePtr(interfaceTarget);
        dependency->kext_size = CFDataGetLength(interfaceTarget);
        dependency->kext_name = 
            createUTF8CStringForCFString(interfaceTargetName);

        dependency->interface = (u_char *) 
            CFDataGetBytePtr(aKext->loadInfo->linkedExecutable);
        dependency->interface_size =
            CFDataGetLength(aKext->loadInfo->linkedExecutable);
        dependency->interface_name = createUTF8CStringForCFString(
            OSKextGetIdentifier(aKext));
    } else {
        dependency->kext = (u_char *)
            CFDataGetBytePtr(aKext->loadInfo->linkedExecutable);
        dependency->kext_size = 
            CFDataGetLength(aKext->loadInfo->linkedExecutable);
        dependency->kext_name = createUTF8CStringForCFString(
            OSKextGetIdentifier(aKext));

        dependency->interface = NULL;
        dependency->interface_size = 0;
        dependency->interface_name = NULL;
    }

    dependency->is_direct_dependency = isDirect;
    result = TRUE;

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
CFDataRef __OSKextCopyStrippedExecutable(OSKextRef aKext)
{
    CFDataRef result = NULL;
    CFMutableDataRef strippedExecutable = NULL;
    u_char *file;
    u_long linkeditSize;

    if (!aKext->loadInfo->linkedExecutable) goto finish;

    strippedExecutable = CFDataCreateMutableCopy(kCFAllocatorDefault,
        CFDataGetLength(aKext->loadInfo->linkedExecutable),
        aKext->loadInfo->linkedExecutable);
    if (!strippedExecutable) goto finish;

    file = (u_char *) CFDataGetMutableBytePtr(strippedExecutable);
    if (!file) goto finish;

    /* Find the linkedit segment.  If it's not the last segment, then freeing
     * it will fragment the kext into multiple VM regions, so we'll have to skip it.
     */
    if (__OSKextIsArchitectureLP64()) {
        struct segment_command_64 * linkedit =
            macho_get_segment_by_name_64((struct mach_header_64 *)file, SEG_LINKEDIT);
        if (!linkedit) goto finish;

        if ((round_page(aKext->loadInfo->loadAddress) + round_page(aKext->loadInfo->loadSize)) !=
             round_page(linkedit->vmaddr) + round_page(linkedit->vmsize))
        {
            goto finish;
        }
    } else {
        struct segment_command * linkedit =
            macho_get_segment_by_name((struct mach_header *)file, SEG_LINKEDIT);
        if (!linkedit) goto finish;

        if ((round_page(aKext->loadInfo->loadAddress) + round_page(aKext->loadInfo->loadSize)) !=
             round_page(linkedit->vmaddr) + round_page(linkedit->vmsize))
        {
            goto finish;
        }
    }

    /* Remove the headers and truncate the data object */

    if (!macho_remove_linkedit(file, &linkeditSize)) {
        goto finish;
    }
    CFDataSetLength(strippedExecutable, CFDataGetLength(strippedExecutable) - linkeditSize);

    result = CFRetain(strippedExecutable);
finish:
    SAFE_RELEASE_NULL(strippedExecutable);
    return result;
}

/*********************************************************************
*********************************************************************/
static Boolean __OSKextPerformLink(
    OSKextRef               aKext,
    CFDataRef               kernelImage,
    uint64_t                kernelLoadAddress,
    Boolean                 stripSymbolsFlag,
    KXLDContext           * kxldContext)
{
    Boolean                    result              = false;
    char                     * bundleIDCString     = NULL;      // must free
    CFArrayRef                 dependencies        = NULL;      // must release
    CFMutableArrayRef          indirectDependencies = NULL;     // must release
    CFDataRef                  kextExecutable      = NULL;      // must release

    kern_return_t              kxldResult          = KERN_FAILURE;
    KXLDDependency           * kxldDependencies    = NULL;      // must free
    CFIndex                    numKxldDependencies = 0;
    kxld_addr_t                kmodInfoKern        = 0;

    CFIndex                    numDirectDependencies    = 0;
    CFIndex                    numIndirectDependencies  = 0;

    __OSKextKXLDCallbackContext linkAddressContext;

    u_char                   * relocBytes          = NULL;    // do not free
    u_char                  ** relocBytesPtr       = NULL;    // do not free

    CFDataRef                  relocData           = NULL;    // must release
    char                       kextPath[PATH_MAX];
    CFIndex                    i;

    if (!OSKextDeclaresExecutable(aKext)) {
        result = true;
        goto finish;
    }

    __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
        /* resolveToBase */ false, kextPath);

    kextExecutable = OSKextCopyExecutableForArchitecture(aKext,
        OSKextGetArchitecture());
    if (!kextExecutable) {
        OSKextLog(aKext, kOSKextLogErrorLevel,
            "Can't link %s - architecture %s not found", kextPath,
            OSKextGetArchitecture()->name);

        goto finish;
    }

    if (OSKextIsInterface(aKext)) {
        aKext->loadInfo->linkedExecutable = CFRetain(kextExecutable);
        aKext->loadInfo->prelinkedExecutable = CFRetain(kextExecutable);
        aKext->loadInfo->loadSize = CFDataGetLength(kextExecutable);
        result = true;
        goto finish;
    }

    dependencies = OSKextCopyLinkDependencies(aKext, /* needAll */ true);
    if (!dependencies) {
        goto finish;
    }

    OSKextLog(aKext, kOSKextLogProgressLevel | kOSKextLogLinkFlag,
        "Linking %s.", kextPath);

    bundleIDCString = createUTF8CStringForCFString(
        OSKextGetIdentifier(aKext));


    numDirectDependencies = CFArrayGetCount(dependencies);
    if (!numDirectDependencies) {
        OSKextLog(aKext,
            kOSKextLogErrorLevel | kOSKextLogLinkFlag,
            "Internal error: attempting to link a kext without its dependencies.");
        goto finish;
    }

    if (__OSKextGetBleedthroughFlag(aKext)) {
        numIndirectDependencies = 0;
    } else {
        indirectDependencies = OSKextCopyIndirectDependencies(aKext,
            /* needAllFlag */ TRUE);
        if (!indirectDependencies) {
            goto finish;
        }

        for (i = 0; i < CFArrayGetCount(indirectDependencies); ++i) {
            OSKextRef indirectDependency = 
                (OSKextRef) CFArrayGetValueAtIndex(indirectDependencies, i);

           /* Filter out duplicates and codeless kexts.
            */
            if (!OSKextDeclaresExecutable(indirectDependency) ||
                CFArrayContainsValue(dependencies, RANGE_ALL(dependencies),
                    indirectDependency)) 
            {
                CFArrayRemoveValueAtIndex(indirectDependencies, i);
                i--;
                continue;
            }
        }

        numIndirectDependencies = CFArrayGetCount(indirectDependencies);
    }

    numKxldDependencies = numDirectDependencies + numIndirectDependencies;
    kxldDependencies = (KXLDDependency *) 
        calloc(numKxldDependencies, sizeof(*kxldDependencies));
    if (!kxldDependencies) {
        OSKextLogMemError();
        goto finish;
    }

    for (i = 0; i < numDirectDependencies; i++) {
        OSKextRef dependency = 
            (OSKextRef) CFArrayGetValueAtIndex(dependencies, i);

        if (!__OSKextInitKXLDDependency(&kxldDependencies[i],
            dependency, kernelImage, TRUE)) {
            
            goto finish;
        }
    }

    for (i = 0; i < numIndirectDependencies; i++) {
        OSKextRef dependency = 
            (OSKextRef) CFArrayGetValueAtIndex(indirectDependencies, i);

        if (!__OSKextInitKXLDDependency(
            &kxldDependencies[i + numDirectDependencies], 
            dependency, kernelImage, FALSE)) {
            
            goto finish;
        }
    }

    relocBytesPtr = &relocBytes;

   /* Advise the system that we need all the mmapped bytes right away.
    */
    (void)posix_madvise((void *)CFDataGetBytePtr(kextExecutable),
        CFDataGetLength(kextExecutable),
        POSIX_MADV_WILLNEED);

    linkAddressContext.kernelLoadAddress = kernelLoadAddress;
    linkAddressContext.kext = aKext;

    kxldResult = kxld_link_file(kxldContext,
        (void *)CFDataGetBytePtr(kextExecutable),
        CFDataGetLength(kextExecutable),
        bundleIDCString,
        /* callbackData */ (void *)&linkAddressContext,
        kxldDependencies, numKxldDependencies,
        relocBytesPtr, /* kmod_info */ &kmodInfoKern);

    for (i = 0; i < numKxldDependencies; i++) {
        SAFE_FREE(kxldDependencies[i].kext_name);
        SAFE_FREE(kxldDependencies[i].interface_name);
    }

    if (kxldResult != KERN_SUCCESS) {
        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogLinkFlag,
            "Link failed (error code %d).", kxldResult);
        goto finish;
    }

    if (relocBytes && aKext->loadInfo->loadSize) {
        relocData = CFDataCreateWithBytesNoCopy(CFGetAllocator(aKext),
            relocBytes, aKext->loadInfo->loadSize, kCFAllocatorDefault);
        if (!relocData) {
            OSKextLogMemError();
            goto finish;
        }
        aKext->loadInfo->linkedExecutable = CFRetain(relocData);
        aKext->loadInfo->kmodInfoAddress = kmodInfoKern;

        if (stripSymbolsFlag) {
            aKext->loadInfo->prelinkedExecutable = __OSKextCopyStrippedExecutable(aKext);
            aKext->loadInfo->loadSize = CFDataGetLength(aKext->loadInfo->prelinkedExecutable);
        }

        if (!aKext->loadInfo->prelinkedExecutable) {
            aKext->loadInfo->prelinkedExecutable = CFRetain(relocData);
        }
    }

    result = true;

finish:

   /* Advise the system that we no longer need the mmapped executable.
    */
    if (kextExecutable) {
        (void)posix_madvise((void *)CFDataGetBytePtr(kextExecutable),
            CFDataGetLength(kextExecutable),
            POSIX_MADV_DONTNEED);
    }

    SAFE_RELEASE(kextExecutable);
    SAFE_RELEASE(dependencies);
    SAFE_RELEASE(indirectDependencies);

    SAFE_RELEASE(relocData);

    SAFE_FREE(kxldDependencies);
    SAFE_FREE(bundleIDCString);

    return result;
}

/*********************************************************************
*********************************************************************/
Boolean __OSKextExtractDebugSymbols(
    OSKextRef                   aKext,
    CFMutableDictionaryRef      symbols)
{
    Boolean       result              = false;
    CFStringRef   relocName           = NULL;       // must release

    if (!OSKextDeclaresExecutable(aKext) || OSKextIsInterface(aKext)) {
        result = true;
        goto finish;
    }

    if (aKext->loadInfo->linkedExecutable) {
        relocName = CFStringCreateWithFormat(CFGetAllocator(aKext), NULL,
            CFSTR("%@.%s"),
            OSKextGetIdentifier(aKext),
            __kOSKextSymbolFileSuffix);
        if (!relocName) {
            OSKextLogMemError();
            goto finish;
        }

        CFDictionarySetValue(symbols, relocName, aKext->loadInfo->linkedExecutable);
    }

    result = true;

finish:
    SAFE_RELEASE(relocName);
    
    return result;
}

/*********************************************************************
*********************************************************************/
Boolean __OSKextGenerateDebugSymbols(
    OSKextRef                aKext,
    CFDataRef                kernelImage,
    uint64_t                 kernelLoadAddress,
    KXLDContext            * kxldContext,
    CFMutableDictionaryRef   symbols)
{
    Boolean       result              = false;
    CFArrayRef    dependencies        = NULL;       // must release
    CFIndex       count, i;


    if (!__OSKextCreateLoadInfo(aKext)) {
        OSKextLogMemError();
        goto finish;
    }

   /* Check if we already linked this one. If so, we've also already done
    * its dependencies.
    */
    if (aKext->loadInfo->linkedExecutable) {
        result = true;
        goto finish;
    }

    dependencies = OSKextCopyLinkDependencies(aKext, /* needAll */ true);
    if (!dependencies) {
        result = false;
        goto finish;
    }

    count = CFArrayGetCount(dependencies);
    for (i = 0; i < count; i++) {
        OSKextRef dependency = (OSKextRef)CFArrayGetValueAtIndex(
            dependencies, i);

        result = __OSKextGenerateDebugSymbols(dependency, kernelImage,
             kernelLoadAddress, kxldContext, symbols);
        if (!result) {
            goto finish;
        }
    }

    result = __OSKextPerformLink(aKext, kernelImage, kernelLoadAddress,
        false /* stripSymbolsFlag */, kxldContext);
    if (!result) {
        goto finish;
    }

    result = __OSKextExtractDebugSymbols(aKext, symbols);
    if (!result) {
        goto finish;
    }

    result = true;
finish:
    SAFE_RELEASE(dependencies);

    return result;
}

/*********************************************************************
* Link address callback for kxld, for symbol generation only. If a
* kext has no address set, we won't be saving its symbols, so we
* return a fake nonzero address by default.
*********************************************************************/
kxld_addr_t __OSKextLinkAddressCallback(
    u_long              size,
    KXLDAllocateFlags * flags __unused,
    void              * user_data)
{
    kxld_addr_t result      = 0;
    kxld_addr_t kextAddress = 0;
    static kxld_addr_t loadAddressOffset = 0;
    __OSKextKXLDCallbackContext * context =
        (__OSKextKXLDCallbackContext *)user_data;

    context->kext->loadInfo->loadSize = size;

    kextAddress = (kxld_addr_t)OSKextGetLoadAddress(context->kext);
    if (kextAddress) {
        result = kextAddress;
    } else {
        result = context->kernelLoadAddress + loadAddressOffset;
        loadAddressOffset += size;
    }
    return result;
}

/*********************************************************************
*********************************************************************/
void __OSKextLoggingCallback(
    KXLDLogSubsystem    subsystem, 
    KXLDLogLevel        level, 
    const char        * format, 
    va_list             argList,
    void              * user_data)
{
    __OSKextKXLDCallbackContext * context = 
        (__OSKextKXLDCallbackContext *)user_data;
    OSKextRef aKext = (context) ? context->kext : NULL;
    OSKextLogSpec logSpec = 0;

    switch (subsystem) {
    case kKxldLogLinking:
        logSpec |= kOSKextLogLinkFlag;
        break;
    case kKxldLogPatching:
        logSpec |= kOSKextLogPatchFlag;
        break;
    }

    switch (level) {
    case kKxldLogExplicit:
        logSpec |= kOSKextLogExplicitLevel;
        break;
    case kKxldLogErr:
        logSpec |= kOSKextLogErrorLevel;
        break;
    case kKxldLogWarn:
        logSpec |= kOSKextLogWarningLevel;
        break;
    case kKxldLogBasic:
        logSpec |= kOSKextLogProgressLevel;
        break;
    case kKxldLogDetail:
        logSpec |= kOSKextLogDetailLevel;
        break;
    case kKxldLogDebug:
        logSpec |= kOSKextLogDebugLevel;
        break;
    }

    OSKextVLog(aKext, logSpec, format, argList);
}

/*********************************************************************
*********************************************************************/
CFDictionaryRef OSKextGenerateDebugSymbols(
    OSKextRef aKext,
    CFDataRef kernelImage)
{
    CFMutableDictionaryRef   result             = NULL;
    CFDataRef                kernelImageCopy    = NULL;
    KXLDContext            * kxldContext        = NULL;
    KXLDFlags                kxldFlags          = 0;
    kern_return_t            kxldResult         = 0;
    uint64_t                 kernelLoadAddress  = 0;

   /* If the kernelImage is not given, then the current architecture must match
    * that of the running kernel.
    */
    if (!kernelImage) {
        if (OSKextGetArchitecture() == OSKextGetRunningKernelArchitecture()) {
            kernelImageCopy = __OSKextCopyRunningKernelImage();
            if (!kernelImageCopy || !CFDataGetLength(kernelImageCopy)) {
                OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogLinkFlag,
                    "Can't get running kernel image.");
                SAFE_RELEASE_NULL(result);
                goto finish;
            }

            kernelImage = kernelImageCopy;
        } else {
            OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogLinkFlag,
                "Can't generate debug symbols; no kernel file provided "
                "and current arch %s does not match running kernel %s.",
                OSKextGetArchitecture()->name,
                OSKextGetRunningKernelArchitecture()->name);
            goto finish;
        }
    }

    if (!OSKextResolveDependencies(aKext)) {
        goto finish;
    }

    result = CFDictionaryCreateMutable(CFGetAllocator(aKext), 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!result) {
        OSKextLogMemError();
        goto finish;
    }

    kxldResult = kxld_create_context(&kxldContext, __OSKextLinkAddressCallback,
        __OSKextLoggingCallback, kxldFlags, OSKextGetArchitecture()->cputype, 
        OSKextGetArchitecture()->cpusubtype);
    if (kxldResult != KERN_SUCCESS) {
        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogLinkFlag,
             "Can't create link context.");
        goto finish;
    }

    kernelLoadAddress = __OSKextGetFakeLoadAddress(kernelImage);
    if (!kernelLoadAddress) {
        goto finish;
    }

    if (!__OSKextGenerateDebugSymbols(aKext, kernelImage,
        kernelLoadAddress, kxldContext, result)) 
    {
        SAFE_RELEASE_NULL(result);
        goto finish;
    }

finish:
    SAFE_RELEASE(kernelImageCopy);

    if (kxldContext) kxld_destroy_context(kxldContext);

    return result;
}

/*********************************************************************
*********************************************************************/
Boolean OSKextNeedsLoadAddressForDebugSymbols(OSKextRef aKext)
{
    Boolean result = false;

    if (!OSKextIsKernelComponent(aKext) &&
        !OSKextIsInterface(aKext)       &&
        !OSKextGetLoadAddress(aKext)    &&
        OSKextDeclaresExecutable(aKext)) {

        result = true;
    }
    return result;
}
#endif /* !IOKIT_EMBEDDED */

/*********************************************************************
*********************************************************************/
OSReturn __OSKextUnload(
    OSKextRef   aKext,
    CFStringRef kextIdentifier,
    Boolean     terminateServiceAndRemovePersonalities)
{
    OSReturn               result        = kOSReturnError;
    char                 * kextIDCString = NULL;  // must free
    CFDictionaryRef        kextRequest   = NULL;  // must release
    CFMutableDictionaryRef requestArgs   = NULL;  // do not release
    char                 * termMessage   =
                           " (with termnation of IOServices)";
    char                   kextPath[PATH_MAX];

    if (aKext) {
        kextIdentifier = OSKextGetIdentifier(aKext);
        __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
            /* resolveToBase */ false, kextPath);
    } else {
        kextIDCString = createUTF8CStringForCFString(kextIdentifier);
    }

    OSKextLog(aKext,
        kOSKextLogProgressLevel |
        kOSKextLogIPCFlag | kOSKextLogLoadFlag,
        "Requesting unload of %s%s.",
        aKext ? kextPath : kextIDCString,
        terminateServiceAndRemovePersonalities ? termMessage : "");

    kextRequest = __OSKextCreateKextRequest(
        CFSTR(kKextRequestPredicateUnload),
        kextIdentifier, /* argsOut */ &requestArgs);
    if (!kextRequest || !requestArgs) {
        goto finish;
    }

    if (terminateServiceAndRemovePersonalities) {
        CFDictionarySetValue(requestArgs,
            CFSTR(kKextRequestArgumentTerminateIOServicesKey),
            kCFBooleanTrue);
    }

    result = __OSKextSendKextRequest(aKext, kextRequest,
        /* cfResponseOut */ NULL,
        /* rawResponseOut */ NULL, /* rawResponseLengthOut */ NULL);
    if (result != kOSReturnSuccess) {
        OSKextLog(aKext,
            kOSKextLogErrorLevel | kOSKextLogIPCFlag,
            "Failed to unload %s - %s.",
            aKext ? kextPath : kextIDCString,
            safe_mach_error_string(result));
        goto finish;
    } else {
        OSKextLog(aKext,
            kOSKextLogProgressLevel |
            kOSKextLogIPCFlag | kOSKextLogLoadFlag,
            "Successfully unloaded %s.",
            aKext ? kextPath : kextIDCString);
    }

finish:
    SAFE_FREE(kextIDCString);
    SAFE_RELEASE(kextRequest);
    return result;
}

/*********************************************************************
*********************************************************************/
OSReturn OSKextUnload(
    OSKextRef aKext,
    Boolean   terminateServiceAndRemovePersonalities)
{
    return __OSKextUnload(aKext, /* kextIdentifier */ NULL,
        terminateServiceAndRemovePersonalities);
}

/*********************************************************************
*********************************************************************/
OSReturn OSKextUnloadKextWithIdentifier(
    CFStringRef kextIdentifier,
    Boolean     terminateServiceAndRemovePersonalities)
{
    return __OSKextUnload(/* kext */ NULL, kextIdentifier,
        terminateServiceAndRemovePersonalities);
}

/*********************************************************************
*********************************************************************/
Boolean OSKextIsStarted(OSKextRef aKext)
{
    Boolean result = false;
    
    if (!aKext->loadInfo) {
        goto finish;
    }
    if (aKext->loadInfo->kernelLoadInfo) {
        __OSKextCheckLoaded(aKext);
    }
    if (aKext->loadInfo->flags.isStarted) {
        result = true;
    }

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
kern_return_t OSKextStart(OSKextRef aKext)
{
    OSReturn result = kOSReturnError;
    char kextPath[PATH_MAX];

    __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
        /* resolveToBase */ false, kextPath);
    // xxx - check level
    OSKextLog(aKext,
        kOSKextLogProgressLevel |
        kOSKextLogIPCFlag | kOSKextLogLoadFlag,
        "Requesting start of %s.", kextPath);

    result = __OSKextSimpleKextRequest(aKext,
        CFSTR(kKextRequestPredicateStart), /* responseOut */ NULL);

    if (result == kOSReturnSuccess) {
        OSKextLog(aKext, kOSKextLogProgressLevel |
            kOSKextLogIPCFlag | kOSKextLogLoadFlag,
            "Started %s.", kextPath);
    } else {
        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogLoadFlag,
            "Failed to start %s - %s.", kextPath,
            safe_mach_error_string(result));
    }

    return result;
}

/*********************************************************************
*********************************************************************/
kern_return_t OSKextStop(OSKextRef aKext)
{
    OSReturn result = kOSReturnError;
    char kextPath[PATH_MAX];

    __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
        /* resolveToBase */ false, kextPath);

    OSKextLog(aKext,
        kOSKextLogProgressLevel |
        kOSKextLogIPCFlag | kOSKextLogLoadFlag,
        "Requesting stop of %s.", kextPath);

    result = __OSKextSimpleKextRequest(aKext, CFSTR(kKextRequestPredicateStop),
        /* responseOut */ NULL);

    if (result == kOSReturnSuccess) {
        OSKextLog(aKext,
            kOSKextLogProgressLevel |
            kOSKextLogIPCFlag | kOSKextLogLoadFlag,
            "Successfully stopped %s.", kextPath);
    } else {
        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogLoadFlag,
            "Failed to stop %s - %s.", kextPath,
            safe_mach_error_string(result));
    }

    return result;
}

/*********************************************************************
*********************************************************************/
OSReturn
OSKextSendPersonalitiesToKernel(CFArrayRef personalities,
    Boolean resetFlag)
{
    OSReturn      result = kOSReturnError;
    CFDataRef     serializedPersonalities = NULL;  // must release
    void        * dataPtr;
    CFIndex       dataLength = 0;
    uint32_t      sendDataFlag = resetFlag ? kIOCatalogResetDrivers : kIOCatalogAddDrivers;

    if (!personalities) {
        result = kOSKextReturnInvalidArgument;
        goto finish;
    }

    if (!CFArrayGetCount(personalities)) {
        result = kOSReturnSuccess;
        goto finish;
    }

    OSKextLog(/* kext */ NULL,
        kOSKextLogStepLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
        "Sending %d personalit%s to the kernel.",
        (int)CFArrayGetCount(personalities),
        (CFArrayGetCount(personalities) != 1) ? "ies" : "y");

    serializedPersonalities = IOCFSerialize(personalities, kNilOptions);
    if (!serializedPersonalities) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogLoadFlag,
            "Can't serialize personalities.");
        result = kOSKextReturnSerialization;
        goto finish;
    }

    dataPtr = (void *)CFDataGetBytePtr(serializedPersonalities);
    dataLength = CFDataGetLength(serializedPersonalities);

    result = IOCatalogueSendData(kIOMasterPortDefault,
        sendDataFlag, dataPtr, dataLength);

    if (result != KERN_SUCCESS) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel |
            kOSKextLogLoadFlag | kOSKextLogIPCFlag,
           "Failed to send personalities to the kernel.");
       goto finish;
    }

finish:
    SAFE_RELEASE(serializedPersonalities);

    return result;
}

/*********************************************************************
*********************************************************************/
OSReturn OSKextSendKextPersonalitiesToKernel(
    OSKextRef  aKext,
    CFArrayRef personalityNames)
{
    OSReturn          result               = kOSReturnSuccess;
    CFArrayRef        personalities        = NULL;  // must release
    CFMutableArrayRef mutablePersonalities = NULL;  // do not release; alias
    char              kextPath[PATH_MAX];
    char            * nameCString          = NULL;  // must free
    CFIndex           count, i;

    __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
        /* resolveToBase */ false, kextPath);

    count = 0;
    if (personalityNames) {
        count = CFArrayGetCount(personalityNames);
    }
    if (!count) {
        personalities = OSKextCopyPersonalitiesArray(aKext);
        if (personalities && CFArrayGetCount(personalities)) {
            OSKextLog(aKext, kOSKextLogStepLevel | kOSKextLogLoadFlag,
                "Sending all personalties for %s to the kernel.",
                kextPath);
        } else {
            OSKextLog(aKext, kOSKextLogStepLevel | kOSKextLogLoadFlag,
                "%s has no personalities to send to kernel.",
                kextPath);
        }
    } else {
        CFDictionaryRef allPersonalities = OSKextGetValueForInfoDictionaryKey(
            aKext, CFSTR(kIOKitPersonalitiesKey));

        if (!allPersonalities && !CFDictionaryGetCount(allPersonalities)) {
            OSKextLog(aKext, kOSKextLogStepLevel | kOSKextLogLoadFlag,
                "%s has no personalities to send to kernel.",
                kextPath);
            goto finish;
        }
        mutablePersonalities = CFArrayCreateMutable(CFGetAllocator(aKext),
            0, &kCFTypeArrayCallBacks);
        if (!mutablePersonalities) {
            OSKextLogMemError();
            goto finish;
        }
        personalities = mutablePersonalities;

        OSKextLog(aKext,
            kOSKextLogDetailLevel |
            kOSKextLogLoadFlag | kOSKextLogIPCFlag,
            "Sending named personalities of %s to the kernel:",
            kextPath);

        for (i = 0; i < count; i++) {
            CFStringRef     name = CFArrayGetValueAtIndex(personalityNames, i);
            CFDictionaryRef personality = CFDictionaryGetValue(allPersonalities,
                name);

            SAFE_FREE_NULL(nameCString);
            nameCString = createUTF8CStringForCFString(name);
            if (!nameCString) {
                OSKextLogMemError();
                goto finish;
            }

            if (!personality) {
                OSKextLog(aKext,
                    kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                    "Personality %s not found in %s.",
                    nameCString, kextPath);
                result = kOSKextReturnInvalidArgument;
                goto finish;
            }

            OSKextLog(aKext,
                kOSKextLogDetailLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
                "    %s", nameCString);
            CFArrayAppendValue(mutablePersonalities, personality);
        }
    }

    if (personalities && CFArrayGetCount(personalities)) {
        OSReturn sendResult = OSKextSendPersonalitiesToKernel(personalities,
            /* reset? */ FALSE);
        if (sendResult != kOSReturnSuccess) {
            result = sendResult;
        }
    }
finish:
    SAFE_FREE(nameCString);
    SAFE_RELEASE(personalities);
    return result;
}

/*********************************************************************
*********************************************************************/
OSReturn OSKextSendPersonalitiesOfKextsToKernel(CFArrayRef kextArray,
    Boolean resetFlag)
{
    OSReturn      result            = kOSReturnSuccess;
    CFArrayRef    personalities     = NULL;  // must release
    CFIndex       count;

    count = CFArrayGetCount(kextArray);
    if (!count) {
        goto finish;
    }
    personalities = OSKextCopyPersonalitiesOfKexts(kextArray);
    if (!personalities || !CFArrayGetCount(personalities)) {
        // xxx - there could be an error buried in that call above...
        goto finish;
    }

    result = OSKextSendPersonalitiesToKernel(personalities, resetFlag);

finish:
    SAFE_RELEASE(personalities);
    return result;
}

/*********************************************************************
*********************************************************************/
OSReturn __OSKextRemovePersonalities(
    OSKextRef   aKext,
    CFStringRef aBundleID)
{
    OSReturn        result            = kOSReturnError;
    kern_return_t   iocatalogueResult = KERN_SUCCESS;
    CFDictionaryRef personality       = NULL;  // must release
    CFDataRef       data              = NULL;  // must release
    void          * dataPointer       = NULL;  // do not free
    CFIndex         dataLength        = 0;
    char            kextPath[PATH_MAX];

    personality = CFDictionaryCreate(CFGetAllocator(aKext),
        (const void **)&kCFBundleIdentifierKey,
        (const void **)&aBundleID, 1,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!personality) {
        OSKextLogMemError();
        goto finish;
    }

    __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
        /* resolveToBase */ FALSE, kextPath);

    data = IOCFSerialize(personality, kNilOptions);
    if (!data) {
        OSKextLog(aKext,
            kOSKextLogErrorLevel | kOSKextLogIPCFlag,
            "Can't serialize personalities for %s.",
            kextPath);
        goto finish;
    }

    dataPointer = (void *)CFDataGetBytePtr(data);
    dataLength = CFDataGetLength(data);
    iocatalogueResult = IOCatalogueSendData(kIOMasterPortDefault,
        kIOCatalogRemoveDrivers,
        dataPointer, dataLength);

    if (iocatalogueResult != KERN_SUCCESS) {
       OSKextLog(aKext,
           kOSKextLogErrorLevel | kOSKextLogIPCFlag,
           "Failed to remove personalities of %s from IOCatalogue - %s.",
           kextPath,
           safe_mach_error_string(iocatalogueResult));
       goto finish;
    }
    result = kOSReturnSuccess;
finish:
    SAFE_RELEASE(data);
    SAFE_RELEASE(personality);

    return result;
}

/*********************************************************************
*********************************************************************/
OSReturn OSKextRemoveKextPersonalitiesFromKernel(OSKextRef aKext)
{
    return __OSKextRemovePersonalities(aKext, OSKextGetIdentifier(aKext));
}


/*********************************************************************
*********************************************************************/
OSReturn OSKextRemovePersonalitiesForIdentifierFromKernel(
    CFStringRef aBundleID)
{
    return __OSKextRemovePersonalities(/* kext */ NULL, aBundleID);
}

/*********************************************************************
*********************************************************************/
void __OSKextProcessLoadInfo(
    const void * vKey __unused,
    const void * vValue,
          void * vContext __unused)
{
    CFDictionaryRef loadInfoDict  = (CFDictionaryRef)vValue;
    CFStringRef     bundleID        = NULL;  // do not release
    char          * bundleIDCString = NULL;  // must free
    CFArrayRef      matchingKexts   = NULL;  // must release
    OSKextRef       aKext           = NULL;  // do not release
    Boolean         foundOne        = FALSE;
    OSKextVersion   loadedVersion   = -1;
    CFStringRef     scratchString   = NULL;  // do not release

    char            kextPath[PATH_MAX];
    char            kextVersBuffer[kOSKextVersionMaxLength];
    char            loadedVersBuffer[kOSKextVersionMaxLength] = __kStringUnknown;
    CFIndex         count, i;

   /* See if we have any kexts for the bundle identifier.
    */
    bundleID = CFDictionaryGetValue(loadInfoDict, kCFBundleIdentifierKey);
    if (!bundleID) {
        goto finish;
    }
    matchingKexts = OSKextCopyKextsWithIdentifier(bundleID);
    if (!matchingKexts) {
        goto finish;
    }

    bundleIDCString = createUTF8CStringForCFString(bundleID);

    scratchString = CFDictionaryGetValue(loadInfoDict, kCFBundleVersionKey);
    if (scratchString) {
        loadedVersion = OSKextParseVersionCFString(scratchString);
        OSKextVersionGetString(loadedVersion,
            loadedVersBuffer, sizeof(loadedVersBuffer));
    } else {
        OSKextLog(aKext,
            kOSKextLogErrorLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
            "Kernel load info for %s lacks a CFBundleVersion.",
            bundleIDCString);
        goto finish;
    }


    count = CFArrayGetCount(matchingKexts);
    for (i = 0; i < count; i++) {
        aKext = (OSKextRef)CFArrayGetValueAtIndex(matchingKexts, i);

        if (!__OSKextCreateLoadInfo(aKext)) {
            OSKextLogMemError();
            goto finish;
        }

        __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
            /* resolveToBase */ false, kextPath);
        OSKextVersionGetString(OSKextGetVersion(aKext),
            kextVersBuffer, sizeof(kextVersBuffer));

       /* If the versions don't match, there's nothing more to check!
        */
        if (loadedVersion != aKext->version) {
            aKext->loadInfo->flags.otherCFBundleVersionIsLoaded = 1;
            continue;
        }

       /* We only retain loadInfoDict if we have to lazily figure out
        * whether this kext is loaded. If we know another version is loaded
        * there's no more to do. Note that we need to release the existing one!
        */
        SAFE_RELEASE_NULL(aKext->loadInfo->kernelLoadInfo);
        aKext->loadInfo->kernelLoadInfo = CFRetain(loadInfoDict);
        foundOne = TRUE;

        // xxx - do we want to do anything with:
        // xxx - prelinked, path, metaclasses, dependencies?
    }

finish:
    if (!foundOne && !CFEqual(bundleID, CFSTR(kOSKextKernelIdentifier))) {
        OSKextLog(aKext,
            kOSKextLogDetailLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
            "For loaded kext %s, v%s: no opened kext matches.",
            bundleIDCString, loadedVersBuffer);
    }

    SAFE_FREE(bundleIDCString);
    SAFE_RELEASE(matchingKexts);
    return;
}

/*********************************************************************
// xxx - don't want to log with lazy eval any more
*********************************************************************/
static void __OSKextCheckLoaded(OSKextRef aKext)
{
    CFDataRef     kextUUID           = NULL;  // must release
    CFDataRef     loadedUUID         = NULL;  // do not release
    uuid_string_t kextUUIDCString    = "";
    uuid_string_t loadedUUIDCString  = "";
    CFNumberRef   scratchNumber      = NULL;  // do not release
    CFBooleanRef  scratchBool        = NULL;  // do not release
    char          kextPath[PATH_MAX];
    char          kextVersBuffer[kOSKextVersionMaxLength];

    if (!aKext->loadInfo || !aKext->loadInfo->kernelLoadInfo) {
        goto finish;
    }

    __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
        /* resolveToBase? */ false, kextPath);

    /*******
    * UUID *
    ********/
    loadedUUID = CFDictionaryGetValue(aKext->loadInfo->kernelLoadInfo,
        CFSTR(kOSBundleUUIDKey));
    kextUUID = OSKextCopyUUIDForArchitecture(aKext,
        OSKextGetRunningKernelArchitecture());

    if (loadedUUID) {
        uuid_unparse(CFDataGetBytePtr(loadedUUID), loadedUUIDCString);
    }
    if (kextUUID) {
        uuid_unparse(CFDataGetBytePtr(kextUUID), kextUUIDCString);
    }
    OSKextVersionGetString(OSKextGetVersion(aKext), kextVersBuffer,
        sizeof(kextVersBuffer));

    if ( (loadedUUID && kextUUID && CFEqual(loadedUUID, kextUUID)) ||
         (!loadedUUID && !kextUUID) ) {

        aKext->loadInfo->flags.isLoaded = 1;
        
        /* Will need to find a decent way to log this & following
         * when we do the logging cleanup after the seed.
         */
        OSKextLog(aKext,
            // xxx - check level
            kOSKextLogDebugLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
            "%s (version %s%s%s) is loaded.",
            kextPath, // xxx - better to use bundle ID?
            kextVersBuffer,
            kextUUIDCString[0] ? ", UUID " : ", no UUID",
            kextUUIDCString);
    } else {
        aKext->loadInfo->flags.otherUUIDIsLoaded = 1;

        OSKextLog(aKext,
            kOSKextLogDebugLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
            "%s (version %s%s%s): same version, different UUID (%s) is loaded.",
            kextPath, // xxx - better to use bundle ID?
            kextVersBuffer,
            kextUUIDCString[0] ? ", UUID " : ", no UUID",
            kextUUIDCString,
            loadedUUIDCString[0] ? loadedUUIDCString : "none");

        goto finish;
    }

    /**********
    * Started *
    ***********/
    scratchBool = CFDictionaryGetValue(aKext->loadInfo->kernelLoadInfo,
        CFSTR(kOSBundleStartedKey));
    if (scratchBool && CFBooleanGetValue(scratchBool)) {
        aKext->loadInfo->flags.isStarted = 1;

        OSKextLog(aKext,
            kOSKextLogDebugLevel | kOSKextLogLoadFlag  | kOSKextLogIPCFlag,
            "%s (version %s): is%s started.",
            kextPath, // xxx - better to use bundle ID?
            kextVersBuffer,
            aKext->loadInfo->flags.isStarted ? "" : " not");

    }

    /***********
    * load tag *
    ************/
    scratchNumber = CFDictionaryGetValue(aKext->loadInfo->kernelLoadInfo,
        CFSTR(kOSBundleLoadTagKey));
    if (scratchNumber) {
        uint32_t loadTag;
        if (CFNumberGetValue(scratchNumber, kCFNumberSInt32Type, &loadTag)) {
            aKext->loadInfo->loadTag = loadTag;
        } else {
            // xxx - log an error?
        }
    }

    /***************
    * load address *
    ****************/
    scratchNumber = CFDictionaryGetValue(aKext->loadInfo->kernelLoadInfo,
        CFSTR(kOSBundleLoadAddressKey));
    if (scratchNumber) {
        uint64_t loadAddress;
        if (CFNumberGetValue(scratchNumber, kCFNumberSInt64Type,
            &loadAddress)) {

           /* Call the internal SetLoadAddress function so we don't call
            * right back into this function making an infinite recursion.
            */
            __OSKextSetLoadAddress(aKext, loadAddress);
        } else {
            // xxx - log an error?
        }
    }

    /**************
    * header size *
    ***************/
    scratchNumber = CFDictionaryGetValue(aKext->loadInfo->kernelLoadInfo,
        CFSTR(kOSBundleLoadSizeKey));
    if (scratchNumber) {
        uint32_t loadSize;
        if (CFNumberGetValue(scratchNumber, kCFNumberSInt32Type,
            &loadSize)) {

            aKext->loadInfo->loadSize = loadSize;
        } else {
            // xxx - log an error?
        }
    }
    
finish:

    if (aKext->loadInfo) {
        SAFE_RELEASE_NULL(aKext->loadInfo->kernelLoadInfo);
    }
    SAFE_RELEASE(kextUUID);
    return;
}

/*********************************************************************
*********************************************************************/
OSReturn OSKextReadLoadedKextInfo(
    CFArrayRef kextIdentifiers,
    Boolean    flushDependenciesFlag)
{
    OSReturn           result      = kOSReturnError;
    CFArrayRef         kexts       = NULL;  // must release
    CFDictionaryRef    allLoadInfo = NULL;  // must release
    const NXArchInfo * currentArch = NULL;  // do not free
    const NXArchInfo * kernelArch  = NULL;  // do not free
    CFIndex            count, i;

    pthread_once(&__sOSKextInitialized, __OSKextInitialize);

    currentArch = OSKextGetArchitecture();
    kernelArch = OSKextGetRunningKernelArchitecture();

    if (currentArch != kernelArch) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
            "Can't read loaded kext info - current architecture %s != kernel's architecture %s.",
            currentArch->name, kernelArch->name);
        result = kOSKextReturnArchNotFound;
        goto finish;
    }

   /* Dump all load info, including resolved dependencies as requested.
    */
    if (!kextIdentifiers) {
        OSKextFlushLoadInfo(/* kext */ NULL, flushDependenciesFlag);
    } else {
        kexts = OSKextCopyKextsWithIdentifiers(kextIdentifiers);
        if (!kexts) {
            // none to update.
            goto finish;
        }
        count = CFArrayGetCount(kexts);
        for (i = 0; i < count; i++) {
            OSKextRef thisKext = (OSKextRef) CFArrayGetValueAtIndex(kexts, i);
            OSKextFlushLoadInfo(thisKext, flushDependenciesFlag);
        }
    }

    if (kextIdentifiers && CFArrayGetCount(kextIdentifiers)) {
        CFIndex numIdentifiers = CFArrayGetCount(kextIdentifiers);
        OSKextLog(/* kext */ NULL,
            // xxx - check level
            kOSKextLogProgressLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
            "Reading load info for %u kext%s.",
            (uint32_t)numIdentifiers,
            numIdentifiers == 1 ? "" : "s");
    } else {
        OSKextLog(/* kext */ NULL,
            // xxx - check level
            kOSKextLogProgressLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
            "Reading load info for all kexts.");
    }

    // xxx - need to specify just the props OSKext objects actually use
    allLoadInfo = OSKextCopyLoadedKextInfo(kextIdentifiers, __sOSKextInfoEssentialKeys);
    if (!allLoadInfo) {
        // xxx - this is usually an error, though not always!
        // xxx - we really lack a way to propagate it
        goto finish;
    }
    
    CFDictionaryApplyFunction(allLoadInfo, __OSKextProcessLoadInfo,
        /* context */ NULL);

    result = kOSReturnSuccess;

finish:
    SAFE_RELEASE(kexts);
    SAFE_RELEASE(allLoadInfo);

    return result;
}

/*********************************************************************
*********************************************************************/
Boolean OSKextIsLoaded(OSKextRef aKext)
{
    Boolean result = false;

    if (!aKext->loadInfo) {
        goto finish;
    }
    if (aKext->loadInfo->kernelLoadInfo) {
        __OSKextCheckLoaded(aKext);
    }
    if (aKext->loadInfo->flags.isLoaded) {
        result = true;
    }

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
uint64_t OSKextGetLoadAddress(OSKextRef aKext)
{
    uint64_t result = 0x0;

    if (!aKext->loadInfo) {
        goto finish;
    }
    if (aKext->loadInfo->kernelLoadInfo) {
        __OSKextCheckLoaded(aKext);
    }
    result = aKext->loadInfo->loadAddress;
    
finish:
    return result;
}

/*******************************************************************************
* Internal set-load-address function to avoid infinite recursion from processing
* load info.
*******************************************************************************/
Boolean __OSKextSetLoadAddress(OSKextRef aKext, uint64_t address)
{
    Boolean result = false;
    char    kextPath[PATH_MAX];

    if (!__OSKextCreateLoadInfo(aKext)) {
        goto finish;
    }
    
    __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
        /* resolveToBase */ false, kextPath);

    if (!__OSKextIsArchitectureLP64() && address > UINT32_MAX) {

        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogLoadFlag,
            "Attempt to set 64-bit load address - %s.",
            kextPath);
        goto finish;
    }

    if (__OSKextIsArchitectureLP64()) {
        OSKextLog(aKext, kOSKextLogDebugLevel | kOSKextLogLinkFlag | kOSKextLogLoadFlag,
            "setting load address of %s to 0x%0llx",
            kextPath, (unsigned long long)address);
    } else {
        OSKextLog(aKext, kOSKextLogDebugLevel | kOSKextLogLinkFlag | kOSKextLogLoadFlag,
            "setting load address of %s to 0x%0x",
            kextPath, (int)address);
    }

    aKext->loadInfo->loadAddress = address;
    result = true;
finish:
    return result;
}

/*******************************************************************************
*******************************************************************************/
Boolean OSKextSetLoadAddress(OSKextRef aKext, uint64_t address)
{
    Boolean result = false;

    if (!__OSKextCreateLoadInfo(aKext)) {
        goto finish;
    }
    
   /* Process any pending load info for the kext before overwriting
    * the load address.
    */
    if (aKext->loadInfo->kernelLoadInfo) {
        __OSKextCheckLoaded(aKext);
    }

    result = __OSKextSetLoadAddress(aKext, address);

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
Boolean OSKextOtherVersionIsLoaded(OSKextRef aKext, Boolean * uuidFlag)
{
    Boolean result = false;
    
    if (!aKext->loadInfo) {
        goto finish;
    }
    if (aKext->loadInfo->kernelLoadInfo) {
        __OSKextCheckLoaded(aKext);
    }
    if (aKext->loadInfo->flags.otherCFBundleVersionIsLoaded ||
        aKext->loadInfo->flags.otherUUIDIsLoaded) {

        result = true;
    }
    if (uuidFlag) {
        *uuidFlag = aKext->loadInfo->flags.otherUUIDIsLoaded ? true : false;
    }

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
uint32_t OSKextGetLoadTag(OSKextRef aKext)
{
    uint32_t result = 0;

    if (!aKext->loadInfo) {
        goto finish;
    }
    if (aKext->loadInfo->kernelLoadInfo) {
        __OSKextCheckLoaded(aKext);
    }
    result = aKext->loadInfo->loadTag;

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
static void __OSKextFlushLoadInfoApplierFunction(
    const void * vKey __unused,
    const void * vValue,
          void * vContext)
{
    OSKextRef aKext = (OSKextRef)vValue;
    Boolean   flushDependenciesFlag = *(Boolean *)vContext;

    OSKextFlushLoadInfo(aKext, flushDependenciesFlag);
    return;
}

void OSKextFlushLoadInfo(
    OSKextRef aKext,
    Boolean   flushDependenciesFlag)
{
    static Boolean flushingAll = false;
    char           kextPath[PATH_MAX];

    pthread_once(&__sOSKextInitialized, __OSKextInitialize);

    if (aKext) {
        if (OSKextGetURL(aKext)) {
            __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
                /* resolveToBase */ false, kextPath);
        }
        if (aKext->loadInfo) {

            if (!flushingAll) {
                OSKextLog(aKext,
                    kOSKextLogStepLevel | kOSKextLogKextBookkeepingFlag,
                    "Flushing load info for %s (%s dependencies)",
                    kextPath,
                    flushDependenciesFlag ? "with" : "keeping");
            }

            SAFE_RELEASE_NULL(aKext->loadInfo->kernelLoadInfo);
            SAFE_RELEASE_NULL(aKext->loadInfo->executableURL);
            SAFE_RELEASE_NULL(aKext->loadInfo->executable);
            SAFE_RELEASE_NULL(aKext->loadInfo->linkedExecutable);
            SAFE_RELEASE_NULL(aKext->loadInfo->prelinkedExecutable);
            if (flushDependenciesFlag) {
                OSKextFlushDependencies(aKext);
            }
            SAFE_FREE_NULL(aKext->loadInfo);

           /* The executable could change by the time we read it again,
            * so clear all validation/authentication flags. Leave
            * diagnostics in place (a bit funky I suppose).
            */
            aKext->flags.valid = 0;
            aKext->flags.invalid = 0;
            aKext->flags.validated = 0;
            aKext->flags.authentic = 0;
            aKext->flags.inauthentic = 0;
            aKext->flags.authenticated = 0;
        }
    } else if (__sOSKextsByURL) {
        flushingAll = true;
        OSKextLog(/* kext */ NULL,
            kOSKextLogStepLevel | kOSKextLogKextBookkeepingFlag,
            "Flushing load info for all kexts (%s dependencies)",
            flushDependenciesFlag ? "with" : "keeping");

        CFDictionaryApplyFunction(__sOSKextsByURL,
            __OSKextFlushLoadInfoApplierFunction,
            /* context */ &flushDependenciesFlag);
        flushingAll = false;
    }
    return;
}

/*********************************************************************
*********************************************************************/
CFArrayRef _OSKextCopyKernelRequests(void)
{
    CFArrayRef             result        = NULL;
    OSReturn               op_result     = kOSReturnError;
    CFMutableDictionaryRef requestDict   = NULL;  // must release

    OSKextLog(/* kext */ NULL,
        kOSKextLogDebugLevel | kOSKextLogIPCFlag,
        "Reading requests from kernel.");

    requestDict = __OSKextCreateKextRequest(
        CFSTR(kKextRequestPredicateGetKernelRequests),
        /* bundleID */ NULL, /* argsOut */ NULL);

    op_result = __OSKextSendKextRequest(/* kext */ NULL, requestDict,
        (CFTypeRef *)&result,
        /* rawResponseOut */ NULL, /* rawResponseLengthOut */ NULL);
    if (op_result != kOSReturnSuccess) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogIPCFlag,
            "Failed to read requests from kernel - %s.",
            safe_mach_error_string(op_result));
        SAFE_RELEASE_NULL(result);
        goto finish;
    }

    if (!result || CFArrayGetTypeID() != CFGetTypeID(result)) {
        SAFE_RELEASE_NULL(result);
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogIPCFlag,
            "Requests from kernel missing or of wrong type.");
        goto finish;
    }

finish:
    SAFE_RELEASE(requestDict);
    return result;
}

/*********************************************************************
*********************************************************************/
OSReturn _OSKextSendResource(
    CFDictionaryRef request,
    OSReturn        requestResult,
    CFDataRef       resource)
{
    OSReturn               result           = kOSReturnError;
    CFDictionaryRef        requestArgs      = NULL;  // do not release
    CFMutableDictionaryRef response         = NULL;  // must release
    CFMutableDictionaryRef responseArgs     = NULL;  // must release
    CFNumberRef            requestResultNum = NULL;  // must release
    
    requestArgs = CFDictionaryGetValue(request,
        CFSTR(kKextRequestArgumentsKey));
    if (!requestArgs) {
        result = kOSKextReturnInvalidArgument;
        goto finish;
    }
    
    response = CFDictionaryCreateMutableCopy(kCFAllocatorDefault,
        0, request);
    if (!response) {
        OSKextLogMemError();
        goto finish;
    }
    
    responseArgs = CFDictionaryCreateMutableCopy(kCFAllocatorDefault,
        0, requestArgs);
    if (!responseArgs) {
        OSKextLogMemError();
        goto finish;
    }
    
    CFDictionarySetValue(response, CFSTR(kKextRequestPredicateKey),
        CFSTR(kKextRequestPredicateSendResource));
    CFDictionarySetValue(response, CFSTR(kKextRequestArgumentsKey),
        responseArgs);
        
    if (resource) {
        CFDictionarySetValue(responseArgs, CFSTR(kKextRequestArgumentValueKey),
            resource);
    }
    
    requestResultNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type,
        (SInt32 *)&requestResult);

   /* Let's not treat this as fatal, we'd like to get the waiting callback
    * cleared in the kernel and it can just send an error.
    */
    if (requestResultNum) {
        CFDictionarySetValue(responseArgs, CFSTR(kKextRequestArgumentResultKey),
            requestResultNum);
    }

    result = __OSKextSendKextRequest(/* kext */ NULL, response,
        /* cfResponseOut */ NULL,
        /* rawResponseOut */ NULL, /* rawResponseLengthOut */ NULL);

finish:
    SAFE_RELEASE(requestResultNum);
    SAFE_RELEASE(responseArgs);
    SAFE_RELEASE(response);
    return result;
}

/*********************************************************************
*********************************************************************/
CFArrayRef OSKextCreateLoadedKextInfo(
    CFArrayRef kextIdentifiers)
{
    CFArrayRef        result          = NULL;
    CFDictionaryRef   allLoadInfoDict = NULL;  // must release
    CFDictionaryRef * loadInfoItems   = NULL;  // must free
    CFIndex           count;
    
    allLoadInfoDict = OSKextCopyLoadedKextInfo(kextIdentifiers, NULL /* all info */);
    if (!allLoadInfoDict || CFDictionaryGetTypeID() != CFGetTypeID(allLoadInfoDict)) {
        goto finish;
    }

    count = CFDictionaryGetCount(allLoadInfoDict);
    loadInfoItems = malloc(count * sizeof(CFDictionaryRef));;
    if (!loadInfoItems) {
        OSKextLogMemError();
        goto finish;
    }
    CFDictionaryGetKeysAndValues(allLoadInfoDict, /* keys */ NULL,
        (const void **)loadInfoItems);
    
    result = CFArrayCreate(kCFAllocatorDefault,
        (const void **)loadInfoItems, count, &kCFTypeArrayCallBacks);

finish:
    SAFE_RELEASE(allLoadInfoDict);
    return result;
}

/*********************************************************************
*********************************************************************/
CFDictionaryRef OSKextCopyLoadedKextInfo(
    CFArrayRef kextIdentifiers,
    CFArrayRef infoKeys)
{
    CFDictionaryRef        result        = NULL;
    OSReturn               op_result     = kOSReturnError;
    CFMutableDictionaryRef requestDict   = NULL;  // must release
    CFMutableDictionaryRef requestArgs   = NULL;  // do not release
    CFStringRef            infoString    = NULL;  // must release
    char                 * infoCString   = NULL;  // must free

    OSKextLog(/* kext */ NULL,
        kOSKextLogStepLevel | kOSKextLogIPCFlag,
        "Reading loaded kext info from kernel.");

    requestDict = __OSKextCreateKextRequest(CFSTR(kKextRequestPredicateGetLoaded),
        kextIdentifiers, &requestArgs);
        
    if (infoKeys && CFArrayGetCount(infoKeys)) {
        CFDictionarySetValue(requestArgs,
            CFSTR(kKextRequestArgumentInfoKeysKey),
            infoKeys);
    }

    op_result = __OSKextSendKextRequest(/* kext */ NULL, requestDict,
        (CFTypeRef *)&result,
        /* rawResponseOut */ NULL, /* rawResponseLengthOut */ NULL);
    if (op_result != kOSReturnSuccess) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogIPCFlag,
            "Failed to read loaded kext info from kernel - %s.",
            safe_mach_error_string(op_result));
        SAFE_RELEASE_NULL(result);
        goto finish;
    }

    if (!result) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogIPCFlag,
            "Kernel request call returned no data.");
        goto finish;
    }
    if (CFDictionaryGetTypeID() != CFGetTypeID(result)) {
        SAFE_RELEASE_NULL(result);
        // xxx - these flags don't seem quite right
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogIPCFlag,
            "Loaded kext info from kernel is wrong type.");
        goto finish;
    }

    if (__OSKextShouldLog(/* kext */ NULL,
        kOSKextLogDebugLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag)) {

        infoString = createCFStringForPlist_new(result,
            kPListStyleClassic);
        infoCString = createUTF8CStringForCFString(infoString);
        OSKextLog(/* kext */ NULL,
            kOSKextLogDebugLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
            "Loaded kext info:\n%s", infoCString);
    }

finish:
    SAFE_RELEASE(requestDict);
    SAFE_RELEASE(infoString);
    SAFE_FREE(infoCString);

    return result;
}

/*********************************************************************
*********************************************************************/
Boolean __OSKextCreateLoadInfo(OSKextRef aKext)
{
    Boolean result = false;

// xxx - log under kOSKextLogDetailLevel | kOSKextLogKextBookkeepingFlag ?

    if (!aKext->loadInfo) {
        aKext->loadInfo = (__OSKextLoadInfo *)
            malloc(sizeof(*(aKext->loadInfo)));
        if (!aKext->loadInfo) {
            OSKextLogMemError();
            goto finish;
        }
        memset(aKext->loadInfo, 0, sizeof(*(aKext->loadInfo)));
    }
    result = true;
finish:
    return result;
}

/*********************************************************************
*********************************************************************/
Boolean __OSKextCreateMkextInfo(OSKextRef aKext)
{
    Boolean result = false;

    if (!aKext->mkextInfo) {
        aKext->mkextInfo = (__OSKextMkextInfo *)
            malloc(sizeof(*(aKext->mkextInfo)));
        if (!aKext->mkextInfo) {
            OSKextLogMemError();
            goto finish;
        }
        memset(aKext->mkextInfo, 0, sizeof(*(aKext->mkextInfo)));
    }
    result = true;
finish:
    return result;
}

#ifndef IOKIT_EMBEDDED
/*********************************************************************
*********************************************************************/
CFDataRef __OSKextCopyRunningKernelImage(void)
{
    CFDataRef              result          = NULL;
    CFMutableDictionaryRef kextRequest     = NULL;    // must release
    OSReturn               osresult        = kOSReturnError;

    char                 * linkStateBytes  = NULL;  // must vm_deallocate
    uint32_t               linkStateLength = 0;

    kextRequest = __OSKextCreateKextRequest(
        CFSTR(kKextRequestPredicateGetKernelImage),
        /* identifier */ NULL, /* argsOut* */ NULL);
    if (!kextRequest) {
        OSKextLogMemError();
        goto finish;
    }
    osresult = __OSKextSendKextRequest(/* kext */ NULL, kextRequest,
        /* cfResponseOut */ NULL,
        &linkStateBytes, &linkStateLength);
    if (osresult != kOSReturnSuccess) {
        goto finish;
    }

    result = CFDataCreate(kCFAllocatorDefault,
        (u_char *)linkStateBytes, linkStateLength);

finish:
    SAFE_RELEASE(kextRequest);

    if (linkStateBytes && linkStateLength) {
        vm_deallocate(mach_task_self(), (vm_address_t)linkStateBytes,
            linkStateLength);
    }
    return result;
}
#endif /* !IOKIT_EMBEDDED */

#pragma mark Sanity Checking and Diagnostics
/*********************************************************************
* Sanity Checking and Diagnostics
*********************************************************************/

/*********************************************************************
*********************************************************************/
typedef struct {
    OSKextRef          kext;
    CFDictionaryRef    libraries;
    CFMutableArrayRef  propPath;
    Boolean            valid;
    Boolean            hasKernelStyleDependency;
    Boolean            hasKPIStyleDependency;
} __OSKextValidateOSBundleLibraryContext;


static void __OSKextValidateOSBundleLibraryApplierFunction(
    const void * vKey,
    const void * vValue,
          void * vContext)
{
    CFStringRef libID = (CFStringRef)vKey;
    CFStringRef libVersion = (CFStringRef)vValue;
    __OSKextValidateOSBundleLibraryContext * context =
        (__OSKextValidateOSBundleLibraryContext *)vContext;

    OSKextVersion version = -1;

    CFArrayAppendValue(context->propPath, libID);

    if (!__OSKextCheckProperty(context->kext,
        context->libraries,
        /* propKey */ libID,
        /* diagnosticValue */ context->propPath,
        /* expectedType */ CFStringGetTypeID(),
        /* legalValues */ NULL,
        /* required */ false,
        /* typeRequired */ true,
        /* nonnilRequired */ true,
        /* valueOut */ NULL,
        /* valueIsNonnil */ NULL)) {

        context->valid = false;
        goto finish;
    } else {
        version = OSKextParseVersionCFString(libVersion);
        if (version == -1) {
            __OSKextAddDiagnostic(context->kext, kOSKextDiagnosticsFlagValidation,
                kOSKextDiagnosticPropertyIsIllegalValueKey, context->propPath,
                /* note */ NULL);
            context->valid = false;
        }
    }

   /* If the library is any com.apple.kernel* (note inclusion of kernel itself),
    * and the version is too old, we make a note to check for mismatched
    * kmod_info fields when we check the executable.
    */

    // xxx - 64bit kexts won't even have this, but we can fail in dependency
    // xxx - resolution I guess!

    if (CFStringHasPrefix(libID, __kOSKextKernelLibBundleID)) {
        context->hasKernelStyleDependency = true;
        if (version < __sOSNewKmodInfoKernelVersion) {
            context->kext->flags.warnForMismatchedKmodInfo = 1;
        }
    } else if (CFStringHasPrefix(libID, __kOSKextKPIPrefix)) {
        context->hasKPIStyleDependency = true;
        if (version < __sOSNewKmodInfoKernelVersion) {
            context->kext->flags.warnForMismatchedKmodInfo = 1;
        }
    }


finish:
    CFArrayRemoveValueAtIndex(context->propPath,
        CFArrayGetCount(context->propPath) - 1);
    return;
}

/*********************************************************************
*********************************************************************/
typedef struct {
    OSKextRef         kext;
    CFDictionaryRef   personalities;
    CFMutableArrayRef propPath;
    Boolean           valid;
    Boolean           justCheckingIOKitDebug;
} __OSKextValidateIOKitPersonalityContext;

static void __OSKextValidateIOKitPersonalityApplierFunction(
    const void * vKey,
    const void * vValue,
          void * vContext)
{
    CFStringRef     personalityName      = (CFStringRef)vKey;
    CFDictionaryRef personality          = (CFDictionaryRef)vValue;
    __OSKextValidateIOKitPersonalityContext * context =
        (__OSKextValidateIOKitPersonalityContext *)vContext;
    Boolean         checkResult          = false;
    CFStringRef     propKey              = NULL;  // do not release
    Boolean         valueIsNonnil;
    CFStringRef     ioclassProp          = NULL;  // do not release
    CFStringRef     stringValue          = NULL;  // do not release
    Boolean         checkIOMatchCategory = false;
    OSKextRef       personalityKext      = NULL;  // do not release
    CFStringRef     diagnosticString     = NULL;  // must release

    CFArrayAppendValue(context->propPath, personalityName);

    if (!__OSKextCheckProperty(context->kext,
        context->personalities,
        /* propKey */ personalityName,
        /* diagnosticValue */ context->propPath,
        /* expectedType */ CFDictionaryGetTypeID(),
        /* legalValues */ NULL,
        /* required */ false,
        /* typeRequired */ true,
        /* nonnilRequired */ false,
        /* valueOut */ NULL,
        /* valueIsNonnil */ NULL)) {

        context->valid = false;
        goto finish;
    }

   /**********************
    * IOKitDebug: number *
    **********************/

    // xxx - used to disable safe boot loadbility, not worth doing for now
    propKey = CFSTR(kIOKitDebugKey);
    CFArrayAppendValue(context->propPath, propKey);

    checkResult = __OSKextCheckProperty(context->kext,
        personality,
        /* propKey */ propKey,
        /* diagnosticValue */ context->propPath,
        /* expectedType */ CFNumberGetTypeID(),
        /* legalValues */ NULL,
        /* required */ false,
        /* typeRequired */ true,
        /* nonnilRequired */ false,
        /* valueOut */ NULL,
        /* valueIsNonnil */ &valueIsNonnil);
    context->valid = context->valid && checkResult;
    if (checkResult && valueIsNonnil) {
        context->kext->flags.plistHasIOKitDebugFlags = 1;
    }

    CFArrayRemoveValueAtIndex(context->propPath,
        CFArrayGetCount(context->propPath) - 1);

   /* A bit of a hack, but why duplicate that code for one check?
    */
    if (context->justCheckingIOKitDebug) {
        goto finish;
    }

   /******************************
    * CFBundleIdentifier: string *
    ******************************/

    propKey = kCFBundleIdentifierKey;
    CFArrayAppendValue(context->propPath, propKey);

    checkResult = __OSKextCheckProperty(context->kext,
        personality,
        /* propKey */ propKey,
        /* diagnosticValue */ context->propPath,
        /* expectedType */ CFStringGetTypeID(),
        /* legalValues */ NULL,
        /* required */ false,
        /* typeRequired */ true,
        /* nonnilRequired */ true,   // xxx - allow empty string here?
        /* valueOut */ (CFTypeRef *)&stringValue,
        /* valueIsNonnil */ &valueIsNonnil);
    context->valid = context->valid && checkResult;
    if (!stringValue) {
        // xxx - this is really more of a notice than a warning
        __OSKextAddDiagnostic(context->kext, kOSKextDiagnosticsFlagWarnings,
            kOSKextDiagnosticPersonalityHasNoBundleIdentifierKey,
            personalityName, /* note */ NULL);
    } else if (!CFEqual(stringValue, OSKextGetIdentifier(context->kext))) {
        // xxx - this is really more of a notice than a warning
        diagnosticString = CFStringCreateWithFormat(kCFAllocatorDefault,
            /* formatOptions */ NULL, CFSTR("%@ -> %@ (kext is %@)"),
            personalityName, stringValue, context->kext);
        __OSKextAddDiagnostic(context->kext, kOSKextDiagnosticsFlagWarnings,
            kOSKextDiagnosticPersonalityHasDifferentBundleIdentifierKey,
            personalityName, /* note */ NULL);
        SAFE_RELEASE_NULL(diagnosticString);
    }

   /* Check for this condition independent of the other warnings above.
    */
    if (stringValue) {
        personalityKext = OSKextGetKextWithIdentifier(stringValue);
        if (!personalityKext) {
            diagnosticString = CFStringCreateWithFormat(kCFAllocatorDefault,
                /* formatOptions */ NULL, CFSTR("'%@' -> '%@'"),
                personalityName, stringValue);
            __OSKextAddDiagnostic(context->kext, kOSKextDiagnosticsFlagWarnings,
                kOSKextDiagnosticPersonalityNamesUnknownKextKey,
                diagnosticString, /* note */ NULL);
            SAFE_RELEASE_NULL(diagnosticString);
        } else {
            if (!OSKextDeclaresExecutable(personalityKext)) {
                diagnosticString = CFStringCreateWithFormat(kCFAllocatorDefault,
                    /* formatOptions */ NULL, CFSTR("'%@' -> '%@'"),
                    personalityName, stringValue);
                __OSKextAddDiagnostic(context->kext, kOSKextDiagnosticsFlagWarnings,
                    kOSKextDiagnosticPersonalityNamesKextWithNoExecutableKey,
                    diagnosticString, /* note */ NULL);
                SAFE_RELEASE_NULL(diagnosticString);
            }
            // xxx - moving check for personality target loadable
        }
    }

    CFArrayRemoveValueAtIndex(context->propPath,
        CFArrayGetCount(context->propPath) - 1);

   /*******************
    * IOClass: string *
    *******************/

    // xxx - would like to check that class is defined in executable named
    // xxx - by bundle ID of personality

    propKey = CFSTR(kIOClassKey);
    CFArrayAppendValue(context->propPath, propKey);

    checkResult = __OSKextCheckProperty(context->kext,
        personality,
        /* propKey */ propKey,
        /* diagnosticValue */ context->propPath,
        /* expectedType */ CFStringGetTypeID(),
        /* legalValues */ NULL,
        /* required */ true,
        /* typeRequired */ true,
        /* nonnilRequired */ true,
        /* valueOut */ (CFTypeRef *)&ioclassProp,  // we use this later!
        /* valueIsNonnil */ NULL);
    context->valid = context->valid && checkResult;

    CFArrayRemoveValueAtIndex(context->propPath,
        CFArrayGetCount(context->propPath) - 1);

   /****************************************************************
    * IOProviderClass: string                                      *
    * We can't do anything to confirm existence of provider class, *
    * since it could come from the kernel or any other kext.       *
    ****************************************************************/

    propKey = CFSTR(kIOProviderClassKey);
    CFArrayAppendValue(context->propPath, propKey);

    checkResult = __OSKextCheckProperty(context->kext,
        personality,
        /* propKey */ propKey,
        /* diagnosticValue */ context->propPath,
        /* expectedType */ CFStringGetTypeID(),
        /* legalValues */ NULL,
        /* required */ true,
        /* typeRequired */ true,
        /* nonnilRequired */ true,
        /* valueOut */ (CFTypeRef *)&stringValue,
        /* valueIsNonnil */ NULL);
    context->valid = context->valid && checkResult;
    if (checkResult && CFEqual(stringValue, CFSTR(kIOResourcesClass))) {
        checkIOMatchCategory = true;
    }

    CFArrayRemoveValueAtIndex(context->propPath,
        CFArrayGetCount(context->propPath) - 1);

   /***************************
    * IOMatchCategory: string *
    ***************************/

    // xxx - is this used for other than with IOResources match?

    if (checkIOMatchCategory) {

        propKey = CFSTR(kIOMatchCategoryKey);
        CFArrayAppendValue(context->propPath, propKey);

        checkResult = __OSKextCheckProperty(context->kext,
            personality,
            /* propKey */ propKey,
            /* diagnosticValue */ context->propPath,
            /* expectedType */ CFStringGetTypeID(),
            /* legalValues */ NULL,
            /* required */ false,
            /* typeRequired */ true,
            /* nonnilRequired */ false,  // xxx - hm...
            /* valueOut */ (CFTypeRef *)&stringValue,
            /* valueIsNonnil */ NULL);
        context->valid = context->valid && checkResult;
        if (checkResult && stringValue) {
            if (ioclassProp && !CFEqual(ioclassProp, stringValue)) {
                __OSKextAddDiagnostic(context->kext,
                    kOSKextDiagnosticsFlagWarnings,
                    kOSKextDiagnosticNonuniqueIOResourcesMatchKey,
                    personalityName, /* note */ NULL);
            }
        }

        CFArrayRemoveValueAtIndex(context->propPath,
            CFArrayGetCount(context->propPath) - 1);
    }

   /**************************************************************************
    * IOProbeScore: number (warning only)                                    *
    * We can't make this a hard error because it might break shipping kexts. *
    **************************************************************************/

    propKey = CFSTR(kIOProbeScoreKey);
    CFArrayAppendValue(context->propPath, propKey);

    checkResult = __OSKextCheckProperty(context->kext,
        personality,
        /* propKey */ propKey,
        /* diagnosticValue */ context->propPath,
        /* expectedType */ CFNumberGetTypeID(),
        /* legalValues */ NULL,
        /* required */ false,
        /* typeRequired */ false,
        /* nonnilRequired */ false,
        /* valueOut */ NULL,
        /* valueIsNonnil */ NULL);

    // __OSKextCheckProperty() sets a warning; we don't care about checkResult

    CFArrayRemoveValueAtIndex(context->propPath,
        CFArrayGetCount(context->propPath) - 1);

   /*********************
    * end of properties *
    *********************/
finish:

   /* Remove the personality name from the prop path.
    */
    CFArrayRemoveValueAtIndex(context->propPath,
        CFArrayGetCount(context->propPath) - 1);

    SAFE_RELEASE(diagnosticString);

    return;
}

static void __OSKextValidateIOKitPersonalityTargetApplierFunction(
    const void * vKey,
    const void * vValue,
          void * vContext)
{
    CFStringRef     personalityName      = (CFStringRef)vKey;
    CFDictionaryRef personality            = (CFDictionaryRef)vValue;
    __OSKextValidateIOKitPersonalityContext * context =
        (__OSKextValidateIOKitPersonalityContext *)vContext;
    Boolean         checkResult            = false;
    CFStringRef     propKey                = NULL;  // do not release
    Boolean         valueIsNonnil;
    CFStringRef     stringValue            = NULL;  // do not release
    OSKextRef       personalityKext        = NULL;  // do not release
    CFStringRef     diagnosticString       = NULL;  // must release

    CFArrayAppendValue(context->propPath, personalityName);

    if (!__OSKextCheckProperty(context->kext,
        context->personalities,
        /* propKey */ personalityName,
        /* diagnosticValue */ context->propPath,
        /* expectedType */ CFDictionaryGetTypeID(),
        /* legalValues */ NULL,
        /* required */ false,
        /* typeRequired */ true,
        /* nonnilRequired */ false,
        /* valueOut */ NULL,
        /* valueIsNonnil */ NULL)) {

        context->valid = false;
        goto finish;
    }

   /******************************
    * CFBundleIdentifier: string *
    ******************************/

    propKey = kCFBundleIdentifierKey;
    CFArrayAppendValue(context->propPath, propKey);

    checkResult = __OSKextCheckProperty(context->kext,
        personality,
        /* propKey */ propKey,
        /* diagnosticValue */ context->propPath,
        /* expectedType */ CFStringGetTypeID(),
        /* legalValues */ NULL,
        /* required */ false,
        /* typeRequired */ true,
        /* nonnilRequired */ true,   // xxx - allow empty string here?
        /* valueOut */ (CFTypeRef *)&stringValue,
        /* valueIsNonnil */ &valueIsNonnil);
    context->valid = context->valid && checkResult;

    if (stringValue) {
        personalityKext = OSKextGetKextWithIdentifier(stringValue);
        if (personalityKext &&
            personalityKext != context->kext &&
            !OSKextIsLoadable(personalityKext)) {

                // xxx - should we keep format of diagnostic the same and
                // xxx - leave it as a stupid CFURL?
                
                // XXX - THIS IS NOT AN ABSOLUTE PATH
    
                diagnosticString = CFStringCreateWithFormat(kCFAllocatorDefault,
                    /* formatOptions */ NULL, CFSTR("'%@' -> '%@'"),
                    stringValue, OSKextGetURL(personalityKext));
                __OSKextAddDiagnostic(context->kext, kOSKextDiagnosticsFlagWarnings,
                    kOSKextDiagnosticPersonalityNamesNonloadableKextKey,
                    diagnosticString, /* note */ NULL);
                SAFE_RELEASE_NULL(diagnosticString);
        }
    }

    CFArrayRemoveValueAtIndex(context->propPath,
        CFArrayGetCount(context->propPath) - 1);

   /*********************
    * end of properties *
    *********************/
finish:

   /* Remove the personality name from the prop path.
    */
    CFArrayRemoveValueAtIndex(context->propPath,
        CFArrayGetCount(context->propPath) - 1);

    SAFE_RELEASE(diagnosticString);

    return;
}

/*********************************************************************
* validates only for current default arch
*********************************************************************/
Boolean __OSKextValidate(OSKextRef aKext, CFMutableArrayRef propPath)
{
    Boolean           result        = true;  // cleared when we hit a failure
    CFStringRef       propKey       = NULL;  // do not release
    CFMutableArrayRef allocPropPath = NULL;  // must release
    Boolean           checkResult;

    CFDictionaryRef   dictValue = NULL;  // do not release
    Boolean           valueIsNonnil;
    
    char              kextPathCString[PATH_MAX];

    __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
        /* resolveToBase? */ false, kextPathCString);

    OSKextLog(aKext, kOSKextLogStepLevel | kOSKextLogValidationFlag,
        "Validating %s.", kextPathCString);

   /* Even if we already determined the kext had problems (for this arch)
    * without doing a full validation (for this arch), there may be more
    * to find, so clear all the validationflags, start from scratch,
    * and check *everything*.
    */
    aKext->flags.invalid = 0;
    aKext->flags.valid = 0;
    aKext->flags.validated = 0;

    if (!propPath) {
        allocPropPath = propPath = CFArrayCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeArrayCallBacks);
        if (!propPath) {
            OSKextLogMemError();
            goto finish;
        }
    }

   /* Redo the basic processing. If that fails, set the result to false,
    * but don't go to finish unless we didn't get an infoDictionary to validate.
    */
    if (!__OSKextProcessInfoDictionary(aKext, /* kextBundle */ NULL)) {
        result = false;
    }
    if (!aKext->infoDictionary) {
        goto finish;
    }

   /*****************************************************
    * OSBundleAllowUserLoad: boolean
    *****************************************************/

    propKey = CFSTR(kOSBundleAllowUserLoadKey);
    CFArrayAppendValue(propPath, propKey);

    checkResult = __OSKextCheckProperty(aKext,
        aKext->infoDictionary,
        /* propKey */ propKey,
        /* diagnosticValue */ propPath,
        /* expectedType */ CFBooleanGetTypeID(),
        /* legalValues */ NULL,
        /* required */ FALSE,
        /* typeRequired */ true,
        /* nonnilRequired */ FALSE,
        /* valueOut */ NULL,
        /* valueIsNonnil */ NULL);
    result = result && checkResult;

    CFArrayRemoveValueAtIndex(propPath, CFArrayGetCount(propPath) - 1);

   /*****************************************************
    * OSBundleLibraries: dict, values parsable versions *
    *****************************************************/

    propKey = CFSTR(kOSBundleLibrariesKey);
    CFArrayAppendValue(propPath, propKey);

    checkResult = __OSKextCheckProperty(aKext,
        aKext->infoDictionary,
        /* propKey */ propKey,
        /* diagnosticValue */ propPath,
        /* expectedType */ CFDictionaryGetTypeID(),
        /* legalValues */ NULL,
        /* required */ (OSKextDeclaresExecutable(aKext) &&
                       !OSKextIsKernelComponent(aKext)),
        /* typeRequired */ true,
        /* nonnilRequired */ (OSKextDeclaresExecutable(aKext) &&
                       !OSKextIsKernelComponent(aKext)),
        /* valueOut */ (CFTypeRef *)&dictValue,
        /* valueIsNonnil */ NULL);
    result = result && checkResult;
    
   /* First check is for no OSBundleLibraries.
    * All following "else if" mean the kext has at least one.
    */
    if (!dictValue || !CFDictionaryGetCount(dictValue)) {
        if (OSKextDeclaresExecutable(aKext) &&
            !OSKextIsKernelComponent(aKext)) {

            __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
                kOSKextDiagnosticMissingPropertyKey,
                propPath, /* note */ NULL);
        }
    } else if (OSKextIsKernelComponent(aKext)) {
        // xxx - should I catch kernel components that declare dependencies
        // xxx - in case somebody screws up the System.kexts? :-P

        __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
            kOSKextDiagnosticBadSystemPropertyKey, CFSTR(kOSBundleLibrariesKey),
            /* note */ NULL);

        result = false;
    } else if (!OSKextDeclaresExecutable(aKext) && !OSKextIsLibrary(aKext)) {
        __OSKextSetDiagnostic(aKext, kOSKextDiagnosticsFlagWarnings,
            kOSKextDiagnosticCodelessWithLibrariesKey);
    }

    if (checkResult && dictValue) {
        __OSKextValidateOSBundleLibraryContext validateLibrariesContext;
        validateLibrariesContext.kext = aKext;
        validateLibrariesContext.libraries = dictValue;
        validateLibrariesContext.propPath = propPath;
        validateLibrariesContext.valid = true;  // starts true!
        validateLibrariesContext.hasKernelStyleDependency = false;  // starts false!
        validateLibrariesContext.hasKPIStyleDependency = false;  // starts false!
        CFDictionaryApplyFunction(dictValue,
            __OSKextValidateOSBundleLibraryApplierFunction,
            &validateLibrariesContext);
        result = result && validateLibrariesContext.valid;

        if (validateLibrariesContext.hasKernelStyleDependency &&
            validateLibrariesContext.hasKPIStyleDependency) {

            __OSKextSetDiagnostic(aKext, kOSKextDiagnosticsFlagWarnings,
                kOSKextDiagnosticDeclaresBothKernelAndKPIDependenciesKey);
        }
    }

    dictValue = NULL;
    CFArrayRemoveValueAtIndex(propPath, CFArrayGetCount(propPath) - 1);

   /****************************************
    * OSBundleStartupResources: dictionary *
    ****************************************/
    // xxx - verify they exist
    // kOSKextOSBundleStartupResourceNamesKey

   /**********************************
    * IOKitPersonalities: dictionary *
    **********************************/

   /* xxx - Disabling for safe boot if IOKitDebug left out for now */

    propKey = CFSTR(kIOKitPersonalitiesKey);
    CFArrayAppendValue(propPath, propKey);

    // xxx - need to check that each personality is also a dict
    checkResult = __OSKextCheckProperty(aKext,
        aKext->infoDictionary,
        /* propKey */ propKey,
        /* diagnosticValue */ propPath,
        /* expectedType */ CFDictionaryGetTypeID(),
        /* legalValues */ NULL,
        /* required */ false,                 // can't really say it's required!
        /* typeRequired */ true,
        /* nonnilRequired */ false,           // can't really say it's required!
        /* valueOut */ (CFTypeRef *)&dictValue,
        &valueIsNonnil);
    result = result && checkResult;
    if (checkResult && dictValue) {
        __OSKextValidateIOKitPersonalityContext validatePersonalitiesContext;

        validatePersonalitiesContext.kext = aKext;
        validatePersonalitiesContext.personalities = dictValue;
        validatePersonalitiesContext.propPath = propPath;
        validatePersonalitiesContext.valid = true;  // starts true!
        validatePersonalitiesContext.justCheckingIOKitDebug = false;
        CFDictionaryApplyFunction(dictValue,
            __OSKextValidateIOKitPersonalityApplierFunction,
            &validatePersonalitiesContext);
        result = result && validatePersonalitiesContext.valid;
    }

    CFArrayRemoveValueAtIndex(propPath, CFArrayGetCount(propPath) - 1);

   /***********************************************
    * Validate the current arch in the executable *
    ***********************************************/
   // xxx - probably want to validate all arches, and not fail VALIDATION
   // xxx - if a kext doesn't support the current arch

    result = __OSKextValidateExecutable(aKext) && result;

finish:
    SAFE_RELEASE(allocPropPath);

    if (result) {
        aKext->flags.validated = true;
        aKext->flags.valid = true;
    } else {
        aKext->flags.invalid = 1;
    }

    return result;
}

/*********************************************************************
* This public entry point for validation does all the real validation
* via __OSKextValidate() and then adds a check that targets of any
* IOKitPersonalities are in fact loadable themselves. We have to do
* this check outside of the internal check--that is, only on request--
* to avoid an infinite loop if two kexts have personalities that name
* each other, or if a kext has a personality naming another that in
* turn has a link dependency (direct or indirect) on the first kext.
*********************************************************************/
Boolean OSKextValidate(OSKextRef aKext)
{
    Boolean           result    = TRUE;
    CFMutableArrayRef propPath  = NULL;  // must release
    CFStringRef       propKey   = NULL;  // do not release
    Boolean           checkResult;
    CFDictionaryRef   dictValue = NULL;  // do not release

    propPath = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!propPath) {
        OSKextLogMemError();
        goto finish;
    }

    result = __OSKextValidate(aKext, propPath);

    propKey = CFSTR(kIOKitPersonalitiesKey);
    CFArrayAppendValue(propPath, propKey);

    // xxx - need to check that each personality is also a dict
    checkResult = __OSKextCheckProperty(aKext,
        aKext->infoDictionary,
        /* propKey */ propKey,
        /* diagnosticValue */ propPath,
        /* expectedType */ CFDictionaryGetTypeID(),
        /* legalValues */ NULL,
        /* required */ false,                 // can't really say it's required!
        /* typeRequired */ true,
        /* nonnilRequired */ false,           // can't really say it's required!
        /* valueOut */ (CFTypeRef *)&dictValue,
        /* valueIsNonnil */ NULL);
    result = result && checkResult;
    if (checkResult && dictValue) {
        __OSKextValidateIOKitPersonalityContext validatePersonalitiesContext;

        validatePersonalitiesContext.kext = aKext;
        validatePersonalitiesContext.personalities = dictValue;
        validatePersonalitiesContext.propPath = propPath;
        validatePersonalitiesContext.valid = true;  // starts true!
        validatePersonalitiesContext.justCheckingIOKitDebug = false;
        CFDictionaryApplyFunction(dictValue,
            __OSKextValidateIOKitPersonalityTargetApplierFunction,
            &validatePersonalitiesContext);
        result = result && validatePersonalitiesContext.valid;
    }

    CFArrayRemoveValueAtIndex(propPath, CFArrayGetCount(propPath) - 1);

finish:

    SAFE_RELEASE(propPath);

    return result;
}

/*********************************************************************
*********************************************************************/
Boolean __OSKextValidateExecutable(OSKextRef aKext)
{
    Boolean                    result             = false;
    CFDataRef                  executable         = NULL;
    const struct mach_header * mach_header        = NULL;  // do not free
    const void               * file_end           = NULL;  // do not free

    macho_seek_result          seek_result;
    uint8_t                    nlist_type;
    const void               * symbol_address     = NULL;  // do not free

    const kmod_info_32_v1_t  * kmod_info_32       = NULL;  // do not free
    const kmod_info_64_v1_t  * kmod_info_64       = NULL;  // do not free
    CFStringRef                bundleIdentifier   = NULL;  // do not release
    const u_char             * kmodNameCString    = NULL;  // do not free
    const u_char             * kmodVersionCString = NULL;  // do not free
    CFStringRef                kmodName           = NULL;  // must release
    OSKextVersion              version            = -1;

   /* A kext that doesn't declare an executable is automatically ok,
    * as is an interface kext since it won't have a kmod_info struct.
    */
    if (!OSKextDeclaresExecutable(aKext) ||
        OSKextIsInterface(aKext)) {

        result = true;
        goto finish;
    }

   /* If we got here but can't read the executable at all,
    * that is a validation problem.
    */
    if (!__OSKextReadExecutable(aKext)) {
        goto finish;
    }

   /* However, if the kext doesn't support the current arch, that is not a
    * validation problem (see OSKextSupportsArchitecture()) so return true.
    */
    executable = OSKextCopyExecutableForArchitecture(aKext, OSKextGetArchitecture());
    if (!executable) {
        result = true;
        goto finish;
    }

    mach_header = (const struct mach_header *)CFDataGetBytePtr(executable);
    file_end = (((const char *)mach_header) + CFDataGetLength(executable));

    seek_result = macho_find_symbol(
            mach_header, file_end, __kOSKextKmodInfoSymbol, &nlist_type,
            &symbol_address);

    if ((macho_seek_result_found != seek_result) ||
        ((nlist_type & N_TYPE) != N_SECT) ||
        (symbol_address == NULL)) {

        __OSKextSetDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
            kOSKextDiagnosticExecutableBadKey);
        goto finish;
    }

   /*****
    * If the kext requires Jaguar or later, we don't have to inspect
    * the MODULE_NAME and MODULE_VERSION fields of the kmod_info struct.
    * FIXME: change version reference
    */
    if (!aKext->flags.warnForMismatchedKmodInfo) {
        result = true;
        goto finish;
    }

    if (__OSKextIsArchitectureLP64()) {
        kmod_info_64 = (kmod_info_64_v1_t *)symbol_address;
        kmodNameCString = kmod_info_64->name;
        kmodVersionCString = kmod_info_64->version;
    } else {
        kmod_info_32 = (kmod_info_32_v1_t *)symbol_address;
        kmodNameCString = kmod_info_32->name;
        kmodVersionCString = kmod_info_32->version;
    }

    bundleIdentifier = OSKextGetIdentifier(aKext);
    kmodName = CFStringCreateWithCString(kCFAllocatorDefault,
        (const char *)kmodNameCString, kCFStringEncodingUTF8);
    if (!kmodName) {
        OSKextLogMemError();
        goto finish;
    }
    if (!CFEqual(bundleIdentifier, kmodName)) {
        __OSKextSetDiagnostic(aKext, kOSKextDiagnosticsFlagWarnings,
            kOSKextDiagnosticBundleIdentifierMismatchKey);
    }

    version = OSKextParseVersionString((const char *)kmodVersionCString);
    if (version < 0 || aKext->version != version) {
        __OSKextSetDiagnostic(aKext, kOSKextDiagnosticsFlagWarnings,
            kOSKextDiagnosticBundleVersionMismatchKey);
    }

    result = true;

finish:
   /* Advise the system that we no longer need the mmapped executable.
    */
    if (executable) {
        (void)posix_madvise((void *)CFDataGetBytePtr(executable),
            CFDataGetLength(executable),
            POSIX_MADV_DONTNEED);
    }

    // xxx - how do we handle cleanup of load info?
    SAFE_RELEASE(executable);
    SAFE_RELEASE(kmodName);
    return result;
}

/*********************************************************************
* Internal-use function that avoids doing cross-kext validation,
* which can result in an infinite loop. See OSKextValidate() for the
* additional checks.
*********************************************************************/
Boolean __OSKextIsValid(OSKextRef aKext)
{
   /* If we know it's not valid, don't do any expensive checks
    * and just return false right away.
    */
    if (aKext->flags.invalid) {
        return false;
    }
    if (!aKext->flags.validated) {
        __OSKextValidate(aKext, /* propPath */ NULL);
    }

    return aKext->flags.valid ? true : false;
}

/*********************************************************************
* Extern-use function that does cross-kext validation.
* See OSKextValidate() for the additional checks.
*********************************************************************/
Boolean OSKextIsValid(OSKextRef aKext)
{
   /* If we know it's not valid, don't do any expensive checks
    * and just return false right away.
    */
    if (aKext->flags.invalid) {
        return false;
    }
    if (!aKext->flags.validated) {
        OSKextValidate(aKext);
    }

    return aKext->flags.valid ? true : false;
}

/*********************************************************************
*********************************************************************/
Boolean __OSKextAuthenticateURLRecursively(
    OSKextRef aKext,
    CFURLRef  anURL,
    CFURLRef  pluginsURL)
{
    Boolean      result   = true;  // until we hit a bad one
    CFStringRef  filename = NULL;   // must release
    CFURLRef     absURL   = NULL;   // must release
    char         kextPath[PATH_MAX];
    char         urlPath[PATH_MAX];
    struct stat  stat_buf;
    struct stat  lstat_buf;
    CFArrayRef   urlContents = NULL; // must release
    SInt32       error;
    CFIndex      count, i;

    __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
        /* resolveToBase */ FALSE, kextPath);

   /* Ignore all .DS_Store turdfiles created by Finder.
    */
    filename = CFURLCopyLastPathComponent(anURL);
    if (filename && CFEqual(filename, __kDSStoreFilename)) {
        goto finish;
    }

    absURL = CFURLCopyAbsoluteURL(anURL);
    if (!absURL) {
        result = false;
        OSKextLogMemError();
        goto finish;
    }

    if (!__OSKextGetFileSystemPath(/* kext */ NULL, anURL,
        /* resolveToBase */ true, urlPath)) {

        __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
            kOSKextDiagnosticURLConversionKey, anURL, /* note */ NULL);
        result = false;
        goto finish;
    }

    OSKextLog(aKext,
        kOSKextLogStepLevel |
        kOSKextLogAuthenticationFlag | kOSKextLogFileAccessFlag,
        "Authenticating %s file/directory %s.",
        kextPath, urlPath);

    if (0 != stat(urlPath, &stat_buf) || 0 != lstat(urlPath, &lstat_buf)) {
        if (errno == ENOENT) {
            __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagAuthentication,
                kOSKextDiagnosticFileNotFoundKey, anURL, /* note */ NULL);
            result = false;
            // can't continue so goto finish
            goto finish;
        } else {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "Can't stat %s - %s.",
                urlPath, strerror(errno));
            result = false;
            // can't continue so goto finish
            goto finish;
        }
    }

   /* File/dir must be owned by root and not writable by others,
    * and if not owned by gid 0 then not group-writable.
    */
    if ( (stat_buf.st_uid != 0) || (stat_buf.st_gid != 0 ) ||
         (stat_buf.st_mode & S_IWOTH) || (stat_buf.st_mode & S_IWGRP) ) {

        // xxx - log it
        __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagAuthentication,
            kOSKextDiagnosticOwnerPermissionKey, anURL, /* note */ NULL);
        result = false;
        // keep going to get all child URLs
    }

    if ((lstat_buf.st_mode & S_IFMT) == S_IFLNK) {
        __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagWarnings,
            kOSKextDiagnosticSymlinkKey, anURL, /* note */ NULL);
        /* We don't consider this a hard failure. */
    }

    if (!CFURLHasDirectoryPath(anURL)) {
        goto finish;
    }

   /* Plugins are considered not to be part of the kext, since they are
    * whole bundles in their own right and aren't necessarily kexts.
    */
    if (pluginsURL && CFEqual(absURL, pluginsURL)) {
        // result is true in this case
        goto finish;
    }

   /* Check all the child URLs.
    * xxx - should we check for a symlink loop? :-O
    */
    urlContents = CFURLCreatePropertyFromResource(CFGetAllocator(aKext),
        anURL,
        kCFURLFileDirectoryContents, &error);
    if (!urlContents || error) {
        OSKextLog(aKext,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag |
            kOSKextLogAuthenticationFlag,
            "Can't read file %s.", urlPath);
        goto finish;
    }
    count = CFArrayGetCount(urlContents);
    for (i = 0; i < count; i++) {
        CFURLRef thisURL = (CFURLRef)CFArrayGetValueAtIndex(urlContents, i);
        result = __OSKextAuthenticateURLRecursively(aKext, thisURL, pluginsURL) && result;
    }

finish:
    SAFE_RELEASE(filename);
    SAFE_RELEASE(absURL);
    SAFE_RELEASE(urlContents);
    return result;
}

/*********************************************************************
*********************************************************************/
Boolean OSKextAuthenticate(OSKextRef aKext)
{
    Boolean     result        = true;  // cleared when we hit an error
    CFBundleRef kextBundle    = NULL;  // must release
    CFURLRef    pluginsURL    = NULL;  // must release
    CFURLRef    pluginsAbsURL = NULL;  // must release

    aKext->flags.inauthentic = 0;
    aKext->flags.authentic = 0;
    aKext->flags.authenticated = 0;

    if (OSKextIsFromMkext(aKext)) {
        if (aKext->mkextInfo->mkextURL) {
            result = __OSKextAuthenticateURLRecursively(aKext,
                aKext->mkextInfo->mkextURL, /* pluginsURL */ NULL);
            // xxx - need to look up all kexts from the mkext and mark them authenticated
        } else {
            __OSKextSetDiagnostic(aKext, kOSKextDiagnosticsFlagAuthentication,
                kOSKextDiagnosticNoFileKey);
            result = false;
        }
    } else {
        kextBundle = CFBundleCreate(kCFAllocatorDefault, aKext->bundleURL);
        // xxx should log bundle creation/error/release
        if (!kextBundle) {
            result = false;
            goto finish;
        }

        pluginsURL = CFBundleCopyBuiltInPlugInsURL(kextBundle);
        if (pluginsURL) {
            pluginsAbsURL = CFURLCopyAbsoluteURL(pluginsURL);
            if (!pluginsAbsURL) {
                OSKextLogMemError();
                result = false;
                goto finish;
            }
        }

        result = __OSKextAuthenticateURLRecursively(aKext, aKext->bundleURL,
            pluginsAbsURL);
    }

finish:

   /*****
    * All tests passed, yay.
    */
    if (result) {
        aKext->flags.authentic = 1;
        aKext->flags.authenticated = 1;
    }

    SAFE_RELEASE(kextBundle);
    SAFE_RELEASE(pluginsURL);
    SAFE_RELEASE(pluginsAbsURL);

    return result;
}

/*********************************************************************
*********************************************************************/
Boolean OSKextIsAuthentic(OSKextRef aKext)
{
   /* If we know it's not authentic, don't do any expensive checks
    * and just return false right away.
    */
    if (aKext->flags.inauthentic) {
        return false;
    }
    if (!aKext->flags.authenticated) {
        // xxx - maybe we should do an abort-on first error if coming via this func
        OSKextAuthenticate(aKext);
    }

    return aKext->flags.authentic ? true : false;
}

/*********************************************************************
*********************************************************************/
Boolean OSKextIsLoadable(OSKextRef aKext)
{
    Boolean result = false;

    // xxx - should also do a trial link, now
    // xxx - says nothing about whether other vers. is loaded
    // xxx - should we take a safe boot param?
    // xxx - should we just check against actual safe boot?

    if (__OSKextIsValid(aKext) &&
        OSKextIsAuthentic(aKext) &&
        OSKextResolveDependencies(aKext) &&
        OSKextValidateDependencies(aKext) &&
        OSKextAuthenticateDependencies(aKext)) {

        result = true;
    }

// xxx? -    OSKextFlushLoadInfo(aKext);
    return result;
}

/*********************************************************************
* XXX - This will create a bunch of empty subdictionaries, which I
* XXX - don't particularly like.
*********************************************************************/
CFDictionaryRef
OSKextCopyDiagnostics(OSKextRef aKext,
    OSKextDiagnosticsFlags typeFlags)
{
    CFDictionaryRef result = NULL;
    CFMutableDictionaryRef mResult = NULL;

    if (aKext->diagnostics) {
        CFDictionaryRef diagnosticsDict;

        mResult = CFDictionaryCreateMutable(CFGetAllocator(aKext), 0,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (!mResult) {
            OSKextLogMemError();
            goto finish;
        }

        if ((typeFlags & kOSKextDiagnosticsFlagValidation)) {

            diagnosticsDict = __OSKextCopyDiagnosticsDict(aKext,
                kOSKextDiagnosticsFlagValidation);
            if (diagnosticsDict && CFDictionaryGetCount(diagnosticsDict)) {
                CFDictionarySetValue(mResult, kOSKextDiagnosticsValidationKey,
                    diagnosticsDict);
            }
            SAFE_RELEASE_NULL(diagnosticsDict);
        }
        if ((typeFlags & kOSKextDiagnosticsFlagAuthentication)) {

            diagnosticsDict = __OSKextCopyDiagnosticsDict(aKext,
                kOSKextDiagnosticsFlagAuthentication);
            if (diagnosticsDict && CFDictionaryGetCount(diagnosticsDict)) {
                CFDictionarySetValue(mResult, kOSKextDiagnosticsAuthenticationKey,
                    diagnosticsDict);
            }
            SAFE_RELEASE_NULL(diagnosticsDict);
        }
        if ((typeFlags & kOSKextDiagnosticsFlagDependencies)) {

            diagnosticsDict = __OSKextCopyDiagnosticsDict(aKext,
                kOSKextDiagnosticsFlagDependencies);
            if (diagnosticsDict && CFDictionaryGetCount(diagnosticsDict)) {
                CFDictionarySetValue(mResult, kOSKextDiagnosticsDependenciesKey,
                    diagnosticsDict);
            }
            SAFE_RELEASE_NULL(diagnosticsDict);
        }
        if ((typeFlags & kOSKextDiagnosticsFlagWarnings)) {

            diagnosticsDict = __OSKextCopyDiagnosticsDict(aKext,
                kOSKextDiagnosticsFlagWarnings);
            if (diagnosticsDict && CFDictionaryGetCount(diagnosticsDict)) {
                CFDictionarySetValue(mResult, kOSKextDiagnosticsWarningsKey,
                    diagnosticsDict);
            }
            SAFE_RELEASE_NULL(diagnosticsDict);
        }

        if ((typeFlags & kOSKextDiagnosticsFlagBootLevel)) {
            diagnosticsDict = __OSKextCopyDiagnosticsDict(aKext,
                kOSKextDiagnosticsFlagBootLevel);
            if (diagnosticsDict && CFDictionaryGetCount(diagnosticsDict)) {
                CFDictionarySetValue(mResult, kOSKextDiagnosticsBootLevelKey,
                    diagnosticsDict);
            }
            SAFE_RELEASE_NULL(diagnosticsDict);
        }

        result = (CFDictionaryRef)mResult;
    } else {
        result = CFDictionaryCreate(CFGetAllocator(aKext),
            NULL, NULL, 0,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
finish:
    return result;
}

/*********************************************************************
*********************************************************************/
CFDictionaryRef __OSKextCopyDiagnosticsDict(
    OSKextRef              aKext,
    OSKextDiagnosticsFlags type)
{
    CFDictionaryRef result = NULL;
    CFDictionaryRef dictToCopy = NULL;  // do not release

    if (!aKext->diagnostics) {
        goto finish;
    }
    switch (type) {
        case kOSKextDiagnosticsFlagValidation:
            dictToCopy = aKext->diagnostics->validationFailures;
            break;
        case kOSKextDiagnosticsFlagAuthentication:
            dictToCopy = aKext->diagnostics->authenticationFailures;
            break;
        case kOSKextDiagnosticsFlagDependencies:
            dictToCopy = aKext->diagnostics->dependencyFailures;
            break;
        case kOSKextDiagnosticsFlagWarnings:
            dictToCopy = aKext->diagnostics->warnings;
            break;
        case kOSKextDiagnosticsFlagBootLevel:
            dictToCopy = aKext->diagnostics->bootLevel;
            break;
        default:
            break;
    }

    if (dictToCopy) {
        result = CFDictionaryCreateCopy(CFGetAllocator(aKext), dictToCopy);
    }

finish:
    if (!result) {
        result = CFDictionaryCreate(CFGetAllocator(aKext),
            NULL, NULL, 0,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
    return result;
}

/*********************************************************************
*********************************************************************/
/*********************************************************************
*********************************************************************/
void OSKextLogDiagnostics(OSKextRef aKext,
    OSKextDiagnosticsFlags typeFlags)
{
    CFDictionaryRef   diagnosticsDict = NULL;  // must release
    CFStringRef       plistString     = NULL;  // must release
    char            * cString         = NULL;  // must free

    diagnosticsDict = OSKextCopyDiagnostics(aKext, typeFlags);
    if (!diagnosticsDict || !CFDictionaryGetCount(diagnosticsDict)) {
        goto finish;
    }
    plistString = createCFStringForPlist_new(diagnosticsDict,
        kPListStyleDiagnostics);
    if (!plistString) {
        goto finish;
    }
    cString = createUTF8CStringForCFString(plistString);
    if (!cString) {
        goto finish;
    }
    OSKextLog(/* kext */ NULL,
        kOSKextLogExplicitLevel | kOSKextLogGeneralFlag,
        "%s", cString);

finish:
    SAFE_RELEASE(diagnosticsDict);
    SAFE_RELEASE(plistString);
    SAFE_FREE(cString);
    return;
}

/*********************************************************************
*********************************************************************/
typedef struct {
    OSKextDiagnosticsFlags typeFlags;
} __OSKextFlushDiagnosticsContext;

static void __OSKextFlushDiagnosticsApplierFunction(
    const void * vKey __unused,
    const void * vValue,
          void * vContext)
{
    OSKextRef aKext = (OSKextRef)vValue;
    __OSKextFlushDiagnosticsContext * context =
        (__OSKextFlushDiagnosticsContext *)vContext;

    OSKextFlushDiagnostics(aKext, context->typeFlags);
    return;
}

const UInt32 __kOSKextDiagnosticsFlagAllImplemented =
    kOSKextDiagnosticsFlagValidation     |
    kOSKextDiagnosticsFlagAuthentication |
    kOSKextDiagnosticsFlagDependencies   |
    kOSKextDiagnosticsFlagWarnings       |
    kOSKextDiagnosticsFlagBootLevel;

void OSKextFlushDiagnostics(OSKextRef aKext, OSKextDiagnosticsFlags typeFlags)
{
    pthread_once(&__sOSKextInitialized, __OSKextInitialize);

    if (aKext) {
        if (aKext->diagnostics) {
            if (typeFlags & kOSKextDiagnosticsFlagValidation) {
                SAFE_RELEASE_NULL(aKext->diagnostics->validationFailures);
            }
            if (typeFlags & kOSKextDiagnosticsFlagAuthentication) {
                SAFE_RELEASE_NULL(aKext->diagnostics->authenticationFailures);
            }
            if (typeFlags & kOSKextDiagnosticsFlagDependencies) {
                SAFE_RELEASE_NULL(aKext->diagnostics->dependencyFailures);
            }
            if (typeFlags & kOSKextDiagnosticsFlagWarnings) {
                SAFE_RELEASE_NULL(aKext->diagnostics->warnings);
            }
            if (typeFlags & kOSKextDiagnosticsFlagBootLevel) {
                SAFE_RELEASE_NULL(aKext->diagnostics->bootLevel);
            }
            if ((typeFlags & __kOSKextDiagnosticsFlagAllImplemented) ==
                 __kOSKextDiagnosticsFlagAllImplemented) {

                SAFE_FREE_NULL(aKext->diagnostics);
            }
        }
    } else if (__sOSKextsByURL) {
        __OSKextFlushDiagnosticsContext context;
        context.typeFlags = typeFlags;
        CFDictionaryApplyFunction(__sOSKextsByURL,
            __OSKextFlushDiagnosticsApplierFunction, &context);

    }
    return;
}

/*********************************************************************
*********************************************************************/
CFMutableDictionaryRef __OSKextGetDiagnostics(OSKextRef aKext,
    OSKextDiagnosticsFlags type)
{
    CFMutableDictionaryRef result = NULL;

    if (!aKext->diagnostics) {
        aKext->diagnostics = (__OSKextDiagnostics *)malloc(
            sizeof(*(aKext->diagnostics)));
        if (!aKext->diagnostics) {
            OSKextLogMemError();
            goto finish;
        }
        memset(aKext->diagnostics, 0, sizeof(*(aKext->diagnostics)));
    }

    switch (type) {
        case kOSKextDiagnosticsFlagValidation:
            if (!aKext->diagnostics->validationFailures) {
                aKext->diagnostics->validationFailures =
                    CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                    &kCFTypeDictionaryKeyCallBacks,
                    &kCFTypeDictionaryValueCallBacks);
                if (!aKext->diagnostics->validationFailures) {
                    OSKextLogMemError();
                    goto finish;
                }
            }
            result = aKext->diagnostics->validationFailures;
            break;
        case kOSKextDiagnosticsFlagAuthentication:
            if (!aKext->diagnostics->authenticationFailures) {
                aKext->diagnostics->authenticationFailures =
                    CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                    &kCFTypeDictionaryKeyCallBacks,
                    &kCFTypeDictionaryValueCallBacks);
                if (!aKext->diagnostics->authenticationFailures) {
                    OSKextLogMemError();
                    goto finish;
                }
            }
            result = aKext->diagnostics->authenticationFailures;
            break;
        case kOSKextDiagnosticsFlagDependencies:
            if (!aKext->diagnostics->dependencyFailures) {
                aKext->diagnostics->dependencyFailures =
                    CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                    &kCFTypeDictionaryKeyCallBacks,
                    &kCFTypeDictionaryValueCallBacks);
                if (!aKext->diagnostics->dependencyFailures) {
                    OSKextLogMemError();
                    goto finish;
                }
            }
            result = aKext->diagnostics->dependencyFailures;
            break;
        case kOSKextDiagnosticsFlagWarnings:
            if (!aKext->diagnostics->warnings) {
                aKext->diagnostics->warnings =
                    CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                    &kCFTypeDictionaryKeyCallBacks,
                    &kCFTypeDictionaryValueCallBacks);
                if (!aKext->diagnostics->warnings) {
                    OSKextLogMemError();
                    goto finish;
                }
            }
            result = aKext->diagnostics->warnings;
            break;
        case kOSKextDiagnosticsFlagBootLevel:
            if (!aKext->diagnostics->bootLevel) {
                aKext->diagnostics->bootLevel =
                    CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                    &kCFTypeDictionaryKeyCallBacks,
                    &kCFTypeDictionaryValueCallBacks);
                if (!aKext->diagnostics->bootLevel) {
                    OSKextLogMemError();
                    goto finish;
                }
            }
            result = aKext->diagnostics->bootLevel;
            break;
    }

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
void __OSKextSetDiagnostic(
    OSKextRef              aKext,
    OSKextDiagnosticsFlags type,
    CFStringRef            key)
{
    CFMutableDictionaryRef diagnosticDict;

    if (!(type & __sOSKextRecordsDiagnositcs)) {
        goto finish;
    }

    diagnosticDict = __OSKextGetDiagnostics(aKext, type);
    if (!diagnosticDict) {
        goto finish;
    }
    CFDictionarySetValue(diagnosticDict, key, kCFBooleanTrue);

finish:
    return;
}

/*********************************************************************
*********************************************************************/
void __OSKextAddDiagnostic(
    OSKextRef              aKext,
    OSKextDiagnosticsFlags type,
    CFStringRef            key,
    CFTypeRef              value,
    CFTypeRef              note)
{
    CFMutableDictionaryRef diagnosticDict;
    CFMutableArrayRef      valueArray;
    CFMutableArrayRef      createdArray   = NULL;  // must release
    CFStringRef            combinedValue  = NULL;  // must release
    CFStringRef            valueWithNote  = NULL;  // must release
    CFStringRef            valueToSet     = NULL;  // do not release

    if (!(type & __sOSKextRecordsDiagnositcs)) {
        goto finish;
    }

    diagnosticDict = __OSKextGetDiagnostics(aKext, type);
    if (!diagnosticDict) {
        goto finish;
    }

    valueToSet = value;
    if (CFGetTypeID(value) == CFArrayGetTypeID()) {
        combinedValue = CFStringCreateByCombiningStrings(kCFAllocatorDefault,
            (CFArrayRef)value, CFSTR("."));
        if (!combinedValue) {
            OSKextLogMemError();
            goto finish;
        }
        valueToSet = combinedValue;
    }
    
    if (note) {
        valueWithNote = CFStringCreateWithFormat(kCFAllocatorDefault,
            /* options */ NULL, CFSTR("%@ - %@"), valueToSet, note);
        if (!valueWithNote) {
            OSKextLogMemError();
            goto finish;
        }
        valueToSet = valueWithNote;
    }

    valueArray = (CFMutableArrayRef)CFDictionaryGetValue(diagnosticDict, key);
    if (!valueArray) {
        valueArray = createdArray = CFArrayCreateMutable(kCFAllocatorDefault,
            0, &kCFTypeArrayCallBacks);
        if (!valueArray) {
            OSKextLogMemError();
            goto finish;
        }
        CFDictionarySetValue(diagnosticDict, key, valueArray);

   /* Don't allow what should have been a call to __OSKextSetDiagnostic(),
    * which adds a single Boolean true for a given key.
    */
    } else if (CFArrayGetTypeID() != CFGetTypeID(valueArray)) {
        OSKextLog(aKext,
            kOSKextLogErrorLevel | kOSKextLogKextBookkeepingFlag,
            "Internal error in diagnositcs recording");
        goto finish;
    }
    if (!CFArrayGetCountOfValue(valueArray, RANGE_ALL(valueArray), valueToSet)) {
        CFArrayAppendValue(valueArray, valueToSet);
    }

finish:
    SAFE_RELEASE(createdArray);
    SAFE_RELEASE(combinedValue);
    SAFE_RELEASE(valueWithNote);
    return;
}

/*********************************************************************
*********************************************************************/
Boolean __OSKextCheckProperty(
    OSKextRef       aKext,
    CFDictionaryRef aDict,
    CFTypeRef       propKey,
    CFTypeRef       diagnosticValue, /* string or array of strings */
    CFTypeID        expectedType,
    CFArrayRef      legalValues,  /* NULL if not relevant */
    Boolean         required,
    Boolean         typeRequired,
    Boolean         nonnilRequired,
    CFTypeRef     * valueOut,
    Boolean       * valueIsNonnil)
{
    Boolean     result              = false;
    CFTypeRef   value               = NULL;  // do not release
    Boolean     isFloat             = false;
    CFStringRef diagnosticString    = NULL;  // must release
    CFStringRef noteString          = NULL;  // must release
    CFNumberRef zeroValue           = NULL;  // must release
    Boolean     valueIsNonnil_local = false;
    CFIndex     count, i;

    if (valueIsNonnil) {
        *valueIsNonnil = false;
    }
    if (valueOut) {
        *valueOut = NULL;
    }

   /* For the top-level dictionary, use OSKextGetValueForInfoDictionaryKey()
    * so we get arch-specfic variants. For nested dicts, use
    * CFDictionaryGetValue().
    */
    if (aDict == aKext->infoDictionary) {
        value = OSKextGetValueForInfoDictionaryKey(aKext, propKey);
    } else {
        value = CFDictionaryGetValue(aDict, propKey);
    }
    if (!value) {
        if (!required) {
            result = true;
        } else {
            __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
                kOSKextDiagnosticMissingPropertyKey, diagnosticValue,
                /* note */ NULL);
        }
        goto finish;
    } else if (valueOut) {
        *valueOut = value;
    }

   /* IOCFUnserialize() actually doesn't recognize <real>
    * so we'll never see this, bit of a shame.
    */
    isFloat = CFNumberGetTypeID() == CFGetTypeID(value) &&
        CFNumberIsFloatType((CFNumberRef)value);

    if (expectedType != CFGetTypeID(value) || isFloat) {
        const char * expectedTag = NULL;

        if (expectedType == CFStringGetTypeID()) {
            expectedTag = "<string>";
        } else if (expectedType == CFNumberGetTypeID() && isFloat) {
            expectedTag = "<integer> (kexts may not use <real>)";
        } else if (expectedType == CFNumberGetTypeID()) {
            expectedTag = "<integer>";
        } else if (expectedType == CFDataGetTypeID()) {
            expectedTag = "<data>";
        } else if (expectedType == CFBooleanGetTypeID()) {
            expectedTag = "boolean, <true/> or <false/>";
        } else if (expectedType == CFArrayGetTypeID()) {
            expectedTag = "<array>";
        } else if (expectedType == CFDictionaryGetTypeID()) {
            expectedTag = "<dict>";
        }
        
        if (expectedType) {
            noteString = CFStringCreateWithFormat(kCFAllocatorDefault,
                /* formatOptions */ NULL, CFSTR("should be %s"),
                expectedTag);
        }
        __OSKextAddDiagnostic(aKext,
            typeRequired ? kOSKextDiagnosticsFlagValidation : kOSKextDiagnosticsFlagWarnings,
            typeRequired ? kOSKextDiagnosticPropertyIsIllegalTypeKey : kOSKextDiagnosticTypeWarningKey,
            diagnosticString ? diagnosticString : diagnosticValue, noteString);
        goto finish;
    }

    if (legalValues) {
        Boolean valueIsLegal = false;
        count = CFArrayGetCount(legalValues);
        for (i = 0; i < count; i++) {
            CFTypeRef thisValue = CFArrayGetValueAtIndex(legalValues, i);
            if (CFEqual(thisValue, value)) {
                valueIsLegal = true;
                break;
            }
        }
        if (!valueIsLegal) {
            __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
                kOSKextDiagnosticPropertyIsIllegalValueKey, diagnosticValue,
                /* note */ NULL);
        }
    }

    if (expectedType == CFBooleanGetTypeID()) {
        CFBooleanRef boolValue = (CFBooleanRef)value;
        valueIsNonnil_local = CFBooleanGetValue(boolValue);
    } else if (expectedType == CFStringGetTypeID()) {
        CFStringRef stringValue = (CFStringRef)value;
        valueIsNonnil_local = CFStringGetLength(stringValue) ? true : false;
    } else if (expectedType == CFDataGetTypeID()) {
        CFDataRef dataValue = (CFDataRef)value;
        valueIsNonnil_local = CFDataGetLength(dataValue) ? true : false;
    } else if (expectedType == CFArrayGetTypeID()) {
        CFArrayRef arrayValue = (CFArrayRef)value;
        valueIsNonnil_local = CFArrayGetCount(arrayValue) ? true : false;
    } else if (expectedType == CFDictionaryGetTypeID()) {
        CFDictionaryRef dictValue = (CFDictionaryRef)value;
        valueIsNonnil_local = CFDictionaryGetCount(dictValue) ? true : false;
    } else if (expectedType == CFNumberGetTypeID()) {
        CFNumberRef numberValue = (CFNumberRef)value;
        int zero = 0;
        zeroValue = CFNumberCreate(kCFAllocatorDefault,
            kCFNumberIntType, &zero);
        if (!zeroValue) {
            OSKextLogMemError();
        }
        if (kCFCompareEqualTo !=
            CFNumberCompare(numberValue, zeroValue, NULL)) {

            valueIsNonnil_local = true;
        }
    }

    if (valueIsNonnil) {
         *valueIsNonnil = valueIsNonnil_local;
     }

    if (nonnilRequired && !valueIsNonnil_local) {
        __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
            kOSKextDiagnosticPropertyIsIllegalValueKey, diagnosticValue,
            /* note */ NULL);

        goto finish;
    }

    result = true;

finish:
    SAFE_RELEASE(diagnosticString);
    SAFE_RELEASE(noteString);
    SAFE_RELEASE(zeroValue);
    return result;
}

#pragma mark Misc Private Functions

/*********************************************************************
*********************************************************************/
Boolean __OSKextReadInfoDictionary(
    OSKextRef   aKext,
    CFBundleRef kextBundle)
{
    Boolean        result        = false;
    CFBundleRef    createdBundle = NULL;  // must release
    CFURLRef       infoDictURL   = NULL;  // must release
    struct stat    statbuf;
    CFDataRef      infoDictData  = NULL;  // must release
    char         * infoDictXML   = NULL;  // must free
    int            fd = -1;               // must close
    ssize_t        totalBytesRead;
    CFStringRef    errorString   = NULL;  // must release
    char        *  errorCString  = NULL;  // must free
    char           kextPath[PATH_MAX];
    char           mkextPath[PATH_MAX] = __kStringUnknown;
    char           infoDictPath[PATH_MAX];

    __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
        /* resolveToBase */ false, kextPath);

    if (aKext->infoDictionary) {
        result = true;
        goto finish;
    }

   /* Big error if we have no info dict and we came from an mkext.
    */
    if (aKext->staticFlags.isFromMkext) {
        __OSKextGetFileSystemPath(/* kext */ NULL,
            aKext->mkextInfo->mkextURL,
            /* resolveToBase */ false, mkextPath);
        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
            "%s created from m%s is missing its info dictionary.",
            kextPath, mkextPath);
        result = false;
        goto finish;
    }

    if (!kextBundle) {
        OSKextLog(aKext, kOSKextLogDebugLevel | kOSKextLogFileAccessFlag,
            "Opening CFBundle for %s.", kextPath);
        kextBundle = createdBundle = CFBundleCreate(kCFAllocatorDefault,
            aKext->bundleURL);
        if (!createdBundle) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
                "Can't open CFBundle for %s.", kextPath);
            __OSKextSetDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
                kOSKextDiagnosticNotABundleKey);
            goto finish;
        }
    }
    infoDictURL = _CFBundleCopyInfoPlistURL(kextBundle);

    if (!infoDictURL) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "%s has no Info.plist file.", kextPath);
        __OSKextSetDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
            kOSKextDiagnosticNotABundleKey);
        goto finish;
    }

    if (!__OSKextGetFileSystemPath(/* kext */ NULL, infoDictURL,
        /* resolveToBase */ true, infoDictPath)) {

        __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
            kOSKextDiagnosticURLConversionKey, infoDictURL, /* note */ NULL);
        goto finish;
    }
    if (0 != stat(infoDictPath, &statbuf)) {
        if (errno == ENOENT) {
            __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
                kOSKextDiagnosticFileNotFoundKey, infoDictURL, /* note */ NULL);
        } else {
            __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
                kOSKextDiagnosticStatFailureKey, infoDictURL, /* note */ NULL);
        }
        goto finish;
    }
    fd = open(infoDictPath, O_RDONLY);
    if (fd < 0) {
        __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
            kOSKextDiagnosticFileAccessKey, infoDictURL, /* note */ NULL);
        goto finish;
    }

    infoDictXML = (char *)malloc((1 + statbuf.st_size) * sizeof(char));
    if (!infoDictXML) {
       /* XXX - Basically hosed if this happens. */
        OSKextLogMemError();
        goto finish;
    }

    for (totalBytesRead = 0; totalBytesRead < statbuf.st_size; /* nothing */) {
        ssize_t bytesRead = read(fd, infoDictXML + totalBytesRead,
            statbuf.st_size - totalBytesRead);
        if (bytesRead < 0) {
            __OSKextSetDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
                kOSKextDiagnosticFileAccessKey);
            goto finish;
        }
        totalBytesRead += bytesRead;
    }

    infoDictXML[totalBytesRead] = '\0';

    aKext->infoDictionary = (CFDictionaryRef)IOCFUnserialize(
        (const char *)infoDictXML, kCFAllocatorDefault, 0, &errorString);
    if (!aKext->infoDictionary ||
        CFDictionaryGetTypeID() != CFGetTypeID(aKext->infoDictionary)) {

       /* We're going to fail init for a new kext object, so in addition
        * to logging we need to print an error message immediately.
        */
        __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
            kOSKextDiagnosticBadPropertyListXMLKey,
            errorString, /* note */ NULL);

        if (errorString) {
            errorCString = createUTF8CStringForCFString(errorString);
        }
        OSKextLog(aKext, kOSKextLogErrorLevel,
            "Can't read info dictionary for %s: %s.",
            kextPath, errorCString ? errorCString : "(unknown error)");

        goto finish;
    }

    result = true;

finish:
    if (createdBundle) {
        OSKextLog(aKext, kOSKextLogDebugLevel | kOSKextLogFileAccessFlag,
            "Releasing CFBundle for %s.",
            kextPath);
    }
    SAFE_RELEASE(createdBundle);
    SAFE_RELEASE(infoDictURL);
    SAFE_RELEASE(infoDictData);
    SAFE_FREE(infoDictXML);
    SAFE_RELEASE(errorString);
    SAFE_FREE(errorCString);

    if (fd >= 0) {
        close(fd);
    }

    if (!result) {
        aKext->flags.invalid = 1;
        aKext->flags.valid = 0;
    }
    return result;
}

/*********************************************************************
*********************************************************************/
Boolean __OSKextProcessInfoDictionary(
    OSKextRef   aKext,
    CFBundleRef kextBundle)
{
    Boolean           result = false;
    Boolean           valueIsNonnil;
    Boolean           checkResult;
    CFMutableArrayRef propPath            = NULL;   // must release
    CFStringRef       propKey             = NULL;   // do not release
    CFBooleanRef      boolValue           = NULL;   // do not release
    CFStringRef       stringValue         = NULL;   // do not release
    CFDictionaryRef   dictValue           = NULL;   // do not release
    CFTypeRef         debugLevel          = NULL;   // do not release
    Boolean           isInterfaceSetFalse = false;
    OSKextVersion     bundleVersion      = -1;
    OSKextVersion     compatibleVersion  = -1;

   /* Remove the kext from the lookup dictionary (if there). Its identifier or
    * version may change if we read the info dictionary from disk. This happens
    * if we're realizing from the identifier cache or have flushed the info
    * dictionary.
    */
    __OSKextRemoveKextFromIdentifierDict(aKext, __sOSKextsByIdentifier);

    if (!__OSKextReadInfoDictionary(aKext, kextBundle)) {
        goto finish;
    }

   /* Set to true so any failed property check clears.
    */
    result = true;

   /*********************************
    * CFBundlePackageType == "KEXT" *
    *********************************/

   /* This check is somewhat pedantic, but it pretty much means
    * the rest of the checks aren't worth doing.
    */
    propKey = _kCFBundlePackageTypeKey;
    checkResult = __OSKextCheckProperty(aKext,
        aKext->infoDictionary,
        /* propKey */ propKey,
        /* diagnosticValue */ propKey,
        /* expectedType */ CFStringGetTypeID(),
        /* legalValues */ __sOSKextPackageTypeValues,
        /* required */ true,
        /* typeRequired */ true,
        /* nonnilRequired */ true,
        /* valueOut */ NULL,
        /* valueIsNonnil */ NULL);
    result = result && checkResult;
    if (!checkResult) {
        goto finish;
    }

   /*****
    * For all remaining checks, fall through so we do them all even on failure.
    * It's cheap enough to do and means the dev. won't have to do multiple
    * passes to find every error.
    *****/

   /*******************************************
    * CFBundleIdentifier: string, length < 64 *
    *******************************************/

    propKey = kCFBundleIdentifierKey;
    checkResult = __OSKextCheckProperty(aKext,
        aKext->infoDictionary,
        /* propKey */ propKey,
        /* diagnosticValue */ propKey,
        /* expectedType */ CFStringGetTypeID(),
        /* legalValues */ NULL,
        /* required */ true,
        /* typeRequired */ true,
        /* nonnilRequired */ true,
        /* valueOut */ (CFTypeRef *)&stringValue,
        /* valueIsNonnil */ NULL);
    result = result && checkResult;
    if (checkResult && stringValue) {
        SAFE_RELEASE_NULL(aKext->bundleID);
        if (stringValue) {
            aKext->bundleID = CFRetain(stringValue);
        }
        if (CFStringGetLength(stringValue) > KMOD_MAX_NAME - 1) {
            __OSKextSetDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
                kOSKextDiagnosticIdentifierOrVersionTooLongKey);
            result = false;
        }
    } else {
        aKext->bundleID = CFSTR(__kOSKextUnknownIdentifier);
    }

   /*************************************
    * CFBundleVersion: string, parsable *
    *************************************/

    propKey = kCFBundleVersionKey;
    checkResult = __OSKextCheckProperty(aKext,
        aKext->infoDictionary,
        /* propKey */ propKey,
        /* diagnosticValue */ propKey,
        /* expectedType */ CFStringGetTypeID(),
        /* legalValues */ NULL,
        /* required */ true,
        /* typeRequired */ true,
        /* nonnilRequired */ false,   // we catch it on the parse
        /* valueOut */ (CFTypeRef *)&stringValue,
        /* valueIsNonnil */ NULL);
    result = result && checkResult;
    if (checkResult && stringValue) {
        if (CFStringGetLength(stringValue) > KMOD_MAX_NAME - 1) {
            __OSKextSetDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
                kOSKextDiagnosticIdentifierOrVersionTooLongKey);
            result = false;
        } else {
            bundleVersion = OSKextParseVersionCFString(stringValue);
            if (bundleVersion == -1) {
            __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
                kOSKextDiagnosticPropertyIsIllegalValueKey, kCFBundleVersionKey,
                /* note */ NULL);
                result = false;
            }
            aKext->version = bundleVersion;
        }
    }

   /*******************************************************************
    * OSBundleCompatibleVersion: string, parsable, <= CFBundleVersion *
    *******************************************************************/

    propKey = CFSTR(kOSBundleCompatibleVersionKey);
    checkResult = __OSKextCheckProperty(aKext,
        aKext->infoDictionary,
        /* propKey */ propKey,
        /* diagnosticValue */ propKey,
        /* expectedType */ CFStringGetTypeID(),
        /* legalValues */ NULL,
        /* required */ false,
        /* typeRequired */ true,
        /* nonnilRequired */ false,  // we catch it on the parse
        /* valueOut */ (CFTypeRef *)&stringValue,
        /* valueIsNonnil */ NULL);
    result = result && checkResult;
    if (checkResult && stringValue) {
        compatibleVersion = OSKextParseVersionCFString(stringValue);
        if (compatibleVersion == -1) {
            result = false;
        }
        aKext->compatibleVersion = compatibleVersion;

        if (compatibleVersion > bundleVersion) {
            __OSKextSetDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
                kOSKextDiagnosticCompatibleVersionLaterThanVersionKey);
            result = false;
        }
    }

   /********************************
    * OSBundleIsInterface: Boolean *
    ********************************/

   /* Check whether the kext is an interface before checking whether it's
    * a kernel component. If the kext is a kernel component, it's implicitly
    * an interface too, so we're not going to require OSBundleIsInterface
    * be set for them.
    */
    propKey = CFSTR(kOSBundleIsInterfaceKey);
    checkResult = __OSKextCheckProperty(aKext,
        aKext->infoDictionary,
        /* propKey */ propKey,
        /* diagnosticValue */ propKey,
        /* expectedTypes */ CFBooleanGetTypeID(),
        /* legalValues */ NULL,
        /* required */ false,
        /* typeRequired */ true,
        /* nonnilRequired */ false,
        /* valueOut */ (CFTypeRef *)&boolValue,
        &valueIsNonnil);
    result = result && checkResult;
    if (valueIsNonnil) {
        aKext->flags.isInterface = 1;
    }

   /* However, if it is set, and set to false, that's a big no-no for a
    * kernel component.
    */
    if (boolValue) {
        isInterfaceSetFalse = !CFBooleanGetValue(boolValue);
    }

   /**************************************
    * OSBundleIsKernelComponent: Boolean *
    **************************************/

    propKey = CFSTR(kOSKernelResourceKey);
    checkResult = __OSKextCheckProperty(aKext,
        aKext->infoDictionary,
        /* propKey */ propKey,
        /* diagnosticValue */ propKey,
        /* expectedType */ CFBooleanGetTypeID(),
        /* legalValues */ NULL,
        /* required */false,
        /* typeRequired */ true,
        /* nonnilRequired */ false,
        /* valueOut */ NULL,
        &valueIsNonnil);
    result = result && checkResult;
    if (valueIsNonnil) {
        aKext->flags.isKernelComponent = 1;
    }

    if (aKext->flags.isKernelComponent) {
        if (isInterfaceSetFalse) {
            __OSKextSetDiagnostic(aKext, kOSKextDiagnosticsFlagWarnings,
                kOSKextDiagnosticKernelComponentNotInterfaceKey);
        }
        aKext->flags.isInterface = 1;
    }

   /******************************
    * CFBundleExecutable: string *
    ******************************/

    propKey = kCFBundleExecutableKey;
    checkResult = __OSKextCheckProperty(aKext,
        aKext->infoDictionary,
        /* propKey */ propKey,
        /* diagnosticValue */ propKey,
        /* expectedType */ CFStringGetTypeID(),
        /* legalValues */ NULL,
        /* required */ false,
        /* typeRequired */ true,
        /* nonnilRequired */ true,  // xxx - ok for empty prop here?
        /* valueOut */ NULL,
        &valueIsNonnil);
    result = result && checkResult;
    if (valueIsNonnil) {
        aKext->flags.declaresExecutable = 1;
    }

#if SHARED_EXECUTABLE
    propKey = CFSTR(kOSBundleSharedExecutableIdentifierKey);
    checkResult = __OSKextCheckProperty(aKext,
        aKext->infoDictionary,
        /* propKey */ propKey,
        /* diagnosticValue */ propKey,
        /* expectedType */ CFStringGetTypeID(),
        /* legalValues */ NULL,
        /* required */ false,
        /* typeRequired */ true,
        /* nonnilRequired */ false,  // xxx - ok for empty prop here?
        /* valueOut */ NULL,
        &valueIsNonnil);
    result = result && checkResult;
   /* Can't have both executable and shared! */
    if (aKext->flags.declaresExecutable) {
        if (valueIsNonnil) {
            __OSKextSetDiagnostic(aKext, kOSKextDiagnosticsFlagValidation,
                kOSKextDiagnosticSharedExecutableAndExecutableKey);
        }
    } else if (valueIsNonnil) {
        aKext->flags.declaresExecutable = 1;
    }
#endif /* SHARED_EXECUBTABLE */

   /**************************************
    * OSBundleEnableKextLogging: boolean *
    *************************************/

    propKey = CFSTR(kOSBundleEnableKextLoggingKey);
    checkResult = __OSKextCheckProperty(aKext,
        aKext->infoDictionary,
        /* propKey */ propKey,
        /* diagnosticValue */ propKey,
        /* expectedType */ CFBooleanGetTypeID(),
        /* legalValues */ NULL,
        /* required */ false,
        /* typeRequired */ true,
        /* nonnilRequired */ false,
        /* propKey */ (CFTypeRef *)&boolValue,
        &valueIsNonnil);
    result = result && checkResult;

    if (valueIsNonnil) {
        aKext->flags.loggingEnabled = CFBooleanGetValue(boolValue) ? 1 : 0;
        aKext->flags.plistHasEnableLoggingSet = CFBooleanGetValue(boolValue) ? 1 : 0;
    }

   /*****************************************
    * OSBundleDebugLevelKey: DEPRECATED (?) *
    *****************************************/

    propKey = CFSTR(kOSBundleDebugLevelKey);
    debugLevel = OSKextGetValueForInfoDictionaryKey(aKext, propKey);
    if (debugLevel) {
        __OSKextAddDiagnostic(aKext, kOSKextDiagnosticsFlagWarnings,
            kOSKextDiagnosticDeprecatedPropertyKey, CFSTR(kOSBundleDebugLevelKey),
            /* note */ NULL);
    }

   /*****************************************
    * OSBundleRequired: string, legal value *
    *****************************************/

   /* Any legal value for OSBundleRequired means kext is safe-boot
    * loadable.
    */
    propKey = CFSTR(kOSBundleRequiredKey);
    checkResult = __OSKextCheckProperty(aKext,
        aKext->infoDictionary,
        /* propKey */ propKey,
        /* diagnosticValue */ propKey,
        /* expectedType */ CFStringGetTypeID(),
        /* legalValues */ __sOSKextOSBundleRequiredValues,
        /* required */ false,
        /* typeRequired */ true,
        /* nonnilRequired */ false,  // required values given
        /* valueOut */ NULL,
        &valueIsNonnil);
    result = result && checkResult;
    if (valueIsNonnil) {
        aKext->flags.isLoadableInSafeBoot = 1;
    } else {
        if (OSKextGetActualSafeBoot() || OSKextGetSimulatedSafeBoot()) {
            __OSKextSetDiagnostic(aKext, kOSKextDiagnosticsFlagBootLevel,
                kOSKextDiagnosticIneligibleInSafeBoot);
        }
    }

   /***********************************************
    * IOKitPersonalities: dictionary              *
    * Further validation done in OSKextValidate() *
    ***********************************************/

    propKey = CFSTR(kIOKitPersonalitiesKey);
    checkResult = __OSKextCheckProperty(aKext,
        aKext->infoDictionary,
        /* propKey */ propKey,
        /* diagnosticValue */ propKey,
        /* expectedType */ CFDictionaryGetTypeID(),
        /* legalValues */ NULL,
        /* required */ false,
        /* typeRequired */ true,
        /* nonnilRequired */ false,  // ok to have empty dict fr. template
        /* valueOut */ (CFTypeRef *)&dictValue,
        &valueIsNonnil);
    result = result && checkResult;
    if (dictValue) {
        __OSKextValidateIOKitPersonalityContext validatePersonalitiesContext;

       /* We also need to check for an IOKitDebug property in any personality.
        * propPath is needed for the applier function call.
        */
        propPath = CFArrayCreateMutable(CFGetAllocator(aKext), 0,
            &kCFTypeArrayCallBacks);
        if (!propPath) {
            OSKextLogMemError();
            result = false;
            goto finish;
        }
        CFArrayAppendValue(propPath, propKey);

        validatePersonalitiesContext.kext = aKext;
        validatePersonalitiesContext.personalities = dictValue;
        validatePersonalitiesContext.propPath = propPath;
        validatePersonalitiesContext.valid = true;  // starts true!
        validatePersonalitiesContext.justCheckingIOKitDebug = true;
        CFDictionaryApplyFunction(dictValue,
            __OSKextValidateIOKitPersonalityApplierFunction,
            &validatePersonalitiesContext);
        result = result && validatePersonalitiesContext.valid;

        CFArrayRemoveValueAtIndex(propPath, CFArrayGetCount(propPath) - 1);
    }

finish:
   /* If we know the kext is invalid now, mark it. We can't say it's
    * valid yet without doing other, more expensive checks.
    */
    if (!result) {
        aKext->flags.invalid = 1;
        aKext->flags.valid = 0;
    }

   /* Add the kext (back) to the lookup dictionary. Its identifier or
    * version may have changed.
    * xxx - we should catch a failure to insert in identifier dict
    * xxx - but ultimately there isn't much we can do.
    */
    (void)__OSKextRecordKextInIdentifierDict(aKext, __sOSKextsByIdentifier);

    SAFE_RELEASE(propPath);

    return result;
}

#pragma mark Mkext and Prelinked Kernel Files
/*********************************************************************
*********************************************************************/
Boolean OSKextIsFromMkext(OSKextRef aKext)
{
    return aKext->staticFlags.isFromMkext ? true : false;
}

/*********************************************************************
*********************************************************************/
#define REQUIRED_MATCH(flags, string, type)  \
    (((flags) & kOSKextOSBundleRequired ## type ## Flag) && \
    string && CFEqual(string, CFSTR(kOSBundleRequired ## type)))

Boolean OSKextMatchesRequiredFlags(OSKextRef aKext,
    OSKextRequiredFlags requiredFlags)
{
    Boolean     result         = false;
    CFStringRef requiredString = NULL;  // do not release

    requiredString = OSKextGetValueForInfoDictionaryKey(aKext,
        CFSTR(kOSBundleRequiredKey));

    if (REQUIRED_MATCH(requiredFlags, requiredString, Root) ||
        REQUIRED_MATCH(requiredFlags, requiredString, LocalRoot) ||
        REQUIRED_MATCH(requiredFlags, requiredString, NetworkRoot) ||
        REQUIRED_MATCH(requiredFlags, requiredString, Console) ||
        REQUIRED_MATCH(requiredFlags, requiredString, SafeBoot)) {

        result = true;
    } else if (!requiredFlags) {
        result = true;
    }

    return result;
}

/*********************************************************************
*********************************************************************/
CFArrayRef OSKextFilterRequiredKexts(
    CFArrayRef          kextArray,
    OSKextRequiredFlags requiredFlags)
{
    CFMutableArrayRef result = NULL;
    CFIndex count, i;

    result = CFArrayCreateMutable(CFGetAllocator(kextArray), 0,
        &kCFTypeArrayCallBacks);
    if (!result) {
        OSKextLogMemError();
        goto finish;
    }

    if (!kextArray) {
        kextArray = OSKextGetAllKexts();
    }

    count = CFArrayGetCount(kextArray);
    for (i = 0; i < count; i++) {
        OSKextRef thisKext = (OSKextRef)CFArrayGetValueAtIndex(kextArray, i);
        if (OSKextMatchesRequiredFlags(thisKext, requiredFlags)) {
            CFArrayAppendValue(result, thisKext);
        }
    }

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
#define GZIP_WINDOW_OFFSET (16)

Boolean __OSKextAddCompressedFileToMkext(
    OSKextRef        aKext,
    CFMutableDataRef mkextData,
    CFDataRef        fileData,
    Boolean          plistFlag,
    Boolean        * compressed)
{
    Boolean             result           = false;
    const UInt8       * fileBuffer       = CFDataGetBytePtr(fileData);
    UInt8             * mkextBuffer;
    mkext2_header     * mkextHeader;
    CFIndex             mkextStartLength = CFDataGetLength(mkextData);
    uint32_t            fullSize         = CFDataGetLength(fileData);
    unsigned long       compressedSize;
    uint32_t            compressedSize32;
    mkext2_file_entry * entryPtr         = NULL;
    int                 zlib_result;
    uint32_t            entrySize;
    UInt8  *            compressDest;
    z_stream            zstream;
    Boolean             zstream_inited   = false;

    *compressed = false;

   /* zlib doc says compression buffer must be source len + 0.1% + 12 bytes,
    * so start with that much.
    */
    compressedSize = fullSize + ((fullSize+1000)/1000) + 12;

    if (plistFlag) {
        entrySize = 0;
    } else {
        entrySize = sizeof(mkext2_file_entry);
    }

    CFDataSetLength(mkextData,
        mkextStartLength + entrySize + compressedSize);
    mkextBuffer = CFDataGetMutableBytePtr(mkextData);

    if (plistFlag) {
        compressDest = mkextBuffer + mkextStartLength;
    } else {
        entryPtr = (mkext2_file_entry *)(mkextBuffer + mkextStartLength);
        entryPtr->full_size = OSSwapHostToBigInt32(fullSize);
        compressDest = entryPtr->data;
    }

    zstream.next_in   = (UInt8 *)fileBuffer;
    zstream.next_out  = compressDest;
    zstream.avail_in  = fullSize;
    zstream.avail_out = compressedSize;
    zstream.zalloc    = Z_NULL;
    zstream.zfree     = Z_NULL;
    zstream.opaque    = Z_NULL;

    
    zlib_result = deflateInit2(&zstream, Z_DEFAULT_COMPRESSION,  Z_DEFLATED,
        15, 8 /* memLevel */, Z_DEFAULT_STRATEGY);
    if (Z_OK != zlib_result) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "zlib deflateInit failed.");
        goto finish;
    } else {
        zstream_inited = true;
    }

    zlib_result = deflate(&zstream, Z_FINISH);
    if (zlib_result == Z_STREAM_END) {
        compressedSize = zstream.total_out;
    } else if (zlib_result == Z_OK) {
       /* deflate filled output buffer, meaning the data doesn't compress.
        */
        compressedSize = zstream.total_out;
    } else {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "zlib deflate failed.");
        goto finish;
    }

    zlib_result = Z_OK;

    // xxx - check for truncation?
    compressedSize32 = (uint32_t)compressedSize;

   /* Only accept the compression if it actually shrinks the file.
    */
    if (Z_OK == zlib_result) {
        result = true;
        if (compressedSize32 >= fullSize) {
            *compressed = false;
        } else {
            *compressed = true;
            if (plistFlag) {
                mkextHeader = (mkext2_header *)mkextBuffer;
                mkextHeader->plist_offset =
                    OSSwapHostToBigInt32(mkextStartLength);
                mkextHeader->plist_full_size =
                    OSSwapHostToBigInt32(CFDataGetLength(fileData));
                mkextHeader->plist_compressed_size =
                    OSSwapHostToBigInt32(compressedSize32);
                OSKextLog(aKext, kOSKextLogDetailLevel | kOSKextLogArchiveFlag,
                    "Compressed info dict from %u to %u bytes (%.2f%%).",
                    fullSize, compressedSize32,
                    (100.0 * (float)compressedSize32/(float)fullSize));
            } else {
                entryPtr->compressed_size = OSSwapHostToBigInt32(compressedSize32);
                OSKextLog(aKext, kOSKextLogDetailLevel | kOSKextLogArchiveFlag,
                    "Compressed executable from %u to %u bytes (%.2f%%).",
                    fullSize, compressedSize32,
                    (100.0 * (float)compressedSize32/(float)fullSize));
            }
           /* Trim off the extra room left for the compress() call.
            */
            CFDataSetLength(mkextData,
                mkextStartLength + entrySize + compressedSize32);
        }
    }

finish:
   /* Don't bother checking return, nothing we can do on fail.
    */
    if (zstream_inited) deflateEnd(&zstream);

   /* If error, revert the mkext data to its original length
    * so we don't have dead space in it.
    */
    if (result != true) {
        CFDataSetLength(mkextData, mkextStartLength);
    }
    return result;
}

/*********************************************************************
* Need to distinguish if we're generating mkext for a kernel load or
* just to make an mkext!
*********************************************************************/
Boolean __OSKextAddToMkext(
    OSKextRef         aKext,
    CFMutableDataRef  mkextData,
    CFMutableArrayRef mkextInfoDictArray,
    char            * volumePath,
    Boolean           compressFlag)
{
    Boolean                result                 = false;
    CFMutableDictionaryRef infoDictionary         = NULL;   // must release
    CFStringRef            bundlePath             = NULL;   // must release
    CFStringRef            executableRelPath       = NULL;  // must release
    char                   kextPath[PATH_MAX];
    char                 * kextVolPath = kextPath;
    CFDataRef              executable             = NULL;   // must release
    CFIndex                mkextDataStartLength   = CFDataGetLength(mkextData);
    uint32_t               mkextEntryOffset;
    CFNumberRef            mkextEntryOffsetNum    = NULL;   // must release
    mkext2_file_entry      entryScratch;
    void                 * mkextEntryFile         = NULL;   // need to free if compressed
    Boolean                compressed             = false;  // true if successfully compressed

    __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
        /* resolveToBase */ true, kextPath);

    OSKextLog(aKext, kOSKextLogStepLevel | kOSKextLogArchiveFlag,
        "Adding %s to mkext.", kextPath);

    infoDictionary = OSKextCopyInfoDictionary(aKext);
    if (!infoDictionary) {
        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
            "Can't get info dictionary for %s.", kextPath);
        goto finish;
    }

   /* If the kext has logging enabled, whether originally in the plist
    * or via run-time setting, set the plist property.
    */
    if (aKext->flags.loggingEnabled) {
        CFDictionarySetValue(infoDictionary, CFSTR(kOSBundleEnableKextLoggingKey),
            kCFBooleanTrue);
    }

    // xxx - need to validate

    // xxx - this duplicates shared executables in the mkext
    executable = OSKextCopyExecutableForArchitecture(aKext, OSKextGetArchitecture());
    if (!executable && OSKextDeclaresExecutable(aKext)) {
        OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
            "Can't get executable for %s (architecture %s).", kextPath,
            OSKextGetArchitecture()->name);
        goto finish;
    }

    if (executable) {
        uint32_t entryFileSize;

       /* Advise the system that we're reading the executable sequentially.
        */
        (void)posix_madvise((void *)CFDataGetBytePtr(executable),
            CFDataGetLength(executable),
            POSIX_MADV_SEQUENTIAL);

        mkextEntryOffset = mkextDataStartLength;
        mkextEntryOffsetNum = CFNumberCreate(CFGetAllocator(aKext),
            kCFNumberSInt32Type, &mkextEntryOffset);
        if (!mkextEntryOffsetNum) {
            OSKextLogMemError();
            goto finish;
        }
        CFDictionarySetValue(infoDictionary, CFSTR(kMKEXTExecutableKey),
            mkextEntryOffsetNum);

        entryFileSize = CFDataGetLength(executable);
        entryScratch.full_size = OSSwapHostToBigInt32(entryFileSize);

        if (compressFlag && (!__OSKextAddCompressedFileToMkext(aKext,
                mkextData, executable, /* plist */ false, &compressed))) {

            OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
                "%s failed to compress executable.", kextPath);
            goto finish;
        }
        if (!compressed) {
            mkextEntryFile = (void *)CFDataGetBytePtr(executable);
            entryScratch.compressed_size = OSSwapHostToBigInt32(0);
            CFDataAppendBytes(mkextData, (const UInt8 *)&entryScratch,
                sizeof(entryScratch));
            CFDataAppendBytes(mkextData, (const UInt8 *)mkextEntryFile,
                entryFileSize);
        }
        // xxx - name file in log msg
        OSKextLog(aKext, kOSKextLogDetailLevel | kOSKextLogArchiveFlag,
            "%s added %u-byte %scompressed executable to mkext.",
            kextPath, entryFileSize, compressFlag ? "" : "non");
    }

    if (!__OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
        /* resolveToBase */ true, kextPath)) {
     
        OSKextLogStringError(aKext);
        goto finish;
    }

    kextVolPath = __absPathOnVolume(kextPath, volumePath);

    bundlePath = CFStringCreateWithBytes(CFGetAllocator(aKext),
        (UInt8 *)kextVolPath, strlen(kextVolPath),
        kCFStringEncodingUTF8, false);
    if (!bundlePath) {
        OSKextLogMemError();
        goto finish;
    }
    CFDictionarySetValue(infoDictionary, CFSTR(kMKEXTBundlePathKey),
        bundlePath);

    executableRelPath = __OSKextCopyExecutableRelativePath(aKext);
    if (executableRelPath) {
        CFDictionarySetValue(infoDictionary, CFSTR(kMKEXTExecutableRelativePathKey),
            executableRelPath);
    }

    CFArrayAppendValue(mkextInfoDictArray, infoDictionary);

    result = true;

finish:
    if (!result) {
        CFDataSetLength(mkextData, mkextDataStartLength);
    }

   /* Advise the system that we no longer need the mmapped executable.
    */
    if (executable) {
        (void)posix_madvise((void *)CFDataGetBytePtr(executable),
            CFDataGetLength(executable),
            POSIX_MADV_DONTNEED);
    }

    SAFE_RELEASE(infoDictionary);
    SAFE_RELEASE(bundlePath);
    SAFE_RELEASE(executableRelPath);
    SAFE_RELEASE(executable);
    SAFE_RELEASE(mkextEntryOffsetNum);
    if (compressed) {
        SAFE_FREE(mkextEntryFile);
    }
    return result;
}

// xxx - need to move this to mkext.c file
__private_extern__ u_int32_t
mkext_adler32(u_int8_t *buffer, int32_t length)
{
    int32_t cnt;
    u_int32_t  result, lowHalf, highHalf;

    lowHalf = 1;
    highHalf = 0;

    for (cnt = 0; cnt < length; cnt++) {
        if ((cnt % 5000) == 0) {
            lowHalf  %= 65521L;
            highHalf %= 65521L;
        }

        lowHalf += buffer[cnt];
        highHalf += lowHalf;
    }

    lowHalf  %= 65521L;
    highHalf %= 65521L;

    result = (highHalf << 16) | lowHalf;

    return result;
}

/*********************************************************************
*********************************************************************/
CFDataRef __OSKextCreateMkext(
    CFAllocatorRef      allocator,
    CFArrayRef          kextArray,
    CFURLRef            volumeRootURL,
    OSKextRequiredFlags requiredFlags,
    Boolean             compressFlag,
    Boolean             skipLoadedFlag,
    CFDictionaryRef     loadArgsDict)
{
    CFMutableDataRef         result             = NULL;
    CFMutableDataRef         mkextData          = NULL;  // must release
    mkext2_header            mkextHeaderScratch;
    mkext2_header          * mkextHeader;
    void                   * mkextEnd;
    CFMutableDictionaryRef   mkextPlist         = NULL;  // must release
    CFMutableArrayRef        mkextInfoDictArray = NULL;  // must release
    CFDataRef                mkextPlistData     = NULL;  // must release
    Boolean                  compressed         = false; // true if successfully compressed
    uint32_t                 adlerChecksum;
    char                     kextPath[PATH_MAX];
    char                     volumePath[PATH_MAX] = "";
    CFIndex                  count, i, numKexts;

    if (!kextArray) {
        kextArray = OSKextGetAllKexts();
    }
    count = CFArrayGetCount(kextArray);
    if (!count) {
        goto finish;
    }

    mkextData = CFDataCreateMutable(allocator, 0);
    if (!mkextData) {
        OSKextLogMemError();
        goto finish;
    }

    mkextPlist = CFDictionaryCreateMutable(allocator, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!mkextPlist) {
        OSKextLogMemError();
        goto finish;
    }

    mkextInfoDictArray = CFArrayCreateMutable(allocator, 0,
        &kCFTypeArrayCallBacks);
    if (!mkextInfoDictArray) {
        OSKextLogMemError();
        goto finish;
    }

    if (volumeRootURL) {
        if (!CFURLGetFileSystemRepresentation(volumeRootURL,
            /* resolveToBase */ TRUE, (UInt8 *)volumePath, sizeof(volumePath))) {

            OSKextLogStringError(/* kext */ NULL);
            goto finish;
        }
    }

    CFDictionarySetValue(mkextPlist, CFSTR(kMKEXTInfoDictionariesKey),
        mkextInfoDictArray);
    if (loadArgsDict) {
        CFDictionarySetValue(mkextPlist, CFSTR(kKextRequestPredicateKey),
            CFSTR(kKextRequestPredicateLoad));
        CFDictionarySetValue(mkextPlist, CFSTR(kKextRequestArgumentsKey),
            loadArgsDict);
    }

//printPList_new(stderr, mkextPlist, kPListStyleClassic);

    CFDataAppendBytes(mkextData, (const UInt8 *)&mkextHeaderScratch,
        sizeof(mkextHeaderScratch));

    for (i = 0, numKexts = 0; i < count; i++) {
        OSKextRef thisKext = (OSKextRef)CFArrayGetValueAtIndex(kextArray, i);

        __OSKextGetFileSystemPath(thisKext, /* otherURL */ NULL,
            /* resolveToBase */ false, kextPath);

        if (!__OSKextIsValid(thisKext)) {
            OSKextLog(thisKext,
                kOSKextLogStepLevel | kOSKextLogArchiveFlag,
                 "%s is not valid; omitting from mkext.",
                 kextPath);
            continue;
        }

        if (skipLoadedFlag && OSKextIsLoaded(thisKext)) {
            OSKextLog(thisKext, kOSKextLogDebugLevel | kOSKextLogArchiveFlag,
                 "Omitting loaded kext %s from mkext for kernel load.",
                 kextPath);
            continue;
        }

        if (OSKextMatchesRequiredFlags(thisKext, requiredFlags)) {
            if (OSKextSupportsArchitecture(thisKext, NULL)) {
                if (!__OSKextAddToMkext(thisKext, mkextData,
                    mkextInfoDictArray, volumePath, compressFlag)) {

                    goto finish;
                }
                numKexts++;
            } else {
                OSKextLog(thisKext, kOSKextLogStepLevel | kOSKextLogArchiveFlag,
                     "%s does not contain code for architecture %s.",
                     kextPath, OSKextGetArchitecture()->name);
            }
        }
    }

   /* The mkext v2 format requires all XML buffers to be nul-terminated.
    * Fortunately IOCFSerialize does just that.
    */
    mkextPlistData = IOCFSerialize(mkextPlist, kNilOptions);
    if (!mkextPlistData) {
        goto finish;
    }

    compressed = false;
    if (compressFlag && !__OSKextAddCompressedFileToMkext(/* kext */ NULL,
            mkextData, mkextPlistData, /* plist */ true, &compressed)) {

        goto finish;
    }
    if (!compressed) {
        mkextHeader = (mkext2_header *)CFDataGetMutableBytePtr(mkextData);
        mkextHeader->plist_offset =
            OSSwapHostToBigInt32(CFDataGetLength(mkextData));
        mkextHeader->plist_compressed_size =
            OSSwapHostToBigInt32(0);
        mkextHeader->plist_full_size =
            OSSwapHostToBigInt32(CFDataGetLength(mkextPlistData));
        CFDataAppendBytes(mkextData, CFDataGetBytePtr(mkextPlistData),
            CFDataGetLength(mkextPlistData));
    }

    mkextHeader = (mkext2_header *)CFDataGetMutableBytePtr(mkextData);
    mkextHeader->magic = OSSwapHostToBigInt32(MKEXT_MAGIC);
    mkextHeader->signature = OSSwapHostToBigInt32(MKEXT_SIGN);
    mkextHeader->length = OSSwapHostToBigInt32(CFDataGetLength(mkextData));

    mkextHeader->version = OSSwapHostToBigInt32(MKEXT_VERS_2);
    mkextHeader->numkexts = OSSwapHostToBigInt32(numKexts);
    mkextHeader->cputype = OSSwapHostToBigInt32(OSKextGetArchitecture()->cputype);
    mkextHeader->cpusubtype = OSSwapHostToBigInt32(OSKextGetArchitecture()->cpusubtype);

    mkextEnd = ((void *)mkextHeader + CFDataGetLength(mkextData));
    adlerChecksum = mkext_adler32((uint8_t *)&(mkextHeader->version),
        mkextEnd - (void *)&(mkextHeader->version));
    mkextHeader->adler32 = OSSwapHostToBigInt32(adlerChecksum);

    result = mkextData;
    CFRetain(result);

    OSKextLog(/* kext */ NULL, kOSKextLogProgressLevel | kOSKextLogArchiveFlag,
        "Created mkext for architecture %s containing %u kexts.",
        __sOSKextArchInfo->name, (int)numKexts);

finish:
    SAFE_RELEASE(mkextInfoDictArray);
    SAFE_RELEASE(mkextPlist);
    SAFE_RELEASE(mkextData);
    SAFE_RELEASE(mkextPlistData);
    return result;
}

/*********************************************************************
*********************************************************************/
CFDataRef OSKextCreateMkext(
    CFAllocatorRef      allocator,
    CFArrayRef          kextArray,
    CFURLRef            volumeRootURL,
    OSKextRequiredFlags requiredFlags,
    Boolean             compressFlag)
{
    return __OSKextCreateMkext(allocator,
        kextArray, volumeRootURL, requiredFlags,
        compressFlag, /* skipLoaded */ false, /* loadArgsDict */ NULL);
}

/*********************************************************************
*********************************************************************/
CFDataRef __OSKextUncompressMkext2FileData(
    CFAllocatorRef   allocator,
    const UInt8    * buffer,
    uint32_t        compressedSize,
    uint32_t        fullSize)
{
    CFDataRef  result            = NULL;
    CFDataRef  createdData       = NULL; // release on error
    uint32_t   uncompressedSize;
    uint8_t  * uncompressedData  = NULL;   // free on error
    int        zlib_result;
    z_stream   zstream;
    Boolean    zstream_inited    = false;

    if (!compressedSize) {
        createdData = CFDataCreate(allocator, buffer, fullSize);
        if (createdData) {
            UInt8 * dataBuffer = (UInt8 *)CFDataGetBytePtr(createdData);
            if (!dataBuffer) {
                goto finish;
            }
            result = createdData;
        }
    }

   /* Add 1 for a terminating nul byte for plist XML.
    */
    uncompressedData = (void *)malloc(fullSize);
    if (!uncompressedData) {
        OSKextLogMemError();
        goto finish;
    }

    zstream.next_in   = (UInt8 *)buffer;
    zstream.next_out  = uncompressedData;
    zstream.avail_in  = compressedSize;
    zstream.avail_out = fullSize;
    zstream.zalloc    = Z_NULL;
    zstream.zfree     = Z_NULL;
    zstream.opaque    = Z_NULL;

    zlib_result = inflateInit(&zstream);
    if (Z_OK != zlib_result) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "zlib inflateInit failed.");
        goto finish;
    } else {
        zstream_inited = true;
    }

    zlib_result = inflate(&zstream, Z_FINISH);
    if (zlib_result == Z_STREAM_END) {
        uncompressedSize = zstream.total_out;
    } else if (zlib_result == Z_OK) {
        // xxx - will we even see this result?
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "zlib inflate discrepancy, uncompressed size != original size.");
        goto finish;
    } else {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "zlib inflate failed: %s.",
            zstream.msg ? zstream.msg : "unknown");
        goto finish;
    }

    if (uncompressedSize != fullSize) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "zlib inflate discrepancy, uncompressed size != original size.");
        goto finish;
    }

    result = CFDataCreateWithBytesNoCopy(allocator,
        uncompressedData, fullSize, kCFAllocatorMalloc);
    if (!result) {
        OSKextLogMemError();
    }

finish:
   /* Don't bother checking return, nothing we can do on fail.
    */
    if (zstream_inited) inflateEnd(&zstream);

    if (!result) {
        SAFE_FREE(uncompressedData);
        SAFE_RELEASE(createdData);
    }
    return result;
}

/*********************************************************************
*********************************************************************/
CFArrayRef OSKextCreateKextsFromMkextFile(CFAllocatorRef allocator,
    CFURLRef  anURL)
{
    CFArrayRef result = NULL;
    CFDataRef  mkextData = NULL;   // must release

    pthread_once(&__sOSKextInitialized, __OSKextInitialize);

    if (!CFURLCreateDataAndPropertiesFromResource(allocator, anURL,
        &mkextData, /* properties */NULL, /* desiredProperties */NULL,
        /* errorCode */ NULL)) {

        OSKextLogMemError();
        goto finish;
    }

    result = __OSKextCreateKextsFromMkext(allocator, mkextData, anURL);
    if (!result) {
        goto finish;
    }

finish:
    SAFE_RELEASE(mkextData);
    return result;
}

/*********************************************************************
*********************************************************************/
CFArrayRef OSKextCreateKextsFromMkextData(CFAllocatorRef allocator,
    CFDataRef mkextData)
{
    pthread_once(&__sOSKextInitialized, __OSKextInitialize);

    return __OSKextCreateKextsFromMkext(allocator, mkextData, NULL);
}

/*********************************************************************
*********************************************************************/
CFArrayRef __OSKextCreateKextsFromMkext(
    CFAllocatorRef allocator,
    CFDataRef mkextData,
    CFURLRef  mkextURL)
{
    CFMutableArrayRef   result                     = NULL;
    CFMutableArrayRef   kexts                      = NULL;  // must release
    uint32_t            magic;
    fat_iterator        fatIterator                = NULL;  // must fat_iterator_close()
    mkext2_header     * mkextHeader;
    void              * mkextEnd;
    CFDictionaryRef     mkextPlist                 = NULL;  // must release
    CFArrayRef          mkextInfoDictArray         = NULL;  // do not release
    uint32_t            adlerChecksum;
    uint32_t            mkextPlistOffset;
    uint32_t            mkextPlistCompressedSize;
    uint32_t            mkextPlistFullSize;
    CFStringRef         errorString                = NULL;  // must release
    char              * errorCString               = NULL;  // must free
    CFDataRef           mkextPlistUncompressedData = NULL;  // must release
    const char        * mkextPlistDataBuffer       = NULL;  // do not free
    CFIndex             count, i;

   /* Initialize lazy runtime data.
    */
    pthread_once(&__sOSKextInitialized, __OSKextInitialize);

    kexts = CFArrayCreateMutable(allocator, 0,
        &kCFTypeArrayCallBacks);
    if (!kexts) {
        OSKextLogMemError();
        goto finish;
    }

    magic = MAGIC32(CFDataGetBytePtr(mkextData));
    if (ISFAT(magic)) {
        fatIterator = fat_iterator_for_data(CFDataGetBytePtr(mkextData),
            CFDataGetBytePtr(mkextData) + CFDataGetLength(mkextData),
            1 /* mach-o only */);
        if (!fatIterator) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
                "Can't read mkext fat header.");
            goto finish;
        }
        mkextHeader = fat_iterator_find_arch(fatIterator,
            OSKextGetArchitecture()->cputype,
            OSKextGetArchitecture()->cpusubtype,
            &mkextEnd);
        if (!mkextHeader) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
                "Architecture %s not found in mkext.",
                OSKextGetArchitecture()->name);
            goto finish;
        }
    } else {
        mkextHeader = (mkext2_header *)CFDataGetBytePtr(mkextData);
        mkextEnd = (char *)mkextHeader + CFDataGetLength(mkextData);
    }

    if ((MKEXT_GET_MAGIC(mkextHeader) != MKEXT_MAGIC) ||
        (MKEXT_GET_SIGNATURE(mkextHeader) != MKEXT_SIGN)) {

        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
            "Bad mkext magic/signature.");
        goto finish;
    }
    if ((int32_t)OSSwapBigToHostInt32(mkextHeader->length) !=
        ((char *)mkextEnd - (char *)mkextHeader)) {

        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
            "Mkext length field %d does not match mkext actual size %d.",
            OSSwapBigToHostInt32(mkextHeader->length),
            (int)((char *)mkextEnd - (char *)mkextHeader));
        goto finish;
    }

    if ((OSSwapBigToHostInt32(mkextHeader->version) != MKEXT_VERS_2)) {
        OSKextLog(/* kext */ NULL, kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
            "Unsupported mkext version 0x%x.",
            OSSwapBigToHostInt32(mkextHeader->version));
        goto finish;
    }

    mkextEnd = ((void *)mkextHeader + CFDataGetLength(mkextData));
    adlerChecksum = mkext_adler32((uint8_t *)&(mkextHeader->version),
        mkextEnd - (void *)&(mkextHeader->version));
    if (OSSwapBigToHostInt32(mkextHeader->adler32) != adlerChecksum) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
            "Mkext checksum error.");
        goto finish;
    }

    mkextPlistOffset = OSSwapBigToHostInt32(mkextHeader->plist_offset);
    mkextPlistCompressedSize =
        OSSwapBigToHostInt32(mkextHeader->plist_compressed_size);

    OSKextLog(/* kext */ NULL,
        kOSKextLogDebugLevel | kOSKextLogFileAccessFlag,
        "Mkext plist compressed size is %u.", mkextPlistCompressedSize);

    mkextPlistFullSize = OSSwapBigToHostInt32(mkextHeader->plist_full_size);

    OSKextLog(/* kext */ NULL,
        kOSKextLogDebugLevel | kOSKextLogFileAccessFlag,
        "Mkext plist full size is %u.", mkextPlistFullSize);

    if (mkextPlistCompressedSize) {
        mkextPlistUncompressedData = __OSKextUncompressMkext2FileData(
            CFGetAllocator(mkextData),
            (const UInt8 *)mkextHeader + mkextPlistOffset,
            mkextPlistCompressedSize, mkextPlistFullSize);
        if (!mkextPlistUncompressedData) {
            goto finish;
        }
        mkextPlistDataBuffer = (const char *)
            CFDataGetBytePtr(mkextPlistUncompressedData);
    } else {
        mkextPlistDataBuffer = (const char *)mkextHeader + mkextPlistOffset;
    }

   /* IOCFSerialize added a nul byte to the end of the string. Very nice of it.
    */
    mkextPlist = IOCFUnserialize(
        mkextPlistDataBuffer, allocator,
        kNilOptions, &errorString);
    if (!mkextPlist || (CFGetTypeID(mkextPlist) != CFDictionaryGetTypeID())) {
        errorCString = createUTF8CStringForCFString(errorString);
        OSKextLog(/* kext */ NULL, kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
            "Failed to read XML from mkext: %s.",
            errorCString ? errorCString : "(unknown error)");
        goto finish;
    }

    mkextInfoDictArray = (CFArrayRef)CFDictionaryGetValue(
        mkextPlist, CFSTR(kMKEXTInfoDictionariesKey));
    if (!mkextInfoDictArray ||
        (CFGetTypeID(mkextInfoDictArray) != CFArrayGetTypeID())) {

        OSKextLog(/* kext */ NULL, kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
            "Mkext plist has no kexts.");
        goto finish;
    }

    count = CFArrayGetCount(mkextInfoDictArray);
    for (i = 0; i < count; i++) {
        CFDictionaryRef infoDict =
            (CFDictionaryRef)CFArrayGetValueAtIndex(mkextInfoDictArray, i);

        OSKextRef aKext = __OSKextAlloc(allocator, NULL);
        if (!aKext) {
            OSKextLogMemError();
            goto finish;
        }
        if (!__OSKextInitFromMkext(aKext, infoDict, mkextURL, mkextData)) {
            CFRelease(aKext);
            goto finish;
        }
        CFArrayAppendValue(kexts, aKext);
    }

    result = kexts;
    CFRetain(result);

finish:
    SAFE_RELEASE(kexts);
    SAFE_RELEASE(errorString);
    SAFE_RELEASE(mkextPlist);
    SAFE_RELEASE(mkextPlistUncompressedData);

    SAFE_FREE(errorCString);
    if (fatIterator) fat_iterator_close(fatIterator);

    return result;
}

/*********************************************************************
*********************************************************************/
CFDataRef __OSKextExtractMkext2FileEntry(
    OSKextRef   aKext,
    CFDataRef   mkextData,
    CFNumberRef offsetNum,
    CFStringRef filename)  // NULL for executable
{
    CFDataRef           result = NULL;
    const UInt8       * mkext = CFDataGetBytePtr(mkextData);
    uint32_t            entryOffset;
    mkext2_file_entry * fileEntry;
    uint32_t            fullSize;
    uint32_t            compressedSize;
    char              * filenameCString = NULL;  // must free
    char                mkextPath[PATH_MAX] = "";

    if (aKext->mkextInfo->mkextURL) {
        __OSKextGetFileSystemPath(/* kext: mkext, not the kext! */ NULL,
            aKext->mkextInfo->mkextURL,
            /* resolveToBase */ false, mkextPath);
    }

    if (filename) {
        filenameCString = createUTF8CStringForCFString(filename);
    }
    // xxx - check on resource stuff in mkexts
    OSKextLog(aKext,
        kOSKextLogDetailLevel | kOSKextLogArchiveFlag,
        "Extracting %s%s from %s.",
        (filenameCString ? "resource file " : "executable"),
        (filenameCString ? filenameCString : ""),
        (mkextPath[0] ? mkextPath : "mkext data"));

    if (!CFNumberGetValue(offsetNum, kCFNumberSInt32Type,
        (SInt32 *)&entryOffset)) {

        // xxx - log?
        goto finish;
    }

    fileEntry = (mkext2_file_entry *)(mkext + entryOffset);
    fullSize = OSSwapBigToHostInt32(fileEntry->full_size);
    compressedSize = OSSwapBigToHostInt32(fileEntry->compressed_size);
    if (compressedSize) {
        result = __OSKextUncompressMkext2FileData(CFGetAllocator(aKext),
            fileEntry->data,
            compressedSize, fullSize);
        if (!result) {
            OSKextLog(aKext, kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
                "Failed to uncompress %s%s from %s.",
                (filenameCString ? "resource file " : "executable"),
                (filenameCString ? filenameCString : ""),
                (mkextPath[0] ? mkextPath : "mkext data"));
        }
        goto finish;
    } else {
        result = CFDataCreate(CFGetAllocator(aKext), fileEntry->data, fullSize);
    }

finish:
    SAFE_FREE(filenameCString);
    return result;
}

#ifndef IOKIT_EMBEDDED
/*********************************************************************
*********************************************************************/
static boolean_t __OSKextSwapHeaders(
    CFDataRef kernelImage)
{
    u_char *file = (u_char *) CFDataGetBytePtr(kernelImage);

    return macho_swap(file);
}

/*********************************************************************
*********************************************************************/
static boolean_t __OSKextUnswapHeaders(
    CFDataRef kernelImage)
{
    u_char *file = (u_char *) CFDataGetBytePtr(kernelImage);

    return macho_unswap(file);
}

/*********************************************************************
*********************************************************************/
static boolean_t __OSKextGetLastKernelLoadAddr(
    CFDataRef  kernelImage, 
    uint64_t * lastLoadAddrOut)
{
    boolean_t            result         = false;
    const UInt8        * kernelImagePtr = CFDataGetBytePtr(kernelImage);
    uint64_t             lastLoadAddr   = 0;
    uint64_t             i;

    if (ISMACHO64(MAGIC32(kernelImagePtr))) {
        struct mach_header_64 * kernel_header =
            (struct mach_header_64 *)kernelImagePtr;
        struct segment_command_64 * seg_cmd = NULL;

        seg_cmd = (struct segment_command_64 *)
            ((uintptr_t)kernel_header + sizeof(*kernel_header));
        for (i = 0; i < kernel_header->ncmds; i++){
            if (seg_cmd->cmd == LC_SEGMENT_64) {
                if (seg_cmd->vmaddr + seg_cmd->vmsize > lastLoadAddr) {
                    lastLoadAddr = seg_cmd->vmaddr + seg_cmd->vmsize;
                }
            }
            seg_cmd = (struct segment_command_64 *)
                ((uintptr_t)seg_cmd + seg_cmd->cmdsize);
        }
    } else {
        struct mach_header * kernel_header =
            (struct mach_header *)kernelImagePtr;
        struct segment_command * seg_cmd = NULL;

        seg_cmd = (struct segment_command *)
            ((uintptr_t)kernel_header + sizeof(*kernel_header));
        for (i = 0; i < kernel_header->ncmds; i++){
            if (seg_cmd->cmd == LC_SEGMENT) {
                if (seg_cmd->vmaddr + seg_cmd->vmsize > lastLoadAddr) {
                    lastLoadAddr = seg_cmd->vmaddr + seg_cmd->vmsize;
                }
            }
            seg_cmd = (struct segment_command *)
                ((uintptr_t)seg_cmd + seg_cmd->cmdsize);
        }
    }

    if (lastLoadAddrOut) *lastLoadAddrOut = lastLoadAddr;
    result = true;

    return result;
}

/*********************************************************************
*********************************************************************/
static boolean_t __OSKextGetSegmentAddressAndOffset(
    CFDataRef kernelImage, const char *segname, 
    uint32_t *fileOffsetOut, uint64_t *loadAddrOut)
{
    boolean_t result = false;
    uint32_t fileOffset = 0;
    uint64_t loadAddr = 0;
    
    if (!__OSKextIsArchitectureLP64()) {
        struct mach_header *mach_header = (struct mach_header *) CFDataGetBytePtr(kernelImage);
        struct segment_command *seg = macho_get_segment_by_name(mach_header, segname);
        if (!seg) {
            goto finish;
        }
        
        fileOffset = seg->fileoff;
        loadAddr = seg->vmaddr;
    } else {
        struct mach_header_64 *mach_header = (struct mach_header_64 *) CFDataGetBytePtr(kernelImage);
        struct segment_command_64 *seg = macho_get_segment_by_name_64(mach_header, segname);
        if (!seg) {
            goto finish;
        }
        
        fileOffset = seg->fileoff;
        loadAddr = seg->vmaddr;
    }

    if (fileOffsetOut) *fileOffsetOut = fileOffset;
    if (loadAddrOut) *loadAddrOut = loadAddr;
    result = true;

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
static boolean_t __OSKextGetSegmentFileAndVMSize(
    CFDataRef kernelImage, const char *segname, 
    uint64_t *fileSizeOut, uint64_t *VMSizeOut)
{
    boolean_t result = false;
    uint64_t filesize = 0;
    uint64_t vmsize = 0;
    
    if (!__OSKextIsArchitectureLP64()) {
        struct mach_header *mach_header = (struct mach_header *) CFDataGetBytePtr(kernelImage);
        struct segment_command *seg = macho_get_segment_by_name(mach_header, segname);
        if (!seg) {
            goto finish;
        }
        
        filesize = seg->filesize;
        vmsize = seg->vmsize;
    } else {
        struct mach_header_64 *mach_header = (struct mach_header_64 *) CFDataGetBytePtr(kernelImage);
        struct segment_command_64 *seg = macho_get_segment_by_name_64(mach_header, segname);
        if (!seg) {
            goto finish;
        }
        
        filesize = seg->filesize;
        vmsize = seg->vmsize;
    }

    if (fileSizeOut) *fileSizeOut = filesize;
    if (VMSizeOut) *VMSizeOut = vmsize;
    result = true;

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
static boolean_t __OSKextSetSegmentAddress(
    CFDataRef kernelImage, const char *segname, uint64_t loadAddr)
{
    boolean_t result = false;
    
    if (!__OSKextIsArchitectureLP64()) {
        struct mach_header *mach_header = (struct mach_header *) CFDataGetBytePtr(kernelImage);
        struct segment_command *seg = macho_get_segment_by_name(mach_header, segname);
        if (!seg) {
            goto finish;
        }
        
        seg->vmaddr = (uint32_t) loadAddr;
    } else {
        struct mach_header_64 *mach_header = (struct mach_header_64 *) CFDataGetBytePtr(kernelImage);
        struct segment_command_64 *seg = macho_get_segment_by_name_64(mach_header, segname);
        if (!seg) {
            goto finish;
        }
        
        seg->vmaddr = loadAddr;
    }

    result = true;

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
static boolean_t __OSKextSetSegmentVMSize(
    CFDataRef kernelImage, const char *segname, uint64_t vmsize)
{
    boolean_t result = false;
    
    if (!__OSKextIsArchitectureLP64()) {
        struct mach_header *mach_header = (struct mach_header *) CFDataGetBytePtr(kernelImage);
        struct segment_command *seg = macho_get_segment_by_name(mach_header, segname);
        if (!seg) {
            goto finish;
        }
        
        seg->vmsize = (uint32_t) vmsize;
    } else {
        struct mach_header_64 *mach_header = (struct mach_header_64 *) CFDataGetBytePtr(kernelImage);
        struct segment_command_64 *seg = macho_get_segment_by_name_64(mach_header, segname);
        if (!seg) {
            goto finish;
        }
        
        seg->vmsize = vmsize;
    }

    result = true;

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
static boolean_t __OSKextSetSegmentOffset(
    CFDataRef kernelImage, const char *segname, uint64_t fileOffset)
{
    boolean_t result = false;
    
    if (!__OSKextIsArchitectureLP64()) {
        struct mach_header *mach_header = (struct mach_header *) CFDataGetBytePtr(kernelImage);
        struct segment_command *seg = macho_get_segment_by_name(mach_header, segname);
        if (!seg) {
            goto finish;
        }
        
        seg->fileoff = (uint32_t) fileOffset;
    } else {
        struct mach_header_64 *mach_header = (struct mach_header_64 *) CFDataGetBytePtr(kernelImage);
        struct segment_command_64 *seg = macho_get_segment_by_name_64(mach_header, segname);
        if (!seg) {
            goto finish;
        }
        
        seg->fileoff = fileOffset;
    }

    result = true;

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
static boolean_t __OSKextSetSegmentFilesize(
    CFDataRef kernelImage, const char *segname, uint64_t filesize)
{
    boolean_t result = false;
    
    if (!__OSKextIsArchitectureLP64()) {
        struct mach_header *mach_header = (struct mach_header *) CFDataGetBytePtr(kernelImage);
        struct segment_command *seg = macho_get_segment_by_name(mach_header, segname);
        if (!seg) {
            goto finish;
        }
        
        seg->filesize = (uint32_t) filesize;
    } else {
        struct mach_header_64 *mach_header = (struct mach_header_64 *) CFDataGetBytePtr(kernelImage);
        struct segment_command_64 *seg = macho_get_segment_by_name_64(mach_header, segname);
        if (!seg) {
            goto finish;
        }
        
        seg->filesize = filesize;
    }

    result = true;

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
static boolean_t __OSKextSetSectionAddress(
    CFDataRef kernelImage, const char *segname, 
    const char *sectname, uint64_t loadAddr)
{
    boolean_t result = false;
    
    if (!__OSKextIsArchitectureLP64()) {
        struct mach_header *mach_header = (struct mach_header *) CFDataGetBytePtr(kernelImage);
        struct section *sect = macho_get_section_by_name(mach_header, segname, sectname);
        if (!sect) {
            goto finish;
        }
        
        sect->addr = (uint32_t) loadAddr;
    } else {
        struct mach_header_64 *mach_header = (struct mach_header_64 *) CFDataGetBytePtr(kernelImage);
        struct section_64 *sect = macho_get_section_by_name_64(mach_header, segname, sectname);
        if (!sect) {
            goto finish;
        }
        
        sect->addr = loadAddr;
    }

    result = true;

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
static boolean_t __OSKextSetSectionSize(
    CFDataRef kernelImage, const char *segname, 
    const char *sectname, uint64_t size)
{
    boolean_t result = false;
    
    if (!__OSKextIsArchitectureLP64()) {
        struct mach_header *mach_header = (struct mach_header *) CFDataGetBytePtr(kernelImage);
        struct section *sect = macho_get_section_by_name(mach_header, segname, sectname);
        if (!sect) {
            goto finish;
        }
        
        sect->size = (uint32_t) size;
    } else {
        struct mach_header_64 *mach_header = (struct mach_header_64 *) CFDataGetBytePtr(kernelImage);
        struct section_64 *sect = macho_get_section_by_name_64(mach_header, segname, sectname);
        if (!sect) {
            goto finish;
        }
        
        sect->size = size;
    }

    result = true;

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
static boolean_t __OSKextSetSectionOffset(
    CFDataRef kernelImage, const char *segname, 
    const char *sectname, uint32_t fileOffset)
{
    boolean_t result = false;
    
    if (!__OSKextIsArchitectureLP64()) {
        struct mach_header *mach_header = (struct mach_header *) CFDataGetBytePtr(kernelImage);
        struct section *sect = macho_get_section_by_name(mach_header, segname, sectname);
        if (!sect) {
            goto finish;
        }
        
        sect->offset = fileOffset;
    } else {
        struct mach_header_64 *mach_header = (struct mach_header_64 *) CFDataGetBytePtr(kernelImage);
        struct section_64 *sect = macho_get_section_by_name_64(mach_header, segname, sectname);
        if (!sect) {
            goto finish;
        }
        
        sect->offset = fileOffset;
    }

    result = true;

finish:
    return result;
}

/*********************************************************************
*********************************************************************/
#define FAKE_32BIT_LOAD_ADDRESS  (0x10000000)
#define LP64_LOAD_ADDRESS_OFFSET (2 * 1024 * 1024 * 1024ULL)

static uint64_t __OSKextGetFakeLoadAddress(CFDataRef kernelImage)
{    
    uint64_t                    result         = 0;
    CFNumberRef                 loadAddressNum = NULL;  // must release
    const UInt8               * executable     = NULL;  // do not free
    const UInt8               * executableEnd  = NULL;  // do not free
    fat_iterator                fatIterator    = NULL;  // must fat_iterator_close
    struct mach_header_64     * machHeader     = NULL;  // do not free
    void                      * machEnd        = NULL;  // do not free
    struct segment_command_64 * textSegment    = NULL;  // do not free

   /* We have no address-limits on 32-bit. Just fudge a number.
    */
    if (!__OSKextIsArchitectureLP64()) {
        result = FAKE_32BIT_LOAD_ADDRESS;
        goto finish;
    }

    if (!kernelImage) {
        if ((kOSReturnSuccess != __OSKextSimpleKextRequest(/* kext */ NULL,
            CFSTR(kKextRequestPredicateGetKernelLoadAddress),
            (CFTypeRef *)&loadAddressNum)) ||
            !loadAddressNum ||
            (CFGetTypeID(loadAddressNum) != CFNumberGetTypeID()) ||
            !CFNumberGetValue(loadAddressNum, kCFNumberSInt64Type, &result)) {

            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogIPCFlag,
                "Can't get running kernel load address.");
            result = 0;
            goto finish;
        }
        
        goto finish;
    }

    executable = CFDataGetBytePtr(kernelImage);
    executableEnd = executable + CFDataGetLength(kernelImage);
    fatIterator = fat_iterator_for_data(executable, executableEnd,
        1 /* mach-o only */);

    if (!fatIterator) {
        OSKextLog(/* kext */ NULL, kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Can't read kernel file.");
        goto finish;
    }
    
    machHeader = (struct mach_header_64 *)fat_iterator_find_arch(fatIterator,
        OSKextGetArchitecture()->cputype, OSKextGetArchitecture()->cpusubtype,
        (void **)&machEnd);
    if (!machHeader) {
        OSKextLog(/* kext */ NULL, kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Can't find architecture %s in kernel file.",
                OSKextGetArchitecture()->name);
        goto finish;
    }

    textSegment = macho_get_segment_by_name_64(machHeader, "__TEXT");
    if (!textSegment) {
        OSKextLog(/* kext */ NULL, kOSKextLogErrorLevel | kOSKextLogFileAccessFlag,
            "Can't find text segment in kernel file.");
        goto finish;
    }
    
   /* LP64 uses a "kext basement", which is an offset below the end of the
    * kernel's text segment.
    */
    result = mach_vm_round_page(textSegment->vmaddr + textSegment->vmsize) 
        - LP64_LOAD_ADDRESS_OFFSET;

finish:
    if (fatIterator) {
        fat_iterator_close(fatIterator);
    }
    SAFE_RELEASE(loadAddressNum);
    return result;
}

/*********************************************************************
*********************************************************************/
static CFArrayRef __OSKextPrelinkKexts(
    CFArrayRef        kextArray,
    CFDataRef         kernelImage,
    uint64_t          loadAddrBase,
    uint64_t          sourceAddrBase,
    KXLDContext     * kxldContext,
    u_long          * loadSizeOut,
    Boolean           needAllFlag,
    Boolean           skipAuthenticationFlag,
    Boolean           printDiagnosticsFlag,
    Boolean           stripSymbolsFlag)
{
    CFArrayRef        result   = NULL;
    boolean_t         success  = false;
    CFMutableArrayRef loadList = NULL;
    uint64_t          loadAddr = loadAddrBase;
    uint64_t          sourceAddr = sourceAddrBase;
    u_long            loadSize = 0;
    OSKextLogSpec     linkLogLevel;
    char            * kextIdentifierCString = NULL;  // must free
    CFIndex           i;

    linkLogLevel = needAllFlag ? kOSKextLogErrorLevel : kOSKextLogWarningLevel;

    /* Calculate the load list for the set of kexts. If needAllFlag is false,
     * then kexts whose dependencies do not resolve will fail to link and will
     * be excluded from the cache, while all others will get prelinked. 
     */
    
    loadList = OSKextCopyLoadListForKexts(kextArray, needAllFlag);
    if (!loadList) {
        OSKextLog(/* kext */ NULL, kOSKextLogErrorLevel,
            "Can't resolve dependencies amongst kexts for prelinked kernel.");
        goto finish;
    }

    for (i = 0; i < CFArrayGetCount(loadList); ++i) {
        OSKextRef aKext = (OSKextRef) CFArrayGetValueAtIndex(loadList, i);

        if (!__OSKextCheckForPrelinkedKernel(aKext, needAllFlag, 
            skipAuthenticationFlag, printDiagnosticsFlag)) {

            if (needAllFlag) {
                OSKextLog(/* kext */ aKext, linkLogLevel | kOSKextLogLinkFlag,
                    "Aborting prelink.");
                goto finish;
            } else {
                CFArrayRemoveValueAtIndex(loadList, i--);
            }
        }
    }

    /* Link each kext in the load list */

    for (i = 0; i < CFArrayGetCount(loadList); ++i) {
        OSKextRef aKext = (OSKextRef) CFArrayGetValueAtIndex(loadList, i);

        SAFE_FREE_NULL(kextIdentifierCString);

        if (!OSKextDeclaresExecutable(aKext)) {
            continue;
        }

        kextIdentifierCString =
            createUTF8CStringForCFString(OSKextGetIdentifier(aKext));

       /* Set the load address of the kext.
        */
        loadAddr = loadAddrBase + loadSize;
        sourceAddr = sourceAddrBase + loadSize;
        
       /* OSKextSetLoadAddress() creates aKext->loadInfo.
        */
        OSKextSetLoadAddress(aKext, loadAddr);
        aKext->loadInfo->sourceAddress = sourceAddr;

       /* Perform the link operation. Note we pass 0 for the
        * kernelLoadAddress because we should have a valid address
        * set for every kext when doing a prelinked kernel.
        */
        success = __OSKextPerformLink(aKext, kernelImage,
            /* kernelLoadAddress */ 0, stripSymbolsFlag, kxldContext);
        if (!success) {
            OSKextLog(/* kext */ aKext, linkLogLevel | kOSKextLogLinkFlag,
                "Prelink failed for %s; %s.",
                kextIdentifierCString,
                needAllFlag ? "aborting prelink" : "omitting from prelinked kernel");
            if (needAllFlag) {
                goto finish;
            }
            CFArrayRemoveValueAtIndex(loadList, i--);
            continue;
        }

        loadSize += round_page(aKext->loadInfo->loadSize);
    }

    result = CFRetain(loadList);
    *loadSizeOut = loadSize;

finish:
    SAFE_RELEASE(loadList);
    SAFE_FREE(kextIdentifierCString);
    return result;
}

/*********************************************************************
*********************************************************************/
static CFDataRef __OSKextCreatePrelinkInfoDictionary(
    CFArrayRef loadList,
    CFURLRef   volumeRootURL,
    Boolean    includeAllPersonalities)
{
    CFDataRef                   result                  = NULL; // do not release

    char                        kextPath[PATH_MAX]      = "";
    char                        volumePath[PATH_MAX]    = "";
    CFArrayRef                  allKextsByBundleID      = NULL; // must release
    CFArrayRef                  kextPersonalities       = NULL; // must release
    CFMutableArrayRef           kextInfoDictArray       = NULL; // must release
    CFDataRef                   prelinkInfoData         = NULL; // must release
    CFDataRef                   uuid                    = NULL; // must release
    CFMutableDictionaryRef      kextInfoDict            = NULL; // must release
    CFMutableDictionaryRef      prelinkInfoDict         = NULL; // must release
    CFNumberRef                 cfnum                   = NULL; // must release
    CFStringRef                 bundleVolPath           = NULL; // must release
    CFStringRef                 executableRelPath       = NULL; // must release
    CFStringRef                 archPersonalitiesKey    = NULL; // must release
    CFSetRef                    loadListIDs             = NULL; // must release
    char                      * kextVolPath             = NULL; // do not free
    int64_t                     num                     = 0;
    int                         i                       = 0;
    int                         count                   = 0;

    /* Get the C string for the volume root URL. */

    if (volumeRootURL) {
        if (!CFURLGetFileSystemRepresentation(volumeRootURL,
            /* resolveToBase */ TRUE, (UInt8 *)volumePath, sizeof(volumePath))) {

            OSKextLogStringError(/* kext */ NULL);
            goto finish;
        }
    }

    /* Create a dictionary for all prelinked kernel metadata */

    prelinkInfoDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!prelinkInfoDict) {
        OSKextLogMemError();
        goto finish;
    }

    /* Create an array to hold all of the info dictionaries */

    kextInfoDictArray = CFArrayCreateMutable(kCFAllocatorDefault,
        CFArrayGetCount(loadList), &kCFTypeArrayCallBacks);
    if (!kextInfoDictArray) {
        OSKextLogMemError();
        goto finish;
    }

    CFDictionarySetValue(prelinkInfoDict, CFSTR(kPrelinkInfoDictionaryKey),
        kextInfoDictArray);

    /* We'll need the arch-specific personalities key in the loop body */

    archPersonalitiesKey = __OSKextCreateCompositeKey(
        CFSTR("kIOKitPersonalitiesKey"), OSKextGetArchitecture()->name);
    if (!archPersonalitiesKey) {
        OSKextLogMemError();
        goto finish;
    }

    /* Create an info dictionary for each kext in the load list */

    count = CFArrayGetCount(loadList);
    for (i = 0; i < count; ++i) {
        Boolean   gotPath = FALSE;
        OSKextRef aKext   = (OSKextRef)CFArrayGetValueAtIndex(loadList, i);

        SAFE_RELEASE_NULL(kextInfoDict);
        SAFE_RELEASE_NULL(bundleVolPath);

       /* We need to know if we got a valid path down below.
        * For logging it doesn't matter.
        */
        gotPath = __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
            /* resolveToBase */ true, kextPath);

        OSKextLog(aKext, kOSKextLogStepLevel | kOSKextLogArchiveFlag,
            "Adding %s to prelinked kernel.", kextPath);

        /* Get the existing info dictionary from the kext */

        kextInfoDict = OSKextCopyInfoDictionary(aKext);
        if (!kextInfoDict) {
            OSKextLogMemError();
            goto finish;
        }

        /* We only want early boot kexts to have personalities in the
         * prelinked kernel. If kexts with OSBundleRequired="Safe Boot" or no
         * OSBundleRequired property start too early, they can cause problems
         * with the boot process.  These kexts will still be prelinked, and
         * they will be started when kextd is up and passes personalities for
         * all kexts to the kernel.
         */
        if (!includeAllPersonalities) {
            if (!__OSKextRequiredAtEarlyBoot(aKext)) {
                CFDictionaryRemoveValue(kextInfoDict, CFSTR(kIOKitPersonalitiesKey));
                CFDictionaryRemoveValue(kextInfoDict, archPersonalitiesKey);
            }
        }

        /* Add the load address, source address, and kmod info address information.
         */
        if (OSKextDeclaresExecutable(aKext)) {
            cfnum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type,
                &aKext->loadInfo->loadAddress);
            if (!cfnum) {
                OSKextLogMemError();
                goto finish;
            }
            CFDictionarySetValue(kextInfoDict, CFSTR(kPrelinkExecutableLoadKey), cfnum);
            SAFE_RELEASE_NULL(cfnum);

            cfnum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type,
                &aKext->loadInfo->sourceAddress);
            if (!cfnum) {
                OSKextLogMemError();
                goto finish;
            }
            CFDictionarySetValue(kextInfoDict, CFSTR(kPrelinkExecutableSourceKey), cfnum);
            SAFE_RELEASE_NULL(cfnum);

            num = CFDataGetLength(aKext->loadInfo->prelinkedExecutable);
            cfnum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &num);
            if (!cfnum) {
                OSKextLogMemError();
                goto finish;
            }
            CFDictionarySetValue(kextInfoDict, CFSTR(kPrelinkExecutableSizeKey), cfnum);
            SAFE_RELEASE_NULL(cfnum);

            cfnum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type,
                &aKext->loadInfo->kmodInfoAddress);
            if (!cfnum) {
                OSKextLogMemError();
                goto finish;
            }
            CFDictionarySetValue(kextInfoDict, CFSTR(kPrelinkKmodInfoKey), cfnum);
            SAFE_RELEASE_NULL(cfnum);
        }

       /* If this is an interface kext, add its UUID.
        */
        if (OSKextDeclaresExecutable(aKext) && OSKextIsInterface(aKext)) {
            uuid = OSKextCopyUUIDForArchitecture(aKext, OSKextGetArchitecture());
            if (uuid) {
                CFDictionarySetValue(kextInfoDict, CFSTR(kPrelinkInterfaceUUIDKey),
                    uuid);
                SAFE_RELEASE_NULL(uuid);
            }
        }

       /* Add the kext's absolute path on the volume to the kernelcache.
        */
        if (gotPath) {

            kextVolPath = __absPathOnVolume(kextPath, volumePath);
            if (!kextVolPath) {
                OSKextLogMemError();
                goto finish;
            }

            bundleVolPath = CFStringCreateWithBytes(CFGetAllocator(aKext),
                (UInt8 *)kextVolPath, strlen(kextVolPath),
                kCFStringEncodingUTF8, false);
            if (!bundleVolPath) {
                OSKextLogMemError();
                goto finish;
            }
            CFDictionarySetValue(kextInfoDict, CFSTR(kPrelinkBundlePathKey), bundleVolPath);

           /* For optimizing dtrace we also add the relative path within the kext
            * to its executable (if there is one).
            */
            executableRelPath = __OSKextCopyExecutableRelativePath(aKext);
            if (executableRelPath) {
                CFDictionarySetValue(kextInfoDict, CFSTR(kPrelinkExecutableRelativePathKey),
                    executableRelPath);
            }

        }

        /* Add this info dictionary to the info dict array */

        CFArrayAppendValue(kextInfoDictArray, kextInfoDict);
    }

    /* Serialize the info dictionary */

    prelinkInfoData = IOCFSerialize(prelinkInfoDict, kNilOptions);
    if (!prelinkInfoData) {
        OSKextLogMemError();
        goto finish;
    }

    result = CFRetain(prelinkInfoData);

finish:
    SAFE_RELEASE(allKextsByBundleID);
    SAFE_RELEASE(kextPersonalities);
    SAFE_RELEASE(kextInfoDictArray);
    SAFE_RELEASE(prelinkInfoData);
    SAFE_RELEASE(uuid);
    SAFE_RELEASE(kextInfoDict);
    SAFE_RELEASE(prelinkInfoDict);
    SAFE_RELEASE(cfnum);
    SAFE_RELEASE(bundleVolPath);
    SAFE_RELEASE(executableRelPath);
    SAFE_RELEASE(archPersonalitiesKey);
    SAFE_RELEASE(loadListIDs);
    return result;
}

/*********************************************************************
*********************************************************************/
static Boolean
__OSKextRequiredAtEarlyBoot(
    OSKextRef   theKext)
{
    CFStringRef         bundleRequired  = NULL;

    bundleRequired = (CFStringRef)OSKextGetValueForInfoDictionaryKey(theKext,
        CFSTR(kOSBundleRequiredKey));

    return (bundleRequired && kCFCompareEqualTo != 
        CFStringCompare(bundleRequired, CFSTR(kOSBundleRequiredSafeBoot), 0));
}

#if 0 /* masking out compiler warnings */

/*********************************************************************
*********************************************************************/
static CFArrayRef
__OSKextCopyKextsByBundleID(void)
{
    CFArrayRef          result          = NULL;  // do not release
    CFArrayRef          kextList        = NULL;  // must release
    CFStringRef       * bundleIDs       = NULL;  // must free
    OSKextRef         * theKexts        = NULL;  // must free
    int                 count           = 0;
    int                 i               = 0;

    count = CFDictionaryGetCount(__sOSKextsByIdentifier);

    /* Get all of the bundle IDs in the system */

    bundleIDs = malloc(count * sizeof(CFStringRef));
    if (!bundleIDs) {
        OSKextLogMemError();
        goto finish;
    }

    CFDictionaryGetKeysAndValues(__sOSKextsByIdentifier,
        (const void **)bundleIDs, NULL);

    /* Get a kext for each of the bundle IDs */

    theKexts = malloc(count * sizeof(OSKextRef));
    if (!theKexts) {
        OSKextLogMemError();
        goto finish;
    }

    for (i = 0; i < count; ++i) {
        theKexts[i] = OSKextGetKextWithIdentifier(bundleIDs[i]);
        if (!theKexts[i]) {
            OSKextLog(/* kext */ NULL, 
                kOSKextLogErrorLevel | kOSKextLogLinkFlag,
                "Internal error: failed to find a kext with bundle ID: %s",
                CFStringGetCStringPtr(bundleIDs[i], kCFStringEncodingMacRoman));
            goto finish;
        }
    }

    kextList = CFArrayCreate(kCFAllocatorDefault, (const void **) theKexts,
        count, &kCFTypeArrayCallBacks);
    if (!kextList) {
        OSKextLogMemError();
        goto finish;
    }

    result = CFRetain(kextList);
finish:
    SAFE_RELEASE(kextList);
    SAFE_FREE(bundleIDs);
    SAFE_FREE(theKexts);

    return result;
}

/*********************************************************************
*********************************************************************/
static CFSetRef
__OSKextCopyBundleIDsForKexts(
    CFArrayRef  theKexts)
{
    CFSetRef            result          = NULL; // do not release
    CFSetRef            bundleIDSet     = NULL; // must release
    CFStringRef       * bundleIDs       = NULL; // must free
    OSKextRef           aKext           = NULL; // do not release
    int                 count           = 0;
    int                 i               = 0;

    count = CFArrayGetCount(theKexts);
    bundleIDs = malloc(count * sizeof(CFStringRef));
    if (!bundleIDs) {
        OSKextLogMemError();
        goto finish;
    }

    for (i = 0; i < count; ++i) {
        aKext = (OSKextRef) CFArrayGetValueAtIndex(theKexts, i);
        bundleIDs[i] = aKext->bundleID;
    }

    bundleIDSet = CFSetCreate(kCFAllocatorDefault,
        (const void **) bundleIDs, count, &kCFTypeSetCallBacks);
    if (!bundleIDSet) {
        OSKextLogMemError();
        goto finish;
    }

    result = CFRetain(bundleIDSet);
finish:
    SAFE_RELEASE(bundleIDSet);
    SAFE_FREE(bundleIDs);
    return result;
}

#endif /* masking out compiler warnings */

/*********************************************************************
*********************************************************************/
static u_long __OSKextCopyPrelinkedKexts(
    CFMutableDataRef prelinkImage,
    CFArrayRef       loadList,
    u_long           fileOffsetBase,
    uint64_t         sourceAddrBase)
{
    boolean_t   success     = false;
    u_char    * prelinkData = CFDataGetMutableBytePtr(prelinkImage);
    u_long      size        = 0;
    u_long      totalSize   = 0;
    u_long      fileOffset  = fileOffsetBase;
    uint64_t    sourceAddr  = sourceAddrBase;
    int i = 0;

    /* Set the text segment and section address and offset */

    success = __OSKextSetSegmentAddress(prelinkImage, kPrelinkTextSegment, 
        sourceAddrBase);
    if (!success) {
        goto finish;
    }
    
    success = __OSKextSetSegmentOffset(prelinkImage, kPrelinkTextSegment, 
        fileOffsetBase);
    if (!success) {
        goto finish;
    }

    success = __OSKextSetSectionAddress(prelinkImage, kPrelinkTextSegment,
        kPrelinkTextSection, sourceAddrBase);
    if (!success) {
        goto finish;
    }

    success = __OSKextSetSectionOffset(prelinkImage, kPrelinkTextSegment,
        kPrelinkTextSection, fileOffset);
    if (!success) {
        goto finish;
    }

    /* Copy all kext executables */

    for (i = 0; i < CFArrayGetCount(loadList); ++i) {
        OSKextRef aKext = (OSKextRef) CFArrayGetValueAtIndex(loadList, i);

        if (!OSKextDeclaresExecutable(aKext)) {
            continue;
        }

       /* xxx - Is it safe to assume aKext->loadInfo exists here?
        */
        memcpy(prelinkData + fileOffset + size, 
            CFDataGetBytePtr(aKext->loadInfo->prelinkedExecutable),
            CFDataGetLength(aKext->loadInfo->prelinkedExecutable));

        size += round_page(CFDataGetLength(aKext->loadInfo->prelinkedExecutable));
    }

    sourceAddr += size;
    fileOffset += size;
    totalSize += size;

    /* Set the text segment and section size */

    success = __OSKextSetSegmentVMSize(prelinkImage, kPrelinkTextSegment, size);
    if (!success) {
        goto finish;
    }
    
    success = __OSKextSetSegmentFilesize(prelinkImage, kPrelinkTextSegment, size);
    if (!success) {
        goto finish;
    }

    success = __OSKextSetSectionSize(prelinkImage, kPrelinkTextSegment,
        kPrelinkTextSection, size);
    if (!success) {
        goto finish;
    }

finish:
    return totalSize;
}


/*********************************************************************
*********************************************************************/
static u_long __OSKextCopyPrelinkInfoDictionary(
    CFMutableDataRef prelinkImage,
    CFDataRef        prelinkInfoData,
    u_long           fileOffset,
    uint64_t         sourceAddr)
{
    boolean_t    success    = false;
    u_char    * prelinkData = CFDataGetMutableBytePtr(prelinkImage);
    u_long      size        = 0;

    size = CFDataGetLength(prelinkInfoData);
    memcpy(prelinkData + fileOffset, CFDataGetBytePtr(prelinkInfoData), size);

    /* Set the info dictionary segment headers */

    success = __OSKextSetSegmentAddress(prelinkImage, kPrelinkInfoSegment,
        sourceAddr);
    if (!success) {
        goto finish;
    }

    success = __OSKextSetSegmentVMSize(prelinkImage, kPrelinkInfoSegment, 
        round_page(size));
    if (!success) {
        goto finish;
    }

    success = __OSKextSetSegmentOffset(prelinkImage, kPrelinkInfoSegment,
        fileOffset);
    if (!success) {
        goto finish;
    }

    success = __OSKextSetSegmentFilesize(prelinkImage, kPrelinkInfoSegment,
        size);
    if (!success) {
        goto finish;
    }

    /* Set the info dictionary section headers */

    success = __OSKextSetSectionAddress(prelinkImage, kPrelinkInfoSegment,
        kPrelinkInfoSection, sourceAddr);
    if (!success) {
        goto finish;
    }

    success = __OSKextSetSectionOffset(prelinkImage, kPrelinkInfoSegment,
        kPrelinkInfoSection, fileOffset);
    if (!success) {
        goto finish;
    }

    success = __OSKextSetSectionSize(prelinkImage, kPrelinkInfoSegment,
        kPrelinkInfoSection, size);
    if (!success) {
        goto finish;
    }

finish:
    return round_page(size);
}

/*********************************************************************
*********************************************************************/
CFDataRef OSKextCreatePrelinkedKernel(
    CFDataRef           kernelImage,
    CFArrayRef          kextArray,
    CFURLRef            volumeRootURL,
    uint32_t            flags,
    CFDictionaryRef   * symbolsOut)
{
    CFDataRef                result             = NULL;
    kern_return_t            kxldResult         = KERN_FAILURE;
    boolean_t                success            = false;
    boolean_t                swapped            = false;
    KXLDContext            * kxldContext        = NULL;
    CFArrayRef               loadList           = NULL;
    CFDataRef                prelinkInfoData    = NULL;
    CFMutableDataRef         prelinkImage       = NULL;
    CFMutableDictionaryRef   symbols            = NULL;
    u_long                   prelinkSize        = 0;
    u_long                   size               = 0;
    uint32_t                 baseFileOffset     = 0;
    uint32_t                 fileOffset         = 0;
    uint64_t                 textLoadAddr       = 0;
    uint64_t                 textVMSize         = 0;
    uint64_t                 baseLoadAddr       = 0;
    uint64_t                 baseSourceAddr     = 0;
    uint64_t                 sourceAddr         = 0;
    
    /* Set up kxld's link context */

    kxldResult = kxld_create_context(&kxldContext,
        __OSKextLinkAddressCallback, __OSKextLoggingCallback, /* flags */ 0,
        OSKextGetArchitecture()->cputype, OSKextGetArchitecture()->cpusubtype);
    if (kxldResult != KERN_SUCCESS) {
        OSKextLog(/* kext */ NULL, kOSKextLogErrorLevel | kOSKextLogLinkFlag,
             "Can't create link context.");
        goto finish;
    }

    /* Swap kernel if necessary */
    swapped = __OSKextSwapHeaders(kernelImage);

   /* Get the last last VM load address specified in the kernel image
    * (that is, the next "available" one).
    */
    success = __OSKextGetLastKernelLoadAddr(kernelImage, &baseSourceAddr);
    if (!success) {
        OSKextLog(/* kext */ NULL, kOSKextLogErrorLevel,
            "Can't get last load address for kernel.");
        goto finish;
    }

    baseFileOffset = round_page(CFDataGetLength(kernelImage));
    baseSourceAddr = mach_vm_round_page(baseSourceAddr);

    /* For LP64 systems, we prelink the kexts into a VM region that starts 2GB
     * from the top of the kernel __TEXT segment.
     */
    if (__OSKextIsArchitectureLP64()) {
        success = __OSKextGetSegmentAddressAndOffset(kernelImage,
            SEG_TEXT, NULL, &textLoadAddr);
        if (!success) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
                "Could not get kernel text address.");
            goto finish;
        }

        success = __OSKextGetSegmentFileAndVMSize(kernelImage,
            SEG_TEXT, NULL, &textVMSize);
        if (!success) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
                "Could not get kernel text vmsize.");
            goto finish;
        }

        baseLoadAddr = mach_vm_round_page(textLoadAddr + textVMSize);
        if (baseLoadAddr < __kOSKextMaxKextDisplacementLP64) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogArchiveFlag,
                "Kext base load address underflow.");
            goto finish;
        }

        baseLoadAddr -= __kOSKextMaxKextDisplacementLP64;
    } else {
        baseLoadAddr = baseSourceAddr;
    }

    prelinkSize = baseFileOffset;
    sourceAddr = baseSourceAddr;

    /* Perform kext links */

    loadList = __OSKextPrelinkKexts(kextArray, kernelImage, 
        baseLoadAddr, sourceAddr, kxldContext, &size,
        (flags & kOSKextKernelcacheNeedAllFlag),
        (flags & kOSKextKernelcacheSkipAuthenticationFlag),
        (flags & kOSKextKernelcachePrintDiagnosticsFlag),
        (flags & kOSKextKernelcacheStripSymbolsFlag));
    if (!loadList) {
        goto finish;
    }

    prelinkSize += size;
    sourceAddr += size;

    /* Create the serialized info dictionary */

    prelinkInfoData = __OSKextCreatePrelinkInfoDictionary(loadList,
        volumeRootURL, (flags & kOSKextKernelcacheIncludeAllPersonalitiesFlag));
    if (!prelinkInfoData) {
        goto finish;
    }

    size = round_page(CFDataGetLength(prelinkInfoData));
    prelinkSize += size;

   /* Allocate a buffer to contain the prelinked kernel.
    * It may end up smaller than prelinkSize when we copy
    * the base kernel image, but it won't end up bigger!
    */
    prelinkImage = CFDataCreateMutable(kCFAllocatorDefault, prelinkSize);
    if (!prelinkImage) {
        OSKextLogMemError();
        goto finish;
    }

    CFDataSetLength(prelinkImage, prelinkSize);

    /* Copy the base kernel */

    if (swapped) {
        __OSKextUnswapHeaders(kernelImage);
        swapped = false;
    }

    CFDataReplaceBytes(prelinkImage, CFRangeMake(0, CFDataGetLength(kernelImage)),
        CFDataGetBytePtr(kernelImage), CFDataGetLength(kernelImage));

    /* Reset the fileOffset and sourceAddr */

    fileOffset = baseFileOffset;
    sourceAddr = baseSourceAddr;

    /* Copy the kexts */

    size = __OSKextCopyPrelinkedKexts(prelinkImage, loadList, 
        fileOffset, sourceAddr);

    fileOffset += size;
    sourceAddr += size;

    /* Copy the info dictionary */

    size = __OSKextCopyPrelinkInfoDictionary(prelinkImage, prelinkInfoData,
        fileOffset, sourceAddr);
    fileOffset += size;
    sourceAddr += size;

   /* Trim the new image to the size actually copied.
    */
    CFDataSetLength(prelinkImage, fileOffset);

    /* Save the kexts' symbols if requested */
    
    if (symbolsOut) {
        int i = 0;

        symbols = CFDictionaryCreateMutable(kCFAllocatorDefault,
            CFArrayGetCount(loadList), &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
        if (!symbols) {
            goto finish;
        }

        for (i = 0; i < CFArrayGetCount(loadList); ++i) {
            OSKextRef aKext = (OSKextRef) CFArrayGetValueAtIndex(loadList, i);
            if (!aKext) {
                printf("%u: NULL kext\n", i);
                continue;
            }

            success = __OSKextExtractDebugSymbols(aKext, symbols);
            if (!success) {
                goto finish;
            }
        }

        *symbolsOut = CFRetain(symbols);
    }

    result = CFRetain(prelinkImage);

finish:
    if (swapped) __OSKextUnswapHeaders(kernelImage);

    SAFE_RELEASE(loadList);
    SAFE_RELEASE(prelinkInfoData);
    SAFE_RELEASE(prelinkImage);
    SAFE_RELEASE(symbols);
    if (kxldContext) kxld_destroy_context(kxldContext);

    return result;
}

/*********************************************************************
*********************************************************************/
Boolean __OSKextCheckForPrelinkedKernel(
    OSKextRef aKext,
    Boolean   needAllFlag,
    Boolean   skipAuthenticationFlag,
    Boolean   printDiagnosticsFlag)
{
    char kextPath[PATH_MAX];
    const NXArchInfo * arch = NULL;
    OSKextLogSpec logLevel = needAllFlag ? kOSKextLogErrorLevel :
        kOSKextLogWarningLevel;
    
    __OSKextGetFileSystemPath(aKext, /* otherURL */ NULL,
        /* resolveToBase */ TRUE, kextPath);

    if (!__OSKextIsValid(aKext)) {
        OSKextLog(aKext,
            logLevel | kOSKextLogArchiveFlag,
            "%s is not valid; omitting from prelinked kernel.",
            kextPath);
        if (printDiagnosticsFlag) {
            OSKextLogDiagnostics(aKext, kOSKextDiagnosticsFlagAll);
        }
        return false;
    }

    arch = OSKextGetArchitecture();
    if (!OSKextSupportsArchitecture(aKext, arch)) {
        OSKextLog(aKext,
            logLevel | kOSKextLogArchiveFlag,
            "%s doesn't support architecture %s; "
            "omitting from prelinked kernel.",
             kextPath, arch->name);
        return false;
    }

    if (!skipAuthenticationFlag && !OSKextIsAuthentic(aKext)) {
        OSKextLog(aKext,
            logLevel | kOSKextLogArchiveFlag,
            "%s is not authentic; omitting from prelinked kernel.",
            kextPath);
            if (printDiagnosticsFlag) {
                OSKextLogDiagnostics(aKext, kOSKextDiagnosticsFlagAll);
            }
        return false;
    }

    return true;
}
#endif /* !IOKIT_EMBEDDED */


#pragma mark Misc
/*********************************************************************
*********************************************************************/
CFComparisonResult __OSKextCompareIdentifiers(
    const void * val1,
    const void * val2,
    void       * context __unused)
{
    CFStringRef identifier1 = OSKextGetIdentifier((OSKextRef)val1);
    CFStringRef identifier2 = OSKextGetIdentifier((OSKextRef)val2);
    return CFStringCompare(identifier1, identifier2,
        kCFCompareCaseInsensitive|kCFCompareForcedOrdering);
}

/*********************************************************************
* If we have a volume path, strip it off the front of the path to the
* kext so we have a volume-relative path--but keep the slash on the
* beginning so that it looks absolute for the cache. We only look for
* one trailing slash...sequential slashes in a path get boiled down
* anyhow.
*********************************************************************/
char * __absPathOnVolume(
    const char * path,
    const char * volumePath)
{
    char * result = (char *)path;

    if (volumePath && volumePath[0]) {
        size_t volumePathLength = strlen(volumePath);
        if (volumePath[volumePathLength - 1] == '/') {
            volumePathLength--;
        }
        if (volumePathLength &&
            !strncmp(result, volumePath, volumePathLength))
        {
            result += volumePathLength;
        }
    }

    return result;
}

/*********************************************************************
*********************************************************************/
CFStringRef __OSKextCopyExecutableRelativePath(OSKextRef aKext)
{
    CFStringRef  result            = NULL;
    CFURLRef     kextAbsURL        = NULL;  // must release
    CFStringRef  kextAbsPath       = NULL;  // must release
    CFURLRef     executableURL     = NULL;  // must release
    CFURLRef     executableAbsURL  = NULL;  // must release
    CFStringRef  executableAbsPath = NULL;  // must release
    CFStringRef  executableRelPath = NULL;  // must release

    kextAbsURL = CFURLCopyAbsoluteURL(aKext->bundleURL);
    if (!kextAbsURL) {
        goto finish;
    }
    kextAbsPath = CFURLCopyFileSystemPath(kextAbsURL, kCFURLPOSIXPathStyle);
    if (!kextAbsPath) {
        goto finish;
    }

    executableURL = _CFBundleCopyExecutableURLInDirectory(OSKextGetURL(aKext));
    if (!executableURL) {
        goto finish;
    }
    executableAbsURL = CFURLCopyAbsoluteURL(executableURL);
    if (!executableAbsURL) {
        goto finish;
    }
    executableAbsPath = CFURLCopyFileSystemPath(executableAbsURL, kCFURLPOSIXPathStyle);
    if (!executableAbsPath) {
        goto finish;
    }
    CFRange subRange;
    
    subRange.location = CFStringGetLength(kextAbsPath) + 1; /* +1 for the slash */
    subRange.length = CFStringGetLength(executableAbsPath) - subRange.location;

    result = CFStringCreateWithSubstring(kCFAllocatorDefault,
        executableAbsPath, subRange);

finish:

    SAFE_RELEASE(kextAbsURL);
    SAFE_RELEASE(kextAbsPath);
    SAFE_RELEASE(executableURL);
    SAFE_RELEASE(executableAbsURL);
    SAFE_RELEASE(executableAbsPath);
    SAFE_RELEASE(executableRelPath);

    return result;
}

#if PRAGMA_MARK
/********************************************************************/
#pragma mark URL Utilities
/********************************************************************/
#endif

/*********************************************************************
*********************************************************************/
CFStringRef _CFURLCopyAbsolutePath(CFURLRef anURL)
{
    CFStringRef result = NULL;
    CFURLRef    absURL = NULL;  // must release

    absURL = CFURLCopyAbsoluteURL(anURL);
    if (!absURL) {
        goto finish;
    }
    
    result = CFURLCopyFileSystemPath(absURL, kCFURLPOSIXPathStyle);
    
finish:
    SAFE_RELEASE(absURL);
    return result;
}

#if PRAGMA_MARK
/*********************************************************************
#pragma mark Logging
*********************************************************************/
#endif

static inline bool logSpecMatch(
    OSKextLogSpec msgLogSpec,
    OSKextLogSpec logFilter) __attribute__((always_inline));

static inline bool logSpecMatch(
    OSKextLogSpec msgLogSpec,
    OSKextLogSpec logFilter)
{
    OSKextLogSpec filterKextGlobal  = logFilter & kOSKextLogKextOrGlobalMask;
    OSKextLogSpec filterLevel       = logFilter & kOSKextLogLevelMask;
    OSKextLogSpec filterFlags       = logFilter & kOSKextLogFlagsMask;

    OSKextLogSpec msgKextGlobal    = msgLogSpec & kOSKextLogKextOrGlobalMask;
    OSKextLogSpec msgLevel         = msgLogSpec & kOSKextLogLevelMask;
    OSKextLogSpec msgFlags         = msgLogSpec & kOSKextLogFlagsMask;

   /* Explicit messages always get logged.
    */
    if (msgLevel == kOSKextLogExplicitLevel) {
        return true;
    }

   /* Warnings and errors are logged regardless of the flags.
    */
    if (msgLevel <= kOSKextLogBasicLevel && (msgLevel <= filterLevel)) {
        return true;
    }

   /* A verbose message that isn't for a logging-enabled kext and isn't global
    * does *not* get logged.
    */
    if (!msgKextGlobal && !filterKextGlobal) {
        return false;
    }

   /* Warnings and errors are logged regardless of the flags.
    * All other messages must fit the flags and
    * have a level at or below the filter.
    *
    */
    if ((msgFlags & filterFlags) && (msgLevel <= filterLevel)) {
        return true;
    }
    return false;
}

static bool __OSKextShouldLog(
    OSKextRef     aKext,
    OSKextLogSpec msgLogSpec)
{
    if (!aKext || aKext->flags.loggingEnabled) {
        msgLogSpec = msgLogSpec | kOSKextLogKextOrGlobalMask;
    }

    return logSpecMatch(msgLogSpec, __sUserLogFilter);
}

/*********************************************************************
*********************************************************************/
void OSKextLog(
    OSKextRef        aKext,
    OSKextLogSpec    msgLogSpec,
    const char     * format, ...)
{
    va_list argList;

    va_start(argList, format);
    OSKextVLog(aKext, msgLogSpec, format, argList);
    va_end(argList);
}

/*********************************************************************
*********************************************************************/
void OSKextVLog(
    OSKextRef        aKext,
    OSKextLogSpec    msgLogSpec,
    const char     * format,
    va_list          srcArgList)
{
    va_list          argList;
    char           * outputCString = NULL;  // must free

    if (!__sOSKextLogOutputFunction) {
        goto finish;
    }
    
    // xxx - I could check for a bad msgLogSpec (such as intersect or user)
    // xxx - but we can't report the bad callsite so never mind.

    if (!__OSKextShouldLog(aKext, msgLogSpec)) {
        goto finish;
    }

   /* No goto from here until past va_end()!
    */    
    va_copy(argList, srcArgList);
    vasprintf(&outputCString, format, argList);
    va_end(argList);

    if (outputCString) {
        __sOSKextLogOutputFunction(aKext, msgLogSpec, "%s", outputCString);
    }

finish:
    SAFE_FREE(outputCString);
    return;
}

/*********************************************************************
*********************************************************************/
void OSKextLogCFString(
    OSKextRef        aKext,
    OSKextLogSpec    msgLogSpec,
    CFStringRef      format, ...)
{
    va_list argList;

    va_start(argList, format);
    OSKextVLogCFString(aKext, msgLogSpec, format, argList);
    va_end(argList);
}

/*********************************************************************
*********************************************************************/
void OSKextVLogCFString(
    OSKextRef        aKext,
    OSKextLogSpec    msgLogSpec,
    CFStringRef      format,
    va_list          srcArgList)
{
    va_list          argList;
    CFStringRef      outputString = NULL;  // must release
    size_t           cStringLength;
    char           * outputCString = NULL;  // must free

    if (!__sOSKextLogOutputFunction) {
        goto finish;
    }
    
    // xxx - I could check for a bad msgLogSpec (such as intersect or user)
    // xxx - but we can't report the bad callsite so never mind.

    if (!__OSKextShouldLog(aKext, msgLogSpec)) {
        goto finish;
    }

   /* No goto from here until past va_end()!
    */    
    va_copy(argList, srcArgList);
    outputString = CFStringCreateWithFormatAndArguments(kCFAllocatorDefault,
        /* options */ NULL, format, srcArgList);
    va_end(argList);
    if (!outputString) {
        goto finish;
    }

    cStringLength = CFStringGetMaximumSizeForEncoding(
        CFStringGetLength(outputString), kCFStringEncodingUTF8);
    outputCString = (char *)malloc(cStringLength);
    if (!outputCString) {
        goto finish;
    }

    if (!CFStringGetCString(outputString, outputCString, cStringLength,
        kCFStringEncodingUTF8)) {

        goto finish;
    }

    if (outputCString) {
        __sOSKextLogOutputFunction(aKext, msgLogSpec, "%s", outputCString);
    }

finish:
    SAFE_RELEASE(outputString);
    SAFE_FREE(outputCString);
    return;
}

/*********************************************************************
*********************************************************************/
void __sOSKextDefaultLogFunction(
    OSKextRef       aKext __unused,
    OSKextLogSpec   msgLogSpec __unused,
    const char    * format, ...)
{
    va_list   argList;
    FILE    * stream = stderr;
    
    va_start(argList, format);
    vfprintf(stream, format, argList);
    va_end(argList);

    fprintf(stream, "\n");
    return;
}

/*********************************************************************
*********************************************************************/
void __OSKextLogKernelMessages(
    OSKextRef aKext,
    CFTypeRef kernelMessages)
{
    CFArrayRef  logInfoArray  = (CFArrayRef)kernelMessages;
    CFArrayRef  flagsArray    = NULL;  // do not release
    CFArrayRef  messagesArray = NULL;  // do not release
    CFIndex     count, i;

    if (!__sOSKextLogOutputFunction) {
        goto finish;
    }
    
    if (CFGetTypeID(logInfoArray) != CFArrayGetTypeID()) {
        // xxx log error :-)
        goto finish;
    }

    if (CFArrayGetCount(logInfoArray) != 2) {
        // xxx log error :-)
        goto finish;
    }
    
    flagsArray = (CFArrayRef)CFArrayGetValueAtIndex(logInfoArray, 0);
    messagesArray = (CFArrayRef)CFArrayGetValueAtIndex(logInfoArray, 1);

    if ((CFGetTypeID(flagsArray) != CFArrayGetTypeID()) ||
        (CFGetTypeID(messagesArray) != CFArrayGetTypeID()) ||
        (CFArrayGetCount(flagsArray) != CFArrayGetCount(messagesArray))) {
        
        // xxx log error :-)
        goto finish;
    }

    count = CFArrayGetCount(messagesArray);
    for (i = 0; i < count; i++) {
        CFNumberRef flagsNum = (CFNumberRef)CFArrayGetValueAtIndex(
            flagsArray, i);
        OSKextLogSpec msgLogSpec;
        CFStringRef string = (CFStringRef)CFArrayGetValueAtIndex(
            messagesArray, i);

        if (CFNumberGetValue(flagsNum, kCFNumberSInt32Type, &msgLogSpec)) {
            // xxx try this on the stack before allocating
            // xxx - need to get the msgLogSpec from the kernel
            char * cString = createUTF8CStringForCFString(string);
            if (cString) {
                __sOSKextLogOutputFunction(aKext, msgLogSpec, "(kernel) %s", cString);
                SAFE_FREE_NULL(cString);
            }
        } else {
            // xxx log error :-)
        }
    }

finish:
    return;
}

/*******************************************************************************
* safe_mach_error_string()
*******************************************************************************/
static const char * safe_mach_error_string(mach_error_t error_code)
{
    const char * result = mach_error_string(error_code);
    if (!result) {
        result = "(unknown)";
    }
    return result;
}
