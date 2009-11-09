/*
 * Copyright (c) 2000-2008 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Authorization.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitServer.h>
#include <libc.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/bootstrap.h>
#include <mach/kmod.h>
#include <sys/proc_info.h>
#include <libproc.h>
#include <libgen.h>
#include <bsm/libbsm.h>
#include <servers/bootstrap.h>	// bootstrap mach ports

#include <IOKit/kext/kextmanager_types.h>
#include <IOKit/kext/OSKext.h>
#include <IOKit/kext/OSKextPrivate.h>
#include <IOKit/kext/KextManagerPriv.h>
#include <System/libkern/kext_request_keys.h>

#include "kext_tools_util.h"
#include "bootfiles.h"
#include "fork_program.h"
#include "kextd_globals.h"
#include "paths.h"
#include "kextd_request.h"
#include "kextd_main.h"
#include "kextd_usernotification.h"

#include "kextd_mach.h"  // mig-generated, not in project

#include "bootcaches.h"

CFArrayRef readKextPropertyValues(CFStringRef propertyKey);
kern_return_t sendPropertyValueResponse(
    CFArrayRef    propertyValues,
    char       ** xml_data_out,
    int         * xml_data_length);
void kextdProcessKernelLoadRequest(
    CFDictionaryRef   request);
void kextdProcessKernelResourceRequest(
    CFDictionaryRef   request);
kern_return_t kextdProcessUserLoadRequest(
    CFDictionaryRef request,
    uid_t           remote_euid,
    pid_t           remote_pid);
static OSReturn checkNonrootLoadAllowed(
    OSKextRef kext,
    uid_t     remote_euid,
    pid_t     remote_pid);

#pragma mark KextManager RPC routines & support
/*******************************************************************************
*******************************************************************************/
kern_return_t _kextmanager_path_for_bundle_id(
    mach_port_t       server,
    kext_bundle_id_t  bundle_id,
    posix_path_t      path,        // PATH_MAX
    OSReturn        * kext_result)
{
    kern_return_t result    = kOSReturnSuccess;
    CFStringRef   kextID    = NULL;  // must release
    OSKextRef     theKext   = NULL;  // must release
    CFURLRef      kextURL   = NULL;  // do not release
    CFURLRef      absURL    = NULL;  // must release

    *kext_result = kOSReturnError;
    path[0] = '\0';
    
    OSKextLog(/* kext */ NULL,
        kOSKextLogDebugLevel | kOSKextLogIPCFlag,
        "Received client request for path to bundle %s.",
        bundle_id);

    kextID = CFStringCreateWithCString(kCFAllocatorDefault, bundle_id,
        kCFStringEncodingUTF8);
    if (!kextID) {
        OSKextLogMemError();
        *kext_result = kOSKextReturnNoMemory;
        goto finish;
    }

    theKext = OSKextCreateWithIdentifier(kCFAllocatorDefault, kextID);
    if (!theKext) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogIPCFlag,
            "Kext %s not found for client path request.", bundle_id);
        *kext_result = kOSKextReturnNotFound;
        goto finish;
    }
    kextURL = OSKextGetURL(theKext);
    if (!kextURL) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogIPCFlag,
            "Kext %s found for client path request, but has no URL.", bundle_id);
        goto finish;
    }
    absURL = CFURLCopyAbsoluteURL(kextURL);
    if (!absURL) {
        OSKextLogMemError();
        *kext_result = kOSKextReturnNoMemory;
        goto finish;
    }
    if (!CFURLGetFileSystemRepresentation(absURL, /* resolveToBase */ true,
        (UInt8 *)path, PATH_MAX)) {
        
        *kext_result = kOSKextReturnSerialization;
        goto finish;
    }

    *kext_result = kOSReturnSuccess;

    OSKextLog(/* kext */ NULL,
        kOSKextLogDebugLevel | kOSKextLogIPCFlag,
        "Returning path %s for identifier %s.", path, bundle_id);

finish:
    SAFE_RELEASE(kextID);
    SAFE_RELEASE(theKext);
    SAFE_RELEASE(absURL);

    return result;
}

#pragma mark Loginwindow RPC routines & support
/*******************************************************************************
* This function is executed in the main thread after its run loop gets
* kicked by a client request.
*******************************************************************************/
kern_return_t _kextmanager_create_property_value_array(
    mach_port_t  server,
    char        * property_key,
    char       ** xml_data_out,
    int         * xml_data_length)

