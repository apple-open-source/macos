#if !__LP64__

#include <sys/cdefs.h>

#include <libc.h>
#include <CoreFoundation/CFRuntime.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitServer.h>
#include <IOKit/IOCFSerialize.h>
#include <pthread.h>
#include <mach/mach_error.h>
#include <mach/mach_port.h>
#include <zlib.h>
#include <errno.h>
#include <fts.h>

#include "KXKextManager.h"
#include "KXKextManager_private.h"
#include "KXKextRepository_private.h"

#include "load.h"
#include "path_util.h"
#include "printPList.h"
#include "vers_rsrc.h"

/*******************************************************************************
* The basic data structure for a kext.
*
* where CFBundleVersion appears in keys, it is a canonicalized string
* created by calling VERS_string().
*******************************************************************************/

typedef struct __KXKextManager {

    CFRuntimeBase  cfBase;   // base CFType information

    SInt32 logLevel;
    void (*log_func)(const char * format, ...) __attribute__((format(printf, 1, 2)));
    void (*error_log_func)(const char * format, ...) __attribute__((format(printf, 1, 2)));
    int  (*user_approve_func)(int defaultAnswer, const char * format, ...) __attribute__((format(printf, 2, 3)));
    int  (*user_veto_func)(int defaultAnswer, const char * format, ...) __attribute__((format(printf, 2, 3)));
    const char * (*user_input_func)(const char * format, ...) __attribute__((format(printf, 1, 2)));

    CFMutableArrayRef       repositoryList;
    
   /*****
    * This dictionary holds all KXKexts that are presumably loadable (valid,
    * eligible during safe boot if we're in that mode, enabled). The keys are 
    * the CFBundleIdentifiers; the values are linked lists of kexts going from
    * most recent to least recent versions, with duplicate versions hanging
    * off each entry. Authentication failures cause kexts to be removed
    * from this database.
    */
    CFMutableDictionaryRef  candidateKexts;

#if 0
    // This may be used if I implement sending personalities on-demand
    // down to the kernel, so that the kernel doesn't have to hold the
    // entire catalog.
    //
    CFMutableDictionaryRef  loadablePersonalities; // Keys are IOClassMatch
                                    //  names, values arrays of personalities.
#endif 0

    int              clearRelationshipsDisableCount;
    Boolean          needsClearRelationships;
    Boolean          needsCalculateRelationships;

    Boolean          performsFullTests;
    Boolean          performsStrictAuthentication;

    // If false, forks & execs kextload; else does link/load in-task.
    Boolean          performLoadsInTask;
    Boolean          safeBoot;

    // kextd will send all repositories to catalog
    Boolean          willUpdateCatalog;

    // Repositories hold kexts with validation/authentication failures
    // (in badKexts), but the manager holds the ones with missing dependencies.
    CFMutableArrayRef kextsWithMissingDependencies;

} __KXKextManager, * __KXKextManagerRef;

#define KEXT_PLUGIN_SUBPATH  (CFSTR(".kext/Contents/PlugIns"))

/*******************************************************************************
* Private function declarations. Definitions at bottom of this file.
*******************************************************************************/
static void __km_null_log(const char * format, ...);
static void __km_null_err_log(const char * format, ...);
static int __km_null_approve(int defaultAnswer, const char * format, ...);
static int __km_null_veto(int defaultAnswer, const char * format, ...);
static const char * __km_null_input(const char * format, ...);

static void __KXKextManagerInitialize(void);
static __KXKextManagerRef __KXKextManagerCreatePrivate(
    CFAllocatorRef       allocator,
    CFAllocatorContext * context);
static void __KXKextManagerReleaseContents(CFTypeRef aKextManager);
static CFStringRef __KXKextManagerCopyDebugDescription(CFTypeRef cf);

static void __KXKextManagerClearRelationships(
    KXKextManagerRef aKextManager);
void __KXKextManagerClearDependencyRelationships(
    KXKextManagerRef aKextManager);
static Boolean __versionNumberForString(
    CFStringRef aVersionString,
    VERS_version * version);
static KXKextRef __KXKextManagerGetKextWithIdentifierAndVersionNumber(
    KXKextManagerRef aKextManager,
    CFStringRef identifier,
    VERS_version * versionNumber);
static KXKextRef __KXKextManagerGetKextWithIdentifierCompatibleWithVersionNumber(
    KXKextManagerRef aKextManager,
    CFStringRef identifier,
    VERS_version * versionNumber);

static Boolean __KXKextManagerCheckForRepositoryCache(
    KXKextManagerRef aKextManager,
    CFURLRef aDirectory,
    CFURLRef * cacheURL, // out param
    Boolean * isCurrent,
    Boolean * canUpdate);

static int __KXKextManagerCheckPersonalityForSafeBoot(
    KXKextManagerRef aKextManager,
    CFDictionaryRef aPersonality,
    const char * personality_name);

extern KXKextManagerError __KXKextRealizeFromCache(KXKextRef aKext);


/*******************************************************************************
*
*******************************************************************************/
CFStringRef KXKextManagerErrorStringForError(KXKextManagerError error)
{
    if (error == kKXKextManagerErrorNone) {
        return CFSTR("no error");
    } else if (error == kKXKextManagerErrorUnspecified) {
        return CFSTR("unspecified error");
    } else if (error == kKXKextManagerErrorInvalidArgument) {
        return CFSTR("invalid argument");
    } else if (error == kKXKextManagerErrorNoMemory) {
        return CFSTR("no memory");
    } else if (error == kKXKextManagerErrorFileAccess) {
        return CFSTR("file access/permissions");
    } else if (error == kKXKextManagerErrorNotADirectory) {
        return CFSTR("not a directory");
    } else if (error == kKXKextManagerErrorDiskFull) {
        return CFSTR("disk full");
    } else if (error == kKXKextManagerErrorSerialization) {
        return CFSTR("serialization error");
    } else if (error == kKXKextManagerErrorCompression) {
        return CFSTR("compression error");
    } else if (error == kKXKextManagerErrorIPC) {
        return CFSTR("IPC error");
    } else if (error == kKXKextManagerErrorChildTask) {
        return CFSTR("forked task exited abnormally");
    } else if (error == kKXKextManagerErrorUserAbort) {
        return CFSTR("user canceled load");
    } else if (error == kKXKextManagerErrorKernelResource) {
        return CFSTR("kernel resources unavailable");
    } else if (error == kKXKextManagerErrorKernelPermission) {
        return CFSTR("kernel access denied");
    } else if (error == kKXKextManagerErrorKextNotFound) {
        return CFSTR("requested kernel extention could not be found");
    } else if (error == kKXKextManagerErrorURLNotInRepository) {
        return CFSTR("URL for extension does not lie within repository");
    } else if (error == kKXKextManagerErrorNotABundle) {
        return CFSTR("not a bundle");
    } else if (error == kKXKextManagerErrorNotAKext) {
        return CFSTR("not a kernel extension");
    } else if (error == kKXKextManagerErrorValidation) {
        return CFSTR("validation error");
    } else if (error == kKXKextManagerErrorBootLevel) {
        return CFSTR("not eligible for boot level (safe boot)");
    } else if (error == kKXKextManagerErrorDisabled) {
        return CFSTR("extension is disabled");
    } else if (error == kKXKextManagerErrorAuthentication) {
        return CFSTR("authentication error");
    } else if (error == kKXKextManagerErrorDependency) {
        return CFSTR("error resolving dependencies");
    } else if (error == kKXKextManagerErrorDependencyLoop) {
        return CFSTR("possible loop in dependencies");
    } else if (error == kKXKextManagerErrorCache) {
        return CFSTR("cached extension info dictionary doesn't match actual");
    } else if (error == kKXKextManagerErrorAlreadyLoaded) {
        return CFSTR("extension is already loaded");
    } else if (error == kKXKextManagerErrorLoadExecutableBad) {
        return CFSTR("bad executable");
    } else if (error == kKXKextManagerErrorLoadExecutableNoArch) {
        return CFSTR("exectuable doesn't contain code for this computer");
    } else if (error == kKXKextManagerErrorLinkLoad) {
        return CFSTR("link/load error");
    }
    return CFSTR("unknown error");
}

/*******************************************************************************
*
*******************************************************************************/
static char static_error_buffer[100];

const char * KXKextManagerErrorStaticCStringForError(KXKextManagerError error)
{
    char * return_string = static_error_buffer;
    CFStringRef errorString = NULL;  // don't release

    errorString = KXKextManagerErrorStringForError(error);
    if (!CFStringGetCString(errorString, static_error_buffer,
        sizeof(static_error_buffer) - 1, kCFStringEncodingUTF8)) {

        return "(string conversion failure)";
    }
    return return_string;
}

/******************************************************************************
* cStringForCFString()
******************************************************************************/
static char * cStringForCFString(CFStringRef aString)
{
    char * result;  // returned
    CFIndex length;
    int error = 0;

    length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(aString),
        kCFStringEncodingMacRoman) + sizeof('\0');
    result = (char *)malloc(length * sizeof(char));
    if (!result) {
         error = 1;
         goto finish;
     }
    
    if (!CFStringGetCString(aString, result, length,
        kCFStringEncodingMacRoman)) {

        error = 1;
         goto finish;
    }

finish:
    if (error && result) free(result);
    return result;
}

/*******************************************************************************
* CF CLASS DEF'N STUFF
*******************************************************************************/
static CFTypeID __kKXKextManagerTypeID = _kCFRuntimeNotATypeID;

