/*
 * Copyright (c) 2007 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

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
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <net/pfkeyv2.h>

#include <netinet/in.h>
#ifndef HAVE_NETINET6_IPSEC
#include <netinet/ipsec.h>
#else 
#include <netinet6/ipsec.h>
#endif


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef ENABLE_HYBRID
#include <resolv.h>
#endif

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "sockmisc.h"
#include "debug.h"
#include "handler.h"
#include "schedule.h"
#include "localconf.h"
#include "remoteconf.h"
#include "grabmyaddr.h"
#include "isakmp_var.h"
#include "isakmp.h"
#include "oakley.h"
#include "pfkey.h"
#include "ipsec_doi.h"
#include "isakmp_inf.h"
#ifdef ENABLE_HYBRID
#include "isakmp_cfg.h"
#include "isakmp_unity.h"
#endif
#include "session.h"
#include "gcmalloc.h"
#include "sainfo.h"
#include "ipsec_doi.h"
#include "nattraversal.h"
#include "fsm.h"

#include "vpn_control.h"
#include "vpn_control_var.h"
#include "strnames.h"
#include "ike_session.h"
#include "ipsecMessageTracer.h"


static int vpn_get_ph2pfs (phase1_handle_t *);

int
vpn_connect(struct bound_addr *srv, int oper)
{
	int error = -1;
	struct sockaddr_storage *dst;
	struct remoteconf *rmconf;
	struct sockaddr_storage *remote = NULL;
	struct sockaddr_storage *local = NULL;
	u_int16_t port;

	dst = racoon_calloc(1, sizeof(struct sockaddr_storage));	// this should come from the bound_addr parameter
	if (dst == NULL)
		goto out;
	((struct sockaddr_in *)(dst))->sin_len = sizeof(struct sockaddr_in);
	((struct sockaddr_in *)(dst))->sin_family = AF_INET;
	((struct sockaddr_in *)(dst))->sin_port = 500;
	((struct sockaddr_in *)(dst))->sin_addr.s_addr = srv->address;

	/* find appropreate configuration */
	rmconf = getrmconf(dst);
	if (rmconf == NULL) {
		plog(ASL_LEVEL_ERR, 
			"no configuration found "
			"for %s\n", saddrwop2str((struct sockaddr *)dst));
		goto out1;
	}
	
	/*
	 * Find the source address
	 */
	if (rmconf->forced_local != NULL) {
		if ((local = dupsaddr(rmconf->forced_local)) == NULL) {
			plog(ASL_LEVEL_ERR, "failed to duplicate local address\n");
			goto out1;
		}
	} else if ((local = getlocaladdr((struct sockaddr *)dst)) == NULL) {
		plog(ASL_LEVEL_ERR, "cannot get local address\n");
		goto out1;
	}
	
	/* get remote IP address and port number. */
	if ((remote = dupsaddr(dst)) == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to duplicate address\n");
		goto out1;
	}

	switch (remote->ss_family) {
	case AF_INET:
		((struct sockaddr_in *)remote)->sin_port =
			((struct sockaddr_in *)rmconf->remote)->sin_port;
		break;
#ifdef INET6
	case AF_INET6:
		((struct sockaddr_in6 *)remote)->sin6_port =
			((struct sockaddr_in6 *)rmconf->remote)->sin6_port;
		break;
#endif
	default:
		plog(ASL_LEVEL_ERR, 
			"invalid family: %d\n",
			remote->ss_family);
		goto out1;
		break;
	}

	port = ntohs(getmyaddrsport(local));
	if (set_port(local, port) == NULL) 
		goto out1;

	plog(ASL_LEVEL_INFO, 
		"accept a request to establish IKE-SA: "
		"%s\n", saddrwop2str((struct sockaddr *)remote));

	IPSECLOGASLMSG("IPSec connecting to server %s\n",
				   saddrwop2str((struct sockaddr *)remote));
	if (ikev1_ph1begin_i(NULL, rmconf, remote, local, oper) < 0)
		goto out1;
	error = 0;

out1:
	if (dst != NULL)
		racoon_free(dst);
	if (local != NULL)
		racoon_free(local);
	if (remote != NULL)
		racoon_free(remote);
out:

	return error;
}

int
vpn_disconnect(struct bound_addr *srv, const char *reason)
{
	union {									// Wcast-align fix - force alignment
        struct sockaddr_storage	ss;
        struct sockaddr_in  saddr;
    } u;

	bzero(&u.saddr, sizeof(u.saddr));
	u.saddr.sin_len = sizeof(u.saddr);
	u.saddr.sin_addr.s_addr = srv->address;
	u.saddr.sin_port = 0;
	u.saddr.sin_family = AF_INET;

	IPSECLOGASLMSG("IPSec disconnecting from server %s\n",
				   saddrwop2str((struct sockaddr *)&u.ss));	

	ike_sessions_stopped_by_controller(&u.ss,
                                       0,
                                       reason);
	if (ike_session_purgephXbydstaddrwop(&u.ss) > 0) {
		return 0;
	} else {
		return -1;
	}
}

