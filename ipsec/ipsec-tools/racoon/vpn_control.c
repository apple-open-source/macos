/* $Id: vpn_control.c,v 1.17.2.4 2005/07/12 11:49:44 manubsd Exp $ */

/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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

#include <System/net/pfkeyv2.h>

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

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "sockmisc.h"
#include "debug.h"

#include "schedule.h"
#include "localconf.h"
#include "remoteconf.h"
#include "grabmyaddr.h"
#include "isakmp_var.h"
#include "isakmp.h"
#include "oakley.h"
#include "handler.h"
#include "evt.h"
#include "pfkey.h"
#include "ipsec_doi.h"
#include "vpn_control.h"
#include "vpn_control_var.h"
#include "isakmp_inf.h"
#include "session.h"
#include "gcmalloc.h"

#ifdef ENABLE_VPNCONTROL_PORT
char *vpncontrolsock_path = VPNCONTROLSOCK_PATH;
uid_t vpncontrolsock_owner = 0;
gid_t vpncontrolsock_group = 0;
mode_t vpncontrolsock_mode = 0600;

static struct sockaddr_un sunaddr;
static int vpncontrol_process(struct vpnctl_socket_elem *, char *);
static int vpncontrol_reply(int, char *);
static void vpncontrol_close_comm(struct vpnctl_socket_elem *);

int
vpncontrol_handler()
{
	struct sockaddr_storage from;
	socklen_t fromlen = sizeof(from);

	struct vpnctl_socket_elem *sock_elem;
	
	sock_elem = racoon_malloc(sizeof(struct vpnctl_socket_elem));
	if (sock_elem == NULL) {
		plog(LLV_ERROR, LOCATION, NULL,
			"memory error: %s\n", strerror(errno));
		return -1;
	}
	LIST_INIT(&sock_elem->bound_addresses);

	sock_elem->sock = accept(lcconf->sock_vpncontrol, (struct sockaddr *)&from, &fromlen);
	if (sock_elem->sock < 0) {
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to accept vpn_control command: %s\n", strerror(errno));
		racoon_free(sock_elem);
		return -1;
	}
	LIST_INSERT_HEAD(&lcconf->vpnctl_comm_socks, sock_elem, chain);
	plog(LLV_NOTIFY, LOCATION, NULL,
		"accepted connection on vpn control socket.\n");
		
	check_auto_exit();
		
	return 0;
}

int
vpncontrol_comm_handler(struct vpnctl_socket_elem *elem)
{
	struct vpnctl_hdr hdr;
	char *combuf = NULL;
	int len;

	/* get buffer length */
	while ((len = recv(elem->sock, (char *)&hdr, sizeof(hdr), MSG_PEEK)) < 0) {
		if (errno == EINTR)
			continue;
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to recv vpn_control command: %s\n", strerror(errno));
		goto end;
	}
	if (len == 0) {
		plog(LLV_NOTIFY, LOCATION, NULL,
			"vpn_control socket closed by peer.\n");
		vpncontrol_close_comm(elem);
		return -1;
	}
		
	/* sanity check */
	if (len < sizeof(hdr)) {
		plog(LLV_ERROR, LOCATION, NULL,
			"invalid header length of vpn_control command - len=%d - expected %d\n", len, sizeof(hdr));
		goto end;
	}

	/* get buffer to receive */
	if ((combuf = racoon_malloc(ntohs(hdr.len) + sizeof(hdr))) == 0) {
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to alloc buffer for vpn_control command\n");
		goto end;
	}

	/* get real data */
	while ((len = recv(elem->sock, combuf, ntohs(hdr.len) + sizeof(hdr), 0)) < 0) {
		if (errno == EINTR)
			continue;
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to recv vpn_control command: %s\n",
			strerror(errno));
		goto end;
	}

	(void)vpncontrol_process(elem, combuf);

end:
	if (combuf)
		racoon_free(combuf);
	return 0;		// return -1 only if a socket is closed
}

