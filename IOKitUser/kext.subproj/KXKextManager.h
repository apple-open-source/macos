/*!
 * @header KXKextManager
 * KXKextManager maintains a database of kernel extensions (kexts),
 * allowing kexts to be retrieved by their filesystem location,
 * CFBundleIdentifier, and other attributes. If the current process
 * is running as the superuser, it can also use the kext manager to
 * load kernel extensions and to send I/O Kit personalities to the
 * kernel in order to trigger driver matching.
 * 
 * KXKextManager organizes its kernel extensions in two ways. The
 * first is by collections of kexts, which are known as repositories.
 * The only kind of repository currently supported is a directory
 * in the filesystem containing kext bundles; others may be added
 * in the future. There are functions for adding and removing
 * repositories, which scan their directories for kernel extension
 * bundles, as well as functions for adding and removing
 * individual kernel extensions; the latter automatically add a
 * repository as needed, but this repository doesn't scan its directory
 * for other kernel extensions.
 *
 * The second way that KXKextManager organizes extensions spans all
 * of the known repositories, collecting kexts into a dictionary
 * keyed by CFBundleIdentifier. This allows any kext to be found for
 * a given identifier. If there are multiple bundles for a given
 * identifier, the first kext under the identifier has accessor
 * functions for the next prior version and for any duplicate versions.
 *
 * other settings: safeboot, loadintask, performtests, logLevel, userInput
 *
 * heading: using kextmanager to load kexts
 * heading: using kextmanager to inspect & manage kexts
 */


#ifndef __KXKEXTMANAGER_H__
#define __KXKEXTMANAGER_H__

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
*
*******************************************************************************/
#include <CoreFoundation/CoreFoundation.h>

typedef struct __KXKextManager * KXKextManagerRef;

typedef enum {

    kKXKextManagerErrorNone = 0,

    // General errors
    kKXKextManagerErrorUnspecified,
    kKXKextManagerErrorInvalidArgument,
    kKXKextManagerErrorNoMemory,
    kKXKextManagerErrorFileAccess,
    kKXKextManagerErrorNotADirectory,
    kKXKextManagerErrorDiskFull,
    kKXKextManagerErrorSerialization,
    kKXKextManagerErrorCompression,
    kKXKextManagerErrorIPC,
    kKXKextManagerErrorChildTask,
    kKXKextManagerErrorUserAbort,

    // Kernel access errors
    kKXKextManagerErrorKernelError,
    kKXKextManagerErrorKernelResource,
    kKXKextManagerErrorKernelPermission,

    // Kext cache error
    kKXKextManagerErrorCache,

    // Kext validity/eligibility errors
    kKXKextManagerErrorKextNotFound,
    kKXKextManagerErrorURLNotInRepository,
    kKXKextManagerErrorNotABundle,            // check kext for specifics
    kKXKextManagerErrorNotAKext,              // no ".kext" extension, type not "KEXT"
    kKXKextManagerErrorValidation,            // check kext for specifics
    kKXKextManagerErrorBootLevel,             // ineligible during safe boot
    kKXKextManagerErrorDisabled,

    // Dependency resolution and other load-time errors
    // These may be triggered by a dependency of the kext being loaded
    kKXKextManagerErrorAuthentication,        // check kext(s) for specifics
    kKXKextManagerErrorDependency,            // check kext(s) for specifics
    kKXKextManagerErrorDependencyLoop,        // potential loop
    kKXKextManagerErrorLoadExecutableBad,     // not a kmod
    kKXKextManagerErrorLoadExecutableNoArch,  // has no arch for this computer
    kKXKextManagerErrorAlreadyLoaded,
    kKXKextManagerErrorLoadedVersionDiffers,  // kext itself
    kKXKextManagerErrorDependencyLoadedVersionDiffers,  // kext dependency
    kKXKextManagerErrorLinkLoad, 
    
    // 
    kKXKextManagerErrorKextHasNoReceipt,
    kKXKextManagerErrorKextIsModified

} KXKextManagerError;

typedef enum {
    kKXKextManagerLogLevelSilent      = -2,   // no notices, no errors
    kKXKextManagerLogLevelErrorsOnly  = -1,
    kKXKextManagerLogLevelDefault     = 0,
    kKXKextManagerLogLevelBasic       = 1,
    kKXKextManagerLogLevelLoadBasic   = 2,
    kKXKextManagerLogLevelDetails     = 3,
    kKXKextManagerLogLevelKexts       = 4,
    kKXKextManagerLogLevelKextDetails = 5,
    kKXKextManagerLogLevelLoadDetails = 6
} KXKextManagerLogLevel;

#include "KXKext.h"
#include "KXKextRepository.h"


#define kKXSystemExtensionsFolder        (CFSTR("/System/Library/Extensions"))
#define kKXKextRepositoryCacheExtension  (CFSTR("kextcache"))

