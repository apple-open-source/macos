/*	$NetBSD: isakmp_cfg.c,v 1.12.6.1 2007/06/07 20:06:34 manu Exp $	*/

/* Id: isakmp_cfg.c,v 1.55 2006/08/22 18:17:17 manubsd Exp */

/*
 * Copyright (C) 2004-2006 Emmanuel Dreyfus
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
#include <sys/queue.h>

#include <utmpx.h>
#include <util.h>


#ifdef __FreeBSD__
# include <libutil.h>
#endif
#ifdef __NetBSD__
#  include <util.h>
#endif

#include <netinet/in.h>
#include <arpa/inet.h>

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
#if HAVE_STDINT_H
#include <stdint.h>
#endif
#include <ctype.h>
#include <resolv.h>

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "sockmisc.h"
#include "schedule.h"
#include "debug.h"
#include "fsm.h"

#include "isakmp_var.h"
#include "isakmp.h"
#include "handler.h"
#include "throttle.h"
#include "remoteconf.h"
#include "localconf.h"
#include "crypto_openssl.h"
#include "isakmp_inf.h"
#include "isakmp_xauth.h"
#include "isakmp_unity.h"
#include "isakmp_cfg.h"
#include "strnames.h"
#include "vpn_control.h"
#include "vpn_control_var.h"
#include "ike_session.h"
#include "ipsecSessionTracer.h"
#include "ipsecMessageTracer.h"
#include "nattraversal.h"

struct isakmp_cfg_config isakmp_cfg_config;

static vchar_t *buffer_cat (vchar_t *s, vchar_t *append);
static vchar_t *isakmp_cfg_net (phase1_handle_t *, struct isakmp_data *);
#if 0
static vchar_t *isakmp_cfg_void (phase1_handle_t *, struct isakmp_data *);
#endif
static vchar_t *isakmp_cfg_addr4 (phase1_handle_t *, 
				 struct isakmp_data *, in_addr_t *);
static void isakmp_cfg_getaddr4 (struct isakmp_data *, struct in_addr *);
static vchar_t *isakmp_cfg_addr4_list (phase1_handle_t *,
				      struct isakmp_data *, in_addr_t *, int);
static void isakmp_cfg_appendaddr4 (struct isakmp_data *, 
				   struct in_addr *, int *, int);
static void isakmp_cfg_getstring (struct isakmp_data *,char *);
void isakmp_cfg_iplist_to_str (char *, int, void *, int);

#define ISAKMP_CFG_LOGIN	1
#define ISAKMP_CFG_LOGOUT	2

/* 
 * Handle an ISAKMP config mode packet
 * We expect HDR, HASH, ATTR
 */
void
isakmp_cfg_r(iph1, msg)
	phase1_handle_t *iph1;
	vchar_t *msg;
{
	struct isakmp *packet;
	struct isakmp_gen *ph;
	int tlen;
	char *npp;
	int np;
	vchar_t *dmsg;
	struct isakmp_ivm *ivm;
	phase2_handle_t *iph2;
	int               error = -1;

	/* Check that the packet is long enough to have a header */
	if (msg->l < sizeof(*packet)) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKE_PACKET_RX_FAIL,
								CONSTSTR("MODE-Config. Unexpected short packet"),
								CONSTSTR("Failed to process short MODE-Config packet"));
		plog(ASL_LEVEL_ERR, "Unexpected short packet\n");
		return;
	}

	packet = (struct isakmp *)msg->v;

	/* Is it encrypted? It should be encrypted */
	if ((packet->flags & ISAKMP_FLAG_E) == 0) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKE_PACKET_RX_FAIL,
								CONSTSTR("MODE-Config. User credentials sent in cleartext"),
								CONSTSTR("Dropped cleattext User credentials"));
		plog(ASL_LEVEL_ERR, 
		    "User credentials sent in cleartext!\n");
		return;
	}

	/* 
	 * Decrypt the packet. If this is the beginning of a new
	 * exchange, reinitialize the IV
	 */
	if (iph1->mode_cfg->ivm == NULL ||
	    iph1->mode_cfg->last_msgid != packet->msgid )
		iph1->mode_cfg->ivm = 
		    isakmp_cfg_newiv(iph1, packet->msgid);
	ivm = iph1->mode_cfg->ivm;

	dmsg = oakley_do_decrypt(iph1, msg, ivm->iv, ivm->ive);
	if (dmsg == NULL) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKE_PACKET_RX_FAIL,
								CONSTSTR("MODE-Config. Failed to decrypt packet"),
								CONSTSTR("Failed to decrypt MODE-Config packet"));
		plog(ASL_LEVEL_ERR, 
		    "failed to decrypt message\n");
		return;
	}

	plog(ASL_LEVEL_DEBUG, "MODE_CFG packet\n");

	/* Now work with the decrypted packet */
	packet = (struct isakmp *)dmsg->v;
	tlen = dmsg->l - sizeof(*packet);
	ph = (struct isakmp_gen *)(packet + 1);

	np = packet->np;
	while ((tlen > 0) && (np != ISAKMP_NPTYPE_NONE)) {
		/* Check that the payload header fits in the packet */
		if (tlen < sizeof(*ph)) {
			 plog(ASL_LEVEL_WARNING, 
			      "Short payload header\n");
			 goto out;
		}

		/* Check that the payload fits in the packet */
		if (tlen < ntohs(ph->len)) {
			plog(ASL_LEVEL_WARNING, 
			      "Short payload\n");
			goto out;
		}
		
		plog(ASL_LEVEL_DEBUG, "Seen payload %d\n", np);

		switch(np) {
		case ISAKMP_NPTYPE_HASH: {
			vchar_t *check;
			vchar_t *payload;
			size_t plen;
			struct isakmp_gen *nph;

			plen = ntohs(ph->len);
			nph = (struct isakmp_gen *)((char *)ph + plen);
			plen = ntohs(nph->len);
            /* Check that the hash payload fits in the packet */
			if (tlen < (plen + ntohs(ph->len))) {
				plog(ASL_LEVEL_WARNING, 
					 "Invalid Hash payload. len %d, overall-len %d\n",
					 ntohs(nph->len),
					 (int)plen);
				goto out;
			}
            
			if ((payload = vmalloc(plen)) == NULL) {
				plog(ASL_LEVEL_ERR, 
				    "Cannot allocate memory\n");
				goto out;
			}
			memcpy(payload->v, nph, plen);

			if ((check = oakley_compute_hash1(iph1, 
			    packet->msgid, payload)) == NULL) {
				plog(ASL_LEVEL_ERR, 
				    "Cannot compute hash\n");
				vfree(payload);
				goto out;
			}

			if (memcmp(ph + 1, check->v, check->l) != 0) {
				plog(ASL_LEVEL_ERR, 
				    "Hash verification failed\n");
				vfree(payload);
				vfree(check);
				goto out;
			}
			vfree(payload);
			vfree(check);
			break;
		}
		case ISAKMP_NPTYPE_ATTR: {
			struct isakmp_pl_attr *attrpl;

			attrpl = (struct isakmp_pl_attr *)ph;
			isakmp_cfg_attr_r(iph1, packet->msgid, attrpl, msg);

			break;
		}
		default:
			 plog(ASL_LEVEL_WARNING, 
			      "Unexpected next payload %d\n", np);
			 /* Skip to the next payload */
			 break;
		}

		/* Move to the next payload */
		np = ph->np;
		tlen -= ntohs(ph->len);
		npp = (char *)ph;
		ph = (struct isakmp_gen *)(npp + ntohs(ph->len));
	}

	error = 0;
	/* find phase 2 in case pkt scheduled for resend */
	iph2 = ike_session_getph2bymsgid(iph1, packet->msgid);
	if (iph2 == NULL)
		goto out;		/* no resend scheduled */
	SCHED_KILL(iph2->scr);	/* turn off schedule */
	ike_session_unlink_phase2(iph2);

	IPSECSESSIONTRACEREVENT(iph1->parent_session,
							IPSECSESSIONEVENTCODE_IKE_PACKET_RX_SUCC,
							CONSTSTR("MODE-Config"),
							CONSTSTR(NULL));