{
    kern_return_t result         = kOSReturnError;

    CFStringRef   propertyKey    = NULL;  // must release
    CFArrayRef    propertyValues = NULL;  // must release

    OSKextLog(/* kext */ NULL,
        kOSKextLogProgressLevel | kOSKextLogGeneralFlag,
        "Received client request for property value array.");

    if (!xml_data_out || !xml_data_length) {
        result = kOSKextReturnInvalidArgument;
        goto finish;
    }

    *xml_data_length = 0;
    *xml_data_out    = NULL;

    propertyKey = CFStringCreateWithCString(kCFAllocatorDefault, property_key,
        kCFStringEncodingUTF8);
    if (!propertyKey) {
        OSKextLogMemError();
        goto finish;
    }

    propertyValues = readKextPropertyValues(propertyKey);
    if (propertyValues) {
        result = sendPropertyValueResponse(propertyValues,
            xml_data_out, xml_data_length);
        goto finish;
    }

finish:
    SAFE_RELEASE(propertyKey);
    SAFE_RELEASE(propertyValues);

    OSKextFlushInfoDictionary(NULL /* all kexts */);
    OSKextFlushLoadInfo(NULL /* all kexts */, /* flushDependencies */ true);

    return result;
}

/*******************************************************************************
*******************************************************************************/
CFArrayRef readKextPropertyValues(CFStringRef propertyKey)
{
    CFMutableArrayRef result                     = NULL;
    CFArrayRef        systemExtensionsFolderURLs = NULL;  // need not release
    CFArrayRef        directoryValues            = NULL;  // must release
    CFIndex           count, i;

    systemExtensionsFolderURLs = OSKextGetSystemExtensionsFolderURLs();
    if (!systemExtensionsFolderURLs ||
        !CFArrayGetCount(systemExtensionsFolderURLs)) {
        goto finish;
    }

    result = CFArrayCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if (!result) {
        OSKextLogMemError();
        goto finish;
    }

    count = CFArrayGetCount(systemExtensionsFolderURLs);
    for (i = 0; i < count; i++) {
        CFURLRef directoryURL = CFArrayGetValueAtIndex(
            systemExtensionsFolderURLs, i);

        SAFE_RELEASE_NULL(directoryValues);
        
       /* We bravely soldier on in event of any errors getting the values.
        * Errors will be logged.
        */
        if (readKextPropertyValuesForDirectory(directoryURL, propertyKey,
            gKernelArchInfo, /* forceUpdate? */ false, &directoryValues)) {

            CFArrayAppendArray(result, directoryValues,
                RANGE_ALL(directoryValues));
        }
    }

finish:
    SAFE_RELEASE(directoryValues);
    return result;
}

/*******************************************************************************
*******************************************************************************/
kern_return_t sendPropertyValueResponse(
    CFArrayRef    propertyValues,
    char       ** xml_data_out,
    int         * xml_data_length)
{
    kern_return_t result    = kOSReturnError;
    CFDataRef     plistData = NULL;  // must release
    CFErrorRef    error     = NULL;  // must relase

    plistData = CFPropertyListCreateData(kCFAllocatorDefault,
         propertyValues, kCFPropertyListBinaryFormat_v1_0,
         /* options */ 0,
         &error);
    if (!plistData) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            "Can't create plist data for property value response.");
        log_CFError(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
            error);
        goto finish;
    }

    *xml_data_length = (int)CFDataGetLength(plistData);

    if (*xml_data_length) {
        result = vm_allocate(mach_task_self(), (vm_address_t *)xml_data_out,
            *xml_data_length, VM_FLAGS_ANYWHERE);
        if (result != kOSReturnSuccess) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
                "vm_allocate() failed.");
            goto finish;
        }
        memcpy(*xml_data_out, CFDataGetBytePtr(plistData), *xml_data_length);
    }

finish:
    SAFE_RELEASE(plistData);
    SAFE_RELEASE(error);
    
    return result;
}

#pragma mark Kernel Kext Requests
/*******************************************************************************
* Incoming MIG message from kernel to let us know we should fetch requests from
* it using kextd_process_kernel_requests().
*******************************************************************************/
kern_return_t svc_kextd_ping(mach_port_t mp __unused)
{
    bool shutdownRequested = false;

    if (_gKextutilLock) {
        gKernelRequestsPending = true;
        return kOSReturnSuccess;
    } else {
        shutdownRequested = kextd_process_kernel_requests();
        if (shutdownRequested) {
            CFRunLoopStop(CFRunLoopGetCurrent());
        }
    }
    return kOSReturnSuccess;
}

