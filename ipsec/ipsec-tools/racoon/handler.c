/*	$NetBSD: handler.c,v 1.9.6.6 2007/06/06 09:20:12 vanhu Exp $	*/

/* Id: handler.c,v 1.28 2006/05/26 12:17:29 manubsd Exp */

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "sockmisc.h"
#include "debug.h"

#ifdef ENABLE_HYBRID
#include <resolv.h>
#endif

#include "schedule.h"
#include "grabmyaddr.h"
#include "algorithm.h"
#include "crypto_openssl.h"
#include "policy.h"
#include "proposal.h"
#include "isakmp_var.h"
#include "evt.h"
#include "isakmp.h"
#ifdef ENABLE_HYBRID
#include "isakmp_xauth.h"  
#include "isakmp_cfg.h"
#endif
#include "isakmp_inf.h"
#include "oakley.h"
#include "remoteconf.h"
#include "localconf.h"
#include "handler.h"
#include "gcmalloc.h"
#include "nattraversal.h"
#include "ike_session.h"

#include "sainfo.h"

#ifdef HAVE_GSSAPI
#include "gssapi.h"
#endif
#include "power_mgmt.h"

static LIST_HEAD(_ph1tree_, ph1handle) ph1tree;
static LIST_HEAD(_ph2tree_, ph2handle) ph2tree;
static LIST_HEAD(_ctdtree_, contacted) ctdtree;
static LIST_HEAD(_rcptree_, recvdpkt) rcptree;

static void del_recvdpkt __P((struct recvdpkt *));
static void rem_recvdpkt __P((struct recvdpkt *));
static void sweep_recvdpkt __P((void *));

/*
 * functions about management of the isakmp status table
 */
/* %%% management phase 1 handler */
/*
 * search for isakmpsa handler with isakmp index.
 */

extern caddr_t val2str(const char *, size_t);

struct ph1handle *
getph1byindex(index)
	isakmp_index *index;
{
	struct ph1handle *p;

	LIST_FOREACH(p, &ph1tree, chain) {
		if (p->status == PHASE1ST_EXPIRED)
			continue;
		if (memcmp(&p->index, index, sizeof(*index)) == 0)
			return p;
	}

	return NULL;
}

/*
 * search for isakmp handler by i_ck in index.
 */
struct ph1handle *
getph1byindex0(index)
	isakmp_index *index;
{
	struct ph1handle *p;

	LIST_FOREACH(p, &ph1tree, chain) {
		if (p->status == PHASE1ST_EXPIRED)
			continue;
		if (memcmp(&p->index, index, sizeof(cookie_t)) == 0)
			return p;
	}

	return NULL;
}

/*
 * search for isakmpsa handler by source and remote address.
 * don't use port number to search because this function search
 * with phase 2's destinaion.
 */
struct ph1handle *
getph1byaddr(local, remote)
	struct sockaddr *local, *remote;
{
	struct ph1handle *p;

	plog(LLV_DEBUG2, LOCATION, NULL, "getph1byaddr: start\n");
	plog(LLV_DEBUG2, LOCATION, NULL, "local: %s\n", saddr2str(local));
	plog(LLV_DEBUG2, LOCATION, NULL, "remote: %s\n", saddr2str(remote));

	LIST_FOREACH(p, &ph1tree, chain) {
		if (p->status == PHASE1ST_EXPIRED)
			continue;
		plog(LLV_DEBUG2, LOCATION, NULL, "p->local: %s\n", saddr2str(p->local));
		plog(LLV_DEBUG2, LOCATION, NULL, "p->remote: %s\n", saddr2str(p->remote));
		if (CMPSADDR(local, p->local) == 0
			&& CMPSADDR(remote, p->remote) == 0){
			plog(LLV_DEBUG2, LOCATION, NULL, "matched\n");
			return p;
		}
	}

	plog(LLV_DEBUG2, LOCATION, NULL, "no match\n");

	return NULL;
}

struct ph1handle *
getph1byaddrwop(local, remote)
	struct sockaddr *local, *remote;
{
	struct ph1handle *p;

	LIST_FOREACH(p, &ph1tree, chain) {
		if (p->status == PHASE1ST_EXPIRED)
			continue;
		if (cmpsaddrwop(local, p->local) == 0
		 && cmpsaddrwop(remote, p->remote) == 0)
			return p;
	}

	return NULL;
}

/*
 * search for isakmpsa handler by remote address.
 * don't use port number to search because this function search
 * with phase 2's destinaion.
 */
struct ph1handle *
getph1bydstaddrwop(remote)
	struct sockaddr *remote;
{
	struct ph1handle *p;

	LIST_FOREACH(p, &ph1tree, chain) {
		if (p->status == PHASE1ST_EXPIRED)
			continue;
		if (cmpsaddrwop(remote, p->remote) == 0)
			return p;
	}

	return NULL;
}

int
islast_ph1(ph1) 
	struct ph1handle *ph1;
{
	struct ph1handle *p;

	LIST_FOREACH(p, &ph1tree, chain) {
		if (p->is_dying || p->status == PHASE1ST_EXPIRED)
			continue;
		if (CMPSADDR(ph1->remote, p->remote) == 0) {
			if (p == ph1)
				continue;
			return 0;
		}
	}
	return 1;
}

/*
 * dump isakmp-sa
 */
