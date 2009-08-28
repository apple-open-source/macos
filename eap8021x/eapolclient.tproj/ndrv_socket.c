
/*
 * Copyright (c) 2001-2002 Apple Computer, Inc. All rights reserved.
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
 * ndrv_socket.c
 * - wrapper for allocating an NDRV socket
 */

/* 
 * Modification History
 *
 * October 26, 2001	Dieter Siegmund (dieter@apple)
 * - created
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/dlil.h>
#include <net/ndrv.h>
#include <net/ethernet.h>
#include <sys/sockio.h>

#include "ndrv_socket.h"

int
ndrv_socket(const char * ifname)
{
    struct sockaddr_ndrv 	ndrv;
    int 			s;

    s = socket(AF_NDRV, SOCK_RAW, 0);
    if (s < 0) {
	fprintf(stderr, "ndrv_socket: socket() failed: %s\n", strerror(errno));
	goto failed;
    }
    strlcpy((char *)ndrv.snd_name, ifname, sizeof(ndrv.snd_name));
    ndrv.snd_len = sizeof(ndrv);
    ndrv.snd_family = AF_NDRV;
    if (bind(s, (struct sockaddr *)&ndrv, sizeof(ndrv)) < 0) {
	fprintf(stderr, "ndrv_socket: bind() failed: %s\n", strerror(errno));
	goto failed;
    }
    return (s);
 failed:
    if (s >= 0) {
	close(s);
    }
    return (-1);
}

int
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
        fprintf(stderr, "setsockopt(NDRV_SETDMXSPEC) failed: %s\n", 
		strerror(errno));
	return (status);
    }
    return (0);
}

int
ndrv_socket_add_multicast(int s, const struct sockaddr_dl * dl_p)
{
    int			status;

    status = setsockopt(s, SOL_NDRVPROTO, NDRV_ADDMULTICAST, 
			dl_p, dl_p->sdl_len);
    if (status < 0) {
        fprintf(stderr, "setsockopt(NDRV_ADDMULTICAST) failed: %s\n", 
		strerror(errno));
	return (status);
    }
    return (0);
}

int
ndrv_socket_remove_multicast(int s, const struct sockaddr_dl * dl_p)
{
    int			status;

    status = setsockopt(s, SOL_NDRVPROTO, NDRV_DELMULTICAST, 
			dl_p, dl_p->sdl_len);
    if (status < 0) {
        fprintf(stderr, "setsockopt(NDRV_DELMULTICAST) failed: %s\n", 
		strerror(errno));
	return (status);
    }
    return (0);
}

#ifdef TEST_NDRV_SOCKET
int
main(int argc, const char * argv[])
{
    int fd;

    if (argc < 1) {
	fprintf(stderr, "usage: ndrv <ifname>\n");
	exit(1);
    }
    fd = ndrv_socket(argv[1]);
    if (fd < 0) {
	fprintf(stderr, "ndrv_socket(%s) failed\n", argv[1]);
    }
    close(fd);
    exit(1);
}

#endif TEST_NDRV_SOCKET