out:
	if (error) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKE_PACKET_RX_FAIL,
								CONSTSTR("MODE-Config"),
								CONSTSTR("Failed to process Mode-Config packet"));
	}
	vfree(dmsg);
}

int
isakmp_cfg_attr_r(iph1, msgid, attrpl, msg) 
	phase1_handle_t *iph1;
	u_int32_t msgid;
	struct isakmp_pl_attr *attrpl;
       vchar_t *msg;
{
	int type = attrpl->type;

	plog(ASL_LEVEL_DEBUG, 
	     "Configuration exchange type %s\n", s_isakmp_cfg_ptype(type));
	switch (type) {
	case ISAKMP_CFG_ACK:
		/* ignore, but this is the time to reinit the IV */
		oakley_delivm(iph1->mode_cfg->ivm);
		iph1->mode_cfg->ivm = NULL;
		return 0;            
		break;

	case ISAKMP_CFG_REPLY:
		return isakmp_cfg_reply(iph1, attrpl);
		break;

	case ISAKMP_CFG_REQUEST:
		iph1->msgid = msgid;
		return isakmp_cfg_request(iph1, attrpl, msg);
		break;

	case ISAKMP_CFG_SET:
		iph1->msgid = msgid;
		return isakmp_cfg_set(iph1, attrpl, msg);
		break;

	default:
		plog(ASL_LEVEL_WARNING, 
		     "Unepected configuration exchange type %d\n", type);
		return -1;
		break;
	}

	return 0;
}

int
isakmp_cfg_reply(iph1, attrpl)
	phase1_handle_t *iph1;
	struct isakmp_pl_attr *attrpl;
{
	struct isakmp_data *attr;
	int tlen;
	size_t alen;
	char *npp;
	int type;
	int error;

	if (iph1->mode_cfg->flags & ISAKMP_CFG_GOT_REPLY)
		return 0;	/* already received this - duplicate packet */
		
	tlen = ntohs(attrpl->h.len);
	attr = (struct isakmp_data *)(attrpl + 1);
	tlen -= sizeof(*attrpl);

	while (tlen > 0) {
		type = ntohs(attr->type);

		/* Handle short attributes */
		if ((type & ISAKMP_GEN_MASK) == ISAKMP_GEN_TV) {
			type &= ~ISAKMP_GEN_MASK;

			plog(ASL_LEVEL_DEBUG, 
			     "Short attribute %s = %d\n", 
			     s_isakmp_cfg_type(type), ntohs(attr->lorv));

			switch (type) {			
			case XAUTH_TYPE:
				if ((error = xauth_attr_reply(iph1, 
				    attr, ntohs(attrpl->id))) != 0)
					return error;
				break;

				break;

			default:
				plog(ASL_LEVEL_WARNING, 
				     "Ignored short attribute %s\n",
				     s_isakmp_cfg_type(type));
				break;
			}

			tlen -= sizeof(*attr);
			attr++;
			continue;
		}

		type = ntohs(attr->type);
		alen = ntohs(attr->lorv);

		/* Check that the attribute fit in the packet */
		if (tlen < alen) {
			plog(ASL_LEVEL_ERR, 
			     "Short attribute %s\n",
			     s_isakmp_cfg_type(type));
			return -1;
		}

		plog(ASL_LEVEL_DEBUG, 
		     "Attribute %s, len %zu\n", 
		     s_isakmp_cfg_type(type), alen);

		switch(type) {
		case XAUTH_TYPE:
		case XAUTH_USER_NAME:
		case XAUTH_USER_PASSWORD:
		case XAUTH_PASSCODE:
		case XAUTH_MESSAGE:
		case XAUTH_CHALLENGE:
		case XAUTH_DOMAIN:
		case XAUTH_STATUS:
		case XAUTH_NEXT_PIN:
		case XAUTH_ANSWER:
			if ((error = xauth_attr_reply(iph1, 
			    attr, ntohs(attrpl->id))) != 0)
				return error;
			break;
		case INTERNAL_IP4_ADDRESS:
			if ((iph1->mode_cfg->flags & ISAKMP_CFG_GOT_ADDR4) == 0) {
				isakmp_cfg_getaddr4(attr, &iph1->mode_cfg->addr4);
				iph1->mode_cfg->flags |= ISAKMP_CFG_GOT_ADDR4;
			}
			break;
		case INTERNAL_IP4_NETMASK:
			if ((iph1->mode_cfg->flags & ISAKMP_CFG_GOT_MASK4) == 0) {
				isakmp_cfg_getaddr4(attr, &iph1->mode_cfg->mask4);
				iph1->mode_cfg->flags |= ISAKMP_CFG_GOT_MASK4;
			}
			break;
		case INTERNAL_IP4_DNS:
			if ((iph1->mode_cfg->flags & ISAKMP_CFG_GOT_DNS4) == 0) {
				isakmp_cfg_appendaddr4(attr, 
					&iph1->mode_cfg->dns4[iph1->mode_cfg->dns4_index],
					&iph1->mode_cfg->dns4_index, MAXNS);
				iph1->mode_cfg->flags |= ISAKMP_CFG_GOT_DNS4;
			}
			break;
		case INTERNAL_IP4_NBNS:
			if ((iph1->mode_cfg->flags & ISAKMP_CFG_GOT_WINS4) == 0) {
				isakmp_cfg_appendaddr4(attr, 
					&iph1->mode_cfg->wins4[iph1->mode_cfg->wins4_index],
					&iph1->mode_cfg->wins4_index, MAXNS);
				iph1->mode_cfg->flags |= ISAKMP_CFG_GOT_WINS4;
			}
			break;
		case UNITY_DEF_DOMAIN:
			if ((iph1->mode_cfg->flags & ISAKMP_CFG_GOT_DEFAULT_DOMAIN) == 0) {
				isakmp_cfg_getstring(attr, 
					iph1->mode_cfg->default_domain);
				iph1->mode_cfg->flags |= ISAKMP_CFG_GOT_DEFAULT_DOMAIN;
			}
			break;
		case UNITY_SPLIT_INCLUDE:
		case UNITY_LOCAL_LAN:
		case UNITY_SPLITDNS_NAME:
		case UNITY_BANNER:
		case UNITY_SAVE_PASSWD:
		case UNITY_NATT_PORT:
		case UNITY_FW_TYPE:
		case UNITY_BACKUP_SERVERS:
		case UNITY_DDNS_HOSTNAME:
		case APPLICATION_VERSION:
		case UNITY_PFS:
			isakmp_unity_reply(iph1, attr);
			break;
		case INTERNAL_IP4_SUBNET:
		case INTERNAL_ADDRESS_EXPIRY:
			if (iph1->started_by_api)
				break;	/* not actually ignored - don't fall thru */
			// else fall thru
		default:
			plog(ASL_LEVEL_WARNING, 
			     "Ignored attribute %s\n",
			     s_isakmp_cfg_type(type));
			break;
		}

		npp = (char *)attr;
		attr = (struct isakmp_data *)(npp + sizeof(*attr) + alen);
		tlen -= (sizeof(*attr) + alen);
	}
	iph1->mode_cfg->flags |= ISAKMP_CFG_GOT_REPLY;
	
