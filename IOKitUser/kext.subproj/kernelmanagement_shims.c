/*
 * Copyright (c) 2000-2008, 2012 Apple Inc. All rights reserved.
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

#include <pthread/pthread.h>
#include <sys/sysctl.h>
#include <SoftLinking/SoftLinking.h>

#include "OSKext.h"
#include "OSKextPrivate.h"

/* Avoid creating dependency cycles, since KernelManagement -> Foundaation -> ... -> IOKitUser */
SOFT_LINK_OPTIONAL_FRAMEWORK(Frameworks, KernelManagement);
SOFT_LINK_FUNCTION(KernelManagement, KMLoadExtensionsWithPaths, SOFT_KMLoadExtensionsWithPaths,
                   OSReturn,
                   (CFArrayRef paths),
                   (paths));
SOFT_LINK_FUNCTION(KernelManagement, KMExtensionPathForBundleIdentifier, SOFT_KMExtensionPathForBundleIdentifier,
                   CFStringRef,
                   (CFStringRef identifier),
                   (identifier));
SOFT_LINK_FUNCTION(KernelManagement, KMLoadExtensionsWithIdentifiers, SOFT_KMLoadExtensionsWithIdentifiers,
                   OSReturn,
                   (CFArrayRef identifiers),
                   (identifiers));

bool shimmingEnabled()
{
	uint32_t backOff = 0; // there's a new sheriff in town
	size_t sizeOfBackOff = sizeof(backOff);
	if (!(sysctlbyname("hw.use_kernelmanagerd", &backOff, &sizeOfBackOff, NULL, 0) == 0 && backOff)) {
		OSKextLog(NULL,
			kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
			"Shimming not enabled - defaulting to legacy behavior.");
		return false;
	}

	if (!isKernelManagementAvailable()) {
		OSKextLog(NULL,
			kOSKextLogErrorLevel | kOSKextLogGeneralFlag,
			"KernelManagement soft link failed - defaulting to legacy behavior.");
		return false;
	}
	return true;
}

CFStringRef kernelmanagement_path_for_bundle_id(CFStringRef identifier)
{
	CFStringRef result = SOFT_KMExtensionPathForBundleIdentifier(identifier);
	return result;
}

OSReturn kernelmanagement_load_kext_url(CFURLRef url)
{
    CFStringRef path = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
    if (!path) {
	return kOSReturnError;
    }

    const void *pathArray[] = { (void *)path };
    CFArrayRef paths = CFArrayCreate(kCFAllocatorDefault, (const void **)&pathArray, 1, &kCFTypeArrayCallBacks);
    if (!paths) {
        return kOSReturnError;
    }

    OSReturn result = SOFT_KMLoadExtensionsWithPaths(paths);

    CFRelease(paths);
    return result;
}

OSReturn kernelmanagement_load_kext_identifier(CFStringRef identifier)
{
    const void *identifiersArray[] = { (void *)identifier };
    CFArrayRef identifiers = CFArrayCreate(kCFAllocatorDefault, (const void **)&identifiersArray, 1, &kCFTypeArrayCallBacks);
    if (!identifiers) {
        return kOSReturnError;
    }

    OSReturn result = SOFT_KMLoadExtensionsWithIdentifiers(identifiers);

    CFRelease(identifiers);
    return result;
}
