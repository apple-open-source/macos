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

#ifndef _KEXTPRIVATE_H_
#define _KEXTPRIVATE_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include <mach/mach_types.h>
#include <IOKit/kext/KEXTManager.h>
#include "KEXTDefs.h"

typedef struct _KEXTManager {
    SInt32 _refcount;
    Boolean _safeBoot;
    KEXTManagerMode _mode;
    KEXTManagerBundleLoadingCallbacks bcb;
    KEXTManagerModuleLoadingCallbacks mcb;
    KEXTManagerPersonalityLoadingCallbacks pcb;
    KEXTManagerConfigsCallbacks ccb;
    CFMutableDictionaryRef _entities;  // The database filled with 'entities'.
    CFMutableDictionaryRef _urlRels;   // URL relationships to bundles.
    CFMutableDictionaryRef _modRels;   // Bundle relationships to modules.
    CFMutableDictionaryRef _perRels;   // Bundle relationships to personalities..
    CFMutableDictionaryRef _cfgRels;   // Bundle relationships to configurations.
    CFMutableDictionaryRef _noloads;   // Dictionary of modules not to load.
    CFDateRef _configsDate;            // Modification date of the configs database.
    void (*_logErrorFunc)(const char * message); // error callback
    void (*_logMessageFunc)(const char * message); // info callback
    void * _context;
    mach_port_t _catPort;
} KEXTManager;


void _KEXTManagerShow(KEXTManagerRef manager);
void _KEXTBundleShow(KEXTBundleRef bundle);

KEXTReturn KERN2KEXTReturn(kern_return_t kr);

Boolean 	_KEXTModuleEqualCB(const void *ptr1, const void *ptr2);
Boolean 	_KEXTPersonalityEqualCB(const void *ptr1, const void *ptr2);
KEXTReturn	_KEXTPersonalityLoad(KEXTPersonalityRef personality, Boolean performMatching);
void    	_KEXTModuleCreatePath(KEXTModuleRef module, Boolean isCFStyle);
CFDictionaryRef	_KEXTModuleGetProperties(KEXTModuleRef module);
CFDictionaryRef _KEXTPersonalityGetProperties(KEXTPersonalityRef personality);
mach_port_t     _KEXTManagerGetMachPort(KEXTManagerRef manager);

KEXTReturn      KEXTSendDataToCatalog(mach_port_t port, int flag, CFTypeRef obj);
KEXTReturn	KEXTLoadModule(CFStringRef path, CFArrayRef dependencies);
KEXTReturn	KEXTLoadPersonality(CFDictionaryRef personalityDict, Boolean performMatching);

CFURLRef
_KEXTBundleCreateFileURL(KEXTBundleRef bundle, CFStringRef name, CFStringRef type);

/*!
    @function KEXTBundleCreate
    @abstract Creates a KEXTBundle object from the URL path.
    @param url An URL which is used to find the URL in the file system.
    @param error A refrence to a KEXTReturn variable.
    @result Returns a reference to a KEXTPersonality object or NULL upon error.  The error condition is returned via the 'error' argument.
*/
KEXTBundleRef		KEXTBundleCreate(CFURLRef url, CFDictionaryRef properties, Boolean isCFStyle);
/*!
    @function KEXTModuleCreate
    @abstract Creates a KEXTModule object from the property list provided and tagged with an URL for identification (typically the parent KEXTBundle's own URL is used).  This function need not be called when using KEXTBundle API's.
    @param url An URL which is used to identify it's parent KEXTBundle.
    @param properties The property list describing the attributes of the module.  This is taken from the parent bundle's Info-macosx.plist.
    @result Returns a reference to a KEXTModule object.
*/
KEXTModuleRef		KEXTModuleCreate(CFStringRef parentKey, CFDictionaryRef properties); //, Boolean isCFStyle);
/*!
    @function KEXTPersonalityCreate
    @abstract Creates a KEXTPersonality object from the property list provided and tagged with an URL for identification (typically the parent KEXTBundle's own URL is used).  This function need not be called when using KEXTBundle API's.
    @param url An URL which is used to identify it's parent KEXTBundle.
    @param properties The property list describing the attributes of the module.  This is taken from the parent bundle's Info-macosx.plist.
    @result Returns a reference to a KEXTPersonality object.
*/
KEXTPersonalityRef	KEXTPersonalityCreate(CFStringRef parentKey, CFDictionaryRef properties);
/*!
    @function KEXTManagerFree
    @abstract Deallocate's the KEXTManager and any other resources it may have allocated.
    @param manager A reference to KEXTManager object.
*/
void			KEXTManagerFree(KEXTManagerRef manager);

// Read config info database from disk, or update if it's already been read.
KEXTReturn              KEXTManagerScanConfigs(KEXTManagerRef manager, CFURLRef url);


static inline KEXTManager * deref(KEXTManagerRef manager) { return (KEXTManager *)manager; }

#if defined(__cplusplus)
} /* "C" */
#endif

#endif /* _KEXTPRIVATE_H_ */