vchar_t *
dumpph1()
{
	struct ph1handle *iph1;
	struct ph1dump *pd;
	int cnt = 0;
	vchar_t *buf;

	/* get length of buffer */
	LIST_FOREACH(iph1, &ph1tree, chain)
		cnt++;

	buf = vmalloc(cnt * sizeof(struct ph1dump));
	if (buf == NULL) {
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to get buffer\n");
		return NULL;
	}
	pd = (struct ph1dump *)buf->v;

	LIST_FOREACH(iph1, &ph1tree, chain) {
		memcpy(&pd->index, &iph1->index, sizeof(iph1->index));
		pd->status = iph1->status;
		pd->side = iph1->side;
		memcpy(&pd->remote, iph1->remote, sysdep_sa_len(iph1->remote));
		memcpy(&pd->local, iph1->local, sysdep_sa_len(iph1->local));
		pd->version = iph1->version;
		pd->etype = iph1->etype;
		pd->created = iph1->created;
		pd->ph2cnt = iph1->ph2cnt;
		pd++;
	}

	return buf;
}

/*
 * create new isakmp Phase 1 status record to handle isakmp in Phase1
 */
struct ph1handle *
newph1()
{
	struct ph1handle *iph1;

	/* create new iph1 */
	iph1 = racoon_calloc(1, sizeof(*iph1));
	if (iph1 == NULL)
		return NULL;

	iph1->status = PHASE1ST_SPAWN;

#ifdef ENABLE_DPD
	iph1->dpd_support = 0;
	iph1->dpd_lastack = 0;
	iph1->dpd_seq = 0;
	iph1->dpd_fails = 0;
    iph1->peer_sent_ike = 0;
	iph1->dpd_r_u = NULL;
#endif
#ifdef ENABLE_VPNCONTROL_PORT
	iph1->ping_sched = NULL;
#endif
	iph1->is_dying = 0;
	return iph1;
}

/*
 * delete new isakmp Phase 1 status record to handle isakmp in Phase1
 */
void
delph1(iph1)
	struct ph1handle *iph1;
{
	if (iph1 == NULL)
		return;

	/* SA down shell script hook */
	script_hook(iph1, SCRIPT_PHASE1_DOWN);

	EVT_PUSH(iph1->local, iph1->remote, EVTT_PHASE1_DOWN, NULL);

#ifdef ENABLE_NATT
	if (iph1->natt_options) {
		racoon_free(iph1->natt_options);
		iph1->natt_options = NULL;
	}
#endif

#ifdef ENABLE_HYBRID
	if (iph1->mode_cfg)
		isakmp_cfg_rmstate(iph1);
	VPTRINIT(iph1->xauth_awaiting_userinput_msg);
#endif

#ifdef ENABLE_DPD
	if (iph1->dpd_r_u != NULL)
		SCHED_KILL(iph1->dpd_r_u);
#endif
#ifdef ENABLE_VPNCONTROL_PORT
	if (iph1->ping_sched != NULL)
		SCHED_KILL(iph1->ping_sched);
#endif

	if (iph1->remote) {
		racoon_free(iph1->remote);
		iph1->remote = NULL;
	}
	if (iph1->local) {
		racoon_free(iph1->local);
		iph1->local = NULL;
	}

	if (iph1->approval) {
		delisakmpsa(iph1->approval);
		iph1->approval = NULL;
	}

	VPTRINIT(iph1->authstr);

	sched_scrub_param(iph1);
	iph1->sce = NULL;
	iph1->sce_rekey = NULL;
	iph1->scr = NULL;

	VPTRINIT(iph1->sendbuf);

	VPTRINIT(iph1->dhpriv);
	VPTRINIT(iph1->dhpub);
	VPTRINIT(iph1->dhpub_p);
	VPTRINIT(iph1->dhgxy);
	VPTRINIT(iph1->nonce);
	VPTRINIT(iph1->nonce_p);
	VPTRINIT(iph1->skeyid);
	VPTRINIT(iph1->skeyid_d);
	VPTRINIT(iph1->skeyid_a);
	VPTRINIT(iph1->skeyid_e);
	VPTRINIT(iph1->key);
	VPTRINIT(iph1->hash);
	VPTRINIT(iph1->sig);
	VPTRINIT(iph1->sig_p);
	oakley_delcert(iph1->cert);
	iph1->cert = NULL;
	oakley_delcert(iph1->cert_p);
	iph1->cert_p = NULL;
	oakley_delcert(iph1->crl_p);
	iph1->crl_p = NULL;
	oakley_delcert(iph1->cr_p);
	iph1->cr_p = NULL;
	VPTRINIT(iph1->id);
	VPTRINIT(iph1->id_p);

	if(iph1->approval != NULL)
		delisakmpsa(iph1->approval);

	if (iph1->ivm) {
		oakley_delivm(iph1->ivm);
		iph1->ivm = NULL;
	}

	VPTRINIT(iph1->sa);
	VPTRINIT(iph1->sa_ret);

#ifdef HAVE_GSSAPI
	VPTRINIT(iph1->gi_i);
	VPTRINIT(iph1->gi_r);

	gssapi_free_state(iph1);
#endif

	if (iph1->parent_session) {
		ike_session_unlink_ph1_from_session(iph1);
	}
	if (iph1->rmconf) {
		unlink_rmconf_from_ph1(iph1->rmconf);
		iph1->rmconf = NULL;
	}
	
	racoon_free(iph1);
}

/*
 * create new isakmp Phase 1 status record to handle isakmp in Phase1
 */
int
insph1(iph1)
	struct ph1handle *iph1;
{
	/* validity check */
	if (iph1->remote == NULL) {
		plog(LLV_ERROR, LOCATION, NULL,
			"invalid isakmp SA handler. no remote address.\n");
		return -1;
	}
	LIST_INSERT_HEAD(&ph1tree, iph1, chain);

