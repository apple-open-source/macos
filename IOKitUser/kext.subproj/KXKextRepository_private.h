#ifndef __KXKEXTREPOSITORY_PRIVATE_H__
#define __KXKEXTREPOSITORY_PRIVATE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "KXKextRepository.h"
#include "KXKextManager_private.h"
#include "KXKext_private.h"

#define _CACHE_VERSION_KEY  CFSTR("KextCacheVersion")
#define _CACHE_PATH_KEY     CFSTR("RepositoryPath")
#define _CACHE_SCANS_KEY    CFSTR("ScansForKexts")
#define _CACHE_KEXTS_KEY    CFSTR("Kexts")


/*******************************************************************************
* This file is for declaring private  API used by code other than
* kextrepository.c, which must therefore be visible to other files within the
* framework.
*******************************************************************************/

KXKextRepositoryRef _KXKextRepositoryCreate(CFAllocatorRef alloc);

KXKextManagerError _KXKextRepositoryInitWithDirectory(
    KXKextRepositoryRef aRepository,
    CFURLRef aDirectory,
    Boolean scanDirectory,  // sets scansForKexts
    KXKextManagerRef aManager);

KXKextManagerError _KXKextRepositoryInitWithCache(
    KXKextRepositoryRef aRepository,
    CFDictionaryRef     aDictionary,
    KXKextManagerRef    aManager);

const char * _KXKextRepositoryCopyCanonicalPathnameAsCString(
    KXKextRepositoryRef aRepository);

CFArrayRef _KXKextRepositoryGetCandidateKexts(KXKextRepositoryRef aRepository);
CFArrayRef _KXKextRepositoryGetBadKexts(KXKextRepositoryRef aRepository);

KXKextManagerError _KXKextRepositoryScanDirectoryForKexts(
    KXKextRepositoryRef aRepository,
    CFURLRef     aDirectory,
    CFArrayRef   existingKexts,
    CFArrayRef * addedKexts,
    CFArrayRef * removedKexts,
    CFArrayRef * notKexts);

void _KXKextRepositoryAddKext(
    KXKextRepositoryRef aRepository,
    KXKextRef aKext);
void _KXKextRepositoryAddKexts(
    KXKextRepositoryRef aRepository,
    CFArrayRef kextArray);

void _KXKextRepositoryAddBadKext(
    KXKextRepositoryRef aRepository,
    KXKextRef aKext);
void _KXKextRepositoryAddBadKexts(
    KXKextRepositoryRef aRepository,
    CFArrayRef kextArray);

void _KXKextRepositoryRemoveKext(
    KXKextRepositoryRef aRepository,
    KXKextRef aKext);
void _KXKextRepositoryDisqualifyKext(
    KXKextRepositoryRef aRepository,
    KXKextRef aKext);
void _KXKextRepositoryRequalifyKext(
    KXKextRepositoryRef aRepository,
    KXKextRef aKext);

// Clears all current/dependency info in kexts. The kext manager
// invokes this whenever a repository or kext is added or removed.
void _KXKextRepositoryClearRelationships(KXKextRepositoryRef aRepository);
void _KXKextRepositoryClearDependencyRelationships(
    KXKextRepositoryRef aRepository);

void _KXKextRepositoryMarkKextsNotLoaded(KXKextRepositoryRef aRepository);

void _KXKextRepositoryClearLoadFailures(KXKextRepositoryRef aRepository);

CFDictionaryRef _KXKextRepositoryCopyCacheDictionary(
    KXKextRepositoryRef aRepository);

Boolean _KXKextRepositoryInvalidateCaches(
    KXKextRepositoryRef aRepository);

#if 0

// Unpacks the mkext archive into a temporary directory and
// initializes a repository from it.
//
KXKextManagerError KXKextRepositoryInitFromMultikextArchive(
    KXKextRepositoryRef aRepository,
    char * archive,
    KXKextManagerRef aManager);

#endif 0

#ifdef __cplusplus
}
#endif

#endif __KXKEXTREPOSITORY_PRIVATE_H__
