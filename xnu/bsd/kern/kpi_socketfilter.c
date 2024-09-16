/*
 * Copyright (c) 2003-2021 Apple Inc. All rights reserved.
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

#include <sys/kpi_socketfilter.h>

#include <sys/socket.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/proc.h>
#include <kern/locks.h>
#include <kern/thread.h>
#include <kern/debug.h>
#include <net/kext_net.h>
#include <net/if.h>
#include <net/net_api_stats.h>
#if SKYWALK
#include <skywalk/lib/net_filter_event.h>
#endif /* SKYWALK */
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include <libkern/libkern.h>
#include <libkern/OSAtomic.h>

#include <libkern/sysctl.h>
#include <libkern/OSDebug.h>

#include <os/refcnt.h>

#include <stdbool.h>
#include <string.h>

#if SKYWALK
#include <skywalk/core/skywalk_var.h>
#endif /* SKYWALK */

#include <net/sockaddr_utils.h>

#define SFEF_ATTACHED           0x1     /* SFE is on socket list */
#define SFEF_NODETACH           0x2     /* Detach should not be called */
#define SFEF_NOSOCKET           0x4     /* Socket is gone */

struct socket_filter_entry {
	struct socket_filter_entry      *sfe_next_onsocket;
	struct socket_filter_entry      *sfe_next_onfilter;
	struct socket_filter_entry      *sfe_next_oncleanup;

	struct socket_filter            *sfe_filter;
	struct socket                   *sfe_socket;
	void                            *sfe_cookie;

	uint32_t                        sfe_flags;
	struct os_refcnt                sfe_refcount;
};

struct socket_filter {
	TAILQ_ENTRY(socket_filter)      sf_protosw_next;
	TAILQ_ENTRY(socket_filter)      sf_global_next;
	struct socket_filter_entry      *sf_entry_head;

	struct protosw                  *sf_proto;
	struct sflt_filter              sf_filter;
	struct os_refcnt                sf_refcount;
	uint32_t                        sf_flags;
};

#define SFF_INTERNAL    0x1

TAILQ_HEAD(socket_filter_list, socket_filter);

static LCK_GRP_DECLARE(sock_filter_lock_grp, "socket filter lock");
static LCK_RW_DECLARE(sock_filter_lock, &sock_filter_lock_grp);
static LCK_MTX_DECLARE(sock_filter_cleanup_lock, &sock_filter_lock_grp);

static struct socket_filter_list        sock_filter_head =
    TAILQ_HEAD_INITIALIZER(sock_filter_head);
static struct socket_filter_entry       *sock_filter_cleanup_entries = NULL;
static thread_t                         sock_filter_cleanup_thread = NULL;

static void sflt_cleanup_thread(void *, wait_result_t);
static void sflt_detach_locked(struct socket_filter_entry *entry);

#undef sflt_register
static errno_t sflt_register_common(const struct sflt_filter *filter, int domain,
    int type, int protocol, bool is_internal);
errno_t sflt_register(const struct sflt_filter *filter, int domain,
    int type, int protocol);

#if SKYWALK
static bool net_check_compatible_sfltr(void);
bool net_check_compatible_alf(void);
static bool net_check_compatible_parental_controls(void);
#endif /* SKYWALK */

#pragma mark -- Internal State Management --

__private_extern__ int
sflt_permission_check(struct inpcb *inp)
{
	/* Only IPv4 or IPv6 sockets can bypass filters */
	if (!(inp->inp_vflag & INP_IPV4) &&
	    !(inp->inp_vflag & INP_IPV6)) {
		return 0;
	}
	/* Sockets that have incoproc or management entitlements bypass socket filters. */
	if (INP_INTCOPROC_ALLOWED(inp) || INP_MANAGEMENT_ALLOWED(inp)) {
		return 1;
	}
	/* Sockets bound to an intcoproc or management interface bypass socket filters. */
	if ((inp->inp_flags & INP_BOUND_IF) &&
	    (IFNET_IS_INTCOPROC(inp->inp_boundifp) ||
	    IFNET_IS_MANAGEMENT(inp->inp_boundifp))) {
		return 1;
	}
#if NECP
	/*
	 * Make sure that the NECP policy is populated.
	 * If result is not populated, the policy ID will be
	 * NECP_KERNEL_POLICY_ID_NONE. Note that if the result
	 * is populated, but there was no match, it will be
	 * NECP_KERNEL_POLICY_ID_NO_MATCH.
	 * Do not call inp_update_necp_policy() to avoid scoping
	 * a socket prior to calls to bind().
	 */
	if (inp->inp_policyresult.policy_id == NECP_KERNEL_POLICY_ID_NONE) {
		necp_socket_find_policy_match(inp, NULL, NULL, 0);
	}

	/* If the filter unit is marked to be "no filter", bypass filters */
	if (inp->inp_policyresult.results.filter_control_unit ==
	    NECP_FILTER_UNIT_NO_FILTER) {
		return 1;
	}
#endif /* NECP */
	return 0;
}

static void
sflt_retain_locked(struct socket_filter *filter)
{
	os_ref_retain_locked(&filter->sf_refcount);
}

static void
sflt_release_locked(struct socket_filter *filter)
{
	if (os_ref_release_locked(&filter->sf_refcount) == 0) {
		/* Call the unregistered function */
		if (filter->sf_filter.sf_unregistered) {
			lck_rw_unlock_exclusive(&sock_filter_lock);
			filter->sf_filter.sf_unregistered(
				filter->sf_filter.sf_handle);
			lck_rw_lock_exclusive(&sock_filter_lock);
		}

		/* Free the entry */
		kfree_type(struct socket_filter, filter);
	}
}

static void
sflt_entry_retain(struct socket_filter_entry *entry)
{
	os_ref_retain(&entry->sfe_refcount);
}

static void
sflt_entry_release(struct socket_filter_entry *entry)
{
	if (os_ref_release(&entry->sfe_refcount) == 0) {
		/* That was the last reference */

		/* Take the cleanup lock */
		lck_mtx_lock(&sock_filter_cleanup_lock);

		/* Put this item on the cleanup list */
		entry->sfe_next_oncleanup = sock_filter_cleanup_entries;
		sock_filter_cleanup_entries = entry;

		/* If the item is the first item in the list */
		if (entry->sfe_next_oncleanup == NULL) {
			if (sock_filter_cleanup_thread == NULL) {
				/* Create a thread */
				kernel_thread_start(sflt_cleanup_thread,
				    NULL, &sock_filter_cleanup_thread);
			} else {
				/* Wakeup the thread */
				wakeup(&sock_filter_cleanup_entries);
			}
		}

		/* Drop the cleanup lock */
		lck_mtx_unlock(&sock_filter_cleanup_lock);
	}
}

