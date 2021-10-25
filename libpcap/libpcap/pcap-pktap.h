/*
 * Copyright (c) 2018 Apple Inc. All rights reserved.
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

#ifndef pcap_pktap_h
#define pcap_pktap_h

#include <net/pktap.h>

#include <stdbool.h>

#ifndef PTH_FLAG_REXMIT
#define	PTH_FLAG_REXMIT		0x00008000 /* Packet is a retransmission */
#endif /* PTH_FLAG_REXMIT */

#ifndef PTH_FLAG_KEEP_ALIVE
#define	PTH_FLAG_KEEP_ALIVE	0x00010000 /* Is keep alive packet */
#endif /* PTH_FLAG_KEEP_ALIVE */

#ifndef PTH_FLAG_SOCKET
#define	PTH_FLAG_SOCKET		0x00020000 /* Packet on a Socket */
#endif /* PTH_FLAG_SOCKET */

#ifndef PTH_FLAG_NEXUS_CHAN
#define	PTH_FLAG_NEXUS_CHAN	0x00040000 /* Packet on a nexus channel */
#endif /* PTH_FLAG_NEXUS_CHAN */

#ifndef PTH_FLAG_V2_HDR
#define PTH_FLAG_V2_HDR		0x00080000 /* Version 2 of pktap */
#endif /* PTH_FLAG_V2_HDR */


pcap_t *pktap_create(const char *device, char *ebuf, int *is_ours);


#endif /* pcap_pktap_h */