/*******************************************************************************
*******************************************************************************/
bool kextd_process_kernel_requests(void)
{
    CFArrayRef      kernelRequests           = NULL;  // must release
    Boolean         prelinkedKernelRequested = false;
    Boolean         shutdownRequested        = false;
    char          * scratchCString           = NULL;  // must free
    CFIndex         count, i;

   /* Stay in the while loop until _OSKextGetKernelRequests() returns
    * no more requests.
    */
    while (1) {
        SAFE_RELEASE_NULL(kernelRequests);
        kernelRequests = _OSKextGetKernelRequests();
        if (!kernelRequests || !CFArrayGetCount(kernelRequests)) {
            break;
        }
        
        count = CFArrayGetCount(kernelRequests);
        for (i = 0; i < count; i++) {
            CFDictionaryRef request      = NULL; // do not release
            CFStringRef     predicate    = NULL; // do not release

            SAFE_FREE_NULL(scratchCString);

            request = CFArrayGetValueAtIndex(kernelRequests, i);
            predicate = request ? CFDictionaryGetValue(request,
                CFSTR(kKextRequestPredicateKey)) : NULL;

            if (!request) {
                OSKextLog(/* kext */ NULL,
                    kOSKextLogErrorLevel | kOSKextLogIPCFlag,
                    "Empty kernel request.");
                continue;
            }
            if (!predicate) {
                OSKextLog(/* kext */ NULL,
                    kOSKextLogErrorLevel | kOSKextLogIPCFlag,
                    "No predicate in kernel request.");
                continue;
            }

           /* Check the request predicate and process it or note it needs
            * to be processed.
            */
            if (CFEqual(predicate, CFSTR(kKextRequestPredicateRequestPrelink))) {
                OSKextLog(/* kext */ NULL,
                    kOSKextLogProgressLevel | kOSKextLogIPCFlag,
                    "Got prelink kernel request.");
                prelinkedKernelRequested = true;
            } else if (CFEqual(predicate, CFSTR(kKextRequestPredicateRequestKextdExit))) {
                OSKextLog(/* kext */ NULL,
                    kOSKextLogProgressLevel | kOSKextLogIPCFlag,
                    "Got exit request from kernel.");
                shutdownRequested = true;
            } else if (CFEqual(predicate, CFSTR(kKextRequestPredicateRequestLoad))) {
                OSKextLog(/* kext */ NULL,
                    kOSKextLogProgressLevel | kOSKextLogIPCFlag,
                    "Got load request from kernel.");
                kextdProcessKernelLoadRequest(request);
            } else if (CFEqual(predicate, CFSTR(kKextRequestPredicateRequestResource))) {
                OSKextLog(/* kext */ NULL,
                    kOSKextLogProgressLevel | kOSKextLogIPCFlag,
                    "Got resource file request from kernel.");
                kextdProcessKernelResourceRequest(request);
            } else {
                scratchCString = createUTF8CStringForCFString(predicate);
                OSKextLog(/* kext */ NULL,
                    kOSKextLogErrorLevel | kOSKextLogIPCFlag,
                    "Unknown predicate%s%s in kernel request.",
                    scratchCString ? " " : "",
                    scratchCString ? scratchCString : "");
            }
        } /* for (i = 0; i < count; i++) */
    } /* while (1) */
    
// finish:

    if (prelinkedKernelRequested && !isBootRootActive()) {
        char * const kextcacheArgs[] = {
            "/usr/sbin/kextcache",
            "-F",
            "-system-prelinked-kernel",
            NULL };

        OSKextLog(/* kext */ NULL,
            kOSKextLogProgressLevel | kOSKextLogGeneralFlag,
            "Building prelinked kernel.");

        (void)fork_program("/usr/sbin/kextcache",
            kextcacheArgs,
            /* waitFlag */ false);

    }
    
    gKernelRequestsPending = false;

    SAFE_FREE(scratchCString);
    SAFE_RELEASE(kernelRequests);

    OSKextFlushInfoDictionary(NULL /* all kexts */);
    OSKextFlushLoadInfo(NULL /* all kexts */, /* flushDependencies */ true);

    return shutdownRequested;
}

