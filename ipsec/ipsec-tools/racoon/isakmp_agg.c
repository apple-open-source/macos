/*	$NetBSD: isakmp_agg.c,v 1.9 2006/09/30 21:49:37 manu Exp $	*/

/* Id: isakmp_agg.c,v 1.28 2006/04/06 16:46:08 manubsd Exp */

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

/* Aggressive Exchange (Aggressive Mode) */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "sockmisc.h"
#include "schedule.h"
#include "debug.h"

#ifdef ENABLE_HYBRID
#include <resolv.h>
#endif

#include "fsm.h"
#include "localconf.h"
#include "remoteconf.h"
#include "isakmp_var.h"
#include "isakmp.h"
#include "oakley.h"
#include "handler.h"
#include "ipsec_doi.h"
#include "crypto_openssl.h"
#include "pfkey.h"
#include "isakmp_agg.h"
#include "isakmp_inf.h"
#ifdef ENABLE_HYBRID
#include "isakmp_xauth.h"
#include "isakmp_cfg.h"
#endif
#ifdef ENABLE_FRAG
#include "isakmp_frag.h"
#endif
#include "vendorid.h"
#include "strnames.h"

#ifdef ENABLE_NATT
#include "nattraversal.h"
#endif

#include "vpn_control.h"
#include "vpn_control_var.h"
#include "ipsecSessionTracer.h"
#include "ipsecMessageTracer.h"
#ifndef HAVE_OPENSSL
#include <Security/SecDH.h>
#endif

/*
 * begin Aggressive Mode as initiator.
 */
/*
 * send to responder
 * 	psk: HDR, SA, KE, Ni, IDi1
 * 	sig: HDR, SA, KE, Ni, IDi1 [, CR ]
 *   gssapi: HDR, SA, KE, Ni, IDi1, GSSi
 * 	rsa: HDR, SA, [ HASH(1),] KE, <IDi1_b>Pubkey_r, <Ni_b>Pubkey_r
 * 	rev: HDR, SA, [ HASH(1),] <Ni_b>Pubkey_r, <KE_b>Ke_i,
 * 	     <IDii_b>Ke_i [, <Cert-I_b>Ke_i ]
 */
int
agg_i1send(iph1, msg)
	phase1_handle_t *iph1;
	vchar_t *msg; /* must be null */
{
	struct payload_list *plist = NULL;
	int need_cr = 0;
	vchar_t *cr = NULL; 
	int error = -1;
#ifdef ENABLE_NATT
	vchar_t *vid_natt[MAX_NATT_VID_COUNT] = { NULL };
	int i;
#endif
#ifdef ENABLE_HYBRID
	vchar_t *vid_xauth = NULL;
	vchar_t *vid_unity = NULL;
#endif
#ifdef ENABLE_FRAG
	vchar_t *vid_frag = NULL;
#endif
#ifdef ENABLE_DPD
	vchar_t *vid_dpd = NULL;
#endif

    /* validity check */
	if (iph1->status != IKEV1_STATE_AGG_I_START) {
		plog(ASL_LEVEL_ERR,
             "status mismatched %d.\n", iph1->status);
		goto end;
	}

	/* validity check */
	if (msg != NULL) {
		plog(ASL_LEVEL_ERR,
			"msg has to be NULL in this function.\n");
		goto end;
	}

	/* create isakmp index */
	memset(&iph1->index, 0, sizeof(iph1->index));
	isakmp_newcookie((caddr_t)&iph1->index, iph1->remote, iph1->local);

	/* make ID payload into isakmp status */
	if (ipsecdoi_setid1(iph1) < 0) {
		plog(ASL_LEVEL_ERR, 
			 "failed to set ID");
		goto end;
	}

	/* create SA payload for my proposal */
	iph1->sa = ipsecdoi_setph1proposal(iph1);
	if (iph1->sa == NULL) {
		plog(ASL_LEVEL_ERR, 
			 "failed to set proposal");
		goto end;
	}

	/* consistency check of proposals */
	if (iph1->rmconf->dhgrp == NULL) {
		plog(ASL_LEVEL_ERR, 
			"configuration failure about DH group.\n");
		goto end;
	}

	/* generate DH public value */
#ifdef HAVE_OPENSSL
	if (oakley_dh_generate(iph1->rmconf->dhgrp,
						   &iph1->dhpub, &iph1->dhpriv) < 0) {	
#else
	if (oakley_dh_generate(iph1->rmconf->dhgrp,
						   &iph1->dhpub, &iph1->publicKeySize, &iph1->dhC) < 0) {
#endif		
		plog(ASL_LEVEL_ERR, 
			 "failed to generate DH");
		goto end;
	}

	/* generate NONCE value */
	iph1->nonce = eay_set_random(iph1->rmconf->nonce_size);
	if (iph1->nonce == NULL) {
		plog(ASL_LEVEL_ERR, 
			 "failed to generate NONCE");
		goto end;
	}

#ifdef ENABLE_HYBRID
	/* Do we need Xauth VID? */
	switch (RMAUTHMETHOD(iph1)) {
	case FICTIVE_AUTH_METHOD_XAUTH_PSKEY_I:
	case OAKLEY_ATTR_AUTH_METHOD_HYBRID_RSA_I:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSASIG_I:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAENC_I:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAREV_I:
		if ((vid_xauth = set_vendorid(VENDORID_XAUTH)) == NULL)
			plog(ASL_LEVEL_ERR, 
			     "Xauth vendor ID generation failed\n");
		if ((vid_unity = set_vendorid(VENDORID_UNITY)) == NULL)
			plog(ASL_LEVEL_ERR, 
			     "Unity vendor ID generation failed\n");
		break;
	default:
		break;
	}
#endif

#ifdef ENABLE_FRAG
	if (iph1->rmconf->ike_frag) {
		vid_frag = set_vendorid(VENDORID_FRAG);
		if (vid_frag != NULL)
			vid_frag = isakmp_frag_addcap(vid_frag,
			    VENDORID_FRAG_AGG);
		if (vid_frag == NULL)
			plog(ASL_LEVEL_ERR, 
			    "Frag vendorID construction failed\n");
	}		
#endif

	/* create CR if need */
	if (iph1->rmconf->send_cr
	 && oakley_needcr(iph1->rmconf->proposal->authmethod)) {
		need_cr = 1;
		cr = oakley_getcr(iph1);
		if (cr == NULL) {
			plog(ASL_LEVEL_ERR, 
				"failed to get CR");
			goto end;
		}
	}

	plog(ASL_LEVEL_DEBUG, "authmethod is %s\n",
		s_oakley_attr_method(iph1->rmconf->proposal->authmethod));

	/* set SA payload to propose */
	plist = isakmp_plist_append(plist, iph1->sa, ISAKMP_NPTYPE_SA);

	/* create isakmp KE payload */
	plist = isakmp_plist_append(plist, iph1->dhpub, ISAKMP_NPTYPE_KE);

	/* create isakmp NONCE payload */
	plist = isakmp_plist_append(plist, iph1->nonce, ISAKMP_NPTYPE_NONCE);

	/* create isakmp ID payload */
	plist = isakmp_plist_append(plist, iph1->id, ISAKMP_NPTYPE_ID);

	/* create isakmp CR payload */
	if (need_cr)
		plist = isakmp_plist_append(plist, cr, ISAKMP_NPTYPE_CR);

#ifdef ENABLE_FRAG
	if (vid_frag)
		plist = isakmp_plist_append(plist, vid_frag, ISAKMP_NPTYPE_VID);
#endif
#ifdef ENABLE_NATT
	/* 
	 * set VID payload for NAT-T if NAT-T 
	 * support allowed in the config file 
	 */
	if (iph1->rmconf->nat_traversal) 
		plist = isakmp_plist_append_natt_vids(plist, vid_natt);
#endif
#ifdef ENABLE_HYBRID
	if (vid_xauth)
		plist = isakmp_plist_append(plist, 
		    vid_xauth, ISAKMP_NPTYPE_VID);
	if (vid_unity)
		plist = isakmp_plist_append(plist, 
		    vid_unity, ISAKMP_NPTYPE_VID);
#endif
#ifdef ENABLE_DPD
	if(iph1->rmconf->dpd){
		vid_dpd = set_vendorid(VENDORID_DPD);
		if (vid_dpd != NULL)
			plist = isakmp_plist_append(plist, vid_dpd, ISAKMP_NPTYPE_VID);
	}
#endif

	iph1->sendbuf = isakmp_plist_set_all (&plist, iph1);

#ifdef HAVE_PRINT_ISAKMP_C
	isakmp_printpacket(iph1->sendbuf, iph1->local, iph1->remote, 0);
#endif

	/* send the packet, add to the schedule to resend */
	iph1->retry_counter = iph1->rmconf->retry_counter;
	if (isakmp_ph1resend(iph1) == -1) {
		plog(ASL_LEVEL_ERR, 
			 "failed to send packet");
		goto end;
	}

	fsm_set_state(&iph1->status, IKEV1_STATE_AGG_I_MSG1SENT);

	error = 0;

	IPSECSESSIONTRACEREVENT(iph1->parent_session,
							IPSECSESSIONEVENTCODE_IKE_PACKET_TX_SUCC,
							CONSTSTR("Initiator, Aggressive-Mode message 1"),
							CONSTSTR(NULL));
	
end:
	if (error) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKE_PACKET_TX_FAIL,
								CONSTSTR("Initiator, Aggressive-Mode Message 1"),
								CONSTSTR("Failed to transmit Aggressive-Mode Message 1"));
	}
	if (cr)
		vfree(cr);