__attribute__((noreturn))
static void
sflt_cleanup_thread(void *blah, wait_result_t blah2)
{
#pragma unused(blah, blah2)
	while (1) {
		lck_mtx_lock(&sock_filter_cleanup_lock);
		while (sock_filter_cleanup_entries == NULL) {
			/* Sleep until we've got something better to do */
			msleep(&sock_filter_cleanup_entries,
			    &sock_filter_cleanup_lock, PWAIT,
			    "sflt_cleanup", NULL);
		}

		/* Pull the current list of dead items */
		struct socket_filter_entry *dead = sock_filter_cleanup_entries;
		sock_filter_cleanup_entries = NULL;

		/* Drop the lock */
		lck_mtx_unlock(&sock_filter_cleanup_lock);

		/* Take the socket filter lock */
		lck_rw_lock_exclusive(&sock_filter_lock);

		/* Cleanup every dead item */
		struct socket_filter_entry *__single entry;
		for (entry = dead; entry; entry = dead) {
			struct socket_filter_entry **__single nextpp;

			dead = entry->sfe_next_oncleanup;

			/* Call detach function if necessary - drop the lock */
			if ((entry->sfe_flags & SFEF_NODETACH) == 0 &&
			    entry->sfe_filter->sf_filter.sf_detach) {
				entry->sfe_flags |= SFEF_NODETACH;
				lck_rw_unlock_exclusive(&sock_filter_lock);

				/*
				 * Warning - passing a potentially
				 * dead socket may be bad
				 */
				entry->sfe_filter->sf_filter.sf_detach(
					entry->sfe_cookie, entry->sfe_socket);

				lck_rw_lock_exclusive(&sock_filter_lock);
			}

			/*
			 * Pull entry off the socket list --
			 * if the socket still exists
			 */
			if ((entry->sfe_flags & SFEF_NOSOCKET) == 0) {
				for (nextpp = &entry->sfe_socket->so_filt;
				    *nextpp;
				    nextpp = &(*nextpp)->sfe_next_onsocket) {
					if (*nextpp == entry) {
						*nextpp =
						    entry->sfe_next_onsocket;
						break;
					}
				}
			}

			/* Pull entry off the filter list */
			for (nextpp = &entry->sfe_filter->sf_entry_head;
			    *nextpp; nextpp = &(*nextpp)->sfe_next_onfilter) {
				if (*nextpp == entry) {
					*nextpp = entry->sfe_next_onfilter;
					break;
				}
			}

			/*
			 * Release the filter -- may drop lock, but that's okay
			 */
			sflt_release_locked(entry->sfe_filter);
			entry->sfe_socket = NULL;
			entry->sfe_filter = NULL;
			kfree_type(struct socket_filter_entry, entry);
		}

		/* Drop the socket filter lock */
		lck_rw_unlock_exclusive(&sock_filter_lock);
	}
	/* NOTREACHED */
}

static int
sflt_attach_locked(struct socket *so, struct socket_filter *filter,
    int socklocked)
{
	int error = 0;
	struct socket_filter_entry *__single entry = NULL;

	if (sflt_permission_check(sotoinpcb(so))) {
		return 0;
	}

	if (filter == NULL) {
		return ENOENT;
	}

	for (entry = so->so_filt; entry; entry = entry->sfe_next_onfilter) {
		if (entry->sfe_filter->sf_filter.sf_handle ==
		    filter->sf_filter.sf_handle) {
			return EEXIST;
		}
	}
	/* allocate the socket filter entry */
	entry = kalloc_type(struct socket_filter_entry, Z_WAITOK | Z_NOFAIL);

	/* Initialize the socket filter entry */
	entry->sfe_cookie = NULL;
	entry->sfe_flags = SFEF_ATTACHED;
	os_ref_init(&entry->sfe_refcount, NULL); /* corresponds to SFEF_ATTACHED flag set */

	/* Put the entry in the filter list */
	sflt_retain_locked(filter);
	entry->sfe_filter = filter;
	entry->sfe_next_onfilter = filter->sf_entry_head;
	filter->sf_entry_head = entry;

	/* Put the entry on the socket filter list */
	entry->sfe_socket = so;
	entry->sfe_next_onsocket = so->so_filt;
	so->so_filt = entry;

	if (entry->sfe_filter->sf_filter.sf_attach) {
		/* Retain the entry while we call attach */
		sflt_entry_retain(entry);

		/*
		 * Release the filter lock --
		 * callers must be aware we will do this
		 */
		lck_rw_unlock_exclusive(&sock_filter_lock);

		/* Unlock the socket */
		if (socklocked) {
			socket_unlock(so, 0);
		}

		/* It's finally safe to call the filter function */
		error = entry->sfe_filter->sf_filter.sf_attach(
			&entry->sfe_cookie, so);

		/* Lock the socket again */
		if (socklocked) {
			socket_lock(so, 0);
		}

		/* Lock the filters again */
		lck_rw_lock_exclusive(&sock_filter_lock);

		/*
		 * If the attach function returns an error,
		 * this filter must be detached
		 */
		if (error) {
			/* don't call sf_detach */
			entry->sfe_flags |= SFEF_NODETACH;
			sflt_detach_locked(entry);
		}

		/* Release the retain we held through the attach call */
		sflt_entry_release(entry);
	}

	return error;
}

errno_t
sflt_attach_internal(socket_t socket, sflt_handle handle)
{
	if (socket == NULL || handle == 0) {
		return EINVAL;
	}

	int result = EINVAL;

	lck_rw_lock_exclusive(&sock_filter_lock);

	struct socket_filter *__single filter = NULL;
	TAILQ_FOREACH(filter, &sock_filter_head, sf_global_next) {
		if (filter->sf_filter.sf_handle == handle) {
			break;
		}
	}

	if (filter) {
		result = sflt_attach_locked(socket, filter, 1);
	}

	lck_rw_unlock_exclusive(&sock_filter_lock);

	return result;
}

static void
sflt_detach_locked(struct socket_filter_entry *entry)
{
	if ((entry->sfe_flags & SFEF_ATTACHED) != 0) {
		entry->sfe_flags &= ~SFEF_ATTACHED;
		sflt_entry_release(entry);
	}
}

#pragma mark -- Socket Layer Hooks --

