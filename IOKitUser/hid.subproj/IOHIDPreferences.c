//
//  IOHIDPreferences.c
//  IOKit
//
//  Created by AB on 10/25/19.
//

#include "IOHIDPreferences.h"
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include "IOHIDLibPrivate.h"

#define HIDPreferencesFrameworkPath "/System/Library/PrivateFrameworks/HIDPreferences.framework/HIDPreferences"

#define kHIDPreferencesSet          "HIDPreferencesSet"
#define kHIDPreferencesSetMultiple  "HIDPreferencesSetMultiple"
#define kHIDPreferencesCopy         "HIDPreferencesCopy"
#define kHIDPreferencesCopyMultiple "HIDPreferencesCopyMultiple"
#define kHIDPreferencesSynchronize  "HIDPreferencesSynchronize"
#define kHIDPreferencesCopyDomain   "HIDPreferencesCopyDomain"
#define kHIDPreferencesSetDomain    "HIDPreferencesSetDomain"

#pragma mark -
#define kHIDPreferencesSetForInstance          "HIDPreferencesSetForInstance"
#define kHIDPreferencesSetMultipleForInstance  "HIDPreferencesSetMultipleForInstance"
#define kHIDPreferencesCopyForInstance         "HIDPreferencesCopyForInstance"
#define kHIDPreferencesCopyMultipleForInstance "HIDPreferencesCopyMultipleForInstance"
#define kHIDPreferencesSynchronizeForInstance  "HIDPreferencesSynchronizeForInstance"
#define kHIDPreferencesCopyDomainForInstance   "HIDPreferencesCopyDomainForInstance"
#define kHIDPreferencesSetDomainForInstance    "HIDPreferencesSetDomainForInstance"
#define kHIDPreferencesCreateInstance          "HIDPreferencesCreateInstance"


typedef void (*HIDPreferencesSetPtr)(CFStringRef key, CFTypeRef __nullable value, CFStringRef user, CFStringRef host, CFStringRef domain);
typedef void (*HIDPreferencesSetMultiplePtr)(CFDictionaryRef __nullable keysToSet , CFArrayRef __nullable keysToRemove, CFStringRef user, CFStringRef host, CFStringRef domain);
typedef CFTypeRef (*HIDPreferencesCopyPtr)(CFStringRef key, CFStringRef user, CFStringRef host, CFStringRef domain);
typedef CFDictionaryRef (*HIDPreferencesCopyMultiplePtr)(CFArrayRef __nullable keys, CFStringRef user, CFStringRef host, CFStringRef domain);
typedef void (*HIDPreferencesSynchronizePtr)(CFStringRef user, CFStringRef host, CFStringRef domain);
typedef CFTypeRef (*HIDPreferencesCopyDomainPtr)(CFStringRef key, CFStringRef domain);
typedef void (*HIDPreferencesSetDomainPtr)(CFStringRef key,  CFTypeRef __nullable value, CFStringRef domain);

#pragma mark -
typedef void (*HIDPreferencesSetForInstancePtr)(CFTypeRef hidPreference, CFStringRef key, CFTypeRef __nullable value, CFStringRef user, CFStringRef host, CFStringRef domain);
typedef void (*HIDPreferencesSetMultipleForInstancePtr)(CFTypeRef hidPreference, CFDictionaryRef __nullable keysToSet , CFArrayRef __nullable keysToRemove, CFStringRef user, CFStringRef host, CFStringRef domain);
typedef CFTypeRef (*HIDPreferencesCopyForInstancePtr)(CFTypeRef hidPreference, CFStringRef key, CFStringRef user, CFStringRef host, CFStringRef domain);
typedef CFDictionaryRef (*HIDPreferencesCopyMultipleForInstancePtr)(CFTypeRef hidPreference, CFArrayRef __nullable keys, CFStringRef user, CFStringRef host, CFStringRef domain);
typedef void (*HIDPreferencesSynchronizeForInstancePtr)(CFTypeRef hidPreference, CFStringRef user, CFStringRef host, CFStringRef domain);
typedef CFTypeRef (*HIDPreferencesCopyDomainForInstancePtr)(CFTypeRef hidPreference, CFStringRef key, CFStringRef domain);
typedef void (*HIDPreferencesSetDomainForInstancePtr)(CFTypeRef hidPreference, CFStringRef key,  CFTypeRef __nullable value, CFStringRef domain);
typedef CFTypeRef (*HIDPreferencesCreateInstancePtr)(IOHIDPreferencesOption option);