#ifdef ENABLE_FRAG
	if (vid_frag)
		vfree(vid_frag);
#endif
#ifdef ENABLE_NATT
	for (i = 0; i < MAX_NATT_VID_COUNT && vid_natt[i] != NULL; i++)
		vfree(vid_natt[i]);
#endif
#ifdef ENABLE_HYBRID
	if (vid_xauth != NULL)
		vfree(vid_xauth);
	if (vid_unity != NULL)
		vfree(vid_unity);
#endif
#ifdef ENABLE_DPD
	if (vid_dpd != NULL)
		vfree(vid_dpd);
#endif

	return error;
}

/*
 * receive from responder
 * 	psk: HDR, SA, KE, Nr, IDr1, HASH_R
 * 	sig: HDR, SA, KE, Nr, IDr1, [ CR, ] [ CERT, ] SIG_R
 *   gssapi: HDR, SA, KE, Nr, IDr1, GSSr, HASH_R
 * 	rsa: HDR, SA, KE, <IDr1_b>PubKey_i, <Nr_b>PubKey_i, HASH_R
 * 	rev: HDR, SA, <Nr_b>PubKey_i, <KE_b>Ke_r, <IDir_b>Ke_r, HASH_R
 */
int
agg_i2recv(iph1, msg)
	phase1_handle_t *iph1;
	vchar_t *msg;
{
	vchar_t *pbuf = NULL;
	struct isakmp_parse_t *pa;
	vchar_t *satmp = NULL;
	int error = -1;
	int vid_numeric;
	int ptype;
	int received_cert = 0;

#ifdef ENABLE_NATT
	int natd_seq = 0;
	struct natd_payload {
		int seq;
		vchar_t *payload;
		TAILQ_ENTRY(natd_payload) chain;
	};
	TAILQ_HEAD(_natd_payload, natd_payload) natd_tree;
	TAILQ_INIT(&natd_tree);
#endif

    /* validity check */
	if (iph1->status != IKEV1_STATE_AGG_I_MSG1SENT) {
		plog(ASL_LEVEL_ERR,
             "status mismatched %d.\n", iph1->status);
		goto end;
	}

	/* validate the type of next payload */
	pbuf = isakmp_parse(msg);
	if (pbuf == NULL) {
		plog(ASL_LEVEL_ERR, 
			 "failed to parse msg");
		goto end;
	}
	pa = ALIGNED_CAST(struct isakmp_parse_t *)pbuf->v;

	iph1->pl_hash = NULL;

	/* SA payload is fixed postion */
	if (pa->type != ISAKMP_NPTYPE_SA) {
		plog(ASL_LEVEL_ERR,
			"received invalid next payload type %d, "
			"expecting %d.\n",
			pa->type, ISAKMP_NPTYPE_SA);
		goto end;
	}

	if (isakmp_p2ph(&satmp, pa->ptr) < 0) {
		plog(ASL_LEVEL_ERR, 
			 "failed to process SA payload");
		goto end;
	}
	pa++;

	for (/*nothing*/;
	     pa->type != ISAKMP_NPTYPE_NONE;
	     pa++) {

		switch (pa->type) {
		case ISAKMP_NPTYPE_KE:
			if (isakmp_p2ph(&iph1->dhpub_p, pa->ptr) < 0) {
				plog(ASL_LEVEL_ERR, 
					 "failed to process KE payload");
				goto end;
			}
			break;
		case ISAKMP_NPTYPE_NONCE:
			if (isakmp_p2ph(&iph1->nonce_p, pa->ptr) < 0) {
				plog(ASL_LEVEL_ERR, 
					 "failed to process NONCE payload");
				goto end;
			}
			break;
		case ISAKMP_NPTYPE_ID:
			if (isakmp_p2ph(&iph1->id_p, pa->ptr) < 0) {
				plog(ASL_LEVEL_ERR, 
					 "failed to process ID payload");
				goto end;
			}
			break;
		case ISAKMP_NPTYPE_HASH:
			iph1->pl_hash = (struct isakmp_pl_hash *)pa->ptr;
			break;
		case ISAKMP_NPTYPE_CR:
			if (oakley_savecr(iph1, pa->ptr) < 0) {
				plog(ASL_LEVEL_ERR, 
					 "failed to process CR payload");
				goto end;
			}
			break;
		case ISAKMP_NPTYPE_CERT:
			if (oakley_savecert(iph1, pa->ptr) < 0) {
				plog(ASL_LEVEL_ERR, 
					 "failed to process CERT payload");
				goto end;
			}
			received_cert = 1;
			break;
		case ISAKMP_NPTYPE_SIG:
			if (isakmp_p2ph(&iph1->sig_p, pa->ptr) < 0) {
				plog(ASL_LEVEL_ERR, 
					 "failed to process SIG payload");
				goto end;
			}
			break;
		case ISAKMP_NPTYPE_VID:
			vid_numeric = check_vendorid(pa->ptr);
#ifdef ENABLE_NATT
			if (iph1->rmconf->nat_traversal && 
			    natt_vendorid(vid_numeric))
				natt_handle_vendorid(iph1, vid_numeric);
#endif
#ifdef ENABLE_HYBRID
			switch (vid_numeric) {
			case VENDORID_XAUTH:
				iph1->mode_cfg->flags |= 
				    ISAKMP_CFG_VENDORID_XAUTH;
				break;

			case VENDORID_UNITY:
				iph1->mode_cfg->flags |= 
				    ISAKMP_CFG_VENDORID_UNITY;
				break;
			default:
				break;
			}
#endif
#ifdef ENABLE_DPD
			if (vid_numeric == VENDORID_DPD && iph1->rmconf->dpd) {
				iph1->dpd_support=1;
				plog(ASL_LEVEL_DEBUG, 
					 "remote supports DPD\n");
			}
#endif
#ifdef ENABLE_FRAG
			if ((vid_numeric == VENDORID_FRAG) &&
				(vendorid_frag_cap(pa->ptr) & VENDORID_FRAG_AGG)) {
				plog(ASL_LEVEL_DEBUG, 
					 "remote supports FRAGMENTATION\n");
				iph1->frag = 1;
			}
#endif
			break;
		case ISAKMP_NPTYPE_N:
			isakmp_check_notify(pa->ptr, iph1);
			break;

#ifdef ENABLE_NATT
		case ISAKMP_NPTYPE_NATD_DRAFT:
		case ISAKMP_NPTYPE_NATD_RFC:
		case ISAKMP_NPTYPE_NATD_BADDRAFT:
			if (NATT_AVAILABLE(iph1) && iph1->natt_options != NULL &&
			    pa->type == iph1->natt_options->payload_nat_d) {
				struct natd_payload *natd;
				natd = (struct natd_payload *)racoon_malloc(sizeof(*natd));
				if (!natd) {
					plog(ASL_LEVEL_ERR, 
						 "failed to pre-process NATD payload");
					goto end;
				}

				natd->payload = NULL;

				if (isakmp_p2ph (&natd->payload, pa->ptr) < 0) {
					plog(ASL_LEVEL_ERR, 
						 "failed to process NATD payload");
					goto end;
				}

				natd->seq = natd_seq++;

				TAILQ_INSERT_TAIL(&natd_tree, natd, chain);
				break;
			}
			/* %%% Be lenient here - some servers send natd payloads */
			/* when nat not detected								 */
			break;
#endif

		default:
			/* don't send information, see isakmp_ident_r1() */
			plog(ASL_LEVEL_ERR,
				"ignore the packet, "
				"received unexpecting payload type %d.\n",
				pa->type);
			goto end;
		}
	}

	if (received_cert) {
		oakley_verify_certid(iph1);
	}

	/* payload existency check */
	if (iph1->dhpub_p == NULL || iph1->nonce_p == NULL) {
		plog(ASL_LEVEL_ERR,
			"few isakmp message received.\n");
		goto end;
	}

	/* verify identifier */
	if (ipsecdoi_checkid1(iph1) != 0) {
		plog(ASL_LEVEL_ERR,
			"invalid ID payload.\n");
		goto end;
	}

	/* check SA payload and set approval SA for use */
	if (ipsecdoi_checkph1proposal(satmp, iph1) < 0) {
		plog(ASL_LEVEL_ERR,
			"failed to get valid proposal.\n");
		/* XXX send information */
		goto end;
	}
	VPTRINIT(iph1->sa_ret);

	/* fix isakmp index */
	memcpy(&iph1->index.r_ck, &((struct isakmp *)msg->v)->r_ck,
		sizeof(cookie_t));

#ifdef ENABLE_NATT
	if (NATT_AVAILABLE(iph1)) {
		struct natd_payload *natd = NULL;
		int natd_verified;
		
		plog(ASL_LEVEL_INFO,
		     "Selected NAT-T version: %s\n",
		     vid_string_by_id(iph1->natt_options->version));

		/* set both bits first so that we can clear them
		   upon verifying hashes */
		iph1->natt_flags |= NAT_DETECTED;
                        
		while ((natd = TAILQ_FIRST(&natd_tree)) != NULL) {
			/* this function will clear appropriate bits bits 
			   from iph1->natt_flags */
			natd_verified = natt_compare_addr_hash (iph1,
				natd->payload, natd->seq);

			plog (ASL_LEVEL_INFO, "NAT-D payload #%d %s\n",
				natd->seq - 1,
				natd_verified ? "verified" : "doesn't match");
			
			vfree (natd->payload);

			TAILQ_REMOVE(&natd_tree, natd, chain);
			racoon_free (natd);
		}

		plog (ASL_LEVEL_INFO, "NAT %s %s%s\n",
		      iph1->natt_flags & NAT_DETECTED ? 
		      		"detected:" : "not detected",
		      iph1->natt_flags & NAT_DETECTED_ME ? "ME " : "",
		      iph1->natt_flags & NAT_DETECTED_PEER ? "PEER" : "");

		if (iph1->natt_flags & NAT_DETECTED)
			natt_float_ports (iph1);
		ike_session_update_natt_version(iph1);
	}
#endif

	/* compute sharing secret of DH */
#ifdef HAVE_OPENSSL
	if (oakley_dh_compute(iph1->rmconf->dhgrp, iph1->dhpub,
						  iph1->dhpriv, iph1->dhpub_p, &iph1->dhgxy) < 0) {
#else
		if (oakley_dh_compute(iph1->rmconf->dhgrp, iph1->dhpub_p, iph1->publicKeySize, &iph1->dhgxy, &iph1->dhC) < 0) {
#endif
		plog(ASL_LEVEL_ERR, 
			 "failed to compute DH");
		goto end;
	}

	/* generate SKEYIDs & IV & final cipher key */
	if (oakley_skeyid(iph1) < 0) {
		plog(ASL_LEVEL_ERR, 
			 "failed to generate SKEYID");
		goto end;
	}
	if (oakley_skeyid_dae(iph1) < 0) {
		plog(ASL_LEVEL_ERR, 
			 "failed to generate SKEYID-DAE");
		goto end;
	}
	if (oakley_compute_enckey(iph1) < 0) {
		plog(ASL_LEVEL_ERR, 
			 "failed to generate ENCKEY");
		goto end;
	}
	if (oakley_newiv(iph1) < 0) {
		plog(ASL_LEVEL_ERR, 
			 "failed to generate IV");
		goto end;
	}

	/* validate authentication value */
	ptype = oakley_validate_auth(iph1);
	if (ptype != 0) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKEV1_PH1_AUTH_FAIL,
								CONSTSTR("Initiator, Aggressive-Mode Message 2"),
								CONSTSTR("Failed to authenticate, Aggressive-Mode Message 2"));
		if (ptype == -1) {
			/* message printed inner oakley_validate_auth() */
			goto end;
		}
		isakmp_info_send_n1(iph1, ptype, NULL);
		goto end;
	}
	IPSECSESSIONTRACEREVENT(iph1->parent_session,
							IPSECSESSIONEVENTCODE_IKEV1_PH1_AUTH_SUCC,
							CONSTSTR("Initiator, Aggressive-Mode Message 2"),
							CONSTSTR(NULL));
	
	if (oakley_checkcr(iph1) < 0) {
		/* Ignore this error in order to be interoperability. */
		;
	}

	/* change status of isakmp status entry */
	fsm_set_state(&iph1->status, IKEV1_STATE_AGG_I_MSG2RCVD);

