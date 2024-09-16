/*
 * Copyright (c) 2000-2019 Apple Inc. All rights reserved.
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
 *
 *
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Karels at Berkeley Software Design, Inc.
 *
 * Quite extensively rewritten by Poul-Henning Kamp of the FreeBSD
 * project, to make these variables more userfriendly.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)kern_sysctl.c	8.4 (Berkeley) 4/14/94
 */


#include <kern/counter.h>
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/proc_internal.h>
#include <sys/kauth.h>
#include <sys/systm.h>
#include <sys/sysproto.h>

#include <vm/vm_pageout_xnu.h>

#include <os/atomic_private.h>

#include <security/audit/audit.h>
#include <pexpert/pexpert.h>

#include <IOKit/IOBSD.h>

#if CONFIG_MACF
#include <security/mac_framework.h>
#endif

#if defined(HAS_APPLE_PAC)
#include <os/hash.h>
#include <ptrauth.h>
#endif /* defined(HAS_APPLE_PAC) */

#include <libkern/coreanalytics/coreanalytics.h>

#if DEBUG || DEVELOPMENT
#include <os/system_event_log.h>
#endif /* DEBUG || DEVELOPMENT */

static LCK_GRP_DECLARE(sysctl_lock_group, "sysctl");
static LCK_RW_DECLARE(sysctl_geometry_lock, &sysctl_lock_group);
static LCK_MTX_DECLARE(sysctl_unlocked_node_lock, &sysctl_lock_group);

/*
 * Conditionally allow dtrace to see these functions for debugging purposes.
 */
#ifdef STATIC
#undef STATIC
#endif
#if 0
#define STATIC
#else
#define STATIC static
#endif

/* forward declarations  of static functions */
STATIC void sysctl_sysctl_debug_dump_node(struct sysctl_oid_list *l, int i);
STATIC int sysctl_sysctl_debug(struct sysctl_oid *oidp, void *arg1,
    int arg2, struct sysctl_req *req);
STATIC int sysctl_sysctl_name(struct sysctl_oid *oidp, void *arg1,
    int arg2, struct sysctl_req *req);
STATIC int sysctl_sysctl_next_ls(struct sysctl_oid_list *lsp,
    int *name, u_int namelen, int *next, int *len, int level,
    struct sysctl_oid **oidpp);
STATIC int sysctl_old_kernel(struct sysctl_req *req, const void *p, size_t l);
STATIC int sysctl_new_kernel(struct sysctl_req *req, void *p, size_t l);
STATIC int name2oid(char *name, int *oid, size_t *len);
STATIC int sysctl_sysctl_name2oid(struct sysctl_oid *oidp, void *arg1, int arg2, struct sysctl_req *req);
STATIC int sysctl_sysctl_next(struct sysctl_oid *oidp, void *arg1, int arg2,
    struct sysctl_req *req);
STATIC int sysctl_sysctl_oidfmt(struct sysctl_oid *oidp, void *arg1, int arg2, struct sysctl_req *req);
STATIC int sysctl_old_user(struct sysctl_req *req, const void *p, size_t l);
STATIC int sysctl_new_user(struct sysctl_req *req, void *p, size_t l);

STATIC void sysctl_create_user_req(struct sysctl_req *req, struct proc *p, user_addr_t oldp,
    size_t oldlen, user_addr_t newp, size_t newlen);
STATIC int sysctl_root(boolean_t from_kernel, boolean_t string_is_canonical, char *namestring, size_t namestringlen, int *name, size_t namelen, struct sysctl_req *req);

int     kernel_sysctl(struct proc *p, int *name, size_t namelen, void *old, size_t *oldlenp, void *new, size_t newlen);
int     kernel_sysctlbyname(const char *name, void *oldp, size_t *oldlenp, void *newp, size_t newlen);
int     userland_sysctl(boolean_t string_is_canonical,
    char *namestring, size_t namestringlen,
    int *name, u_int namelen, struct sysctl_req *req,
    size_t *retval);

SECURITY_READ_ONLY_LATE(struct sysctl_oid_list) sysctl__children; /* root list */
__SYSCTL_EXTENSION_NODE();

/*
 * Initialization of the MIB tree.
 *
 * Order by number in each list.
 */

static void
sysctl_register_oid_locked(struct sysctl_oid *new_oidp,
    struct sysctl_oid *oidp)
{
	struct sysctl_oid_list *parent = new_oidp->oid_parent;
	struct sysctl_oid_list *parent_rw = NULL;
	struct sysctl_oid *p, **prevp;

	p = SLIST_FIRST(parent);
	if (p && p->oid_number == OID_MUTABLE_ANCHOR) {
		parent_rw = p->oid_arg1;
	}

	if (oidp->oid_number == OID_AUTO) {
		int n = OID_AUTO_START;

		/*
		 * If this oid has a number OID_AUTO, give it a number which
		 * is greater than any current oid.  Make sure it is at least
		 * OID_AUTO_START to leave space for pre-assigned oid numbers.
		 */

		SLIST_FOREACH_PREVPTR(p, prevp, parent, oid_link) {
			if (p->oid_number >= n) {
				n = p->oid_number + 1;
			}
		}

		if (parent_rw) {
			SLIST_FOREACH_PREVPTR(p, prevp, parent_rw, oid_link) {
				if (p->oid_number >= n) {
					n = p->oid_number + 1;
				}
			}
		}

		/*
		 * Reflect the number in an allocated OID into the template
		 * of the caller for sysctl_unregister_oid() compares.
		 */
		oidp->oid_number = new_oidp->oid_number = n;
	} else {
		/*
		 * Insert the oid into the parent's list in order.
		 */
		SLIST_FOREACH_PREVPTR(p, prevp, parent, oid_link) {
			if (oidp->oid_number == p->oid_number) {
				panic("attempting to register a sysctl at previously registered slot : %d",
				    oidp->oid_number);
			} else if (oidp->oid_number < p->oid_number) {
				break;
			}
		}

		if (parent_rw) {
			SLIST_FOREACH_PREVPTR(p, prevp, parent_rw, oid_link) {
				if (oidp->oid_number == p->oid_number) {
					panic("attempting to register a sysctl at previously registered slot : %d",
					    oidp->oid_number);
				} else if (oidp->oid_number < p->oid_number) {
					break;
				}
			}
		}
	}

#if defined(HAS_APPLE_PAC)
	if (oidp->oid_handler) {
		/*
		 * Sign oid_handler address-discriminated upon installation to make it
		 * harder to replace with an arbitrary function pointer.  Blend with
		 * a hash of oid_arg1 for robustness against memory corruption.
		 */
		oidp->oid_handler = ptrauth_auth_and_resign(oidp->oid_handler,
		    ptrauth_key_function_pointer,
		    ptrauth_function_pointer_type_discriminator(typeof(oidp->oid_handler)),
		    ptrauth_key_function_pointer,
		    ptrauth_blend_discriminator(&oidp->oid_handler,
		    os_hash_kernel_pointer(oidp->oid_arg1)));
	}
#endif /* defined(HAS_APPLE_PAC) */

	SLIST_NEXT(oidp, oid_link) = *prevp;
	*prevp = oidp;
}

void
sysctl_register_oid(struct sysctl_oid *new_oidp)
{
	struct sysctl_oid *oidp;

	if (new_oidp->oid_number < OID_AUTO) {
		panic("trying to register a node %p with an invalid oid_number: %d",
		    new_oidp, new_oidp->oid_number);
	}
	if (new_oidp->oid_kind & CTLFLAG_PERMANENT) {
		panic("Use sysctl_register_oid_early to register permanent nodes");
	}

	/*
	 * The OID can be old-style (needs copy), new style without an earlier
	 * version (also needs copy), or new style with a matching version (no
	 * copy needed).  Later versions are rejected (presumably, the OID
	 * structure was changed for a necessary reason).
	 */
	if (!(new_oidp->oid_kind & CTLFLAG_OID2)) {
#if __x86_64__
		oidp = kalloc_type(struct sysctl_oid, Z_WAITOK | Z_ZERO | Z_NOFAIL);
		/*
		 * Copy the structure only through the oid_fmt field, which
		 * is the last field in a non-OID2 OID structure.
		 *
		 * Note:	We may want to set the oid_descr to the
		 *		oid_name (or "") at some future date.
		 */
		memcpy(oidp, new_oidp, offsetof(struct sysctl_oid, oid_descr));
#else
		panic("Old style sysctl without a version number isn't supported");
#endif
	} else {
		/* It's a later version; handle the versions we know about */
		switch (new_oidp->oid_version) {
		case SYSCTL_OID_VERSION:
			/* current version */
			oidp = new_oidp;
			break;
		default:
			return;                 /* rejects unknown version */
		}
	}

	lck_rw_lock_exclusive(&sysctl_geometry_lock);
	sysctl_register_oid_locked(new_oidp, oidp);
	lck_rw_unlock_exclusive(&sysctl_geometry_lock);
}

