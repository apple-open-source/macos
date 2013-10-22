/*
 * Copyright (c) 2001-2013 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * eapol_socket.c
 * - wrapper for allocating an NDRV socket for use with 802.1X
 */

/* 
 * Modification History
 *
 * October 26, 2001	Dieter Siegmund (dieter@apple)
 * - created
 *
 * August 31, 2010	Dieter Siegmund (dieter@apple)
 * - combined ndrv_socket.c/eapol_socket.c, moved to framework
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/dlil.h>
#include <net/ndrv.h>
#include <net/ethernet.h>
#include <sys/sockio.h>
#include <fcntl.h>
#include <sys/filio.h>
#include <syslog.h>
#include "EAPOL.h"
#include "EAPLog.h"
#include "symbol_scope.h"
#include "eapol_socket.h"

#define EAPOL_802_1_X_FAMILY	0x8021ec /* XXX needs official number! */

static const struct ether_addr eapol_multicast = {
    EAPOL_802_1_X_GROUP_ADDRESS
};

STATIC int
ndrv_socket(const char * ifname)
{
    struct sockaddr_ndrv 	ndrv;
    int 			s;

    s = socket(AF_NDRV, SOCK_RAW, 0);
    if (s < 0) {
	EAPLOG_FL(LOG_NOTICE, "socket() failed: %s",
		  strerror(errno));
	goto failed;
    }
    strlcpy((char *)ndrv.snd_name, ifname, sizeof(ndrv.snd_name));
    ndrv.snd_len = sizeof(ndrv);
    ndrv.snd_family = AF_NDRV;
    if (bind(s, (struct sockaddr *)&ndrv, sizeof(ndrv)) < 0) {
	EAPLOG_FL(LOG_NOTICE, "bind() failed: %s", strerror(errno));
	goto failed;
    }
    return (s);
 failed:
    if (s >= 0) {
	close(s);
    }
    return (-1);
}

STATIC int
ndrv_socket_bind(int s, u_int32_t family, const u_int16_t * ether_types,
		 int ether_types_count)
{
    int				i;
    struct ndrv_protocol_desc	proto;
    struct ndrv_demux_desc *	demux;
    int				status;

    demux = (struct ndrv_demux_desc *)
	malloc(sizeof(*demux) * ether_types_count);
    proto.version = NDRV_PROTOCOL_DESC_VERS;
    proto.protocol_family = family;
    proto.demux_count = ether_types_count;
    proto.demux_list = demux;
    for (i = 0; i < ether_types_count; i++) {
	demux[i].type = NDRV_DEMUXTYPE_ETHERTYPE;
	demux[i].length = sizeof(demux[i].data.ether_type);
	demux[i].data.ether_type = htons(ether_types[i]);
    }
    status = setsockopt(s, SOL_NDRVPROTO, NDRV_SETDMXSPEC, 
			(caddr_t)&proto, sizeof(proto));
    free(demux);
    if (status < 0) {
	syslog(LOG_NOTICE, "setsockopt(NDRV_SETDMXSPEC) failed: %s", 
	       strerror(errno));
	return (status);
    }
    return (0);
}

STATIC int
ndrv_socket_add_multicast(int s, const struct sockaddr_dl * dl_p)
{
    int			status;

    status = setsockopt(s, SOL_NDRVPROTO, NDRV_ADDMULTICAST, 
			dl_p, dl_p->sdl_len);
    if (status < 0) {
	syslog(LOG_NOTICE, "setsockopt(NDRV_ADDMULTICAST) failed: %s", 
	       strerror(errno));
	return (status);
    }
    return (0);
}

STATIC bool
eapol_socket_add_multicast(int s)
{
    struct sockaddr_dl		dl;

    bzero(&dl, sizeof(dl));
    dl.sdl_len = sizeof(dl);
    dl.sdl_family = AF_LINK;
    dl.sdl_type = IFT_ETHER;
    dl.sdl_nlen = 0;
    dl.sdl_alen = sizeof(eapol_multicast);
    bcopy(&eapol_multicast,
	  dl.sdl_data, 
	  sizeof(eapol_multicast));
    if (ndrv_socket_add_multicast(s, &dl) < 0) {
	syslog(LOG_NOTICE, "eapol_socket: ndrv_socket_add_multicast failed, %s",
	       strerror(errno));
	return (false);
    }
    return (true);
}