	if (iph1->started_by_api || (iph1->is_rekey && iph1->parent_session && iph1->parent_session->is_client)) {
		/* connection was started by API - save attr list for passing to VPN controller */
		if (iph1->mode_cfg->attr_list != NULL)	/* shouldn't happen */
			vfree(iph1->mode_cfg->attr_list);
		if (ntohs(attrpl->h.len) < sizeof(*attrpl)) {
			plog(ASL_LEVEL_ERR, 
				 "invalid cfg-attr-list, attr-len %d\n",
				 ntohs(attrpl->h.len));
			return -1;
		}
		alen = ntohs(attrpl->h.len) - sizeof(*attrpl);
		if ((iph1->mode_cfg->attr_list = vmalloc(alen)) == NULL) {
			plog(ASL_LEVEL_ERR, 
			     "Cannot allocate memory for mode-cfg attribute list\n");
			return -1;
		}
		memcpy(iph1->mode_cfg->attr_list->v, attrpl + 1, alen);
	}
		
		
#ifdef ENABLE_VPNCONTROL_PORT
	if (FSM_STATE_IS_ESTABLISHED(iph1->status))
		vpncontrol_notify_phase_change(0, FROM_LOCAL, iph1, NULL);
#endif

	return 0;
}

int
isakmp_cfg_request(iph1, attrpl, msg)
	phase1_handle_t *iph1;
	struct isakmp_pl_attr *attrpl;
       vchar_t *msg;
{
	struct isakmp_data *attr;
	int tlen;
	size_t alen;
	char *npp;
	vchar_t *payload = NULL;
	struct isakmp_pl_attr *reply;
	vchar_t *reply_attr;
	int type;
	int error = -1;

	tlen = ntohs(attrpl->h.len);
	attr = (struct isakmp_data *)(attrpl + 1);
	tlen -= sizeof(*attrpl);

	/*
	 * if started_by_api then we are a VPN client and if we receive
	 * a mode-cfg request it needs to go to the VPN controller to
	 * retrieve the appropriate data (name, pw, pin, etc.)
	 */
	if (iph1->started_by_api || ike_session_is_client_ph1_rekey(iph1)) {		
		/* 
		 * if we already received this one - ignore it
		 * we are waiting for a reply from the vpn control socket
		 */
		if (iph1->xauth_awaiting_userinput)			
			return 0;	
			
		/* otherwise - save the msg id and call and send the status notification */
		iph1->pended_xauth_id = attrpl->id;		/* network byte order */
		if (vpncontrol_notify_need_authinfo(iph1, attrpl + 1, tlen))
			goto end;
		iph1->xauth_awaiting_userinput = 1;
               iph1->xauth_awaiting_userinput_msg = vdup(msg); // dup the message for later
		ike_session_start_xauth_timer(iph1);

		IPSECLOGASLMSG("IPSec Extended Authentication requested.\n");

		return 0;
	}

	if ((payload = vmalloc(sizeof(*reply))) == NULL) {
		plog(ASL_LEVEL_ERR, "Cannot allocate memory\n");
		return -1;
	}
	memset(payload->v, 0, sizeof(*reply));
	
	while (tlen > 0) {
		reply_attr = NULL;
		type = ntohs(attr->type);

		/* Handle short attributes */
		if ((type & ISAKMP_GEN_MASK) == ISAKMP_GEN_TV) {
			type &= ~ISAKMP_GEN_MASK;

			plog(ASL_LEVEL_DEBUG, 
			     "Short attribute %s = %d\n", 
			     s_isakmp_cfg_type(type), ntohs(attr->lorv));

			switch (type) {
			case XAUTH_TYPE:
				reply_attr = isakmp_xauth_req(iph1, attr);
				break;
			default:
				plog(ASL_LEVEL_WARNING, 
				     "Ignored short attribute %s\n",
				     s_isakmp_cfg_type(type));
				break;
			}

			tlen -= sizeof(*attr);
			attr++;

			if (reply_attr != NULL) {
				payload = buffer_cat(payload, reply_attr);
				vfree(reply_attr);
			}

			continue;
		}
		
		type = ntohs(attr->type);
		alen = ntohs(attr->lorv);

		/* Check that the attribute fit in the packet */
		if (tlen < alen) {
			plog(ASL_LEVEL_ERR, 
			     "Short attribute %s\n",
			     s_isakmp_cfg_type(type));
			goto end;
		}

		plog(ASL_LEVEL_DEBUG, 
		     "Attribute %s, len %zu\n",
		     s_isakmp_cfg_type(type), alen);

		switch(type) {
		case INTERNAL_IP4_ADDRESS:
		case INTERNAL_IP4_NETMASK:
		case INTERNAL_IP4_DNS:
		case INTERNAL_IP4_NBNS:
		case INTERNAL_IP4_SUBNET:
			reply_attr = isakmp_cfg_net(iph1, attr);
			break;

		case XAUTH_TYPE:
		case XAUTH_USER_NAME:
		case XAUTH_USER_PASSWORD:
		case XAUTH_PASSCODE:
		case XAUTH_MESSAGE:
		case XAUTH_CHALLENGE:
		case XAUTH_DOMAIN:
		case XAUTH_STATUS:
		case XAUTH_NEXT_PIN:
		case XAUTH_ANSWER:
			reply_attr = isakmp_xauth_req(iph1, attr);
			break;

		case APPLICATION_VERSION:
			reply_attr = isakmp_cfg_string(iph1, 
			    attr, ISAKMP_CFG_RACOON_VERSION);
			break;

		case UNITY_BANNER:
		case UNITY_PFS:
		case UNITY_SAVE_PASSWD:
		case UNITY_DEF_DOMAIN:
		case UNITY_DDNS_HOSTNAME:
		case UNITY_FW_TYPE:
		case UNITY_SPLITDNS_NAME:
		case UNITY_SPLIT_INCLUDE:
		case UNITY_LOCAL_LAN:
		case UNITY_NATT_PORT:
		case UNITY_BACKUP_SERVERS:
			reply_attr = isakmp_unity_req(iph1, attr);
			break;

		case INTERNAL_ADDRESS_EXPIRY:
		default:
			plog(ASL_LEVEL_WARNING, 
			     "Ignored attribute %s\n",
			     s_isakmp_cfg_type(type));
			break;
		}

		npp = (char *)attr;
		attr = (struct isakmp_data *)(npp + sizeof(*attr) + alen);
		tlen -= (sizeof(*attr) + alen);

		if (reply_attr != NULL) {
			payload = buffer_cat(payload, reply_attr);
			vfree(reply_attr);
		}
	}

	reply = (struct isakmp_pl_attr *)payload->v;
	reply->h.len = htons(payload->l);
	reply->type = ISAKMP_CFG_REPLY;
	reply->id = attrpl->id;

	plog(ASL_LEVEL_DEBUG, 
		    "Sending MODE_CFG REPLY\n");

	error = isakmp_cfg_send(iph1, payload, 
	    ISAKMP_NPTYPE_ATTR, ISAKMP_FLAG_E, 0, 0, msg);

	
end:
	vfree(payload);

	return error;
}

