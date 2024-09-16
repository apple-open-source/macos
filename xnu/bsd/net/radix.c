/*
 * Copyright (c) 2000-2013 Apple Inc. All rights reserved.
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
/*
 * Copyright (c) 1988, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)radix.c	8.4 (Berkeley) 11/2/94
 * $FreeBSD: src/sys/net/radix.c,v 1.20.2.2 2001/03/06 00:56:50 obrien Exp $
 */

/*
 * Routines to build and maintain radix trees for routing lookups.
 */
#ifndef _RADIX_H_
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/domain.h>
#include <sys/syslog.h>
#include <net/radix.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <kern/locks.h>
#endif

static int      rn_walktree_from(struct radix_node_head *h, void *a,
    void *m, walktree_f_t *f, void *w);
static int rn_walktree(struct radix_node_head *, walktree_f_t *, void *);
static struct radix_node *rn_insert(void *, struct radix_node_head *, int *, struct radix_node[2]);
static struct radix_node *rn_newpair(void * __sized_by(vlen), uint8_t vlen, int, struct radix_node[2]);
static struct radix_node *rn_search(void *, struct radix_node *);
static struct radix_node *rn_search_m(void *, struct radix_node *, void *);

static int max_keylen;
static struct radix_mask *rn_mkfreelist;
static struct radix_node_head *mask_rnhead;
static char *addmask_key;
static char normal_chars[] = {0, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, -1};
static char *rn_zeros, *rn_ones;

static zone_t radix_node_zone;
KALLOC_TYPE_DEFINE(radix_node_head_zone, struct radix_node_head, KT_DEFAULT);

#define rn_masktop (mask_rnhead->rnh_treetop)
#undef Bcmp
#define Bcmp(a, b, l) \
	(l == 0 ? 0 : bcmp((caddr_t)(a), (caddr_t)(b), (uint32_t)l))

static int      rn_lexobetter(void *m_arg, void *n_arg);
static struct radix_mask * rn_new_radix_mask(struct radix_node *tt, struct radix_mask *next);
static int rn_satisfies_leaf(char *trial, struct radix_node *leaf, int skip,
    rn_matchf_t *f, void *w);

#define RN_MATCHF(rn, f, arg)   (f == NULL || (*f)((rn), arg))

/*
 * The data structure for the keys is a radix tree with one way
 * branching removed.  The index rn_bit at an internal node n represents a bit
 * position to be tested.  The tree is arranged so that all descendants
 * of a node n have keys whose bits all agree up to position rn_bit - 1.
 * (We say the index of n is rn_bit.)
 *
 * There is at least one descendant which has a one bit at position rn_bit,
 * and at least one with a zero there.
 *
 * A route is determined by a pair of key and mask.  We require that the
 * bit-wise logical and of the key and mask to be the key.
 * We define the index of a route to associated with the mask to be
 * the first bit number in the mask where 0 occurs (with bit number 0
 * representing the highest order bit).
 *
 * We say a mask is normal if every bit is 0, past the index of the mask.
 * If a node n has a descendant (k, m) with index(m) == index(n) == rn_bit,
 * and m is a normal mask, then the route applies to every descendant of n.
 * If the index(m) < rn_bit, this implies the trailing last few bits of k
 * before bit b are all 0, (and hence consequently true of every descendant
 * of n), so the route applies to all descendants of the node as well.
 *
 * Similar logic shows that a non-normal mask m such that
 * index(m) <= index(n) could potentially apply to many children of n.
 * Thus, for each non-host route, we attach its mask to a list at an internal
 * node as high in the tree as we can go.
 *
 * The present version of the code makes use of normal routes in short-
 * circuiting an explict mask and compare operation when testing whether
 * a key satisfies a normal route, and also in remembering the unique leaf
 * that governs a subtree.
 */

static inline void*
__sized_by(*arglen)
__attribute__((always_inline))
rnarg_unpack(void *packed_arg, uint8_t *arglen)
{
	if (!packed_arg) {
		*arglen = 0;
		return NULL;
	}
	*arglen = *((uint8_t * __single)packed_arg);
	return __unsafe_forge_bidi_indexable(void *, packed_arg, *arglen);
}

static inline char
__attribute__((always_inline))
rnarg_get(caddr_t rnarg __sized_by(arglen), uint8_t arglen, unsigned int offset)
{
	if (arglen <= offset) {
		return 0;
	}
	return rnarg[offset];
}


static struct radix_node *
rn_search(void *v_arg, struct radix_node *head)
{
	struct radix_node *x = head;
	uint8_t vlen = 0;
	caddr_t v = rnarg_unpack(v_arg, &vlen);

	while (x->rn_bit >= 0) {
		if (x->rn_bmask & rnarg_get(v, vlen, x->rn_offset)) {
			x = x->rn_right;
		} else {
			x = x->rn_left;
		}
	}
	return x;
}

