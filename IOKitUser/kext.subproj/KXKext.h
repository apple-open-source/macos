#ifndef __KXKEXT_H__
#define __KXKEXT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <mach/mach.h>
#include <mach/kmod.h>
#include <mach-o/kld.h>
#include <mach-o/fat.h>

#include <CoreFoundation/CoreFoundation.h>

typedef struct __KXKext * KXKextRef;

#include "KXKextManager.h"
#include "KXKextRepository.h"

typedef enum {
    kKXKextLogLevelNone    = 0,
    kKXKextLogLevelBasic   = 1,
    kKXKextLogLevelDetails = 2
} KXKextLogLevel;

typedef enum {
    kKXKextIntegrityUnknown = 0,
    kKXKextIntegrityCorrect,
    kKXKextIntegrityKextIsModified,
    kKXKextIntegrityNoReceipt,
	kKXKextIntegrityNotApple
} KXKextIntegrityState;

/*******************************************************************************
*
*******************************************************************************/
CFTypeID KXKextGetTypeID(void);

CFBundleRef     KXKextGetBundle(KXKextRef aKext);
CFDictionaryRef KXKextGetInfoDictionary(KXKextRef aKext);
Boolean KXKextIsFromCache(KXKextRef aKext);

KXKextRepositoryRef KXKextGetRepository(KXKextRef aKext);
KXKextManagerRef    KXKextGetManager(KXKextRef aKext);

KXKextRef KXKextGetPriorVersionKext(KXKextRef aKext);
KXKextRef KXKextGetDuplicateVersionKext(KXKextRef aKext);

CFStringRef KXKextGetBundlePathInRepository(KXKextRef aKext);
CFStringRef KXKextGetBundleDirectoryName(KXKextRef aKext);
CFStringRef KXKextCopyAbsolutePath(KXKextRef aKext);
CFURLRef    KXKextGetAbsoluteURL(KXKextRef aKext);

Boolean     KXKextGetIsKernelResource(KXKextRef aKext);
Boolean     KXKextGetDeclaresExecutable(KXKextRef aKext);

CFArrayRef KXKextGetPlugins(KXKextRef aKext);
Boolean    KXKextIsAPlugin(KXKextRef aKext);
KXKextRef  KXKextGetContainerForPluginKext(KXKextRef aKext);

Boolean KXKextIsValid(KXKextRef aKext);
// This may be NULL
CFMutableDictionaryRef KXKextGetValidationFailures(KXKextRef aKext);

Boolean                KXKextHasDebugProperties(KXKextRef aKext);
Boolean                KXKextIsEligibleDuringSafeBoot(KXKextRef aKext);
Boolean                KXKextIsEnabled(KXKextRef aKext);

CFStringRef            KXKextGetBundleIdentifier(KXKextRef aKext);

CFDictionaryRef        KXKextGetBundleLibraryVersions(KXKextRef aKext);
Boolean KXKextIsCompatibleWithVersionString(
    KXKextRef aKext,
    CFStringRef aVersionString);

Boolean         KXKextHasPersonalities(KXKextRef aKext);
CFDictionaryRef KXKextCopyPersonalities(KXKextRef aKext);
CFArrayRef      KXKextCopyPersonalitiesArray(KXKextRef aKext);

KXKextIntegrityState KXKextGetIntegrityState(KXKextRef aKext);

Boolean            KXKextHasBeenAuthenticated(KXKextRef aKext);
KXKextManagerError KXKextAuthenticate(KXKextRef aKext);
KXKextManagerError KXKextMarkAuthentic(KXKextRef aKext);
Boolean            KXKextIsAuthentic(KXKextRef aKext);
// This may be NULL
CFMutableDictionaryRef KXKextGetAuthenticationFailures(KXKextRef aKext);

KXKextManagerError KXKextResolveDependencies(KXKextRef aKext);
Boolean            KXKextGetHasAllDependencies(KXKextRef aKext);

// The results for these functions are valid only after calling
// KXKextManagerResolveDependenciesForKext(). Otherwise they all
// return NULL.
CFArrayRef      KXKextGetDirectDependencies(KXKextRef aKext);
CFDictionaryRef KXKextGetMissingDependencyErrors(KXKextRef aKext);

// These aren't really useful, are they? Would a manager util
// want them?
CFMutableArrayRef KXKextCopyAllDependencies(KXKextRef aKext);
CFMutableArrayRef KXKextCopyIndirectDependencies(KXKextRef aKext);

// Finds all kexts that depend on aKext within aKext's
// manager.
CFMutableArrayRef KXKextCopyAllDependents(KXKextRef aKext);

// This may be NULL
CFMutableDictionaryRef KXKextGetWarnings(KXKextRef aKext);

Boolean KXKextIsLoadable(KXKextRef aKext, Boolean safeBoot);

// isLoaded is the same value for all kexts of a given {id, version}.
// There's no way to determine from an {id, version} which bundle in
// the filesystem actually got loaded, so the kext manager just marks
// all duplicates as loaded.
Boolean KXKextIsLoaded(KXKextRef aKext);
Boolean KXKextOtherVersionIsLoaded(KXKextRef aKext);

vm_address_t KXKextGetStartAddress(KXKextRef aKext);

Boolean KXKextGetLoadFailed(KXKextRef aKext);
void KXKextSetLoadFailed(KXKextRef aKext, Boolean flag);

/* KXKextPrintDiagnostics() prints all errors and warnings. */
void KXKextPrintDiagnostics(KXKextRef aKext, FILE * stream);
void KXKextPrintWarnings(KXKextRef aKext, FILE * stream);

