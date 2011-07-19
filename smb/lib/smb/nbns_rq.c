/*
 * Copyright (c) 2000, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: nbns_rq.c,v 1.13.140.1 2006/04/14 23:49:37 gcolley Exp $
 */
#include <netsmb/netbios.h>
#include <sys/smb_byte_order.h>
#include <netsmb/upi_mbuf.h>
#include <sys/mchain.h>
#include <netsmb/smb_lib.h>
#include <netsmb/nb_lib.h>
#include "preference.h"

/*
 * nbns request
 */
struct nbns_rq {
	int		nr_opcode;
	int		nr_nmflags;
	int		nr_rcode;
	int		nr_qdcount;
	int		nr_ancount;
	int		nr_nscount;
	int		nr_arcount;
	struct nb_name*	nr_qdname;
	uint16_t	nr_qdtype;
	uint16_t	nr_qdclass;
	struct sockaddr_in nr_dest;	/* receiver of query */
	struct sockaddr_in nr_sender;	/* sender of response */
	int		nr_rpnmflags;
	int		nr_rprcode;
	uint16_t	nr_rpancount;
	uint16_t	nr_rpnscount;
	uint16_t	nr_rparcount;
	uint16_t	nr_trnid;
	struct nb_ctx *	nr_nbd;
	struct mbchain	nr_rq;
	struct mdchain	nr_rp;
	struct nb_ifdesc *nr_if;
	int		nr_flags; /* endian-ness depends on host */
	int		nr_fd;
	int32_t	nr_timo;
};

static struct nb_ifdesc *nb_iflist;


static char * smb_optstrncpy(char *d, char *s, unsigned maxlen)
{
	if (d && s) {
		strncpy(d, s, maxlen);
		d[maxlen] = (char)0;
	}
	return (d);
}

static int nbns_rq_create(int opcode, struct smb_prefs *prefs, 
						  struct nbns_rq **rqpp)
{
	struct nbns_rq *rqp;
	static uint16_t trnid;
	int error;
	
	rqp = malloc(sizeof(*rqp));
	if (rqp == NULL)
		return ENOMEM;
	bzero(rqp, sizeof(*rqp));
	error = mb_init(&rqp->nr_rq);
	if (error) {
		free(rqp);
		return error;
	}
	if (prefs) {
		rqp->nr_timo = prefs->NetBIOSResolverTimeout;
	} else {
		rqp->nr_timo = DefaultNetBIOSResolverTimeout;
	}

	rqp->nr_opcode = opcode;
	rqp->nr_trnid = trnid++;
	*rqpp = rqp;
	return 0;
}

static void nbns_rq_done(struct nbns_rq *rqp)
{
	if (rqp == NULL)
		return;
	if (rqp->nr_fd >= 0)
		close(rqp->nr_fd);
	mb_done(&rqp->nr_rq);
	md_done(&rqp->nr_rp);
	free(rqp);
}

/*
 * Extract resource record from the packet. Assume that there is only
 * one mbuf.
 */
static int nbns_rq_getrr(struct nbns_rq *rqp, struct nbns_rr *rrp)
{
	mdchain_t mdp = &rqp->nr_rp;
	u_char *cp;
	int error, len;
	
	bzero(rrp, sizeof(*rrp));
	cp = (u_char *)(mdp->md_pos);
	len = nb_encname_len((char *)cp);
	if (len < 1)
		return EINVAL;
	rrp->rr_name = cp;
	error = md_get_mem(mdp, NULL, len, MB_MSYSTEM);
	if (error)
		return error;
	md_get_uint16be(mdp, &rrp->rr_type);
	md_get_uint16be(mdp, &rrp->rr_class);
	md_get_uint32be(mdp, &rrp->rr_ttl);
	md_get_uint16be(mdp, &rrp->rr_rdlength);
	rrp->rr_data = (u_char *)mdp->md_pos;
	error = md_get_mem(mdp, NULL, rrp->rr_rdlength, MB_MSYSTEM);
	return error;
}

