/*	$NetBSD: isakmp_ident.c,v 1.6 2006/10/02 21:41:59 manu Exp $	*/

/* Id: isakmp_ident.c,v 1.21 2006/04/06 16:46:08 manubsd Exp */

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

/* Identity Protecion Exchange (Main Mode) */

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
#include "isakmp_ident.h"
#include "isakmp_inf.h"
#include "vendorid.h"

#ifdef ENABLE_NATT
#include "nattraversal.h"
#endif
#ifdef ENABLE_HYBRID
#include <resolv.h>
#include "isakmp_xauth.h"
#include "isakmp_cfg.h"
#endif
#ifdef ENABLE_FRAG 
#include "isakmp_frag.h"
#endif

#include "vpn_control.h"
#include "vpn_control_var.h"
#include "ipsecSessionTracer.h"
#include "ipsecMessageTracer.h"
#ifndef HAVE_OPENSSL
#include <Security/SecDH.h>
#endif

static vchar_t *ident_ir2mx (phase1_handle_t *);
static vchar_t *ident_ir3mx (phase1_handle_t *);

/* %%%
 * begin Identity Protection Mode as initiator.
 */
/*
 * send to responder
 * 	psk: HDR, SA
 * 	sig: HDR, SA
 * 	rsa: HDR, SA
 * 	rev: HDR, SA
 */
int
ident_i1send(iph1, msg)
	phase1_handle_t *iph1;
	vchar_t *msg; /* must be null */
{
	struct payload_list *plist = NULL;
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
	if (iph1->status != IKEV1_STATE_IDENT_I_START) {
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

	/* create SA payload for my proposal */
	iph1->sa = ipsecdoi_setph1proposal(iph1);
	if (iph1->sa == NULL) {
		plog(ASL_LEVEL_ERR, 
			 "failed to set proposal");
		goto end;
	}

	/* set SA payload to propose */
	plist = isakmp_plist_append(plist, iph1->sa, ISAKMP_NPTYPE_SA);

#ifdef ENABLE_NATT
	/* set VID payload for NAT-T if NAT-T support allowed in the config file */
	if (iph1->rmconf->nat_traversal) 
		plist = isakmp_plist_append_natt_vids(plist, vid_natt);
#endif
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
		else
			plist = isakmp_plist_append(plist,
			    vid_xauth, ISAKMP_NPTYPE_VID);
			
		if ((vid_unity = set_vendorid(VENDORID_UNITY)) == NULL)
			plog(ASL_LEVEL_ERR, 
			     "Unity vendor ID generation failed\n");
		else
                	plist = isakmp_plist_append(plist,
			    vid_unity, ISAKMP_NPTYPE_VID);
		break;
	default:
		break;
	}
#endif
#ifdef ENABLE_FRAG
	if (iph1->rmconf->ike_frag) {
		if ((vid_frag = set_vendorid(VENDORID_FRAG)) == NULL) {
			plog(ASL_LEVEL_ERR, 
			    "Frag vendorID construction failed\n");
		} else {
			vid_frag = isakmp_frag_addcap(vid_frag,
			    VENDORID_FRAG_IDENT);
			plist = isakmp_plist_append(plist, 
			    vid_frag, ISAKMP_NPTYPE_VID);
		}
	}
#endif
#ifdef ENABLE_DPD
	if(iph1->rmconf->dpd){
		vid_dpd = set_vendorid(VENDORID_DPD);
		if (vid_dpd != NULL)
			plist = isakmp_plist_append(plist, vid_dpd,
			    ISAKMP_NPTYPE_VID);
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

	fsm_set_state(&iph1->status, IKEV1_STATE_IDENT_I_MSG1SENT);

	error = 0;

	IPSECSESSIONTRACEREVENT(iph1->parent_session,
							IPSECSESSIONEVENTCODE_IKE_PACKET_TX_SUCC,
							CONSTSTR("Initiator, Main-Mode message 1"),
							CONSTSTR(NULL));
	
end:
	if (error) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKE_PACKET_TX_FAIL,
								CONSTSTR("Initiator, Main-Mode Message 1"),
								CONSTSTR("Failed to transmit Main-Mode Message 1"));
	}
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
 * 	psk: HDR, SA
 * 	sig: HDR, SA
 * 	rsa: HDR, SA
 * 	rev: HDR, SA
 */
int
ident_i2recv(iph1, msg)
	phase1_handle_t *iph1;
	vchar_t *msg;
{
	vchar_t *pbuf = NULL;
	struct isakmp_parse_t *pa;
	vchar_t *satmp = NULL;
	int error = -1;
	int vid_numeric;

    /* validity check */
	if (iph1->status != IKEV1_STATE_IDENT_I_MSG1SENT) {
		plog(ASL_LEVEL_ERR,
             "status mismatched %d.\n", iph1->status);
		goto end;
	}

	/* validate the type of next payload */
	/*
	 * NOTE: RedCreek(as responder) attaches N[responder-lifetime] here,
	 *	if proposal-lifetime > lifetime-redcreek-wants.
	 *	(see doi-08 4.5.4)
	 *	=> According to the seciton 4.6.3 in RFC 2407, This is illegal.
	 * NOTE: we do not really care about ordering of VID and N.
	 *	does it matters?
	 * NOTE: even if there's multiple VID/N, we'll ignore them.
	 */
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
		case ISAKMP_NPTYPE_VID:
			vid_numeric = check_vendorid(pa->ptr);
#ifdef ENABLE_NATT
			if (iph1->rmconf->nat_traversal && natt_vendorid(vid_numeric))
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
			if (vid_numeric == VENDORID_DPD && iph1->rmconf->dpd)
				iph1->dpd_support=1;
#endif
#ifdef ENABLE_FRAG
			if ((vid_numeric == VENDORID_FRAG) &&
				(vendorid_frag_cap(pa->ptr) & VENDORID_FRAG_IDENT)) {
				plog(ASL_LEVEL_DEBUG, 
					 "remote supports FRAGMENTATION\n");
				iph1->frag = 1;
			}
#endif
			break;
		default:
			/* don't send information, see ident_r1recv() */
			plog(ASL_LEVEL_ERR,
				"ignore the packet, "
				"received unexpecting payload type %d.\n",
				pa->type);
			goto end;
		}
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
	if (ipsecdoi_checkph1proposal(satmp, iph1) < 0) {
		plog(ASL_LEVEL_ERR,
			"failed to get valid proposal.\n");
		/* XXX send information */
		goto end;
	}
	VPTRINIT(iph1->sa_ret);

	fsm_set_state(&iph1->status, IKEV1_STATE_IDENT_I_MSG2RCVD);

