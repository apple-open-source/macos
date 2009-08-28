/*
 * Copyright (c) 2000, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2008 Apple Inc. All rights reserved.
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
 * $Id: nb_net.c,v 1.12 2006/01/06 07:53:01 lindak Exp $
 */
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <asl.h>

#include <net/if.h>

#include <ctype.h>
#include <netdb.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <netsmb/netbios.h>
#include <netsmb/smb_lib.h>
#include <netsmb/nb_lib.h>
#include "charsets.h"


static int SocketUtilsIncrementIfReqIter(UInt8** inIfReqIter, struct ifreq* ifr)
{
    *inIfReqIter += sizeof(ifr->ifr_name) + ifr->ifr_addr.sa_len;
	
    /* If the length of the addr is 0, use the family to determine the addr size */
    if (ifr->ifr_addr.sa_len == 0) {
        switch (ifr->ifr_addr.sa_family) {
		case AF_INET:
			*inIfReqIter += sizeof(struct sockaddr_in);
			break;
		default:
			*inIfReqIter += sizeof(struct sockaddr);
			return FALSE;
        }
    }
    return TRUE;
}

/*
 * Currently we only look for AF_INET address some day we need to check
 * for AF_INET6, but since we don't support AF_INET6 yet no need to do that
 * work.
 */