static int nbns_rq_recv(struct nbns_rq *rqp)
{
	mdchain_t mdp = &rqp->nr_rp;
	void *rpdata = mbuf_data(mdp->md_top);
	fd_set rd, wr, ex;
	struct timeval tv;
	struct sockaddr_in sender;
	int s = rqp->nr_fd;
	int n;
	socklen_t len;
	
	FD_ZERO(&rd);
	FD_ZERO(&wr);
	FD_ZERO(&ex);
	FD_SET(s, &rd);
	
	tv.tv_sec = 0;
	tv.tv_usec = 500000; /* We wait half a second for a response */
	
	n = select(s + 1, &rd, &wr, &ex, &tv);
	if (n == -1)
		return -1;
	if (n == 0)
		return ETIMEDOUT;
	if (FD_ISSET(s, &rd) == 0)
		return ETIMEDOUT;
	len = (socklen_t)sizeof(sender);
	n = (int)recvfrom(s, rpdata, mbuf_maxlen(mdp->md_top), 0, (struct sockaddr*)&sender, &len);
	if (n < 0)
		return errno;
	mbuf_setlen(mdp->md_top, n);
	rqp->nr_sender = sender;
	return 0;
}

static int nbns_rq_opensocket(struct nbns_rq *rqp)
{
	struct sockaddr_in locaddr;
	int opt, s;
	
	s = rqp->nr_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		return errno;
	if (rqp->nr_flags & NBRQF_BROADCAST) {
		opt = 1;
		if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &opt, (socklen_t)sizeof(opt)) < 0)
			return errno;
		if (rqp->nr_if == NULL)
			return ENETDOWN;
		bzero(&locaddr, sizeof(locaddr));
		locaddr.sin_family = AF_INET;
		locaddr.sin_len = sizeof(locaddr);
		locaddr.sin_addr = rqp->nr_if->id_addr;
		rqp->nr_dest.sin_addr.s_addr = rqp->nr_if->id_addr.s_addr | ~rqp->nr_if->id_mask.s_addr;
		if (bind(s, (struct sockaddr*)&locaddr, (socklen_t)sizeof(locaddr)) < 0)
			return errno;
	}
	return 0;
}

static int nbns_rq_send(struct nbns_rq *rqp)
{
	mbchain_t mbp = &rqp->nr_rq;
	int s = rqp->nr_fd;
	
	if (sendto(s, mbuf_data(mbp->mb_top), mbp->mb_count, 0,
			   (struct sockaddr*)&rqp->nr_dest, (socklen_t)sizeof(rqp->nr_dest)) < 0)
		return errno;
	return 0;
}

static int nbns_rq_run(struct nbns_rq *rqp, uint16_t *cancel)
{
	mdchain_t mdp;
	uint16_t rpid;
	uint8_t nmflags;
	int error, retrycount;
	
	rqp->nr_if = nb_iflist;
again:
	error = nbns_rq_opensocket(rqp);
	if (error)
		return error;
	/*
	 * The configuration file alwows the user to set the total amount of time 
	 * we will wait for a NetBIOS name lookup to complete. So will always wait 
	 * half a second per attempt. So NetBIOSResolverTimeout is the number of seconds we want to 
	 * wait. So NetBIOSResolverTimeout * 2 is the number of retries we will attmept.
	 */
	retrycount = rqp->nr_timo * 2;
	for (;;) {
		if (cancel && *cancel)
			return ECANCELED;
		
		error = nbns_rq_send(rqp);
		if (error)
			return error;
		error = nbns_rq_recv(rqp);
		if (error) {
			if (error != ETIMEDOUT || --retrycount == 0) {
				if ((rqp->nr_nmflags & NBNS_NMFLAG_BCAST) &&
				    rqp->nr_if != NULL &&
				    rqp->nr_if->id_next != NULL) {
					rqp->nr_if = rqp->nr_if->id_next;
					close(rqp->nr_fd);
					goto again;
				} else
					return error;
			}
			continue;
		}
		mdp = &rqp->nr_rp;
		if (md_get_uint16be(mdp, &rpid))
			return EINVAL;
		
		if (rpid != rqp->nr_trnid)
			return EINVAL;
		break;
	}
	if (md_get_uint8(mdp, &nmflags))
		return EINVAL;
	
	rqp->nr_rpnmflags = (nmflags & 7) << 4;
	if (md_get_uint8(mdp, &nmflags))
		return EINVAL;
	rqp->nr_rpnmflags |= (nmflags & 0xf0) >> 4;
	rqp->nr_rprcode = nmflags & 0xf;
	if (rqp->nr_rprcode)
		return nb_error_to_errno(rqp->nr_rprcode);
	
	if (md_get_uint16be(mdp, &rpid))	/* QDCOUNT */
		return EINVAL;
	if (md_get_uint16be(mdp, &rqp->nr_rpancount))
		return EINVAL;
	if (md_get_uint16be(mdp, &rqp->nr_rpnscount))
		return EINVAL;
	if (md_get_uint16be(mdp, &rqp->nr_rparcount))
		return EINVAL;
	return 0;
}