__private_extern__ void
sflt_initsock(struct socket *so)
{
	/*
	 * Can only register socket filter for internet protocols
	 */
	if (SOCK_DOM(so) != PF_INET && SOCK_DOM(so) != PF_INET6) {
		return;
	}

	/*
	 * Point to the real protosw, as so_proto might have been
	 * pointed to a modified version.
	 */
	struct protosw *__single proto = so->so_proto->pr_protosw;

	lck_rw_lock_shared(&sock_filter_lock);
	if (TAILQ_FIRST(&proto->pr_filter_head) != NULL) {
		/* Promote lock to exclusive */
		if (!lck_rw_lock_shared_to_exclusive(&sock_filter_lock)) {
			lck_rw_lock_exclusive(&sock_filter_lock);
		}

		/*
		 * Warning: A filter unregistering will be pulled out of
		 * the list.  This could happen while we drop the lock in
		 * sftl_attach_locked or sflt_release_locked.  For this
		 * reason we retain a reference on the filter (or next_filter)
		 * while calling this function.  This protects us from a panic,
		 * but it could result in a socket being created without all
		 * of the global filters if we're attaching a filter as it
		 * is removed, if that's possible.
		 */
		struct socket_filter *__single filter =
		    TAILQ_FIRST(&proto->pr_filter_head);

		sflt_retain_locked(filter);

		while (filter) {
			struct socket_filter *__single filter_next;
			/*
			 * Warning: sflt_attach_private_locked
			 * will drop the lock
			 */
			sflt_attach_locked(so, filter, 0);

			filter_next = TAILQ_NEXT(filter, sf_protosw_next);
			if (filter_next) {
				sflt_retain_locked(filter_next);
			}

			/*
			 * Warning: filt_release_locked may remove
			 * the filter from the queue
			 */
			sflt_release_locked(filter);
			filter = filter_next;
		}
	}
	lck_rw_done(&sock_filter_lock);
}

/*
 * sflt_termsock
 *
 * Detaches all filters from the socket.
 */
__private_extern__ void
sflt_termsock(struct socket *so)
{
	/*
	 * Fast path to avoid taking the lock
	 */
	if (so->so_filt == NULL) {
		return;
	}

	lck_rw_lock_exclusive(&sock_filter_lock);

	struct socket_filter_entry *__single entry;

	while ((entry = so->so_filt) != NULL) {
		/* Pull filter off the socket */
		so->so_filt = entry->sfe_next_onsocket;
		entry->sfe_flags |= SFEF_NOSOCKET;

		/* Call detach */
		sflt_detach_locked(entry);

		/*
		 * On sflt_termsock, we can't return until the detach function
		 * has been called.  Call the detach function - this is gross
		 * because the socket filter entry could be freed when we drop
		 * the lock, so we make copies on  the stack and retain
		 * everything we need before dropping the lock.
		 */
		if ((entry->sfe_flags & SFEF_NODETACH) == 0 &&
		    entry->sfe_filter->sf_filter.sf_detach) {
			void *__single sfe_cookie = entry->sfe_cookie;
			struct socket_filter *__single sfe_filter = entry->sfe_filter;

			/* Retain the socket filter */
			sflt_retain_locked(sfe_filter);

			/* Mark that we've called the detach function */
			entry->sfe_flags |= SFEF_NODETACH;

			/* Drop the lock before calling the detach function */
			lck_rw_unlock_exclusive(&sock_filter_lock);
			sfe_filter->sf_filter.sf_detach(sfe_cookie, so);
			lck_rw_lock_exclusive(&sock_filter_lock);

			/* Release the filter */
			sflt_release_locked(sfe_filter);
		}
	}

	lck_rw_unlock_exclusive(&sock_filter_lock);
}


static void
sflt_notify_internal(struct socket *so, sflt_event_t event, void *param,
    sflt_handle handle)
{
	if (so->so_filt == NULL) {
		return;
	}

	struct socket_filter_entry *__single entry;
	int unlocked = 0;

	lck_rw_lock_shared(&sock_filter_lock);
	for (entry = so->so_filt; entry; entry = entry->sfe_next_onsocket) {
		if ((entry->sfe_flags & SFEF_ATTACHED) &&
		    entry->sfe_filter->sf_filter.sf_notify &&
		    ((handle && entry->sfe_filter->sf_filter.sf_handle !=
		    handle) || !handle)) {
			/*
			 * Retain the filter entry and release
			 * the socket filter lock
			 */
			sflt_entry_retain(entry);
			lck_rw_unlock_shared(&sock_filter_lock);

			/* If the socket isn't already unlocked, unlock it */
			if (unlocked == 0) {
				unlocked = 1;
				socket_unlock(so, 0);
			}

			/* Finally call the filter */
			entry->sfe_filter->sf_filter.sf_notify(
				entry->sfe_cookie, so, event, param);

			/*
			 * Take the socket filter lock again
			 * and release the entry
			 */
			lck_rw_lock_shared(&sock_filter_lock);
			sflt_entry_release(entry);
		}
	}
	lck_rw_unlock_shared(&sock_filter_lock);

	if (unlocked != 0) {
		socket_lock(so, 0);
	}
}

__private_extern__ void
sflt_notify(struct socket *so, sflt_event_t event, void  *param)
{
	sflt_notify_internal(so, event, param, 0);
}

static void
sflt_notify_after_register(struct socket *so, sflt_event_t event,
    sflt_handle handle)
{
	sflt_notify_internal(so, event, NULL, handle);
}

__private_extern__ int
sflt_ioctl(struct socket *so, u_long cmd, caddr_t __sized_by(IOCPARM_LEN(cmd)) data)
{
	if (so->so_filt == NULL || sflt_permission_check(sotoinpcb(so))) {
		return 0;
	}

	struct socket_filter_entry *__single entry;
	int unlocked = 0;
	int error = 0;

	lck_rw_lock_shared(&sock_filter_lock);
	for (entry = so->so_filt; entry && error == 0;
	    entry = entry->sfe_next_onsocket) {
		if ((entry->sfe_flags & SFEF_ATTACHED) &&
		    entry->sfe_filter->sf_filter.sf_ioctl) {
			/*
			 * Retain the filter entry and release
			 * the socket filter lock
			 */
			sflt_entry_retain(entry);
			lck_rw_unlock_shared(&sock_filter_lock);

			/* If the socket isn't already unlocked, unlock it */
			if (unlocked == 0) {
				socket_unlock(so, 0);
				unlocked = 1;
			}

			/* Call the filter */
			error = entry->sfe_filter->sf_filter.sf_ioctl(
				entry->sfe_cookie, so, cmd, data);

			/*
			 * Take the socket filter lock again
			 * and release the entry
			 */
			lck_rw_lock_shared(&sock_filter_lock);
			sflt_entry_release(entry);
		}
	}
	lck_rw_unlock_shared(&sock_filter_lock);

	if (unlocked) {
		socket_lock(so, 0);
	}

	return error;
}