#ifdef ENABLE_VPNCONTROL_PORT
	vpncontrol_notify_phase_change(1, FROM_REMOTE, iph1, NULL);
#endif

	error = 0;

	IPSECSESSIONTRACEREVENT(iph1->parent_session,
							IPSECSESSIONEVENTCODE_IKE_PACKET_RX_SUCC,
							CONSTSTR("Initiator, Aggressive-Mode message 2"),
							CONSTSTR(NULL));
	
end:
	if (error) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKE_PACKET_RX_FAIL,
								CONSTSTR("Initiator, Aggressive-Mode Message 2"),
								CONSTSTR("Failure processing Aggressive-Mode Message 2"));
	}

	if (pbuf)
		vfree(pbuf);
	if (satmp)
		vfree(satmp);
	if (error) {
		VPTRINIT(iph1->dhpub_p);
		VPTRINIT(iph1->nonce_p);
		VPTRINIT(iph1->id_p);
		oakley_delcert(iph1->cert_p);
		iph1->cert_p = NULL;
		oakley_delcert(iph1->crl_p);
		iph1->crl_p = NULL;
		VPTRINIT(iph1->sig_p);
		oakley_delcert(iph1->cr_p);
		iph1->cr_p = NULL;
	}

	return error;
}

/*
 * send to responder
 * 	psk: HDR, HASH_I
 *   gssapi: HDR, HASH_I
 * 	sig: HDR, [ CERT, ] SIG_I
 * 	rsa: HDR, HASH_I
 * 	rev: HDR, HASH_I
 */