CFTypeID KXKextManagerGetTypeID(void) {
    return __kKXKextManagerTypeID;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerRef KXKextManagerCreate(CFAllocatorRef alloc)
{
    __KXKextManagerRef newManager = NULL;

    newManager = __KXKextManagerCreatePrivate(alloc, NULL);
    if (!newManager) {
        goto finish;
    }

    newManager->logLevel = kKXKextManagerLogLevelDefault; // basic logging
    newManager->log_func = &__km_null_log;
    newManager->error_log_func = &__km_null_err_log;
    newManager->user_approve_func = &__km_null_approve;
    newManager->user_veto_func = &__km_null_veto;
    newManager->user_input_func = &__km_null_input;

finish:
    return (KXKextManagerRef)newManager;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError KXKextManagerInit(
    KXKextManagerRef aKextManager,
    Boolean loadInTaskFlag,
    Boolean safeBootFlag)
{
    KXKextManagerError result = kKXKextManagerErrorNone;

    aKextManager->repositoryList = CFArrayCreateMutable(
        kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!aKextManager->repositoryList) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    aKextManager->candidateKexts = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (!aKextManager->candidateKexts) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

#if 0
    aKextManager->loadablePersonalities = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (!aKextManager->loadablePersonalities) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }
#endif 0

    aKextManager->kextsWithMissingDependencies = CFArrayCreateMutable(
        kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    if (!aKextManager->kextsWithMissingDependencies) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    aKextManager->clearRelationshipsDisableCount = 0;
    aKextManager->needsClearRelationships = false;
    aKextManager->needsCalculateRelationships = true;

    aKextManager->performsFullTests = false;
    aKextManager->performsStrictAuthentication = false;

    aKextManager->performLoadsInTask = loadInTaskFlag;
    aKextManager->safeBoot = safeBootFlag;

finish:
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextManagerSetLogLevel(
    KXKextManagerRef aKextManager,
    SInt32 level)
{
    aKextManager->logLevel = level;
    kload_set_log_level(level);
    return;
}

/*******************************************************************************
*
*******************************************************************************/
SInt32 KXKextManagerGetLogLevel(
    KXKextManagerRef aKextManager)
{
    return aKextManager->logLevel;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextManagerSetLogFunction(
    KXKextManagerRef aKextManager,
    void (*func)(const char * format, ...))
{
    if (!func) {
        aKextManager->log_func = &__km_null_log;
    } else {
        aKextManager->log_func = func;
    }

   /* Also set this as the load library's log function.
    */
    kload_set_log_function(func);

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextManagerSetErrorLogFunction(
    KXKextManagerRef aKextManager,
    void (*func)(const char * format, ...))
{
    if (!func) {
        aKextManager->error_log_func = &__km_null_err_log;
    } else {
        aKextManager->error_log_func = func;
    }

   /* Also set this as the load library's log function.
    */
    kload_set_error_log_function(func);

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextManagerSetUserApproveFunction(
    KXKextManagerRef aKextManager,
    int (*func)(int defaultAnswer, const char * format, ...))
{
    if (!func) {
        aKextManager->user_approve_func = &__km_null_approve;
    } else {
        aKextManager->user_approve_func = func;
    }

   /* Also set this as the load library's log function.
    */
    kload_set_user_approve_function(func);

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextManagerSetUserVetoFunction(
    KXKextManagerRef aKextManager,
    int (*func)(int defaultAnswer, const char * format, ...))
{
    if (!func) {
        aKextManager->user_veto_func = &__km_null_veto;
    } else {
        aKextManager->user_veto_func = func;
    }

   /* Also set this as the load library's log function.
    */
    kload_set_user_veto_function(func);

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextManagerSetUserInputFunction(
    KXKextManagerRef aKextManager,
    const char * (*func)(const char * format, ...))
{
    if (!func) {
        aKextManager->user_input_func = &__km_null_input;
    } else {
        aKextManager->user_input_func = func;
    }

   /* Also set this as the load library's log function.
    */
    kload_set_user_input_function(func);

    return;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean KXKextManagerGetSafeBootMode(KXKextManagerRef aKextManager)
{
    return aKextManager->safeBoot;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextManagerSetSafeBootMode(KXKextManagerRef aKextManager,
    Boolean flag)
{
    aKextManager->safeBoot = flag;
    return;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean KXKextManagerGetPerformLoadsInTask(KXKextManagerRef aKextManager)
{
    return aKextManager->performLoadsInTask;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextManagerSetPerformLoadsInTask(KXKextManagerRef aKextManager,
    Boolean flag)
{
     aKextManager->performLoadsInTask = flag;
    return;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean KXKextManagerPerformsFullTests(KXKextManagerRef aKextManager)
{
    return aKextManager->performsFullTests;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextManagerSetPerformsFullTests(KXKextManagerRef aKextManager,
    Boolean flag)
{
    aKextManager->performsFullTests = flag;
    return;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError KXKextManagerAddRepositoryDirectory(
    KXKextManagerRef aKextManager,
    CFURLRef directoryURL,
    Boolean scanForKexts,
    Boolean useCache,
    KXKextRepositoryRef * theRepository)
{
    KXKextManagerError result = kKXKextManagerErrorNone;

    KXKextRepositoryRef foundRepository = NULL;  // don't release
    KXKextRepositoryRef newRepository = NULL;  // must release
    char * url_path = NULL;         // must free

    CFURLRef nonDirURL = NULL; // must release
    CFURLRef cacheURL = NULL;  // must release

    Boolean createNewCache = false;

    if (theRepository) {
        *theRepository = NULL;
    }

    if (KXKextManagerPerformsFullTests(aKextManager)) {
	useCache = FALSE;
    }

    url_path = PATH_CanonicalizedCStringForURL(directoryURL);

    _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelDetails, NULL, 0,
        "adding repository %s", url_path);

   /*****
    * First let's see if a repository has already been registered
    * under the given URL. If one has we can just return that.
    */
    foundRepository = KXKextManagerGetRepositoryForDirectory(
        aKextManager, directoryURL);
    if (foundRepository) {
        if (theRepository) {
            *theRepository = foundRepository;
        }

        _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelDetails, NULL, 0,
            "repository %s exists", url_path);

        if (!KXKextRepositoryGetScansForKexts(foundRepository) &&
            scanForKexts) {

            KXKextRepositorySetScansForKexts(foundRepository, scanForKexts);
        }
        goto finish;
    }

    if (useCache) {
        Boolean isCurrent = false;
        Boolean canUpdate = false;

        if (__KXKextManagerCheckForRepositoryCache(aKextManager,
                directoryURL, &cacheURL, &isCurrent, &canUpdate)) {

            if (isCurrent) {

                result = _KXKextManagerAddRepositoryFromCacheFile(
                    aKextManager, cacheURL, directoryURL, theRepository);
                if (result == kKXKextManagerErrorNone) {
                    goto finish;
                } else {
                    _KXKextManagerLogError(aKextManager,
                        "error using cache for %s; using repository itself",
                        url_path);
                    createNewCache = true;
                    // fall through to load from filesystem
                }
            } else if (canUpdate) {
                createNewCache = true;
                // fall through to load from filesystem
            }
        } else if (canUpdate) {
            createNewCache = true;
            // fall through to load from filesystem
        }
    }

   /*****
    * We didn't find an existing repository with the given URL,
    * so we have to create it.
    */
    newRepository = _KXKextRepositoryCreate(kCFAllocatorDefault);
    if (!newRepository) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    result = _KXKextRepositoryInitWithDirectory(newRepository, directoryURL,
        scanForKexts, aKextManager);
    if (result != kKXKextManagerErrorNone) {
        goto finish;
    }

    _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelDetails, NULL, 0,
        "added repository %s", url_path);

    CFArrayAppendValue(aKextManager->repositoryList, newRepository);
    KXKextManagerClearRelationships(aKextManager);

    if (theRepository) {
        *theRepository = newRepository;
    }

finish:
    if (result != kKXKextManagerErrorNone) {
        _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelDetails, NULL, 0,
        "failed to add repository %s", url_path);
    }

    if (createNewCache) {
        if (foundRepository) {
            KXKextRepositoryWriteCache(foundRepository, NULL);
        }
        if (newRepository) {
            KXKextRepositoryWriteCache(newRepository, NULL);
        }
    }

    if (newRepository) CFRelease(newRepository); // self has a ref
    if (url_path)      free(url_path);
    if (nonDirURL)     CFRelease(nonDirURL);
    if (cacheURL)      CFRelease(cacheURL);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextManagerRemoveRepositoryDirectory(
    KXKextManagerRef aKextManager,
    CFURLRef directoryURL)
{
    KXKextRepositoryRef foundRepository = NULL;  // don't release
    char * url_path = NULL;         // must free

    if (_KXKextManagerCheckLogLevel(aKextManager, kKXKextManagerLogLevelDetails,
        NULL, 0)) {
        url_path = PATH_CanonicalizedCStringForURL(directoryURL);
        _KXKextManagerLogMessage(aKextManager,
            "request to remove repository %s", url_path);
    }

    foundRepository = KXKextManagerGetRepositoryForDirectory(
        aKextManager, directoryURL);
    if (foundRepository) {
        _KXKextManagerRemoveRepository(aKextManager, foundRepository);
    }

    if (url_path) free(url_path);

    return;
}

/*******************************************************************************
*
*******************************************************************************/
CFArrayRef KXKextManagerGetRepositories(KXKextManagerRef aKextManager)
{
    return aKextManager->repositoryList;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextRepositoryRef KXKextManagerGetRepositoryForDirectory(
    KXKextManagerRef aKextManager,
    CFURLRef aDirectory)
{
    CFURLRef    canonicalURL = NULL;    // must release
    CFStringRef directoryString = NULL; // must release
    KXKextRepositoryRef theRepository = NULL;  // returned; don't release
    CFURLRef checkURL = NULL;           // must release

    canonicalURL = PATH_CopyCanonicalizedURL(aDirectory);
    if (!canonicalURL) {
        goto finish;
    }

    if (!aKextManager->repositoryList) {
        goto finish;
    } else {
        CFIndex count, i;

        count = CFArrayGetCount(aKextManager->repositoryList);
        for (i = 0; i < count; i++) {

            KXKextRepositoryRef checkRepository = NULL; // don't release

            checkRepository = (KXKextRepositoryRef)CFArrayGetValueAtIndex(
                    aKextManager->repositoryList, i);
            checkURL = KXKextRepositoryCopyURL(checkRepository);
            if (!checkURL) {
                goto finish;  // we'll see a fatal no-memory error soon enough
            }

            if (CFEqual(canonicalURL, checkURL)) {
                theRepository = checkRepository;
                goto finish;
            }

            CFRelease(checkURL);
            checkURL = NULL;
        }
    }

finish:

    if (canonicalURL)    CFRelease(canonicalURL);
    if (directoryString) CFRelease(directoryString);
    if (checkURL)        CFRelease(checkURL);

    return theRepository;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextManagerResetAllRepositories(KXKextManagerRef aKextManager)
{
    CFIndex count, i;

    _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelDetails, NULL, 0,
        "resetting all repositories");

   /* Ensure all references to CFBundles are dropped *before* resetting,
    * or CFBundleCreate won't bother to read from disk.
    */
    __KXKextManagerClearRelationships(aKextManager);

    KXKextManagerDisableClearRelationships(aKextManager);

    count = CFArrayGetCount(aKextManager->repositoryList);
    for (i = 0; i < count; i++) {
        KXKextRepositoryRef thisRepository = (KXKextRepositoryRef)
            CFArrayGetValueAtIndex(aKextManager->repositoryList, i);
        KXKextRepositoryReset(thisRepository);
    }

    KXKextManagerEnableClearRelationships(aKextManager);

    return;
}

/*******************************************************************************
*
*******************************************************************************/

#define kSystemKextPath 	"/System/Library/Extensions/System.kext"

KXKextManagerError KXKextManagerAddKextWithURL(
    KXKextManagerRef aKextManager,
    CFURLRef kextURL,
    Boolean includePlugins,
    KXKextRef * theKext)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    KXKextRepositoryRef repository = NULL;  // don't release
    KXKextRef foundKext = NULL;  // don't release
    KXKextRef newKext = NULL;    // must release
    CFURLRef localURL = NULL;
    char * url_path = NULL;      // must free
    char kext_url[MAXPATHLEN] = "(unknown)";
    Boolean error = false;

    url_path = PATH_CanonicalizedCStringForURL(kextURL);

    // add the whole system.kext for compatibility
    if (url_path && !strncmp(url_path, kSystemKextPath, strlen(kSystemKextPath)))
    {
	free(url_path);
	url_path = NULL;
	localURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, CFSTR(kSystemKextPath),
						    kCFURLPOSIXPathStyle, true);
	if (!localURL) {
            error = true;
            result = kKXKextManagerErrorUnspecified;
            goto finish;
	}
	kextURL = localURL;
	url_path = PATH_CanonicalizedCStringForURL(kextURL);
    }

    if (!url_path) {
        CFStringGetCString(CFURLGetString(kextURL),
            kext_url, sizeof(kext_url) - 1, kCFStringEncodingUTF8);

        _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelDetails, NULL, 0,
            "can't determine absolute pathname for kext  %s (%s)",
            kext_url, KXKextManagerErrorStaticCStringForError(result));
        error = true;
        result = kKXKextManagerErrorUnspecified;
        goto finish;
    }

 
   _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelDetails, NULL, 0,
        "request to add kernel extension %s", url_path);

    foundKext = KXKextManagerGetKextWithURL(aKextManager, kextURL);
    if (foundKext) {
        _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelDetails, NULL, 0,
            "kernel extension %s exists", url_path);

        if (theKext) {
            *theKext = foundKext;
            goto finish;
        }
    } else {
        newKext = _KXKextCreate(kCFAllocatorDefault);
        if (!newKext) {
            error = true;
            result = kKXKextManagerErrorNoMemory;
            goto finish;
        }

        if (theKext) {
            *theKext = newKext;
        }

        result = _KXKextInitWithURLInManager(newKext, kextURL, aKextManager);
        if (result == kKXKextManagerErrorFileAccess ||
            result == kKXKextManagerErrorNotADirectory ||
            result == kKXKextManagerErrorKextNotFound ||
            result == kKXKextManagerErrorURLNotInRepository ||
            result == kKXKextManagerErrorNotABundle ||
            result == kKXKextManagerErrorNotAKext) {

            // kext is completely unusable, do not store in repository
            if (theKext) {
                *theKext = NULL;
            }
            error = true;
            goto finish;
        } else if (result != kKXKextManagerErrorNone) {
            repository = KXKextGetRepository(newKext);
            if (repository) {
                _KXKextRepositoryAddBadKext(repository, newKext);
            }
            error = true;
            goto finish;
        }
        repository = KXKextGetRepository(newKext);
        if (repository) {
            _KXKextRepositoryAddKext(repository, newKext);
        }

        if (includePlugins) {
            KXKextManagerError scanResult = kKXKextManagerErrorNone;
            CFArrayRef addedPlugins = NULL;
            CFArrayRef badKextPlugins = NULL;

            scanResult = _KXKextScanPlugins(newKext, &addedPlugins,
                &badKextPlugins, NULL);
            if (scanResult == kKXKextManagerErrorNone) {
                if (repository) {
                    _KXKextRepositoryAddKexts(repository, addedPlugins);
                    _KXKextRepositoryAddBadKexts(repository, badKextPlugins);
                }
            }

            if (addedPlugins) {
                CFRelease(addedPlugins);
                addedPlugins = NULL;
            }
            if (badKextPlugins) {
                CFRelease(badKextPlugins);
                badKextPlugins = NULL;
            }

        }

        _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelDetails, NULL, 0,
            "added kernel extension %s%s", url_path,
            KXKextGetPlugins(newKext) ? " (and plugins)" : "");

    }

finish:
    if (error) {
        char * kext_name = "(unknown)";

        if (url_path) {
            kext_name = url_path;
        } else if (kext_url) {
            kext_name = kext_url;
        }
        _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelDetails, NULL, 0,
            "failed to add kernel extension %s (%s)",
            kext_name, KXKextManagerErrorStaticCStringForError(result));
    }

    if (url_path) free(url_path);
    if (newKext)  CFRelease(newKext);
    if (localURL) CFRelease(localURL);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextManagerRemoveKext(
    KXKextManagerRef aKextManager __unused,
    KXKextRef aKext)
{
    KXKextRepositoryRef repository = KXKextGetRepository(aKext);
    if (repository) {
        _KXKextRepositoryRemoveKext(repository, aKext);
    }
    return;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError KXKextManagerRescanKext(
    KXKextManagerRef aKextManager,
    KXKextRef aKext,
    Boolean scanForPlugins,
    KXKextRef * rescannedKext)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    CFURLRef kextURL = NULL;  // must release
    KXKextRef newKext = NULL; // must release (is retained by repository)

    if (rescannedKext) {
        *rescannedKext = NULL;
    }

    kextURL = KXKextGetAbsoluteURL(aKext);
    if (!kextURL) {
        result = kKXKextManagerErrorUnspecified;
        goto finish;
    }

    KXKextManagerRemoveKext(aKextManager, aKext);

    result = KXKextManagerAddKextWithURL(aKextManager, kextURL, scanForPlugins, &newKext);

finish:
    if (rescannedKext && newKext) {
        *rescannedKext = newKext;
    }
    if (newKext) CFRelease(newKext);
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextManagerDisqualifyKext(KXKextManagerRef aKextManager __unused,
    KXKextRef aKext)
{
    KXKextRepositoryRef repository = KXKextGetRepository(aKext);
    if (repository) {
        _KXKextRepositoryDisqualifyKext(repository, aKext);
    }
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextManagerRequalifyKext(KXKextManagerRef aKextManager __unused,
    KXKextRef aKext)
{
    KXKextRepositoryRef repository = KXKextGetRepository(aKext);
    if (repository) {
    _KXKextRepositoryRequalifyKext(repository, aKext);
    }
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextManagerCalculateVersionRelationships(KXKextManagerRef aKextManager)
{
    CFIndex numRepositories, i;
    CFArrayRef candidateKexts = NULL;

    _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelDetails, NULL, 0,
        "calculating version relationships");

   /* Do this now whether it's enabled or not.
    */
    __KXKextManagerClearRelationships(aKextManager);
    __KXKextManagerClearDependencyRelationships(aKextManager);

    numRepositories = CFArrayGetCount(aKextManager->repositoryList);

   /*****
    * Go through every repository and pull together all the kexts
    * that are potentially loadable, building version chains where
    * duplicates exist for the same ID.
    */
    for (i = 0; i < numRepositories; i++) {
        CFIndex numKexts, k;
        KXKextRepositoryRef thisRepository =
           (KXKextRepositoryRef)CFArrayGetValueAtIndex(
           aKextManager->repositoryList, i);

        candidateKexts = _KXKextRepositoryGetCandidateKexts(thisRepository);
        numKexts = CFArrayGetCount(candidateKexts);

        for (k = 0; k < numKexts; k++) {
            KXKextRef thisKext =
                (KXKextRef)CFArrayGetValueAtIndex(candidateKexts, k);
            CFStringRef thisKextID = KXKextGetBundleIdentifier(thisKext);
            KXKextRef alreadyKext = NULL;

            // skip kexts known to be unloadable
            if (!KXKextIsValid(thisKext)) {
                continue;
            }
            if (KXKextGetLoadFailed(thisKext)) {
                continue;
            }
            if (aKextManager->safeBoot &&
                !KXKextIsEligibleDuringSafeBoot(thisKext)) {
                continue;
            }
            if (!KXKextIsEnabled(thisKext)) {
                continue;
            }

            // store by id, build version chains
            alreadyKext = (KXKextRef)CFDictionaryGetValue(
                aKextManager->candidateKexts, thisKextID);
            if (!alreadyKext) {
                CFDictionarySetValue(aKextManager->candidateKexts,
                    thisKextID, thisKext);
            } else if (_KXKextGetVersion(thisKext) >
                _KXKextGetVersion(alreadyKext)) {

                _KXKextSetPriorVersionKext(thisKext, alreadyKext);
                CFDictionarySetValue(aKextManager->candidateKexts,
                    thisKextID, thisKext);
            } else {
                _KXKextAddPriorOrDuplicateVersionKext(alreadyKext, thisKext);
            }
        }
    }

    // FIXME: Need to record array of loadable personalities too.
    
    aKextManager->needsCalculateRelationships = false;

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextManagerResolveAllKextDependencies(KXKextManagerRef aKextManager)
{
    CFIndex numKexts, i;
    CFStringRef * ids = NULL;  // must free
    KXKextRef * kexts = NULL;  // must free

    _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelDetails, NULL, 0,
        "resolving dependencies for all kernel extensions");

    numKexts = CFDictionaryGetCount(aKextManager->candidateKexts);
    if (numKexts) {
        ids = (CFStringRef *)malloc(numKexts * sizeof(CFStringRef));
        kexts = (KXKextRef *)malloc(numKexts * sizeof(KXKextRef));
        if (!ids || !kexts) {
            goto finish;
        }

        CFDictionaryGetKeysAndValues(aKextManager->candidateKexts,
            (const void **)ids, (const void **)kexts);

       /*****
        * Go through the two-way tree for prior & duplicate versions
        * of each kext, having them resolve their dependencies.
        */
        for (i = 0; i < numKexts; i++) {
            KXKextRef thisKext = kexts[i];
            KXKextRef dupKext = NULL;

            while (thisKext) {
                KXKextResolveDependencies(thisKext);

                dupKext = KXKextGetDuplicateVersionKext(thisKext);
                while (dupKext) {
                    KXKextResolveDependencies(dupKext);
                    dupKext = KXKextGetDuplicateVersionKext(dupKext);
                }

                thisKext = KXKextGetPriorVersionKext(thisKext);
            }
        }

       /*****
        * Now go through the tree and remove any that don't have their
        * dependencies.
        */
        for (i = 0; i < numKexts; i++) {
            KXKextRef thisKext = kexts[i];
            KXKextRef nextKext = NULL;
            KXKextRef prevKext = NULL;

            while (thisKext) {

                KXKextRef dupKext = NULL;

                nextKext = KXKextGetPriorVersionKext(thisKext);
                dupKext = KXKextGetDuplicateVersionKext(thisKext);

               /*****
                * First check the primary entry for each version. If there
                * is a duplicate version of the primary, relink the chains
                * as needed.
                */
                if (!KXKextGetHasAllDependencies(thisKext)) {

                    if (_KXKextManagerCheckLogLevel(aKextManager, kKXKextManagerLogLevelKexts,
                        NULL, 0)) {

                        const char * kext_name =
                            _KXKextCopyCanonicalPathnameAsCString(thisKext);
                        if (kext_name) {
                            _KXKextManagerLogMessage(aKextManager,
                                "extension %s is missing dependencies", kext_name);
                            free((char *)kext_name);
                        }
                    }

                    CFArrayAppendValue(aKextManager->kextsWithMissingDependencies,
                        thisKext);

                    if (!dupKext) {

                       /* No duplicate version, so this is a straightforward
                        * linked-list manipulation. Link the previous kext
                        * checked (which has a *later* version) to the next
                        * kext. If there is no previous kext, just replace the
                        * entry in the dictionary with the next one to be checked
                        * if there is one, otherwise just remove it.
                        */
                        if (prevKext) {
                            _KXKextSetPriorVersionKext(prevKext, nextKext);
                        } else if (nextKext) {
                            CFDictionarySetValue(aKextManager->candidateKexts,
                                ids[i], nextKext);
                        } else {
                            CFDictionaryRemoveValue(aKextManager->candidateKexts,
                                ids[i]);
                        }
                        thisKext = nextKext;
                        continue;
                    } else {

                       /* With a duplicate version, things are trickier.
                        * If there's a previous kext, relink the duplicate
                        * into the chain. If there's no previous kext,
                        * just replace the entry in the dictionary with the
                        * duplicate. In either case prepare to check the duplicate
                        * kext, which is now the root of the version tree, as the
                        * next one (the whole duplicate chain is checked below).
                        */
                        if (prevKext) {
                            _KXKextSetPriorVersionKext(prevKext, dupKext);
                            _KXKextSetPriorVersionKext(dupKext, nextKext);
                        } else {
                            CFDictionarySetValue(aKextManager->candidateKexts,
                                ids[i], dupKext);
                            _KXKextSetPriorVersionKext(dupKext, nextKext);
                        }
                        thisKext = dupKext;
                        continue;
                    }
                } else {

                   /*****
                    * The first kext in a possible chain of duplicate versions
                    * passed the test. Now check all of its duplicates by peeking
                    * ahead and checking that one. If it fails, link the dupKext
                    * to the duplicate past the peeked kext (if any).
                    */
                    if (dupKext) {
                        KXKextRef peekDupKext =
                            KXKextGetDuplicateVersionKext(dupKext);

                        while (peekDupKext) {
                            if (!KXKextGetHasAllDependencies(peekDupKext)) {

                                if (_KXKextManagerCheckLogLevel(aKextManager, kKXKextManagerLogLevelKexts,
                                    NULL, 0)) {

                                    const char * kext_name =
                                        _KXKextCopyCanonicalPathnameAsCString(
                                            thisKext);
                                    if (kext_name) {
                                        _KXKextManagerLogMessage(aKextManager,
                                            "extension %s is missing dependencies",
                                            kext_name);
                                        free((char *)kext_name);
                                    }
                                }

                                CFArrayAppendValue(
                                    aKextManager->kextsWithMissingDependencies,
                                    peekDupKext);

                                _KXKextSetDuplicateVersionKext(dupKext,
                                    KXKextGetDuplicateVersionKext(peekDupKext));
                            } else {
                                dupKext = peekDupKext;
                            }
                            peekDupKext =
                                KXKextGetDuplicateVersionKext(peekDupKext);
                        }
                    }

                   /* After all that, prepare to check the next kext in the
                    * chain of prior versions.
                    */
                    prevKext = thisKext;
                    thisKext = nextKext;
                }
            }
        }
    }

    if (aKextManager->performsFullTests) {
        CFIndex numRepositories = CFArrayGetCount(aKextManager->repositoryList);

        _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelBasic, NULL, 0,
                "resolving dependencies for kernel extensions with validation"
                " and authentication failures");

        for (i = 0; i < numRepositories; i++) {
            KXKextRepositoryRef thisRepository =
                (KXKextRepositoryRef)CFArrayGetValueAtIndex(
                aKextManager->repositoryList, i);
            KXKextRepositoryResolveBadKextDependencies(thisRepository);
        }
    }

finish:
    if (ids)   free(ids);
    if (kexts) free(kexts);
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextManagerClearRelationships(KXKextManagerRef aKextManager)
{
    CFIndex count, i;

    if (aKextManager->clearRelationshipsDisableCount > 0) {
        _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelDetails, NULL, 0,
                    "request to clear relationships while disabled; pending");

        aKextManager->needsClearRelationships = true;
        aKextManager->needsCalculateRelationships = true;
        return;
    }

    _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelDetails, NULL, 0,
            "clearing all version/dependency relationships "
            "among kernel extensions");

    __KXKextManagerClearRelationships(aKextManager);

    // Have each repository clear its kexts' relationships
    count = CFArrayGetCount(aKextManager->repositoryList);
    for (i = 0; i < count; i++) {
        KXKextRepositoryRef thisRepository = (KXKextRepositoryRef)
            CFArrayGetValueAtIndex(aKextManager->repositoryList, i);
        _KXKextRepositoryClearRelationships(thisRepository);
    }
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextManagerEnableClearRelationships(KXKextManagerRef aKextManager)
{
    if (aKextManager->clearRelationshipsDisableCount > 0) {
        aKextManager->clearRelationshipsDisableCount--;
    }
    if (aKextManager->needsClearRelationships) {
        KXKextManagerClearRelationships(aKextManager);
    }

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextManagerDisableClearRelationships(KXKextManagerRef aKextManager)
{
    aKextManager->clearRelationshipsDisableCount++;
    return;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextRepositoryRef KXKextManagerGetRepositoryWithURL(
    KXKextManagerRef aKextManager,
    CFURLRef anURL)
{
    CFURLRef    canonicalURL = NULL;    // must release
    CFStringRef directoryString = NULL; // must release
    KXKextRepositoryRef foundRepository = NULL;
    CFURLRef checkURL = NULL;           // must release

    canonicalURL = PATH_CopyCanonicalizedURL(anURL);
    if (!canonicalURL) {
        goto finish;
    }

    directoryString = CFURLCopyFileSystemPath(canonicalURL,
        kCFURLPOSIXPathStyle);
    if (!directoryString || !aKextManager->repositoryList) {
        goto finish;
    } else {
        CFIndex count, i;

        count = CFArrayGetCount(aKextManager->repositoryList);

        for (i = 0; i < count; i++) {

            KXKextRepositoryRef checkRepository = NULL; // don't release

            checkRepository = (KXKextRepositoryRef)CFArrayGetValueAtIndex(
                    aKextManager->repositoryList, i);
            checkURL = KXKextRepositoryCopyURL(checkRepository);
            if (!checkURL) {
                goto finish;  // we'll see a fatal no-memory error soon enough
            }

            if (CFEqual(anURL, checkURL)) {
                foundRepository = checkRepository;
                goto finish;
            }

            CFRelease(checkURL);
            checkURL = NULL;
        }
    }

finish:

    if (canonicalURL)    CFRelease(canonicalURL);
    if (directoryString) CFRelease(directoryString);
    if (checkURL)        CFRelease(checkURL);
    return foundRepository;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextRepositoryRef KXKextManagerGetRepositoryForKextWithURL(
    KXKextManagerRef aKextManager,
    CFURLRef anURL)
{
    CFURLRef absURL = NULL;  // must release
    CFURLRef scratchURL = NULL;  // must release
    CFStringRef scratchString = NULL; // must release
    CFRange scratchRange;

    KXKextRepositoryRef foundRepository = NULL;

    scratchString = CFURLCopyPathExtension(anURL);
    if (!scratchString ||
        kCFCompareEqualTo != CFStringCompare(scratchString, CFSTR("kext"), 0)) {
        goto finish;
    }
    CFRelease(scratchString);
    scratchString = NULL;

   /*****
    * Hmm, we don't have a kext registered under the given URL.
    * Maybe we have a repository where it should be, though?
    */

    absURL = PATH_CopyCanonicalizedURL(anURL);
    if (!absURL) {
        goto finish;
    }

    // drop the kext name at the end of the path to get the repository
    scratchURL = CFURLCreateCopyDeletingLastPathComponent(kCFAllocatorDefault,
        absURL);
    if (!scratchURL) {
        goto finish;
    }
    CFRelease(absURL);
    absURL = scratchURL;
    scratchURL = NULL;

    foundRepository = KXKextManagerGetRepositoryWithURL(aKextManager, absURL);
    if (foundRepository) {
        goto finish;
    }

   /*****
    * If we couldn't find a repository for an unregistered kext, let's get
    * funky and see if we can find a repository where the requested kext
    * would be a plugin of some other kext. This assumes knowledge of
    * bundle directory structure, which isn't so cool, but that's what we
    * have to do....
    */

    scratchString = CFURLCopyFileSystemPath(absURL, kCFURLPOSIXPathStyle);
    if (!scratchString) {
        goto finish;
    }

    if (CFStringHasSuffix(scratchString, KEXT_PLUGIN_SUBPATH)) {

        CFStringRef scratchString2;  // reassigned; do not release
        CFRange foundRange;

        scratchRange = CFRangeMake(0,
            CFStringGetLength(scratchString) -
              CFStringGetLength(KEXT_PLUGIN_SUBPATH));

       /* Looking for ...... v that slash right there!
        * "/some/long/path/to/TheBundle.kext/Contents/PlugIns"
        */
        if (!CFStringFindWithOptions(scratchString, CFSTR("/"),
            scratchRange, kCFCompareBackwards, &foundRange)) {

            goto finish;
        }

        scratchRange = CFRangeMake(0, foundRange.location);

        scratchString2 = CFStringCreateWithSubstring(
            kCFAllocatorDefault, scratchString, scratchRange);
        if (!scratchString2) {
            goto finish;
        }
        CFRelease(scratchString);
        scratchString = scratchString2;
    }

    if (absURL) {
        CFRelease(absURL);
        absURL = NULL;
    }
    scratchURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, scratchString,
        kCFURLPOSIXPathStyle, true);
    if (!scratchURL) {
        goto finish;
    }
    absURL = PATH_CopyCanonicalizedURL(scratchURL);
    if (!absURL) {
        goto finish;
    }

    foundRepository = KXKextManagerGetRepositoryWithURL(aKextManager, absURL);
    if (foundRepository) {
        goto finish;
    }


finish:

    if (absURL)        CFRelease(absURL);
    if (scratchURL)    CFRelease(scratchURL);
    if (scratchString) CFRelease(scratchString);
    return foundRepository;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextRef KXKextManagerGetKextWithURL(
    KXKextManagerRef aKextManager,
    CFURLRef anURL)
{
    CFIndex count, i;
    KXKextRef foundKext = NULL;   // don't release

    count = CFArrayGetCount(aKextManager->repositoryList);
    for (i = 0; i < count; i++) {
        KXKextRepositoryRef thisRepository = (KXKextRepositoryRef)
            CFArrayGetValueAtIndex(aKextManager->repositoryList, i);
        foundKext = KXKextRepositoryGetKextWithURL(thisRepository,
            anURL);
        if (foundKext) {
            goto finish;
        }
    }

finish:

    return foundKext;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextRef KXKextManagerGetKextWithIdentifier(KXKextManagerRef aKextManager,
    CFStringRef identifier)
{
    return __KXKextManagerGetKextWithIdentifierAndVersionNumber(
        aKextManager, identifier, NULL);
}

/*******************************************************************************
*
*******************************************************************************/
KXKextRef KXKextManagerGetKextWithIdentifierAndVersionString(
    KXKextManagerRef aKextManager,
    CFStringRef identifier,
    CFStringRef versionString)
{
    VERS_version version;

    if (versionString && !__versionNumberForString(versionString, &version)) {
        return NULL;
    }

    return __KXKextManagerGetKextWithIdentifierAndVersionNumber(
        aKextManager, identifier, &version);
}

/*******************************************************************************
*
*******************************************************************************/
KXKextRef KXKextManagerGetKextWithIdentifierCompatibleWithVersionString(
    KXKextManagerRef aKextManager,
    CFStringRef identifier,
    CFStringRef versionString)
{
    VERS_version version;
    VERS_version * versionPtr = NULL;

    if (versionString) {
        if (!__versionNumberForString(versionString, &version)) {
            return NULL;
        } else {
          versionPtr = &version;
        }
    }

    return __KXKextManagerGetKextWithIdentifierCompatibleWithVersionNumber(
        aKextManager, identifier, versionPtr);
}

/*******************************************************************************
*
*******************************************************************************/
CFArrayRef KXKextManagerCopyKextsWithIdentifier(KXKextManagerRef aKextManager,
    CFStringRef identifier)
{
    CFMutableArrayRef theKexts = NULL;  // returned
    KXKextRef thisKext = NULL;          // don't release
    KXKextRef dupKext = NULL;           // don't release

    theKexts = CFArrayCreateMutable(kCFAllocatorDefault,
        0, &kCFTypeArrayCallBacks);
    if (!theKexts) {
        goto finish;
    }

    thisKext = KXKextManagerGetKextWithIdentifier(aKextManager, identifier);

   /* Add all duplicates of each version.
    * FIXME: return an empty array, or return null?
    */
    while (thisKext) {
        CFArrayAppendValue(theKexts, thisKext);

        dupKext = thisKext;
        while ( (dupKext = KXKextGetDuplicateVersionKext(dupKext)) ) {
            CFArrayAppendValue(theKexts, dupKext);
        }

        thisKext = KXKextGetPriorVersionKext(thisKext);
    }

finish:
    return theKexts;
}

/*******************************************************************************
*
*******************************************************************************/
CFMutableArrayRef KXKextManagerCopyAllKexts(KXKextManagerRef aKextManager)
{
    CFMutableArrayRef kextArray = NULL;  // returned
    KXKextRef * kexts = NULL;           // must free
    CFIndex count, i;

    kextArray = CFArrayCreateMutable(kCFAllocatorDefault,
        0, &kCFTypeArrayCallBacks);
    if (!kextArray) {
        goto finish;
    }

    count = CFArrayGetCount(aKextManager->repositoryList);
    for (i = 0; i < count; i++) {
        KXKextRepositoryRef thisRepository = (KXKextRepositoryRef)
            CFArrayGetValueAtIndex(aKextManager->repositoryList, i);

        CFArrayRef candidateKexts =
            _KXKextRepositoryGetCandidateKexts(thisRepository);
        CFArrayRef badKexts = _KXKextRepositoryGetBadKexts(thisRepository);

        CFArrayAppendArray(kextArray, candidateKexts,
            CFRangeMake(0, CFArrayGetCount(candidateKexts)));
        CFArrayAppendArray(kextArray, badKexts,
            CFRangeMake(0, CFArrayGetCount(badKexts)));
    }

finish:
    if (kexts) free(kexts);
    return kextArray;
}

/*******************************************************************************
*
*******************************************************************************/
CFArrayRef KXKextManagerGetKextsWithMissingDependencies(
    KXKextManagerRef aKextManager)
{
    return aKextManager->kextsWithMissingDependencies;
}

/*******************************************************************************
*
*******************************************************************************/
CFArrayRef KXKextManagerCopyAllKextPersonalities(
    KXKextManagerRef aKextManager)
{
    CFMutableArrayRef thePersonalities = NULL;  // returned
    CFIndex count, i;
    KXKextRef * values = NULL;  // must free

   /*****
    * Make sure we have the candidate kexts arranged with their version
    * relationships. Force a clear of the relationship trees if necessary
    * and then (re)build it as needed.
    */
    if (aKextManager->needsClearRelationships) {
        __KXKextManagerClearRelationships(aKextManager);
    }
    if (aKextManager->needsCalculateRelationships) {
        KXKextManagerCalculateVersionRelationships(aKextManager);
    }

    thePersonalities = CFArrayCreateMutable(kCFAllocatorDefault,
        0, &kCFTypeArrayCallBacks);
    if (!thePersonalities) {
        _KXKextManagerLogError(aKextManager, "no memory?");
        goto finish;
    }

    count = CFDictionaryGetCount(aKextManager->candidateKexts);

    values = (KXKextRef *)malloc(count * sizeof(KXKextRef));
    if (!values) {
        _KXKextManagerLogError(aKextManager, "no memory?");
        goto finish;
    }

    CFDictionaryGetKeysAndValues(aKextManager->candidateKexts, NULL,
        (const void **)values);

    for (i = 0; i < count; i++) {
        KXKextRef thisKext = values[i];
        CFArrayRef personalities = KXKextCopyPersonalitiesArray(thisKext);
        if (personalities) {
            CFArrayAppendArray(thePersonalities, personalities,
                CFRangeMake(0, CFArrayGetCount(personalities)));
            CFRelease(personalities);
        }
    }

finish:

    if (values) free(values);
    return (CFArrayRef)thePersonalities;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError KXKextManagerSendAllKextPersonalitiesToCatalog(
    KXKextManagerRef aKextManager)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    Boolean cacheError = false;
    CFMutableArrayRef thePersonalities = NULL;  // returned
    CFIndex numRepositories, nonCachedKextCount, probCachedKextCount, i;
    KXKextRef * values = NULL;  // must free

    IOCatalogueReset(kIOMasterPortDefault, kIOCatalogResetDefault);

   /*****
    * Make sure we have the candidate kexts arranged with their version
    * relationships. Force a clear of the relationship trees if necessary
    * and then (re)build it as needed.
    */

    if (aKextManager->needsClearRelationships) {
        __KXKextManagerClearRelationships(aKextManager);
    }
    if (aKextManager->needsCalculateRelationships) {
        KXKextManagerCalculateVersionRelationships(aKextManager);
    }

    probCachedKextCount = CFDictionaryGetCount(aKextManager->candidateKexts);
    numRepositories = CFArrayGetCount(aKextManager->repositoryList);
    for (i = 0; i < numRepositories; i++) {
        KXKextRepositoryRef thisRepository = (KXKextRepositoryRef)
            CFArrayGetValueAtIndex(aKextManager->repositoryList, i);

        result = KXKextRepositorySendCatalogFromCache(thisRepository, aKextManager->candidateKexts);
        // regardless of result, aKextManager->candidateKexts may have been altered
        aKextManager->needsCalculateRelationships = true;
        if (kKXKextManagerErrorCache == result) {
            cacheError = true;
        }
    }

    result = kKXKextManagerErrorNone;
    nonCachedKextCount = CFDictionaryGetCount(aKextManager->candidateKexts);
    _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelDefault, NULL, 0,
        "%ld cached, %ld uncached personalities to catalog",
        probCachedKextCount - nonCachedKextCount, nonCachedKextCount);

    if (!nonCachedKextCount) {
        goto finish;
    }

    if (cacheError) {
        __KXKextManagerClearRelationships(aKextManager);
        KXKextManagerCalculateVersionRelationships(aKextManager);
    }

    nonCachedKextCount = CFDictionaryGetCount(aKextManager->candidateKexts);
    values = (KXKextRef *)malloc(nonCachedKextCount * sizeof(KXKextRef));
    if (!values) {
        _KXKextManagerLogError(aKextManager, "no memory?");
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    CFDictionaryGetKeysAndValues(aKextManager->candidateKexts, NULL,
        (const void **)values);

    thePersonalities = CFArrayCreateMutable(kCFAllocatorDefault,
        0, &kCFTypeArrayCallBacks);
    if (!thePersonalities) {
        _KXKextManagerLogError(aKextManager, "no memory?");
        goto finish;
    }

    for (i = 0; i < nonCachedKextCount; i++) {
        KXKextRef thisKext = values[i];
        CFArrayRef personalities = KXKextCopyPersonalitiesArray(thisKext);
        if (personalities) {
            CFArrayAppendArray(thePersonalities, personalities,
                CFRangeMake(0, CFArrayGetCount(personalities)));
            CFRelease(personalities);
        }
    }

   /* Ensure all references to CFBundles are dropped *before* resetting,
    * or CFBundleCreate won't bother to read from disk.
    */
    __KXKextManagerClearRelationships(aKextManager);

    for (i = 0; i < numRepositories; i++) {
        KXKextRepositoryRef thisRepository = (KXKextRepositoryRef)
            CFArrayGetValueAtIndex(aKextManager->repositoryList, i);

        KXKextRepositoryResetIfNeeded(thisRepository);
    }

    result = KXKextManagerSendPersonalitiesToCatalog(aKextManager, thePersonalities);

finish:
    if (values) free(values);
    if (thePersonalities) CFRelease(thePersonalities);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextManagerAuthenticateKexts(KXKextManagerRef aKextManager)
{
    CFIndex count, i;

    _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelDetails, NULL, 0,
        "authenticating all kernel extensions");

    count = CFArrayGetCount(aKextManager->repositoryList);
    for (i = 0; i < count; i++) {
        KXKextRepositoryRef thisRepository = (KXKextRepositoryRef)
            CFArrayGetValueAtIndex(aKextManager->repositoryList, i);
        KXKextRepositoryAuthenticateKexts(thisRepository);
    }

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextManagerMarkKextsAuthentic(KXKextManagerRef aKextManager)
{
    CFIndex count, i;

    _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelDetails, NULL, 0,
        "marking all kernel extensions authentic");

    count = CFArrayGetCount(aKextManager->repositoryList);
    for (i = 0; i < count; i++) {
        KXKextRepositoryRef thisRep = (KXKextRepositoryRef)
            CFArrayGetValueAtIndex(aKextManager->repositoryList, i);
        KXKextRepositoryMarkKextsAuthentic(thisRep);
    }

    return;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean KXKextManagerPerformsStrictAuthentication(
    KXKextManagerRef aKextManager)
{
    return aKextManager->performsStrictAuthentication;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextManagerSetPerformsStrictAuthentication(
    KXKextManagerRef aKextManager,
    Boolean flag)
{
    aKextManager->performsStrictAuthentication = flag;
    return;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean KXKextManagerWillUpdateCatalog(
    KXKextManagerRef aKextManager)
{
    return aKextManager->willUpdateCatalog;
}

/*******************************************************************************
*
*******************************************************************************/

void KXKextManagerSetWillUpdateCatalog(
    KXKextManagerRef aKextManager,
    Boolean flag)
{
    aKextManager->willUpdateCatalog = flag;
    return;
}
/*******************************************************************************
*
*******************************************************************************/

void KXKextManagerVerifyIntegrityOfAllKexts(KXKextManagerRef aKextManager) {
    return;

}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError KXKextManagerCheckForLoadedKexts(
    KXKextManagerRef aKextManager)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    CFIndex repository_count, r, k;
    mach_port_t host_port = MACH_PORT_NULL;
    unsigned int loaded_bytecount;
    CFIndex kmod_count;
    int mach_result;
    kmod_info_t * loaded_modules = NULL;  // must vm_deallocate()
    CFStringRef kmodName = NULL;          // must release
    VERS_version kmod_vers;

    _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelDetails, NULL, 0,
        "checking kernel for loaded extensions");

    repository_count = CFArrayGetCount(aKextManager->repositoryList);
    for (r = 0; r < repository_count; r++) {
        KXKextRepositoryRef thisRepository = (KXKextRepositoryRef)
            CFArrayGetValueAtIndex(aKextManager->repositoryList, r);
        _KXKextRepositoryMarkKextsNotLoaded(thisRepository);
    }

    host_port = mach_host_self();
    mach_result = kmod_get_info(host_port,
	    (void *)&loaded_modules, &loaded_bytecount);
    if (mach_result != KERN_SUCCESS) {
        _KXKextManagerLogError(aKextManager, "kmod_get_info() failed");
        result = kKXKextManagerErrorUnspecified;
        goto finish;
    }

    kmod_count = loaded_bytecount / sizeof(kmod_info_t);

   /*****
    * Find out which modules have already been loaded & verify
    * that loaded versions are same as requested.
    */
    for (k = 0; k < kmod_count; k++) {
        kmod_info_t * this_kmod = &(loaded_modules[k]);  // don't free
        KXKextRef thisKext = NULL;                       // don't release

        if (kmodName) {
            CFRelease(kmodName);
            kmodName = NULL;
        }

        _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelKexts, NULL, 0,
            "    found %s [version %s]", this_kmod->name,
                this_kmod->version);

        kmodName = CFStringCreateWithCString(kCFAllocatorDefault,
            this_kmod->name, kCFStringEncodingUTF8);
        if (!kmodName) {
            result = kKXKextManagerErrorNoMemory;
            goto finish;
        }
        kmod_vers = VERS_parse_string(this_kmod->version);
        if (kmod_vers < 0) {
            _KXKextManagerLogError(aKextManager,
                "can't parse version %s of module %s; skipping",
                this_kmod->version, this_kmod->name);
            continue;
        }

        thisKext = KXKextManagerGetKextWithIdentifier(
            aKextManager, kmodName);
        CFRelease(kmodName);
        kmodName = NULL;

        while (thisKext) {
            KXKextRef dupKext = thisKext;

            while (dupKext) {
                _KXKextSetStartAddress(dupKext, this_kmod->address);
                if (_KXKextGetVersion(dupKext) == kmod_vers) {
                    _KXKextSetIsLoaded(dupKext, true);
                } else {
                    _KXKextSetOtherVersionIsLoaded(dupKext, true);
                }
                dupKext = KXKextGetDuplicateVersionKext(dupKext);
            }
            thisKext = KXKextGetPriorVersionKext(thisKext);
        }

       /* I don't know why I have to perform this check; kmod_get_info() should
        * just return the proper amount of kmod info structs it gave me.
        */
        if (!this_kmod->next) {
            break;
        }
    }

finish:

   /* Dispose of the host port to prevent security breaches and port
    * leaks. We don't care about the kern_return_t value of this
    * call for now as there's nothing we can do if it fails.
    */
    if (MACH_PORT_NULL != host_port) {
        mach_port_deallocate(mach_task_self(), host_port);
    }

    if (loaded_modules) {
        vm_deallocate(mach_task_self(), (vm_address_t)loaded_modules,
            loaded_bytecount);
        loaded_modules = 0;
    }
    if (kmodName)    CFRelease(kmodName);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError KXKextManagerLoadKext(
    KXKextManagerRef aKextManager,
    KXKextRef aKext)
{
    KXKextManagerError result = kKXKextManagerErrorNone;

    result = KXKextManagerLoadKextUsingOptions(
        aKextManager,
        aKext,
        NULL,  // kext name (for log messages); will get taken from aKext
        NULL,  // kernel file; default will get used
        NULL,  // patch_dir; no patches
        NULL,  // symbol_dir; no symbol files
        true,  // check_loaded_for_dependencies; yes, check this
        true,  // do_load; yes, of course we're loading
        true,  // do_start_kext; yes, start the kext's code running
        0,     // not at all interactive
        false, // ask_overwrite_symbols; no waiting for input
        false, // overwrite_symbols; we're not writing symbols
        false, // get_addrs_from_kernel; we're not writing symbols
        0,     // num_addresses; we're not writing symbols
        NULL   // addresses; we're not writing symbols
    );

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError KXKextManagerLoadKextWithIdentifier(
    KXKextManagerRef aKextManager,
    CFStringRef identifier)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    KXKextRef theKext = NULL;  // don't release

    theKext = KXKextManagerGetKextWithIdentifier(aKextManager,
        identifier);
    if (!theKext) {
        result = kKXKextManagerErrorKextNotFound;
        goto finish;
    }
    result = KXKextManagerLoadKext(aKextManager, theKext);

finish:

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError KXKextManagerLoadKextUsingOptions(
    KXKextManagerRef aKextManager,
    KXKextRef aKext,
    const char * kext_name,
    const char * kernel_file,
    const char * patch_dir,
    const char * symbol_dir,
    Boolean check_loaded_for_dependencies,
    Boolean do_load,
    Boolean do_start_kext,
    int     interactive_level,
    Boolean ask_overwrite_symbols,
    Boolean overwrite_symbols,
    Boolean get_addrs_from_kernel,
    unsigned int num_addresses,
    char ** addresses)
{
    KXKextManagerError result = kKXKextManagerErrorNone;

    result = _KXKextManagerPrepareKextForLoading(aKextManager, aKext, kext_name,
        check_loaded_for_dependencies, do_load, NULL /* inauthenticKexts*/);
    if (result != kKXKextManagerErrorNone) {
        goto finish;
    }

    result = _KXKextManagerLoadKextUsingOptions(
        aKextManager, aKext,
        kext_name, kernel_file, patch_dir, symbol_dir,
        do_load, do_start_kext, interactive_level,
        ask_overwrite_symbols, overwrite_symbols, get_addrs_from_kernel,
        num_addresses, addresses);

finish:
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError _KXKextManagerPrepareKextForLoading(
    KXKextManagerRef aKextManager,
    KXKextRef aKext,
    const char * kext_name,
    Boolean check_loaded_for_dependencies,
    Boolean do_load,
    CFMutableArrayRef inauthenticKexts)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    const char * kext_name_buffer = NULL;  // must free
    CFMutableArrayRef kextList = NULL;  // must release
    CFIndex count, i;
    KXKextManagerError auth_result = kKXKextManagerErrorNone;

    result = __KXKextRealizeFromCache(aKext);
    if (result != kKXKextManagerErrorNone) {
        goto finish;
    }

    if (KXKextGetIsKernelResource(aKext)) {
        result = kKXKextManagerErrorLoadKernelComponent;
        goto finish;
    }

    if (KXKextGetDeclaresExecutable(aKext) &&
	    !KXKextSupportsHostArchitecture(aKext)) {

        result = kKXKextManagerErrorLoadExecutableNoArch;
        goto finish;
    }

   /*****
    * The kext_name is optional. If not provided, set it to the absolute
    * pathname of aKext.
    */
    if (!kext_name) {
        kext_name_buffer = _KXKextCopyCanonicalPathnameAsCString(aKext);
        if (!kext_name_buffer) {
            result = kKXKextManagerErrorNoMemory;
            goto finish;
        }
        kext_name = kext_name_buffer;
    }

    _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelBasic, NULL, 0,
        "loading extension %s", kext_name);

    if (!KXKextIsValid(aKext)) {
        _KXKextManagerLogError(aKextManager,
            "request to load invalid extension %s", kext_name);
        result = kKXKextManagerErrorValidation;
        goto finish;
    }

    /*if (!KXKextGetDeclaresExecutable(aKext)) {
        _KXKextManagerLogError(aKextManager,
            "request to load extension with no executable, %s", kext_name);
        result = kKXKextManagerErrorValidation;
        goto finish;
    }*/

    if (aKextManager->safeBoot && !KXKextIsEligibleDuringSafeBoot(aKext)) {
        _KXKextManagerLogError(aKextManager,
            "request to load non-safe-boot extension %s during safe boot",
            kext_name);
        result = kKXKextManagerErrorBootLevel;
        goto finish;
    }

    if (!KXKextIsEnabled(aKext)) {
        _KXKextManagerLogError(aKextManager,
            "request to load disabled extension %s",
            kext_name);
        result = kKXKextManagerErrorDisabled;
        goto finish;
    }

   /*****
    * Make sure we have the candidate kexts arranged with their version
    * relationships. Force a clear of the relationship trees if necessary
    * and then (re)build as needed.
    */
    if (aKextManager->needsClearRelationships) {
        __KXKextManagerClearRelationships(aKextManager);
        __KXKextManagerClearDependencyRelationships(aKextManager);
    }
    if (aKextManager->needsCalculateRelationships) {
        KXKextManagerCalculateVersionRelationships(aKextManager);
    }

   /*****
    * Before doing anything more check which kexts are already loaded.
    */
    if (!check_loaded_for_dependencies) {
#if 0
        _KXKextManagerLogError(aKextManager,
            "warning: not checking for loaded extensions can result in"
            " faulty dependency resolution and a load failure");
        // FIXME: should this clear the isLoaded & otherVersionIsLoaded
        // FIXME: ...attributes of the kexts?
#endif 0
    } else {
        result = KXKextManagerCheckForLoadedKexts(aKextManager);
        if (result != kKXKextManagerErrorNone) {
            _KXKextManagerLogError(aKextManager,
                "can't check which kernel extensions are currently loaded");
            goto finish;
        }

        if (do_load) {
            if (KXKextIsLoaded(aKext)) {
                _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelDetails, NULL, 0,
                    "kernel extension %s is already loaded", kext_name);
                result = kKXKextManagerErrorAlreadyLoaded;
                goto finish;
            }

            if (KXKextOtherVersionIsLoaded(aKext)) {
                _KXKextManagerLogError(aKextManager,
                 "a different version of kernel extension %s is already loaded",
                    kext_name);
                result = kKXKextManagerErrorLoadedVersionDiffers;
                goto finish;
            }
        }
    }

   /*****
    * Now resolve dependencies on the kext being loaded. We blow away all
    * existing dependency info to make sure what we have is up to date.
    */
    __KXKextManagerClearDependencyRelationships(aKextManager);
    result = KXKextResolveDependencies(aKext);
    if (result != kKXKextManagerErrorNone) {
        CFArrayAppendValue(aKextManager->kextsWithMissingDependencies,
            aKext);

        _KXKextManagerLogError(aKextManager,
            "cannot resolve dependencies for kernel extension %s",
            kext_name);

        // Do not disqualify kext; this is not a hard failure.
        goto finish;
    }

   /*****
    * Authenticate the dependency list. If any of them fails, disqualify
    * it from future consideration, and after checking them all, return
    * an authentication failure result.
    */
    KXKextManagerDisableClearRelationships(aKextManager);
    //
    // do not goto finish until the matching
    // KXKextManagerEnableClearRelationships() has been called!

    kextList = KXKextCopyAllDependencies(aKext);
    if (!kextList) {
        kextList = CFArrayCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeArrayCallBacks);
        if (!kextList) {
            result = kKXKextManagerErrorNoMemory;
        }            
    }

    if (kextList) {
        CFArrayAppendValue(kextList, aKext);
        count = CFArrayGetCount(kextList);
        for (i = 0; i < count; i++) {
            KXKextRef thisKext = (KXKextRef)CFArrayGetValueAtIndex(
                kextList, i);

           /* Check if a loaded dependency is of a different version
            * from the one being considered.
            */
            if (KXKextOtherVersionIsLoaded(thisKext)) {
                const char * this_kext_name = NULL; // must free
                this_kext_name =
                    _KXKextCopyCanonicalPathnameAsCString(thisKext);
                _KXKextManagerLogError(aKextManager,
                     "a different version of dependency extension %s is already loaded",
                     this_kext_name ? this_kext_name : "(unknown)");
                if (this_kext_name) free((char *)this_kext_name);

                if (result == kKXKextManagerErrorNone || 
                    result == kKXKextManagerErrorDependencyLoadedVersionDiffers) {

                    result = kKXKextManagerErrorDependencyLoadedVersionDiffers;
                } else {
                    result = kKXKextManagerErrorUnspecified;  // multiple errors
                }

                if (!KXKextManagerPerformsFullTests(aKextManager)) {
                    break;
                }
            }

           /* We allow the kext to say it's been authenticated and
            * not check it over and over.
            */
            if (!KXKextIsAuthentic(thisKext)) {
                auth_result = KXKextAuthenticate(thisKext);

                if (auth_result != kKXKextManagerErrorNone) {
                    if (result == kKXKextManagerErrorNone || result == auth_result) {
                        result = auth_result;
                    } else {
                        result = kKXKextManagerErrorUnspecified;  // multiple errors
                    }
                }


                if (auth_result == kKXKextManagerErrorAuthentication) {
                    if (inauthenticKexts) {
                        CFArrayAppendValue(inauthenticKexts, thisKext);
                        continue;
                    } else {
                        const char * this_kext_name = NULL; // must free
                        this_kext_name =
                            _KXKextCopyCanonicalPathnameAsCString(thisKext);
                        if (auth_result == kKXKextManagerErrorAuthentication) {
                            _KXKextManagerLogError(aKextManager,
                                "authentication failed for extension %s",
                                this_kext_name ? this_kext_name : "(unknown)");
                        } else if (auth_result == kKXKextManagerErrorCache) {
                            _KXKextManagerLogError(aKextManager,
                               "cache inconsistency noted for extension %s",
                                this_kext_name ? this_kext_name : "(unknown)");
                        } else {
                            _KXKextManagerLogError(aKextManager,
                               "error during authentication of extension %s",
                                this_kext_name ? this_kext_name : "(unknown)");
                        }
                        KXKextManagerDisqualifyKext(aKextManager, thisKext);
                        if (this_kext_name) free((char *)this_kext_name);

                        if (!KXKextManagerPerformsFullTests(aKextManager)) {
                            break;
                        }
                    }
                }
            }
        }
    }

    KXKextManagerEnableClearRelationships(aKextManager);
    //
    // OK to goto finish now

finish:
    if (kext_name_buffer) free((char *)kext_name_buffer);
    if (kextList)         CFRelease(kextList);

    return result;
}

KXKextManagerError _KXKextManagerLoadKextUsingOptions(
    KXKextManagerRef aKextManager,
    KXKextRef aKext,
    const char * kext_name,
    const char * kernel_file,
    const char * patch_dir,
    const char * symbol_dir,
    IOOptionBits load_options,
    Boolean do_start_kext,
    int     interactive_level,
    Boolean ask_overwrite_symbols,
    Boolean overwrite_symbols,
    Boolean get_addrs_from_kernel,
    unsigned int num_addresses,
    char ** addresses)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    KXKextManagerError loader_result = kKXKextManagerErrorNone;
    const char * kext_name_buffer = NULL;  // must free
    dgraph_t * dgraph = NULL;     // must dgraph_free()
    pid_t fork_pid;
    Boolean do_load, do_prelink;
    unsigned i;
    Boolean exit_on_finish = false;  // for a forked task

    if (KXKextGetIsKernelResource(aKext)) {
        result = kKXKextManagerErrorLoadKernelComponent;
        goto finish;
    }

   /*****
    * The kext_name is optional. If not provided, set it to the absolute
    * pathname of aKext.
    */
    if (!kext_name) {
        kext_name_buffer = _KXKextCopyCanonicalPathnameAsCString(aKext);
        if (!kext_name_buffer) {
            result = kKXKextManagerErrorNoMemory;
            goto finish;
        }
        kext_name = kext_name_buffer;
    }

    if (symbol_dir && (kKXKextManagerLoadPrelink == load_options)) {
	do_load = false;
	do_prelink = true;
    } else {
	do_load = (kKXKextManagerLoadNone != load_options);
	do_prelink = false;
    }

    if (do_load && (kKXKextManagerLoadKextd != load_options) && !KXKextHasPersonalities(aKext)) {
	/*****
	* if this isn't a kextd request, give the kernel a chance to load it from the prelinked list,
	* and record the load request.
	*/
	CFStringRef     key = CFSTR("OSBundleModuleDemand");
	CFStringRef     value = KXKextGetBundleIdentifier(aKext);
	CFDictionaryRef dict = CFDictionaryCreate(kCFAllocatorDefault,
					(const void **) &key, (const void **) &value, 1,
					&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFArrayRef      array = CFArrayCreate(kCFAllocatorDefault, (const void **) &dict, 1,
					&kCFTypeArrayCallBacks);
	CFRelease(dict);
	KXKextManagerSendPersonalitiesToCatalog(aKextManager, array);
	CFRelease(array);
    }

   /*****
    * Get the dgraph for the kext so we can do the load.
    */
    dgraph = _KXKextCreateDgraph(aKext);
    if (!dgraph) {
        // FIXME: Make better message.
        _KXKextManagerLogError(aKextManager,
            "can't generate internal dependency graph for %s", kext_name);
        KXKextManagerDisqualifyKext(aKextManager, aKext);
        result = kKXKextManagerErrorUnspecified;
        goto finish;
    }

   /*****
    */

    for (i = 0; i < dgraph->length; i++) {
        dgraph_entry_t * entry = dgraph->graph[i];

        if (!entry) {
            result = kload_error_unspecified;
            goto finish;
        }

        if (entry->is_kernel_component && !entry->is_symbol_set) {
            continue;
        }

	if (symbol_dir) {
#define __KLOAD_SYMBOL_EXTENSION   ".sym"
	    unsigned long length = strlen(symbol_dir) + 1 +
		strlen(entry->expected_kmod_name) +
		strlen(__KLOAD_SYMBOL_EXTENSION) + 1;
	    if (length >= MAXPATHLEN) {
		kload_log_error(
		    "output symbol filename \"%s/%s%s\" would be too long" KNL,
		    symbol_dir, entry->expected_kmod_name,
		    __KLOAD_SYMBOL_EXTENSION);
		result = kload_error_invalid_argument;
		goto finish;
	    }
    
	    entry->link_output_file = (char *)malloc(length);
	    if (!entry->link_output_file) {
		kload_log_error("memory allocation failure" KNL);
		result = kload_error_no_memory;
		goto finish;
	    }
    
	    entry->link_output_file_alloc = true;
	    strlcpy(entry->link_output_file, symbol_dir, length);
	    strlcat(entry->link_output_file, "/", length);
	    strlcat(entry->link_output_file, entry->expected_kmod_name, length);
	    if (do_prelink) {
		if (kload_file_exists(entry->link_output_file) > 0) {
		    entry->name = entry->link_output_file;
		}
	    } else {
		strlcat(entry->link_output_file, __KLOAD_SYMBOL_EXTENSION, length);
	    }
	}
    }

   /*****
    * If we're just doing symbols with user-supplied addresses, all the
    * modules must have nonzero addresses set. These either come from the
    * num_addresses/addresses arguments, or the user is asked to provide
    * them, meaning this routine will block waiting for input.
    */
    if (!do_load && !do_prelink && symbol_dir && !get_addrs_from_kernel) {
        if (num_addresses > 0) {
            unsigned m;
            loader_result = kload_set_load_addresses_from_args(
                dgraph, addresses);

            if (loader_result == kKXKextManagerErrorInvalidArgument) {
                _KXKextManagerLogError(aKextManager,
                    "these modules don't have addresses set:");
                for (m = 0; m < dgraph->length; m++) {
                    dgraph_entry_t * entry = dgraph->load_order[m];
                    if (entry->is_kernel_component) {
                        continue;
                    }
                    if (!entry->loaded_address) {
                        _KXKextManagerLogError(aKextManager, "    %s",
                            entry->expected_kmod_name);
                    }
                }
                _KXKextManagerLogError(aKextManager, "");  // for a newline
                result = kKXKextManagerErrorInvalidArgument;
                goto finish;
            } else if (loader_result != kKXKextManagerErrorNone) {
                _KXKextManagerLogError(aKextManager, "error getting load addresses");
                result = kKXKextManagerErrorUnspecified;
                goto finish;
            }
        } else {
            loader_result = kload_request_load_addresses(dgraph);
            if (loader_result != kKXKextManagerErrorNone) {
                _KXKextManagerLogError(aKextManager, "error getting load addresses");
                result = kKXKextManagerErrorUnspecified;
                goto finish;
            }
        }
    }

   /*****
    * Perform the load.
    */
    if (!aKextManager->performLoadsInTask) {
        _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelDetails, NULL, 0,
            "forking child task to perform load");

        fork_pid = fork();
        if (fork_pid < 0) {
            _KXKextManagerLogError(aKextManager, "can't fork child process to load");
            result = kKXKextManagerErrorUnspecified;
            goto finish;
        } else if (fork_pid == 0) {
            // child
            exit_on_finish = true;
        } else {
            // parent
            pid_t wait_pid;
            int status = 0;  // always clear status before calling wait!

            wait_pid = waitpid(fork_pid, &status, WUNTRACED);
            if (WIFEXITED(status)) {
                result = WEXITSTATUS(status);  // taken from this function!
            } else if (WIFSIGNALED(status)) {
                _KXKextManagerLogError(aKextManager,
                    "forked load task exited by signal (%d)", WTERMSIG(status));
                result = kKXKextManagerErrorChildTask;
            } else if (WIFSTOPPED(status)) {
                _KXKextManagerLogError(aKextManager,
                    "forked load task has stopped");
                result = kKXKextManagerErrorChildTask;
            } else {
                _KXKextManagerLogError(aKextManager,
                    "unknown result from forked load task");
                result = kKXKextManagerErrorChildTask;
            }
            goto finish;
        }
    }

    // FIXME: this can duplicate the earlier work in getting kmod info
    // FIXME: ...if we need to get load addresses from the kernel.

    result = kload_load_dgraph(dgraph,
        kernel_file,
        /* patch file */ NULL, patch_dir,
        /* symbol file */ NULL, symbol_dir,
        do_load, do_start_kext, do_prelink,
        interactive_level,
        ask_overwrite_symbols, overwrite_symbols);

    if (result != kKXKextManagerErrorNone) {

       /*****
        * We shouldn't see these "already" errors, but it can't hurt to check.
        */
        if (result == kKXKextManagerErrorAlreadyLoaded) {
            _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelDetails, NULL, 0,
                "kernel extension %s is already loaded", kext_name);
        } else if (result == kKXKextManagerErrorLoadedVersionDiffers) {
            _KXKextManagerLogError(aKextManager,
                "a different version of %s, or of one of its dependencies,"
                " is already loaded", kext_name);
        } else if (result == kKXKextManagerErrorUserAbort) {
            _KXKextManagerLogError(aKextManager,
                "the user aborted the load of extension");
        } else {
            _KXKextManagerLogError(aKextManager,
                "a link/load error occured for kernel extension %s", kext_name);
        }
        goto finish;
    }

finish:
   /* A forked child load must exit immediately and not attempt
    * clean up.
    */
    if (exit_on_finish) {
        exit(result);
    }
    if (dgraph)             dgraph_free(dgraph, 1);
    if (kext_name_buffer)   free((char *)kext_name_buffer);

   /* If the load failed, mark the kext so and clear the database of
    * kexts so it gets rebuilt when next needed. This kext will be
    * excluded from the database until another kext is added, possibly
    * resolving dependencies.
    */
    if (result != kKXKextManagerErrorNone &&
        result != kKXKextManagerErrorAlreadyLoaded) {

        KXKextSetLoadFailed(aKext, true);
        KXKextManagerClearRelationships(aKextManager);
    }

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError KXKextManagerSendKextPersonalitiesToCatalog(
    KXKextManagerRef aKextManager,
    KXKextRef aKext,
    CFArrayRef personalityNames,
    Boolean includeDependencies,
    int interactiveLevel,
    Boolean safeBoot)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    const char * url_path = NULL;     // must free
    char * kext_name = NULL;         // must free
    char * personality_name = NULL;  // must free
    CFStringRef * personalityKeys = NULL;  // must free
    CFStringRef * personalities = NULL;  // must free

    CFStringRef kextPath = NULL;     // must release
    CFMutableArrayRef kexts = NULL;  // dependencies + kext; must release

   /* These 3 objects are matched by index in triples so we maintain
    * association between {kext, personality name, personality}.
    */
    CFMutableArrayRef kextsToSend = NULL; // must release
    CFMutableArrayRef personalityNamesToSend = NULL;  // must release
    CFMutableArrayRef personalitiesToSend = NULL;  // must release

    CFDictionaryRef kextPersonalities = NULL;    // must release

    KXKextRef thisKext = NULL;  // do not release
    CFStringRef personalityName = NULL;  // do not release

    CFRange personalityNamesRange;
    CFIndex numPersonalityNames = 0;
    CFIndex numKexts, kextIndex;
    CFIndex numPersonalities, personalityIndex;

    url_path = _KXKextCopyCanonicalPathnameAsCString(aKext);
    if (!url_path) {
        _KXKextManagerLogError(aKextManager, "internal error, or no memory?");
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    _KXKextManagerLogMessageAtLevel(aKextManager,
        kKXKextManagerLogLevelKexts, NULL, 0,
        "loading personalities for extension %s%s",  url_path,
        includeDependencies ? " and dependencies" : "");

   /* Get a list of dependencies in load order, and append the kext itself.
    */
    kexts = KXKextCopyAllDependencies(aKext);
    if (!kexts) {
        _KXKextManagerLogError(aKextManager, "no memory?");
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }
    
    CFArrayAppendValue(kexts, aKext);
    numKexts = CFArrayGetCount(kexts);

   /* Make the arrays for our local bookkeeping.
    */
    kextsToSend = CFArrayCreateMutable(kCFAllocatorDefault,
        0, &kCFTypeArrayCallBacks);

    personalitiesToSend = CFArrayCreateMutable(kCFAllocatorDefault,
        0, &kCFTypeArrayCallBacks);

    personalityNamesToSend = CFArrayCreateMutable(kCFAllocatorDefault,
        0, &kCFTypeArrayCallBacks);
    if (!kextsToSend || !personalitiesToSend || !personalityNamesToSend) {
        _KXKextManagerLogError(aKextManager, "no memory?");
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    numPersonalityNames = 0;
    if (personalityNames) {
        numPersonalities = CFArrayGetCount(personalityNames);
        if (numPersonalityNames) {
            personalityNamesRange = CFRangeMake(0, numPersonalities);
        }
    }

    for (kextIndex = 0; kextIndex < numKexts; kextIndex++) {
        Boolean isDependency = (kextIndex + 1 < numKexts) ? true : false;

        thisKext = (KXKextRef)CFArrayGetValueAtIndex(kexts, kextIndex);

        if (kext_name) {
            free(kext_name);
            kext_name = NULL;
        }
        if (personalityKeys) {
            free(personalityKeys);
            personalityKeys = NULL;
        }
        if (personalities) {
            free(personalities);
            personalities = NULL;
        }

        kextPersonalities = KXKextCopyPersonalities(thisKext);
        if (!kextPersonalities) {
            continue;
        }

        numPersonalities = CFDictionaryGetCount(kextPersonalities);
        if (!numPersonalities) {
            continue;  // Skip this kext.
        }

        kextPath = KXKextCopyAbsolutePath(thisKext);
        if (!kextPath) {
            _KXKextManagerLogError(aKextManager, "no memory?");
            result = kKXKextManagerErrorNoMemory;
            goto finish;
        }
        kext_name = cStringForCFString(kextPath);
        CFRelease(kextPath);
        kextPath = NULL;

        personalityKeys = (CFStringRef *)malloc(
            numPersonalities * sizeof(CFTypeRef));
        personalities = (CFStringRef *)malloc(
            numPersonalities * sizeof(CFTypeRef));

        if (!kext_name || !personalityKeys || !personalities) {
            _KXKextManagerLogError(aKextManager, "no memory?");
            result = kKXKextManagerErrorNoMemory;
            goto finish;
        }
        CFDictionaryGetKeysAndValues(kextPersonalities,
            (const void **)personalityKeys,
            (const void **)personalities);

        for (personalityIndex = 0;
             personalityIndex < numPersonalities;
             personalityIndex++) {

            CFDictionaryRef personality =
                (CFDictionaryRef)personalities[personalityIndex];
            int okToSend = 1;

            personalityName = (CFStringRef)personalityKeys[personalityIndex];

            if (personality_name) {
                free(personality_name);
                personality_name = NULL;
            }
            personality_name = cStringForCFString(personalityName);
            if (!personality_name) {
                _KXKextManagerLogError(aKextManager, "no memory?");
                result = kKXKextManagerErrorNoMemory;
                goto finish;
            }

            if (numPersonalityNames) {
                if (-1 == CFArrayGetFirstIndexOfValue(personalityNames,
                    personalityNamesRange, personalityName)) {

                    continue;  // Not named, skip this personality
                }
            }

            if (safeBoot) {
                if (!__KXKextManagerCheckPersonalityForSafeBoot(aKextManager,
                    personality, personality_name)) {
                    
                    continue;  // Skip this personality
                }
            }

            if (isDependency) {
                if (interactiveLevel == 2) {
                    okToSend = aKextManager->user_veto_func(1,
                        "Send personality \"%s\" of dependency %s to the kernel",
                        personality_name, kext_name);
                }
            } else if (interactiveLevel >= 1) {
                okToSend = aKextManager->user_veto_func(
                    1, "Send personality \"%s\" to the kernel",
                    personality_name);
            }
            if (okToSend > 0) {
                CFArrayAppendValue(kextsToSend, thisKext);
                CFArrayAppendValue(personalityNamesToSend, personalityName);
                CFArrayAppendValue(personalitiesToSend, personality);
            } else if (okToSend < 0) {
                _KXKextManagerLogError(aKextManager, "internal failure");
                result = kKXKextManagerErrorUnspecified;
                goto finish;
            }
        }

    }

    numPersonalities = CFArrayGetCount(personalitiesToSend);

    if (_KXKextManagerCheckLogLevel(aKextManager,
        kKXKextManagerLogLevelBasic, NULL, 0)) {
        
        KXKextRef lastKext = NULL;  // do not release

        if (!numPersonalities) {
            _KXKextManagerLogMessage(aKextManager, "no personalities to send");
        } else {
            _KXKextManagerLogMessage(aKextManager, "sending personalities to kernel:");
        }
        for (personalityIndex = 0;
             personalityIndex < numPersonalities;
             personalityIndex++) {
     
            thisKext = (KXKextRef)CFArrayGetValueAtIndex(kextsToSend,
                personalityIndex);
            personalityName = (CFStringRef)CFArrayGetValueAtIndex(
                personalityNamesToSend, personalityIndex);

            if (thisKext != lastKext) {
                if (kext_name) {
                    free(kext_name);
                    kext_name = NULL;
                }
                kextPath = KXKextCopyAbsolutePath(thisKext);
                if (!kextPath) {
                    _KXKextManagerLogError(aKextManager, "no memory?");
                    result = kKXKextManagerErrorNoMemory;
                    goto finish;
                }
                kext_name = cStringForCFString(kextPath);
                CFRelease(kextPath);
                kextPath = NULL;
            }

            if (personality_name) {
                free(personality_name);
                personality_name = NULL;
            }
            personality_name = cStringForCFString(personalityName);

            if (!kext_name || !personality_name) {
                _KXKextManagerLogError(aKextManager, "no memory?");
                result = kKXKextManagerErrorNoMemory;
                goto finish;
            }

            if (thisKext != lastKext) {
                _KXKextManagerLogMessage(aKextManager,
                    "    from extension %s:", kext_name);
            }
            _KXKextManagerLogMessage(aKextManager, "        %s",
                personality_name);
                
            lastKext = thisKext;
        }
    }

    if (numPersonalities) {
        result = KXKextManagerSendPersonalitiesToCatalog(
            aKextManager, personalitiesToSend);
    }

finish:

    if (url_path)                 free((char *)url_path);
    if (kext_name)                free(kext_name);
    if (personality_name)         free(personality_name);
    if (personalityKeys)          free(personalityKeys);
    if (personalities)            free(personalities);

    if (kextPath)                 CFRelease(kextPath);
    if (kexts)                    CFRelease(kexts);
    if (kextsToSend)              CFRelease(kextsToSend);
    if (personalityNamesToSend)   CFRelease(personalityNamesToSend);
    if (personalitiesToSend)      CFRelease(personalitiesToSend);
    if (kextPersonalities)        CFRelease(kextPersonalities);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError KXKextManagerSendPersonalitiesToCatalog(
    KXKextManagerRef aKextManager,
    CFArrayRef personalities)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    kern_return_t kern_result = KERN_SUCCESS;
    CFDataRef data = NULL;
    CFIndex len = 0;
    void * ptr;

    if (!personalities) {
        result = kKXKextManagerErrorInvalidArgument;
        goto finish;
    }

    _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelBasic, NULL, 0,
        "sending %ld personalit%s to the kernel", CFArrayGetCount(personalities),
        (CFArrayGetCount(personalities) != 1) ? "ies" : "y");

    data = IOCFSerialize(personalities, kNilOptions);
    if ( !data ) {
        _KXKextManagerLogError(aKextManager, "error serializing personalities");
        result = kKXKextManagerErrorSerialization;
        goto finish;
    }

    len = CFDataGetLength(data);
    ptr = (void *)CFDataGetBytePtr(data);
    kern_result = IOCatalogueSendData(kIOMasterPortDefault, kIOCatalogAddDrivers,
        ptr, len);

    // FIXME: check specific kernel error result for permission or whatever
    if (kern_result != KERN_SUCCESS) {
       _KXKextManagerLogError(aKextManager, "couldn't send personalities to catalog");
       result = kKXKextManagerErrorKernelError;
       goto finish;
    }

finish:
    if (data) CFRelease(data);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
void KXKextManagerRemoveKextPersonalitiesFromCatalog(
    KXKextManagerRef aKextManager,
    KXKextRef aKext)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    const char * url_path = NULL;     // must free
    CFArrayRef personalities = NULL;  // must release
    CFIndex count, i;

    url_path = _KXKextCopyCanonicalPathnameAsCString(aKext);

    if (url_path) {
        _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelKexts, NULL, 0,
            "removing personalities for extension %s", url_path);
    }

    personalities = KXKextCopyPersonalitiesArray(aKext);
    if (!personalities) {
        // don't consider this an error
        goto finish;
    }

    count = CFArrayGetCount(personalities);
    for (i = 0; i < count; i++) {
        CFDictionaryRef thisPersonality = (CFDictionaryRef)
            CFArrayGetValueAtIndex(personalities, i);
        result = KXKextManagerRemovePersonalitiesFromCatalog(
            aKextManager, thisPersonality);
        if (result != kKXKextManagerErrorNone) {
            goto finish;
        }
    }

finish:
    if (url_path)      free((char *)url_path);
    if (personalities) CFRelease(personalities);

    return;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError KXKextManagerRemovePersonalitiesFromCatalog(
    KXKextManagerRef aKextManager,
    CFDictionaryRef matchingPersonality)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    kern_return_t kern_result = KERN_SUCCESS;
    CFDataRef data = NULL;
    CFIndex dataLength = 0;
    void * dataPointer;

    data = IOCFSerialize(matchingPersonality, kNilOptions);
    if (!data) {
        _KXKextManagerLogError(aKextManager, "error serializing personalities");
        result = kKXKextManagerErrorSerialization;
        goto finish;
    }

    // FIXME: Why is the 1 added to the length?
    dataLength = CFDataGetLength(data) + 1;
    dataPointer = (void *)CFDataGetBytePtr(data);
    kern_result = IOCatalogueSendData(kIOMasterPortDefault, kIOCatalogRemoveDrivers,
        dataPointer, dataLength);

    // FIXME: check specific kernel error result for permission or whatever
    if (kern_result != KERN_SUCCESS) {
       _KXKextManagerLogError(aKextManager,
           "couldn't remove personalities from catalog");
       result = kKXKextManagerErrorKernelError;
       goto finish;
    }

finish:
    if (data) CFRelease(data);

    return result;
}

/*******************************************************************************
********************************************************************************
* FRAMEWORK-PRIVATE API BELOW HERE
********************************************************************************
*******************************************************************************/

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerLogFunction _KXKextManagerGetLogFunction(
    KXKextManagerRef aKextManager)
{
    return aKextManager->log_func;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerErrorLogFunction _KXKextManagerGetErrorLogFunction(
    KXKextManagerRef aKextManager)
{
    return aKextManager->error_log_func;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerUserApproveFunction _KXKextManagerGetUserApproveFunction(
    KXKextManagerRef aKextManager)
{
    return aKextManager->user_approve_func;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerUserVetoFunction _KXKextManagerGetUserVetoFunction(
    KXKextManagerRef aKextManager)
{
    return aKextManager->user_veto_func;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerUserInputFunction _KXKextManagerGetUserInputFunction(
    KXKextManagerRef aKextManager)
{
    return aKextManager->user_input_func;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextManagerLogMessageAtLevel(KXKextManagerRef aKextManager,
    KXKextManagerLogLevel logLevel,
    KXKextRef aKext,  // may be NULL, in which case kextLogLevel is irrelevant
    KXKextLogLevel kextLogLevel,
    const char * format, ...)
{
    va_list ap;
    char * output_string = NULL;

    if (!_KXKextManagerCheckLogLevel(aKextManager, logLevel, aKext, kextLogLevel)) {
        goto finish;
    }

    va_start(ap, format);
    vasprintf(&output_string, format, ap);
    va_end(ap);

    if (!output_string) {
        goto finish;
    }

    aKextManager->log_func("%s", output_string);

finish:
    if (output_string) {
        free(output_string);
    }
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextManagerLogMessage(KXKextManagerRef aKextManager,
    const char * format, ...)
{
    va_list ap;
    char * output_string = NULL;

    if (aKextManager->logLevel <= kKXKextManagerLogLevelSilent) {
        goto finish;
    }

    va_start(ap, format);
    vasprintf(&output_string, format, ap);
    va_end(ap);

    if (!output_string) {
        goto finish;
    }

    aKextManager->log_func("%s", output_string);

finish:
    if (output_string) {
        free(output_string);
    }
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextManagerLogError(KXKextManagerRef aKextManager,
    const char * format, ...)
{
    va_list ap;
    char * output_string = NULL;

    if (aKextManager->logLevel <= kKXKextManagerLogLevelSilent) {
        goto finish;
    }

    va_start(ap, format);
    vasprintf(&output_string, format, ap);
    va_end(ap);

    if (!output_string) {
        goto finish;
    }

    aKextManager->error_log_func("%s", output_string);

finish:
    if (output_string) {
        free(output_string);
    }
    return;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean _KXKextManagerCheckLogLevel(KXKextManagerRef aKextManager,
    KXKextManagerLogLevel logLevel,
    KXKextRef aKext,
    KXKextManagerLogLevel kextLogLevel)
{
    if (aKextManager->logLevel >= logLevel) return true;
    if (aKext && (_KXKextGetLogLevel(aKext) >= kextLogLevel)) return true;
    return false;
}

/*******************************************************************************
*
*******************************************************************************/
#define GZIP_RATIO    (6)
#define MAX_REALLOCS (16)

KXKextManagerError _KXKextManagerAddRepositoryFromCacheFile(
    KXKextManagerRef aKextManager,
    CFURLRef fileURL,
    CFURLRef repositoryURL,
    KXKextRepositoryRef * theRepository)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    char * cache_path = NULL;   // must free
    struct stat stat_buf;
    int gzip_fd = -1;
    ssize_t gzip_length;
    ssize_t data_length;
    unsigned char * gzip_data = NULL;
    unsigned char * data = NULL;
    ssize_t bytes_read, total_bytes_read;
    z_stream zstream;
    int z_result = Z_UNKNOWN;
    int num_reallocs;
    Boolean inflate_tried = false;

    CFDataRef fileData = NULL;  // must release
    CFStringRef errorString = NULL;  // must release
    CFDictionaryRef cacheDictionary = NULL;  // must release

    cache_path = PATH_CanonicalizedCStringForURL(fileURL);
    if (!cache_path) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    if (-1 == stat(cache_path, &stat_buf)) {
        _KXKextManagerLogError(aKextManager,
            "can't stat kext cache file %s",
            cache_path);
        result = kKXKextManagerErrorFileAccess;
        goto finish;
    }

    gzip_fd = open(cache_path, O_RDONLY);
    if (gzip_fd == -1) {
        _KXKextManagerLogError(aKextManager,
            "can't open kext cache file %s for reading",
            cache_path);
        result = kKXKextManagerErrorFileAccess;
        goto finish;
    }

   /* Prepare to read the file into memory and uncompress it there.
    * gzread() has proven inefficient.
    */
    gzip_length = stat_buf.st_size;
    data_length = GZIP_RATIO * gzip_length;
    gzip_data = (unsigned char *)malloc(gzip_length * sizeof(char));
    data = (unsigned char *)malloc(data_length * sizeof(char));
    if (!gzip_data || !data) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    total_bytes_read = 0;
    while (total_bytes_read < gzip_length) {
        bytes_read = read(gzip_fd, (void *)(gzip_data + total_bytes_read),
            gzip_length - total_bytes_read);
        if (bytes_read <= 0) {
            // we rely below on errno being set if this has returned -1
            break;
        } else {
            total_bytes_read += bytes_read;
        }
    }
    if (total_bytes_read < gzip_length) {
        // errno will be set if this condition is true
        _KXKextManagerLogError(aKextManager,
            "error reading kext cache file %s - %s",
            cache_path, strerror(errno));
        result = kKXKextManagerErrorFileAccess;
        goto finish;
    }

    zstream.next_in   = gzip_data;
    zstream.avail_in  = gzip_length;
    zstream.next_out  = data;
    zstream.avail_out = data_length;
    zstream.zalloc    = NULL;
    zstream.zfree     = NULL;
    zstream.opaque    = NULL;

   /* In order to read gzip data, we need to specify the default
    * bit window of 15, and add 32, per the zlib.h comments.
    */
    z_result = inflateInit2(&zstream, 15 + 32);
    if (z_result != Z_OK) {
        _KXKextManagerLogError(aKextManager,
            "error initializing uncompression engine %s",
            cache_path);
        result = kKXKextManagerErrorSerialization;
        goto finish;
    }
    inflate_tried = true;

    num_reallocs = 0;
    while (num_reallocs < MAX_REALLOCS && z_result == Z_OK) {
        z_result = inflate(&zstream, Z_NO_FLUSH);
        if (z_result == Z_STREAM_END) {
            // success! nothing do here, actually
            break;
        } else if ((z_result == Z_OK) || (z_result == Z_BUF_ERROR)) {
            num_reallocs++;
            data_length *= 2;
            data = realloc(data, data_length);
            if (!data) {
                result = kKXKextManagerErrorNoMemory;
                goto finish;
            }
            zstream.next_out  = data + zstream.total_out;
            zstream.avail_out = data_length;
        } else {
            break;
        }
    }
    if (z_result != Z_STREAM_END) {
        _KXKextManagerLogError(aKextManager,
            "error uncompressing kext cache file %s - %s",
            cache_path, zstream.msg ? zstream.msg : "(unknown)");
        result = kKXKextManagerErrorSerialization;
        goto finish;
    }

    fileData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
        (const UInt8 *)data, zstream.total_out, kCFAllocatorNull);
    if (!fileData) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    cacheDictionary = CFPropertyListCreateFromXMLData(kCFAllocatorDefault,
        fileData, kCFPropertyListImmutable, &errorString);
    if (!cacheDictionary) {
        if (errorString) {
            CFIndex bufSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(errorString),
		    kCFStringEncodingUTF8) + sizeof('\0');
            char * error_string = (char *)malloc(bufSize);
            if (!error_string) {
                result = kKXKextManagerErrorNoMemory;
                goto finish;
            }
            if (CFStringGetCString(errorString, error_string,
                    bufSize, kCFStringEncodingUTF8)) {
                _KXKextManagerLogError(aKextManager, "error reading cache data %s: %s",
                    cache_path, error_string);
            }
            free(error_string);
        } else {
            _KXKextManagerLogError(aKextManager, "error reading cache data %s",
                cache_path);
        }
        result = kKXKextManagerErrorCache;
        goto finish;
    }

    if (CFGetTypeID(cacheDictionary) != CFDictionaryGetTypeID()) {
        _KXKextManagerLogError(aKextManager, "cache file %s contains invalid data",
            cache_path);
        result = kKXKextManagerErrorInvalidArgument;
        goto finish;
    }

    result = _KXKextManagerAddRepositoryFromCacheDictionary(
        aKextManager, cacheDictionary, repositoryURL, theRepository);

finish:

    if (inflate_tried) {
        inflateEnd(&zstream);
    }
    if (gzip_fd >= 0) {
        close(gzip_fd);
    }
    if (gzip_data)       free(gzip_data);
    if (data)            free(data);
    if (cache_path)      free(cache_path);
    if (fileData)        CFRelease(fileData);
    if (cacheDictionary) CFRelease(cacheDictionary);
    if (errorString)     CFRelease(errorString);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError _KXKextManagerAddRepositoryFromCacheDictionary(
    KXKextManagerRef aKextManager,
    CFDictionaryRef aRepositoryCache,
    CFURLRef repositoryURL,
    KXKextRepositoryRef * theRepository)
{
    KXKextManagerError result = kKXKextManagerErrorNone;

    CFStringRef repositoryPath = NULL;  // must release
    char repository_path[MAXPATHLEN];

    CFURLRef repositoryAbsURL = NULL;  // must release
    KXKextRepositoryRef foundRepository = NULL;  // don't release
    KXKextRepositoryRef newRepository = NULL;  // must release

    if (theRepository) {
        *theRepository = NULL;
    }

    repositoryAbsURL = PATH_CopyCanonicalizedURL(repositoryURL);
    if (!repositoryAbsURL) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    repositoryPath = CFURLCopyFileSystemPath(repositoryAbsURL,
        kCFURLPOSIXPathStyle);
    if (!repositoryPath) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    if (!CFStringGetFileSystemRepresentation(repositoryPath, repository_path, CFStringGetMaximumSizeOfFileSystemRepresentation(repositoryPath))) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelDetails, NULL, 0,
        "adding repository %s from a cache", repository_path);

   /*****
    * First let's see if a repository has already been registered
    * under the given URL. If one has we can just return that.
    */
    foundRepository = KXKextManagerGetRepositoryForDirectory(
        aKextManager, repositoryAbsURL);
    if (foundRepository) {
        if (theRepository) {
            *theRepository = foundRepository;
        }
        _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelDetails, NULL, 0,
            "repository %s exists", repository_path);

        goto finish;
    }

   /*****
    * We didn't find an existing repository with the given URL,
    * so we have to create it.
    */
    newRepository = _KXKextRepositoryCreate(kCFAllocatorDefault);
    if (!newRepository) {
        result = kKXKextManagerErrorNoMemory;
        goto finish;
    }

    result = _KXKextRepositoryInitWithCache(newRepository,
        aRepositoryCache, repositoryAbsURL, aKextManager);
    if (result != kKXKextManagerErrorNone) {
        goto finish;
    }

    _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelDetails, NULL, 0,
        "added repository %s from cache", repository_path);

    CFArrayAppendValue(aKextManager->repositoryList, newRepository);
    KXKextManagerClearRelationships(aKextManager);

    if (theRepository) {
        *theRepository = newRepository;
    }

finish:
    if (result != kKXKextManagerErrorNone) {
        _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelDetails, NULL, 0,
            "failed to add repository %s from cache", repository_path);
    }

    if (newRepository)    CFRelease(newRepository);
    if (repositoryPath)   CFRelease(repositoryPath);
    if (repositoryAbsURL) CFRelease(repositoryAbsURL);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextManagerClearLoadFailures(KXKextManagerRef aKextManager)
{
    CFIndex count, i;
    // Have each repository clear its load failures
    count = CFArrayGetCount(aKextManager->repositoryList);
    for (i = 0; i < count; i++) {
        KXKextRepositoryRef thisRepository = (KXKextRepositoryRef)
            CFArrayGetValueAtIndex(aKextManager->repositoryList, i);
        _KXKextRepositoryClearLoadFailures(thisRepository);
    }
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void _KXKextManagerRemoveRepository(
    KXKextManagerRef aKextManager,
    KXKextRepositoryRef aRepository)
{
    CFIndex count, i;

    KXKextManagerClearRelationships(aKextManager);
    __KXKextManagerClearDependencyRelationships(aKextManager);

    count = CFArrayGetCount(aKextManager->repositoryList);
    i = count;
    while (1) {
        KXKextRepositoryRef thisRepository = NULL;  // don't release

        i--;
        thisRepository = (KXKextRepositoryRef)CFArrayGetValueAtIndex(
            aKextManager->repositoryList, i);
        if (thisRepository == aRepository) {
            CFArrayRemoveValueAtIndex(aKextManager->repositoryList, i);
        }
        if (i == 0) break;
    }

    return;
}

/*******************************************************************************
********************************************************************************
* MODULE-PRIVATE API BELOW HERE
********************************************************************************
*******************************************************************************/

/*******************************************************************************
*
*******************************************************************************/
static void __km_null_log(const char * format __unused, ...)
{
    return;
}

/*******************************************************************************
*
*******************************************************************************/
static void __km_null_err_log(const char * format __unused, ...)
{
    return;
}

/*******************************************************************************
* Default is to *not* approve.
*******************************************************************************/
static int __km_null_approve(int defaultAnswer __unused, const char * format __unused, ...)
{
    return 0;
}

/*******************************************************************************
* Default is to approve and *not* veto.
*******************************************************************************/
static int __km_null_veto(int defaultAnswer __unused, const char * format __unused, ...)
{
    return 1;
}

/*******************************************************************************
*
*******************************************************************************/
static const char * __km_null_input(const char * format __unused, ...)
{
    return NULL;
}

/*******************************************************************************
*
*******************************************************************************/
static const CFRuntimeClass __KXKextManagerClass = {
    0,                       // version
    "KXKextManager",         // className
    NULL,                    // init
    NULL,                    // copy
    __KXKextManagerReleaseContents, // finalize
    NULL,                    // equal
    NULL,                    // hash
    NULL,                    // copyFormattingDesc
    __KXKextManagerCopyDebugDescription  // copyDebugDesc
};

static void __KXKextManagerInitialize(void)
{
    __kKXKextManagerTypeID = _CFRuntimeRegisterClass(&__KXKextManagerClass);
    return;
}

/*******************************************************************************
*
*******************************************************************************/
static pthread_once_t initialized = PTHREAD_ONCE_INIT;

static __KXKextManagerRef __KXKextManagerCreatePrivate(
    CFAllocatorRef       allocator,
    CFAllocatorContext * context __unused)
{
    __KXKextManagerRef newManager = NULL;
    void * offset = NULL;
    UInt32 size;

    /* initialize runtime */
    pthread_once(&initialized, __KXKextManagerInitialize);

    /* allocate session */
    size  = sizeof(__KXKextManager) - sizeof(CFRuntimeBase);
    newManager = (__KXKextManagerRef)_CFRuntimeCreateInstance(allocator,
        __kKXKextManagerTypeID, size, NULL);
    if (!newManager) {
        return NULL;
    }
    offset = newManager;
    bzero(offset + sizeof(CFRuntimeBase), size);

    return (__KXKextManagerRef)newManager;
}

/*******************************************************************************
*
*******************************************************************************/
static CFStringRef __KXKextManagerCopyDebugDescription(CFTypeRef cf)
{
    CFAllocatorRef     allocator = CFGetAllocator(cf);
    CFMutableStringRef result;

    result = CFStringCreateMutable(allocator, 0);
    CFStringAppendFormat(result, NULL, CFSTR("<KXKextManager %p [%p]> {\n"), cf, allocator);
    // add useful stuff here
    CFStringAppendFormat(result, NULL, CFSTR("}"));

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
void __KXKextManagerReleaseContents(CFTypeRef cf)
{
    KXKextManagerRef aKextManager = (KXKextManagerRef)cf;

    if (aKextManager->repositoryList) {
        CFRelease(aKextManager->repositoryList);
    }
    if (aKextManager->candidateKexts) {
        CFRelease(aKextManager->candidateKexts);
    }
#if 0
    if (aKextManager->loadablePersonalities) {
        CFRelease(aKextManager->loadablePersonalities);
    }
#endif 0
    if (aKextManager->kextsWithMissingDependencies) {
        CFRelease(aKextManager->kextsWithMissingDependencies);
    }

    return;
}

/*******************************************************************************
*
*******************************************************************************/
static void __KXKextManagerClearRelationships(
    KXKextManagerRef aKextManager)
{
    CFIndex count, i;
    // Have each repository clear its dependencies
    count = CFArrayGetCount(aKextManager->repositoryList);
    for (i = 0; i < count; i++) {
        KXKextRepositoryRef thisRepository = (KXKextRepositoryRef)
            CFArrayGetValueAtIndex(aKextManager->repositoryList, i);
        _KXKextRepositoryClearRelationships(thisRepository);
    }

    CFDictionaryRemoveAllValues(aKextManager->candidateKexts);
#if 0
    CFDictionaryRemoveAllValues(aKextManager->loadablePersonalities);
#endif 0
    CFArrayRemoveAllValues(aKextManager->kextsWithMissingDependencies);

    aKextManager->needsClearRelationships = false;
    aKextManager->needsCalculateRelationships = true;

    return;
}

/*******************************************************************************
*
*******************************************************************************/
void __KXKextManagerClearDependencyRelationships(
    KXKextManagerRef aKextManager)
{
    CFIndex count, i;
    // Have each repository clear its dependencies
    count = CFArrayGetCount(aKextManager->repositoryList);
    for (i = 0; i < count; i++) {
        KXKextRepositoryRef thisRepository = (KXKextRepositoryRef)
            CFArrayGetValueAtIndex(aKextManager->repositoryList, i);
        _KXKextRepositoryClearDependencyRelationships(thisRepository);
    }

    CFArrayRemoveAllValues(aKextManager->kextsWithMissingDependencies);

    return;
}
/*******************************************************************************
*
*******************************************************************************/
static Boolean __versionNumberForString(
    CFStringRef aVersionString,
    VERS_version * version)
{
    char vers_buffer[32];  // more than long enough for legal vers

    if (!CFStringGetCString(aVersionString,
        vers_buffer, sizeof(vers_buffer) - 1, kCFStringEncodingUTF8)) {

        return false;
    } else {
        vers_buffer[sizeof(vers_buffer) - 1] = '\0';

        *version = VERS_parse_string(vers_buffer);
        if (*version < 0) {
            return false;
        }
    }

    return true;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextRef KXKextManagerGetLoadedOrLatestKextWithIdentifier(
    KXKextManagerRef aKextManager,
    CFStringRef identifier)
{
    KXKextRef foundKext = NULL;
    KXKextRef checkKext = NULL;

   /*****
    * Make sure we have the candidate kexts arranged with their version
    * relationships. Force a clear of the relationship trees if necessary
    * and then (re)build it as needed.
    */
    if (aKextManager->needsClearRelationships) {
        __KXKextManagerClearRelationships(aKextManager);
    }
    if (aKextManager->needsCalculateRelationships) {
        KXKextManagerCalculateVersionRelationships(aKextManager);
    }

   /*****
    * Check for loaded kext, but don't bail if this doesn't work;
    * we'll just return the latest kext that we can find.
    */
    KXKextManagerCheckForLoadedKexts(aKextManager);

    foundKext = (KXKextRef)CFDictionaryGetValue(aKextManager->candidateKexts,
        identifier);

    checkKext = foundKext;
    while (checkKext) {
        if (KXKextIsLoaded(checkKext)) {
            foundKext = checkKext;
            goto finish;
        } else {
            checkKext = KXKextGetPriorVersionKext(checkKext);
        }
    }

finish:
    return foundKext;
}

/*******************************************************************************
*
*******************************************************************************/
static KXKextRef __KXKextManagerGetKextWithIdentifierAndVersionNumber(
    KXKextManagerRef aKextManager,
    CFStringRef identifier,
    VERS_version * versionNumber)
{
    KXKextRef foundKext = NULL;

   /*****
    * Make sure we have the candidate kexts arranged with their version
    * relationships. Force a clear of the relationship trees if necessary
    * and then (re)build it as needed.
    */
    if (aKextManager->needsClearRelationships) {
        __KXKextManagerClearRelationships(aKextManager);
    }
    if (aKextManager->needsCalculateRelationships) {
        KXKextManagerCalculateVersionRelationships(aKextManager);
    }

    foundKext = (KXKextRef)CFDictionaryGetValue(aKextManager->candidateKexts,
        identifier);

    if (!versionNumber) {
        goto finish;
    }

    while (foundKext) {
        if (_KXKextGetVersion(foundKext) == *versionNumber) {
            goto finish;
        } else {
            foundKext = KXKextGetPriorVersionKext(foundKext);
        }
    }

finish:
    return foundKext;
}

/*******************************************************************************
*
*******************************************************************************/
static KXKextRef __KXKextManagerGetKextWithIdentifierCompatibleWithVersionNumber(
    KXKextManagerRef aKextManager,
    CFStringRef identifier,
    VERS_version * versionNumber)
{
    KXKextRef scanKext = NULL;
    KXKextRef foundKext = NULL;

   /*****
    * Make sure we have the candidate kexts arranged with their version
    * relationships. Force a clear of the relationship trees if necessary
    * and then (re)build it as needed.
    */
    if (aKextManager->needsClearRelationships) {
        __KXKextManagerClearRelationships(aKextManager);
    }
    if (aKextManager->needsCalculateRelationships) {
        KXKextManagerCalculateVersionRelationships(aKextManager);
    }

    scanKext = (KXKextRef)CFDictionaryGetValue(aKextManager->candidateKexts,
        identifier);

    if (!versionNumber) {
        foundKext = scanKext;
        goto finish;
    }

    while (scanKext) {

        if (_KXKextIsCompatibleWithVersionNumber(scanKext, *versionNumber)) {

           /* If we find a compatible kext that's loaded, we're done!
            * Otherwise if we hadn't yet found a compatible kext, note
            * this one but keep looking in case there's a loaded but
            * earlier compatible version.
            */
            if (KXKextIsLoaded(scanKext)) {
                foundKext = scanKext;
                goto finish;
            } else if (!foundKext) {
                foundKext = scanKext;
            }
        }
        scanKext = KXKextGetPriorVersionKext(scanKext);
    }

finish:
    return foundKext;
}

/*******************************************************************************
*
*******************************************************************************/
static Boolean __KXKextManagerCheckForRepositoryCache(
    KXKextManagerRef aKextManager,
    CFURLRef aDirectory,
    CFURLRef * cacheURL, // out param
    Boolean * isCurrent,
    Boolean * canUpdate)
{
    Boolean result = false;
    CFURLRef absURL = NULL;        // must release
    CFURLRef cacheDirURL = NULL;   // must release
    CFURLRef theCacheURL = NULL;   // must release

    char * repository_path = NULL; // must free
    char * cache_dir_path = NULL;  // must free
    char * cache_path = NULL;      // must free

    struct stat repository_stat;
    struct stat cache_dir_stat;
    struct stat cache_stat;

    Boolean no_cache_dir = false;

    if (cacheURL) {
        *cacheURL = NULL;
    }

    if (isCurrent) {
        *isCurrent = false;
    }

    if (canUpdate) {
        *canUpdate = false;
    }

    if (!aDirectory) {
        goto finish;
    }

    absURL = PATH_CopyCanonicalizedURLAndSetDirectory(
        aDirectory, false);
    if (!absURL) {
        goto finish;
    }

    cacheDirURL = CFURLCreateCopyAppendingPathComponent(
        kCFAllocatorDefault, absURL,
        CFSTR(_kKXKextCachesFolderName),
        true /* isDirectory */);
    if (!cacheDirURL) {
        goto finish;
    }

    theCacheURL = CFURLCreateCopyAppendingPathComponent(
        kCFAllocatorDefault, cacheDirURL,
        CFSTR(_kKXKextPlistCacheFilename),
        false /* isDirectory */);
    if (!theCacheURL) {
        goto finish;
    }

    repository_path = PATH_CanonicalizedCStringForURL(absURL);
    if (!repository_path) {
        goto finish;
    }

    cache_dir_path = PATH_CanonicalizedCStringForURL(cacheDirURL);
    if (!cache_dir_path) {
        goto finish;
    }

    cache_path = PATH_CanonicalizedCStringForURL(theCacheURL);
    if (!cache_path) {
        goto finish;
    }

   /* Check the repository directory first. If it doesn't exist we have more
    * serious problems, but the caller will have to deal with that.
    */
    if (stat(repository_path, &repository_stat) != 0) {
        if (errno == ENOENT || errno == ENOTDIR) {
            result = false;
            goto finish;
        } else if (errno == EACCES) {
            _KXKextManagerLogError(aKextManager,
                "%s: cannot check existence; permission denied", repository_path);
            result = false;
            goto finish;
        } else {
            result = false;
            goto finish;
        }
    }

    if (!(repository_stat.st_mode & S_IFDIR)) {
            _KXKextManagerLogError(aKextManager,
                "%s is not a directory", repository_path);
            result = false;
            goto finish;
    }

   /* Check the cache directory.
    */
    if (stat(cache_dir_path, &cache_dir_stat) != 0) {
        if (errno == ENOENT) {
            // We may be able to create it.
            result = false;
            no_cache_dir = true;
            goto check_update;
        } else if (errno == EACCES) {
            _KXKextManagerLogError(aKextManager,
                "%s: cannot check existence; permission denied", cache_dir_path);
            result = false;
            goto finish;
        } else {
            result = false;
            goto finish;
        }
    }

    if (!(cache_dir_stat.st_mode & S_IFDIR)) {
            _KXKextManagerLogError(aKextManager,
                "%s is not a directory", cache_dir_path);
            result = false;
            goto finish;
    }

   /* Check the cache file. If it doesn't exist we know the result will
    * be false. We may still have to check whether a cache can be
    * created, however.
    */
    if (stat(cache_path, &cache_stat) != 0) {
        if (errno == ENOENT || errno == ENOTDIR) {
            result = false;
            goto check_update;
        } else if (errno == EACCES) {
            _KXKextManagerLogError(aKextManager,
                "%s: cannot check existence; permission denied", cache_path);
            result = false;
            // won't be able to check if can update
            goto finish;
        } else {
            result = false;
            // won't be able to check if can update
            goto finish;
        }
    }

   /* Now see if the cache file is a regular file. If it is, then check
    * modification dates and permissions.
    */
    if (cache_stat.st_mode & S_IFREG) {
        result = true;
        if (isCurrent) {
            if (cache_stat.st_mtime == (repository_stat.st_mtime + 1)) {
                *isCurrent = true;
            } 
        }
    } else {
        _KXKextManagerLogError(aKextManager,
            "cache file %s is not a regular file",
            cache_path);
        result = false;
        goto finish;
    }

check_update:

    if ((access(cache_dir_path, W_OK) == 0) ||
        (no_cache_dir && (access(repository_path, W_OK) == 0))) {

        if (canUpdate) {
            *canUpdate = true;
        }
    }

finish:
    if (absURL)      CFRelease(absURL);
    if (cacheDirURL) CFRelease(cacheDirURL);

    if (repository_path) free(repository_path);
    if (cache_dir_path)  free(cache_dir_path);
    if (cache_path)      free(cache_path);

    if (cacheURL) {
        if (result) {
            *cacheURL = theCacheURL;
        } else {
            CFRelease(theCacheURL);
        }
    }
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
static int __KXKextManagerCheckPersonalityForSafeBoot(
    KXKextManagerRef aKextManager,
    CFDictionaryRef aPersonality,
    const char * personality_name)
{
    int result = 1;
    CFTypeRef rawValue = NULL;
    CFNumberRef number = NULL;
    long long int numValue = 0;

    rawValue = CFDictionaryGetValue(aPersonality, CFSTR("IOKitDebug"));
    if (!rawValue) {
        goto finish;
    }

    number = (CFNumberRef)rawValue;
    if (!CFNumberGetValue(number, kCFNumberLongLongType, &numValue)) {
        result = 0;
        goto finish;
    }

    if (numValue != 0) {
        _KXKextManagerLogMessageAtLevel(aKextManager, kKXKextManagerLogLevelBasic, NULL, 0,
            "safe boot mode active: "
            "personality \"%s\" has a nonzero IOKitDebug value "
            "and will not be sent to the kernel",
            personality_name);

        result = 0;
        goto finish;
    }

finish:
    return result;
}

#endif // !__LP64__