/*******************************************************************************
* Kernel load request.
*******************************************************************************/
void
kextdProcessKernelLoadRequest(CFDictionaryRef   request)
{
    CFDictionaryRef requestArgs     = NULL; // do not release
    OSKextRef       osKext          = NULL; // do not release
    OSReturn        osLoadResult    = kOSKextReturnNotFound;

    CFArrayRef      loadList        = NULL;  // must release
    CFStringRef     kextIdentifier  = NULL;  // do not release
    CFStringRef     kextPath        = NULL;  // must release
    char          * kext_id         = NULL;  // must free

    requestArgs = request ? CFDictionaryGetValue(request,
        CFSTR(kKextRequestArgumentsKey)) : NULL;
    kextIdentifier = requestArgs ? CFDictionaryGetValue(requestArgs,
        CFSTR(kKextRequestArgumentBundleIdentifierKey)) : NULL;

    if (!requestArgs) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
            "No arguments in kernel kext load request.");
        goto finish;
    }
    if (!kextIdentifier) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
            "No kext ID in kernel kext load request.");
        goto finish;
    }
    kext_id = createUTF8CStringForCFString(kextIdentifier);
    if (!kext_id) {
        // xxx - not much we can do here.
        OSKextLogMemError();
        goto finish;
    }

   /* Read the extensions if necessary (also resets the release timer).
    */
    readExtensions();

    OSKextLog(/* kext */ NULL,
        kOSKextLogProgressLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
        "Kernel requests kext with id %s.", kext_id);

    osKext = OSKextGetKextWithIdentifier(kextIdentifier);
    if (!osKext) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
            "Kext id %s not found; removing personalities.", kext_id);
        OSKextRemovePersonalitiesForIdentifierFromKernel(kextIdentifier);
        goto finish;
    }

   /* xxx - under what circumstances should we remove personalities?
    * xxx - if the request gets into the kernel and fails, OSKext.cpp
    * xxx - removes them, but there can be other failures on the way....
    */
    osLoadResult = OSKextLoad(osKext);
    if (osLoadResult != kOSReturnSuccess) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
            "Load %s failed; removing personalities.", kext_id);
        OSKextRemoveKextPersonalitiesFromKernel(osKext);
    } else {
        if (kOSReturnSuccess != IOCatalogueModuleLoaded(
            kIOMasterPortDefault, kext_id)) {

            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
                "Failed to notify IOCatalogue that %s loaded.",
                kext_id);
        } else {
            OSKextLog(/* kext */ NULL,
                kOSKextLogProgressLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
                "Loaded %s and notified IOCatalogue.",
                kext_id);
        }
    }

    if (osLoadResult == kOSKextReturnAuthentication) {
        loadList = OSKextCopyLoadList(osKext, /* needAll? */ false);
        recordNonsecureKexts(loadList);
    }

finish:
    SAFE_FREE(kext_id);
    SAFE_RELEASE(kextPath);
    return;
}

/*******************************************************************************
* Kernel resource file request.
*******************************************************************************/
#define kDSStoreFilename   ".DS_Store"

void
kextdProcessKernelResourceRequest(
    CFDictionaryRef   request)
{
    OSReturn        osResult               = kOSReturnError;

    CFDictionaryRef requestArgs            = NULL;  // do not release
    OSKextRef       osKext                 = NULL;  // must release
    CFDataRef       resource               = NULL;  // must release
    OSReturn        requestResult          = kOSKextReturnInvalidArgument;

    CFStringRef     kextIdentifier         = NULL;  // do not release
    CFStringRef     resourceName           = NULL;  // do not release
    CFURLRef        kextURL                = NULL;  // do not release
    char          * kextIdentifierCString  = NULL;  // must free
    char          * resourceNameCString    = NULL;  // must free
    char            kextPathCString[PATH_MAX];

    requestArgs = request ? CFDictionaryGetValue(request,
        CFSTR(kKextRequestArgumentsKey)) : NULL;
    kextIdentifier = requestArgs ? CFDictionaryGetValue(requestArgs,
        CFSTR(kKextRequestArgumentBundleIdentifierKey)) : NULL;
    resourceName = requestArgs ? CFDictionaryGetValue(requestArgs,
        CFSTR(kKextRequestArgumentNameKey)) : NULL;

    OSKextLog(/* kext */ NULL,
        kOSKextLogDebugLevel | kOSKextLogIPCFlag,
        "Request for resource.");

    if (!requestArgs) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogProgressLevel | kOSKextLogIPCFlag,
            "No arguments in kernel kext resource request.");
        goto finish;
    }
    if (!kextIdentifier) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogProgressLevel | kOSKextLogIPCFlag,
            "No kext ID in kernel kext resource request.");
        goto finish;
    }
    if (!resourceName) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogProgressLevel | kOSKextLogIPCFlag,
            "No resource name in kernel kext resource request.");
        goto finish;
    }

    requestResult = kOSKextReturnNoMemory;

    kextIdentifierCString = createUTF8CStringForCFString(kextIdentifier);
    resourceNameCString = createUTF8CStringForCFString(resourceName);
    if (!kextIdentifierCString || !resourceNameCString) {
        // xxx - not much we can do here.
        OSKextLogMemError();
        goto finish;
    }

    if (CFEqual(resourceName, CFSTR(kDSStoreFilename))) {
        requestResult = kOSKextReturnInvalidArgument;
        OSKextLog(/* kext */ NULL,
            kOSKextLogProgressLevel | kOSKextLogIPCFlag | kOSKextLogFileAccessFlag,
            "Request for %s resource by %s - not allowed.",
            kDSStoreFilename, kextIdentifierCString);
        goto finish;
    }

   /* Read the extensions if necessary (also resets the release timer).
    */
    readExtensions();

    OSKextLog(/* kext */ NULL,
        kOSKextLogProgressLevel | kOSKextLogIPCFlag,
        "Kernel requests resource %s from kext id %s.",
        resourceNameCString, kextIdentifierCString);

    requestResult = kOSKextReturnNotFound;

    osKext = OSKextCreateWithIdentifier(kCFAllocatorDefault, kextIdentifier);
    if (!osKext) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogProgressLevel | kOSKextLogIPCFlag,
            "Kext id %s not found; can't retrieve requested resource.",
            kextIdentifierCString);
        goto finish;
    }
    
    requestResult = kOSReturnError;

    kextURL = OSKextGetURL(osKext);
    if (!kextURL ||
        !CFURLGetFileSystemRepresentation(kextURL, /* resolveToBase? */ TRUE,
            (UInt8 *)kextPathCString, sizeof(kextPathCString))) {

            strlcpy(kextPathCString, "(unknown)", sizeof("(unknown)"));
    }

    OSKextLog(/* kext */ NULL,
        kOSKextLogProgressLevel | kOSKextLogIPCFlag,
        "Seeking resource %s in %s.",
        resourceNameCString, kextPathCString);

    if (!OSKextIsValid(osKext)) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogProgressLevel | kOSKextLogIPCFlag | kOSKextLogValidationFlag,
            "%s is not valid; can't retrieve requested resource.",
            kextPathCString);
        requestResult = kOSKextReturnValidation;
        goto finish;
    }
    

    if (!OSKextIsAuthentic(osKext)) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogProgressLevel | kOSKextLogIPCFlag | kOSKextLogAuthenticationFlag,
            "%s is not authentic; can't retrieve requested resource.",
            kextPathCString);
        requestResult = kOSKextReturnAuthentication;
        goto finish;
    }
    
    resource = OSKextCopyResource(osKext, resourceName,
        /* resourceType */ NULL);
    if (!resource) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogProgressLevel | kOSKextLogIPCFlag | kOSKextLogFileAccessFlag,
            "Can't find resource %s in %s.",
            resourceNameCString, kextPathCString);
        requestResult = kOSKextReturnNotFound;
        goto finish;
    }

    requestResult = kOSReturnSuccess;

    OSKextLog(/* kext */ NULL,
        kOSKextLogProgressLevel | kOSKextLogIPCFlag | kOSKextLogValidationFlag,
        "Found resource %s in %s; sending to kernel.",
        resourceNameCString, kextPathCString);

