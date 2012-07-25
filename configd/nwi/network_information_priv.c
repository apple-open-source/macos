/*
 * Copyright (c) 2011 Apple Inc. All rights reserved.
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

#include <arpa/inet.h>
#include <notify.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdbool.h>
#include "network_information_priv.h"
#include <limits.h>

sa_family_t nwi_af_list[] = {AF_INET, AF_INET6};

static __inline__ unsigned int
nwi_state_compute_size(unsigned int n)
{
	return (offsetof(nwi_state, nwi_ifstates[n]));

}
__private_extern__
nwi_state_t
nwi_state_copy_priv(nwi_state_t src)
{
	nwi_state_t dest = NULL;

	if (src == NULL) {
		return dest;
	}

	dest = malloc(src->size);

	if (dest != NULL) {
		bcopy(src, dest, src->size);

		dest->ref = 1;
	}
	return dest;
}

__private_extern__
nwi_state_t
nwi_state_new(nwi_state_t old_state, int elems)
{
	nwi_state_t state = NULL;
	int new_size;

	if (old_state == NULL && elems == 0) {
		return NULL;
	}

	/* Need to insert a last node for each of the v4/v6 list */
	new_size = (elems != 0)?
			(sizeof(nwi_state)  + nwi_state_compute_size((elems+1) * 2)):0;

	/* Should we reallocate? */
	if (old_state != NULL) {
		if (old_state->size >= new_size) {
			return (old_state);
		}
	}

	state = malloc(new_size);
	if (state == NULL) {
		return NULL;
	}

	state->size = new_size;

	/*
	 * v4 list is stored 0 to elems,
	 * v6 list is stored elems + 1 to 2 * elems + 2
	 */
	state->ipv6_start = elems + 1;

	if (old_state != NULL) {
		state->ipv6_count = old_state->ipv6_count;
		if (state->ipv6_count > 0) {
			bcopy((void*) &old_state->nwi_ifstates[old_state->ipv6_start],
			      (void*) &state->nwi_ifstates[state->ipv6_start],
			      old_state->ipv6_count * sizeof(nwi_ifstate));
		}

		state->ipv4_count = old_state->ipv4_count;

		if (state->ipv4_count > 0) {
			bcopy((void*) old_state->nwi_ifstates,
			      (void*) state->nwi_ifstates,
			      old_state->ipv4_count * sizeof(nwi_ifstate));
		}

		free(old_state);
	} else {
		state->ipv4_count = 0;
		state->ipv6_count = 0;
	}
	nwi_state_set_last(state, AF_INET);
	nwi_state_set_last(state, AF_INET6);

	state->ref = 1;
	return state;
}

static inline
nwi_ifstate_t nwi_ifstate_get_last(nwi_state_t state, int af, uint32_t** last)
{
	uint32_t*	count;
	int		idx;

	count = (af == AF_INET)
		?&state->ipv4_count:&state->ipv6_count;

	idx = (af == AF_INET)
		?state->ipv4_count:(state->ipv6_start + state->ipv6_count);

	*last = count;

	return &state->nwi_ifstates[idx];

}

__private_extern__
void
nwi_insert_ifstate(nwi_state_t state,
		   const char* ifname, int af,
		   uint64_t flags, Rank rank,
		   void* ifa)
{
	nwi_ifstate_t 	ifstate;

	/* Will only insert unique elements in the list */
	ifstate = nwi_state_get_ifstate_with_name(state, af, ifname);

	/* Already present, just ignore it */
	if (ifstate != NULL) {
		if (ifstate->rank < rank) {
			return;
		}
	}

	if (ifstate == NULL) {
		uint32_t	*last;

		/* We need to append it as the last element */
		ifstate = nwi_ifstate_get_last(state, af, &last);
		strcpy(ifstate->ifname, ifname);
		ifstate->af_alias = NULL;
		ifstate->af = af;
		ifstate->diff_ch = NULL;
		(*last)++;
	}

	/* We need to update the address/rank/flag fields for the existing/new
	 * element */
	if (ifa != NULL) {
		switch (af) {
			case AF_INET:
				ifstate->iaddr = *((struct in_addr *) ifa);
				break;
			case AF_INET6:
				ifstate->iaddr6 = *((struct in6_addr *) ifa);
				break;
			default:
				break;
		}

	}

	ifstate->rank = rank;
	ifstate->flags = flags;

	return;
}