int
agg_i3send(iph1, msg)
	phase1_handle_t *iph1;
	vchar_t *msg;
{
	struct payload_list *plist = NULL;
	int need_cert = 0;
	int error = -1;
	vchar_t *gsshash = NULL;
#ifdef ENABLE_NATT
	vchar_t *natd[2] = { NULL, NULL };
#endif
    vchar_t *notp_unity = NULL;
    vchar_t *notp_ini = NULL;

    /* validity check */
	if (iph1->status != IKEV1_STATE_AGG_I_MSG2RCVD) {
		plog(ASL_LEVEL_ERR,
             "status mismatched %d.\n", iph1->status);
		goto end;
	}

	/* generate HASH to send */
	plog(ASL_LEVEL_DEBUG, "generate HASH_I\n");
	iph1->hash = oakley_ph1hash_common(iph1, GENERATE);
	if (iph1->hash == NULL) {
		plog(ASL_LEVEL_ERR,
			 "failed to generate HASH");
		goto end;
	}

	switch (AUTHMETHOD(iph1)) {
	case OAKLEY_ATTR_AUTH_METHOD_PSKEY:
#ifdef ENABLE_HYBRID
	case FICTIVE_AUTH_METHOD_XAUTH_PSKEY_I:
	case OAKLEY_ATTR_AUTH_METHOD_HYBRID_RSA_I:
#endif  
		/* set HASH payload */
		plist = isakmp_plist_append(plist, 
		    iph1->hash, ISAKMP_NPTYPE_HASH);
		break;

	case OAKLEY_ATTR_AUTH_METHOD_RSASIG:
#ifdef ENABLE_HYBRID
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSASIG_I:
#endif
		/* XXX if there is CR or not ? */

		if (oakley_getmycert(iph1) < 0) {
			plog(ASL_LEVEL_ERR, 
				 "failed to get mycert");
			goto end;
		}

		if (oakley_getsign(iph1) < 0) {
			plog(ASL_LEVEL_ERR, 
				 "failed to get sign");
			goto end;
		}

		if (iph1->cert != NULL && iph1->rmconf->send_cert)
			need_cert = 1;

		/* add CERT payload if there */
		// we don't support sending of certchains
		if (need_cert)
			plist = isakmp_plist_append(plist, iph1->cert->pl, ISAKMP_NPTYPE_CERT);

		/* add SIG payload */
		plist = isakmp_plist_append(plist, iph1->sig, ISAKMP_NPTYPE_SIG);
		break;

	case OAKLEY_ATTR_AUTH_METHOD_RSAENC:
	case OAKLEY_ATTR_AUTH_METHOD_RSAREV:
#ifdef ENABLE_HYBRID
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAENC_I:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAREV_I:
#endif
		break;
	}

#ifdef ENABLE_NATT
	/* generate NAT-D payloads */
	if (NATT_AVAILABLE(iph1)) {
		plog (ASL_LEVEL_INFO, "Adding remote and local NAT-D payloads.\n");
		if ((natd[0] = natt_hash_addr (iph1, iph1->remote)) == NULL) {
			plog(ASL_LEVEL_ERR, 
				"NAT-D hashing failed for %s\n", saddr2str((struct sockaddr *)iph1->remote));
			goto end;
		}

		if ((natd[1] = natt_hash_addr (iph1, iph1->local)) == NULL) {
			plog(ASL_LEVEL_ERR, 
				"NAT-D hashing failed for %s\n", saddr2str((struct sockaddr *)iph1->local));
			goto end;
		}
		/* old Apple version sends natd payloads in the wrong order */
		if (iph1->natt_options->version == VENDORID_NATT_APPLE) {
			plist = isakmp_plist_append(plist, natd[1], iph1->natt_options->payload_nat_d);
			plist = isakmp_plist_append(plist, natd[0], iph1->natt_options->payload_nat_d);
		} else
		{
			plist = isakmp_plist_append(plist, natd[0], iph1->natt_options->payload_nat_d);
			plist = isakmp_plist_append(plist, natd[1], iph1->natt_options->payload_nat_d);
		}
	}
#endif


	iph1->sendbuf = isakmp_plist_set_all (&plist, iph1);
	
#ifdef HAVE_PRINT_ISAKMP_C
	isakmp_printpacket(iph1->sendbuf, iph1->local, iph1->remote, 0);
#endif


	/* send to responder */
	if (isakmp_send(iph1, iph1->sendbuf) < 0) {
		plog(ASL_LEVEL_ERR, 
			 "failed to send packet");
		goto end;
	}

	/* the sending message is added to the received-list. */
	if (ike_session_add_recvdpkt(iph1->remote, iph1->local, iph1->sendbuf, msg,
                     PH1_NON_ESP_EXTRA_LEN(iph1, iph1->sendbuf), PH1_FRAG_FLAGS(iph1)) == -1) {
		plog(ASL_LEVEL_ERR , 
			"failed to add a response packet to the tree.\n");
		goto end;
	}

	/* set encryption flag */
	iph1->flags |= ISAKMP_FLAG_E;

	fsm_set_state(&iph1->status, IKEV1_STATE_PHASE1_ESTABLISHED);

	IPSECSESSIONTRACEREVENT(iph1->parent_session,
							IPSECSESSIONEVENTCODE_IKEV1_PH1_INIT_SUCC,
							CONSTSTR("Initiator, Aggressive-Mode"),
							CONSTSTR(NULL));

	error = 0;

	IPSECSESSIONTRACEREVENT(iph1->parent_session,
							IPSECSESSIONEVENTCODE_IKE_PACKET_TX_SUCC,
							CONSTSTR("Initiator, Aggressive-Mode message 3"),
							CONSTSTR(NULL));

end:
	if (error) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKE_PACKET_TX_FAIL,
								CONSTSTR("Initiator, Aggressive-Mode Message 3"),
								CONSTSTR("Failed to transmit Aggressive-Mode Message 3"));
	}