__private_extern__ int
sflt_bind(struct socket *so, const struct sockaddr *nam)
{
	if (so->so_filt == NULL || sflt_permission_check(sotoinpcb(so))) {
		return 0;
	}

	struct socket_filter_entry *__single entry;
	int unlocked = 0;
	int error = 0;

	lck_rw_lock_shared(&sock_filter_lock);
	for (entry = so->so_filt; entry && error == 0;
	    entry = entry->sfe_next_onsocket) {
		if ((entry->sfe_flags & SFEF_ATTACHED) &&
		    entry->sfe_filter->sf_filter.sf_bind) {
			/*
			 * Retain the filter entry and
			 * release the socket filter lock
			 */
			sflt_entry_retain(entry);
			lck_rw_unlock_shared(&sock_filter_lock);

			/* If the socket isn't already unlocked, unlock it */
			if (unlocked == 0) {
				socket_unlock(so, 0);
				unlocked = 1;
			}

			/* Call the filter */
			error = entry->sfe_filter->sf_filter.sf_bind(
				entry->sfe_cookie, so, nam);

			/*
			 * Take the socket filter lock again and
			 * release the entry
			 */
			lck_rw_lock_shared(&sock_filter_lock);
			sflt_entry_release(entry);
		}
	}
	lck_rw_unlock_shared(&sock_filter_lock);

	if (unlocked) {
		socket_lock(so, 0);
	}

	return error;
}

__private_extern__ int
sflt_listen(struct socket *so)
{
	if (so->so_filt == NULL || sflt_permission_check(sotoinpcb(so))) {
		return 0;
	}

	struct socket_filter_entry *__single entry;
	int unlocked = 0;
	int error = 0;

	lck_rw_lock_shared(&sock_filter_lock);
	for (entry = so->so_filt; entry && error == 0;
	    entry = entry->sfe_next_onsocket) {
		if ((entry->sfe_flags & SFEF_ATTACHED) &&
		    entry->sfe_filter->sf_filter.sf_listen) {
			/*
			 * Retain the filter entry and release
			 * the socket filter lock
			 */
			sflt_entry_retain(entry);
			lck_rw_unlock_shared(&sock_filter_lock);

			/* If the socket isn't already unlocked, unlock it */
			if (unlocked == 0) {
				socket_unlock(so, 0);
				unlocked = 1;
			}

			/* Call the filter */
			error = entry->sfe_filter->sf_filter.sf_listen(
				entry->sfe_cookie, so);

			/*
			 * Take the socket filter lock again
			 * and release the entry
			 */
			lck_rw_lock_shared(&sock_filter_lock);
			sflt_entry_release(entry);
		}
	}
	lck_rw_unlock_shared(&sock_filter_lock);

	if (unlocked) {
		socket_lock(so, 0);
	}

	return error;
}

__private_extern__ int
sflt_accept(struct socket *head, struct socket *so,
    const struct sockaddr *local, const struct sockaddr *remote)
{
	if (so->so_filt == NULL || sflt_permission_check(sotoinpcb(so))) {
		return 0;
	}

	struct socket_filter_entry *__single entry;
	int unlocked = 0;
	int error = 0;

	lck_rw_lock_shared(&sock_filter_lock);
	for (entry = so->so_filt; entry && error == 0;
	    entry = entry->sfe_next_onsocket) {
		if ((entry->sfe_flags & SFEF_ATTACHED) &&
		    entry->sfe_filter->sf_filter.sf_accept) {
			/*
			 * Retain the filter entry and
			 * release the socket filter lock
			 */
			sflt_entry_retain(entry);
			lck_rw_unlock_shared(&sock_filter_lock);

			/* If the socket isn't already unlocked, unlock it */
			if (unlocked == 0) {
				socket_unlock(so, 0);
				unlocked = 1;
			}

			/* Call the filter */
			error = entry->sfe_filter->sf_filter.sf_accept(
				entry->sfe_cookie, head, so, local, remote);

			/*
			 * Take the socket filter lock again
			 * and release the entry
			 */
			lck_rw_lock_shared(&sock_filter_lock);
			sflt_entry_release(entry);
		}
	}
	lck_rw_unlock_shared(&sock_filter_lock);

	if (unlocked) {
		socket_lock(so, 0);
	}

	return error;
}

__private_extern__ int
sflt_getsockname(struct socket *so, struct sockaddr **local)
{
	if (so->so_filt == NULL || sflt_permission_check(sotoinpcb(so))) {
		return 0;
	}

	struct socket_filter_entry *__single entry;
	int unlocked = 0;
	int error = 0;

	lck_rw_lock_shared(&sock_filter_lock);
	for (entry = so->so_filt; entry && error == 0;
	    entry = entry->sfe_next_onsocket) {
		if ((entry->sfe_flags & SFEF_ATTACHED) &&
		    entry->sfe_filter->sf_filter.sf_getsockname) {
			/*
			 * Retain the filter entry and
			 * release the socket filter lock
			 */
			sflt_entry_retain(entry);
			lck_rw_unlock_shared(&sock_filter_lock);

			/* If the socket isn't already unlocked, unlock it */
			if (unlocked == 0) {
				socket_unlock(so, 0);
				unlocked = 1;
			}

			/* Call the filter */
			error = entry->sfe_filter->sf_filter.sf_getsockname(
				entry->sfe_cookie, so, local);

			/*
			 * Take the socket filter lock again
			 * and release the entry
			 */
			lck_rw_lock_shared(&sock_filter_lock);
			sflt_entry_release(entry);
		}
	}
	lck_rw_unlock_shared(&sock_filter_lock);

	if (unlocked) {
		socket_lock(so, 0);
	}

	return error;
}

