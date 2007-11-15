#if !__LP64__

#ifndef __KXKEXTREPOSITORY_H__
#define __KXKEXTREPOSITORY_H__

#include <sys/cdefs.h>

__BEGIN_DECLS


#include <CoreFoundation/CoreFoundation.h>

typedef struct __KXKextRepository * KXKextRepositoryRef;

#include "KXKext.h"
#include "KXKextManager.h"

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerRef KXKextRepositoryGetManager(KXKextRepositoryRef aRepository);
CFStringRef KXKextRepositoryGetPath(KXKextRepositoryRef aRepository);
CFURLRef KXKextRepositoryCopyURL(KXKextRepositoryRef aRepository);

// turning on scansForKexts when it was off immediately causes a
// reset; the return value is the result of the reset
Boolean KXKextRepositoryGetScansForKexts(KXKextRepositoryRef aRepository);
KXKextManagerError KXKextRepositorySetScansForKexts(
    KXKextRepositoryRef aRepository,
    Boolean flag);
Boolean KXKextRepositoryGetNeedsReset(KXKextRepositoryRef aRepository);
void KXKextRepositorySetNeedsReset(
    KXKextRepositoryRef aRepository,
    Boolean flag);

void KXKextRepositoryResolveBadKextDependencies(KXKextRepositoryRef aRepository);

// Empties all kexts from the repository.
void KXKextRepositoryEmpty(KXKextRepositoryRef aRepository);

// Behavior depends on scansForKexts setting; if true, checks for
// added and removed kexts, but if false only checks for removed kexts.
// The repository must validate & authenticate after doing any scan in
// order to be useful.
KXKextManagerError KXKextRepositoryScan(
    KXKextRepositoryRef aRepository);

// If repository is set to scan for additions, empties the repository
// and rescans its directory. Otherwise records the URLs of all kexts
// in the repository, empties the repository, and attempts to recreate
// those kexts from disk and not from the cache.
KXKextManagerError KXKextRepositoryResetIfNeeded(
    KXKextRepositoryRef aRepository);
KXKextManagerError KXKextRepositoryReset(
    KXKextRepositoryRef aRepository);

/*****
 * These tests must be performed before a repository can
 * be considered useful.
 */
void KXKextRepositoryAuthenticateKexts(KXKextRepositoryRef aRepository);
void KXKextRepositoryMarkKextsAuthentic(KXKextRepositoryRef aRepository);
Boolean KXKextRepositoryHasAuthenticated(KXKextRepositoryRef aRepository);
void KXKextRepositoryCheckIntegrityOfKexts(KXKextRepositoryRef aRepository, CFMutableArrayRef bomArray);

CFArrayRef KXKextRepositoryCopyCandidateKexts(KXKextRepositoryRef aRepository);
CFArrayRef KXKextRepositoryCopyBadKexts(KXKextRepositoryRef aRepository);


KXKextRef KXKextRepositoryGetKextWithURL(
    KXKextRepositoryRef aRepository, CFURLRef anURL);
KXKextRef KXKextRepositoryGetKextWithBundlePathInRepository(
    KXKextRepositoryRef aRepository, CFStringRef bundlePathInRepository);

KXKextManagerError KXKextRepositoryWriteCache(
    KXKextRepositoryRef aRepository,
    CFURLRef anURL);  // may be null; will write to repositoryPath.kextcache

KXKextManagerError KXKextRepositorySendCatalogFromCache(
    KXKextRepositoryRef aRepository,
    CFMutableDictionaryRef candidateKexts);

#if 0
#endif 0

__END_DECLS

#endif __KXKEXTREPOSITORY_H__
#endif // !__LP64__