static HIDPreferencesSetPtr __setPtr = NULL;
static HIDPreferencesSetMultiplePtr __setMultiplePtr = NULL;
static HIDPreferencesCopyPtr __copyPtr = NULL;
static HIDPreferencesCopyMultiplePtr __copyMultiplePtr = NULL;
static HIDPreferencesSynchronizePtr __synchronizePtr = NULL;
static HIDPreferencesCopyDomainPtr __copyDomainPtr = NULL;
static HIDPreferencesSetDomainPtr __setDomainPtr = NULL;

#pragma mark -
static HIDPreferencesSetForInstancePtr __setForInstancePtr = NULL;
static HIDPreferencesSetMultipleForInstancePtr __setMultipleForInstancePtr = NULL;
static HIDPreferencesCopyForInstancePtr __copyForInstancePtr = NULL;
static HIDPreferencesCopyMultipleForInstancePtr __copyMultipleForInstancePtr = NULL;
static HIDPreferencesSynchronizeForInstancePtr __synchronizeForInstancePtr = NULL;
static HIDPreferencesCopyDomainForInstancePtr __copyDomainForInstancePtr = NULL;
static HIDPreferencesSetDomainForInstancePtr __setDomainForInstancePtr = NULL;
static HIDPreferencesCreateInstancePtr __createPtr = NULL;

static void* HIDPreferencesFrameworkGetSymbol( void* handle, const char* symbolName) {
    
    if (!handle) {
        return NULL;
    }
    
    return dlsym(handle, symbolName);
}

static void __loadFramework()
{
 
    static void *haHandle = NULL;
    static dispatch_once_t haOnce = 0;
    
    dispatch_once(&haOnce, ^{
        
        haHandle = dlopen(HIDPreferencesFrameworkPath, RTLD_LAZY);
        // safe place for saving required symbols
        if (!haHandle) {
            IOHIDLog("Failed to load %s",HIDPreferencesFrameworkPath);
            return;
        }
        __setPtr = (HIDPreferencesSetPtr)HIDPreferencesFrameworkGetSymbol(haHandle, kHIDPreferencesSet);
        __setMultiplePtr = (HIDPreferencesSetMultiplePtr)HIDPreferencesFrameworkGetSymbol(haHandle, kHIDPreferencesSetMultiple);
        __copyPtr = (HIDPreferencesCopyPtr)HIDPreferencesFrameworkGetSymbol(haHandle, kHIDPreferencesCopy);
        __copyMultiplePtr = (HIDPreferencesCopyMultiplePtr)HIDPreferencesFrameworkGetSymbol(haHandle, kHIDPreferencesCopyMultiple);
        __synchronizePtr = (HIDPreferencesSynchronizePtr)HIDPreferencesFrameworkGetSymbol(haHandle, kHIDPreferencesSynchronize);
        __copyDomainPtr = (HIDPreferencesCopyDomainPtr)HIDPreferencesFrameworkGetSymbol(haHandle, kHIDPreferencesCopyDomain);
        __setDomainPtr = (HIDPreferencesSetDomainPtr)HIDPreferencesFrameworkGetSymbol(haHandle, kHIDPreferencesSetDomain);
        
        __setForInstancePtr = (HIDPreferencesSetForInstancePtr)HIDPreferencesFrameworkGetSymbol(haHandle, kHIDPreferencesSetForInstance);
        __setMultipleForInstancePtr = (HIDPreferencesSetMultipleForInstancePtr)HIDPreferencesFrameworkGetSymbol(haHandle, kHIDPreferencesSetMultipleForInstance);
        __copyForInstancePtr = (HIDPreferencesCopyForInstancePtr)HIDPreferencesFrameworkGetSymbol(haHandle, kHIDPreferencesCopyForInstance);
        __copyMultipleForInstancePtr = (HIDPreferencesCopyMultipleForInstancePtr)HIDPreferencesFrameworkGetSymbol(haHandle, kHIDPreferencesCopyMultipleForInstance);
        __synchronizeForInstancePtr = (HIDPreferencesSynchronizeForInstancePtr)HIDPreferencesFrameworkGetSymbol(haHandle, kHIDPreferencesSynchronizeForInstance);
        __copyDomainForInstancePtr = (HIDPreferencesCopyDomainForInstancePtr)HIDPreferencesFrameworkGetSymbol(haHandle, kHIDPreferencesCopyDomainForInstance);
        __setDomainForInstancePtr = (HIDPreferencesSetDomainForInstancePtr)HIDPreferencesFrameworkGetSymbol(haHandle, kHIDPreferencesSetDomainForInstance);
        __createPtr = (HIDPreferencesCreateInstancePtr)HIDPreferencesFrameworkGetSymbol(haHandle, kHIDPreferencesCreateInstance);
    });
    
}