#ifdef ENABLE_VPNCONTROL_PORT
	vpncontrol_notify_phase_change(1, FROM_REMOTE, iph1, NULL);
#endif

	error = 0;

	IPSECSESSIONTRACEREVENT(iph1->parent_session,
							IPSECSESSIONEVENTCODE_IKE_PACKET_RX_SUCC,
							CONSTSTR("Initiator, Main-Mode message 2"),
							CONSTSTR(NULL));
	
end:
	if (error) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKE_PACKET_RX_FAIL,
								CONSTSTR("Initiator, Main-Mode Message 2"),
								CONSTSTR("Failed to process Main-Mode Message 2"));
	}
	if (pbuf)
		vfree(pbuf);
	if (satmp)
		vfree(satmp);
	return error;
}

/*
 * send to responder
 * 	psk: HDR, KE, Ni
 * 	sig: HDR, KE, Ni
 *   gssapi: HDR, KE, Ni, GSSi
 * 	rsa: HDR, KE, [ HASH(1), ] <IDi1_b>PubKey_r, <Ni_b>PubKey_r
 * 	rev: HDR, [ HASH(1), ] <Ni_b>Pubkey_r, <KE_b>Ke_i,
 * 	          <IDi1_b>Ke_i, [<<Cert-I_b>Ke_i]
 */
int
ident_i3send(iph1, msg)
	phase1_handle_t *iph1;
	vchar_t *msg;
{
	int error = -1;

    /* validity check */
	if (iph1->status != IKEV1_STATE_IDENT_I_MSG2RCVD) {
		plog(ASL_LEVEL_ERR,
             "status mismatched %d.\n", iph1->status);
		goto end;
	}

	/* fix isakmp index */
	memcpy(&iph1->index.r_ck, &((struct isakmp *)msg->v)->r_ck,
		sizeof(cookie_t));

	/* generate DH public value */
#ifdef HAVE_OPENSSL
	if (oakley_dh_generate(iph1->approval->dhgrp,
						   &iph1->dhpub, &iph1->dhpriv) < 0) {
#else
	if (oakley_dh_generate(iph1->approval->dhgrp,
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

	/* create buffer to send isakmp payload */
	iph1->sendbuf = ident_ir2mx(iph1);
	if (iph1->sendbuf == NULL) {
		plog(ASL_LEVEL_ERR, 
			 "failed to create send buffer");
		goto end;
	}

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

	/* the sending message is added to the received-list. */
	if (ike_session_add_recvdpkt(iph1->remote, iph1->local, iph1->sendbuf, msg,
                     PH1_NON_ESP_EXTRA_LEN(iph1, iph1->sendbuf), PH1_FRAG_FLAGS(iph1)) == -1) {
		plog(ASL_LEVEL_ERR , 
			"failed to add a response packet to the tree.\n");
		goto end;
	}

	fsm_set_state(&iph1->status, IKEV1_STATE_IDENT_I_MSG3SENT);

	error = 0;

	IPSECSESSIONTRACEREVENT(iph1->parent_session,
							IPSECSESSIONEVENTCODE_IKE_PACKET_TX_SUCC,
							CONSTSTR("Initiator, Main-Mode message 3"),
							CONSTSTR(NULL));
	
end:
	if (error) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKE_PACKET_TX_FAIL,
								CONSTSTR("Initiator, Main-Mode Message 3"),
								CONSTSTR("Failed to transmit Main-Mode Message 3"));
	}
	return error;
}

/*
 * receive from responder
 * 	psk: HDR, KE, Nr
 * 	sig: HDR, KE, Nr [, CR ]
 *   gssapi: HDR, KE, Nr, GSSr
 * 	rsa: HDR, KE, <IDr1_b>PubKey_i, <Nr_b>PubKey_i
 * 	rev: HDR, <Nr_b>PubKey_i, <KE_b>Ke_r, <IDr1_b>Ke_r,
 */
int
ident_i4recv(iph1, msg)
	phase1_handle_t *iph1;
	vchar_t *msg;
{
	vchar_t *pbuf = NULL;
	struct isakmp_parse_t *pa;
	int error = -1;
	int vid_numeric;
#ifdef ENABLE_NATT
	vchar_t	*natd_received;
	int natd_seq = 0, natd_verified;
#endif

    /* validity check */
	if (iph1->status != IKEV1_STATE_IDENT_I_MSG3SENT) {
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

	for (pa = ALIGNED_CAST(struct isakmp_parse_t *)pbuf->v;
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
		case ISAKMP_NPTYPE_VID:
			vid_numeric = check_vendorid(pa->ptr);
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
			if (vid_numeric == VENDORID_DPD && iph1->rmconf->dpd)
				iph1->dpd_support=1;
#endif
				
			break;
		case ISAKMP_NPTYPE_CR:
			if (oakley_savecr(iph1, pa->ptr) < 0) {
				plog(ASL_LEVEL_ERR, 
					 "failed to process CR payload");
				goto end;
			}
			break;

#ifdef ENABLE_NATT
		case ISAKMP_NPTYPE_NATD_DRAFT:
		case ISAKMP_NPTYPE_NATD_RFC:
		case ISAKMP_NPTYPE_NATD_BADDRAFT:
			if (NATT_AVAILABLE(iph1) && iph1->natt_options != NULL &&
			    pa->type == iph1->natt_options->payload_nat_d) {
				natd_received = NULL;
				if (isakmp_p2ph (&natd_received, pa->ptr) < 0) {
					plog(ASL_LEVEL_ERR, 
						 "failed to process NATD payload");
					goto end;
				}
                        
				/* set both bits first so that we can clear them
				   upon verifying hashes */
				if (natd_seq == 0)
					iph1->natt_flags |= NAT_DETECTED;
                        
				/* this function will clear appropriate bits bits 
				   from iph1->natt_flags */
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
			/* don't send information, see ident_r1recv() */
			plog(ASL_LEVEL_ERR,
				"ignore the packet, "
				"received unexpecting payload type %d.\n",
				pa->type);
			goto end;
		}
	}

#ifdef ENABLE_NATT
	if (NATT_AVAILABLE(iph1)) {
		plog (ASL_LEVEL_INFO, "NAT %s %s%s\n",
		      iph1->natt_flags & NAT_DETECTED ? 
		      		"detected:" : "not detected",
		      iph1->natt_flags & NAT_DETECTED_ME ? "ME " : "",
		      iph1->natt_flags & NAT_DETECTED_PEER ? "PEER" : "");
		if (iph1->natt_flags & NAT_DETECTED)
			natt_float_ports (iph1);
	}
#endif

	/* payload existency check */
	if (iph1->dhpub_p == NULL || iph1->nonce_p == NULL) {
		plog(ASL_LEVEL_ERR,
			"few isakmp message received.\n");
		goto end;
	}

	if (oakley_checkcr(iph1) < 0) {
		/* Ignore this error in order to be interoperability. */
		;
	}

	fsm_set_state(&iph1->status, IKEV1_STATE_IDENT_I_MSG4RCVD);

	error = 0;

	IPSECSESSIONTRACEREVENT(iph1->parent_session,
							IPSECSESSIONEVENTCODE_IKE_PACKET_RX_SUCC,
							CONSTSTR("Initiator, Main-Mode message 4"),
							CONSTSTR(NULL));
	
end:
	if (error) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKE_PACKET_RX_FAIL,
								CONSTSTR("Initiator, Main-Mode Message 4"),
								CONSTSTR("Failed to process Main-Mode Message 4"));
	}
	if (pbuf)
		vfree(pbuf);
	if (error) {
		VPTRINIT(iph1->dhpub_p);
		VPTRINIT(iph1->nonce_p);
		VPTRINIT(iph1->id_p);
		oakley_delcert(iph1->cr_p);
		iph1->cr_p = NULL;
	}

	return error;
}