static int nbns_rq_prepare(struct nbns_rq *rqp)
{
	mbchain_t mbp = &rqp->nr_rq;
	uint8_t nmflags;
	u_char *cp;
	int len, error;
	
	error = md_init_rcvsize(&rqp->nr_rp, NBDG_MAXSIZE);
	if (error)
		return error;
	if (rqp->nr_dest.sin_addr.s_addr == htonl(INADDR_BROADCAST)) {
		rqp->nr_nmflags |= NBNS_NMFLAG_BCAST;
		rqp->nr_flags |= NBRQF_BROADCAST;
		if (nb_iflist == NULL) {
			error = nb_enum_if(&nb_iflist, 100);
			if (error)
				return error;
		}
	}
	mb_put_uint16be(mbp, rqp->nr_trnid);
	nmflags = ((rqp->nr_opcode & 0x1F) << 3) | ((rqp->nr_nmflags & 0x70) >> 4);
	mb_put_uint8(mbp, nmflags);
	mb_put_uint8(mbp, (rqp->nr_nmflags & 0x0f) << 4 /* rcode */);
	mb_put_uint16be(mbp, rqp->nr_qdcount);
	mb_put_uint16be(mbp, rqp->nr_ancount);
	mb_put_uint16be(mbp, rqp->nr_nscount);
	mb_put_uint16be(mbp, rqp->nr_arcount);
	if (rqp->nr_qdcount) {
		if (rqp->nr_qdcount > 1)
			return EINVAL;
		len = NB_ENCNAMELEN + 2;
		cp = (u_char *)mb_reserve(mbp, len);
		if (cp == NULL)
			return ENOMEM;
		
		nb_name_encode(rqp->nr_qdname, cp);
		mb_put_uint16be(mbp, rqp->nr_qdtype);
		mb_put_uint16be(mbp, rqp->nr_qdclass);
	}
	mb_pullup(mbp);
	return 0;
}

/* 
 * Resolve a NetBIOS name to an set of address.
 */
