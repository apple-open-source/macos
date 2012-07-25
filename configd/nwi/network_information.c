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


#include <pthread.h>
#include <notify.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include "network_information.h"
#include "network_information_priv.h"

static nwi_state_t	G_nwi_state		= NULL;
static pthread_mutex_t	nwi_store_lock		= PTHREAD_MUTEX_INITIALIZER;
static boolean_t	nwi_store_token_valid	= FALSE;

static pthread_once_t	initialized		= PTHREAD_ONCE_INIT;
static int		nwi_store_token;


/* Private */
static
void
_nwi_state_initialize(void)
{
	const char	*nwi_key	= nwi_state_get_notify_key();
	uint32_t	status		= notify_register_check(nwi_key,
								&nwi_store_token);

	if (status != NOTIFY_STATUS_OK) {
		fprintf(stderr, "nwi_state: registration failed (%u)\n", status);
	}
	else {
		nwi_store_token_valid = TRUE;
	}
}

static
void
nwi_set_alias(nwi_state* state, nwi_ifstate* ifstate)
{
	nwi_ifstate*	  ifstate_alias;
	int af = ifstate->af;
	int af_alias;

	af_alias = (af == AF_INET)?AF_INET6:AF_INET;

	ifstate_alias =
		nwi_state_get_ifstate_with_name(state, af_alias,
					ifstate->ifname);

	if (ifstate_alias != NULL) {
		ifstate_alias->af_alias = ifstate;
	}
	ifstate->af_alias = ifstate_alias;
	return;
}

static
void
_nwi_state_reset_alias(nwi_state_t state) {
	int i;

	for (i = 0; i < state->ipv4_count; i++) {
		state->nwi_ifstates[i].af_alias = NULL;
	}

	for (i = state->ipv6_start;
	     i < state->ipv6_start + state->ipv6_count; i++) {
		nwi_set_alias(state, &state->nwi_ifstates[i]);
	}
}

/* Public APIs' */
/*
 * Function: nwi_state_get_notify_key
 * Purpose:
 *   Returns the BSD notify key to use to monitor when the state changes.
 *
 * Note:
 *   The nwi_state_copy API uses this notify key to monitor when the state
 *   changes, so each invocation of nwi_state_copy returns the current
 *   information.
 */
const char *
nwi_state_get_notify_key()
{
	return "com.apple.system.SystemConfiguration.nwi";
}

#define ATOMIC_INC(p)		__sync_fetch_and_add((p), 1)		// return (n++);
#define ATOMIC_DEC(p)		__sync_sub_and_fetch((p), 1)		// return (--n);

static void
nwi_state_retain(nwi_state_t state)
{
	ATOMIC_INC(&state->ref);
	return;
}

/*
 * Function: nwi_state_release
 * Purpose:
 *   Release the memory associated with the network state.
 */
void
nwi_state_release(nwi_state_t state)
{
	if (ATOMIC_DEC(&state->ref) == 0) {
		free(state);
	}
	return;
}

/*
 * Function: nwi_state_copy
 * Purpose:
 *   Returns the current network state information.
 *   Release after use by calling nwi_state_release().
 */
nwi_state_t
nwi_state_copy(void)
{
	nwi_state_t     nwi_state = NULL;
	nwi_state_t	old_state = NULL;

	pthread_once(&initialized, _nwi_state_initialize);
	pthread_mutex_lock(&nwi_store_lock);

	if (G_nwi_state != NULL) {
		int		check = 0;
		uint32_t	status;

		if (nwi_store_token_valid == FALSE) {
			/* have to throw cached copy away every time */
			check = 1;
		}
		else {
			status = notify_check(nwi_store_token, &check);
			if (status != NOTIFY_STATUS_OK) {
				fprintf(stderr, "nwi notify_check: failed with %u\n",
					status);
				/* assume that it changed, throw cached copy away */
				check = 1;
			}
		}
		if (check != 0) {
			/* new need snapshot */
			old_state = G_nwi_state;
			G_nwi_state = NULL;
		}
	}
	/* Let's populate the cache if it's empty */
	if (G_nwi_state == NULL) {
		G_nwi_state = _nwi_state_copy();
		if (G_nwi_state != NULL) {
			/* one reference for G_nwi_state */
			nwi_state_retain(G_nwi_state);
			_nwi_state_reset_alias(G_nwi_state);
		}
	}
	if (G_nwi_state != NULL) {
		/* another reference for this caller */
		nwi_state_retain(G_nwi_state);
	}
	nwi_state = G_nwi_state;
	pthread_mutex_unlock(&nwi_store_lock);

	if (old_state != NULL) {
		/* get rid of G_nwi_state reference */
		nwi_state_release(old_state);
	}
	return nwi_state;
}

/*
 * Function: _nwi_state_ack
 * Purpose:
 *   Acknowledge receipt and any changes associated with the [new or
 *   updated] network state.
 */
void
_nwi_state_ack(nwi_state_t state, const char *bundle_id)
{
	return;
}

