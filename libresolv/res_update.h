/*-
 * SPDX-License-Identifier: ISC
 *
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1999 by Internet Software Consortium, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 *	$Id: res_update.h,v 1.3 2005/04/27 04:56:15 sra Exp $
 * $FreeBSD$
 */

#ifndef __RES_UPDATE_H
#define __RES_UPDATE_H

/*! \file */

#include <sys/types.h>
#include <arpa/nameser.h>
#include <resolv.h>
#ifndef __APPLE__
#include <isc/list.h>
#else
#define LIST(type) struct { type *head, *tail; }
#define LINK(type) struct { type *prev, *next; }
#define INIT_LIST(list) do { (list).head = NULL; (list).tail = NULL; } while (0)
#define HEAD(list) ((list).head)
#define TAIL(list) ((list).tail)
#define EMPTY(list) ((list).head == NULL)
#define PREV(elt, link) ((elt)->link.prev)
#define NEXT(elt, link) ((elt)->link.next)
#define INIT_LINK_TYPE(elt, link, type) \
	do { \
		(elt)->link.prev = (type *)(-1); \
		(elt)->link.next = (type *)(-1); \
	} while (0)
#define INIT_LINK(elt, link) INIT_LINK_TYPE(elt, link, void)
#define APPEND(list, elt, link) \
	do { \
		if ((list).tail != NULL) \
			(list).tail->link.next = (elt); \
		else \
			(list).head = (elt); \
		(elt)->link.prev = (list).tail; \
		(elt)->link.next = NULL; \
		(list).tail = (elt); \
	} while (0)
#define PREPEND(list, elt, link) \
	do { \
		if ((list).head != NULL) \
			(list).head->link.prev = (elt); \
		else \
			(list).tail = (elt); \
		(elt)->link.prev = NULL; \
		(elt)->link.next = (list).head; \
		(list).head = (elt); \
	} while (0)
#define UNLINK_TYPE(list, elt, link, type) \
	do { \
		if ((elt)->link.next != NULL) \
			(elt)->link.next->link.prev = (elt)->link.prev; \
		else \
			(list).tail = (elt)->link.prev; \
		if ((elt)->link.prev != NULL) \
			(elt)->link.prev->link.next = (elt)->link.next; \
		else \
			(list).head = (elt)->link.next; \
		INIT_LINK_TYPE(elt, link, type); \
	} while (0)
#define UNLINK(list, elt, link) \
	UNLINK_TYPE(list, elt, link, void)
#define LINKED(elt, link) ((void *)((elt)->link.prev) != (void *)(-1))
#endif

/*%
 * This RR-like structure is particular to UPDATE.
 */
struct ns_updrec {
	struct {
		struct ns_updrec *prev;
		struct ns_updrec *next;
	} r_link, r_glink;
	ns_sect		r_section;	/*%< ZONE/PREREQUISITE/UPDATE */
	char *		r_dname;	/*%< owner of the RR */
	ns_class	r_class;	/*%< class number */
	ns_type		r_type;		/*%< type number */
	u_int32_t	r_ttl;		/*%< time to live */
	u_char *	r_data;		/*%< rdata fields as text string */
	u_int		r_size;		/*%< size of r_data field */
	int		r_opcode;	/*%< type of operation */
	/* following fields for private use by the resolver/server routines */
	struct databuf *r_dp;		/*%< databuf to process */
	struct databuf *r_deldp;	/*%< databuf's deleted/overwritten */
	u_int		r_zone;		/*%< zone number on server */
};
typedef struct ns_updrec ns_updrec;
typedef struct {
	ns_updrec *head;
	ns_updrec *tail;
} ns_updque;

#define res_mkupdate		__res_mkupdate
#define res_update		__res_update
#define res_mkupdrec		__res_mkupdrec
#define res_freeupdrec		__res_freeupdrec
#define res_nmkupdate		__res_nmkupdate
#define res_nupdate		__res_nupdate

int		res_mkupdate(ns_updrec *, u_char *, int);
int		res_update(ns_updrec *);
ns_updrec *	res_mkupdrec(int, const char *, u_int, u_int, u_long);
void		res_freeupdrec(ns_updrec *);
int		res_nmkupdate(res_state, ns_updrec *, u_char *, int);
int		res_nupdate(res_state, ns_updrec *, ns_tsig_key *);

#endif /*__RES_UPDATE_H*/

/*! \file */
