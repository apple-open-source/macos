/*
 * Copyright (c) 2008,2010-2013 Apple Inc. All Rights Reserved.
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

#include "SecTask.h"

#include <utilities/debugging.h>

#include <AssertMacros.h>
#include <CoreFoundation/CFRuntime.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFUnserialize.h>
#include <System/sys/codesign.h>
#include <bsm/libbsm.h>
#include <inttypes.h>
#include <utilities/SecCFWrappers.h>

#define USE_LIBPROC  0
#if USE_LIBPROC
#include <libproc.h>
#else
#include <sys/sysctl.h>
#endif

struct __SecTask {
    CFRuntimeBase base;
    
    pid_t pid_self;
    audit_token_t token;
    
    /* Track whether we've loaded entitlements independently since after the
     * load, entitlements may legitimately be NULL */
    Boolean entitlementsLoaded;
    CFDictionaryRef entitlements;
};

static bool check_task(SecTaskRef task) {
    return SecTaskGetTypeID() == CFGetTypeID(task);
}

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
    pid_t pid;
    if (task->pid_self==-1) {
        audit_token_to_au32(task->token, NULL, NULL, NULL, NULL, NULL, &pid, NULL, NULL);
    } else {
        pid = task->pid_self;
    }

#if USE_LIBPROC
#define MAX_PROCNAME 32
    char task_name[MAX_PROCNAME + 1] = {};
    proc_name(pid, task_name, MAX_PROCNAME);
#else
    const char *task_name;
    int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, pid};
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    if (sysctl(mib, 4, &kp, &len, NULL, 0) == -1 || len == 0)
        task_name = strerror(errno);
    else
        task_name = kp.kp_proc.p_comm;
#endif

    return CFStringCreateWithFormat(CFGetAllocator(task), NULL, CFSTR("%s[%" PRIdPID "]"), task_name, pid);
}

CFGiblisWithFunctions(SecTask, NULL, NULL, SecTaskFinalize, NULL, NULL, NULL, SecTaskCopyDebugDescription, NULL, NULL, NULL)

static SecTaskRef init_task_ref(CFAllocatorRef allocator)
{
    CFIndex extra = sizeof(struct __SecTask) - sizeof(CFRuntimeBase);
    return (SecTaskRef) _CFRuntimeCreateInstance(allocator, SecTaskGetTypeID(), extra, NULL);
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

struct csheader {
        uint32_t magic;
        uint32_t length;
};

static int
csops_task(SecTaskRef task, int ops, void *blob, size_t size)
{
    if (task->pid_self==-1) {
        pid_t pid;
        audit_token_to_au32(task->token, NULL, NULL, NULL, NULL, NULL, &pid, NULL, NULL);
        return csops_audittoken(pid, ops, blob, size, &task->token);
    }
    else
        return csops(task->pid_self, ops, blob, size);
}

/* This may need to be exported at some point */
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


static bool SecTaskLoadEntitlements(SecTaskRef task, CFErrorRef *error)
{
    CFMutableDictionaryRef entitlements = NULL;
    struct csheader header;
    uint8_t *buffer = NULL;
    uint32_t bufferlen;
    int ret;

    
    ret = csops_task(task, CS_OPS_ENTITLEMENTS_BLOB, &header, sizeof(header));
    /* Any other combination means no entitlements */
    if (ret == -1 && errno == ERANGE) {
        bufferlen = ntohl(header.length);
        /* check for insane values */
        if (bufferlen > 1024 * 1024 || bufferlen < 8) {
            ret = EINVAL;
            goto out;
        }
        buffer = malloc(bufferlen);
        if (buffer == NULL) {
            ret = ENOMEM;
            goto out;
        }
        ret = csops_task(task, CS_OPS_ENTITLEMENTS_BLOB, buffer, bufferlen);
        if (ret) {
            ret = errno;
            goto out;
        }

        CFDataRef data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, buffer+8, bufferlen-8, kCFAllocatorNull);
        entitlements = (CFMutableDictionaryRef) CFPropertyListCreateWithData(kCFAllocatorDefault, data, kCFPropertyListMutableContainers, NULL, error);
        CFRelease(data);

        if((entitlements==NULL) || (CFGetTypeID(entitlements)!=CFDictionaryGetTypeID())){
            ret = EINVAL;
            goto out;
        }
    }

    task->entitlements = entitlements ? CFRetain(entitlements) : NULL;
    task->entitlementsLoaded = true;

out:
    if(entitlements)
        CFRelease(entitlements);
    if(buffer)
        free(buffer);
    if (ret && error && *error==NULL)
        *error = CFErrorCreate(NULL, kCFErrorDomainPOSIX, ret, NULL);
    return ret == 0;
}


CFTypeRef SecTaskCopyValueForEntitlement(SecTaskRef task, CFStringRef entitlement, CFErrorRef *error)
{
    CFTypeRef value = NULL;
    require(check_task(task), out);

    /* Load entitlements if necessary */
    if (task->entitlementsLoaded == false) {
        require_quiet(SecTaskLoadEntitlements(task, error), out);
    }

    if (task->entitlements != NULL) {
        value = CFDictionaryGetValue(task->entitlements, entitlement);
        
        /* Return something the caller must release */
        if (value != NULL) {
            CFRetain(value);
        }
    }
out:
    return value;
}

CFDictionaryRef SecTaskCopyValuesForEntitlements(SecTaskRef task, CFArrayRef entitlements, CFErrorRef *error)
{
    CFMutableDictionaryRef values = NULL;
    require(check_task(task), out);

    /* Load entitlements if necessary */
    if (task->entitlementsLoaded == false) {
        SecTaskLoadEntitlements(task, error);
    }
    
    /* Iterate over the passed in entitlements, populating the dictionary
     * If entitlements were loaded but none were present, return an empty
     * dictionary */
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
out:
    return values;
}