void IOHIDPreferencesSet(CFStringRef key, CFTypeRef __nullable value, CFStringRef user, CFStringRef host, CFStringRef domain) {
    
    __loadFramework();
    //Switch back to original
    if (!__setPtr) {
        IOHIDLogInfo("Failed to find %s for set, switch to default CFPreferences",HIDPreferencesFrameworkPath);
        CFPreferencesSetValue(key, value, domain, user, host);
        return;
    }
    
    __setPtr(key, value, user, host, domain);
}

void IOHIDPreferencesSetMultiple(CFDictionaryRef __nullable keysToSet , CFArrayRef __nullable keysToRemove, CFStringRef user, CFStringRef host, CFStringRef domain) {
    
    __loadFramework();

    if (!__setMultiplePtr) {
        IOHIDLogInfo("Failed to find %s for set multiple , switch to default CFPreferences",HIDPreferencesFrameworkPath);
        CFPreferencesSetMultiple(keysToSet, keysToRemove, domain, user, host);
        return;
    }
    
    __setMultiplePtr(keysToSet, keysToRemove, user, host, domain);
}


CFTypeRef __nullable IOHIDPreferencesCopy(CFStringRef key, CFStringRef user, CFStringRef host, CFStringRef domain) {
    
    __loadFramework();

    if (!__copyPtr) {
        IOHIDLogInfo("Failed to find %s for copy, switch to default CFPreferences",HIDPreferencesFrameworkPath);
        return CFPreferencesCopyValue(key, domain, user, host);
    }
    
    return __copyPtr(key, user, host, domain);
    
}

CFDictionaryRef __nullable IOHIDPreferencesCopyMultiple(CFArrayRef __nullable keys, CFStringRef user, CFStringRef host, CFStringRef domain) {
    
    __loadFramework();

    if (!__copyMultiplePtr) {
        IOHIDLogInfo("Failed to find %s for copy multiple, switch to default CFPreferences",HIDPreferencesFrameworkPath);
        return CFPreferencesCopyMultiple(keys, domain, user, host);
    }
    
    return __copyMultiplePtr(keys, user, host, domain);
}

void IOHIDPreferencesSynchronize(CFStringRef user, CFStringRef host, CFStringRef domain) {
    
    __loadFramework();

    if (!__synchronizePtr) {
        IOHIDLogInfo("Failed to find %s for synchronize, switch to default CFPreferences",HIDPreferencesFrameworkPath);
        CFPreferencesSynchronize(domain, user, host);
        return;
    }
    
    __synchronizePtr(user, host, domain);
    
}

CFTypeRef __nullable IOHIDPreferencesCopyDomain(CFStringRef key, CFStringRef domain) {
    
    __loadFramework();

    if (!__copyDomainPtr) {
        IOHIDLogInfo("Failed to find %s for copy domain, switch to default CFPreferences",HIDPreferencesFrameworkPath);
        return CFPreferencesCopyAppValue(key, domain);;
    }
    
    return __copyDomainPtr(key, domain);
    
}

void IOHIDPreferencesSetDomain(CFStringRef key,  CFTypeRef __nullable value, CFStringRef domain) {
    
    __loadFramework();

    if (!__setDomainPtr) {
        IOHIDLogInfo("Failed to find %s for set domain, switch to default CFPreferences",HIDPreferencesFrameworkPath);
        CFPreferencesSetAppValue(key, value, domain);
        return;
    }
    
    __setDomainPtr(key, value, domain);
}

#pragma mark -
#pragma mark -

