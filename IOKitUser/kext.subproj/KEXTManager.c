/*
 * Copyright (c) 1999-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * HISTORY
 *
 */

#include "KEXTPrivate.h"
#include "vers_rsrc.h"
#include "CFAdditions.h"
#include <IOKit/IOKitServer.h>
#include <IOKit/IOKitLib.h>
#include <sys/stat.h>
#include <stdarg.h>

#include <IOKit/IOCFURLAccess.h>

static KEXTReturn
URLResourceExists(
            CFURLRef                                url );

static KEXTReturn
URLResourceCreateModifcationTime(
            CFURLRef                                url,
            CFDateRef                             * date );

static KEXTReturn
URLResourceCreateData(
            CFURLRef                                url,
            CFDataRef                             * data );

static void
ArrayAddModuleEntities(
            KEXTModuleRef                            module,
            KEXTManagerRef                           manager );

static void
ArrayAddPersonalityEntities(
            KEXTPersonalityRef                       personality,
            KEXTManagerRef                           manager );

#if 0
static void
ArrayAddConfigEntities(
            KEXTConfigRef                            config,
            KEXTManagerRef                           manager );
#endif

static void
ArrayRemoveModuleEntities(
            KEXTModuleRef                            module,
            KEXTManagerRef                           manager );

static void
ArrayRemovePersonalityEntities(
            KEXTPersonalityRef                       personality,
            KEXTManagerRef                           manager );

#if 0
static void
ArrayRemoveConfigEntities(
            KEXTConfigRef                            config,
            KEXTManagerRef                           manager );
#endif

static void
DictionaryCopyAllBundles(
            CFStringRef                              key,
            KEXTEntityRef                            entity,
            CFMutableArrayRef                        array );

static void
DictionaryCopyAllModules(
            CFStringRef                              key,
            KEXTEntityRef                            entity,
            CFMutableArrayRef                        array );

static void
DictionaryCopyAllPersonalities(
            CFStringRef                              key,
            KEXTEntityRef                            entity,
            CFMutableArrayRef                        array );

static void
DictionaryCopyAllConfigs(
            CFStringRef                              key,
            KEXTEntityRef                            entity,
            CFMutableArrayRef                        array );

static void
_KEXTManagerAddRelation(
            CFMutableDictionaryRef                  relationsDict,
            KEXTEntityRef                           entity );

static void
_KEXTManagerRemoveRelation(
            CFMutableDictionaryRef                  relationsDict,
            KEXTEntityRef                           entity );

static void
_KEXTManagerAddBundleEntity(
            KEXTManagerRef                           manager,
            KEXTBundleRef                            bundle );

static KEXTReturn
_KEXTManagerAddBundle(
            KEXTManagerRef	manager,
            CFURLRef		bundleUrl,
            Boolean		toplevel);

static void
_KEXTManagerAddModuleEntity(
            KEXTManagerRef                           manager,
            KEXTModuleRef                            module );

static void
_KEXTManagerAddPersonalityEntity(
            KEXTManagerRef                           manager,
            KEXTPersonalityRef                       personality );

static void
_KEXTManagerAddConfigEntity(
            KEXTManagerRef                           manager,
            KEXTConfigRef                            config );

static void
_KEXTManagerRemoveBundleEntity(
            KEXTManagerRef                           manager,
            KEXTBundleRef                            bundle );

static void
_KEXTManagerRemoveModuleEntity(
            KEXTManagerRef                           manager,
            KEXTModuleRef                            module );

static void
_KEXTManagerRemovePersonalityEntity(
            KEXTManagerRef                           manager,
            KEXTPersonalityRef                       personality );

static void
_KEXTManagerRemoveConfigEntity(
            KEXTManagerRef                           manager,
            KEXTConfigRef                            config );

static void
_KEXTManagerRemoveBundleItemsOnly(
            KEXTManagerRef                           manager,
            KEXTBundleRef                            bundle );

void
_KEXTManagerRemoveConfigEntityWithCallback(
            KEXTManagerRef                           manager,
            KEXTConfigRef                            config );

static KEXTReturn
_KEXTManagerScanBundles(
            KEXTManagerRef	manager,
            CFURLRef		url,
            Boolean		toplevel);

static void _KEXTManagerLogError(KEXTManagerRef manager,
    CFStringRef string, ...);
static void _KEXTManagerLogMessage(KEXTManagerRef manager,
    CFStringRef string, ...);

//**********************************************************//
//
//    Misc. file I/O functions.
//
//**********************************************************//

// Test the existence of the resource.
static KEXTReturn
URLResourceExists(
            CFURLRef                                url )
{
    CFBooleanRef val;
    Boolean boolval;
    SInt32 err;

    if ( !url ) {
        return kKEXTReturnBadArgument;
    }
    
    val = IOURLCreatePropertyFromResource(
                            kCFAllocatorDefault,
                            url,
                            kIOURLFileExists,
                            &err);
    if ( !val ) {
        return kKEXTReturnBundleNotFound;
    }

    boolval = CFBooleanGetValue(val);
    CFRelease(val);
    if ( !boolval ) {
        return kKEXTReturnBundleNotFound;
    }

    return kKEXTReturnSuccess;
}

// Create the modification time for a URL resource.
static KEXTReturn
URLResourceCreateModifcationTime(
            CFURLRef                                url,
            CFDateRef                             * date )
{
    SInt32 err;

    if ( !url || !date ) {
        return kKEXTReturnBadArgument;
    }
    
    *date = IOURLCreatePropertyFromResource(
                        kCFAllocatorDefault,
                        url,
                        kIOURLFileLastModificationTime,
                        &err);
    if ( !*date ) {
        return kKEXTReturnError;
    }

    return kKEXTReturnSuccess;
}

// Create data object from URL resource.
static KEXTReturn
URLResourceCreateData(
            CFURLRef                                url,
            CFDataRef                             * data )
{
    Boolean val;
    SInt32 err;

    *data = NULL;
    val = IOURLCreateDataAndPropertiesFromResource(
                        kCFAllocatorDefault,
                        url,
                        data,
                        NULL,
                        NULL,
                        &err);
    if ( !val ) {
        if ( *data ) {
            CFRelease(*data);
            *data = NULL;
        }
        return kKEXTReturnResourceNotFound;
    }

    return kKEXTReturnSuccess;
}


//**********************************************************//
//
//  URL stuff.
//
//**********************************************************//

static Boolean
_KEXTManagerIsKernelExtensionURL(CFURLRef url)
{
    CFStringRef path = CFURLGetString(url);

    return ( CFStringHasSuffix(path, CFSTR("kext"))
        ||   CFStringHasSuffix(path, CFSTR("kext/")) );
}

// Creates a URL path to the module file.
static CFURLRef
_KEXTManagerCreateURLForModule(
            KEXTManagerRef                           manager,
            KEXTModuleRef                            module )
{
    KEXTReturn error;
    KEXTBundleRef bundle;
    CFStringRef primaryKey;
    CFStringRef file;

    primaryKey = KEXTModuleGetPrimaryKey(module);
    if ( !primaryKey) {
        return NULL;
    }

    bundle = KEXTManagerGetBundleWithModule(manager, primaryKey);
    if ( !bundle ) {
        return NULL;
    }

    file = KEXTModuleGetProperty(module, CFSTR(kModuleFileKey));
    if ( !file ) {
        return NULL;
    }

    return KEXTBundleCreateURLForResource(bundle, file, NULL, &error);
}

//**********************************************************//
//
//    Misc. internal relationship setters.
//
//**********************************************************//

static void
_KEXTManagerAddRelation(
            CFMutableDictionaryRef                  relationsDict,
            KEXTEntityRef                           entity )
{
    CFStringRef parentKey;
    CFStringRef primaryKey;
    CFMutableArrayRef keyArray;
    CFIndex count;

    parentKey = CFDictionaryGetValue(entity, CFSTR("ParentKey"));
    if ( !parentKey ) {
        return;
    }

    keyArray = (CFMutableArrayRef)CFDictionaryGetValue(relationsDict, parentKey);
    if ( !keyArray ) {
        keyArray = CFArrayCreateMutable(
                            kCFAllocatorDefault, 0,
                            &kCFTypeArrayCallBacks);
        if ( !keyArray ) {
            return;
        }

        CFDictionarySetValue(relationsDict, parentKey, keyArray);
        CFRelease(keyArray);
    }

    count = CFArrayGetCount(keyArray);
    primaryKey = CFDictionaryGetValue(entity, CFSTR("PrimaryKey"));
    if( !CFArrayContainsValue(keyArray, CFRangeMake(0, count), primaryKey) ) {
        CFArrayAppendValue(keyArray, primaryKey);
    }
}

static void
_KEXTManagerRemoveRelation(
            CFMutableDictionaryRef                  relationsDict,
            KEXTEntityRef                           entity )
{
    CFStringRef parentKey;
    CFStringRef primaryKey;
    CFMutableArrayRef keyArray;
    CFIndex count;
    CFIndex index;

    parentKey = CFDictionaryGetValue(entity, CFSTR("ParentKey"));
    primaryKey = CFDictionaryGetValue(entity, CFSTR("PrimaryKey"));

    if ( !parentKey || !primaryKey ) {
        return;
    }

    keyArray = (CFMutableArrayRef)CFDictionaryGetValue(relationsDict, parentKey);
    if ( !keyArray ) {
        return;
    }

    count = CFArrayGetCount(keyArray);
    index = CFArrayGetFirstIndexOfValue(keyArray, CFRangeMake(0, count), primaryKey);
    if ( index != -1 ) {
        CFArrayRemoveValueAtIndex(keyArray, index);
    }
}



//**********************************************************//
//
//    Common internal entity getters/setters.
//
//**********************************************************//

static inline KEXTEntityRef
_KEXTManagerGetEntityWithKey(
            KEXTManagerRef                           manager,
            CFStringRef                              primaryKey )
{
    return (KEXTEntityRef)CFDictionaryGetValue(deref(manager)->_entities, primaryKey);
}

static inline void
_KEXTManagerAddEntity(
            KEXTManagerRef                           manager,
            KEXTEntityRef                            entity )
{
    CFStringRef primaryKey;

    primaryKey = CFDictionaryGetValue(entity, CFSTR("PrimaryKey"));
    if ( primaryKey ) {
        CFDictionarySetValue(deref(manager)->_entities, primaryKey, entity);
    }
}

static void
_KEXTManagerAddBundleEntity(
            KEXTManagerRef                           manager,
            KEXTBundleRef                            bundle )
{
    CFURLRef url;
    CFStringRef primaryKey;

    if ( !manager || !bundle ) {
        return;
    }
    
    // Add entity to the KEXTManagerDatabase.
    _KEXTManagerAddEntity(manager, bundle);

    // Add URL to the URL relationship database.
    url = KEXTBundleCopyURL(bundle);
    if ( url ) {
        primaryKey = KEXTBundleGetPrimaryKey(bundle);
        CFDictionarySetValue(deref(manager)->_urlRels, url, primaryKey);
        CFRelease(url);
    }
}

static void
_KEXTManagerAddModuleEntity(
            KEXTManagerRef                           manager,
            KEXTModuleRef                            module )
{
    if ( !manager || !module ) {
        return;
    }
    
    _KEXTManagerAddEntity(manager, module);

    // Create a relationship between the parent bundle
    // and the module.
    _KEXTManagerAddRelation(deref(manager)->_modRels, module);
}

static void
_KEXTManagerAddPersonalityEntity(
            KEXTManagerRef                           manager,
            KEXTPersonalityRef                       personality )
{
    if ( !manager || !personality ) {
        return;
    }

    _KEXTManagerAddEntity(manager, personality);
    
    // Create a relationship between the parent bundle
    // and the personality.
    _KEXTManagerAddRelation(deref(manager)->_perRels, personality);
}

static void
_KEXTManagerAddConfigEntity(
            KEXTManagerRef                           manager,
            KEXTConfigRef                            config )
{
    if ( !manager || !config ) {
        return;
    }

    _KEXTManagerAddEntity(manager, config);

    // Create a relationship between the parent bundle
    // and the configuration.
    _KEXTManagerAddRelation(deref(manager)->_cfgRels, config);
}

static inline void
_KEXTManagerRemoveEntity(
            KEXTManagerRef                           manager,
            KEXTEntityRef                            entity )
{
    CFStringRef primaryKey;

    primaryKey = CFDictionaryGetValue(entity, CFSTR("PrimaryKey"));
    if ( primaryKey ) {
        CFDictionaryRemoveValue(deref(manager)->_entities, primaryKey);
    }
}

static void
_KEXTManagerRemoveBundleEntity(
            KEXTManagerRef                           manager,
            KEXTBundleRef                            bundle )
{
    CFURLRef url;

    if ( !manager || !bundle ) {
        return;
    }
    
    // Remove the URL relationship to this bundle.
    url = KEXTBundleCopyURL(bundle);
    if ( url ) {
        CFDictionaryRemoveValue(deref(manager)->_urlRels, url);
        CFRelease(url);
    }
    _KEXTManagerRemoveEntity(manager, bundle);

}