finish:
    // now we send it to the kernel
    osResult = _OSKextSendResource(request, requestResult, resource);

    SAFE_RELEASE(resource);
    SAFE_RELEASE(osKext);
    SAFE_FREE(kextIdentifierCString);
    SAFE_FREE(resourceNameCString);
    return;
}

#pragma mark User Space Kext Load Requests
/*******************************************************************************
* User space load request.
*******************************************************************************/
kern_return_t
_kextmanager_load_kext(
    mach_port_t   server,
    audit_token_t audit_token,
    char        * xml_data_in,
    int           xml_data_length)
{
    OSReturn        result      = kOSReturnError;
    CFDataRef       requestData = NULL;  // must release
    CFDictionaryRef request     = NULL;  // must release
    CFErrorRef      error       = NULL;  // must release
    pid_t           remote_pid  = -1;
    uid_t           remote_euid = -1;

    requestData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
        (const UInt8 *)xml_data_in, xml_data_length,
        /* deallocator */ kCFAllocatorNull);
    if (!requestData) {
        OSKextLogMemError();
        result = kOSKextReturnNoMemory;
        goto finish;
    }
    request = CFPropertyListCreateWithData(kCFAllocatorDefault,
        requestData, /* options */ 0, /* format */ NULL,
        &error);
    if (!request) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogIPCFlag,
            "Can't read kext load request.");
        log_CFError(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogIPCFlag,
            error);
        result = kOSKextReturnSerialization;
        goto finish;
    }
    if (CFGetTypeID(request) != CFDictionaryGetTypeID()) {
        result = kOSKextReturnBadData;
        goto finish;
    }

    audit_token_to_au32(audit_token, /* audit UID */ NULL,
            &remote_euid, /* egid */ NULL, /* ruid */ NULL, /* rgid */ NULL,
            &remote_pid, /* asid */ NULL, /* au_tid_t */ NULL);

    result = kextdProcessUserLoadRequest(request, remote_euid, remote_pid);

finish:
    SAFE_RELEASE(requestData);
    SAFE_RELEASE(request);
    SAFE_RELEASE(error);

   /* MIG is consume-on-success
    * xxx - do we need separate result & op-result?
    */
    if (result == kOSReturnSuccess) {
        vm_deallocate(mach_task_self(), (vm_address_t)xml_data_in,
            (vm_size_t)xml_data_length);
    }
    return result;
}

