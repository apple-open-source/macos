/*
 * Copyright (c) 2016-2023 Apple Inc. All rights reserved.
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
 * Copyright (c) 1999 Michael Smith <msmith@freebsd.org>
 * All rights reserved.
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
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <kern/queue.h>
#include <kern/locks.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/eventhandler.h>
#include <sys/sysctl.h>
#include <sys/mcache.h> /* for VERIFY() */
#include <os/log.h>

int evh_debug = 0;

SYSCTL_NODE(_kern, OID_AUTO, eventhandler, CTLFLAG_RW | CTLFLAG_LOCKED,
    0, "Eventhandler");
SYSCTL_INT(_kern_eventhandler, OID_AUTO, debug, CTLFLAG_RW | CTLFLAG_LOCKED,
    &evh_debug, 0, "Eventhandler debug mode");

struct eventhandler_entry_arg eventhandler_entry_dummy_arg = { .ee_fm_uuid = { 0 }, .ee_fr_uuid = { 0 } };

/* List of 'slow' lists */
static struct eventhandler_lists_ctxt evthdlr_lists_ctxt_glb;
static LCK_GRP_DECLARE(eventhandler_mutex_grp, "eventhandler");

LCK_GRP_DECLARE(el_lock_grp, "eventhandler list");
LCK_ATTR_DECLARE(el_lock_attr, 0, 0);

struct eventhandler_entry_generic {
	struct eventhandler_entry       ee;
	void                           *func;
};

static struct eventhandler_list *_eventhandler_find_list(
	struct eventhandler_lists_ctxt *evthdlr_lists_ctxt, const char *name);

void
eventhandler_lists_ctxt_init(struct eventhandler_lists_ctxt *evthdlr_lists_ctxt)
{
	VERIFY(evthdlr_lists_ctxt != NULL);

	TAILQ_INIT(&evthdlr_lists_ctxt->eventhandler_lists);
	evthdlr_lists_ctxt->eventhandler_lists_initted = 1;
	lck_mtx_init(&evthdlr_lists_ctxt->eventhandler_mutex,
	    &eventhandler_mutex_grp, LCK_ATTR_NULL);
}

/*
 * Initialize the eventhandler list.
 */
void
eventhandler_init(void)
{
	evhlog(debug, "%s: init", __func__);
	eventhandler_lists_ctxt_init(&evthdlr_lists_ctxt_glb);
}

/*
 * Insertion is O(n) due to the priority scan, but optimises to O(1)
 * if all priorities are identical.
 */
