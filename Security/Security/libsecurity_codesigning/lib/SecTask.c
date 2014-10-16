/*
 * Copyright (c) 2009-2014 Apple Inc. All Rights Reserved.
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
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFUnserialize.h>
#include <System/sys/codesign.h>
#include <bsm/libbsm.h>
#include <inttypes.h>
#include <pthread.h>
#include <sys/sysctl.h>

#include "SecCode.h"
#include "SecCodePriv.h"
#include "SecRequirement.h"

#include "SecTask.h"
#include "SecTaskPriv.h"


struct __SecTask {
	CFRuntimeBase base;

	pid_t pid;

	audit_token_t *token;
	audit_token_t token_storage;

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


// Define PRIdPID (proper printf format string for pid_t)
#define PRIdPID PRId32

static CFStringRef SecTaskCopyDebugDescription(CFTypeRef cfTask)
{
    SecTaskRef task = (SecTaskRef) cfTask;
    const char *task_name;
    int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, task->pid};
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    if (sysctl(mib, 4, &kp, &len, NULL, 0) == -1 || len == 0)
        task_name = strerror(errno);
    else
        task_name = kp.kp_proc.p_comm;

    return CFStringCreateWithFormat(CFGetAllocator(task), NULL, CFSTR("%s[%" PRIdPID "]"), task_name, task->pid);
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
	SecTaskRef task;

	task = SecTaskCreateWithPID(allocator, audit_token_to_pid(token));
	if (task != NULL) {
#if 0
		task->token_storage = token;
		task->token = &task->token_storage;
#endif
	}

	return task;
}

SecTaskRef SecTaskCreateFromSelf(CFAllocatorRef allocator)
{
	return SecTaskCreateWithPID(allocator, getpid());
}

/*
 * Determine if the given task meets a specified requirement.
 */
OSStatus
SecTaskValidateForRequirement(SecTaskRef task, CFStringRef requirement)
{
    OSStatus status;
    SecCodeRef code = NULL;
    SecRequirementRef req = NULL;
    pid_t pid = task->pid;
    if (pid <= 0) {
        return errSecParam;
    }
    status = SecCodeCreateWithPID(pid, kSecCSDefaultFlags, &code);
    //syslog(LOG_NOTICE, "SecTaskValidateForRequirement: SecCodeCreateWithPID=%d", status);
    if (!status) {
        status = SecRequirementCreateWithString(requirement,
                                                kSecCSDefaultFlags, &req);
        //syslog(LOG_NOTICE, "SecTaskValidateForRequirement: SecRequirementCreateWithString=%d", status);
    }
    if (!status) {
        status = SecCodeCheckValidity(code, kSecCSDefaultFlags, req);
        //syslog(LOG_NOTICE, "SecTaskValidateForRequirement: SecCodeCheckValidity=%d", status);
    }
    if (req)
        CFRelease(req);
    if (code)
        CFRelease(code);

    return status;
}

static CFRange myMakeRange(CFIndex loc, CFIndex len) {
	CFRange r = {.location = loc, .length = len };
	return r;
}
struct csheader {
	uint32_t magic;
	uint32_t length;
};

static int
csops_task(SecTaskRef task, int ops, void *blob, size_t size)
{
	if (task->token)
		return csops_audittoken(task->pid, ops, blob, size, task->token);
	else
		return csops(task->pid, ops, blob, size);
}

static int SecTaskLoadEntitlements(SecTaskRef task, CFErrorRef *error)
{
	CFMutableDataRef data = NULL;
	struct csheader header;
	uint32_t bufferlen;
	int ret;

	ret = csops_task(task, CS_OPS_ENTITLEMENTS_BLOB, &header, sizeof(header));
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
	ret = csops_task(task, CS_OPS_ENTITLEMENTS_BLOB, CFDataGetMutableBytePtr(data), bufferlen);
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