static void
_KEXTManagerRemoveModuleEntity(
            KEXTManagerRef                           manager,
            KEXTModuleRef                            module )
{
    if ( !manager || !module ) {
        return;
    }
    
    _KEXTManagerRemoveEntity(manager, module);

    _KEXTManagerRemoveRelation(deref(manager)->_modRels, module);
}

static void
_KEXTManagerRemovePersonalityEntity(
            KEXTManagerRef                           manager,
            KEXTPersonalityRef                       personality )
{
    if ( !manager || !personality ) {
        return;
    }
    
    _KEXTManagerRemoveEntity(manager, personality);

    _KEXTManagerRemoveRelation(deref(manager)->_perRels, personality);
}

static void
_KEXTManagerRemoveConfigEntity(
            KEXTManagerRef                           manager,
            KEXTConfigRef                            config )
{
    if ( !manager || !config ) {
        return;
    }
    
    _KEXTManagerRemoveEntity(manager, config);

    _KEXTManagerRemoveRelation(deref(manager)->_cfgRels, config);
}

static CFArrayRef
_KEXTManagerGetModuleKeysForBundle(
            KEXTManagerRef                           manager,
            KEXTBundleRef                            bundle )
{
    CFStringRef key;

    key = KEXTBundleGetPrimaryKey(bundle);
    return CFDictionaryGetValue(deref(manager)->_modRels, key);
}

static CFArrayRef
_KEXTManagerGetPersonalityKeysForBundle(
            KEXTManagerRef                           manager,
            KEXTBundleRef                            bundle )
{
    CFStringRef key;

    key = KEXTBundleGetPrimaryKey(bundle);
    return CFDictionaryGetValue(deref(manager)->_perRels, key);
}

static CFArrayRef
_KEXTManagerGetConfigKeysForBundle(
            KEXTManagerRef                           manager,
            KEXTBundleRef                            bundle )
{
    CFStringRef key;

    key = KEXTBundleGetPrimaryKey(bundle);
    return CFDictionaryGetValue(deref(manager)->_cfgRels, key);
}

//**********************************************************//
//
//    Misc. internal CFArray applier functions.
//
//**********************************************************//

static void
ArrayAddModuleEntities(
            KEXTModuleRef                            module,
            KEXTManagerRef                           manager )
{
    _KEXTManagerAddModuleEntity(manager, module);
}

static void
ArrayAddPersonalityEntities(
            KEXTPersonalityRef                       personality,
            KEXTManagerRef                           manager )
{
    _KEXTManagerAddPersonalityEntity(manager, personality);
}

#if 0
static void
ArrayAddConfigEntities(
            KEXTConfigRef                            config,
            KEXTManagerRef                           manager )
{
    _KEXTManagerAddConfigEntity(manager, config);
}
#endif

static void
ArrayRemoveModuleEntities(
            KEXTModuleRef                            module,
            KEXTManagerRef                           manager )
{
    _KEXTManagerRemoveModuleEntity(manager, module);
}

static void
ArrayRemovePersonalityEntities(
            KEXTPersonalityRef                       personality,
            KEXTManagerRef                           manager )
{
    _KEXTManagerRemovePersonalityEntity(manager, personality);
}

#if 0
static void
ArrayRemoveConfigEntities(
            KEXTConfigRef                            config,
            KEXTManagerRef                           manager )
{
    _KEXTManagerRemoveConfigEntity(manager, config);
}
#endif

static void
DictionaryCopyAllBundles(
            CFStringRef                              key,
            KEXTEntityRef                            entity,
            CFMutableArrayRef                        array )
{
    CFStringRef type;

    type = KEXTManagerGetEntityType(entity);
    if ( CFEqual(type, KEXTBundleGetEntityType()) ) {
        CFArrayAppendValue(array, entity);
    }
}

static void
DictionaryCopyAllModules(
            CFStringRef                              key,
            KEXTEntityRef                            entity,
            CFMutableArrayRef                        array )
{
    CFStringRef type;

    type = KEXTManagerGetEntityType(entity);
    if ( CFEqual(type, KEXTModuleGetEntityType()) ) {
        CFArrayAppendValue(array, entity);
    }
}

static void
DictionaryCopyAllPersonalities(
            CFStringRef                              key,
            KEXTEntityRef                            entity,
            CFMutableArrayRef                        array )
{
    CFStringRef type;
    CFBooleanRef prop;

    prop = CFDictionaryGetValue(entity, CFSTR("IsConfig"));
    if ( prop && CFEqual(prop, kCFBooleanTrue) ) {
        return;
    }
    type = KEXTManagerGetEntityType(entity);
    if ( CFEqual(type, KEXTPersonalityGetEntityType()) ) {
        CFArrayAppendValue(array, entity);
    }
}

static void
DictionaryCopyAllConfigs(
            CFStringRef                              key,
            KEXTEntityRef                            entity,
            CFMutableArrayRef                        array )
{
    CFStringRef type;
    CFBooleanRef prop;

    prop = CFDictionaryGetValue(entity, CFSTR("IsConfig"));
    if ( !prop || !CFEqual(prop, kCFBooleanTrue) ) {
        return;
    }
    type = KEXTManagerGetEntityType(entity);
    if ( CFEqual(type, KEXTPersonalityGetEntityType()) ) {
        CFArrayAppendValue(array, entity);
    }
}

static void
ArrayCopyEntityForKey(
            CFStringRef                              key,
            void                                   * context[] )
{
    KEXTManagerRef manager;
    KEXTEntityRef entity;
    CFMutableArrayRef array;

    manager = context[0];
    array = context[1];

    entity = _KEXTManagerGetEntityWithKey(manager, key);
    if ( entity ) {
        CFArrayAppendValue(array, entity);
    }
}
            

//**********************************************************//
//
//   Misc. internal functions.
//
//**********************************************************//

static void ArrayGatherDependencyPaths(const void * val, void * context[])
{
    KEXTManagerRef manager;
    KEXTModuleRef module;
    CFMutableArrayRef array;
    CFStringRef path;
    CFURLRef url;

    module = (KEXTModuleRef)val;
    manager = context[0];
    array = context[1];

    url = _KEXTManagerCreateURLForModule(manager, module);
    if ( !url ) {
        return;
    }

    path = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
    CFRelease(url);
    if ( !path ) {
        return;
    }

    CFArrayAppendValue(array, path);
    CFRelease(path);
}

Boolean KEXTManagerCheckSafeBootForModule(
            KEXTManagerRef                           manager,
            KEXTModuleRef                            module)
{
    Boolean result = false;
    CFStringRef modSafe;

    if (!deref(manager)->_safeBoot) {
        result = true;
        goto finish;
    }

    modSafe = KEXTModuleGetProperty(module, CFSTR("OSBundleRequired"));
    if (!modSafe) {
        result = false;
        goto finish;
    }

    if (kCFCompareEqualTo == CFStringCompare(modSafe, CFSTR("Root"), 0) ||
        kCFCompareEqualTo == CFStringCompare(modSafe, CFSTR("Local-Root"), 0) ||
        kCFCompareEqualTo == CFStringCompare(modSafe, CFSTR("Network-Root"), 0) ||
        kCFCompareEqualTo == CFStringCompare(modSafe, CFSTR("Console"), 0) ||
        kCFCompareEqualTo == CFStringCompare(modSafe, CFSTR("Safe Boot"), 0) ) {

        result = true;
        goto finish;

    }

finish:
    return result;
}


// Callback for loading a module into the kernel.
static KEXTReturn
_KEXTManagerLoadModule(
            KEXTManagerRef                           manager,
            KEXTModuleRef                            module,
            CFArrayRef                               dependencies )
{
    KEXTReturn error;
    CFMutableArrayRef paths;
    CFStringRef primaryKey;
    CFStringRef mode;
    CFStringRef moduleName;
    CFRange range;
    void * params[2];

    if ( !module || !manager ) {
        return kKEXTReturnBadArgument;
    }

    primaryKey = KEXTModuleGetPrimaryKey(module);
    if ( !primaryKey ) {
        return kKEXTReturnError;
    }

    moduleName = KEXTModuleGetProperty(module, CFSTR(kNameKey));
    if ( !moduleName ) {
        return kKEXTReturnError;
    }

    // Check the noloads list for this module.  If it's
    // not to be loaded, then don't load it.
    mode = CFDictionaryGetValue(deref(manager)->_noloads, primaryKey);
    if ( mode && CFEqual(mode, CFSTR("Disabled")) ) {
        _KEXTManagerLogError(manager,
            CFSTR("Extension %@ is disabled.\n"), moduleName);
        return kKEXTReturnModuleDisabled;
    }

    paths = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if ( !paths ) {
        return kKEXTReturnNoMemory;
    }

    params[0] = manager;
    params[1] = paths;

    range = CFRangeMake(0, CFArrayGetCount(dependencies));
    
    CFArrayApplyFunction(
                    dependencies,
                    range,
                    (CFArrayApplierFunction)ArrayGatherDependencyPaths,
                    params);

    error = kKEXTReturnError;
    do {
        CFURLRef url;
        CFStringRef path;

        url = _KEXTManagerCreateURLForModule(manager, module);
        if ( !url ) {
            error = kKEXTReturnNoMemory;
            break;
        }

        path = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
        CFRelease(url);
        if ( !path ) {
            error = kKEXTReturnNoMemory;
            break;
        }

        error = KEXTLoadModule(path, paths);
        CFRelease(path);
    } while ( false );

    CFRelease(paths);

    return error;
}

// Callback for removing a personality from the IOCatalogue.
KEXTReturn
_KEXTManagerUnloadPersonality(
            KEXTManagerRef                           manager,
            KEXTPersonalityRef                       personality )
{
    KEXTReturn error;
    CFDictionaryRef matchingDict;

    if ( !manager || !personality ) {
        return kKEXTReturnBadArgument;
    }

    matchingDict = _KEXTPersonalityGetProperties(personality);

    // First, remove personality from IOCatalogue.
    error = KEXTSendDataToCatalog(deref(manager)->_catPort,
                                  kIOCatalogRemoveDrivers,
                                  matchingDict);

    if ( error != kKEXTReturnSuccess ) {
        return error;
    }

    return kKEXTReturnSuccess;
}

// Callback for bundle authentication.
static KEXTReturn
BundleAuthenticateCallback(
            CFURLRef                                 url,
            void                                   * context )
{
    return KEXTManagerAuthenticateURL(url);
}



//**********************************************************//
//
//  Public API's.
//
//**********************************************************//

#define kAdministratorUID 0

KEXTReturn
KEXTManagerAuthenticateURL(
            CFURLRef                                 url )
{
    KEXTReturn error;
    CFArrayRef attribs;
    CFDictionaryRef dict;
    Boolean ret;
    SInt32 err;
    CFNumberRef uid = NULL;

    const void * vals[] = {
        kIOURLFileExists, kIOURLFileOwnerID, kIOURLFilePOSIXMode
    };

    dict = NULL;

    if ( !url || (CFGetTypeID(url) != CFURLGetTypeID()) ) {
        return kKEXTReturnBadArgument;
    }

    attribs = CFArrayCreate(kCFAllocatorDefault, vals, 3, &kCFTypeArrayCallBacks);
    if ( !attribs ) {
        return kKEXTReturnNoMemory;
    }

    error = kKEXTReturnError;
    ret = IOURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault, url, NULL, &dict, attribs, &err);
    CFRelease(attribs);
    if ( ret ) do {
        SInt32 uidVal;
        mode_t modeVal;
        CFBooleanRef cfbool;
        CFNumberRef mode;

        cfbool = CFDictionaryGetValue(dict, kIOURLFileExists);
        if (!cfbool || !CFBooleanGetValue(cfbool)) {
            error =  kKEXTReturnResourceNotFound;
            break;
        }

        uid = CFDictionaryGetValue(dict, kIOURLFileOwnerID);
        if (!uid || !CFNumberGetValue(uid, kCFNumberSInt32Type, &uidVal)
        ||  uidVal != kAdministratorUID) {
            error =  kKEXTReturnPermissionError;
            break;
        }

        mode = CFDictionaryGetValue(dict, kIOURLFilePOSIXMode);
        if ( !mode ) {
            error = kKEXTReturnError;
            break;
        }

        if ( !CFNumberGetValue(mode, kCFNumberShortType, &modeVal) ) {
            error = kKEXTReturnError;
            break;
        }

        // Check Group and Other write permissions.
        if ( (modeVal & S_IWOTH) ||
             (modeVal & S_IWGRP) ) {
            error = kKEXTReturnPermissionError;
        }

        error = kKEXTReturnSuccess;
    } while ( false );

    if ( dict )
        CFRelease(dict);

    return error;
}