static struct radix_node *
rn_search_m(void *v_arg, struct radix_node *head, void *m_arg)
{
	struct radix_node *x = head;
	uint8_t vlen = 0;
	caddr_t v = rnarg_unpack(v_arg, &vlen);
	uint8_t mlen = 0;
	caddr_t m = rnarg_unpack(m_arg, &mlen);

	while (x->rn_bit >= 0) {
		if ((x->rn_bmask & rnarg_get(m, mlen, x->rn_offset)) &&
		    (x->rn_bmask & rnarg_get(v, vlen, x->rn_offset))) {
			x = x->rn_right;
		} else {
			x = x->rn_left;
		}
	}
	return x;
}

int
rn_refines(void *m_arg, void *n_arg)
{
	uint8_t mlen, nlen;
	caddr_t m = rnarg_unpack(m_arg, &mlen);
	caddr_t n = rnarg_unpack(n_arg, &nlen);
	caddr_t lim, lim2 = lim = n + nlen;
	int longer = nlen - mlen;
	n++;
	m++;
	int masks_are_equal = 1;

	if (longer > 0) {
		lim -= longer;
	}
	while (n < lim) {
		if (*n & ~(*m)) {
			return 0;
		}
		if (*n++ != *m++) {
			masks_are_equal = 0;
		}
	}
	while (n < lim2) {
		if (*n++) {
			return 0;
		}
	}
	if (masks_are_equal && (longer < 0)) {
		for (lim2 = m - longer; m < lim2;) {
			if (*m++) {
				return 1;
			}
		}
	}
	return !masks_are_equal;
}

struct radix_node *
rn_lookup(void *v_arg, void *m_arg, struct radix_node_head *head)
{
	return rn_lookup_args(v_arg, m_arg, head, NULL, NULL);
}

struct radix_node *
rn_lookup_args(void *v_arg, void *m_arg, struct radix_node_head *head,
    rn_matchf_t *f, void *w)
{
	struct radix_node *x;
	caddr_t netmask = NULL;

	if (m_arg) {
		x = rn_addmask(m_arg, 1, head->rnh_treetop->rn_offset);
		if (x == 0) {
			return NULL;
		}
		/*
		 * Note: the auxillary mask is stored as a "key".
		 */
		netmask = rn_get_key(x);
	}
	x = rn_match_args(v_arg, head, f, w);
	if (x && netmask) {
		while (x && rn_get_mask(x) != netmask) {
			x = x->rn_dupedkey;
		}
	}
	return x;
}

/*
 * Returns true if address 'trial' has no bits differing from the
 * leaf's key when compared under the leaf's mask.  In other words,
 * returns true when 'trial' matches leaf.  If a leaf-matching
 * routine is passed in, it is also used to find a match on the
 * conditions defined by the caller of rn_match.
 */
static int
rn_satisfies_leaf(char *trial, struct radix_node *leaf, int skip,
    rn_matchf_t *f, void *w)
{
	uint8_t cplen;
	char *cp = rnarg_unpack(trial, &cplen);
	char *cp2 = rn_get_key(leaf);
	char *cp3 = rn_get_mask(leaf);
	char *cplim;
	int length = min(*(u_char *)cp, *(u_char *)cp2);

	if (cp3 == 0) {
		cp3 = rn_ones;
	} else {
		length = min(length, *(u_char *)cp3);
	}
	cplim = cp + length; cp3 += skip; cp2 += skip;
	for (cp += skip; cp < cplim; cp++, cp2++, cp3++) {
		if ((*cp ^ *cp2) & *cp3) {
			return 0;
		}
	}

	return RN_MATCHF(leaf, f, w);
}

struct radix_node *
rn_match(void *v_arg, struct radix_node_head *head)
{
	return rn_match_args(v_arg, head, NULL, NULL);
}