#ifdef ENABLE_NATT
	if (natd[0])
		vfree(natd[0]);
	if (natd[1])
		vfree(natd[1]);
#endif
	if (notp_unity)
		vfree(notp_unity);
	if (notp_ini)
		vfree(notp_ini);
	if (gsshash)
		vfree(gsshash);
	return error;
}

/*
 * receive from initiator
 * 	psk: HDR, SA, KE, Ni, IDi1
 * 	sig: HDR, SA, KE, Ni, IDi1 [, CR ]
 *   gssapi: HDR, SA, KE, Ni, IDi1 , GSSi
 * 	rsa: HDR, SA, [ HASH(1),] KE, <IDi1_b>Pubkey_r, <Ni_b>Pubkey_r
 * 	rev: HDR, SA, [ HASH(1),] <Ni_b>Pubkey_r, <KE_b>Ke_i,
 * 	     <IDii_b>Ke_i [, <Cert-I_b>Ke_i ]
 */
int
agg_r1recv(iph1, msg)
	phase1_handle_t *iph1;
	vchar_t *msg;
{
	int error = -1;
	vchar_t *pbuf = NULL;
	struct isakmp_parse_t *pa;
	int vid_numeric;

    /* validity check */
	if (iph1->status != IKEV1_STATE_AGG_R_START) {
		plog(ASL_LEVEL_ERR,
             "status mismatched %d.\n", iph1->status);
		goto end;
	}

	/* validate the type of next payload */
	pbuf = isakmp_parse(msg);
	if (pbuf == NULL) {
		plog(ASL_LEVEL_ERR, 
			 "failed to parse msg");
		goto end;
	}
	pa = ALIGNED_CAST(struct isakmp_parse_t *)pbuf->v;

	/* SA payload is fixed postion */
	if (pa->type != ISAKMP_NPTYPE_SA) {
		plog(ASL_LEVEL_ERR,
			"received invalid next payload type %d, "
			"expecting %d.\n",
			pa->type, ISAKMP_NPTYPE_SA);
		goto end;
	}
	if (isakmp_p2ph(&iph1->sa, pa->ptr) < 0) {
		plog(ASL_LEVEL_ERR, 
			 "failed to process SA payload");
		goto end;
	}
	pa++;

	for (/*nothing*/;
	     pa->type != ISAKMP_NPTYPE_NONE;
	     pa++) {

		plog(ASL_LEVEL_DEBUG, 
			"received payload of type %s\n",
			s_isakmp_nptype(pa->type));

		switch (pa->type) {
		case ISAKMP_NPTYPE_KE:
			if (isakmp_p2ph(&iph1->dhpub_p, pa->ptr) < 0) {
				plog(ASL_LEVEL_ERR, 
					 "failed to process KE payload");
				goto end;
			}
			break;
		case ISAKMP_NPTYPE_NONCE:
			if (isakmp_p2ph(&iph1->nonce_p, pa->ptr) < 0) {
				plog(ASL_LEVEL_ERR, 
					 "failed to process NONCE payload");
				goto end;
			}
			break;
		case ISAKMP_NPTYPE_ID:
			if (isakmp_p2ph(&iph1->id_p, pa->ptr) < 0) {
				plog(ASL_LEVEL_ERR, 
					 "failed to process ID payload");
				goto end;
			}
			break;
		case ISAKMP_NPTYPE_VID:
			vid_numeric = check_vendorid(pa->ptr);

#ifdef ENABLE_NATT
			if (iph1->rmconf->nat_traversal &&
			    natt_vendorid(vid_numeric)) {
				natt_handle_vendorid(iph1, vid_numeric);
				break;
			}
#endif
#ifdef ENABLE_HYBRID
			switch (vid_numeric) {
			case VENDORID_XAUTH:
				iph1->mode_cfg->flags |= 
				    ISAKMP_CFG_VENDORID_XAUTH;
				break;

			case VENDORID_UNITY:
				iph1->mode_cfg->flags |= 
				    ISAKMP_CFG_VENDORID_UNITY;
				break;
			default:
				break;
			}
#endif
#ifdef ENABLE_DPD
			if (vid_numeric == VENDORID_DPD && iph1->rmconf->dpd) {
				iph1->dpd_support=1;
				plog(ASL_LEVEL_DEBUG, 
					 "remote supports DPD\n");
			}
#endif
#ifdef ENABLE_FRAG
			if ((vid_numeric == VENDORID_FRAG) &&
				(vendorid_frag_cap(pa->ptr) & VENDORID_FRAG_AGG)) {
				plog(ASL_LEVEL_DEBUG, 
					 "remote supports FRAGMENTATION\n");
				iph1->frag = 1;
			}
#endif
			break;

		case ISAKMP_NPTYPE_CR:
			if (oakley_savecr(iph1, pa->ptr) < 0) {
				plog(ASL_LEVEL_ERR, 
					 "failed to process CR payload");
				goto end;
			}
			break;

		default:
			/* don't send information, see isakmp_ident_r1() */
			plog(ASL_LEVEL_ERR,
				"ignore the packet, "
				"received unexpecting payload type %d.\n",
				pa->type);
			goto end;
		}
	}

	/* payload existency check */
	if (iph1->dhpub_p == NULL || iph1->nonce_p == NULL) {
		plog(ASL_LEVEL_ERR,
			"few isakmp message received.\n");
		goto end;
	}

	/* verify identifier */
	if (ipsecdoi_checkid1(iph1) != 0) {
		plog(ASL_LEVEL_ERR,
			"invalid ID payload.\n");
		goto end;
	}

#ifdef ENABLE_NATT
	if (NATT_AVAILABLE(iph1)) {
		plog(ASL_LEVEL_INFO,
		     "Selected NAT-T version: %s\n",
		     vid_string_by_id(iph1->natt_options->version));
		ike_session_update_natt_version(iph1);
	}
#endif

	/* check SA payload and set approval SA for use */
	if (ipsecdoi_checkph1proposal(iph1->sa, iph1) < 0) {
		plog(ASL_LEVEL_ERR,
			"failed to get valid proposal.\n");
		/* XXX send information */
		goto end;
	}

	if (oakley_checkcr(iph1) < 0) {
		/* Ignore this error in order to be interoperability. */
		;
	}

	fsm_set_state(&iph1->status, IKEV1_STATE_AGG_R_MSG1RCVD);

	error = 0;

	IPSECSESSIONTRACEREVENT(iph1->parent_session,
							IPSECSESSIONEVENTCODE_IKE_PACKET_RX_SUCC,
							CONSTSTR("Responder, Aggressive-Mode message 1"),
							CONSTSTR(NULL));
	
end:
	if (error) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKE_PACKET_RX_FAIL,
								CONSTSTR("Responder, Aggressive-Mode Message 1"),
								CONSTSTR("Failed to process Aggressive-Mode Message 1"));
	}

	if (pbuf)
		vfree(pbuf);
	if (error) {
		VPTRINIT(iph1->sa);
		VPTRINIT(iph1->dhpub_p);
		VPTRINIT(iph1->nonce_p);
		VPTRINIT(iph1->id_p);
		oakley_delcert(iph1->cr_p);
		iph1->cr_p = NULL;
	}

	return error;
}