	return 0;
}

void
remph1(iph1)
	struct ph1handle *iph1;
{
	LIST_REMOVE(iph1, chain);
}

/*
 * flush isakmp-sa
 */
void
flushph1(int ignore_estab_or_assert_handles)
{
	struct ph1handle *p, *next;
	
	plog(LLV_DEBUG2, LOCATION, NULL,
		 "flushing ph1 handles: ignore_estab_or_assert %d...\n", ignore_estab_or_assert_handles);

	for (p = LIST_FIRST(&ph1tree); p; p = next) {
		next = LIST_NEXT(p, chain);

		if (ignore_estab_or_assert_handles && p->parent_session && !p->parent_session->stopped_by_vpn_controller && p->parent_session->is_asserted) {
			plog(LLV_DEBUG2, LOCATION, NULL,
				 "skipping phase1 %s that's asserted...\n",
				 isakmp_pindex(&p->index, 0));
			continue;
		}

		/* send delete information */
		if (p->status == PHASE1ST_ESTABLISHED) {
			if (ignore_estab_or_assert_handles &&
			    ike_session_has_negoing_ph2(p->parent_session)) {
				plog(LLV_DEBUG2, LOCATION, NULL,
					 "skipping phase1 %s that's established... because it's needed by children phase2s\n",
					 isakmp_pindex(&p->index, 0));
			    continue;
		    }
			/* send delete information */
			plog(LLV_DEBUG2, LOCATION, NULL,
				 "got a phase1 %s to flush...\n",
				 isakmp_pindex(&p->index, 0));
			isakmp_info_send_d1(p);
		}

		ike_session_stopped_by_controller(p->parent_session,
										  ike_session_stopped_by_flush);
		remph1(p);
		delph1(p);
	}
}

void
initph1tree()
{
	LIST_INIT(&ph1tree);
}

/* %%% management phase 2 handler */
/*
 * search ph2handle with policy id.
 */
struct ph2handle *
getph2byspid(spid)
      u_int32_t spid;
{
	struct ph2handle *p;

	LIST_FOREACH(p, &ph2tree, chain) {
		/*
		 * there are ph2handle independent on policy
		 * such like informational exchange.
		 */
		if (p->spid == spid)
			return p;
	}

	return NULL;
}

/*
 * search ph2handle with sequence number.
 */
struct ph2handle *
getph2byseq(seq)
	u_int32_t seq;
{
	struct ph2handle *p;

	LIST_FOREACH(p, &ph2tree, chain) {
		if (p->seq == seq)
			return p;
	}

	return NULL;
}

/*
 * search ph2handle with message id.
 */
struct ph2handle *
getph2bymsgid(iph1, msgid)
	struct ph1handle *iph1;
	u_int32_t msgid;
{
	struct ph2handle *p;

	LIST_FOREACH(p, &ph2tree, chain) {
		if (p->msgid == msgid)
			return p;
	}

	return NULL;
}

struct ph2handle *
getph2byid(src, dst, spid)
	struct sockaddr *src, *dst;
	u_int32_t spid;
{
	struct ph2handle *p;

	LIST_FOREACH(p, &ph2tree, chain) {
		if (spid == p->spid &&
		    CMPSADDR(src, p->src) == 0 &&
		    CMPSADDR(dst, p->dst) == 0){
			/* Sanity check to detect zombie handlers
			 * XXX Sould be done "somewhere" more interesting,
			 * because we have lots of getph2byxxxx(), but this one
			 * is called by pk_recvacquire(), so is the most important.
			 */
			if(p->status < PHASE2ST_ESTABLISHED &&
			   p->retry_counter == 0
			   && p->sce == NULL && p->scr == NULL){
				plog(LLV_DEBUG, LOCATION, NULL,
					 "Zombie ph2 found, expiring it\n");
				isakmp_ph2expire(p);
			}else
				return p;
		}
	}

	return NULL;
}

struct ph2handle *
getph2bysaddr(src, dst)
	struct sockaddr *src, *dst;
{
	struct ph2handle *p;

	LIST_FOREACH(p, &ph2tree, chain) {
		if (cmpsaddrstrict(src, p->src) == 0 &&
		    cmpsaddrstrict(dst, p->dst) == 0)
			return p;
	}

	return NULL;
}

/*
 * call by pk_recvexpire().
 */
struct ph2handle *
getph2bysaidx(src, dst, proto_id, spi)
	struct sockaddr *src, *dst;
	u_int proto_id;
	u_int32_t spi;
{
	struct ph2handle *iph2;
	struct saproto *pr;

	LIST_FOREACH(iph2, &ph2tree, chain) {
		if (iph2->proposal == NULL && iph2->approval == NULL)
			continue;
		if (iph2->approval != NULL) {
			for (pr = iph2->approval->head; pr != NULL;
			     pr = pr->next) {
				if (proto_id != pr->proto_id)
					break;
				if (spi == pr->spi || spi == pr->spi_p)
					return iph2;
			}
		} else if (iph2->proposal != NULL) {
			for (pr = iph2->proposal->head; pr != NULL;
			     pr = pr->next) {
				if (proto_id != pr->proto_id)
					break;
				if (spi == pr->spi)
					return iph2;
			}
		}
	}

	return NULL;
}

/*
 * create new isakmp Phase 2 status record to handle isakmp in Phase2
 */
struct ph2handle *
newph2()
{
	struct ph2handle *iph2 = NULL;

	/* create new iph2 */
	iph2 = racoon_calloc(1, sizeof(*iph2));
	if (iph2 == NULL)
		return NULL;

	iph2->status = PHASE1ST_SPAWN;
	iph2->is_dying = 0;

	return iph2;
}