__startup_func
void
sysctl_register_oid_early(struct sysctl_oid *oidp)
{
	assert((oidp->oid_kind & CTLFLAG_OID2) &&
	    (oidp->oid_kind & CTLFLAG_PERMANENT) &&
	    oidp->oid_version == SYSCTL_OID_VERSION);
	assert(startup_phase < STARTUP_SUB_SYSCTL);

	/*
	 * Clear the flag so that callers can use sysctl_register_oid_early
	 * again if they wish to register their node.
	 */
	if (oidp->oid_kind & CTLFLAG_NOAUTO) {
		oidp->oid_kind &= ~CTLFLAG_NOAUTO;
		return;
	}

	sysctl_register_oid_locked(oidp, oidp);
}

void
sysctl_unregister_oid(struct sysctl_oid *oidp)
{
	struct sysctl_oid *removed_oidp = NULL; /* OID removed from tree */
#if __x86_64__
	struct sysctl_oid *old_oidp = NULL;     /* OID compatibility copy */
#endif
	struct sysctl_oid_list *lsp;

	/* Get the write lock to modify the geometry */
	lck_rw_lock_exclusive(&sysctl_geometry_lock);

	lsp = oidp->oid_parent;
	if (SLIST_FIRST(lsp) && SLIST_FIRST(lsp)->oid_number == OID_MUTABLE_ANCHOR) {
		lsp = SLIST_FIRST(lsp)->oid_arg1;
	}

	if (oidp->oid_kind & CTLFLAG_PERMANENT) {
		panic("Trying to unregister permanent sysctl %p", oidp);
	}

	if (!(oidp->oid_kind & CTLFLAG_OID2)) {
#if __x86_64__
		/*
		 * We're using a copy so we can get the new fields in an
		 * old structure, so we have to iterate to compare the
		 * partial structure; when we find a match, we remove it
		 * normally and free the memory.
		 */
		SLIST_FOREACH(old_oidp, lsp, oid_link) {
			if (!memcmp(&oidp->oid_number, &old_oidp->oid_number, (offsetof(struct sysctl_oid, oid_descr) - offsetof(struct sysctl_oid, oid_number)))) {
				break;
			}
		}
		if (old_oidp != NULL) {
			SLIST_REMOVE(lsp, old_oidp, sysctl_oid, oid_link);
			removed_oidp = old_oidp;
		}
#else
		panic("Old style sysctl without a version number isn't supported");
#endif
	} else {
		/* It's a later version; handle the versions we know about */
		switch (oidp->oid_version) {
		case SYSCTL_OID_VERSION:
			/* We can just remove the OID directly... */
			SLIST_REMOVE(lsp, oidp, sysctl_oid, oid_link);
			removed_oidp = oidp;
			break;
		default:
			/* XXX: Can't happen; probably tree coruption.*/
			break;                  /* rejects unknown version */
		}
	}

#if defined(HAS_APPLE_PAC)
	if (removed_oidp && removed_oidp->oid_handler) {
		/*
		 * Revert address-discriminated signing performed by
		 * sysctl_register_oid() (in case this oid is registered again).
		 */
		removed_oidp->oid_handler = ptrauth_auth_and_resign(removed_oidp->oid_handler,
		    ptrauth_key_function_pointer,
		    ptrauth_blend_discriminator(&removed_oidp->oid_handler,
		    os_hash_kernel_pointer(removed_oidp->oid_arg1)),
		    ptrauth_key_function_pointer,
		    ptrauth_function_pointer_type_discriminator(typeof(removed_oidp->oid_handler)));
	}
#endif /* defined(HAS_APPLE_PAC) */

	/*
	 * We've removed it from the list at this point, but we don't want
	 * to return to the caller until all handler references have drained
	 * out.  Doing things in this order prevent other people coming in
	 * and starting new operations against the OID node we want removed.
	 *
	 * Note:	oidp could be NULL if it wasn't found.
	 */
	while (removed_oidp && removed_oidp->oid_refcnt) {
		lck_rw_sleep(&sysctl_geometry_lock, LCK_SLEEP_EXCLUSIVE,
		    &removed_oidp->oid_refcnt, THREAD_UNINT);
	}

	/* Release the write lock */
	lck_rw_unlock_exclusive(&sysctl_geometry_lock);

#if __x86_64__
	/* If it was allocated, free it after dropping the lock */
	kfree_type(struct sysctl_oid, old_oidp);
#endif
}

/*
 * Exported in BSDKernel.exports, kept for binary compatibility
 */
#if defined(__x86_64__)
void
sysctl_register_fixed(void)
{
}
#endif

/*
 * New handler interface
 *   If the sysctl caller (user mode or kernel mode) is interested in the
 *   value (req->oldptr != NULL), we copy the data (bigValue etc.) out,
 *   if the caller wants to set the value (req->newptr), we copy
 *   the data in (*pValue etc.).
 */

int
sysctl_io_number(struct sysctl_req *req, long long bigValue, size_t valueSize, void *pValue, int *changed)
{
	int             smallValue;
	int             error;

	if (changed) {
		*changed = 0;
	}

	/*
	 * Handle the various combinations of caller buffer size and
	 * data value size.  We are generous in the case where the
	 * caller has specified a 32-bit buffer but the value is 64-bit
	 * sized.
	 */

	/* 32 bit value expected or 32 bit buffer offered */
	if (((valueSize == sizeof(int)) ||
	    ((req->oldlen == sizeof(int)) && (valueSize == sizeof(long long))))
	    && (req->oldptr)) {
		smallValue = (int)bigValue;
		if ((long long)smallValue != bigValue) {
			return ERANGE;
		}
		error = SYSCTL_OUT(req, &smallValue, sizeof(smallValue));
	} else {
		/* any other case is either size-equal or a bug */
		error = SYSCTL_OUT(req, &bigValue, valueSize);
	}
	/* error or nothing to set */
	if (error || !req->newptr) {
		return error;
	}

	/* set request for constant */
	if (pValue == NULL) {
		return EPERM;
	}

	/* set request needs to convert? */
	if ((req->newlen == sizeof(int)) && (valueSize == sizeof(long long))) {
		/* new value is 32 bits, upconvert to 64 bits */
		error = SYSCTL_IN(req, &smallValue, sizeof(smallValue));
		if (!error) {
			*(long long *)pValue = (long long)smallValue;
		}
	} else if ((req->newlen == sizeof(long long)) && (valueSize == sizeof(int))) {
		/* new value is 64 bits, downconvert to 32 bits and range check */
		error = SYSCTL_IN(req, &bigValue, sizeof(bigValue));
		if (!error) {
			smallValue = (int)bigValue;
			if ((long long)smallValue != bigValue) {
				return ERANGE;
			}
			*(int *)pValue = smallValue;
		}
	} else {
		/* sizes match, just copy in */
		error = SYSCTL_IN(req, pValue, valueSize);
	}
	if (!error && changed) {
		*changed = 1;
	}
	return error;
}

int
sysctl_io_string(struct sysctl_req *req, char *pValue, size_t valueSize, int trunc, int *changed)
{
	int error;
	size_t len = strlen(pValue) + 1;

	if (changed) {
		*changed = 0;
	}

	if (trunc && req->oldptr && req->oldlen && (req->oldlen < len)) {
		/* If trunc != 0, if you give it a too small (but larger than
		 * 0 bytes) buffer, instead of returning ENOMEM, it truncates the
		 * returned string to the buffer size.  This preserves the semantics
		 * of some library routines implemented via sysctl, which truncate
		 * their returned data, rather than simply returning an error. The
		 * returned string is always nul (ascii '\0') terminated. */
		error = SYSCTL_OUT(req, pValue, req->oldlen - 1);
		if (!error) {
			char c = '\0';
			error = SYSCTL_OUT(req, &c, 1);
		}
	} else {
		/* Copy string out */
		error = SYSCTL_OUT(req, pValue, len);
	}

	/* error or no new value */
	if (error || !req->newptr) {
		return error;
	}

	/* attempt to set read-only value */
	if (valueSize == 0) {
		return EPERM;
	}

	/* make sure there's room for the new string */
	if (req->newlen >= valueSize) {
		return EINVAL;
	}

	/* copy the string in and force nul termination */
	error = SYSCTL_IN(req, pValue, req->newlen);
	pValue[req->newlen] = '\0';

	if (!error && changed) {
		*changed = 1;
	}
	return error;
}

int
sysctl_io_opaque(struct sysctl_req *req, void *pValue, size_t valueSize, int *changed)
{
	int error;

	if (changed) {
		*changed = 0;
	}

	/* Copy blob out */
	error = SYSCTL_OUT(req, pValue, valueSize);

	/* error or nothing to set */
	if (error || !req->newptr) {
		return error;
	}

	error = SYSCTL_IN(req, pValue, valueSize);

	if (!error && changed) {
		*changed = 1;
	}
	return error;
}