/*
 * send to responder
 * 	psk: HDR*, IDi1, HASH_I
 * 	sig: HDR*, IDi1, [ CR, ] [ CERT, ] SIG_I
 *   gssapi: HDR*, IDi1, < Gssi(n) | HASH_I >
 * 	rsa: HDR*, HASH_I
 * 	rev: HDR*, HASH_I
 */
int
ident_i5send(iph1, msg0)
	phase1_handle_t *iph1;
	vchar_t *msg0;
{
	int error = -1;
	int dohash = 1;

    /* validity check */
	if (iph1->status != IKEV1_STATE_IDENT_I_MSG4RCVD) {
		plog(ASL_LEVEL_ERR,
             "status mismatched %d.\n", iph1->status);
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

	/* make ID payload into isakmp status */
	if (ipsecdoi_setid1(iph1) < 0) {
		plog(ASL_LEVEL_ERR, 
			 "failed to set ID");
		goto end;
	}

	/* generate HASH to send */
	if (dohash) {
		iph1->hash = oakley_ph1hash_common(iph1, GENERATE);
		if (iph1->hash == NULL) {
			plog(ASL_LEVEL_ERR, 
				 "failed to generate HASH");
			goto end;
		}
	} else
		iph1->hash = NULL;

	/* set encryption flag */
	iph1->flags |= ISAKMP_FLAG_E;

	/* create HDR;ID;HASH payload */
	iph1->sendbuf = ident_ir3mx(iph1);
	if (iph1->sendbuf == NULL) {
		plog(ASL_LEVEL_ERR, 
			 "failed to allocate send buffer");
		goto end;
	}

	/* send the packet, add to the schedule to resend */
	iph1->retry_counter = iph1->rmconf->retry_counter;
	if (isakmp_ph1resend(iph1) == -1) {
		plog(ASL_LEVEL_ERR, 
			 "failed to send packet");
		goto end;
	}

	/* the sending message is added to the received-list. */
	if (ike_session_add_recvdpkt(iph1->remote, iph1->local, iph1->sendbuf, msg0,
                     PH1_NON_ESP_EXTRA_LEN(iph1, iph1->sendbuf), PH1_FRAG_FLAGS(iph1)) == -1) {
		plog(ASL_LEVEL_ERR , 
			"failed to add a response packet to the tree.\n");
		goto end;
	}

	/* see handler.h about IV synchronization. */
	memcpy(iph1->ivm->ive->v, iph1->ivm->iv->v, iph1->ivm->iv->l);

	fsm_set_state(&iph1->status, IKEV1_STATE_IDENT_I_MSG5SENT);

	error = 0;

	IPSECSESSIONTRACEREVENT(iph1->parent_session,
							IPSECSESSIONEVENTCODE_IKE_PACKET_TX_SUCC,
							CONSTSTR("Initiator, Main-Mode message 5"),
							CONSTSTR(NULL));
	
end:
	if (error) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKE_PACKET_TX_FAIL,
								CONSTSTR("Initiator, Main-Mode Message 5"),
								CONSTSTR("Failed to transmit Main-Mode Message 5"));
	}
	return error;
}

/*
 * receive from responder
 * 	psk: HDR*, IDr1, HASH_R
 * 	sig: HDR*, IDr1, [ CERT, ] SIG_R
 *   gssapi: HDR*, IDr1, < GSSr(n) | HASH_R >
 * 	rsa: HDR*, HASH_R
 * 	rev: HDR*, HASH_R
 */