CFTypeRef __nullable IOHIDPreferencesCreateInstance(IOHIDPreferencesOption option) {
    __loadFramework();
    //Switch back to original
    if (!__createPtr) {
        IOHIDLogInfo("Failed to find %s for create",HIDPreferencesFrameworkPath);
        return NULL;
    }
    return __createPtr(option);
}

void IOHIDPreferencesSetForInstance(CFTypeRef hidPreference, CFStringRef key, CFTypeRef __nullable value, CFStringRef user, CFStringRef host, CFStringRef domain) {
    
    __loadFramework();
    //Switch back to original
    if (!__setForInstancePtr) {
        IOHIDLogInfo("Failed to find %s for set, switch to default CFPreferences",HIDPreferencesFrameworkPath);
        CFPreferencesSetValue(key, value, domain, user, host);
        return;
    }
    
    __setForInstancePtr(hidPreference, key, value, user, host, domain);
}

void IOHIDPreferencesSetMultipleForInstance(CFTypeRef hidPreference, CFDictionaryRef __nullable keysToSet , CFArrayRef __nullable keysToRemove, CFStringRef user, CFStringRef host, CFStringRef domain) {
    
    __loadFramework();

    if (!__setMultipleForInstancePtr) {
        IOHIDLogInfo("Failed to find %s for set multiple , switch to default CFPreferences",HIDPreferencesFrameworkPath);
        CFPreferencesSetMultiple(keysToSet, keysToRemove, domain, user, host);
        return;
    }
    
    __setMultipleForInstancePtr(hidPreference, keysToSet, keysToRemove, user, host, domain);
}


CFTypeRef __nullable IOHIDPreferencesCopyForInstance(CFTypeRef hidPreference, CFStringRef key, CFStringRef user, CFStringRef host, CFStringRef domain) {
    
    __loadFramework();

    if (!__copyForInstancePtr) {
        IOHIDLogInfo("Failed to find %s for copy, switch to default CFPreferences",HIDPreferencesFrameworkPath);
        return CFPreferencesCopyValue(key, domain, user, host);
    }
    
    return __copyForInstancePtr(hidPreference, key, user, host, domain);
    
}

CFDictionaryRef __nullable IOHIDPreferencesCopyMultipleForInstance(CFTypeRef hidPreference, CFArrayRef __nullable keys, CFStringRef user, CFStringRef host, CFStringRef domain) {
    
    __loadFramework();

    if (!__copyMultipleForInstancePtr) {
        IOHIDLogInfo("Failed to find %s for copy multiple, switch to default CFPreferences",HIDPreferencesFrameworkPath);
        return CFPreferencesCopyMultiple(keys, domain, user, host);
    }
    
    return __copyMultipleForInstancePtr(hidPreference, keys, user, host, domain);
}

void IOHIDPreferencesSynchronizeForInstance(CFTypeRef hidPreference, CFStringRef user, CFStringRef host, CFStringRef domain) {
    
    __loadFramework();

    if (!__synchronizeForInstancePtr) {
        IOHIDLogInfo("Failed to find %s for synchronize, switch to default CFPreferences",HIDPreferencesFrameworkPath);
        CFPreferencesSynchronize(domain, user, host);
        return;
    }
    
    __synchronizeForInstancePtr(hidPreference, user, host, domain);
    
}

CFTypeRef __nullable IOHIDPreferencesCopyDomainForInstance(CFTypeRef hidPreference , CFStringRef key, CFStringRef domain) {
    
    __loadFramework();

    if (!__copyDomainForInstancePtr) {
        IOHIDLogInfo("Failed to find %s for copy domain, switch to default CFPreferences",HIDPreferencesFrameworkPath);
        return CFPreferencesCopyAppValue(key, domain);;
    }
    
    return __copyDomainForInstancePtr(hidPreference, key, domain);
    
}

void IOHIDPreferencesSetDomainForInstance(CFTypeRef hidPreference, CFStringRef key,  CFTypeRef __nullable value, CFStringRef domain) {
    
    __loadFramework();

    if (!__setDomainForInstancePtr) {
        IOHIDLogInfo("Failed to find %s for set domain, switch to default CFPreferences",HIDPreferencesFrameworkPath);
        CFPreferencesSetAppValue(key, value, domain);
        return;
    }
    
    __setDomainForInstancePtr(hidPreference,  key, value, domain);
}