KEXTManagerRef
KEXTManagerCreate(
            KEXTManagerBundleLoadingCallbacks      * bundleCallbacks,
            KEXTManagerModuleLoadingCallbacks      * moduleCallbacks,
            KEXTManagerPersonalityLoadingCallbacks * personalityCallbacks,
            KEXTManagerConfigsCallbacks            * configsCallbacks,
            void                                   * context,
            void                      (* logErrorCallback)(const char * s),
            void                      (* logMessageCallback)(const char * s),
            Boolean                                  safeBoot,
            KEXTReturn                             * error )
{
    KEXTManager * manager;

    *error = kKEXTReturnSuccess;
    manager = (KEXTManager *)malloc(sizeof(KEXTManager));
    if ( !manager )
        return NULL;

    memset(manager, '\0', sizeof(KEXTManager));

    manager->_refcount = 1;
    manager->_safeBoot = safeBoot;
    manager->_context = context;
    manager->_logErrorFunc = logErrorCallback;
    manager->_logMessageFunc = logMessageCallback;
    *error = KERN2KEXTReturn(IOMasterPort(bootstrap_port, &manager->_catPort));
    if ( *error != kKEXTReturnSuccess ) {
        KEXTManagerFree((KEXTManagerRef)manager);
        return NULL;
    }

    manager->_entities = CFDictionaryCreateMutable(
                        kCFAllocatorDefault, 0,
                        &kCFTypeDictionaryKeyCallBacks,
                        &kCFTypeDictionaryValueCallBacks);

    manager->_urlRels = CFDictionaryCreateMutable(
                        kCFAllocatorDefault, 0,
                        &kCFTypeDictionaryKeyCallBacks,
                        &kCFTypeDictionaryValueCallBacks);

    manager->_modRels = CFDictionaryCreateMutable(
                        kCFAllocatorDefault, 0,
                        &kCFTypeDictionaryKeyCallBacks,
                        &kCFTypeDictionaryValueCallBacks);

    manager->_perRels = CFDictionaryCreateMutable(
                        kCFAllocatorDefault, 0,
                        &kCFTypeDictionaryKeyCallBacks,
                        &kCFTypeDictionaryValueCallBacks);

    manager->_cfgRels = CFDictionaryCreateMutable(
                        kCFAllocatorDefault, 0,
                        &kCFTypeDictionaryKeyCallBacks,
                        &kCFTypeDictionaryValueCallBacks);

    manager->_noloads = CFDictionaryCreateMutable(
                        kCFAllocatorDefault, 0,
                        &kCFTypeDictionaryKeyCallBacks,
                        &kCFTypeDictionaryValueCallBacks);

    if ( !manager->_entities ) {
        KEXTManagerFree((KEXTManagerRef)manager);
        *error = kKEXTReturnNoMemory;
        return NULL;
    }

    manager->bcb.BundleAuthentication = BundleAuthenticateCallback;
    if ( bundleCallbacks ) {
        manager->bcb.BundleWillAdd = bundleCallbacks->BundleWillAdd;
        manager->bcb.BundleWillRemove = bundleCallbacks->BundleWillRemove;
        manager->bcb.BundleWasAdded = bundleCallbacks->BundleWasAdded;
        manager->bcb.BundleWasRemoved = bundleCallbacks->BundleWasRemoved;
        if ( bundleCallbacks->BundleAuthentication )
            manager->bcb.BundleAuthentication = bundleCallbacks->BundleAuthentication;
    }

    if ( moduleCallbacks ) {
        manager->mcb.ModuleWillLoad  = moduleCallbacks->ModuleWillLoad;
        manager->mcb.ModuleWasLoaded = moduleCallbacks->ModuleWasLoaded;
        manager->mcb.ModuleError = moduleCallbacks->ModuleError;
        manager->mcb.ModuleWillUnload = moduleCallbacks->ModuleWillUnload;
        manager->mcb.ModuleWasUnloaded = moduleCallbacks->ModuleWasUnloaded;
    }

    if ( personalityCallbacks ) {
        manager->pcb.PersonalityWillLoad = personalityCallbacks->PersonalityWillLoad;
        manager->pcb.PersonalityWasLoaded = personalityCallbacks->PersonalityWasLoaded;
        manager->pcb.PersonalityError = personalityCallbacks->PersonalityError;
        manager->pcb.PersonalityWillUnload = personalityCallbacks->PersonalityWillUnload;
        manager->pcb.PersonalityWasUnloaded = personalityCallbacks->PersonalityWasUnloaded;
    }

    if ( configsCallbacks ) {
        manager->ccb.ConfigWillAdd = configsCallbacks->ConfigWillAdd;
        manager->ccb.ConfigWasAdded = configsCallbacks->ConfigWasAdded;
        manager->ccb.ConfigWillRemove = configsCallbacks->ConfigWillRemove;
        manager->ccb.ConfigWasRemoved = configsCallbacks->ConfigWasRemoved;
    }
    
    manager->_mode = kKEXTManagerDefaultMode;
    manager->_configsDate = NULL;

    return (KEXTManagerRef)manager;
}

KEXTManagerRef
KEXTManagerRetain(
            KEXTManagerRef                           manager )
{
    deref(manager)->_refcount++;
    return manager;
}

void
KEXTManagerRelease(
            KEXTManagerRef                           manager )
{
    deref(manager)->_refcount--;
    if ( deref(manager)->_refcount < 1 ) {
        KEXTManagerFree(manager);
    }
}

void
KEXTManagerFree(
            KEXTManagerRef                           manager )
{
    if ( deref(manager)->_entities ) {
        CFRelease(deref(manager)->_entities);
    }
    if ( deref(manager)->_modRels) {
        CFRelease(deref(manager)->_modRels);
    }
    if ( deref(manager)->_perRels) {
        CFRelease(deref(manager)->_perRels);
    }
    if ( deref(manager)->_urlRels) {
        CFRelease(deref(manager)->_urlRels);
    }
    if ( deref(manager)->_noloads ) {
        CFRelease(deref(manager)->_noloads);
    }
    if ( deref(manager)->_cfgRels ) {
        CFRelease(deref(manager)->_cfgRels);
    }
    if ( deref(manager)->_configsDate ) {
        CFRelease(deref(manager)->_configsDate);
    }

    free(manager);
}

void
KEXTManagerSetMode(
            KEXTManagerRef                           manager,
            KEXTManagerMode                          mode )
{
    if ( !manager) {
        return;
    }

    deref(manager)->_mode = mode;
}

void
KEXTManagerReset(
            KEXTManagerRef                           manager )
{
    CFDictionaryRemoveAllValues(deref(manager)->_entities);
    CFDictionaryRemoveAllValues(deref(manager)->_modRels);
    CFDictionaryRemoveAllValues(deref(manager)->_perRels);
    CFDictionaryRemoveAllValues(deref(manager)->_cfgRels);
    CFDictionaryRemoveAllValues(deref(manager)->_noloads);
}

static inline CFArrayRef
_KEXTManagerCopyRelationsForBundle(
            CFMutableDictionaryRef                  relationsDict,
            KEXTBundleRef                           bundle )
{
    CFStringRef primaryKey;
    CFArrayRef array;

    primaryKey = KEXTBundleGetPrimaryKey(bundle);
    if ( !primaryKey ) {
        return NULL;
    }

    array = CFDictionaryGetValue(relationsDict, primaryKey);
    if ( !array ) {
        return NULL;
    }
    return CFArrayCreateCopy(kCFAllocatorDefault, array);
}

CFArrayRef
KEXTManagerCopyModulesForBundle(
            KEXTManagerRef                           manager,
            KEXTBundleRef                            bundle)
{
    CFMutableArrayRef array;
    CFArrayRef moduleKeys;
    CFRange range;
    void * context[2];

    moduleKeys = _KEXTManagerGetModuleKeysForBundle(manager, bundle);
    if ( !moduleKeys ) {
        return NULL;
    }
    
    array = CFArrayCreateMutable(
                        kCFAllocatorDefault, 0,
                        &kCFTypeArrayCallBacks);

    if ( !array ) {
        return NULL;
    }
    
    context[0] = manager;
    context[1] = array;

    range = CFRangeMake(0, CFArrayGetCount(moduleKeys));
    CFArrayApplyFunction(moduleKeys, range, (CFArrayApplierFunction)ArrayCopyEntityForKey, context);

    return array;
}

CFArrayRef
KEXTManagerCopyPersonalitiesForBundle(
            KEXTManagerRef                           manager,
            KEXTBundleRef                            bundle)
{
    CFMutableArrayRef array;
    CFArrayRef personKeys;
    CFRange range;
    void * context[2];

    personKeys = _KEXTManagerGetPersonalityKeysForBundle(manager, bundle);
    if ( !personKeys ) {
        return NULL;
    }
    
    array = CFArrayCreateMutable(
                        kCFAllocatorDefault, 0,
                        &kCFTypeArrayCallBacks);

    if ( !array ) {
        return NULL;
    }

    context[0] = manager;
    context[1] = array;

    range = CFRangeMake(0, CFArrayGetCount(personKeys));
    CFArrayApplyFunction(personKeys, range, (CFArrayApplierFunction)ArrayCopyEntityForKey, context);

    return array;
}

CFArrayRef
KEXTManagerCopyConfigsForBundle(
            KEXTManagerRef                           manager,
            KEXTBundleRef                            bundle)
{
    CFMutableArrayRef array;
    CFArrayRef configKeys;
    CFRange range;
    void * context[2];

    configKeys = _KEXTManagerGetConfigKeysForBundle(manager, bundle);
    if ( !configKeys ) {
        return NULL;
    }

    array = CFArrayCreateMutable(
                        kCFAllocatorDefault, 0,
                        &kCFTypeArrayCallBacks);

    if ( !array ) {
        return NULL;
    }

    context[0] = manager;
    context[1] = array;

    range = CFRangeMake(0, CFArrayGetCount(configKeys));
    CFArrayApplyFunction(configKeys, range, (CFArrayApplierFunction)ArrayCopyEntityForKey, context);

    return array;
}

/*********
 * A module in the new scheme is just the top level of the plist,
 * minus the personalities dictionary.
 */
static CFArrayRef
_KEXTBundleCopyModule (
            CFDictionaryRef                          props /* a bundle dict */ )
{
    CFArrayRef      moduleArray;
    CFDictionaryRef moduleDict;
    CFDictionaryRef personalitiesDict;

    moduleDict = CFDictionaryCreateMutableCopy(NULL, NULL, props);
    if (!moduleDict) {
        return NULL;
    }

    personalitiesDict = CFDictionaryGetValue(moduleDict, CFSTR("IOKitPersonalities"));
    if (personalitiesDict) {
        CFDictionaryRemoveValue(moduleDict, CFSTR("IOKitPersonalities"));
    }

    moduleArray = CFArrayCreate(kCFAllocatorDefault, &moduleDict, 1, &kCFTypeArrayCallBacks);
    CFRelease(moduleDict);

    return moduleArray;  // perhaps it should be made immutable....
}

static void _personalityInjectName(
    const void * key,
    const void * val,
    void * context)
{
    CFStringRef * personalityName;
    CFDictionaryRef * personality;

    personalityName = (CFStringRef *)key;
    personality = (CFDictionaryRef *)val;

    CFDictionarySetValue(personality, CFSTR("IOPersonalityName"), personalityName);

    return;
}


static CFArrayRef
_KEXTBundleCopyPersonalities (
            CFDictionaryRef                          props /* a bundle dict */ )
{
    CFDictionaryRef personalities;
    CFDictionaryRef personsMutable;
    CFArrayRef personalitiesArray;

    CFIndex numPersonalities;
    void ** personalityValues;

    personalities = CFDictionaryGetValue(props, CFSTR(kPersonalitiesKey));
    if ( ! personalities ) {
        return NULL;
    }

    numPersonalities = CFDictionaryGetCount(personalities);
    if (numPersonalities < 1) {
        return NULL;
    }

    personsMutable = CFDictionaryCreateMutableCopy(NULL, 0, personalities);
    if ( ! personsMutable ) {
        return NULL;
    }

   /* Put the key for each personality into its dictionary so we can track
    * things by name (using the "IOPersonalityName" key).
    */
    CFDictionaryApplyFunction(personsMutable, & _personalityInjectName, NULL);

    personalityValues = (void **)malloc(numPersonalities * sizeof(void *));
    if (!personalityValues) {
        return NULL;
    }
    CFDictionaryGetKeysAndValues(personsMutable, NULL, personalityValues);
    personalitiesArray = CFArrayCreateMutable(kCFAllocatorDefault,
        numPersonalities, &kCFTypeArrayCallBacks);
    if (!personalitiesArray) {
        return NULL;
    }

    CFArrayReplaceValues(personalitiesArray, CFRangeMake(0, 0),
        personalityValues, numPersonalities);

    free(personalityValues);

    return personalitiesArray;
}

static void ArrayGetModuleAndKey(const void * val, void * context[])
{
    CFMutableArrayRef keys;
    CFMutableArrayRef mods;
    CFStringRef bundleKey;
    KEXTModuleRef mod;

    bundleKey = context[0];
    mods = context[1];
    keys = context[2];
    
    mod = KEXTModuleCreate(bundleKey, val);
    if ( !mod ) {
        return;
    }

    CFArrayAppendValue(mods, mod);
    CFArrayAppendValue(keys, KEXTModuleGetPrimaryKey(mod));
    CFRelease(mod);
}