/*
 * initialize ph2handle
 * NOTE: don't initialize src/dst.
 *       SPI in the proposal is cleared.
 */
void
initph2(iph2)
	struct ph2handle *iph2;
{
	sched_scrub_param(iph2);
	iph2->sce = NULL;
	iph2->scr = NULL;

	VPTRINIT(iph2->sendbuf);
	VPTRINIT(iph2->msg1);

	/* clear spi, keep variables in the proposal */
	if (iph2->proposal) {
		struct saproto *pr;
		for (pr = iph2->proposal->head; pr != NULL; pr = pr->next)
			pr->spi = 0;
	}

	/* clear approval */
	if (iph2->approval) {
		flushsaprop(iph2->approval);
		iph2->approval = NULL;
	}

	/* clear the generated policy */
	if (iph2->spidx_gen) {
		delsp_bothdir((struct policyindex *)iph2->spidx_gen);
		racoon_free(iph2->spidx_gen);
		iph2->spidx_gen = NULL;
	}

	if (iph2->pfsgrp) {
		oakley_dhgrp_free(iph2->pfsgrp);
		iph2->pfsgrp = NULL;
	}

	VPTRINIT(iph2->dhpriv);
	VPTRINIT(iph2->dhpub);
	VPTRINIT(iph2->dhpub_p);
	VPTRINIT(iph2->dhgxy);
	VPTRINIT(iph2->id);
	VPTRINIT(iph2->id_p);
	VPTRINIT(iph2->nonce);
	VPTRINIT(iph2->nonce_p);
	VPTRINIT(iph2->sa);
	VPTRINIT(iph2->sa_ret);

	if (iph2->ivm) {
		oakley_delivm(iph2->ivm);
		iph2->ivm = NULL;
	}
}

/*
 * delete new isakmp Phase 2 status record to handle isakmp in Phase2
 */
void
delph2(iph2)
	struct ph2handle *iph2;
{
	initph2(iph2);

	if (iph2->src) {
		racoon_free(iph2->src);
		iph2->src = NULL;
	}
	if (iph2->dst) {
		racoon_free(iph2->dst);
		iph2->dst = NULL;
	}
	if (iph2->src_id) {
	      racoon_free(iph2->src_id);
	      iph2->src_id = NULL;
	}
	if (iph2->dst_id) {
	      racoon_free(iph2->dst_id);
	      iph2->dst_id = NULL;
	}

	if (iph2->proposal) {
		flushsaprop(iph2->proposal);
		iph2->proposal = NULL;
	}

	if (iph2->parent_session) {
		ike_session_unlink_ph2_from_session(iph2);
	}
	if (iph2->sainfo) {
		unlink_sainfo_from_ph2(iph2->sainfo);
		iph2->sainfo = NULL;
	}
	if (iph2->ext_nat_id) {
		vfree(iph2->ext_nat_id);
		iph2->ext_nat_id = NULL;
	}
	if (iph2->ext_nat_id_p) {
		vfree(iph2->ext_nat_id_p);
		iph2->ext_nat_id_p = NULL;
	}

	racoon_free(iph2);
}

/*
 * create new isakmp Phase 2 status record to handle isakmp in Phase2
 */
int
insph2(iph2)
	struct ph2handle *iph2;
{
	LIST_INSERT_HEAD(&ph2tree, iph2, chain);

	return 0;
}

void
remph2(iph2)
	struct ph2handle *iph2;
{
	LIST_REMOVE(iph2, chain);
}

void
initph2tree()
{
	LIST_INIT(&ph2tree);
}

void
flushph2(int ignore_estab_or_assert_handles)
{
	struct ph2handle *p, *next;

	plog(LLV_DEBUG2, LOCATION, NULL,
		 "flushing ph2 handles: ignore_estab_or_assert %d...\n", ignore_estab_or_assert_handles);

	for (p = LIST_FIRST(&ph2tree); p; p = next) {
		next = LIST_NEXT(p, chain);
		if (p->is_dying || p->status == PHASE2ST_EXPIRED) {
			continue;
		}
		if (ignore_estab_or_assert_handles && p->parent_session && !p->parent_session->stopped_by_vpn_controller && p->parent_session->is_asserted) {
			plog(LLV_DEBUG2, LOCATION, NULL,
				 "skipping phase2 handle that's asserted...\n");
			continue;
		}
		if (p->status == PHASE2ST_ESTABLISHED){
			if (ignore_estab_or_assert_handles) {
				plog(LLV_DEBUG2, LOCATION, NULL,
					 "skipping ph2 handler that's established...\n");
			    continue;
		    }
			/* send delete information */
			plog(LLV_DEBUG2, LOCATION, NULL,
				 "got an established ph2 handler to flush...\n");
			isakmp_info_send_d2(p);
		}else{
			plog(LLV_DEBUG2, LOCATION, NULL,
				 "got a ph2 handler to flush (state %d)\n", p->status);
		}
		
		ike_session_stopped_by_controller(p->parent_session,
										  ike_session_stopped_by_flush);
		delete_spd(p);
		unbindph12(p);
		remph2(p);
		delph2(p);
	}
}

/*
 * Delete all Phase 2 handlers for this src/dst/proto.  This
 * is used during INITIAL-CONTACT processing (so no need to
 * send a message to the peer).
 */
