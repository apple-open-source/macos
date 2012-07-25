/*
 * Copyright (c) 2007-2009 Apple Inc. All Rights Reserved.
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
#include <mach/mach_error.h>
#include <servers/bootstrap.h>

#include <pthread.h>
#include <stdbool.h>
#include <sys/queue.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecItem.h>
#include <Security/SecBasePriv.h>
#include <Security/SecInternal.h>

#include <security_utilities/debugging.h>
#include "securityd_client.h"
#include "securityd_req.h"

#if NO_SERVER
#define CHECK_ENTITLEMENTS 0
#else
#define CHECK_ENTITLEMENTS 1
#endif

struct securityd *gSecurityd;

/* XXX Not thread safe right now. */
static CFArrayRef gAccessGroups = NULL;

CFArrayRef SecAccessGroupsGetCurrent(void) {
#if !CHECK_ENTITLEMENTS
    if (!gAccessGroups) {
        /* Initialize gAccessGroups for tests. */
        const void *agrps[] = {
            CFSTR("test"),
#if 1
            CFSTR("apple"),
            CFSTR("lockdown-identities"),
#else
            CFSTR("*"),
#endif
        };
        gAccessGroups = CFArrayCreate(NULL, agrps,
            sizeof(agrps) / sizeof(*agrps), &kCFTypeArrayCallBacks);
    }
#endif
    return gAccessGroups;
}

void SecAccessGroupsSetCurrent(CFArrayRef accessGroups) {
    if (accessGroups)
        CFRetain(accessGroups);
    CFReleaseSafe(gAccessGroups);
    gAccessGroups = accessGroups;
}

static mach_port_t securityd_port = MACH_PORT_NULL;
static pthread_once_t init_once = PTHREAD_ONCE_INIT;
static pthread_key_t port_key;

static CFStringRef kSecRunLoopModeRef = CFSTR("com.apple.securityd.runloop");

struct request {
    uint32_t seq_no;
    uint32_t msgid;
    SLIST_ENTRY(request) next;
    int32_t rcode;
    void *data;
    size_t length;
    volatile bool done;
};

typedef struct per_thread {
    SLIST_HEAD(request_head, request) head;;
    CFMachPortRef port;
    uint32_t seq_no;
    mach_msg_id_t notification; /* XXX for stacked requests, notifications
                                    need to be matched up with seq_nos such
                                    that only failed requests error out/retry */
} *per_thread_t;


static void destroy_per_thread(void *ex)
{
    per_thread_t pt = (per_thread_t)ex;
    CFMachPortInvalidate(pt->port);
    CFRelease(pt->port);
    free(pt);
}

static void securityd_port_reset(void) 
{
    pthread_once_t temp = PTHREAD_ONCE_INIT;
    init_once = temp;
    securityd_port = MACH_PORT_NULL;
}

static void securityd_port_lookup(void) 
{
    kern_return_t ret;
    mach_port_t bootstrap_lookup_port = MACH_PORT_NULL;

    /* bootstrap_port is not initialized on embedded */
    ret = task_get_bootstrap_port(mach_task_self(), &bootstrap_lookup_port);
    if (ret != KERN_SUCCESS) {
        secdebug("client", "task_get_bootstrap_port(): 0x%x: %s\n", ret, mach_error_string(ret));
    }

    ret = bootstrap_look_up(bootstrap_lookup_port, 
            SECURITYSERVER_BOOTSTRAP_NAME, 
            &securityd_port);
    if (ret != KERN_SUCCESS) {
        secdebug("client", "bootstrap_look_up(): 0x%x: %s\n", ret, mach_error_string(ret));
    }

    pthread_key_create(&port_key, destroy_per_thread);

    int err = pthread_atfork(NULL, NULL, securityd_port_reset);
    if (err) {
        secdebug("client", "pthread_atfork(): %d: %s\n", errno, strerror(errno));
    }
}

union max_msg_size_union {
    union __RequestUnion__securityd_client_securityd_request_subsystem request;
};

static uint8_t reply_buffer[sizeof(union max_msg_size_union) + MAX_TRAILER_SIZE];

static boolean_t maybe_notification(mach_msg_header_t *request)
{
    mach_no_senders_notification_t * notify = (mach_no_senders_notification_t *)request;
    if ((notify->not_header.msgh_id > MACH_NOTIFY_LAST) ||
            (notify->not_header.msgh_id < MACH_NOTIFY_FIRST))
        return false;        /* if this is not a notification message */

    per_thread_t pt = (per_thread_t)pthread_getspecific(port_key);
    assert(pt);
    mach_msg_id_t notification_id = notify->not_header.msgh_id;

    switch(notification_id) {
        case MACH_NOTIFY_SEND_ONCE: 
            /* our send-once right for a reply died in the hands of another */
            pt->notification = notification_id;
            CFRunLoopStop(CFRunLoopGetCurrent());
            break;
        default:
            secdebug("client", "unexpected notification %d", pt->notification);
            break;
    }
    return true;
}

extern boolean_t securityd_reply_server
(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);

static void cfmachport_callback(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
    if (!maybe_notification((mach_msg_header_t *)msg))
        securityd_reply_server(msg, (mach_msg_header_t *)reply_buffer);
}