static CFMutableArrayRef
_KEXTManagerCreateModulesArray(
            CFStringRef                              bundleKey,
            CFDictionaryRef                          props,
            CFMutableArrayRef                      * keys,
            Boolean                                  isCFStyle )
{
    CFMutableArrayRef modules;
    CFMutableArrayRef modKeys;
    CFArrayRef array;
    CFRange range;
    void * context[3];

    if ( !props || !bundleKey || !keys  )
        return NULL;

    array = _KEXTBundleCopyModule(props);
    if ( !array ) {
        return NULL;
    }

    modKeys = CFArrayCreateMutable(
                        kCFAllocatorDefault, 0, 
                        &kCFTypeArrayCallBacks);

    modules = CFArrayCreateMutable(
                        kCFAllocatorDefault, 0,
                        &kCFTypeArrayCallBacks);

    if ( !modules || !modKeys ) {
        if ( modules ) {
            CFRelease(modules);
        }
        if ( modKeys ) {
            CFRelease(modKeys);
        }
        CFRelease(array);
        return NULL;
    }

    context[0] = (void *)bundleKey;
    context[1] = modules;
    context[2] = modKeys;

    range = CFRangeMake(0, CFArrayGetCount(array));
    CFArrayApplyFunction(array, range, (CFArrayApplierFunction)ArrayGetModuleAndKey, context);
    CFRelease(array);

    *keys = modKeys;

    return modules;
}

static void ArrayGetPersonalityAndKey(const void * val, void * context[])
{
    CFMutableArrayRef personKeys;
    CFMutableArrayRef persons;
    CFStringRef bundleKey;
    KEXTPersonalityRef person;

    bundleKey = context[0];
    persons = context[1];
    personKeys = context[2];

    person = KEXTPersonalityCreate(bundleKey, val);
    if ( !person ) {
        return;
    }

    CFArrayAppendValue(persons, person);
    CFArrayAppendValue(personKeys, KEXTPersonalityGetPrimaryKey(person));
    CFRelease(person);
}

static CFMutableArrayRef
_KEXTManagerCreatePersonsArray(
            CFStringRef                              bundleKey,
            CFDictionaryRef                          props, 
            CFMutableArrayRef                      * keys,
            Boolean                                  isCFStyle )
{
    CFMutableArrayRef persons;
    CFMutableArrayRef personKeys;
    CFArrayRef array;
    CFRange range;
    void * context[3];

    if ( !props || !bundleKey || !keys ) {
        return NULL;
    }

    array = _KEXTBundleCopyPersonalities(props);
    if ( !array ) {
        return NULL;
    }

    personKeys = CFArrayCreateMutable(
                        kCFAllocatorDefault, 0,
                        &kCFTypeArrayCallBacks);

    persons = CFArrayCreateMutable(
                        kCFAllocatorDefault, 0,
                        &kCFTypeArrayCallBacks);

    if ( !persons || !personKeys ) {
        if ( persons ) {
            CFRelease(persons);
        }
        if ( personKeys ) {
            CFRelease(personKeys);
        }
        CFRelease(array);
        return NULL;
    }

    context[0] = (void *)bundleKey;
    context[1] = persons;
    context[2] = personKeys;

    range = CFRangeMake(0, CFArrayGetCount(array));
    CFArrayApplyFunction(array, range, (CFArrayApplierFunction)ArrayGetPersonalityAndKey, context);
    CFRelease(array);

    *keys = personKeys;

    return persons;
}

// Extract the Info-macosx.plist from the 
// bundle and turn it into an object.
static KEXTReturn
_KEXTManagerCreateProperties(
            KEXTManagerRef                           manager,
            CFURLRef                                 url,
            CFDictionaryRef                        * properties,
            Boolean                                * isCFStyle )
{
    KEXTReturn ret;
    CFStringRef urlString;
    CFStringRef newPath;
    CFStringRef str;
    CFURLRef newUrl;
    CFDataRef xmlData;
    
    *isCFStyle = true;

    urlString = CFURLGetString(url);
    
    newPath = CFStringCreateWithFormat(
                        kCFAllocatorDefault,
                        NULL,
                        CFSTR("%@/Contents/Info.plist"),
                        urlString);

    if ( !newPath ) {
        return kKEXTReturnNoMemory;
    }

    newUrl = CFURLCreateWithString(kCFAllocatorDefault, newPath, NULL);
    CFRelease(newPath);
    if ( !newUrl ) {
        return kKEXTReturnNoMemory;
    }

    ret = URLResourceCreateData(newUrl, &xmlData);
    CFRelease(newUrl);
    
    if ( ret != kKEXTReturnSuccess ) {
        _KEXTManagerLogError(manager,
            CFSTR("%@ is not a valid kernel extension bundle.\n"), urlString);
        return kKEXTReturnNotKext;
    }
    
    if ( !xmlData ) {
        _KEXTManagerLogError(manager,
            CFSTR("%@ is not a valid kernel extension bundle.\n"), urlString);
        return kKEXTReturnNotKext;
    }

    *properties = (CFDictionaryRef)CFPropertyListCreateFromXMLData(
                        kCFAllocatorDefault,
                        xmlData,
                        kCFPropertyListImmutable,
                        &str);

    CFRelease(xmlData);

    if ( !*properties ) {
        KEXTError(kKEXTReturnSerializationError, CFSTR("Error obtaining property list"));
        if ( str ) {
            CFRelease(str);
        }
        return kKEXTReturnSerializationError;
    }

    if ( CFDictionaryGetTypeID() != CFGetTypeID(*properties) ) {
        CFRelease(*properties);
        *properties = NULL;
        return kKEXTReturnError;
    }

    return kKEXTReturnSuccess;
}

// This function creates all the necessary entities from the url provided.
static KEXTReturn
_KEXTManagerCreateEntities(
            KEXTManagerRef                           manager,
            CFURLRef                                 url,
            KEXTBundleRef                          * bundle,
            CFArrayRef                             * modules,
            CFArrayRef                             * persons )
{
    KEXTReturn ret;
    CFStringRef urlString;
    CFStringRef bundleKey;
    CFDateRef date;
    CFDictionaryRef properties;
    CFArrayRef keys;
    Boolean isCFStyle;

    if ( !url || !bundle || !modules || !persons ) {
        return kKEXTReturnBadArgument;
    }

    urlString = CFURLGetString(url);

    // Make sure we are a KEXT bundle.
    if (! _KEXTManagerIsKernelExtensionURL(url)) {
        _KEXTManagerLogError(manager,
            CFSTR("%@ is not a valid kernel extension bundle.\n"), urlString);
        return kKEXTReturnNotKext;
    }

    // Check existence of file/folder.
    ret = URLResourceExists(url);
    if ( ret != kKEXTReturnSuccess ) {
        return ret;
    }

    // Check modification times.
    ret = URLResourceCreateModifcationTime(url, &date);
    if ( ret != kKEXTReturnSuccess ) {
        return ret;
    }

    ret = _KEXTManagerCreateProperties(manager, url, &properties, &isCFStyle);
    if ( ret != kKEXTReturnSuccess) {
        _KEXTManagerLogError(manager,
            CFSTR("%@ is not a valid kernel extension bundle.\n"), urlString);
        CFRelease(date);
        return ret;
    }

    *bundle = KEXTBundleCreate(url, properties, isCFStyle);
    if (!*bundle) {
        CFRelease(properties);
        CFRelease(date);
        _KEXTManagerLogError(manager,
            CFSTR("%@ is not a valid kernel extension bundle.\n"), urlString);
        return kKEXTReturnNotKext;
    }
    bundleKey = KEXTBundleGetPrimaryKey(*bundle);

    // Add special keys to the KEXTBundle object.
    *modules = _KEXTManagerCreateModulesArray(bundleKey, properties, &keys, isCFStyle);
    if ( *modules ) {
        if ( keys ) {
            // XXX -- We really don't need this...
            // CFDictionarySetValue(*bundle, CFSTR("BundleModules"), keys);
            CFRelease(keys);
        }
        CFDictionarySetValue(*bundle, CFSTR("BundleType"), CFSTR("KMOD"));
    }

    *persons = _KEXTManagerCreatePersonsArray(bundleKey, properties, &keys, isCFStyle);
    if ( *persons ) {
        if ( keys ) {
            // XXX -- We really don't need this...
            // CFDictionarySetValue(*bundle, CFSTR("BundlePersons"), keys);
            CFRelease(keys);
        }
        CFDictionarySetValue(*bundle, CFSTR("BundleType"), CFSTR("KEXT"));
    }

    CFDictionarySetValue(*bundle, CFSTR("ModificationDate"), date);
    CFRelease(date);

    return kKEXTReturnSuccess;
}

// Return an array of all the entities in the database.
CFArrayRef
KEXTManagerCopyAllEntities(
            KEXTManagerRef                           manager )
{
    void ** vals;
    CFArrayRef array;
    CFIndex count;

    count = CFDictionaryGetCount(deref(manager)->_entities);
    vals = (void **)malloc(sizeof(void *) * count);
    CFDictionaryGetKeysAndValues(deref(manager)->_entities, NULL, vals);
    array = CFArrayCreate(kCFAllocatorDefault, vals, count, &kCFTypeArrayCallBacks);
    free(vals);

    return array;
}

CFArrayRef
KEXTManagerCopyAllBundles(
            KEXTManagerRef                           manager )
{
    CFDictionaryRef entities;
    CFMutableArrayRef array;

    if ( !manager ) {
        return NULL;
    }
    
    entities = deref(manager)->_entities;
    array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if ( !array ) {
        return NULL;
    }
    
    CFDictionaryApplyFunction(
                    entities,
                    (CFDictionaryApplierFunction)DictionaryCopyAllBundles,
                    array);

    return array;
}

CFArrayRef
KEXTManagerCopyAllModules(
            KEXTManagerRef                           manager )
{
    CFDictionaryRef entities;
    CFMutableArrayRef array;

    if ( !manager ) {
        return NULL;
    }

    entities = deref(manager)->_entities;
    array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if ( !array ) {
        return NULL;
    }

    CFDictionaryApplyFunction(
                    entities,
                    (CFDictionaryApplierFunction)DictionaryCopyAllModules,
                    array);

    return array;
}

CFArrayRef
KEXTManagerCopyAllPersonalities(
            KEXTManagerRef                           manager )
{
    CFDictionaryRef entities;
    CFMutableArrayRef array;

    if ( !manager ) {
        return NULL;
    }

    entities = deref(manager)->_entities;
    array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if ( !array ) {
        return NULL;
    }

    CFDictionaryApplyFunction(
                    entities,
                    (CFDictionaryApplierFunction)DictionaryCopyAllPersonalities,
                    array);

    return array;
}

CFArrayRef
KEXTManagerCopyAllConfigs(
            KEXTManagerRef                           manager )
{
    CFDictionaryRef entities;
    CFMutableArrayRef array;

    if ( !manager ) {
        return NULL;
    }

    entities = deref(manager)->_entities;
    array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if ( !array ) {
        return NULL;
    }

    CFDictionaryApplyFunction(
                    entities,
                    (CFDictionaryApplierFunction)DictionaryCopyAllConfigs,
                    array);

    return array;
}


// *The* function which removes a bundle and all of its contents
// from the database.
void
KEXTManagerRemoveBundle(
            KEXTManagerRef                           manager,
            CFStringRef                              primaryKey )
{
    KEXTManager * man = (KEXTManager *)manager;
    KEXTBundleRef bundle;

    if ( !manager || !primaryKey ) {
        return;
    }

    bundle = _KEXTManagerGetEntityWithKey(manager, primaryKey);
    if ( !bundle ) {
        return;
    }

    // First check with the "user" if this bundle should be added or not.
    if ( man->bcb.BundleWillRemove ) {
        if ( !man->bcb.BundleWillRemove(manager, bundle, man->_context) )
            return;
    }

    CFRetain(bundle);
    // Now remove all the bundle items.
    _KEXTManagerRemoveBundleItemsOnly(manager, bundle);
    
    if ( man->bcb.BundleWasRemoved ) {
        man->bcb.BundleWasRemoved(manager, bundle, man->_context);
    }
    CFRelease(bundle);
}

// Add a bundle complete with its contents to the KEXTManager database.
static inline void
_KEXTManagerAddBundleAndContents(
            KEXTManagerRef                           manager,
            KEXTBundleRef                            bundle,
            CFArrayRef                               modules,
            CFArrayRef                               persons )
{
    CFRange range;
    
    // Add bundle descriptor to the KEXTManager database.
    _KEXTManagerAddBundleEntity(manager, bundle);
    
    // Add module descriptors to the KEXTManager database.
    if ( modules ) {
        range = CFRangeMake(0, CFArrayGetCount(modules));
        CFArrayApplyFunction(
                        modules,
                        range,
                        (CFArrayApplierFunction)ArrayAddModuleEntities,
                        manager);
    }
    // Add personality descriptors to the KEXTManager database.
    if ( persons ) {
        range = CFRangeMake(0, CFArrayGetCount(persons));
        CFArrayApplyFunction(
                        persons,
                        range,
                        (CFArrayApplierFunction)ArrayAddPersonalityEntities,
                        manager);
    }
}

