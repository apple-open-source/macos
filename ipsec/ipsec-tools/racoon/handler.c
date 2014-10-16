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
#include "fsm.h"

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
#include "isakmp_frag.h"

#include "sainfo.h"

#include "power_mgmt.h"

extern LIST_HEAD(_ike_session_tree_, ike_session) ike_session_tree;
static LIST_HEAD(_ctdtree_, contacted) ctdtree;
static LIST_HEAD(_rcptree_, recvdpkt) rcptree;

static void ike_session_del_recvdpkt (struct recvdpkt *);
static void ike_session_rem_recvdpkt (struct recvdpkt *);
static void sweep_recvdpkt (void *);

/*
 * functions about management of the isakmp status table
 */
/* %%% management phase 1 handler */
/*
 * search for isakmpsa handler with isakmp index.
 */

extern caddr_t val2str (const char *, size_t);

static phase1_handle_t *
getph1byindex(ike_session_t *session, isakmp_index *index)
{
	phase1_handle_t *p = NULL;
	
	LIST_FOREACH(p, &session->ph1tree, ph1ofsession_chain) {
		if (FSM_STATE_IS_EXPIRED(p->status))
			continue;
		if (memcmp(&p->index, index, sizeof(*index)) == 0)
			return p;
	}
    
	return NULL;
}

phase1_handle_t *
ike_session_getph1byindex(ike_session_t *session, isakmp_index *index)
{
    phase1_handle_t *p;
    ike_session_t   *cur_session = NULL;

    if (session)
        return getph1byindex(session, index);

    LIST_FOREACH(cur_session, &ike_session_tree, chain) {
        if ((p = getph1byindex(cur_session, index)) != NULL)
            return p;
    }
    return NULL;
}


/*
 * search for isakmp handler by i_ck in index.
 */

static phase1_handle_t *
getph1byindex0 (ike_session_t *session, isakmp_index *index)
{
    phase1_handle_t *p = NULL;
    
    LIST_FOREACH(p, &session->ph1tree, ph1ofsession_chain) {
        if (FSM_STATE_IS_EXPIRED(p->status))
            continue;
        if (memcmp(&p->index.i_ck, &index->i_ck, sizeof(cookie_t)) == 0)
            return p;   
    }
    return NULL;
}

phase1_handle_t *
ike_session_getph1byindex0(ike_session_t *session, isakmp_index *index)
{
    phase1_handle_t *p = NULL;
    ike_session_t   *cur_session = NULL;
    
    if (session)
        return getph1byindex0(session, index);
    
    LIST_FOREACH(cur_session, &ike_session_tree, chain) {
        if ((p = getph1byindex0(cur_session, index)) != NULL)
            return p;
	}
    
	return NULL;
}

/*
 * search for isakmpsa handler by source and remote address.
 * don't use port number to search because this function search
 * with phase 2's destinaion.
 */
phase1_handle_t *
ike_session_getph1byaddr(ike_session_t *session, struct sockaddr_storage *local, struct sockaddr_storage *remote)
{
    phase1_handle_t *p = NULL;
    
	plog(ASL_LEVEL_DEBUG, "getph1byaddr: start\n");
	plog(ASL_LEVEL_DEBUG, "local: %s\n", saddr2str((struct sockaddr *)local));
	plog(ASL_LEVEL_DEBUG, "remote: %s\n", saddr2str((struct sockaddr *)remote));
    
	LIST_FOREACH(p, &session->ph1tree, ph1ofsession_chain) {
		if (FSM_STATE_IS_EXPIRED(p->status))
			continue;
		plog(ASL_LEVEL_DEBUG, "p->local: %s\n", saddr2str((struct sockaddr *)p->local));
		plog(ASL_LEVEL_DEBUG, "p->remote: %s\n", saddr2str((struct sockaddr *)p->remote));
		if (CMPSADDR(local, p->local) == 0
			&& CMPSADDR(remote, p->remote) == 0){
			plog(ASL_LEVEL_DEBUG, "matched\n");
			return p;
		}
	}
    
	plog(ASL_LEVEL_DEBUG, "no match\n");
    
	return NULL;
}

static phase1_handle_t *
sgetph1byaddrwop(ike_session_t *session, struct sockaddr_storage *local, struct sockaddr_storage *remote)
{
    phase1_handle_t *p = NULL;
    
	LIST_FOREACH(p, &session->ph1tree, ph1ofsession_chain) {
		if (FSM_STATE_IS_EXPIRED(p->status))
			continue;
		if (cmpsaddrwop(local, p->local) == 0
            && cmpsaddrwop(remote, p->remote) == 0)
			return p;
	}
    
	return NULL;
}

phase1_handle_t *
ike_session_getph1byaddrwop(ike_session_t *session, struct sockaddr_storage *local, struct sockaddr_storage *remote)
{
	phase1_handle_t *p;
    ike_session_t   *cur_session = NULL;
    
    if (session)
        return sgetph1byaddrwop(session, local, remote);
    
    LIST_FOREACH(cur_session, &ike_session_tree, chain) {
        if ((p = sgetph1byaddrwop(cur_session, local, remote)) != NULL)
            return p;
	}
    
	return NULL;
}