static int32_t send_receive(uint32_t msg_id, const void *data_in, size_t length_in,
        void **data_out, size_t *length_out)
{
    pthread_once(&init_once, securityd_port_lookup);

    CFRunLoopRef rl = CFRunLoopGetCurrent();

    per_thread_t pt = (per_thread_t)pthread_getspecific(port_key);
    if (!pt) {
        pt = calloc(1, sizeof(*pt));
        SLIST_INIT(&pt->head);
        pt->port = CFMachPortCreate (NULL, cfmachport_callback, NULL, false);
        CFRunLoopSourceRef source = CFMachPortCreateRunLoopSource(NULL, pt->port, 0/*order*/);
        CFRunLoopAddSource(rl, source, kSecRunLoopModeRef);
        CFRelease(source);
        pthread_setspecific(port_key, pt);
    }

    struct request req = { pt->seq_no++, msg_id, };
    SLIST_INSERT_HEAD(&pt->head, &req, next);

    int retries = 1;
    kern_return_t ret;
    do {
        /* 64 bits cast: worst case here is the client send a truncated query, which the server will reject */
        /* Debug check: The size of the request is of type natural_t.
            The following is correct as long as natural_t is unsigned int */
        assert(length_in<=UINT_MAX);
        ret = securityd_client_request(securityd_port, 
                CFMachPortGetPort(pt->port), req.seq_no, msg_id, 
                (void *)data_in, (unsigned int) length_in);
        secdebug("client", "securityd_client_request %d sent, retry %d, result %d\n", 
            req.seq_no, retries, ret);

        if (!ret) {
            pt->notification = 0;
            while (!pt->notification && !req.done) {
                CFRunLoopRunInMode(kSecRunLoopModeRef, 10000, true);
                secdebug("client", "return from runloop, notification %d, request %d %sdone\n",
                    pt->notification, req.seq_no, !req.done ? "not " : "");
            }
        } else {
            secdebug("client", "failed to send request to securityd (err=%d).", ret);
        }

    } while ((ret || pt->notification) && retries--);

    SLIST_REMOVE_HEAD(&pt->head, next);

    struct request *next_head = SLIST_FIRST(&pt->head);
    if (next_head) {
        /* stop runloop if "new head" is also already done */
        if (pt->notification || next_head->done)
            CFRunLoopStop(rl);
    }

    if (req.done) {
        if (data_out)
            *data_out = req.data;
        if (length_out)
            *length_out = req.length;
        return req.rcode;
    }

    return errSecNotAvailable;
}

kern_return_t securityd_server_reply(mach_port_t receiver,
        uint32_t seq_no, int32_t rcode,
        uint8_t *msg_data, mach_msg_type_number_t msg_length);

kern_return_t securityd_server_reply(mach_port_t receiver,
        uint32_t seq_no, int32_t rcode,
        uint8_t *msg_data, mach_msg_type_number_t msg_length)
{
    secdebug("client", "reply from port %d request_id %d data(%d,%p)\n",
       receiver, seq_no, msg_length, msg_data);
    per_thread_t pt = (per_thread_t)pthread_getspecific(port_key);
    assert(pt);
    struct request *req;
    SLIST_FOREACH(req, &pt->head, next) {
        if (req->seq_no == seq_no) {
            req->rcode = rcode;
            if (msg_length && msg_data) {
                req->data = malloc(msg_length);
                if (req->data) {
                    req->length = msg_length;
                    memcpy(req->data, msg_data, msg_length);
                } else
                    req->length = 0;
            }
            req->done = true;
            /* if multiple requests were queued during nested invocations 
               we wait until the last one inserted which is the deepest
               nested one is done */
            if (req == SLIST_FIRST(&pt->head))
                CFRunLoopStop(CFRunLoopGetCurrent());
            break;
        }
    }
    return 0;
}

OSStatus ServerCommandSendReceive(uint32_t id, CFTypeRef in, CFTypeRef *out)
{
    CFDataRef data_in = NULL, data_out = NULL;
    void *bytes_out = NULL; size_t length_out = 0;

    if (in) {
#ifndef NDEBUG
        CFDataRef query_debug = CFPropertyListCreateXMLData(kCFAllocatorDefault, in);
        if (query_debug) {
            secdebug("client", "securityd query: %.*s\n",
                CFDataGetLength(query_debug), CFDataGetBytePtr(query_debug));
            CFReleaseSafe(query_debug);
        }
#endif
        CFErrorRef error = NULL;
        data_in = CFPropertyListCreateData(kCFAllocatorDefault, in,
                                           kCFPropertyListBinaryFormat_v1_0,
                                           0, &error);
        if (!data_in) {
            secdebug("client", "failed to encode query: %@", error);
            CFReleaseSafe(error);
            return errSecItemIllegalQuery;
        }
    }

    OSStatus status = send_receive(id, data_in ? CFDataGetBytePtr(data_in) : NULL, 
        data_in ? CFDataGetLength(data_in) : 0, &bytes_out, &length_out);
    if (data_in) CFRelease(data_in);

    if (bytes_out && length_out) {
        data_out = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, bytes_out, length_out, kCFAllocatorMalloc);
        if (out)
            *out = CFPropertyListCreateFromXMLData(kCFAllocatorDefault, data_out, kCFPropertyListImmutable, NULL);
        CFRelease(data_out);
    } 
    else
        if (!status && out)
            *out = NULL;

    return status;	
}

/* vi:set ts=4 sw=4 et: */
