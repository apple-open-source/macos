/*	$NetBSD: isakmp.c,v 1.20.6.7 2007/08/01 11:52:20 vanhu Exp $	*/

/* Id: isakmp.c,v 1.74 2006/05/07 21:32:59 manubsd Exp */

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
#define __APPLE_API_PRIVATE

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/queue.h>

#include <netinet/in.h>
#include <net/if_var.h>
#include <netinet6/in6_var.h>
#include <arpa/inet.h>

#ifndef HAVE_NETINET6_IPSEC
#include <netinet/ipsec.h>
#else 
#include <netinet6/ipsec.h>
#endif

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
#include <netdb.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <ctype.h>
#ifdef ENABLE_HYBRID
#include <resolv.h>
#endif

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "sockmisc.h"
#include "schedule.h"
#include "debug.h"
#include "session.h"
#include "fsm.h"

#include "remoteconf.h"
#include "localconf.h"
#include "grabmyaddr.h"
#include "isakmp_var.h"
#include "isakmp.h"
#include "oakley.h"
#include "handler.h"
#include "proposal.h"
#include "ipsec_doi.h"
#include "pfkey.h"
#include "crypto_openssl.h"
#include "policy.h"
#include "isakmp_ident.h"
#include "isakmp_agg.h"
#include "isakmp_quick.h"
#include "isakmp_inf.h"
#include "vpn_control.h"
#include "vpn_control_var.h"
#ifdef ENABLE_HYBRID
#include "vendorid.h"
#include "isakmp_xauth.h"
#include "isakmp_unity.h"
#include "isakmp_cfg.h"
#endif
#ifdef ENABLE_FRAG
#include "isakmp_frag.h"
#endif
#include "strnames.h"

#include <fcntl.h>

#ifdef ENABLE_NATT
# include "nattraversal.h"
#endif
#include "ike_session.h"
#  include <netinet/in.h>
#  include <netinet/udp.h>
#  include <netinet/in_systm.h>
#  include <netinet/ip.h>
#  define SOL_UDP IPPROTO_UDP
#include "ipsecSessionTracer.h"
#include "ipsecMessageTracer.h"
#include "power_mgmt.h"


extern caddr_t val2str (const char *, size_t);
u_char i_ck0[] = { 0,0,0,0,0,0,0,0 }; /* used to verify the i_ck. */
u_char r_ck0[] = { 0,0,0,0,0,0,0,0 }; /* used to verify the r_ck. */


static void isakmp_main (vchar_t *, struct sockaddr_storage *, struct sockaddr_storage *);
static void ikev1_received_packet(vchar_t *, struct sockaddr_storage *, struct sockaddr_storage *);
static int ikev1_ph1begin_r (ike_session_t *session, vchar_t *, struct sockaddr_storage *, struct sockaddr_storage *, u_int8_t);
static int ikev1_ph2begin_i (phase1_handle_t *, phase2_handle_t *);
static int ikev1_ph2begin_r (phase1_handle_t *, vchar_t *);


#ifdef ENABLE_FRAG
static void frag_handler (phase1_handle_t *, vchar_t *, struct sockaddr_storage *, struct sockaddr_storage *);
#endif

/*
 * isakmp packet handler
 */
void
isakmp_handler(int so_isakmp)
{
	struct isakmp isakmp;
	union {
		u_int64_t	force_align;				// Wcast-align fix - force alignment
		char		buf[sizeof (isakmp) + 4];
		u_int32_t	non_esp[2];
		char		lbuf[sizeof(struct udphdr) + 
				     sizeof(struct ip) + 
				     sizeof(isakmp) + 4];
	} x;
	struct sockaddr_storage remote;
	struct sockaddr_storage local;
	unsigned int remote_len = sizeof(remote);
	unsigned int local_len = sizeof(local);
	ssize_t len = 0;
	int extralen = 0;
	u_short port;
	vchar_t *buf = NULL, *tmpbuf = NULL;
	int error = -1;

	if (slept_at || woke_at) {
		plog(ASL_LEVEL_DEBUG, 
			 "ignoring isakmp port until power-mgmt event is handled.\n");
		return;
	}

	/* read message by MSG_PEEK */
	while ((len = recvfromto(so_isakmp, x.buf, sizeof(x),
		    MSG_PEEK, &remote, &remote_len, &local, &local_len)) < 0) {
		if (errno == EINTR)
			continue;
		plog(ASL_LEVEL_ERR, 
			"failed to receive isakmp packet: %s\n",
			strerror (errno));
		goto end;
	}

	/* keep-alive packet - ignore */
	if (len == 1 && (x.buf[0]&0xff) == 0xff) {
		/* Pull the keep-alive packet */
		if ((len = recvfrom(so_isakmp, (char *)x.buf, 1,
		    0, (struct sockaddr *)&remote, &remote_len)) != 1) {
			plog(ASL_LEVEL_ERR, 
			    "failed to receive keep alive packet: %s\n",
			    strerror (errno));
        }
		goto end;
	}


	/* we don't know about portchange yet, 
	   look for non-esp marker instead */
	if (x.non_esp[0] == 0 && x.non_esp[1] != 0)
		extralen = NON_ESP_MARKER_LEN;

	/* now we know if there is an extra non-esp 
	   marker at the beginning or not */
	memcpy ((char *)&isakmp, x.buf + extralen, sizeof (isakmp));

	/* check isakmp header length, as well as sanity of header length */
	if (len < sizeof(isakmp) || ntohl(isakmp.len) < sizeof(isakmp)) {
		plog(ASL_LEVEL_ERR,
			"packet shorter than isakmp header size (size: %zu, minimum expected: %zu)\n", len, sizeof(isakmp));
		/* dummy receive */
		if ((len = recvfrom(so_isakmp, (char *)&isakmp, sizeof(isakmp),
			    0, (struct sockaddr *)&remote, &remote_len)) < 0) {
			plog(ASL_LEVEL_ERR, 
				"failed to receive isakmp packet: %s\n",
				strerror (errno));
		}
		goto end;
	}

	/* reject it if the size is tooooo big. */
	if (ntohl(isakmp.len) > 0xffff) {
		plog(ASL_LEVEL_ERR, 
			"the length in the isakmp header is too big.\n");
		if ((len = recvfrom(so_isakmp, (char *)&isakmp, sizeof(isakmp),
			    0, (struct sockaddr *)&remote, &remote_len)) < 0) {
			plog(ASL_LEVEL_ERR, 
				"failed to receive isakmp packet: %s\n",
				strerror (errno));
		}
		goto end;
	}

	/* read real message */
	if ((tmpbuf = vmalloc(ntohl(isakmp.len) + extralen)) == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to allocate reading buffer (%u Bytes)\n",
			ntohl(isakmp.len) + extralen);
		/* dummy receive */
		if ((len = recvfrom(so_isakmp, (char *)&isakmp, sizeof(isakmp),
			    0, (struct sockaddr *)&remote, &remote_len)) < 0) {
			plog(ASL_LEVEL_ERR, 
				"failed to receive isakmp packet: %s\n", 
				strerror (errno));
				error = -2;    /* serious problem with socket */
		}
		goto end;
	}

	while ((len = recvfromto(so_isakmp, (char *)tmpbuf->v, tmpbuf->l,
	                    0, &remote, &remote_len, &local, &local_len)) < 0) {
		if (errno == EINTR)
			continue;
		plog(ASL_LEVEL_ERR, 
			"failed to receive isakmp packet: %s\n",
			strerror (errno));
		goto end;
	}

	if (len < extralen) {
		plog(ASL_LEVEL_ERR, 
			 "invalid len (%zd Bytes) & extralen (%d Bytes)\n",
			 len, extralen);
		goto end;
	}

	if ((buf = vmalloc(len - extralen)) == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to allocate reading buffer (%lu Bytes)\n",
			(len - extralen));
		goto end;
	}
	
	memcpy (buf->v, tmpbuf->v + extralen, buf->l);

	len -= extralen;
	
	if (len != buf->l) {
		plog(ASL_LEVEL_ERR, "received invalid length (%zd != %zu), why ?\n",
			len, buf->l);
		goto end;
	}

	plog(ASL_LEVEL_DEBUG, "%zd bytes message received %s\n",
			 len, saddr2str_fromto("from %s to %s", 
								   (struct sockaddr *)&remote,
								   (struct sockaddr *)&local));

	/* avoid packets with malicious port/address */
	switch (remote.ss_family) {
	case AF_INET:
		port = ((struct sockaddr_in *)&remote)->sin_port;
		break;
#ifdef INET6
	case AF_INET6:
		port = ((struct sockaddr_in6 *)&remote)->sin6_port;
		break;
#endif
	default:
		plog(ASL_LEVEL_ERR, 
			"invalid family: %d\n", remote.ss_family);
		goto end;
	}
	if (port == 0) {
		plog(ASL_LEVEL_ERR,
			"src port == 0 (valid as UDP but not with IKE)\n");
		goto end;
	}

	/* XXX: check sender whether to be allowed or not to accept */

	/* XXX: I don't know how to check isakmp half connection attack. */

	/* simply reply if the packet was processed. */

	if (ike_session_check_recvdpkt(&remote, &local, buf)) {
		IPSECLOGASLMSG("Received retransmitted packet from %s.\n",
					   saddr2str((struct sockaddr *)&remote));

		plog(ASL_LEVEL_NOTICE, 
			"the packet is retransmitted by %s.\n",
			saddr2str((struct sockaddr *)&remote));
		error = 0;
		goto end;
	}

	/* isakmp main routine */
	isakmp_main(buf, &remote, &local);

end:
	if (tmpbuf != NULL)
		vfree(tmpbuf);
	if (buf != NULL)
		vfree(buf);

	return;
}

/*
 * main processing to handle isakmp payload
 */
static void
isakmp_main(vchar_t *msg, struct sockaddr_storage *remote,  struct sockaddr_storage *local)
{
	struct isakmp *isakmp = (struct isakmp *)msg->v;
    u_int8_t isakmp_version = isakmp->v;

#ifdef HAVE_PRINT_ISAKMP_C
	isakmp_printpacket(msg, remote, local, 0);
#endif

	/* the initiator's cookie must not be zero */
	if (memcmp(&isakmp->i_ck, r_ck0, sizeof(cookie_t)) == 0) {
		plog(ASL_LEVEL_ERR, 
			"malformed cookie received.\n");
		return;
	}

	/* Check the Major and Minor Version fields. */
	/*
	 * XXX Is is right to check version here ?
	 * I think it may no be here because the version depends
	 * on exchange status.
	 */
	if (ISAKMP_GETMAJORV(isakmp_version) != ISAKMP_MAJOR_VERSION_IKEV1 &&
        ISAKMP_GETMAJORV(isakmp_version) != ISAKMP_MAJOR_VERSION_IKEV2) {        
			plog(ASL_LEVEL_ERR, "invalid major version %d.\n", isakmp_version);
			return;
    }

#if 0
#if ISAKMP_MINOR_VERSION > 0        //%%%%%%%% fix this
		if (ISAKMP_GETMINORV(isakmp->v) < ISAKMP_MINOR_VERSION) {   //%%%%%%%%%%%%%%% ??????
			plog(ASL_LEVEL_ERR,
				"invalid minor version %d.\n",
				ISAKMP_GETMINORV(isakmp->v));
			return;
		}
#endif
#endif

    if (isakmp_version == ISAKMP_VERSION_NUMBER_IKEV1) {
		/* check the Flags field. */
		/* XXX How is the exclusive check, E and A ? */
		if (isakmp->flags & ~(ISAKMP_FLAG_E | ISAKMP_FLAG_C | ISAKMP_FLAG_A)) { //%%%%%%%%%%%% any other flags for IKEv2 ?????
			plog(ASL_LEVEL_ERR, "invalid flag 0x%02x.\n", isakmp->flags);
			return;
		}

		/* ignore commit bit. */
		if (ISSET(isakmp->flags, ISAKMP_FLAG_C)) {
			if (isakmp->msgid == 0) {                   //%%%%% does this apply to V2
				isakmp_info_send_nx(isakmp, remote, local,
									ISAKMP_NTYPE_INVALID_FLAGS, NULL);
				plog(ASL_LEVEL_ERR, "Commit bit on Phase 1 forbidden.\n");
				return;
			}
		}

        ikev1_received_packet(msg, local, remote);
	}
    return;
}

/*
 * ikev1_received_packet
 * Handler for received IKEv1 Packets
 */