/*
 * send to initiator
 * 	psk: HDR, SA, KE, Nr, IDr1, HASH_R
 * 	sig: HDR, SA, KE, Nr, IDr1, [ CR, ] [ CERT, ] SIG_R
 *   gssapi: HDR, SA, KE, Nr, IDr1, GSSr, HASH_R
 * 	rsa: HDR, SA, KE, <IDr1_b>PubKey_i, <Nr_b>PubKey_i, HASH_R
 * 	rev: HDR, SA, <Nr_b>PubKey_i, <KE_b>Ke_r, <IDir_b>Ke_r, HASH_R
 */
int
agg_r2send(iph1, msg)
	phase1_handle_t *iph1;
	vchar_t *msg;
{
	struct payload_list *plist = NULL;
	int need_cr = 0;
	int need_cert = 0;
	vchar_t *cr = NULL;
	int error = -1;
#ifdef ENABLE_HYBRID
	vchar_t *xauth_vid = NULL;
	vchar_t *unity_vid = NULL;
#endif
#ifdef ENABLE_NATT
	vchar_t *vid_natt = NULL;
	vchar_t *natd[2] = { NULL, NULL };
#endif
#ifdef ENABLE_DPD
	vchar_t *vid_dpd = NULL;
#endif
#ifdef ENABLE_FRAG
	vchar_t *vid_frag = NULL;
#endif

    /* validity check */
	if (iph1->status != IKEV1_STATE_AGG_R_MSG1RCVD) {
		plog(ASL_LEVEL_ERR,
             "status mismatched %d.\n", iph1->status);
		goto end;
	}

	/* set responder's cookie */
	isakmp_newcookie((caddr_t)&iph1->index.r_ck, iph1->remote, iph1->local);

	/* make ID payload into isakmp status */
	if (ipsecdoi_setid1(iph1) < 0) {
		plog(ASL_LEVEL_ERR, 
			 "failed to set ID");
		goto end;
	}

	/* generate DH public value */
#ifdef HAVE_OPENSSL
	if (oakley_dh_generate(iph1->rmconf->dhgrp,
						   &iph1->dhpub, &iph1->dhpriv) < 0) {	
#else
	if (oakley_dh_generate(iph1->rmconf->dhgrp,
						   &iph1->dhpub, &iph1->publicKeySize, &iph1->dhC) < 0) {
#endif
		plog(ASL_LEVEL_ERR, 
			 "failed to generate DH");
		goto end;
	}

	/* generate NONCE value */
	iph1->nonce = eay_set_random(iph1->rmconf->nonce_size);
	if (iph1->nonce == NULL) {
		plog(ASL_LEVEL_ERR, 
			 "failed to generate NONCE");
		goto end;
	}

	/* compute sharing secret of DH */
#ifdef HAVE_OPENSSL
		if (oakley_dh_compute(iph1->approval->dhgrp, iph1->dhpub,
							  iph1->dhpriv, iph1->dhpub_p, &iph1->dhgxy) < 0) {
#else
	if (oakley_dh_compute(iph1->approval->dhgrp, iph1->dhpub_p, iph1->publicKeySize, &iph1->dhgxy, &iph1->dhC) < 0) {
#endif
		plog(ASL_LEVEL_ERR, 
			 "failed to compute DH");
		goto end;
	}

	/* generate SKEYIDs & IV & final cipher key */
	if (oakley_skeyid(iph1) < 0) {
		plog(ASL_LEVEL_ERR, 
			 "failed to generate SKEYID");
		goto end;
	}
	if (oakley_skeyid_dae(iph1) < 0) {
		plog(ASL_LEVEL_ERR, 
			 "failed to generate SKEYID-DAE");
		goto end;
	}
	if (oakley_compute_enckey(iph1) < 0) {
		plog(ASL_LEVEL_ERR, 
			 "failed to generate ENCKEY");
		goto end;
	}
	if (oakley_newiv(iph1) < 0) {
		plog(ASL_LEVEL_ERR, 
			 "failed to generate IV");
		goto end;
	}

	/* generate HASH to send */
	plog(ASL_LEVEL_DEBUG, "generate HASH_R\n");
	iph1->hash = oakley_ph1hash_common(iph1, GENERATE);
	if (iph1->hash == NULL) {
		plog(ASL_LEVEL_ERR, 
			 "failed to generate GSS HASH");
		goto end;
	}

	/* create CR if need */
	if (iph1->rmconf->send_cr
	 && oakley_needcr(iph1->approval->authmethod)) {
		need_cr = 1;
		cr = oakley_getcr(iph1);
		if (cr == NULL) {
			plog(ASL_LEVEL_ERR, 
				"failed to get CR.\n");
			goto end;
		}
	}

#ifdef ENABLE_NATT
	/* Has the peer announced NAT-T? */
	if (NATT_AVAILABLE(iph1)) {
	  	/* set chosen VID */
		vid_natt = set_vendorid(iph1->natt_options->version);

		/* generate NAT-D payloads */
		plog (ASL_LEVEL_INFO, "Adding remote and local NAT-D payloads.\n");
		if ((natd[0] = natt_hash_addr (iph1, iph1->remote)) == NULL) {
			plog(ASL_LEVEL_ERR, 
				"NAT-D hashing failed for %s\n", saddr2str((struct sockaddr *)iph1->remote));
			goto end;
		}

		if ((natd[1] = natt_hash_addr (iph1, iph1->local)) == NULL) {
			plog(ASL_LEVEL_ERR, 
				"NAT-D hashing failed for %s\n", saddr2str((struct sockaddr *)iph1->local));
			goto end;
		}
	}
#endif
#ifdef ENABLE_DPD
	/* Only send DPD support if remote announced DPD and if DPD support is active */
	if (iph1->dpd_support && iph1->rmconf->dpd)
		vid_dpd = set_vendorid(VENDORID_DPD);
#endif
#ifdef ENABLE_FRAG
	if (iph1->frag) {
		vid_frag = set_vendorid(VENDORID_FRAG);
		if (vid_frag != NULL)
			vid_frag = isakmp_frag_addcap(vid_frag,
			    VENDORID_FRAG_AGG);
		if (vid_frag == NULL)
			plog(ASL_LEVEL_ERR, 
			    "Frag vendorID construction failed\n");
	}
#endif

	switch (AUTHMETHOD(iph1)) {
	case OAKLEY_ATTR_AUTH_METHOD_PSKEY:
#ifdef ENABLE_HYBRID
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_PSKEY_R:
#endif
		/* set SA payload to reply */
		plist = isakmp_plist_append(plist, iph1->sa_ret, ISAKMP_NPTYPE_SA);

		/* create isakmp KE payload */
		plist = isakmp_plist_append(plist, iph1->dhpub, ISAKMP_NPTYPE_KE);

		/* create isakmp NONCE payload */
		plist = isakmp_plist_append(plist, iph1->nonce, ISAKMP_NPTYPE_NONCE);

		/* create isakmp ID payload */
		plist = isakmp_plist_append(plist, iph1->id, ISAKMP_NPTYPE_ID);

		/* create isakmp HASH payload */
		plist = isakmp_plist_append(plist, 
		    iph1->hash, ISAKMP_NPTYPE_HASH);

		/* create isakmp CR payload if needed */
		if (need_cr)
			plist = isakmp_plist_append(plist, cr, ISAKMP_NPTYPE_CR);
		break;
	case OAKLEY_ATTR_AUTH_METHOD_RSASIG:
#ifdef ENABLE_HYBRID
	case OAKLEY_ATTR_AUTH_METHOD_HYBRID_RSA_R:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSASIG_R:
#endif
		/* XXX if there is CR or not ? */

		if (oakley_getmycert(iph1) < 0) {
			plog(ASL_LEVEL_ERR, 
				 "failed to get mycert");
			goto end;
		}

		if (oakley_getsign(iph1) < 0) {
			plog(ASL_LEVEL_ERR, 
				 "failed to get sign");
			goto end;
		}

		if (iph1->cert != NULL && iph1->rmconf->send_cert)
			need_cert = 1;

		/* set SA payload to reply */
		plist = isakmp_plist_append(plist, iph1->sa_ret, ISAKMP_NPTYPE_SA);

		/* create isakmp KE payload */
		plist = isakmp_plist_append(plist, iph1->dhpub, ISAKMP_NPTYPE_KE);

		/* create isakmp NONCE payload */
		plist = isakmp_plist_append(plist, iph1->nonce, ISAKMP_NPTYPE_NONCE);

		/* add ID payload */
		plist = isakmp_plist_append(plist, iph1->id, ISAKMP_NPTYPE_ID);

		/* add CERT payload if there */
		if (need_cert)
			plist = isakmp_plist_append(plist, iph1->cert->pl, ISAKMP_NPTYPE_CERT);

		/* add SIG payload */
		plist = isakmp_plist_append(plist, iph1->sig, ISAKMP_NPTYPE_SIG);

		/* create isakmp CR payload if needed */
		if (need_cr)
			plist = isakmp_plist_append(plist, 
			    cr, ISAKMP_NPTYPE_CR);
		break;

	case OAKLEY_ATTR_AUTH_METHOD_RSAENC:
	case OAKLEY_ATTR_AUTH_METHOD_RSAREV:
#ifdef ENABLE_HYBRID
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAENC_R:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAREV_R:
#endif
		break;
	}

#ifdef ENABLE_HYBRID
	if (iph1->mode_cfg->flags & ISAKMP_CFG_VENDORID_XAUTH) {
		plog (ASL_LEVEL_INFO, "Adding xauth VID payload.\n");
		if ((xauth_vid = set_vendorid(VENDORID_XAUTH)) == NULL) {
			plog(ASL_LEVEL_ERR, 
			    "Cannot create Xauth vendor ID\n");
			goto end;
		}
		plist = isakmp_plist_append(plist, 
		    xauth_vid, ISAKMP_NPTYPE_VID);
	}

	if (iph1->mode_cfg->flags & ISAKMP_CFG_VENDORID_UNITY) {
		if ((unity_vid = set_vendorid(VENDORID_UNITY)) == NULL) {
			plog(ASL_LEVEL_ERR, 
			    "Cannot create Unity vendor ID\n");
			goto end;
		}
		plist = isakmp_plist_append(plist, 
		    unity_vid, ISAKMP_NPTYPE_VID);
	}
#endif

#ifdef ENABLE_NATT
	/* append NAT-T payloads */
	if (vid_natt) {
		/* chosen VID */
		plist = isakmp_plist_append(plist, vid_natt, ISAKMP_NPTYPE_VID);
		/* NAT-D */
		/* old Apple version sends natd payloads in the wrong order */
		if (iph1->natt_options->version == VENDORID_NATT_APPLE) {
			plist = isakmp_plist_append(plist, natd[1], iph1->natt_options->payload_nat_d);
			plist = isakmp_plist_append(plist, natd[0], iph1->natt_options->payload_nat_d);
		} else
		{
			plist = isakmp_plist_append(plist, natd[0], iph1->natt_options->payload_nat_d);
			plist = isakmp_plist_append(plist, natd[1], iph1->natt_options->payload_nat_d);
		}
	}
#endif

#ifdef ENABLE_FRAG
	if (vid_frag)
		plist = isakmp_plist_append(plist, vid_frag, ISAKMP_NPTYPE_VID);
#endif

#ifdef ENABLE_DPD
	if (vid_dpd)
		plist = isakmp_plist_append(plist, vid_dpd, ISAKMP_NPTYPE_VID);
#endif

	iph1->sendbuf = isakmp_plist_set_all (&plist, iph1);

#ifdef HAVE_PRINT_ISAKMP_C
	isakmp_printpacket(iph1->sendbuf, iph1->local, iph1->remote, 1);
#endif

	/* send the packet, add to the schedule to resend */
	iph1->retry_counter = iph1->rmconf->retry_counter;
	if (isakmp_ph1resend(iph1) == -1) {
		plog(ASL_LEVEL_ERR , 
			 "failed to send packet");
		goto end;
	}

	/* the sending message is added to the received-list. */
	if (ike_session_add_recvdpkt(iph1->remote, iph1->local, iph1->sendbuf, msg,
                     PH1_NON_ESP_EXTRA_LEN(iph1, iph1->sendbuf), PH1_FRAG_FLAGS(iph1)) == -1) {
		plog(ASL_LEVEL_ERR , 
			"failed to add a response packet to the tree.\n");
		goto end;
	}

	fsm_set_state(&iph1->status, IKEV1_STATE_AGG_R_MSG2SENT);

#ifdef ENABLE_VPNCONTROL_PORT
	vpncontrol_notify_phase_change(1, FROM_LOCAL, iph1, NULL);
#endif

	error = 0;

	IPSECSESSIONTRACEREVENT(iph1->parent_session,
							IPSECSESSIONEVENTCODE_IKE_PACKET_TX_SUCC,
							CONSTSTR("Responder, Aggressive-Mode message 2"),
							CONSTSTR(NULL));
	
end:
	if (error) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKE_PACKET_TX_FAIL,
								CONSTSTR("Responder, Aggressive-Mode Message 2"),
								CONSTSTR("Failed to process Aggressive-Mode Message 2"));
	}
	if (cr)
		vfree(cr);
#ifdef ENABLE_HYBRID
	if (xauth_vid)
		vfree(xauth_vid);
	if (unity_vid)
		vfree(unity_vid);
#endif
#ifdef ENABLE_NATT
	if (vid_natt)
		vfree(vid_natt);
	if (natd[0])
		vfree(natd[0]);
	if (natd[1])
		vfree(natd[1]);
#endif
#ifdef ENABLE_DPD
	if (vid_dpd)
		vfree(vid_dpd);
#endif
#ifdef ENABLE_FRAG
	if (vid_frag)
		vfree(vid_frag);
#endif

	return error;
}

