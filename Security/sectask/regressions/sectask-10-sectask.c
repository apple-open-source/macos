/*
 * Copyright (c) 2007-2009,2013-2014 Apple Inc.  All Rights Reserved.
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



#include <stdio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecTask.h>
#include <Security/SecEntitlements.h>
#include <AssertMacros.h>
#include <TargetConditionals.h>
#include <sys/sysctl.h>

#include "utilities/SecCFRelease.h"

#include "sectask_regressions.h"

/* IPC stuff:

 This is a hack to get our own audittoken:
 We send a simple request with no argument to our mach port.
 The mach port callback copy the audittoken to a global.
 */

#include <mach/mach.h>
#include <mach/message.h>
#include "sectask_ipc.h"

static audit_token_t g_self_audittoken = {{0}};

kern_return_t sectask_server_request(mach_port_t receiver,
        audit_token_t auditToken);
kern_return_t sectask_server_request(mach_port_t receiver,
        audit_token_t auditToken)
{
    memcpy(&g_self_audittoken, &auditToken, sizeof(g_self_audittoken));

    CFRunLoopStop(CFRunLoopGetCurrent());

    return 0;
}

extern boolean_t sectask_ipc_server(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);

union max_msg_size_union {
    union __RequestUnion__sectask_client_sectask_ipc_subsystem reply;
};

static uint8_t reply_buffer[sizeof(union max_msg_size_union) + MAX_TRAILER_SIZE];

static void server_callback(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
    mach_msg_header_t *message = (mach_msg_header_t *)msg;
    mach_msg_header_t *reply = (mach_msg_header_t *)reply_buffer;

    sectask_ipc_server(message, reply);

}

static
void init_self_audittoken(void)
{
    /* create a mach port and an event source */
    CFMachPortRef server_port = CFMachPortCreate (NULL, server_callback, NULL, false);
    CFRunLoopSourceRef server_source = CFMachPortCreateRunLoopSource(NULL, server_port, 0/*order*/);

    /* add the source to the current run loop */
    CFRunLoopAddSource(CFRunLoopGetCurrent(), server_source, kCFRunLoopDefaultMode);
    CFRelease(server_source);

    /* Send the request */
    sectask_client_request(CFMachPortGetPort(server_port));

    /* Run the loop to process the message */
    CFRunLoopRun();

    /* done */
    CFRelease(server_port);

}

static
CFStringRef copyProcName(pid_t pid) {
    const char *task_name;
    int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, pid};
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    if (sysctl(mib, 4, &kp, &len, NULL, 0) == -1 || len == 0)
        task_name = strerror(errno);
    else
        task_name = kp.kp_proc.p_comm;
    return CFStringCreateWithCString(kCFAllocatorDefault, task_name, kCFStringEncodingASCII);
}

/* Actual test code */

int sectask_10_sectask_self(int argc, char *const *argv)
{
    SecTaskRef task=NULL;
    CFStringRef appId=NULL;
    CFStringRef signingIdentifier=NULL;
    plan_tests(6);

    ok(task=SecTaskCreateFromSelf(kCFAllocatorDefault), "SecTaskCreateFromSelf");
    require(task, out);

    /* TODO: remove the todo once xcode signs simulator binaries */
SKIP: {
#if TARGET_IPHONE_SIMULATOR
        todo("no entitlements in the simulator binaries yet, until <rdar://problem/12194625>");
#endif
        ok(appId=SecTaskCopyValueForEntitlement(task, kSecEntitlementApplicationIdentifier, NULL), "SecTaskCopyValueForEntitlement");
        skip("appId is NULL", 1, appId);
        ok(CFEqual(appId, CFSTR("com.apple.security.regressions")), "Application Identifier match");

        ok(signingIdentifier=SecTaskCopySigningIdentifier(task, NULL), "SecTaskCopySigningIdentifier");
        ok(CFEqual(signingIdentifier, CFBundleGetIdentifier(CFBundleGetMainBundle())), "CodeSigning Identifier match");
    }

    pid_t pid = getpid();
    CFStringRef name = copyProcName(pid);
    CFStringRef expected = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@[%d]"), name, pid);
    CFStringRef desc = CFCopyDescription(task);
    ok(CFEqual(desc, expected), "task-description expected: %@ got: %@", expected,  desc);
    CFReleaseSafe(name);
    CFReleaseSafe(desc);
    CFReleaseSafe(expected);

out:
    CFReleaseSafe(task);
    CFReleaseSafe(appId);
    CFReleaseSafe(signingIdentifier);

    return 0;
}

int sectask_11_sectask_audittoken(int argc, char *const *argv)
{
    SecTaskRef task=NULL;
    CFStringRef appId=NULL;
    CFStringRef signingIdentifier=NULL;

    plan_tests(6);

    init_self_audittoken();

    ok(task=SecTaskCreateWithAuditToken(kCFAllocatorDefault, g_self_audittoken), "SecTaskCreateFromAuditToken");
    require(task, out);

    /* TODO: remove the todo once xcode signs simulator binaries */
SKIP: {
#if TARGET_IPHONE_SIMULATOR
    todo("no entitlements in the simulator binaries yet, until <rdar://problem/12194625>");
#endif
    ok(appId=SecTaskCopyValueForEntitlement(task, kSecEntitlementApplicationIdentifier, NULL), "SecTaskCopyValueForEntitlement");
    skip("appId is NULL", 1, appId);
    ok(CFEqual(appId, CFSTR("com.apple.security.regressions")), "Application Identifier match");
    ok(signingIdentifier=SecTaskCopySigningIdentifier(task, NULL), "SecTaskCopySigningIdentifier");
    ok(CFEqual(signingIdentifier, CFBundleGetIdentifier(CFBundleGetMainBundle())), "CodeSigning Identifier match");
}

    pid_t pid = getpid();
    CFStringRef name = copyProcName(pid);
    CFStringRef expected = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@[%d]"), name, pid);
    CFStringRef desc = CFCopyDescription(task);
    ok(CFEqual(desc, expected), "task-description expected: %@ got: %@", expected,  desc);

    CFReleaseSafe(name);
    CFReleaseSafe(desc);
    CFReleaseSafe(expected);

out:
    CFReleaseSafe(task);
    CFReleaseSafe(appId);
    CFReleaseSafe(signingIdentifier);

    return 0;
}