static void
ikev1_received_packet(vchar_t *msg, struct sockaddr_storage *local, struct sockaddr_storage *remote)
{
    ike_session_t       *session;
    phase1_handle_t     *iph1;
    
    struct isakmp *isakmp = (struct isakmp *)msg->v;
    isakmp_index *index = (isakmp_index *)isakmp;
    
    session = ike_session_get_session(local, remote, 1);
    if (!session) {
        plog (ASL_LEVEL_INFO, "failed to allocate or find ike session.\n");
        fatal_error(-1);
    }
        
	iph1 = ike_session_getph1byindex(session, index);
	if (iph1 != NULL) {
		/* validity check */
		if (memcmp(&isakmp->r_ck, r_ck0, sizeof(cookie_t)) == 0 &&
		    iph1->side == INITIATOR) {
			IPSECSESSIONTRACEREVENT(iph1->parent_session,
									IPSECSESSIONEVENTCODE_IKE_PACKET_RX_FAIL,
									CONSTSTR("Malformed or unexpected cookie"),
									CONSTSTR("Failed to process packet (malformed/unexpected cookie)"));
			plog(ASL_LEVEL_DEBUG, 
				"Malformed cookie received or "
				"the initiator's cookies collide.\n");
			return;
		}
        
        
		/* Floating ports for NAT-T */
		if (NATT_AVAILABLE(iph1) &&
		    ! (iph1->natt_flags & NAT_PORTS_CHANGED) &&
		    ((cmpsaddrstrict(iph1->remote, remote) != 0) ||
		    (cmpsaddrstrict(iph1->local, local) != 0)))
		{     //%%%%%%%%%%%%%%%%%%%% make this a separate function - ikev2 needs it      
			/* prevent memory leak */
			racoon_free(iph1->remote);
			racoon_free(iph1->local);
			iph1->remote = NULL;
			iph1->local = NULL;

			/* copy-in new addresses */
			iph1->remote = dupsaddr(remote);
			if (iph1->remote == NULL) {
				IPSECSESSIONTRACEREVENT(iph1->parent_session,
										IPSECSESSIONEVENTCODE_IKE_PACKET_RX_FAIL,
										CONSTSTR("Failed to duplicate remote address"),
										CONSTSTR("Failed to process Phase 1 message (can't duplicate remote address"));
				plog(ASL_LEVEL_ERR,
				   "Phase 1 failed: dupsaddr failed.\n");
                fatal_error(-1);
			}
			iph1->local = dupsaddr(local);
			if (iph1->local == NULL) {
				IPSECSESSIONTRACEREVENT(iph1->parent_session,
										IPSECSESSIONEVENTCODE_IKE_PACKET_RX_FAIL,
										CONSTSTR("Failed to duplicate local address"),
										CONSTSTR("Failed to process Phase 1 message (can't duplicate local address"));
				plog(ASL_LEVEL_ERR,
				   "Phase 1 failed: dupsaddr failed.\n");
                fatal_error(-1);                
            }

			/* set the flag to prevent further port floating
			   (FIXME: should we allow it? E.g. when the NAT gw 
			    is rebooted?) */
			iph1->natt_flags |= NAT_PORTS_CHANGED | NAT_ADD_NON_ESP_MARKER;
			
			/* print some neat info */
			plog (ASL_LEVEL_INFO, 
			      "NAT-T: ports changed to: %s\n",
			      saddr2str_fromto("%s<->%s", (struct sockaddr *)iph1->remote, (struct sockaddr *)iph1->local));
		}

		/* must be same addresses in one stream of a phase at least. */
		if (cmpsaddrstrict(iph1->remote, remote) != 0) {
			char *saddr_db, *saddr_act;

			saddr_db = racoon_strdup(saddr2str((struct sockaddr *)iph1->remote));
			saddr_act = racoon_strdup(saddr2str((struct sockaddr *)remote));
			STRDUP_FATAL(saddr_db);
			STRDUP_FATAL(saddr_act);

			plog(ASL_LEVEL_WARNING, 
				"Remote address mismatched. db=%s, act=%s\n",
				saddr_db, saddr_act);

			racoon_free(saddr_db);
			racoon_free(saddr_act);
		}

		/*
		 * don't check of exchange type here because other type will have
		 * the same index, for example, informational exchange.
		 */

        // received ike packets: update dpd checks 
        isakmp_reschedule_info_monitor_if_pending(iph1, "IKE packet received from peer");

	}
      
    //
    // Check exchange type and process accordingly
    //
	switch (isakmp->etype) {

        case ISAKMP_ETYPE_IDENT:
        case ISAKMP_ETYPE_AGG:
            {   
                /* phase 1 validity check */
                if (isakmp->msgid != 0) {
                    plog(ASL_LEVEL_ERR, "Message id should be zero in Phase 1.\n");
                    return;
                }
                
                /* search for isakmp status record of phase 1 */
                if (iph1 == NULL) {
                    /*
                     * the packet must be the 1st message from a initiator
                     * or the 2nd message from the responder.
                     */
                    
                    /* search for phase1 handle by index without r_ck */
                    iph1 = ike_session_getph1byindex0(session, index);
                    if (iph1 == NULL) {
                        /*it must be the 1st message from a initiator.*/
                        if (memcmp(&isakmp->r_ck, r_ck0,
                                   sizeof(cookie_t)) != 0) {
                            
                            plog(ASL_LEVEL_DEBUG, "Malformed cookie received "
                                 "or the spi expired.\n");
                            return;
                        }
                        
                        /* Initiation of new exchange */
                        ikev1_ph1begin_r(session, msg, remote, local, isakmp->etype);
                        return;
                    }
                }
                
                /*
                 * Don't delete phase 1 handler for mismatch
                 * because of no authentication has been completed.
                 */
                if (iph1->etype != isakmp->etype) {
                    IPSECSESSIONTRACEREVENT(iph1->parent_session,
                                            IPSECSESSIONEVENTCODE_IKE_PACKET_RX_FAIL,
                                            CONSTSTR("Mismatched exchange type"),
                                            CONSTSTR("Failed to process Phase 1 message (mismatched exchange type)"));
                    plog(ASL_LEVEL_ERR,
                         "Exchange type is mismatched: "
                         "db=%s packet=%s, ignore it.\n",
                         s_isakmp_etype(iph1->etype),
                         s_isakmp_etype(isakmp->etype));
                    return;
                }
                
                if (isakmp->np == ISAKMP_NPTYPE_FRAG) {
                    frag_handler(iph1, msg, remote, local);
                    return;
                }
                fsm_ikev1_phase1_process_payloads(iph1, msg);
            }
            break;
            
        case ISAKMP_ETYPE_INFO:
        case ISAKMP_ETYPE_ACKINFO:
            {
                /*
                 * iph1 must be present for Information message.
                 * if iph1 is null then trying to get the phase1 status
                 * as the packet from responder againt initiator's 1st
                 * exchange in phase 1.
                 * NOTE: We think such informational exchange should be ignored.
                 */
                if (iph1 == NULL) {
                    iph1 = ike_session_getph1byindex0(session, index);
                    if (iph1 == NULL) {
                        plog(ASL_LEVEL_ERR, "Unknown Informational exchange received.\n");
                        return;
                    }
                    if (cmpsaddrstrict(iph1->remote, remote) != 0) {
                        plog(ASL_LEVEL_WARNING,
                             "Remote address mismatched. "
                             "db=%s\n",
                             saddr2str((struct sockaddr *)iph1->remote));
                    }
                }                
                if (isakmp->np == ISAKMP_NPTYPE_FRAG)
                    return frag_handler(iph1, msg, remote, local);
                
                if (isakmp_info_recv(iph1, msg) < 0)
                    return;
            }
            break;
            
        case ISAKMP_ETYPE_QUICK:
            {
                u_int32_t msgid = isakmp->msgid;
                phase2_handle_t *iph2;
                
                if (iph1 == NULL) {
                    isakmp_info_send_nx(isakmp, remote, local,
                                        ISAKMP_NTYPE_INVALID_COOKIE, NULL);
                    plog(ASL_LEVEL_ERR, "Can't start the quick mode, "
                         "there is no ISAKMP-SA, %s\n", isakmp_pindex((isakmp_index *)&isakmp->i_ck,
                                                                      isakmp->msgid));
                    return;
                }
    #ifdef ENABLE_HYBRID
                /* Reinit the IVM if it's still there */		
                if (iph1->mode_cfg && iph1->mode_cfg->ivm) {
                    oakley_delivm(iph1->mode_cfg->ivm);
                    iph1->mode_cfg->ivm = NULL;
                }
    #endif
                if (isakmp->np == ISAKMP_NPTYPE_FRAG) {
                    frag_handler(iph1, msg, remote, local);
                    return;
                }
                
                /* check status of phase 1 whether negotiated or not. */
                if (!FSM_STATE_IS_ESTABLISHED(iph1->status)) {
                    IPSECSESSIONTRACEREVENT(iph1->parent_session,
                                            IPSECSESSIONEVENTCODE_IKEV1_PH2_INIT_DROP,
                                            CONSTSTR("Can't start Phase 2 without valid Phase 1"),
                                            CONSTSTR("Failed to start Phase 2 responder (no established Phase 1"));
                    plog(ASL_LEVEL_ERR, "can't start the quick mode, "
                         "there is no valid ISAKMP-SA, %s\n", isakmp_pindex(&iph1->index, iph1->msgid));
                    return;
                }
                
                /* search isakmp phase 2 stauts record. */
                iph2 = ike_session_getph2bymsgid(iph1, msgid);
                if (iph2 == NULL) {
                    /* it must be new negotiation as responder */
                    ikev1_ph2begin_r(iph1, msg);
                    return;
                }
                
                /* commit bit. */
                /* XXX
                 * we keep to set commit bit during negotiation.
                 * When SA is configured, bit will be reset.
                 * XXX
                 * don't initiate commit bit.  should be fixed in the future.
                 */
                if (ISSET(isakmp->flags, ISAKMP_FLAG_C))
                    iph2->flags |= ISAKMP_FLAG_C;
                
                if (ISSET(isakmp->flags, ISAKMP_FLAG_E) &&
                    (iph2->ph1 == NULL || iph2->ph1->approval == NULL)) {
                    IPSECSESSIONTRACEREVENT(iph2->parent_session,
                                            IPSECSESSIONEVENTCODE_IKEV1_PH2_INIT_DROP,
                                            CONSTSTR("Can't continue Phase 2 without valid Phase 1"),
                                            CONSTSTR("Failed to continue Phase 2 resonder (invalid linked Phase 1"));
                    plog(ASL_LEVEL_ERR, "can't start the quick mode, "
                         "invalid linked ISAKMP-SA\n");
                    return;			
                }
                fsm_ikev1_phase2_process_payloads(iph2, msg);
            }
            break;
            
        case ISAKMP_ETYPE_CFG:
            {
                if (iph1 == NULL) {
                    plog(ASL_LEVEL_ERR,
                         "mode config %d from %s, "
                         "but we have no ISAKMP-SA.\n",
                         isakmp->etype, saddr2str((struct sockaddr *)remote));
                    return;
                }
                if (!FSM_STATE_IS_ESTABLISHED(iph1->status)) {
                    plog(ASL_LEVEL_ERR,
                         "mode config %d from %s, "
                         "but ISAKMP-SA %s isn't established.\n",
                         isakmp->etype, saddr2str((struct sockaddr *)remote),
                         isakmp_pindex(&iph1->index, iph1->msgid));
                    return;
                }
                if (isakmp->np == ISAKMP_NPTYPE_FRAG)
                    return frag_handler(iph1, msg, remote, local);
                isakmp_cfg_r(iph1, msg);
            }
            break;

        case ISAKMP_ETYPE_NEWGRP:        
        case ISAKMP_ETYPE_AUTH:
        case ISAKMP_ETYPE_NONE:
        default:
            plog(ASL_LEVEL_ERR,
                 "Invalid exchange type %d from %s.\n",
                 isakmp->etype, saddr2str((struct sockaddr *)remote));
            break;
	}
}

/* new negotiation of phase 1 for initiator */
int
ikev1_ph1begin_i(ike_session_t *session, struct remoteconf *rmconf, struct sockaddr_storage *remote, 
                 struct sockaddr_storage *local, int started_by_api)
{

	phase1_handle_t *iph1;
#ifdef ENABLE_STATS
	struct timeval start, end;
#endif

    if (session == NULL) {
        session = ike_session_get_session(local, remote, 1);
        if (!session) {
            plog (ASL_LEVEL_INFO, "failed to allocate or find ike session.\n");
            fatal_error(-1);
        }
    }
        
	/* get new entry to isakmp status table. */
	iph1 = ike_session_newph1(ISAKMP_VERSION_NUMBER_IKEV1);
	if (iph1 == NULL)
		return -1;

	iph1->rmconf = rmconf;
    retain_rmconf(iph1->rmconf);
	iph1->side = INITIATOR;
	iph1->started_by_api = started_by_api;
	iph1->version = ISAKMP_VERSION_NUMBER_IKEV1;
	iph1->msgid = 0;
	iph1->flags = 0;
	iph1->ph2cnt = 0;

#ifdef ENABLE_HYBRID
	if ((iph1->mode_cfg = isakmp_cfg_mkstate()) == NULL) {
		/* don't call remph1(iph1) until after insph1(iph1) is called */
		ike_session_delph1(iph1);
		return -1;
	}
#endif

	if(rmconf->ike_frag == ISAKMP_FRAG_FORCE)
		iph1->frag = 1;
	else
		iph1->frag = 0;
	iph1->frag_chain = NULL;
	iph1->approval = NULL;

	/* XXX copy remote address */
	if (copy_ph1addresses(iph1, rmconf, remote, local) < 0) {
		/* don't call remph1(iph1) until after insph1(iph1) is called */
		iph1 = NULL; /* deleted in copy_ph1addresses */
		return -1;
	}

	if (ike_session_link_phase1(session, iph1) != 0) {
		plog(ASL_LEVEL_DEBUG, "Failed to link ph1 to session\n");
		ike_session_delph1(iph1);
		return -1;
	}
	// HACK!!! to track rekeys across SIGHUPs
	if (started_by_api == VPN_RESTARTED_BY_API &&
		!iph1->is_rekey) {
		iph1->parent_session->established = 1;
		iph1->parent_session->ikev1_state.ph2cnt++;
		iph1->is_rekey = 1;
	}

	/* start phase 1 exchange */
	iph1->etype = rmconf->etypes->type;
    if (iph1->etype == ISAKMP_ETYPE_IDENT)
        fsm_set_state(&iph1->status, IKEV1_STATE_IDENT_I_START);
    else if (iph1->etype == ISAKMP_ETYPE_AGG)
        fsm_set_state(&iph1->status, IKEV1_STATE_AGG_I_START);
    else
        return -1;

	plog(ASL_LEVEL_DEBUG, "===\n");
    {
	char *a;

	a = racoon_strdup(saddr2str((struct sockaddr *)iph1->local));
	STRDUP_FATAL(a);

	plog(ASL_LEVEL_INFO,
		"initiate new phase 1 negotiation: %s<=>%s\n",
		a, saddr2str((struct sockaddr *)iph1->remote));
	racoon_free(a);
    }
	plog(ASL_LEVEL_INFO,
		"begin %s mode.\n",
		s_isakmp_etype(iph1->etype));

#ifdef ENABLE_STATS
	gettimeofday(&iph1->start, NULL);
	gettimeofday(&start, NULL);
#endif

	IPSECLOGASLMSG("IPSec Phase 1 started (Initiated by me).\n");

    if (fsm_ikev1_phase1_send_response(iph1, NULL)) {
        ike_session_unlink_phase1(iph1);
		return -1;
	}

#ifdef ENABLE_STATS
	gettimeofday(&end, NULL);
	syslog(LOG_NOTICE, "%s(%s): %8.6f",
		"Phase 1",
		s_isakmp_state(iph1->etype, iph1->side, iph1->status),
		timedelta(&start, &end));
#endif

#ifdef ENABLE_VPNCONTROL_PORT
	vpncontrol_notify_phase_change(1, FROM_LOCAL, iph1, NULL);
#endif
	
	return 0;
}

/* new negotiation of phase 1 for responder */
static int
ikev1_ph1begin_r(ike_session_t *session, vchar_t *msg, struct sockaddr_storage *remote, 
                 struct sockaddr_storage *local, u_int8_t etype)
{

    struct isakmp *isakmp = (struct isakmp *)msg->v;
	struct remoteconf *rmconf;
	phase1_handle_t *iph1;
	struct etypes *etypeok;
#ifdef ENABLE_STATS
	struct timeval start, end;
#endif

	/* look for my configuration */
	rmconf = getrmconf(remote);
	if (rmconf == NULL) {
		plog(ASL_LEVEL_ERR,
			"couldn't find "
			"configuration.\n");
		return -1;
	}

	/* check to be acceptable exchange type */
	etypeok = check_etypeok(rmconf, etype);
	if (etypeok == NULL) {
		plog(ASL_LEVEL_ERR,
			"not acceptable %s mode\n", s_isakmp_etype(etype));
		return -1;
	}
        
	/* get new entry to isakmp status table. */
	iph1 = ike_session_newph1(ISAKMP_VERSION_NUMBER_IKEV1);
	if (iph1 == NULL)
		return -1;

	memcpy(&iph1->index.i_ck, &isakmp->i_ck, sizeof(iph1->index.i_ck));
	iph1->rmconf = rmconf;
	retain_rmconf(iph1->rmconf);
	iph1->flags = 0;
	iph1->side = RESPONDER;
	iph1->started_by_api = 0;
	iph1->etype = etypeok->type;
	iph1->version = isakmp->v;
	iph1->msgid = 0;
    
    if (iph1->etype == ISAKMP_ETYPE_IDENT)
        fsm_set_state(&iph1->status, IKEV1_STATE_IDENT_R_START);
    else if (iph1->etype == ISAKMP_ETYPE_AGG)
        fsm_set_state(&iph1->status, IKEV1_STATE_AGG_R_START);
    else
        return -1;


#ifdef ENABLE_HYBRID
	if ((iph1->mode_cfg = isakmp_cfg_mkstate()) == NULL) {
		/* don't call remph1(iph1) until after insph1(iph1) is called */
		ike_session_delph1(iph1);
		return -1;
	}
#endif

	iph1->frag = 0;
	iph1->frag_chain = NULL;
	iph1->approval = NULL;

	/* RFC3947 says that we MUST accept new phases1 on NAT-T floated port.
	 * We have to setup this flag now to correctly generate the first reply.
	 * Don't know if a better check could be done for that ?
	 */
	if(extract_port(local) == lcconf->port_isakmp_natt)
		iph1->natt_flags |= (NAT_PORTS_CHANGED);