int
isakmp_cfg_set(iph1, attrpl, msg)
	phase1_handle_t *iph1;
	struct isakmp_pl_attr *attrpl;
    vchar_t *msg;
{
	struct isakmp_data *attr;
	int tlen;
	size_t alen;
	char *npp;
	vchar_t *payload;
	struct isakmp_pl_attr *reply;
	vchar_t *reply_attr;
	int type;
	int error = -1;

	if ((payload = vmalloc(sizeof(*reply))) == NULL) {
		plog(ASL_LEVEL_ERR, "Cannot allocate memory\n");
		return -1;
	}
	memset(payload->v, 0, sizeof(*reply));

	tlen = ntohs(attrpl->h.len);
	attr = (struct isakmp_data *)(attrpl + 1);
	tlen -= sizeof(*attrpl);
	
	/* 
	 * We should send ack for the attributes we accepted 
	 */
	while (tlen > 0) {
		reply_attr = NULL;
		type = ntohs(attr->type);

		plog(ASL_LEVEL_DEBUG, 
		     "Attribute %s\n", 
		     s_isakmp_cfg_type(type & ~ISAKMP_GEN_MASK));
		
		switch (type & ~ISAKMP_GEN_MASK) {
		case XAUTH_STATUS:
			reply_attr = isakmp_xauth_set(iph1, attr);
			break;
		default:
			plog(ASL_LEVEL_DEBUG, 
			     "Unexpected SET attribute %s\n", 
		     	     s_isakmp_cfg_type(type & ~ISAKMP_GEN_MASK));
			break;
		}

		if (reply_attr != NULL) {
			payload = buffer_cat(payload, reply_attr);
			vfree(reply_attr);
		}

		/* 
		 * Move to next attribute. If we run out of the packet, 
		 * tlen becomes negative and we exit. 
		 */
		if ((type & ISAKMP_GEN_MASK) == ISAKMP_GEN_TV) {
			tlen -= sizeof(*attr);
			attr++;
		} else {
			alen = ntohs(attr->lorv);
			tlen -= (sizeof(*attr) + alen);
			npp = (char *)attr;
			attr = (struct isakmp_data *)
			    (npp + sizeof(*attr) + alen);
		}
	}

	reply = (struct isakmp_pl_attr *)payload->v;
	reply->h.len = htons(payload->l);
	reply->type = ISAKMP_CFG_ACK;
	reply->id = attrpl->id;

	plog(ASL_LEVEL_DEBUG, 
		     "Sending MODE_CFG ACK\n");

	error = isakmp_cfg_send(iph1, payload, 
	    ISAKMP_NPTYPE_ATTR, ISAKMP_FLAG_E, 0, 0, msg);

	if (iph1->mode_cfg->flags & ISAKMP_CFG_DELETE_PH1) {
		if (FSM_STATE_IS_ESTABLISHED(iph1->status))
			isakmp_info_send_d1(iph1);
		isakmp_ph1expire(iph1);
		iph1 = NULL;
	}
	vfree(payload);

	/* 
	 * If required, request ISAKMP mode config information: ignore rekeys
	 */
	if ((iph1 != NULL) && (!iph1->is_rekey) && (iph1->rmconf->mode_cfg) && (error == 0))
		error = isakmp_cfg_getconfig(iph1);

	return error;
}


static vchar_t *
buffer_cat(s, append)
	vchar_t *s;
	vchar_t *append;
{
	vchar_t *new;

	new = vmalloc(s->l + append->l);
	if (new == NULL) {
		plog(ASL_LEVEL_ERR, 
		    "Cannot allocate memory\n");
		return s;
	}

	memcpy(new->v, s->v, s->l);
	memcpy(new->v + s->l, append->v, append->l);

	vfree(s);
	return new;
}

static vchar_t *
isakmp_cfg_net(iph1, attr)
	phase1_handle_t *iph1;
	struct isakmp_data *attr;
{
	int type;
	int confsource;

	type = ntohs(attr->type);

	/* 
	 * Don't give an address to a peer that did not succeed Xauth
	 */
	if (xauth_check(iph1) != 0) {
		plog(ASL_LEVEL_ERR, 
		    "Attempt to start phase config whereas Xauth failed\n");
		return NULL;
	}

	confsource = isakmp_cfg_config.confsource;
	/*
	 * If we have to fall back to a local
	 * configuration source, we will jump
	 * back to this point.
	 */

	switch(type) {
	case INTERNAL_IP4_ADDRESS:
		switch(confsource) {
		case ISAKMP_CFG_CONF_LOCAL:
			if (isakmp_cfg_getport(iph1) == -1) {
				plog(ASL_LEVEL_ERR, 
				    "Port pool depleted\n");
				break;
			}

			iph1->mode_cfg->addr4.s_addr = 
			    htonl(ntohl(isakmp_cfg_config.network4) 
			    + iph1->mode_cfg->port);
			iph1->mode_cfg->flags |= ISAKMP_CFG_ADDR4_LOCAL;
			break;

		default:
			plog(ASL_LEVEL_ERR, 
			    "Unexpected confsource\n");
		}
			
		return isakmp_cfg_addr4(iph1, 
		    attr, &iph1->mode_cfg->addr4.s_addr);
		break;

	case INTERNAL_IP4_NETMASK:
		switch(confsource) {
		case ISAKMP_CFG_CONF_LOCAL:
			iph1->mode_cfg->mask4.s_addr 
			    = isakmp_cfg_config.netmask4;
			iph1->mode_cfg->flags |= ISAKMP_CFG_MASK4_LOCAL;
			break;

		default:
			plog(ASL_LEVEL_ERR, 
			    "Unexpected confsource\n");
		}
		return isakmp_cfg_addr4(iph1, attr, 
		    &iph1->mode_cfg->mask4.s_addr);
		break;

	case INTERNAL_IP4_DNS:
		return isakmp_cfg_addr4_list(iph1, 
		    attr, &isakmp_cfg_config.dns4[0], 
		    isakmp_cfg_config.dns4_index);
		break;

	case INTERNAL_IP4_NBNS:
		return isakmp_cfg_addr4_list(iph1, 
		    attr, &isakmp_cfg_config.nbns4[0], 
		    isakmp_cfg_config.nbns4_index);
		break;

	case INTERNAL_IP4_SUBNET:
		return isakmp_cfg_addr4(iph1, 
		    attr, &isakmp_cfg_config.network4);
		break;

	default:
		plog(ASL_LEVEL_ERR, "Unexpected type %d\n", type);
		break;
	}
	return NULL;
}