static int nbns_resolvename_internal(struct nb_ctx *ctx, struct smb_prefs *prefs, 
							  const char *name, uint8_t nodeType,
							  CFMutableArrayRef *outAddressArray, uint16_t port, 
							  int allowLocalConn, int tryBothPorts, uint16_t *cancel)
{
	CFMutableArrayRef addressArray = NULL;
	CFMutableDataRef addressData;
	struct connectAddress conn;
	struct nbns_rq *rqp;
	struct nb_name nn;
	struct nbns_rr rr;
	int error, rdrcount;
	u_char *current_ip, *end_of_rr;

	/* If we are trying both ports always put port 139 in after port 445 */
	if (tryBothPorts && (port == NBSS_TCP_PORT_139))
		port = SMB_TCP_PORT_445;

	if (strlen(name) > NB_NAMELEN)
		return ENAMETOOLONG;

	addressArray = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeArrayCallBacks);
	if (!addressArray) {
		return ENOMEM;
	}
	
	error = nbns_rq_create(NBNS_OPCODE_QUERY, prefs, &rqp);
	if (error) {
		CFRelease(addressArray);
		return error;
	}

	bzero(&nn, sizeof(nn));
	strlcpy((char *)nn.nn_name, name, sizeof(nn.nn_name));
	nn.nn_type = nodeType;
	rqp->nr_nmflags = NBNS_NMFLAG_RD;
	rqp->nr_qdname = &nn;
	rqp->nr_qdtype = NBNS_QUESTION_TYPE_NB;
	rqp->nr_qdclass = NBNS_QUESTION_CLASS_IN;
	rqp->nr_qdcount = 1;
	memcpy(&rqp->nr_dest, &ctx->nb_ns, sizeof(rqp->nr_dest));
	error = nbns_rq_prepare(rqp);
	if (error) {
		goto done;
	}
	rdrcount = NBNS_MAXREDIRECTS;
	for (;;) {
		error = nbns_rq_run(rqp, cancel);
		if (error)
			break;
		if ((rqp->nr_rpnmflags & NBNS_NMFLAG_AA) == 0) {
			if (rdrcount-- == 0) {
				error = ETOOMANYREFS;
				break;
			}
			error = nbns_rq_getrr(rqp, &rr);
			if (! error)
				error = nbns_rq_getrr(rqp, &rr);
			if (error)
				break;
			bcopy(rr.rr_data, &rqp->nr_dest.sin_addr, 4);
			rqp->nr_flags &= ~NBRQF_BROADCAST;
			continue;
		}
		if (rqp->nr_rpancount == 0) {
			error = EHOSTUNREACH;
			break;
		}
		error = nbns_rq_getrr(rqp, &rr);
		if (error)
			break;

		/* We have an answer, so store away the address of the server that responded */
		ctx->nb_sender = rqp->nr_sender;

		end_of_rr = rr.rr_data + rr.rr_rdlength;
		for(current_ip = rr.rr_data + 2; current_ip < end_of_rr; current_ip += 6) {
			bzero(&conn, sizeof(conn));
			conn.in4.sin_len = (int)sizeof(struct sockaddr_in);
			conn.in4.sin_family = AF_INET;
			conn.in4.sin_port = htons(port);
			memcpy(&conn.in4.sin_addr, current_ip, 4);
			/* Check to make sure we are not connecting to ourself */		
			if (isLocalIPAddress((struct sockaddr *)&conn.addr, port, allowLocalConn)) {
				smb_log_info("The address for `%s' is a loopback address, not allowed!", 
							 ASL_LEVEL_DEBUG, name);
				error = ELOOP;	/* AFP returns ELOOP, so we will do the same */
				goto done; 
			}
			addressData = CFDataCreateMutable(NULL, 0);
			if (addressData) {
				/* The name is the netbios name, we need a netbios sockaddr */
				if (port == NBSS_TCP_PORT_139)
					convertToNetBIOSaddr(&conn.storage, name);
				
				CFDataAppendBytes(addressData, (const UInt8 *)&conn, (CFIndex)sizeof(conn));
				CFArrayAppendValue(addressArray, addressData);
				CFRelease(addressData);
			}
			/* We only try both ports with IPv4 */
			if (tryBothPorts) {
				conn.in4.sin_port = htons(NBSS_TCP_PORT_139);
				/* The name is the netbios name, we need a netbios sockaddr */
				convertToNetBIOSaddr(&conn.storage, name);
				
				addressData = CFDataCreateMutable(NULL, 0);
				if (addressData) {
					CFDataAppendBytes(addressData, (const UInt8 *)&conn, (CFIndex)sizeof(conn));
					CFArrayAppendValue(addressArray, addressData);
					CFRelease(addressData);
				}
			}
		}
		break;
	} /* end big for loop */

	
done:
	if (CFArrayGetCount(addressArray) == 0) {
		error = EHOSTUNREACH;
	}
	nbns_rq_done(rqp);
	
	if (error) {
		if (addressArray)
			CFRelease(addressArray);
		addressArray = NULL;
	}
	*outAddressArray = addressArray;
	
	return error;
}

/* 
 * Resolve a NetBIOS name to an set of address, We always try WINS first if a
 * server is provide. If no WINS server or we fail to find one with the WINS
 * server then try broadcast.
 */
int nbns_resolvename(struct nb_ctx *ctx, struct smb_prefs *prefs, const char *name, 
					 uint8_t nodeType, CFMutableArrayRef *outAddressArray, uint16_t port, 
					 int allowLocalConn, int tryBothPorts, uint16_t *cancel)
{
	int error;
	
	error = nb_ctx_resolve(ctx, prefs->WINSAddresses);
	if (!error)
		error = nbns_resolvename_internal(ctx, prefs, name, nodeType, outAddressArray, 
										  port,  allowLocalConn, tryBothPorts, cancel);
	/* We tried it with WINS and failed try broadcast now */
	if (error && (prefs->WINSAddresses != NULL)) {
		error = nb_ctx_resolve(ctx, NULL);
		if (error == 0) {
			error = nbns_resolvename_internal(ctx, prefs, name, nodeType, outAddressArray, 
											  port, allowLocalConn, tryBothPorts, cancel);
		}
	}
	return error;
}