int
eapol_socket(const char * ifname, bool is_wireless)
{
    uint16_t		ether_types[2] = { EAPOL_802_1_X_ETHERTYPE,
					   IEEE80211_PREAUTH_ETHERTYPE };
    int			ether_types_count;
    int 		opt = 1;
    int 		s;

    s = ndrv_socket(ifname);
    if (s < 0) {
	syslog(LOG_NOTICE, "eapol_socket: ndrv_socket failed");
	goto failed;
    }
    if (ioctl(s, FIONBIO, &opt) < 0) {
	syslog(LOG_NOTICE, "eapol_socket: FIONBIO failed, %s", 
	       strerror(errno));
	goto failed;
    }
    if (is_wireless == false) {
	/* ethernet needs multicast */
	ether_types_count = 1;
	if (eapol_socket_add_multicast(s) == false) {
	    goto failed;
	}
    }
    else {
	ether_types_count = 2;
    }
    if (ndrv_socket_bind(s, EAPOL_802_1_X_FAMILY, ether_types,
			 ether_types_count) < 0) {
	syslog(LOG_NOTICE, "eapol_socket: ndrv_socket_bind failed, %s",
	       strerror(errno));
	goto failed;
    }
    return (s);
 failed:
    if (s >= 0) {
	close(s);
    }
    return (-1);
    
}

#ifdef TEST_EAPOL_SOCKET

static int
get_ifm_type(const char * name)
{
    int			i;
    struct ifmediareq	ifm;
    int			media_static[20];
    int			media_static_count = sizeof(media_static) / sizeof(media_static[0]);
    int			s;
    int			ifm_type = 0;
    bool		supports_full_duplex = false;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
	perror("socket");
	goto done;
    }
    bzero(&ifm, sizeof(ifm));
    strlcpy(ifm.ifm_name, name, sizeof(ifm.ifm_name));
    if (ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifm) < 0) {
	goto done;
    }
    ifm_type = IFM_TYPE(ifm.ifm_current);
    if (ifm_type != IFM_ETHER) {
	goto done;
    }
    if (ifm.ifm_count == 0) {
	goto done;
    }
    if (ifm.ifm_count > media_static_count) {
	ifm.ifm_ulist = (int *)malloc(ifm.ifm_count * sizeof(int));
    }
    else {
	ifm.ifm_ulist = media_static;
    }
    if (ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifm) == -1) {
	goto done;
    }
    if (ifm.ifm_count == 1
	&& IFM_SUBTYPE(ifm.ifm_ulist[0]) == IFM_AUTO) {
	/* only support autoselect, not really ethernet */
	goto done;
    }
    for (i = 0; i < ifm.ifm_count; i++) {
	if ((ifm.ifm_ulist[i] & IFM_FDX) != 0) {
	    supports_full_duplex = true;
	    break;
	}
    }

 done:
    if (s >= 0) {
	close(s);
    }
    if (ifm_type == IFM_ETHER && supports_full_duplex == false) {
	/* not really ethernet */
	ifm_type = 0;
    }
    return (ifm_type);
}

#include <sys/stat.h>

int
main(int argc, const char * argv[])
{
    int fd;
    int ifm_type;

    if (argc < 2) {
	fprintf(stderr, "usage: eapol <ifname>\n");
	exit(1);
    }
    ifm_type = get_ifm_type(argv[1]);
    switch (ifm_type) {
    case IFM_ETHER:
	break;
    case IFM_IEEE80211:
	break;
    default:
	fprintf(stderr, "interface %s is not valid\n", argv[1]);
	exit(1);
	break;
    }
    fd = eapol_socket(argv[1], (ifm_type == IFM_IEEE80211));
    if (fd < 0) {
	fprintf(stderr, "eapol_socket(%s) failed\n", argv[1]);
    }
    {
	struct stat		sb;

	if (fstat(fd, &sb) == 0) {
	    if (S_ISSOCK(sb.st_mode)) {
		fprintf(stderr, "%d a socket\n", fd);
	    }
	}
    }

    close(fd);
    exit(1);
}

#endif /* TEST_EAPOL_SOCKET */