/*
 * receive from initiator
 * 	psk: HDR, HASH_I
 *   gssapi: HDR, HASH_I
 * 	sig: HDR, [ CERT, ] SIG_I
 * 	rsa: HDR, HASH_I
 * 	rev: HDR, HASH_I
 */
int
agg_r3recv(iph1, msg0)
	phase1_handle_t *iph1;
	vchar_t *msg0;
{
	vchar_t *msg = NULL;
	vchar_t *pbuf = NULL;
	struct isakmp_parse_t *pa;
	int error = -1;
	int ptype;

#ifdef ENABLE_NATT
	int natd_seq = 0;
#endif
	int received_cert = 0;

    /* validity check */
	if (iph1->status != IKEV1_STATE_AGG_R_MSG2SENT) {
		plog(ASL_LEVEL_ERR,
             "status mismatched %d.\n", iph1->status);
		goto end;
	}

	/* decrypting if need. */
	/* XXX configurable ? */
	if (ISSET(((struct isakmp *)msg0->v)->flags, ISAKMP_FLAG_E)) {
		msg = oakley_do_decrypt(iph1, msg0,
					iph1->ivm->iv, iph1->ivm->ive);
		if (msg == NULL) {
			plog(ASL_LEVEL_ERR, 
				 "failed to decrypt msg");
			goto end;
		}
	} else
		msg = vdup(msg0);

	/* validate the type of next payload */
	pbuf = isakmp_parse(msg);
	if (pbuf == NULL) {
		plog(ASL_LEVEL_ERR, 
			 "failed to parse msg");
		goto end;
	}

	iph1->pl_hash = NULL;

	for (pa = ALIGNED_CAST(struct isakmp_parse_t *)pbuf->v;
	     pa->type != ISAKMP_NPTYPE_NONE;
	     pa++) {

		switch (pa->type) {
		case ISAKMP_NPTYPE_HASH:
			iph1->pl_hash = (struct isakmp_pl_hash *)pa->ptr;
			break;
		case ISAKMP_NPTYPE_VID:
			(void)check_vendorid(pa->ptr);
			break;
		case ISAKMP_NPTYPE_CERT:
			if (oakley_savecert(iph1, pa->ptr) < 0) {
				plog(ASL_LEVEL_ERR, 
					 "failed to process CERT payload");
				goto end;
			}
			received_cert = 1;
			break;
		case ISAKMP_NPTYPE_SIG:
			if (isakmp_p2ph(&iph1->sig_p, pa->ptr) < 0) {
				plog(ASL_LEVEL_ERR, 
					 "failed to process SIG payload");
				goto end;
			}
			break;
		case ISAKMP_NPTYPE_N:
			isakmp_check_notify(pa->ptr, iph1);
			break;

#ifdef ENABLE_NATT
		case ISAKMP_NPTYPE_NATD_DRAFT:
		case ISAKMP_NPTYPE_NATD_RFC:
			if (NATT_AVAILABLE(iph1) && iph1->natt_options != NULL &&
				pa->type == iph1->natt_options->payload_nat_d)
			{
				vchar_t *natd_received = NULL;
				int natd_verified;
				
				if (isakmp_p2ph (&natd_received, pa->ptr) < 0) {
					plog(ASL_LEVEL_ERR, 
						 "failed to process NATD payload");
					goto end;
				}
				
				if (natd_seq == 0)
					iph1->natt_flags |= NAT_DETECTED;
				
				natd_verified = natt_compare_addr_hash (iph1,
					natd_received, natd_seq++);
				
				plog (ASL_LEVEL_INFO, "NAT-D payload #%d %s\n",
					natd_seq - 1,
					natd_verified ? "verified" : "doesn't match");
				
				vfree (natd_received);
				break;
			}
			/* %%%% Be lenient here - some servers send natd payloads */
			/* when no nat is detected								  */
			break;
#endif

		default:
			/* don't send information, see isakmp_ident_r1() */
			plog(ASL_LEVEL_ERR,
				"ignore the packet, "
				"received unexpecting payload type %d.\n",
				pa->type);
			goto end;
		}
	}

#ifdef ENABLE_NATT
	if (NATT_AVAILABLE(iph1))
		plog (ASL_LEVEL_INFO, "NAT %s %s%s\n",
		      iph1->natt_flags & NAT_DETECTED ? 
		      		"detected:" : "not detected",
		      iph1->natt_flags & NAT_DETECTED_ME ? "ME " : "",
		      iph1->natt_flags & NAT_DETECTED_PEER ? "PEER" : "");
#endif

	if (received_cert) {
		oakley_verify_certid(iph1);
	}
	
	/* validate authentication value */
	ptype = oakley_validate_auth(iph1);
	if (ptype != 0) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKEV1_PH1_AUTH_FAIL,
								CONSTSTR("Responder, Aggressive-Mode Message 3"),
								CONSTSTR("Failed to authenticate Aggressive-Mode Message 3"));
		if (ptype == -1) {
			/* message printed inner oakley_validate_auth() */
			goto end;
		}
		isakmp_info_send_n1(iph1, ptype, NULL);
		goto end;
	}
	IPSECSESSIONTRACEREVENT(iph1->parent_session,
							IPSECSESSIONEVENTCODE_IKEV1_PH1_AUTH_SUCC,
							CONSTSTR("Responder, Aggressive-Mode Message 3"),
							CONSTSTR(NULL));

	fsm_set_state(&iph1->status, IKEV1_STATE_AGG_R_MSG3RCVD);

	error = 0;

	IPSECSESSIONTRACEREVENT(iph1->parent_session,
							IPSECSESSIONEVENTCODE_IKE_PACKET_RX_SUCC,
							CONSTSTR("Responder, Aggressive-Mode message 3"),
							CONSTSTR(NULL));
	
