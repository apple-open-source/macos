/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Portions of this software have been released under the following terms:
 *
 * (c) Copyright 1989-1993 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989-1993 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989-1993 DIGITAL EQUIPMENT CORPORATION
 *
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 * permission to use, copy, modify, and distribute this file for any
 * purpose is hereby granted without fee, provided that the above
 * copyright notices and this notice appears in all source code copies,
 * and that none of the names of Open Software Foundation, Inc., Hewlett-
 * Packard Company or Digital Equipment Corporation be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  Neither Open Software
 * Foundation, Inc., Hewlett-Packard Company nor Digital
 * Equipment Corporation makes any representations about the suitability
 * of this software for any purpose.
 *
 * Copyright (c) 2007, Novell, Inc. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Novell Inc. nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* Horrible Darwin/BSD hack */
#if (defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__)) && defined(_POSIX_C_SOURCE)
#undef _POSIX_C_SOURCE
#endif

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#ifndef UUID_BUILD_STANDALONE
#include <dce/dce.h>
#include <dce/dce_utils.h>
#else
#include "uuid.h"
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
/* Bizarre hack for HP-UX ia64 where a system header
 * makes reference to a kernel-only data structure
 */
#if defined(__hpux) && defined(__ia64)
union mpinfou {};
#endif
#include <net/if.h>
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif
#ifdef notdef
#include <sys/sysctl.h>
#endif
#ifdef HAVE_NET_IF_DL_H
#include <net/if_dl.h>
#endif
#ifdef HAVE_NET_IF_ARP_H
#include <net/if_arp.h>
#endif
void dce_get_802_addr(dce_802_addr_t *addr, error_status_t *st)
{
	char buf[sizeof(struct ifreq) * 32];
	struct ifconf ifc;
	struct ifreq *ifr;
	int s, i;
	struct sockaddr *sa;
#ifdef AF_LINK
	struct sockaddr_dl *sdl;
#endif
#if defined(SIOCGARP)
	struct arpreq arpreq;
#endif
	struct ifreq ifreq;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		*st = utils_s_802_cant_read;
		return;
	}

	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	memset(buf, 0, sizeof(buf));

	i = ioctl(s, SIOCGIFCONF, (char *)&ifc);
	if (i < 0) {
		*st = utils_s_802_cant_read;
		close(s);
		return;
	}

	for (i = 0; i < ifc.ifc_len; ) {
		ifr = (struct ifreq *)&ifc.ifc_buf[i];
		/* Initialize sa here because it is used under the next
		 * label, and the loopback check could jump directly
		 * to the next label.
		 */
#ifdef AF_LINK
		sa = &ifr->ifr_addr;
#else
		sa = NULL;
#endif

#ifdef SIOCGIFFLAGS
		/* Skip over loopback and point-to-point interfaces. */
		memcpy(&ifreq, ifr, sizeof(ifreq));
		if (ioctl(s, SIOCGIFFLAGS, &ifreq) == 0) {
			if (ifreq.ifr_flags & (IFF_POINTOPOINT|IFF_LOOPBACK)) {
			  goto next;
			}
		}
#endif

#ifdef AF_LINK
		if (sa->sa_family == AF_LINK) {
			sdl = (struct sockaddr_dl *)sa;
			if (sdl->sdl_alen == 6) {
				memcpy(addr->eaddr, (unsigned char *)sdl->sdl_data + sdl->sdl_nlen, 6);
				*st = error_status_ok;
				close(s);
				return;
			}
		}
#endif /* AF_LINK */

#if defined(SIOCGIFHWADDR)
		memcpy(&ifreq, ifr, sizeof(ifreq));
		if (ioctl(s, SIOCGIFHWADDR, &ifreq) == 0) {
			memcpy(addr->eaddr, &ifreq.ifr_hwaddr.sa_data, 6);
			*st = error_status_ok;
			close(s);
			return;
		}
#elif defined(SIOCGARP)
		memset(&arpreq, 0, sizeof(arpreq));
		arpreq.arp_pa = ifr->ifr_dstaddr;
		arpreq.arp_flags = 0;
		if (ioctl(s, SIOCGARP, &arpreq) == 0) {
#ifdef AF_LINK
			sdl = (struct sockaddr_dl *)&arpreq.arp_ha;
			memcpy(addr->eaddr, (unsigned char *)&sdl->sdl_data + sdl->sdl_nlen, 6);
#else
			memcpy(addr->eaddr, (unsigned char*)&arpreq.arp_ha.sa_data[0], 6);
#endif
			*st = error_status_ok;
			close(s);
			return;
		}
#elif defined(SIOCGENADDR)
		memcpy(&ifreq, ifr, sizeof(ifreq));
		if (ioctl(s, SIOCGENADDR, &ifreq) == 0) {
			memcpy(addr->eaddr, ifreq.ifr_enaddr, 6);
			*st = error_status_ok;
			close(s);
			return;
		}
#else
		//#error Please implement dce_get_802_addr() for your operating system
#endif

	next:
#if !defined(__svr4__) && !defined(linux) && !defined(_HPUX) /* XXX FixMe to be portable */
		if (sa && sa->sa_len > sizeof(ifr->ifr_addr)) {
			i += sizeof(ifr->ifr_name) + sa->sa_len;
		} else
#endif
			i += sizeof(*ifr);

	}

	{
	    unsigned int seed = 1;
	    unsigned char* buf = (unsigned char*) addr->eaddr;
	    int i;

	    for (i = 0; i < 6; i++)
	    {
		buf[i] = rand_r(&seed);
	    }

	    close(s);
	    return;
	}

	*st = utils_s_802_cant_read;
	close(s);
	return;
}