static eventhandler_tag
eventhandler_register_internal(
	struct eventhandler_lists_ctxt *evthdlr_lists_ctxt,
	struct eventhandler_list *list,
	const char *name, eventhandler_tag epn)
{
	struct eventhandler_list                *__single new_list;
	struct eventhandler_entry               *__single ep;

	VERIFY(strlen(name) <= (sizeof(new_list->el_name) - 1));

	if (evthdlr_lists_ctxt == NULL) {
		evthdlr_lists_ctxt = &evthdlr_lists_ctxt_glb;
	}

	VERIFY(evthdlr_lists_ctxt->eventhandler_lists_initted); /* eventhandler registered too early */
	VERIFY(epn != NULL); /* cannot register NULL event */

	evhlog(debug, "%s: registering event_type=%s\n", __func__, name);

	/* lock the eventhandler lists */
	lck_mtx_lock_spin(&evthdlr_lists_ctxt->eventhandler_mutex);

	/* Do we need to find/create the (slow) list? */
	if (list == NULL) {
		/* look for a matching, existing list */
		list = _eventhandler_find_list(evthdlr_lists_ctxt, name);

		/* Do we need to create the list? */
		if (list == NULL) {
			lck_mtx_convert_spin(&evthdlr_lists_ctxt->eventhandler_mutex);
			new_list = kalloc_type(struct eventhandler_list, Z_WAITOK_ZERO);
			evhlog2(debug, "%s: creating list \"%s\"", __func__, name);
			list = new_list;
			list->el_flags = 0;
			list->el_runcount = 0;
			bzero(&list->el_lock, sizeof(list->el_lock));
			(void) snprintf(list->el_name, sizeof(list->el_name), "%s", name);
			TAILQ_INSERT_HEAD(&evthdlr_lists_ctxt->eventhandler_lists, list, el_link);
		}
	}
	if (!(list->el_flags & EHL_INITTED)) {
		TAILQ_INIT(&list->el_entries);
		EHL_LOCK_INIT(list);
		list->el_flags |= EHL_INITTED;
	}
	lck_mtx_unlock(&evthdlr_lists_ctxt->eventhandler_mutex);

	KASSERT(epn->ee_priority != EHE_DEAD_PRIORITY,
	    ("%s: handler for %s registered with dead priority", __func__, name));

	/* sort it into the list */
	evhlog2(debug, "%s: adding item %p (function %p to \"%s\"", __func__, (void *)VM_KERNEL_ADDRPERM(epn),
	    (void *)VM_KERNEL_UNSLIDE(((struct eventhandler_entry_generic *)epn)->func), name);
	EHL_LOCK(list);
	TAILQ_FOREACH(ep, &list->el_entries, ee_link) {
		if (ep->ee_priority != EHE_DEAD_PRIORITY &&
		    epn->ee_priority < ep->ee_priority) {
			TAILQ_INSERT_BEFORE(ep, epn, ee_link);
			break;
		}
	}
	if (ep == NULL) {
		TAILQ_INSERT_TAIL(&list->el_entries, epn, ee_link);
	}
	EHL_UNLOCK(list);
	return epn;
}

eventhandler_tag
eventhandler_register(struct eventhandler_lists_ctxt *evthdlr_lists_ctxt,
    struct eventhandler_list *list, const char *name,
    void *func, struct eventhandler_entry_arg arg, int priority)
{
	struct eventhandler_entry_generic       *__single eg;

	/* allocate an entry for this handler, populate it */
	eg = kalloc_type(struct eventhandler_entry_generic, Z_WAITOK_ZERO);
	eg->func = func;
	eg->ee.ee_arg = arg;
	eg->ee.ee_priority = priority;

	return eventhandler_register_internal(evthdlr_lists_ctxt, list, name, &eg->ee);
}

void
eventhandler_deregister(struct eventhandler_list *list, eventhandler_tag tag)
{
	struct eventhandler_entry       *__single ep = tag;

	EHL_LOCK_ASSERT(list, LCK_MTX_ASSERT_OWNED);
	if (ep != NULL) {
		/* remove just this entry */
		if (list->el_runcount == 0) {
			evhlog2(debug, "%s: removing item %p from \"%s\"", __func__, (void *)VM_KERNEL_ADDRPERM(ep),
			    list->el_name);
			/*
			 * We may have purged the list because of certain events.
			 * Make sure that is not the case when a specific entry
			 * is being removed.
			 */
			if (!TAILQ_EMPTY(&list->el_entries)) {
				TAILQ_REMOVE(&list->el_entries, ep, ee_link);
			}
			EHL_LOCK_CONVERT(list);
			kfree_type(struct eventhandler_entry, ep);
		} else {
			evhlog2(debug, "%s: marking item %p from \"%s\" as dead", __func__,
			    (void *)VM_KERNEL_ADDRPERM(ep), list->el_name);
			ep->ee_priority = EHE_DEAD_PRIORITY;
		}
	} else {
		/* remove entire list */
		if (list->el_runcount == 0) {
			evhlog2(debug, "%s: removing all items from \"%s\"", __func__,
			    list->el_name);
			EHL_LOCK_CONVERT(list);
			while (!TAILQ_EMPTY(&list->el_entries)) {
				ep = TAILQ_FIRST(&list->el_entries);
				TAILQ_REMOVE(&list->el_entries, ep, ee_link);
				kfree_type(struct eventhandler_entry, ep);
			}
		} else {
			evhlog2(debug, "%s: marking all items from \"%s\" as dead",
			    __func__, list->el_name);
			TAILQ_FOREACH(ep, &list->el_entries, ee_link)
			ep->ee_priority = EHE_DEAD_PRIORITY;
		}
	}
	while (list->el_runcount > 0) {
		msleep((caddr_t)list, &list->el_lock, PSPIN, "evhrm", 0);
	}
	EHL_UNLOCK(list);
}