/*
 * SYSCTL_OID enumerators
 *
 * Because system OIDs are immutable, they are composed of 2 lists hanging from
 * a first dummy OID_MUTABLE_ANCHOR node that has an immutable list hanging from
 * its `oid_parent` field and a mutable list hanging from its oid_arg1 one.
 *
 * Those enumerators abstract away the implicit merging of those two lists in
 * two possible order:
 * - oid_number order (which will interleave both sorted lists)
 * - system order which will list the immutable list first,
 *   and the mutable list second.
 */
struct sysctl_oid_iterator {
	struct sysctl_oid *a;
	struct sysctl_oid *b;
};

static struct sysctl_oid_iterator
sysctl_oid_iterator_begin(struct sysctl_oid_list *l)
{
	struct sysctl_oid_iterator it = { };
	struct sysctl_oid *a = SLIST_FIRST(l);

	if (a == NULL) {
		return it;
	}

	if (a->oid_number == OID_MUTABLE_ANCHOR) {
		it.a = SLIST_NEXT(a, oid_link);
		it.b = SLIST_FIRST((struct sysctl_oid_list *)a->oid_arg1);
	} else {
		it.a = a;
	}
	return it;
}

static struct sysctl_oid *
sysctl_oid_iterator_next_num_order(struct sysctl_oid_iterator *it)
{
	struct sysctl_oid *a = it->a;
	struct sysctl_oid *b = it->b;

	if (a == NULL && b == NULL) {
		return NULL;
	}

	if (a == NULL) {
		it->b = SLIST_NEXT(b, oid_link);
		return b;
	}

	if (b == NULL || a->oid_number <= b->oid_number) {
		it->a = SLIST_NEXT(a, oid_link);
		return a;
	}

	it->b = SLIST_NEXT(b, oid_link);
	return b;
}

#define SYSCTL_OID_FOREACH_NUM_ORDER(oidp, l) \
	for (struct sysctl_oid_iterator it = sysctl_oid_iterator_begin(l); \
	        ((oidp) = sysctl_oid_iterator_next_num_order(&it)); )

static struct sysctl_oid *
sysctl_oid_iterator_next_system_order(struct sysctl_oid_iterator *it)
{
	struct sysctl_oid *a = it->a;
	struct sysctl_oid *b = it->b;

	if (a) {
		it->a = SLIST_NEXT(a, oid_link);
		return a;
	}

	if (b) {
		it->b = SLIST_NEXT(b, oid_link);
		return b;
	}

	return NULL;
}

#define SYSCTL_OID_FOREACH_SYS_ORDER(oidp, l) \
	for (struct sysctl_oid_iterator it = sysctl_oid_iterator_begin(l); \
	        ((oidp) = sysctl_oid_iterator_next_system_order(&it)); )

/*
 * "Staff-functions"
 *
 * These functions implement a presently undocumented interface
 * used by the sysctl program to walk the tree, and get the type
 * so it can print the value.
 * This interface is under work and consideration, and should probably
 * be killed with a big axe by the first person who can find the time.
 * (be aware though, that the proper interface isn't as obvious as it
 * may seem, there are various conflicting requirements.
 *
 * {0,0}	printf the entire MIB-tree.
 * {0,1,...}	return the name of the "..." OID.
 * {0,2,...}	return the next OID.
 * {0,3}	return the OID of the name in "new"
 * {0,4,...}	return the kind & format info for the "..." OID.
 */

/*
 * sysctl_sysctl_debug_dump_node
 *
 * Description:	Dump debug information for a given sysctl_oid_list at the
 *		given oid depth out to the kernel log, via printf
 *
 * Parameters:	l				sysctl_oid_list pointer
 *		i				current node depth
 *
 * Returns:	(void)
 *
 * Implicit:	kernel log, modified
 *
 * Locks:	Assumes sysctl_geometry_lock is held prior to calling
 *
 * Notes:	This function may call itself recursively to resolve Node
 *		values, which potentially have an inferioer sysctl_oid_list
 *
 *		This function is only callable indirectly via the function
 *		sysctl_sysctl_debug()
 *
 * Bugs:	The node depth indentation does not work; this may be an
 *		artifact of leading space removal by the log daemon itself
 *		or some intermediate routine.
 */
STATIC void
sysctl_sysctl_debug_dump_node(struct sysctl_oid_list *l, int i)
{
	struct sysctl_oid *oidp;
	struct sysctl_oid_list *lp;
	const char *what;

	SYSCTL_OID_FOREACH_SYS_ORDER(oidp, l) {
		switch (oidp->oid_kind & CTLTYPE) {
		case CTLTYPE_NODE:
			lp = oidp->oid_arg1;
			what = "Node   ";
			if (lp && SLIST_FIRST(lp) &&
			    SLIST_FIRST(lp)->oid_number == OID_MUTABLE_ANCHOR) {
				what = "NodeExt";
			} else {
			}
			break;
		case CTLTYPE_INT:
			what = "Int    ";
			break;
		case CTLTYPE_STRING:
			what = "String ";
			break;
		case CTLTYPE_QUAD:
			what = "Quad   ";
			break;
		case CTLTYPE_OPAQUE:
			what = "Opaque ";
			break;
		default:
			what = "Unknown";
			break;
		}

		printf("%*s%-3d[%c%c%c%c%c] %s %s\n", i, "", oidp->oid_number,
		    oidp->oid_kind & CTLFLAG_LOCKED ? 'L':' ',
		    oidp->oid_kind & CTLFLAG_RD ? 'R':' ',
		    oidp->oid_kind & CTLFLAG_WR ? 'W':' ',
		    oidp->oid_kind & CTLFLAG_PERMANENT ? ' ':'*',
		    oidp->oid_handler ? 'h' : ' ',
		    what, oidp->oid_name);

		if ((oidp->oid_kind & CTLTYPE) == CTLTYPE_NODE) {
			if (!oidp->oid_handler) {
				sysctl_sysctl_debug_dump_node(lp, i + 2);
			}
		}
	}
}

/*
 * sysctl_sysctl_debug
 *
 * Description:	This function implements the "sysctl.debug" portion of the
 *		OID space for sysctl.
 *
 * OID:		0, 0
 *
 * Parameters:	__unused
 *
 * Returns:	ENOENT
 *
 * Implicit:	kernel log, modified
 *
 * Locks:	Acquires and then releases a read lock on the
 *		sysctl_geometry_lock
 */
STATIC int
sysctl_sysctl_debug(__unused struct sysctl_oid *oidp, __unused void *arg1,
    __unused int arg2, __unused struct sysctl_req *req)
{
	lck_rw_lock_shared(&sysctl_geometry_lock);
	sysctl_sysctl_debug_dump_node(&sysctl__children, 0);
	lck_rw_done(&sysctl_geometry_lock);
	return ENOENT;
}

SYSCTL_PROC(_sysctl, 0, debug, CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_LOCKED,
    0, 0, sysctl_sysctl_debug, "-", "");

/*
 * sysctl_sysctl_name
 *
 * Description:	Convert an OID into a string name; this is used by the user
 *		space sysctl() command line utility; this is done in a purely
 *		advisory capacity (e.g. to provide node names for "sysctl -A"
 *		output).
 *
 * OID:		0, 1
 *
 * Parameters:	oidp				__unused
 *		arg1				A pointer to the OID name list
 *						integer array, beginning at
 *						adjusted option base 2
 *		arg2				The number of elements which
 *						remain in the name array
 *
 * Returns:	0				Success
 *	SYSCTL_OUT:EPERM			Permission denied
 *	SYSCTL_OUT:EFAULT			Bad user supplied buffer
 *	SYSCTL_OUT:???				Return value from user function
 *						for SYSCTL_PROC leaf node
 *
 * Implict:	Contents of user request buffer, modified
 *
 * Locks:	Acquires and then releases a read lock on the
 *		sysctl_geometry_lock
 *
 * Notes:	SPI (System Programming Interface); this is subject to change
 *		and may not be relied upon by third party applications; use
 *		a subprocess to communicate with the "sysctl" command line
 *		command instead, if you believe you need this functionality.
 *		Preferrably, use sysctlbyname() instead.
 *
 *		Setting of the NULL termination of the output string is
 *		delayed until after the geometry lock is dropped.  If there
 *		are no Entries remaining in the OID name list when this
 *		function is called, it will still write out the termination
 *		byte.
 *
 *		This function differs from other sysctl functions in that
 *		it can not take an output buffer length of 0 to determine the
 *		space which will be required.  It is suggested that the buffer
 *		length be PATH_MAX, and that authors of new sysctl's refrain
 *		from exceeding this string length.
 */
STATIC int
sysctl_sysctl_name(__unused struct sysctl_oid *oidp, void *arg1, int arg2,
    struct sysctl_req *req)
{
	int *name = (int *) arg1;
	u_int namelen = arg2;
	int error = 0;
	struct sysctl_oid *oid;
	struct sysctl_oid_list *lsp = &sysctl__children, *lsp2;
	char tempbuf[10] = {};