end:
	if (error) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKE_PACKET_RX_FAIL,
								CONSTSTR("Responder, Aggressive-Mode Message 3"),
								CONSTSTR("Failed to process Aggressive-Mode Message 3"));
	}
	if (pbuf)
		vfree(pbuf);
	if (msg)
		vfree(msg);
	if (error) {
		oakley_delcert(iph1->cert_p);
		iph1->cert_p = NULL;
		oakley_delcert(iph1->crl_p);
		iph1->crl_p = NULL;
		VPTRINIT(iph1->sig_p);
	}

	return error;
}

/*
 * status update and establish isakmp sa.
 */
int
agg_rfinalize(iph1, msg)
	phase1_handle_t *iph1;
	vchar_t *msg;
{
	int error = -1;

    /* validity check */
	if (iph1->status != IKEV1_STATE_AGG_R_MSG3RCVD) {
		plog(ASL_LEVEL_ERR,
             "status mismatched %d.\n", iph1->status);
		goto end;
	}

	/* IV synchronized when packet encrypted. */
	/* see handler.h about IV synchronization. */
	if (ISSET(((struct isakmp *)msg->v)->flags, ISAKMP_FLAG_E))
		memcpy(iph1->ivm->iv->v, iph1->ivm->ive->v, iph1->ivm->iv->l);

	/* set encryption flag */
	iph1->flags |= ISAKMP_FLAG_E;

	fsm_set_state(&iph1->status, IKEV1_STATE_PHASE1_ESTABLISHED);

	IPSECSESSIONTRACEREVENT(iph1->parent_session,
							IPSECSESSIONEVENTCODE_IKEV1_PH1_RESP_SUCC,
							CONSTSTR("Responder, Aggressive-Mode"),
							CONSTSTR(NULL));
	
	error = 0;

end:
	return error;
}