static int
vpncontrol_process(struct vpnctl_socket_elem *elem, char *combuf)
{
	u_int16_t	error = 0;
	struct vpnctl_hdr *hdr = (struct vpnctl_hdr *)combuf;

	switch (ntohs(hdr->msg_type)) {
	
		case VPNCTL_CMD_BIND:
			{
				struct vpnctl_cmd_bind *pkt = (struct vpnctl_cmd_bind *)combuf;
				struct bound_addr *addr;
			
				plog(LLV_DEBUG, LOCATION, NULL,
					"received bind command on vpn control socket.\n");
				addr = racoon_malloc(sizeof(struct bound_addr));
				if (addr == NULL) {
					plog(LLV_ERROR, LOCATION, NULL,	
						"memory error: %s\n", strerror(errno));
					error = -1;
					break;
				}
				addr->address = pkt->address;
				LIST_INSERT_HEAD(&elem->bound_addresses, addr, chain);
				lcconf->auto_exit_state |= LC_AUTOEXITSTATE_CLIENT;	/* client side */
			}
			break;
			
		case VPNCTL_CMD_UNBIND:
			{
				struct vpnctl_cmd_unbind *pkt = (struct vpnctl_cmd_unbind *)combuf;
				struct bound_addr *addr;
				struct bound_addr *t_addr;

				plog(LLV_DEBUG, LOCATION, NULL,
					"received unbind command on vpn control socket.\n");
				LIST_FOREACH_SAFE(addr, &elem->bound_addresses, chain, t_addr) {
					if (pkt->address == 0xFFFFFFFF ||
						pkt->address == addr->address) {
						LIST_REMOVE(addr, chain);
						racoon_free(addr);
					}
				}
			}
			break;

		case VPNCTL_CMD_REDIRECT:
			{
				struct vpnctl_cmd_redirect *redirect_msg = (struct vpnctl_cmd_redirect *)combuf;
				struct redirect *raddr;
				struct redirect *t_raddr;
				int found = 0;
				
				plog(LLV_DEBUG, LOCATION, NULL,
					"received redirect command on vpn control socket - address = %x.\n", ntohl(redirect_msg->redirect_address));
				
				LIST_FOREACH_SAFE(raddr, &lcconf->redirect_addresses, chain, t_raddr) {
					if (raddr->cluster_address == redirect_msg->address) {
						if (redirect_msg->redirect_address == 0) {
							LIST_REMOVE(raddr, chain);
							racoon_free(raddr);
						} else {
							raddr->redirect_address = redirect_msg->redirect_address;
							raddr->force = ntohs(redirect_msg->force);
						}
						found = 1;
						break;
					}
				}
				if (!found) {
					raddr = racoon_malloc(sizeof(struct redirect));
					if (raddr == NULL) {
						plog(LLV_DEBUG, LOCATION, NULL,
							"cannot allcoate memory for redirect address.\n");					
						error = -1;
						break;
					}
					raddr->cluster_address = redirect_msg->address;
					raddr->redirect_address = redirect_msg->redirect_address;
					raddr->force = ntohs(redirect_msg->force);
					LIST_INSERT_HEAD(&lcconf->redirect_addresses, raddr, chain);
					
				}
			}
			break;
			
		case VPNCTL_CMD_PING:
			break;	// just reply for now

		default:
			plog(LLV_ERROR, LOCATION, NULL,
				"invalid command: %d\n", ntohs(hdr->msg_type));
			error = -1;		// for now
			break;
	}

	hdr->len = 0;
	hdr->result = htons(error);
	if (vpncontrol_reply(elem->sock, combuf) < 0)
		return -1;

	return 0;

}