__private_extern__ int
sflt_getpeername(struct socket *so, struct sockaddr **remote)
{
	if (so->so_filt == NULL || sflt_permission_check(sotoinpcb(so))) {
		return 0;
	}

	struct socket_filter_entry *__single entry;
	int unlocked = 0;
	int error = 0;

	lck_rw_lock_shared(&sock_filter_lock);
	for (entry = so->so_filt; entry && error == 0;
	    entry = entry->sfe_next_onsocket) {
		if ((entry->sfe_flags & SFEF_ATTACHED) &&
		    entry->sfe_filter->sf_filter.sf_getpeername) {
			/*
			 * Retain the filter entry and release
			 * the socket filter lock
			 */
			sflt_entry_retain(entry);
			lck_rw_unlock_shared(&sock_filter_lock);

			/* If the socket isn't already unlocked, unlock it */
			if (unlocked == 0) {
				socket_unlock(so, 0);
				unlocked = 1;
			}

			/* Call the filter */
			error = entry->sfe_filter->sf_filter.sf_getpeername(
				entry->sfe_cookie, so, remote);

			/*
			 * Take the socket filter lock again
			 * and release the entry
			 */
			lck_rw_lock_shared(&sock_filter_lock);
			sflt_entry_release(entry);
		}
	}
	lck_rw_unlock_shared(&sock_filter_lock);

	if (unlocked) {
		socket_lock(so, 0);
	}

	return error;
}

__private_extern__ int
sflt_connectin(struct socket *so, const struct sockaddr *remote)
{
	if (so->so_filt == NULL || sflt_permission_check(sotoinpcb(so))) {
		return 0;
	}

	struct socket_filter_entry *__single entry;
	int unlocked = 0;
	int error = 0;

	lck_rw_lock_shared(&sock_filter_lock);
	for (entry = so->so_filt; entry && error == 0;
	    entry = entry->sfe_next_onsocket) {
		if ((entry->sfe_flags & SFEF_ATTACHED) &&
		    entry->sfe_filter->sf_filter.sf_connect_in) {
			/*
			 * Retain the filter entry and release
			 * the socket filter lock
			 */
			sflt_entry_retain(entry);
			lck_rw_unlock_shared(&sock_filter_lock);

			/* If the socket isn't already unlocked, unlock it */
			if (unlocked == 0) {
				socket_unlock(so, 0);
				unlocked = 1;
			}

			/* Call the filter */
			error = entry->sfe_filter->sf_filter.sf_connect_in(
				entry->sfe_cookie, so, remote);

			/*
			 * Take the socket filter lock again
			 * and release the entry
			 */
			lck_rw_lock_shared(&sock_filter_lock);
			sflt_entry_release(entry);
		}
	}
	lck_rw_unlock_shared(&sock_filter_lock);

	if (unlocked) {
		socket_lock(so, 0);
	}

	return error;
}

static int
sflt_connectout_common(struct socket *so, const struct sockaddr *nam)
{
	struct socket_filter_entry *__single entry;
	int unlocked = 0;
	int error = 0;

	lck_rw_lock_shared(&sock_filter_lock);
	for (entry = so->so_filt; entry && error == 0;
	    entry = entry->sfe_next_onsocket) {
		if ((entry->sfe_flags & SFEF_ATTACHED) &&
		    entry->sfe_filter->sf_filter.sf_connect_out) {
			/*
			 * Retain the filter entry and release
			 * the socket filter lock
			 */
			sflt_entry_retain(entry);
			lck_rw_unlock_shared(&sock_filter_lock);

			/* If the socket isn't already unlocked, unlock it */
			if (unlocked == 0) {
				socket_unlock(so, 0);
				unlocked = 1;
			}

			/* Call the filter */
			error = entry->sfe_filter->sf_filter.sf_connect_out(
				entry->sfe_cookie, so, nam);

			/*
			 * Take the socket filter lock again
			 * and release the entry
			 */
			lck_rw_lock_shared(&sock_filter_lock);
			sflt_entry_release(entry);
		}
	}
	lck_rw_unlock_shared(&sock_filter_lock);

	if (unlocked) {
		socket_lock(so, 0);
	}

	return error;
}

__private_extern__ int
sflt_connectout(struct socket *so, const struct sockaddr *innam)
{
	const struct sockaddr *nam = (const struct sockaddr *__indexable)innam;
	char buf[SOCK_MAXADDRLEN];
	struct sockaddr *sa;
	int error;

	if (so->so_filt == NULL || sflt_permission_check(sotoinpcb(so))) {
		return 0;
	}

	/*
	 * Workaround for rdar://23362120
	 * Always pass a buffer that can hold an IPv6 socket address
	 */
	bzero(buf, sizeof(buf));
	SOCKADDR_COPY(nam, buf, nam->sa_len);
	sa = (struct sockaddr *)buf;

	error = sflt_connectout_common(so, sa);
	if (error != 0) {
		return error;
	}

	/*
	 * If the address was modified, copy it back
	 */
	if (SOCKADDR_CMP(sa, nam, nam->sa_len) != 0) {
		SOCKADDR_COPY(sa, __DECONST_SA(nam), nam->sa_len);
	}

	return 0;
}

__private_extern__ int
sflt_setsockopt(struct socket *so, struct sockopt *sopt)
{
	if (so->so_filt == NULL || sflt_permission_check(sotoinpcb(so))) {
		return 0;
	}

	/* Socket-options are checked at the MPTCP-layer */
	if (so->so_flags & SOF_MP_SUBFLOW) {
		return 0;
	}

	struct socket_filter_entry *__single entry;
	int unlocked = 0;
	int error = 0;

	lck_rw_lock_shared(&sock_filter_lock);
	for (entry = so->so_filt; entry && error == 0;
	    entry = entry->sfe_next_onsocket) {
		if ((entry->sfe_flags & SFEF_ATTACHED) &&
		    entry->sfe_filter->sf_filter.sf_setoption) {
			/*
			 * Retain the filter entry and release
			 * the socket filter lock
			 */
			sflt_entry_retain(entry);
			lck_rw_unlock_shared(&sock_filter_lock);

			/* If the socket isn't already unlocked, unlock it */
			if (unlocked == 0) {
				socket_unlock(so, 0);
				unlocked = 1;
			}

			/* Call the filter */
			error = entry->sfe_filter->sf_filter.sf_setoption(
				entry->sfe_cookie, so, sopt);

			/*
			 * Take the socket filter lock again
			 * and release the entry
			 */
			lck_rw_lock_shared(&sock_filter_lock);
			sflt_entry_release(entry);
		}
	}
	lck_rw_unlock_shared(&sock_filter_lock);

	if (unlocked) {
		socket_lock(so, 0);
	}

	return error;
}