void
deleteallph2(src, dst, proto_id)
	struct sockaddr *src, *dst;
	u_int proto_id;
{
	struct ph2handle *iph2, *next;
	struct saproto *pr;

	for (iph2 = LIST_FIRST(&ph2tree); iph2 != NULL; iph2 = next) {
		next = LIST_NEXT(iph2, chain);
		if (iph2->is_dying || iph2->status == PHASE2ST_EXPIRED) {
			continue;
		}
		if (iph2->proposal == NULL && iph2->approval == NULL)
			continue;
		if (cmpsaddrwop(src, iph2->src) != 0 ||
		    cmpsaddrwop(dst, iph2->dst) != 0) {
            continue;
        }
        if (iph2->approval != NULL) {
			for (pr = iph2->approval->head; pr != NULL;
			     pr = pr->next) {
				if (proto_id == pr->proto_id)
					goto zap_it;
			}
		} else if (iph2->proposal != NULL) {
			for (pr = iph2->proposal->head; pr != NULL;
			     pr = pr->next) {
				if (proto_id == pr->proto_id)
					goto zap_it;
			}
		}
		continue;
 zap_it:
        plog(LLV_DEBUG2, LOCATION, NULL,
             "deleteallph2: got a ph2 handler...\n");
        if (iph2->status == PHASE2ST_ESTABLISHED)
            isakmp_info_send_d2(iph2);
        ike_session_stopped_by_controller(iph2->parent_session,
                                          ike_session_stopped_by_flush);
		unbindph12(iph2);
		remph2(iph2);
		delph2(iph2);
	}
}

/*
 * Delete all Phase 1 handlers for this src/dst.
 */
void
deleteallph1(src, dst)
struct sockaddr *src, *dst;
{
	struct ph1handle *iph1, *next;

	for (iph1 = LIST_FIRST(&ph1tree); iph1 != NULL; iph1 = next) {
		next = LIST_NEXT(iph1, chain);
		if (cmpsaddrwop(src, iph1->local) != 0 ||
		    cmpsaddrwop(dst, iph1->remote) != 0) {
			continue;
        }
        plog(LLV_DEBUG2, LOCATION, NULL,
             "deleteallph1: got a ph1 handler...\n");
        if (iph1->status == PHASE2ST_ESTABLISHED)
		isakmp_info_send_d1(iph1);

		ike_session_stopped_by_controller(iph1->parent_session,
						  ike_session_stopped_by_flush);
		remph1(iph1);
		delph1(iph1);
	}
}

/* %%% */
void
bindph12(iph1, iph2)
	struct ph1handle *iph1;
	struct ph2handle *iph2;
{
	if (iph2->ph1 && (struct ph1handle *)iph2->ph1bind.le_next == iph1) {
		plog(LLV_ERROR, LOCATION, NULL, "duplicate %s.\n", __FUNCTION__);		
	}
	iph2->ph1 = iph1;
	LIST_INSERT_HEAD(&iph1->ph2tree, iph2, ph1bind);
}

void
unbindph12(iph2)
	struct ph2handle *iph2;
{
	if (iph2->ph1 != NULL) {
		plog(LLV_DEBUG, LOCATION, NULL, "unbindph12.\n");
		iph2->ph1 = NULL;
		LIST_REMOVE(iph2, ph1bind);
	}
}

void
rebindph12(new_ph1, iph2)
struct ph1handle *new_ph1;
struct ph2handle *iph2;
{
	if (!new_ph1) {
		return;
	}

	// reconcile the ph1-to-ph2 binding
	plog(LLV_DEBUG, LOCATION, NULL, "rebindph12.\n");
	unbindph12(iph2);
	bindph12(new_ph1, iph2);
	// recalculate ivm since ph1 binding has changed
	if (iph2->ivm != NULL) {
		oakley_delivm(iph2->ivm);
		if (new_ph1->status == PHASE1ST_ESTABLISHED) {
			iph2->ivm = oakley_newiv2(new_ph1, iph2->msgid);
			plog(LLV_DEBUG, LOCATION, NULL, "ph12 binding changed... recalculated ivm.\n");
		} else {
			iph2->ivm = NULL;
		}
	}
}

/* %%% management contacted list */
/*
 * search contacted list.
 */
struct contacted *
getcontacted(remote)
	struct sockaddr *remote;
{
	struct contacted *p;

	LIST_FOREACH(p, &ctdtree, chain) {
		if (cmpsaddrstrict(remote, p->remote) == 0)
			return p;
	}

	return NULL;
}

/*
 * create new isakmp Phase 2 status record to handle isakmp in Phase2
 */
int
inscontacted(remote)
	struct sockaddr *remote;
{
	struct contacted *new;

	/* create new iph2 */
	new = racoon_calloc(1, sizeof(*new));
	if (new == NULL)
		return -1;

	new->remote = dupsaddr(remote);
	if (new->remote == NULL) {
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to allocate buffer.\n");
		racoon_free(new);
		return -1;
	}

	LIST_INSERT_HEAD(&ctdtree, new, chain);

	return 0;
}


void
clear_contacted()
{
	struct contacted *c, *next;
	
	for (c = LIST_FIRST(&ctdtree); c; c = next) {
		next = LIST_NEXT(c, chain);
		LIST_REMOVE(c, chain);
		racoon_free(c->remote);
		racoon_free(c);
	}
}

void
initctdtree()
{
	LIST_INIT(&ctdtree);
}

time_t
get_exp_retx_interval (int num_retries, int fixed_retry_interval)
{
	// first 3 retries aren't exponential
	if (num_retries <= 3) {
		return (time_t)fixed_retry_interval;
	} else {
		return (time_t)(num_retries * fixed_retry_interval);
	}
}