struct radix_node *
rn_match_args(void *v_arg, struct radix_node_head *head,
    rn_matchf_t *f, void *w)
{
	uint8_t vlen0;
	caddr_t v = rnarg_unpack(v_arg, &vlen0);
	struct radix_node *t = head->rnh_treetop, *x;
	caddr_t cp = v, cp2;
	caddr_t cplim;
	struct radix_node *saved_t, *top = t;
	int off = t->rn_offset, vlen = vlen0, matched_off;
	int test, b, rn_bit;

	/*
	 * Open code rn_search(v, top) to avoid overhead of extra
	 * subroutine call.
	 */
	for (; t->rn_bit >= 0;) {
		if (t->rn_bmask & cp[t->rn_offset]) {
			t = t->rn_right;
		} else {
			t = t->rn_left;
		}
	}
	/*
	 * See if we match exactly as a host destination
	 * or at least learn how many bits match, for normal mask finesse.
	 *
	 * It doesn't hurt us to limit how many bytes to check
	 * to the length of the mask, since if it matches we had a genuine
	 * match and the leaf we have is the most specific one anyway;
	 * if it didn't match with a shorter length it would fail
	 * with a long one.  This wins big for class B&C netmasks which
	 * are probably the most common case...
	 */
	if (rn_get_mask(t)) {
		vlen = rn_get_masklen(t);
	}
	cp += off;
	cp2 = rn_get_key(t) + off;
	cplim = v + vlen;

	for (; cp < cplim; cp++, cp2++) {
		if (*cp != *cp2) {
			goto on1;
		}
	}
	/*
	 * This extra grot is in case we are explicitly asked
	 * to look up the default.  Ugh!
	 *
	 * Never return the root node itself, it seems to cause a
	 * lot of confusion.
	 */
	if (t->rn_flags & RNF_ROOT) {
		t = t->rn_dupedkey;
	}
	if (t == NULL || RN_MATCHF(t, f, w)) {
		return t;
	} else {
		/*
		 * Although we found an exact match on the key,
		 * f() is looking for some other criteria as well.
		 * Continue looking as if the exact match failed.
		 */
		if (t->rn_parent->rn_flags & RNF_ROOT) {
			/* Hit the top; have to give up */
			return NULL;
		}
		b = 0;
		goto keeplooking;
	}
on1:
	test = (*cp ^ *cp2) & 0xff; /* find first bit that differs */
	for (b = 7; (test >>= 1) > 0;) {
		b--;
	}
keeplooking:
	matched_off = (int)(cp - v);
	b += matched_off << 3;
	rn_bit = -1 - b;
	/*
	 * If there is a host route in a duped-key chain, it will be first.
	 */
	saved_t = t;
	if (rn_get_mask(t) == 0) {
		t = t->rn_dupedkey;
	}
	for (; t; t = t->rn_dupedkey) {
		/*
		 * Even if we don't match exactly as a host,
		 * we may match if the leaf we wound up at is
		 * a route to a net.
		 */
		if (t->rn_flags & RNF_NORMAL) {
			if ((rn_bit <= t->rn_bit) && RN_MATCHF(t, f, w)) {
				return t;
			}
		} else if (rn_satisfies_leaf(v, t, matched_off, f, w)) {
			return t;
		}
	}
	t = saved_t;
	/* start searching up the tree */
	do {
		struct radix_mask *m;
		t = t->rn_parent;
		m = t->rn_mklist;
		/*
		 * If non-contiguous masks ever become important
		 * we can restore the masking and open coding of
		 * the search and satisfaction test and put the
		 * calculation of "off" back before the "do".
		 */
		while (m) {
			if (m->rm_flags & RNF_NORMAL) {
				if ((rn_bit <= m->rm_bit) &&
				    RN_MATCHF(m->rm_leaf, f, w)) {
					return m->rm_leaf;
				}
			} else {
				off = min(t->rn_offset, matched_off);
				x = rn_search_m(v, t, rm_get_mask(m));
				while (x && rn_get_mask(x) != rm_get_mask(m)) {
					x = x->rn_dupedkey;
				}
				if (x && rn_satisfies_leaf(v, x, off, f, w)) {
					return x;
				}
			}
			m = m->rm_mklist;
		}
	} while (t != top);
	return NULL;
}

#ifdef RN_DEBUG
int     rn_nodenum;
struct  radix_node *rn_clist;
int     rn_saveinfo;
int     rn_debug =  1;
#endif

static struct radix_node *
rn_newpair(void *v __sized_by(vlen), uint8_t vlen, int b, struct radix_node nodes[2])
{
	struct radix_node *tt = nodes, *t = tt + 1;
	t->rn_bit = (short)b;
	t->rn_bmask = 0x80 >> (b & 7);
	t->rn_left = tt;
	t->rn_offset = b >> 3;
	tt->rn_bit = -1;
	rn_set_key(tt, v, vlen);
	tt->rn_parent = t;
	tt->rn_flags = t->rn_flags = RNF_ACTIVE;
	tt->rn_mklist = t->rn_mklist = NULL;
#ifdef RN_DEBUG
	tt->rn_info = rn_nodenum++; t->rn_info = rn_nodenum++;
	tt->rn_twin = t;
	tt->rn_ybro = rn_clist;
	rn_clist = tt;
#endif
	return t;
}

static struct radix_node *
rn_insert(void *v_arg, struct radix_node_head *head, int *dupentry,
    struct radix_node nodes[2])
{
	uint8_t vlen;
	caddr_t v = rnarg_unpack(v_arg, &vlen);
	struct radix_node *top = head->rnh_treetop;
	int head_off = top->rn_offset;
	struct radix_node *t = rn_search(v_arg, top);
	caddr_t cp = v + head_off;
	int b;
	struct radix_node *tt;
	/*
	 * Find first bit at which v and t->rn_key differ
	 */
	{
		caddr_t cp2 = rn_get_key(t) + head_off;
		int cmp_res;
		caddr_t cplim = v + vlen;

		while (cp < cplim) {
			if (*cp2++ != *cp++) {
				goto on1;
			}
		}
		*dupentry = 1;
		return t;
on1:
		*dupentry = 0;
		cmp_res = (cp[-1] ^ cp2[-1]) & 0xff;
		for (b = (int)(cp - v) << 3; cmp_res; b--) {
			cmp_res >>= 1;
		}
	}
	{
		struct radix_node *p, *x = top;
		cp = v;
		do {
			p = x;
			if (cp[x->rn_offset] & x->rn_bmask) {
				x = x->rn_right;
			} else {
				x = x->rn_left;
			}
		} while (b > (unsigned) x->rn_bit);
		/* x->rn_bit < b && x->rn_bit >= 0 */
#ifdef RN_DEBUG
		if (rn_debug) {
			log(LOG_DEBUG, "rn_insert: Going In:\n"), traverse(p);
		}
#endif
		t = rn_newpair(v_arg, vlen, b, nodes);
		tt = t->rn_left;
		if ((cp[p->rn_offset] & p->rn_bmask) == 0) {
			p->rn_left = t;
		} else {
			p->rn_right = t;
		}
		x->rn_parent = t;
		t->rn_parent = p; /* frees x, p as temp vars below */
		if ((cp[t->rn_offset] & t->rn_bmask) == 0) {
			t->rn_right = x;
		} else {
			t->rn_right = tt;
			t->rn_left = x;
		}
#ifdef RN_DEBUG
		if (rn_debug) {
			log(LOG_DEBUG, "rn_insert: Coming Out:\n"), traverse(p);
		}
#endif
	}
	return tt;
}

