/*
 * Copyright (c) 2008 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 * 
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
#ifndef __OSKEXTPRIVATE_H__
#define __OSKEXTPRIVATE_H__

#include <CoreFoundation/CoreFoundation.h>
#include <System/libkern/OSKextLib.h>
#include <System/libkern/OSKextLibPrivate.h>
#include <mach-o/arch.h>
#include <paths.h>
#include <sys/stat.h>

/* If you aren't IOKitUser or kext_tools, you shouldn't be using this
 * file. Its contents will change without warning.
 */
 
#if PRAGMA_MARK
/********************************************************************/
#pragma mark Standard System Extensions Folders
/********************************************************************/
#endif
/*
 * Only the first is currently used.
 */
#define _kOSKextNumSystemExtensionsFolders (1)

#define _kOSKextSystemLibraryExtensionsFolder           \
            "/System/Library/Extensions"
#define _kOSKextLibraryExtensionsFolder                 \
            "/Library/Extensions"
#define _kOSKextAppleInternalLibraryExtensionsFolder    \
            "/AppleInternal/Library/Extensions"

#if PRAGMA_MARK
/********************************************************************/
#pragma mark Kext Cache Folders & Files
/********************************************************************/
#endif
/*********************************************************************
* All kext cache files now live under /System/Library/Caches in
* com.apple.kext.caches. The system extensions folders are duplicated
* under this node, and at their bottom are the individual cache files
* for id->URL mapping, and for I/O Kit personalities (owned by the
* kext_tools project, specifically kextd(8) and kextcache(8)).
*
* Here's a schematic:
* ______________________________________________________________________
* /System/Library/Caches/com.apple.kext.caches/System/Library/Extensions/ ...
*     ID->URL Cache: KextIdentifiers.plist.gz (OSBundles?)
*     Personalities Cache: IOKitPersonalities_<arch>.plist.gz
*
* System boot caches (prelinked kernel and mkext) are in the
* com.apple.kext.caches folder. See the kext_tools project for more.
*********************************************************************/
#define _kOSKextCachesRootFolder                       \
            "/System/Library/Caches/com.apple.kext.caches"

#define _kOSKextDirectoryCachesSubfolder   "Directories"
#define _kOSKextStartupCachesSubfolder     "Startup"

#define _kOSKextStartupMkextFilename       "Extensions.mkext"
#define _kOSKextStartupMkextFolderPath     _kOSKextCachesRootFolder       "/" \
                                           _kOSKextStartupCachesSubfolder
#define _kOSKextStartupMkextPath           _kOSKextStartupMkextFolderPath "/" \
                                           _kOSKextStartupMkextFilename

#define _kOSKextIdentifierCacheBasename    "KextIdentifiers"
#define _kOSKextPrelinkedKernelBasename    "kernelcache"

#define _kOSKextCacheFileMode      (0644)
#define _kOSKextCacheFileModeMask  (0777)
#define _kOSKextCacheFolderMode    (0755)

#if PRAGMA_MARK
/********************************************************************/
#pragma mark Cache Functions
/********************************************************************/
#endif

typedef enum {
    _kOSKextCacheFormatRaw,
    _kOSKextCacheFormatCFXML,
    _kOSKextCacheFormatCFBinary,
    _kOSKextCacheFormatIOXML,
} _OSKextCacheFormat;

Boolean _OSKextReadCache(
    CFURLRef                  folderURL,
    CFStringRef               cacheName,
    const NXArchInfo        * arch,
    _OSKextCacheFormat        format,
    Boolean                   parseXMLFlag,
    CFPropertyListRef       * cacheContentsOut);
Boolean _OSKextCreateFolderForCacheURL(CFURLRef cacheURL);
Boolean _OSKextWriteCache(
    CFPropertyListRef         plist,
    CFURLRef                  folderURL,
    CFStringRef               cacheName,
    const NXArchInfo        * arch,
    _OSKextCacheFormat        format);
Boolean _OSKextReadFromIdentifierCacheForFolder(
    CFURLRef            anURL,
    CFMutableArrayRef * kextsOut);
Boolean _OSKextWriteIdentifierCacheForKextsInDirectory(
    CFArrayRef kextArray,
    CFURLRef   directoryURL,
    Boolean    forceFlag);
CFArrayRef _OSKextGetKernelRequests(void);
OSReturn _OSKextSendResource(
    CFDictionaryRef request,
    OSReturn        requestResult,
    CFDataRef       resource);

#if PRAGMA_MARK
/********************************************************************/
#pragma mark Logging Macros for Common Errors
/********************************************************************/
#endif

#if DEBUG

#define OSKextLogMemError()   \
    OSKextLog(NULL, \
        kOSKextLogErrorLevel | kOSKextLogGeneralFlag, \
        "Memory allocation failure, %s, line %d.", __FILE__, __LINE__)
#define OSKextLogStringError(kext)   \
    OSKextLog((kext), \
        kOSKextLogErrorLevel | kOSKextLogGeneralFlag, \
        "String/URL conversion failure, %s, line %d.", __FILE__, __LINE__)

#else /* DEBUG */

#define OSKextLogMemError()   \
    OSKextLog(NULL, \
        kOSKextLogErrorLevel | kOSKextLogGeneralFlag, \
        "Memory allocation failure.")
#define OSKextLogStringError(kext)   \
    OSKextLog((kext), \
        kOSKextLogErrorLevel | kOSKextLogGeneralFlag, \
        "String/URL conversion failure.")

#endif /* DEBUG */

#endif /* __OSKEXTPRIVATE_H__ */