/*******************************************************************************
*******************************************************************************/
const char * nameForPID(pid_t pid)
{
    char * result      = NULL;
    int    path_length = 0;
    char   path[PROC_PIDPATHINFO_MAXSIZE];

    path_length = proc_pidpath(pid, path,
        sizeof(path));
    if (path_length > 0) {
        result = basename(path);
    }
    if (!result) {
        result = "(unknown)";
    }
    return result;
}

/*******************************************************************************
*******************************************************************************/
#define UNKNOWN_KEXT  "unknown kext"
#define SYSTEM_FOLDER "/System/"

#define _kSystemExtensionsDirSlash   (kSystemExtensionsDir "/")
#define _kSystemFilesystemsDirSlash  ("/System/Library/Filesystems/")

/*******************************************************************************
*******************************************************************************/
static CFURLRef createAbsOrRealURLForURL(
    CFURLRef   anURL,
    uid_t      remote_euid,
    pid_t      remote_pid,
    OSReturn * error)
{
    CFURLRef result      = NULL;
    OSReturn localError  = kOSReturnSuccess;
    Boolean  inSLE       = FALSE;
    Boolean  inSLF       = FALSE;
    Boolean  checkedReal = FALSE;
    char     urlPathCString[PATH_MAX];
    char     realpathCString[PATH_MAX];

    if (!CFURLGetFileSystemRepresentation(anURL, /* resolveToBase? */ TRUE,
        (UInt8 *)urlPathCString, sizeof(urlPathCString)))
    {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
            "Can't get path from URL for kext load request.");
        localError = kOSKextReturnSerialization;
        goto finish;
    }

    if (remote_euid == 0) {
        result = CFURLCopyAbsoluteURL(anURL);
        if (!result) {
            OSKextLogMemError();
            goto finish;
        }
        goto finish;
    } else {

        inSLE = (0 == strncmp(urlPathCString, _kSystemExtensionsDirSlash,
            strlen(_kSystemExtensionsDirSlash)));
        inSLF = (0 == strncmp(urlPathCString, _kSystemFilesystemsDirSlash,
            strlen(_kSystemFilesystemsDirSlash)));

       /*****
        * May want to open these checks to use OSKextGetSystemExtensionsFolderURLs().
        * For now, keep it tight and just do /System/Library/Extensions & Filesystems.
        */
        if (!inSLE && !inSLF) {
            localError = kOSKextReturnNotPrivileged;
            if (!inSLE && !inSLF) {
                OSKextLog(/* kext */ NULL,
                    kOSKextLogErrorLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
                    "Request from non-root process '%s' (euid %d) to load %s - "
                    "not in system extensions or filesystems folder%s.",
                    nameForPID(remote_pid), remote_euid, urlPathCString,
                    checkedReal ? " (symlink redirect?)" : "");
            }
            goto finish;
        }

        if (!realpath(urlPathCString, realpathCString)) {

            localError = kOSReturnError; // xxx - should we have a filesystem error?
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
                "Unable to resolve raw path %s.", urlPathCString);
            goto finish;
        }

       /*****
        * Check the path once more now that we've resolved it with realpath().
        */
        inSLE = (0 == strncmp(realpathCString, _kSystemExtensionsDirSlash,
            strlen(_kSystemExtensionsDirSlash)));
        inSLF = (0 == strncmp(realpathCString, _kSystemFilesystemsDirSlash,
            strlen(_kSystemFilesystemsDirSlash)));
        checkedReal = TRUE;

        if (!inSLE && !inSLF) {

            localError = kOSKextReturnNotPrivileged;
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
                "Request from non-root process '%s' (euid %d) to load %s - "
                "(real path %s) - not in system extensions or filesystems folder.",
                nameForPID(remote_pid), remote_euid, urlPathCString,
                realpathCString);
            goto finish;
        }
    }

    result = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
        (UInt8 *)realpathCString, strlen(realpathCString), /* isDir */ TRUE);
    if (!result) {
        OSKextLogMemError();
        goto finish;
    }

finish:
    if (error) {
        *error = localError;
    }
    return result;
}