struct radix_node *
rn_addmask(void *n_arg, int search, int skip)
{
	caddr_t netmask = (caddr_t)n_arg;
	struct radix_node *x;
	caddr_t cp, cplim;
	int b = 0, mlen, j;
	int maskduplicated, m0, isnormal;
	struct radix_node *saved_x;
	static int last_zeroed = 0;

	if ((mlen = *(u_char *)netmask) > max_keylen) {
		mlen = max_keylen;
	}
	if (skip == 0) {
		skip = 1;
	}
	if (mlen <= skip) {
		return mask_rnhead->rnh_nodes;
	}
	if (skip > 1) {
		Bcopy(rn_ones + 1, addmask_key + 1, skip - 1);
	}
	if ((m0 = mlen) > skip) {
		Bcopy(netmask + skip, addmask_key + skip, mlen - skip);
	}
	/*
	 * Trim trailing zeroes.
	 */
	for (cp = addmask_key + mlen; (cp > addmask_key) && cp[-1] == 0;) {
		cp--;
	}
	mlen = (int)(cp - addmask_key);
	if (mlen <= skip) {
		if (m0 >= last_zeroed) {
			last_zeroed = mlen;
		}
		return mask_rnhead->rnh_nodes;
	}
	if (m0 < last_zeroed) {
		Bzero(addmask_key + m0, last_zeroed - m0);
	}
	*addmask_key = last_zeroed = (char)mlen;
	x = rn_search(addmask_key, rn_masktop);
	if (Bcmp(addmask_key, rn_get_key(x), mlen) != 0) {
		x = NULL;
	}
	if (x || search) {
		return x;
	}
	x = saved_x = zalloc_flags(radix_node_zone, Z_WAITOK_ZERO_NOFAIL);
	netmask = cp = (caddr_t)(x + 2);
	Bcopy(addmask_key, cp, mlen);
	x = rn_insert(cp, mask_rnhead, &maskduplicated, x);
	if (maskduplicated) {
		log(LOG_ERR, "rn_addmask: mask impossibly already in tree");
		zfree(radix_node_zone, saved_x);
		return x;
	}
	mask_rnhead->rnh_cnt++;
	/*
	 * Calculate index of mask, and check for normalcy.
	 */
	cplim = netmask + mlen; isnormal = 1;
	for (cp = netmask + skip; (cp < cplim) && *(u_char *)cp == 0xff;) {
		cp++;
	}
	if (cp != cplim) {
		for (j = 0x80; (j & *cp) != 0; j >>= 1) {
			b++;
		}
		if (*cp != normal_chars[b] || cp != (cplim - 1)) {
			isnormal = 0;
		}
	}
	b += (cp - netmask) << 3;
	x->rn_bit = (short)(-1 - b);
	if (isnormal) {
		x->rn_flags |= RNF_NORMAL;
	}
	return x;
}

static int
/* XXX: arbitrary ordering for non-contiguous masks */
rn_lexobetter(void *m_arg, void *n_arg)
{
	u_char *mp = m_arg, *np = n_arg, *lim;

	if (*mp > *np) {
		return 1;  /* not really, but need to check longer one first */
	}
	if (*mp == *np) {
		for (lim = mp + *mp; mp < lim;) {
			if (*mp++ > *np++) {
				return 1;
			}
		}
	}
	return 0;
}

static struct radix_mask *
rn_new_radix_mask(struct radix_node *tt, struct radix_mask *next)
{
	struct radix_mask *m;

	MKGet(m);
	m->rm_bit = tt->rn_bit;
	m->rm_flags = tt->rn_flags;
	if (tt->rn_flags & RNF_NORMAL) {
		m->rm_leaf = tt;
	} else {
		rm_set_mask(m, rn_get_mask(tt), rn_get_masklen(tt));
	}
	m->rm_mklist = next;
	tt->rn_mklist = m;
	return m;
}

struct radix_node *
rn_addroute(void *v_arg, void *n_arg, struct radix_node_head *head,
    struct radix_node treenodes[2])
{
	uint8_t vlen, masklen;
	caddr_t v = rnarg_unpack(v_arg, &vlen);
	caddr_t netmask = rnarg_unpack(n_arg, &masklen);
	struct radix_node *t, *x = NULL, *tt;
	struct radix_node *saved_tt, *top = head->rnh_treetop;
	short b = 0, b_leaf = 0;
	int keyduplicated;
	caddr_t mmask;
	struct radix_mask *m, **mp;