int
ident_i6recv(iph1, msg0)
	phase1_handle_t *iph1;
	vchar_t *msg0;
{
	vchar_t *pbuf = NULL;
	struct isakmp_parse_t *pa;
	vchar_t *msg = NULL;
	int error = -1;
	int type;
	int vid_numeric;
	int received_cert = 0;

    /* validity check */
	if (iph1->status != IKEV1_STATE_IDENT_I_MSG5SENT) {
		plog(ASL_LEVEL_ERR,
             "status mismatched %d.\n", iph1->status);
		goto end;
	}

	/* decrypting */
	if (!ISSET(((struct isakmp *)msg0->v)->flags, ISAKMP_FLAG_E)) {
		plog(ASL_LEVEL_ERR,
			"ignore the packet, "
			"expecting the packet encrypted.\n");
		goto end;
	}
	msg = oakley_do_decrypt(iph1, msg0, iph1->ivm->iv, iph1->ivm->ive);
	if (msg == NULL) {
		plog(ASL_LEVEL_ERR,
			 "failed to decrypt");
		goto end;
	}

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
#ifdef ENABLE_DPD
				if (vid_numeric == VENDORID_DPD && iph1->rmconf->dpd)
					iph1->dpd_support=1;
#endif
				break;
		case ISAKMP_NPTYPE_N:
			isakmp_check_notify(pa->ptr, iph1);
			break;
		default:
			/* don't send information, see ident_r1recv() */
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

	/* verify identifier */
	if (ipsecdoi_checkid1(iph1) != 0) {
		plog(ASL_LEVEL_ERR,
			"invalid ID payload.\n");
		goto end;
	}

	/* validate authentication value */
    type = oakley_validate_auth(iph1);
    if (type != 0) {
        IPSECSESSIONTRACEREVENT(iph1->parent_session,
                                IPSECSESSIONEVENTCODE_IKEV1_PH1_AUTH_FAIL,
                                CONSTSTR("Initiator, Main-Mode Message 6"),
                                CONSTSTR("Failed to authenticate Main-Mode Message 6"));
        if (type == -1) {
            /* msg printed inner oakley_validate_auth() */
            goto end;
        }
        isakmp_info_send_n1(iph1, type, NULL);
        goto end;
    }
    IPSECSESSIONTRACEREVENT(iph1->parent_session,
                            IPSECSESSIONEVENTCODE_IKEV1_PH1_AUTH_SUCC,
                            CONSTSTR("Initiator, Main-Mode Message 6"),
                            CONSTSTR(NULL));


	/*
	 * XXX: Should we do compare two addresses, ph1handle's and ID
	 * payload's.
	 */

	plogdump(ASL_LEVEL_DEBUG, iph1->id_p->v, iph1->id_p->l, "peer's ID:");

	/* see handler.h about IV synchronization. */
	memcpy(iph1->ivm->iv->v, iph1->ivm->ive->v, iph1->ivm->ive->l);

	/*
	 * If we got a GSS token, we need to this roundtrip again.
	 */
	fsm_set_state(&iph1->status, IKEV1_STATE_IDENT_I_MSG6RCVD);

	error = 0;

	IPSECSESSIONTRACEREVENT(iph1->parent_session,
							IPSECSESSIONEVENTCODE_IKE_PACKET_RX_SUCC,
							CONSTSTR("Initiator, Main-Mode message 6"),
							CONSTSTR(NULL));
	
end:
	if (error) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKE_PACKET_RX_FAIL,
								CONSTSTR("Initiator, Main-Mode Message 6"),
								CONSTSTR("Failed to transmit Main-Mode Message 6"));
	}
	if (pbuf)
		vfree(pbuf);
	if (msg)
		vfree(msg);

	if (error) {
		VPTRINIT(iph1->id_p);
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
ident_ifinalize(iph1, msg)
	phase1_handle_t *iph1;
	vchar_t *msg;
{
	int error = -1;

    /* validity check */
	if (iph1->status != IKEV1_STATE_IDENT_I_MSG6RCVD) {
		plog(ASL_LEVEL_ERR,
             "status mismatched %d.\n", iph1->status);
		goto end;
	}

	/* see handler.h about IV synchronization. */
	memcpy(iph1->ivm->iv->v, iph1->ivm->ive->v, iph1->ivm->iv->l);

	fsm_set_state(&iph1->status, IKEV1_STATE_PHASE1_ESTABLISHED);

	IPSECSESSIONTRACEREVENT(iph1->parent_session,
							IPSECSESSIONEVENTCODE_IKEV1_PH1_INIT_SUCC,
							CONSTSTR("Initiator, Main-Mode"),
							CONSTSTR(NULL));
	
	error = 0;

end:
	return error;
}

/*
 * receive from initiator
 * 	psk: HDR, SA
 * 	sig: HDR, SA
 * 	rsa: HDR, SA
 * 	rev: HDR, SA
 */
int
ident_r1recv(iph1, msg)
	phase1_handle_t *iph1;
	vchar_t *msg;
{
	vchar_t *pbuf = NULL;
	struct isakmp_parse_t *pa;
	int error = -1;
	int vid_numeric;

	/* validity check */
	if (iph1->status != IKEV1_STATE_IDENT_R_START) {
		plog(ASL_LEVEL_ERR,
			"status mismatched %d.\n", iph1->status);
		goto end;
	}

	/* validate the type of next payload */
	/*
	 * NOTE: XXX even if multiple VID, we'll silently ignore those.
	 */
	pbuf = isakmp_parse(msg);
	if (pbuf == NULL) {
		plog(ASL_LEVEL_ERR, 
			 "failed to parse msg");
		goto end;
	}
	pa = ALIGNED_CAST(struct isakmp_parse_t *)pbuf->v;

	/* check the position of SA payload */
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

		switch (pa->type) {
		case ISAKMP_NPTYPE_VID:
			vid_numeric = check_vendorid(pa->ptr);
#ifdef ENABLE_NATT
			if (iph1->rmconf->nat_traversal && natt_vendorid(vid_numeric))
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
			if (vid_numeric == VENDORID_DPD && iph1->rmconf->dpd)
				iph1->dpd_support=1;
#endif
#ifdef ENABLE_FRAG
			if ((vid_numeric == VENDORID_FRAG) &&
				(vendorid_frag_cap(pa->ptr) & VENDORID_FRAG_IDENT)) {
				plog(ASL_LEVEL_DEBUG, 
					 "remote supports FRAGMENTATION\n");
				iph1->frag = 1;
			}
#endif
			break;
		default:
			/*
			 * We don't send information to the peer even
			 * if we received malformed packet.  Because we
			 * can't distinguish the malformed packet and
			 * the re-sent packet.  And we do same behavior
			 * when we expect encrypted packet.
			 */
			plog(ASL_LEVEL_ERR,
				"ignore the packet, "
				"received unexpecting payload type %d.\n",
				pa->type);
			goto end;
		}
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

	fsm_set_state(&iph1->status, IKEV1_STATE_IDENT_R_MSG1RCVD);

	error = 0;

	IPSECSESSIONTRACEREVENT(iph1->parent_session,
							IPSECSESSIONEVENTCODE_IKE_PACKET_RX_SUCC,
							CONSTSTR("Responder, Main-Mode message 1"),
							CONSTSTR(NULL));
	
end:
	if (error) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKE_PACKET_RX_FAIL,
								CONSTSTR("Responder, Main-Mode Message 1"),
								CONSTSTR("Failed to process Main-Mode Message 1"));
	}
	if (pbuf)
		vfree(pbuf);
	if (error) {
		VPTRINIT(iph1->sa);
	}

	return error;
}

/*
 * send to initiator
 * 	psk: HDR, SA
 * 	sig: HDR, SA
 * 	rsa: HDR, SA
 * 	rev: HDR, SA
 */
