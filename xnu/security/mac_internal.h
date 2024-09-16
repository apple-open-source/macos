/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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

/*-
 * Copyright (c) 1999, 2000, 2001, 2002 Robert N. M. Watson
 * Copyright (c) 2001 Ilmar S. Habibulin
 * Copyright (c) 2001, 2002, 2003, 2004 Networks Associates Technology, Inc.
 * Copyright (c) 2005 SPARTA, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson and Ilmar Habibulin for the
 * TrustedBSD Project.
 *
 * This software was developed for the FreeBSD Project in part by Network
 * Associates Laboratories, the Security Research Division of Network
 * Associates, Inc. under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"),
 * as part of the DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _SECURITY_MAC_INTERNAL_H_
#define _SECURITY_MAC_INTERNAL_H_

#ifndef PRIVATE
#warning "MAC policy is not KPI, see Technical Q&A QA1574, this header will be removed in next version"
#endif

#include <string.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <security/mac.h>
#include <security/mac_framework.h>
#include <security/mac_policy.h>
#include <security/mac_data.h>
#include <sys/sysctl.h>
#include <kern/locks.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/sbuf.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/vnode.h>
#include <mach/sdt.h>

/*
 * MAC Framework sysctl namespace.
 */

SYSCTL_DECL(_security);
SYSCTL_DECL(_security_mac);

extern int mac_late;

struct mac_policy_list_element {
	struct mac_policy_conf *mpc;
};

struct mac_policy_list {
	u_int                           numloaded;
	u_int                           max;
	u_int                           maxindex;
	u_int                           staticmax;
	u_int                           chunks;
	u_int                           freehint;
	struct mac_policy_list_element  *entries;
};

typedef struct mac_policy_list mac_policy_list_t;


/*
 * Policy that has registered with the framework for a specific
 * label namespace name.
 */
struct mac_label_listener {
	mac_policy_handle_t             mll_handle;
	SLIST_ENTRY(mac_label_listener) mll_list;
};

SLIST_HEAD(mac_label_listeners_t, mac_label_listener);

/*
 * Type of list used to manage label namespace names.
 */
struct mac_label_element {
	char                            mle_name[MAC_MAX_LABEL_ELEMENT_NAME];
	struct mac_label_listeners_t    mle_listeners;
	SLIST_ENTRY(mac_label_element)  mle_list;
};

SLIST_HEAD(mac_label_element_list_t, mac_label_element);

/*
 * MAC Framework global variables.
 */

extern struct mac_label_element_list_t mac_label_element_list;
extern struct mac_label_element_list_t mac_static_label_element_list;

extern struct mac_policy_list mac_policy_list;

/*
 * global flags to control whether a MACF subsystem is configured
 * at all in the system.
 */
extern unsigned int mac_device_enforce;
extern unsigned int mac_pipe_enforce;
extern unsigned int mac_posixsem_enforce;
extern unsigned int mac_posixshm_enforce;
extern unsigned int mac_proc_enforce;
extern unsigned int mac_socket_enforce;
extern unsigned int mac_system_enforce;
extern unsigned int mac_sysvmsg_enforce;
extern unsigned int mac_sysvsem_enforce;
extern unsigned int mac_sysvshm_enforce;
extern unsigned int mac_vm_enforce;
extern unsigned int mac_vnode_enforce;

extern unsigned int mac_label_vnodes;
extern unsigned int mac_vnode_label_count;

static bool mac_proc_check_enforce(proc_t p);

static __inline__ bool
mac_proc_check_enforce(proc_t p)
{
#if CONFIG_MACF
	// Don't apply policies to the kernel itself.
	return p != kernproc;
#else
#pragma unused(p)
	return false;
#endif // CONFIG_MACF
}

static bool mac_cred_check_enforce(kauth_cred_t cred);

static __inline__ bool
mac_cred_check_enforce(kauth_cred_t cred)
{
#if CONFIG_MACF
	return cred != proc_ucred_unsafe(kernproc);
#else
#pragma unused(p)
	return false;
#endif // CONFIG_MACF
}

/*
 * MAC Framework infrastructure functions.
 */

int mac_error_select(int error1, int error2);

void  mac_policy_list_busy(void);
int   mac_policy_list_conditional_busy(void);
void  mac_policy_list_unbusy(void);

#if KERNEL
int   mac_check_structmac_consistent(struct user_mac *mac);
#else
int   mac_check_structmac_consistent(struct mac *mac);
#endif

int mac_cred_label_externalize(struct label *, char *e, char *out, size_t olen, int flags);
int mac_vnode_label_externalize(struct label *, char *e, char *out, size_t olen, int flags);

int mac_cred_label_internalize(struct label *label, char *string);
int mac_vnode_label_internalize(struct label *label, char *string);

typedef int (^mac_getter_t)(char *, char *, size_t);
typedef int (^mac_setter_t)(char *, size_t);

int mac_do_get(struct proc *p, user_addr_t mac_p, mac_getter_t getter);
int mac_do_set(struct proc *p, user_addr_t mac_p, mac_setter_t setter);