__private_extern__
void
nwi_state_clear(nwi_state_t state, int af)
{
	uint32_t* count;

	count = (af == AF_INET)
		?&state->ipv4_count:&state->ipv6_count;

	*count = 0;
	nwi_state_set_last(state, af);
	return;

}

__private_extern__
void
nwi_state_set_last(nwi_state_t state, int af)
{
	int last_elem_idx;

	if (state == NULL) {
		return;
	}

	/* The last element is an element with the flags set as
	 * NWI_IFSTATE_FLAGS_NOT_IN_LIST */
	last_elem_idx = (af == AF_INET)
			?state->ipv4_count
			:(state->ipv6_start + state->ipv6_count);

	state->nwi_ifstates[last_elem_idx].ifname[0] = '\0';
	state->nwi_ifstates[last_elem_idx].flags
	    |= NWI_IFSTATE_FLAGS_NOT_IN_LIST;
}

__private_extern__
void
_nwi_state_dump(int level, nwi_state_t state)
{
	const char *		addr_str;
	void *			address;
	int			i;
	char 			ntopbuf[INET6_ADDRSTRLEN];
	nwi_ifstate_t 		scan;


	if (state == NULL) {
		syslog(level, "<empty nwi_state>");
		return;
	}
	syslog(level, "nwi_state = { gen = %llu size = %u #ipv4 = %u #ipv6 = %u }",
	       state->generation_count,
	       state->size,
	       state->ipv4_count,
	       state->ipv6_count);

	if (state->ipv4_count) {
		syslog(level, "IPv4:");
		for (i = 0, scan = state->nwi_ifstates;
		     i < state->ipv4_count; i++, scan++) {
			bool has_dns = (scan->flags & NWI_IFSTATE_FLAGS_HAS_DNS) != 0;
			bool never = (scan->flags & NWI_IFSTATE_FLAGS_NOT_IN_LIST) != 0;

			address = nwi_ifstate_get_address(scan);
			addr_str =  inet_ntop(scan->af, address, ntopbuf, sizeof(ntopbuf));

			syslog(level, "    [%d]: %s%s%s%s rank %u iaddr: %s " ,
			       i, scan->ifname, scan->diff_ch != NULL?scan->diff_ch:"",
			       has_dns ? " dns" : "",
			       never ? " never" : "",
			       scan->rank,
			       addr_str);
		}
	}
	if (state->ipv6_count) {
		syslog(level, "IPv6:");
		for (i = 0, scan = state->nwi_ifstates + state->ipv6_start;
		     i < state->ipv6_count; i++, scan++) {
			bool has_dns = (scan->flags & NWI_IFSTATE_FLAGS_HAS_DNS) != 0;
			bool never = (scan->flags & NWI_IFSTATE_FLAGS_NOT_IN_LIST) != 0;

			address = nwi_ifstate_get_address(scan);
			addr_str =  inet_ntop(scan->af, address, ntopbuf, sizeof(ntopbuf));
			syslog(level, "    [%d]: %s%s%s%s rank %u iaddr6: %s ",
			       i, scan->ifname, scan->diff_ch != NULL?scan->diff_ch:"",
			       has_dns ? " dns" : "",
			       never ? " never" : "",
			       scan->rank,
			       addr_str);
		}
	}
	return;
}


#define	unchanged	""
#define added		"+"
#define deleted		"-"
#define changed		"!"

__private_extern__
void *
nwi_ifstate_get_address(nwi_ifstate_t ifstate)
{
	return (void *)&ifstate->iaddr;
}

__private_extern__
const char *
nwi_ifstate_get_diff_str(nwi_ifstate_t ifstate)
{
	return ifstate->diff_ch;
}

static
inline
boolean_t
nwi_ifstate_has_changed(nwi_ifstate_t ifstate1, nwi_ifstate_t ifstate2)
{
	if (ifstate1->rank != ifstate2->rank) {
		return TRUE;
	}

	if (ifstate1->flags != ifstate2->flags) {
		return TRUE;
	}

	if (ifstate1->af == AF_INET) {
		if (memcmp(&ifstate1->iaddr, &ifstate2->iaddr, sizeof(struct in_addr)) != 0) {
			return TRUE;
		}
	} else {
		if (memcmp(&ifstate1->iaddr6, &ifstate2->iaddr6, sizeof(struct in6_addr)) != 0) {
			return TRUE;
		}
	}
	return FALSE;
}