/*
 * Internal version for use when eventhandler list is already locked.
 */
static struct eventhandler_list *
_eventhandler_find_list(struct eventhandler_lists_ctxt *evthdlr_lists_ctxt,
    const char *name)
{
	struct eventhandler_list        *__single list;

	VERIFY(evthdlr_lists_ctxt != NULL);

	LCK_MTX_ASSERT(&evthdlr_lists_ctxt->eventhandler_mutex, LCK_MTX_ASSERT_OWNED);
	TAILQ_FOREACH(list, &evthdlr_lists_ctxt->eventhandler_lists, el_link) {
		if (!strlcmp(list->el_name, name, EVENTHANDLER_MAX_NAME)) {
			break;
		}
	}
	return list;
}

/*
 * Lookup a "slow" list by name.  Returns with the list locked.
 */
struct eventhandler_list *
eventhandler_find_list(struct eventhandler_lists_ctxt *evthdlr_lists_ctxt,
    const char *name)
{
	struct eventhandler_list        *__single list;

	if (evthdlr_lists_ctxt == NULL) {
		evthdlr_lists_ctxt = &evthdlr_lists_ctxt_glb;
	}

	if (!evthdlr_lists_ctxt->eventhandler_lists_initted) {
		return NULL;
	}

	/* scan looking for the requested list */
	lck_mtx_lock_spin(&evthdlr_lists_ctxt->eventhandler_mutex);
	list = _eventhandler_find_list(evthdlr_lists_ctxt, name);
	if (list != NULL) {
		lck_mtx_convert_spin(&evthdlr_lists_ctxt->eventhandler_mutex);
		EHL_LOCK_SPIN(list);
	}
	lck_mtx_unlock(&evthdlr_lists_ctxt->eventhandler_mutex);

	return list;
}

/*
 * Prune "dead" entries from an eventhandler list.
 */
void
eventhandler_prune_list(struct eventhandler_list *list)
{
	struct eventhandler_entry *__single ep, *__single en;

	int pruned = 0;

	evhlog2(debug, "%s: pruning list \"%s\"", __func__, list->el_name);
	EHL_LOCK_ASSERT(list, LCK_MTX_ASSERT_OWNED);
	TAILQ_FOREACH_SAFE(ep, &list->el_entries, ee_link, en) {
		if (ep->ee_priority == EHE_DEAD_PRIORITY) {
			TAILQ_REMOVE(&list->el_entries, ep, ee_link);
			kfree_type(struct eventhandler_entry, ep);
			pruned++;
		}
	}
	if (pruned > 0) {
		wakeup(list);
	}
}

/*
 * This should be called when last reference to an object
 * is being released.
 * The individual event type lists must be purged when the object
 * becomes defunct.
 */
void
eventhandler_lists_ctxt_destroy(struct eventhandler_lists_ctxt *evthdlr_lists_ctxt)
{
	struct eventhandler_list        *__single list = NULL;
	struct eventhandler_list        *__single list_next = NULL;

	lck_mtx_lock(&evthdlr_lists_ctxt->eventhandler_mutex);
	TAILQ_FOREACH_SAFE(list, &evthdlr_lists_ctxt->eventhandler_lists,
	    el_link, list_next) {
		VERIFY(TAILQ_EMPTY(&list->el_entries));
		EHL_LOCK_DESTROY(list);
		kfree_type(struct eventhandler_list, list);
	}
	lck_mtx_unlock(&evthdlr_lists_ctxt->eventhandler_mutex);
	lck_mtx_destroy(&evthdlr_lists_ctxt->eventhandler_mutex,
	    &eventhandler_mutex_grp);
	return;
}
