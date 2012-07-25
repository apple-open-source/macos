/*
 * Copyright (c) 2007-2009 Apple Inc.  All Rights Reserved.
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

#include <mach/mach.h>
#include <mach/message.h>
#include <servers/bootstrap.h>

#include <stdlib.h>
#include <sys/queue.h>

#include <Security/SecInternal.h>
#include <Security/SecBasePriv.h>
#include <Security/SecItemPriv.h> /* For SecItemDeleteAll */
#include <CoreFoundation/CoreFoundation.h>

#include <asl.h>
#include <bsm/libbsm.h>
#include <security_utilities/debugging.h>
#include <sys/sysctl.h>

#include "securityd_client.h"
#include "securityd_rep.h"
#include "securityd_server.h"

#include <securityd/spi.h>

#ifndef SECITEM_SHIM_OSX
#include <Security/SecTask.h>
#include <Security/SecEntitlements.h>
#endif

#if INDIGO || SECITEM_SHIM_OSX
#define CHECK_ENTITLEMENTS 0
#else
#define CHECK_ENTITLEMENTS 1
#endif

/* Time after which securityd exits. */
#if NDEBUG
#define TIMEOUT_IN_SECONDS  10.0
#else
#define TIMEOUT_IN_SECONDS  10000.0
#endif

#if SECITEM_SHIM_OSX
/* defines from <Security/SecTask.h> */
typedef struct __SecTask *SecTaskRef;
/* defines from <Security/SecEntitlements.h> */
#define kSecEntitlementGetTaskAllow CFSTR("get-task-allow")
#define kSecEntitlementApplicationIdentifier CFSTR("application-identifier")
#define kSecEntitlementKeychainAccessGroups CFSTR("keychain-access-groups")
#define kSecEntitlementModifyAnchorCertificates CFSTR("modify-anchor-certificates")
#define kSecEntitlementDebugApplications CFSTR("com.apple.springboard.debugapplications")
#define kSecEntitlementOpenSensitiveURL CFSTR("com.apple.springboard.opensensitiveurl")
#define kSecEntitlementWipeDevice CFSTR("com.apple.springboard.wipedevice")
#define kSecEntitlementRemoteNotificationConfigure CFSTR("com.apple.remotenotification.configure")
#define kSecEntitlementMigrateKeychain CFSTR("migrate-keychain")
#define kSecEntitlementRestoreKeychain CFSTR("restore-keychain")

#endif

static mach_port_t _server_port = MACH_PORT_NULL;
static unsigned int active_requests = 0;
static CFRunLoopTimerRef idle_timer = NULL;

static mach_port_t server_port(void *info)
{
    if (!_server_port) {
        kern_return_t ret;

        ret = bootstrap_check_in(bootstrap_port, SECURITYSERVER_BOOTSTRAP_NAME, &_server_port);
        if (ret == KERN_SUCCESS) {
            secdebug("server", "bootstrap_check_in() succeeded, return checked in port: 0x%x\n", _server_port);
            return _server_port;
        }

#if 0
        ret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &_server_port);
        if (ret != KERN_SUCCESS) {
            secdebug("server", "mach_port_allocate(): 0x%x: %s\n", ret, mach_error_string(ret));
            exit(2);
        }

        ret = mach_port_insert_right(mach_task_self(), _server_port, _server_port, MACH_MSG_TYPE_MAKE_SEND);
        if (ret != KERN_SUCCESS) {
            secdebug("server", "mach_port_insert_right(): 0x%x: %s\n", ret, mach_error_string(ret));
            exit(3);
        }

        ret = bootstrap_register(bootstrap_port, SECURITYSERVER_BOOTSTRAP_NAME, _server_port);
        if (ret != BOOTSTRAP_SUCCESS) {
            secdebug("server", "bootstrap_register(): 0x%x: %s\n", ret, mach_error_string(ret));
            exit(4);
        }
#else
        exit(1);
#endif

    }

    return _server_port;
}