	/*
	 * In dealing with non-contiguous masks, there may be
	 * many different routes which have the same mask.
	 * We will find it useful to have a unique pointer to
	 * the mask to speed avoiding duplicate references at
	 * nodes and possibly save time in calculating indices.
	 */
	if (netmask) {
		if ((x = rn_addmask(netmask, 0, top->rn_offset)) == 0) {
			return NULL;
		}
		b_leaf = x->rn_bit;
		b = -1 - x->rn_bit;
		/*
		 * Note: the auxillary mask is stored as a "key".
		 */
		netmask = rn_get_key(x);
	}
	/*
	 * Deal with duplicated keys: attach node to previous instance
	 */
	saved_tt = tt = rn_insert(v, head, &keyduplicated, treenodes);
	if (keyduplicated) {
		for (t = tt; tt; t = tt, tt = tt->rn_dupedkey) {
			if (rn_get_mask(tt) == netmask) {
				return NULL;
			}
			if (netmask == 0 ||
			    (rn_get_mask(tt) != NULL &&
			    ((b_leaf < tt->rn_bit)  /* index(netmask) > node */
			    || rn_refines(netmask, rn_get_mask(tt))
			    || rn_lexobetter(netmask, rn_get_mask(tt))))) {
				break;
			}
		}
		/*
		 * If the mask is not duplicated, we wouldn't
		 * find it among possible duplicate key entries
		 * anyway, so the above test doesn't hurt.
		 *
		 * We sort the masks for a duplicated key the same way as
		 * in a masklist -- most specific to least specific.
		 * This may require the unfortunate nuisance of relocating
		 * the head of the list.
		 */
		if (tt == saved_tt) {
			struct  radix_node *xx = x;
			/* link in at head of list */
			(tt = treenodes)->rn_dupedkey = t;
			tt->rn_flags = t->rn_flags;
			tt->rn_parent = x = t->rn_parent;
			t->rn_parent = tt;                      /* parent */
			if (x->rn_left == t) {
				x->rn_left = tt;
			} else {
				x->rn_right = tt;
			}
			saved_tt = tt; x = xx;
		} else {
			(tt = treenodes)->rn_dupedkey = t->rn_dupedkey;
			t->rn_dupedkey = tt;
			tt->rn_parent = t;                      /* parent */
			if (tt->rn_dupedkey) {                  /* parent */
				tt->rn_dupedkey->rn_parent = tt; /* parent */
			}
		}
#ifdef RN_DEBUG
		t = tt + 1; tt->rn_info = rn_nodenum++; t->rn_info = rn_nodenum++;
		tt->rn_twin = t; tt->rn_ybro = rn_clist; rn_clist = tt;
#endif
		rn_set_key(tt, v, vlen);
		tt->rn_bit = -1;
		tt->rn_flags = RNF_ACTIVE;
	}
	head->rnh_cnt++;
	/*
	 * Put mask in tree.
	 */
	if (netmask) {
		rn_set_mask(tt, netmask, masklen);
		tt->rn_bit = x->rn_bit;
		tt->rn_flags |= x->rn_flags & RNF_NORMAL;
	}
	t = saved_tt->rn_parent;
	if (keyduplicated) {
		goto on2;
	}
	b_leaf = -1 - t->rn_bit;
	if (t->rn_right == saved_tt) {
		x = t->rn_left;
	} else {
		x = t->rn_right;
	}
	/* Promote general routes from below */
	if (x->rn_bit < 0) {
		for (mp = &t->rn_mklist; x; x = x->rn_dupedkey) {
			if (rn_get_mask(x) != NULL && (x->rn_bit >= b_leaf) && x->rn_mklist == 0) {
				*mp = m = rn_new_radix_mask(x, NULL);
				if (m) {
					mp = &m->rm_mklist;
				}
			}
		}
	} else if (x->rn_mklist) {
		/*
		 * Skip over masks whose index is > that of new node
		 */
		for (mp = &x->rn_mklist; (m = *mp); mp = &m->rm_mklist) {
			if (m->rm_bit >= b_leaf) {
				break;
			}
		}
		t->rn_mklist = m; *mp = NULL;
	}
on2:
	/* Add new route to highest possible ancestor's list */
	if ((netmask == 0) || (b > t->rn_bit)) {
		return tt; /* can't lift at all */
	}
	b_leaf = tt->rn_bit;
	do {
		x = t;
		t = t->rn_parent;
	} while (b <= t->rn_bit && x != top);
	/*
	 * Search through routes associated with node to
	 * insert new route according to index.
	 * Need same criteria as when sorting dupedkeys to avoid
	 * double loop on deletion.
	 */
	for (mp = &x->rn_mklist; (m = *mp); mp = &m->rm_mklist) {
		if (m->rm_bit < b_leaf) {
			continue;
		}
		if (m->rm_bit > b_leaf) {
			break;
		}
		if (m->rm_flags & RNF_NORMAL) {
			mmask = rn_get_mask(m->rm_leaf);
			if (tt->rn_flags & RNF_NORMAL) {
				log(LOG_ERR,
				    "Non-unique normal route, mask not entered");
				return tt;
			}
		} else {
			mmask = rm_get_mask(m);
		}
		if (mmask == netmask) {
			m->rm_refs++;
			tt->rn_mklist = m;
			return tt;
		}
		if (rn_refines(netmask, mmask)
		    || rn_lexobetter(netmask, mmask)) {
			break;
		}
	}
	*mp = rn_new_radix_mask(tt, *mp);
	return tt;
}