/*******************************************************************************
*******************************************************************************/
static OSReturn
checkNonrootLoadAllowed(
    OSKextRef kext,
    uid_t     remote_euid,
    pid_t     remote_pid)
{
    OSReturn    result       = kOSKextReturnNotPrivileged;
    CFArrayRef  loadList     = NULL;  // must release
    CFStringRef kextPath     = NULL;  // must release
    Boolean     kextAllows   = TRUE;
    Boolean     systemFolder = TRUE;
    char        kextPathCString[PATH_MAX];
    CFIndex     count, index;

    loadList = OSKextCopyLoadList(kext, /* needAll?*/ TRUE);
    if (!loadList) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogLoadFlag |
            kOSKextLogDependenciesFlag | kOSKextLogIPCFlag,
            "Can't resolve dependencies for kext load request.");
        result = kOSKextReturnDependencies;
        goto finish;
    }
    
    count = CFArrayGetCount(loadList);
    for (index = count - 1; index >= 0; index--) {
        OSKextRef thisKext = (OSKextRef)CFArrayGetValueAtIndex(loadList, index);
        CFBooleanRef allowed = (CFBooleanRef)OSKextGetValueForInfoDictionaryKey(
            thisKext, CFSTR(kOSBundleAllowUserLoadKey));
        CFURLRef  kextURL = OSKextGetURL(thisKext);

        SAFE_RELEASE_NULL(kextPath);

        kextPath = CFURLCopyFileSystemPath(kextURL, kCFURLPOSIXPathStyle);
        if (!kextPath) {
            OSKextLogMemError();
            result = kOSKextReturnNoMemory;
        }
        if (!CFURLGetFileSystemRepresentation(kextURL, /* resolveToBase? */ TRUE,
            (UInt8 *)kextPathCString, sizeof(kextPathCString))) {
            strlcpy(kextPathCString, UNKNOWN_KEXT, sizeof(UNKNOWN_KEXT));
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
                "Can't get path from URL for kext load request.");
            result = kOSKextReturnSerialization;
            goto finish;
        }

        if (!allowed ||
            (CFGetTypeID(allowed) != CFBooleanGetTypeID()) ||
            !CFBooleanGetValue(allowed)) {

            kextAllows = FALSE;
            goto finish;
        }
    }

    result = kOSReturnSuccess;
    
finish:
    SAFE_RELEASE(loadList);
    SAFE_RELEASE(kextPath);

    if (!kextAllows || !systemFolder) {

        result = kOSKextReturnNotPrivileged;

        if (!kextAllows) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
                "Request from non-root process '%s' (euid %d) to load %s - not allowed.",
                nameForPID(remote_pid), remote_euid, kextPathCString);
        }
        if (!systemFolder) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
                "Request from non-root process '%s' (euid %d) to load %s - not in /System/.",
                nameForPID(remote_pid), remote_euid, kextPathCString);
        }
    }

    return result;
}

