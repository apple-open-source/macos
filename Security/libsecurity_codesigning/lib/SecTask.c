/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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
#include <CoreFoundation/CFRuntime.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFUnserialize.h>

#include <System/sys/codesign.h>

#include <pthread.h>

#include <bsm/libbsm.h>

#include "SecTask.h"

struct __SecTask {
	CFRuntimeBase base;

	pid_t pid;

	/* Track whether we've loaded entitlements independently since after the
	 * load, entitlements may legitimately be NULL */
	Boolean entitlementsLoaded;
	CFDictionaryRef entitlements;
};

enum {
	kSecCodeMagicEntitlement = 0xfade7171,		/* entitlement blob */
};


CFTypeID _kSecTaskTypeID = _kCFRuntimeNotATypeID;

static void SecTaskFinalize(CFTypeRef cfTask)
{
	SecTaskRef task = (SecTaskRef) cfTask;

	if (task->entitlements != NULL) {
		CFRelease(task->entitlements);
		task->entitlements = NULL;
	}
}

static CFStringRef SecTaskCopyDebugDescription(CFTypeRef cfTask)
{
	SecTaskRef task = (SecTaskRef) cfTask;

	return CFStringCreateWithFormat(CFGetAllocator(task), NULL, CFSTR("<SecTask %p>"), task);
}

static void SecTaskRegisterClass(void)
{
	static const CFRuntimeClass SecTaskClass = {
		.version = 0,
		.className = "SecTask",
		.init = NULL,
		.copy = NULL,
		.finalize = SecTaskFinalize,
		.equal = NULL,
		.hash = NULL,
		.copyFormattingDesc = NULL,
		.copyDebugDesc = SecTaskCopyDebugDescription,
	};

	_kSecTaskTypeID = _CFRuntimeRegisterClass(&SecTaskClass);
}

CFTypeID SecTaskGetTypeID(void)
{
	static pthread_once_t secTaskRegisterClassOnce = PTHREAD_ONCE_INIT;

	/* Register the class with the CF runtime the first time through */
	pthread_once(&secTaskRegisterClassOnce, SecTaskRegisterClass);

	return _kSecTaskTypeID;
}

static SecTaskRef SecTaskCreateWithPID(CFAllocatorRef allocator, pid_t pid)
{
	CFIndex extra = sizeof(struct __SecTask) - sizeof(CFRuntimeBase);
	SecTaskRef task = (SecTaskRef) _CFRuntimeCreateInstance(allocator, SecTaskGetTypeID(), extra, NULL);
	if (task != NULL) {
		task->pid = pid;
		task->entitlementsLoaded = false;
		task->entitlements = NULL;
	}

	return task;

}

SecTaskRef SecTaskCreateWithAuditToken(CFAllocatorRef allocator, audit_token_t token)
{
	pid_t pid;

	audit_token_to_au32(token,
			    /* auidp */ NULL,
			    /* euidp */ NULL,
			    /* egidp */ NULL,
			    /* ruidp */ NULL,
			    /* rgidp */ NULL,
			    /* pidp  */ &pid,
			    /* asidp */ NULL,
			    /* tidp  */ NULL);
	return SecTaskCreateWithPID(allocator, pid);
}

SecTaskRef SecTaskCreateFromSelf(CFAllocatorRef allocator)
{
	return SecTaskCreateWithPID(allocator, getpid());
}

static CFRange myMakeRange(CFIndex loc, CFIndex len) {
	CFRange r = {.location = loc, .length = len };
	return r;
}
struct csheader {
	uint32_t magic;
	uint32_t length;
};

static int SecTaskLoadEntitlements(SecTaskRef task, CFErrorRef *error)
{
	CFMutableDataRef data = NULL;
	struct csheader header;
	uint32_t bufferlen;
	int ret;

	ret = csops(task->pid, CS_OPS_ENTITLEMENTS_BLOB, &header, sizeof(header));
	if (ret != -1 || errno != ERANGE) {
		/* no entitlements */
		task->entitlementsLoaded = true;
		return 0;
	}

	bufferlen = ntohl(header.length);
	/* check for insane values */
	if (bufferlen > 1024 * 1024 || bufferlen < 8) {
		ret = EINVAL;
		goto out;
	}
	data = CFDataCreateMutable(NULL, bufferlen);
	if (data == NULL) {
		ret = ENOMEM;
		goto out;
	}
	CFDataSetLength(data, bufferlen);
	ret = csops(task->pid, CS_OPS_ENTITLEMENTS_BLOB, CFDataGetMutableBytePtr(data), bufferlen);
	if (ret) {
		ret = errno;
		goto out;
	}
	CFDataDeleteBytes(data, myMakeRange(0, 8));
	task->entitlements = CFPropertyListCreateWithData(NULL, data, 0, NULL, error);
	task->entitlementsLoaded = true;
 out:
	if (data)
		CFRelease(data);
	if (ret && error)
		*error = CFErrorCreate(NULL, kCFErrorDomainMach, ret, NULL);

	return ret;
}

CFTypeRef SecTaskCopyValueForEntitlement(SecTaskRef task, CFStringRef entitlement, CFErrorRef *error)
{
	/* Load entitlements if necessary */
	if (task->entitlementsLoaded == false) {
		SecTaskLoadEntitlements(task, error);
	}

	CFTypeRef value = NULL;
	if (task->entitlements != NULL) {
		value = CFDictionaryGetValue(task->entitlements, entitlement);

		/* Return something the caller must release */
		if (value != NULL) {
			CFRetain(value);
		}
	}

	return value;
}

CFDictionaryRef SecTaskCopyValuesForEntitlements(SecTaskRef task, CFArrayRef entitlements, CFErrorRef *error)
{
	/* Load entitlements if necessary */
	if (task->entitlementsLoaded == false) {
		SecTaskLoadEntitlements(task, error);
	}

	/* Iterate over the passed in entitlements, populating the dictionary
	 * If entitlements were loaded but none were present, return an empty
	 * dictionary */
	CFMutableDictionaryRef values = NULL;
	if (task->entitlementsLoaded == true) {

		CFIndex i, count = CFArrayGetCount(entitlements);
		values = CFDictionaryCreateMutable(CFGetAllocator(task), count, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		if (task->entitlements != NULL) {
			for (i = 0; i < count; i++) {
				CFStringRef entitlement = CFArrayGetValueAtIndex(entitlements, i);
				CFTypeRef value = CFDictionaryGetValue(task->entitlements, entitlement);
				if (value != NULL) {
					CFDictionarySetValue(values, entitlement, value);
				}
			}
		}
	}

	return values;
}