int
vpn_start_ph2(struct bound_addr *addr, struct vpnctl_cmd_start_ph2 *pkt)
{
	struct vpnctl_sa_selector *selector_ptr;
	struct vpnctl_algo *algo_ptr, *next_algo;
	int					i, j, defklen;
	struct sainfoalg	*new_algo;
	struct sainfo		*new_sainfo = NULL, *check;
	u_int16_t			class, algorithm, keylen;
	phase1_handle_t	*ph1;
	struct sockaddr_in	saddr;
	
	struct id {
		u_int8_t type;		/* ID Type */
		u_int8_t proto_id;	/* Protocol ID */
		u_int16_t port;		/* Port */
		u_int32_t addr;		/* IPv4 address */
		u_int32_t mask;
	} *id_ptr;
	
	/* verify ph1 exists */	
	bzero(&saddr, sizeof(saddr));
	saddr.sin_len = sizeof(saddr);
	saddr.sin_addr.s_addr = addr->address;
	saddr.sin_port = 0;
	saddr.sin_family = AF_INET;
	ph1 = ike_session_getph1bydstaddrwop(NULL, (struct sockaddr_storage *)(&saddr));
	if (ph1 == NULL) {
		plog(ASL_LEVEL_ERR,
			"Cannot start Phase 2 - no Phase 1 found.\n");
		return -1;
	}
	if (!FSM_STATE_IS_ESTABLISHED(ph1->status)) {
		plog(ASL_LEVEL_ERR,
			 "Cannot start Phase 2 - Phase 1 not established.\n");
		return -1;
	}

	selector_ptr = (struct vpnctl_sa_selector *)(pkt + 1);
	algo_ptr = (struct vpnctl_algo *)(selector_ptr + ntohs(pkt->selector_count));

	for (i = 0; i < ntohs(pkt->selector_count); i++, selector_ptr++) {
		new_sainfo = create_sainfo();
		if (new_sainfo == NULL) {
			plog(ASL_LEVEL_ERR, 
				"Unable to allocate sainfo struct.\n");
			goto fail;
		}
		
		if (ntohl(selector_ptr->src_tunnel_mask) == 0xFFFFFFFF)
			new_sainfo->idsrc = vmalloc(sizeof(struct id) - sizeof(u_int32_t));
		else
			new_sainfo->idsrc = vmalloc(sizeof(struct id));
		if (new_sainfo->idsrc == NULL) {
			plog(ASL_LEVEL_ERR, 
				 "Unable to allocate id struct.\n");
			goto fail;
		}
		if (selector_ptr->dst_tunnel_mask == 0xFFFFFFFF)
			new_sainfo->iddst = vmalloc(sizeof(struct id) - sizeof(u_int32_t));
		else
			new_sainfo->iddst = vmalloc(sizeof(struct id));
		if (new_sainfo->iddst == NULL) {
			plog(ASL_LEVEL_ERR, 
				 "Unable to allocate id struct.\n");
			goto fail;
		}			
		
		id_ptr = ALIGNED_CAST(struct id *)new_sainfo->idsrc->v;
		if (ntohl(selector_ptr->src_tunnel_mask) == 0xFFFFFFFF)
			id_ptr->type = IPSECDOI_ID_IPV4_ADDR;
		else {
			id_ptr->type = IPSECDOI_ID_IPV4_ADDR_SUBNET;
			id_ptr->mask = selector_ptr->src_tunnel_mask;
		}
		id_ptr->addr = selector_ptr->src_tunnel_address;
		id_ptr->port = selector_ptr->src_tunnel_port;
		id_ptr->proto_id = selector_ptr->ul_protocol;
				
		id_ptr = ALIGNED_CAST(struct id *)new_sainfo->iddst->v;
		if (selector_ptr->dst_tunnel_mask == 0xFFFFFFFF)
			id_ptr->type = IPSECDOI_ID_IPV4_ADDR;
		else {
			id_ptr->type = IPSECDOI_ID_IPV4_ADDR_SUBNET;
			id_ptr->mask = selector_ptr->dst_tunnel_mask;
		}
		id_ptr->addr = selector_ptr->dst_tunnel_address;
		id_ptr->port = selector_ptr->dst_tunnel_port;
		id_ptr->proto_id = selector_ptr->ul_protocol;		
				
		new_sainfo->dynamic = addr->address;
		new_sainfo->lifetime = ntohl(pkt->lifetime);
		
		if (ntohs(pkt->pfs_group) != 0) {
			new_sainfo->pfs_group = algtype2doi(algclass_isakmp_dh, ntohs(pkt->pfs_group));
			if (new_sainfo->pfs_group == -1) {
				plog(ASL_LEVEL_ERR, "Invalid dh group specified\n");
				goto fail;
			}
		}
		for (j = 0, next_algo = algo_ptr; j < ntohs(pkt->algo_count); j++, next_algo++) {

			new_algo = newsainfoalg();
			if (new_algo == NULL) {
				plog(ASL_LEVEL_ERR, 
					"Failed to allocate algorithm structure\n");
				goto fail;
			}

			class = ntohs(next_algo->algo_class);
			algorithm = ntohs(next_algo->algo);
			keylen = ntohs(next_algo->key_len);
			
			new_algo->alg = algtype2doi(class, algorithm);
			if (new_algo->alg == -1) {
				plog(ASL_LEVEL_ERR, "Algorithm mismatched\n");
				racoon_free(new_algo);
				goto fail;
			}

			defklen = default_keylen(class, algorithm);
			if (defklen == 0) {
				if (keylen) {
					plog(ASL_LEVEL_ERR, "keylen not allowed\n");
					racoon_free(new_algo);
					goto fail;
				}
			} else {
				if (keylen && check_keylen(class, algorithm, keylen) < 0) {
					plog(ASL_LEVEL_ERR, "invalid keylen %d\n", keylen);
					racoon_free(new_algo);
					goto fail;
				}
			}

			if (keylen)
				new_algo->encklen = keylen;
			else
				new_algo->encklen = defklen;

			/* check if it's supported algorithm by kernel */
			if (!(class == algclass_ipsec_auth && algorithm == algtype_non_auth)
			 && pk_checkalg(class, algorithm, new_algo->encklen)) {
				int a = algclass2doi(class);
				int b = new_algo->alg;
				if (a == IPSECDOI_ATTR_AUTH)
					a = IPSECDOI_PROTO_IPSEC_AH;
				plog(ASL_LEVEL_ERR, 
					"Algorithm %s not supported by the kernel (missing module?)\n", s_ipsecdoi_trns(a, b));
				racoon_free(new_algo);
				goto fail;
			}
			inssainfoalg(&new_sainfo->algs[class], new_algo);
		}

		if (new_sainfo->algs[algclass_ipsec_enc] == 0) {
			plog(ASL_LEVEL_ERR, 
				"No encryption algorithm at %s\n", sainfo2str(new_sainfo));
			goto fail;
		}
		if (new_sainfo->algs[algclass_ipsec_auth] == 0) {
			plog(ASL_LEVEL_ERR, 
				"No authentication algorithm at %s\n", sainfo2str(new_sainfo));
			goto fail;
		}
		if (new_sainfo->algs[algclass_ipsec_comp] == 0) {
			plog(ASL_LEVEL_ERR, 
				"No compression algorithm at %s\n", sainfo2str(new_sainfo));
			goto fail;
		}

		/* duplicate check */
		check = getsainfo(new_sainfo->idsrc, new_sainfo->iddst, new_sainfo->id_i, 0);
		if (check && (!check->idsrc && !new_sainfo->idsrc)) {
			plog(ASL_LEVEL_ERR, "Duplicated sainfo: %s\n", sainfo2str(new_sainfo));
			goto fail;
		}
		//plog(ASL_LEVEL_DEBUG, "create sainfo: %s\n", sainfo2str(new_sainfo));
		inssainfo(new_sainfo);
		new_sainfo = NULL;
	}
	
	return 0;
	
fail:
	if (new_sainfo)
		release_sainfo(new_sainfo);
	flushsainfo_dynamic((u_int32_t)addr->address);
	return -1;
}