/*
 * search for isakmpsa handler by remote address.
 * don't use port number to search because this function search
 * with phase 2's destinaion.
 */
phase1_handle_t *
sike_session_getph1bydstaddrwop(ike_session_t *session, struct sockaddr_storage *remote)
{
	phase1_handle_t *p = NULL;
           
    LIST_FOREACH(p, &session->ph1tree, ph1ofsession_chain) {
        if (FSM_STATE_IS_EXPIRED(p->status))
            continue;
        if (cmpsaddrwop(remote, p->remote) == 0)
            return p;
    }
    
    return NULL;
}

phase1_handle_t *
ike_session_getph1bydstaddrwop(ike_session_t *session, struct sockaddr_storage *remote)
{
	phase1_handle_t *p;
    ike_session_t   *cur_session = NULL;
    
    if (session)
        return sike_session_getph1bydstaddrwop(session, remote);
    else {
        LIST_FOREACH(cur_session, &ike_session_tree, chain) {
            if ((p = sike_session_getph1bydstaddrwop(cur_session, remote)) != NULL)
                return p;
        }
    }
    return NULL;
}

int
ike_session_islast_ph1(phase1_handle_t *ph1) 
{
    phase1_handle_t *p = NULL;
    
    LIST_FOREACH(p, &ph1->parent_session->ph1tree, ph1ofsession_chain) {
		if (p->is_dying || FSM_STATE_IS_EXPIRED(p->status))
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
 * create new isakmp Phase 1 status record to handle isakmp in Phase1
 */
phase1_handle_t *
ike_session_newph1(unsigned int version)
{
	phase1_handle_t *iph1;
    
	/* create new iph1 */
	iph1 = racoon_calloc(1, sizeof(*iph1));
	if (iph1 == NULL)
		return NULL;
	iph1->version = version;

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
    plog(ASL_LEVEL_DEBUG, "*** New Phase 1\n");
	return iph1;
}

/*
 * delete new isakmp Phase 1 status record to handle isakmp in Phase1
 */
void
ike_session_delph1(phase1_handle_t *iph1)
{
	if (iph1 == NULL)
		return;
    
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
	if (iph1->dpd_r_u)
		SCHED_KILL(iph1->dpd_r_u);
#endif
#ifdef ENABLE_VPNCONTROL_PORT
	if (iph1->ping_sched)
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
    
	sched_scrub_param(iph1);
    if (iph1->sce)
        SCHED_KILL(iph1->sce);
    if (iph1->sce_rekey)
        SCHED_KILL(iph1->sce_rekey);
    if (iph1->scr)
        SCHED_KILL(iph1->scr);
    
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
	VPTRINIT(iph1->skeyid_a_p);
	VPTRINIT(iph1->skeyid_e);
    VPTRINIT(iph1->skeyid_e_p);
	VPTRINIT(iph1->key);
    VPTRINIT(iph1->key_p);
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
    
	if (iph1->rmconf) {
		release_rmconf(iph1->rmconf);
		iph1->rmconf = NULL;
	}
    
	racoon_free(iph1);
}

void
ike_session_flush_all_phase1_for_session(ike_session_t *session, int ignore_estab_or_assert_handles)
{
	phase1_handle_t *p, *next;
		
    LIST_FOREACH_SAFE(p, &session->ph1tree, ph1ofsession_chain, next) {
        if (ignore_estab_or_assert_handles && p->parent_session && !p->parent_session->stopped_by_vpn_controller && p->parent_session->is_asserted) {
            plog(ASL_LEVEL_DEBUG,
                 "Skipping Phase 1 %s that's asserted...\n",
                 isakmp_pindex(&p->index, 0));
            continue;
        }
        
        /* send delete information */
        if (FSM_STATE_IS_ESTABLISHED(p->status)) {
            if (ignore_estab_or_assert_handles &&
                (ike_session_has_negoing_ph2(p->parent_session) || ike_session_has_established_ph2(p->parent_session))) {
                plog(ASL_LEVEL_DEBUG,
                     "Skipping Phase 1 %s that's established... because it's needed by children Phase 2s\n",
                     isakmp_pindex(&p->index, 0));
                continue;
            }
            /* send delete information */
            plog(ASL_LEVEL_DEBUG,
                 "Got a Phase 1 %s to flush...\n",
                 isakmp_pindex(&p->index, 0));
            isakmp_info_send_d1(p);
        }
        
        ike_session_stopped_by_controller(p->parent_session,
                                          ike_session_stopped_by_flush);
        
        ike_session_unlink_phase1(p);
    }
}

/*
 * flush isakmp-sa
 */
void
ike_session_flush_all_phase1(int ignore_estab_or_assert_handles)
{
    ike_session_t *session = NULL;
    ike_session_t *next_session = NULL;
	
	plog(ASL_LEVEL_DEBUG,
		 "Flushing Phase 1 handles: ignore_estab_or_assert %d...\n", ignore_estab_or_assert_handles);
    
    LIST_FOREACH_SAFE(session, &ike_session_tree, chain, next_session) {
        ike_session_flush_all_phase1_for_session(session, ignore_estab_or_assert_handles);
    }
}



/*
 * search ph2handle with policy id.
 */
phase2_handle_t *
ike_session_getph2byspid(u_int32_t spid)
{
    ike_session_t *session = NULL;
    phase2_handle_t *p;
    
    LIST_FOREACH(session, &ike_session_tree, chain) {
        LIST_FOREACH(p, &session->ph2tree, ph2ofsession_chain) {
            /*
             * there are ph2handle independent on policy
             * such like informational exchange.
             */
            if (p->spid == spid)
                return p;
        }
    }
    
	return NULL;
}


/*
 * search ph2handle with sequence number.
 * Used by PF_KEY functions to locate the phase2
 */
phase2_handle_t *
ike_session_getph2byseq(u_int32_t seq)
{
    ike_session_t *session;
	phase2_handle_t *p;
    
    LIST_FOREACH(session, &ike_session_tree, chain) {
        LIST_FOREACH(p, &session->ph2tree, ph2ofsession_chain) {
            if (p->seq == seq)
                return p;
        }
    }
	return NULL;
}

/*
 * search ph2handle with message id.
 */
phase2_handle_t *
ike_session_getph2bymsgid(phase1_handle_t *iph1, u_int32_t msgid)
{
	phase2_handle_t *p;
    
	LIST_FOREACH(p, &iph1->parent_session->ph2tree, ph2ofsession_chain) {
		if (p->msgid == msgid && !p->is_defunct)
			return p;
	}
    
	return NULL;
}

phase2_handle_t *
ike_session_getonlyph2(phase1_handle_t *iph1)
{
    phase2_handle_t *only_ph2 = NULL;
	phase2_handle_t *p = NULL;
    
	LIST_FOREACH(p, &iph1->bound_ph2tree, ph2ofsession_chain) {
		if (only_ph2) return NULL;
        only_ph2 = p;
	}
    
	return only_ph2;
}

phase2_handle_t *
ike_session_getph2byid(struct sockaddr_storage *src, struct sockaddr_storage *dst, u_int32_t spid)
{
    ike_session_t *session = NULL;
    ike_session_t *next_session = NULL;
    phase2_handle_t *p;
    phase2_handle_t *next_iph2;
    
    LIST_FOREACH_SAFE(session, &ike_session_tree, chain, next_session) {
        LIST_FOREACH_SAFE(p, &session->ph2tree, ph2ofsession_chain, next_iph2) {
            if (spid == p->spid &&
                CMPSADDR(src, p->src) == 0 &&
                CMPSADDR(dst, p->dst) == 0){
                /* Sanity check to detect zombie handlers
                 * XXX Sould be done "somewhere" more interesting,
                 * because we have lots of getph2byxxxx(), but this one
                 * is called by pk_recvacquire(), so is the most important.
                 */
                if(!FSM_STATE_IS_ESTABLISHED_OR_EXPIRED(p->status) &&
                   p->retry_counter == 0
                   && p->sce == 0 && p->scr == 0 &&
                   p->retry_checkph1 == 0){
                    plog(ASL_LEVEL_DEBUG,
                         "Zombie ph2 found, expiring it\n");
                    isakmp_ph2expire(p);
                }else
                    return p;
            }
        }
    }
    
	return NULL;
}

#ifdef NOT_USED
phase2_handle_t *
ike_session_getph2bysaddr(struct sockaddr_storage *src, struct sockaddr_storage *dst)
{
    ike_session_t *session;
	phase2_handle_t *p;
    
    LIST_FOREACH(session, &ike_session_tree, chain) {
        LIST_FOREACH(p, &session->ph2tree, chain) {
            if (cmpsaddrstrict(src, p->src) == 0 &&
                cmpsaddrstrict(dst, p->dst) == 0)
                return p;
        }
    }
    
	return NULL;
}
#endif /* NOT_USED */

/*
 * call by pk_recvexpire().
 */
phase2_handle_t *
ike_session_getph2bysaidx(struct sockaddr_storage *src, struct sockaddr_storage *dst, u_int proto_id, u_int32_t spi)
{
    ike_session_t *session;
	phase2_handle_t *iph2;
	struct saproto *pr;
    
    LIST_FOREACH(session, &ike_session_tree, chain) {
        LIST_FOREACH(iph2, &session->ph2tree, ph2ofsession_chain) {
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
    }
    
	return NULL;
}

phase2_handle_t *
ike_session_getph2bysaidx2(struct sockaddr_storage *src, struct sockaddr_storage *dst, u_int proto_id, u_int32_t spi, u_int32_t *opposite_spi)
{
    ike_session_t *session;
	phase2_handle_t *iph2;
	struct saproto *pr;
    
    LIST_FOREACH(session, &ike_session_tree, chain) {
        LIST_FOREACH(iph2, &session->ph2tree, ph2ofsession_chain) {
            if (iph2->proposal == NULL && iph2->approval == NULL)
                continue;
            if (iph2->approval != NULL) {
                for (pr = iph2->approval->head; pr != NULL;
                     pr = pr->next) {
                    if (proto_id != pr->proto_id)
                        break;
                    if (spi == pr->spi || spi == pr->spi_p) {
						if (opposite_spi) {
							*opposite_spi = (spi == pr->spi)? pr->spi_p : pr->spi;
						}
                        return iph2;
					}
                }
            } else if (iph2->proposal != NULL) {
                for (pr = iph2->proposal->head; pr != NULL;
                     pr = pr->next) {
                    if (proto_id != pr->proto_id)
                        break;
                    if (spi == pr->spi || spi == pr->spi_p) {
						if (opposite_spi) {
							*opposite_spi = (spi == pr->spi)? pr->spi_p : pr->spi;
						}
                        return iph2;
					}
                }
            }
        }
    }
    
	return NULL;
}

/*
 * create new isakmp Phase 2 status record to handle isakmp in Phase2
 */
phase2_handle_t *
ike_session_newph2(unsigned int version, int type)
{
	phase2_handle_t *iph2 = NULL;
    
	/* create new iph2 */
	iph2 = racoon_calloc(1, sizeof(*iph2));
	if (iph2 == NULL)
		return NULL;
    iph2->version = version;
    iph2->phase2_type = type;
	iph2->is_dying = 0;
    
    plog(ASL_LEVEL_DEBUG, "*** New Phase 2\n");
	return iph2;
}

/*
 * initialize ph2handle
 * NOTE: don't initialize src/dst.
 *       SPI in the proposal is cleared.
 */
void
ike_session_initph2(phase2_handle_t *iph2)
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
		delsp_bothdir(iph2->spidx_gen);
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
ike_session_delph2(phase2_handle_t *iph2)
{
	ike_session_initph2(iph2);
    
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
    
	if (iph2->sainfo) {
		release_sainfo(iph2->sainfo);
		iph2->sainfo = NULL;
	}
    VPTRINIT(iph2->id);
    VPTRINIT(iph2->id_p);
	VPTRINIT(iph2->ext_nat_id);
	VPTRINIT(iph2->ext_nat_id_p);
    
    if (iph2->sce)
        SCHED_KILL(iph2->sce);
    if (iph2->scr)
        SCHED_KILL(iph2->scr);
    
	racoon_free(iph2);
}

void
ike_session_flush_all_phase2_for_session(ike_session_t *session, int ignore_estab_or_assert_handles)
{
    phase2_handle_t *p = NULL;
    phase2_handle_t *next = NULL;
    LIST_FOREACH_SAFE(p, &session->ph2tree, ph2ofsession_chain, next) {
        if (p->is_dying || FSM_STATE_IS_EXPIRED(p->status)) {
            continue;
        }
        if (ignore_estab_or_assert_handles && p->parent_session && !p->parent_session->stopped_by_vpn_controller && p->parent_session->is_asserted) {
            plog(ASL_LEVEL_DEBUG,
                 "skipping phase2 handle that's asserted...\n");
            continue;
        }
        if (FSM_STATE_IS_ESTABLISHED(p->status)){
            if (ignore_estab_or_assert_handles) {
                plog(ASL_LEVEL_DEBUG,
                     "skipping ph2 handler that's established...\n");
                continue;
            }
            /* send delete information */
            plog(ASL_LEVEL_DEBUG,
                 "got an established ph2 handler to flush...\n");
            isakmp_info_send_d2(p);
        }else{
            plog(ASL_LEVEL_DEBUG,
                 "got a ph2 handler to flush (state %d)\n", p->status);
        }
        
        ike_session_stopped_by_controller(p->parent_session,
                                          ike_session_stopped_by_flush);
        delete_spd(p);
        ike_session_unlink_phase2(p);
    }
}

void
ike_session_flush_all_phase2(int ignore_estab_or_assert_handles)
{
    ike_session_t *session = NULL;
    ike_session_t *next_session = NULL;
    
	plog(ASL_LEVEL_DEBUG,
		 "flushing ph2 handles: ignore_estab_or_assert %d...\n", ignore_estab_or_assert_handles);
    
    LIST_FOREACH_SAFE(session, &ike_session_tree, chain, next_session) {
        ike_session_flush_all_phase2_for_session(session, ignore_estab_or_assert_handles);
    }
}

/*
 * Delete all Phase 2 handlers for this src/dst/proto.  This
 * is used during INITIAL-CONTACT processing (so no need to
 * send a message to the peer).
 */
//%%%%%%%%%%%%%%%%%%% make this smarter - find session using addresses ????
void
ike_session_deleteallph2(struct sockaddr_storage *src, struct sockaddr_storage *dst, u_int proto_id)
{
    ike_session_t *session = NULL;
    ike_session_t *next_session = NULL;
    phase2_handle_t *iph2 = NULL;
    phase2_handle_t *next_iph2 = NULL;
	struct saproto *pr;
    
    LIST_FOREACH_SAFE(session, &ike_session_tree, chain, next_session) {
        LIST_FOREACH_SAFE(iph2, &session->ph2tree, ph2ofsession_chain, next_iph2) {
            if (iph2->is_dying || FSM_STATE_IS_EXPIRED(iph2->status)) {
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
            plog(ASL_LEVEL_DEBUG,
                 "deleteallph2: got a ph2 handler...\n");
            if (FSM_STATE_IS_ESTABLISHED(iph2->status))
                isakmp_info_send_d2(iph2);
            ike_session_stopped_by_controller(iph2->parent_session,
                                              ike_session_stopped_by_flush);
            ike_session_unlink_phase2(iph2);
        }
    }
}

/*
 * Delete all Phase 1 handlers for this src/dst.
 */
void
ike_session_deleteallph1(struct sockaddr_storage *src, struct sockaddr_storage *dst)
{
    ike_session_t *session = NULL;
    ike_session_t *next_session = NULL;
    phase1_handle_t *iph1 = NULL;
    phase1_handle_t *next_iph1 = NULL;
    
    LIST_FOREACH_SAFE(session, &ike_session_tree, chain, next_session) {
        LIST_FOREACH_SAFE(iph1, &session->ph1tree, ph1ofsession_chain, next_iph1) {
            if (cmpsaddrwop(src, iph1->local) != 0 ||
                cmpsaddrwop(dst, iph1->remote) != 0) {
                continue;
            }
            plog(ASL_LEVEL_DEBUG,
                 "deleteallph1: got a ph1 handler...\n");
            if (FSM_STATE_IS_ESTABLISHED(iph1->status))
                isakmp_info_send_d1(iph1);
            
            ike_session_stopped_by_controller(iph1->parent_session, ike_session_stopped_by_flush);
            ike_session_unlink_phase1(iph1);
        }
    }
}


/* %%% management contacted list */
/*
 * search contacted list.
 */
struct contacted *
ike_session_getcontacted(remote)
struct sockaddr_storage *remote;
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
ike_session_inscontacted(remote)
struct sockaddr_storage *remote;
{
	struct contacted *new;
    
	/* create new iph2 */
	new = racoon_calloc(1, sizeof(*new));
	if (new == NULL)
		return -1;
    
	new->remote = dupsaddr(remote);
	if (new->remote == NULL) {
		plog(ASL_LEVEL_ERR,
             "failed to allocate buffer.\n");
		racoon_free(new);
		return -1;
	}
    
	LIST_INSERT_HEAD(&ctdtree, new, chain);
    
	return 0;
}


void
ike_session_clear_contacted()
{
	struct contacted *c, *next;
	LIST_FOREACH_SAFE(c, &ctdtree, chain, next) {
		LIST_REMOVE(c, chain);
		racoon_free(c->remote);
		racoon_free(c);
	}
}

void
ike_session_initctdtree()
{
	LIST_INIT(&ctdtree);
}

time_t
ike_session_get_exp_retx_interval (int num_retries, int fixed_retry_interval)
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
ike_session_check_recvdpkt(remote, local, rbuf)
struct sockaddr_storage *remote, *local;
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
		plog(ASL_LEVEL_ERR,
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
		plog(ASL_LEVEL_WARNING,
             "the packet retransmitted in a short time from %s\n",
             saddr2str((struct sockaddr *)remote));
		/*XXX should it be error ? */
	}
    
	/* select the socket to be sent */
	s = getsockmyaddr((struct sockaddr *)r->local);
	if (s == -1)
		return -1;
    
	// don't send if we recently sent a response.
	if (r->time_send && t > r->time_send) {
		d = t - r->time_send;
		if (d  < r->retry_interval) {
			plog(ASL_LEVEL_ERR, "already responded within the past %ld secs\n", d);
			return 1;
		}
	}
    
#ifdef ENABLE_FRAG
	if (r->frag_flags && r->sendbuf->l > ISAKMP_FRAG_MAXLEN) {
		/* resend the packet if needed */
		plog(ASL_LEVEL_ERR, "!!! retransmitting frags\n");
		len = sendfragsfromto(s, r->sendbuf,
							  r->local, r->remote, lcconf->count_persend,
							  r->frag_flags);
	} else {
		plog(ASL_LEVEL_ERR, "!!! skipped retransmitting frags: frag_flags %x, r->sendbuf->l %zu, max %d\n", r->frag_flags, r->sendbuf->l, ISAKMP_FRAG_MAXLEN);
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
		plog(ASL_LEVEL_ERR, "sendfromto failed\n");
		return -1;
	}
    
	/* check the retry counter */
	r->retry_counter--;
	if (r->retry_counter <= 0) {
		ike_session_rem_recvdpkt(r);
		ike_session_del_recvdpkt(r);
		plog(ASL_LEVEL_DEBUG,
             "deleted the retransmission packet to %s.\n",
             saddr2str((struct sockaddr *)remote));
	} else {
		r->time_send = t;
		r->retry_interval = ike_session_get_exp_retx_interval((lcconf->retry_counter - r->retry_counter),
												  lcconf->retry_interval);
	}
    
	return 1;
}

/*
 * adding a hash of received packet into the received list.
 */
int
ike_session_add_recvdpkt(remote, local, sbuf, rbuf, non_esp, frag_flags)
struct sockaddr_storage *remote, *local;
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
		plog(ASL_LEVEL_ERR,
             "failed to allocate buffer.\n");
		return -1;
	}
    
	new->hash = eay_md5_one(rbuf);
	if (!new->hash) {
		plog(ASL_LEVEL_ERR,
             "failed to allocate buffer.\n");
		ike_session_del_recvdpkt(new);
		return -1;
	}
	new->remote = dupsaddr(remote);
	if (new->remote == NULL) {
		plog(ASL_LEVEL_ERR,
             "failed to allocate buffer.\n");
		ike_session_del_recvdpkt(new);
		return -1;
	}
	new->local = dupsaddr(local);
	if (new->local == NULL) {
		plog(ASL_LEVEL_ERR,
             "failed to allocate buffer.\n");
		ike_session_del_recvdpkt(new);
		return -1;
	}
    
	if (non_esp) {
		plog (ASL_LEVEL_DEBUG, "Adding NON-ESP marker\n");
        
        /* If NAT-T port floating is in use, 4 zero bytes (non-ESP marker) 
         must added just before the packet itself. For this we must 
         allocate a new buffer and release it at the end. */
        if ((new->sendbuf = vmalloc (sbuf->l + non_esp)) == NULL) {
            plog(ASL_LEVEL_ERR, 
                 "failed to allocate extra buf for non-esp\n");
            ike_session_del_recvdpkt(new);
            return -1;
        }
        *ALIGNED_CAST(u_int32_t *)new->sendbuf->v = 0;
        memcpy(new->sendbuf->v + non_esp, sbuf->v, sbuf->l);
    } else {
        new->sendbuf = vdup(sbuf);
        if (new->sendbuf == NULL) {
            plog(ASL_LEVEL_ERR, 
                 "failed to allocate buffer.\n");
            ike_session_del_recvdpkt(new);
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
	new->retry_interval = ike_session_get_exp_retx_interval((lcconf->retry_counter - new->retry_counter),
												lcconf->retry_interval);
    
	LIST_INSERT_HEAD(&rcptree, new, chain);
    
	return 0;
}

void
ike_session_del_recvdpkt(r)
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
ike_session_rem_recvdpkt(r)
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
    
	LIST_FOREACH_SAFE(r, &rcptree, chain, next) {
		if (t - r->created > lt) {
			ike_session_rem_recvdpkt(r);
			ike_session_del_recvdpkt(r);
		}
	}
    
	sched_new(lt, sweep_recvdpkt, &rcptree);
}

void
ike_session_clear_recvdpkt()
{
	struct recvdpkt *r, *next;
	
	LIST_FOREACH_SAFE(r, &rcptree, chain, next) {
		ike_session_rem_recvdpkt(r);
		ike_session_del_recvdpkt(r);
	}
	sched_scrub_param(&rcptree);
}

void
ike_session_init_recvdpkt()
{
	time_t lt = lcconf->retry_counter * lcconf->retry_interval;
    
	LIST_INIT(&rcptree);
    
	sched_new(lt, sweep_recvdpkt, &rcptree);
}

#ifdef NOT_USED
#ifdef ENABLE_HYBRID
/* 
 * Returns 0 if the address was obtained by ISAKMP mode config, 1 otherwise
 * This should be in isakmp_cfg.c but ph1tree being private, it must be there
 */
int
exclude_cfg_addr(const struct sockaddr_storage *addr)
{
    ike_session_t *session;
	phase1_handle_t *p;
	struct sockaddr_in *sin;
    
    LIST_FOREACH(session, &ike_session_tree, chain) {
        LIST_FOREACH(p, &session->ph1tree, chain) {
            if ((p->mode_cfg != NULL) &&
                (p->mode_cfg->flags & ISAKMP_CFG_GOT_ADDR4) &&
                (addr->ss_family == AF_INET)) {
                sin = (struct sockaddr_in *)addr;
                if (sin->sin_addr.s_addr == p->mode_cfg->addr4.s_addr)
                    return 0;
            }
        }
    }
    
	return 1;
}
#endif
#endif /* NOT_USED */

int
ike_session_expire_session(ike_session_t *session)
{    
	int    found = 0;
	phase1_handle_t *p;
	phase1_handle_t *next;
	phase2_handle_t *p2;
    
    if (session == NULL)
        return 0;
    
    LIST_FOREACH(p2, &session->ph2tree, ph2ofsession_chain) {
        if (p2->is_dying || FSM_STATE_IS_EXPIRED(p2->status)) {
            continue;
        }

        // Don't send a delete, since the ph1 implies the removal of ph2s
        isakmp_ph2expire(p2);
        found++;
    }
    
    LIST_FOREACH_SAFE(p, &session->ph1tree, ph1ofsession_chain, next) {
        if (p->is_dying || FSM_STATE_IS_EXPIRED(p->status)) {
            continue;
        }

        ike_session_purge_ph2s_by_ph1(p);
        if (FSM_STATE_IS_ESTABLISHED(p->status))
            isakmp_info_send_d1(p);
        isakmp_ph1expire(p);
        found++;
    }

	return found;
}

#ifdef ENABLE_HYBRID
int
ike_session_purgephXbydstaddrwop(struct sockaddr_storage *remote)
{
	int    found = 0;
    ike_session_t *session = NULL;
    ike_session_t *next_session = NULL;
	phase1_handle_t *p;
	phase2_handle_t *p2;
    
    LIST_FOREACH_SAFE(session, &ike_session_tree, chain, next_session) {
        LIST_FOREACH(p2, &session->ph2tree, ph2ofsession_chain) {
		if (p2->is_dying || FSM_STATE_IS_EXPIRED(p2->status)) {
			continue;
		}
            if (cmpsaddrwop(remote, p2->dst) == 0) {
                plog(ASL_LEVEL_DEBUG,
                     "in %s... purging Phase 2 structures\n", __FUNCTION__);
                if (FSM_STATE_IS_ESTABLISHED(p2->status))
                    isakmp_info_send_d2(p2);
                isakmp_ph2expire(p2);
                found++;
            }
        }
        
        LIST_FOREACH(p, &session->ph1tree, ph1ofsession_chain) {
		if (p->is_dying || FSM_STATE_IS_EXPIRED(p->status)) {
			continue;
		}
            if (cmpsaddrwop(remote, p->remote) == 0) {
                plog(ASL_LEVEL_DEBUG,
                     "in %s... purging Phase 1 and related Phase 2 structures\n", __FUNCTION__);
                ike_session_purge_ph2s_by_ph1(p);
                if (FSM_STATE_IS_ESTABLISHED(p->status))
                    isakmp_info_send_d1(p);
                isakmp_ph1expire(p);
                found++;
            }
        }
    }
    
	return found;
}

void
ike_session_purgephXbyspid(u_int32_t spid, int del_boundph1)
{
    ike_session_t *session = NULL;
    ike_session_t *next_session = NULL;
    phase2_handle_t *iph2 = NULL;
    phase2_handle_t *next_iph2 = NULL;
    phase1_handle_t *iph1 = NULL;
    phase1_handle_t *next_iph1 = NULL;

    LIST_FOREACH_SAFE(session, &ike_session_tree, chain, next_session) {
        // do ph2's first... we need the ph1s for notifications
        LIST_FOREACH_SAFE(iph2, &session->ph2tree, ph2ofsession_chain, next_iph2) {
            if (spid == iph2->spid) {
                if (iph2->is_dying || FSM_STATE_IS_EXPIRED(iph2->status)) {
                    continue;
                }
                if (FSM_STATE_IS_ESTABLISHED(iph2->status)) {
                    isakmp_info_send_d2(iph2);
                }
                ike_session_stopped_by_controller(iph2->parent_session,
                                                  ike_session_stopped_by_flush);
                isakmp_ph2expire(iph2); // iph2 will go down 1 second later.
            }
        }
        
        // do the ph1s last.   %%%%%%%%%%%%%%%%%% re-organize this - check del_boundph1 first
        LIST_FOREACH_SAFE(iph2, &session->ph2tree, ph2ofsession_chain, next_iph2) {
            if (spid == iph2->spid) {
                if (del_boundph1 && iph2->parent_session) {
                    LIST_FOREACH_SAFE(iph1, &iph2->parent_session->ph1tree, ph1ofsession_chain, next_iph1) {
                        if (iph1->is_dying || FSM_STATE_IS_EXPIRED(iph1->status)) {
                            continue;
                        }
                        if (FSM_STATE_IS_ESTABLISHED(iph1->status)) {
                            isakmp_info_send_d1(iph1);
                        }
                        isakmp_ph1expire(iph1);
                    }
                }
            }
        }
    }
}

#endif

#ifdef ENABLE_DPD
int
ike_session_ph1_force_dpd (struct sockaddr_storage *remote)
{
    int status = -1;
    ike_session_t *session = NULL;
    phase1_handle_t *p = NULL;
    
    LIST_FOREACH(session, &ike_session_tree, chain) {
        LIST_FOREACH(p, &session->ph1tree, ph1ofsession_chain) {
            if (cmpsaddrwop(remote, p->remote) == 0) {
                if (FSM_STATE_IS_ESTABLISHED(p->status) &&
                    !p->is_dying &&
                    p->dpd_support &&
                    p->rmconf->dpd_interval) {
                    if(!p->dpd_fails) {
                        isakmp_info_send_r_u(p);
                        status = 0;
                    } else {
                        plog(ASL_LEVEL_DEBUG, "Skipping forced-DPD for Phase 1 (dpd already in progress).\n");
                    }
                    if (p->parent_session) {
                        p->parent_session->controller_awaiting_peer_resp = 1;
                    }
                } else {
                    plog(ASL_LEVEL_DEBUG, "Skipping forced-DPD for Phase 1 (status %d, dying %d, dpd-support %d, dpd-interval %d).\n",
                         p->status, p->is_dying, p->dpd_support, p->rmconf->dpd_interval);
                }
            }
        }
    }
    
	return status;
}
#endif

void
sweep_sleepwake(void)
{
    ike_session_t   *session = NULL;
    ike_session_t   *next_session = NULL;
    phase2_handle_t *iph2 = NULL;
    phase2_handle_t *next_iph2 = NULL;
    phase1_handle_t *iph1 = NULL;
    phase1_handle_t *next_iph1 = NULL;

    LIST_FOREACH_SAFE(session, &ike_session_tree, chain, next_session) {
        // do the ph1s.
        LIST_FOREACH_SAFE(iph1, &session->ph1tree, ph1ofsession_chain, next_iph1) {
            if (iph1->parent_session && iph1->parent_session->is_asserted) {
                plog(ASL_LEVEL_DEBUG, "Skipping sweep of Phase 1 %s because it's been asserted.\n",
                     isakmp_pindex(&iph1->index, 0));
                continue;
            }
            if (iph1->is_dying || FSM_STATE_IS_EXPIRED(iph1->status)) {
                plog(ASL_LEVEL_DEBUG, "Skipping sweep of Phase 1 %s because it's already expired.\n",
                     isakmp_pindex(&iph1->index, 0));
                continue;
            }
            if (iph1->sce) {                
                time_t xtime;
                if (sched_get_time(iph1->sce, &xtime)) {
                    if (xtime <= swept_at) {
                        SCHED_KILL(iph1->sce);
                        SCHED_KILL(iph1->sce_rekey);
                        iph1->is_dying = 1;
                        fsm_set_state(&iph1->status, IKEV1_STATE_PHASE1_EXPIRED);
                        ike_session_update_ph1_ph2tree(iph1); // move unbind/rebind ph2s to from current ph1
                        iph1->sce = sched_new(1, isakmp_ph1delete_stub, iph1);
                        plog(ASL_LEVEL_DEBUG, "Phase 1 %s expired while sleeping: quick deletion.\n",
                             isakmp_pindex(&iph1->index, 0));
                    }
                }
            }
            if (iph1->sce_rekey) {
                time_t xtime;
                if (sched_get_time(iph1->sce_rekey, &xtime)) {
                    if (FSM_STATE_IS_EXPIRED(iph1->status) || xtime <= swept_at) {
                        SCHED_KILL(iph1->sce_rekey);
                    }
                }
            }
            if (iph1->scr) {
                time_t xtime;
                if (sched_get_time(iph1->scr, &xtime)) {
                    if (FSM_STATE_IS_EXPIRED(iph1->status) || xtime <= swept_at) {
                        SCHED_KILL(iph1->scr);
                    }
                }
            }
    #ifdef ENABLE_DPD
            if (iph1->dpd_r_u) {
                time_t xtime;
                if (sched_get_time(iph1->dpd_r_u, &xtime)) {
                    if (FSM_STATE_IS_EXPIRED(iph1->status) || xtime <= swept_at) {
                        SCHED_KILL(iph1->dpd_r_u);
                    }
                }
            }
    #endif 
        }
        
        // do ph2's next
        LIST_FOREACH_SAFE(iph2, &session->ph2tree, ph2ofsession_chain, next_iph2) {
            if (iph2->parent_session && iph2->parent_session->is_asserted) {
                plog(ASL_LEVEL_DEBUG, "Skipping sweep of Phase 2 because it's been asserted.\n");
                continue;
            }
            if (iph2->is_dying || FSM_STATE_IS_EXPIRED(iph2->status)) {
                plog(ASL_LEVEL_DEBUG, "Skipping sweep of Phase 2 because it's already expired.\n");
                continue;
            }
            if (iph2->sce) {
                time_t xtime;
                if (sched_get_time(iph2->sce, &xtime)) {
                    if (xtime <= swept_at) {
                        fsm_set_state(&iph2->status, IKEV1_STATE_PHASE2_EXPIRED);
                        iph2->is_dying = 1;
                        isakmp_ph2expire(iph2); // iph2 will go down 1 second later.
                        ike_session_stopped_by_controller(iph2->parent_session,
                                                      ike_session_stopped_by_sleepwake);
                        plog(ASL_LEVEL_DEBUG, "Phase 2 expired while sleeping: quick deletion.\n");
                    }
                }
            }
            if (iph2->scr) {
                time_t xtime;
                if (sched_get_time(iph2->scr, &xtime)) {
                    if (FSM_STATE_IS_EXPIRED(iph2->status) || xtime <= swept_at) {
                        SCHED_KILL(iph2->scr);
                    }
                }
            }
        }
    }
    //%%%%%%%%%%%%%%% fix this
	// do the ike_session last
	ike_session_sweep_sleepwake();
}