int
ident_r2send(iph1, msg)
	phase1_handle_t *iph1;
	vchar_t *msg;
{
	struct payload_list *plist = NULL;
	int error = -1;
	vchar_t *gss_sa = NULL;
#ifdef ENABLE_NATT
	vchar_t *vid_natt = NULL;
#endif
#ifdef ENABLE_HYBRID
        vchar_t *vid_xauth = NULL;
        vchar_t *vid_unity = NULL;
#endif  
#ifdef ENABLE_DPD
	vchar_t *vid_dpd = NULL;
#endif
#ifdef ENABLE_FRAG          
	vchar_t *vid_frag = NULL;
#endif 

	/* validity check */
	if (iph1->status != IKEV1_STATE_IDENT_R_MSG1RCVD) {
		plog(ASL_LEVEL_ERR,
			"status mismatched %d.\n", iph1->status);
		goto end;
	}

	/* set responder's cookie */
	isakmp_newcookie((caddr_t)&iph1->index.r_ck, iph1->remote, iph1->local);
    gss_sa = iph1->sa_ret;

	/* set SA payload to reply */
	plist = isakmp_plist_append(plist, gss_sa, ISAKMP_NPTYPE_SA);

#ifdef ENABLE_HYBRID
	if (iph1->mode_cfg->flags & ISAKMP_CFG_VENDORID_XAUTH) {
		plog (ASL_LEVEL_INFO, "Adding xauth VID payload.\n");
		if ((vid_xauth = set_vendorid(VENDORID_XAUTH)) == NULL) {
			plog(ASL_LEVEL_ERR, 
			    "Cannot create Xauth vendor ID\n");
			goto end;
		}
		plist = isakmp_plist_append(plist,
		    vid_xauth, ISAKMP_NPTYPE_VID);
	}

	if (iph1->mode_cfg->flags & ISAKMP_CFG_VENDORID_UNITY) {
		if ((vid_unity = set_vendorid(VENDORID_UNITY)) == NULL) {
			plog(ASL_LEVEL_ERR, 
			    "Cannot create Unity vendor ID\n");
			goto end;
		}
		plist = isakmp_plist_append(plist,
		    vid_unity, ISAKMP_NPTYPE_VID);
	}
#endif
#ifdef ENABLE_NATT
	/* Has the peer announced NAT-T? */
	if (NATT_AVAILABLE(iph1))
		vid_natt = set_vendorid(iph1->natt_options->version);

	if (vid_natt)
		plist = isakmp_plist_append(plist, vid_natt, ISAKMP_NPTYPE_VID);
#endif
#ifdef ENABLE_DPD
	/* XXX only send DPD VID if remote sent it ? */
	if(iph1->rmconf->dpd){
		vid_dpd = set_vendorid(VENDORID_DPD);
		if (vid_dpd != NULL)
			plist = isakmp_plist_append(plist, vid_dpd, ISAKMP_NPTYPE_VID);
	}
#endif
#ifdef ENABLE_FRAG
	if (iph1->frag) {
		vid_frag = set_vendorid(VENDORID_FRAG);
		if (vid_frag != NULL)
			vid_frag = isakmp_frag_addcap(vid_frag,
			    VENDORID_FRAG_IDENT);
		if (vid_frag == NULL)
			plog(ASL_LEVEL_ERR, 
			    "Frag vendorID construction failed\n");
		else
			plist = isakmp_plist_append(plist, 
			     vid_frag, ISAKMP_NPTYPE_VID);
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

	/* the sending message is added to the received-list. */
	if (ike_session_add_recvdpkt(iph1->remote, iph1->local, iph1->sendbuf, msg,
                     PH1_NON_ESP_EXTRA_LEN(iph1, iph1->sendbuf), PH1_FRAG_FLAGS(iph1)) == -1) {
		plog(ASL_LEVEL_ERR , 
			"failed to add a response packet to the tree.\n");
		goto end;
	}

	fsm_set_state(&iph1->status, IKEV1_STATE_IDENT_R_MSG2SENT);

#ifdef ENABLE_VPNCONTROL_PORT
	vpncontrol_notify_phase_change(1, FROM_LOCAL, iph1, NULL);
#endif

	error = 0;

	IPSECSESSIONTRACEREVENT(iph1->parent_session,
							IPSECSESSIONEVENTCODE_IKE_PACKET_TX_SUCC,
							CONSTSTR("Responder, Main-Mode message 2"),
							CONSTSTR(NULL));
	
end:
	if (error) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKE_PACKET_TX_FAIL,
								CONSTSTR("Responder, Main-Mode Message 2"),
								CONSTSTR("Failed to transmit Main-Mode Message 2"));
	}
#ifdef ENABLE_NATT
	if (vid_natt)
		vfree(vid_natt);
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
#ifdef ENABLE_FRAG
	if (vid_frag != NULL)
		vfree(vid_frag);
#endif

	return error;
}

/*
 * receive from initiator
 * 	psk: HDR, KE, Ni
 * 	sig: HDR, KE, Ni
 *   gssapi: HDR, KE, Ni, GSSi
 * 	rsa: HDR, KE, [ HASH(1), ] <IDi1_b>PubKey_r, <Ni_b>PubKey_r
 * 	rev: HDR, [ HASH(1), ] <Ni_b>Pubkey_r, <KE_b>Ke_i,
 * 	          <IDi1_b>Ke_i, [<<Cert-I_b>Ke_i]
 */