int nbns_getnodestatus(struct sockaddr *targethost, struct nb_ctx *ctx,
					   struct smb_prefs *prefs, uint16_t *cancel, char *nbt_server, 
					   char *workgroup, CFMutableArrayRef nbrrArray)
{
	struct nbns_rq *rqp;
	struct nbns_rr rr;
	struct nb_name nn;
	struct nbns_nr *nrp;
	char nrtype;
	char *cp, *retname = NULL;
	unsigned char nrcount;
	int error, i, foundserver = 0, foundgroup = 0;

	if (targethost->sa_family != AF_INET) 
		return EINVAL;
	error = nbns_rq_create(NBNS_OPCODE_QUERY, prefs, &rqp);
	if (error)
		return error;
	bzero(&nn, sizeof(nn));
	strlcpy((char *)nn.nn_name, "*", sizeof(nn.nn_name));
	nn.nn_type = NBT_WKSTA;
	rqp->nr_nmflags = 0;
	rqp->nr_qdname = &nn;
	rqp->nr_qdtype = NBNS_QUESTION_TYPE_NBSTAT;
	rqp->nr_qdclass = NBNS_QUESTION_CLASS_IN;
	rqp->nr_qdcount = 1;
	rqp->nr_dest = *(struct sockaddr_in *)(void *)targethost;
	rqp->nr_dest.sin_port = htons(NBNS_UDP_PORT_137);
	if (rqp->nr_dest.sin_addr.s_addr == INADDR_ANY)
		rqp->nr_dest.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	error = nbns_rq_prepare(rqp);
	if (error) {
		nbns_rq_done(rqp);
		return error;
	}
	for (;;) {
		error = nbns_rq_run(rqp, cancel);
		if (error)
			break;
		if (rqp->nr_rpancount == 0) {
			error = EHOSTUNREACH;
			break;
		}
		error = nbns_rq_getrr(rqp, &rr);
		if (error)
			break;
		nrcount = (unsigned char)(*(rr.rr_data));
		rr.rr_data++;
		for (i = 1, nrp = (struct nbns_nr *)(void *)rr.rr_data;
		     i <= nrcount; ++i, ++nrp) {
			uint16_t	nbFlags = betohs(nrp->nr_beflags);
			
			/*
			 * They want all the names in the format it came into from the 
			 * network. Copy in the NetBIOS name and then the flags.
			 */
			 if (nbrrArray) {
				CFMutableDataRef addressData = CFDataCreateMutable(NULL, 0);
				CFDataAppendBytes(addressData, (const UInt8 *)nrp->nr_name, (CFIndex)NB_NAMELEN);
				CFDataAppendBytes(addressData, (const UInt8 *)&nbFlags, (CFIndex)sizeof(uint16_t));
				CFArrayAppendValue(nbrrArray, addressData);
				CFRelease(addressData);				
			}
			
			nrtype = nrp->nr_name[NB_NAMELEN-1];
			/* Terminate the string: */
			nrp->nr_name[NB_NAMELEN-1] = (char)0;
			/* Strip off trailing spaces */
			for (cp = &nrp->nr_name[NB_NAMELEN-2];
			     cp >= nrp->nr_name; --cp) {
				if (*cp != (char)0x20)
					break;
				*cp = (char)0;
			}
			if (nbFlags & NBNS_GROUPFLG) {
				if (!foundgroup ||
				    (foundgroup != NBT_WKSTA+1 &&
				     nrtype == NBT_WKSTA)) {
						if (workgroup)
							smb_optstrncpy(workgroup, nrp->nr_name, SMB_MAXNetBIOSNAMELEN);
					foundgroup = nrtype+1;
				}
			} else {
				/* Track at least ONE name, in case
				   no server name is found */
				retname = nrp->nr_name;
			}
			if (nrtype == NBT_SERVER) {
				if (nbt_server)
					smb_optstrncpy(nbt_server, nrp->nr_name, SMB_MAXNetBIOSNAMELEN);
				foundserver = 1;
			}
		}
		if (!foundserver && nbt_server)
			smb_optstrncpy(nbt_server, retname, SMB_MAXNetBIOSNAMELEN);
		ctx->nb_sender = rqp->nr_sender;
		break;
	}
	nbns_rq_done(rqp);
	return error;
}