#if 0
static vchar_t *
isakmp_cfg_void(iph1, attr)
	phase1_handle_t *iph1;
	struct isakmp_data *attr;
{
	vchar_t *buffer;
	struct isakmp_data *new;

	if ((buffer = vmalloc(sizeof(*attr))) == NULL) {
		plog(ASL_LEVEL_ERR, "Cannot allocate memory\n");
		return NULL;
	}

	new = (struct isakmp_data *)buffer->v;

	new->type = attr->type;
	new->lorv = htons(0);

	return buffer;
}
#endif

vchar_t *
isakmp_cfg_copy(iph1, attr)
	phase1_handle_t *iph1;
	struct isakmp_data *attr;
{
	vchar_t *buffer;
	size_t len = 0;

	if ((ntohs(attr->type) & ISAKMP_GEN_MASK) == ISAKMP_GEN_TLV)
		len = ntohs(attr->lorv);

	if ((buffer = vmalloc(sizeof(*attr) + len)) == NULL) {
		plog(ASL_LEVEL_ERR, "Cannot allocate memory\n");
		return NULL;
	}

	memcpy(buffer->v, attr, sizeof(*attr) + ntohs(attr->lorv));

	return buffer;
}

vchar_t *
isakmp_cfg_short(iph1, attr, value)
	phase1_handle_t *iph1;
	struct isakmp_data *attr;
	int value;
{
	vchar_t *buffer;
	struct isakmp_data *new;
	int type;

	if ((buffer = vmalloc(sizeof(*attr))) == NULL) {
		plog(ASL_LEVEL_ERR, "Cannot allocate memory\n");
		return NULL;
	}

	new = (struct isakmp_data *)buffer->v;
	type = ntohs(attr->type) & ~ISAKMP_GEN_MASK;

	new->type = htons(type | ISAKMP_GEN_TV);
	new->lorv = htons(value);

	return buffer;
}

vchar_t *
isakmp_cfg_varlen(iph1, attr, string, len)
	phase1_handle_t *iph1;
	struct isakmp_data *attr;
	char *string;
	size_t len;
{
	vchar_t *buffer;
	struct isakmp_data *new;
	char *data;

	if ((buffer = vmalloc(sizeof(*attr) + len)) == NULL) {
		plog(ASL_LEVEL_ERR, "Cannot allocate memory\n");
		return NULL;
	}

	new = (struct isakmp_data *)buffer->v;

	new->type = attr->type;
	new->lorv = htons(len);
	data = (char *)(new + 1);

	memcpy(data, string, len);
	
	return buffer;
}
vchar_t *
isakmp_cfg_string(iph1, attr, string)
	phase1_handle_t *iph1;
	struct isakmp_data *attr;
	char *string;
{
	size_t len = strlen(string);
	return isakmp_cfg_varlen(iph1, attr, string, len);
}

static vchar_t *
isakmp_cfg_addr4(iph1, attr, addr)
	phase1_handle_t *iph1;
	struct isakmp_data *attr;
	in_addr_t *addr;
{
	vchar_t *buffer;
	struct isakmp_data *new;
	size_t len;

	len = sizeof(*addr);
	if ((buffer = vmalloc(sizeof(*attr) + len)) == NULL) {
		plog(ASL_LEVEL_ERR, "Cannot allocate memory\n");
		return NULL;
	}

	new = (struct isakmp_data *)buffer->v;

	new->type = attr->type;
	new->lorv = htons(len);
	memcpy(new + 1, addr, len);
	
	return buffer;
}

static vchar_t *
isakmp_cfg_addr4_list(iph1, attr, addr, nbr)
	phase1_handle_t *iph1;
	struct isakmp_data *attr;
	in_addr_t *addr;
	int nbr;
{
	int error = -1;
	vchar_t *buffer = NULL;
	vchar_t *bufone = NULL;
	struct isakmp_data *new;
	size_t len;
	int i;

	len = sizeof(*addr);
	if ((buffer = vmalloc(0)) == NULL) {
		plog(ASL_LEVEL_ERR, "Cannot allocate memory\n");
		goto out;
	}
	for(i = 0; i < nbr; i++) {
		if ((bufone = vmalloc(sizeof(*attr) + len)) == NULL) {
			plog(ASL_LEVEL_ERR, 
			    "Cannot allocate memory\n");
			goto out;
		}
		new = (struct isakmp_data *)bufone->v;
		new->type = attr->type;
		new->lorv = htons(len);
		memcpy(new + 1, &addr[i], len);
		new += (len + sizeof(*attr));
		buffer = buffer_cat(buffer, bufone);
		vfree(bufone);
	}

	error = 0;

out:
	if ((error != 0) && (buffer != NULL)) {
		vfree(buffer);
		buffer = NULL;
	}

	return buffer;
}

struct isakmp_ivm *
isakmp_cfg_newiv(iph1, msgid)
	phase1_handle_t *iph1;
	u_int32_t msgid;
{
	struct isakmp_cfg_state *ics = iph1->mode_cfg;

	if (ics == NULL) {
		plog(ASL_LEVEL_ERR, 
		    "isakmp_cfg_newiv called without mode config state\n");
		return NULL;
	}

	if (ics->ivm != NULL)
		oakley_delivm(ics->ivm);

	ics->ivm = oakley_newiv2(iph1, msgid);
	ics->last_msgid = msgid;

	return ics->ivm;
}

