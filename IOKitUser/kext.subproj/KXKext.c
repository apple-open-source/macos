#if !__LP64__

#define _KEXT_KEYS

#include <libc.h>
#include <fts.h>
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFBundlePriv.h>
#include <errno.h>
#include <pthread.h>
#include <mach-o/nlist.h>
#include <mach-o/dyld.h>
#include <sys/types.h>   
#include <sys/mman.h>   

#include "KXKext.h"
#include "KXKext_private.h"
#include "fat_util.h"
#include "macho_util.h"
#include "path_util.h"
#include "vers_rsrc.h"
#include "printPList.h"


/*******************************************************************************
* Internal declarations for error dictionary keys and values.
*******************************************************************************/

CFStringRef kKXKextErrorKeyFileAccess = NULL;
CFStringRef kKXKextErrorKeyBundleNotInRepository = NULL;
CFStringRef kKXKextErrorKeyNotABundle = NULL;
CFStringRef kKXKextErrorKeyNotAKextBundle = NULL;
CFStringRef kKXKextErrorKeyBadPropertyList = NULL;
CFStringRef kKXKextErrorKeyMissingProperty = NULL;
CFStringRef kKXKextErrorKeyPropertyIsIllegalType = NULL;
CFStringRef kKXKextErrorKeyPropertyIsIllegalValue = NULL;
CFStringRef kKXKextErrorKeyIdentifierOrVersionTooLong = NULL;
CFStringRef kKXKextErrorKeyPersonalitiesNotNested = NULL;
CFStringRef kKXKextErrorKeyMissingExecutable = NULL;
CFStringRef kKXKextErrorKeyCompatibleVersionLaterThanVersion = NULL;
CFStringRef kKXKextErrorKeyExecutableBad = NULL;
CFStringRef kKXKextErrorKeyExecutableBadArch = NULL;

CFStringRef kKXKextErrorKeyStatFailure = NULL;
CFStringRef kKXKextErrorKeyFileNotFound = NULL;
CFStringRef kKXKextErrorKeyOwnerPermission = NULL;
CFStringRef kKXKextErrorKeyChecksum = NULL;
CFStringRef kKXKextErrorKeySignature = NULL;
CFStringRef kKXKextErrorKeyDependenciesUnresolvable = NULL;
CFStringRef kKXKextErrorKeyPossibleDependencyLoop = NULL;

CFStringRef kKXKextErrorKeyNonuniqueIOResourcesMatch = NULL;
CFStringRef kKXKextErrorKeyNoExplicitKernelDependency = NULL;
CFStringRef kKXKextErrorKeyBundleIdentifierMismatch = NULL;
CFStringRef kKXKextErrorKeyBundleVersionMismatch = NULL;
CFStringRef kKXKextErrorKeyDeclaresBothKernelAndKPIDependencies = NULL;

CFStringRef kKXKextDependencyUnavailable = NULL;
CFStringRef kKXKextDependencyNoCompatibleVersion = NULL;
CFStringRef kKXKextDependencyCompatibleVersionUndeclared = NULL;
CFStringRef kKXKextIndirectDependencyUnresolvable = NULL;
CFStringRef kKXKextDependencyCircularReference = NULL;

/*******************************************************************************
* The basic data structure for a kext.
*
* Loadability test order:
* 1. Valid
* 2. Eligible for boot level
* 3. Enabled  --  not yet implemented (may never be)
* 4. Authentic
* 5. Has all dependencies
*******************************************************************************/

typedef struct __KXKext {

    CFRuntimeBase  cfBase;   // base CFType information

    SInt32 logLevel; // taken from "OSBundleDebugLevel" property in infoDictionary

    KXKextManagerRef    manager;
    KXKextRepositoryRef repository;  // Gives absolute path up to kext bundle

    CFDictionaryRef   infoDictionary;  // May be from cache!

    // A kext plist cache always, always lists top-level kexts before
    // all plugin kexts. That way all of the relationships can be
    // resolved as the kexts are initialized from their cache entries.
    //
    CFMutableArrayRef plugins;
    KXKextRef         container;

    // The POSIX pathname, relative to the repository, of the bundle.
    // For a plugin this will include the parent kext's name and subdirectories
    // to the plugins directory.
    // This cannot be a CFURLRef because that data type does filesystem
    // ops when created and we want to delay those until a request is
    // actually made against the kext (for kexts from a cache).
    //
    CFStringRef       bundlePathInRepository;  // May be from cache!

    // The basic bundle directory name; last part of bundlePathInRepository.
    // Held by kext to avoid having multiple copies made by clients, as
    // that seems to be the only CF way to get the last part of a path.
    // This may be from the cache, as it otherwise has to be created from
    // an URL and we don't want to hit the filesystem when using the cache.
    //
    CFStringRef       bundleDirectoryName;

    // These properties only exist once a request has been made against
    // the kext and we hit the disk to verify that it's there.
    //
    // For kexts created from an info cache bundle will be NULL.
    // The necessary filesystem ops may not have been peformed yet,
    // so none of the validation etc. info is defined. When a
    // request is actually made against this kext, the filesystem
    // ops will be performed.
    //
    CFBundleRef       bundle;        // Only exists once we've hit the disk.
    CFURLRef          bundleURL;

    struct {
        unsigned int declaresExecutable:1;
        unsigned int isKernelResource:1;

        unsigned int isValid:1;          // valid bundle and all kext props legal
        unsigned int isEligibleDuringSafeBoot:1;
        unsigned int isEnabled:1;        // persistent enabling not yet implemented
                                         // (need to determine on-disk indication)

        unsigned int canAuthenticate:1;
        unsigned int hasBeenAuthenticated:1;
        unsigned int isAuthentic:1;      // plist file owned by root, signed(?)

        unsigned int canResolveDependencies:1;
        unsigned int hasAllDependencies:1;

        // this gets set for all kexts of a given {id, version}
        // so it doesn't necessarily mean this particular kext's
        // executable code is in the kernel! There's really no
        // way to know given the current architecture.
        unsigned int isLoaded:1;
        unsigned int otherVersionIsLoaded:1;

       /*****
        * This flag is set whenever a kext fails to load, for whatever reason.
        * The kext manager only includes in its database of candidate kexts
       * those that do not have this flag set. This flag is cleared whenever
        * a kext is added to the manager, in case an unresolved dependency has
        * now become resolvable, but not when a kext is removed.
        */
        unsigned int loadFailed:1;
        unsigned int hasIOKitDebugProperty:1;
        unsigned int reserved:2;
    } flags;

    /* 3131947: integrityState is set depending on whether or not the kext's files 
     * match any receipts in a com.apple receipt.
     */
     KXKextIntegrityState integrityState;

    // These values are cached during validation.
    char * versionString;      // used to add kmod entries to dependency graph
    VERS_version version;            // used for quick compatibility calculation
    VERS_version compatibleVersion;
    vm_address_t startAddress;

    // These must be cleared/recalculated with any change to a repository!
    //
    KXKextRef   priorVersion;  // Next kext with same identifier but
                               // earlier version.
    KXKextRef   nextDuplicate; // Next kext with same identifier & version.

    CFMutableArrayRef  directDependencies;    // may have some missing
        // may be derived from cached infoDictionary data

   /*****
    * Any failed diagnotic tests get their results put in one of these
    * three dictionaries. See the header file for what keys and values
    * go in them. If the manager does not perform full tests, then the
    * first failure encountered will cease testing and any of
    * these dictionaries will have exactly one entry. If the manager
    * does perform full tests, then as many errors as are found will
    * be in each dictionary.
    */
    CFMutableDictionaryRef validationFailures;
    CFMutableDictionaryRef authenticationFailures;
    CFMutableDictionaryRef missingDependencies; // whether direct or indirect!
    CFMutableDictionaryRef warnings;

} __KXKext, * __KXKextRef;

/*******************************************************************************
* Private function declarations. Definitions at bottom of this file.
*******************************************************************************/
static void __KXKextInitialize(void);
static CFStringRef __KXKextCopyDebugDescription(CFTypeRef cf);
static KXKextRef __KXKextCreatePrivate(
    CFAllocatorRef       allocator,
    CFAllocatorContext * context);

static void __KXKextResetTestFlags(KXKextRef aKext);
static void __KXKextInitBlank(KXKextRef aKext);

static void __KXKextReleaseContents(CFTypeRef cf);

static Boolean __KXKextCheckLogLevel(
    KXKextRef aKext,
    SInt32 managerLogLevel,
    SInt32 kextLogLevel,
    Boolean exact);

static KXKextManagerError __KXKextValidate(KXKextRef aKext);
static KXKextManagerError __KXKextValidateExecutable(KXKextRef aKext,
    Boolean requiresNewKextManager);

static CFMutableDictionaryRef __KXKextGetOrCreateValidationFailures(KXKextRef aKext);
static CFMutableDictionaryRef __KXKextGetOrCreateAuthenticationFailures(
    KXKextRef aKext);
static CFMutableDictionaryRef __KXKextGetOrCreateWarnings(KXKextRef aKext);

// validation failure array values
static CFMutableArrayRef __KXKextGetMissingProperties(KXKextRef aKext);
static CFMutableArrayRef __KXKextGetIllegalTypeProperties(KXKextRef aKext);
static CFMutableArrayRef __KXKextGetIllegalValueProperties(KXKextRef aKext);

// authentication failure array values
static CFMutableArrayRef __KXKextGetMissingFiles(KXKextRef aKext);
static CFMutableArrayRef __KXKextGetBadOwnerPermsFiles(KXKextRef aKext);

static Boolean __KXKextCheckPropertyType(
    KXKextRef aKext,
    CFDictionaryRef aDictionary,
    CFStringRef propKey,
    CFArrayRef propPathArray,
    Boolean  isRequired,
    CFTypeID expectedType,
    CFTypeRef * rawValueOut);
static Boolean __KXKextCheckStringProperty(
    KXKextRef aKext,
    CFDictionaryRef aDictionary,
    CFStringRef propKey,
    CFArrayRef  propPathArray,
    Boolean     isRequired,
    CFStringRef expectedValue,
    CFStringRef *stringValueOut);
static Boolean __KXKextCheckVersionProperty(
    KXKextRef aKext,
    CFDictionaryRef aDictionary,
    CFStringRef propKey,
    CFArrayRef propPathArray,
    Boolean isRequired,
    char ** versionStringOut,
    VERS_version *versionOut);
static Boolean __KXKextCheckPersonalityTypes(KXKextRef aKext,
    CFTypeRef cfObj, CFMutableArrayRef propPathArray);

static KXKextManagerError __KXKextAuthenticateURLAndParents(KXKextRef aKext,
    CFURLRef anURL,
    CFURLRef topURL /* must be absolute */,
    CFMutableDictionaryRef checkedURLs);
static KXKextManagerError __KXKextAuthenticateURL(KXKextRef aKext,
    CFURLRef anURL);
static KXKextManagerError __KXKextMakeFTSEntrySecure(KXKextRef aKext,
    FTSENT * ftsentry);

static KXKextManagerError __KXKextCheckKmod(KXKextRef aKext,
    Boolean requiresNewKextManager);
static KXKextManagerError __check_kmod_info(KXKextRef aKext,
    struct mach_header * mach_header,
    void * file_end,
    Boolean requiresNewKextManager);

static KXKextManagerError __KXKextResolveDependencies(KXKextRef aKext,
    unsigned int recursionDepth);

static char * __KXKextCopyDgraphEntryName(KXKextRef aKext);
static char * __KXKextCopyDgraphKmodName(KXKextRef aKext);
static Boolean __KXKextAddDependenciesToDgraph(KXKextRef aKext,
    CFArrayRef dependencies,
    dgraph_t * dgraph,
    Boolean    skipKernelDependencies);
static void __KXKextAddDependenciesToArray(KXKextRef aKext,
    CFMutableArrayRef dependencyArray);

KXKextManagerError __KXKextRealizeFromCache(KXKextRef aKext);
CFDictionaryRef __KXKextCopyInfoDictionaryFromCache(
    KXKextRef aKext,
    CFDictionaryRef cDict,
    Boolean makeSubstitutions);
CFDictionaryRef __KXKextCopyInfoDictionaryForCache(
    KXKextRef aKext,
    CFDictionaryRef infoDict,
    Boolean makeSubstitutions);
CFTypeRef __KXKextCopyPListForCache(
    KXKextRef aKext,
    CFTypeRef pList,
    Boolean * error);

/*******************************************************************************
* Core Foundation Class Definition Stuff
*
* Private functions are at the bottom of this file with other module-internal
* code.
*******************************************************************************/

/* This gets set by __KXKextInitialize().
 */
static CFTypeID __kKXKextTypeID = _kCFRuntimeNotATypeID;

CFTypeID KXKextGetTypeID(void) {
    return __kKXKextTypeID;
}

/*******************************************************************************
*
*******************************************************************************/
CFBundleRef KXKextGetBundle(KXKextRef aKext)
{
    if (__KXKextRealizeFromCache(aKext) != kKXKextManagerErrorNone) {
        return NULL;
    }
    return aKext->bundle;
}