int isLocalNetworkAddress(u_int32_t addr)
{
    UInt32		kMaxAddrBufferSize = 2048;
    UInt8 		buffer[kMaxAddrBufferSize];
	int			so;
    UInt8* 		ifReqIter = NULL;
	struct ifconf ifc;
	struct ifreq ifreq, *ifr;
	int foundit = FALSE;
	
	if ((so = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		smb_log_info("%s: socket failed!", -1, ASL_LEVEL_ERR, __FUNCTION__);
		return foundit;
	}
	ifc.ifc_len = (int)sizeof (buffer);
    ifc.ifc_buf = (char*) buffer;
	if (ioctl(so, SIOCGIFCONF, (char *)&ifc) < 0) {
		smb_log_info("%s: ioctl (get interface configuration)!", -1, ASL_LEVEL_ERR, __FUNCTION__);
		goto WeAreDone;
	}
    for (ifReqIter = buffer; ifReqIter < (buffer + ifc.ifc_len);) {
        ifr = (struct ifreq*) ifReqIter;
        if (!SocketUtilsIncrementIfReqIter(&ifReqIter, ifr)) {
			smb_log_info("%s: SocketUtilsIncrementIfReqIter failed!", 0, ASL_LEVEL_ERR, __FUNCTION__);
            break;
        }
		ifreq = *ifr;
        if ((ifr->ifr_addr.sa_family != AF_INET) || (strncmp(ifr->ifr_name, "lo", 2) == 0))
			continue;
		
		if (ioctl(so, SIOCGIFFLAGS, (char *)&ifreq) < 0) {
			smb_log_info("%s: SIOCGIFFLAGS ioctl failed!", -1, ASL_LEVEL_ERR, __FUNCTION__);
			continue;
		}
		if (ifreq.ifr_flags & IFF_UP) {
			struct sockaddr_in *laddr = (struct sockaddr_in *)&(ifreq.ifr_addr);
			if ((u_int32_t)laddr->sin_addr.s_addr == addr) {
				foundit = TRUE;
				break;
			}
		}
	}
WeAreDone:
	(void) close(so);
	return foundit;
}

int
nb_getlocalname(char *name, size_t maxsize)
{
	char buf[_POSIX_HOST_NAME_MAX+1], *cp;

	if (gethostname(buf, sizeof(buf)) != 0)
		return errno;
	cp = strchr(buf, '.');
	if (cp)
		*cp = 0;
	/* 
	 * Use strlcpy to make sure we do not overrun the buffer. The string
	 * will get copy. We return ENAMETOOLONG and let the calling routine
	 * decide what to do.
	 */
	if (strlcpy(name, buf, maxsize) >= maxsize)
		return ENAMETOOLONG;
	str_upper(name, name);
	return 0;
}

/* 
 * This routine does not currently support ipv6, it only deals with
 * ipv4 and even at that not well.
 */
int nb_resolvehost_in(const char *name, struct sockaddr **dest, u_int16_t port, int allow_local_conn)
{
	struct hostent* h;
	struct sockaddr_in *sinp;
	int len;

	h = gethostbyname(name);
	if (!h) {
		smb_log_info("%s: can't get server address %s!", 0, ASL_LEVEL_DEBUG, __FUNCTION__, name);
		return EHOSTUNREACH;
	}
	
	/* Need to deal with AF_INET6 in the future */
	if (h->h_addrtype != AF_INET) {
		smb_log_info("%s: we only support ipv4 currently %d!", 0, ASL_LEVEL_DEBUG, __FUNCTION__, h->h_addrtype);
		return EAFNOSUPPORT;
	}

	if (h->h_length != 4) {
		smb_log_info("%s: address for `%s' has invalid length!\n", 0, ASL_LEVEL_DEBUG, __FUNCTION__, name);					
		return EAFNOSUPPORT;
	}
	
	if (! allow_local_conn) {
		if (*(u_int32_t *)h->h_addr == (u_int32_t)htonl(INADDR_LOOPBACK)) {
			smb_log_info("The address for `%s' is a loopback address, not allowed!\n", 0, ASL_LEVEL_ERR, name);
			/* AFP now returns ELOOP, so we will do the same */
			return ELOOP;		
		}
		
		if (isLocalNetworkAddress(*(u_int32_t *)h->h_addr) == TRUE) {
			smb_log_info("The address for `%s' is a local address, not allowed!\n", 0, ASL_LEVEL_ERR, name);			
			/* AFP now returns ELOOP, so we will do the same */
			return ELOOP;		
		}		
	}

	len = (int)sizeof(struct sockaddr_in);
	sinp = malloc(len);
	*dest = (struct sockaddr*)sinp;
	if (sinp == NULL)
		return ENOMEM;
	bzero(sinp, len);
	sinp->sin_len = len;
	sinp->sin_family = h->h_addrtype;
	memcpy(&sinp->sin_addr.s_addr, h->h_addr, 4);
	sinp->sin_port = htons(port);
	return 0;
}

int
nb_enum_if(struct nb_ifdesc **iflist, int maxif)
{  
	struct ifconf ifc;
	struct ifreq *ifrqp;
	struct nb_ifdesc *ifd;
	struct in_addr iaddr, imask;
	char *ifrdata, *iname;
	int s, rdlen, ifcnt, error, iflags, i;
	unsigned len;

	*iflist = NULL;
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1)
		return errno;

	rdlen = (int)(maxif * sizeof(struct ifreq));
	ifrdata = malloc(rdlen);
	if (ifrdata == NULL) {
		error = ENOMEM;
		goto bad;
	}
	ifc.ifc_len = rdlen;
	ifc.ifc_buf = ifrdata;
	if (ioctl(s, SIOCGIFCONF, &ifc) != 0) {
		error = errno;
		goto bad;
	} 
	ifrqp = ifc.ifc_req;
	ifcnt = ifc.ifc_len / (int)sizeof(struct ifreq);
	error = 0;
	/* freebsd bug: ifreq size is variable - must use _SIZEOF_ADDR_IFREQ */
	for (i = 0; i < ifc.ifc_len;
	     i += len, ifrqp = (struct ifreq *)((caddr_t)ifrqp + len)) {
		len = (int)_SIZEOF_ADDR_IFREQ(*ifrqp);
		/* XXX for now, avoid IP6 broadcast performance costs */
		if (ifrqp->ifr_addr.sa_family != AF_INET)
			continue;
		if (ioctl(s, SIOCGIFFLAGS, ifrqp) != 0)
			continue;
		iflags = ifrqp->ifr_flags;
		if ((iflags & IFF_UP) == 0 || (iflags & IFF_BROADCAST) == 0)
			continue;

		if (ioctl(s, SIOCGIFADDR, ifrqp) != 0 ||
		    ifrqp->ifr_addr.sa_family != AF_INET)
			continue;
		iname = ifrqp->ifr_name;
		if (strlen(iname) >= sizeof(ifd->id_name))
			continue;
		iaddr = (*(struct sockaddr_in *)&ifrqp->ifr_addr).sin_addr;

		if (ioctl(s, SIOCGIFNETMASK, ifrqp) != 0)
			continue;
		imask = ((struct sockaddr_in *)&ifrqp->ifr_addr)->sin_addr;

		ifd = malloc(sizeof(struct nb_ifdesc));
		if (ifd == NULL)
			return ENOMEM;
		bzero(ifd, sizeof(struct nb_ifdesc));
		strlcpy(ifd->id_name, iname, sizeof(ifd->id_name));
		ifd->id_flags = iflags;
		ifd->id_addr = iaddr;
		ifd->id_mask = imask;
		ifd->id_next = *iflist;
		*iflist = ifd;
	}
bad:
	free(ifrdata);
	close(s);
	return error;
}  