struct radix_node *
rn_delete(void *v_arg, void *netmask_arg, struct radix_node_head *head)
{
	struct radix_node *t, *p, *x, *tt;
	struct radix_mask *m, *saved_m, **mp;
	struct radix_node *dupedkey, *saved_tt, *top;
	caddr_t v, netmask;
	int b, head_off, vlen;

	v = v_arg;
	netmask = netmask_arg;
	x = head->rnh_treetop;
	tt = rn_search(v, x);
	head_off = x->rn_offset;
	vlen =  *(u_char *)v;
	saved_tt = tt;
	top = x;
	if (tt == 0 ||
	    Bcmp(v + head_off, rn_get_key(tt) + head_off, vlen - head_off)) {
		return NULL;
	}
	/*
	 * Delete our route from mask lists.
	 */
	if (netmask) {
		if ((x = rn_addmask(netmask, 1, head_off)) == 0) {
			return NULL;
		}
		netmask = rn_get_key(x);
		while (rn_get_mask(tt) != netmask) {
			if ((tt = tt->rn_dupedkey) == 0) {
				return NULL;
			}
		}
	}
	if (rn_get_mask(tt) == 0 || (saved_m = m = tt->rn_mklist) == 0) {
		goto on1;
	}
	if (tt->rn_flags & RNF_NORMAL) {
		if (m->rm_leaf != tt || m->rm_refs > 0) {
			log(LOG_ERR, "rn_delete: inconsistent annotation\n");
			return NULL;  /* dangling ref could cause disaster */
		}
	} else {
		if (rm_get_mask(m) != rn_get_mask(tt)) {
			log(LOG_ERR, "rn_delete: inconsistent annotation\n");
			goto on1;
		}
		if (--m->rm_refs >= 0) {
			goto on1;
		}
	}
	b = -1 - tt->rn_bit;
	t = saved_tt->rn_parent;
	if (b > t->rn_bit) {
		goto on1; /* Wasn't lifted at all */
	}
	do {
		x = t;
		t = t->rn_parent;
	} while (b <= t->rn_bit && x != top);
	for (mp = &x->rn_mklist; (m = *mp); mp = &m->rm_mklist) {
		if (m == saved_m) {
			*mp = m->rm_mklist;
			if (tt->rn_mklist == m) {
				tt->rn_mklist = *mp;
			}
			MKFree(m);
			break;
		}
	}
	if (m == 0) {
		log(LOG_ERR, "rn_delete: couldn't find our annotation\n");
		if (tt->rn_flags & RNF_NORMAL) {
			return NULL; /* Dangling ref to us */
		}
	}
on1:
	/*
	 * Eliminate us from tree
	 */
	if (tt->rn_flags & RNF_ROOT) {
		return NULL;
	}
	head->rnh_cnt--;
#ifdef RN_DEBUG
	/* Get us out of the creation list */
	for (t = rn_clist; t && t->rn_ybro != tt; t = t->rn_ybro) {
	}
	if (t) {
		t->rn_ybro = tt->rn_ybro;
	}
#endif
	t = tt->rn_parent;
	dupedkey = saved_tt->rn_dupedkey;
	if (dupedkey) {
		/*
		 * at this point, tt is the deletion target and saved_tt
		 * is the head of the dupekey chain
		 */
		if (tt == saved_tt) {
			/* remove from head of chain */
			x = dupedkey; x->rn_parent = t;
			if (t->rn_left == tt) {
				t->rn_left = x;
			} else {
				t->rn_right = x;
			}
		} else {
			/* find node in front of tt on the chain */
			for (x = p = saved_tt; p && p->rn_dupedkey != tt;) {
				p = p->rn_dupedkey;
			}
			if (p) {
				p->rn_dupedkey = tt->rn_dupedkey;
				if (tt->rn_dupedkey) {          /* parent */
					tt->rn_dupedkey->rn_parent = p;
				}
				/* parent */
			} else {
				log(LOG_ERR, "rn_delete: couldn't find us\n");
			}
		}
		t = tt + 1;
		if (t->rn_flags & RNF_ACTIVE) {
#ifndef RN_DEBUG
			*++x = *t;
			p = t->rn_parent;
#else
			b = t->rn_info;
			*++x = *t;
			t->rn_info = b;
			p = t->rn_parent;
#endif
			if (p->rn_left == t) {
				p->rn_left = x;
			} else {
				p->rn_right = x;
			}
			x->rn_left->rn_parent = x;
			x->rn_right->rn_parent = x;
		}
		goto out;
	}
	if (t->rn_left == tt) {
		x = t->rn_right;
	} else {
		x = t->rn_left;
	}
	p = t->rn_parent;
	if (p->rn_right == t) {
		p->rn_right = x;
	} else {
		p->rn_left = x;
	}
	x->rn_parent = p;
	/*
	 * Demote routes attached to us.
	 */
	if (t->rn_mklist) {
		if (x->rn_bit >= 0) {
			for (mp = &x->rn_mklist; (m = *mp);) {
				mp = &m->rm_mklist;
			}
			*mp = t->rn_mklist;
		} else {
			/* If there are any key,mask pairs in a sibling
			 *  duped-key chain, some subset will appear sorted
			 *  in the same order attached to our mklist */
			for (m = t->rn_mklist; m && x; x = x->rn_dupedkey) {
				if (m == x->rn_mklist) {
					struct radix_mask *mm = m->rm_mklist;
					x->rn_mklist = NULL;
					if (--(m->rm_refs) < 0) {
						MKFree(m);
					}
					m = mm;
				}
			}
			if (m) {
				log(LOG_ERR, "rn_delete: Orphaned Mask "
				    "0x%llx at 0x%llx\n",
				    (uint64_t)VM_KERNEL_ADDRPERM(m),
				    (uint64_t)VM_KERNEL_ADDRPERM(x));
			}
		}
	}
	/*
	 * We may be holding an active internal node in the tree.
	 */
	x = tt + 1;
	if (t != x) {
#ifndef RN_DEBUG
		*t = *x;
#else
		b = t->rn_info;
		*t = *x;
		t->rn_info = b;
#endif
		t->rn_left->rn_parent = t;
		t->rn_right->rn_parent = t;
		p = x->rn_parent;
		if (p->rn_left == x) {
			p->rn_left = t;
		} else {
			p->rn_right = t;
		}
	}
out:
	tt->rn_flags &= ~RNF_ACTIVE;
	tt[1].rn_flags &= ~RNF_ACTIVE;
	return tt;
}