	lck_rw_lock_shared(&sysctl_geometry_lock);
	while (namelen) {
		if (!lsp) {
			snprintf(tempbuf, sizeof(tempbuf), "%d", *name);
			if (req->oldidx) {
				error = SYSCTL_OUT(req, ".", 1);
			}
			if (!error) {
				error = SYSCTL_OUT(req, tempbuf, strlen(tempbuf));
			}
			if (error) {
				lck_rw_done(&sysctl_geometry_lock);
				return error;
			}
			namelen--;
			name++;
			continue;
		}
		lsp2 = 0;
		SYSCTL_OID_FOREACH_NUM_ORDER(oid, lsp) {
			if (oid->oid_number != *name) {
				continue;
			}

			if (req->oldidx) {
				error = SYSCTL_OUT(req, ".", 1);
			}
			if (!error) {
				error = SYSCTL_OUT(req, oid->oid_name,
				    strlen(oid->oid_name));
			}
			if (error) {
				lck_rw_done(&sysctl_geometry_lock);
				return error;
			}

			namelen--;
			name++;

			if ((oid->oid_kind & CTLTYPE) != CTLTYPE_NODE) {
				break;
			}

			if (oid->oid_handler) {
				break;
			}

			lsp2 = (struct sysctl_oid_list *)oid->oid_arg1;
			break;
		}
		lsp = lsp2;
	}
	lck_rw_done(&sysctl_geometry_lock);
	return SYSCTL_OUT(req, "", 1);
}

SYSCTL_NODE(_sysctl, 1, name, CTLFLAG_RD | CTLFLAG_LOCKED, sysctl_sysctl_name, "");

/*
 * sysctl_sysctl_next_ls
 *
 * Description:	For a given OID name value, return the next consecutive OID
 *		name value within the geometry tree
 *
 * Parameters:	lsp				The OID list to look in
 *		name				The OID name to start from
 *		namelen				The length of the OID name
 *		next				Pointer to new oid storage to
 *						fill in
 *		len				Pointer to receive new OID
 *						length value of storage written
 *		level				OID tree depth (used to compute
 *						len value)
 *		oidpp				Pointer to OID list entry
 *						pointer; used to walk the list
 *						forward across recursion
 *
 * Returns:	0				Returning a new entry
 *		1				End of geometry list reached
 *
 * Implicit:	*next				Modified to contain the new OID
 *		*len				Modified to contain new length
 *
 * Locks:	Assumes sysctl_geometry_lock is held prior to calling
 *
 * Notes:	This function will not return OID values that have special
 *		handlers, since we can not tell wheter these handlers consume
 *		elements from the OID space as parameters.  For this reason,
 *		we STRONGLY discourage these types of handlers
 */
STATIC int
sysctl_sysctl_next_ls(struct sysctl_oid_list *lsp, int *name, u_int namelen,
    int *next, int *len, int level, struct sysctl_oid **oidpp)
{
	struct sysctl_oid *oidp;

	*len = level;
	SYSCTL_OID_FOREACH_NUM_ORDER(oidp, lsp) {
		*next = oidp->oid_number;
		*oidpp = oidp;

		if (!namelen) {
			if ((oidp->oid_kind & CTLTYPE) != CTLTYPE_NODE) {
				return 0;
			}
			if (oidp->oid_handler) {
				/* We really should call the handler here...*/
				return 0;
			}
			lsp = (struct sysctl_oid_list *)oidp->oid_arg1;

			if (!SLIST_FIRST(lsp)) {
				/* This node had no children - skip it! */
				continue;
			}

			if (!sysctl_sysctl_next_ls(lsp, 0, 0, next + 1,
			    len, level + 1, oidpp)) {
				return 0;
			}
			goto next;
		}

		if (oidp->oid_number < *name) {
			continue;
		}

		if (oidp->oid_number > *name) {
			if ((oidp->oid_kind & CTLTYPE) != CTLTYPE_NODE) {
				return 0;
			}
			if (oidp->oid_handler) {
				return 0;
			}
			lsp = (struct sysctl_oid_list *)oidp->oid_arg1;
			if (!sysctl_sysctl_next_ls(lsp, name + 1, namelen - 1,
			    next + 1, len, level + 1, oidpp)) {
				return 0;
			}
			goto next;
		}
		if ((oidp->oid_kind & CTLTYPE) != CTLTYPE_NODE) {
			continue;
		}

		if (oidp->oid_handler) {
			continue;
		}

		lsp = (struct sysctl_oid_list *)oidp->oid_arg1;
		if (!sysctl_sysctl_next_ls(lsp, name + 1, namelen - 1, next + 1,
		    len, level + 1, oidpp)) {
			return 0;
		}
next:
		namelen = 1;
		*len = level;
	}
	return 1;
}

/*
 * sysctl_sysctl_next
 *
 * Description:	This is an iterator function designed to iterate the oid tree
 *		and provide a list of OIDs for use by the user space "sysctl"
 *		command line tool
 *
 * OID:		0, 2
 *
 * Parameters:	oidp				__unused
 *		arg1				Pointer to start OID name
 *		arg2				Start OID name length
 *		req				Pointer to user request buffer
 *
 * Returns:	0				Success
 *		ENOENT				Reached end of OID space
 *	SYSCTL_OUT:EPERM			Permission denied
 *	SYSCTL_OUT:EFAULT			Bad user supplied buffer
 *	SYSCTL_OUT:???				Return value from user function
 *						for SYSCTL_PROC leaf node
 *
 * Implict:	Contents of user request buffer, modified
 *
 * Locks:	Acquires and then releases a read lock on the
 *		sysctl_geometry_lock
 *
 * Notes:	SPI (System Programming Interface); this is subject to change
 *		and may not be relied upon by third party applications; use
 *		a subprocess to communicate with the "sysctl" command line
 *		command instead, if you believe you need this functionality.
 *		Preferrably, use sysctlbyname() instead.
 *
 *		This function differs from other sysctl functions in that
 *		it can not take an output buffer length of 0 to determine the
 *		space which will be required.  It is suggested that the buffer
 *		length be PATH_MAX, and that authors of new sysctl's refrain
 *		from exceeding this string length.
 */
STATIC int
sysctl_sysctl_next(__unused struct sysctl_oid *oidp, void *arg1, int arg2,
    struct sysctl_req *req)
{
	int *name = (int *) arg1;
	u_int namelen = arg2;
	int i, j, error;
	struct sysctl_oid *oid;
	struct sysctl_oid_list *lsp = &sysctl__children;
	int newoid[CTL_MAXNAME] = {};

	lck_rw_lock_shared(&sysctl_geometry_lock);
	i = sysctl_sysctl_next_ls(lsp, name, namelen, newoid, &j, 1, &oid);
	lck_rw_done(&sysctl_geometry_lock);
	if (i) {
		return ENOENT;
	}
	error = SYSCTL_OUT(req, newoid, j * sizeof(int));
	return error;
}

SYSCTL_NODE(_sysctl, 2, next, CTLFLAG_RD | CTLFLAG_LOCKED, sysctl_sysctl_next, "");

/*
 * name2oid
 *
 * Description:	Support function for use by sysctl_sysctl_name2oid(); looks
 *		up an OID name given a string name.
 *
 * Parameters:	name				NULL terminated string name
 *		oid				Pointer to receive OID name
 *		len				Pointer to receive OID length
 *						pointer value (see "Notes")
 *
 * Returns:	0				Success
 *		ENOENT				Entry not found
 *
 * Implicit:	*oid				Modified to contain OID value
 *		*len				Modified to contain OID length
 *
 * Locks:	Assumes sysctl_geometry_lock is held prior to calling
 */
STATIC int
name2oid(char *name, int *oid, size_t *len)
{
	struct sysctl_oid_iterator it;
	struct sysctl_oid *oidp;
	char *p;
	char i;

	if (!*name) {
		return ENOENT;
	}

	p = name + strlen(name) - 1;
	if (*p == '.') {
		*p = '\0';
	}

	*len = 0;

	for (p = name; *p && *p != '.'; p++) {
		;
	}
	i = *p;
	if (i == '.') {
		*p = '\0';
	}

	it = sysctl_oid_iterator_begin(&sysctl__children);
	oidp = sysctl_oid_iterator_next_system_order(&it);

	while (oidp && *len < CTL_MAXNAME) {
		if (strcmp(name, oidp->oid_name)) {
			oidp = sysctl_oid_iterator_next_system_order(&it);
			continue;
		}
		*oid++ = oidp->oid_number;
		(*len)++;

		if (i == '\0') {
			return 0;
		}

		if ((oidp->oid_kind & CTLTYPE) != CTLTYPE_NODE) {
			break;
		}

		if (oidp->oid_handler) {
			break;
		}

		it = sysctl_oid_iterator_begin(oidp->oid_arg1);
		oidp = sysctl_oid_iterator_next_system_order(&it);

		*p = i; /* restore */
		name = p + 1;
		for (p = name; *p && *p != '.'; p++) {
			;
		}
		i = *p;
		if (i == '.') {
			*p = '\0';
		}
	}
	return ENOENT;
}