/* Derived from isakmp_info_send_common */
int
isakmp_cfg_send(iph1, payload, np, flags, new_exchange, retry_count, msg)
	phase1_handle_t *iph1;
	vchar_t *payload;
	u_int32_t np;
	int flags;
	int new_exchange;
	int retry_count;
    vchar_t *msg;
{
	phase2_handle_t *iph2 = NULL;
	vchar_t *hash = NULL;
	struct isakmp *isakmp;
	struct isakmp_gen *gen;
	char *p;
	int tlen;
	int error = -1;
	struct isakmp_cfg_state *ics = iph1->mode_cfg;

	/* Check if phase 1 is established */
	if ((!FSM_STATE_IS_ESTABLISHED(iph1->status)) || 
	    (iph1->local == NULL) ||
	    (iph1->remote == NULL)) {
		plog(ASL_LEVEL_ERR, 
		    "ISAKMP mode config exchange with immature phase 1\n");
		goto end;
	}

	/* add new entry to isakmp status table */
	iph2 = ike_session_newph2(ISAKMP_VERSION_NUMBER_IKEV1, PHASE2_TYPE_CFG);
	if (iph2 == NULL) {
		plog(ASL_LEVEL_ERR,
			 "failed to allocate ph2");
		goto end;
	}

	iph2->dst = dupsaddr(iph1->remote);
	if (iph2->dst == NULL) {
		plog(ASL_LEVEL_ERR,
			 "failed to duplicate remote address");
		ike_session_delph2(iph2);
		goto end;
	}
	iph2->src = dupsaddr(iph1->local);
	if (iph2->src == NULL) {
		plog(ASL_LEVEL_ERR,
			 "failed to duplicate local address");
		ike_session_delph2(iph2);
		goto end;
	}

	switch (iph1->remote->ss_family) {
	case AF_INET:
#if (!defined(ENABLE_NATT)) || (defined(BROKEN_NATT))
		((struct sockaddr_in *)iph2->dst)->sin_port = 0;
		((struct sockaddr_in *)iph2->src)->sin_port = 0;
#endif
		break;
#ifdef INET6
	case AF_INET6:
#if (!defined(ENABLE_NATT)) || (defined(BROKEN_NATT))
		((struct sockaddr_in6 *)iph2->dst)->sin6_port = 0;
		((struct sockaddr_in6 *)iph2->src)->sin6_port = 0;
#endif
		break;
#endif
	default:
		plog(ASL_LEVEL_ERR, 
			"invalid family: %d\n", iph1->remote->ss_family);
		ike_session_delph2(iph2);
		goto end;
	}
	iph2->side = INITIATOR;
	fsm_set_state(&iph2->status, IKEV1_STATE_INFO);  

	if (new_exchange)
		iph2->msgid = isakmp_newmsgid2(iph1);
	else
		iph2->msgid = iph1->msgid;

	/* get IV and HASH(1) if skeyid_a was generated. */
	if (iph1->skeyid_a != NULL) {
		if (new_exchange) {
			if (isakmp_cfg_newiv(iph1, iph2->msgid) == NULL) {
				plog(ASL_LEVEL_ERR, 
					 "failed to generate IV");
				ike_session_delph2(iph2);
				goto end;
			}
		}

		/* generate HASH(1) */
		hash = oakley_compute_hash1(iph1, iph2->msgid, payload);
		if (hash == NULL) {
			plog(ASL_LEVEL_ERR, 
				 "failed to generate HASH");
			ike_session_delph2(iph2);
			goto end;
		}

		/* initialized total buffer length */
		tlen = hash->l;
		tlen += sizeof(*gen);
	} else {
		/* IKE-SA is not established */
		hash = NULL;

		/* initialized total buffer length */
		tlen = 0;
	}
	if ((flags & ISAKMP_FLAG_A) == 0)
		iph2->flags = (hash == NULL ? 0 : ISAKMP_FLAG_E);
	else
		iph2->flags = (hash == NULL ? 0 : ISAKMP_FLAG_A);

	ike_session_link_ph2_to_ph1(iph1, iph2);

	tlen += sizeof(*isakmp) + payload->l;

	/* create buffer for isakmp payload */
	iph2->sendbuf = vmalloc(tlen);
	if (iph2->sendbuf == NULL) { 
		plog(ASL_LEVEL_ERR, 
			"failed to get buffer to send.\n");
		goto err;
	}

	/* create isakmp header */
	isakmp = (struct isakmp *)iph2->sendbuf->v;
	memcpy(&isakmp->i_ck, &iph1->index.i_ck, sizeof(cookie_t));
	memcpy(&isakmp->r_ck, &iph1->index.r_ck, sizeof(cookie_t));
	isakmp->np = hash == NULL ? (np & 0xff) : ISAKMP_NPTYPE_HASH;
	isakmp->v = iph1->version;
	isakmp->etype = ISAKMP_ETYPE_CFG;
	isakmp->flags = iph2->flags;
	memcpy(&isakmp->msgid, &iph2->msgid, sizeof(isakmp->msgid));
	isakmp->len = htonl(tlen);
	p = (char *)(isakmp + 1);

	/* create HASH payload */
	if (hash != NULL) {
		gen = (struct isakmp_gen *)p;
		gen->np = np & 0xff;
		gen->len = htons(sizeof(*gen) + hash->l);
		p += sizeof(*gen);
		memcpy(p, hash->v, hash->l);
		p += hash->l;
	}

	/* add payload */
	memcpy(p, payload->v, payload->l);
	p += payload->l;

#ifdef HAVE_PRINT_ISAKMP_C
	isakmp_printpacket(iph2->sendbuf, iph1->local, iph1->remote, 1);
#endif
	
	plog(ASL_LEVEL_DEBUG, "MODE_CFG packet to send\n");

	/* encoding */
	if (ISSET(isakmp->flags, ISAKMP_FLAG_E)) {
		vchar_t *tmp;

		tmp = oakley_do_encrypt(iph1, iph2->sendbuf,
			ics->ivm->ive, ics->ivm->iv);
		VPTRINIT(iph2->sendbuf);
		if (tmp == NULL) {
			plog(ASL_LEVEL_ERR, 
				 "failed to encrypt packet");
			goto err;
		}
		iph2->sendbuf = tmp;
	}

	/* HDR*, HASH(1), ATTR */
	
	if (retry_count > 0) {
		iph2->retry_counter = retry_count;
		if (isakmp_ph2resend(iph2) < 0) {
			plog(ASL_LEVEL_ERR, 
				 "failed to resend packet");
			VPTRINIT(iph2->sendbuf);
			goto err;
		}
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKEV1_CFG_RETRANSMIT,
								CONSTSTR("Mode-Config retransmit"),
								CONSTSTR(NULL));
		error = 0;
		goto end;
	}
	
	if (isakmp_send(iph2->ph1, iph2->sendbuf) < 0) {
		plog(ASL_LEVEL_ERR, 
			 "failed to send packet");
		VPTRINIT(iph2->sendbuf);
		goto err;
	}
       if (msg) {
               /* the sending message is added to the received-list. */
               if (ike_session_add_recvdpkt(iph1->remote, iph1->local, iph2->sendbuf, msg,
                                PH2_NON_ESP_EXTRA_LEN(iph2, iph2->sendbuf), PH1_FRAG_FLAGS(iph1)) == -1) {
                       plog(ASL_LEVEL_ERR , 
                            "failed to add a response packet to the tree.\n");
               }
       }
    
	plog(ASL_LEVEL_DEBUG, 
		"sendto mode config %s.\n", s_isakmp_nptype(np));

	/*
	 * XXX We might need to resend the message...
	 */

	error = 0;
	VPTRINIT(iph2->sendbuf);

	IPSECSESSIONTRACEREVENT(iph1->parent_session,
							IPSECSESSIONEVENTCODE_IKE_PACKET_TX_SUCC,
							CONSTSTR("Mode-Config message"),
							CONSTSTR(NULL));
	
err:
	if (error) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKE_PACKET_TX_FAIL,
								CONSTSTR("Mode-Config message"),
								CONSTSTR("Failed to transmit Mode-Config message"));
	}
	ike_session_unlink_phase2(iph2);