int
ident_r3recv(iph1, msg)
	phase1_handle_t *iph1;
	vchar_t *msg;
{
	vchar_t *pbuf = NULL;
	struct isakmp_parse_t *pa;
	int error = -1;
#ifdef ENABLE_NATT
	int natd_seq = 0;
#endif

	/* validity check */
	if (iph1->status != IKEV1_STATE_IDENT_R_MSG2SENT) {
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

	for (pa = ALIGNED_CAST(struct isakmp_parse_t *)pbuf->v;
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
		case ISAKMP_NPTYPE_VID:
			(void)check_vendorid(pa->ptr);
			break;
		case ISAKMP_NPTYPE_CR:
			plog(ASL_LEVEL_WARNING,
				"CR received, ignore it. "
				"It should be in other exchange.\n");
			break;

#ifdef ENABLE_NATT
		case ISAKMP_NPTYPE_NATD_DRAFT:
		case ISAKMP_NPTYPE_NATD_RFC:
		case ISAKMP_NPTYPE_NATD_BADDRAFT:
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
			/* don't send information, see ident_r1recv() */
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

	/* payload existency check */
	if (iph1->dhpub_p == NULL || iph1->nonce_p == NULL) {
		plog(ASL_LEVEL_ERR,
			"few isakmp message received.\n");
		goto end;
	}

	fsm_set_state(&iph1->status, IKEV1_STATE_IDENT_R_MSG3RCVD);

	error = 0;

	IPSECSESSIONTRACEREVENT(iph1->parent_session,
							IPSECSESSIONEVENTCODE_IKE_PACKET_RX_SUCC,
							CONSTSTR("Responder, Main-Mode message 3"),
							CONSTSTR(NULL));
	
end:
	if (error) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKE_PACKET_RX_FAIL,
								CONSTSTR("Responder, Main-Mode Message 3"),
								CONSTSTR("Failed to process Main-Mode Message 3"));
	}
	if (pbuf)
		vfree(pbuf);

	if (error) {
		VPTRINIT(iph1->dhpub_p);
		VPTRINIT(iph1->nonce_p);
		VPTRINIT(iph1->id_p);
	}

	return error;
}

/*
 * send to initiator
 * 	psk: HDR, KE, Nr
 * 	sig: HDR, KE, Nr [, CR ]
 *   gssapi: HDR, KE, Nr, GSSr
 * 	rsa: HDR, KE, <IDr1_b>PubKey_i, <Nr_b>PubKey_i
 * 	rev: HDR, <Nr_b>PubKey_i, <KE_b>Ke_r, <IDr1_b>Ke_r,
 */