/*
 * sysctl_sysctl_name2oid
 *
 * Description:	Translate a string name to an OID name value; this is used by
 *		the sysctlbyname() function as well as by the "sysctl" command
 *		line command.
 *
 * OID:		0, 3
 *
 * Parameters:	oidp				__unused
 *		arg1				__unused
 *		arg2				__unused
 *		req				Request structure
 *
 * Returns:	ENOENT				Input length too short
 *		ENAMETOOLONG			Input length too long
 *		ENOMEM				Could not allocate work area
 *	SYSCTL_IN/OUT:EPERM			Permission denied
 *	SYSCTL_IN/OUT:EFAULT			Bad user supplied buffer
 *	SYSCTL_IN/OUT:???			Return value from user function
 *	name2oid:ENOENT				Not found
 *
 * Implicit:	*req				Contents of request, modified
 *
 * Locks:	Acquires and then releases a read lock on the
 *		sysctl_geometry_lock
 *
 * Notes:	SPI (System Programming Interface); this is subject to change
 *		and may not be relied upon by third party applications; use
 *		a subprocess to communicate with the "sysctl" command line
 *		command instead, if you believe you need this functionality.
 *		Preferrably, use sysctlbyname() instead.
 *
 *		This function differs from other sysctl functions in that
 *		it can not take an output buffer length of 0 to determine the
 *		space which will be required.  It is suggested that the buffer
 *		length be PATH_MAX, and that authors of new sysctl's refrain
 *		from exceeding this string length.
 */
STATIC int
sysctl_sysctl_name2oid(__unused struct sysctl_oid *oidp, __unused void *arg1,
    __unused int arg2, struct sysctl_req *req)
{
	char *p;
	int error, oid[CTL_MAXNAME] = {};
	size_t len = 0;          /* set by name2oid() */

	if (req->newlen < 1) {
		return ENOENT;
	}
	if (req->newlen >= MAXPATHLEN) { /* XXX arbitrary, undocumented */
		return ENAMETOOLONG;
	}

	p = (char *)kalloc_data(req->newlen + 1, Z_WAITOK);
	if (!p) {
		return ENOMEM;
	}

	error = SYSCTL_IN(req, p, req->newlen);
	if (error) {
		kfree_data(p, req->newlen + 1);
		return error;
	}

	p[req->newlen] = '\0';

	/*
	 * Note:	We acquire and release the geometry lock here to
	 *		avoid making name2oid needlessly complex.
	 */
	lck_rw_lock_shared(&sysctl_geometry_lock);
	error = name2oid(p, oid, &len);
	lck_rw_done(&sysctl_geometry_lock);

	kfree_data(p, req->newlen + 1);

	if (error) {
		return error;
	}

	error = SYSCTL_OUT(req, oid, len * sizeof *oid);
	return error;
}

SYSCTL_PROC(_sysctl, 3, name2oid, CTLFLAG_RW | CTLFLAG_ANYBODY | CTLFLAG_KERN | CTLFLAG_LOCKED, 0, 0,
    sysctl_sysctl_name2oid, "I", "");

/*
 * sysctl_sysctl_oidfmt
 *
 * Description:	For a given OID name, determine the format of the data which
 *		is associated with it.  This is used by the "sysctl" command
 *		line command.
 *
 * OID:		0, 4
 *
 * Parameters:	oidp				__unused
 *		arg1				The OID name to look up
 *		arg2				The length of the OID name
 *		req				Pointer to user request buffer
 *
 * Returns:	0				Success
 *		EISDIR				Malformed request
 *		ENOENT				No such OID name
 *	SYSCTL_OUT:EPERM			Permission denied
 *	SYSCTL_OUT:EFAULT			Bad user supplied buffer
 *	SYSCTL_OUT:???				Return value from user function
 *
 * Implict:	Contents of user request buffer, modified
 *
 * Locks:	Acquires and then releases a read lock on the
 *		sysctl_geometry_lock
 *
 * Notes:	SPI (System Programming Interface); this is subject to change
 *		and may not be relied upon by third party applications; use
 *		a subprocess to communicate with the "sysctl" command line
 *		command instead, if you believe you need this functionality.
 *
 *		This function differs from other sysctl functions in that
 *		it can not take an output buffer length of 0 to determine the
 *		space which will be required.  It is suggested that the buffer
 *		length be PATH_MAX, and that authors of new sysctl's refrain
 *		from exceeding this string length.
 */
STATIC int
sysctl_sysctl_oidfmt(__unused struct sysctl_oid *oidp, void *arg1, int arg2,
    struct sysctl_req *req)
{
	int *name = (int *) arg1;
	int error = ENOENT;             /* default error: not found */
	u_int namelen = arg2;
	u_int indx;
	struct sysctl_oid_iterator it;
	struct sysctl_oid *oid;

	lck_rw_lock_shared(&sysctl_geometry_lock);

	it = sysctl_oid_iterator_begin(&sysctl__children);
	oid = sysctl_oid_iterator_next_system_order(&it);

	indx = 0;
	while (oid && indx < CTL_MAXNAME) {
		if (oid->oid_number == name[indx]) {
			indx++;
			if ((oid->oid_kind & CTLTYPE) == CTLTYPE_NODE) {
				if (oid->oid_handler) {
					goto found;
				}
				if (indx == namelen) {
					goto found;
				}
				it = sysctl_oid_iterator_begin(oid->oid_arg1);
				oid = sysctl_oid_iterator_next_system_order(&it);
			} else {
				if (indx != namelen) {
					error =  EISDIR;
					goto err;
				}
				goto found;
			}
		} else {
			oid = sysctl_oid_iterator_next_system_order(&it);
		}
	}
	/* Not found */
	goto err;

found:
	if (!oid->oid_fmt) {
		goto err;
	}
	error = SYSCTL_OUT(req,
	    &oid->oid_kind, sizeof(oid->oid_kind));
	if (!error) {
		error = SYSCTL_OUT(req, oid->oid_fmt,
		    strlen(oid->oid_fmt) + 1);
	}
err:
	lck_rw_done(&sysctl_geometry_lock);
	return error;
}

SYSCTL_NODE(_sysctl, 4, oidfmt, CTLFLAG_RD | CTLFLAG_LOCKED, sysctl_sysctl_oidfmt, "");


/*
 * Default "handler" functions.
 */

/*
 * Handle an int, signed or unsigned.
 * Two cases:
 *     a variable:  point arg1 at it.
 *     a constant:  pass it in arg2.
 */

int
sysctl_handle_int(__unused struct sysctl_oid *oidp, void *arg1, int arg2,
    struct sysctl_req *req)
{
	return sysctl_io_number(req, arg1? *(int*)arg1: arg2, sizeof(int), arg1, NULL);
}

/*
 * Handle a long, signed or unsigned.  arg1 points to it.
 */

int
sysctl_handle_long(__unused struct sysctl_oid *oidp, void *arg1,
    __unused int arg2, struct sysctl_req *req)
{
	if (!arg1) {
		return EINVAL;
	}
	return sysctl_io_number(req, *(long*)arg1, sizeof(long), arg1, NULL);
}

/*
 * Handle a quad, signed or unsigned.  arg1 points to it.
 */

int
sysctl_handle_quad(__unused struct sysctl_oid *oidp, void *arg1,
    __unused int arg2, struct sysctl_req *req)
{
	if (!arg1) {
		return EINVAL;
	}
	return sysctl_io_number(req, *(long long*)arg1, sizeof(long long), arg1, NULL);
}

/*
 * Expose an int value as a quad.
 *
 * This interface allows us to support interfaces defined
 * as using quad values while the implementation is still
 * using ints.
 */
int
sysctl_handle_int2quad(__unused struct sysctl_oid *oidp, void *arg1,
    __unused int arg2, struct sysctl_req *req)
{
	int error = 0;
	long long val;
	int newval;

	if (!arg1) {
		return EINVAL;
	}
	val = (long long)*(int *)arg1;
	error = SYSCTL_OUT(req, &val, sizeof(long long));

	if (error || !req->newptr) {
		return error;
	}

	error = SYSCTL_IN(req, &val, sizeof(long long));
	if (!error) {
		/*
		 * Value must be representable; check by
		 * casting and then casting back.
		 */
		newval = (int)val;
		if ((long long)newval != val) {
			error = ERANGE;
		} else {
			*(int *)arg1 = newval;
		}
	}
	return error;
}

/*
 * Handle our generic '\0' terminated 'C' string.
 * Two cases:
 *      a variable string:  point arg1 at it, arg2 is max length.
 *      a constant string:  point arg1 at it, arg2 is zero.
 */

int
sysctl_handle_string( __unused struct sysctl_oid *oidp, void *arg1, int arg2,
    struct sysctl_req *req)
{
	return sysctl_io_string(req, arg1, arg2, 0, NULL);
}

/*
 * Handle any kind of opaque data.
 * arg1 points to it, arg2 is the size.
 */