/*******************************************************************************
********************************************************************************
* NOT YET IMPLEMENTED
********************************************************************************
*******************************************************************************/
#if 0
// not implemented; we have no on-disk mechanism for this yet.
// A simple list within the repository directory would suffice,
// or even entries put into the kext cache dictionaries
KXKextManagerError KXKextSetEnabled(KXKextRef aKext, Boolean flag);

#endif 0

/*******************************************************************************
* ERROR DICTIONARY KEYS AND VALUES
*******************************************************************************/

#ifndef _KEXT_KEYS

/*****
 * VALIDATION ERRORS
 */

// Can't read bundle files; value is irrelevant.
// FIXME: Make this an array of files that can't be accessed.
extern const CFStringRef kKXKextErrorKeyFileAccess;

// The bundle was initialized in a repository with an URL that
// isn't in that repository.
extern const CFStringRef kKXKextErrorKeyBundleNotInRepository;

// Bundle can't be created, or generic structure error; value is irrelevant.
extern const CFStringRef kKXKextErrorKeyNotABundle;

// Bundle doesn't end in ".kext".
extern const CFStringRef kKXKextErrorKeyNotAKextBundle;

// Generic property list error that can't be narrowed down; value is irrelevant.
extern const CFStringRef kKXKextErrorKeyBadPropertyList;

// One or more required properties are missing (or for a collection,
// the collection has no contents); value is a CFArray of the missing
// property keys.
// Properties nested within dictionaries or arrays are indicated
// as key:key:key or as key:[index]:key.
extern const CFStringRef kKXKextErrorKeyMissingProperty;

// Some value is an illegal type (<real> in a personality), or the wrong type
// (probe score as a string, for example); value for this key is an array
// of property key paths. Properties nested within dictionaries or arrays are
// indicated as key:key:key or as key:[index]:key.
extern const CFStringRef kKXKextErrorKeyPropertyIsIllegalType;

// Some properties have specific values, such as type being "KEXT".
// This key represents an illegal value; the value for this key
// is an array of the errant properties' keys.
// Properties nested within dictionaries or arrays are indicated
// as key:key:key or as key:[index]:key.
extern const CFStringRef kKXKextErrorKeyPropertyIsIllegalValue;

// A kext's CFBundleIdentifier and CFBundleVersion strings may
// not be longer than KMOD_MAX_NAME, as defined in <mach/kmod.h>.
extern const CFStringRef kKXKextErrorKeyIdentifierOrVersionTooLong;

// The IOKitPersonalities property must be a dictionary of dictionaries.
// If the property isn't a dictionary at all, then the illegal type
// error above applies; otherwise, if every value in the dictionary is
// not itself a dictionary, this error applies. The value is irrelevant.
extern const CFStringRef kKXKextErrorKeyPersonalitiesNotNested;

// The info dictionary names an executable named but it can't be found;
// value is irrelevant.
extern const CFStringRef kKXKextErrorKeyMissingExecutable;

// value is irrelevant.
extern const CFStringRef kKXKextErrorKeyCompatibleVersionLaterThanVersion;

// These are expensive to determine....
extern const CFStringRef kKXKextErrorKeyExecutableBad;
extern const CFStringRef kKXKextErrorKeyBundleIdentifierMismatch; // plist vs binary
// value is true
extern const CFStringRef kKXKextErrorKeyBundleVersionMismatch;    // plist vs binary
// value is true
extern const CFStringRef kKXKextErrorKeyExecutableBadArch; // Executable present but
                                                    //  not for this arch.


/*****
 * AUTHENTICATION ERRORS
 */

// Couldn't get any info to perform authentication.
extern const CFStringRef kKXKextErrorKeyStatFailure;

// Couldn't get any info to perform authentication.
extern const CFStringRef kKXKextErrorKeyFileNotFound;

// One or more files is not owned by root with proper permissions.
// Value is an array of filenames relative to the bundle directory.
extern const CFStringRef kKXKextErrorKeyOwnerPermission;

// The bundle's calculated checksum doesn't match the recorded checksum;
// value is irrelevant.
extern const CFStringRef kKXKextErrorKeyChecksum;

// The bundle's signature could not be authenticated; value is irrelevant.
extern const CFStringRef kKXKextErrorKeySignature;


/*****
 * DEPENDENCY ERRORS
 */

// One or more dependencies could not be resolved. Value is a dictionary
// whose keys are CFBundleIdentifiers of the dependencies and values are
// CFString constants as listed below, giving the nature of the missing
// dependency.
extern const CFStringRef kKXKextErrorKeyDependenciesUnresolvable;

/*****
 * DEPENDENCY ERROR INDICATORS
 *
 * These constants are used as values in the dictionary of missing
 * dependencies to indicate the nature of each missing dependency.
 */

// No version of the named dependency could be found.
extern const CFStringRef kKXKextDependencyUnavailable;

// A compatible version of a named dependency could not be found.
extern const CFStringRef kKXKextDependencyNoCompatibleVersion;

// The only versions of the named dependency found don't declare
// a compatible version property.
extern const CFStringRef kKXKextDependencyCompatibleVersionUndeclared;

// All of the kext's direct dependencies were found, but one of those
// has dependencies that could not be resolved.
extern const CFStringRef kKXKextIndirectDependencyUnresolvable;

// Either a circular reference was definitely found, or the dependency
// stack got ridiculously deep (over 255).
extern const CFStringRef kKXKextDependencyCircularReference;

#endif _KEXT_KEYS

#ifdef __cplusplus
}
#endif

#endif __KXKEXT_H__
