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

#ifdef __APPLE__
#include <System/net/pfkeyv2.h>
#else
#include <net/pfkeyv2.h>
#endif

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
#include "evt.h"
#include "pfkey.h"
#include "ipsec_doi.h"
#include "admin.h"
#include "admin_var.h"
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

#include "vpn_control.h"
#include "vpn_control_var.h"
#include "strnames.h"
#include "ike_session.h"


static int vpn_get_ph2pfs(struct ph1handle *);

int
vpn_connect(struct bound_addr *srv)
{
	int error = -1;
	struct sockaddr *dst;
	struct remoteconf *rmconf;
	struct sockaddr *remote = NULL;
	struct sockaddr *local = NULL;
	u_int16_t port;

	dst = racoon_calloc(1, sizeof(struct sockaddr));	// this should come from the bound_addr parameter
	if (dst == NULL)
		goto out;
	((struct sockaddr_in *)(dst))->sin_len = sizeof(struct sockaddr_in);
	((struct sockaddr_in *)(dst))->sin_family = AF_INET;
	((struct sockaddr_in *)(dst))->sin_port = 500;
	((struct sockaddr_in *)(dst))->sin_addr.s_addr = srv->address;

	/*
	 * Find the source address
	 */	 
	if ((local = getlocaladdr(dst)) == NULL) {
		plog(LLV_ERROR, LOCATION, NULL,
			"cannot get local address\n");
		goto out1;
	}

	/* find appropreate configuration */
	rmconf = getrmconf(dst);
	if (rmconf == NULL) {
		plog(LLV_ERROR, LOCATION, NULL,
			"no configuration found "
			"for %s\n", saddrwop2str(dst));
		goto out1;
	}

	/* get remote IP address and port number. */
	if ((remote = dupsaddr(dst)) == NULL) {
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to duplicate address\n");
		goto out1;
	}

	switch (remote->sa_family) {
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
		plog(LLV_ERROR, LOCATION, NULL,
			"invalid family: %d\n",
			remote->sa_family);
		goto out1;
		break;
	}

	port = ntohs(getmyaddrsport(local));
	if (set_port(local, port) == NULL) 
		goto out1;

	plog(LLV_INFO, LOCATION, NULL,
		"accept a request to establish IKE-SA: "
		"%s\n", saddrwop2str(remote));

	/* begin ident mode */
	if (isakmp_ph1begin_i(rmconf, remote, local, 1) < 0)
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
vpn_disconnect(struct bound_addr *srv)
{
	struct sockaddr_in	saddr;

	bzero(&saddr, sizeof(saddr));
	saddr.sin_len = sizeof(saddr);
	saddr.sin_addr.s_addr = srv->address;
	saddr.sin_port = 0;
	saddr.sin_family = AF_INET;
    	ike_sessions_stopped_by_controller(&saddr,
                                       0,
                                       ike_session_stopped_by_vpn_disconnect);
	if (purgephXbydstaddrwop((struct sockaddr *)(&saddr)) > 0) {
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
	struct ph1handle	*ph1;
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
	ph1 = getph1bydstaddrwop((struct sockaddr *)(&saddr));
	if (ph1 == NULL) {
		plog(LLV_ERROR, LOCATION, NULL,
			"cannot start phase2 - no phase1 found.\n");
		return -1;
	}
	if (ph1->status != PHASE1ST_ESTABLISHED) {
		plog(LLV_ERROR, LOCATION, NULL,
			 "cannot start phase2 - phase1 not established.\n");
		return -1;
	}

	selector_ptr = (struct vpnctl_sa_selector *)(pkt + 1);
	algo_ptr = (struct vpnctl_algo *)(selector_ptr + ntohs(pkt->selector_count));

	for (i = 0; i < ntohs(pkt->selector_count); i++, selector_ptr++) {
		new_sainfo = newsainfo();
		if (new_sainfo == NULL) {
			plog(LLV_ERROR, LOCATION, NULL,
				"unable to allocate sainfo struct.\n");
			goto fail;
		}
		
		if (ntohl(selector_ptr->src_tunnel_mask) == 0xFFFFFFFF)
			new_sainfo->idsrc = vmalloc(sizeof(struct id) - sizeof(u_int32_t));
		else
			new_sainfo->idsrc = vmalloc(sizeof(struct id));
		if (new_sainfo->idsrc == NULL) {
			plog(LLV_ERROR, LOCATION, NULL,
				 "unable to allocate id struct.\n");
			goto fail;
		}
		if (selector_ptr->dst_tunnel_mask == 0xFFFFFFFF)
			new_sainfo->iddst = vmalloc(sizeof(struct id) - sizeof(u_int32_t));
		else
			new_sainfo->iddst = vmalloc(sizeof(struct id));
		if (new_sainfo->iddst == NULL) {
			plog(LLV_ERROR, LOCATION, NULL,
				 "unable to allocate id struct.\n");
			goto fail;
		}			
		
		id_ptr = (struct id *)new_sainfo->idsrc->v;
		if (ntohl(selector_ptr->src_tunnel_mask) == 0xFFFFFFFF)
			id_ptr->type = IPSECDOI_ID_IPV4_ADDR;
		else {
			id_ptr->type = IPSECDOI_ID_IPV4_ADDR_SUBNET;
			id_ptr->mask = selector_ptr->src_tunnel_mask;
		}
		id_ptr->addr = selector_ptr->src_tunnel_address;
		id_ptr->port = selector_ptr->src_tunnel_port;
		id_ptr->proto_id = selector_ptr->ul_protocol;
				
		id_ptr = (struct id *)new_sainfo->iddst->v;
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
				plog(LLV_ERROR, LOCATION, NULL, "invalid dh group specified\n");
				goto fail;
			}
		}
		for (j = 0, next_algo = algo_ptr; j < ntohs(pkt->algo_count); j++, next_algo++) {

			new_algo = newsainfoalg();
			if (new_algo == NULL) {
				plog(LLV_ERROR, LOCATION, NULL,
					"failed to allocate algorithm structure\n");
				goto fail;
			}

			class = ntohs(next_algo->algo_class);
			algorithm = ntohs(next_algo->algo);
			keylen = ntohs(next_algo->key_len);
			
			new_algo->alg = algtype2doi(class, algorithm);
			if (new_algo->alg == -1) {
				plog(LLV_ERROR, LOCATION, NULL, "algorithm mismatched\n");
				racoon_free(new_algo);
				goto fail;
			}

			defklen = default_keylen(class, algorithm);
			if (defklen == 0) {
				if (keylen) {
					plog(LLV_ERROR, LOCATION, NULL, "keylen not allowed\n");
					racoon_free(new_algo);
					goto fail;
				}
			} else {
				if (keylen && check_keylen(class, algorithm, keylen) < 0) {
					plog(LLV_ERROR, LOCATION, NULL, "invalid keylen %d\n", keylen);
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
				plog(LLV_ERROR, LOCATION, NULL, 
					"algorithm %s not supported by the kernel (missing module?)\n", s_ipsecdoi_trns(a, b));
				racoon_free(new_algo);
				goto fail;
			}
			inssainfoalg(&new_sainfo->algs[class], new_algo);
		}

		if (new_sainfo->algs[algclass_ipsec_enc] == 0) {
			plog(LLV_ERROR, LOCATION, NULL,
				"no encryption algorithm at %s\n", sainfo2str(new_sainfo));
			goto fail;
		}
		if (new_sainfo->algs[algclass_ipsec_auth] == 0) {
			plog(LLV_ERROR, LOCATION, NULL,
				"no authentication algorithm at %s\n", sainfo2str(new_sainfo));
			goto fail;
		}
		if (new_sainfo->algs[algclass_ipsec_comp] == 0) {
			plog(LLV_ERROR, LOCATION, NULL,
				"no compression algorithm at %s\n", sainfo2str(new_sainfo));
			goto fail;
		}

		/* duplicate check */
		check = getsainfo(new_sainfo->idsrc, new_sainfo->iddst, new_sainfo->id_i, 0);
		if (check && (!check->idsrc && !new_sainfo->idsrc)) {
			plog(LLV_ERROR, LOCATION, NULL,"duplicated sainfo: %s\n", sainfo2str(new_sainfo));
			goto fail;
		}
		plog(LLV_DEBUG2, LOCATION, NULL, "create sainfo: %s\n", sainfo2str(new_sainfo));
		inssainfo(new_sainfo);
		new_sainfo = NULL;
	}
	
	return 0;
	
fail:
	if (new_sainfo)
		delsainfo(new_sainfo);
	flushsainfo_dynamic(addr);
	return -1;
}

static int 
vpn_get_ph2pfs(struct ph1handle *ph1)
{
}


int
vpn_get_config(struct ph1handle *iph1, struct vpnctl_status_phase_change **msg, size_t *msg_size)
{

	struct vpnctl_modecfg_params *params;
	struct myaddrs *myaddr;
	u_int16_t ifname_len, msize;
	u_int8_t  *cptr;
	
	*msg = NULL;
	msize = 0;
	
	if (((struct sockaddr_in *)iph1->local)->sin_family != AF_INET) {
		plog(LLV_ERROR, LOCATION, NULL,
			"IPv6 not supported for mode config.\n");
		return -1;
	}
	
	if (iph1->mode_cfg->attr_list == NULL)
		return 1;	/* haven't received configuration yet */
		
	myaddr = find_myaddr(iph1->local, 0);
	if (myaddr == NULL) {
		plog(LLV_ERROR, LOCATION, NULL,
			"unable to find address structure.\n");
		return -1;
	}
	
	msize = sizeof(struct vpnctl_status_phase_change) 
			+ sizeof(struct vpnctl_modecfg_params);
	msize += iph1->mode_cfg->attr_list->l;

	*msg = racoon_calloc(1, msize);
	if (*msg == NULL) {
		plog(LLV_ERROR, LOCATION, NULL,
			"faled to allocate space for message.\n");
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

	return 0;
}


int
vpn_xauth_reply(u_int32_t address, void *attr_list, size_t attr_len)
{

	struct isakmp_pl_attr *reply;
	void* attr_ptr;
	vchar_t *payload = NULL;
	struct ph1handle	*iph1;
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
	iph1 = getph1bydstaddrwop((struct sockaddr *)(&saddr));
	if (iph1 == NULL) {
		plog(LLV_ERROR, LOCATION, NULL,
			"cannot reply to xauth request - no ph1 found.\n");
		goto end;
	}

	if (iph1->xauth_awaiting_userinput == 0) {
		plog(LLV_ERROR, LOCATION, NULL, "Huh? recvd xauth reply data with no xauth reply pending \n");
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
		plog(LLV_ERROR, LOCATION, NULL, "invalid auth info received from VPN Control socket.\n");
		goto end;
	}
	
	payload = vmalloc(sizeof(struct isakmp_pl_attr) + attr_len);
	if (payload == NULL) {	
		plog(LLV_ERROR, LOCATION, NULL, "Cannot allocate memory for xauth reply\n");
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

	plog(LLV_DEBUG, LOCATION, NULL, 
		    "Sending MODE_CFG REPLY\n");
	error = isakmp_cfg_send(iph1, payload, 
	    ISAKMP_NPTYPE_ATTR, ISAKMP_FLAG_E, 0, 0, iph1->xauth_awaiting_userinput_msg);
	VPTRINIT(iph1->xauth_awaiting_userinput_msg);
	ike_session_stop_xauth_timer(iph1);

end:
	if (payload)
		vfree(payload);
	return error;
}