int
sysctl_handle_opaque(__unused struct sysctl_oid *oidp, void *arg1, int arg2,
    struct sysctl_req *req)
{
	return sysctl_io_opaque(req, arg1, arg2, NULL);
}

/*
 * Transfer functions to/from kernel space.
 */
STATIC int
sysctl_old_kernel(struct sysctl_req *req, const void *p, size_t l)
{
	size_t i = 0;

	if (req->oldptr) {
		i = l;
		if (i > req->oldlen - req->oldidx) {
			i = req->oldlen - req->oldidx;
		}
		if (i > 0) {
			bcopy((const void*)p, CAST_DOWN(char *, (req->oldptr + req->oldidx)), i);
		}
	}
	req->oldidx += l;
	if (req->oldptr && i != l) {
		return ENOMEM;
	}
	return 0;
}

STATIC int
sysctl_new_kernel(struct sysctl_req *req, void *p, size_t l)
{
	if (!req->newptr) {
		return 0;
	}
	if (req->newlen - req->newidx < l) {
		return EINVAL;
	}
	bcopy(CAST_DOWN(char *, (req->newptr + req->newidx)), p, l);
	req->newidx += l;
	return 0;
}

int
kernel_sysctl(struct proc *p, int *name, size_t namelen, void *old, size_t *oldlenp, void *new, size_t newlen)
{
	int error = 0;
	struct sysctl_req req;

	/*
	 * Construct request.
	 */
	bzero(&req, sizeof req);
	req.p = p;
	if (oldlenp) {
		req.oldlen = *oldlenp;
	}
	if (old) {
		req.oldptr = CAST_USER_ADDR_T(old);
	}
	if (newlen) {
		req.newlen = newlen;
		req.newptr = CAST_USER_ADDR_T(new);
	}
	req.oldfunc = sysctl_old_kernel;
	req.newfunc = sysctl_new_kernel;
	req.lock = 1;

	/* make the request */
	error = sysctl_root(TRUE, FALSE, NULL, 0, name, namelen, &req);

	if (error && error != ENOMEM) {
		return error;
	}

	if (oldlenp) {
		*oldlenp = req.oldidx;
	}

	return error;
}

/*
 * Transfer function to/from user space.
 */
STATIC int
sysctl_old_user(struct sysctl_req *req, const void *p, size_t l)
{
	int error = 0;
	size_t i = 0;

	if (req->oldptr) {
		if (req->oldlen - req->oldidx < l) {
			return ENOMEM;
		}
		i = l;
		if (i > req->oldlen - req->oldidx) {
			i = req->oldlen - req->oldidx;
		}
		if (i > 0) {
			error = copyout((const void*)p, (req->oldptr + req->oldidx), i);
		}
	}
	req->oldidx += l;
	if (error) {
		return error;
	}
	if (req->oldptr && i < l) {
		return ENOMEM;
	}
	return 0;
}

STATIC int
sysctl_new_user(struct sysctl_req *req, void *p, size_t l)
{
	int error;

	if (!req->newptr) {
		return 0;
	}
	if (req->newlen - req->newidx < l) {
		return EINVAL;
	}
	error = copyin((req->newptr + req->newidx), p, l);
	req->newidx += l;
	return error;
}

#define WRITE_EXPERIMENT_FACTORS_ENTITLEMENT "com.apple.private.write-kr-experiment-factors"
/*
 * Is the current task allowed to write to experiment factors?
 * tasks with the WRITE_EXPERIMENT_FACTORS_ENTITLEMENT are always allowed to write these.
 * In the development / debug kernel we also allow root to write them.
 */
STATIC bool
can_write_experiment_factors(__unused struct sysctl_req *req)
{
	if (IOCurrentTaskHasEntitlement(WRITE_EXPERIMENT_FACTORS_ENTITLEMENT)) {
		return true;
	}
#if DEBUG || DEVELOPMENT
	return !proc_suser(req->p);
#else
	return false;
#endif /* DEBUG || DEVELOPMENT */
}

/*
 * Traverse our tree, and find the right node, execute whatever it points
 * at, and return the resulting error code.
 */

int
sysctl_root(boolean_t from_kernel, boolean_t string_is_canonical,
    char *namestring, size_t namestringlen,
    int *name, size_t namelen, struct sysctl_req *req)
{
	u_int indx;
	int i;
	struct sysctl_oid_iterator it;
	struct sysctl_oid *oid;
	sysctl_handler_t oid_handler = NULL;
	int error;
	boolean_t unlocked_node_found = FALSE;
	boolean_t namestring_started = FALSE;

	/* Get the read lock on the geometry */
	lck_rw_lock_shared(&sysctl_geometry_lock);

	if (string_is_canonical) {
		/* namestring is actually canonical, name/namelen needs to be populated */
		error = name2oid(namestring, name, &namelen);
		if (error) {
			goto err;
		}
	}

	it = sysctl_oid_iterator_begin(&sysctl__children);
	oid = sysctl_oid_iterator_next_system_order(&it);

	indx = 0;
	while (oid && indx < CTL_MAXNAME) {
		if (oid->oid_number == name[indx]) {
			if (!from_kernel && !string_is_canonical) {
				if (namestring_started) {
					if (strlcat(namestring, ".", namestringlen) >= namestringlen) {
						error = ENAMETOOLONG;
						goto err;
					}
				}

				if (strlcat(namestring, oid->oid_name, namestringlen) >= namestringlen) {
					error = ENAMETOOLONG;
					goto err;
				}
				namestring_started = TRUE;
			}

			indx++;
			if (!(oid->oid_kind & CTLFLAG_LOCKED)) {
				unlocked_node_found = TRUE;
			}
			if (oid->oid_kind & CTLFLAG_NOLOCK) {
				req->lock = 0;
			}
			/*
			 * For SYSCTL_PROC() functions which are for sysctl's
			 * which have parameters at the end of their OID
			 * space, you need to OR CTLTYPE_NODE into their
			 * access value.
			 *
			 * NOTE: For binary backward compatibility ONLY! Do
			 * NOT add new sysctl's that do this!  Existing
			 * sysctl's which do this will eventually have
			 * compatibility code in user space, and this method
			 * will become unsupported.
			 */
			if ((oid->oid_kind & CTLTYPE) == CTLTYPE_NODE) {
				if (oid->oid_handler) {
					goto found;
				}
				if (indx == namelen) {
					error = ENOENT;
					goto err;
				}

				it = sysctl_oid_iterator_begin(oid->oid_arg1);
				oid = sysctl_oid_iterator_next_system_order(&it);
			} else {
				if (indx != namelen) {
					error = EISDIR;
					goto err;
				}
				goto found;
			}
		} else {
			oid = sysctl_oid_iterator_next_system_order(&it);
		}
	}
	error = ENOENT;
	goto err;
found:

	/*
	 * indx is the index of the first remaining OID name,
	 * for sysctls that take them as arguments
	 */
	if (!from_kernel && !string_is_canonical && (indx < namelen)) {
		char tempbuf[10];
		u_int indx2;

		for (indx2 = indx; indx2 < namelen; indx2++) {
			snprintf(tempbuf, sizeof(tempbuf), "%d", name[indx2]);

			if (namestring_started) {
				if (strlcat(namestring, ".", namestringlen) >= namestringlen) {
					error = ENAMETOOLONG;
					goto err;
				}
			}

			if (strlcat(namestring, tempbuf, namestringlen) >= namestringlen) {
				error = ENAMETOOLONG;
				goto err;
			}
			namestring_started = TRUE;
		}
	}

	/* If writing isn't allowed */
	if (req->newptr && (!(oid->oid_kind & CTLFLAG_WR) ||
	    ((oid->oid_kind & CTLFLAG_SECURE) && securelevel > 0))) {
		error = (EPERM);
		goto err;
	}

	/*
	 * If we're inside the kernel, the OID must be marked as kernel-valid.
	 */
	if (from_kernel && !(oid->oid_kind & CTLFLAG_KERN)) {
		error = (EPERM);
		goto err;
	}

	if (req->newptr && req->p) {
		if (oid->oid_kind & CTLFLAG_EXPERIMENT) {
			/*
			 * Experiment factors have different permissions since they need to be
			 * writable by procs with WRITE_EXPERIMENT_FACTORS_ENTITLEMENT.
			 */
			if (!can_write_experiment_factors(req)) {
				error = (EPERM);
				goto err;
			}
		} else {
			/*
			 * This is where legacy enforcement of permissions occurs.  If the
			 * flag does not say CTLFLAG_ANYBODY, then we prohibit anyone but
			 * root from writing new values down.  If local enforcement happens
			 * at the leaf node, then it needs to be set as CTLFLAG_ANYBODY.  In
			 * addition, if the leaf node is set this way, then in order to do
			 * specific enforcement, it has to be of type SYSCTL_PROC.
			 */
			if (!(oid->oid_kind & CTLFLAG_ANYBODY) &&
			    (error = proc_suser(req->p))) {
				goto err;
			}
		}
	}

	/*
	 * sysctl_unregister_oid() may change the handler value, so grab it
	 * under the lock.
	 */
	oid_handler = oid->oid_handler;
	if (!oid_handler) {
		error = EINVAL;
		goto err;
	}

	/*
	 * Reference the OID and drop the geometry lock; this prevents the
	 * OID from being deleted out from under the handler call, but does
	 * not prevent other calls into handlers or calls to manage the
	 * geometry elsewhere from blocking...
	 */
	if ((oid->oid_kind & CTLFLAG_PERMANENT) == 0) {
		OSAddAtomic(1, &oid->oid_refcnt);
	}

	lck_rw_done(&sysctl_geometry_lock);

#if CONFIG_MACF
	if (!from_kernel) {
		error = mac_system_check_sysctlbyname(kauth_cred_get(),
		    namestring,
		    name,
		    namelen,
		    req->oldptr,
		    req->oldlen,
		    req->newptr,
		    req->newlen);
		if (error) {
			goto dropref;
		}
	}
#endif

	/*
	 * ...however, we still have to grab the mutex for those calls which
	 * may be into code whose reentrancy is protected by it.
	 */
	if (unlocked_node_found) {
		lck_mtx_lock(&sysctl_unlocked_node_lock);
	}

#if defined(HAS_APPLE_PAC)
	/*
	 * oid_handler is signed address-discriminated by sysctl_register_oid().
	 */
	oid_handler = ptrauth_auth_and_resign(oid_handler,
	    ptrauth_key_function_pointer,
	    ptrauth_blend_discriminator(&oid->oid_handler,
	    os_hash_kernel_pointer(oid->oid_arg1)),
	    ptrauth_key_function_pointer,
	    ptrauth_function_pointer_type_discriminator(typeof(oid_handler)));
#endif /* defined(HAS_APPLE_PAC) */

	if ((oid->oid_kind & CTLTYPE) == CTLTYPE_NODE) {
		i = oid_handler(oid, name + indx, (int)(namelen - indx), req);
	} else {
		i = oid_handler(oid, oid->oid_arg1, oid->oid_arg2, req);
	}
	error = i;

	if (unlocked_node_found) {
		lck_mtx_unlock(&sysctl_unlocked_node_lock);
	}

#if CONFIG_MACF
	/* only used from another CONFIG_MACF block */
dropref:
#endif

	/*
	 * This is tricky... we re-grab the geometry lock in order to drop
	 * the reference and wake on the address; since the geometry
	 * lock is a reader/writer lock rather than a mutex, we have to
	 * wake on all apparent 1->0 transitions.  This abuses the drop
	 * after the reference decrement in order to wake any lck_rw_sleep()
	 * in progress in sysctl_unregister_oid() that slept because of a
	 * non-zero reference count.
	 *
	 * Note:	OSAddAtomic() is defined to return the previous value;
	 *		we use this and the fact that the lock itself is a
	 *		barrier to avoid waking every time through on "hot"
	 *		OIDs.
	 */
	lck_rw_lock_shared(&sysctl_geometry_lock);

	if ((oid->oid_kind & CTLFLAG_PERMANENT) == 0) {
		if (OSAddAtomic(-1, &oid->oid_refcnt) == 1) {
			wakeup(&oid->oid_refcnt);
		}
	}

err:
	lck_rw_done(&sysctl_geometry_lock);
	return error;
}