static int
vpncontrol_reply(int so, char *combuf)
{
	size_t tlen;

	tlen = send(so, combuf, sizeof(struct vpnctl_hdr), 0);
	if (tlen < 0) {
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to send vpn_control message: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

int
vpncontrol_notify_ike_failed(u_int16_t notify_code, u_int16_t from, u_int32_t address, u_int16_t data_len, u_int8_t *data)
{
	struct vpnctl_status_failed *msg; 
	struct vpnctl_socket_elem *sock_elem;
	struct bound_addr *bound_addr;
	size_t tlen, len;
	
	len = sizeof(struct vpnctl_status_failed) + data_len;
	
	msg = (struct vpnctl_status_failed *)racoon_malloc(len);
	if (msg == NULL) {
		plog(LLV_DEBUG, LOCATION, NULL,
				"unable to allcate memory for vpn control status message.\n");
		return -1;
	}
		
	msg->hdr.msg_type = htons(VPNCTL_STATUS_IKE_FAILED);
	msg->hdr.flags = msg->hdr.cookie = msg->hdr.reserved = msg->hdr.result = 0;
	msg->hdr.len = htons(len - sizeof(struct vpnctl_hdr));
	msg->address = address;
	msg->ike_code = htons(notify_code);
	msg->from = htons(from);
	if (data_len > 0)
		memcpy(msg->data, data, data_len);	
	plog(LLV_DEBUG, LOCATION, NULL,
			"sending vpn_control ike notify failed message - code=%d  from=%s.\n", notify_code,
					(from == FROM_LOCAL ? "local" : "remote"));

	LIST_FOREACH(sock_elem, &lcconf->vpnctl_comm_socks, chain) {
		LIST_FOREACH(bound_addr, &sock_elem->bound_addresses, chain) {
			if (bound_addr->address == 0xFFFFFFFF ||
				bound_addr->address == address) {
				tlen = send(sock_elem->sock, msg, len, 0);
				if (tlen < 0) {
					plog(LLV_ERROR, LOCATION, NULL,
						"unable to send vpn_control ike notify failed: %s\n", strerror(errno));
				}
				break;
			}
		}
	}
	return 0;
}


int
vpncontrol_notify_phase_change(int start, u_int16_t from, struct ph1handle *iph1, struct ph2handle *iph2)
{
	struct vpnctl_status_phase_change msg; 
	struct vpnctl_socket_elem *sock_elem;
	struct bound_addr *bound_addr;
	size_t tlen;	
	u_int32_t address;
		
	if (iph1) {
		if (iph1->remote->sa_family == AF_INET)
			address = ((struct sockaddr_in *)iph1->remote)->sin_addr.s_addr;
		else
			return 0;		// for now
		msg.hdr.msg_type = htons(start ? 
			(from == FROM_LOCAL ? VPNCTL_STATUS_PH1_START_US : VPNCTL_STATUS_PH1_START_PEER) 
			: VPNCTL_STATUS_PH1_ESTABLISHED);
	} else {
		if (iph2->dst->sa_family == AF_INET)
			address = ((struct sockaddr_in *)iph2->dst)->sin_addr.s_addr;
		else
			return 0;		// for now
		msg.hdr.msg_type = htons(start ? VPNCTL_STATUS_PH2_START : VPNCTL_STATUS_PH2_ESTABLISHED);
	}
	msg.hdr.flags = msg.hdr.cookie = msg.hdr.reserved = msg.hdr.result = 0;
	msg.hdr.len = htons(sizeof(struct vpnctl_status_phase_change) - sizeof(struct vpnctl_hdr));
	msg.address = address;

	LIST_FOREACH(sock_elem, &lcconf->vpnctl_comm_socks, chain) {
		LIST_FOREACH(bound_addr, &sock_elem->bound_addresses, chain) {
			if (bound_addr->address == 0xFFFFFFFF ||
				bound_addr->address == address) {
				tlen = send(sock_elem->sock, &msg, sizeof(struct vpnctl_status_phase_change), 0);
				if (tlen < 0) {
					plog(LLV_ERROR, LOCATION, NULL,
						"failed to send vpn_control phase change status: %s\n", strerror(errno));
				}
				break;
			}
		}
	}

	return 0;
}


int
vpncontrol_init()
{
	if (vpncontrolsock_path == NULL) {
		lcconf->sock_vpncontrol = -1;
		return 0;
	}

	memset(&sunaddr, 0, sizeof(sunaddr));
	sunaddr.sun_family = AF_UNIX;
	snprintf(sunaddr.sun_path, sizeof(sunaddr.sun_path),
		"%s", vpncontrolsock_path);

	lcconf->sock_vpncontrol = socket(AF_UNIX, SOCK_STREAM, 0);
	if (lcconf->sock_vpncontrol == -1) {
		plog(LLV_ERROR, LOCATION, NULL,
			"socket: %s\n", strerror(errno));
		return -1;
	}

	unlink(sunaddr.sun_path);
	if (bind(lcconf->sock_vpncontrol, (struct sockaddr *)&sunaddr,
			sizeof(sunaddr)) != 0) {
		plog(LLV_ERROR, LOCATION, NULL,
			"bind(sockname:%s): %s\n",
			sunaddr.sun_path, strerror(errno));
		(void)close(lcconf->sock_vpncontrol);
		return -1;
	}

	if (chown(sunaddr.sun_path, vpncontrolsock_owner, vpncontrolsock_group) != 0) {
		plog(LLV_ERROR, LOCATION, NULL, 
		    "chown(%s, %d, %d): %s\n", 
		    sunaddr.sun_path, vpncontrolsock_owner, 
		    vpncontrolsock_group, strerror(errno));
		(void)close(lcconf->sock_vpncontrol);
		return -1;
	}

	if (chmod(sunaddr.sun_path, vpncontrolsock_mode) != 0) {
		plog(LLV_ERROR, LOCATION, NULL, 
		    "chmod(%s, 0%03o): %s\n", 
		    sunaddr.sun_path, vpncontrolsock_mode, strerror(errno));
		(void)close(lcconf->sock_vpncontrol);
		return -1;
	}

	if (listen(lcconf->sock_vpncontrol, 5) != 0) {
		plog(LLV_ERROR, LOCATION, NULL,
			"listen(sockname:%s): %s\n",
			sunaddr.sun_path, strerror(errno));
		(void)close(lcconf->sock_vpncontrol);
		return -1;
	}
	plog(LLV_DEBUG, LOCATION, NULL,
		"opened %s as racoon management.\n", sunaddr.sun_path);

	return 0;
}


void
vpncontrol_close()
{
	struct vpnctl_socket_elem *elem;
	struct vpnctl_socket_elem *t_elem;
	
	if (lcconf->sock_vpncontrol != -1) {
		close(lcconf->sock_vpncontrol);
		lcconf->sock_vpncontrol = -1;
	}
	LIST_FOREACH_SAFE(elem, &lcconf->vpnctl_comm_socks, chain, t_elem)
		vpncontrol_close_comm(elem);
}

static void
vpncontrol_close_comm(struct vpnctl_socket_elem *elem)
{
	struct bound_addr *addr;
	struct bound_addr *t_addr;
	
	LIST_REMOVE(elem, chain);
	if (elem->sock != -1)	
		close(elem->sock);
	LIST_FOREACH_SAFE(addr, &elem->bound_addresses, chain, t_addr) {
		LIST_REMOVE(addr, chain);
		racoon_free(addr);
	}
	racoon_free(elem);
	check_auto_exit();
}

int
vpn_control_connected(void)
{
	if (LIST_EMPTY(&lcconf->vpnctl_comm_socks))
		return 0;
	else
		return 1;
}

#endif