// Remove the bundle and it's contents, but don't remove
// config information.  Leave that to the KEXTRemoveBundle
// function.
static void
_KEXTManagerRemoveBundleItemsOnly(
            KEXTManagerRef                           manager,
            KEXTBundleRef                            bundle )
{
    CFStringRef primaryKey;
    CFArrayRef array;
    CFRange range;

    primaryKey = KEXTBundleGetPrimaryKey(bundle);
    if ( !primaryKey ) {
        return;
    }

    array = KEXTManagerCopyModulesForBundle(manager, bundle);
    if ( array ) {
        range = CFRangeMake(0, CFArrayGetCount(array));
        CFArrayApplyFunction(
                    array, range,
                    (CFArrayApplierFunction)ArrayRemoveModuleEntities,
                    manager);
    }

    array = KEXTManagerCopyPersonalitiesForBundle(manager, bundle);
    if ( array ) {
        range = CFRangeMake(0, CFArrayGetCount(array));
        CFArrayApplyFunction(
                    array, range,
                    (CFArrayApplierFunction)ArrayRemovePersonalityEntities,
                    manager);
    }
    
    _KEXTManagerRemoveBundleEntity(manager, bundle);
}

KEXTReturn
KEXTManagerAddBundle(
            KEXTManagerRef                           manager,
            CFURLRef                                 bundleUrl )
{
    return _KEXTManagerAddBundle(manager, bundleUrl, /* toplevel */ true);
}

static KEXTReturn
_KEXTManagerAddBundle(
            KEXTManagerRef	manager,
            CFURLRef		bundleUrl,
            Boolean		toplevel)
{
    KEXTBundleRef bundle;
    KEXTReturn ret;
    CFMutableArrayRef modules;
    CFMutableArrayRef persons;
    CFStringRef primaryKey;

    if ( !manager || !bundleUrl ) {
        return kKEXTReturnBadArgument;
    }

    ret = URLResourceExists(bundleUrl);
    if ( ret != kKEXTReturnSuccess ) {
        return ret;
    }

    ret = deref(manager)->bcb.BundleAuthentication(bundleUrl, deref(manager)->_context);
    if ( ret != kKEXTReturnSuccess ) {
        return ret;
    }

    // Create entries.
    ret = _KEXTManagerCreateEntities(manager, bundleUrl, &bundle, &modules, &persons);
    if ( ret != kKEXTReturnSuccess ) {
        return ret;
    }

    if ( deref(manager)->bcb.BundleWillAdd ) {
        if ( !deref(manager)->bcb.BundleWillAdd(manager, bundle, deref(manager)->_context) ) {
            return kKEXTReturnSuccess;
        }
    }

    // Check if bundle already exists in database.
    primaryKey = KEXTBundleGetPrimaryKey(bundle);
    if ( !primaryKey ) {
        return kKEXTReturnPropertyNotFound;
    }

    ret = kKEXTReturnSuccess;
    do {
        KEXTBundleRef bundle2;

        bundle2 = _KEXTManagerGetEntityWithKey(manager, primaryKey);
        if ( bundle2 ) {
            CFStringRef info1;
            CFStringRef info2;

            // Apparently there is already a bundle in the database.
            info1 = KEXTBundleGetProperty(bundle, CFSTR("BundleInfo"));
            info2 = KEXTBundleGetProperty(bundle2, CFSTR("BundleInfo"));

            if ( (!info2 && !info1)
            ||    (info1 &&  info2 && CFEqual(info1, info2)) ) {
                // These are two different bundles.
                // Let the client application figure it out.
                if ( deref(manager)->bcb.BundleError ) {
                    ret = deref(manager)->bcb.BundleError(
                                    manager,
                                    bundle,
                                    kKEXTReturnBundleAlreadyExists,
                                    deref(manager)->_context);
                }
                if ( ret != kKEXTReturnSuccess ) {
                    break;
                }

            }
            // Replace the bundle.
            _KEXTManagerRemoveBundleItemsOnly(manager, bundle2);
        }

        _KEXTManagerAddBundleAndContents(manager, bundle, modules, persons);

        if ( deref(manager)->bcb.BundleWasAdded ) {
            deref(manager)->bcb.BundleWasAdded(manager, bundle, deref(manager)->_context);
        }
    } while ( false );

    // We may have to recurse into the bundle so lets check.
    if (toplevel) {
        CFStringRef tmpStr;
        CFURLRef pluginURL = 0;

        tmpStr = KEXTBundleGetProperty(bundle, CFSTR("BundleFormat"));
        if ( CFEqual(tmpStr, CFSTR("CF")) ) {
            pluginURL = _KEXTBundleCreateFileURL(bundle,
                                                 CFSTR("PlugIns/"),
                                                 NULL);
        }

        if (pluginURL) {
            // We can ignore sub kext bundle errors
            (void) _KEXTManagerScanBundles(manager,
                                           pluginURL,
                            /* toplevel */ false);
	    CFRelease(pluginURL);
        }
    }

    if ( bundle )
	CFRelease(bundle);
    if ( modules )
	CFRelease(modules);
    if ( persons )
        CFRelease(persons);

    return ret;
}

CFIndex
KEXTManagerGetCount(
            KEXTManagerRef                           manager )
{
    return CFDictionaryGetCount(deref(manager)->_entities);
}

static void DictionaryRemoveBundleWithURL(void * key, void * val, void * context)
{
    CFURLRef url;
    KEXTManagerRef manager;
    KEXTBundleRef bundle;

    manager = context;
    url = val;

    if ( !val || !context ) {
        return;
    }

    bundle = _KEXTManagerGetEntityWithKey(manager, val);
    if ( bundle ) {
        KEXTManagerRemoveBundle(manager, KEXTBundleGetPrimaryKey(bundle));
    }
}

// Checks to see if any bundles have been modified, added, or deleted.
static KEXTReturn
_KEXTManagerScanBundles(
            KEXTManagerRef	manager,
            CFURLRef		url,
            Boolean		toplevel)
{
    CFArrayRef array;
    CFMutableArrayRef kextList;
    CFMutableDictionaryRef urlRelCopy = 0;
    CFIndex count;
    CFIndex index;
    SInt32 err;

    array = (CFArrayRef)IOURLCreatePropertyFromResource(
                                kCFAllocatorDefault, url,
                                kIOURLFileDirectoryContents,
                                &err);
    if ( !array )
        return kKEXTReturnResourceNotFound;

    // Now search for the kernel extensions in the directory.
    kextList = CFArrayCreateMutable
                    (kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    count = CFArrayGetCount(array);
    for ( index = 0; index < count; index++ ) {
        CFURLRef entry;
        CFURLRef url;
        CFStringRef path;

        entry = CFArrayGetValueAtIndex(array, index);
        if (!entry || !_KEXTManagerIsKernelExtensionURL(entry))
            continue;

        entry = CFURLCopyAbsoluteURL(entry);
        path = CFURLGetString(entry);
        url = CFURLCreateWithString(kCFAllocatorDefault, path, NULL);
        CFRelease(entry);

        CFArrayAppendValue(kextList, url);
        CFRelease(url);
    }
    CFRelease(array); array = NULL;

    urlRelCopy = CFDictionaryCreateMutableCopy
                    (kCFAllocatorDefault, 0,  deref(manager)->_urlRels);

    count = CFArrayGetCount(kextList);
    for ( index = 0; index < count; index++ ) {
        CFURLRef kextURL;
        KEXTBundleRef bundle;
        CFStringRef bundleKey;

        kextURL = CFArrayGetValueAtIndex(kextList, index);

        // If the bundle's url already exists, then avoid creating a new one.
        bundleKey = CFDictionaryGetValue(urlRelCopy, kextURL);
        if ( !bundleKey ) {
            // This may possibly be a new bundle.
            _KEXTManagerAddBundle(manager, kextURL, toplevel);
        }
        else {
            // An url to an existing bundle was found.  Check to see
            // if it has been deleted or modified since it was added
            // to the database.
            bundle = _KEXTManagerGetEntityWithKey(manager, bundleKey);
            
            // Has an existing bundle been removed or modified?
            switch ( KEXTBundleValidate(bundle) ) {
                case kKEXTBundleValidateRemoved:
                    KEXTManagerRemoveBundle(manager, bundleKey);
                    break;
                    
                case kKEXTBundleValidateModified:
                    _KEXTManagerRemoveBundleItemsOnly(manager, bundle);
                    _KEXTManagerAddBundle(manager, kextURL, toplevel);
                    break;

                default:
                    break;
            }
            CFDictionaryRemoveValue(urlRelCopy, kextURL);
        }
    }

    if (toplevel) {
        // Now if there are any left over urls, this means the bundles
        // they refer to have been removed.
        CFDictionaryApplyFunction(urlRelCopy, (CFDictionaryApplierFunction)
            DictionaryRemoveBundleWithURL, manager);
    }
    CFRelease(urlRelCopy);
    
    CFRelease(kextList);
    
    return kKEXTReturnSuccess;
}

// Scan the folder for KEXT bundles.
// Also, check for driver configuration files.
KEXTReturn
KEXTManagerScanPath(
            KEXTManagerRef                           manager,
            CFURLRef                                 url )
{
    KEXTReturn error;
    
    if ( !url || !CFURLHasDirectoryPath(url) ) {
        return kKEXTReturnBadArgument;
    }

    error = kKEXTReturnSuccess;

    do {
        error = KEXTManagerScanConfigs(manager, url);
        if ( (error != kKEXTReturnSuccess)
        &&   (error != kKEXTReturnResourceNotFound) ) {
            printf("Error (%d): cannot get configs.\n", error);
            // break;
        }

        error = _KEXTManagerScanBundles(manager, url, /* toplevel */ true);
        if ( error != kKEXTReturnSuccess ) {
            break;
        }
    } while ( false );

    return error;
}

KEXTBundleRef
KEXTManagerGetBundle(
            KEXTManagerRef                           manager,
            CFStringRef                              primaryKey )
{
    if ( !manager || !primaryKey ) {
        return NULL;
    }
    return _KEXTManagerGetEntityWithKey(manager, primaryKey);
}

KEXTBundleRef
KEXTManagerGetBundleWithURL(
            KEXTManagerRef                           manager,
            CFURLRef                                 url )
{
    CFStringRef primaryKey;
    
    if ( !manager || !url ) {
        return NULL;
    }

    primaryKey = CFDictionaryGetValue(deref(manager)->_urlRels, url);
    return KEXTManagerGetBundle(manager, primaryKey);
}

KEXTBundleRef
KEXTManagerGetBundleWithModule(
            KEXTManagerRef                           manager,
            CFStringRef                              moduleName )
{
    KEXTModuleRef module;
    CFStringRef bundleKey;

    if ( !manager || !moduleName ) {
        return NULL;
    }
    
    module = _KEXTManagerGetEntityWithKey(manager, moduleName);
    if ( !module ) {
        return NULL;
    }

    bundleKey = CFDictionaryGetValue(module, CFSTR("ParentKey"));
    
    return _KEXTManagerGetEntityWithKey(manager, bundleKey);
}

KEXTModuleRef
KEXTManagerGetModule(
            KEXTManagerRef                           manager,
            CFStringRef                              moduleName)
{
    KEXTModuleRef module;
    CFStringRef primaryKey;
    
    if ( !manager || !moduleName ) {
        return NULL;
    }

    primaryKey = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("KEXTModule?%@"), moduleName);
    module = _KEXTManagerGetEntityWithKey(manager, primaryKey);
    CFRelease(primaryKey);

    return module;
}

KEXTPersonalityRef
KEXTManagerGetPersonality(
            KEXTManagerRef                           manager,
            CFStringRef                              primaryKey)
{
    if ( !manager || !primaryKey ) {
        return NULL;
    }
    return _KEXTManagerGetEntityWithKey(manager, primaryKey);
}

static Boolean _CFStringToVers(CFStringRef versionString, UInt32 * vers)
{
    Boolean result = true;
    char        vers_buffer[32];  // more than long enough for legal vers

    if (!CFStringGetCString(versionString,
        vers_buffer, sizeof(vers_buffer) - 1, kCFStringEncodingMacRoman)) {
        result = false;
        goto finish;
    }

    if (!VERS_parse_string(vers_buffer, vers)) {
        result = false;
        goto finish;
    }
finish:
    return result;
}

static Boolean _KEXTManagerGetModuleVers(KEXTManagerRef manager,
    KEXTModuleRef module,
    CFStringRef versionKey,
    UInt32 * vers)
{
    Boolean result = true;
    CFStringRef thisName = NULL;
    CFStringRef thisVersion = NULL;
    char        vers_buffer[32];  // more than long enough for legal vers

    thisName = KEXTModuleGetProperty(module, CFSTR(kNameKey));
    if (!thisName) {
        _KEXTManagerLogError(manager,
            CFSTR("Extension has no \"CFBundleIdentifier\" property.\n"));
        result = false;
        goto finish;
    }

    thisVersion = KEXTModuleGetProperty(module, versionKey);
    if (!thisVersion) {
        _KEXTManagerLogError(manager,
            CFSTR("Extension %@ has no \"%@\" property.\n"),
            thisName, versionKey);
        result = false;
        goto finish;
    }

    if (!CFStringGetCString(thisVersion,
        vers_buffer, sizeof(vers_buffer) - 1, kCFStringEncodingMacRoman)) {

        _KEXTManagerLogError(manager,
            CFSTR("Can't extract version for extension %@.\n"),
            thisName);
        result = false;
        goto finish;
    }

    if (!VERS_parse_string(vers_buffer, vers)) {

        _KEXTManagerLogError(manager,
            CFSTR("Extension %@ has an invalid version string.\n"),
            thisName);
        result = false;
        goto finish;
    }
finish:
    return result;
}


