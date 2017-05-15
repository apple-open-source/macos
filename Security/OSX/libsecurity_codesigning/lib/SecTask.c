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
#include <syslog.h>
#include <sys/sysctl.h>
#include <utilities/SecCFWrappers.h>


#include "SecCode.h"
#include "SecCodePriv.h"
#include "SecRequirement.h"

#include "SecTask.h"
#include "SecTaskPriv.h"


struct __SecTask {
	CFRuntimeBase base;

	pid_t pid_self;

	audit_token_t token;

	/* Track whether we've loaded entitlements independently since after the
	 * load, entitlements may legitimately be NULL */
	Boolean entitlementsLoaded;
	CFDictionaryRef entitlements;

    /* for debugging only, shown by debugDescription */
    int lastFailure;
};

enum {
	kSecCodeMagicEntitlement = 0xfade7171,		/* entitlement blob */
};


CFTypeID _kSecTaskTypeID = _kCFRuntimeNotATypeID;

static void SecTaskFinalize(CFTypeRef cfTask)
{
	SecTaskRef task = (SecTaskRef) cfTask;
    CFReleaseNull(task->entitlements);
}


// Define PRIdPID (proper printf format string for pid_t)
#define PRIdPID PRId32

static CFStringRef SecTaskCopyDebugDescription(CFTypeRef cfTask)
{
    SecTaskRef task = (SecTaskRef) cfTask;
    pid_t pid;

    if (task->pid_self==-1) {
        audit_token_to_au32(task->token, NULL, NULL, NULL, NULL, NULL, &pid, NULL, NULL);
    } else {
        pid = task->pid_self;
    }

    const char *task_name;
    int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, pid};
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    if (sysctl(mib, 4, &kp, &len, NULL, 0) == -1 || len == 0)
        task_name = strerror(errno);
    else
        task_name = kp.kp_proc.p_comm;

    return CFStringCreateWithFormat(CFGetAllocator(task), NULL, CFSTR("%s[%" PRIdPID "]"), task_name, pid);
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

static SecTaskRef init_task_ref(CFAllocatorRef allocator)
{
    CFIndex extra = sizeof(struct __SecTask) - sizeof(CFRuntimeBase);
    return (SecTaskRef) _CFRuntimeCreateInstance(allocator, SecTaskGetTypeID(), extra, NULL);
}

SecTaskRef SecTaskCreateWithAuditToken(CFAllocatorRef allocator, audit_token_t token)
{
    SecTaskRef task = init_task_ref(allocator);
    if (task != NULL) {

        memcpy(&task->token, &token, sizeof(token));
        task->entitlementsLoaded = false;
        task->entitlements = NULL;
        task->pid_self = -1;
    }

    return task;
}

SecTaskRef SecTaskCreateFromSelf(CFAllocatorRef allocator)
{
    SecTaskRef task = init_task_ref(allocator);
    if (task != NULL) {

        memset(&task->token, 0, sizeof(task->token));
        task->entitlementsLoaded = false;
        task->entitlements = NULL;
        task->pid_self = getpid();
    }

    return task;
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
    CFDataRef auditData = NULL;
    CFNumberRef pidRef = NULL;

    CFMutableDictionaryRef codeDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if(task->pid_self==-1) {
        auditData = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)&task->token, sizeof(audit_token_t));
        CFDictionarySetValue(codeDict, kSecGuestAttributeAudit, auditData);
    } else {
        pidRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &task->pid_self);
        CFDictionarySetValue(codeDict, kSecGuestAttributePid, pidRef);
    }

    status = SecCodeCopyGuestWithAttributes(NULL, codeDict, kSecCSDefaultFlags, &code);
    CFReleaseNull(codeDict);
    CFReleaseNull(auditData);
    CFReleaseNull(pidRef);

    if (!status) {
        status = SecRequirementCreateWithString(requirement,
                                                kSecCSDefaultFlags, &req);
    }
    if (!status) {
        status = SecCodeCheckValidity(code, kSecCSDefaultFlags, req);
    }

    CFReleaseNull(req);
    CFReleaseNull(code);

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
    int rc;
    if (task->pid_self==-1) {
        pid_t pid;
        audit_token_to_au32(task->token, NULL, NULL, NULL, NULL, NULL, &pid, NULL, NULL);
        rc = csops_audittoken(pid, ops, blob, size, &task->token);
    }
    else
        rc = csops(task->pid_self, ops, blob, size);
    task->lastFailure = (rc == -1) ? errno : 0;
    return rc;
}

static int SecTaskLoadEntitlements(SecTaskRef task, CFErrorRef *error)
{
	CFMutableDataRef data = NULL;
	struct csheader header;
	uint32_t bufferlen;
	int ret;

	ret = csops_task(task, CS_OPS_ENTITLEMENTS_BLOB, &header, sizeof(header));
	if (ret == 0) {
		// we only gave a header's worth of buffer. If this succeeded, we have no entitlements
		task->entitlementsLoaded = true;
		return 0;
	}
	if (errno != ERANGE) {
		// ERANGE means "your buffer is too small, it now tells you how much you need
		// EINVAL is what the kernel says for unsigned code AND broken code, so we'll have to let that pass
		if (errno == EINVAL) {
			task->entitlementsLoaded = true;
			return 0;
		}
		ret = errno;
		goto out;
	}
	// kernel told us the needed buffer size in header.length; proceed

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
		*error = CFErrorCreate(NULL, kCFErrorDomainPOSIX, ret, NULL);

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

Boolean SecTaskEntitlementsValidated(SecTaskRef task) {
    // TODO: Cache the result
    uint32_t csflags = 0;
    const uint32_t mask = CS_VALID | CS_KILL | CS_ENTITLEMENTS_VALIDATED;
    int rc = csops_task(task, CS_OPS_STATUS, &csflags, sizeof(csflags));
    return rc != -1 && ((csflags & mask) == mask);
}

CFStringRef
SecTaskCopySigningIdentifier(SecTaskRef task, CFErrorRef *error)
{
    CFStringRef signingId = NULL;
    char *data = NULL;
    struct csheader header;
    uint32_t bufferlen;
    int ret;

    ret = csops_task(task, CS_OPS_IDENTITY, &header, sizeof(header));
    if (ret != -1 || errno != ERANGE)
        return NULL;

    bufferlen = ntohl(header.length);
    /* check for insane values */
    if (bufferlen > 1024 * 1024 || bufferlen < 8) {
        ret = EINVAL;
        goto out;
    }
    data = malloc(bufferlen + 1);
    if (data == NULL) {
        ret = ENOMEM;
        goto out;
    }
    ret = csops_task(task, CS_OPS_IDENTITY, data, bufferlen);
    if (ret) {
        ret = errno;
        goto out;
    }
    data[bufferlen] = '\0';

    signingId = CFStringCreateWithCString(NULL, data + 8, kCFStringEncodingUTF8);

out:
    if (data)
        free(data);
    if (ret && error)
        *error = CFErrorCreate(NULL, kCFErrorDomainPOSIX, ret, NULL);

    return signingId;
}

uint32_t
SecTaskGetCodeSignStatus(SecTaskRef task)
{
    uint32_t flags = 0;
    if (csops_task(task, CS_OPS_STATUS, &flags, sizeof(flags)) != 0)
        return 0;
    return flags;
}

