#ifndef __KXKEXT_PRIVATE_H__
#define __KXKEXT_PRIVATE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "KXKext.h"
#include "KXKextManager_private.h"
#include "KXKextRepository_private.h"
#include "dgraph.h"
#include "vers_rsrc.h"

/*******************************************************************************
* This file is for declaring private  API used by code other than kext.c,
* which must therefore be visible to other files within the framework.
*******************************************************************************/

KXKextRef _KXKextCreate(CFAllocatorRef alloc);

// not used; may remove
KXKextManagerError _KXKextInitWithBundlePathInManager(
    KXKextRef aKext,
    CFStringRef aBundlePath,
    KXKextManagerRef aManager);
KXKextManagerError _KXKextInitWithURLInManager(
    KXKextRef aKext,
    CFURLRef anURL,
    KXKextManagerRef aManager);
// not used; may remove
KXKextManagerError _KXKextInitWithBundleInManager(
    KXKextRef aKext,
    CFBundleRef aBundle,
    KXKextManagerRef aManager);
// not used; may remove
KXKextManagerError _KXKextInitWithBundlePathInRepository(
    KXKextRef aKext,
    CFStringRef aBundlePath,
    KXKextRepositoryRef aRepository);
KXKextManagerError _KXKextInitWithURLInRepository(
    KXKextRef aKext,
    CFURLRef anURL,
    KXKextRepositoryRef aRepository);
/* This is the primary initializer for kexts created from bundles.
 */
KXKextManagerError _KXKextInitWithBundleInRepository(
    KXKextRef aKext,
    CFBundleRef aBundle,
    KXKextRepositoryRef aRepository);

KXKextManagerError _KXKextInitWithCacheDictionaryInRepository(
    KXKextRef aKext,
    CFDictionaryRef aDictionary,
    KXKextRepositoryRef aRepository);

const char * _KXKextCopyCanonicalPathnameAsCString(KXKextRef aKext);
const char * _KXKextCopyBundlePathInRepositoryAsCString(KXKextRef aKext);

KXKextManagerLogLevel _KXKextGetLogLevel(KXKextRef aKext);

VERS_version _KXKextGetVersion(KXKextRef aKext);
VERS_version _KXKextGetCompatibleVersion(KXKextRef aKext);

Boolean _KXKextIsCompatibleWithVersionNumber(
    KXKextRef aKext,
    VERS_version version);

void _KXKextClearVersionRelationships(KXKextRef aKext);
void _KXKextClearDependencies(KXKextRef aKext);

void _KXKextSetPriorVersionKext(KXKextRef aKext, KXKextRef priorKext);
Boolean _KXKextAddPriorOrDuplicateVersionKext(KXKextRef aKext,
    KXKextRef priorKext);
void   _KXKextSetDuplicateVersionKext(KXKextRef aKext, KXKextRef thatKext);
Boolean   _KXKextAddDuplicateVersionKext(KXKextRef aKext, KXKextRef thatKext);

void _KXKextSetIsLoaded(KXKextRef aKext, Boolean flag);
void _KXKextSetHasBeenAuthenticated(KXKextRef aKext, Boolean flag);
void _KXKextSetOtherVersionIsLoaded(KXKextRef aKext, Boolean flag);

KXKextManagerError _KXKextScanPlugins(KXKextRef aKext,
    CFArrayRef * goodPlugins,
    CFArrayRef * badPlugins,
    CFArrayRef * removedPlugins);

void _KXKextSetContainerForPluginKext(
    KXKextRef aKext,
    KXKextRef containerKext);
void _KXKextAddPlugin(KXKextRef aKext, KXKextRef pluginKext);
void _KXKextRemovePlugin(KXKextRef aKext, KXKextRef pluginKext);
void _KXKextRemoveAllPlugins(KXKextRef aKext);

dgraph_t * _KXKextCreateDgraph(KXKextRef aKext);

CFDictionaryRef _KXKextCopyCacheDictionary(KXKextRef aKext);

KXKextManagerError _KXKextMakeSecure(KXKextRef aKext);

KXKextManagerError _KXKextCheckIntegrity(KXKextRef aKext, CFMutableArrayRef bomArray);
void _KXKextSetStartAddress(KXKextRef aKext, vm_address_t newAddr);

#ifdef __cplusplus
}
#endif

#endif __KXKEXT_PRIVATE_H__