static
inline
nwi_ifstate_t
nwi_ifstate_append(nwi_state_t state, nwi_ifstate_t scan)
{
	nwi_ifstate_t 	new_ifstate = NULL;
	uint32_t	*last;

	new_ifstate = nwi_ifstate_get_last(state, scan->af, &last);
	memcpy(new_ifstate, scan, sizeof(*scan));
	(*last)++;
	return new_ifstate;
}

static
inline
void
nwi_ifstate_set_diff_str(nwi_ifstate_t ifstate, const char * ch)
{
	ifstate->diff_ch = ch;
}

static
void
nwi_state_merge_added(nwi_state_t state, nwi_state_t old_state,
		    nwi_state_t new_state)
{
	int idx;
	nwi_ifstate_t scan;

	/* Iterate through v4 and v6 list and annotate the diff flags */
	for (idx = 0; idx < sizeof(nwi_af_list)/sizeof(nwi_af_list[0]); idx++) {
		scan = nwi_state_get_first_ifstate(new_state, nwi_af_list[idx]);

		while (scan != NULL) {
			nwi_ifstate_t 	existing_ifstate, new_ifstate;
			const char* 	ifname;

			ifname = nwi_ifstate_get_ifname(scan);

			existing_ifstate = nwi_state_get_ifstate_with_name(old_state, scan->af, ifname);

			/* Add the element that is not in the store */
			new_ifstate = nwi_ifstate_append(state, scan);

			/* These are potentially "added" elements unless they are
			 * in the old list */
			nwi_ifstate_set_diff_str(new_ifstate, added);

			if (existing_ifstate != NULL) {
				if (nwi_ifstate_has_changed(existing_ifstate, new_ifstate) == TRUE) {
					nwi_ifstate_set_diff_str(new_ifstate, changed);
				} else {
					nwi_ifstate_set_diff_str(new_ifstate, unchanged);
				}
			}
			scan = nwi_ifstate_get_next(scan, scan->af);
		}
		nwi_state_set_last(state, nwi_af_list[idx]);
	}

	return;
}

static
void
nwi_state_merge_removed(nwi_state_t state, nwi_state_t old_state)
{
	int idx;
	nwi_ifstate_t scan;

	/* Iterate through v4 and v6 list and annotate the diff flags */
	for (idx = 0; idx < sizeof(nwi_af_list)/sizeof(nwi_af_list[0]); idx++) {
		scan = nwi_state_get_first_ifstate(old_state, nwi_af_list[idx]);

		while (scan != NULL) {
			nwi_ifstate_t 	existing_ifstate;
			const char* 	ifname;

			ifname = nwi_ifstate_get_ifname(scan);

			existing_ifstate = nwi_state_get_ifstate_with_name(state, scan->af, ifname);

			/* Any elements that has not been added means that they are removed */
			if (existing_ifstate == NULL) {
				nwi_ifstate_t new_ifstate = nwi_ifstate_append(state, scan);
				nwi_ifstate_set_diff_str(new_ifstate, deleted);
			}
			scan = nwi_ifstate_get_next(scan, scan->af);
		}
		nwi_state_set_last(state, nwi_af_list[idx]);
	}

}


__private_extern__
nwi_state_t
nwi_state_diff(nwi_state_t old_state, nwi_state_t new_state)
{
	nwi_state_t	diff;
	int		total_count = 0;

	if (old_state != NULL) {
		total_count = old_state->ipv4_count + old_state->ipv6_count;
	}

	if (new_state != NULL) {
		total_count += new_state->ipv4_count + new_state->ipv6_count;
	}

	if (total_count == 0) {
		return NULL;
	}

	diff = nwi_state_new(NULL, total_count);

	nwi_state_merge_added(diff, old_state,  new_state);
	nwi_state_merge_removed(diff, old_state);

	/* Diff consists of a nwi_state_t with annotated diff_ch's */
	return diff;
}