/*
 * Function: nwi_state_get_generation
 * Purpose:
 *   Returns the generation (mach_time) of the nwi_state data.
 *   Every time the data is updated due to changes
 *   in the network, this value will change.
 */
uint64_t
nwi_state_get_generation(nwi_state_t state)
{
	return (state->generation_count);
}

/*
 * Function: nwi_ifstate_get_ifname
 * Purpose:
 *   Return the interface name of the specified ifstate.
 */
const char *
nwi_ifstate_get_ifname(nwi_ifstate_t ifstate)
{
	return (ifstate != NULL?ifstate->ifname:NULL);

}

static uint64_t
flags_from_af(int af)
{
    return ((af == AF_INET)
	    ? NWI_IFSTATE_FLAGS_HAS_IPV4
	    : NWI_IFSTATE_FLAGS_HAS_IPV6);
}
/*
 * Function: nwi_ifstate_get_flags
 * Purpose:
 *   Return the flags for the given ifstate (see above for bit definitions).
 */
nwi_ifstate_flags
nwi_ifstate_get_flags(nwi_ifstate_t ifstate)
{
	nwi_ifstate_t		alias = ifstate->af_alias;
	nwi_ifstate_flags 	flags = 0ULL;

	flags |= flags_from_af(ifstate->af);
	if ((ifstate->flags & NWI_IFSTATE_FLAGS_HAS_DNS) != 0) {
		flags |= NWI_IFSTATE_FLAGS_HAS_DNS;

	}
	if (alias != NULL) {
		flags |= flags_from_af(alias->af);
		if ((alias->flags & NWI_IFSTATE_FLAGS_HAS_DNS) != 0) {
			flags |= NWI_IFSTATE_FLAGS_HAS_DNS;
		}
	}
	return flags;
}

/*
 * Function: nwi_state_get_first_ifstate
 * Purpose:
 *   Returns the first and highest priority interface that has connectivity
 *   for the specified address family 'af'. 'af' is either AF_INET or AF_INET6.
 *   The connectivity provided is for general networking.   To get information
 *   about an interface that isn't available for general networking, use
 *   nwi_state_get_ifstate().
 *
 *   Use nwi_ifstate_get_next() to get the next, lower priority interface
 *   in the list.
 *
 *   Returns NULL if no connectivity for the specified address family is
 *   available.
 */
nwi_ifstate_t
nwi_state_get_first_ifstate(nwi_state_t state, int af)
{
	nwi_ifstate_t ifstate;

	if (state == NULL) {
		return NULL;
	}

	ifstate =
		nwi_state_get_ifstate_with_index(state, af, 0);

	if ((ifstate->flags & NWI_IFSTATE_FLAGS_NOT_IN_LIST)
	    != 0) {
		ifstate =  NULL;
	}

	return ifstate;

}

/*
 * Function: nwi_state_get_ifstate
 * Purpose:
 *   Return information for the specified interface 'ifname'.
 *
 *   This API directly returns the ifstate for the specified interface.
 *   This is the only way to access information about an interface that isn't
 *   available for general networking.
 *
 *   Returns NULL if no information is available for that interface.
 */
nwi_ifstate_t
nwi_state_get_ifstate(nwi_state_t state, const char * ifname)
{
	nwi_ifstate_t ifstate = nwi_state_get_ifstate_with_name(state, AF_INET, ifname);
	if (ifstate == NULL) {
		ifstate = nwi_state_get_ifstate_with_name(state, AF_INET6, ifname);
	}
	return ifstate;

}

/*
 * Function: nwi_ifstate_get_next
 * Purpose:
 *   Returns the next, lower priority nwi_ifstate_t after the specified
 *   'ifstate' for the protocol family 'af'.
 *
 *   Returns NULL when the end of the list is reached.
 */
nwi_ifstate_t
nwi_ifstate_get_next(nwi_ifstate_t ifstate, int af)
{
	nwi_ifstate_t alias, next;

	alias =
		(af == ifstate->af)?ifstate:ifstate->af_alias;

	if (alias == NULL) {
		return NULL;
	}

	/* We don't return interfaces marked rank never */
	if ((alias->flags & NWI_IFSTATE_FLAGS_NOT_IN_LIST) != 0) {
		return NULL;
	}

	next = ++alias;

	if ((next->flags & NWI_IFSTATE_FLAGS_NOT_IN_LIST) == 0) {
		return next;
	}
	return NULL;
}

/*
 * Function: nwi_ifstate_compare_rank
 * Purpose:
 *   Compare the relative rank of two nwi_ifstate_t objects.
 *
 *   The "rank" indicates the importance of the underlying interface.
 *
 * Returns:
 *   0 	if ifstate1 and ifstate2 are ranked equally
 *  -1	if ifstate1 is ranked ahead of ifstate2
 *   1	if ifstate2 is ranked ahead of ifstate1
 */
int
nwi_ifstate_compare_rank(nwi_ifstate_t ifstate1, nwi_ifstate_t ifstate2)
{
	return RankCompare(ifstate1->rank, ifstate2->rank);
}