/*
 * This is the same as rn_walktree() except for the parameters and the
 * exit.
 */
static int
rn_walktree_from(struct radix_node_head *h, void *a, void *m, walktree_f_t *f,
    void *w)
{
	int error;
	struct radix_node *base, *next;
	u_char *xa = (u_char *)a;
	u_char *xm = (u_char *)m;
	struct radix_node *rn, *last;
	int stopping;
	int lastb;
	int rnh_cnt;

	/*
	 * This gets complicated because we may delete the node while
	 * applying the function f to it; we cannot simply use the next
	 * leaf as the successor node in advance, because that leaf may
	 * be removed as well during deletion when it is a clone of the
	 * current node.  When that happens, we would end up referring
	 * to an already-freed radix node as the successor node.  To get
	 * around this issue, if we detect that the radix tree has changed
	 * in dimension (smaller than before), we simply restart the walk
	 * from the top of tree.
	 */
restart:
	last = NULL;
	stopping = 0;
	rnh_cnt = h->rnh_cnt;

	/*
	 * rn_search_m is sort-of-open-coded here.
	 */
	for (rn = h->rnh_treetop; rn->rn_bit >= 0;) {
		last = rn;
		if (!(rn->rn_bmask & xm[rn->rn_offset])) {
			break;
		}

		if (rn->rn_bmask & xa[rn->rn_offset]) {
			rn = rn->rn_right;
		} else {
			rn = rn->rn_left;
		}
	}

	/*
	 * Two cases: either we stepped off the end of our mask,
	 * in which case last == rn, or we reached a leaf, in which
	 * case we want to start from the last node we looked at.
	 * Either way, last is the node we want to start from.
	 */
	rn = last;
	lastb = rn->rn_bit;

	/* First time through node, go left */
	while (rn->rn_bit >= 0) {
		rn = rn->rn_left;
	}

	while (!stopping) {
		base = rn;
		/* If at right child go back up, otherwise, go right */
		while (rn->rn_parent->rn_right == rn
		    && !(rn->rn_flags & RNF_ROOT)) {
			rn = rn->rn_parent;

			/* if went up beyond last, stop */
			if (rn->rn_bit <= lastb) {
				stopping = 1;
				/*
				 * XXX we should jump to the 'Process leaves'
				 * part, because the values of 'rn' and 'next'
				 * we compute will not be used. Not a big deal
				 * because this loop will terminate, but it is
				 * inefficient and hard to understand!
				 */
			}
		}

		/*
		 * The following code (bug fix) inherited from FreeBSD is
		 * currently disabled, because our implementation uses the
		 * RTF_PRCLONING scheme that has been abandoned in current
		 * FreeBSD release.  The scheme involves setting such a flag
		 * for the default route entry, and therefore all off-link
		 * destinations would become clones of that entry.  Enabling
		 * the following code would be problematic at this point,
		 * because the removal of default route would cause only
		 * the left-half of the tree to be traversed, leaving the
		 * right-half untouched.  If there are clones of the entry
		 * that reside in that right-half, they would not be deleted
		 * and would linger around until they expire or explicitly
		 * deleted, which is a very bad thing.
		 *
		 * This code should be uncommented only after we get rid
		 * of the RTF_PRCLONING scheme.
		 */
#if 0
		/*
		 * At the top of the tree, no need to traverse the right
		 * half, prevent the traversal of the entire tree in the
		 * case of default route.
		 */
		if (rn->rn_parent->rn_flags & RNF_ROOT) {
			stopping = 1;
		}
#endif

		/* Find the next *leaf* to start from */
		for (rn = rn->rn_parent->rn_right; rn->rn_bit >= 0;) {
			rn = rn->rn_left;
		}
		next = rn;
		/* Process leaves */
		while ((rn = base) != 0) {
			base = rn->rn_dupedkey;
			if (!(rn->rn_flags & RNF_ROOT)
			    && (error = (*f)(rn, w))) {
				return error;
			}
		}
		/* If one or more nodes got deleted, restart from top */
		if (h->rnh_cnt < rnh_cnt) {
			goto restart;
		}
		rn = next;
		if (rn->rn_flags & RNF_ROOT) {
			stopping = 1;
		}
	}
	return 0;
}