int
ident_r4send(iph1, msg)
	phase1_handle_t *iph1;
	vchar_t *msg;
{
	int error = -1;

	/* validity check */
	if (iph1->status != IKEV1_STATE_IDENT_R_MSG3RCVD) {
		plog(ASL_LEVEL_ERR,
			"status mismatched %d.\n", iph1->status);
		goto end;
	}

	/* generate DH public value */
#ifdef HAVE_OPENSSL
	if (oakley_dh_generate(iph1->approval->dhgrp,
						   &iph1->dhpub, &iph1->dhpriv) < 0) {
#else
		if (oakley_dh_generate(iph1->approval->dhgrp,
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

	/* create HDR;KE;NONCE payload */
	iph1->sendbuf = ident_ir2mx(iph1);
	if (iph1->sendbuf == NULL) {
		plog(ASL_LEVEL_ERR, 
			 "failed to allocate send buffer");
		goto end;
	}

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

	/* the sending message is added to the received-list. */
	if (ike_session_add_recvdpkt(iph1->remote, iph1->local, iph1->sendbuf, msg,
                     PH1_NON_ESP_EXTRA_LEN(iph1, iph1->sendbuf), PH1_FRAG_FLAGS(iph1)) == -1) {
		plog(ASL_LEVEL_ERR , 
			"failed to add a response packet to the tree.\n");
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

	fsm_set_state(&iph1->status, IKEV1_STATE_IDENT_R_MSG4SENT);

	error = 0;

	IPSECSESSIONTRACEREVENT(iph1->parent_session,
							IPSECSESSIONEVENTCODE_IKE_PACKET_TX_SUCC,
							CONSTSTR("Responder, Main-Mode message 4"),
							CONSTSTR(NULL));
	
end:
	if (error) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKE_PACKET_TX_FAIL,
								CONSTSTR("Responder, Main-Mode Message 4"),
								CONSTSTR("Failed to transmit Main-Mode Message 4"));
	}
	return error;
}

/*
 * receive from initiator
 * 	psk: HDR*, IDi1, HASH_I
 * 	sig: HDR*, IDi1, [ CR, ] [ CERT, ] SIG_I
 *   gssapi: HDR*, [ IDi1, ] < GSSi(n) | HASH_I >
 * 	rsa: HDR*, HASH_I
 * 	rev: HDR*, HASH_I
 */
int
ident_r5recv(iph1, msg0)
	phase1_handle_t *iph1;
	vchar_t *msg0;
{
	vchar_t *msg = NULL;
	vchar_t *pbuf = NULL;
	struct isakmp_parse_t *pa;
	int error = -1;
	int type;
	int received_cert = 0;

	/* validity check */
	if (iph1->status != IKEV1_STATE_IDENT_R_MSG4SENT) {
		plog(ASL_LEVEL_ERR,
			"status mismatched %d.\n", iph1->status);
		goto end;
	}

	/* decrypting */
	if (!ISSET(((struct isakmp *)msg0->v)->flags, ISAKMP_FLAG_E)) {
		plog(ASL_LEVEL_ERR,
			"reject the packet, "
			"expecting the packet encrypted.\n");
		goto end;
	}
	msg = oakley_do_decrypt(iph1, msg0, iph1->ivm->iv, iph1->ivm->ive);
	if (msg == NULL) {
		plog(ASL_LEVEL_ERR,
			 "failed to decrypt");
		goto end;
	}

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
			(void)check_vendorid(pa->ptr);
			break;
		case ISAKMP_NPTYPE_N:
			isakmp_check_notify(pa->ptr, iph1);
			break;
		default:
			/* don't send information, see ident_r1recv() */
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
	/* XXX same as ident_i4recv(), should be merged. */
    {
	int ng = 0;

	switch (AUTHMETHOD(iph1)) {
	case OAKLEY_ATTR_AUTH_METHOD_PSKEY:
#ifdef ENABLE_HYBRID
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_PSKEY_R:
	case OAKLEY_ATTR_AUTH_METHOD_HYBRID_RSA_R:
#endif
		if (iph1->id_p == NULL || iph1->pl_hash == NULL)
			ng++;
		break;
	case OAKLEY_ATTR_AUTH_METHOD_RSASIG:
#ifdef ENABLE_HYBRID
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSASIG_R:
#endif
		if (iph1->id_p == NULL || iph1->sig_p == NULL)
			ng++;
		break;
	case OAKLEY_ATTR_AUTH_METHOD_RSAENC:
	case OAKLEY_ATTR_AUTH_METHOD_RSAREV:
#ifdef ENABLE_HYBRID
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAENC_R:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAREV_R:
#endif
		if (iph1->pl_hash == NULL)
			ng++;
		break;
	default:
		plog(ASL_LEVEL_ERR,
			"invalid authmethod %d why ?\n",
			iph1->approval->authmethod);
		goto end;
	}
	if (ng) {
		plog(ASL_LEVEL_ERR,
			"few isakmp message received.\n");
		goto end;
	}
    }

	/* verify identifier */
	if (ipsecdoi_checkid1(iph1) != 0) {
		plog(ASL_LEVEL_ERR,
			"invalid ID payload.\n");
		goto end;
	}

	/* validate authentication value */

    type = oakley_validate_auth(iph1);
    if (type != 0) {
        IPSECSESSIONTRACEREVENT(iph1->parent_session,
                                IPSECSESSIONEVENTCODE_IKEV1_PH1_AUTH_FAIL,
                                CONSTSTR("Responder, Main-Mode Message 5"),
                                CONSTSTR("Failed to authenticate Main-Mode Message 5"));
        if (type == -1) {
            /* msg printed inner oakley_validate_auth() */
            goto end;
        }
        isakmp_info_send_n1(iph1, type, NULL);
        goto end;
    }
    IPSECSESSIONTRACEREVENT(iph1->parent_session,
                            IPSECSESSIONEVENTCODE_IKEV1_PH1_AUTH_SUCC,
                            CONSTSTR("Responder, Main-Mode Message 5"),
                            CONSTSTR(NULL));

	if (oakley_checkcr(iph1) < 0) {
		/* Ignore this error in order to be interoperability. */
		;
	}

	/*
	 * XXX: Should we do compare two addresses, ph1handle's and ID
	 * payload's.
	 */

	plogdump(ASL_LEVEL_DEBUG, iph1->id_p->v, iph1->id_p->l, "peer's ID\n");

	/* see handler.h about IV synchronization. */
	memcpy(iph1->ivm->iv->v, iph1->ivm->ive->v, iph1->ivm->ive->l);

	fsm_set_state(&iph1->status, IKEV1_STATE_IDENT_R_MSG5RCVD);
	error = 0;

	IPSECSESSIONTRACEREVENT(iph1->parent_session,
							IPSECSESSIONEVENTCODE_IKE_PACKET_RX_SUCC,
							CONSTSTR("Responder, Main-Mode message 5"),
							CONSTSTR(NULL));
	
end:
	if (error) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKE_PACKET_RX_FAIL,
								CONSTSTR("Responder, Main-Mode Message 5"),
								CONSTSTR("Failed to process Main-Mode Message 5"));
	}
	if (pbuf)
		vfree(pbuf);
	if (msg)
		vfree(msg);

	if (error) {
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
 * send to initiator
 * 	psk: HDR*, IDr1, HASH_R
 * 	sig: HDR*, IDr1, [ CERT, ] SIG_R
 *   gssapi: HDR*, IDr1, < GSSr(n) | HASH_R >
 * 	rsa: HDR*, HASH_R
 * 	rev: HDR*, HASH_R
 */
int
ident_r6send(iph1, msg)
	phase1_handle_t *iph1;
	vchar_t *msg;
{
	int error = -1;
	int dohash = 1;

	/* validity check */
	if (iph1->status != IKEV1_STATE_IDENT_R_MSG5RCVD) {
		plog(ASL_LEVEL_ERR,
			"status mismatched %d.\n", iph1->status);
		goto end;
	}

	/* make ID payload into isakmp status */
	if (ipsecdoi_setid1(iph1) < 0) {
		plog(ASL_LEVEL_ERR,
			 "failed to set ID");
		goto end;
	}

	if (dohash) {
		/* generate HASH to send */
		plog(ASL_LEVEL_DEBUG, "generate HASH_R\n");
		iph1->hash = oakley_ph1hash_common(iph1, GENERATE);
		if (iph1->hash == NULL) {
			plog(ASL_LEVEL_ERR,
				 "failed to generate HASH");
			goto end;
		}
	} else
		iph1->hash = NULL;

	/* set encryption flag */
	iph1->flags |= ISAKMP_FLAG_E;

	/* create HDR;ID;HASH payload */
	iph1->sendbuf = ident_ir3mx(iph1);
	if (iph1->sendbuf == NULL) {
		plog(ASL_LEVEL_ERR, 
			 "failed to create send buffer");
		goto end;
	}

	/* send HDR;ID;HASH to responder */
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

	/* see handler.h about IV synchronization. */
	memcpy(iph1->ivm->ive->v, iph1->ivm->iv->v, iph1->ivm->iv->l);

	fsm_set_state(&iph1->status, IKEV1_STATE_PHASE1_ESTABLISHED);

	IPSECSESSIONTRACEREVENT(iph1->parent_session,
							IPSECSESSIONEVENTCODE_IKEV1_PH1_RESP_SUCC,
							CONSTSTR("Responder, Main-Mode"),
							CONSTSTR(NULL));
	
	error = 0;

	IPSECSESSIONTRACEREVENT(iph1->parent_session,
							IPSECSESSIONEVENTCODE_IKE_PACKET_TX_SUCC,
							CONSTSTR("Responder, Main-Mode message 6"),
							CONSTSTR(NULL));
	
end:
	if (error) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKE_PACKET_TX_FAIL,
								CONSTSTR("Responder, Main-Mode Message 6"),
								CONSTSTR("Failed to process Main-Mode Message 6"));
	}

	return error;
}

/*
 * This is used in main mode for:
 * initiator's 3rd exchange send to responder
 * 	psk: HDR, KE, Ni
 * 	sig: HDR, KE, Ni
 * 	rsa: HDR, KE, [ HASH(1), ] <IDi1_b>PubKey_r, <Ni_b>PubKey_r
 * 	rev: HDR, [ HASH(1), ] <Ni_b>Pubkey_r, <KE_b>Ke_i,
 * 	          <IDi1_b>Ke_i, [<<Cert-I_b>Ke_i]
 * responders 2nd exchnage send to initiator
 * 	psk: HDR, KE, Nr
 * 	sig: HDR, KE, Nr [, CR ]
 * 	rsa: HDR, KE, <IDr1_b>PubKey_i, <Nr_b>PubKey_i
 * 	rev: HDR, <Nr_b>PubKey_i, <KE_b>Ke_r, <IDr1_b>Ke_r,
 */