// Create a dependency list for loading the given module.  
// If the list is incomplete, the function will return false.
static KEXTReturn
_KEXTManagerGraphDependencies(
            KEXTManagerRef                           manager,
            KEXTModuleRef                            module,
            CFStringRef                              requestedModuleName,
            CFMutableArrayRef                        array )
{
    KEXTReturn result = kKEXTReturnSuccess;
    CFDictionaryRef dependencies = NULL;  // do not release
    KEXTModuleRef mod = NULL;             // do not release
    CFTypeID scratch = NULL;              // do not release
    Boolean isKernelResource = false;
    CFRange range;

    CFIndex    numDependencies = 0;
    void **    nameList = NULL;      // must free
    void **    versionList = NULL;   // must free
    CFArrayRef nameArray = NULL;     // must release
    CFArrayRef versionArray = NULL;  // must release
    CFIndex    i;

    if ( !manager || !module || !array) {
        result = kKEXTReturnBadArgument;
        goto finish;
    }

    if (CFArrayGetCount(array) > 255) {
            _KEXTManagerLogError(manager,
                CFSTR("Dependency list for %@ is "
                "ridiculously long (circular reference?).\n"),
                requestedModuleName);
            result = kKEXTReturnError;
            goto finish;
    }


    // Check to see if a special properties exists for this "module".
    scratch = KEXTModuleGetProperty(module, CFSTR("OSKernelResource"));
    if ( scratch ) {
       isKernelResource = CFBooleanGetValue(scratch);
    }

   /* All KEXTs must declare their dependencies, at least on the kernel.
    */
    if (isKernelResource) {
        result = kKEXTReturnSuccess;
        goto finish;
    }

    dependencies = KEXTModuleGetProperty(module, CFSTR(kRequiresKey));
    if ( ! dependencies ) {
        _KEXTManagerLogError(manager,
            CFSTR("Extension %@ "
            "declares no dependencies.\n"),
            requestedModuleName);
#if 0
// make a failure after folks have converted kexts
        result = kKEXTReturnError;
        goto finish;
#else
        _KEXTManagerLogError(manager,
            CFSTR("Loading anyway.\n"));
        goto no_dependencies;
#endif 0
    }

    if (CFGetTypeID(dependencies) != CFDictionaryGetTypeID() ) {
        result = kKEXTReturnError;
        goto finish;
    }

    numDependencies = CFDictionaryGetCount(dependencies);
    if (numDependencies < 1) {
        // FIXME: Make this an error when all kexts properly
        // FIXME: declare their kernel dependencies.
        goto no_dependencies;
    }

    nameList    = (void **)malloc(numDependencies * sizeof(void *));
    if (! nameList) {
        result = kKEXTReturnNoMemory;
        goto finish;
    }

    versionList = (void **)malloc(numDependencies * sizeof(void *));
    if (! versionList) {
        result = kKEXTReturnNoMemory;
        goto finish;
    }

    CFDictionaryGetKeysAndValues(dependencies, nameList, versionList);

    nameArray = CFArrayCreate(NULL, nameList, numDependencies, 
        &kCFTypeArrayCallBacks);
    if (! nameArray) {
        result = kKEXTReturnNoMemory;
        goto finish;
    }

    versionArray = CFArrayCreate(NULL, versionList, numDependencies, &kCFTypeArrayCallBacks);
    if (! versionArray) {
        result = kKEXTReturnNoMemory;
        goto finish;
    }

    for ( i = 0; i < numDependencies; i++ ) {
        CFStringRef name;
        CFStringRef version;

        UInt32  req_vers;
        UInt32  dep_vers;
        UInt32  dep_compat_vers;

        name = CFArrayGetValueAtIndex(nameArray, i);
        if ( !name ) continue;

        version = CFArrayGetValueAtIndex(versionArray, i);
        if ( !version ) {
            _KEXTManagerLogError(manager,
                CFSTR("No required version for dependency %@ of extension %@.\n"),
                name, requestedModuleName);
            result = kKEXTReturnError;
            goto finish;
        }

        if (!_CFStringToVers(version, &req_vers)) {
            _KEXTManagerLogError(manager,
                CFSTR("Can't parse required version for dependency %@ "
                "of extension %@.\n"),
                name, requestedModuleName);
            result = kKEXTReturnError;
            goto finish;
        }

        mod = KEXTManagerGetModule(manager, name);
        if ( !mod ) {
            _KEXTManagerLogError(manager,
                CFSTR("Can't find dependency %@ of extension %@.\n"),
                name, requestedModuleName);
            result = kKEXTReturnMissingDependency;
            goto finish;
        }

        if (!_KEXTManagerGetModuleVers(manager, mod,
            CFSTR("CFBundleVersion"), &dep_vers)) {
            _KEXTManagerLogError(manager,
                CFSTR("Dependency %@ of extension %@ "
                "declares no version.\n"),
                name, requestedModuleName);

#if 0
// make a failure after folks have converted kexts
            result = kKEXTReturnError;
            goto finish;

#else
            _KEXTManagerLogError(manager,
                CFSTR("Loading anyway.\n"));
#endif 0
        }

        if (!_KEXTManagerGetModuleVers(manager, mod,
            CFSTR("OSBundleCompatibleVersion"),
            &dep_compat_vers)) {

            _KEXTManagerLogError(manager,
                CFSTR("Dependency %@ of extension %@ "
                "declares no compatible version.\n"),
                name, requestedModuleName);
#if 0
// make a failure after folks have converted kexts
            result = kKEXTReturnError;
            goto finish;
#else
            _KEXTManagerLogError(manager,
                CFSTR("Loading anyway.\n"));
#endif 0
        }

        if (req_vers > dep_vers || req_vers < dep_compat_vers) {
            _KEXTManagerLogError(manager,
                CFSTR("Installed version %@ of dependency %@ is not compatible "
                "with extension %@.\n"),
                version, name, requestedModuleName);
#if 0
// make a failure after folks have converted kexts
            result = kKEXTReturnDependencyVersionMismatch;
            goto finish;
#else
            _KEXTManagerLogError(manager,
                CFSTR("Loading anyway.\n"));
#endif 0
        }

        range = CFRangeMake(0, CFArrayGetCount(array));
        if ( !CFArrayContainsValue(array, range, mod)) {
            result = _KEXTManagerGraphDependencies(manager, mod,
                requestedModuleName, array);
            if (result != kKEXTReturnSuccess) {
                goto finish;
            }
        }
    }

no_dependencies:

    // Prevent "special" modules from being added to the dependency list
    // as used by kmodload.
    if ( ! isKernelResource ) {
        if (numDependencies < 1) {
            _KEXTManagerLogError(manager,
                CFSTR("Extension %@ declares no dependencies.\n"),
                requestedModuleName);
// FIXME: Make this an error.
            _KEXTManagerLogError(manager,
                CFSTR("Loading anyway.\n"));
        }
        range = CFRangeMake(0, CFArrayGetCount(array));
        if ( !CFArrayContainsValue(array, range, module) ) {
            CFArrayAppendValue(array, module);
        }
    }

finish:
    // do not release dependencies
    // do not release mod
    // do not release scratch
    if (nameList)     free(nameList);
    if (versionList)  free(versionList);
    if (nameArray)    CFRelease(nameArray);
    if (versionArray) CFRelease(versionArray);

    return result;
}

Boolean
KEXTManagerCopyModuleDependencies(
            KEXTManagerRef                           manager,
            KEXTModuleRef                            module,
            CFArrayRef                             * array )
{
    KEXTReturn graphDependencyResult;
    CFStringRef moduleName;
    CFMutableArrayRef modules;
    Boolean ret;

    if ( !manager || !module || !array ) {
        return false;
    }
    
    moduleName = KEXTModuleGetProperty(module, CFSTR(kNameKey));
    if ( !moduleName ) {
        return false;
    }

    modules = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if ( !modules ) {
        return false;
    }

    graphDependencyResult = _KEXTManagerGraphDependencies(manager, module,
        moduleName, modules);
    if (graphDependencyResult == kKEXTReturnSuccess) {
        ret = true;
    } else {
        ret = false;
    }

    // Return the contents, even if there is an error since it
    // will have every dependency up to the point of the failure.
    *array = modules;

    return ret;
}

// Load a module.
KEXTReturn
KEXTManagerLoadModule(
            KEXTManagerRef                           manager,
            KEXTModuleRef                            module )
{
    KEXTManager * man;

    KEXTReturn graphDependencyResult;
    CFStringRef moduleName;
    CFMutableArrayRef array;
    CFStringRef primaryKey;
    CFStringRef mode;
    CFIndex count;
    CFIndex i;
    KEXTReturn error;

    if ( !manager || !module )
        return kKEXTReturnBadArgument;

    primaryKey = KEXTModuleGetPrimaryKey(module);
    if ( !primaryKey ) {
        return kKEXTReturnError;
    }

    moduleName = KEXTModuleGetProperty(module, CFSTR(kNameKey));
    if ( !moduleName ) {
        return kKEXTReturnError;
    }

    man = (KEXTManager *)manager;

    // Check the noloads list for this module.  If it's
    // not to be loaded, then don't load it.
    mode = CFDictionaryGetValue(man->_noloads, primaryKey);
    if ( mode && CFEqual(mode, CFSTR("Disabled")) ) {
        return kKEXTReturnModuleDisabled;
    }

    if ( KEXTModuleIsLoaded(module, &error) ) {
        if ( error == kKEXTReturnSuccess && man->mcb.ModuleError )
            error = man->mcb.ModuleError(manager, module, kKEXTReturnModuleAlreadyLoaded, man->_context);

        return error;
    }

    if (!KEXTManagerCheckSafeBootForModule(manager, module)) {
        _KEXTManagerLogError(manager,
            CFSTR("Extension %@ is not a safe-boot extension.\n"), moduleName);
        return kKEXTReturnModuleDisabled;
    }

    array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if ( !array ) {
        return kKEXTReturnNoMemory;
    }

    graphDependencyResult = _KEXTManagerGraphDependencies(manager, module,
        moduleName, array);
    if (graphDependencyResult != kKEXTReturnSuccess) {
        CFRelease(array);
        return graphDependencyResult;
    }

    error = kKEXTReturnSuccess;
    count = CFArrayGetCount(array);
    for ( i = 0; i < count; i++ ) {
        KEXTModuleRef mod1;
        KEXTReturn err;
        CFMutableArrayRef deps;
        CFIndex j;

        mod1 = (KEXTModuleRef)CFArrayGetValueAtIndex(array, i);
        if ( !mod1 )
            continue;

        if ( KEXTModuleIsLoaded(mod1, &err) ) {
            if ( man->mcb.ModuleError ) {
                error = man->mcb.ModuleError(manager, mod1, kKEXTReturnModuleAlreadyLoaded, man->_context);
                if ( (error != kKEXTReturnSuccess) &&
                     (error != kKEXTReturnModuleAlreadyLoaded) )
                    break;
            }
            continue;
        }
        
        if ( man->mcb.ModuleWillLoad ) {
            if ( !man->mcb.ModuleWillLoad(manager, mod1, man->_context) )
                continue;
        }
        deps = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        for ( j = 0; j < i; j++ ) {
            KEXTModuleRef dep;

            dep = (KEXTModuleRef)CFArrayGetValueAtIndex(array, j);
            CFArrayAppendValue(deps, dep);
        }
        error = _KEXTManagerLoadModule(manager, mod1, deps);
        CFRelease(deps);
        if ( (error != kKEXTReturnSuccess) && man->mcb.ModuleError ) {
            error = man->mcb.ModuleError(manager, mod1, error, man->_context);
        }
        if ( error != kKEXTReturnSuccess )
            break;

        if ( man->mcb.ModuleWasLoaded ) {
            man->mcb.ModuleWasLoaded(manager, mod1, man->_context);
        }
    }
    CFRelease(array);

    return error;
}