static int 
vpn_get_ph2pfs(phase1_handle_t *ph1)
{
}


int
vpn_get_config(phase1_handle_t *iph1, struct vpnctl_status_phase_change **msg, size_t *msg_size)
{

	struct vpnctl_modecfg_params *params;
	struct myaddrs *myaddr;
	u_int16_t ifname_len, msize;
	u_int8_t  *cptr;
	
	*msg = NULL;
	msize = 0;
	
	if (((struct sockaddr_in *)iph1->local)->sin_family != AF_INET) {
		plog(ASL_LEVEL_ERR, 
			"IPv6 not supported for mode config.\n");
		return -1;
	}
	
	if (iph1->mode_cfg->attr_list == NULL)
		return 1;	/* haven't received configuration yet */
		
	myaddr = find_myaddr((struct sockaddr *)iph1->local, 0);
	if (myaddr == NULL) {
		plog(ASL_LEVEL_ERR, 
			"Unable to find address structure.\n");
		return -1;
	}
	
	msize = sizeof(struct vpnctl_status_phase_change) 
			+ sizeof(struct vpnctl_modecfg_params);
	msize += iph1->mode_cfg->attr_list->l;

	*msg = racoon_calloc(1, msize);
	if (*msg == NULL) {
		plog(ASL_LEVEL_ERR, 
			"Failed to allocate space for message.\n");
		return -1;
	}
	
	(*msg)->hdr.flags = htons(VPNCTL_FLAG_MODECFG_USED);
	params = (struct vpnctl_modecfg_params *)(*msg + 1);
	params->outer_local_addr = ((struct sockaddr_in *)iph1->local)->sin_addr.s_addr;
	params->outer_remote_port = htons(0);
	params->outer_local_port = htons(0);
	ifname_len = strlen(myaddr->ifname);
	memset(&params->ifname, 0, IFNAMSIZ);
	memcpy(&params->ifname, myaddr->ifname, ifname_len < IFNAMSIZ ? ifname_len : IFNAMSIZ-1);
	cptr = (u_int8_t *)(params + 1);
	memcpy(cptr, iph1->mode_cfg->attr_list->v, iph1->mode_cfg->attr_list->l);
	*msg_size = msize;

	IPSECLOGASLMSG("IPSec Network Configuration established.\n");

	return 0;
}