	/* copy remote address */
	if (copy_ph1addresses(iph1, rmconf, remote, local) < 0) {
		/* don't call remph1(iph1) until after insph1(iph1) is called */
		iph1 = NULL; /* deleted in copy_ph1addresses */
		return -1;
	}

	if (ike_session_link_phase1(session, iph1) != 0) {
		ike_session_delph1(iph1);
		return -1;
	}

	plog(ASL_LEVEL_DEBUG, "===\n");
    {
	char *a;

	a = racoon_strdup(saddr2str((struct sockaddr *)iph1->local));
	STRDUP_FATAL(a);

	plog(ASL_LEVEL_INFO,
		"respond new phase 1 negotiation: %s<=>%s\n",
		a, saddr2str((struct sockaddr *)iph1->remote));
	racoon_free(a);
    }
	plog(ASL_LEVEL_INFO,
		"begin %s mode.\n", s_isakmp_etype(etype));

#ifdef ENABLE_STATS
	gettimeofday(&iph1->start, NULL);
	gettimeofday(&start, NULL);
#endif

	IPSECLOGASLMSG("IPSec Phase 1 started (Initiated by peer).\n");

	/* now that we have a phase1 handle, feed back into our
	 * main receive function to catch fragmented packets
	 */
	isakmp_main(msg, remote, local);
    return 0;
}

/* new negotiation of phase 2 for initiator */
static int
ikev1_ph2begin_i(phase1_handle_t *iph1, phase2_handle_t *iph2)
{
    
#ifdef ENABLE_HYBRID
	if (xauth_check(iph1) != 0) {
		plog(ASL_LEVEL_ERR,
		    "Attempt to start phase 2 whereas Xauth failed\n");
		return -1;
	}
#endif

	/* found ISAKMP-SA. */
	plog(ASL_LEVEL_DEBUG, "===\n");
	plog(ASL_LEVEL_DEBUG, "begin QUICK mode.\n");
    {
	char *a;
	a = racoon_strdup(saddr2str((struct sockaddr *)iph2->src));
	STRDUP_FATAL(a);

	plog(ASL_LEVEL_INFO,
		"initiate new phase 2 negotiation: %s<=>%s\n",
		a, saddr2str((struct sockaddr *)iph2->dst));
	racoon_free(a);
    }

#ifdef ENABLE_STATS
	gettimeofday(&iph2->start, NULL);
#endif

	iph2->is_dying = 0;
	fsm_set_state(&iph2->status, IKEV1_STATE_QUICK_I_START);

	IPSECLOGASLMSG("IPSec Phase 2 started (Initiated by me).\n");
    if (quick_iprep(iph2, NULL))
		return -1;
	
#ifdef ENABLE_VPNCONTROL_PORT
	vpncontrol_notify_phase_change(1, FROM_LOCAL, NULL, iph2);
#endif

	return 0;
}

/* new negotiation of phase 2 for responder */
static int
ikev1_ph2begin_r(phase1_handle_t *iph1, vchar_t *msg)
{
	struct isakmp *isakmp = (struct isakmp *)msg->v;
	phase2_handle_t *iph2 = 0;
	int error;
#ifdef ENABLE_STATS
	struct timeval start, end;
#endif
#ifdef ENABLE_HYBRID
	if (xauth_check(iph1) != 0) {
		plog(ASL_LEVEL_ERR,
		    "Attempt to start Phase 2 whereas Xauth failed\n");
		return -1;
	}
#endif

	iph2 = ike_session_newph2(ISAKMP_VERSION_NUMBER_IKEV1, PHASE2_TYPE_SA);
	if (iph2 == NULL) {
		plog(ASL_LEVEL_ERR,
			"failed to allocate Phase 2 entry.\n");
		return -1;
	}

	iph2->side = RESPONDER;
	iph2->version = ISAKMP_VERSION_NUMBER_IKEV1;
	fsm_set_state(&iph2->status, IKEV1_STATE_QUICK_R_START);
	iph2->flags = isakmp->flags;
	iph2->msgid = isakmp->msgid;
	iph2->seq = pk_getseq();
	iph2->ivm = oakley_newiv2(iph1, iph2->msgid);
	if (iph2->ivm == NULL) {
		ike_session_delph2(iph2);
		return -1;
	}
	iph2->dst = dupsaddr(iph1->remote);	/* XXX should be considered */
	if (iph2->dst == NULL) {
		ike_session_delph2(iph2);
		return -1;
	}
	switch (iph2->dst->ss_family) {
	case AF_INET:
#ifndef ENABLE_NATT
		((struct sockaddr_in *)iph2->dst)->sin_port = 0;
#endif
		break;
#ifdef INET6
	case AF_INET6:
#ifndef ENABLE_NATT
		((struct sockaddr_in6 *)iph2->dst)->sin6_port = 0;
#endif
		break;
#endif
	default:
		plog(ASL_LEVEL_ERR,
			"invalid family: %d\n", iph2->dst->ss_family);
		ike_session_delph2(iph2);
		return -1;
	}

	iph2->src = dupsaddr(iph1->local);	/* XXX should be considered */
	if (iph2->src == NULL) {
		ike_session_delph2(iph2);
		return -1;
	}
	switch (iph2->src->ss_family) {
	case AF_INET:
#ifndef ENABLE_NATT
		((struct sockaddr_in *)iph2->src)->sin_port = 0;
#endif
		break;
#ifdef INET6
	case AF_INET6:
#ifndef ENABLE_NATT
		((struct sockaddr_in6 *)iph2->src)->sin6_port = 0;
#endif
		break;
#endif
	default:
		plog(ASL_LEVEL_ERR,
			"invalid family: %d\n", iph2->src->ss_family);
		ike_session_delph2(iph2);
		return -1;
	}
	
	if (ike_session_link_ph2_to_ph1(iph1, iph2))
        return -1;    
	iph2->is_dying = 0;

	plog(ASL_LEVEL_DEBUG, "===\n");
    {
	char *a;

	a = racoon_strdup(saddr2str((struct sockaddr *)iph2->src));
	STRDUP_FATAL(a);

	plog(ASL_LEVEL_INFO,
		"respond new phase 2 negotiation: %s<=>%s\n",
		a, saddr2str((struct sockaddr *)iph2->dst));
	racoon_free(a);
    }

#ifdef ENABLE_STATS
	gettimeofday(&start, NULL);
#endif

	IPSECLOGASLMSG("IPSec Phase 2 started (Initiated by peer).\n");

    error = fsm_ikev1_phase2_process_payloads(iph2, msg);
    if (error)
        return error;

#ifdef ENABLE_VPNCONTROL_PORT
	vpncontrol_notify_phase_change(1, FROM_REMOTE, NULL, iph2);
#endif

	return 0;
}

int
ikev1_phase1_established(phase1_handle_t *iph1)
{
    int   spi_cmp;
    u_int rekey_lifetime;
    int ini_contact = iph1->rmconf->ini_contact;
    
#ifdef ENABLE_STATS
    gettimeofday(&iph1->end, NULL);
    syslog(LOG_NOTICE, "%s(%s): %8.6f",
           "Phase 1", s_isakmp_etype(iph1->etype),
           timedelta(&iph1->start, &iph1->end));
#endif
    
#ifdef ENABLE_VPNCONTROL_PORT	
    
    if (iph1->side == RESPONDER &&
        iph1->local->ss_family == AF_INET) {
        
        struct redirect *addr;
        
        LIST_FOREACH(addr, &lcconf->redirect_addresses, chain) {
            if (((struct sockaddr_in *)iph1->local)->sin_addr.s_addr == addr->cluster_address) {
                vchar_t *raddr = vmalloc(sizeof(u_int32_t));
                
                if (raddr == NULL) {
                    plog(ASL_LEVEL_ERR,
                         "failed to send redirect message - memory error.\n");
                } else {
                    memcpy(raddr->v, &addr->redirect_address, sizeof(u_int32_t));
                    (void)isakmp_info_send_n1(iph1, ISAKMP_NTYPE_LOAD_BALANCE, raddr);
                    plog(ASL_LEVEL_DEBUG, "sent redirect notification - address = %x.\n", ntohl(addr->redirect_address));
                    vfree(raddr);
                    if (addr->force) {
                        (void)ike_session_update_ph1_ph2tree(iph1);
                        isakmp_ph1expire(iph1);
                    }
                }
            }
            return 0;
        }
    }
#endif 
    /* save created date. */
    (void)time(&iph1->created);
    
    /* add to the schedule to expire, and save back pointer. */
    iph1->sce = sched_new(iph1->approval->lifetime,
                          isakmp_ph1expire_stub, iph1);
    
    if (iph1->rmconf->initiate_ph1rekey) {
        if (iph1->side == INITIATOR) {
            spi_cmp = memcmp(&iph1->index.i_ck, &iph1->index.r_ck, sizeof(iph1->index.i_ck));
            if (spi_cmp == 0)
                spi_cmp = 1;
        } else {
            spi_cmp = memcmp(&iph1->index.r_ck, &iph1->index.i_ck, sizeof(iph1->index.r_ck));
            if (spi_cmp == 0)
                spi_cmp = -1;
        }
        rekey_lifetime = ike_session_get_rekey_lifetime((spi_cmp > 0),
                                                        iph1->approval->lifetime);
        if (rekey_lifetime) {
            iph1->sce_rekey = sched_new(rekey_lifetime,
                                        isakmp_ph1rekeyexpire_stub,
                                        iph1);
        } else {
            /* iph1->approval->lifetime is too small (e.g. 1) so why bother?
             * LOG ERROR
             */
            plog(ASL_LEVEL_ERR,
                 "failed to get rekey timer - lifetime is too small... probably.\n");
        }
    }
    
#ifdef ENABLE_HYBRID
    /* ignore xauth if it is a rekey */
    if (!iph1->is_rekey &&
        iph1->mode_cfg->flags & ISAKMP_CFG_VENDORID_XAUTH) {
        switch(AUTHMETHOD(iph1)) {
            case OAKLEY_ATTR_AUTH_METHOD_HYBRID_RSA_R:
            case OAKLEY_ATTR_AUTH_METHOD_XAUTH_PSKEY_R:
            case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSASIG_R:
            case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAENC_R:
            case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAREV_R:
                xauth_sendreq(iph1);
                /* XXX Don't process INITIAL_CONTACT */
                ini_contact = 0;
                break;
            default:
                break;
        }
    }
#endif
#ifdef ENABLE_DPD
    /* Schedule the r_u_there.... */
    if(iph1->dpd_support && iph1->rmconf->dpd_interval)
        isakmp_sched_r_u(iph1, 0);
#endif
    
    /* INITIAL-CONTACT processing */
    /* ignore initial-contact if it is a rekey */
    /* don't send anything if local test mode. */
    if (!iph1->is_rekey && !f_local && ini_contact && !ike_session_getcontacted(iph1->remote)) {
        /* send INITIAL-CONTACT */
        isakmp_info_send_n1(iph1,
                            ISAKMP_NTYPE_INITIAL_CONTACT, NULL);
        /* insert a node into contacted list. */
        if (ike_session_inscontacted(iph1->remote) == -1) {
            plog(ASL_LEVEL_ERR,
                 "failed to add contacted list.\n");
            /* ignore */
        }
    }
    
    log_ph1established(iph1);
    plog(ASL_LEVEL_DEBUG, "===\n");
    
    ike_session_cleanup_other_established_ph1s(iph1->parent_session, iph1);
    
#ifdef ENABLE_VPNCONTROL_PORT
    vpncontrol_notify_phase_change(0, FROM_LOCAL, iph1, NULL);
    vpncontrol_notify_peer_resp_ph1(1, iph1);
#endif
    
    return 0;
}


/*
 * parse ISAKMP payloads, without ISAKMP base header.
 */
vchar_t *
isakmp_parsewoh(np0, gen, len)
	int np0;
	struct isakmp_gen *gen;
	int len;
{
	u_char np = np0 & 0xff;
	int tlen, plen;
	vchar_t *result;
	struct isakmp_parse_t *p, *ep;

	plog(ASL_LEVEL_DEBUG, "begin.\n");

	/*
	 * 5 is a magic number, but any value larger than 2 should be fine
	 * as we do vrealloc() in the following loop.
	 */
	result = vmalloc(sizeof(struct isakmp_parse_t) * 5);
	if (result == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to get buffer.\n");
		return NULL;
	}                   
                                // Wcast-align fix (void*) - result = aligned buffer of struct isakmp_parse_t
	p = ALIGNED_CAST(struct isakmp_parse_t *)result->v;              
	ep = ALIGNED_CAST(struct isakmp_parse_t *)(result->v + result->l - sizeof(*ep));

	tlen = len;

	/* parse through general headers */
	while (0 < tlen && np != ISAKMP_NPTYPE_NONE) {
		if (tlen <= sizeof(struct isakmp_gen)) {
			/* don't send information, see isakmp_ident_r1() */
			plog(ASL_LEVEL_ERR, 
				"invalid length of payload\n");
			vfree(result);
			return NULL;
		}

		plog(ASL_LEVEL_DEBUG, 
			"seen nptype=%u(%s)\n", np, s_isakmp_nptype(np));

		p->type = np;
		p->len = ntohs(gen->len);
		if (p->len < sizeof(struct isakmp_gen) || p->len > tlen) {
			plog(ASL_LEVEL_DEBUG, 
				"invalid length of payload\n");
			vfree(result);
			return NULL;
		}
		p->ptr = gen;
		p++;
		if (ep <= p) {
			int off;

			off = p - ALIGNED_CAST(struct isakmp_parse_t *)result->v;
			result = vrealloc(result, result->l * 2);
			if (result == NULL) {
				plog(ASL_LEVEL_DEBUG, 
					"failed to realloc buffer.\n");
				vfree(result);
				return NULL;
			}
			ep = ALIGNED_CAST(struct isakmp_parse_t *)                  
				(result->v + result->l - sizeof(*ep));
			p = ALIGNED_CAST(struct isakmp_parse_t *)result->v;
			p += off;
		}

		np = gen->np;
		plen = ntohs(gen->len);
		gen = (struct isakmp_gen *)((caddr_t)gen + plen);
		tlen -= plen;
	}
	p->type = ISAKMP_NPTYPE_NONE;
	p->len = 0;
	p->ptr = NULL;

	plog(ASL_LEVEL_DEBUG, "succeed.\n");

	return result;
}

/*
 * parse ISAKMP payloads, including ISAKMP base header.
 */
vchar_t *
isakmp_parse(buf)
	vchar_t *buf;
{
	struct isakmp *isakmp = (struct isakmp *)buf->v;
	struct isakmp_gen *gen;
	int tlen;
	vchar_t *result;
	u_char np;

	np = isakmp->np;
	gen = (struct isakmp_gen *)(buf->v + sizeof(*isakmp));
	tlen = buf->l - sizeof(struct isakmp);
	result = isakmp_parsewoh(np, gen, tlen);

	return result;
}

int
isakmp_init(void)
{

	ike_session_initctdtree();
	ike_session_init_recvdpkt();

	if (isakmp_open() < 0)
		goto err;

	return(0);

err:
	isakmp_close();
	return(-1);
}

void
isakmp_cleanup()
{
	ike_session_clear_recvdpkt();
	ike_session_clear_contacted();
}

/*
 * make strings containing i_cookie + r_cookie + msgid
 */