/*
 * check the response has been sent to the peer.  when not, simply reply
 * the buffered packet to the peer.
 * OUT:
 *	 0:	the packet is received at the first time.
 *	 1:	the packet was processed before.
 *	 2:	the packet was processed before, but the address mismatches.
 *	-1:	error happened.
 */
int
check_recvdpkt(remote, local, rbuf)
	struct sockaddr *remote, *local;
	vchar_t *rbuf;
{
	vchar_t *hash;
	struct recvdpkt *r;
	time_t t, d;
	int len, s;

	/* set current time */
	t = time(NULL);

	hash = eay_md5_one(rbuf);
	if (!hash) {
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to allocate buffer.\n");
		return -1;
	}

	LIST_FOREACH(r, &rcptree, chain) {
		if (memcmp(hash->v, r->hash->v, r->hash->l) == 0)
			break;
	}
	vfree(hash);

	/* this is the first time to receive the packet */
	if (r == NULL)
		return 0;

	/*
	 * the packet was processed before, but the remote address mismatches.
         * ignore the port to accomodate port changes (e.g. floating).
	 */
	if (cmpsaddrwop(remote, r->remote) != 0) {
	        return 2;
        }

	/*
	 * it should not check the local address because the packet
	 * may arrive at other interface.
	 */

	/* check the previous time to send */
	if (t - r->time_send < 1) {
		plog(LLV_WARNING, LOCATION, NULL,
			"the packet retransmitted in a short time from %s\n",
			saddr2str(remote));
		/*XXX should it be error ? */
	}

	/* select the socket to be sent */
	s = getsockmyaddr(r->local);
	if (s == -1)
		return -1;

	// don't send if we recently sent a response.
	if (r->time_send && t > r->time_send) {
		d = t - r->time_send;
		if (d  < r->retry_interval) {
			plog(LLV_ERROR, LOCATION, NULL, "already responded within the past %ld secs\n", d);
			return 1;
		}
	}

#ifdef ENABLE_FRAG
	if (r->frag_flags && r->sendbuf->l > ISAKMP_FRAG_MAXLEN) {
		/* resend the packet if needed */
		plog(LLV_ERROR, LOCATION, NULL, "!!! retransmitting frags\n");
		len = sendfragsfromto(s, r->sendbuf,
							  r->local, r->remote, lcconf->count_persend,
							  r->frag_flags);
	} else {
		plog(LLV_ERROR, LOCATION, NULL, "!!! skipped retransmitting frags: frag_flags %x, r->sendbuf->l %d, max %d\n", r->frag_flags, r->sendbuf->l, ISAKMP_FRAG_MAXLEN);
		/* resend the packet if needed */
		len = sendfromto(s, r->sendbuf->v, r->sendbuf->l,
						 r->local, r->remote, lcconf->count_persend);
	}
#else
	/* resend the packet if needed */
	len = sendfromto(s, r->sendbuf->v, r->sendbuf->l,
			r->local, r->remote, lcconf->count_persend);
#endif
	if (len == -1) {
		plog(LLV_ERROR, LOCATION, NULL, "sendfromto failed\n");
		return -1;
	}

	/* check the retry counter */
	r->retry_counter--;
	if (r->retry_counter <= 0) {
		rem_recvdpkt(r);
		del_recvdpkt(r);
		plog(LLV_DEBUG, LOCATION, NULL,
			"deleted the retransmission packet to %s.\n",
			saddr2str(remote));
	} else {
		r->time_send = t;
		r->retry_interval = get_exp_retx_interval((lcconf->retry_counter - r->retry_counter),
												  lcconf->retry_interval);
	}

	return 1;
}

/*
 * adding a hash of received packet into the received list.
 */
int
add_recvdpkt(remote, local, sbuf, rbuf, non_esp, frag_flags)
	struct sockaddr *remote, *local;
	vchar_t *sbuf, *rbuf;
    size_t non_esp;
    u_int32_t frag_flags;
{
	struct recvdpkt *new = NULL;

	if (lcconf->retry_counter == 0) {
		/* no need to add it */
		return 0;
	}

	new = racoon_calloc(1, sizeof(*new));
	if (!new) {
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to allocate buffer.\n");
		return -1;
	}

	new->hash = eay_md5_one(rbuf);
	if (!new->hash) {
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to allocate buffer.\n");
		del_recvdpkt(new);
		return -1;
	}
	new->remote = dupsaddr(remote);
	if (new->remote == NULL) {
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to allocate buffer.\n");
		del_recvdpkt(new);
		return -1;
	}
	new->local = dupsaddr(local);
	if (new->local == NULL) {
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to allocate buffer.\n");
		del_recvdpkt(new);
		return -1;
	}

	if (non_esp) {
		plog (LLV_DEBUG, LOCATION, NULL, "Adding NON-ESP marker\n");

        /* If NAT-T port floating is in use, 4 zero bytes (non-ESP marker) 
         must added just before the packet itself. For this we must 
         allocate a new buffer and release it at the end. */
        if ((new->sendbuf = vmalloc (sbuf->l + non_esp)) == NULL) {
            plog(LLV_ERROR, LOCATION, NULL, 
                 "failed to allocate extra buf for non-esp\n");
            del_recvdpkt(new);
            return -1;
        }
        *(u_int32_t *)new->sendbuf->v = 0;
        memcpy(new->sendbuf->v + non_esp, sbuf->v, sbuf->l);
    } else {
        new->sendbuf = vdup(sbuf);
        if (new->sendbuf == NULL) {
            plog(LLV_ERROR, LOCATION, NULL,
                 "failed to allocate buffer.\n");
            del_recvdpkt(new);
            return -1;
        }
    }

	new->retry_counter = lcconf->retry_counter;
	new->time_send = 0;
	new->created = time(NULL);
#ifdef ENABLE_FRAG
	if (frag_flags) {
		new->frag_flags = frag_flags;
	}
#endif
	new->retry_interval = get_exp_retx_interval((lcconf->retry_counter - new->retry_counter),
												lcconf->retry_interval);

	LIST_INSERT_HEAD(&rcptree, new, chain);

	return 0;
}