int
vpn_xauth_reply(u_int32_t address, void *attr_list, size_t attr_len)
{

	struct isakmp_pl_attr *reply;
	void* attr_ptr;
	vchar_t *payload = NULL;
	phase1_handle_t	*iph1;
	struct sockaddr_in	saddr;
	int error = -1;
	int tlen = attr_len;
	struct isakmp_data *attr;
	char *dataptr = (char *)attr_list;

	/* find ph1 */	
	bzero(&saddr, sizeof(saddr));
	saddr.sin_len = sizeof(saddr);
	saddr.sin_addr.s_addr = address;
	saddr.sin_port = 0;
	saddr.sin_family = AF_INET;
	iph1 = ike_session_getph1bydstaddrwop(NULL, (struct sockaddr_storage *)(&saddr));
	if (iph1 == NULL) {
		plog(ASL_LEVEL_ERR, 
			"Cannot reply to xauth request - no ph1 found.\n");
		goto end;
	}

	if (iph1->xauth_awaiting_userinput == 0) {
		plog(ASL_LEVEL_ERR, "Received xauth reply data with no xauth reply pending \n");
		goto end;
	}
	
	/* validate attr lengths */
	while (tlen > 0)
	{
		int tlv;
		
		attr = (struct isakmp_data *)dataptr;
		tlv = (attr->type & htons(0x8000)) == 0;
		
		if (tlv) {
			tlen -= ntohs(attr->lorv);
			dataptr += ntohs(attr->lorv);
		}
		tlen -= sizeof(u_int32_t);
		dataptr += sizeof(u_int32_t);
	}
	if (tlen != 0) {
		plog(ASL_LEVEL_ERR, "Invalid auth info received from VPN Control socket.\n");
		goto end;
	}
	
	payload = vmalloc(sizeof(struct isakmp_pl_attr) + attr_len);
	if (payload == NULL) {	
		plog(ASL_LEVEL_ERR, "Cannot allocate memory for xauth reply\n");
		goto end;
	}
	memset(payload->v, 0, sizeof(reply));

	reply = (struct isakmp_pl_attr *)payload->v;
	reply->h.len = htons(payload->l);
	reply->type = ISAKMP_CFG_REPLY;
	reply->id = iph1->pended_xauth_id;	/* network byte order */
	iph1->xauth_awaiting_userinput = 0;	/* no longer waiting */
	attr_ptr = reply + 1;
	memcpy(attr_ptr, attr_list, attr_len);

	plog(ASL_LEVEL_DEBUG, 
		    "Sending MODE_CFG REPLY\n");
	error = isakmp_cfg_send(iph1, payload, 
	    ISAKMP_NPTYPE_ATTR, ISAKMP_FLAG_E, 0, 0, iph1->xauth_awaiting_userinput_msg);
	VPTRINIT(iph1->xauth_awaiting_userinput_msg);
	ike_session_stop_xauth_timer(iph1);

	IPSECLOGASLMSG("IPSec Extended Authentication sent.\n");

end:
	if (payload)
		vfree(payload);
	return error;
}

int
vpn_assert(struct sockaddr_storage *src_addr, struct sockaddr_storage *dst_addr)
{
	if (ike_session_assert(src_addr, dst_addr)) {
		plog(ASL_LEVEL_ERR, 
			 "Cannot assert - no matching session.\n");
		return -1;
	}

	return 0;
}