#if CHECK_ENTITLEMENTS
static CFStringRef SecTaskCopyStringForEntitlement(SecTaskRef task,
    CFStringRef entitlement)
{
    CFStringRef value = (CFStringRef)SecTaskCopyValueForEntitlement(task,
        entitlement, NULL);
    if (value && CFGetTypeID(value) != CFStringGetTypeID()) {
        CFRelease(value);
        value = NULL;
    }

    return value;
}

static CFArrayRef SecTaskCopyArrayOfStringsForEntitlement(SecTaskRef task,
    CFStringRef entitlement)
{
    CFArrayRef value = (CFArrayRef)SecTaskCopyValueForEntitlement(task,
        entitlement, NULL);
    if (value) {
        if (CFGetTypeID(value) == CFArrayGetTypeID()) {
            CFIndex ix, count = CFArrayGetCount(value);
            for (ix = 0; ix < count; ++ix) {
                CFStringRef string = (CFStringRef)CFArrayGetValueAtIndex(value, ix);
                if (CFGetTypeID(string) != CFStringGetTypeID()) {
                    CFRelease(value);
                    value = NULL;
                    break;
                }
            }
        } else {
            CFRelease(value);
            value = NULL;
        }
    }

    return value;
}
#endif /* CHECK_ENTITLEMENTS */

static CFArrayRef SecTaskCopyAccessGroups(SecTaskRef task) {
#if CHECK_ENTITLEMENTS
    CFStringRef appID = SecTaskCopyStringForEntitlement(task,
        kSecEntitlementApplicationIdentifier);
    CFArrayRef groups = SecTaskCopyArrayOfStringsForEntitlement(task,
        kSecEntitlementKeychainAccessGroups);
    if (appID) {
        if (groups) {
            CFMutableArrayRef nGroups = CFArrayCreateMutableCopy(
                CFGetAllocator(groups), CFArrayGetCount(groups) + 1, groups);
            CFArrayAppendValue(nGroups, appID);
            CFRelease(groups);
            groups = nGroups;
        } else {
            groups = CFArrayCreate(CFGetAllocator(task),
                (const void **)&appID, 1, &kCFTypeArrayCallBacks);
        }
        CFRelease(appID);
    }
#else
    CFArrayRef groups = SecAccessGroupsGetCurrent();
    if (groups)
        CFRetain(groups);
#endif
    return groups;
}

static bool SecTaskGetBooleanValueForEntitlement(SecTaskRef task,
    CFStringRef entitlement) {
#if CHECK_ENTITLEMENTS
    CFStringRef canModify = (CFStringRef)SecTaskCopyValueForEntitlement(task,
        entitlement, NULL);
    if (!canModify)
        return false;
    CFTypeID canModifyType = CFGetTypeID(canModify);
    if (CFBooleanGetTypeID() == canModifyType) {
        return CFBooleanGetValue((CFBooleanRef)canModify);
    } else
        return false;
#else
    return true;
#endif /* !CHECK_ENTITLEMENTS */
}

#if 0
static CFDataRef CFArrayGetCheckedDataAtIndex() {
}
#endif

static bool isDictionary(CFTypeRef cfType) {
    return cfType && CFGetTypeID(cfType) == CFDictionaryGetTypeID();
}

static bool isData(CFTypeRef cfType) {
    return cfType && CFGetTypeID(cfType) == CFDataGetTypeID();
}

static bool isString(CFTypeRef cfType) {
    return cfType && CFGetTypeID(cfType) == CFStringGetTypeID();
}

static bool isArray(CFTypeRef cfType) {
    return cfType && CFGetTypeID(cfType) == CFArrayGetTypeID();
}

static bool isArrayOfLength(CFTypeRef cfType, CFIndex length) {
    return isArray(cfType) && CFArrayGetCount(cfType) == length;
}