end:
	if (hash)
		vfree(hash);
	return error;
}


void
isakmp_cfg_rmstate(phase1_handle_t *iph1)
{
	struct isakmp_cfg_state **state = &iph1->mode_cfg;
    
    
    if (*state == NULL)
        return;
    
	if ((*state)->flags & ISAKMP_CFG_PORT_ALLOCATED)
		isakmp_cfg_putport(iph1, (*state)->port);
    
	/* Delete the IV if it's still there */
	if((*state)->ivm) {
		oakley_delivm((*state)->ivm);
		(*state)->ivm = NULL;
	}
    
	/* Free any allocated splitnet lists */
	if((*state)->split_include != NULL)
		splitnet_list_free((*state)->split_include,
                           &(*state)->include_count);
	if((*state)->split_local != NULL)
		splitnet_list_free((*state)->split_local,
                           &(*state)->local_count);
    
	xauth_rmstate(&(*state)->xauth);
	
	if ((*state)->attr_list)
		vfree((*state)->attr_list);
    
	racoon_free((*state));
	(*state) = NULL;
    
	return;
}

struct isakmp_cfg_state *
isakmp_cfg_mkstate(void) 
{
	struct isakmp_cfg_state *state;

	if ((state = racoon_malloc(sizeof(*state))) == NULL) {
		plog(ASL_LEVEL_ERR, 
		    "Cannot allocate memory for mode config state\n");
		return NULL;
	}
	memset(state, 0, sizeof(*state));

	return state;
}

int 
isakmp_cfg_getport(iph1)
	phase1_handle_t *iph1;
{
	unsigned int i;
	size_t size = isakmp_cfg_config.pool_size;

	if (iph1->mode_cfg->flags & ISAKMP_CFG_PORT_ALLOCATED)
		return iph1->mode_cfg->port;

	if (isakmp_cfg_config.port_pool == NULL) {
		plog(ASL_LEVEL_ERR, 
		    "isakmp_cfg_config.port_pool == NULL\n");
		return -1;
	}

	for (i = 0; i < size; i++) {
		if (isakmp_cfg_config.port_pool[i].used == 0)
			break;
	}

	if (i == size) {
		plog(ASL_LEVEL_ERR, 
		    "No more addresses available\n");
			return -1;
	}

	isakmp_cfg_config.port_pool[i].used = 1;

	plog(ASL_LEVEL_INFO, "Using port %d\n", i);

	iph1->mode_cfg->flags |= ISAKMP_CFG_PORT_ALLOCATED;
	iph1->mode_cfg->port = i;

	return i;
}

int 
isakmp_cfg_putport(iph1, index)
	phase1_handle_t *iph1;
	unsigned int index;
{
	if (isakmp_cfg_config.port_pool == NULL) {
		plog(ASL_LEVEL_ERR, 
		    "isakmp_cfg_config.port_pool == NULL\n");
		return -1;
	}

	if (isakmp_cfg_config.port_pool[index].used == 0) {
		plog(ASL_LEVEL_ERR, 
		    "Attempt to release an unallocated address (port %d)\n",
		    index);
		return -1;
	}

	isakmp_cfg_config.port_pool[index].used = 0;
	iph1->mode_cfg->flags &= ISAKMP_CFG_PORT_ALLOCATED;

	plog(ASL_LEVEL_INFO, "Released port %d\n", index);

	return 0;
}

	
int 
isakmp_cfg_getconfig(iph1)
	phase1_handle_t *iph1;
{
	vchar_t *buffer;
	struct isakmp_pl_attr *attrpl;
	struct isakmp_data *attr;
	size_t len;
	vchar_t *version = NULL;
	int error;
	int attrcount;
	int i;
	int attrlist[] = {
		INTERNAL_IP4_ADDRESS,
		INTERNAL_IP4_NETMASK,
		INTERNAL_IP4_DNS,
		INTERNAL_IP4_NBNS,
		INTERNAL_ADDRESS_EXPIRY,
		APPLICATION_VERSION,
		UNITY_BANNER,
		UNITY_DEF_DOMAIN,
		UNITY_SPLITDNS_NAME,
		UNITY_SPLIT_INCLUDE,
		UNITY_LOCAL_LAN,
	};

	attrcount = sizeof(attrlist) / sizeof(*attrlist);
	len = sizeof(*attrpl) + sizeof(*attr) * attrcount;
	
	if (iph1->started_by_api) {
		if (iph1->remote->ss_family == AF_INET) {
			struct vpnctl_socket_elem *sock_elem;
			struct bound_addr *bound_addr;
			u_int32_t address;

			address = ((struct sockaddr_in *)iph1->remote)->sin_addr.s_addr;
			LIST_FOREACH(sock_elem, &lcconf->vpnctl_comm_socks, chain) {
				LIST_FOREACH(bound_addr, &sock_elem->bound_addresses, chain) {
					if (bound_addr->address == address) {
						if ((version = bound_addr->version))
							len += bound_addr->version->l;
						break;
					}
				}
			}
		}
	}
	
	if ((buffer = vmalloc(len)) == NULL) {
		plog(ASL_LEVEL_ERR, "Cannot allocate memory\n");
		return -1;
	}

	attrpl = (struct isakmp_pl_attr *)buffer->v;
	attrpl->h.len = htons(len);
	attrpl->type = ISAKMP_CFG_REQUEST;
	attrpl->id = htons((u_int16_t)(eay_random() & 0xffff));

	attr = (struct isakmp_data *)(attrpl + 1);

	for (i = 0; i < attrcount; i++) {
		switch (attrlist[i]) {
			case APPLICATION_VERSION:
				if (version) {
					attr->type = htons(attrlist[i]);
					attr->lorv = htons(version->l);
					memcpy(attr + 1, version->v, version->l);
					attr = (struct isakmp_data *)(((char *)(attr + 1)) + version->l);
					break;
				} else /* fall thru */;
			default:
				attr->type = htons(attrlist[i]);
				attr->lorv = htons(0);
				attr++;
				break;
		}
	}

	plog(ASL_LEVEL_DEBUG, 
		    "Sending MODE_CFG REQUEST\n");

	error = isakmp_cfg_send(iph1, buffer,
	    ISAKMP_NPTYPE_ATTR, ISAKMP_FLAG_E, 1, iph1->rmconf->retry_counter, NULL);

	vfree(buffer);

	IPSECLOGASLMSG("IPSec Network Configuration requested.\n");

	return error;
}

static void
isakmp_cfg_getaddr4(attr, ip)
	struct isakmp_data *attr;
	struct in_addr *ip;
{
	size_t alen = ntohs(attr->lorv);
	in_addr_t *addr;

	if (alen != sizeof(*ip)) {
		plog(ASL_LEVEL_ERR, "Bad IPv4 address len\n");
		return;
	}

	addr = ALIGNED_CAST(in_addr_t *)(attr + 1);     // Wcast-align fix (void*) - attr comes from packet data in a vchar_t
	ip->s_addr = *addr;

	return;
}