__private_extern__ int
sflt_getsockopt(struct socket *so, struct sockopt *sopt)
{
	if (so->so_filt == NULL || sflt_permission_check(sotoinpcb(so))) {
		return 0;
	}

	/* Socket-options are checked at the MPTCP-layer */
	if (so->so_flags & SOF_MP_SUBFLOW) {
		return 0;
	}

	struct socket_filter_entry *__single entry;
	int unlocked = 0;
	int error = 0;

	lck_rw_lock_shared(&sock_filter_lock);
	for (entry = so->so_filt; entry && error == 0;
	    entry = entry->sfe_next_onsocket) {
		if ((entry->sfe_flags & SFEF_ATTACHED) &&
		    entry->sfe_filter->sf_filter.sf_getoption) {
			/*
			 * Retain the filter entry and release
			 * the socket filter lock
			 */
			sflt_entry_retain(entry);
			lck_rw_unlock_shared(&sock_filter_lock);

			/* If the socket isn't already unlocked, unlock it */
			if (unlocked == 0) {
				socket_unlock(so, 0);
				unlocked = 1;
			}

			/* Call the filter */
			error = entry->sfe_filter->sf_filter.sf_getoption(
				entry->sfe_cookie, so, sopt);

			/*
			 * Take the socket filter lock again
			 * and release the entry
			 */
			lck_rw_lock_shared(&sock_filter_lock);
			sflt_entry_release(entry);
		}
	}
	lck_rw_unlock_shared(&sock_filter_lock);

	if (unlocked) {
		socket_lock(so, 0);
	}

	return error;
}

__private_extern__ int
sflt_data_out(struct socket *so, const struct sockaddr *to, mbuf_t *data,
    mbuf_t *control, sflt_data_flag_t flags)
{
	if (so->so_filt == NULL || sflt_permission_check(sotoinpcb(so))) {
		return 0;
	}

	/* Socket-options are checked at the MPTCP-layer */
	if (so->so_flags & SOF_MP_SUBFLOW) {
		return 0;
	}

	struct socket_filter_entry *__single entry;
	int unlocked = 0;
	int setsendthread = 0;
	int error = 0;

	lck_rw_lock_shared(&sock_filter_lock);
	for (entry = so->so_filt; entry && error == 0;
	    entry = entry->sfe_next_onsocket) {
		if ((entry->sfe_flags & SFEF_ATTACHED) &&
		    entry->sfe_filter->sf_filter.sf_data_out) {
			/*
			 * Retain the filter entry and
			 * release the socket filter lock
			 */
			sflt_entry_retain(entry);
			lck_rw_unlock_shared(&sock_filter_lock);

			/* If the socket isn't already unlocked, unlock it */
			if (unlocked == 0) {
				if (so->so_send_filt_thread == NULL) {
					setsendthread = 1;
					so->so_send_filt_thread =
					    current_thread();
				}
				socket_unlock(so, 0);
				unlocked = 1;
			}

			/* Call the filter */
			error = entry->sfe_filter->sf_filter.sf_data_out(
				entry->sfe_cookie, so, to, data, control, flags);

			/*
			 * Take the socket filter lock again
			 * and release the entry
			 */
			lck_rw_lock_shared(&sock_filter_lock);
			sflt_entry_release(entry);
		}
	}
	lck_rw_unlock_shared(&sock_filter_lock);

	if (unlocked) {
		socket_lock(so, 0);
		if (setsendthread) {
			so->so_send_filt_thread = NULL;
		}
	}

	return error;
}

__private_extern__ int
sflt_data_in(struct socket *so, const struct sockaddr *from, mbuf_t *data,
    mbuf_t *control, sflt_data_flag_t flags)
{
	if (so->so_filt == NULL || sflt_permission_check(sotoinpcb(so))) {
		return 0;
	}

	/* Socket-options are checked at the MPTCP-layer */
	if (so->so_flags & SOF_MP_SUBFLOW) {
		return 0;
	}

	struct socket_filter_entry *__single entry;
	int error = 0;
	int unlocked = 0;

	lck_rw_lock_shared(&sock_filter_lock);

	for (entry = so->so_filt; entry && (error == 0);
	    entry = entry->sfe_next_onsocket) {
		if ((entry->sfe_flags & SFEF_ATTACHED) &&
		    entry->sfe_filter->sf_filter.sf_data_in) {
			/*
			 * Retain the filter entry and
			 * release the socket filter lock
			 */
			sflt_entry_retain(entry);
			lck_rw_unlock_shared(&sock_filter_lock);

			/* If the socket isn't already unlocked, unlock it */
			if (unlocked == 0) {
				unlocked = 1;
				socket_unlock(so, 0);
			}

			/* Call the filter */
			error = entry->sfe_filter->sf_filter.sf_data_in(
				entry->sfe_cookie, so, from, data, control, flags);

			/*
			 * Take the socket filter lock again
			 * and release the entry
			 */
			lck_rw_lock_shared(&sock_filter_lock);
			sflt_entry_release(entry);
		}
	}
	lck_rw_unlock_shared(&sock_filter_lock);

	if (unlocked) {
		socket_lock(so, 0);
	}

	return error;
}

#pragma mark -- KPI --

errno_t
sflt_attach(socket_t socket, sflt_handle handle)
{
	socket_lock(socket, 1);
	errno_t result = sflt_attach_internal(socket, handle);
	socket_unlock(socket, 1);
	return result;
}

errno_t
sflt_detach(socket_t socket, sflt_handle handle)
{
	struct socket_filter_entry *__single entry;
	errno_t result = 0;

	if (socket == NULL || handle == 0) {
		return EINVAL;
	}

	lck_rw_lock_exclusive(&sock_filter_lock);
	for (entry = socket->so_filt; entry; entry = entry->sfe_next_onsocket) {
		if (entry->sfe_filter->sf_filter.sf_handle == handle &&
		    (entry->sfe_flags & SFEF_ATTACHED) != 0) {
			break;
		}
	}

	if (entry != NULL) {
		sflt_detach_locked(entry);
	}
	lck_rw_unlock_exclusive(&sock_filter_lock);

	return result;
}

struct solist {
	struct solist *next;
	struct socket *so;
};