void
del_recvdpkt(r)
	struct recvdpkt *r;
{
	if (r->remote)
		racoon_free(r->remote);
	if (r->local)
		racoon_free(r->local);
	if (r->hash)
		vfree(r->hash);
	if (r->sendbuf)
		vfree(r->sendbuf);
	racoon_free(r);
}

void
rem_recvdpkt(r)
	struct recvdpkt *r;
{
	LIST_REMOVE(r, chain);
}

void
sweep_recvdpkt(dummy)
	void *dummy;
{
	struct recvdpkt *r, *next;
	time_t t, lt;

	/* set current time */
	t = time(NULL);

	/* set the lifetime of the retransmission */
	lt = lcconf->retry_counter * lcconf->retry_interval;

	for (r = LIST_FIRST(&rcptree); r; r = next) {
		next = LIST_NEXT(r, chain);

		if (t - r->created > lt) {
			rem_recvdpkt(r);
			del_recvdpkt(r);
		}
	}

	sched_new(lt, sweep_recvdpkt, &rcptree);
}

void
clear_recvdpkt()
{
	struct recvdpkt *r, *next;
	
	for (r = LIST_FIRST(&rcptree); r; r = next) {
		next = LIST_NEXT(r, chain);
		rem_recvdpkt(r);
		del_recvdpkt(r);
	}
	sched_scrub_param(&rcptree);
}

void
init_recvdpkt()
{
	time_t lt = lcconf->retry_counter * lcconf->retry_interval;

	LIST_INIT(&rcptree);

	sched_new(lt, sweep_recvdpkt, &rcptree);
}

#ifdef ENABLE_HYBRID
/* 
 * Returns 0 if the address was obtained by ISAKMP mode config, 1 otherwise
 * This should be in isakmp_cfg.c but ph1tree being private, it must be there
 */
int
exclude_cfg_addr(addr)
	const struct sockaddr *addr;
{
	struct ph1handle *p;
	struct sockaddr_in *sin;

	LIST_FOREACH(p, &ph1tree, chain) {
		if ((p->mode_cfg != NULL) &&
		    (p->mode_cfg->flags & ISAKMP_CFG_GOT_ADDR4) &&
		    (addr->sa_family == AF_INET)) {
			sin = (struct sockaddr_in *)addr;
			if (sin->sin_addr.s_addr == p->mode_cfg->addr4.s_addr)
				return 0;
		}
	}

	return 1;
}
#endif

#ifdef ENABLE_HYBRID
struct ph1handle *
getph1bylogin(login)
	char *login;
{
	struct ph1handle *p;

	LIST_FOREACH(p, &ph1tree, chain) {
		if (p->mode_cfg == NULL)
			continue;
		if (strncmp(p->mode_cfg->login, login, LOGINLEN) == 0)
			return p;
	}

	return NULL;
}

int
purgeph1bylogin(login)
	char *login;
{
	struct ph1handle *p;
	int found = 0;

	LIST_FOREACH(p, &ph1tree, chain) {
		if (p->mode_cfg == NULL)
			continue;
		if (strncmp(p->mode_cfg->login, login, LOGINLEN) == 0) {
			if (p->status == PHASE1ST_ESTABLISHED)
				isakmp_info_send_d1(p);
			purge_remote(p);
			found++;
		}
	}

	return found;
}

int
purgephXbydstaddrwop(remote)
struct sockaddr *remote;
{
	int    found = 0;
	struct ph1handle *p;
	struct ph2handle *p2;

	LIST_FOREACH(p2, &ph2tree, chain) {
		if (cmpsaddrwop(remote, p2->dst) == 0) {
            plog(LLV_WARNING, LOCATION, NULL,
                 "in %s... purging phase2s\n", __FUNCTION__);
			if (p2->status == PHASE2ST_ESTABLISHED)
				isakmp_info_send_d2(p2);
			if (p2->status < PHASE2ST_EXPIRED) {
				isakmp_ph2expire(p2);
			} else {
				isakmp_ph2delete(p2);
			}
			found++;
		}
	}

	LIST_FOREACH(p, &ph1tree, chain) {
		if (cmpsaddrwop(remote, p->remote) == 0) {
            plog(LLV_WARNING, LOCATION, NULL,
                 "in %s... purging phase1 and related phase2s\n", __FUNCTION__);
            ike_session_purge_ph2s_by_ph1(p);
			if (p->status == PHASE1ST_ESTABLISHED)
				isakmp_info_send_d1(p);
            isakmp_ph1expire(p);
			found++;
		}
	}

	return found;
}