KEXTReturn
KEXTManagerUnloadModule(
            KEXTManagerRef                           manager,
            KEXTModuleRef                            module )
{
    KEXTReturn ret;
    CFStringRef modName;
    kern_return_t status;
    char name[256];

    ret = kKEXTReturnError;

    if ( !manager || !module )
        return kKEXTReturnBadArgument;

    modName = KEXTModuleGetProperty(module, CFSTR(kNameKey));
    if ( !modName || !CFStringGetCString(modName, name, 256, kCFStringEncodingMacRoman) ) {
        return kKEXTReturnError;
    }

    if ( deref(manager)->mcb.ModuleWillUnload ) {
        if ( !deref(manager)->mcb.ModuleWillUnload(manager, module, deref(manager)->_context) ) {
            return kKEXTReturnSuccess;
        }
    }

    status = IOCatalogueTerminate(deref(manager)->_catPort, kIOCatalogModuleUnload, name);
    ret = KERN2KEXTReturn(status);

    if ( (ret != kKEXTReturnSuccess) &&
         deref(manager)->mcb.ModuleError ) {
        ret = deref(manager)->mcb.ModuleError(manager, module, ret, deref(manager)->_context);
    }
    if ( (ret == kKEXTReturnSuccess ) &&
         deref(manager)->mcb.ModuleWasUnloaded ) {
        deref(manager)->mcb.ModuleWasUnloaded(manager, module, deref(manager)->_context);
    }

    return ret;
}

static void ArrayPersonalityWillLoad(const void * val, void * context[])
{
    KEXTManagerRef manager;
    KEXTPersonalityRef personality;
    CFMutableArrayRef toload;
    Boolean boolval;

    personality = (KEXTPersonalityRef)val;
    manager = context[0];
    toload = context[1];

    boolval = true;
    if ( deref(manager)->pcb.PersonalityWillLoad ) {
        boolval = deref(manager)->pcb.PersonalityWillLoad(
                                    manager,
                                    personality,
                                    deref(manager)->_context);
    }

    if ( true ) {
        CFArrayAppendValue(toload, personality);
    }
}

static void ArrayPersonalityCollectProperties(const void * val, void * context)
{
    KEXTPersonalityRef person;
    CFDictionaryRef props;
    CFMutableArrayRef properties;

    person = (KEXTPersonalityRef)val;
    properties = context;

    props = CFDictionaryGetValue(person, CFSTR("PersonProperties"));
    if ( props ) {
        CFArrayAppendValue(properties, props);
    }
}

static void ArrayPersonalityPostamble(const void * val, void * context[])
{
    KEXTManagerRef manager;
    KEXTPersonalityRef person;
    KEXTReturn error;

    person = (KEXTPersonalityRef)val;
    manager = context[0];
    error = *(KEXTReturn *)context[1];

    if ( (error != kKEXTReturnSuccess) &&
         deref(manager)->pcb.PersonalityError ) {
        error = deref(manager)->pcb.PersonalityError(
                                    manager,
                                    person,
                                    error,
                                    deref(manager)->_context);
    }
    if ( error != kKEXTReturnSuccess ) {
        return;
    }
    if ( deref(manager)->pcb.PersonalityWasLoaded ) {
        deref(manager)->pcb.PersonalityWasLoaded(
                                    manager,
                                    person,
                                    deref(manager)->_context);
    }
}

KEXTReturn
KEXTManagerLoadPersonalities(
            KEXTManagerRef                           manager,
            CFArrayRef                               personalities )
{
    CFMutableArrayRef toload;
    CFMutableArrayRef properties;
    CFRange range;
    KEXTReturn error;
    void * context[2];

    if ( !manager || !personalities ) {
        return kKEXTReturnBadArgument;
    }

    toload = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if ( !toload ) {
        return kKEXTReturnNoMemory;
    }
    
    properties = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if ( !properties ) {
        CFRelease(toload);
        return kKEXTReturnNoMemory;
    }

    context[0] = manager;
    context[1] = toload;

    range = CFRangeMake(0, CFArrayGetCount(personalities));
    CFArrayApplyFunction(personalities,
                         range,
                         (CFArrayApplierFunction)ArrayPersonalityWillLoad,
                         context);

    if ( CFArrayGetCount(personalities) < 1 ) {
        return kKEXTReturnSuccess;
    }
    
    range = CFRangeMake(0, CFArrayGetCount(toload));
    CFArrayApplyFunction(toload,
                         range,
                         (CFArrayApplierFunction)ArrayPersonalityCollectProperties,
                         properties);

    error = KEXTSendDataToCatalog(deref(manager)->_catPort, kIOCatalogAddDrivers, properties);
    CFRelease(properties);

    context[0] = manager;
    context[1] = &error;
    
    CFArrayApplyFunction(toload,
                         range,
                         (CFArrayApplierFunction)ArrayPersonalityPostamble,
                         context);

    CFRelease(toload);
    
    return error;
}

KEXTReturn
KEXTManagerUnloadPersonality(
            KEXTManagerRef                           manager,
            KEXTPersonalityRef                       personality )
{
    KEXTReturn error;
    
    if ( !manager || !personality ) {
        return kKEXTReturnBadArgument;
    }

    if ( deref(manager)->pcb.PersonalityWillUnload ) {
        if ( !deref(manager)->pcb.PersonalityWillUnload(manager, personality, deref(manager)->_context) ) {
            return kKEXTReturnSuccess;
        }
    }
    error = _KEXTManagerUnloadPersonality(manager, personality);
    if ( (error != kKEXTReturnSuccess) &&
         deref(manager)->pcb.PersonalityError ) {
        error = deref(manager)->pcb.PersonalityError(manager, personality, error, deref(manager)->_context);
    }
    if ( (error == kKEXTReturnSuccess) &&
         deref(manager)->pcb.PersonalityWasUnloaded ) {
        deref(manager)->pcb.PersonalityWasUnloaded(manager, personality, deref(manager)->_context);
    }

    return error;
}

// @@@gvdl: Talk to simon about this.  How does kextunload work.
KEXTReturn
KEXTManagerTerminatePersonality(
            KEXTManagerRef                           manager,
            KEXTPersonalityRef                       personality )
{
    kern_return_t status;
    KEXTReturn ret;
    CFStringRef str;
    char name[256];

    ret = kKEXTReturnError;

    if ( !manager || !personality ) {
        return kKEXTReturnBadArgument;
    }

    // XXX -- this should be changed to do a more exact match of the personality.
    // This way we can target individual drivers to terminate.
    str = KEXTPersonalityGetProperty(personality, CFSTR("IOClass"));
    if ( str ) {
        if ( !CFStringGetCString(str, name, 256, kCFStringEncodingMacRoman) ) {
            return kKEXTReturnError;
        }

        status = IOCatalogueTerminate(
                            deref(manager)->_catPort,
                            kIOCatalogServiceTerminate,
                            name);

        ret = KERN2KEXTReturn(status);
    }
    else {
        ret = kKEXTReturnPropertyNotFound;
    }

    return ret;
}

CFStringRef
KEXTManagerGetEntityType(
            KEXTEntityRef                            entity)
{
    if ( !entity ) {
        return NULL;
    }
    return CFDictionaryGetValue(entity, CFSTR("EntityType"));
}

mach_port_t
_KEXTManagerGetMachPort(
            KEXTManagerRef                           manager )
{
    return deref(manager)->_catPort;
}


static void logCFString(void (*logFunc)(const char *),
    CFStringRef format,
    va_list arguments)
{
    CFStringRef string = 0;
    unsigned stringLength;
    unsigned bufferLength;
    char * buffer = NULL;

    string = CFStringCreateWithFormatAndArguments(
        NULL, NULL, format, arguments);
    if (!string) return;

    stringLength = CFStringGetLength(string);
    bufferLength = (1 + stringLength) * sizeof(char);
    buffer = (char *)malloc(bufferLength);
    if (!buffer) return;

    if (!CFStringGetCString(string, buffer, bufferLength,
        kCFStringEncodingMacRoman)) {
        goto finish;
    }
        
    logFunc(buffer);

finish:
    if (buffer) free(buffer);
    if (string) CFRelease(string);
    return;
}

static void _KEXTManagerLogError(KEXTManagerRef manager,
    CFStringRef format, ...)
{
    KEXTManager * mgr = (KEXTManager *)manager;
    va_list args;

    if (!mgr->_logErrorFunc) return;

    va_start(args, format);
    logCFString(mgr->_logErrorFunc, format, args);
    va_end(args);

    return;
}

static void _KEXTManagerLogMessage(KEXTManagerRef manager,
    CFStringRef format, ...)
{
    KEXTManager * mgr = (KEXTManager *)manager;
    va_list args;

    if (!mgr->_logMessageFunc) return;

    va_start(args, format);
    logCFString(mgr->_logMessageFunc, format, args);
    va_end(args);

    return;
}


void
_KEXTManagerShow(
            KEXTManagerRef                           manager )
{
    printf("entities:");CFShow(deref(manager)->_entities);
    printf("modules:");CFShow(deref(manager)->_modRels);
    printf("persons:");CFShow(deref(manager)->_perRels);
    printf("urls   :");CFShow(deref(manager)->_urlRels);
    printf("configs:");CFShow(deref(manager)->_cfgRels);
    printf("noloads:");CFShow(deref(manager)->_noloads);
    printf("date   :");CFShow(deref(manager)->_configsDate);
}



//***********************************************************//
//
// KEXT configuration stuff.
//
//
//***********************************************************//

static void CopyNoLoads(const void * key, const void * val, void * context)
{
    CFMutableArrayRef array;
    CFDictionaryRef dict;
    const void * keys[2];
    const void * vals[2];

    if ( !val || !key ) {
        return;
    }

    array = context;

    keys[0] = CFSTR("PrimaryKey"); vals[0] = key;
    keys[1] = CFSTR("LoadValue");  vals[1] = val;
    
    dict = CFDictionaryCreate(kCFAllocatorDefault, keys, vals, 2,
                              &kCFTypeDictionaryKeyCallBacks,
                              &kCFTypeDictionaryValueCallBacks);
    
    CFArrayAppendValue(array, dict);
    CFRelease(dict);
}

// Save always saves to the current database no matter what mode
// we are in.
KEXTReturn
KEXTManagerSaveConfigs(
            KEXTManagerRef                           manager,
            CFURLRef                                 url )
{
    CFArrayRef array;
    CFMutableArrayRef noloads;
    CFDictionaryRef dict;
    CFDataRef data;
    CFURLRef path;
    SInt32 error;
    KEXTReturn ret;
    const void * vals[2];
    const void * keys[2];

    if ( !manager || !url ) {
        return kKEXTReturnBadArgument;
    }
    
    array = KEXTManagerCopyAllConfigs(manager);
    if ( !array ) {
        return kKEXTReturnNoMemory;
    }

    noloads = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if ( !noloads ) {
        CFRelease(array);
        return kKEXTReturnNoMemory;
    }

    // Get the list of non-loadable modules.
    CFDictionaryApplyFunction(deref(manager)->_noloads, (CFDictionaryApplierFunction)CopyNoLoads, noloads);
    
    if ( (CFArrayGetCount(array) < 1) &&
         (CFArrayGetCount(noloads) < 1) ) {
        CFRelease(array);
        CFRelease(noloads);
        return kKEXTReturnSuccess;
    }

    keys[0] = CFSTR("Configs");  vals[0] = array;
    keys[1] = CFSTR("Modules");  vals[1] = noloads;
    
    dict = CFDictionaryCreate(kCFAllocatorDefault, keys, vals, 2,
                              &kCFTypeDictionaryKeyCallBacks,
                              &kCFTypeDictionaryValueCallBacks);

    CFRelease(array);
    CFRelease(noloads);
    if ( !dict ) {
        return kKEXTReturnNoMemory;
    }
    
    // XXX -- when in normal mode, move the current configuration
    // database to the previous database and save the new one as
    // current.  Otherwise just save as current.
    if ( deref(manager)->_mode == kKEXTManagerDefaultMode ) {
        // XXX -- CFURL has no way to rename a resource, it looks
        // like we'll need to use UNIX filesystem API.
#warning finish me.
    }

    data = CFPropertyListCreateXMLData(kCFAllocatorDefault, dict);
    CFRelease(dict);
    if ( !data ) {
        return kKEXTReturnNoMemory;
    }

    ret = kKEXTReturnSuccess;
    do {
        path = CFURLCreateCopyAppendingPathComponent(
                            kCFAllocatorDefault,
                            url,
                            CFSTR("Configuration.current"),
                            false);
        if ( !path ) {
            ret = kKEXTReturnNoMemory;
            break;
        }

        ret = IOURLWriteDataAndPropertiesToResource(path, data, NULL, &error);
        CFRelease(path);
        if ( ret != kKEXTReturnSuccess ) {
            break;
        }
    } while ( false );

    CFRelease(data);
    
    return ret;
}

static void RemoveEntitiesWithConfig(const void * val, void * context)
{
    KEXTConfigRef config1;
    KEXTConfigRef config2;
    CFMutableDictionaryRef dict;
    CFStringRef primaryKey;

    config1 = (KEXTConfigRef)val;
    dict = context;

    primaryKey = KEXTPersonalityGetPrimaryKey(config1);
    if ( !primaryKey ) {
        return;
    }

    // Remove values from dict if they are still present and not modified.  
    // Whatever is leftover in dict will be removed from the database.
    config2 = (KEXTConfigRef)CFDictionaryGetValue(dict, primaryKey);
    if ( !config2 ) {
        return;
    }
    if ( !CFEqual(config1, config2) ) {
        return;
    }
    
    CFDictionaryRemoveValue(dict, primaryKey);
}