static errno_t
sflt_register_common(const struct sflt_filter *infilter, int domain, int type,
    int  protocol, bool is_internal)
{
	const struct sflt_filter *filter = (const struct sflt_filter *__indexable)infilter;
	struct socket_filter *__single sock_filt = NULL;
	struct socket_filter *__single match = NULL;
	int error = 0;
	struct protosw *__single pr;
	unsigned int len;
	struct socket *__single so;
	struct inpcb *__single inp;
	struct solist *__single solisthead = NULL, *__single solist = NULL;

	if ((domain != PF_INET) && (domain != PF_INET6)) {
		return ENOTSUP;
	}

	pr = pffindproto(domain, protocol, type);
	if (pr == NULL) {
		return ENOENT;
	}

	if (filter->sf_attach == NULL || filter->sf_detach == NULL ||
	    filter->sf_handle == 0 || filter->sf_name == NULL) {
		return EINVAL;
	}

	/* Allocate the socket filter */
	sock_filt = kalloc_type(struct socket_filter,
	    Z_WAITOK | Z_ZERO | Z_NOFAIL);

	/* Legacy sflt_filter length; current structure minus extended */
	len = sizeof(*filter) - sizeof(struct sflt_filter_ext);
	/*
	 * Include extended fields if filter defines SFLT_EXTENDED.
	 * We've zeroed out our internal sflt_filter placeholder,
	 * so any unused portion would have been taken care of.
	 */
	if (filter->sf_flags & SFLT_EXTENDED) {
		unsigned int ext_len = filter->sf_len;

		if (ext_len > sizeof(struct sflt_filter_ext)) {
			ext_len = sizeof(struct sflt_filter_ext);
		}

		len += ext_len;
	}
	bcopy(filter, &sock_filt->sf_filter, len);

	lck_rw_lock_exclusive(&sock_filter_lock);
	/* Look for an existing entry */
	TAILQ_FOREACH(match, &sock_filter_head, sf_global_next) {
		if (match->sf_filter.sf_handle ==
		    sock_filt->sf_filter.sf_handle) {
			break;
		}
	}

	/* Add the entry only if there was no existing entry */
	if (match == NULL) {
		TAILQ_INSERT_TAIL(&sock_filter_head, sock_filt, sf_global_next);
		if ((sock_filt->sf_filter.sf_flags & SFLT_GLOBAL) != 0) {
			TAILQ_INSERT_TAIL(&pr->pr_filter_head, sock_filt,
			    sf_protosw_next);
			sock_filt->sf_proto = pr;
		}
		os_ref_init(&sock_filt->sf_refcount, NULL);

		OSIncrementAtomic64(&net_api_stats.nas_sfltr_register_count);
		INC_ATOMIC_INT64_LIM(net_api_stats.nas_sfltr_register_total);
		if (is_internal) {
			sock_filt->sf_flags |= SFF_INTERNAL;
			OSIncrementAtomic64(&net_api_stats.nas_sfltr_register_os_count);
			INC_ATOMIC_INT64_LIM(net_api_stats.nas_sfltr_register_os_total);
		}
	}
#if SKYWALK
	if (kernel_is_macos_or_server()) {
		net_filter_event_mark(NET_FILTER_EVENT_SOCKET,
		    net_check_compatible_sfltr());
		net_filter_event_mark(NET_FILTER_EVENT_ALF,
		    net_check_compatible_alf());
		net_filter_event_mark(NET_FILTER_EVENT_PARENTAL_CONTROLS,
		    net_check_compatible_parental_controls());
	}
#endif /* SKYWALK */

	lck_rw_unlock_exclusive(&sock_filter_lock);

	if (match != NULL) {
		kfree_type(struct socket_filter, sock_filt);
		return EEXIST;
	}

	if (!(filter->sf_flags & SFLT_EXTENDED_REGISTRY)) {
		return error;
	}

	/*
	 * Setup the filter on the TCP and UDP sockets already created.
	 */
#define SOLIST_ADD(_so) do {                                            \
	solist->next = solisthead;                                      \
	sock_retain((_so));                                             \
	solist->so = (_so);                                             \
	solisthead = solist;                                            \
} while (0)
	if (protocol == IPPROTO_TCP) {
		lck_rw_lock_shared(&tcbinfo.ipi_lock);
		LIST_FOREACH(inp, tcbinfo.ipi_listhead, inp_list) {
			so = inp->inp_socket;
			if (so == NULL || (so->so_state & SS_DEFUNCT) ||
			    (!(so->so_flags & SOF_MP_SUBFLOW) &&
			    (so->so_state & SS_NOFDREF)) ||
			    !SOCK_CHECK_DOM(so, domain) ||
			    !SOCK_CHECK_TYPE(so, type)) {
				continue;
			}
			solist = kalloc_type(struct solist, Z_NOWAIT);
			if (!solist) {
				continue;
			}
			SOLIST_ADD(so);
		}
		lck_rw_done(&tcbinfo.ipi_lock);
	} else if (protocol == IPPROTO_UDP) {
		lck_rw_lock_shared(&udbinfo.ipi_lock);
		LIST_FOREACH(inp, udbinfo.ipi_listhead, inp_list) {
			so = inp->inp_socket;
			if (so == NULL || (so->so_state & SS_DEFUNCT) ||
			    (!(so->so_flags & SOF_MP_SUBFLOW) &&
			    (so->so_state & SS_NOFDREF)) ||
			    !SOCK_CHECK_DOM(so, domain) ||
			    !SOCK_CHECK_TYPE(so, type)) {
				continue;
			}
			solist = kalloc_type(struct solist, Z_NOWAIT);
			if (!solist) {
				continue;
			}
			SOLIST_ADD(so);
		}
		lck_rw_done(&udbinfo.ipi_lock);
	}
	/* XXX it's possible to walk the raw socket list as well */
#undef SOLIST_ADD

	while (solisthead) {
		sflt_handle handle = filter->sf_handle;

		so = solisthead->so;
		socket_lock(so, 0);
		sflt_initsock(so);
		if (so->so_state & SS_ISCONNECTING) {
			sflt_notify_after_register(so, sock_evt_connecting,
			    handle);
		} else if (so->so_state & SS_ISCONNECTED) {
			sflt_notify_after_register(so, sock_evt_connected,
			    handle);
		} else if ((so->so_state &
		    (SS_ISDISCONNECTING | SS_CANTRCVMORE | SS_CANTSENDMORE)) ==
		    (SS_ISDISCONNECTING | SS_CANTRCVMORE | SS_CANTSENDMORE)) {
			sflt_notify_after_register(so, sock_evt_disconnecting,
			    handle);
		} else if ((so->so_state &
		    (SS_CANTRCVMORE | SS_CANTSENDMORE | SS_ISDISCONNECTED)) ==
		    (SS_CANTRCVMORE | SS_CANTSENDMORE | SS_ISDISCONNECTED)) {
			sflt_notify_after_register(so, sock_evt_disconnected,
			    handle);
		} else if (so->so_state & SS_CANTSENDMORE) {
			sflt_notify_after_register(so, sock_evt_cantsendmore,
			    handle);
		} else if (so->so_state & SS_CANTRCVMORE) {
			sflt_notify_after_register(so, sock_evt_cantrecvmore,
			    handle);
		}
		socket_unlock(so, 0);
		/* XXX no easy way to post the sock_evt_closing event */
		sock_release(so);
		solist = solisthead;
		solisthead = solisthead->next;
		kfree_type(struct solist, solist);
	}

	return error;
}