void
sysctl_create_user_req(struct sysctl_req *req, struct proc *p, user_addr_t oldp,
    size_t oldlen, user_addr_t newp, size_t newlen)
{
	bzero(req, sizeof(*req));

	req->p = p;

	req->oldlen = oldlen;
	req->oldptr = oldp;

	if (newlen) {
		req->newlen = newlen;
		req->newptr = newp;
	}

	req->oldfunc = sysctl_old_user;
	req->newfunc = sysctl_new_user;
	req->lock = 1;

	return;
}

int
sysctl(proc_t p, struct sysctl_args *uap, __unused int32_t *retval)
{
	int error, new_error;
	size_t oldlen = 0, newlen;
	int name[CTL_MAXNAME];
	struct sysctl_req req;
	char *namestring;
	size_t namestringlen = MAXPATHLEN;

	/*
	 * all top-level sysctl names are non-terminal
	 */
	if (uap->namelen > CTL_MAXNAME || uap->namelen < 2) {
		return EINVAL;
	}
	error = copyin(uap->name, &name[0], uap->namelen * sizeof(int));
	if (error) {
		return error;
	}

	AUDIT_ARG(ctlname, name, uap->namelen);

	if (uap->newlen > SIZE_T_MAX) {
		return EINVAL;
	}
	newlen = (size_t)uap->newlen;

	if (uap->oldlenp != USER_ADDR_NULL) {
		uint64_t        oldlen64 = fuulong(uap->oldlenp);

		/*
		 * If more than 4G, clamp to 4G
		 */
		if (oldlen64 > SIZE_T_MAX) {
			oldlen = SIZE_T_MAX;
		} else {
			oldlen = (size_t)oldlen64;
		}
	}

	sysctl_create_user_req(&req, p, uap->old, oldlen, uap->new, newlen);

	/* Guess that longest length for the passed-in MIB, if we can be more aggressive than MAXPATHLEN */
	if (uap->namelen == 2) {
		if (name[0] == CTL_KERN && name[1] < KERN_MAXID) {
			namestringlen = 32; /* "kern.speculative_reads_disabled" */
		} else if (name[0] == CTL_HW && name[1] < HW_MAXID) {
			namestringlen = 32; /* "hw.cachelinesize_compat" */
		}
	}

	namestring = (char *)kalloc_data(namestringlen, Z_WAITOK);
	if (!namestring) {
		oldlen = 0;
		goto err;
	}

	error = userland_sysctl(FALSE, namestring, namestringlen, name, uap->namelen, &req, &oldlen);

	kfree_data(namestring, namestringlen);

	if ((error) && (error != ENOMEM)) {
		return error;
	}

err:
	if (uap->oldlenp != USER_ADDR_NULL) {
		/*
		 * Only overwrite the old error value on a new error
		 */
		new_error = suulong(uap->oldlenp, oldlen);

		if (new_error) {
			error = new_error;
		}
	}

	return error;
}

// sysctlbyname is also exported as KPI to kexts
// and the syscall name cannot conflict with it
int
sys_sysctlbyname(proc_t p, struct sysctlbyname_args *uap, __unused int32_t *retval)
{
	int error, new_error;
	size_t oldlen = 0, newlen;
	char *name;
	size_t namelen = 0;
	struct sysctl_req req;
	int oid[CTL_MAXNAME];

	if (uap->namelen >= MAXPATHLEN) { /* XXX arbitrary, undocumented */
		return ENAMETOOLONG;
	}
	namelen = (size_t)uap->namelen;

	name = (char *)kalloc_data(namelen + 1, Z_WAITOK);
	if (!name) {
		return ENOMEM;
	}

	error = copyin(uap->name, name, namelen);
	if (error) {
		kfree_data(name, namelen + 1);
		return error;
	}
	name[namelen] = '\0';

	/* XXX
	 * AUDIT_ARG(ctlname, name, uap->namelen);
	 */

	if (uap->newlen > SIZE_T_MAX) {
		kfree_data(name, namelen + 1);
		return EINVAL;
	}
	newlen = (size_t)uap->newlen;

	if (uap->oldlenp != USER_ADDR_NULL) {
		uint64_t        oldlen64 = fuulong(uap->oldlenp);

		/*
		 * If more than 4G, clamp to 4G
		 */
		if (oldlen64 > SIZE_T_MAX) {
			oldlen = SIZE_T_MAX;
		} else {
			oldlen = (size_t)oldlen64;
		}
	}

	sysctl_create_user_req(&req, p, uap->old, oldlen, uap->new, newlen);

	error = userland_sysctl(TRUE, name, namelen + 1, oid, CTL_MAXNAME, &req, &oldlen);

	kfree_data(name, namelen + 1);

	if ((error) && (error != ENOMEM)) {
		return error;
	}

	if (uap->oldlenp != USER_ADDR_NULL) {
		/*
		 * Only overwrite the old error value on a new error
		 */
		new_error = suulong(uap->oldlenp, oldlen);

		if (new_error) {
			error = new_error;
		}
	}

	return error;
}

/*
 * This is used from various compatibility syscalls too.  That's why name
 * must be in kernel space.
 */
int
userland_sysctl(boolean_t string_is_canonical,
    char *namestring, size_t namestringlen,
    int *name, u_int namelen, struct sysctl_req *req,
    size_t *retval)
{
	int error = 0;
	struct sysctl_req req2;

	do {
		/* if EAGAIN, reset output cursor */
		req2 = *req;
		if (!string_is_canonical) {
			namestring[0] = '\0';
		}

		error = sysctl_root(FALSE, string_is_canonical, namestring, namestringlen, name, namelen, &req2);
	} while (error == EAGAIN);

	if (error && error != ENOMEM) {
		return error;
	}

	if (retval) {
		if (req2.oldptr && req2.oldidx > req2.oldlen) {
			*retval = req2.oldlen;
		} else {
			*retval = req2.oldidx;
		}
	}
	return error;
}