static void idle_timer_proc(CFRunLoopTimerRef timer, void *info)
{
    if (active_requests == 0) {
        /* If the idle timer fired and we have no active requests we exit. */
        exit(0);
    }
}

static void cancel_timeout()
{
    if (idle_timer) {
        CFRunLoopTimerInvalidate(idle_timer);
        CFRelease(idle_timer);
        idle_timer = NULL;
    }
}

static void register_timeout(void)
{
    if (idle_timer == NULL) {
        idle_timer = CFRunLoopTimerCreate(kCFAllocatorDefault,
            CFAbsoluteTimeGetCurrent() + TIMEOUT_IN_SECONDS,
            TIMEOUT_IN_SECONDS, 0, 0, idle_timer_proc, NULL);
        if (idle_timer == NULL) {
            asl_log(NULL, NULL, ASL_LEVEL_CRIT,
                "FATAL: failed to create idle timer, exiting.");
            exit(1);
        }
        CFRunLoopAddTimer(CFRunLoopGetCurrent(), idle_timer,
            kCFRunLoopDefaultMode);
    }
}

static void request_begin(void)
{
    if (active_requests++ == 0) {
        /* First request, cancel timer. */
        cancel_timeout();
    }
}

static void request_end(void)
{
    if (--active_requests == 0) {
        /* Last request, set timer. */
        register_timeout();
    }
}

/* AUDIT[securityd](done):
   reply (checked by mig) is a caller provided mach_port.
   request_id (checked by mig) is caller provided value, that matches the
       mig entry for the server function.
 */
kern_return_t securityd_server_send_reply(mach_port_t reply,
    uint32_t request_id, OSStatus status, CFTypeRef args_out) {
    CFDataRef data_out = NULL;
    if (args_out) {
#ifndef NDEBUG
        CFDataRef query_debug = CFPropertyListCreateXMLData(kCFAllocatorDefault,
            args_out);
        secdebug("client", "securityd response: %.*s\n",
            CFDataGetLength(query_debug), CFDataGetBytePtr(query_debug));
        CFReleaseSafe(query_debug);
#endif
        CFErrorRef error = NULL;
        data_out = CFPropertyListCreateData(kCFAllocatorDefault, args_out,
                                           kCFPropertyListBinaryFormat_v1_0,
                                           0, &error);
		CFRelease(args_out);
        if (error) {
            secdebug("server", "failed to encode return data: %@", error);
             CFReleaseSafe(error);
        }
    }

    void *p = (data_out ? (void *)CFDataGetBytePtr(data_out) : NULL);
    CFIndex l = (data_out ? CFDataGetLength(data_out) : 0);
    /* 64 bits cast: securityd should never generate replies bigger than 2^32 bytes.
       Worst case is we are truncating the reply we send to the client. This would only
       cause the client side to not be able to decode the response. */
    assert((unsigned long)l<UINT_MAX); /* Debug check */
    kern_return_t err = securityd_client_reply(reply, request_id, status,
        p, (unsigned int)l);

    CFReleaseSafe(data_out);

    request_end();

    return err;
}

struct securityd_server_trust_evaluation_context {
    mach_port_t reply;
    uint32_t request_id;
};