CFStringRef  KXKextManagerErrorStringForError(KXKextManagerError error);
const char * KXKextManagerErrorStaticCStringForError(KXKextManagerError error);

// FIXME: Add API for adding the standard Extensions folder to the manager.
// FIXME: Add API for emitting debug symbols on load.

CFTypeID KXKextManagerGetTypeID(void);

KXKextManagerRef KXKextManagerCreate(CFAllocatorRef alloc);

KXKextManagerError KXKextManagerInit(
    KXKextManagerRef aKextManager,
    Boolean loadInTaskFlag,
    Boolean safeBootFlag);

// log level 0 (none, default) to 6 (a whole lot!)
void KXKextManagerSetLogLevel(
    KXKextManagerRef aKextManager,
    SInt32 level);
SInt32 KXKextManagerGetLogLevel(
    KXKextManagerRef aKextManager);
void KXKextManagerSetLogFunction(
    KXKextManagerRef aKextManager,
    void (*func)(const char * format, ...));
void KXKextManagerSetErrorLogFunction(
    KXKextManagerRef aKextManager,
    void (*func)(const char * format, ...));
void KXKextManagerSetUserApproveFunction(
    KXKextManagerRef aKextManager,
    int (*func)(int defaultAnswer, const char * format, ...));
void KXKextManagerSetUserVetoFunction(
    KXKextManagerRef aKextManager,
    int (*func)(int defaultAnswer, const char * format, ...));
void KXKextManagerSetUserInputFunction(
    KXKextManagerRef aKextManager,
    const char * (*func)(const char * format, ...));

Boolean KXKextManagerGetSafeBootMode(KXKextManagerRef aKextManager);
void KXKextManagerSetSafeBootMode(KXKextManagerRef aKextManager,
    Boolean flag);

Boolean KXKextManagerGetPerformLoadsInTask(KXKextManagerRef aKextManager);
void KXKextManagerSetPerformLoadsInTask(KXKextManagerRef aKextManager,
    Boolean flag);

// Set this before adding any repositories or kexts.
Boolean KXKextManagerPerformsFullTests(KXKextManagerRef aKextManager);
void KXKextManagerSetPerformsFullTests(KXKextManagerRef aKextManager,
    Boolean flag);

/*****
 * Adding or removing a repository invalidates all calculated
 * relationship info among currently known kexts. This info
 * must be recalculated using KXKextManagerCalculateVersionRelationships()
 * before you can do any further work with the kext manager.
 */
KXKextManagerError KXKextManagerAddRepositoryDirectory(
    KXKextManagerRef aKextManager,
    CFURLRef directoryURL,
    Boolean scanForKexts,
    Boolean useCache,
    KXKextRepositoryRef * theRepository);  // out param

void KXKextManagerRemoveRepositoryDirectory(
    KXKextManagerRef aKextManager,
    CFURLRef directoryURL);

CFArrayRef KXKextManagerGetRepositories(KXKextManagerRef aKextManager);
KXKextRepositoryRef KXKextManagerGetRepositoryForDirectory(
    KXKextManagerRef aKextManager,
    CFURLRef aDirectory);

void KXKextManagerResetAllRepositories(KXKextManagerRef aKextManager);

// Both of the functions below cause a ClearRelationships of all kexts
// if a kext is added or removed!

// Finds or creates repository for given kext URL and adds kext
// to that repository if necessary.
KXKextManagerError KXKextManagerAddKextWithURL(
    KXKextManagerRef aKextManager,
    CFURLRef kextURL,
    Boolean includePlugins,
    KXKextRef * theKext);  // out param
void KXKextManagerRemoveKext(
    KXKextManagerRef aKextManager,
    KXKextRef aKext);

// Rebuilds kext from disk. Causes a ClearRelationships of all kexts!
// FIXME: Does this f'n belong in KXKextRepository, in KXKext, or even in
// FIXME: ... KXKextManager?
KXKextManagerError KXKextManagerRescanKext(
    KXKextManagerRef aKextManager,
    KXKextRef aKext,   // INVALID after this call unless retained; even then not in mgr any more
    Boolean scanForPlugins,
    KXKextRef * rescannedKext);  // the newly rescanned kext

void KXKextManagerDisqualifyKext(KXKextManagerRef aKextManager,
    KXKextRef aKext);
void KXKextManagerRequalifyKext(KXKextManagerRef aKextManager,
    KXKextRef aKext);

// Builds arrays & linked lists of loadable kexts and personalities,
// and resolves dependencies.
void KXKextManagerCalculateVersionRelationships(KXKextManagerRef aKextManager);
void KXKextManagerResolveAllKextDependencies(KXKextManagerRef aKextManager);

// Clears all current/dependency info in kexts and rebuilds dictionary
// of loadable kexts and personalities to reflect possible new or removed
// versions of kexts. A repository must invoke this whenever a kext is
// added or removed.
void KXKextManagerClearRelationships(KXKextManagerRef aKextManager);
void KXKextManagerEnableClearRelationships(KXKextManagerRef aKextManager);
void KXKextManagerDisableClearRelationships(KXKextManagerRef aKextManager);

