#ifndef __KXKEXTMANAGER_PRIVATE_H__
#define __KXKEXTMANAGER_PRIVATE_H__

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
* This file is for declaring private  API used by code other than
* kextrepository.c, which must therefore be visible to other files within the
* framework.
*******************************************************************************/

#include <IOKit/IOTypes.h>

#include "KXKextManager.h"
#include "KXKextRepository_private.h"
#include "KXKext_private.h"

typedef void (*KXKextManagerLogFunction)(const char * format, ...);
typedef void (*KXKextManagerErrorLogFunction)(const char * format, ...);
typedef int  (*KXKextManagerUserApproveFunction)(
    int default_answer, const char * format, ...);
typedef int  (*KXKextManagerUserVetoFunction)(
    int default_answer, const char * format, ...);
typedef const char * (*KXKextManagerUserInputFunction)(const char * format, ...);

KXKextManagerLogFunction _KXKextManagerGetLogFunction(
    KXKextManagerRef aKextManager);
KXKextManagerErrorLogFunction _KXKextManagerGetErrorLogFunction(
    KXKextManagerRef aKextManager);
KXKextManagerUserApproveFunction _KXKextManagerGetUserApproveFunction(
    KXKextManagerRef aKextManager);
KXKextManagerUserVetoFunction _KXKextManagerGetUserVetoFunction(
    KXKextManagerRef aKextManager);
KXKextManagerUserInputFunction _KXKextManagerGetUserInputFunction(
    KXKextManagerRef aKextManager);

#define _KMApprove _KXKextManagerGetUserApproveFunction
#define _KMVeto    _KXKextManagerGetUserVetoFunction
#define _KMInput   _KXKextManagerGetUserInputFunction

void _KXKextManagerLogMessageAtLevel(KXKextManagerRef aKextManager,
    KXKextManagerLogLevel logLevel,
    KXKextRef aKext,  // may be NULL, in which case kextLogLevel is irrelevant
    KXKextLogLevel kextLogLevel,
    const char * format, ...);
void _KXKextManagerLogMessage(KXKextManagerRef aKextManager, const char * format, ...);
void _KXKextManagerLogError(KXKextManagerRef aKextManager, const char * format, ...);

Boolean _KXKextManagerCheckLogLevel(KXKextManagerRef aKextManager,
    KXKextManagerLogLevel logLevel,
    KXKextRef aKext, // may be NULL
    KXKextManagerLogLevel kextLogLevel);

KXKextManagerError _KXKextManagerAddRepositoryFromCacheFile(
    KXKextManagerRef aKextManager,
    CFURLRef fileURL,
    CFURLRef repositoryURL,
    KXKextRepositoryRef * theRepository);

KXKextManagerError _KXKextManagerAddRepositoryFromCacheDictionary(
    KXKextManagerRef aKextManager,
    CFDictionaryRef aRepositoryCache,
    CFURLRef repositoryURL,
    KXKextRepositoryRef * theRepository);  // out param

void _KXKextManagerClearLoadFailures(KXKextManagerRef aKextManager);

KXKextManagerError _KXKextManagerPrepareKextForLoading(
    KXKextManagerRef aKextManager,
    KXKextRef aKext,
    const char * kext_name,
    Boolean check_loaded_for_dependencies,
    Boolean do_load,
    CFMutableArrayRef inauthenticKexts);
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
    char ** addresses);

// load_options
enum 
{
    kKXKextManagerLoadNone	= false,
    kKXKextManagerLoadKernel	= true,
    kKXKextManagerLoadPrelink	= 2,
    kKXKextManagerLoadKextd	= 3
};

void _KXKextManagerRemoveRepository(
    KXKextManagerRef aKextManager,
    KXKextRepositoryRef aRepository);

#define _kKXKextRepositoryCacheVersion (1)

#ifdef __cplusplus
}
#endif

#endif __KXKEXTMANAGER_PRIVATE_H__