const char *
isakmp_pindex(index, msgid)
	const isakmp_index *index;
	const u_int32_t msgid;
{
	static char buf[64];
	const u_char *p;
	int i, j;

	memset(buf, 0, sizeof(buf));

	/* copy index */
	p = (const u_char *)index;
	for (j = 0, i = 0; i < sizeof(isakmp_index); i++) {
		snprintf((char *)&buf[j], sizeof(buf) - j, "%02x", p[i]);
		j += 2;
		switch (i) {
		case 7:
			buf[j++] = ':';
		}
	}

	if (msgid == 0)
		return buf;

	/* copy msgid */
	snprintf((char *)&buf[j], sizeof(buf) - j, ":%08x", ntohs(msgid));

	return buf;
}


/* open ISAKMP sockets. */
int
isakmp_open(void)
{
	const int yes = 1;
	int ifnum = 0, encap_ifnum = 0;
#ifdef INET6
	int pktinfo;
#endif
	struct myaddrs *p;
    int tentative_failures = 0;
    int s;
    
	for (p = lcconf->myaddrs; p; p = p->next) {
		if (!p->addr)
			continue;
		if (p->sock != -1) {
			ifnum++;
			if (p->udp_encap)
				encap_ifnum++;
			continue;		// socket already open	
		}

		/* warn if wildcard address - should we forbid this? */
		switch (p->addr->ss_family) {
		case AF_INET:
			if (((struct sockaddr_in *)p->addr)->sin_addr.s_addr == 0)
				plog(ASL_LEVEL_WARNING, 
					"listening to wildcard address,"
					"broadcast IKE packet may kill you\n");
			break;
#ifdef INET6
		case AF_INET6:
			if (IN6_IS_ADDR_UNSPECIFIED(&((struct sockaddr_in6 *)p->addr)->sin6_addr))
				plog(ASL_LEVEL_WARNING, 
					"listening to wildcard address, "
					"broadcast IKE packet may kill you\n");
			break;
#endif
		default:
			plog(ASL_LEVEL_ERR, 
				"unsupported address family %d\n",
				lcconf->default_af);
			goto err_and_next;
		}

#ifdef INET6
		if (p->addr->ss_family == AF_INET6 &&
		    IN6_IS_ADDR_MULTICAST(&((struct sockaddr_in6 *)
					    p->addr)->sin6_addr))
		{
			plog(ASL_LEVEL_DEBUG, 
				"Ignoring multicast address %s\n",
				saddr2str((struct sockaddr *)p->addr));
				racoon_free(p->addr);
				p->addr = NULL;
			continue;
		}
#endif

		if ((p->sock = socket(p->addr->ss_family, SOCK_DGRAM, 0)) < 0) {
			plog(ASL_LEVEL_ERR, 
				"socket (%s)\n", strerror(errno));
			goto err_and_next;
		}

		if (fcntl(p->sock, F_SETFL, O_NONBLOCK) == -1)
			plog(ASL_LEVEL_ERR, "failed to put socket in non-blocking mode\n");

		/* receive my interface address on inbound packets. */
		switch (p->addr->ss_family) {
		case AF_INET:
			if (setsockopt(p->sock, IPPROTO_IP,
				       IP_RECVDSTADDR,
					(const void *)&yes, sizeof(yes)) < 0) {
				plog(ASL_LEVEL_ERR, 
					"setsockopt IP_RECVDSTADDR (%s)\n", 
					strerror(errno));
				goto err_and_next;
			}
			break;
#ifdef INET6
		case AF_INET6:
#ifdef INET6_ADVAPI
			pktinfo = IPV6_RECVPKTINFO;
#else
			pktinfo = IPV6_RECVDSTADDR;
#endif
			if (setsockopt(p->sock, IPPROTO_IPV6, pktinfo,
					(const void *)&yes, sizeof(yes)) < 0)
			{
				plog(ASL_LEVEL_ERR, 
					"setsockopt IPV6_RECVDSTADDR (%d):%s\n",
					pktinfo, strerror(errno));
				goto err_and_next;
			}
			break;
#endif
		}

#ifdef IPV6_USE_MIN_MTU
		if (p->addr->ss_family == AF_INET6 &&
		    setsockopt(p->sock, IPPROTO_IPV6, IPV6_USE_MIN_MTU,
		    (void *)&yes, sizeof(yes)) < 0) {
			plog(ASL_LEVEL_ERR, 
			    "setsockopt IPV6_USE_MIN_MTU (%s)\n", 
			    strerror(errno));
			return -1;
		}
#endif

		if (setsockopt_bypass(p->sock, p->addr->ss_family) < 0)
			goto err_and_next;

		if (extract_port(p->addr) == PORT_ISAKMP) {
			if (setsockopt(p->sock, SOL_SOCKET, SO_NOTIFYCONFLICT, 
				(void *)&yes, sizeof(yes)) < 0) {
				plog(ASL_LEVEL_ERR,
					"setsockopt (%s)\n", strerror(errno));
				goto err_and_next;
			}
		}

		if (bind(p->sock, (struct sockaddr *)p->addr, sysdep_sa_len((struct sockaddr *)p->addr)) < 0) {
			int tmp_errno = errno;
			plog(ASL_LEVEL_ERR,
				"failed to bind to address %s (%s).\n",
				saddr2str((struct sockaddr *)p->addr), strerror(tmp_errno));
#ifdef INET6
			// if bind failed b/c of a tentative v6 address, try again later
			if (tmp_errno == EADDRNOTAVAIL && p->addr->ss_family == AF_INET6) {
				struct in6_ifreq ifr6;

				bzero(&ifr6, sizeof(ifr6));
				strlcpy(ifr6.ifr_name, p->ifname, sizeof(ifr6.ifr_name));
				memcpy(&ifr6.ifr_addr, p->addr, sizeof(ifr6.ifr_addr));
				if (ioctl(p->sock, SIOCGIFAFLAG_IN6, &ifr6) >= 0) {
					/*
					 * the tentative flag may have cleared between the bind() and ioctl() calls (i.e due to timing), so 
					 * try infering that it was tentative from ensuring the absense other cases of EADDRNOTAVAIL.
					 */
					if ((ifr6.ifr_ifru.ifru_flags6 & (IN6_IFF_ANYCAST | IN6_IFF_DUPLICATED | IN6_IFF_DETACHED | IN6_IFF_DEPRECATED)) == 0) {
						// address may have been tentantive... invalidate sock but leave address around for another try later
						plog(ASL_LEVEL_ERR,
							 "failed to bind to address %s: because interface address is/was not ready (flags %x).\n",
							 saddr2str((struct sockaddr *)p->addr), ifr6.ifr_ifru.ifru_flags6);
						close(p->sock);
						p->sock = -1;
						tentative_failures = 1;
						continue;
					} else {
						plog(ASL_LEVEL_ERR,
							 "failed to bind to address %s: because of interface address error, flags %x.\n",
							 saddr2str((struct sockaddr *)p->addr), ifr6.ifr_ifru.ifru_flags6);
					}
				} else {
					plog(ASL_LEVEL_ERR,
						 "failed to bind to address %s: can't read interface address flags.\n",
						 saddr2str((struct sockaddr *)p->addr));
				}
			}
#endif
			close(p->sock);
			p->sock = -1;
			goto err_and_next;
		}
		
		ifnum++;
		if (p->udp_encap)
			encap_ifnum++;
        
        s = p->sock;
        
        if (p->source != NULL) {
            dispatch_source_cancel(p->source);
            p->source = NULL;
        }
        p->source = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, p->sock, 0, dispatch_get_main_queue());
        if (p->source == NULL) {
            plog(ASL_LEVEL_ERR, "could not create isakmp socket source.");
            return -1;
        }
        dispatch_source_set_event_handler(p->source, 
                                          ^{
                                              isakmp_handler(s);
                                          });
        dispatch_source_t the_source = p->source;
        dispatch_source_set_cancel_handler(p->source,
                                           ^{
                                               close(s);
                                               dispatch_release(the_source);
                                           });        dispatch_resume(p->source);

		plog(ASL_LEVEL_INFO, 
			"%s used as isakmp port (fd=%d)\n",
			saddr2str((struct sockaddr *)p->addr), p->sock);
		continue;

	err_and_next:
		racoon_free(p->addr);
		p->addr = NULL;
        p->sock = -1;
		if (! lcconf->autograbaddr && lcconf->strict_address) {
			return -1;
        }
        
		continue;
	}

	if (!ifnum) {
		plog(ASL_LEVEL_ERR, 
			"no address could be bound.\n");

		return -1;
	}

#ifdef ENABLE_NATT
	if (natt_enabled_in_rmconf() && !encap_ifnum) {
		plog(ASL_LEVEL_WARNING, 
			"NAT-T is enabled in at least one remote{} section,\n");
		plog(ASL_LEVEL_WARNING, 
			"but no 'isakmp_natt' address was specified!\n");
	}
#endif

    if (tentative_failures)
        sched_new(5, update_myaddrs, NULL);

	return 0;
}

void
isakmp_suspend_sockets()
{
	struct myaddrs *p;
    
	for (p = lcconf->myaddrs; p; p = p->next) {
        if (p->source)
            dispatch_suspend(p->source);
	}
}

void
isakmp_close()
{
	isakmp_close_sockets();
	clear_myaddr();
}

void
isakmp_close_sockets()
{
	struct myaddrs *p;

	for (p = lcconf->myaddrs; p; p = p->next) {

		if (!p->addr)
			continue;

        if (p->source) {
            dispatch_source_cancel(p->source);
            p->source = NULL;
            p->in_use = 0;
            p->sock = -1;
        }
	}
}


// close sockets for addresses that have gone away
void
isakmp_close_unused()
{
	struct myaddrs *p, *next, **prev;
	
	prev = &(lcconf->myaddrs);
	for (p = lcconf->myaddrs; p; p = next) {
		next = p->next;
		if (p->in_use == 0) { 	// not in use ?
            if (p->source) {    
                dispatch_source_cancel(p->source);
                p->source = NULL;
            }
            *prev = p->next;
			delmyaddr(p);
		} else
            prev = &(p->next);
	}
}

int
isakmp_send(iph1, sbuf)
	phase1_handle_t *iph1;
	vchar_t *sbuf;
{
	int len = 0;
	int s;
	vchar_t *vbuf = NULL;
#ifdef ENABLE_NATT
	size_t extralen = NON_ESP_MARKER_USE(iph1) ? NON_ESP_MARKER_LEN : 0;

#ifdef ENABLE_FRAG
	/* 
	 * Do not add the non ESP marker for a packet that will
	 * be fragmented. The non ESP marker should appear in 
	 * all fragment's packets, but not in the fragmented packet
	 */
	if (iph1->frag && sbuf->l > ISAKMP_FRAG_MAXLEN) 
		extralen = 0;
#endif
	if (extralen)
		plog (ASL_LEVEL_DEBUG, "Adding NON-ESP marker\n");

	/* If NAT-T port floating is in use, 4 zero bytes (non-ESP marker) 
	   must added just before the packet itself. For this we must 
	   allocate a new buffer and release it at the end. */
	if (extralen) {
		if ((vbuf = vmalloc (sbuf->l + extralen)) == NULL) {
			plog(ASL_LEVEL_ERR, 
			    "vbuf allocation failed\n");
			return -1;
		}
		*ALIGNED_CAST(u_int32_t *)vbuf->v = 0;
		memcpy (vbuf->v + extralen, sbuf->v, sbuf->l);
		sbuf = vbuf;
	}
#endif

	/* select the socket to be sent */
	s = getsockmyaddr((struct sockaddr *)iph1->local);
	if (s == -1){
		if ( vbuf != NULL )
			vfree(vbuf);
		return -1;
	}

	plog (ASL_LEVEL_DEBUG, "%zu bytes %s\n", sbuf->l, 
	      saddr2str_fromto("from %s to %s", (struct sockaddr *)iph1->local, (struct sockaddr *)iph1->remote));

#ifdef ENABLE_FRAG
	if (iph1->frag && sbuf->l > ISAKMP_FRAG_MAXLEN) {
		if (isakmp_sendfrags(iph1, sbuf) == -1) {
			plog(ASL_LEVEL_ERR, 
			    "isakmp_sendfrags failed\n");
			if ( vbuf != NULL )
				vfree(vbuf);
			return -1;
		}
	} else 
#endif
	{
		len = sendfromto(s, sbuf->v, sbuf->l,
		    iph1->local, iph1->remote, lcconf->count_persend);
		if (len == -1) {
			plog(ASL_LEVEL_ERR, "sendfromto failed\n");
			if ( vbuf != NULL )
				vfree(vbuf);
			return -1;
		}
	}
	
	if ( vbuf != NULL )
		vfree(vbuf);
	
	return 0;
}

/* called from scheduler */
void
isakmp_ph1resend_stub(p)
	void *p;
{
	phase1_handle_t *iph1;

	iph1=(phase1_handle_t *)p;
	if(isakmp_ph1resend(iph1) < 0){
		if(iph1->scr != 0){
			/* Should not happen...
			 */
			SCHED_KILL(iph1->scr);
		}

		ike_session_unlink_phase1(iph1);
	}
}

int
isakmp_ph1resend(iph1)
	phase1_handle_t *iph1;
{
	time_t retry_interval;
	
    
	// make sure there is a buffer to send
	// isakmp_plist_set_all() could have returned NULL
	if (iph1->sendbuf == NULL)
		return -1;

	/* Note: NEVER do the rem/del here, it will be done by the caller or by the _stub function
	 */
	if (iph1->retry_counter <= 0) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKEV1_PH1_MAX_RETRANSMIT,
								CONSTSTR("Phase 1 Maximum Retransmits"),
								CONSTSTR("Phase 1 negotiation failed (Maximum retransmits)"));

		plog(ASL_LEVEL_ERR, 
			"Phase 1 negotiation failed due to time up. %s\n",
			isakmp_pindex(&iph1->index, iph1->msgid));
		if (iph1->side == INITIATOR && iph1->is_rekey && iph1->parent_session && iph1->parent_session->is_client) {
			/* to get around a bug on the peer, in which rekeys to port 4500 are dropped */
			if (isakmp_ph1rekeyretry(iph1) == 0)
				return 0;
		}
		return -1;
	} else {
		ike_session_ph1_retransmits(iph1);
	}

	if (isakmp_send(iph1, iph1->sendbuf) < 0){
		if (iph1->rmconf->retry_counter != iph1->retry_counter) {
			IPSECSESSIONTRACEREVENT(iph1->parent_session,
									IPSECSESSIONEVENTCODE_IKE_PACKET_TX_FAIL,
									CONSTSTR("Phase 1 Retransmit"),
									CONSTSTR("Failed to retrasmit Phase1"));
		}
		plog(ASL_LEVEL_ERR,
			 "Phase 1 negotiation failed due to send error. %s\n",
			 isakmp_pindex(&iph1->index, iph1->msgid));
		return -1;
	}

	if (iph1->rmconf->retry_counter != iph1->retry_counter) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKE_PACKET_TX_SUCC,
								CONSTSTR("Phase 1 Retransmit"),
								CONSTSTR(NULL));
	}

	plog(ASL_LEVEL_DEBUG,
		"Resend Phase 1 packet %s\n",
		isakmp_pindex(&iph1->index, iph1->msgid));

	iph1->retry_counter--;
	retry_interval = ike_session_get_exp_retx_interval((iph1->rmconf->retry_counter - iph1->retry_counter),
										   iph1->rmconf->retry_interval);
	iph1->scr = sched_new(retry_interval,
		isakmp_ph1resend_stub, iph1);

	return 0;
}