static void
isakmp_cfg_appendaddr4(attr, ip, num, max)
	struct isakmp_data *attr;
	struct in_addr *ip;
	int *num;
	int max;
{
	size_t alen = ntohs(attr->lorv);
	in_addr_t *addr;

	if (alen != sizeof(*ip)) {
		plog(ASL_LEVEL_ERR, "Bad IPv4 address len\n");
		return;
	}
	if (*num == max) {
		plog(ASL_LEVEL_ERR, "Too many addresses given\n");
		return;
	}

	addr = ALIGNED_CAST(in_addr_t *)(attr + 1);      // Wcast-align fix (void*) - attr comes from packet data in a vchar_t
	ip->s_addr = *addr;
	(*num)++;

	return;
}

static void
isakmp_cfg_getstring(attr, str)
	struct isakmp_data *attr;
	char *str;
{
	size_t alen = ntohs(attr->lorv);
	char *src;
	src = (char *)(attr + 1);

	memcpy(str, src, (alen > MAXPATHLEN ? MAXPATHLEN : alen));

	return;
}

#define IP_MAX 40

void
isakmp_cfg_iplist_to_str(dest, count, addr, withmask)
	char *dest;
	int count;
	void *addr;
	int withmask;
{
	int i;
	int p;
	int l;
	struct unity_network tmp;
	for(i = 0, p = 0; i < count; i++) {
		if(withmask == 1)
			l = sizeof(struct unity_network);
		else
			l = sizeof(struct in_addr);
		memcpy(&tmp, addr, l);
		addr += l;
		if((uint32_t)tmp.addr4.s_addr == 0)
			break;
	
		inet_ntop(AF_INET, &tmp.addr4, dest + p, IP_MAX);
		p += strlen(dest + p);
		if(withmask == 1) {
			dest[p] = '/';
			p++;
			inet_ntop(AF_INET, &tmp.mask4, dest + p, IP_MAX);
			p += strlen(dest + p);
		}
		dest[p] = ' ';
		p++;
	}
	if(p > 0)
		dest[p-1] = '\0';
	else
		dest[0] = '\0';
}

int
isakmp_cfg_resize_pool(size)
	int size;
{
	struct isakmp_cfg_port *new_pool;
	size_t len;
	int i;

	if (size == isakmp_cfg_config.pool_size)
		return 0;

	plog(ASL_LEVEL_INFO, 
	    "Resize address pool from %zu to %d\n",
	    isakmp_cfg_config.pool_size, size);

	/* If a pool already exists, check if we can shrink it */
	if ((isakmp_cfg_config.port_pool != NULL) &&
	    (size < isakmp_cfg_config.pool_size)) {
		for (i = isakmp_cfg_config.pool_size-1; i >= size; --i) {
			if (isakmp_cfg_config.port_pool[i].used) {
				plog(ASL_LEVEL_ERR, 
				    "resize pool from %zu to %d impossible "
				    "port %d is in use\n", 
				    isakmp_cfg_config.pool_size, size, i);
				size = i;
				break;
			}	
		}
	}

	len = size * sizeof(*isakmp_cfg_config.port_pool);
	new_pool = racoon_realloc(isakmp_cfg_config.port_pool, len);
	if (new_pool == NULL) {
		plog(ASL_LEVEL_ERR, 
		    "resize pool from %zu to %d impossible: %s",
		    isakmp_cfg_config.pool_size, size, strerror(errno));
		return -1;
	}

	/* If size increase, intialize correctly the new records */
	if (size > isakmp_cfg_config.pool_size) {
		size_t unit;
		size_t old_size;

		unit =  sizeof(*isakmp_cfg_config.port_pool);
		old_size = isakmp_cfg_config.pool_size;

		bzero((char *)new_pool + (old_size * unit), 
		    (size - old_size) * unit);
	}

	isakmp_cfg_config.port_pool = new_pool;
	isakmp_cfg_config.pool_size = size;

	return 0;
}

int
isakmp_cfg_init(cold) 
	int cold;
{
	int i;
#if 0
	int error;
#endif

	isakmp_cfg_config.network4 = (in_addr_t)0x00000000;
	isakmp_cfg_config.netmask4 = (in_addr_t)0x00000000;
	for (i = 0; i < MAXNS; i++)
		isakmp_cfg_config.dns4[i] = (in_addr_t)0x00000000;
	isakmp_cfg_config.dns4_index = 0;
	for (i = 0; i < MAXWINS; i++)
		isakmp_cfg_config.nbns4[i] = (in_addr_t)0x00000000;
	isakmp_cfg_config.nbns4_index = 0;
	if (cold != ISAKMP_CFG_INIT_COLD) {
		if (isakmp_cfg_config.port_pool) {
			racoon_free(isakmp_cfg_config.port_pool);
		}
	}
	isakmp_cfg_config.port_pool = NULL;
	isakmp_cfg_config.pool_size = 0;
	isakmp_cfg_config.authsource = ISAKMP_CFG_AUTH_SYSTEM;
	isakmp_cfg_config.groupsource = ISAKMP_CFG_GROUP_SYSTEM;
	if (cold != ISAKMP_CFG_INIT_COLD) {
		if (isakmp_cfg_config.grouplist != NULL) {
			for (i = 0; i < isakmp_cfg_config.groupcount; i++)
				racoon_free(isakmp_cfg_config.grouplist[i]);
			racoon_free(isakmp_cfg_config.grouplist);
		}
	}
	isakmp_cfg_config.grouplist = NULL;
	isakmp_cfg_config.groupcount = 0;
	isakmp_cfg_config.confsource = ISAKMP_CFG_CONF_LOCAL;
	isakmp_cfg_config.accounting = ISAKMP_CFG_ACCT_NONE;
	isakmp_cfg_config.auth_throttle = THROTTLE_PENALTY;
	strlcpy(isakmp_cfg_config.default_domain, ISAKMP_CFG_DEFAULT_DOMAIN,
	    sizeof(isakmp_cfg_config.default_domain));
	strlcpy(isakmp_cfg_config.motd, ISAKMP_CFG_MOTD, sizeof(isakmp_cfg_config.motd));

	if (cold != ISAKMP_CFG_INIT_COLD )
		if (isakmp_cfg_config.splitnet_list != NULL)
			splitnet_list_free(isakmp_cfg_config.splitnet_list,
				&isakmp_cfg_config.splitnet_count);
	isakmp_cfg_config.splitnet_list = NULL;
	isakmp_cfg_config.splitnet_count = 0;
	isakmp_cfg_config.splitnet_type = 0;

	isakmp_cfg_config.pfs_group = 0;
	isakmp_cfg_config.save_passwd = 0;

	if (cold != ISAKMP_CFG_INIT_COLD )
		if (isakmp_cfg_config.splitdns_list != NULL)
			racoon_free(isakmp_cfg_config.splitdns_list);
	isakmp_cfg_config.splitdns_list = NULL;
	isakmp_cfg_config.splitdns_len = 0;

#if 0
	if (cold == ISAKMP_CFG_INIT_COLD) {
		if ((error = isakmp_cfg_resize_pool(ISAKMP_CFG_MAX_CNX)) != 0)
			return error;
	}
#endif

	return 0;
}