/*******************************************************************************
*
*******************************************************************************/
CFDictionaryRef KXKextGetInfoDictionary(KXKextRef aKext)
{
    return aKext->infoDictionary;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextRef KXKextGetPriorVersionKext(KXKextRef aKext)
{
    return aKext->priorVersion;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextRef KXKextGetDuplicateVersionKext(KXKextRef aKext)
{
    return aKext->nextDuplicate;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextRepositoryRef KXKextGetRepository(KXKextRef aKext)
{
    return aKext->repository;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerRef KXKextGetManager(KXKextRef aKext)
{
    return KXKextRepositoryGetManager(aKext->repository);
}

/*******************************************************************************
*
*******************************************************************************/
CFStringRef KXKextGetBundlePathInRepository(KXKextRef aKext)
{
    return aKext->bundlePathInRepository;
}

/*******************************************************************************
*
*******************************************************************************/
CFStringRef KXKextGetBundleDirectoryName(KXKextRef aKext)
{
    return aKext->bundleDirectoryName;
}

/*******************************************************************************
*
*******************************************************************************/
CFStringRef KXKextCopyAbsolutePath(KXKextRef aKext)
{
    CFStringRef absPath = NULL;
    CFStringRef repositoryPath = NULL; // don't release

    repositoryPath = KXKextRepositoryGetPath(aKext->repository);
    if (!repositoryPath) {
        goto finish;
    }

    absPath = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
        CFSTR("%@/%@"), repositoryPath, aKext->bundlePathInRepository);

finish:

    return absPath;
}

/*******************************************************************************
*
*******************************************************************************/
CFURLRef KXKextGetAbsoluteURL(KXKextRef aKext)
{
    return aKext->bundleURL;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean KXKextGetIsKernelResource(KXKextRef aKext)
{
    if (!aKext->flags.isKernelResource) {
        return false;
    }
    if (aKext->flags.declaresExecutable ||
        CFDictionaryGetValue(aKext->infoDictionary,
            CFSTR("OSBundleSharedExecutableIdentifier"))) {

        return 2;
    }
    return 1;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean KXKextGetDeclaresExecutable(KXKextRef aKext)
{
    return aKext->flags.declaresExecutable ? true : false;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean KXKextIsAPlugin(KXKextRef aKext)
{
    return (aKext->container != NULL);
}

/*******************************************************************************
*
*******************************************************************************/
KXKextRef KXKextGetContainerForPluginKext(KXKextRef aKext)
{
    return aKext->container;
}

/*******************************************************************************
*
*******************************************************************************/
CFArrayRef KXKextGetPlugins(KXKextRef aKext)
{
    return (CFArrayRef)aKext->plugins;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean KXKextIsValid(KXKextRef aKext)
{
    return aKext->flags.isValid ? true : false;
}

/*******************************************************************************
*
*******************************************************************************/
CFMutableDictionaryRef KXKextGetValidationFailures(KXKextRef aKext)
{
    return aKext->validationFailures;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean KXKextHasDebugProperties(KXKextRef aKext)
{
    return (aKext->flags.hasIOKitDebugProperty || (aKext->logLevel > 0)) ?
        true : false;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean KXKextIsEligibleDuringSafeBoot(KXKextRef aKext)
{
    return aKext->flags.isEligibleDuringSafeBoot ? true : false;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean KXKextIsEnabled(KXKextRef aKext)
{
    return aKext->flags.isEnabled ? true : false;
}

/*******************************************************************************
*
*******************************************************************************/
CFStringRef KXKextGetBundleIdentifier(KXKextRef aKext)
{
    if (!aKext->infoDictionary) return NULL;

    return (CFStringRef)CFDictionaryGetValue(aKext->infoDictionary,
        CFSTR("CFBundleIdentifier"));
}

/*******************************************************************************
*
*******************************************************************************/
CFDictionaryRef KXKextGetBundleLibraryVersions(KXKextRef aKext)
{
    if (!aKext->infoDictionary) {
        return NULL;
    }
    return CFDictionaryGetValue(aKext->infoDictionary, CFSTR("OSBundleLibraries"));
}

/*******************************************************************************
*
*******************************************************************************/
Boolean KXKextIsCompatibleWithVersionString(
    KXKextRef aKext,
    CFStringRef aVersionString)
{
    char vers_buffer[32];  // more than long enough for legal vers
    VERS_version version;

    if (!aKext->flags.isValid) {
        return false;
    }

    if (aKext->compatibleVersion == 0) {
        return false;
    }

    if (!CFStringGetCString(aVersionString,
        vers_buffer, sizeof(vers_buffer) - 1, kCFStringEncodingUTF8)) {

        return false;
    } else {
        vers_buffer[sizeof(vers_buffer) - 1] = '\0';

        version = VERS_parse_string(vers_buffer);
        if (version < 0) {
                return false;
        }
    }

    if (aKext->compatibleVersion <= version && version <= aKext->version) {
        return true;
    }

    return false;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean KXKextHasPersonalities(KXKextRef aKext)
{
    CFTypeRef       pObj;

    pObj = CFDictionaryGetValue(aKext->infoDictionary, CFSTR("IOKitPersonalities"));
    if (!pObj) {
        return false;
    } else if ((kCFBooleanTrue == pObj) || (kCFBooleanFalse == pObj)) {
        return true;
    } else if (CFDictionaryGetCount((CFDictionaryRef)pObj) == 0) {
        return false;
    }
    return true;
}

/*******************************************************************************
*
*******************************************************************************/
CFDictionaryRef KXKextCopyPersonalities(KXKextRef aKext)
{
    Boolean error = false;
    CFDictionaryRef pDict = NULL;     // don't release
    CFMutableDictionaryRef newPDict = NULL;  // returned
    CFIndex count, i;
    CFStringRef * keys = NULL;   // must free
    CFStringRef * values = NULL; // must free

    newPDict = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (!newPDict) {
        error = true;
        goto finish;
    }

    pDict = (CFDictionaryRef)CFDictionaryGetValue(aKext->infoDictionary,
        CFSTR("IOKitPersonalities"));
    if (!pDict) {
        // not an error to have no personalities
        goto finish;
    }

    if ((kCFBooleanTrue == (CFTypeRef) pDict) || (kCFBooleanFalse == (CFTypeRef) pDict)) {
        KXKextManagerError
        checkResult = __KXKextRealizeFromCache(aKext);
        // We don't care if it's inconsistent, we want the real data from the bundle,
        // but mark the repository so it will rescan
        if (checkResult == kKXKextManagerErrorCache) {
            KXKextRepositorySetNeedsReset(aKext->repository, true);
        } else if (checkResult != kKXKextManagerErrorNone) {
            error = true;
            goto finish;
        }
        pDict = (CFDictionaryRef)CFDictionaryGetValue(aKext->infoDictionary,
            CFSTR("IOKitPersonalities"));
        if (!pDict) {
            // not an error to have no personalities
            goto finish;
        }
    }

    count = CFDictionaryGetCount(pDict);
    keys = (CFStringRef *)malloc(count * sizeof(CFStringRef));
    values = (CFStringRef *)malloc(count * sizeof(CFTypeRef));

    CFDictionaryGetKeysAndValues(pDict, (const void **)keys,
        (const void **)values);

    for (i = 0; i < count; i++) {
        CFDictionaryRef thisPersonality = (CFDictionaryRef)values[i];
        if (CFDictionaryGetValue(thisPersonality, CFSTR("CFBundleIdentifier"))) {
            CFDictionarySetValue(newPDict, keys[i], thisPersonality);
        } else {
            CFMutableDictionaryRef personalityCopy = NULL;  // must release
            personalityCopy = CFDictionaryCreateMutableCopy(kCFAllocatorDefault,
                0 /* capacity limit */, thisPersonality);
            if (!personalityCopy) {
                error = true;
                goto finish;
            }
            CFDictionarySetValue(personalityCopy, CFSTR("CFBundleIdentifier"),
                KXKextGetBundleIdentifier(aKext));
            CFDictionarySetValue(newPDict, keys[i], personalityCopy);
            CFRelease(personalityCopy);
        }
    }

finish:
    if (error) {
        if (newPDict) CFRelease(newPDict);
        newPDict = NULL;
    }
    if (keys)    free(keys);
    if (values)  free(values);

    return newPDict;
}


CFArrayRef KXKextCopyPersonalitiesArray(KXKextRef aKext)
{
    Boolean error = false;
    CFDictionaryRef pDict = NULL;     // don't release
    CFMutableArrayRef newPArray = NULL;  // returned
    CFIndex count, i;
    CFStringRef * values = NULL; // must free

    newPArray = CFArrayCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeArrayCallBacks);
    if (!newPArray) {
        error = true;
        goto finish;
    }

    pDict = (CFDictionaryRef)CFDictionaryGetValue(aKext->infoDictionary,
        CFSTR("IOKitPersonalities"));
    if (!pDict) {
        // not an error to have no personalities
        goto finish;
    }

    if ((kCFBooleanTrue == (CFTypeRef) pDict) || (kCFBooleanFalse == (CFTypeRef) pDict)) {
        KXKextManagerError checkResult = __KXKextRealizeFromCache(aKext);
        // We don't care if it's inconsistent, we want the real data from the bundle,
        // but mark the repository so it will rescan
        if (checkResult == kKXKextManagerErrorCache) {
            KXKextRepositorySetNeedsReset(aKext->repository, true);
        } else if (checkResult != kKXKextManagerErrorNone) {
            error = true;
            goto finish;
        }
        pDict = (CFDictionaryRef)CFDictionaryGetValue(aKext->infoDictionary,
            CFSTR("IOKitPersonalities"));
        if (!pDict) {
            // not an error to have no personalities
            goto finish;
        }
    }

    count = CFDictionaryGetCount(pDict);
    if (!count) {
        goto finish;
    }
    values = (CFStringRef *)malloc(count * sizeof(CFTypeRef));
    if (!values) {
        error = true;
        goto finish;
    }

    CFDictionaryGetKeysAndValues(pDict, NULL, (const void **)values);

    for (i = 0; i < count; i++) {
        CFDictionaryRef thisPersonality = (CFDictionaryRef)values[i];
        if (CFDictionaryGetValue(thisPersonality, CFSTR("CFBundleIdentifier"))) {
            CFArrayAppendValue(newPArray, thisPersonality);
        } else {
            CFMutableDictionaryRef personalityCopy = NULL;  // must release
            personalityCopy = CFDictionaryCreateMutableCopy(kCFAllocatorDefault,
                0 /* capacity limit */, thisPersonality);
            if (!personalityCopy) {
                error = true;
                goto finish;
            }
            CFDictionarySetValue(personalityCopy, CFSTR("CFBundleIdentifier"),
                KXKextGetBundleIdentifier(aKext));
            CFArrayAppendValue(newPArray, personalityCopy);
            CFRelease(personalityCopy);
        }
    }

finish:
    if (error) {
        if (newPArray) CFRelease(newPArray);
        newPArray = NULL;
    }
    if (values)  free(values);

    return newPArray;
}

/*******************************************************************************
*
*******************************************************************************/

KXKextIntegrityState KXKextGetIntegrityState(KXKextRef aKext) {
    return aKext->integrityState;
}


/*******************************************************************************
*
*******************************************************************************/
Boolean  KXKextHasBeenAuthenticated(KXKextRef aKext)
{
    return aKext->flags.hasBeenAuthenticated ? true : false;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError KXKextAuthenticate(KXKextRef aKext)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    KXKextManagerError checkResult = kKXKextManagerErrorNone;
    CFMutableDictionaryRef checkedURLs = NULL;    // must release
    CFStringRef bundlePath = NULL;    // must release
    CFURLRef infoDictURL = NULL;      // must release
    CFURLRef backscanURL = NULL;      // must release
    CFURLRef executableURL = NULL;    // must release


    aKext->flags.isAuthentic = 0;

    checkedURLs = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
    if (!checkedURLs) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

   /*****
    * Pre-authentication checks.
    */
    checkResult = __KXKextRealizeFromCache(aKext);
    if (checkResult != kKXKextManagerErrorNone) {
        result = checkResult;
        // there can be no further test at this point
        goto finish;
    }

    if (!aKext->flags.canAuthenticate) {
        result = kKXKextManagerErrorAuthentication;
        // there can be no further test at this point
        goto finish;
    }

    if (_KXKextManagerCheckLogLevel(KXKextGetManager(aKext), kKXKextManagerLogLevelKexts,
        aKext, kKXKextLogLevelBasic)) {

        const char * kext_name = _KXKextCopyCanonicalPathnameAsCString(aKext);
        if (kext_name) {
            _KXKextManagerLogMessage(KXKextGetManager(aKext),
                "authenticating extension %s", kext_name);
            free((char *)kext_name);
        }
    }

    if (!__KXKextGetOrCreateAuthenticationFailures(aKext)) {
        if (!aKext->authenticationFailures) {
            result = kKXKextManagerErrorNoMemory;
            goto finish;
        }
    }
    CFDictionaryRemoveAllValues(aKext->authenticationFailures);

   /*****
    * Authenticate the bundle's URL.
    */
    if (!aKext->bundleURL) {
        result = kKXKextManagerErrorInvalidArgument;
        goto finish;
    }

    if (_KXKextManagerCheckLogLevel(KXKextGetManager(aKext), kKXKextManagerLogLevelKextDetails,
        aKext, kKXKextLogLevelDetails)) {

        const char * bundle_path =
            PATH_CanonicalizedCStringForURL(aKext->bundleURL);
        if (bundle_path) {
            _KXKextManagerLogMessage(KXKextGetManager(aKext),
                "authenticating bundle directory %s",
                bundle_path);
            free((char *)bundle_path);
        }
    }

    checkResult = __KXKextAuthenticateURL(aKext, aKext->bundleURL);
    if (checkResult != kKXKextManagerErrorNone) {
        result = checkResult;
        if (result == kKXKextManagerErrorNoMemory ||
            !KXKextManagerPerformsFullTests(aKext->manager)) {

            goto finish;
        }
    }

   /* Note that we've checked the bundle URL.
    */
    CFDictionarySetValue(checkedURLs, aKext->bundleURL, kCFBooleanTrue);

   /*****
    * Old-style authentication checked only the bundle directory. New-style
    * authentication checks the info dictionary file, the executable, and
    * every filesystem node from them up to the bundle.
    */
    if (!KXKextManagerPerformsStrictAuthentication(aKext->manager)) {
        aKext->flags.isAuthentic = 1;
        goto finish;
    }

   /*****
    * Authenticate the bundle's info dictionary file and containing directory.
    */
    infoDictURL = _CFBundleCopyInfoPlistURL(aKext->bundle);
    if (!infoDictURL) {
        result = kKXKextManagerErrorUnspecified;
        goto finish;
    }

    checkResult = __KXKextAuthenticateURLAndParents(aKext, infoDictURL,
        aKext->bundleURL, checkedURLs);
    if (checkResult != kKXKextManagerErrorNone) {
        result = checkResult;
        if (result == kKXKextManagerErrorNoMemory ||
            !KXKextManagerPerformsFullTests(aKext->manager)) {

            goto finish;
        }
    }

   /*****
    * Authenticate the bundle's executable and containing directory.
    */
    if (aKext->flags.declaresExecutable) {

        executableURL = CFBundleCopyExecutableURL(aKext->bundle);

       /*****
        * This is unfortunate, but necessary. In order to spare disk I/O and
        * file stats during validation, we may not know if the executable is
        * even present. So we have to check here and possible return a
        * VALIDATION error from the authentication function.
        */
        if (!executableURL) {
            CFDictionarySetValue(__KXKextGetOrCreateValidationFailures(aKext),
                kKXKextErrorKeyMissingExecutable,
                kCFBooleanTrue);
            result = kKXKextManagerErrorValidation;
            if (!KXKextManagerPerformsFullTests(aKext->manager)) {
                goto finish;
            }
        } else {
            checkResult = __KXKextAuthenticateURLAndParents(aKext, executableURL,
                aKext->bundleURL, checkedURLs);
            if (checkResult != kKXKextManagerErrorNone) {
                result = checkResult;
                if (result == kKXKextManagerErrorNoMemory ||
                    !KXKextManagerPerformsFullTests(aKext->manager)) {

                    goto finish;
                }
            }
        }
    }

   /*****
    * All tests passed, yay.
    */
    if (result == kKXKextManagerErrorNone) {
        aKext->flags.isAuthentic = 1;
    }

finish:

    aKext->flags.hasBeenAuthenticated = 1;

    if (checkedURLs)      CFRelease(checkedURLs);
    if (bundlePath)       CFRelease(bundlePath);
    if (infoDictURL)      CFRelease(infoDictURL);
    if (backscanURL)      CFRelease(backscanURL);
    if (executableURL)    CFRelease(executableURL);

    if (_KXKextManagerCheckLogLevel(KXKextGetManager(aKext), kKXKextManagerLogLevelKexts,
        aKext, kKXKextLogLevelBasic)) {

        const char * kext_name = _KXKextCopyCanonicalPathnameAsCString(aKext);
        if (kext_name) {
            _KXKextManagerLogMessage(KXKextGetManager(aKext),
               "extension %s is%s authentic",
               kext_name, aKext->flags.isAuthentic ? "" : " not");
            free((char *)kext_name);
        }
    }

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError KXKextMarkAuthentic(KXKextRef aKext)
{
    KXKextManagerError result = kKXKextManagerErrorNone;

    if (_KXKextManagerCheckLogLevel(KXKextGetManager(aKext), kKXKextManagerLogLevelKexts,
        aKext, kKXKextLogLevelBasic)) {

        const char * kext_name = _KXKextCopyCanonicalPathnameAsCString(aKext);
        if (kext_name) {
            _KXKextManagerLogMessage(KXKextGetManager(aKext),
                "marking extension %s authentic",
                kext_name);
            free((char *)kext_name);
        }
    }

    aKext->flags.isAuthentic = 1;
    aKext->flags.hasBeenAuthenticated = 1;

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean  KXKextIsAuthentic(KXKextRef aKext)
{
    return aKext->flags.isAuthentic ? true : false;
}

/*******************************************************************************
*
*******************************************************************************/
CFMutableDictionaryRef KXKextGetAuthenticationFailures(KXKextRef aKext)
{
    return aKext->authenticationFailures;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError KXKextResolveDependencies(KXKextRef aKext)
{
    if (_KXKextManagerCheckLogLevel(KXKextGetManager(aKext), kKXKextManagerLogLevelKexts,
        aKext, kKXKextLogLevelBasic)) {

        const char * kext_name = _KXKextCopyCanonicalPathnameAsCString(aKext);
        if (kext_name) {
            _KXKextManagerLogMessage(KXKextGetManager(aKext),
                "resolving dependencies for extension %s",
                kext_name);
            free((char *)kext_name);
        }
    }

    return __KXKextResolveDependencies(aKext, 0);
}

/*******************************************************************************
*
*******************************************************************************/
Boolean KXKextGetHasAllDependencies(KXKextRef aKext)
{
    return aKext->flags.hasAllDependencies ? true : false;
}

/*******************************************************************************
*
*******************************************************************************/
CFArrayRef KXKextGetDirectDependencies(KXKextRef aKext)
{
    return aKext->directDependencies;
}

/*******************************************************************************
*
*******************************************************************************/
CFMutableArrayRef KXKextCopyAllDependencies(KXKextRef aKext)
{
    CFMutableArrayRef allDependencies = NULL;

    allDependencies = CFArrayCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeArrayCallBacks);
    if (!allDependencies) {
        goto finish;
    }

    __KXKextAddDependenciesToArray(aKext, allDependencies);

finish:
    if (allDependencies && !CFArrayGetCount(allDependencies)) {
        CFRelease(allDependencies);
        allDependencies = NULL;
    }
    return allDependencies;
}

/*******************************************************************************
*
*******************************************************************************/
CFMutableArrayRef KXKextCopyIndirectDependencies(KXKextRef aKext)
{
    CFMutableArrayRef indirectDependencies = NULL;
    CFIndex count, i;

    if (!aKext->flags.hasAllDependencies || !aKext->directDependencies) {
        goto finish;
    }

    indirectDependencies = CFArrayCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeArrayCallBacks);
    if (!indirectDependencies) {
        goto finish;
    }

    count = CFArrayGetCount(aKext->directDependencies);
    for (i = 0; i < count; i++) {
        KXKextRef thisDependency = (KXKextRef)CFArrayGetValueAtIndex(
            aKext->directDependencies, i);
        __KXKextAddDependenciesToArray(thisDependency, indirectDependencies);
    }

finish:
    if (indirectDependencies && !CFArrayGetCount(indirectDependencies)) {
        CFRelease(indirectDependencies);
        indirectDependencies = NULL;
    }
    return indirectDependencies;
}

/*******************************************************************************
*
*******************************************************************************/
CFMutableArrayRef KXKextCopyAllDependents(KXKextRef aKext)
{
    CFMutableArrayRef allDependents = NULL;  // returned
    CFArrayRef allKexts = NULL;  // must release
    CFArrayRef kextDependencies = NULL;  // must release
    CFIndex count, i;

    allDependents = CFArrayCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeArrayCallBacks);
    if (!allDependents) {
        goto finish;
    }

    allKexts = KXKextManagerCopyAllKexts(KXKextGetManager(aKext));
    if (!allKexts) {
        goto finish;
    }

    count = CFArrayGetCount(allKexts);
    for (i = 0; i < count; i++) {
        KXKextRef thisKext = (KXKextRef)CFArrayGetValueAtIndex(allKexts, i);

        if (kextDependencies) {
            CFRelease(kextDependencies);
            kextDependencies = NULL;
        }
        kextDependencies = KXKextCopyAllDependencies(thisKext);
        if (!kextDependencies) {
            continue;
        }
        if (kCFNotFound != CFArrayGetFirstIndexOfValue(kextDependencies,
            CFRangeMake(0, CFArrayGetCount(kextDependencies)), aKext)) {

            CFArrayAppendValue(allDependents, thisKext);
        }
    }

finish:
    if (allKexts)         CFRelease(allKexts);
    if (kextDependencies) CFRelease(kextDependencies);

    if (allDependents && !CFArrayGetCount(allDependents)) {
        CFRelease(allDependents);
        allDependents = NULL;
    }
    return allDependents;
}

/*******************************************************************************
*
*******************************************************************************/
CFMutableDictionaryRef KXKextGetWarnings(KXKextRef aKext)
{
    return aKext->warnings;
}

/*******************************************************************************
*
*******************************************************************************/
CFDictionaryRef KXKextGetMissingDependencyErrors(KXKextRef aKext)
{
    return aKext->missingDependencies;
}

 /*******************************************************************************
 *
 *******************************************************************************/
Boolean KXKextSupportsHostArchitecture(KXKextRef aKext)
{
    Boolean result = false;
    fat_iterator fiter = NULL;       // must close

    fiter = _KXKextCopyFatIterator(aKext);
     if (!fiter) {
        goto finish;
    }

    if (fat_iterator_find_host_arch(fiter, NULL) != NULL) {
        result = true;
    }

finish:
    if (fiter)           fat_iterator_close(fiter);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean KXKextIsLoadable(KXKextRef aKext, Boolean safeBoot)
{
    Boolean result = (
        (aKext->flags.isValid) &&
        (aKext->flags.isEligibleDuringSafeBoot || !safeBoot) &&
        (aKext->flags.hasBeenAuthenticated && aKext->flags.isAuthentic) &&
        aKext->flags.isEnabled &&
        aKext->flags.hasAllDependencies);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean KXKextIsLoaded(KXKextRef aKext)
{
    return aKext->flags.isLoaded ? true : false;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean KXKextOtherVersionIsLoaded(KXKextRef aKext)
{
    return aKext->flags.otherVersionIsLoaded ? true : false;
}

vm_address_t KXKextGetStartAddress(KXKextRef aKext) {
    return aKext->startAddress;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean KXKextIsFromCache(KXKextRef aKext)
{
    if (aKext->bundle) return false;
    return true;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean KXKextGetLoadFailed(KXKextRef aKext)
{
    return aKext->flags.loadFailed ? true : false;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextSetLoadFailed(KXKextRef aKext, Boolean flag)
{
    aKext->flags.loadFailed = (flag ? 1 : 0);
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextPrintDiagnostics(KXKextRef aKext,
    FILE * stream)
{
    CFDictionaryRef validationFailures = NULL;      // don't release
    CFDictionaryRef authenticationFailures = NULL;  // don't release
    CFDictionaryRef missingDependencies = NULL;     // don't release
    CFDictionaryRef warnings = NULL;                // don't release

    if (!stream) {
        stream = stdout;
    }

    validationFailures = KXKextGetValidationFailures(aKext);
    authenticationFailures = KXKextGetAuthenticationFailures(aKext);
    missingDependencies = KXKextGetMissingDependencyErrors(aKext);
    warnings = KXKextGetWarnings(aKext);

    if (validationFailures && CFDictionaryGetCount(validationFailures)) {
        fprintf(stream, "Validation failures:\n");
        printPList(stream, validationFailures);
    }
    if (authenticationFailures && CFDictionaryGetCount(authenticationFailures)) {
        fprintf(stream, "Authentication failures:\n");
        printPList(stream, authenticationFailures);
    }
    if (missingDependencies && CFDictionaryGetCount(missingDependencies)) {
        fprintf(stream, "Missing dependencies:\n");
        printPList(stream, missingDependencies);
    }
    if (warnings && CFDictionaryGetCount(warnings)) {
        fprintf(stream, "Warnings:\n");
        printPList(stream, warnings);
    }

    fflush(stream);
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextPrintWarnings(KXKextRef aKext,FILE * stream)
{
    CFDictionaryRef warnings = NULL;                // don't release

    if (!stream) {
        stream = stdout;
    }

    warnings = KXKextGetWarnings(aKext);

    if (warnings && CFDictionaryGetCount(warnings)) {
        fprintf(stream, "Warnings\n");
        printPList(stream, warnings);
    }

    fflush(stream);
    return;
}

/*******************************************************************************
********************************************************************************
* FRAMEWORK-PRIVATE API BELOW HERE
********************************************************************************
*******************************************************************************/

/*******************************************************************************
*
*******************************************************************************/
KXKextRef _KXKextCreate(CFAllocatorRef alloc)
{
    KXKextRef newKext = NULL;

    newKext = __KXKextCreatePrivate(alloc, NULL);
    if (!newKext) {
        goto finish;
    }

finish:
    return (KXKextRef)newKext;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError _KXKextInitWithBundlePathInManager(
    KXKextRef aKext,
    CFStringRef aBundlePath,
    KXKextManagerRef aManager)
{
    KXKextManagerError result = kKXKextManagerErrorNone;

    CFURLRef bundleURL = NULL;   // must release

    bundleURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
        aBundlePath, kCFURLPOSIXPathStyle, true);
    if (!bundleURL) {
        // If the bundleURL couldn't be created we don't know why
        // without further investigation. We could be out of memory;
        // the entry in the filesystem might not exist; we might not
        // have access to it.
        //
        result = kKXKextManagerErrorUnspecified;
        goto finish;
    }

    result = _KXKextInitWithURLInManager(aKext, bundleURL,
        aManager);

finish:
    if (bundleURL)  CFRelease(bundleURL);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError _KXKextInitWithURLInManager(
    KXKextRef aKext,
    CFURLRef anURL,
    KXKextManagerRef aManager)
{
    KXKextManagerError result = kKXKextManagerErrorNone;

    CFURLRef scratchURL = NULL;  // must release
    KXKextRepositoryRef theRepository = NULL;  // don't release

    theRepository = KXKextManagerGetRepositoryForKextWithURL(aManager,
        anURL);
    if (!theRepository) {
        // drop the kext name at the end of the path to get the repository
        scratchURL = CFURLCreateCopyDeletingLastPathComponent(
            kCFAllocatorDefault, anURL);
        if (!scratchURL) {
            goto finish;
        }
        result = KXKextManagerAddRepositoryDirectory(aManager,
            scratchURL, false, false, &theRepository);
        if (result != kKXKextManagerErrorNone) {
            goto finish;
        }
    }

    result = _KXKextInitWithURLInRepository(aKext, anURL,
        theRepository);

finish:
    if (scratchURL) CFRelease(scratchURL);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError _KXKextInitWithBundleInManager(
    KXKextRef aKext,
    CFBundleRef aBundle,
    KXKextManagerRef aManager)
{
    KXKextManagerError result = kKXKextManagerErrorNone;

    CFURLRef bundleURL = NULL;   // must release
    CFURLRef scratchURL = NULL;  // must release
    KXKextRepositoryRef theRepository = NULL;  // don't release

    bundleURL = CFBundleCopyBundleURL(aBundle);

    theRepository = KXKextManagerGetRepositoryForKextWithURL(aManager,
        bundleURL);
    if (!theRepository) {
        // drop the kext name at the end of the path to get the repository
        scratchURL = CFURLCreateCopyDeletingLastPathComponent(
            kCFAllocatorDefault, bundleURL);
        if (!scratchURL) {
            goto finish;
        }
        result = KXKextManagerAddRepositoryDirectory(aManager,
            scratchURL, false, false, &theRepository);
        if (result != kKXKextManagerErrorNone) {
            goto finish;
        }
    }

    result = _KXKextInitWithBundleInRepository(aKext, aBundle,
        theRepository);

finish:
    if (bundleURL)  CFRelease(bundleURL);
    if (scratchURL) CFRelease(scratchURL);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError _KXKextInitWithBundlePathInRepository(
    KXKextRef aKext,
    CFStringRef aBundlePath,
    KXKextRepositoryRef aRepository)
{
    KXKextManagerError result = kKXKextManagerErrorNone;

    CFURLRef bundleURL = NULL;  // must release

   /* Set this right away so we can refer to it in case of failure.
    */
    aKext->repository = aRepository; // do not retain up pointers!

    bundleURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
        aBundlePath, kCFURLPOSIXPathStyle, true);
    if (!bundleURL) {
        // If the bundleURL couldn't be created we don't know why
        // without further investigation. We could be out of memory;
        // the entry in the filesystem might not exist; we might not
        // have access to it.
        //
        result = kKXKextManagerErrorUnspecified;
        goto finish;
    }

    result = _KXKextInitWithURLInRepository(aKext, bundleURL,
        aRepository);

finish:
    CFRelease(bundleURL);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError _KXKextInitWithURLInRepository(
    KXKextRef aKext,
    CFURLRef anURL,
    KXKextRepositoryRef aRepository)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    CFBundleRef aBundle = NULL;    // must release
    const char * bundle_path = NULL; // must free

   /* Set this right away so we can refer to it in case of failure.
    */
    aKext->repository = aRepository; // do not retain up pointers!

    // FIXME: Check for existence of directory!!!

    aBundle = CFBundleCreate(kCFAllocatorDefault, anURL);
    if (!aBundle) {
        struct stat stat_buf;

        bundle_path = PATH_CanonicalizedCStringForURL(anURL);
        if (!bundle_path) {
            result = kKXKextManagerErrorNoMemory;
            goto finish;
        }

        if (stat(bundle_path, &stat_buf) != 0) {
            if (errno == ENOENT || errno == ENOTDIR) {
                _KXKextManagerLogError(KXKextRepositoryGetManager(aRepository),
                    "%s: no such bundle file exists", bundle_path);
                result = kKXKextManagerErrorFileAccess;
                goto finish;
            } else if (errno == EACCES) {
                _KXKextManagerLogError(KXKextRepositoryGetManager(aRepository),
                    "%s: permission denied", bundle_path);
                result = kKXKextManagerErrorFileAccess;
                goto finish;
            } else {
                _KXKextManagerLogError(KXKextRepositoryGetManager(aRepository),
                    "%s: can't create bundle", bundle_path);
                result = kKXKextManagerErrorUnspecified;
                goto finish;
            }
        }

        if ( !(stat_buf.st_mode & S_IFDIR) ) {
            _KXKextManagerLogError(KXKextRepositoryGetManager(aRepository),
                "%s is not a directory", bundle_path);
            result = kKXKextManagerErrorNotADirectory;
            goto finish;
        }

        result = kKXKextManagerErrorUnspecified;
        goto finish;
    }

    result = _KXKextInitWithBundleInRepository(aKext,
        aBundle, aRepository);

finish:
    if (aBundle)     CFRelease(aBundle);
    if (bundle_path) free((char *)bundle_path);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError _KXKextInitWithBundleInRepository(
    KXKextRef aKext,
    CFBundleRef aBundle,
    KXKextRepositoryRef aRepository)
{
    KXKextManagerError result = kKXKextManagerErrorNone;

    CFURLRef    bundleRelativeURL = NULL;      // must release
    CFStringRef repositoryPath = NULL;         // must release
    CFStringRef bundlePath = NULL;             // must release
    CFStringRef bundleExtension = NULL;        // must release

    CFStringRef bundleRepositoryPath = NULL;   // don't release

    if (!aKext || !aBundle || !aRepository) {
        result = kKXKextManagerErrorInvalidArgument;
        goto finish;
    }

   /*****
    * Blank out all the fields that haven't yet been set.
    */
    __KXKextInitBlank(aKext);


   /*****
    * Now set all the ones that can have values.
    */

   /* Save the repository and the bundle.
    */
    aKext->manager = KXKextRepositoryGetManager(aRepository);
    aKext->repository = aRepository; // do not retain up pointers!
    aKext->bundle = (CFBundleRef)CFRetain(aBundle);

   /* Figure out the absolute URL for the bundle.
    */
    bundleRelativeURL = CFBundleCopyBundleURL(aKext->bundle);
    if (!bundleRelativeURL) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }
    aKext->bundleURL = PATH_CopyCanonicalizedURL(bundleRelativeURL);
    if (!aKext->bundleURL) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

   /* Get the kext's bare directory name and its path relative to the
    * repository.
    */
    aKext->bundleDirectoryName = CFURLCopyLastPathComponent(aKext->bundleURL);
    if (!aKext->bundleDirectoryName) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    repositoryPath = KXKextRepositoryGetPath(aKext->repository);
    if (!repositoryPath) {
        result = kKXKextManagerErrorUnspecified;
        goto finish;
    }

    bundlePath = CFURLCopyFileSystemPath(aKext->bundleURL, kCFURLPOSIXPathStyle);
    if (!bundlePath) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    bundleRepositoryPath = CFStringCreateWithSubstring(kCFAllocatorDefault,
        bundlePath, CFRangeMake(0, CFStringGetLength(repositoryPath)));
    if (!bundleRepositoryPath) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    aKext->bundlePathInRepository =
        CFStringCreateWithSubstring(kCFAllocatorDefault, bundlePath,
        CFRangeMake(CFStringGetLength(repositoryPath) + 1,
        CFStringGetLength(bundlePath) - CFStringGetLength(repositoryPath) - 1));
    if (!aKext->bundlePathInRepository) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

   /* Check whether kext is in repository directory.
    */
    if (CFStringCompare(repositoryPath, bundleRepositoryPath, 0) !=
            kCFCompareEqualTo) {

        CFDictionarySetValue(__KXKextGetOrCreateValidationFailures(aKext),
            kKXKextErrorKeyBundleNotInRepository,
            kCFBooleanTrue);
        result = kKXKextManagerErrorURLNotInRepository;
        goto finish;
    }

   /* Now check whether this thing is even a proper bundle.
    */
    if (!CFBundleGetIdentifier(aKext->bundle)) {
        CFDictionarySetValue(__KXKextGetOrCreateValidationFailures(aKext),
            kKXKextErrorKeyNotABundle,
            kCFBooleanTrue);
        result = kKXKextManagerErrorNotABundle;
        goto finish;
    }

    aKext->infoDictionary = CFBundleGetInfoDictionary(aKext->bundle);
    if (!aKext->infoDictionary) {
        CFDictionarySetValue(__KXKextGetOrCreateValidationFailures(aKext),
            kKXKextErrorKeyNotABundle,
            kCFBooleanTrue);
        result = kKXKextManagerErrorNotABundle;
        goto finish;
    } else {
        CFRetain(aKext->infoDictionary);
    }

    bundleExtension = CFURLCopyPathExtension(aKext->bundleURL);

    // FIXME: Need to check for "kext/" too? Maybe check for a
    // FIXME: ...proper extension before doing all this work?
    if (!bundleExtension ||
        CFStringCompare(CFSTR("kext"), bundleExtension, 0) !=
        kCFCompareEqualTo) {

        CFDictionarySetValue(__KXKextGetOrCreateValidationFailures(aKext),
            kKXKextErrorKeyNotAKextBundle,
            kCFBooleanTrue);
        result = kKXKextManagerErrorNotAKext;
        goto finish;
    }

    result = __KXKextValidate(aKext);

finish:
    if (bundleRelativeURL)      CFRelease(bundleRelativeURL);
    if (bundlePath)             CFRelease(bundlePath);
    if (bundleRepositoryPath)   CFRelease(bundleRepositoryPath);
    if (bundleExtension)        CFRelease(bundleExtension);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError _KXKextInitWithCacheDictionaryInRepository(
    KXKextRef aKext,
    CFDictionaryRef aDictionary,
    KXKextRepositoryRef aRepository)
{
    KXKextManagerError result = kKXKextManagerErrorNone;

    CFStringRef repositoryPath = NULL;  // don't release
    CFStringRef bundlePath = NULL;      // must release
    CFStringRef bundleExtension = NULL; // must release

    CFDictionaryRef cacheInfoDictionary = NULL;  // don't release

    CFArrayRef plugins = NULL;          // don't release
    CFIndex count, i;

    if (!aKext || !aDictionary || !aRepository) {
        result = kKXKextManagerErrorInvalidArgument;
        goto finish;
    }

   /* Blank out all the fields that haven't yet been set.
    */
    __KXKextInitBlank(aKext);


   /*****
    * Now set all the ones that can have values.
    */

   /* Save the repository and the bundle.
    */
    aKext->manager = KXKextRepositoryGetManager(aRepository);
    aKext->repository = aRepository; // do not retain up pointers!
    aKext->bundle = NULL;  // created when needed

    repositoryPath = KXKextRepositoryGetPath(aKext->repository);
    aKext->bundlePathInRepository = (CFStringRef)CFDictionaryGetValue(aDictionary,
        CFSTR("bundlePathInRepository"));
    if (!aKext->bundlePathInRepository ||
        CFGetTypeID(aKext->bundlePathInRepository) != CFStringGetTypeID()) {

        result = kKXKextManagerErrorInvalidArgument;
        goto finish;
    }
    CFRetain(aKext->bundlePathInRepository);

    cacheInfoDictionary = (CFDictionaryRef)CFDictionaryGetValue(aDictionary,
        CFSTR("infoDictionary"));
    if (!cacheInfoDictionary ||
        CFGetTypeID(cacheInfoDictionary) != CFDictionaryGetTypeID()) {

        result = kKXKextManagerErrorInvalidArgument;
        goto finish;
    }
    aKext->infoDictionary = __KXKextCopyInfoDictionaryFromCache(
        aKext, cacheInfoDictionary, false /* make key subs */);
    if (!aKext->infoDictionary) {

        result = kKXKextManagerErrorCache;
        goto finish;
    }

   /* Figure out the absolute URL for the bundle.
    */
    bundlePath = CFStringCreateWithFormat(kCFAllocatorDefault, NULL /* options */,
        CFSTR("%@/%@"), repositoryPath, aKext->bundlePathInRepository);

    aKext->bundleURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
        bundlePath, kCFURLPOSIXPathStyle, true);
    if (!aKext->bundleURL) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

   /* Get the kext's bare directory name and its path relative to the
    * repository.
    */
    aKext->bundleDirectoryName = CFURLCopyLastPathComponent(aKext->bundleURL);
    if (!aKext->bundleDirectoryName) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    bundleExtension = CFURLCopyPathExtension(aKext->bundleURL);

    // FIXME: Need to check for "kext/" too? Maybe check for a
    // FIXME: ...proper extension before doing all this work?
    if (!bundleExtension ||
        CFStringCompare(CFSTR("kext"), bundleExtension, 0) !=
        kCFCompareEqualTo) {

        CFDictionarySetValue(__KXKextGetOrCreateValidationFailures(aKext),
            kKXKextErrorKeyNotAKextBundle,
            kCFBooleanTrue);
        result = kKXKextManagerErrorNotAKext;
        goto finish;
    }

    result = __KXKextValidate(aKext);
    if (result != kKXKextManagerErrorNone) {
        goto finish;
    }

   /* Plugins aren't allowed to have plugins!
    */
    if (aKext->container) goto finish;

   /* Check for plugins.
    */
    plugins = (CFArrayRef)CFDictionaryGetValue(aDictionary,
        CFSTR("plugins"));
    if (!plugins) {
        goto finish;
    }

    if (CFGetTypeID(plugins) != CFArrayGetTypeID()) {
        result = kKXKextManagerErrorInvalidArgument;
        goto finish;
    }

   /* Load up the plugins.
    */
    count = CFArrayGetCount(plugins);
    for (i = 0; i < count; i++) {
        CFDictionaryRef pDict = (CFDictionaryRef)CFArrayGetValueAtIndex(
            plugins, i);
        KXKextRef newKext = NULL;  // must release
        KXKextManagerError kextResult = kKXKextManagerErrorNone;
        CFURLRef kextURL = NULL;  // must release
        char * kext_path = NULL;  // must free

        newKext = _KXKextCreate(kCFAllocatorDefault);
        if (!newKext) {
            result = kKXKextManagerErrorNoMemory;
            goto finish;
        }
        kextResult = _KXKextInitWithCacheDictionaryInRepository(newKext, pDict,
            aRepository);

        kextURL = KXKextGetAbsoluteURL(newKext);  // don't release
        kext_path = NULL;  // must free
        if (kextURL) { 
            kext_path = PATH_CanonicalizedCStringForURL(kextURL);
        }

        if (kextResult != kKXKextManagerErrorNone) {
            _KXKextRepositoryAddBadKext(aRepository, newKext);

            if (_KXKextManagerCheckLogLevel(KXKextGetManager(aKext), kKXKextManagerLogLevelDetails,
                NULL, 0)) {

                _KXKextManagerLogMessage(KXKextGetManager(aKext),
                    "added invalid cached kernel extension %s",
                    kext_path ? kext_path : "(unknown)");
            }
        } else {
            _KXKextRepositoryAddKext(aRepository, newKext);
            if (_KXKextManagerCheckLogLevel(KXKextGetManager(aKext), kKXKextManagerLogLevelDetails,
                NULL, 0)) {

                _KXKextManagerLogMessage(KXKextGetManager(aKext),
                    "added cached kernel extension %s",
                    kext_path ? kext_path : "(unknown)");
            }
        }
        _KXKextAddPlugin(aKext, newKext);
        CFRelease(newKext);
        newKext = NULL;
        if (kext_path) free(kext_path);
    }


finish:
    if (bundlePath)             CFRelease(bundlePath);
    if (bundleExtension)        CFRelease(bundleExtension);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextSetPriorVersionKext(KXKextRef aKext, KXKextRef thatKext)
{
    KXKextRef oldPriorVersion = NULL;

    oldPriorVersion = aKext->priorVersion;
    aKext->priorVersion = thatKext;
    if (aKext->priorVersion) CFRetain(aKext->priorVersion);
    if (oldPriorVersion) CFRelease(oldPriorVersion);

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextClearVersionRelationships(KXKextRef aKext)
{
    if (aKext->priorVersion) {
        CFRelease(aKext->priorVersion);
        aKext->priorVersion = NULL;
    }

    if (aKext->nextDuplicate) {
        CFRelease(aKext->nextDuplicate);
        aKext->nextDuplicate = NULL;
    }

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextClearDependencies(KXKextRef aKext)
{
    aKext->flags.hasAllDependencies = 0;

    if (aKext->directDependencies) {
        CFRelease(aKext->directDependencies);
        aKext->directDependencies = NULL;
    }

    if (aKext->missingDependencies) {
        CFRelease(aKext->missingDependencies);
        aKext->missingDependencies = NULL;
    }

    return;
}

/*******************************************************************************
*
*******************************************************************************/
char * _KXKextCopyCanonicalPathnameAsCString(KXKextRef aKext)
{
    char * abs_path = NULL;      // returned
    CFStringRef absPath = NULL;  // must release
    CFIndex pathSize;
    Boolean error = false;

    absPath = KXKextCopyAbsolutePath(aKext);
    if (!absPath) {
        goto finish;
    }

    pathSize = 1 + CFStringGetMaximumSizeOfFileSystemRepresentation(absPath);
    abs_path = (char *)malloc(pathSize * sizeof(char));
    if (!abs_path) {
        goto finish;
    }

    if (!CFStringGetFileSystemRepresentation(absPath, abs_path, pathSize)) {
        error = true;
    }

finish:
    if (absPath) CFRelease(absPath);
    if (error && abs_path) {
        free(abs_path);
        abs_path = NULL;
    }
    return abs_path;
}

/*******************************************************************************
*
*******************************************************************************/
char * _KXKextCopyBundlePathInRepositoryAsCString(KXKextRef aKext)
{
    char * path = NULL;             // returned
    CFStringRef bundlePath = NULL;  // don't release
    CFIndex pathSize;
    Boolean error = false;

    bundlePath = KXKextGetBundlePathInRepository(aKext);
    if (!bundlePath) {
        goto finish;
    }

    pathSize = 1 + CFStringGetMaximumSizeOfFileSystemRepresentation(bundlePath);
    path = (char *)malloc(pathSize * sizeof(char));
    if (!path) {
        goto finish;
    }

    if (!CFStringGetFileSystemRepresentation(bundlePath, path, pathSize)) {

        error = true;
    }

finish:
    if (error && path) {
        free(path);
        path = NULL;
    }
    return path;
}

/*******************************************************************************
*
*******************************************************************************/
char * _KXKextCopyExecutableCanonicalPathnameAsCString(KXKextRef aKext)
{
    char * abs_path = NULL;      // returned
    KXKextManagerError checkResult = kKXKextManagerErrorNone;
    CFURLRef executableURL = 0;      // must release

    checkResult = __KXKextRealizeFromCache(aKext);
    if (checkResult != kKXKextManagerErrorNone) {
        goto finish;
    }

    executableURL = CFBundleCopyExecutableURL(aKext->bundle);
    if (!executableURL) {
        goto finish;
    }

    abs_path = PATH_CanonicalizedCStringForURL(executableURL);

finish:
    if (executableURL) CFRelease(executableURL);
    return abs_path;
}

/*******************************************************************************
*
*******************************************************************************/
fat_iterator _KXKextCopyFatIterator(KXKextRef aKext)
{
    fat_iterator fiter = NULL;       // returned
    char * module_path = NULL;       // must free

    module_path = _KXKextCopyExecutableCanonicalPathnameAsCString(aKext);
    if (!module_path) {
        goto finish;
    }

    fiter = fat_iterator_open(module_path, 1 /* mach-o only */);
    if (!fiter) {
        goto finish;
    }

finish:
    if (module_path) free(module_path);
    return fiter;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerLogLevel _KXKextGetLogLevel(KXKextRef aKext)
{
    return aKext->logLevel;
}

/*******************************************************************************
*
*******************************************************************************/
VERS_version _KXKextGetVersion(KXKextRef aKext)
{
    return aKext->version;
}

/*******************************************************************************
*
*******************************************************************************/
VERS_version _KXKextGetCompatibleVersion(KXKextRef aKext)
{
    return aKext->compatibleVersion;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean _KXKextIsCompatibleWithVersionNumber(
    KXKextRef aKext,
    VERS_version version)
{
    if (!aKext->flags.isValid) {
        return false;
    }

    if (aKext->compatibleVersion == 0) {
        return false;
    }

    if (aKext->compatibleVersion <= version && version <= aKext->version) {
        return true;
    }

    return false;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean _KXKextAddPriorOrDuplicateVersionKext(KXKextRef aKext,
    KXKextRef priorKext)
{
    KXKextRef peekingKext = NULL;
    KXKextRef insertionKext = NULL;

// FIXME: If a kext's compatibility range overlaps another's, then
// FIXME: ...toss the one with the older version (unless it's already
// FIXME: ...loaded (but that's hard to determine)).
// FIXME: ...Will also need a way to note this problem in the kext being
// FIXME: ...disqualified. Consider a validation failure?

   /* Kext being added must not have any prior versions of its own.
    */
    if (KXKextGetPriorVersionKext(priorKext)) {
        return false;
    }

   /* Refuse to add a kext whose version is later than aKext's, and
    * add as a duplicate one whose version is the same.
    */
    if (_KXKextGetVersion(priorKext) > _KXKextGetVersion(aKext)) {
        return false;
    } else if (_KXKextGetVersion(aKext) == _KXKextGetVersion(priorKext)) {
        return _KXKextAddDuplicateVersionKext(aKext, priorKext);
    }

   /* Find where to put the requested kext. If we find one in the linked
    * list with the same version, tack it on the **END** of the found kext's
    * duplicate version list. If we find one whose version is earlier,
    * insert it before the found kext. Otherwise we'll end up tacking it
    * onto the end of the list.
    *
    * Duplicate kexts are treated on a first-come, first-eligible basis.
    * This means that kexts from repositories added earliest to the manager
    * have priority, and within a single repository the ordering of
    * duplicates is explicitly undefined and may change from invocation to
    * invocation.
    */
    peekingKext = insertionKext = aKext;
    while (peekingKext) {
        peekingKext = KXKextGetPriorVersionKext(insertionKext);
        if (peekingKext) {
            if (_KXKextGetVersion(peekingKext) == _KXKextGetVersion(priorKext)) {
                return _KXKextAddDuplicateVersionKext(peekingKext, priorKext);
            } else if (_KXKextGetVersion(peekingKext) <
                       _KXKextGetVersion(priorKext)) {

                break;
            }
            insertionKext = peekingKext;
        }
    }

    _KXKextSetPriorVersionKext(priorKext, peekingKext);
    _KXKextSetPriorVersionKext(insertionKext, priorKext);

    return true;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextSetDuplicateVersionKext(KXKextRef aKext, KXKextRef thatKext)
{
    KXKextRef oldNextDuplicate = NULL;

    oldNextDuplicate = aKext->nextDuplicate;
    aKext->nextDuplicate = thatKext;
    if (aKext->nextDuplicate) CFRetain(aKext->nextDuplicate);
    if (oldNextDuplicate) CFRelease(oldNextDuplicate);

    return;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean _KXKextAddDuplicateVersionKext(KXKextRef aKext, KXKextRef dupVersKext)
{
    KXKextRef peekingKext = NULL;
    KXKextRef insertionKext = NULL;

// FIXME: What to do if a dup has a different compatible version???

// FIXME: Will also need a way to note this problem in the kext being
// FIXME: ...disqualified. Consider a validation failure?

    if (_KXKextGetVersion(aKext) != _KXKextGetVersion(dupVersKext)) {
        return false;
    }

    if (KXKextGetDuplicateVersionKext(dupVersKext)) {
        return false;
    }

    peekingKext = insertionKext = aKext;

   /* Just find the end of the linked list.
    */
    while (peekingKext) {
        peekingKext = KXKextGetDuplicateVersionKext(insertionKext);
        if (peekingKext) insertionKext = peekingKext;
    }

    _KXKextSetDuplicateVersionKext(insertionKext, dupVersKext);

    return true;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError _KXKextScanPlugins(KXKextRef aKext,
    CFArrayRef * goodPlugins,
    CFArrayRef * badPlugins,
    CFArrayRef * removedPlugins)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    KXKextManagerError checkResult = kKXKextManagerErrorNone;

    CFStringRef bundlePath = NULL;    // must release
    CFURLRef pluginsRelURL = NULL;    // must release
    CFStringRef pluginsString = NULL; // must release
    CFURLRef pluginsAbsURL = NULL;    // must release
    CFStringRef pluginsAbsString = NULL; // must release
    CFArrayRef pluginArray = NULL;
    CFIndex count, i;
    KXKextRef thisPlugin = NULL;

    checkResult = __KXKextRealizeFromCache(aKext);
    if (checkResult != kKXKextManagerErrorNone) {
        result = checkResult;
        // there can be no further test at this point
        goto finish;
    }

    if (!aKext->infoDictionary) {
        result = kKXKextManagerErrorNotABundle;
        CFDictionarySetValue(__KXKextGetOrCreateValidationFailures(aKext),
            kKXKextErrorKeyNotABundle,
            kCFBooleanTrue);
        aKext->flags.canAuthenticate = 0;
        goto finish;
    }

    if (aKext->container) goto finish; // Plugins aren't allowed to have plugins!

    bundlePath = CFURLCopyFileSystemPath(aKext->bundleURL,
        kCFURLPOSIXPathStyle);
    if (!bundlePath) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    pluginsRelURL = CFBundleCopyBuiltInPlugInsURL(aKext->bundle);
    if (!pluginsRelURL) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    pluginsString = CFURLCopyFileSystemPath(pluginsRelURL,
        kCFURLPOSIXPathStyle);
    if (!pluginsString) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    pluginsAbsString = CFStringCreateWithFormat(kCFAllocatorDefault,
        NULL, CFSTR("%@/%@"), bundlePath, pluginsString);
    if (!pluginsAbsString) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    pluginsAbsURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
        pluginsAbsString, kCFURLPOSIXPathStyle, true);
    if (!pluginsAbsURL) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

#if 0
    pluginsAbsURL = CFURLCreateCopyAppendingPathComponent(kCFAllocatorDefault,
        aKext->bundleURL, pluginsString, true);
    if (!pluginsAbsURL) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }
#endif 0
    result = _KXKextRepositoryScanDirectoryForKexts(
        aKext->repository,
        pluginsAbsURL,
        aKext->plugins,
        goodPlugins,
        badPlugins,
        removedPlugins);

   /* It's okay for there to be no plugins directory.
    */
    if (result == kKXKextManagerErrorNotADirectory) {
        result = kKXKextManagerErrorNone;
    }

    if (result != kKXKextManagerErrorNone) {
        goto finish;
    }

    if (goodPlugins && *goodPlugins) {
        pluginArray = *goodPlugins;
        count = CFArrayGetCount(pluginArray);
        for (i = 0; i < count; i++) {
            thisPlugin = (KXKextRef)CFArrayGetValueAtIndex(pluginArray, i);
            _KXKextAddPlugin(aKext, thisPlugin);
        }
    }

    if (removedPlugins && *removedPlugins) {
        pluginArray = *removedPlugins;
        count = CFArrayGetCount(pluginArray);
        for (i = 0; i < count; i++) {
            thisPlugin = (KXKextRef)CFArrayGetValueAtIndex(pluginArray, i);
            _KXKextAddPlugin(aKext, thisPlugin);
        }
    }

finish:

    if (bundlePath) CFRelease(bundlePath);
    if (pluginsRelURL) CFRelease(pluginsRelURL);
    if (pluginsString) CFRelease(pluginsString);
    if (pluginsAbsString) CFRelease(pluginsAbsString);
    if (pluginsAbsURL) CFRelease(pluginsAbsURL);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextSetContainerForPluginKext(
    KXKextRef aKext,
    KXKextRef containerKext)
{
    aKext->container = containerKext;
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextAddPlugin(KXKextRef aKext, KXKextRef pluginKext)
{
    CFIndex i;

    if (!aKext->plugins) {
        aKext->plugins = CFArrayCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeArrayCallBacks);
        if (!aKext->plugins) return;
    }

    i = CFArrayGetFirstIndexOfValue(aKext->plugins,
        CFRangeMake(0, CFArrayGetCount(aKext->plugins)), pluginKext);

    if (i == kCFNotFound) {
        CFArrayAppendValue(aKext->plugins, pluginKext);
        _KXKextSetContainerForPluginKext(pluginKext, aKext);
    }


    return;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextRemovePlugin(KXKextRef aKext, KXKextRef pluginKext)
{
    CFIndex i;

    if (!aKext->plugins) {
        goto finish;
    }

    _KXKextSetContainerForPluginKext(pluginKext, NULL);

    i = CFArrayGetFirstIndexOfValue(aKext->plugins,
        CFRangeMake(0, CFArrayGetCount(aKext->plugins)), pluginKext);

    if (i != kCFNotFound) {
        CFArrayRemoveValueAtIndex(aKext->plugins, i);
    }

finish:
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextRemoveAllPlugins(KXKextRef aKext)
{
    CFIndex count, i;

    if (!aKext->plugins) {
        goto finish;
    }

    count = CFArrayGetCount(aKext->plugins);
    for (i = 0; i < count; i++) {
        KXKextRef thisKext = (KXKextRef)CFArrayGetValueAtIndex(aKext->plugins, i);
        _KXKextSetContainerForPluginKext(thisKext, NULL);
    }

    CFArrayRemoveAllValues(aKext->plugins);

finish:
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextSetIsLoaded(KXKextRef aKext, Boolean flag)
{
    aKext->flags.isLoaded = (flag ? 1 : 0);
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextSetHasBeenAuthenticated(KXKextRef aKext, Boolean flag)
{
    aKext->flags.hasBeenAuthenticated = (flag ? 1 : 0);
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextSetOtherVersionIsLoaded(KXKextRef aKext, Boolean flag)
{
    aKext->flags.otherVersionIsLoaded = (flag ? 1 : 0);
    return;
}

/*******************************************************************************
*
*******************************************************************************/
dgraph_t * _KXKextCreateDgraph(KXKextRef aKext)
{
    int error = 0;
    dgraph_t * dgraph = NULL;  // don't free
    dgraph_error_t result = dgraph_valid;

    if (!KXKextGetHasAllDependencies(aKext)) {
        _KXKextManagerLogError(KXKextGetManager(aKext),
            "kext doesn't have all dependencies");
        goto finish;
    }

    dgraph = (dgraph_t *)malloc(sizeof(dgraph_t));
    if (!dgraph) {
        goto finish;
    }

    result = dgraph_init(dgraph);
    if (result != dgraph_valid) {
        _KXKextManagerLogError(KXKextGetManager(aKext),
            "new dgraph is invalid");
        error = 1;
        goto finish;
    }

    if (!__KXKextAddDependenciesToDgraph(aKext,
        KXKextGetDirectDependencies(aKext), dgraph, false)) {

        _KXKextManagerLogError(KXKextGetManager(aKext),
            "can't add dependencies to dgraph");
        error = 1;
        goto finish;
    }

    dgraph->root = dgraph_find_root(dgraph);
    dgraph_establish_load_order(dgraph);

    if (!dgraph->root) {
        _KXKextManagerLogError(KXKextGetManager(aKext),
            "dependency graph has no root");
        error = 1;
        goto finish;
    }

finish:
    if (error) {
        dgraph_free(dgraph, 1);
        dgraph = NULL;
    }
    return dgraph;
}

/*******************************************************************************
*
*******************************************************************************/
CFDictionaryRef _KXKextCopyCacheDictionary(KXKextRef aKext)
{
    Boolean error = false;
    CFMutableDictionaryRef theDictionary = NULL; // returned
    CFDictionaryRef cacheInfoDictionary = NULL;  // must release
    CFMutableArrayRef plugins = NULL;            // must release

    theDictionary = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
    if (!theDictionary) {
        error = true;
        goto finish;
    }

    CFDictionarySetValue(theDictionary, CFSTR("bundlePathInRepository"),
        aKext->bundlePathInRepository);

   /* We have to make sure the cache's info dictionary doesn't contain
    * any non-xml-able CF types.
    */
    cacheInfoDictionary = __KXKextCopyInfoDictionaryForCache(
        aKext, aKext->infoDictionary, false /* make key subs */);
    if (!cacheInfoDictionary) {
        goto finish;
    }
    CFDictionarySetValue(theDictionary, CFSTR("infoDictionary"),
        cacheInfoDictionary);

    if (aKext->plugins) {
        CFIndex count, i;

        plugins = CFArrayCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeArrayCallBacks);
        if (!plugins) {
            error = true;
            goto finish;
        }
        CFDictionarySetValue(theDictionary, CFSTR("plugins"),
            plugins);
        // do not release plugins here

        count = CFArrayGetCount(aKext->plugins);
        for (i = 0; i < count; i++) {
            KXKextRef plugin = (KXKextRef)CFArrayGetValueAtIndex(
                aKext->plugins, i);
            CFDictionaryRef pDict = _KXKextCopyCacheDictionary(plugin);
            if (!pDict) {
                error = true;
                goto finish;
            }
            CFArrayAppendValue(plugins, pDict);
            CFRelease(pDict); // clean up within loop
            pDict = NULL;
        }
    }

finish:
    if (cacheInfoDictionary) CFRelease(cacheInfoDictionary);
    if (plugins)             CFRelease(plugins);

    if (error) {
        if (theDictionary) CFRelease(theDictionary);
        theDictionary = NULL;
    }

    return theDictionary;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError _KXKextMakeSecure(KXKextRef aKext)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    KXKextManagerError checkResult = kKXKextManagerErrorNone;
    CFURLRef absURL = NULL;       // must release
    CFStringRef urlPath = NULL;  // must release
    char path[MAXPATHLEN] = "";
    char * const paths[2] = { path, NULL };
    FTS * ftscursor = NULL;  // must fts_close()
    FTSENT * ftsentry = NULL;

    checkResult = __KXKextRealizeFromCache(aKext);
    if (checkResult != kKXKextManagerErrorNone) {
        result = checkResult;
        // there can be no further test at this point
        goto finish;
    }

    absURL = PATH_CopyCanonicalizedURL(aKext->bundleURL);
    if (!absURL) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    urlPath = CFURLCopyFileSystemPath(absURL, kCFURLPOSIXPathStyle);
    if (!urlPath) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    if (!CFStringGetFileSystemRepresentation(urlPath, path,
        CFStringGetMaximumSizeOfFileSystemRepresentation(urlPath))) {

        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

   /* Touch the kext's enclosing folder to make sure the cache gets rebuilt.
    */
    if (!_KXKextRepositoryInvalidateCaches(aKext->repository)) {
        _KXKextManagerLogError(KXKextGetManager(aKext),
            "can't invalidate caches for %s",
            path);
        // don't bail on this; keep working on the kext
    }

    // drill down into kext and make each URL secure
    ftscursor = fts_open(paths, (FTS_NOCHDIR | FTS_PHYSICAL),
        NULL);
    if (!ftscursor) {
        _KXKextManagerLogError(KXKextGetManager(aKext),
            "can't scan contents of %s",
            path);
        result = kKXKextManagerErrorFileAccess;
        goto finish;
    }

    while ( (ftsentry = fts_read(ftscursor))) {
        if (! (ftsentry->fts_info & FTS_DP)) {
            result = __KXKextMakeFTSEntrySecure(aKext, ftsentry);
            if (result != kKXKextManagerErrorNone) {
                goto finish;
            }
        }
    }

    aKext->flags.hasBeenAuthenticated = 0;

    result = KXKextAuthenticate(aKext);

finish:
    if (ftscursor) fts_close(ftscursor);
    if (absURL) CFRelease(absURL);
    if (urlPath) CFRelease(urlPath);
    return result;
}

/*******************************************************************************
*
*******************************************************************************/

KXKextManagerError _KXKextCheckIntegrity(KXKextRef aKext, CFMutableArrayRef bomArray) {
    KXKextManagerError result = kKXKextManagerErrorNone;
    CFStringRef path = KXKextCopyAbsolutePath(aKext);
    CFDictionaryRef infoDict = KXKextGetInfoDictionary(aKext);    
    CFStringRef bundleID;

    aKext->integrityState = kKXKextIntegrityUnknown;

    if (!infoDict) {
        result = kKXKextManagerErrorValidation;
        goto finish;
    } else {
        bundleID = (CFStringRef)CFDictionaryGetValue(infoDict, CFSTR("CFBundleIdentifier"));
        if (bundleID) {
            if (CFStringHasPrefix(bundleID, CFSTR("com.apple"))) {
                aKext->integrityState = kKXKextIntegrityUnknown;
            } else { 
                aKext->integrityState = kKXKextIntegrityNotApple;
            }
        }
    }
    
finish:
    if (path) CFRelease(path);
    return result;
}


void _KXKextSetStartAddress(KXKextRef aKext, vm_address_t newAddr) {
    aKext->startAddress = newAddr;
};


/*******************************************************************************
********************************************************************************
* MODULE-PRIVATE API BELOW HERE
********************************************************************************
*******************************************************************************/

/*******************************************************************************
*
*******************************************************************************/
static const CFRuntimeClass __KXKextClass = {
    0,                       // version
    "KXKext",                // className
    NULL,                    // init
    NULL,                    // copy
    __KXKextReleaseContents, // finalize
    NULL,                    // equal
    NULL,                    // hash
    NULL,                    // copyFormattingDesc
    __KXKextCopyDebugDescription  // copyDebugDesc
};

static void __KXKextInitialize(void)
{
    __kKXKextTypeID = _CFRuntimeRegisterClass(&__KXKextClass);

    // FIXME: Need a way to automate setup of these.
    // FIXME: These should be localizable!
    kKXKextErrorKeyFileAccess =
        CFSTR("File access failure; no permission?");
     kKXKextErrorKeyBundleNotInRepository =
        CFSTR("Bundle was created with an URL not in the requested repository");
    kKXKextErrorKeyNotABundle =
        CFSTR("Not a bundle; file structure or property list error?");
    kKXKextErrorKeyNotAKextBundle =
        CFSTR("Bundle is not a kext bundle");
    kKXKextErrorKeyBadPropertyList =
        CFSTR("Info dictionary not valid for kext");
    kKXKextErrorKeyMissingProperty =
        CFSTR("Info dictionary missing required property/value");
    kKXKextErrorKeyPropertyIsIllegalType =
        CFSTR("Info dictionary property value is of illegal type");
    kKXKextErrorKeyPropertyIsIllegalValue =
        CFSTR("Info dictionary property value is illegal");
    kKXKextErrorKeyIdentifierOrVersionTooLong =
        CFSTR("CFBundleIdentifier or CFBundleVersion string is too long");
    kKXKextErrorKeyPersonalitiesNotNested =
        CFSTR("Structure of IOKitPersonalities is incorrect");
    kKXKextErrorKeyMissingExecutable =
        CFSTR("Kext claims an executable file but it doesn't exist");
    kKXKextErrorKeyCompatibleVersionLaterThanVersion =
        CFSTR("Compatible version is later than current version");
    kKXKextErrorKeyExecutableBad =
        CFSTR("Executable file doesn't contain kernel extension code");
    kKXKextErrorKeyExecutableBadArch =
        CFSTR("Executable does not contain code for this architecture");
    kKXKextErrorKeyStatFailure =
        CFSTR("Failed to get authentication info (stat failed)");
    kKXKextErrorKeyFileNotFound =
        CFSTR("File not found");
    kKXKextErrorKeyOwnerPermission =
        CFSTR("File owner/permissions are incorrect "
			"(must be root:wheel, nonwritable by group/other)");
    kKXKextErrorKeyChecksum =
        CFSTR("Checksum does not match");
    kKXKextErrorKeySignature =
        CFSTR("Signature cannot be authenticated");

    kKXKextErrorKeyDependenciesUnresolvable =
        CFSTR("Cannot resolve all dependencies");
    kKXKextErrorKeyPossibleDependencyLoop =
        CFSTR("Too many dependencies; possible loop?");
    kKXKextDependencyUnavailable =
        CFSTR("No valid version of this dependency can be found");
    kKXKextDependencyNoCompatibleVersion =
        CFSTR("A valid compatible version of this dependency cannot be found");
    kKXKextDependencyCompatibleVersionUndeclared =
        CFSTR("No dependency could be found that declares a compatible version");
   kKXKextIndirectDependencyUnresolvable =
        CFSTR("This dependency has dependencies that cannot be resolved");
    kKXKextDependencyCircularReference =
        CFSTR("This dependency may be causing a circular reference");

    kKXKextErrorKeyNonuniqueIOResourcesMatch =
        CFSTR("A personality matches on IOResources but IOMatchCategory "
            "is missing or not equal to its IOClass "
            "(driver may fail to win matching)");
    kKXKextErrorKeyNoExplicitKernelDependency =
        CFSTR("Kext has no explicit kernel dependency");
    kKXKextErrorKeyBundleIdentifierMismatch =
        CFSTR("Kext has a kernel dependency prior to version 6.0 and "
        "CFBundleIdentifier does not match executable's MODULE_NAME");
    kKXKextErrorKeyBundleVersionMismatch =
        CFSTR("Kext has a kernel dependency prior to version 6.0 and "
        "CFBundleVersion does not match executable's MODULE_VERSION");
    kKXKextErrorKeyDeclaresBothKernelAndKPIDependencies =
        CFSTR("Kext has immediate dependencies on both "
        "com.apple.kernel and com.apple.kpi components; use only one style");

    return;
}

/*******************************************************************************
*
*******************************************************************************/
static pthread_once_t initialized = PTHREAD_ONCE_INIT;

static KXKextRef __KXKextCreatePrivate(
    CFAllocatorRef       allocator,
    CFAllocatorContext * context __unused)
{
    KXKextRef newKext = NULL;
    void * offset = NULL;
    UInt32           size;

    /* initialize runtime */
    pthread_once(&initialized, __KXKextInitialize);

    /* allocate session */
    size  = sizeof(__KXKext) - sizeof(CFRuntimeBase);
    newKext = (KXKextRef)_CFRuntimeCreateInstance(allocator,
        __kKXKextTypeID, size, NULL);
    if (!newKext) {
        return NULL;
    }
    offset = newKext;
    bzero(offset + sizeof(CFRuntimeBase), size);
    
    return (KXKextRef)newKext;
}

/*******************************************************************************
*
*******************************************************************************/
static CFStringRef __KXKextCopyDebugDescription(CFTypeRef cf)
{
    CFAllocatorRef     allocator = CFGetAllocator(cf);
    CFMutableStringRef result;

    result = CFStringCreateMutable(allocator, 0);
    CFStringAppendFormat(result, NULL, CFSTR("<KXKext %p [%p]> {\n"), cf, allocator);
    // add useful stuff here
    CFStringAppendFormat(result, NULL, CFSTR("}"));

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
static void __KXKextResetTestFlags(KXKextRef aKext)
{
    aKext->logLevel = kKXKextManagerLogLevelDefault;

    aKext->flags.declaresExecutable = 0;
    aKext->flags.isKernelResource = 0;
    aKext->flags.isValid = 0;
    aKext->flags.isEligibleDuringSafeBoot = 0;
    aKext->flags.isEnabled = 1;  // note this is true by default
    aKext->flags.canAuthenticate = 1;  // assume yes unless validation says no
    aKext->flags.hasBeenAuthenticated = 0;
    aKext->flags.isAuthentic = 0;
    aKext->flags.canResolveDependencies = 1;  // assume yes unless validation says no
    aKext->flags.hasAllDependencies = 0;

    aKext->flags.isLoaded = 0;
    aKext->flags.otherVersionIsLoaded = 0;
    aKext->flags.loadFailed = 0;
    aKext->flags.hasIOKitDebugProperty = 0;

    return;
}

/*******************************************************************************
*
*******************************************************************************/
static void __KXKextInitBlank(KXKextRef aKext)
{
    aKext->manager = NULL;
    aKext->repository = NULL;
    aKext->bundle = NULL;
    aKext->bundleURL = NULL;
    aKext->infoDictionary = NULL;
    aKext->bundleDirectoryName = NULL;
    aKext->bundlePathInRepository = NULL;

    aKext->plugins = NULL;
    aKext->container = NULL;

    __KXKextResetTestFlags(aKext);

    aKext->integrityState = kKXKextIntegrityUnknown;
    aKext->startAddress = 0;
    aKext->versionString = NULL;
    aKext->version = 0;
    aKext->compatibleVersion = 0;
    aKext->priorVersion = NULL;
    aKext->nextDuplicate = NULL;
    aKext->directDependencies = NULL;
    aKext->validationFailures = NULL;
    aKext->authenticationFailures = NULL;
    aKext->missingDependencies = NULL;

    return;
}

/*******************************************************************************
*
*******************************************************************************/
static void __KXKextReleaseContents(CFTypeRef cf)
{
    KXKextRef aKext = (KXKextRef)cf;

    if (aKext->manager) {
        // manager is not retained
        aKext->manager = NULL;
    }

    if (aKext->repository) {
        // repository is not retained
        aKext->repository = NULL;
    }

    if (aKext->bundlePathInRepository) {
        CFRelease(aKext->bundlePathInRepository);
        aKext->bundlePathInRepository = NULL;
    }

    if (aKext->bundleDirectoryName) {
        CFRelease(aKext->bundleDirectoryName);
        aKext->bundleDirectoryName = NULL;
    }

    if (aKext->infoDictionary) {
        CFRelease(aKext->infoDictionary);
        aKext->infoDictionary = NULL;
    }

    if (aKext->container) {
        CFRelease(aKext->container);
        aKext->container = NULL;
    }

    _KXKextRemoveAllPlugins((KXKextRef)aKext);
    if (aKext->plugins) {
        CFRelease(aKext->plugins);
        aKext->plugins = NULL;
    }

    if (aKext->bundleURL) {
        CFRelease(aKext->bundleURL);
        aKext->bundleURL = NULL;
    }

    if (aKext->bundle) {
        CFRelease(aKext->bundle);
        aKext->bundle = NULL;
    }

    if (aKext->versionString) {
        free(aKext->versionString);
        aKext->versionString = NULL;
    }

    if (aKext->priorVersion) {
        CFRelease(aKext->priorVersion);
        aKext->priorVersion = NULL;
    }

    if (aKext->nextDuplicate) {
        CFRelease(aKext->nextDuplicate);
        aKext->nextDuplicate = NULL;
    }

    if (aKext->directDependencies) {
        CFRelease(aKext->directDependencies);
        aKext->directDependencies = NULL;
    }

    if (aKext->validationFailures) {
        CFRelease(aKext->validationFailures);
        aKext->validationFailures = NULL;
    }

    if (aKext->authenticationFailures) {
        CFRelease(aKext->authenticationFailures);
        aKext->authenticationFailures = NULL;
    }

    if (aKext->missingDependencies) {
        CFRelease(aKext->missingDependencies);
        aKext->missingDependencies = NULL;
    }

    if (aKext->warnings) {
        CFRelease(aKext->warnings);
        aKext->warnings = NULL;
    }

    return;
}

/*******************************************************************************
*
*******************************************************************************/
static Boolean __KXKextCheckLogLevel(
    KXKextRef aKext,
    SInt32 managerLogLevel,
    SInt32 kextLogLevel,
    Boolean exact)
{
    int kmLogLevel = KXKextManagerGetLogLevel(aKext->manager);
    if (exact) {
        if (kmLogLevel == managerLogLevel || aKext->logLevel == kextLogLevel) {
            return true;
        }
    } else if (kmLogLevel >= managerLogLevel || aKext->logLevel >= kextLogLevel) {
        return true;
    }
    return false;
}

/*******************************************************************************
*
*******************************************************************************/
static KXKextManagerError __KXKextValidate(KXKextRef aKext)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    CFStringRef propKey;     // for HIGHLY LOCAL reuse (not more than 10 lines)
    CFStringRef stringValue; // do not release
    CFTypeRef   rawValue;    // do not release
    CFNumberRef numValue;    // do not release

    CFMutableArrayRef propPathArray = NULL;      // must release
    CFStringRef       propPathString = NULL;     // must release

    CFIndex numPersonalities = 0;
    CFIndex numPersonalitiesWithIOKitDebug = 0;

    CFTypeRef * personalityKeys = NULL;    // must free
    CFTypeRef * personalityValues = NULL;  // must free

    CFStringRef * keys = NULL;             // must free

    CFMutableArrayRef illegalValueProperties = NULL;
    CFMutableArrayRef missingValueProperties = NULL;

    KXKextManagerError checkResult = kKXKextManagerErrorNone;
    volatile Boolean foundErrors = false;  // only set to true below here

    Boolean checkIOMatchCategory = false;
    CFStringRef classProp = NULL;    // do not release
    VERS_version kernelLibVersion = 0;
    Boolean requiresNewKextManager = false;
    Boolean hasKernelStyleDependency = false;
    Boolean hasKPIStyleDependency = false;

// FIXME: Error to have personality with bundle's CFBundleIdentifier
// FIXME: ...but no executable in bundle.

    // clear these flags just in case
    __KXKextResetTestFlags(aKext);

    if (!__KXKextGetOrCreateValidationFailures(aKext)) {
        if (!aKext->validationFailures) {
            result = kKXKextManagerErrorNoMemory;
            goto finish;
        }
    }
    CFDictionaryRemoveAllValues(aKext->validationFailures);

   /*****
    * This is used to add nested property keys to the error dictionaries.
    */
    propPathArray = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!propPathArray) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;  // this is a fatal error
    }

   /*****
    * Validation of the info dictionary. No filesystem access!
    *****/

   /*****
    * Make sure there's an info dictionary and that it is a dictionary.
    */
    if (!aKext->infoDictionary ||
        CFGetTypeID(aKext->infoDictionary) != CFDictionaryGetTypeID()) {

        CFDictionarySetValue(__KXKextGetOrCreateValidationFailures(aKext),
            kKXKextErrorKeyNotABundle,
            kCFBooleanTrue);
        result = kKXKextManagerErrorNotABundle;
        aKext->flags.canAuthenticate = 0;
        foundErrors = true;
        // This is an immediate stop, so ignore
        // KXKextManagerPerformsFullTests().
        goto finish;
    }


   /*****
    * Check that property CFBundleIdentifier is present and is a string.
    * It must also not be too long.
    */
    if (!__KXKextCheckStringProperty(aKext, aKext->infoDictionary,
            CFSTR("CFBundleIdentifier"), NULL, true, NULL, &stringValue)) {

        result = kKXKextManagerErrorValidation;
        foundErrors = true;
        if (!KXKextManagerPerformsFullTests(aKext->manager)) goto finish;
    }

    if (stringValue && CFStringGetLength(stringValue) > KMOD_MAX_NAME - 1) {
        CFDictionarySetValue(__KXKextGetOrCreateValidationFailures(aKext),
            kKXKextErrorKeyIdentifierOrVersionTooLong,
            kCFBooleanTrue);
        result = kKXKextManagerErrorValidation;
        foundErrors = true;
        if (!KXKextManagerPerformsFullTests(aKext->manager)) goto finish;
    }



   /*****
    * Check that property CFBundlePackageType is "KEXT", but only if we're
    * from a bundle. A kext from a cache omits this property.
    */
    if (!KXKextIsFromCache(aKext) &&
        !__KXKextCheckStringProperty(aKext, aKext->infoDictionary,
            CFSTR("CFBundlePackageType"), NULL, true, CFSTR("KEXT"), NULL)) {

        result = kKXKextManagerErrorNotAKext;
        foundErrors = true;
        if (!KXKextManagerPerformsFullTests(aKext->manager)) goto finish;
    }

   /*****
    * Look for the OSBundleDebugLevel flag and read it.
    */
    rawValue = CFDictionaryGetValue(aKext->infoDictionary,
        CFSTR("OSBundleDebugLevel"));
    if (rawValue) {
        Boolean numError = false;
        if (CFGetTypeID(rawValue) == CFNumberGetTypeID()) {
            numValue = (CFNumberRef)rawValue;
            if (!CFNumberGetValue(numValue, kCFNumberSInt32Type, &aKext->logLevel)) {
                numError = true;
            }
        } else {
            numError = true;
        }
        if (numError) {
            CFMutableArrayRef illegalTypeProperties =
                __KXKextGetIllegalTypeProperties(aKext);
            if (!illegalTypeProperties) {
                result = kKXKextManagerErrorNoMemory;
                goto finish;
            }
            CFArrayAppendValue(illegalTypeProperties, CFSTR("OSBundleDebugLevel"));
            result = kKXKextManagerErrorValidation;
            foundErrors = true;
            if (!KXKextManagerPerformsFullTests(aKext->manager)) goto finish;
        }
    }

#if 0
// This property is omitted from caches and cannot be validated.
// CFBundle should be doing this anyhow, and it doesn't impact
// any functionality of a kext.

   /*****
    * Check that property CFBundleSignature is a string.
    */
    if (!__KXKextCheckStringProperty(aKext, aKext->infoDictionary,
        CFSTR("CFBundleSignature"), NULL, true, NULL, NULL)) {

        result = kKXKextManagerErrorValidation;
        foundErrors = true;
        if (!KXKextManagerPerformsFullTests(aKext->manager)) goto finish;
    }
#endif 0

   /*****
    * Check validity of property CFBundleVersion.
    */
    propKey = CFSTR("CFBundleVersion");
    if (!__KXKextCheckVersionProperty(aKext, aKext->infoDictionary,
        propKey, NULL, true, &aKext->versionString, &aKext->version)) {

        result = kKXKextManagerErrorValidation;
        foundErrors = true;
        if (!KXKextManagerPerformsFullTests(aKext->manager)) goto finish;
    }

   /*****
    * Check property OSBundleCompatibleVersion.
    * This is not required, but if it is present, its value must be legal.
    */
    propKey = CFSTR("OSBundleCompatibleVersion");
    if (!__KXKextCheckVersionProperty(aKext, aKext->infoDictionary,
        propKey, NULL, false, NULL, &aKext->compatibleVersion)) {

        result = kKXKextManagerErrorValidation;
        foundErrors = true;
        if (!KXKextManagerPerformsFullTests(aKext->manager)) goto finish;
    }

    if (aKext->compatibleVersion > aKext->version) {
        CFDictionarySetValue(__KXKextGetOrCreateValidationFailures(aKext),
            kKXKextErrorKeyCompatibleVersionLaterThanVersion,
            kCFBooleanTrue);
        result = kKXKextManagerErrorValidation;
        foundErrors = true;
        if (!KXKextManagerPerformsFullTests(aKext->manager)) goto finish;
    }


   /*****
    * Check that CFBundleExecutable is a string.
    */
    propKey = CFSTR("CFBundleExecutable");
    if (!__KXKextCheckStringProperty(aKext, aKext->infoDictionary,
        propKey, NULL, false, NULL, &stringValue)) {

        result = kKXKextManagerErrorValidation;
        foundErrors = true;
        if (!KXKextManagerPerformsFullTests(aKext->manager)) goto finish;
    }

    if (!stringValue) {
        aKext->flags.declaresExecutable = 0;
    } else {
        aKext->flags.declaresExecutable = 1;
    }


   /*****
    * Check for a kernel pseudo-kext. These don't have dependencies.
    */
    propKey = CFSTR("OSKernelResource");
    if (!__KXKextCheckPropertyType(aKext, aKext->infoDictionary, propKey,
        NULL, false, CFBooleanGetTypeID(), &rawValue)) {

        result = kKXKextManagerErrorValidation;
        foundErrors = true;
        if (!KXKextManagerPerformsFullTests(aKext->manager)) goto finish;
    }
    if (rawValue) {
        aKext->flags.isKernelResource =
            CFBooleanGetValue((CFBooleanRef)rawValue) ? 1 : 0;
        aKext->flags.hasAllDependencies = aKext->flags.isKernelResource;
    }

    propKey = CFSTR("IOKitPersonalities");

    if (KXKextIsFromCache(aKext)) {
        if (kCFBooleanFalse == CFDictionaryGetValue(aKext->infoDictionary, propKey)) {
            // cached invalid
            result = kKXKextManagerErrorValidation;
            foundErrors = true;
            if (!KXKextManagerPerformsFullTests(aKext->manager)) goto finish;
        }
    } else {
       /*****
        * Check property IOKitPersonalities. This is required for proper I/O
        * kit driver matching, but not for the kext itself to be valid. A
        * diagnostic tool will just have to check explicitly for the presence
        * of personalities and show a caution/warning if there are none.
        */
    
       /* Push the personalities key onto the end of the path.
        */
        CFArrayAppendValue(propPathArray, propKey);
    
        if (!__KXKextCheckPropertyType(aKext, aKext->infoDictionary, propKey,
            NULL, false, CFDictionaryGetTypeID(), &rawValue)) {
    
            result = kKXKextManagerErrorValidation;
            foundErrors = true;
            if (!KXKextManagerPerformsFullTests(aKext->manager)) goto finish;
    
        } else if (rawValue) {
    
        /*****
            * Check that all values in IOKitPersonalities are Dicts.
            */
            CFDictionaryRef personalitiesDict = (CFDictionaryRef)rawValue;
            CFIndex i;
    
            numPersonalities = CFDictionaryGetCount(personalitiesDict);
            personalityKeys =
                (CFTypeRef *)malloc(numPersonalities * sizeof(CFTypeRef));
            if (! personalityKeys) {
                result = kKXKextManagerErrorNoMemory;
                goto finish;  // this is a fatal error
            }
    
            personalityValues =
                (CFTypeRef *)malloc(numPersonalities * sizeof(CFTypeRef));
            if (!personalityValues) {
                result = kKXKextManagerErrorNoMemory;
                goto finish;  // this is a fatal error
            }
    
            CFDictionaryGetKeysAndValues(personalitiesDict,
                (const void **)personalityKeys,
                (const void **)personalityValues);
    
            for (i = 0; i < numPersonalities; i++) {
                CFStringRef thisPersonalityName = personalityKeys[i];
                CFTypeRef thisPersonality = personalityValues[i];
                CFDictionaryRef thisPersonalityDict;
    
                if (CFGetTypeID(thisPersonality) != CFDictionaryGetTypeID()) {
                    CFDictionarySetValue(__KXKextGetOrCreateValidationFailures(aKext),
                        kKXKextErrorKeyPersonalitiesNotNested,
                        kCFBooleanTrue);
                    result = kKXKextManagerErrorValidation;
                    foundErrors = true;
                    if (!KXKextManagerPerformsFullTests(aKext->manager)) {
                        goto finish;
                    }
                } else {
    
                   /* Push the personality name onto the end of the path.
                    */
                    CFArrayAppendValue(propPathArray, thisPersonalityName);
    
                    thisPersonalityDict = (CFDictionaryRef)thisPersonality;
    
                   /*****
                    * Make sure the personality has an IOClass
                    * string property.
                    */
                    propKey = CFSTR("IOClass");
                    CFArrayAppendValue(propPathArray, propKey);
                    if (!__KXKextCheckStringProperty(aKext, thisPersonality,
                        propKey, propPathArray, true, NULL, &stringValue)) {
    
                        result = kKXKextManagerErrorValidation;
                        foundErrors = true;
                        if (!KXKextManagerPerformsFullTests(aKext->manager)) {
                            goto finish;
                        }
                    } else if (stringValue) {
                        classProp = stringValue;
                    }

                    CFArrayRemoveValueAtIndex(propPathArray,
                        CFArrayGetCount(propPathArray) - 1);
    
    
                   /*****
                    * Make sure the personality has an IOProviderClass
                    * string property.
                    */
                    propKey = CFSTR("IOProviderClass");
                    CFArrayAppendValue(propPathArray, propKey);
                    if (!__KXKextCheckStringProperty(aKext, thisPersonality,
                        propKey, propPathArray, true, NULL, &stringValue)) {
    
                        result = kKXKextManagerErrorValidation;
                        foundErrors = true;
                        if (!KXKextManagerPerformsFullTests(aKext->manager)) {
                            goto finish;
                        }
                    } else if (stringValue && CFEqual(stringValue,
                        CFSTR("IOResources"))) {
                        
                        checkIOMatchCategory = true;
                    }

                    CFArrayRemoveValueAtIndex(propPathArray,
                        CFArrayGetCount(propPathArray) - 1);

                   /*****
                    * Check on IOResources for missing/odd IOMatchCategory.
                    */
                    if (checkIOMatchCategory) {
                        propKey = CFSTR("IOMatchCategory");
                        CFArrayAppendValue(propPathArray, propKey);
                        if (!__KXKextCheckStringProperty(aKext, thisPersonality,
                            propKey, propPathArray, false, NULL,
                            &stringValue)) {
        
                            result = kKXKextManagerErrorValidation;
                            foundErrors = true;
                            if (!KXKextManagerPerformsFullTests(aKext->manager)) {
                                goto finish;
                            }
                        }
                        if (!stringValue || !CFEqual(stringValue, classProp)) {
                            CFDictionarySetValue(__KXKextGetOrCreateWarnings(aKext),
                                kKXKextErrorKeyNonuniqueIOResourcesMatch,
                                kCFBooleanTrue);
                        }
                        CFArrayRemoveValueAtIndex(propPathArray,
                            CFArrayGetCount(propPathArray) - 1);
                    }

                   /*****
                    * Make sure the personality has a CFBundleIdentifier
                    * string property.
                    */
                    // FIXME: Should just add bundle's own CFBundleIdentifier
                    // FIXME: ...to this personality if the prop isn't there.
                    // FIXME: ...See the fixme below.
                    propKey = CFSTR("CFBundleIdentifier");
                    CFArrayAppendValue(propPathArray, propKey);
                    if (!__KXKextCheckStringProperty(aKext, thisPersonality,
                        propKey, propPathArray, false, NULL, NULL)) {
    
                        result = kKXKextManagerErrorValidation;
                        foundErrors = true;
                        if (!KXKextManagerPerformsFullTests(aKext->manager)) {
                            goto finish;
                        }
                    }
                    CFArrayRemoveValueAtIndex(propPathArray,
                        CFArrayGetCount(propPathArray) - 1);
    
                   /*****
                    * Check that the IOKitDebug property, if present, is a number and
                    * is not a float type. Then, if its value is nonzero, increment
                    * the number of personalities known to have debug set so that we can
                    * check later whether the kext as a whole is eligible for safe boot.
                    */
                    propKey = CFSTR("IOKitDebug");
                    CFArrayAppendValue(propPathArray, propKey);
                    if (!__KXKextCheckPropertyType(aKext, thisPersonality,
                        propKey, propPathArray, false, CFNumberGetTypeID(), &rawValue)) {
    
                        result = kKXKextManagerErrorValidation;
                        foundErrors = true;
                        if (!KXKextManagerPerformsFullTests(aKext->manager)) goto finish;
    
                    } else if (rawValue) {
                        CFNumberRef number = (CFNumberRef)rawValue;
    
                    /* The fact that this is blatantly invalid is checked below
                        * by __KXKextCheckPersonalityTypes().
                        */
                        if (!CFNumberIsFloatType(rawValue)) {
                            long long int numValue = 0;
    
                            if (!CFNumberGetValue(number, kCFNumberLongLongType, &numValue)) {
                                _KXKextManagerLogError(KXKextGetManager(aKext),
                                    "error reading IOKitDebug property");
                                foundErrors = true;
                                if (!KXKextManagerPerformsFullTests(aKext->manager)) {
                                goto finish;
                                }
                            }
    
                            if (numValue != 0) {
                                numPersonalitiesWithIOKitDebug++;
                                aKext->flags.hasIOKitDebugProperty = 1;
                            }
                        }
                    }
    
                   /* Pop the personality name of the end of the path.
                    */
                    CFArrayRemoveValueAtIndex(propPathArray,
                        CFArrayGetCount(propPathArray) - 1);
    
                   /******
                    * Make sure that only kernel-supported plist types are present
                    * in the personality.
                    */
                    if (!__KXKextCheckPersonalityTypes(aKext, thisPersonality,
                        propPathArray)) {
    
                        result = kKXKextManagerErrorValidation;
                        foundErrors = true;
                        if (!KXKextManagerPerformsFullTests(aKext->manager)) {
                            goto finish;
                        }
                    }
    
                   /* Pop the personality name of the end of the path.
                    */
                    CFArrayRemoveValueAtIndex(propPathArray,
                        CFArrayGetCount(propPathArray) - 1);
                }
            }
        }
    }

   /* Empty the property path for subsequent uses.
    */
    CFArrayRemoveAllValues(propPathArray);

   /*****
    * Check property OSBundleLibraries and the version values of each of
    * its entries. Libraries must be declared for any non-kernel kext that
    * declares an executable.
    */
    propKey = CFSTR("OSBundleLibraries");

   /* Push the libarries key onto the end of the path.
    */
    CFArrayAppendValue(propPathArray, propKey);

    if (!__KXKextCheckPropertyType(aKext, aKext->infoDictionary,
         propKey, NULL,
         (!aKext->flags.isKernelResource && aKext->flags.declaresExecutable),
         CFDictionaryGetTypeID(), &rawValue)) {

        result = kKXKextManagerErrorValidation;
        foundErrors = true;
        aKext->flags.canResolveDependencies = 0;

        if (!KXKextManagerPerformsFullTests(aKext->manager)) goto finish;

    } else if (rawValue) {
        CFDictionaryRef bundleLibraries = (CFDictionaryRef)rawValue;
        CFIndex numLibraries, i;

        numLibraries = CFDictionaryGetCount(bundleLibraries);
        if (!numLibraries && !aKext->flags.isKernelResource &&
            aKext->flags.declaresExecutable) {

            result = kKXKextManagerErrorValidation;
            foundErrors = true;
            aKext->flags.canResolveDependencies = 0;
            missingValueProperties = __KXKextGetMissingProperties(aKext);
            if (!missingValueProperties) {
                result = kKXKextManagerErrorNoMemory;
                goto finish;
            }
            CFArrayAppendValue(missingValueProperties, propKey);
            if (!KXKextManagerPerformsFullTests(aKext->manager)) {
                goto finish;
            }
        }
        keys = (CFStringRef *)malloc(numLibraries * sizeof(CFStringRef));
        if (!keys) {
            result = kKXKextManagerErrorNoMemory;
            goto finish;
        }

        CFDictionaryGetKeysAndValues(bundleLibraries, (const void **)keys, NULL);

       /*****
        * Prep the value used to check whether this kext can be
        * used on versions of Mac OS X prior to Jaguar.
        * FIXME: Change that version reference.
        */
        kernelLibVersion = VERS_parse_string("6.0");
        if (kernelLibVersion < 0) {
            result = kKXKextManagerErrorUnspecified;
            goto finish;
        }

        for (i = 0; i < numLibraries; i++) {
            CFStringRef dependencyID = keys[i];
            VERS_version version;

            CFArrayAppendValue(propPathArray, dependencyID);
            if (!__KXKextCheckVersionProperty(aKext, bundleLibraries,
                dependencyID, propPathArray, true, NULL, &version)) {

                result = kKXKextManagerErrorValidation;
                foundErrors = true;
                aKext->flags.canResolveDependencies = 0;
                if (!KXKextManagerPerformsFullTests(aKext->manager)) {
                    goto finish;
                }
            } else if (CFStringHasPrefix(dependencyID,
                       CFSTR("com.apple.kernel"))) {
                hasKernelStyleDependency = true;
                if (version >= kernelLibVersion) {
                    requiresNewKextManager = true;
                }
            } else if (CFStringHasPrefix(dependencyID,
                       CFSTR("com.apple.kpi"))) {
                hasKPIStyleDependency = true;
                if (version >= kernelLibVersion) {
                    requiresNewKextManager = true;
                }
            } 

            CFArrayRemoveValueAtIndex(propPathArray,
                CFArrayGetCount(propPathArray) - 1);
        }

        if (hasKernelStyleDependency && hasKPIStyleDependency) {
            CFDictionarySetValue(__KXKextGetOrCreateWarnings(aKext),
                kKXKextErrorKeyDeclaresBothKernelAndKPIDependencies,
                kCFBooleanTrue);
        }
    }

    CFArrayRemoveAllValues(propPathArray);

    propKey = CFSTR("OSBundleRequired");
    if (!__KXKextCheckStringProperty(aKext, aKext->infoDictionary,
        propKey, NULL, false, NULL, &stringValue)) {

        result = kKXKextManagerErrorValidation;
        foundErrors = true;
        if (!KXKextManagerPerformsFullTests(aKext->manager)) goto finish;

    } else if (stringValue) {

        CFStringRef str = stringValue;

        if (kCFCompareEqualTo == CFStringCompare(str, CFSTR("Root"), 0) ||
          kCFCompareEqualTo == CFStringCompare(str, CFSTR("Local-Root"), 0) ||
          kCFCompareEqualTo == CFStringCompare(str, CFSTR("Network-Root"), 0) ||
          kCFCompareEqualTo == CFStringCompare(str, CFSTR("Console"), 0) ||
          kCFCompareEqualTo == CFStringCompare(str, CFSTR("Safe Boot"), 0) ) {

            aKext->flags.isEligibleDuringSafeBoot = 1;
        } else {
            illegalValueProperties = __KXKextGetIllegalValueProperties(aKext);
            if (!illegalValueProperties) {
                result = kKXKextManagerErrorNoMemory;
                goto finish;
            }
            CFArrayAppendValue(illegalValueProperties, propKey);

            result = kKXKextManagerErrorValidation;
            foundErrors = true;
            if (!KXKextManagerPerformsFullTests(aKext->manager)) goto finish;
        }
    }

   /*****
    * We just checked for OSBundleRequired, which can set the kext's
    * isEligibleDuringSafeBoot, however, if all personalities have the
    * IOKitDebug property set to nonzero, we turn it right back off.
    */
    if (numPersonalities > 0 &&
        numPersonalities == numPersonalitiesWithIOKitDebug) {

        aKext->flags.isEligibleDuringSafeBoot = 0;
    }

   /*****
    * Validation that may access the filesystem, *only* if doing
    * a full diagnostic.
    *****/

   /*****
    * If doing a full diagnostic check that the kext's executable exists
    * and has the right kmod name and version within.
    *
    * This new kext manager's loading code writes into the kmod's kmod_info
    * struct the CFBundleIdentifier and CFBundleVersion from the info
    * dictionary, so this test is only for sanity checking kexts that
    * will be used on versions of Mac OS X that have the old kext manager.
    */
    if (KXKextManagerPerformsFullTests(aKext->manager)) {
        checkResult = __KXKextRealizeFromCache(aKext);
        if (checkResult != kKXKextManagerErrorNone) {
            result = checkResult;
            foundErrors = true;
            goto finish;  // can't guarantee we can check executable so stop
        }
        if (aKext->flags.declaresExecutable && !aKext->flags.isKernelResource) {
            checkResult = __KXKextValidateExecutable(aKext,
                requiresNewKextManager);
            if (checkResult != kKXKextManagerErrorNone) {
                result = checkResult;
                foundErrors = true;
                if (!KXKextManagerPerformsFullTests(aKext->manager)) goto finish;
            }
        }
    }

   /*****
    * Final marking of validity.
    *****/

    if (!foundErrors) {
        aKext->flags.isValid = 1;
    }

finish:

    if (result != kKXKextManagerErrorNone) {
        if (__KXKextCheckLogLevel(aKext, kKXKextManagerLogLevelBasic,
              kKXKextLogLevelBasic, false)) {

            const char * kext_name =
                _KXKextCopyCanonicalPathnameAsCString(aKext);  // must free
            _KXKextManagerLogError(KXKextGetManager(aKext),
                "kext %s is not valid",
            kext_name ? kext_name : "(unknown)");
            if (kext_name) free((char *)kext_name);
        }
    }

    if (propPathArray)     CFRelease(propPathArray);
    if (propPathString)    CFRelease(propPathString);

    if (personalityKeys)   free(personalityKeys);
    if (personalityValues) free(personalityValues);

    if (keys)              free(keys);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
static KXKextManagerError __KXKextValidateExecutable(KXKextRef aKext,
    Boolean requiresNewKextManager)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    KXKextManagerError checkResult = kKXKextManagerErrorNone;
    CFURLRef    executableURL = NULL;      // must release

    checkResult = __KXKextRealizeFromCache(aKext);
    if (checkResult != kKXKextManagerErrorNone) {
        result = checkResult;
        // there can be no further test at this point
        goto finish;
    }

    executableURL = CFBundleCopyExecutableURL(aKext->bundle);
    if (!executableURL) {
        CFDictionarySetValue(__KXKextGetOrCreateValidationFailures(aKext),
            kKXKextErrorKeyMissingExecutable,
            kCFBooleanTrue);
        result = kKXKextManagerErrorValidation;
        if (!KXKextManagerPerformsFullTests(aKext->manager)) {
            goto finish;
        }
    } else {

        checkResult = __KXKextCheckKmod(aKext, requiresNewKextManager);
        if (checkResult != kKXKextManagerErrorNone) {
            result = checkResult;
            if (!KXKextManagerPerformsFullTests(aKext->manager)) {
                goto finish;
            }
        }
    }

finish:
    if (executableURL)     CFRelease(executableURL);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
static CFMutableDictionaryRef __KXKextGetOrCreateValidationFailures(KXKextRef aKext)
{
    if (!aKext->validationFailures) {
        aKext->validationFailures = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
    }
    return aKext->validationFailures;
}

/*******************************************************************************
*
*******************************************************************************/
static CFMutableDictionaryRef __KXKextGetOrCreateAuthenticationFailures(KXKextRef aKext)
{
    if (!aKext->authenticationFailures) {
        aKext->authenticationFailures = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
    }
    return aKext->authenticationFailures;
}

/*******************************************************************************
*
*******************************************************************************/
static CFMutableDictionaryRef __KXKextGetOrCreateWarnings(KXKextRef aKext)
{
    if (!aKext->warnings) {
        aKext->warnings = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
    }
    return aKext->warnings;
}

/*******************************************************************************
*
*******************************************************************************/
static CFMutableArrayRef __KXKextGetMissingProperties(KXKextRef aKext)
{
    CFMutableDictionaryRef failures = NULL;
    CFMutableArrayRef theMissingProperties = NULL;

    failures = __KXKextGetOrCreateValidationFailures(aKext);
    if (!failures) {
        return NULL;
    }

    theMissingProperties = (CFMutableArrayRef)
        CFDictionaryGetValue(failures, kKXKextErrorKeyMissingProperty);

    if (!theMissingProperties) {
        theMissingProperties = CFArrayCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeArrayCallBacks);
        if (!theMissingProperties) {
            return NULL;
        }
        CFDictionarySetValue(failures,
            kKXKextErrorKeyMissingProperty,
            theMissingProperties);
        CFRelease(theMissingProperties);
    }

    return theMissingProperties;
}

/*******************************************************************************
*
*******************************************************************************/
static CFMutableArrayRef __KXKextGetIllegalTypeProperties(KXKextRef aKext)
{
    CFMutableDictionaryRef failures = NULL;
    CFMutableArrayRef theIllegalTypeProperties = NULL;

    failures = __KXKextGetOrCreateValidationFailures(aKext);
    if (!failures) {
        return NULL;
    }

    theIllegalTypeProperties = (CFMutableArrayRef)
        CFDictionaryGetValue(failures, kKXKextErrorKeyPropertyIsIllegalType);

    if (!theIllegalTypeProperties) {
        theIllegalTypeProperties = CFArrayCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeArrayCallBacks);
        if (!theIllegalTypeProperties) {
            return NULL;
        }
        CFDictionarySetValue(failures,
            kKXKextErrorKeyPropertyIsIllegalType,
            theIllegalTypeProperties);
        CFRelease(theIllegalTypeProperties);
    }

    return theIllegalTypeProperties;
}

/*******************************************************************************
*
*******************************************************************************/
static CFMutableArrayRef __KXKextGetIllegalValueProperties(KXKextRef aKext)
{
    CFMutableDictionaryRef failures = NULL;
    CFMutableArrayRef theIllegalValueProperties = NULL;

    failures = __KXKextGetOrCreateValidationFailures(aKext);
    if (!failures) {
        return NULL;
    }

    theIllegalValueProperties = (CFMutableArrayRef)
        CFDictionaryGetValue(failures, kKXKextErrorKeyPropertyIsIllegalValue);

    if (!theIllegalValueProperties) {
        theIllegalValueProperties = CFArrayCreateMutable(kCFAllocatorDefault,
            0, &kCFTypeArrayCallBacks);
        if (!theIllegalValueProperties) {
            return NULL;
        }
        CFDictionarySetValue(failures,
            kKXKextErrorKeyPropertyIsIllegalValue,
            theIllegalValueProperties);
        CFRelease(theIllegalValueProperties);
    }

    return theIllegalValueProperties;
}

/*******************************************************************************
*
*******************************************************************************/
static CFMutableArrayRef __KXKextGetMissingFiles(KXKextRef aKext)
{
    CFMutableDictionaryRef failures = NULL;
    CFMutableArrayRef theArray = NULL;

    failures = __KXKextGetOrCreateAuthenticationFailures(aKext);
    if (!failures) {
        return NULL;
    }

    theArray = (CFMutableArrayRef)
        CFDictionaryGetValue(failures, kKXKextErrorKeyFileNotFound);

    if (!theArray) {
        theArray = CFArrayCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeArrayCallBacks);
        if (!theArray) {
            return NULL;
        }
        CFDictionarySetValue(failures,
            kKXKextErrorKeyFileNotFound,
            theArray);
        CFRelease(theArray);
    }

    return theArray;
}

/*******************************************************************************
*
*******************************************************************************/
static CFMutableArrayRef __KXKextGetBadOwnerPermsFiles(KXKextRef aKext)
{
    CFMutableDictionaryRef failures = NULL;
    CFMutableArrayRef theArray = NULL;

    failures = __KXKextGetOrCreateAuthenticationFailures(aKext);
    if (!failures) {
        return NULL;
    }

    theArray = (CFMutableArrayRef)
        CFDictionaryGetValue(failures, kKXKextErrorKeyOwnerPermission);

    if (!theArray) {
        theArray = CFArrayCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeArrayCallBacks);
        if (!theArray) {
            return NULL;
        }
        CFDictionarySetValue(failures,
            kKXKextErrorKeyOwnerPermission,
            theArray);
        CFRelease(theArray);
    }

    return theArray;
}

/*******************************************************************************
*
*******************************************************************************/
static Boolean __KXKextCheckPropertyType(
    KXKextRef aKext,
    CFDictionaryRef aDictionary,
    CFStringRef propKey,
    CFArrayRef propPathArray,
    Boolean  isRequired,
    CFTypeID expectedType,
    CFTypeRef * rawValueOut)
{
    CFTypeRef         rawValue;
    CFMutableArrayRef missingProperties;
    CFMutableArrayRef illegalTypeProperties;
    CFStringRef       propPathString = NULL;  // must release

    // FIXME: Can't bail caller on allocation failure! However, another
    // FIXME: ...alloc failure is sure to happen soon after so the caller
    // FIXME: ...will bail then.

    if (rawValueOut) * rawValueOut = NULL;

    rawValue = CFDictionaryGetValue(aDictionary,
        propKey);

    if (!rawValue) {
        if (!isRequired) {
            return true;
        } else {
            missingProperties = __KXKextGetMissingProperties(aKext);
            if (!missingProperties) {
                return false;
            }
            if (!propPathArray) {
                CFArrayAppendValue(missingProperties, propKey);
            } else {
                propPathString = CFStringCreateByCombiningStrings(
                    kCFAllocatorDefault, propPathArray, CFSTR(":"));
                if (!propPathString) return false;
                CFArrayAppendValue(missingProperties, propPathString);
                CFRelease(propPathString);
            }

            return false;
        }
    }

    if (rawValueOut) *rawValueOut = rawValue;

    if (CFGetTypeID(rawValue) != expectedType) {
        illegalTypeProperties = __KXKextGetIllegalTypeProperties(aKext);
        if (! illegalTypeProperties) {
            return false;
        }

        if (!propPathArray) {
            CFArrayAppendValue(illegalTypeProperties, propKey);
        } else {
            propPathString = CFStringCreateByCombiningStrings(
                kCFAllocatorDefault, propPathArray, CFSTR(":"));
            if (!propPathString) return false;
            CFArrayAppendValue(illegalTypeProperties, propPathString);
            CFRelease(propPathString);
        }

        return false;
    }

    return true;
}

/*******************************************************************************
*
*******************************************************************************/
static Boolean __KXKextCheckStringProperty(
    KXKextRef aKext,
    CFDictionaryRef aDictionary,
    CFStringRef propKey,
    CFArrayRef propPathArray,
    Boolean     isRequired,
    CFStringRef expectedValue,
    CFStringRef *stringValueOut)
{
    CFTypeRef   rawValue = NULL;
    CFStringRef stringValue = NULL;
    CFMutableArrayRef missingProperties;
    CFMutableArrayRef illegalTypeProperties;
    CFMutableArrayRef illegalValueProperties;
    CFStringRef       propPathString = NULL;  // must release

    // FIXME: Can't bail caller on allocation failure! However, another
    // FIXME: ...alloc failure is sure to happen soon after so the caller
    // FIXME: ...will bail then.

    if (stringValueOut) *stringValueOut = NULL;

    rawValue = CFDictionaryGetValue(aDictionary,
        propKey);

    if (!rawValue) {
        if (!isRequired) {
            return true;
        } else {
            missingProperties = __KXKextGetMissingProperties(aKext);
            if (!missingProperties) {
                return false;
            }
            if (!propPathArray) {
                CFArrayAppendValue(missingProperties, propKey);
            } else {
                propPathString = CFStringCreateByCombiningStrings(
                    kCFAllocatorDefault, propPathArray, CFSTR(":"));
                if (!propPathString) return false;
                CFArrayAppendValue(missingProperties, propPathString);
                CFRelease(propPathString);
            }
            return false;
        }
    }

    if (CFGetTypeID(rawValue) != CFStringGetTypeID()) {
        illegalTypeProperties = __KXKextGetIllegalTypeProperties(aKext);
        if (!illegalTypeProperties) {
            return false;
        }
        if (!propPathArray) {
            CFArrayAppendValue(illegalTypeProperties, propKey);
        } else {
            propPathString = CFStringCreateByCombiningStrings(
                kCFAllocatorDefault, propPathArray, CFSTR(":"));
            if (!propPathString) return false;
            CFArrayAppendValue(illegalTypeProperties, propPathString);
            CFRelease(propPathString);
        }

        return false;
    }

    stringValue = (CFStringRef)rawValue;
    if (stringValueOut) *stringValueOut = stringValue;

    if (expectedValue) {
        if (CFStringCompare(expectedValue, stringValue, 0) !=
            kCFCompareEqualTo) {

            illegalValueProperties = __KXKextGetIllegalValueProperties(aKext);
            if (!illegalValueProperties) {
                return false;
            }

            if (!propPathArray) {
                CFArrayAppendValue(illegalValueProperties, propKey);
            } else {
                propPathString = CFStringCreateByCombiningStrings(
                    kCFAllocatorDefault, propPathArray, CFSTR(":"));
                if (!propPathString) return false;
                CFArrayAppendValue(illegalValueProperties, propPathString);
                CFRelease(propPathString);
            }

            return false;
        }
    }

    return true;
}

/*******************************************************************************
*
*******************************************************************************/
static Boolean __KXKextCheckVersionProperty(
    KXKextRef      aKext,
    CFDictionaryRef aDictionary,
    CFStringRef     propKey,
    CFArrayRef      propPathArray,
    Boolean         isRequired,
    char          **versionStringOut,
    VERS_version   *version)
{
    char vers_buffer[32];  // more than long enough for legal vers
    CFStringRef stringValue = NULL;
    CFStringRef propPathString = NULL;  // must release
    CFMutableArrayRef illegalValueProperties = NULL;

    if (!__KXKextCheckStringProperty(aKext, aDictionary,
        propKey, propPathArray, isRequired, NULL, &stringValue)) {
        if (isRequired) return false;
        else return true;
    }

    if (!isRequired && !stringValue) {
        return true;
    }

    if (!CFStringGetCString(stringValue,
        vers_buffer, sizeof(vers_buffer) - 1, kCFStringEncodingUTF8)) {

        illegalValueProperties = __KXKextGetIllegalValueProperties(aKext);
        if (!illegalValueProperties) return false;

        if (!propPathArray) {
            CFArrayAppendValue(illegalValueProperties, propKey);
        } else {
            propPathString = CFStringCreateByCombiningStrings(
                kCFAllocatorDefault, propPathArray, CFSTR(":"));
            if (!propPathString) return false;

            CFArrayAppendValue(illegalValueProperties, propPathString);
            CFRelease(propPathString);
        }
 
        return false;

    } else {
        vers_buffer[sizeof(vers_buffer) - 1] = '\0';

        if (versionStringOut) {
            *versionStringOut = strdup(vers_buffer);
            if (!*versionStringOut) {
                return false;
            }
        }

        *version = VERS_parse_string(vers_buffer);
        if (*version < 0) {

            illegalValueProperties = __KXKextGetIllegalValueProperties(aKext);
            if (! illegalValueProperties) return false;

            if (!propPathArray) {
                CFArrayAppendValue(illegalValueProperties, propKey);
            } else {
                propPathString = CFStringCreateByCombiningStrings(
                        kCFAllocatorDefault, propPathArray, CFSTR(":"));
                if (!propPathString) return false;

                CFArrayAppendValue(illegalValueProperties, propPathString);
                CFRelease(propPathString);
            }

            return false;
        }
    }

    if (CFEqual(propKey, CFSTR("CFBundleVersion"))) {

        if (CFStringGetLength(stringValue) > KMOD_MAX_NAME - 1) {

            CFDictionarySetValue(__KXKextGetOrCreateValidationFailures(aKext),
                kKXKextErrorKeyIdentifierOrVersionTooLong,
                kCFBooleanTrue);

            return false;
        }
    }

    return true;
}

/*******************************************************************************
*
*******************************************************************************/
static Boolean __KXKextCheckPersonalityTypes(KXKextRef aKext,
    CFTypeRef cfObj, CFMutableArrayRef propPathArray)
{
    Boolean result = true;
    Boolean localResult = true;
    Boolean foundInvalidType = false;
    CFTypeID typeID;
    CFStringRef arrayIndexString = NULL;  // must release
    CFStringRef propPathString = NULL;  // must release
    CFMutableArrayRef illegalTypeProperties = NULL;  // don't release
    CFStringRef * keys = NULL;   // must free
    CFStringRef * values = NULL; // must free

    typeID = CFGetTypeID(cfObj);

    if (typeID == CFDictionaryGetTypeID()) {
        CFDictionaryRef dict = (CFDictionaryRef)cfObj;
        CFIndex count, i;
        count = CFDictionaryGetCount(dict);
        keys = (CFStringRef *)malloc(count * sizeof(CFStringRef));
        values = (CFStringRef *)malloc(count * sizeof(CFTypeRef));

        CFDictionaryGetKeysAndValues(dict, (const void **)keys,
            (const void **)values);

        for (i = 0; i < count; i++) {
            CFArrayAppendValue(propPathArray, keys[i]);
            localResult = __KXKextCheckPersonalityTypes(aKext, values[i], propPathArray);
            CFArrayRemoveValueAtIndex(propPathArray,
                CFArrayGetCount(propPathArray) - 1);
            if (!localResult) {
               result = false;
                if (!KXKextManagerPerformsFullTests(aKext->manager)) {
                    goto finish;
                }
            }
        }
    } else if (typeID == CFArrayGetTypeID()) {
        CFArrayRef array = (CFArrayRef)cfObj;
        CFIndex count, i;
        count = CFArrayGetCount(array);

        for (i = 0; i < count; i++) {
            arrayIndexString = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                CFSTR("[%d]"), i);
            if (!arrayIndexString) {
                result = false;
                goto finish;
            }
            CFArrayAppendValue(propPathArray, arrayIndexString);
            CFRelease(arrayIndexString);
            arrayIndexString = NULL;
            localResult = __KXKextCheckPersonalityTypes(aKext,
                CFArrayGetValueAtIndex(array, i),
                propPathArray);
            CFArrayRemoveValueAtIndex(propPathArray,
                CFArrayGetCount(propPathArray) - 1);
            if (!localResult) {
               result = false;
                if (!KXKextManagerPerformsFullTests(aKext->manager)) {
                    goto finish;
                }
            }
        }
    } else if (typeID == CFStringGetTypeID() || typeID == CFDataGetTypeID() ||
               typeID == CFBooleanGetTypeID()) {

        // these types are all valid atomic types; do nothing
    } else if (typeID == CFNumberGetTypeID()) {
        CFNumberRef number = (CFNumberRef)cfObj;
        if (CFNumberIsFloatType(number)) {
            foundInvalidType = true;
        }
    } else {
        foundInvalidType = true;
    }

    if (foundInvalidType) {
        // add entry to error dict
        illegalTypeProperties = __KXKextGetIllegalTypeProperties(aKext);
        if (!illegalTypeProperties) return false;

        propPathString = CFStringCreateByCombiningStrings(
            kCFAllocatorDefault, propPathArray, CFSTR(":"));
        if (!propPathString) return false;

        CFArrayAppendValue(illegalTypeProperties, propPathString);
        CFRelease(propPathString);
        propPathString = NULL;
        result = false;
        if (!KXKextManagerPerformsFullTests(aKext->manager)) {
            goto finish;
        }
    }

finish:
    if (keys)   free(keys);
    if (values) free(values);
    if (arrayIndexString) CFRelease(arrayIndexString);
    if (propPathString)   CFRelease(propPathString);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
static KXKextManagerError __KXKextAuthenticateURLAndParents(KXKextRef aKext,
    CFURLRef anURL,
    CFURLRef topURL /* must be absolute */,
    CFMutableDictionaryRef checkedURLs)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    KXKextManagerError checkResult = kKXKextManagerErrorNone;
    CFURLRef backscanURL = NULL;     // must release
    char * anURL_path = NULL;        // must free
    char * topURL_path = NULL;       // must free

    backscanURL = PATH_CopyCanonicalizedURL(anURL);
    if (!backscanURL) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    // CHECK THAT anURL is contained by topURL
    anURL_path = PATH_CanonicalizedCStringForURL(backscanURL);
    topURL_path = PATH_CanonicalizedCStringForURL(topURL);
    if (strncmp(anURL_path, topURL_path, strlen(topURL_path))) {
        _KXKextManagerLogError(KXKextGetManager(aKext),"\"%s\" is not contained within \"%s\"",
            anURL_path, topURL_path);
        result = kKXKextManagerErrorUnspecified;
        goto finish;
    }

    while (!CFEqual(backscanURL, topURL)) {
        CFURLRef parentURL = NULL; // must release

       /* Only check this URL if we haven't already done so.
        */
        if (!CFDictionaryGetValue(checkedURLs, backscanURL)) {

            if (_KXKextManagerCheckLogLevel(KXKextGetManager(aKext), kKXKextManagerLogLevelKextDetails,
                aKext, kKXKextLogLevelDetails)) {

                const char * file_path =
                    PATH_CanonicalizedCStringForURL(backscanURL);
                if (file_path) {
                    _KXKextManagerLogMessage(KXKextGetManager(aKext),
                        "authenticating file/directory \"%s\"",
                        file_path);
                    free((char *)file_path);
                }
            }

            checkResult = __KXKextAuthenticateURL(aKext, backscanURL);
            if (checkResult != kKXKextManagerErrorNone) {
                result = checkResult;
                if (result == kKXKextManagerErrorNoMemory ||
                    !KXKextManagerPerformsFullTests(aKext->manager)) {

                    goto finish;
                }
            }
        }

       /* Note that we've checked this URL.
        */
        CFDictionarySetValue(checkedURLs, backscanURL, kCFBooleanTrue);

        parentURL = CFURLCreateCopyDeletingLastPathComponent(
                kCFAllocatorDefault, backscanURL);
        if (!parentURL) {
            result = kKXKextManagerErrorUnspecified;
            goto finish;
        }

        CFRelease(backscanURL);
        backscanURL = parentURL;
        parentURL = NULL;
    }

finish:
    if (backscanURL)  CFRelease(backscanURL);
    if (anURL_path)   free(anURL_path);
    if (topURL_path)  free(topURL_path);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
static KXKextManagerError __KXKextAuthenticateURL(KXKextRef aKext,
    CFURLRef anURL)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    CFMutableArrayRef missingFiles = NULL;
    CFMutableArrayRef badOwnerPermsFiles = NULL;

    CFURLRef absURL = NULL;       // must release
    CFStringRef urlPath = NULL;  // must release
    char path[MAXPATHLEN];
    struct stat stat_buf;

    absURL = PATH_CopyCanonicalizedURL(anURL);
    if (!absURL) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    urlPath = CFURLCopyFileSystemPath(absURL, kCFURLPOSIXPathStyle);
    if (!urlPath) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    if (!CFStringGetFileSystemRepresentation(urlPath, path, CFStringGetMaximumSizeOfFileSystemRepresentation(urlPath))) {

        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    if (stat(path, &stat_buf) != 0) {
        if (errno == ENOENT) {
            missingFiles = __KXKextGetMissingFiles(aKext);
            if (missingFiles &&
                kCFNotFound == CFArrayGetFirstIndexOfValue(missingFiles,
                     CFRangeMake(0, CFArrayGetCount(missingFiles)),
                     urlPath)) {

                    CFArrayAppendValue(missingFiles, urlPath);
            }
            result = kKXKextManagerErrorValidation;
            goto finish;
        } else {
            perror(path);
            result = kKXKextManagerErrorUnspecified;
            goto finish;
        }
    }

   /* File/dir must be owned by root and not writeable by others,
    * and if not owned by gid 0 then not group-writeable.
    */
    if ( (stat_buf.st_uid != 0) || (stat_buf.st_gid != 0 ) ||
         (stat_buf.st_mode & S_IWOTH) || (stat_buf.st_mode & S_IWGRP) ) {

        badOwnerPermsFiles = __KXKextGetBadOwnerPermsFiles(aKext);
        if (badOwnerPermsFiles &&
            kCFNotFound == CFArrayGetFirstIndexOfValue(badOwnerPermsFiles,
                CFRangeMake(0, CFArrayGetCount(badOwnerPermsFiles)),
                urlPath)) {

            CFArrayAppendValue(badOwnerPermsFiles, urlPath);
        }
        result = kKXKextManagerErrorAuthentication;
        goto finish;
    }


finish:
    if (absURL)  CFRelease(absURL);
    if (urlPath) CFRelease(urlPath);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
static KXKextManagerError __KXKextMakeFTSEntrySecure(
    KXKextRef aKext,
    FTSENT * ftsentry)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    int change_result = 0;
    mode_t fixed_mode = 0;

    change_result = chown(ftsentry->fts_path, 0, 0);  // root, wheel
    if (change_result != 0) {
        _KXKextManagerLogError(KXKextGetManager(aKext),
            "can't change ownership/group of %s",
            ftsentry->fts_path);

        result = kKXKextManagerErrorFileAccess;
        goto finish;
    }

    if (ftsentry->fts_statp->st_mode & S_IFDIR) {
        fixed_mode = 0755;  // octal
    } else {
        fixed_mode = 0644;  // octal
    }

    change_result = chmod(ftsentry->fts_path, fixed_mode);
    if (change_result != 0) {
        _KXKextManagerLogError(KXKextGetManager(aKext),
            "can't change permissions on %s",
            ftsentry->fts_path);
        result = kKXKextManagerErrorFileAccess;
        goto finish;
    }

finish:
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
#define KMOD_INFO  "_kmod_info"

static KXKextManagerError __KXKextCheckKmod(KXKextRef aKext,
    Boolean requiresNewKextManager)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    fat_iterator fiter = NULL;     // must close
    void * arch_hdr;
    void * arch_end;

    fiter = _KXKextCopyFatIterator(aKext);
    if (!fiter) {
        CFDictionarySetValue(__KXKextGetOrCreateValidationFailures(aKext),
            kKXKextErrorKeyExecutableBad,
            kCFBooleanTrue);
        result = kKXKextManagerErrorValidation;
        goto finish;
    }

    while ((arch_hdr = (struct mach_hdr *)fat_iterator_next_arch(
         fiter, &arch_end))) {

        KXKextManagerError thisArchResult = kKXKextManagerErrorNone;

       /* XXX: We might want to record the individual archictures that
        * XXX: fail this test, but since the kext needs to be rebuilt
        * XXX: for at least one, it isn't so important.
        */
        thisArchResult = __check_kmod_info(aKext,
            (struct mach_header *)arch_hdr, arch_end, requiresNewKextManager);
        if (thisArchResult != kKXKextManagerErrorNone) {
            result = thisArchResult;
        }
    }

finish:

    if (fiter) fat_iterator_close(fiter);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
static KXKextManagerError __check_kmod_info(KXKextRef aKext,
    struct mach_header * mach_header,
    void * file_end,
    Boolean requiresNewKextManager) {

    KXKextManagerError result = kKXKextManagerErrorNone;
    const void * symbol_address = NULL;   // do not free
    const kmod_info_t * kmod_info = NULL;  // do not free
    CFStringRef bundleIdentifier = NULL;  // do not release
    CFStringRef kmodName = NULL;          // must release
    VERS_version version;
    macho_seek_result seek_result;
    const struct nlist * nlist_entry;

    seek_result = macho_find_symbol(
            mach_header, file_end, KMOD_INFO, &nlist_entry,
            &symbol_address);

    if ((macho_seek_result_found != seek_result) ||
        ((nlist_entry->n_type & N_TYPE) != N_SECT) ||
        (symbol_address == NULL)) {

        CFDictionarySetValue(__KXKextGetOrCreateValidationFailures(aKext),
            kKXKextErrorKeyExecutableBad,
            kCFBooleanTrue);
        result = kKXKextManagerErrorValidation;
        goto finish;
    }

    kmod_info = (kmod_info_t *)symbol_address;

   /*****
    * If the kext requires Jaguar or later, we don't have to inspect
    * the MODULE_NAME and MODULE_VERSION fields of the kmod_info struct.
    * FIXME: change version reference
    */
    if (requiresNewKextManager) {
        goto finish;
    }

    bundleIdentifier = (CFStringRef)CFDictionaryGetValue(aKext->infoDictionary,
        CFSTR("CFBundleIdentifier"));
    kmodName = CFStringCreateWithCString(kCFAllocatorDefault,
        kmod_info->name, kCFStringEncodingUTF8);
    if (!kmodName) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    if (CFStringCompare(bundleIdentifier, kmodName, 0) !=
        kCFCompareEqualTo) {
        CFDictionarySetValue(__KXKextGetOrCreateWarnings(aKext),
            kKXKextErrorKeyBundleIdentifierMismatch,
            kCFBooleanTrue);
    }

    version = VERS_parse_string(kmod_info->version);
    if (version < 0 || aKext->version != version) {
        CFDictionarySetValue(__KXKextGetOrCreateWarnings(aKext),
            kKXKextErrorKeyBundleVersionMismatch,
            kCFBooleanTrue);
    }

finish:
    if (kmodName) CFRelease(kmodName);
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
static KXKextManagerError __KXKextResolveDependencies(KXKextRef aKext,
    unsigned int recursionDepth)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    KXKextManagerRef manager = KXKextGetManager(aKext);

    CFIndex count, i;
    CFDictionaryRef libraries = NULL;
    CFStringRef * libraryIDs = NULL;      // must free
    CFStringRef * libraryVersions = NULL; // must free
    Boolean       hasDirectKernelDependency = false;

    KXKextManagerError localResult = kKXKextManagerErrorNone;

    if (!aKext->flags.canResolveDependencies) {
        const char * kext_name =
            _KXKextCopyCanonicalPathnameAsCString(aKext);  // must free
        if (kext_name && __KXKextCheckLogLevel(aKext,
                kKXKextManagerLogLevelKextDetails,
                kKXKextLogLevelDetails, false)) {

            _KXKextManagerLogError(KXKextGetManager(aKext),
                "%s has validation failures that prevent resolution of dependencies",
                kext_name);
        }
        result = kKXKextManagerErrorUnspecified;
        goto finish;
    }

   /* A kext that's compiled into the kernel trivially has all its dependencies met.
    */
    if (aKext->flags.isKernelResource) {
        if (_KXKextManagerCheckLogLevel(KXKextGetManager(aKext),
            kKXKextManagerLogLevelKextDetails,
            aKext, kKXKextLogLevelDetails)) {

            const char * kext_name =
                _KXKextCopyCanonicalPathnameAsCString(aKext);  // must free
            const char * message;

            message = "%s is a kernel resource and thus has no dependencies";

            if (kext_name) {
                _KXKextManagerLogMessage(KXKextGetManager(aKext),
                    message, kext_name);
            }
            if (kext_name) free((char *)kext_name);
        }

        aKext->flags.hasAllDependencies = 1;
        result = kKXKextManagerErrorNone;
        goto finish;
    }

   /* If we've already done the work, return immediately.
    */
    if (aKext->flags.hasAllDependencies) {
        if (__KXKextCheckLogLevel(aKext, kKXKextManagerLogLevelKextDetails,
              kKXKextLogLevelDetails, false)) {

            const char * kext_name =
                _KXKextCopyCanonicalPathnameAsCString(aKext);  // must releast

            if (kext_name) {
                _KXKextManagerLogMessage(KXKextGetManager(aKext),
                    "extension %s has already resolved its dependencies", kext_name);
            }
            if (kext_name) free((char *)kext_name);
        }

        result = kKXKextManagerErrorNone;
        goto finish;
    }

   /* If we hit a depth this big, there's probably a loop. Bail.
    */
    if (recursionDepth > 255) {
        CFDictionarySetValue(__KXKextGetOrCreateValidationFailures(aKext),
            kKXKextErrorKeyPossibleDependencyLoop,
            kCFBooleanTrue);
        result = kKXKextManagerErrorDependencyLoop;
        goto finish;
    }

   /*****
    * Otherwise clear out any partial dependency information,
    * including info on missing dependencies.
    */
    if (aKext->directDependencies) {
        CFArrayRemoveAllValues(aKext->directDependencies);
    } else {
        aKext->directDependencies = CFArrayCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeArrayCallBacks);
        if (!aKext->directDependencies) {
            result = kKXKextManagerErrorNoMemory;
            goto finish;
        }
    }

    if (aKext->missingDependencies) {
        CFDictionaryRemoveAllValues(aKext->missingDependencies);
    } else {
        aKext->missingDependencies = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
        if (!aKext->missingDependencies) {
            result = kKXKextManagerErrorNoMemory;
            goto finish;
        }
    }

   /*****
    * Now get to work. Start with this kext's dependencies.
    */
    libraries = CFDictionaryGetValue(aKext->infoDictionary,
        CFSTR("OSBundleLibraries"));
    if (libraries) {
        count = CFDictionaryGetCount(libraries);
    }

    if (!libraries || !count) {
        if (!aKext->flags.isKernelResource && aKext->flags.declaresExecutable) {
            // This should have been caught by __KXKextValidate()
            // (and in fact shouldn't allow this function to get this far!),
            // so we don't record the failure here.
            result = kKXKextManagerErrorValidation;
        } else {
            aKext->flags.hasAllDependencies = 1;
        }
        goto finish;
    }

   /* Prepare to iterate the dependecy entries.
    */
    libraryIDs = (CFStringRef *)malloc(count * sizeof(CFStringRef));
    libraryVersions = (CFStringRef *)malloc(count * sizeof(CFStringRef));
    if (!libraryIDs || !libraryVersions) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    CFDictionaryGetKeysAndValues(libraries, (const void **)libraryIDs,
        (const void **)libraryVersions);

    for (i = 0; i < count; i++) {
        KXKextRef thisDependency;

        if (_KXKextManagerCheckLogLevel(KXKextGetManager(aKext),
            kKXKextManagerLogLevelKextDetails,
            aKext, kKXKextLogLevelDetails)) {

            Boolean got_id, got_vers;
            char dep_id[255];
            char dep_vers[255];
            const char * kext_name =
                _KXKextCopyCanonicalPathnameAsCString(aKext);  // must releast

            got_id = CFStringGetCString(libraryIDs[i],
                dep_id, sizeof(dep_id) - 1, kCFStringEncodingUTF8);
            got_vers = CFStringGetCString(libraryVersions[i],
                dep_vers, sizeof(dep_vers) - 1, kCFStringEncodingUTF8);

            if (got_id && got_vers && kext_name) {
                _KXKextManagerLogMessage(KXKextGetManager(aKext),
                    "looking for dependency of extension %s with ID %s, "
                    "compatible with version %s",
                    kext_name, dep_id, dep_vers);
            }
            if (kext_name) free((char *)kext_name);
        }

       /* Find a kext compatible with the needed version.
        */
        thisDependency =
            KXKextManagerGetKextWithIdentifierCompatibleWithVersionString(
                manager, libraryIDs[i], libraryVersions[i]);

        if (thisDependency) {

            hasDirectKernelDependency |= KXKextGetIsKernelResource(thisDependency);

            if (_KXKextManagerCheckLogLevel(KXKextGetManager(aKext),
                 kKXKextManagerLogLevelKextDetails,
                 aKext, kKXKextLogLevelDetails)) {

                const char * kext_name =
                    _KXKextCopyCanonicalPathnameAsCString(aKext);
                const char * dep_name =
                    _KXKextCopyCanonicalPathnameAsCString(thisDependency);
                if (kext_name && dep_name) {
                    _KXKextManagerLogMessage(KXKextGetManager(aKext),
                       "found compatible dependency from extension %s to %s; "
                       "resolving its dependencies",
                        kext_name, dep_name);
                }
                if (kext_name) free((char *)kext_name);
                if (dep_name) free((char *)dep_name);
            }

           /* Found the current one; have it resolve its own dependencies.
            */
            localResult = __KXKextResolveDependencies(thisDependency,
                recursionDepth + 1);

            if (localResult == kKXKextManagerErrorNone) {
               /* No error; so add the dependency to the direct list.
                */
                CFArrayAppendValue(aKext->directDependencies, thisDependency);

            } else {

                if (_KXKextManagerCheckLogLevel(KXKextGetManager(aKext),
                     kKXKextManagerLogLevelKextDetails,
                     aKext, kKXKextLogLevelDetails)) {

                    const char * kext_name =
                        _KXKextCopyCanonicalPathnameAsCString(aKext);
                    const char * dep_name =
                        _KXKextCopyCanonicalPathnameAsCString(thisDependency);
                    if (kext_name && dep_name) {
                        _KXKextManagerLogMessage(KXKextGetManager(aKext),
                            "failed to resolve dependencies for extension %s",
                            dep_name);
                    }
                    if (kext_name) free((char *)kext_name);
                    if (dep_name) free((char *)dep_name);
                }

               /* Oops, couldn't do it. Although we did find this dependency,
                * add its ID to the list of missing dependencies as having
                * unresolvable dependencies of its own.
                */
                CFDictionarySetValue(aKext->missingDependencies, libraryIDs[i],
                    kKXKextIndirectDependencyUnresolvable);

                if (!KXKextManagerPerformsFullTests(aKext->manager)) {
                    goto finish;
                }
            }
        } else {

           /* Didn't find a direct dependency, so the show's over for this kext
            * (unless doing a full diagnostic, in which case keep trying the others).
            */
            KXKextRef incompatibleDependency =
                KXKextManagerGetKextWithIdentifier(manager, libraryIDs[i]);

            if (incompatibleDependency) {
                if (!_KXKextGetCompatibleVersion(incompatibleDependency)) {
                    CFDictionarySetValue(aKext->missingDependencies,
                        libraryIDs[i],
                        kKXKextDependencyCompatibleVersionUndeclared);
                } else {
                    CFDictionarySetValue(aKext->missingDependencies,
                        libraryIDs[i], kKXKextDependencyNoCompatibleVersion);
                }

                if (_KXKextManagerCheckLogLevel(KXKextGetManager(aKext),
                    kKXKextManagerLogLevelKextDetails,
                    aKext, kKXKextLogLevelDetails)) {

                    const char * kext_name =
                        _KXKextCopyCanonicalPathnameAsCString(aKext);
                    char dep_id[255];
                    if (kext_name && CFStringGetCString(libraryIDs[i],
                        dep_id, sizeof(dep_id) - 1, kCFStringEncodingUTF8)) {

                        _KXKextManagerLogMessage(KXKextGetManager(aKext),
                            "can't resolve dependency from %s to ID %s; "
                            "no compatible version of ID %s is known",
                            kext_name, dep_id, dep_id);
                    }
                    if (kext_name) free((char *)kext_name);
                }
            } else {  // no dependency found at all
                CFDictionarySetValue(aKext->missingDependencies, libraryIDs[i],
                    kKXKextDependencyUnavailable);

                if (__KXKextCheckLogLevel(aKext, kKXKextManagerLogLevelKextDetails,
                      kKXKextLogLevelDetails, false)) {
                    const char * kext_name =
                        _KXKextCopyCanonicalPathnameAsCString(aKext);
                    char dep_id[255];
                    if (kext_name && CFStringGetCString(libraryIDs[i],
                        dep_id, sizeof(dep_id) - 1, kCFStringEncodingUTF8)) {

                        _KXKextManagerLogError(KXKextGetManager(aKext),
                            "can't resolve dependency from extension %s to ID %s; "
                            "no extension with ID %s is known",
                            kext_name, dep_id, dep_id);
                    }
                    if (kext_name) free((char *)kext_name);
                }

            }
            result = kKXKextManagerErrorDependency;
            if (!KXKextManagerPerformsFullTests(aKext->manager)) {
                goto finish;
            }
        }
    }

   /* A kext with an executable but without any explicit kernel dependency
    * is assumed dependent on 6.0.
    */
    if ((result == kKXKextManagerErrorNone) &&
        KXKextGetDeclaresExecutable(aKext) &&
        !hasDirectKernelDependency) {

        KXKextRef kernelDependency =
            KXKextManagerGetKextWithIdentifierCompatibleWithVersionString(
                manager, CFSTR("com.apple.kernel.libkern"), CFSTR("6.0"));
        if (kernelDependency) {
            CFArrayAppendValue(aKext->directDependencies, kernelDependency);
            CFDictionarySetValue(__KXKextGetOrCreateWarnings(aKext),
                kKXKextErrorKeyNoExplicitKernelDependency,
                kCFBooleanTrue);

        } else {
            result = kKXKextManagerErrorDependency;
        }
    }

    if (result == kKXKextManagerErrorNone) {
        aKext->flags.hasAllDependencies = 1;
    }

finish:
    if (libraryIDs)       free(libraryIDs);
    if (libraryVersions)  free(libraryVersions);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
static char * __KXKextCopyDgraphEntryName(KXKextRef aKext)
{
    KXKextManagerError checkResult = kKXKextManagerErrorNone;
    CFStringRef bundleID = NULL;       // don't release
    CFURLRef    executableURL = NULL;  // must release
    char * kmod_path = NULL;           // don't free (alias for entry_name)
    char * bundle_identifier = NULL;   // don't free (alias for entry_name)
    char * entry_name = NULL;          // returned
    size_t size;
    int error = 0;

    checkResult = __KXKextRealizeFromCache(aKext);
    if (checkResult != kKXKextManagerErrorNone) {
        // there can be no further test at this point
        error = 1;
        goto finish;
    }

    if (aKext->infoDictionary) {
        CFStringRef ident;
        CFStringRef vers;
        KXKextRef   sharedKext;

        ident = (CFStringRef) CFDictionaryGetValue(aKext->infoDictionary, 
            CFSTR("OSBundleSharedExecutableIdentifier"));
        vers = (CFStringRef) CFDictionaryGetValue(aKext->infoDictionary, 
            CFSTR("CFBundleVersion"));
        if (ident && vers) {
            sharedKext = KXKextManagerGetKextWithIdentifierAndVersionString(
                aKext->manager, ident, vers);
            if (sharedKext) {
                aKext = sharedKext;
                checkResult = __KXKextRealizeFromCache(aKext);
                if (checkResult != kKXKextManagerErrorNone) {
                    // there can be no further test at this point
                    error = 1;
                    goto finish;
                }
            } else {
                const char * kext_name =
                    _KXKextCopyCanonicalPathnameAsCString(aKext);
                _KXKextManagerLogError(KXKextGetManager(aKext),
                    "can't resolve OSBundleSharedExecutableIdentifier from extension %s",
                    kext_name ? kext_name : "");
                if (kext_name) free((char *)kext_name);
                error = 1;
                goto finish;
            }
        }
    }

    if (!KXKextGetDeclaresExecutable(aKext) && KXKextGetIsKernelResource(aKext)) {
        bundleID = KXKextGetBundleIdentifier(aKext);
        size = (sizeof(char) * MAXPATHLEN);
        bundle_identifier = (char *)malloc(size);
        if (!bundle_identifier) {
            error = 1;
            goto finish;
        }
        if (!CFStringGetCString(bundleID,
            bundle_identifier, size - 1, kCFStringEncodingUTF8)) {

            error = 1;
            goto finish;
        }
        entry_name = bundle_identifier;
    } else {
        executableURL = CFBundleCopyExecutableURL(aKext->bundle);
        if (!executableURL) {
            error = 1;
            goto finish;
        }
        kmod_path = PATH_CanonicalizedCStringForURL(executableURL);
        if (!kmod_path) {
            error = 1;
            goto finish;
        }
        entry_name = kmod_path;
    }

finish:

    if (error && entry_name) {
        free(entry_name);
        entry_name = NULL;
    }

    if (executableURL) CFRelease(executableURL);

    return entry_name;
}

/*******************************************************************************
*
*******************************************************************************/
static char * __KXKextCopyDgraphKmodName(KXKextRef aKext)
{
    CFStringRef bundleID = NULL;       // don't release
    char * bundle_identifier = NULL;   // returned
    CFIndex bufSize;
    int error = 0;

    bundleID = (CFStringRef) CFDictionaryGetValue(aKext->infoDictionary, 
        CFSTR("OSBundleSharedExecutableIdentifier"));
    if (!bundleID)
        bundleID = KXKextGetBundleIdentifier(aKext);
    bufSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength (bundleID),
	    kCFStringEncodingUTF8) + sizeof('\0'); 
    bundle_identifier = (char *)malloc(bufSize);
    if (!bundle_identifier) {
        goto finish;
    }
    if (!CFStringGetCString(bundleID,
        bundle_identifier, bufSize, kCFStringEncodingUTF8)) {

        error = 1;
        goto finish;
    }

finish:
    if (error && bundle_identifier) {
        free(bundle_identifier);
        bundle_identifier = NULL;
    }
    return bundle_identifier;
}

/*******************************************************************************
*
*******************************************************************************/
static Boolean __KXKextAddDependenciesToDgraph(KXKextRef aKext,
    CFArrayRef dependencies,
    dgraph_t * dgraph,
    Boolean    skipKernelDependencies)
{
    Boolean result = true;
    char * entry_name = NULL;            // must free
    char * expected_kmod_name = NULL;    // must free
    dgraph_entry_t * aKext_entry = NULL; // don't free
    CFIndex count, i;

    if (KXKextGetDeclaresExecutable(aKext)) {
        entry_name = __KXKextCopyDgraphEntryName(aKext);
        if (!entry_name) {
            result = false;
            goto finish;
        }

        expected_kmod_name = __KXKextCopyDgraphKmodName(aKext);
        if (!expected_kmod_name) {
            result = false;
            goto finish;
        }

        // add aKext as dependent
        aKext_entry = dgraph_add_dependent(dgraph, entry_name,
            expected_kmod_name, aKext->versionString,
            /* load address */ 0, KXKextGetIsKernelResource(aKext));
        if (!aKext_entry) {
            result = false;
            goto finish;
        }

        free(entry_name);
        entry_name = NULL;
        free(expected_kmod_name);
        expected_kmod_name = NULL;
    }

    if (!dependencies) {
        goto finish;
    }

    count = CFArrayGetCount(dependencies);
    for (i = 0; i < count; i++) {
        KXKextRef thisKext = (KXKextRef)CFArrayGetValueAtIndex(
            dependencies, i);

       /* If this kext has an executable, insert it into the dgraph
        * as a dependency of aKext.
        */ 
        if (KXKextGetDeclaresExecutable(thisKext) ||
            KXKextGetIsKernelResource(thisKext)) {

            if (skipKernelDependencies && KXKextGetIsKernelResource(thisKext))
                continue;

            entry_name = __KXKextCopyDgraphEntryName(thisKext);
            if (!entry_name) {
                result = false;
                goto finish;
            }
            if (expected_kmod_name) {
                free(expected_kmod_name);
                expected_kmod_name = NULL;
            }
            expected_kmod_name = __KXKextCopyDgraphKmodName(thisKext);
            if (!expected_kmod_name) {
                result = false;
                goto finish;
            }
            if (!dgraph_add_dependency(dgraph, aKext_entry, entry_name,
                    expected_kmod_name, thisKext->versionString,
                    /* load address */ 0, KXKextGetIsKernelResource(thisKext))) {

                result = false;
                goto finish;
            }

           /* Add the dependencies of a kext with an executable to the dgraph.
            */
            if (!__KXKextAddDependenciesToDgraph(thisKext,
                KXKextGetDirectDependencies(thisKext), dgraph, false)) {
                result = false;
                goto finish;
            }
            free(entry_name);
            entry_name = NULL;
        } else {

           /* For a dependency kext that has no executable, add its dependencies
            * as if they were direct dependencies of aKext.
            */
            if (!__KXKextAddDependenciesToDgraph(aKext,
                KXKextGetDirectDependencies(thisKext), dgraph, true)) {

                result = false;
                goto finish;
            }
        }
    }

finish:

    if (expected_kmod_name) free(expected_kmod_name);
    if (entry_name) free(entry_name);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
static void __KXKextAddDependenciesToArray(KXKextRef aKext,
    CFMutableArrayRef dependencyArray)
{
    if (aKext->directDependencies) {
        CFIndex dCount, dIndex;
        CFIndex aCount, aIndex;

       /*****
        * First have all dependencies add their own dependencies
        * to the array so the array is built in load order.
        */
        dCount = CFArrayGetCount(aKext->directDependencies);
        for (dIndex = 0; dIndex < dCount; dIndex ++) {
            KXKextRef thisDependency = (KXKextRef)CFArrayGetValueAtIndex(
                aKext->directDependencies, dIndex);
            __KXKextAddDependenciesToArray(thisDependency, dependencyArray);
        }

       /*****
        * Now add this kext's direct dependencies, but only if they
        * aren't already in the list.
        */
        aCount = CFArrayGetCount(dependencyArray);
        for (dIndex = 0; dIndex < dCount; dIndex ++) {
            Boolean gotItAlready = false;
            KXKextRef thisDependency = (KXKextRef)CFArrayGetValueAtIndex(
                aKext->directDependencies, dIndex);
            for (aIndex = 0; aIndex < aCount; aIndex ++) {
                KXKextRef checkDependency = (KXKextRef)CFArrayGetValueAtIndex(
                    dependencyArray, aIndex);
                if (thisDependency == checkDependency) {
                    gotItAlready = true;
                    break;
                }
            }
            if (!gotItAlready) {
                CFArrayAppendValue(dependencyArray, thisDependency);
            }
        }
    }

    return;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError __KXKextRealizeFromCache(KXKextRef aKext)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    CFStringRef absBundlePath = NULL;  // must release
    CFDictionaryRef bundleInfoDictionary = NULL;  // don't release
    CFDictionaryRef comparisonInfoDictionary = NULL;  // must release

    if (aKext->bundle) goto finish;

    aKext->bundle = CFBundleCreate(kCFAllocatorDefault, aKext->bundleURL);
    if (!aKext->bundle) {
        // If the bundle couldn't be created we don't know why
        // without further investigation. We could be out of memory;
        // the entry in the filesystem might not exist; we might not
        // have access to it; the URL given may not point at a proper
        // bundle.
        //
        // FIXME? error for "from cache, no bundle"?
        //
        result = kKXKextManagerErrorCache;
        goto finish;
    }

    bundleInfoDictionary = CFBundleGetInfoDictionary(aKext->bundle);
    comparisonInfoDictionary = __KXKextCopyInfoDictionaryForCache(
        aKext, bundleInfoDictionary, false /* sub keys */ );
    if (!comparisonInfoDictionary) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    if (!CFEqual(aKext->infoDictionary, comparisonInfoDictionary)) {
        result = kKXKextManagerErrorCache;
        // don't jump to finish, apply the from-bundle info dictionary
    }
    CFRelease(aKext->infoDictionary);  // save a little memory
    aKext->infoDictionary = bundleInfoDictionary;  // not the comparison dict!
    CFRetain(bundleInfoDictionary);

   /*****
    * Check that property CFBundlePackageType is "KEXT". This is the only
    * property that we validate that isn't in a cached infoDictionary.
    */
    if (!KXKextIsFromCache(aKext) &&
        !__KXKextCheckStringProperty(aKext, aKext->infoDictionary,
            CFSTR("CFBundlePackageType"), NULL, true, CFSTR("KEXT"), NULL)) {
        result = kKXKextManagerErrorCache;
        // do not goto finish any more
    }


finish:
    if (absBundlePath)            CFRelease(absBundlePath);
    if (comparisonInfoDictionary) CFRelease(comparisonInfoDictionary);

    return result;
}


/*******************************************************************************
*
*******************************************************************************/
typedef struct cacheKeySubs {
    CFStringRef longKey;
    CFStringRef shortKey;
    Boolean     required;
} __KXKextCacheKeySubs;

#define NUM_INFO_DICT_SUB_KEYS    (15)
static __KXKextCacheKeySubs __gInfoDictSubKeys[NUM_INFO_DICT_SUB_KEYS];
#if 0
#define NUM_PERSONALITY_SUB_KEYS  (4)
static __KXKextCacheKeySubs __gPersonalitySubKeys[NUM_PERSONALITY_SUB_KEYS];
#endif 0

static void __initSubKeys(void) {
    static Boolean didIt = false;
    if (didIt) return;

    __gInfoDictSubKeys[0].longKey = CFSTR("CFBundleInfoDictionaryVersion");
    __gInfoDictSubKeys[0].shortKey = CFSTR("d");
    __gInfoDictSubKeys[0].required = true;
 
    __gInfoDictSubKeys[1].longKey = CFSTR("CFBundleGetInfoString");
    __gInfoDictSubKeys[1].shortKey = CFSTR("g");
    __gInfoDictSubKeys[1].required = false;
 
    __gInfoDictSubKeys[2].longKey = CFSTR("CFBundleIdentifier");
    __gInfoDictSubKeys[2].shortKey = CFSTR("i");
    __gInfoDictSubKeys[2].required = true;
 
    __gInfoDictSubKeys[3].longKey = CFSTR("CFBundleExecutable");
    __gInfoDictSubKeys[3].shortKey = CFSTR("x");
    __gInfoDictSubKeys[3].required = false;
 
    __gInfoDictSubKeys[4].longKey = CFSTR("CFBundleName");
    __gInfoDictSubKeys[4].shortKey = CFSTR("n");
    __gInfoDictSubKeys[4].required = false;
 
    __gInfoDictSubKeys[5].longKey = CFSTR("CFBundleShortVersionString");
    __gInfoDictSubKeys[5].shortKey = CFSTR("s");
    __gInfoDictSubKeys[5].required = false;
 
    __gInfoDictSubKeys[6].longKey = CFSTR("CFBundleVersion");
    __gInfoDictSubKeys[6].shortKey = CFSTR("v");
    __gInfoDictSubKeys[6].required = true;
 
    __gInfoDictSubKeys[7].longKey = CFSTR("NSHumanReadableCopyright");
    __gInfoDictSubKeys[7].shortKey = CFSTR("c");
    __gInfoDictSubKeys[7].required = false;
 
    __gInfoDictSubKeys[8].longKey = CFSTR("OSBundleCompatibleVersion");
    __gInfoDictSubKeys[8].shortKey = CFSTR("cv");
    __gInfoDictSubKeys[8].required = false;
 
    __gInfoDictSubKeys[9].longKey = CFSTR("OSBundleRequired");
    __gInfoDictSubKeys[9].shortKey = CFSTR("r");
    __gInfoDictSubKeys[9].required = false;
 
    __gInfoDictSubKeys[10].longKey = CFSTR("IOKitPersonalities");
    __gInfoDictSubKeys[10].shortKey = CFSTR("p");
    __gInfoDictSubKeys[10].required = false;
 
    __gInfoDictSubKeys[11].longKey = CFSTR("OSBundleLibraries");
    __gInfoDictSubKeys[11].shortKey = CFSTR("l");
    __gInfoDictSubKeys[11].required = false;
 
    __gInfoDictSubKeys[12].longKey = CFSTR("OSKernelResource");
    __gInfoDictSubKeys[12].shortKey = CFSTR("k");
    __gInfoDictSubKeys[12].required = false;
 
    __gInfoDictSubKeys[13].longKey = CFSTR("OSBundleDebugLevel");
    __gInfoDictSubKeys[13].shortKey = CFSTR("db");
    __gInfoDictSubKeys[13].required = false;

    __gInfoDictSubKeys[14].longKey = CFSTR("OSBundleHelper");
    __gInfoDictSubKeys[14].shortKey = CFSTR("bh");
    __gInfoDictSubKeys[14].required = false;

#if 0
// Can't do this because there are other entries in a personality and
// they can have just about any key.
    __gPersonalitySubKeys[0].longKey = CFSTR("CFBundleIdentifier");
    __gPersonalitySubKeys[0].shortKey = CFSTR("i");
    __gPersonalitySubKeys[0].required = false;
 
    __gPersonalitySubKeys[1].longKey = CFSTR("IOClass");
    __gPersonalitySubKeys[1].shortKey = CFSTR("ioc");
    __gPersonalitySubKeys[1].required = false;
 
    __gPersonalitySubKeys[2].longKey = CFSTR("IONameMatch");
    __gPersonalitySubKeys[2].shortKey = CFSTR("ionm");
    __gPersonalitySubKeys[2].required = false;
 
    __gPersonalitySubKeys[3].longKey = CFSTR("IOProviderClass");
    __gPersonalitySubKeys[3].shortKey = CFSTR("iopc");
    __gPersonalitySubKeys[3].required = false;
 #endif 0

    didIt = true;
    return;
}

static Boolean __KXKextCacheEntry(
    KXKextRef aKext,
    CFDictionaryRef infoDict,
    CFMutableDictionaryRef cDict,
    unsigned subKeyIndex,
    Boolean makeSubstitutions)
{
    Boolean result = true;
    Boolean error = false;
    CFTypeRef infoDictValue = NULL;  // don't release
    CFTypeRef cDictEntry = NULL;  // must release

    __initSubKeys();

    infoDictValue = CFDictionaryGetValue(infoDict,
        __gInfoDictSubKeys[subKeyIndex].longKey);

    if (infoDictValue 
     && (__gInfoDictSubKeys[subKeyIndex].longKey == CFSTR("IOKitPersonalities")))
        infoDictValue = (aKext->flags.isValid ? kCFBooleanTrue : kCFBooleanFalse);

    if (!infoDictValue) {
        if (__gInfoDictSubKeys[subKeyIndex].required) {
            result = false;
        }
        goto finish;
    }
    cDictEntry = __KXKextCopyPListForCache(aKext, infoDictValue, &error);
    if (error) {
        result = false;
        goto finish;
    }
    if (cDictEntry) {
        if (makeSubstitutions) {
            CFDictionarySetValue(cDict,
                __gInfoDictSubKeys[subKeyIndex].shortKey, cDictEntry);
        } else {
            CFDictionarySetValue(cDict,
                __gInfoDictSubKeys[subKeyIndex].longKey, cDictEntry);
        }
        CFRelease(cDictEntry);
    }
finish:
    return result;
}


static Boolean __KXKextUncacheEntry(
    KXKextRef aKext __unused,
    CFDictionaryRef cDict,
    CFMutableDictionaryRef infoDict,
    unsigned subKeyIndex,
    Boolean makeSubstitutions)
{
    Boolean result = true;
    CFTypeRef infoDictValue = NULL;  // don't release

    __initSubKeys();

    if (makeSubstitutions) {
        infoDictValue = CFDictionaryGetValue(cDict,
            __gInfoDictSubKeys[subKeyIndex].shortKey);
    } else {
        infoDictValue = CFDictionaryGetValue(cDict,
            __gInfoDictSubKeys[subKeyIndex].longKey);
    }
    if (!infoDictValue) {
        if (__gInfoDictSubKeys[subKeyIndex].required) {
            result = false;
        }
        goto finish;
    }
    CFDictionarySetValue(infoDict,
        __gInfoDictSubKeys[subKeyIndex].longKey, infoDictValue);
finish:
    return result;
}


CFDictionaryRef __KXKextCopyInfoDictionaryFromCache(
    KXKextRef aKext,
    CFDictionaryRef cDict,
    Boolean makeSubstitutions)
{
    CFMutableDictionaryRef infoDict = NULL;  // returned
    Boolean error = false;
    unsigned int keyIndex;

    __initSubKeys();

    if (!cDict) {
        goto finish;
    }

    infoDict = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (!infoDict) {
        goto finish;
    }

    for (keyIndex = 0; keyIndex < NUM_INFO_DICT_SUB_KEYS; keyIndex++) {
        if (!__KXKextUncacheEntry(aKext, cDict, infoDict, keyIndex,
                makeSubstitutions)) {

            error = true;
            goto finish;
        }
    }

finish:
    if (error) {
        if (infoDict) CFRelease(infoDict);
        infoDict = NULL;
    }
    return infoDict;
}

CFDictionaryRef __KXKextCopyInfoDictionaryForCache(
    KXKextRef aKext,
    CFDictionaryRef infoDict,
    Boolean makeSubstitutions)
{
    CFMutableDictionaryRef cDict = NULL;  // returned
    Boolean error = false;
    unsigned int keyIndex;

    __initSubKeys();

    if (!infoDict) {
        goto finish;
    }

    cDict = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (!cDict) {
        goto finish;
    }

    for (keyIndex = 0; keyIndex < NUM_INFO_DICT_SUB_KEYS; keyIndex++) {
       if (!__KXKextCacheEntry(aKext, infoDict, cDict, keyIndex,
                makeSubstitutions)) {

            error = true;
            goto finish;
        }
    }

finish:
    if (error) {
        if (cDict) CFRelease(cDict);
        cDict = NULL;
    }
    return cDict;
}

/*******************************************************************************
*
*******************************************************************************/
CFTypeRef __KXKextCopyPListForCache(
    KXKextRef aKext,
    CFTypeRef pList,
    Boolean * error)
{
    CFTypeRef cList = NULL;  // returned
    CFTypeID typeID = 0;

    if (!pList) {
        goto finish;
    }

    if (error) {
        *error = false;
    }

    typeID = CFGetTypeID(pList);

    if (typeID == CFDictionaryGetTypeID()) {
        CFDictionaryRef dict = (CFDictionaryRef)pList;
        CFMutableDictionaryRef newDict = NULL;  // returned
        CFIndex count, i;
        CFStringRef * keys = NULL;   // must free
        CFStringRef * values = NULL; // must free
        count = CFDictionaryGetCount(dict);
        keys = (CFStringRef *)malloc(count * sizeof(CFStringRef));
        values = (CFStringRef *)malloc(count * sizeof(CFTypeRef));

        newDict = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
        if (!newDict) {
            if (error) {
                *error = true;
            }
            goto finish;
        }

        cList = newDict;

        CFDictionaryGetKeysAndValues(dict, (const void **)keys,
            (const void **)values);

        for (i = 0; i < count; i++) {
            CFTypeRef entry = __KXKextCopyPListForCache(aKext, values[i], error);
            if (error && *error) {
                CFRelease(newDict);
                cList = NULL;
                goto finish;
            }
            if (entry) {
                CFDictionarySetValue(newDict, keys[i], entry);
                CFRelease(entry);
            }
        }
        free(keys);
        free(values);
        goto finish;

    } else if (typeID == CFArrayGetTypeID()) {
        CFArrayRef array = (CFArrayRef)pList;
        CFMutableArrayRef newArray = NULL;  // returned
        CFIndex count, i;
        count = CFArrayGetCount(array);

        newArray = CFArrayCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeArrayCallBacks);
        if (!newArray) {
            if (error) {
                *error = true;
            }
            goto finish;
        }

        cList = newArray;

        for (i = 0; i < count; i++) {
            CFTypeRef entry = __KXKextCopyPListForCache(aKext,
                CFArrayGetValueAtIndex(array, i), error);
            if (error && *error) {
                CFRelease(newArray);
                cList = NULL;
                goto finish;
            }
            if (entry) {
                CFArrayAppendValue(newArray, entry);
                CFRelease(entry);
            }
        }

        goto finish;

    } else if (typeID == CFStringGetTypeID() ||
               typeID == CFDataGetTypeID() ||
               typeID == CFNumberGetTypeID() ||
               typeID == CFBooleanGetTypeID() ||
               typeID == CFDateGetTypeID()) {

        cList = pList;
        CFRetain(cList);   // caller will have to release
        goto finish;
    } else {
        cList = NULL;
        goto finish;
    }

finish:
    return cList;
}
#endif // !__LP64__