/* called from scheduler */
void
isakmp_ph2resend_stub(p)
	void *p;
{
	phase2_handle_t *iph2;

	iph2=(phase2_handle_t *)p;

	if(isakmp_ph2resend(iph2) < 0){
		ike_session_unlink_phase2(iph2);
	}
}

int
isakmp_ph2resend(iph2)
	phase2_handle_t *iph2;
{
	time_t retry_interval;

	/* Note: NEVER do the unbind/rem/del here, it will be done by the caller or by the _stub function
	 */
	//%%% BUG FIX - related to commit bit usage - crash happened here
	if (iph2->ph1 == 0) {
		plog(ASL_LEVEL_ERR, 
			"Internal error - attempt to re-send Phase 2 with no Phase 1 bound.\n");
		return -1;
	}

    
	if (FSM_STATE_IS_EXPIRED(iph2->ph1->status)){
		IPSECSESSIONTRACEREVENT(iph2->ph1->parent_session,
								IPSECSESSIONEVENTCODE_IKEV1_PH2_MAX_RETRANSMIT,
								CONSTSTR("Underlying Phase 1 expired"),
								CONSTSTR("Failed to retransmit Phase 2 (underlying Phase 1 expired)"));
		plog(ASL_LEVEL_ERR,
			"Phase 2 negotiation failed due to Phase 1 expired. %s\n",
				isakmp_pindex(&iph2->ph1->index, iph2->msgid));
		return -1;
	}

	if (iph2->retry_counter <= 0) {
		IPSECSESSIONTRACEREVENT(iph2->ph1->parent_session,
								IPSECSESSIONEVENTCODE_IKEV1_PH2_MAX_RETRANSMIT,
								CONSTSTR("Phase 2 maximum retransmits"),
								CONSTSTR("Phase 2 negotiation failed (maximum retransmits)"));
		plog(ASL_LEVEL_ERR,
			"Phase 2 negotiation failed due to time up. %s\n",
				isakmp_pindex(&iph2->ph1->index, iph2->msgid));
		return -1;
	} else {
		ike_session_ph2_retransmits(iph2);
	}

	if (isakmp_send(iph2->ph1, iph2->sendbuf) < 0){
		if (iph2->ph1->rmconf->retry_counter != iph2->retry_counter) {
			IPSECSESSIONTRACEREVENT(iph2->ph1->parent_session,
									IPSECSESSIONEVENTCODE_IKE_PACKET_TX_FAIL,
									CONSTSTR("Phase 2 Retransmit"),
									CONSTSTR("Failed to retransmit Phase2 message"));
		}
		plog(ASL_LEVEL_ERR, 
			"Phase 2 negotiation failed due to send error. %s\n",
				isakmp_pindex(&iph2->ph1->index, iph2->msgid));

		return -1;
	}
	if (iph2->ph1->rmconf->retry_counter != iph2->retry_counter) {
		IPSECSESSIONTRACEREVENT(iph2->ph1->parent_session,
								IPSECSESSIONEVENTCODE_IKE_PACKET_TX_SUCC,
								CONSTSTR("Phase 2 Retransmit"),
								CONSTSTR(NULL));
	}

	plog(ASL_LEVEL_DEBUG, 
		"Resend Phase 2 packet %s\n",
		isakmp_pindex(&iph2->ph1->index, iph2->msgid));

	iph2->retry_counter--;
	retry_interval = ike_session_get_exp_retx_interval((iph2->ph1->rmconf->retry_counter - iph2->ph1->retry_counter),
										   iph2->ph1->rmconf->retry_interval);
	iph2->scr = sched_new(retry_interval,
		isakmp_ph2resend_stub, iph2);

#ifdef ENABLE_DPD
    if (iph2->scr) {
        isakmp_reschedule_info_monitor_if_pending(iph2->ph1,
                                                  "Phase 2 packets sent to peer: retransmit timer armed");
    }
#endif /* DPD */
    
	return 0;
}


/* called from scheduler */
void
isakmp_ph1expire_stub(p)
	void *p;
{

	isakmp_ph1expire((phase1_handle_t *)p);
}

void
isakmp_ph1expire(iph1)
	phase1_handle_t *iph1;
{
	char *src, *dst;
	phase1_handle_t *new_iph1;

	SCHED_KILL(iph1->sce);
#ifdef ENABLE_DPD
    SCHED_KILL(iph1->dpd_r_u);
#endif

	if(!FSM_STATE_IS_EXPIRED(iph1->status)){
		src = racoon_strdup(saddr2str((struct sockaddr *)iph1->local));
		dst = racoon_strdup(saddr2str((struct sockaddr *)iph1->remote));
		STRDUP_FATAL(src);
		STRDUP_FATAL(dst);

		plog(ASL_LEVEL_INFO, 
			 "ISAKMP-SA expired %s-%s spi:%s\n",
			 src, dst,
			 isakmp_pindex(&iph1->index, 0));
		racoon_free(src);
		racoon_free(dst);
		fsm_set_state(&iph1->status, IKEV1_STATE_PHASE1_EXPIRED);
		new_iph1 = ike_session_update_ph1_ph2tree(iph1);
	}

	/*
	 * the phase1 deletion is postponed until there is no phase2.
	 */
	if (LIST_FIRST(&iph1->bound_ph2tree) != NULL) {
		iph1->sce = sched_new(1, isakmp_ph1expire_stub, iph1);
		return;
	}
    

	iph1->sce = sched_new(1, isakmp_ph1delete_stub, iph1);
}

/* called from scheduler */
void
isakmp_ph1rekeyexpire_stub(p)
void *p;
{
	
	isakmp_ph1rekeyexpire((phase1_handle_t *)p, FALSE);
}

void
isakmp_ph1rekeyexpire(iph1, ignore_sess_drop_policy)
phase1_handle_t *iph1;
int               ignore_sess_drop_policy;
{
	char              *src, *dst;
	struct remoteconf *rmconf;

	SCHED_KILL(iph1->sce_rekey);

	// We are going to start the rekey.  Let's fire off the
	// phase1 expiration timer if it is not done yet.
	if (!iph1->sce && iph1->approval->lifetimegap) {
		iph1->sce = sched_new(iph1->approval->lifetimegap,
                              isakmp_ph1expire_stub, iph1);
	}
    
	// early exit if iph2->sce == NULL, iph2 isn't established or if entire session is going down
	if (iph1->sce == 0 ||
		!FSM_STATE_IS_ESTABLISHED(iph1->status) ||
		iph1->is_dying) {
		return;
	}

	src = racoon_strdup(saddr2str((struct sockaddr *)iph1->local));
	dst = racoon_strdup(saddr2str((struct sockaddr *)iph1->remote));
	STRDUP_FATAL(src);
	STRDUP_FATAL(dst);

	plog(ASL_LEVEL_INFO, 
		 "ISAKMP-SA rekey-timer expired %s-%s spi:%s\n",
		 src, dst,
		 isakmp_pindex(&iph1->index, 0));
	racoon_free(src);
	racoon_free(dst);

    {
        if (!ignore_sess_drop_policy && ike_session_drop_rekey(iph1->parent_session, IKE_SESSION_REKEY_TYPE_PH1)) {
            return;
        }
	}

	// exit if there is another ph1 that is established (with a pending rekey timer)
	if (ike_session_has_other_established_ph1(iph1->parent_session, iph1)) {
		plog(ASL_LEVEL_INFO,
			 "Request for ISAKMP-SA rekey was ignored "
			 "due to another established ph1.\n");
		return;
	}

	// if there is another ph1 that is negotiating, postpone this rekey for a few seconds later
	if (ike_session_has_other_negoing_ph1(iph1->parent_session, iph1)) {
		plog(ASL_LEVEL_DEBUG, "Reschedule Phase 1 rekey.\n");
		iph1->sce_rekey = sched_new(1,
									isakmp_ph1rekeyexpire_stub,
									iph1);
		return;
	}

    // get rmconf to initiate rekey with
    rmconf = iph1->rmconf;
    if (!rmconf)
        rmconf = getrmconf(iph1->remote);

    if (rmconf) {
        /* begin quick mode */
        plog(ASL_LEVEL_DEBUG, "Begin Phase 1 rekey.\n");

		/* start phase 1 negotiation as a initiator. */
        {
			if (ikev1_ph1begin_i(iph1->parent_session, rmconf, iph1->remote, iph1->local, 0) < 0) {
				plog(ASL_LEVEL_DEBUG, "Phase 1 rekey Failed.\n");
			}
            iph1->is_rekey = TRUE;
		}
	} else {
		plog(ASL_LEVEL_ERR,
			 "Phase1 rekey failed: no configuration found for %s.\n",
			 saddrwop2str((struct sockaddr *)iph1->remote));
	}
}

int
isakmp_ph1rekeyretry(iph1)
phase1_handle_t *iph1;
{
	char              *src, *dst;
	struct remoteconf *rmconf;

	// this code path is meant for floated ph1 rekeys that are failing on the first message
	if (iph1->sce != 0 ||
		iph1->sce_rekey != 0 ||
		((iph1->status != IKEV1_STATE_IDENT_I_MSG1SENT  && 
          iph1->status != IKEV1_STATE_AGG_I_MSG1SENT)
            || ((iph1->natt_flags & NAT_PORTS_CHANGED) == 0)) 
            || (extract_port(iph1->local) != PORT_ISAKMP_NATT && extract_port(iph1->remote) != PORT_ISAKMP_NATT) 
            || iph1->is_dying) {
		return -1;
	}

	src = racoon_strdup(saddr2str((struct sockaddr *)iph1->local));
	dst = racoon_strdup(saddr2str((struct sockaddr *)iph1->remote));
	STRDUP_FATAL(src);
	STRDUP_FATAL(dst);

	plog(ASL_LEVEL_INFO, 
		 "ISAKMP-SA rekey failed... retrying %s-%s spi:%s\n",
		 src, dst,
		 isakmp_pindex(&iph1->index, 0));
	racoon_free(src);
	racoon_free(dst);

	if (ike_session_drop_rekey(iph1->parent_session, IKE_SESSION_REKEY_TYPE_PH1)) {
		plog(ASL_LEVEL_INFO,
			 "request for ISAKMP-SA rekey was ignored "
			 "due to idleness.\n");
		return 0;
	}

	// exit if there is another ph1 that is established (with a pending rekey timer)
	if (ike_session_has_other_established_ph1(iph1->parent_session, iph1)) {
		plog(ASL_LEVEL_INFO,
			 "request to retry ISAKMP-SA rekey was ignored "
			 "due to another established ph1.\n");
		return -1;
	}

	// some servers don't like respond to 4500 for rekeys... try accomodate them
	if (extract_port(iph1->local) == PORT_ISAKMP_NATT) {
		set_port(iph1->local, PORT_ISAKMP);
	}
	if (extract_port(iph1->remote) == PORT_ISAKMP_NATT) {
		set_port(iph1->remote, PORT_ISAKMP);
	}
	iph1->natt_flags &= ~NAT_PORTS_CHANGED;
	rmconf = getrmconf(iph1->remote);
	if (rmconf) {
		/* begin quick mode */
		plog(ASL_LEVEL_DEBUG, "begin Phase 1 rekey retry.\n");

		/* start phase 1 negotiation as a initiator. */
		if (ikev1_ph1begin_i(iph1->parent_session, rmconf, iph1->remote, iph1->local, 0) < 0) {
			plog(ASL_LEVEL_DEBUG, "Phase 1 rekey retry Failed.\n");
			return -1;
		}
		iph1->is_rekey = TRUE;
	} else {
		plog(ASL_LEVEL_ERR,
			 "Phase 1 rekey retry failed: no configuration found for %s.\n",
			 saddrwop2str((struct sockaddr *)iph1->remote));
		return -1;
	}
	return 0;
}

/* called from scheduler */
void
isakmp_ph1delete_stub(p)
	void *p;
{

	isakmp_ph1delete((phase1_handle_t *)p);
}

void
isakmp_ph1delete(iph1)
	phase1_handle_t *iph1;
{
	char *src, *dst;

	SCHED_KILL(iph1->sce);
	SCHED_KILL(iph1->sce_rekey);
#ifdef ENABLE_DPD
    SCHED_KILL(iph1->dpd_r_u);
#endif    

	if (LIST_FIRST(&iph1->bound_ph2tree) != NULL) {
		iph1->sce = sched_new(1, isakmp_ph1delete_stub, iph1);
		return;
	}
    
    isakmp_info_send_d1(iph1);

	/* don't re-negosiation when the phase 1 SA expires. */

	src = racoon_strdup(saddr2str((struct sockaddr *)iph1->local));
	dst = racoon_strdup(saddr2str((struct sockaddr *)iph1->remote));
	STRDUP_FATAL(src);
	STRDUP_FATAL(dst);

	plog(ASL_LEVEL_INFO, 
		"ISAKMP-SA deleted %s-%s spi:%s\n",
		src, dst, isakmp_pindex(&iph1->index, 0));
	racoon_free(src);
	racoon_free(dst);
    
	ike_session_unlink_phase1(iph1);

	return;
}

/* called from scheduler.
 * this function will call only isakmp_ph2delete().
 * phase 2 handler remain forever if kernel doesn't cry a expire of phase 2 SA
 * by something cause.  That's why this function is called after phase 2 SA
 * expires in the userland.
 */
void
isakmp_ph2expire_stub(p)
	void *p;
{

	isakmp_ph2expire((phase2_handle_t *)p);
}