/*
 * Kernel versions of the userland sysctl helper functions.
 *
 * These allow sysctl to be used in the same fashion in both
 * userland and the kernel.
 *
 * Note that some sysctl handlers use copyin/copyout, which
 * may not work correctly.
 *
 * The "sysctlbyname" KPI for use by kexts is aliased to this function.
 */

int
kernel_sysctlbyname(const char *name, void *oldp, size_t *oldlenp, void *newp, size_t newlen)
{
	int oid[CTL_MAXNAME];
	int name2mib_oid[2];
	int error;
	size_t oidlen;

	/* look up the OID with magic service node */
	name2mib_oid[0] = 0;
	name2mib_oid[1] = 3;

	oidlen = sizeof(oid);
	error = kernel_sysctl(current_proc(), name2mib_oid, 2, oid, &oidlen, __DECONST(void *, name), strlen(name));
	oidlen /= sizeof(int);
	if (oidlen > UINT_MAX) {
		error = EDOM;
	}

	/* now use the OID */
	if (error == 0) {
		error = kernel_sysctl(current_proc(), oid, (u_int)oidlen, oldp, oldlenp, newp, newlen);
	}
	return error;
}

int
scalable_counter_sysctl_handler SYSCTL_HANDLER_ARGS
{
#pragma unused(arg2, oidp)
	scalable_counter_t counter = *(scalable_counter_t*) arg1;
	uint64_t value = counter_load(&counter);
	return SYSCTL_OUT(req, &value, sizeof(value));
}

#define X(name, T) \
int \
experiment_factor_##name##_handler SYSCTL_HANDLER_ARGS \
{ \
	int error, changed = 0; \
	T *ptr; \
	T new_value, current_value; \
	struct experiment_spec *spec = (struct experiment_spec *) arg1; \
	if (!arg1) { \
	        return EINVAL; \
	} \
	ptr = (T *)(spec->ptr); \
	current_value = *ptr; \
	error = sysctl_io_number(req, current_value, sizeof(T), &new_value, &changed); \
	if (error != 0) { \
	        return error; \
	} \
	if (changed) { \
	        if (new_value < (T) spec->min_value || new_value > (T) spec->max_value) { \
	                return EINVAL; \
	        } \
	        if (os_atomic_cmpxchg(&spec->modified, false, true, acq_rel)) { \
	                spec->original_value = current_value; \
	        } \
	        os_atomic_store_wide(ptr, new_value, relaxed); \
	} \
	return 0; \
}

experiment_factor_numeric_types
#undef X

#if DEBUG || DEVELOPMENT
static int
sysctl_test_handler SYSCTL_HANDLER_ARGS
{
	int error;
	int64_t value, out = 0;

	error = SYSCTL_IN(req, &value, sizeof(value));
	/* Only run test when new value was provided to prevent just reading or
	 * querying from triggering the test, but still allow for sysctl
	 * presence tests via read requests with NULL oldptr */
	if (error == 0 && req->newptr) {
		/* call the test that was specified in SYSCTL_TEST_REGISTER */
		error = ((int (*)(int64_t, int64_t *))(uintptr_t)arg1)(value, &out);
	}
	if (error == 0) {
		error = SYSCTL_OUT(req, &out, sizeof(out));
	}
	return error;
}

void
sysctl_register_test_startup(struct sysctl_test_setup_spec *spec)
{
	struct sysctl_oid *oid = zalloc_permanent_type(struct sysctl_oid);

	*oid = (struct sysctl_oid){
		.oid_parent     = &sysctl__debug_test_children,
		.oid_number     = OID_AUTO,
		.oid_kind       = CTLTYPE_QUAD | CTLFLAG_OID2 | CTLFLAG_WR |
	    CTLFLAG_PERMANENT | CTLFLAG_LOCKED | CTLFLAG_MASKED,
		.oid_arg1       = (void *)(uintptr_t)spec->st_func,
		.oid_name       = spec->st_name,
		.oid_handler    = sysctl_test_handler,
		.oid_fmt        = "Q",
		.oid_version    = SYSCTL_OID_VERSION,
		.oid_descr      = "",
	};
	sysctl_register_oid_early(oid);
}


extern void vm_analytics_tick(void *arg0, void *arg1);

/* Manual trigger of vm_analytics_tick for testing on dev/debug kernel. */
static int
sysctl_vm_analytics_tick SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2)
	int error, val = 0;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr) {
		return error;
	}
	vm_analytics_tick(NULL, NULL);
	return 0;
}

SYSCTL_PROC(_vm, OID_AUTO, analytics_report, CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_LOCKED | CTLFLAG_MASKED, 0, 0, &sysctl_vm_analytics_tick, "I", "");

/* Manual trigger of record_system_event for testing on dev/debug kernel */
static int
sysctl_test_record_system_event SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2)
	int error, val = 0;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr) {
		return error;
	}
	record_system_event(SYSTEM_EVENT_TYPE_INFO, SYSTEM_EVENT_SUBSYSTEM_TEST, "sysctl test", "this is a test %s", "message");
	return 0;
}

SYSCTL_PROC(_kern, OID_AUTO, test_record_system_event, CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_LOCKED | CTLFLAG_MASKED, 0, 0, &sysctl_test_record_system_event, "-", "");

#endif /* DEBUG || DEVELOPMENT */


CA_EVENT(ca_test_event,
    CA_INT, TestKey,
    CA_BOOL, TestBool,
    CA_STATIC_STRING(CA_UUID_LEN), TestString);

/*
 * Manual testing of sending a CoreAnalytics event
 */
static int
sysctl_test_ca_event SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2)
	int error, val = 0;
	/*
	 * Only send on write
	 */
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr) {
		return error;
	}

	ca_event_t event = CA_EVENT_ALLOCATE(ca_test_event);
	CA_EVENT_TYPE(ca_test_event) * event_data = event->data;
	event_data->TestKey = val;
	event_data->TestBool = true;
	uuid_string_t test_str = "sysctl_test_ca_event";
	strlcpy(event_data->TestString, test_str, CA_UUID_LEN);
	CA_EVENT_SEND(event);
	return 0;
}

SYSCTL_PROC(_kern, OID_AUTO, test_ca_event, CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_LOCKED | CTLFLAG_MASKED, 0, 0, &sysctl_test_ca_event, "I", "");


#if DEVELOPMENT || DEBUG
struct perf_compressor_data {
	user_addr_t buffer;
	size_t buffer_size;
	uint64_t benchmark_time;
	uint64_t bytes_processed;
	uint64_t compressor_growth;
};

static int
sysctl_perf_compressor SYSCTL_HANDLER_ARGS
{
	int error = EINVAL;
	size_t len = sizeof(struct perf_compressor_data);
	struct perf_compressor_data benchmark_data = {0};

	if (req->oldptr == USER_ADDR_NULL || req->oldlen != len ||
	    req->newptr == USER_ADDR_NULL || req->newlen != len) {
		return EINVAL;
	}

	error = SYSCTL_IN(req, &benchmark_data, len);
	if (error) {
		return error;
	}

	kern_return_t ret = run_compressor_perf_test(benchmark_data.buffer, benchmark_data.buffer_size,
	    &benchmark_data.benchmark_time, &benchmark_data.bytes_processed, &benchmark_data.compressor_growth);
	switch (ret) {
	case KERN_SUCCESS:
		error = 0;
		break;
	case KERN_NOT_SUPPORTED:
		error = ENOTSUP;
		break;
	case KERN_INVALID_ARGUMENT:
		error = EINVAL;
		break;
	case KERN_RESOURCE_SHORTAGE:
		error = EAGAIN;
		break;
	default:
		error = ret;
		break;
	}
	if (error != 0) {
		return error;
	}

	return SYSCTL_OUT(req, &benchmark_data, len);
}

/*
 * Compressor & swap performance test
 */
SYSCTL_PROC(_kern, OID_AUTO, perf_compressor, CTLFLAG_WR | CTLFLAG_MASKED | CTLTYPE_STRUCT,
    0, 0, sysctl_perf_compressor, "S", "Compressor & swap benchmark");
#endif /* DEVELOPMENT || DEBUG */

#if CONFIG_JETSAM
extern uint32_t swapout_sleep_threshold;
#if DEVELOPMENT || DEBUG
SYSCTL_UINT(_vm, OID_AUTO, swapout_sleep_threshold, CTLFLAG_RW | CTLFLAG_LOCKED, &swapout_sleep_threshold, 0, "");
#else /* DEVELOPMENT || DEBUG */
SYSCTL_UINT(_vm, OID_AUTO, swapout_sleep_threshold, CTLFLAG_RD | CTLFLAG_LOCKED, &swapout_sleep_threshold, 0, "");
#endif /* DEVELOPMENT || DEBUG */
#endif /* CONFIG_JETSAM */