void
purgephXbyspid(u_int32_t spid,
               int       del_boundph1)
{
	struct ph2handle *iph2;
    struct ph1handle *iph1;

    // do ph2's first... we need the ph1s for notifications
	LIST_FOREACH(iph2, &ph2tree, chain) {
		if (spid == iph2->spid) {
			if (iph2->is_dying || iph2->status == PHASE2ST_EXPIRED) {
				continue;
			}
            if (iph2->status == PHASE2ST_ESTABLISHED) {
                isakmp_info_send_d2(iph2);
            }
            ike_session_stopped_by_controller(iph2->parent_session,
                                              ike_session_stopped_by_flush);
            isakmp_ph2expire(iph2); // iph2 will go down 1 second later.
        }
    }

    // do the ph1s last.
	LIST_FOREACH(iph2, &ph2tree, chain) {
		if (spid == iph2->spid) {
            if (del_boundph1 && iph2->parent_session) {
                for (iph1 = LIST_FIRST(&iph2->parent_session->ikev1_state.ph1tree); iph1; iph1 = LIST_NEXT(iph1, ph1ofsession_chain)) {
					if (iph1->is_dying || iph1->status == PHASE1ST_EXPIRED) {
						continue;
					}
                    if (iph1->status == PHASE1ST_ESTABLISHED) {
                        isakmp_info_send_d1(iph1);
                    }
                    isakmp_ph1expire(iph1);
                }
            }
		}
	}
}

#endif

#ifdef ENABLE_DPD
int
ph1_force_dpd (struct sockaddr *remote)
{
    int status = -1;
    struct ph1handle *p;

    LIST_FOREACH(p, &ph1tree, chain) {
        if (cmpsaddrwop(remote, p->remote) == 0) {
            if (p->status == PHASE1ST_ESTABLISHED &&
                !p->is_dying &&
                p->dpd_support &&
                p->rmconf->dpd_interval) {
                if(!p->dpd_fails) {
                    isakmp_info_send_r_u(p);
                    status = 0;
                } else {
                    plog(LLV_DEBUG2, LOCATION, NULL, "skipping forced-DPD for phase1 (dpd already in progress).\n");
                }
                if (p->parent_session) {
                    p->parent_session->controller_awaiting_peer_resp = 1;
                }
            } else {
                plog(LLV_DEBUG2, LOCATION, NULL, "skipping forced-DPD for phase1 (status %d, dying %d, dpd-support %d, dpd-interval %d).\n",
                     p->status, p->is_dying, p->dpd_support, p->rmconf->dpd_interval);
            }
        }
    }

	return status;
}
#endif

void
sweep_sleepwake(void)
{
	struct ph2handle *iph2;
	struct ph1handle *iph1;

	// do the ph1s.
	LIST_FOREACH(iph1, &ph1tree, chain) {
		if (iph1->parent_session && iph1->parent_session->is_asserted) {
			plog(LLV_DEBUG2, LOCATION, NULL, "skipping sweep of phase1 %s because it's been asserted.\n",
				 isakmp_pindex(&iph1->index, 0));
			continue;
		}
		if (iph1->is_dying || iph1->status >= PHASE1ST_EXPIRED) {
			plog(LLV_DEBUG2, LOCATION, NULL, "skipping sweep of phase1 %s because it's already expired.\n",
				 isakmp_pindex(&iph1->index, 0));
			continue;
		}
		if (iph1->sce) {
			if (iph1->sce->xtime <= swept_at) {
				SCHED_KILL(iph1->sce);
				SCHED_KILL(iph1->sce_rekey);
				iph1->is_dying = 1;
				iph1->status = PHASE1ST_EXPIRED;
				ike_session_update_ph1_ph2tree(iph1); // move unbind/rebind ph2s to from current ph1
				iph1->sce = sched_new(1, isakmp_ph1delete_stub, iph1);
				plog(LLV_DEBUG2, LOCATION, NULL, "phase1 %s expired while sleeping: quick deletion.\n",
				     isakmp_pindex(&iph1->index, 0));
			}
		}
		if (iph1->sce_rekey) {
			if (iph1->status == PHASE1ST_EXPIRED || iph1->sce_rekey->xtime <= swept_at) {
				SCHED_KILL(iph1->sce_rekey);
			}
		}
		if (iph1->scr) {
			if (iph1->status == PHASE1ST_EXPIRED || iph1->scr->xtime <= swept_at) {
				SCHED_KILL(iph1->scr);
			}
		}
#ifdef ENABLE_DPD
		if (iph1->dpd_r_u) {
			if (iph1->status == PHASE1ST_EXPIRED || iph1->dpd_r_u->xtime <= swept_at) {
				SCHED_KILL(iph1->dpd_r_u);
			}
		}
#endif 
	}

	// do ph2's next
	LIST_FOREACH(iph2, &ph2tree, chain) {
		if (iph2->parent_session && iph2->parent_session->is_asserted) {
			plog(LLV_DEBUG2, LOCATION, NULL, "skipping sweep of phase2 because it's been asserted.\n");
			continue;
		}
		if (iph2->is_dying || iph2->status >= PHASE2ST_EXPIRED) {
			plog(LLV_DEBUG2, LOCATION, NULL, "skipping sweep of phase2 because it's already expired.\n");
			continue;
		}
		if (iph2->sce) {
			if (iph2->sce->xtime <= swept_at) {
				iph2->status = PHASE2ST_EXPIRED;
				iph2->is_dying = 1;
				isakmp_ph2expire(iph2); // iph2 will go down 1 second later.
				ike_session_stopped_by_controller(iph2->parent_session,
								  ike_session_stopped_by_sleepwake);
				plog(LLV_DEBUG2, LOCATION, NULL, "phase2 expired while sleeping: quick deletion.\n");
			}
		}
		if (iph2->scr) {
			if (iph2->status == PHASE2ST_EXPIRED || iph2->scr->xtime <= swept_at) {
				SCHED_KILL(iph2->scr);
			}
		}
	}

	// do the ike_session last
	ike_session_sweep_sleepwake();
}