void
isakmp_ph2expire(iph2)
	phase2_handle_t *iph2;
{
	char *src, *dst;

	if (FSM_STATE_IS_EXPIRED(iph2->status)) {
		return;
	}

	SCHED_KILL(iph2->sce);

	src = racoon_strdup(saddrwop2str((struct sockaddr *)iph2->src));
	dst = racoon_strdup(saddrwop2str((struct sockaddr *)iph2->dst));
	STRDUP_FATAL(src);
	STRDUP_FATAL(dst);

	plog(ASL_LEVEL_INFO, 
		"Phase 2 sa expired %s-%s\n", src, dst);
	racoon_free(src);
	racoon_free(dst);

	// delete outgoing SAs
	if (FSM_STATE_IS_ESTABLISHED(iph2->status) && iph2->approval) {
		struct saproto *pr;

		for (pr = iph2->approval->head; pr != NULL; pr = pr->next) {
			if (pr->ok) {
				pfkey_send_delete(lcconf->sock_pfkey,
								  ipsecdoi2pfkey_proto(pr->proto_id),
								  IPSEC_MODE_ANY,
								  iph2->src, iph2->dst, pr->spi_p /* pr->reqid_out */);
		}
	}
    if (iph2->version == ISAKMP_VERSION_NUMBER_IKEV1)
        fsm_set_state(&iph2->status, IKEV1_STATE_PHASE2_EXPIRED);
	iph2->sce = sched_new(1, isakmp_ph2delete_stub, iph2);

	return;
}

/* called from scheduler */
void
isakmp_ph2delete_stub(p)
	void *p;
{

	isakmp_ph2delete((phase2_handle_t *)p);
}

void
isakmp_ph2delete(iph2)
	phase2_handle_t *iph2;
{
	char *src, *dst;

	SCHED_KILL(iph2->sce);

	src = racoon_strdup(saddrwop2str((struct sockaddr *)iph2->src));
	dst = racoon_strdup(saddrwop2str((struct sockaddr *)iph2->dst));
	STRDUP_FATAL(src);
	STRDUP_FATAL(dst);

	plog(ASL_LEVEL_INFO, 
		"Phase 2 sa deleted %s-%s\n", src, dst);
	racoon_free(src);
	racoon_free(dst);

	ike_session_unlink_phase2(iph2);

	return;
}

/* %%%
 * Interface between PF_KEYv2 and ISAKMP
 */
/*
 * receive ACQUIRE from kernel, and begin either phase1 or phase2.
 * if phase1 has been finished, begin phase2.
 */
int
isakmp_post_acquire(phase2_handle_t *iph2)
{
	struct remoteconf *rmconf;
	phase1_handle_t *iph1 = NULL;
	
	plog(ASL_LEVEL_DEBUG, "In post_acquire\n");

	/* search appropreate configuration with masking port. */
	rmconf = getrmconf(iph2->dst);
	if (rmconf == NULL) {
		plog(ASL_LEVEL_ERR, 
			"No configuration found for %s.\n",
			saddrwop2str((struct sockaddr *)iph2->dst));
		return -1;
	}
	/* if passive mode, ignore the acquire message */
	if (rmconf->passive) {
		plog(ASL_LEVEL_DEBUG, 
			"Because of passive mode, ignore the acquire message for %s.\n",
			saddrwop2str((struct sockaddr *)iph2->dst));
		return 0;
	}

    
    // what if there is another ph2 that is negotiating
    if (ike_session_has_other_negoing_ph2(iph2->parent_session, iph2)) {
        // TODO: postpone this rekey for a second later
        plog(ASL_LEVEL_INFO,
             "Request for establishing IPsec-SA was ignored due to another negoing ph2.\n");
        return -1;
    }

    // if this is a phase2 rekeys (the policy may not have the current port number).
    // so, use the appropriate ports.
    if (iph2->is_rekey) {
        ike_session_update_ph2_ports(iph2);
    }
    if (iph2->version == ISAKMP_VERSION_NUMBER_IKEV1)
        iph1 = ike_session_update_ph2_ph1bind(iph2);
	else
		iph1 = ike_session_get_established_or_negoing_ph1(iph2->parent_session);

	/* no IKE-SA found. */
	if (iph1 == NULL) {
		iph2->retry_checkph1 = lcconf->retry_checkph1;
        
		/* start phase 1 negotiation as a initiator. */
        {
            sched_new(1, isakmp_chkph1there_stub, iph2);
            
            plog(ASL_LEVEL_INFO,
                 "IPsec-SA request for %s queued due to no Phase 1 found.\n",
                 saddrwop2str((struct sockaddr *)iph2->dst));
            
            // exit if there is another ph1 that is established (with a pending rekey timer)
            if (ike_session_has_negoing_ph1(iph2->parent_session)) {
                plog(ASL_LEVEL_INFO,
                     "Request for Phase 1 was ignored due to another negotiating Phase 1.\n");
                return 0;
            }
            
			if (ikev1_ph1begin_i(iph2->parent_session, rmconf, iph2->dst, iph2->src, 0) < 0) {
				plog(ASL_LEVEL_INFO,
					 "Request for Phase 1 failed. Will try later.\n");
			}
		}
		return 0;
		/*NOTREACHED*/
	}
    

	/* found ISAKMP-SA, but on negotiation. */
	if (!FSM_STATE_IS_ESTABLISHED(iph1->status)) {
		iph2->retry_checkph1 = lcconf->retry_checkph1;
		sched_new(1, isakmp_chkph1there_stub, iph2);
		plog(ASL_LEVEL_INFO,
			"Request for establishing IPsec-SA was queued due to no phase1 found.\n");
		return 0;
		/*NOTREACHED*/
	}

	/* found established ISAKMP-SA */

	/* found ISAKMP-SA. */

	/* begin quick mode */
    {
		plog(ASL_LEVEL_DEBUG, "Begin QUICK mode.\n");
		if (ikev1_ph2begin_i(iph1, iph2))
			return -1;
	}
	return 0;
}

/*
 * receive GETSPI from kernel.
 */
int
isakmp_post_getspi(phase2_handle_t *iph2)
{
#ifdef ENABLE_STATS
	struct timeval start, end;
#endif
    int error = 0;
    
    /* don't process it because there is no suitable phase1-sa. */
    if (FSM_STATE_IS_EXPIRED(iph2->ph1->status)) {
        plog(ASL_LEVEL_ERR,
             "the negotiation is stopped, "
             "because there is no suitable ISAKMP-SA.\n");
        return -1;
    }
    fsm_set_state(&iph2->status, iph2->side == INITIATOR ? 
                  IKEV1_STATE_QUICK_I_GETSPIDONE : IKEV1_STATE_QUICK_R_GETSPIDONE);
    
#ifdef ENABLE_STATS
    gettimeofday(&start, NULL);
#endif
    switch (iph2->side) {
        case INITIATOR:
            error = quick_i1send(iph2, NULL);
            break;
        case RESPONDER:
            error = quick_r2send(iph2, NULL);
            break;
    }

	if (error)  //%%%%%%%% log something ???
		return -1;
#ifdef ENABLE_STATS
	gettimeofday(&end, NULL);
	plog(ASL_LEVEL_NOTICE, "%s(%s): %8.6f",
		"Phase 2",
		s_isakmp_state(ISAKMP_ETYPE_QUICK, iph2->side, iph2->status),
		timedelta(&start, &end));
#endif

	return 0;
}

/* called by scheduler */
void
isakmp_chkph1there_stub(p)
	void *p;
{
	isakmp_chkph1there((phase2_handle_t *)p);
}

void
isakmp_chkph1there(iph2)
	phase2_handle_t *iph2;
{
	phase1_handle_t *iph1;

	if ((iph2->version == ISAKMP_VERSION_NUMBER_IKEV1 && iph2->status != IKEV1_STATE_QUICK_I_START) ||
	    iph2->is_dying) {
		plog(ASL_LEVEL_DEBUG, "CHKPH1THERE: ph2 handle has advanced too far (status %d, START %d, dying %d)... ignoring\n", iph2->status, IKEV1_STATE_QUICK_I_START, iph2->is_dying);
		return;
	}

	iph2->retry_checkph1--;

	if (iph2->retry_checkph1 < 0 /* %%%%||
		ike_session_verify_ph2_parent_session(iph2) */) {
		if (iph2->retry_checkph1 < 0) {
			plog(ASL_LEVEL_ERR,
				 "Phase 2 negotiation failed "
				 "due to time up waiting for Phase 1. %s\n",
				 sadbsecas2str(iph2->dst, iph2->src,
							   iph2->satype, 0, 0));
		} else {
			plog(ASL_LEVEL_ERR,
				 "Phase 2 negotiation failed "
				 "due to invalid parent session. %s\n",
				 sadbsecas2str(iph2->dst, iph2->src,
							   iph2->satype, 0, 0));
		}
		plog(ASL_LEVEL_INFO, 
			"delete Phase 2 handler.\n");

		/* send acquire to kernel as error */
		pk_sendeacquire(iph2);
		ike_session_unlink_phase2(iph2);
		return;
	}

	iph1 = ike_session_update_ph2_ph1bind(iph2);

	/* XXX Even if ph1 as responder is there, should we not start
	 * phase 2 negotiation ? */
	if (iph1 != NULL
	 && FSM_STATE_IS_ESTABLISHED(iph1->status)) {
		/* found isakmp-sa */

		plog(ASL_LEVEL_DEBUG, "CHKPH1THERE: got a ph1 handler, setting ports.\n");
		plog(ASL_LEVEL_DEBUG, "iph1->local: %s\n", saddr2str((struct sockaddr *)iph1->local));
		plog(ASL_LEVEL_DEBUG, "iph1->remote: %s\n", saddr2str((struct sockaddr *)iph1->remote));
		plog(ASL_LEVEL_DEBUG, "before:\n");
		plog(ASL_LEVEL_DEBUG, "src: %s\n", saddr2str((struct sockaddr *)iph2->src));
		plog(ASL_LEVEL_DEBUG, "dst: %s\n", saddr2str((struct sockaddr *)iph2->dst));
		set_port(iph2->src, extract_port(iph1->local));
		set_port(iph2->dst, extract_port(iph1->remote));
		plog(ASL_LEVEL_DEBUG, "After:\n");
		plog(ASL_LEVEL_DEBUG, "src: %s\n", saddr2str((struct sockaddr *)iph2->src));
		plog(ASL_LEVEL_DEBUG, "dst: %s\n", saddr2str((struct sockaddr *)iph2->dst));

		/* begin quick mode */
        {
			if (ikev1_ph2begin_i(iph1, iph2)) {
				ike_session_unlink_phase2(iph2);
			}
		}
		return;
	}
	if (!ike_session_has_negoing_ph1(iph2->parent_session)) {
        	struct remoteconf *rmconf = getrmconf(iph2->dst);
		/* start phase 1 negotiation as a initiator. */
		if (rmconf) {
            {
				if (ikev1_ph1begin_i(iph2->parent_session, rmconf, iph2->dst, iph2->src, 0) < 0) {
					plog(ASL_LEVEL_DEBUG, "CHKPH1THERE: no established/negoing ph1 handler found... failed to initiate new one\n");
				}
			}			
		} else if (rmconf == NULL) {
			plog(ASL_LEVEL_DEBUG, "CHKPH1THERE: no remoteconf found... failed to initiate new one\n");
		}
	}

	plog(ASL_LEVEL_DEBUG, "CHKPH1THERE: no established ph1 handler found\n");

	/* no isakmp-sa found */
	sched_new(1, isakmp_chkph1there_stub, iph2);

	return;
}

/* copy variable data into ALLOCATED buffer. */
caddr_t
isakmp_set_attr_v(buf, type, val, len)
	caddr_t buf;
	int type;
	caddr_t val;
	int len;
{
	struct isakmp_data *data;

	data = (struct isakmp_data *)buf;
	data->type = htons((u_int16_t)type | ISAKMP_GEN_TLV);
	data->lorv = htons((u_int16_t)len);
	memcpy(data + 1, val, len);

	return buf + sizeof(*data) + len;
}

/* copy fixed length data into ALLOCATED buffer. */
caddr_t
isakmp_set_attr_l(buf, type, val)
	caddr_t buf;
	int type;
	u_int32_t val;
{
	struct isakmp_data *data;

	data = (struct isakmp_data *)buf;
	data->type = htons((u_int16_t)type | ISAKMP_GEN_TV);
	data->lorv = htons((u_int16_t)val);

	return buf + sizeof(*data);
}

/* add a variable data attribute to the buffer by reallocating it. */
vchar_t *
isakmp_add_attr_v(buf0, type, val, len)
	vchar_t *buf0;
	int type;
	caddr_t val;
	int len;
{
	vchar_t *buf = NULL;
	struct isakmp_data *data;
	int tlen;
	int oldlen = 0;

	tlen = sizeof(*data) + len;

	if (buf0) {
		oldlen = buf0->l;
		buf = vrealloc(buf0, oldlen + tlen);
	} else
		buf = vmalloc(tlen);
	if (!buf) {
		plog(ASL_LEVEL_ERR, 
			"failed to get a attribute buffer.\n");
		return NULL;
	}

	data = (struct isakmp_data *)(buf->v + oldlen);
	data->type = htons((u_int16_t)type | ISAKMP_GEN_TLV);
	data->lorv = htons((u_int16_t)len);
	memcpy(data + 1, val, len);

	return buf;
}

/* add a fixed data attribute to the buffer by reallocating it. */
vchar_t *
isakmp_add_attr_l(buf0, type, val)
	vchar_t *buf0;
	int type;
	u_int32_t val;
{
	vchar_t *buf = NULL;
	struct isakmp_data *data;
	int tlen;
	int oldlen = 0;

	tlen = sizeof(*data);

	if (buf0) {
		oldlen = buf0->l;
		buf = vrealloc(buf0, oldlen + tlen);
	} else
		buf = vmalloc(tlen);
	if (!buf) {
		plog(ASL_LEVEL_ERR, 
			"failed to get a attribute buffer.\n");
		return NULL;
	}

	data = (struct isakmp_data *)(buf->v + oldlen);
	data->type = htons((u_int16_t)type | ISAKMP_GEN_TV);
	data->lorv = htons((u_int16_t)val);

	return buf;
}

/*
 * calculate cookie and set.
 */
int
isakmp_newcookie(place, remote, local)
	caddr_t place;
	struct sockaddr_storage *remote;
	struct sockaddr_storage *local;
{
	vchar_t *buf = NULL, *buf2 = NULL;
	char *p;
	int blen;
	int alen;
	caddr_t sa1, sa2;
	time_t t;
	int error = -1;
	u_short port;


	if (remote->ss_family != local->ss_family) {
		plog(ASL_LEVEL_ERR, 
			"address family mismatch, remote:%d local:%d\n",
			remote->ss_family, local->ss_family);
		goto end;
	}
	switch (remote->ss_family) {
	case AF_INET:
		alen = sizeof(struct in_addr);
		sa1 = (caddr_t)&((struct sockaddr_in *)remote)->sin_addr;
		sa2 = (caddr_t)&((struct sockaddr_in *)local)->sin_addr;
		break;
#ifdef INET6
	case AF_INET6:
		alen = sizeof(struct in_addr);
		sa1 = (caddr_t)&((struct sockaddr_in6 *)remote)->sin6_addr;
		sa2 = (caddr_t)&((struct sockaddr_in6 *)local)->sin6_addr;
		break;
#endif
	default:
		plog(ASL_LEVEL_ERR, 
			"invalid family: %d\n", remote->ss_family);
		goto end;
	}
	blen = (alen + sizeof(u_short)) * 2
		+ sizeof(time_t) + lcconf->secret_size;
	buf = vmalloc(blen);
	if (buf == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to get a cookie.\n");
		goto end;
	}
	p = buf->v;

	/* copy my address */
	memcpy(p, sa1, alen);
	p += alen;
	port = ((struct sockaddr_in *)remote)->sin_port;
	memcpy(p, &port, sizeof(u_short));
	p += sizeof(u_short);

	/* copy target address */
	memcpy(p, sa2, alen);
	p += alen;
	port = ((struct sockaddr_in *)local)->sin_port;
	memcpy(p, &port, sizeof(u_short));
	p += sizeof(u_short);

	/* copy time */
	t = time(0);
	memcpy(p, (caddr_t)&t, sizeof(t));
	p += sizeof(t);

	/* copy random value */
	buf2 = eay_set_random(lcconf->secret_size);
	if (buf2 == NULL)
		goto end;
	memcpy(p, buf2->v, lcconf->secret_size);
	p += lcconf->secret_size;
	vfree(buf2);

	buf2 = eay_sha1_one(buf);
	memcpy(place, buf2->v, sizeof(cookie_t));

	sa1 = val2str(place, sizeof (cookie_t));
	plog(ASL_LEVEL_DEBUG, "new cookie:\n%s\n", sa1);
	racoon_free(sa1);

	error = 0;
end:
	if (buf != NULL)
		vfree(buf);
	if (buf2 != NULL)
		vfree(buf2);
	return error;
}

/*
 * save partner's(payload) data into phhandle.
 */
int
isakmp_p2ph(buf, gen)
	vchar_t **buf;
	struct isakmp_gen *gen;
{
	/* XXX to be checked in each functions for logging. */
	if (*buf) {
		plog(ASL_LEVEL_WARNING, 
			"ignore this payload, same payload type exist.\n");
		return -1;
	}

	if (ntohs(gen->len) < sizeof(*gen)) {
		plog(ASL_LEVEL_ERR, 
			 "ignore this payload, invalid payload len %d.\n",
			 ntohs(gen->len));
		return -1;
	}

	*buf = vmalloc(ntohs(gen->len) - sizeof(*gen));
	if (*buf == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to get buffer.\n");
		return -1;
	}
	memcpy((*buf)->v, gen + 1, (*buf)->l);

	return 0;
}

u_int32_t
isakmp_newmsgid2(iph1)
	phase1_handle_t *iph1;
{
	u_int32_t msgid2;

	do {
		msgid2 = eay_random();
	} while (ike_session_getph2bymsgid(iph1, msgid2));

	return msgid2;
}

/*
 * set values into allocated buffer of isakmp header for phase 1
 */
static caddr_t
set_isakmp_header(vbuf, iph1, nptype, etype, flags, msgid)
	vchar_t *vbuf;
	phase1_handle_t *iph1;
	int nptype;
	u_int8_t etype;
	u_int8_t flags;
	u_int32_t msgid;
{
	struct isakmp *isakmp;

	if (vbuf->l < sizeof(*isakmp))
		return NULL;

	isakmp = (struct isakmp *)vbuf->v;

	memcpy(&isakmp->i_ck, &iph1->index.i_ck, sizeof(cookie_t));
	memcpy(&isakmp->r_ck, &iph1->index.r_ck, sizeof(cookie_t));
	isakmp->np = nptype;
	isakmp->v = iph1->version;
	isakmp->etype = etype;
	isakmp->flags = flags;
	isakmp->msgid = msgid;
	isakmp->len = htonl(vbuf->l);

	return vbuf->v + sizeof(*isakmp);
}

/*
 * set values into allocated buffer of isakmp header for phase 1
 */
caddr_t
set_isakmp_header1(vbuf, iph1, nptype)
	vchar_t *vbuf;
	phase1_handle_t *iph1;
	int nptype;
{
	return set_isakmp_header (vbuf, iph1, nptype, iph1->etype, iph1->flags, iph1->msgid);
}

/*
 * set values into allocated buffer of isakmp header for phase 2
 */
caddr_t
set_isakmp_header2(vbuf, iph2, nptype)
	vchar_t *vbuf;
	phase2_handle_t *iph2;
	int nptype;
{
	return set_isakmp_header (vbuf, iph2->ph1, nptype, ISAKMP_ETYPE_QUICK, iph2->flags, iph2->msgid);
}

/*
 * set values into allocated buffer of isakmp payload.
 */
caddr_t
set_isakmp_payload(buf, src, nptype)
	caddr_t buf;
	vchar_t *src;
	int nptype;
{
	struct isakmp_gen *gen;
	caddr_t p = buf;

	plog(ASL_LEVEL_DEBUG, "add payload of len %zu, next type %d\n",
	    src->l, nptype);

	gen = (struct isakmp_gen *)p;
	gen->np = nptype;
	gen->len = htons(sizeof(*gen) + src->l);
	p += sizeof(*gen);
	memcpy(p, src->v, src->l);
	p += src->l;

	return p;
}


#ifdef HAVE_PRINT_ISAKMP_C
/* for print-isakmp.c */
char *snapend;
extern void isakmp_print(const u_char *, u_int, const u_char *);

char *getname(const u_char *);
#ifdef INET6
char *getname6(const u_char *);
#endif
int safeputchar(int);

/*
 * Return a name for the IP address pointed to by ap.  This address
 * is assumed to be in network byte order.
 */
char *
getname(ap)
	const u_char *ap;
{
	struct sockaddr_in addr;
	static char ntop_buf[NI_MAXHOST];

	memset(&addr, 0, sizeof(addr));
	addr.sin_len = sizeof(struct sockaddr_in);
	addr.sin_family = AF_INET;
	memcpy(&addr.sin_addr, ap, sizeof(addr.sin_addr));
	if (getnameinfo(&addr, sizeof(addr),
			ntop_buf, sizeof(ntop_buf), NULL, 0,
			NI_NUMERICHOST | niflags))
		strlcpy(ntop_buf, "?", sizeof(ntop_buf));

	return ntop_buf;
}

#ifdef INET6
/*
 * Return a name for the IP6 address pointed to by ap.  This address
 * is assumed to be in network byte order.
 */
char *
getname6(ap)
	const u_char *ap;
{
	struct sockaddr_in6 addr;
	static char ntop_buf[NI_MAXHOST];

	memset(&addr, 0, sizeof(addr));
	addr.sin6_len = sizeof(struct sockaddr_in6);
	addr.sin6_family = AF_INET6;
	memcpy(&addr.sin6_addr, ap, sizeof(addr.sin6_addr));
	if (getnameinfo(&addr, addr.sin6_len,
			ntop_buf, sizeof(ntop_buf), NULL, 0,
			NI_NUMERICHOST | niflags))
		strlcpy(ntop_buf, "?", sizeof(ntop_buf));

	return ntop_buf;
}
#endif /* INET6 */

int
safeputchar(c)
	int c;
{
	unsigned char ch;

	ch = (unsigned char)(c & 0xff);
	if (c < 0x80 && isprint(c))
		return printf("%c", c & 0xff);
	else
		return printf("\\%03o", c & 0xff);
}

void
isakmp_printpacket(msg, from, my, decoded)
	vchar_t *msg;
	struct sockaddr_storage *from;
	struct sockaddr_storage *my;
	int decoded;
{
#ifdef YIPS_DEBUG
	struct timeval tv;
	int s;
	char hostbuf[NI_MAXHOST];
	char portbuf[NI_MAXSERV];
	struct isakmp *isakmp;
	vchar_t *buf;
#endif

	if (loglevel < ASL_LEVEL_DEBUG)
		return;

#ifdef YIPS_DEBUG
	plog(ASL_LEVEL_DEBUG, "begin.\n");

	gettimeofday(&tv, NULL);
	s = tv.tv_sec % 3600;
	printf("%02d:%02d.%06u ", s / 60, s % 60, (u_int32_t)tv.tv_usec);

	if (from) {
		if (getnameinfo(from, sysdep_sa_len((struct sockaddr *)from), hostbuf, sizeof(hostbuf),
				portbuf, sizeof(portbuf),
				NI_NUMERICHOST | NI_NUMERICSERV | niflags)) {
			strlcpy(hostbuf, "?", sizeof(hostbuf));
			strlcpy(portbuf, "?", sizeof(portbuf));
		}
		printf("%s:%s", hostbuf, portbuf);
	} else
		printf("?");
	printf(" -> ");
	if (my) {
		if (getnameinfo(my, sysdep_sa_len((struct sockaddr *)my), hostbuf, sizeof(hostbuf),
				portbuf, sizeof(portbuf),
				NI_NUMERICHOST | NI_NUMERICSERV | niflags)) {
			strlcpy(hostbuf, "?", sizeof(hostbuf));
			strlcpy(portbuf, "?", sizeof(portbuf));
		}
		printf("%s:%s", hostbuf, portbuf);
	} else
		printf("?");
	printf(": ");

	buf = vdup(msg);
	if (!buf) {
		printf("(malloc fail)\n");
		return;
	}
	if (decoded) {
		isakmp = (struct isakmp *)buf->v;
		if (isakmp->flags & ISAKMP_FLAG_E) {
#if 0
			int pad;
			pad = *(u_char *)(buf->v + buf->l - 1);
			if (buf->l < pad && 2 < vflag)
				printf("(wrong padding)");
#endif
			isakmp->flags &= ~ISAKMP_FLAG_E;
		}
	}

	snapend = buf->v + buf->l;
	isakmp_print(buf->v, buf->l, NULL);
	vfree(buf);
	printf("\n");
	fflush(stdout);

	return;
#endif
}
#endif /*HAVE_PRINT_ISAKMP_C*/

int
copy_ph1addresses(iph1, rmconf, remote, local)
	phase1_handle_t *iph1;
	struct remoteconf *rmconf;
	struct sockaddr_storage *remote, *local;
{
	u_short *port = NULL;

	/* address portion must be grabbed from real remote address "remote" */
	iph1->remote = dupsaddr(remote);
	if (iph1->remote == NULL) {
		ike_session_delph1(iph1);
		return -1;
	}

	/*
	 * if remote has no port # (in case of initiator - from ACQUIRE msg)
	 * - if remote.conf specifies port #, use that
	 * - if remote.conf does not, use 500
	 * if remote has port # (in case of responder - from recvfrom(2))
	 * respect content of "remote".
	 */
	switch (iph1->remote->ss_family) {
	case AF_INET:
		port = &((struct sockaddr_in *)iph1->remote)->sin_port;
		if (*port)
			break;
		*port = ((struct sockaddr_in *)rmconf->remote)->sin_port;
		if (*port)
			break;
		*port = htons(PORT_ISAKMP);
		break;
#ifdef INET6
	case AF_INET6:
		port = &((struct sockaddr_in6 *)iph1->remote)->sin6_port;
		if (*port)
			break;
		*port = ((struct sockaddr_in6 *)rmconf->remote)->sin6_port;
		if (*port)
			break;
		*port = htons(PORT_ISAKMP);
		break;
#endif
	default:
		plog(ASL_LEVEL_ERR, 
			"invalid family: %d\n", iph1->remote->ss_family);
		ike_session_delph1(iph1);
		return -1;
	}

	if (local == NULL)
		iph1->local = getlocaladdr((struct sockaddr *)iph1->remote);
	else
		iph1->local = dupsaddr(local);
	if (iph1->local == NULL) {
		ike_session_delph1(iph1);
		return -1;
	}
	port = NULL;
	switch (iph1->local->ss_family) {
	case AF_INET:
		port = &((struct sockaddr_in *)iph1->local)->sin_port;
		if (*port)
			break;
		*port = ((struct sockaddr_in *)iph1->local)->sin_port;
		if (*port)
			break;
		*port = getmyaddrsport(iph1->local);
		break;
#ifdef INET6
	case AF_INET6:
		port = &((struct sockaddr_in6 *)iph1->local)->sin6_port;
		if (*port)
			break;
		*port = ((struct sockaddr_in6 *)iph1->local)->sin6_port;
		if (*port)
			break;
		*port = getmyaddrsport(iph1->local);
		break;
#endif
	default:
		plog(ASL_LEVEL_ERR,
			"invalid family: %d\n", iph1->local->ss_family);
		ike_session_delph1(iph1);
		return -1;
	}
#ifdef ENABLE_NATT
	if ( port != NULL && *port == htons(lcconf->port_isakmp_natt) ) {
	    plog (ASL_LEVEL_DEBUG, "Marking ports as changed\n");
	    iph1->natt_flags |= NAT_ADD_NON_ESP_MARKER;
	}
#endif

	return 0;
}

void
log_ph1established(iph1)
	const phase1_handle_t *iph1;
{
	char *src, *dst;

	src = racoon_strdup(saddr2str((struct sockaddr *)iph1->local));
	dst = racoon_strdup(saddr2str((struct sockaddr *)iph1->remote));
	STRDUP_FATAL(src);
	STRDUP_FATAL(dst);

	plog(ASL_LEVEL_INFO, 
		"ISAKMP-SA established %s-%s spi:%s\n",
		src, dst,
		isakmp_pindex(&iph1->index, 0));

	racoon_free(src);
	racoon_free(dst);

	IPSECLOGASLMSG("IPSec Phase 1 established (Initiated by %s).\n",
				   (iph1->side == INITIATOR)? "me" : "peer");

	return;
}

struct payload_list *
isakmp_plist_append (struct payload_list *plist, vchar_t *payload, int payload_type)
{
	if (! plist) {
		plist = racoon_malloc (sizeof (struct payload_list));
		plist->prev = NULL;
	}
	else {
		plist->next = racoon_malloc (sizeof (struct payload_list));
		plist->next->prev = plist;
		plist = plist->next;
	}

	plist->next = NULL;
	plist->payload = payload;
	plist->payload_type = payload_type;

	return plist;
}

vchar_t * 
isakmp_plist_set_all (struct payload_list **plist, phase1_handle_t *iph1)
{
	struct payload_list *ptr = *plist, *first;
	size_t tlen = sizeof (struct isakmp), n = 0;
	vchar_t *buf = NULL;
	char *p;

	/* Seek to the first item.  */
	while (ptr->prev) ptr = ptr->prev;
	first = ptr;
	
	/* Compute the whole length.  */
	while (ptr) {
		tlen += ptr->payload->l + sizeof (struct isakmp_gen);
		ptr = ptr->next;
	}

	buf = vmalloc(tlen);
	if (buf == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to get buffer to send.\n");
		goto end;
	}

	ptr = first;

	p = set_isakmp_header1(buf, iph1, ptr->payload_type);
	if (p == NULL)
		goto end;

	while (ptr)
	{
		p = set_isakmp_payload (p, ptr->payload, ptr->next ? ptr->next->payload_type : ISAKMP_NPTYPE_NONE);
		first = ptr;
		ptr = ptr->next;
		racoon_free (first);
		/* ptr->prev = NULL; first = NULL; ... omitted.  */
		n++;
	}

	*plist = NULL;

	return buf;
end:
	if (buf != NULL)
		vfree(buf);
	return NULL;
}

#ifdef ENABLE_FRAG
void
frag_handler(iph1, msg, remote, local)
	phase1_handle_t *iph1;
	vchar_t *msg;
	struct sockaddr_storage *remote;
	struct sockaddr_storage *local;
{
	vchar_t *newmsg;

	if (isakmp_frag_extract(iph1, msg) == 1) {
		if ((newmsg = isakmp_frag_reassembly(iph1)) == NULL) {
			plog(ASL_LEVEL_ERR,  
			    "Packet reassembly failed\n");
			return;
		}

		/* simply reply if the packet was processed. */
		if (ike_session_check_recvdpkt(remote, local, newmsg) > 0) {
			IPSECLOGASLMSG("Received (reassembled) retransmitted packet from %s.\n",
						   saddr2str((struct sockaddr *)remote));

			plog(ASL_LEVEL_NOTICE, 
				 "the reassembled packet is retransmitted by %s.\n",
				 saddr2str((struct sockaddr *)remote));
            vfree(newmsg);
			return;
		}

		isakmp_main(newmsg, remote, local);
        vfree(newmsg);
	}

	return;
}
#endif

void
purge_remote(iph1)
	phase1_handle_t *iph1;
{
	vchar_t *buf = NULL;
	struct sadb_msg *msg, *next, *end;
	struct sadb_sa *sa;
	struct sockaddr_storage *src, *dst;
	caddr_t mhp[SADB_EXT_MAX + 1];
	u_int proto_id;
	phase2_handle_t *iph2;
	phase1_handle_t *new_iph1;

	plog(ASL_LEVEL_INFO, 
		 "purging ISAKMP-SA spi=%s.\n",
		 isakmp_pindex(&(iph1->index), iph1->msgid));

	/* Mark as expired. */
	fsm_set_state(&iph1->status, IKEV1_STATE_PHASE1_EXPIRED);

	new_iph1 = ike_session_update_ph1_ph2tree(iph1);

	/*
	 * Delete all orphaned or binded to the deleting ph1handle phase2 SAs.
	 * Keep all others phase2 SAs.
	 */
	buf = pfkey_dump_sadb(SADB_SATYPE_UNSPEC);
	if (buf == NULL) {
		plog(ASL_LEVEL_DEBUG, 
			"pfkey_dump_sadb returned nothing.\n");
		return;
	}

	msg = ALIGNED_CAST(struct sadb_msg *)buf->v;
	end = ALIGNED_CAST(struct sadb_msg *)(buf->v + buf->l);

	while (msg < end) {
		if ((msg->sadb_msg_len << 3) < sizeof(*msg))
			break;
		next = ALIGNED_CAST(struct sadb_msg *)((caddr_t)msg + (msg->sadb_msg_len << 3));
		if (msg->sadb_msg_type != SADB_DUMP) {
			msg = next;
			continue;
		}

		if (pfkey_align(msg, mhp) || pfkey_check(mhp)) {
			plog(ASL_LEVEL_ERR, 
				"pfkey_check (%s)\n", ipsec_strerror());
			msg = next;
			continue;
		}

		sa = ALIGNED_CAST(struct sadb_sa *)(mhp[SADB_EXT_SA]);
		if (!sa ||
		    !mhp[SADB_EXT_ADDRESS_SRC] ||
		    !mhp[SADB_EXT_ADDRESS_DST]) {
			msg = next;
			continue;
		}
		src = ALIGNED_CAST(struct sockaddr_storage *)PFKEY_ADDR_SADDR(mhp[SADB_EXT_ADDRESS_SRC]);
		dst = ALIGNED_CAST(struct sockaddr_storage *)PFKEY_ADDR_SADDR(mhp[SADB_EXT_ADDRESS_DST]);

		if (sa->sadb_sa_state != SADB_SASTATE_LARVAL &&
		    sa->sadb_sa_state != SADB_SASTATE_MATURE &&
		    sa->sadb_sa_state != SADB_SASTATE_DYING) {
			msg = next;
			continue;
		}

		/*
		 * check in/outbound SAs.
		 * Select only SAs where src == local and dst == remote (outgoing)
		 * or src == remote and dst == local (incoming).
		 */
		if ((CMPSADDR(iph1->local, src) || CMPSADDR(iph1->remote, dst)) &&
			(CMPSADDR(iph1->local, dst) || CMPSADDR(iph1->remote, src))) {
			msg = next;
			continue;
		}

		proto_id = pfkey2ipsecdoi_proto(msg->sadb_msg_satype);
		iph2 = ike_session_getph2bysaidx(src, dst, proto_id, sa->sadb_sa_spi);

		/* Check if there is another valid ISAKMP-SA */
		if (new_iph1 != NULL) {

			if (iph2 == NULL) {
				/* No handler... still send a pfkey_delete message, but log this !*/
				plog(ASL_LEVEL_INFO, 
					"Unknown IPsec-SA spi=%u, hmmmm?\n",
					ntohl(sa->sadb_sa_spi));
			}else{

				/* 
				 * If we have a new ph1, do not purge IPsec-SAs binded
				 *  to a different ISAKMP-SA
				 */
				if (iph2->ph1 != NULL && iph2->ph1 != iph1){
					msg = next;
					continue;
				}

				/* If the ph2handle is established, do not purge IPsec-SA */
				if (FSM_STATE_IS_ESTABLISHED_OR_EXPIRED(iph2->status)) {
					
					plog(ASL_LEVEL_INFO, 
						 "keeping IPsec-SA spi=%u - found valid ISAKMP-SA spi=%s.\n",
						 ntohl(sa->sadb_sa_spi),
						 isakmp_pindex(&(new_iph1->index), new_iph1->msgid));
					msg = next;
					continue;
				}
			}
		}

		
		pfkey_send_delete(lcconf->sock_pfkey,
				  msg->sadb_msg_satype,
				  IPSEC_MODE_ANY,
				  src, dst, sa->sadb_sa_spi);

		/* delete a relative phase 2 handle. */
		if (iph2 != NULL) {
			delete_spd(iph2);
			ike_session_unlink_phase2(iph2);
		}

		plog(ASL_LEVEL_INFO, 
			 "purged IPsec-SA spi=%u.\n",
			 ntohl(sa->sadb_sa_spi));

		msg = next;
	}

	if (buf)
		vfree(buf);

	/* Mark the phase1 handler as EXPIRED */
	plog(ASL_LEVEL_INFO, 
		 "purged ISAKMP-SA spi=%s.\n",
		 isakmp_pindex(&(iph1->index), iph1->msgid));

	SCHED_KILL(iph1->sce);

	iph1->sce = sched_new(1, isakmp_ph1delete_stub, iph1);
}

void 
delete_spd(iph2)
	phase2_handle_t *iph2;
{
	if (iph2 == NULL)
		return;

	/* Delete the SPD entry if we generated it
	 */
	if (iph2->generated_spidx) {
		union {
			u_int64_t	force_align;		// Wcast-align fix - force alignment
			struct policyindex spidx;
		} u;
		struct sockaddr_storage addr;
		u_int8_t pref;
		struct sockaddr_storage *src = iph2->src;
		struct sockaddr_storage *dst = iph2->dst;
		int error;
		int idi2type = 0;/* switch whether copy IDs into id[src,dst]. */

		plog(ASL_LEVEL_INFO, 
			 "generated policy, deleting it.\n");
		
		memset(&u.spidx, 0, sizeof(u.spidx));
		iph2->spidx_gen = &u.spidx;
		
		/* make inbound policy */
		iph2->src = dst;
		iph2->dst = src;
		u.spidx.dir = IPSEC_DIR_INBOUND;
		u.spidx.ul_proto = 0;
		
		/* 
		 * Note: code from get_proposal_r
		 */
		
#define _XIDT(d) (ALIGNED_CAST(struct ipsecdoi_id_b *)((d)->v))->type
		
		/*
		 * make destination address in spidx from either ID payload
		 * or phase 1 address into a address in spidx.
		 */
		if (iph2->id != NULL
			&& (_XIDT(iph2->id) == IPSECDOI_ID_IPV4_ADDR
			|| _XIDT(iph2->id) == IPSECDOI_ID_IPV6_ADDR
			|| _XIDT(iph2->id) == IPSECDOI_ID_IPV4_ADDR_SUBNET
			|| _XIDT(iph2->id) == IPSECDOI_ID_IPV6_ADDR_SUBNET)) {
			/* get a destination address of a policy */
			error = ipsecdoi_id2sockaddr(iph2->id, &u.spidx.dst,
			    &u.spidx.prefd, &u.spidx.ul_proto, iph2->version);
			if (error)
				goto purge;
			
#ifdef INET6
			/*
			 * get scopeid from the SA address.
			 * note that the phase 1 source address is used as
			 * a destination address to search for a inbound 
			 * policy entry because rcoon is responder.
			 */
			if (_XIDT(iph2->id) == IPSECDOI_ID_IPV6_ADDR) {
				if ((error = 
				    setscopeid(&u.spidx.dst, iph2->src)) != 0)
					goto purge;
			}
#endif
			
			if (_XIDT(iph2->id) == IPSECDOI_ID_IPV4_ADDR
				|| _XIDT(iph2->id) == IPSECDOI_ID_IPV6_ADDR)
				idi2type = _XIDT(iph2->id);
			
		} else {
			
			plog(ASL_LEVEL_DEBUG, 
				 "Get a destination address of SP index "
				 "from Phase 1 address "
				 "due to no ID payloads found "
				 "OR because ID type is not address.\n");
			
			/*
			 * copy the SOURCE address of IKE into the 
			 * DESTINATION address of the key to search the 
			 * SPD because the direction of policy is inbound.
			 */
			memcpy(&u.spidx.dst, iph2->src, sysdep_sa_len((struct sockaddr *)iph2->src));
			switch (u.spidx.dst.ss_family) {
				case AF_INET:
					u.spidx.prefd = 
					    sizeof(struct in_addr) << 3;
					break;
#ifdef INET6
				case AF_INET6:
					u.spidx.prefd = 
					    sizeof(struct in6_addr) << 3;
					break;
#endif
				default:
					u.spidx.prefd = 0;
					break;
			}
		}
					
		/* make source address in spidx */
		if (iph2->id_p != NULL
			&& (_XIDT(iph2->id_p) == IPSECDOI_ID_IPV4_ADDR
			|| _XIDT(iph2->id_p) == IPSECDOI_ID_IPV6_ADDR
			|| _XIDT(iph2->id_p) == IPSECDOI_ID_IPV4_ADDR_SUBNET
			|| _XIDT(iph2->id_p) == IPSECDOI_ID_IPV6_ADDR_SUBNET)) {
			/* get a source address of inbound SA */
			error = ipsecdoi_id2sockaddr(iph2->id_p, &u.spidx.src,
			    &u.spidx.prefs, &u.spidx.ul_proto, iph2->version);
			if (error)
				goto purge;

#ifdef INET6
			/*
			 * get scopeid from the SA address.
			 * for more detail, see above of this function.
			 */
			if (_XIDT(iph2->id_p) == IPSECDOI_ID_IPV6_ADDR) {
				error = 
				    setscopeid(&u.spidx.src, iph2->dst);
				if (error)
					goto purge;
			}
#endif

			/* make id[src,dst] if both ID types are IP address and same */
			if (_XIDT(iph2->id_p) == idi2type
				&& u.spidx.dst.ss_family == u.spidx.src.ss_family) {
				iph2->src_id = 
				    dupsaddr(&u.spidx.dst);
				iph2->dst_id = 
				    dupsaddr(&u.spidx.src);
			}

		} else {
			plog(ASL_LEVEL_DEBUG, 
				 "Get a source address of SP index "
				 "from Phase 1 address "
				 "due to no ID payloads found "
				 "OR because ID type is not address.\n");

			/* see above comment. */
			memcpy(&u.spidx.src, iph2->dst, sysdep_sa_len((struct sockaddr *)iph2->dst));
			switch (u.spidx.src.ss_family) {
				case AF_INET:
					u.spidx.prefs = 
					    sizeof(struct in_addr) << 3;
					break;
#ifdef INET6
				case AF_INET6:
					u.spidx.prefs = 
					    sizeof(struct in6_addr) << 3;
					break;
#endif
				default:
					u.spidx.prefs = 0;
					break;
			}
		}

#undef _XIDT

		plog(ASL_LEVEL_DEBUG, 
			 "get a src address from ID payload "
			 "%s prefixlen=%u ul_proto=%u\n",
			 saddr2str((struct sockaddr *)&u.spidx.src),
			 u.spidx.prefs, u.spidx.ul_proto);
		plog(ASL_LEVEL_DEBUG, 
			 "get dst address from ID payload "
			 "%s prefixlen=%u ul_proto=%u\n",
			 saddr2str((struct sockaddr *)&u.spidx.dst),
			 u.spidx.prefd, u.spidx.ul_proto);

		/*
		 * convert the ul_proto if it is 0
		 * because 0 in ID payload means a wild card.
		 */
		if (u.spidx.ul_proto == 0)
			u.spidx.ul_proto = IPSEC_ULPROTO_ANY;

#undef _XIDT

		/* End of code from get_proposal_r
		 */

		if (pk_sendspddelete(iph2) < 0) {
			plog(ASL_LEVEL_ERR, 
				 "pfkey spddelete(inbound) failed.\n");
		}else{
			plog(ASL_LEVEL_DEBUG, 
				 "pfkey spddelete(inbound) sent.\n");
		}

#ifdef HAVE_POLICY_FWD
		/* make forward policy if required */
		if (tunnel_mode_prop(iph2->approval)) {
			u.spidx.dir = IPSEC_DIR_FWD;
			if (pk_sendspddelete(iph2) < 0) {
				plog(ASL_LEVEL_ERR, 
					 "pfkey spddelete(forward) failed.\n");
			}else{
				plog(ASL_LEVEL_DEBUG, 
					 "pfkey spddelete(forward) sent.\n");
			}
		}
#endif

		/* make outbound policy */
		iph2->src = src;
		iph2->dst = dst;
		u.spidx.dir = IPSEC_DIR_OUTBOUND;
		addr = u.spidx.src;
		u.spidx.src = u.spidx.dst;
		u.spidx.dst = addr;
		pref = u.spidx.prefs;
		u.spidx.prefs = u.spidx.prefd;
		u.spidx.prefd = pref;

		if (pk_sendspddelete(iph2) < 0) {
			plog(ASL_LEVEL_ERR, 
				 "pfkey spddelete(outbound) failed.\n");
		}else{
			plog(ASL_LEVEL_DEBUG, 
				 "pfkey spddelete(outbound) sent.\n");
		}
purge:
		iph2->spidx_gen=NULL;
	}
}

#ifdef INET6
u_int32_t
setscopeid(sp_addr0, sa_addr0)
	struct sockaddr_storage *sp_addr0, *sa_addr0;
{
	struct sockaddr_in6 *sp_addr, *sa_addr;
    
	sp_addr = (struct sockaddr_in6 *)sp_addr0;
	sa_addr = (struct sockaddr_in6 *)sa_addr0;

	if (!IN6_IS_ADDR_LINKLOCAL(&sp_addr->sin6_addr)
	 && !IN6_IS_ADDR_SITELOCAL(&sp_addr->sin6_addr)
	 && !IN6_IS_ADDR_MULTICAST(&sp_addr->sin6_addr))
		return 0;

	/* this check should not be here ? */
	if (sa_addr->sin6_family != AF_INET6) {
		plog(ASL_LEVEL_ERR, 
			"can't get scope ID: family mismatch\n");
		return -1;
	}

	if (!IN6_IS_ADDR_LINKLOCAL(&sa_addr->sin6_addr)) {
		plog(ASL_LEVEL_ERR, 
			"scope ID is not supported except of lladdr.\n");
		return -1;
	}

	sp_addr->sin6_scope_id = sa_addr->sin6_scope_id;

	return 0;
}
#endif

vchar_t *
isakmp_plist_append_initial_contact (iph1, plist)
	phase1_handle_t *iph1;
	struct payload_list *plist;
{
	if (!iph1->is_rekey && iph1->rmconf->ini_contact && !ike_session_getcontacted(iph1->remote)) {
		vchar_t *notp_ini = NULL;
		struct isakmp_pl_n np, *nptr;
		char *cptr;

		np.doi = htonl(iph1->rmconf->doitype);
		np.proto_id = IPSECDOI_PROTO_ISAKMP;
		np.spi_size = sizeof(isakmp_index);
		np.type = htons(ISAKMP_NTYPE_INITIAL_CONTACT);
		if ((notp_ini = vmalloc(sizeof(struct isakmp_pl_n) - sizeof(struct isakmp_gen) 
								+ sizeof(isakmp_index)))) {
			nptr = &np;
			memcpy(notp_ini->v, &nptr->doi, sizeof(struct isakmp_pl_n) - sizeof(struct isakmp_gen));
			cptr = notp_ini->v + sizeof(struct isakmp_pl_n) - sizeof(struct isakmp_gen);
			memcpy(cptr, &iph1->index, sizeof(isakmp_index));
			plist = isakmp_plist_append(plist, notp_ini, ISAKMP_NPTYPE_N);
			plog(ASL_LEVEL_DEBUG,
				 "added initial-contact payload.\n");
			
			/* insert a node into contacted list. */
			if (ike_session_inscontacted(iph1->remote) == -1) {
				plog(ASL_LEVEL_ERR,
					 "failed to add contacted list.\n");
				/* ignore */
			}
			return notp_ini;
		} else {
			plog(ASL_LEVEL_ERR,
				 "failed to allocate notification payload.\n");
			return NULL;
		}
	} else {
		plog(ASL_LEVEL_DEBUG, "failed to add initial-contact payload: rekey %d, ini-contact %d, contacted %d.\n",
			 iph1->is_rekey? 1:0, iph1->rmconf->ini_contact, ike_session_getcontacted(iph1->remote)? 1:0);
	}
	return NULL;
}
