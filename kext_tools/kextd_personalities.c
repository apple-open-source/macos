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
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitServer.h>
#include <IOKit/kext/OSKextPrivate.h>
#include <IOKit/IOCFSerialize.h>

#include <sys/stat.h>
#include <sys/mman.h>
#include <libc.h>
#include <zlib.h>

#include "kextd_main.h"
#include "kextd_personalities.h"
#include "kextd_usernotification.h"
#include "kextd_globals.h"

OSReturn sendDirectoryPersonaltiesToKernel(
    CFURLRef           directoryURL,
    const NXArchInfo * arch);
OSReturn sendCachedPersonalitiesToKernel(
    CFURLRef           folderURL,
    const NXArchInfo * arch);

/*******************************************************************************
*******************************************************************************/
OSReturn sendPersonalitiesToKernel(void)
{
    OSReturn   result                     = kOSReturnSuccess;  // optimistic
    CFArrayRef systemExtensionsFolderURLs = NULL;  // need not release
    CFIndex    count, i;

    systemExtensionsFolderURLs = OSKextGetSystemExtensionsFolderURLs();
    if (!systemExtensionsFolderURLs ||
        !CFArrayGetCount(systemExtensionsFolderURLs)) {

        result = kOSReturnError;
        goto finish;
    }

    count = CFArrayGetCount(systemExtensionsFolderURLs);
    for (i = 0; i < count; i++) {
        OSReturn directoryResult = sendDirectoryPersonaltiesToKernel(
            CFArrayGetValueAtIndex(systemExtensionsFolderURLs, i),
            gKernelArchInfo);
        if (directoryResult != kOSReturnSuccess) {
            result = kOSReturnError;
        }
    }

finish:
    return result;
}

/*******************************************************************************
*******************************************************************************/
OSReturn sendDirectoryPersonaltiesToKernel(
    CFURLRef           directoryURL,
    const NXArchInfo * arch)
{
    OSReturn          result                  = kOSReturnError;
    char              directoryPath[PATH_MAX] = "";
    CFArrayRef        kexts                   = NULL;  // must release
    CFMutableArrayRef authenticKexts          = NULL;  // must release
    CFIndex    count, i;

    if (!CFURLGetFileSystemRepresentation(directoryURL,
        /* resolveToBase */ true, (UInt8 *)directoryPath, sizeof(directoryPath))) {

        OSKextLogStringError(/* kext */ NULL);
        goto finish;
    }

   /* Note that we are going to finish on success here! If we
    * sent personalities we are done.
    * sendCachedPersonalitiesToKernel() logs a msg on failure.
    */
    result = sendCachedPersonalitiesToKernel(directoryURL, arch);
    if (result == kOSReturnSuccess) {
        goto finish;
    }
    // not sure we should try to rebuild the cache if the send to kernel fails
    // sendCachedPersonalitiesToKernel logged a msg
    // do not goto finish, try to send from kexts

   /* If we didn't send from cache, send from the kexts. This will cause
    * lots of I/O.
    */
    kexts = OSKextCreateKextsFromURL(kCFAllocatorDefault, directoryURL);
    if (!kexts) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Can't read kexts from %s.", directoryPath);
        goto finish;
    }

    if (!createCFMutableArray(&authenticKexts, &kCFTypeArrayCallBacks)) {
        OSKextLogMemError();
        goto finish;
    }

   /* Check all the kexts to see if we need to raise alerts
    * about improperly-installed extensions.
    */
    recordNonsecureKexts(kexts);

    count = CFArrayGetCount(kexts);
    for (i = 0; i < count; i++) {
        OSKextRef aKext = (OSKextRef)CFArrayGetValueAtIndex(kexts, i);
        if (OSKextIsAuthentic(aKext)) {
            CFArrayAppendValue(authenticKexts, aKext);
        }
    }

    result = OSKextSendPersonalitiesOfKextsToKernel(authenticKexts);
    if (result != kOSReturnSuccess) {
        goto finish;
    }

   /* Now try to write the cache file. Don't save the return value
    * of that function, we're more concerned with whether personalities
    * have actually gone to the kernel.
    */
    writePersonalitiesCache(authenticKexts, /* arch */ gKernelArchInfo,
        directoryURL);

finish:
    SAFE_RELEASE(kexts);
    SAFE_RELEASE(authenticKexts);

    return result;
}

/*******************************************************************************
*******************************************************************************/
OSReturn sendCachedPersonalitiesToKernel(
    CFURLRef           folderURL,
    const NXArchInfo * arch)
{
    OSReturn    result    = kOSReturnError;
    CFDataRef   cacheData = NULL;  // must release
    char        folderPath[PATH_MAX] = "";
    
    if (!CFURLGetFileSystemRepresentation(folderURL,
        /* resolveToBase */ true, (UInt8 *)folderPath, sizeof(folderPath))) {

        OSKextLogStringError(/* kext */ NULL);
        goto finish;
    }

    if (!_OSKextReadCache(folderURL, CFSTR(kIOKitPersonalitiesKey),
        arch, _kOSKextCacheFormatIOXML, /* parseXML? */ false,
        (CFPropertyListRef *)&cacheData)) {

        goto finish;
    }

    OSKextLog(/* kext */ NULL,
        kOSKextLogProgressLevel | kOSKextLogIPCFlag |
        kOSKextLogKextBookkeepingFlag,
        "Sending cached personalities for %s to IOCatalogue.",
        folderPath);

    result = IOCatalogueSendData(kIOMasterPortDefault, kIOCatalogAddDrivers,
        (char *)CFDataGetBytePtr(cacheData), CFDataGetLength(cacheData));
    if (result != kOSReturnSuccess) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogIPCFlag,
           "error: couldn't send personalities to the kernel");
        goto finish;
    } else {
        OSKextLog(/* kext */ NULL,
            kOSKextLogProgressLevel | kOSKextLogIPCFlag |
            kOSKextLogKextBookkeepingFlag,
            "Sent cached personalities for %s to the IOCatalogue.",
            folderPath);
    }
    
    result = kOSReturnSuccess;

finish:
    SAFE_RELEASE(cacheData);
    return result;
}