static void
securityd_server_trust_evaluate_done(const void *userData,
    SecCertificatePathRef chain, CFArrayRef details, CFDictionaryRef info,
    SecTrustResultType result) {
    struct securityd_server_trust_evaluation_context *tec =
        (struct securityd_server_trust_evaluation_context *)userData;

    /* @@@ This code snippit is also in SecTrustServer.c.  I'd factor it,
       but a better fix would be to change the interfaces here to not use
       single in/out args and do all the argument munging in server.c
       and client.c. */
    CFDictionaryRef args_out;
    CFNumberRef resultNumber = NULL;
    CFArrayRef chain_certs = NULL;
    /* Proccess outgoing results. */
    resultNumber = CFNumberCreate(NULL, kCFNumberSInt32Type, &result);
    chain_certs = SecCertificatePathCopyArray(chain);
    const void *out_keys[] = { kSecTrustChainKey, kSecTrustDetailsKey,
        kSecTrustInfoKey, kSecTrustResultKey };
    const void *out_values[] = { chain_certs, details, info, resultNumber };
    args_out = (CFTypeRef)CFDictionaryCreate(kCFAllocatorDefault, out_keys,
        out_values, sizeof(out_keys) / sizeof(*out_keys),
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFReleaseSafe(chain_certs);
    CFReleaseSafe(resultNumber);

    /* Send back the response to the client. */
    securityd_server_send_reply(tec->reply, tec->request_id, noErr, args_out);

    free(tec);
}

kern_return_t securityd_server_request(mach_port_t receiver, mach_port_t reply,
        audit_token_t auditToken,
        uint32_t request_id, uint32_t msg_id, uint8_t *msg_data,
        mach_msg_type_number_t msg_length);

/* AUDIT[securityd](done):
   receiver (unused) is a mach_port owned by this process.
   reply (checked by mig) is a caller provided mach_port.
   auditToken (ok) is a kernel provided audit token.
   request_id (checked by mig) is caller provided value, that matches the
       mig entry for this function.
   msg_id (ok) is caller provided value.
   msg_data (ok) is a caller provided data of length:
   msg_length (ok).
 */
kern_return_t securityd_server_request(mach_port_t receiver, mach_port_t reply,
        audit_token_t auditToken,
        uint32_t request_id, uint32_t msg_id, uint8_t *msg_data,
        mach_msg_type_number_t msg_length)
{
    CFTypeRef args_in = NULL;
    CFTypeRef args_out = NULL;
    bool sendResponse = true;
    const char *op_name;

    request_begin();

    if (msg_length) {
        CFDataRef data_in = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
            msg_data, msg_length, kCFAllocatorNull);
        if (data_in) {
            args_in = CFPropertyListCreateFromXMLData(kCFAllocatorDefault,
                data_in, kCFPropertyListImmutable, NULL);
            CFRelease(data_in);
        }
    }

#if 0
    static int crash_counter = 0;
    if (crash_counter++) {
        secdebug("server", "crash test");
        exit(1);
    }
#endif

    SecTaskRef clientTask = 
#if CHECK_ENTITLEMENTS
    SecTaskCreateWithAuditToken(kCFAllocatorDefault, auditToken);
#else 
    NULL;
#endif
    CFArrayRef groups = SecTaskCopyAccessGroups(clientTask);
    SecAccessGroupsSetCurrent(groups);
    OSStatus status = errSecParam;
    switch(msg_id) {
        case sec_item_add_id:
            op_name = "SecItemAdd";
            if (isDictionary(args_in))
                status = _SecItemAdd(args_in, &args_out, groups);
            break;
        case sec_item_copy_matching_id:
            op_name = "SecItemCopyMatching";
            if (isDictionary(args_in))
                status = _SecItemCopyMatching(args_in, &args_out, groups);
            break;
        case sec_item_delete_id:
            op_name = "SecItemDelete";
            if (isDictionary(args_in))
                status = _SecItemDelete(args_in, groups);
            break;
        case sec_item_update_id:
            op_name = "SecItemUpdate";
            if (isArrayOfLength(args_in, 2)) {
                CFDictionaryRef
                    in0 = (CFDictionaryRef)CFArrayGetValueAtIndex(args_in, 0),
                    in1 = (CFDictionaryRef)CFArrayGetValueAtIndex(args_in, 1);
                if (isDictionary(in0) && isDictionary(in1))
                    status = _SecItemUpdate(in0, in1, groups);
            }
            break;
        case sec_trust_store_contains_id:
        {
            op_name = "SecTrustStoreContains";
            if (!isArray(args_in))
                break;
            CFIndex argc_in = CFArrayGetCount(args_in);
            if (argc_in != 2)
                break;
            CFStringRef domainName = CFArrayGetValueAtIndex(args_in, 0);
            CFDataRef digest = CFArrayGetValueAtIndex(args_in, 1);
            if (!isString(domainName) || !isData(digest))
                break;

            SecTrustStoreRef ts = SecTrustStoreForDomainName(domainName);
            status = !SecTrustStoreContainsCertificateWithDigest(ts, digest);
            break;
        }
        case sec_trust_store_set_trust_settings_id:
        {
            op_name = "SecTrustStoreSetTrustSettings";
            /* Open the trust store unconditially so we can abuse this method
               even in clients that just want to read from the truststore,
               and this call will force it to be created. */
            SecTrustStoreRef ts = SecTrustStoreForDomain(kSecTrustStoreDomainUser);
            if (!isArray(args_in))
                break;
            CFIndex argc_in = CFArrayGetCount(args_in);
            if (argc_in != 1 && argc_in != 2)
                break;
            if (!SecTaskGetBooleanValueForEntitlement(clientTask,
                kSecEntitlementModifyAnchorCertificates)) {
                status = errSecMissingEntitlement;
                break;
            }
            CFDataRef certificateData = (CFDataRef)CFArrayGetValueAtIndex(args_in, 0);
            if (!isData(certificateData))
                break;
            SecCertificateRef certificate = SecCertificateCreateWithData(NULL, certificateData);
            if (certificate) {
                CFTypeRef trustSettingsDictOrArray;
                if (argc_in < 2) {
                    trustSettingsDictOrArray = NULL;
                } else {
                    trustSettingsDictOrArray = CFArrayGetValueAtIndex(args_in, 1);
                    if (trustSettingsDictOrArray) {
                        CFTypeID tst = CFGetTypeID(trustSettingsDictOrArray);
                        if (tst != CFArrayGetTypeID() && tst != CFDictionaryGetTypeID()) {
                            CFRelease(certificate);
                            break;
                        }
                    }
                }
                status = _SecTrustStoreSetTrustSettings(ts, certificate, trustSettingsDictOrArray);
                CFRelease(certificate);
            }
            break;
        }
        case sec_trust_store_remove_certificate_id:
            op_name = "SecTrustStoreRemoveCertificate";
            if (SecTaskGetBooleanValueForEntitlement(clientTask,
                    kSecEntitlementModifyAnchorCertificates)) {
                SecTrustStoreRef ts = SecTrustStoreForDomain(kSecTrustStoreDomainUser);
                if (isData(args_in)) {
                    status = SecTrustStoreRemoveCertificateWithDigest(ts, args_in);
                }
            } else {
                status = errSecMissingEntitlement;
            }
            break;
        case sec_delete_all_id:
            op_name = "SecDeleteAll";
            if (SecTaskGetBooleanValueForEntitlement(clientTask,
                kSecEntitlementWipeDevice)) {
                status = SecItemDeleteAll();
            } else {
                status = errSecMissingEntitlement;
            }
            break;
        case sec_trust_evaluate_id:
            op_name = "SecTrustEvaluate";
            if (isDictionary(args_in)) {
                struct securityd_server_trust_evaluation_context *tec = malloc(sizeof(*tec));
                tec->reply = reply;
                tec->request_id = request_id;
                status = SecTrustServerEvaluateAsync(args_in,
                    securityd_server_trust_evaluate_done, tec);
                if (status == noErr || status == errSecWaitForCallback) {
                    sendResponse = false;
                } else {
                    free(tec);
                }
            }
            break;
        case sec_restore_keychain_id:
            op_name = "SecRestoreKeychain";
            if (SecTaskGetBooleanValueForEntitlement(clientTask,
                kSecEntitlementRestoreKeychain)) {
                status = _SecServerRestoreKeychain();
            } else {
                status = errSecMissingEntitlement;
            }
            break;
        case sec_migrate_keychain_id:
            op_name = "SecMigrateKeychain";
            if (isArray(args_in)) {
                if (SecTaskGetBooleanValueForEntitlement(clientTask,
                    kSecEntitlementMigrateKeychain)) {
                    status = _SecServerMigrateKeychain(args_in, &args_out);
                } else {
                    status = errSecMissingEntitlement;
                }
            }
            break;
        case sec_keychain_backup_id:
            op_name = "SecKeychainBackup";
            if (!args_in || isArray(args_in)) {
                if (SecTaskGetBooleanValueForEntitlement(clientTask,
                    kSecEntitlementRestoreKeychain)) {
                    status = _SecServerKeychainBackup(args_in, &args_out);
                } else {
                    status = errSecMissingEntitlement;
                }
            }
            break;
        case sec_keychain_restore_id:
            op_name = "SecKeychainRestore";
            if (isArray(args_in)) {
                if (SecTaskGetBooleanValueForEntitlement(clientTask,
                    kSecEntitlementRestoreKeychain)) {
                    status = _SecServerKeychainRestore(args_in, &args_out);
                } else {
                    status = errSecMissingEntitlement;
                }
            }
            break;
        default:
            op_name = "invalid_operation";
            status = errSecParam;
            break;
    }

    const char *proc_name;
#ifdef NDEBUG
    if (status == errSecMissingEntitlement) {
#endif
        pid_t pid;
        audit_token_to_au32(auditToken, NULL, NULL, NULL, NULL, NULL, &pid, NULL, NULL);
        int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, pid};
        struct kinfo_proc kp;
        size_t len = sizeof(kp);
        if (sysctl(mib, 4, &kp, &len, NULL, 0) == -1 || len == 0)
            proc_name = strerror(errno);
        else
            proc_name = kp.kp_proc.p_comm;

#ifndef NDEBUG
    if (status == errSecMissingEntitlement) {
#endif
        asl_log(NULL, NULL, ASL_LEVEL_ERR,
            "%s[%u] %s: missing entitlement", proc_name, pid, op_name);
        /* Remap errSecMissingEntitlement -> errSecInteractionNotAllowed. */
        status = errSecInteractionNotAllowed;
    }

    secdebug("ipc", "%s[%u] %s: returning: %d", proc_name, pid, op_name,
        status);

    CFReleaseSafe(groups);
    CFReleaseSafe(clientTask);
    SecAccessGroupsSetCurrent(NULL);

    kern_return_t err = 0;
    if (sendResponse)
        err = securityd_server_send_reply(reply, request_id, status, args_out);

    CFReleaseSafe(args_in);

    return err;
}

extern boolean_t securityd_request_server(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);

union max_msg_size_union {
    union __RequestUnion__securityd_client_securityd_reply_subsystem reply;
};

static uint8_t reply_buffer[sizeof(union max_msg_size_union) + MAX_TRAILER_SIZE];

static void *handle_message(void *msg, CFIndex size,
        CFAllocatorRef allocator, void *info)
{
    mach_msg_header_t *message = (mach_msg_header_t *)msg;
    mach_msg_header_t *reply = (mach_msg_header_t *)reply_buffer;

    securityd_request_server(message, reply);

    return NULL;
}


static void register_server(void)
{
    CFRunLoopSourceContext1 context = { 1, NULL, NULL, NULL, NULL, NULL, NULL,
        server_port, handle_message };
    CFRunLoopSourceRef source = CFRunLoopSourceCreate(NULL, 0,
        (CFRunLoopSourceContext *)&context);

    if (!source) {
        secdebug("server", "failed to create source for port, exiting.");
        exit(1);
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopDefaultMode);
    CFRelease(source);
}


int main(int argc, char *argv[])
{
    securityd_init();
    register_server();
    register_timeout();
    CFRunLoopRun();
    return 0;
}

/* vi:set ts=4 sw=4 et: */