errno_t
sflt_register_internal(const struct sflt_filter *filter, int domain, int type,
    int protocol)
{
	return sflt_register_common(filter, domain, type, protocol, true);
}

#define MAX_NUM_FRAMES 5

errno_t
sflt_register(const struct sflt_filter *filter, int domain, int type,
    int protocol)
{
	return sflt_register_common(filter, domain, type, protocol, false);
}

errno_t
sflt_unregister(sflt_handle handle)
{
	struct socket_filter *__single filter;
	lck_rw_lock_exclusive(&sock_filter_lock);

	/* Find the entry by the handle */
	TAILQ_FOREACH(filter, &sock_filter_head, sf_global_next) {
		if (filter->sf_filter.sf_handle == handle) {
			break;
		}
	}

	if (filter) {
		if (filter->sf_flags & SFF_INTERNAL) {
			VERIFY(OSDecrementAtomic64(&net_api_stats.nas_sfltr_register_os_count) > 0);
		}
		VERIFY(OSDecrementAtomic64(&net_api_stats.nas_sfltr_register_count) > 0);

		/* Remove it from the global list */
		TAILQ_REMOVE(&sock_filter_head, filter, sf_global_next);

		/* Remove it from the protosw list */
		if ((filter->sf_filter.sf_flags & SFLT_GLOBAL) != 0) {
			TAILQ_REMOVE(&filter->sf_proto->pr_filter_head,
			    filter, sf_protosw_next);
		}

		/* Detach from any sockets */
		struct socket_filter_entry *__single entry = NULL;

		for (entry = filter->sf_entry_head; entry;
		    entry = entry->sfe_next_onfilter) {
			sflt_detach_locked(entry);
		}

		/* Release the filter */
		sflt_release_locked(filter);
	}
#if SKYWALK
	if (kernel_is_macos_or_server()) {
		net_filter_event_mark(NET_FILTER_EVENT_SOCKET,
		    net_check_compatible_sfltr());
		net_filter_event_mark(NET_FILTER_EVENT_ALF,
		    net_check_compatible_alf());
		net_filter_event_mark(NET_FILTER_EVENT_PARENTAL_CONTROLS,
		    net_check_compatible_parental_controls());
	}
#endif /* SKYWALK */

	lck_rw_unlock_exclusive(&sock_filter_lock);

	if (filter == NULL) {
		return ENOENT;
	}

	return 0;
}

errno_t
sock_inject_data_in(socket_t so, const struct sockaddr *from, mbuf_t data,
    mbuf_t control, sflt_data_flag_t flags)
{
	int error = 0;

	if (so == NULL || data == NULL) {
		return EINVAL;
	}

	if (flags & sock_data_filt_flag_oob) {
		return ENOTSUP;
	}

	socket_lock(so, 1);

	/* reject if this is a subflow socket */
	if (so->so_flags & SOF_MP_SUBFLOW) {
		error = ENOTSUP;
		goto done;
	}

	if (from) {
		if (sbappendaddr(&so->so_rcv,
		    __DECONST_SA(from), data, control, NULL)) {
			sorwakeup(so);
		}
		goto done;
	}

	if (control) {
		if (sbappendcontrol(&so->so_rcv, data, control, NULL)) {
			sorwakeup(so);
		}
		goto done;
	}

	if (flags & sock_data_filt_flag_record) {
		if (control || from) {
			error = EINVAL;
			goto done;
		}
		if (sbappendrecord(&so->so_rcv, (struct mbuf *)data)) {
			sorwakeup(so);
		}
		goto done;
	}

	if (sbappend(&so->so_rcv, data)) {
		sorwakeup(so);
	}
done:
	socket_unlock(so, 1);
	return error;
}

errno_t
sock_inject_data_out(socket_t so, const struct sockaddr *to, mbuf_t data,
    mbuf_t control, sflt_data_flag_t flags)
{
	int sosendflags = 0;
	int error = 0;

	/* reject if this is a subflow socket */
	if (so->so_flags & SOF_MP_SUBFLOW) {
		return ENOTSUP;
	}

	if (flags & sock_data_filt_flag_oob) {
		sosendflags = MSG_OOB;
	}

#if SKYWALK
	sk_protect_t protect = sk_async_transmit_protect();
#endif /* SKYWALK */

	error = sosend(so, __DECONST_SA(to), NULL,
	    data, control, sosendflags);

#if SKYWALK
	sk_async_transmit_unprotect(protect);
#endif /* SKYWALK */

	return error;
}

sockopt_dir
sockopt_direction(sockopt_t sopt)
{
	return (sopt->sopt_dir == SOPT_GET) ? sockopt_get : sockopt_set;
}

int
sockopt_level(sockopt_t sopt)
{
	return sopt->sopt_level;
}

int
sockopt_name(sockopt_t sopt)
{
	return sopt->sopt_name;
}

size_t
sockopt_valsize(sockopt_t sopt)
{
	return sopt->sopt_valsize;
}

errno_t
sockopt_copyin(sockopt_t sopt, void *__sized_by(len) data, size_t len)
{
	return sooptcopyin(sopt, data, len, len);
}

errno_t
sockopt_copyout(sockopt_t sopt, void *__sized_by(len) data, size_t len)
{
	return sooptcopyout(sopt, data, len);
}

#if SKYWALK
static bool
net_check_compatible_sfltr(void)
{
	if (net_api_stats.nas_sfltr_register_count > net_api_stats.nas_sfltr_register_os_count) {
		return false;
	}
	return true;
}

bool
net_check_compatible_alf(void)
{
	int alf_perm;
	size_t len = sizeof(alf_perm);
	errno_t error;

	error = kernel_sysctlbyname("net.alf.perm", &alf_perm, &len, NULL, 0);
	if (error == 0) {
		if (alf_perm != 0) {
			return false;
		}
	}
	return true;
}

static bool
net_check_compatible_parental_controls(void)
{
	/*
	 * Assumes the first 4 OS socket filters are for ALF and additional
	 * OS filters are for Parental Controls web content filter
	 */
	if (net_api_stats.nas_sfltr_register_os_count > 4) {
		return false;
	}
	return true;
}
#endif /* SKYWALK */