static vchar_t *
ident_ir2mx(iph1)
	phase1_handle_t *iph1;
{
	vchar_t *buf = 0;
	struct payload_list *plist = NULL;
	int need_cr = 0;
	vchar_t *cr = NULL;
	vchar_t *vid = NULL;
	int error = -1;
#ifdef ENABLE_NATT
	vchar_t *natd[2] = { NULL, NULL };
#endif

	/* create CR if need */
	if (iph1->side == RESPONDER
	 && iph1->rmconf->send_cr
	 && oakley_needcr(iph1->approval->authmethod)) {
		need_cr = 1;
		cr = oakley_getcr(iph1);
		if (cr == NULL) {
			plog(ASL_LEVEL_ERR, 
				"failed to get cr buffer.\n");
			goto end;
		}
	}

	/* create isakmp KE payload */
	plist = isakmp_plist_append(plist, iph1->dhpub, ISAKMP_NPTYPE_KE);

	/* create isakmp NONCE payload */
	plist = isakmp_plist_append(plist, iph1->nonce, ISAKMP_NPTYPE_NONCE);

	/* append vendor id, if needed */
	if (vid)
		plist = isakmp_plist_append(plist, vid, ISAKMP_NPTYPE_VID);

	/* create isakmp CR payload if needed */
	if (need_cr)
		plist = isakmp_plist_append(plist, cr, ISAKMP_NPTYPE_CR);

#ifdef ENABLE_NATT
	/* generate and append NAT-D payloads */
	if (NATT_AVAILABLE(iph1))
	{
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

		plog (ASL_LEVEL_INFO, "Adding remote and local NAT-D payloads.\n");
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
	
	buf = isakmp_plist_set_all (&plist, iph1);
	
	error = 0;

end:
	if (error && buf != NULL) {
		vfree(buf);
		buf = NULL;
	}
	if (cr)
		vfree(cr);
	if (vid)
		vfree(vid);

#ifdef ENABLE_NATT
	if (natd[0])
		vfree(natd[0]);
	if (natd[1])
		vfree(natd[1]);
#endif

	return buf;
}

/*
 * This is used in main mode for:
 * initiator's 4th exchange send to responder
 * 	psk: HDR*, IDi1, HASH_I
 * 	sig: HDR*, IDi1, [ CR, ] [ CERT, ] SIG_I
 *   gssapi: HDR*, [ IDi1, ] < GSSi(n) | HASH_I >
 * 	rsa: HDR*, HASH_I
 * 	rev: HDR*, HASH_I
 * responders 3rd exchnage send to initiator
 * 	psk: HDR*, IDr1, HASH_R
 * 	sig: HDR*, IDr1, [ CERT, ] SIG_R
 *   gssapi: HDR*, [ IDr1, ] < GSSr(n) | HASH_R >
 * 	rsa: HDR*, HASH_R
 * 	rev: HDR*, HASH_R
 */
static vchar_t *
ident_ir3mx(iph1)
	phase1_handle_t *iph1;
{
	struct payload_list *plist = NULL;
	vchar_t *buf = NULL, *new = NULL;
	int need_cr = 0;
	int need_cert = 0;
	vchar_t *cr = NULL;
	int error = -1;
	vchar_t *notp_ini = NULL;

	switch (AUTHMETHOD(iph1)) {
	case OAKLEY_ATTR_AUTH_METHOD_PSKEY:
#ifdef ENABLE_HYBRID
	case FICTIVE_AUTH_METHOD_XAUTH_PSKEY_I:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_PSKEY_R:
	case OAKLEY_ATTR_AUTH_METHOD_HYBRID_RSA_I:
#endif
		/* create isakmp ID payload */
		plist = isakmp_plist_append(plist, iph1->id, ISAKMP_NPTYPE_ID);

		/* create isakmp HASH payload */
		plist = isakmp_plist_append(plist, iph1->hash, ISAKMP_NPTYPE_HASH);
		break;
	case OAKLEY_ATTR_AUTH_METHOD_RSASIG:
#ifdef ENABLE_HYBRID
	case OAKLEY_ATTR_AUTH_METHOD_HYBRID_RSA_R:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSASIG_I:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSASIG_R:
#endif 
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

		/* create CR if need */
		if (iph1->side == INITIATOR
		 && iph1->rmconf->send_cr
	 	 && oakley_needcr(iph1->approval->authmethod)) {
			need_cr = 1;
			cr = oakley_getcr(iph1);
			if (cr == NULL) {
				plog(ASL_LEVEL_ERR, 
					"failed to get CR");
				goto end;
			}
		}

		if (iph1->cert != NULL && iph1->rmconf->send_cert)
			need_cert = 1;

		/* add ID payload */
		plist = isakmp_plist_append(plist, iph1->id, ISAKMP_NPTYPE_ID);

		/* add CERT payload if there */
		// we don't support sending of certchains
		if (need_cert)
			plist = isakmp_plist_append(plist, iph1->cert->pl, ISAKMP_NPTYPE_CERT);
		/* add SIG payload */
		plist = isakmp_plist_append(plist, iph1->sig, ISAKMP_NPTYPE_SIG);

		/* create isakmp CR payload */
		if (need_cr)
			plist = isakmp_plist_append(plist, cr, ISAKMP_NPTYPE_CR);
		break;

	case OAKLEY_ATTR_AUTH_METHOD_RSAENC:
	case OAKLEY_ATTR_AUTH_METHOD_RSAREV:
#ifdef ENABLE_HYBRID
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAENC_I:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAENC_R:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAREV_I:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAREV_R:
#endif
		plog(ASL_LEVEL_ERR, 
			"not supported authentication type %d\n",
			iph1->approval->authmethod);
		goto end;
	default:
		plog(ASL_LEVEL_ERR, 
			"invalid authentication type %d\n",
			iph1->approval->authmethod);
		goto end;
	}

	if (iph1->side == INITIATOR) {
		notp_ini = isakmp_plist_append_initial_contact(iph1, plist);
	}
	
	buf = isakmp_plist_set_all (&plist, iph1);
	
#ifdef HAVE_PRINT_ISAKMP_C
	isakmp_printpacket(buf, iph1->local, iph1->remote, 1);
#endif

	/* encoding */
	new = oakley_do_encrypt(iph1, buf, iph1->ivm->ive, iph1->ivm->iv);
	if (new == NULL) {
		plog(ASL_LEVEL_ERR, 
			 "failed to encrypt");
		goto end;
	}

	vfree(buf);

	buf = new;

	error = 0;

end:
	if (cr)
		vfree(cr);
	if (error && buf != NULL) {
		vfree(buf);
		buf = NULL;
	}
	if (notp_ini)
		vfree(notp_ini);

	return buf;
}