static int
rn_walktree(struct radix_node_head *h, walktree_f_t *f, void *w)
{
	int error;
	struct radix_node *base, *next;
	struct radix_node *rn;
	int rnh_cnt;

	/*
	 * This gets complicated because we may delete the node while
	 * applying the function f to it; we cannot simply use the next
	 * leaf as the successor node in advance, because that leaf may
	 * be removed as well during deletion when it is a clone of the
	 * current node.  When that happens, we would end up referring
	 * to an already-freed radix node as the successor node.  To get
	 * around this issue, if we detect that the radix tree has changed
	 * in dimension (smaller than before), we simply restart the walk
	 * from the top of tree.
	 */
restart:
	rn = h->rnh_treetop;
	rnh_cnt = h->rnh_cnt;

	/* First time through node, go left */
	while (rn->rn_bit >= 0) {
		rn = rn->rn_left;
	}
	for (;;) {
		base = rn;
		/* If at right child go back up, otherwise, go right */
		while (rn->rn_parent->rn_right == rn &&
		    (rn->rn_flags & RNF_ROOT) == 0) {
			rn = rn->rn_parent;
		}
		/* Find the next *leaf* to start from */
		for (rn = rn->rn_parent->rn_right; rn->rn_bit >= 0;) {
			rn = rn->rn_left;
		}
		next = rn;
		/* Process leaves */
		while ((rn = base) != NULL) {
			base = rn->rn_dupedkey;
			if (!(rn->rn_flags & RNF_ROOT)
			    && (error = (*f)(rn, w))) {
				return error;
			}
		}
		/* If one or more nodes got deleted, restart from top */
		if (h->rnh_cnt < rnh_cnt) {
			goto restart;
		}
		rn = next;
		if (rn->rn_flags & RNF_ROOT) {
			return 0;
		}
	}
	/* NOTREACHED */
}

int
rn_inithead(void **head, int off)
{
	struct radix_node_head *rnh;
	struct radix_node *t, *tt, *ttt;
	if (off > INT8_MAX) {
		return 0;
	}
	if (*head) {
		return 1;
	}

	rnh = zalloc_flags(radix_node_head_zone, Z_WAITOK_ZERO_NOFAIL);
	*head = rnh;
	t = rn_newpair(rn_zeros, (int8_t)max_keylen, off, rnh->rnh_nodes);
	ttt = rnh->rnh_nodes + 2;
	t->rn_right = ttt;
	t->rn_parent = t;
	tt = t->rn_left;
	tt->rn_flags = t->rn_flags = RNF_ROOT | RNF_ACTIVE;
	tt->rn_bit = (short)(-1 - off);
	*ttt = *tt;
	rn_set_key(ttt, rn_ones, (int8_t)max_keylen);
	rnh->rnh_addaddr = rn_addroute;
	rnh->rnh_deladdr = rn_delete;
	rnh->rnh_matchaddr = rn_match;
	rnh->rnh_matchaddr_args = rn_match_args;
	rnh->rnh_lookup = rn_lookup;
	rnh->rnh_lookup_args = rn_lookup_args;
	rnh->rnh_walktree = rn_walktree;
	rnh->rnh_walktree_from = rn_walktree_from;
	rnh->rnh_treetop = t;
	rnh->rnh_cnt = 3;
	return 1;
}

void
rn_init(void)
{
	char *cp, *cplim;
	struct domain *dom;

	/* lock already held when rn_init is called */
	TAILQ_FOREACH(dom, &domains, dom_entry) {
		if (dom->dom_maxrtkey > max_keylen) {
			max_keylen = dom->dom_maxrtkey;
		}
	}
	if (max_keylen == 0) {
		log(LOG_ERR,
		    "rn_init: radix functions require max_keylen be set\n");
		return;
	}
	rn_zeros = zalloc_permanent(3 * max_keylen, ZALIGN_NONE);
	rn_ones = cp = rn_zeros + max_keylen;
	addmask_key = cplim = rn_ones + max_keylen;
	while (cp < cplim) {
		*cp++ = -1;
	}
	if (rn_inithead((void **)&mask_rnhead, 0) == 0) {
		panic("rn_init 2");
	}

	radix_node_zone = zone_create("radix_node",
	    sizeof(struct radix_node) * 2 + max_keylen,
	    ZC_PGZ_USE_GUARDS | ZC_ZFREE_CLEARMEM);
}
