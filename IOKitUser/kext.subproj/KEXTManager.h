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

#ifndef __KEXTMANAGER_H_
#define __KEXTMANAGER_H_

/********
 * NOTICE
 *
 * This API is deprecated and will be replaced following the first shipping version
 * of Mac OS X.
 */


/*
    The KEXTManager is a user level database which is devoted to the management of kernel extension (KEXT) bundles, linking their resources into the kernel, and synchronizing kernel extension data with kernel level services.  The KEXTManager relies on CoreFoundation container objects (see /System/Library/Frameworks/CoreFoundation.framework/Headers for more information) along with it's own containers which are defined within this header.  The KEXTManager specific containers are: KEXTBundle which is an object representing a kernel extension bundle in the file system; KEXTModule, which is a container containing kernel module information and a tag to pair it up with the KEXTBundle from which it originally derived; and KEXTPersonality, which is a container holding at tag to pair it up with its parent KEXTBundle and data specific to IOKit drivers.
*/

#if defined(__cplusplus)
extern "C" {
#endif

#include <CoreFoundation/CoreFoundation.h>

/*!
    @enum KEXTReturn KEXTManager error codes.
    @constant kKEXTReturnSuccess The return value for successful completion.
    @constant kKEXTReturnError The return value for unknown or generic error condition.
    @constant kKEXTReturnBadArgument The return value for an unacceptable value in a function's argument list.
    @constant kKEXTReturnNoMemory The value returned when a memory resource could not be allocated.
    @constant kKEXTReturnNotKext The value returned from a function when the KEXTManager is passed a reference to a bundle which is not of the proper KEXTBundle format.  This includes non-existent bundles, bundles of inappropriate types, or bundles with bad Info-macosx.plists.
    @constant kKEXTReturnResourceNotFound The return value normally associated with acquiring resource data from a CFURL object.
    @constant kKEXTReturnModuleNotFound The value returned from a function when a requested module is not present in the KEXTManager database or not found within a KEXTBundle.
    @constant kKEXTReturnPersonalityNotFound The value returned from a function when a requested personality is not present in the KEXTManager database or not found within a KEXTBundle.
    @constant kKEXTReturnPropertyNotFound The value returned from a function when a required property (key or value) is not found within the property list of a KEXTBundle, KEXTModule, or KEXTPersonality object.
    @constant kKEXTReturnModuleFileNotFound The value returned when a module file could not be found within a KEXTBundle.
    @constant kKEXTReturnBundleNotFound The value returned from a function when a KEXTBundle in the KEXTManager database can no longer  be found in the filesystem.
    @constant kKEXTReturnMissingDependency The value returned from a function when a module depended upon by another module to be loaded cannot be found.
    @constant kKEXTReturnPermissionError The value returned from a function when the user does not have permission to access a particular resource.
    @constant kKEXTReturnSerializationError The value returned from a function when a KEXTPersonality cannot be properly serialized for transport to the IOCatalogue.
    @constant kKEXTReturnKModLoadError The return value when the kmodload utility cannot link the a module into the kernel.
    @constant kKEXTReturnModuleAlreadyLoaded The error coded returned when attempting to load a KEXTModule which has already been loaded.
*/
typedef enum {
    kKEXTReturnSuccess = 1,
    kKEXTReturnError,
    kKEXTReturnBadArgument,
    kKEXTReturnNoMemory,
    kKEXTReturnNotKext,
    kKEXTReturnResourceNotFound,
    kKEXTReturnModuleNotFound,
    kKEXTReturnPersonalityNotFound,
    kKEXTReturnPropertyNotFound,
    kKEXTReturnModuleFileNotFound,
    kKEXTReturnBundleNotFound,
    kKEXTReturnMissingDependency,
    kKEXTReturnDependencyVersionMismatch,
    kKEXTReturnPermissionError,
    kKEXTReturnSerializationError,
    kKEXTReturnKModLoadError,
    kKEXTReturnModuleAlreadyLoaded,
    kKEXTReturnBundleAlreadyExists,
    kKEXTReturnModuleDisabled,
} KEXTReturn;


typedef struct __KEXTManager  * KEXTManagerRef;
typedef struct __CFDictionary * KEXTEntityRef;
typedef struct __CFString     * KEXTEntityType;

typedef KEXTEntityRef KEXTBundleRef;
typedef KEXTEntityRef KEXTModuleRef;
typedef KEXTEntityRef KEXTPersonalityRef;

typedef KEXTPersonalityRef KEXTConfigRef;

/*!
    @function KEXTError
    @abstract Prints out a human-readable error message.
    @param error The KEXTReturn error code.
    @param text A string which appears before the error message.
*/
void			KEXTError(KEXTReturn error, CFStringRef text);

/*
    KEXTModule container API's.
*/
/*!
    @function KEXTModuleRetain
    @abstract Increments a KEXTModule object's retain count.
    @param module A reference to a KEXTModule object.
    @result Returns a reference to the KEXTModule.
*/
KEXTModuleRef		KEXTModuleRetain(KEXTModuleRef module);
/*!
    @function KEXTModuleRelease
    @abstract Decrements the KEXTModule object's retain count and calls KEXTModuleFree() it when it drops to 0.
    @param module A reference to KEXTModule object.
*/
void			KEXTModuleRelease(KEXTModuleRef module);
/*!
    @function KEXTModuleGetEntityType
    @abstract Returns the type for a KEXTModule entity.
    @result Returns the entity type for the KEXTModule "class".
*/
KEXTEntityType          KEXTModuleGetEntityType(void);
/*!
    @function KEXTModuleGetPrimaryKey
    @abstract Returns a reference to the KEXTModule's primary key.
    @param bundle A reference to the KEXTModule object.
    @result Returns a reference to the KEXTModule's primary key.
    @discussion The primary key is used for finding the module in the KEXTManager database.
*/
CFStringRef             KEXTModuleGetPrimaryKey(KEXTModuleRef module);
/*!
    @function KEXTModuleGetProperty
    @abstract Returns an object from the KEXTModule's internal property list based on the key provided.
    @param module A reference to KEXTModule object.
    @param key The key should be a CFString object which uniquely identifies the value to return. 
    @result Returns a reference to an object referred to by the key.
*/
CFTypeRef		KEXTModuleGetProperty(KEXTModuleRef module, CFStringRef key);
/*!
    @function KEXTModuleEqual
    @abstract Tests two KEXTModules for equality.
    @param module1 A reference to the first KEXTModule object.
    @param module2 A reference to the second KEXTModule object.
    @result Returns a boolean value describing the equality of the two objects.
*/
Boolean			KEXTModuleEqual(KEXTModuleRef module1, KEXTModuleRef module2);
/*!
    @function KEXTModuleIsLoaded
    @abstract Tells the caller if the KEXTModule has been linked into the kernel.
    @param module A reference to KEXTModule object.
    @param error A reference to a KEXTReturn variable which tells the caller if the system call completed correctly.
    @result Returns a value telling the caller if the module is currently linked into the kernel.
*/
Boolean 		KEXTModuleIsLoaded(KEXTModuleRef module,
    KEXTReturn * error);

/*!
    @function KEXTModuleGetLoadedVers
    @abstract Gets the Mac OS 'vers' style version of a module that's loaded in the kernel.
    @param module A reference to KEXTModule object.
    @param vers A pointer to the version being retrieved.
    @result true if the module is linked in the kernel and has a legal version; false otherwise.
*/
Boolean 		KEXTModuleGetLoadedVers(KEXTModuleRef module,
    UInt32 * vers);

/*
    KEXTPersonality container API's.
*/
/*!
    @function KEXTPersonalityRetain
    @abstract Increments a KEXTPersonality object's retain count.
    @param personality A reference to a KEXTPersonality object.
    @result Returns a reference to the KEXTPersonality.
*/
KEXTPersonalityRef	KEXTPersonalityRetain(KEXTPersonalityRef personality);
/*!
    @function KEXTPersonalityRelease
    @abstract Decrements the KEXTPersonality object's retain count and calls KEXTPersonalityFree() it when it drops to 0.
    @param personality A reference to KEXTPersonality object.
*/
void			KEXTPersonalityRelease(KEXTPersonalityRef personality);
/*!
    @function KEXTPersonalityGetEntityType 
    @abstract Returns the type for a KEXTPersonality entity.
    @result Returns the entity type for the KEXTPersonality "class". 
*/
KEXTEntityType          KEXTPersonalityGetEntityType(void);
/*!
    @function KEXTPersonalityGetPrimaryKey
    @abstract Returns a reference to the KEXTPersonality's primary key.
    @param bundle A reference to the KEXTPersonality object.
    @result Returns a reference to the KEXTPersonality's primary key.
    @discussion The primary key is used for finding the module in the KEXTManager database.
*/
CFStringRef             KEXTPersonalityGetPrimaryKey(KEXTPersonalityRef personality);
/*!
    @function KEXTPersonalityGetProperty
    @abstract Returns an object from the KEXTPersonality's internal property list based on the key provided.
    @param personality A reference to KEXTPersonality object.
    @param key The key should be a CFString object which uniquely identifies the value to return.
    @result Returns a reference to an object referred to by the key.
*/
CFTypeRef		KEXTPersonalityGetProperty(KEXTPersonalityRef personality, CFStringRef key);
/*!
    @function KEXTPersonalityEqual
    @abstract Tests two KEXTPersonalitys for equality.
    @param personality1 A reference to the first KEXTPersonality object.
    @param personality2 A reference to the second KEXTPersonality object.
    @result Returns a boolean value describing the equality of the two objects.
*/
Boolean			KEXTPersonalityEqual(KEXTPersonalityRef personality1, KEXTPersonalityRef personality2);

/*
    KEXTBundle container API's.
*/
/*!
    @function KEXTBundleRetain
    @abstract Increments a KEXTBundle object's retain count.
    @param bundle A reference to a KEXTBundle object.
    @result Returns a reference to the KEXTBundle.
*/
KEXTBundleRef   	KEXTBundleRetain(KEXTBundleRef bundle);
/*!
    @function KEXTBundleRelease
    @abstract Decrements the KEXTBundle object's retain count and calls KEXTBundleFree() it when it drops to 0.
    @param bundle A reference to KEXTBundle object.
*/
void			KEXTBundleRelease(KEXTBundleRef bundle);
/*!
    @function KEXTBundleGetEntityType
    @abstract Returns the type for a KEXTBundle entity.
    @result Returns the entity type for the KEXTBundle "class".
*/
KEXTEntityType          KEXTBundleGetEntityType(void);
/*!
    @enum KEXTValidation
    @constant kKEXTBundleValidateNoChange This value indicates that there has been no modification to the KEXT bundle.
    @constant kKEXTBundleValidateRemoved This value indicates that the KEXT bundle has been removed from the filesystem.
    @constant kKEXTBundleValidateModified This value indicates that the KEXT bundle still exists but has been modified.
*/
typedef enum {
    kKEXTBundleValidateNoChange = 1,
    kKEXTBundleValidateRemoved,
    kKEXTBundleValidateModified,
} KEXTValidation;
/*!
    @function KEXTBundleValidate
    @abstract Checks the KEXTBundle in the filesystem to determine if it has been modified or removed.
    @param bundle A reference to KEXTBundle object.
    @result A boolean value indicating that the KEXTBundle has been modified in some way.
*/
KEXTValidation          KEXTBundleValidate(KEXTBundleRef bundle);
/*!
    @function KEXTBundleCopyURL
    @abstract Returns a reference to the KEXTBundle's URL.
    @param bundle A reference to the KEXTBundle object.
    @result Returns a reference to the KEXTBundle's URL.
*/
CFURLRef		KEXTBundleCopyURL(KEXTBundleRef bundle);
/*!
     @function KEXTBundleGetPrimaryKey
     @abstract Returns a reference to the KEXTBundle's primary key.
     @param bundle A reference to the KEXTBundle object.
     @result Returns a reference to the KEXTBundle's primary key.
     @discussion The primary key is used for finding the bundle in the KEXTManager database.
*/
CFStringRef             KEXTBundleGetPrimaryKey(KEXTBundleRef bundle);
/*!
    @function KEXTBundleGetProperty
    @abstract Returns a reference to an object referred to by 'key' contained within the KEXTBundle's internal property list.
    @param bundle A reference to the KEXTBundle object.
    @param key The key should be a CFString object which uniquely identifies the value to return.
    @result Returns a reference to an object referred to by 'key'.
*/
CFTypeRef		KEXTBundleGetProperty(KEXTBundleRef bundle, CFStringRef key);
/*!
    @function KEXTBundleCreateDataFromResource
    @abstract Creates a CFData object from a named resource of a particular type.
    @param bundle A reference to the KEXTBundle object.
    @param name A CFString denoting the name of the resource to be returned.
    @param type A CFString denoting the type of resource.
    @param error A reference to a KEXTReturn variable.
    @result Returns a reference to CFData object or NULL on an error.  The error condition is returned via the 'error' argument.
 */
CFURLRef		KEXTBundleCreateURLForResource(KEXTBundleRef bundle, CFStringRef name, CFStringRef type, KEXTReturn * error);
/*!
    @function KEXTBundleEqual
    @abstract Tests the equality between two KEXTBundle objects.
    @param bundle1 A reference to the first KEXTBundle object.
    @param bundle2 A reference to the second KEXTBundle object.
    @result Returns the equality of the two KEXTBundle objects.
*/
Boolean			KEXTBundleEqual(KEXTBundleRef bundle1, KEXTBundleRef Bundle2);
/*!
    @function KEXTBundleMatchProperties
    @abstract Tests the equality of the intersection of two bundle's property lists.
    @param bundle1 A reference to the first KEXTBundle object.
    @param bundle2 A reference to the second KEXTBundle object.
    @param match A CFArray or CFDictionary containing the range of keys to test.  If NULL, a full comparison is done.
    @result Returns the equality of the intersection of the property lists.
*/
Boolean			KEXTBundleMatchProperties(KEXTBundleRef bundle1, KEXTBundleRef bundle2, CFTypeRef match);

/* KEXTManager callback structures. */
/*!
    @typedef KEXTManagerBundleLoadingCallbacks Structure containing the callbacks for loading KEXTBundles into the KEXTManager database.
    @field version The version number of the structure type being passed in as a parameter to the KEXTManager creation functions. This structure is version 0.
    @field BundleAuthentication This callback is used to add additional authenticity checking when loading scanning bundle resources.  The bundle parameter is the url to a KEXT bundle currently being scanned.  The context parameter is whatever context was provided to the KEXTManager at initialization. Returning false will prevent the KEXTManager from attempting to process the bundle object any further.
    @field BundleWillAdd Once a bundle has been authenticated and validated, it can then be loaded into the KEXTManager database. This callback allows additional checking or the initialization of state before the bundle is added to the database.  Returning a false value from this callback will prevent the bundle from being added to the KEXTManager database.  The manager parameter is the KEXTManager object.  The bundle parameter is the KEXTBundle object to be added to the database.  The context parameter is whatever context was provided to the KEXTManager at initialization.
    @field BundleWasAdded This callback is called after a bundle has been added to the KEXTManager database. The manager parameter is the KEXTManager object.  The bundle parameter is the KEXTBundle object to be added to the database.  The context parameter is whatever context was provided to the KEXTManager at initialization.
    @field BundleWillRemove This callback is called prior to the removal of a KEXTBundle object from the database.  Returning false will prevent its removal. The manager parameter is the KEXTManager object.  The bundle parameter is the KEXTBundle object to be added to the database.  The context parameter is whatever context was provided to the KEXTManager at initialization.
    @field BundleWasRemoved This callback is called after a bundle has been removed from the KEXTManager database. The manager parameter is the KEXTManager object.  The bundle parameter is the KEXTBundle object to be added to the database.  The context parameter is whatever context was provided to the KEXTManager at initialization.
*/
typedef struct {
    CFIndex	version;
    KEXTReturn 	(*BundleAuthentication)(CFURLRef url, void * context);
    Boolean	(*BundleWillAdd)(KEXTManagerRef manager, KEXTBundleRef bundle, void * context);
    void	(*BundleWasAdded)(KEXTManagerRef manager, KEXTBundleRef bundle, void * context);
    KEXTReturn  (*BundleError)(KEXTManagerRef manager, KEXTBundleRef bundle, KEXTReturn error, void * context);
    Boolean	(*BundleWillRemove)(KEXTManagerRef manager, KEXTBundleRef bundle, void * context);
    void	(*BundleWasRemoved)(KEXTManagerRef manager, KEXTBundleRef bundle, void * context);
} KEXTManagerBundleLoadingCallbacks;

/*!
    @typedef KEXTManagerModuleLoadingCallbacks Structure containing the callbacks for loading KEXTModules into the kernel.
    @field version The version number of the structure type being passed in as a parameter to the KEXTManager creation functions. This structure is version 0.
    @field ModuleWillLoad This callback function is called when prior to loading a KEXTModule into the kernel.  Returning false will prevent the module from being loaded. The manager parameter is the KEXTManager object.  The module parameter is the KEXTModule object to be linked into the kernel.  The context parameter is whatever context was provided to the KEXTManager at initialization.
    @field ModuleWasLoaded This callback function is called when a module is successfully loaded into the kernel. The manager parameter is the KEXTManager object.  The module parameter is the KEXTModule object to be linked into the kernel.  The context parameter is whatever context was provided to the KEXTManager at initialization.
    @field ModuleError This callback is called when an error occurred during module loading.  Returning kKEXTReturnSuccess will override the error condition and allow the KEXTManager to continue as if the error did not occur, else return the error contition provided by the error parameter. The manager parameter is the KEXTManager object. The module parameter is the KEXTModule object to be linked into the kernel. The error parameter is the error condition which occurred during loading. The context parameter is whatever context was provided to the KEXTManager at initialization.
    @field ModuleWillUnload This callback is called before a module is to be linked into the kernel.  Returning 'true' will allow the module to be linked into the kernel, 'false' will prevent the module from being linked.
    @field ModuleWasUnloaded This callback is called afer a module has been successfully linked into the kernel.
 */
typedef struct {
    CFIndex 	version;    
    Boolean	(*ModuleWillLoad)(KEXTManagerRef manager, KEXTModuleRef module, void * context);
    void	(*ModuleWasLoaded)(KEXTManagerRef manager, KEXTModuleRef module, void * context);
    KEXTReturn	(*ModuleError)(KEXTManagerRef manager, KEXTModuleRef module, KEXTReturn error, void * context);
    Boolean     (*ModuleWillUnload)(KEXTManagerRef manager, KEXTModuleRef module, void * context);
    void        (*ModuleWasUnloaded)(KEXTManagerRef manager, KEXTModuleRef module, void * context);
} KEXTManagerModuleLoadingCallbacks;

/*!
    @typedef KEXTManagerPersonalityLoadingCallbacks Structure containing the callbacks for loading KEXTPersonalities into the kernel.
    @field version The version number of the structure type being passed in as a parameter to the KEXTManager creation functions. This structure is version 0.
    @field PersonalityWillLoad This callback function is called when prior to loading a KEXTPersonality object into the kernel database. Returning false will prevent the personality from being loaded. The manager parameter is the KEXTManager object. The personality parameter is the KEXTPersonality object to be loaded into the kernel database.  The context parameter is whatever context was provided to the KEXTManager at initialization.
    @field PersonalityWasLoaded This callback function is called when a personality is successfully loaded into the kernel database. The manager parameter is the KEXTManager object.  The personality parameter is the KEXTPersonality object to be loaded into the kernel database.  The context parameter is whatever context was provided to the KEXTManager at initialization.
    @field PersonalityError This callback is called when an error occurred during personality loading.  Returning kKEXTReturnSuccess will override the error condition and allow the KEXTManager to continue as if the error did not occur, else return the error condition provided by the error parameter. The manager parameter is the KEXTManager object. The personality parameter is the KEXTPersonality object to be added to the kernel database. The error parameter is the error condition which occurred during loading. The context parameter is whatever context was provided to the KEXTManager at initialization.
    @field PersonalityWillUnload This callback is called when a personality is requested to be unloaded.  Returning true will allow the personality to be removed from the IOCatalogue, returning false will prevent it from being removed.
    @field PersonalityWasUnloaded This callback is called when a personality has been successfully removed from the IOCatalogue.
 */
typedef struct {
    CFIndex 	version;
    Boolean	(*PersonalityWillLoad)(KEXTManagerRef manager, KEXTPersonalityRef personality, void * context);
    void	(*PersonalityWasLoaded)(KEXTManagerRef manager, KEXTPersonalityRef personality, void * context);
    KEXTReturn	(*PersonalityError)(KEXTManagerRef manager, KEXTPersonalityRef personality, KEXTReturn error, void * context);
    Boolean	(*PersonalityWillUnload)(KEXTManagerRef manager, KEXTPersonalityRef personality, void * context);
    void	(*PersonalityWasUnloaded)(KEXTManagerRef manager, KEXTPersonalityRef personality, void * context);
} KEXTManagerPersonalityLoadingCallbacks;
/*!
    @typedef KEXTManagerConfigsCallbacks Structure containing the callbacks for adding KEXTConfig objects to the configuration database.
    @field version The version number of the structure type being passed in as a parameter to the KEXTManager creation functions. This structure is version 0.
    @field ConfigWillAdd This callback is called before a KEXTConfig object is to be added to the configuration database.
    @field ConfigWasAdded This callback is called after a KEXTCofnig object has been added to the configuration database.
    @field ConfigWillRemove This callback is called before a KEXTConfig entity is to be removed from the configuration database.
    @field ConfigWasRemoved This callback is called after a KEXTConfig entity has been removed from the configuration database.
 */
typedef struct {
    CFIndex     version;
    Boolean     (*ConfigWillAdd)(KEXTManagerRef manager, KEXTConfigRef config, void * context);
    void        (*ConfigWasAdded)(KEXTManagerRef manager, KEXTConfigRef config, void * context);
    Boolean     (*ConfigWillRemove)(KEXTManagerRef manager, KEXTConfigRef config, void * context);
    void        (*ConfigWasRemoved)(KEXTManagerRef manager, KEXTConfigRef config, void * context);
} KEXTManagerConfigsCallbacks;

/*
    KEXTManager API's. 
*/
/*!
    @enum KEXTManagerMode
    @constant kKEXTManagerDefaultMode This value instructs KEXTManager to use all current configurations.
    @constant kKEXTManagerRecoveryMode This value instructs KEXTManager to use previously saved configurations.
    @constant kKEXTManagerInstallMode This value instructs KEXTManager to use configurations used during installation.
*/
typedef enum {
    kKEXTManagerDefaultMode = 1,
    kKEXTManagerRecoveryMode,
    kKEXTManagerInstallMode,
} KEXTManagerMode;
/*!
    @function KEXTManagerCreate
    @abstract Creates a KEXTManager database object.
    @param bundleCallbacks Callback functions dealing with KEXTBundle management.
    @param moduleCallbacks Callback functions dealing with KEXTModule management.
    @param personalityCallbacks Callback functions dealing with KEXTPersonality management.
    @param configCallbacks Callback function notify of changes to the configuration database.
    @param context Callback context parameter.
    @param error If KEXTManager returns NULL, an error will be returned in this parameter.
    @result Returns a reference to a KEXTManager database object or NULL if initialization was unsuccessful.
*/
KEXTManagerRef		KEXTManagerCreate(KEXTManagerBundleLoadingCallbacks * bundleCallbacks,
    KEXTManagerModuleLoadingCallbacks * moduleCallbacks,
    KEXTManagerPersonalityLoadingCallbacks * personalityCallbacks,
    KEXTManagerConfigsCallbacks * configCallbacks,
    void * context,
    void (*errorCallback)(const char *),
    void (*messageCallback)(const char *),
    Boolean safeBoot,
    KEXTReturn * error);
/*!
    @function KEXTManagerRetain
    @abstract Increments a KEXTManager object's retain count.
    @param manager A reference to a KEXTManager object.
    @result Returns a reference to the KEXTManager.
*/
KEXTManagerRef		KEXTManagerRetain(KEXTManagerRef manager);
/*!
    @function KEXTManagerRelease
    @abstract Decrements the KEXTManager object's retain count and calls KEXTManagerFree() it when it drops to 0.
    @param manager A reference to KEXTManager object.
*/
void			KEXTManagerRelease(KEXTManagerRef manager);
/*!
    @function KEXTManagerSetMode
    @abstract Set the KEXTManager into a particular mode of operation.
    @param manager The KEXTManager database object.
    @param mode The mode of operation for the database.
    @results Sets the database into a particular mode of operation.
    @discussion This function sets determines which driver configuration database should be used for lookups when loading modules or their personality configuration info into the kernel.
*/
void                    KEXTManagerSetMode(KEXTManagerRef manager, KEXTManagerMode mode);
/*!
    @function KEXTManagerScanPath
    @abstract Scans for KEXTBundles at the URL presented.  Subsequent re-scans will validate existing KEXTBundles and remove them if necessary.
    @param manager A reference to KEXTManager object.
    @param url The path to the directory containing kernel extensions.
    @result Returns an error if path cannot be accessed or if scanning failed for some reason.
*/
KEXTReturn		KEXTManagerScanPath(KEXTManagerRef manager, CFURLRef url);
/*!
    @function KEXTManagerReset
    @abstract Flushes the KEXTManager database.
    @param manager A reference to KEXTManager object.
*/
void			KEXTManagerReset(KEXTManagerRef manager);
/*!
    @function KEXTManagerGetCount
    @abstract Returns the number of KEXTBundle objects within the KEXTManager database.
    @param manager A reference to KEXTManager object.
    @result Returns the number of KEXTBundle object contained within the KEXTManager database.
*/
CFIndex			KEXTManagerGetCount(KEXTManagerRef manager);
/*!
    @function KEXTManagerCopyAllEntities
    @abstract Returns an array containing references to all objects in the database.
    @param manager A reference to the KEXTManager object.
    @result returns an array containing objects from the database.
*/
CFArrayRef              KEXTManagerCopyAllEntities(KEXTManagerRef manager);
/*!
    @function KEXTManagerCopyAllBundles
    @abstract Returns an array containing references to all bundle entities in the database.
    @param manager A reference to the KEXTManager object.
    @result returns an array containing objects from the database.
*/
CFArrayRef              KEXTManagerCopyAllBundles(KEXTManagerRef manager);
/*!
    @function KEXTManagerCopyAllModules
    @abstract Returns an array containing references to all module entities in the database.
    @param manager A reference to the KEXTManager object.
    @result returns an array containing objects from the database.
*/
CFArrayRef              KEXTManagerCopyAllModules(KEXTManagerRef manager);
/*!
    @function KEXTManagerCopyAllPersonalities
    @abstract Returns an array containing references to all personality entities in the database.
    @param manager A reference to the KEXTManager object.
    @result returns an array containing entities from the database.
*/
CFArrayRef              KEXTManagerCopyAllPersonalities(KEXTManagerRef manager);
/*!
    @function KEXTManagerCopyAllConfigs
    @abstract Returns an array containing references to all config entities in the database.
    @param manager A reference to the KEXTManager object.
    @result returns an array containing entities from the database.
*/
CFArrayRef              KEXTManagerCopyAllConfigs(KEXTManagerRef manager);
/*!
    @function KEXTManagerAddBundle
    @abstract Creates a KEXTBundle, KEXTPersonality, and KEXTModule entries in the KEXTManager database.
    @param manager A reference to the KEXTManager dabase object.
    @param bundleUrl An URL path to the bundle.
    @param bundle A primary key to the KEXTBundle returned to the caller.
    @result Returns kKEXTReturnSucces if the bundle is successfully added to the database.
*/
KEXTReturn     		KEXTManagerAddBundle(KEXTManagerRef manager, CFURLRef bundleUrl);
/*!
    @function KEXTManagerRemoveBundle
    @abstract Removes a KEXTBundle from the KEXTManager database.  The KEXTBundle is automatically released.
    @param manager A reference to KEXTManager object.
    @param primaryKey The primary key identifying the KEXTBundle object to be removed from the KEXTManager database.
*/
void			KEXTManagerRemoveBundle(KEXTManagerRef manager, CFStringRef primaryKey);
/*!
    @function KEXTManagerGetBundle
    @abstract Returns a KEXTBundle from the KEXTManager database, or NULL if none exist.
    @param manager A reference to KEXTManager object.
    @param primaryKey The primary key identifying a particular KEXTBundle object within the KEXTManager database.
    @result Returns a reference to a KEXTBundle or NULL if the bundle does not exist within the database.
*/
KEXTBundleRef		KEXTManagerGetBundle(KEXTManagerRef manager, CFStringRef primaryKey);
/*!
    @function KEXTManagerGetBundleWithURL
    @abstract Returns a KEXTBundle from the KEXTManager database, or NULL if none exist.
    @param manager A reference to KEXTManager object.
    @param url The URL identifying a particular KEXTBundle object within the KEXTManager database.
    @result Returns a reference to a KEXTBundle or NULL if the bundle does not exist within the database.
*/
KEXTBundleRef		KEXTManagerGetBundleWithURL(KEXTManagerRef manager, CFURLRef url);
/*!
    @function KEXTManagerGetBundleWithModule
    @abstract Returns a KEXTBundle containing a particular KEXTModule.
    @param manager A reference to KEXTManager object.
    @param moduleName A CFString containing the name of a KEXTModule.
    @result Returns a reference to a KEXTBundle or NULL if the search was unsuccessful.
*/
KEXTBundleRef		KEXTManagerGetBundleWithModule(KEXTManagerRef manager, CFStringRef moduleName);
/*!
    @function KEXTManagerCopyModuleDependencies
    @abstract Returns an array of modules depended upon by 'module'.
    @param manager A reference to KEXTManager object.
    @param module A KEXTModuleRef object.
    @param array An array variable.  An array will be allocated and poputated by the function.
    @result Returns false if the dependency chain was broken by a missing module, else it will return true and 'array' will be a CFArray object containing the modules depended upon by the 'module' parameter.  These items will be in "load order".
*/
Boolean                 KEXTManagerCopyModuleDependencies(KEXTManagerRef manager, KEXTModuleRef module, CFArrayRef * array);
/*!
    @function KEXTManagerCopyModule
    @abstract Returns an array of all KEXTModule objects referenced by the KEXTBundle.
    @param bundle A reference to the KEXTBundle object.
    @result Returns a CFArray object containing all of the KEXTModules referenced by the KEXTBundle, or NULL if none exist.
*/
CFArrayRef		KEXTManagerCopyModulesForBundle(KEXTManagerRef manager, KEXTBundleRef bundle);
/*!
    @function KEXTManagerCopyPersonalitiesForBundle
    @abstract Returns an array of all KEXTPersonality referenced by the KEXTBundle.
    @param bundle A reference to the KEXTBundle object.
    @result Returns a CFArray object containing all of the KEXTPersonalities referenced by the KEXTBundle, or NULL if none exist.
*/
CFArrayRef		KEXTManagerCopyPersonalitiesForBundle(KEXTManagerRef manager, KEXTBundleRef bundle);
/*!
    @function KEXTManagerGetModule
    @abstract Returns a KEXTModule denoted by a unique module name.
    @param manager A reference to KEXTManager object.
    @param moduleName A CFString containing the name of a KEXTModule.
    @result Returns a reference to a KEXTBundle or NULL if the search was unsuccessful.
*/
KEXTModuleRef		KEXTManagerGetModule(KEXTManagerRef manager, CFStringRef moduleName);
/*!
    @function KEXTManagerGetPersonality
    @abstract Returns a KEXTPersonality contained within the KEXTBundle.
    @param bundle A reference to the KEXTBundle object.
    @param name A CFString containing the name of the KEXTPersonality.
    @result Returns a reference to the KEXTPersonality referred to by primaryKey, or NULL if none exists.
*/
KEXTPersonalityRef	KEXTManagerGetPersonality(KEXTManagerRef manager, CFStringRef primaryKey);
/*!
    @function KEXTManagerGetEntityType
    @abstract Returns a value describing the type of the entity.
    @param entity The entity object.
    @result Returns a CFString object describing the type of the object.
*/
CFStringRef             KEXTManagerGetEntityType(KEXTEntityRef entity);
/*!
    @function KEXTManagerAuthenticateURL
    @abstract Checks the path for ownership by root user.
    @param url The url to be authenticated.
    @result Return kKEXTReturnSuccess if the propertys of the url are owned by root.
*/
KEXTReturn              KEXTManagerAuthenticateURL(CFURLRef url);
/*!
    @function KEXTManagerLoadModule
    @abstract Links a kernel module into the kernel.
    @param manager A reference to KEXTManager object.
    @param module A KEXTModule object.
    @result Returns kKEXTReturnSuccess or an error condition if loading failed.  A CF object will be returned for various error conditions.
*/
KEXTReturn		KEXTManagerLoadModule(KEXTManagerRef manager, KEXTModuleRef module);
/*!
    @function KEXTManagerUnloadModule
    @abstract Unloads a kernel module from the kernel.
    @param manager A reference to KEXTManager object.
    @param module The KEXTModule object to unload.
    @result Returns kKEXTReturnSuccess or an error condition if termination failed.
 */
KEXTReturn              KEXTManagerUnloadModule(KEXTManagerRef manager, KEXTModuleRef module);
/*!
    @function KEXTManagerLoadPersonalities
    @abstract Serializes and sends KEXTPersonality property lists to the IOCatalogue.
    @param manager A reference to KEXTManager object.
    @param personality An array of KEXTPersonality objects.
    @result Returns kKEXTReturnSuccess or an error condition if request failed.
 */
KEXTReturn		KEXTManagerLoadPersonalities(KEXTManagerRef manager, CFArrayRef personalities);
/*!
    @function KEXTManagerUnloadPersonality
    @abstract Removes a personality from the IOCatalogue.
    @param manager A reference to KEXTManager object.
    @param personality A KEXTPersonality object.
    @result Returns kKEXTReturnSuccess or an error condition if request failed.
 */
KEXTReturn              KEXTManagerUnloadPersonality(KEXTManagerRef manager, KEXTPersonalityRef personality);
/*!
    @function KEXTManagerCreateConfig
    @abstract Create a KEXTConfig object from a KEXTPersonality.
    @param manager A reference to KEXTManager object.
    @param personality The KEXTPersonality to be configured.
    @result Returns a KEXTConfig object or NULL if there was an error.
 */
KEXTConfigRef           KEXTManagerCreateConfig(KEXTManagerRef manager, KEXTPersonalityRef personality);
/*!
    @function KEXTManagerAddConfig
    @abstract Add a KEXTConfig entity to the configuration database.
    @param manager A reference to KEXTManager object.
    @param config The KEXTConfig to be added to the database.
    @result Returns a KEXTConfig object or NULL if there was an error.
 */
void                    KEXTManagerAddConfig(KEXTManagerRef manager, KEXTConfigRef config);
/*!
    @function KEXTManagerGetConfig
    @abstract Get a KEXTConfig entity to the configuration database.
    @param manager A reference to KEXTManager object.
    @param primaryKey The primary key of the KEXTConfig entity in the database.
    @result Returns a KEXTConfig object or NULL if it does not exist.
 */
KEXTConfigRef           KEXTManagerGetConfig(KEXTManagerRef manager, CFStringRef primaryKey);
/*!
    @function KEXTManagerRemoveConfig
    @abstract Remove a KEXTConfig entity to the configuration database.
    @param manager A reference to KEXTManager object.
    @param primaryKey The primary key of the KEXTConfig entity in the database.
    @result Returns a KEXTConfig object or NULL if it does not exist.
 */
void                    KEXTManagerRemoveConfig(KEXTManagerRef manager, CFStringRef primaryKey);
/*!
    @function KEXTManagerCopyConfigsForBundle
    @abstract Get all KEXTConfig entities related to a particular bundle.
    @param manager A reference to KEXTManager object.
    @param bundle A KEXTBundle.
    @result Returns an array of KEXTConfig objects, or NULL if none exist for this bundle.
 */
CFArrayRef              KEXTManagerCopyConfigsForBundle(KEXTManagerRef manager, KEXTBundleRef bundle);
/*!
    @function KEXTManagerRemoveConfigsForBundle
    @abstract Removes all configurations for a particular bundle.
    @param manager A reference to KEXTManager object.
    @param bundle A KEXTBundle.
    @result Removes all configurations related to the bundle.
 */
void                    KEXTManagerRemoveConfigsForBundle(KEXTManagerRef manager, KEXTBundleRef bundle);
/*!
@enum KEXTModuleMode
 @constant kKEXTModuleModeDefault This sets the mode of the KEXTModule entity for automatic loading.
 @constant kKEXTModuleModeNoload This set the mode of the KEXTModule entity to prevent it from being loaded.
 @constant kKEXTModuleModeForceLoad
*/
typedef enum {
    kKEXTModuleModeDefault = 1,
    kKEXTModuleModeNoload,
    kKEXTModuleModeForceLoad,
} KEXTModuleMode;
/*!
    @function KEXTManagerSetModuleMode
    @abstract Sets the mode for a particular module.
    @param manager A reference to KEXTManager object.
    @param module A KEXTModule object.
    @result Changes the mode of the module.
 */
void                    KEXTManagerSetModuleMode(KEXTManagerRef manager, KEXTModuleRef module, KEXTModuleMode mode);
/*!
    @function KEXTManagerSaveConfigs
    @abstract Save the configuration database to disk.
    @param manager A reference to KEXTManager object.
    @param url A CFURL path where to save the file.
    @result Returns kKEXTReturnSuccess or an error condition if request failed.
 */
KEXTReturn              KEXTManagerSaveConfigs(KEXTManagerRef manager, CFURLRef url);

#if defined(__cplusplus)
} /* extern "C" */
#endif


#endif /* __KEXTMANAGER_H_ */