/*******************************************************************************
*******************************************************************************/
kern_return_t
kextdProcessUserLoadRequest(
    CFDictionaryRef request,
    uid_t           remote_euid,
    pid_t           remote_pid)
{
    OSReturn          result                   = kOSReturnError;
    CFStringRef       kextID                   = NULL;  // do not release
    CFStringRef       kextPath                 = NULL;  // do not release
    CFArrayRef        dependencyPaths          = NULL;  // do not release
    CFURLRef          kextURL                  = NULL;  // must release
    CFURLRef          kextAbsURL               = NULL;  // must release
    OSKextRef         theKext                  = NULL;  // must release
    CFArrayRef        kexts                    = NULL;  // must release
    CFMutableArrayRef dependencyURLs           = NULL;  // must release
    CFURLRef          dependencyURL            = NULL;  // must release
    CFURLRef          dependencyAbsURL         = NULL;  // must release
    CFArrayRef        dependencyKexts          = NULL;  // must release
    CFArrayRef        loadList                 = NULL;  // must release
    char              scratchCString[PATH_MAX] = "unknown";
    CFIndex           count, index;

   /* First get the identifier or URL to load, and convert it to a C string
    * for logging.
    */
    kextID = (CFStringRef)CFDictionaryGetValue(request, kKextLoadIdentifierKey);
    if (kextID) {
        if (CFGetTypeID(kextID) != CFStringGetTypeID()) {
            result = kOSKextReturnInvalidArgument;
            goto finish;
        }
        CFStringGetCString(kextID, scratchCString,
            sizeof(scratchCString), kCFStringEncodingUTF8);
    } else {
        kextPath = (CFStringRef)CFDictionaryGetValue(request, kKextLoadPathKey);
        if (!kextPath || CFGetTypeID(kextPath) != CFStringGetTypeID()) {
            result = kOSKextReturnInvalidArgument;
            goto finish;
        }

        if (!CFStringHasPrefix(kextPath, CFSTR("/"))) {
            OSKextLog(/* kext */ NULL,
                kOSKextLogErrorLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
                "Error: Request from '%s' (euid %d) to load kext with relative path.",
                nameForPID(remote_pid), remote_euid);
            result = kOSKextReturnInvalidArgument;
            goto finish;
        }

        kextURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
            kextPath, kCFURLPOSIXPathStyle, /* isDir? */ true);
        if (!kextURL) {
            result = kOSKextReturnSerialization;  // xxx - or other?
            goto finish;
        }
        kextAbsURL = createAbsOrRealURLForURL(kextURL,
            remote_euid, remote_pid, &result);
        if (!kextAbsURL) {
            goto finish;
        }
        CFURLGetFileSystemRepresentation(kextAbsURL, /* resolveToBase */ true,
            (UInt8 *)scratchCString, sizeof(scratchCString));
    }

   /* Read the extensions if necessary (also resets the release timer).
    */
    readExtensions();

   /* Now log before the attempt, then try to look up or create the kext.
    */
    if (remote_euid != 0) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogProgressLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
                "Request from '%s' (euid %d) to load %s.",
                nameForPID(remote_pid), remote_euid, scratchCString);
    }

   /* Open any dependencies provided, *before* we create the kext, since
    * a request by identifier must be resolvable from the dependencies
    * as well as system extensions folders.
    */
    dependencyPaths = (CFArrayRef)CFDictionaryGetValue(request,
        kKextLoadDependenciesKey);
    if (dependencyPaths) {
        if (CFGetTypeID(dependencyPaths) != CFArrayGetTypeID()) {
            result = kOSKextReturnInvalidArgument;
            goto finish;
        }
        
        count = CFArrayGetCount(dependencyPaths);

        dependencyURLs = CFArrayCreateMutable(kCFAllocatorDefault,
            /* capacity */ count,
            &kCFTypeArrayCallBacks);
        if (!dependencyURLs) {
            result = kOSKextReturnNoMemory;
            goto finish;
        }
        
        for (index = 0; index < count; index++) {
            CFStringRef thisPath = (CFStringRef)CFArrayGetValueAtIndex(
                dependencyPaths, index);

            SAFE_RELEASE_NULL(dependencyURL);
            SAFE_RELEASE_NULL(dependencyAbsURL);
            if (CFGetTypeID(thisPath) != CFStringGetTypeID()) {
                result = kOSKextReturnInvalidArgument;
                goto finish;
            }
            if (!CFStringHasPrefix(thisPath, CFSTR("/"))) {
                OSKextLog(/* kext */ NULL,
                    kOSKextLogErrorLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
                    "Error: Request to load kext using dependency with relative path.");
                result = kOSKextReturnInvalidArgument;
                goto finish;
            }

            dependencyURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                thisPath, kCFURLPOSIXPathStyle, /* isDir? */ true);
            if (!dependencyURL) {
                result = kOSKextReturnSerialization;  // xxx - or other?
                goto finish;
            }
            dependencyAbsURL = createAbsOrRealURLForURL(dependencyURL,
                remote_euid, remote_pid, &result);
            if (!dependencyAbsURL) {
                goto finish;
            }
            CFArrayAppendValue(dependencyURLs, dependencyAbsURL);
        }
        dependencyKexts = OSKextCreateKextsFromURLs(kCFAllocatorDefault,
            dependencyURLs);
        if (!dependencyKexts) {
            result = kOSReturnError;
            goto finish;
        }
    }

    if (kextID) {
        theKext = OSKextGetKextWithIdentifier(kextID);
        if (theKext) {
            CFRetain(theKext);  // we're going to release it
        }
    } else {
       /* Make sure we also read the plugins of the kext we're asked to load,
        * but only if we manage to open the kext itself (or we'll get too many
        * error messages).
        */
        theKext = OSKextCreate(kCFAllocatorDefault, kextAbsURL);
        if (theKext) {
            kexts = OSKextCreateKextsFromURL(kCFAllocatorDefault, kextAbsURL);
        }
    }

    if (!theKext) {
        OSKextLog(/* kext */ NULL,
            kOSKextLogErrorLevel | kOSKextLogLoadFlag | kOSKextLogIPCFlag,
            "Error: Kext %s - not found/unable to create.", scratchCString);
        result = kOSKextReturnNotFound;
        goto finish;
    }

    if (remote_euid != 0) {
        result = checkNonrootLoadAllowed(theKext, remote_euid, remote_pid);
        if (result != kOSReturnSuccess) {
            goto finish;
        }
    }

   /* The codepath from this function will do any error logging
    * and cleanup needed.
    */
    result = OSKextLoadWithOptions(theKext,
        /* statExclusion */ kOSKextExcludeNone,
        /* addPersonalitiesExclusion */ kOSKextExcludeNone,
        /* personalityNames */ NULL,
        /* delayAutounloadFlag */ false);
    
    if (result == kOSKextReturnAuthentication) {
        loadList = OSKextCopyLoadList(theKext, /* needAll? */ false);
        recordNonsecureKexts(loadList);
    }

finish:            
    SAFE_RELEASE(kextURL);
    SAFE_RELEASE(kextAbsURL);
    SAFE_RELEASE(kexts);
    SAFE_RELEASE(theKext);
    SAFE_RELEASE(dependencyURLs);
    SAFE_RELEASE(dependencyURL);
    SAFE_RELEASE(dependencyAbsURL);
    SAFE_RELEASE(dependencyKexts);
    SAFE_RELEASE(loadList);
    return result;
}