static void ArrayRemoveConfigsWithKey(const void * val, void * context)
{
    KEXTManagerRef manager;
    KEXTConfigRef config;

    if ( !val || !context ) {
        return;
    }
    
    manager = context;
    config = KEXTManagerGetConfig(manager, val);
    if ( config ) {
        _KEXTManagerRemoveConfigEntityWithCallback(manager, config);
    }
}

static void DictionaryRemoveFromConfigs(const void * key, const void * val, void * context)
{
    CFArrayRef configKeys;

    configKeys = (CFArrayRef)val;

    if ( configKeys ) {
        CFRange range;
        
        range = CFRangeMake(0, CFArrayGetCount(configKeys));
        CFArrayApplyFunction(configKeys, range, ArrayRemoveConfigsWithKey, context);
    }
}

static void
_KEXTManagerRemoveMissingConfigs(
            KEXTManagerRef                           manager,
            CFArrayRef                               configs )
{
    CFMutableDictionaryRef dict;
    CFRange range;

    if ( !manager || !deref(manager)->_cfgRels || !configs ) {
        return;
    }
    
    dict = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, deref(manager)->_cfgRels);
    if ( !dict ) {
        return;
    }

    // Find common configs.  Remove them from dict. When done,
    // whatever is in dict needs to be removed from configs database.
    range = CFRangeMake(0, CFArrayGetCount(configs));
    CFArrayApplyFunction(configs, range, RemoveEntitiesWithConfig, dict);
    
    CFDictionaryApplyFunction(dict, DictionaryRemoveFromConfigs, manager);

    CFRelease(dict);
}

static void ArrayAddConfigEntityAndSignal(void * val, void * context)
{
    KEXTManagerRef manager;
    KEXTConfigRef config;

    manager = context;
    config = val;

    KEXTManagerAddConfig(manager, config);
}

static void
_KEXTManagerAddNewConfigs(
            KEXTManagerRef                           manager,
            CFArrayRef                               configs )
{
    CFRange range;

    if ( !manager || !configs ) {
        return;
    }

    range = CFRangeMake(0, CFArrayGetCount(configs));
    CFArrayApplyFunction(configs, range, (CFArrayApplierFunction)ArrayAddConfigEntityAndSignal, manager);
}

static void AddNoLoads(void * val, void * context)
{
    CFMutableDictionaryRef noloads;
    CFDictionaryRef modconf;
    CFStringRef primaryKey;
    CFStringRef loadValue;

    noloads = context;
    modconf = val;

    primaryKey = CFDictionaryGetValue(modconf, CFSTR("PrimaryKey"));
    loadValue = CFDictionaryGetValue(modconf, CFSTR("LoadValue"));

    CFDictionarySetValue(noloads, primaryKey, loadValue);
}

// Read database from disk and udate the in-memory configs database.
KEXTReturn
KEXTManagerScanConfigs(
            KEXTManagerRef                           manager,
            CFURLRef                                 url )
{
    CFDateRef date;
    CFDataRef data;
    CFStringRef path;
    CFStringRef tail;
    CFStringRef str;
    CFURLRef newUrl;
    CFDictionaryRef dict;
    CFArrayRef configs;
    CFArrayRef noloads;
    KEXTReturn error;
    SInt32 cferr;

    if ( !manager ) {
        return kKEXTReturnBadArgument;
    }

    switch ( deref(manager)->_mode ) {
            case kKEXTManagerInstallMode:
                tail = CFSTR("install");
                break;
            case kKEXTManagerRecoveryMode:
                tail = CFSTR("previous");
                break;
            case kKEXTManagerDefaultMode:
                tail = CFSTR("current");
                break;
            default:
                tail = NULL;
                break;
    }

    // Are we in some strange mode of operation?!?!?
    if ( !tail ) {
        return kKEXTReturnBadArgument;
    }

    path = CFURLGetString(url);
    path = CFStringCreateWithFormat(
                            kCFAllocatorDefault,
                            NULL,
                            CFSTR("%@/Configuration.%@"),
                            path,
                            tail);
    if ( !path ) {
        return kKEXTReturnNoMemory;
    }

    newUrl =  CFURLCreateWithString(
                            kCFAllocatorDefault,
                            path,
                            NULL);
    CFRelease(path);
    if ( !newUrl ) {
        return kKEXTReturnNoMemory;
    }

    error = KEXTManagerAuthenticateURL(newUrl);
    if ( (error != kKEXTReturnSuccess) ) {
        CFRelease(newUrl);
        return error;
    }

    // Get the current modification time for the file.
    date = IOURLCreatePropertyFromResource(
                            kCFAllocatorDefault,
                            url,
                            kIOURLFileLastModificationTime,
                            &cferr);

    if ( !date ) {
        CFRelease(newUrl);
        return kKEXTReturnError;
    }

    // Check the date for the database.
    // If there isn't one, then the database is new.
    // If there is one, and the dates differ, then configuration database
    // has been updated.
    if ( deref(manager)->_configsDate ) {

        // If there isn't any change, then we needn't do anything.
        if ( CFEqual(deref(manager)->_configsDate, date) ) {
            CFRelease(date);
            return kKEXTReturnSuccess;
        }

        CFRelease(deref(manager)->_configsDate);
        deref(manager)->_configsDate = NULL;
    }

    deref(manager)->_configsDate = date;

    // Load the configs database from disk.
    error = URLResourceCreateData(newUrl, &data);
    CFRelease(newUrl);
    if (  error != kKEXTReturnSuccess ) {
        return error;
    }

    dict = (CFDictionaryRef)CFPropertyListCreateFromXMLData(
                    kCFAllocatorDefault,
                    data,
                    kCFPropertyListMutableContainersAndLeaves,
                    &str);

    CFRelease(data);
    if ( !dict ) {
        if ( deref(manager)->_configsDate ) {
            CFRelease(deref(manager)->_configsDate);
            deref(manager)->_configsDate = NULL;
        }
        if ( str ) {
            CFRelease(str);
        }
        return kKEXTReturnSerializationError;
    }

    configs = CFDictionaryGetValue(dict, CFSTR("Configs"));
    if ( configs ) {
        // Dump all unused configs and signal the client app.
        _KEXTManagerRemoveMissingConfigs(manager, configs);

        // Now add the new configs.
        _KEXTManagerAddNewConfigs(manager, configs);
    }

    noloads = CFDictionaryGetValue(dict, CFSTR("Modules"));
    if ( noloads ) {
        CFRange range;
        
        // Clean out the old configs.
        CFDictionaryRemoveAllValues(deref(manager)->_noloads);
        
        // Get the list of modules NOT to load.
        range = CFRangeMake(0, CFArrayGetCount(noloads));
        CFArrayApplyFunction(noloads, range, (CFArrayApplierFunction)AddNoLoads, deref(manager)->_noloads);
    }
    CFRelease(dict);

    return kKEXTReturnSuccess;
}

KEXTConfigRef
KEXTManagerCreateConfig(
            KEXTManagerRef                           manager,
            KEXTPersonalityRef                       personality )
{
    KEXTPersonalityRef config;
    CFDictionaryRef props;
    CFMutableDictionaryRef properties;
    Boolean found;

    if ( !manager || !personality ) {
        return NULL;
    }
    
    found = false;
    config = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, personality);
    props = CFDictionaryGetValue(config, CFSTR("PersonProps"));
    if ( props ) {
        properties = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, props);
        CFDictionarySetValue(config, CFSTR("PersonProps"), properties);
        CFRelease(properties);
    }
    if ( config ) {
        CFStringRef key;
        CFStringRef newKey;
        CFIndex index;

        key = CFDictionaryGetValue(config, CFSTR("PrimaryKey"));
        found = false;
        for ( index = 0; index < 0xffff; index++ ) {
            newKey = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@?%x"), key, index);
            if ( !CFDictionaryGetValue(deref(manager)->_entities, newKey) ) {
                CFMutableDictionaryRef dict;
                CFStringRef name;
                CFStringRef newName;
                CFNumberRef score;
                CFIndex num;

                dict = (CFMutableDictionaryRef)CFDictionaryGetValue(config, CFSTR("PersonProperties"));

                CFDictionarySetValue(config, CFSTR("IsConfig"), kCFBooleanTrue);
                CFDictionarySetValue(config, CFSTR("PrimaryKey"), newKey);

                if ( !dict ) {
                    break;
                }

                name = CFDictionaryGetValue(dict, CFSTR("Name"));
                if ( name ) {
                    newName = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@?%x"), name, index);
                    CFDictionarySetValue(dict, CFSTR("Name"), newName);
                    CFRelease(newName);
                }
                // Create a new probe score, make it higher than in the original personality.
                num = 0;
                score = KEXTPersonalityGetProperty(personality, CFSTR("IOProbeScore"));
                if ( score ) {
                    CFNumberGetValue(score, kCFNumberSInt32Type, &num);
                }

                num += 10;
                score = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &num);
                if ( score ) {
                    CFDictionarySetValue(dict, CFSTR("IOProbeScore"), score);
                    CFRelease(score);
                }
                
                CFRelease(newKey);
                found = true;
                break;
            }
            CFRelease(newKey);
        }
    }

    if ( !found && config ) {
        CFRelease(config);
        config = NULL;
    }
    
    return config;
}

void
KEXTManagerAddConfig(
            KEXTManagerRef                           manager,
            KEXTConfigRef                            config )
{
    Boolean ret;
    
    if ( !manager || !config ) {
        return;
    }

    ret = true;
    if ( deref(manager)->ccb.ConfigWillAdd ) {
        ret = deref(manager)->ccb.ConfigWillAdd(manager, config, deref(manager)->_context);
    }

    if ( !ret ) {
        return;
    }

    // Add entity and relationships.
    _KEXTManagerAddConfigEntity(manager, config);

    if ( deref(manager)->ccb.ConfigWasAdded ) {
        deref(manager)->ccb.ConfigWasAdded(manager, config, deref(manager)->_context);
    }
}

void
_KEXTManagerRemoveConfigEntityWithCallback(
            KEXTManagerRef                           manager,
            KEXTConfigRef                            config )
{
    Boolean ret;
    
    ret = true;
    if ( deref(manager)->ccb.ConfigWillRemove ) {
        ret = deref(manager)->ccb.ConfigWillRemove(manager, config, deref(manager)->_context);
    }

    if ( !ret ) {
        return;
    }

    // Remove the config and relationships from the config database.
    CFRetain(config);
    _KEXTManagerRemoveConfigEntity(manager, config);
    
    if ( deref(manager)->ccb.ConfigWasRemoved ) {
        deref(manager)->ccb.ConfigWasRemoved(manager, config, deref(manager)->_context);
    }
    CFRelease(config);
}

KEXTConfigRef
KEXTManagerGetConfig(
            KEXTManagerRef                           manager,
            CFStringRef                              primaryKey )
{
    return _KEXTManagerGetEntityWithKey(manager, primaryKey);
}

void
KEXTManagerRemoveConfig(
            KEXTManagerRef                           manager,
            CFStringRef                              primaryKey )
{
    KEXTConfigRef config;

    if ( !manager || !primaryKey ) {
        return;
    }

    config = KEXTManagerGetConfig(manager, primaryKey);
    if ( config ) {
        _KEXTManagerRemoveConfigEntityWithCallback(manager, config);
    }
}

static void ArrayRemoveConfigEntityWithCallback(void * val, void * context)
{
    KEXTManagerRef manager;
    KEXTConfigRef config;

    if ( !val || !context ) {
        return;
    }

    manager = context;
    config = val;

    _KEXTManagerRemoveConfigEntityWithCallback(manager, config);
}

void
KEXTManagerRemoveConfigsForBundle(
            KEXTManagerRef                           manager,
            KEXTBundleRef                            bundle )
{
    CFArrayRef array;
    
    // Remove bundle configurations.
    array = KEXTManagerCopyConfigsForBundle(manager, bundle);
    if ( array ) {
        CFRange range;

        range = CFRangeMake(0, CFArrayGetCount(array));
        CFArrayApplyFunction(array, range, (CFArrayApplierFunction)ArrayRemoveConfigEntityWithCallback, manager);
        CFRelease(array);
    }
}

void
KEXTManagerSetModuleMode(
            KEXTManagerRef                           manager,
            KEXTModuleRef                            module,
            KEXTModuleMode                           mode )
{
    CFMutableDictionaryRef noloads;
    CFStringRef primaryKey;
    CFStringRef val;

    if ( !manager || !module ) {
        return;
    }

    noloads = deref(manager)->_noloads;
    primaryKey = KEXTModuleGetPrimaryKey(module);
    switch ( mode ) {
        case kKEXTModuleModeDefault: {
            val = CFDictionaryGetValue(noloads, primaryKey);
            if ( val ) {
                CFDictionaryRemoveValue(noloads, primaryKey);
            }
            }
            break;

        case kKEXTModuleModeNoload: {
            CFDictionarySetValue(noloads, primaryKey, CFSTR("Disabled"));
            }
            break;

        case kKEXTModuleModeForceLoad: {
            CFDictionarySetValue(noloads, primaryKey, CFSTR("Force"));
            }
            break;
    }
}