#define MAC_POLICY_ITERATE(...) do {                                \
    struct mac_policy_conf *mpc;                                    \
    u_int i;                                                        \
                                                                    \
    for (i = 0; i < mac_policy_list.staticmax; i++) {               \
	    mpc = mac_policy_list.entries[i].mpc;                       \
	    if (mpc == NULL)                                            \
	            continue;                                           \
                                                                    \
	    __VA_ARGS__                                                 \
    }                                                               \
    if (mac_policy_list_conditional_busy() != 0) {                  \
	    for (; i <= mac_policy_list.maxindex; i++) {                \
	            mpc = mac_policy_list.entries[i].mpc;               \
	            if (mpc == NULL)                                    \
	                    continue;                                   \
                                                                    \
	            __VA_ARGS__                                         \
	    }                                                           \
	    mac_policy_list_unbusy();                                   \
    }                                                               \
} while (0)

enum mac_iterate_types {
	MAC_ITERATE_CHECK = 0,  // error starts at 0, callbacks can change it
	MAC_ITERATE_GRANT = 1,  // error starts as EPERM, callbacks can clear it
	MAC_ITERATE_PERFORM = 2, // no result
};

#define MAC_CHECK_CALL(check, mpc) DTRACE_MACF3(mac__call__ ## check, void *, mpc, int, error, int, MAC_ITERATE_CHECK)
#define MAC_CHECK_RSLT(check, mpc) DTRACE_MACF2(mac__rslt__ ## check, void *, mpc, int, __step_err)

/*
 * MAC_CHECK performs the designated check by walking the policy
 * module list and checking with each as to how it feels about the
 * request.  Note that it returns its value via 'error' in the scope
 * of the caller.
 */
#define MAC_CHECK(check, args...) do {                                   \
    error = 0;                                                           \
    MAC_POLICY_ITERATE({                                                 \
	    if (mpc->mpc_ops->mpo_ ## check != NULL) {                   \
	            MAC_CHECK_CALL(check, mpc);                          \
	            int __step_err = mpc->mpc_ops->mpo_ ## check (args); \
	            MAC_CHECK_RSLT(check, mpc);                          \
	            error = mac_error_select(__step_err, error);         \
	    }                                                            \
    });                                                                  \
} while (0)

/*
 * MAC_GRANT performs the designated check by walking the policy
 * module list and checking with each as to how it feels about the
 * request.  Unlike MAC_CHECK, it grants if any policies return '0',
 * and otherwise returns EPERM.  Note that it returns its value via
 * 'error' in the scope of the caller.
 */
#define MAC_GRANT(check, args...) do {                              \
    error = EPERM;                                                  \
    MAC_POLICY_ITERATE({                                            \
	if (mpc->mpc_ops->mpo_ ## check != NULL) {                  \
	        DTRACE_MACF3(mac__call__ ## check, void *, mpc, int, error, int, MAC_ITERATE_GRANT); \
	        int __step_res = mpc->mpc_ops->mpo_ ## check (args); \
	        if (__step_res == 0) {                              \
	                error = 0;                                  \
	        }                                                   \
	        DTRACE_MACF2(mac__rslt__ ## check, void *, mpc, int, __step_res); \
	    }                                                           \
    });                                                             \
} while (0)

#define MAC_INTERNALIZE(obj, label, instring)                       \
    mac_internalize(offsetof(struct mac_policy_ops, mpo_ ## obj ## _label_internalize), label, instring)

#define MAC_EXTERNALIZE(obj, label, elementlist, outbuf, outbuflen) \
    mac_externalize(offsetof(struct mac_policy_ops, mpo_ ## obj ## _label_externalize), label, elementlist, outbuf, outbuflen)

#define MAC_EXTERNALIZE_AUDIT(obj, label, outbuf, outbuflen)        \
    mac_externalize(offsetof(struct mac_policy_ops, mpo_ ## obj ## _label_externalize_audit), label, "*", outbuf, outbuflen)

#define MAC_PERFORM_CALL(operation, mpc) DTRACE_MACF3(mac__call__ ## operation, void *, mpc, int, 0, int, MAC_ITERATE_PERFORM)
#define MAC_PERFORM_RSLT(operation, mpc) DTRACE_MACF2(mac__rslt__ ## operation, void *, mpc, int, 0)

/*
 * MAC_PERFORM performs the designated operation by walking the policy
 * module list and invoking that operation for each policy.
 */
#define MAC_PERFORM(operation, args...) do {                \
    MAC_POLICY_ITERATE({                                    \
	if (mpc->mpc_ops->mpo_ ## operation != NULL) {      \
	        MAC_PERFORM_CALL(operation, mpc);           \
	        mpc->mpc_ops->mpo_ ## operation (args);     \
	        MAC_PERFORM_RSLT(operation, mpc);           \
	}                                                   \
    });                                                     \
} while (0)


#
struct __mac_get_pid_args;
struct __mac_get_proc_args;
struct __mac_set_proc_args;
struct __mac_get_lcid_args;
struct __mac_get_fd_args;
struct __mac_get_file_args;
struct __mac_get_link_args;
struct __mac_set_fd_args;
struct __mac_set_file_args;
struct __mac_syscall_args;

void mac_policy_addto_labellist(const mac_policy_handle_t, int);
void mac_policy_removefrom_labellist(const mac_policy_handle_t);

int mac_externalize(size_t mpo_externalize_off, struct label *label,
    const char *elementlist, char *outbuf, size_t outbuflen);
int mac_internalize(size_t mpo_internalize_off, struct label *label,
    char *elementlist);
#endif  /* !_SECURITY_MAC_INTERNAL_H_ */