KXKextRepositoryRef KXKextManagerGetRepositoryWithURL(
    KXKextManagerRef aKextManager,
    CFURLRef anURL);
KXKextRepositoryRef KXKextManagerGetRepositoryForKextWithURL(
    KXKextManagerRef aKextManager,
    CFURLRef anURL);

KXKextRef KXKextManagerGetKextWithURL(
    KXKextManagerRef aKextManager,
    CFURLRef anURL);
KXKextRef KXKextManagerGetKextWithIdentifier(KXKextManagerRef aKextManager,
    CFStringRef identifier);
KXKextRef KXKextManagerGetLoadedOrLatestKextWithIdentifier(
    KXKextManagerRef aKextManager,
    CFStringRef identifier);
KXKextRef KXKextManagerGetKextWithIdentifierAndVersionString(
    KXKextManagerRef aKextManager,
    CFStringRef identifier,
    CFStringRef versionString);

// If a kext compatible with the requested version is already loaded,
// that is given preference over any later compatible versions, in
// order to prevent unneccessarily loading lots of different versions
// of a kext.
KXKextRef KXKextManagerGetKextWithIdentifierCompatibleWithVersionString(
    KXKextManagerRef aKextManager,
    CFStringRef identifier,
    CFStringRef versionString);

CFArrayRef KXKextManagerCopyKextsWithIdentifier(KXKextManagerRef aKextManager,
    CFStringRef identifier);

// Creates an array of all known kexts, valid or not. Useful for kext
// manager utilities to display all installed kexts.
CFArrayRef KXKextManagerCopyAllKexts(KXKextManagerRef aKextManager);

CFArrayRef KXKextManagerGetKextsWithMissingDependencies(
    KXKextManagerRef aKextManager);

CFArrayRef KXKextManagerCopyAllKextPersonalities(
    KXKextManagerRef aKextManager);

/*****
 * Normally the manager only authenticates during a load request. These
 * functions perform authentication on all kexts, or mark them all
 * authentic in order to bypass the checks (such as when testing a
 * driver during development).
 */
void KXKextManagerAuthenticateKexts(KXKextManagerRef aKextManager);
void KXKextManagerMarkKextsAuthentic(KXKextManagerRef aKextManager);

Boolean KXKextManagerPerformsStrictAuthentication(
    KXKextManagerRef aKextManager);
void KXKextManagerSetPerformsStrictAuthentication(
    KXKextManagerRef aKextManager,
    Boolean flag);

void KXKextManagerVerifyIntegrityOfAllKexts(KXKextManagerRef aKextManager);

/*******************************************************************************
* any load request blows away all existing dependency info in
* order to calculate the most up-to-date dependency info available
*******************************************************************************/
KXKextManagerError KXKextManagerCheckForLoadedKexts(
    KXKextManagerRef aKextManager);

KXKextManagerError KXKextManagerLoadKext(
    KXKextManagerRef aKextManager,
    KXKextRef aKext);
KXKextManagerError KXKextManagerLoadKextWithIdentifier(
    KXKextManagerRef aKextManager,
    CFStringRef identifier);
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
    int     interactive_level,      // confirm each step of load with user
    Boolean ask_overwrite_symbols,  // used for overwriting symbol/patch files
    Boolean overwrite_symbols,      // used for overwriting symbol/patch files
    Boolean get_addrs_from_kernel,
    unsigned int num_addresses,  // lack of final 2 args causes request
                                 // for user input if (symbol_dir && !do_load)
    char ** addresses);


KXKextManagerError KXKextManagerSendKextPersonalitiesToCatalog(
    KXKextManagerRef aKextManager,
    KXKextRef aKext,
    CFArrayRef personalityNames /* optional */,
    Boolean interactive,
    Boolean safeBoot);
KXKextManagerError KXKextManagerSendPersonalitiesToCatalog(
    KXKextManagerRef aKextManager,
    CFArrayRef personalities);

void KXKextManagerRemoveKextPersonalitiesFromCatalog(
    KXKextManagerRef aKextManager,
    KXKextRef aKext);
KXKextManagerError KXKextManagerRemovePersonalitiesFromCatalog(
    KXKextManagerRef aKextManager,
    CFDictionaryRef matchingPersonality);

/*******************************************************************************
********************************************************************************
*******************************************************************************/
#if 0
// Used by client code to retrieve kexts by various criteria.
//
// Used to handle kernel requests for personalities.
CFArrayRef KXKextManagerCopyPersonalitiesForClassMatch(
    KXKextManagerRef aKextManager,
    CFStringRef classMatchString);

#endif 0

#ifdef __cplusplus
}
#endif

#endif __KXKEXTMANAGER_H__
