/*
 * Copyright (c) 2012-2013 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#ifdef __APPLE__

#define NETDISSECT_REWORKED
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#define __APPLE_PCAP_NG_API
#include <net/pktap.h>
#include <pcap.h>

#ifdef DLT_PCAPNG
#include <pcap/pcap-ng.h>
#include <pcap/pcap-util.h>
#endif /* DLT_PCAPNG */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/queue.h>

#include "netdissect.h"
#include "interface.h"
#include "pktmetadatafilter.h"

extern node_t *pkt_meta_data_expression;

extern netdissect_options *gndo;

extern char *filter_src_buf;

extern int pktap_if_count;

extern u_int packets_mtdt_fltr_drop;

extern char *svc2str(uint32_t);


/*
 * Returns zero if the packet doesn't match, non-zero if it matches
 */
int
pktap_filter_packet(pcap_t *pcap, struct pcap_if_info *if_info,
					const struct pcap_pkthdr *h, const u_char *sp)
{
	struct pktap_header *pktp_hdr;
	const u_char *pkt_data;
	int match = 0;
	
	pktp_hdr = (struct pktap_header *)sp;

	if (h->len < sizeof(struct pktap_header) ||
		h->caplen < sizeof(struct pktap_header) ||
		pktp_hdr->pth_length > h->caplen) {
		error("%s: Packet too short", __func__);
		return (0);
	}
	
	if (if_info == NULL) {
		if_info = pcap_find_if_info_by_name(pcap, pktp_hdr->pth_ifname);
		/*
		 * New interface
		 */
		if (if_info == NULL) {
			if_info = pcap_add_if_info(pcap, pktp_hdr->pth_ifname,
											-1, pktp_hdr->pth_dlt, gndo->ndo_snaplen);
			if (if_info == NULL) {
				error("%s: pcap_add_if_info(%s, %u) failed: %s",
					  __func__, pktp_hdr->pth_ifname, pktp_hdr->pth_dlt, pcap_geterr(pcap));
				return (0);
			}
		}
	}
	
	if (if_info->if_filter_program.bf_insns == NULL)
		match = 1;
	else {
		/*
		 * The actual data packet is past the packet tap header
		 */
		struct pcap_pkthdr tmp_hdr;
        
		bcopy(h, &tmp_hdr, sizeof(struct pcap_pkthdr));
        
		tmp_hdr.caplen -= pktp_hdr->pth_length;
		tmp_hdr.len -= pktp_hdr->pth_length;

		pkt_data = sp + pktp_hdr->pth_length;
        
		match = pcap_offline_filter(&if_info->if_filter_program, &tmp_hdr, pkt_data);
	}
	/*
	 * Filter on packet metadata
	 */
	if (match && pkt_meta_data_expression != NULL) {
		struct pkt_meta_data pmd;
		
		pmd.itf = &pktp_hdr->pth_ifname[0];
		pmd.proc = &pktp_hdr->pth_comm[0];
		pmd.eproc = &pktp_hdr->pth_ecomm[0];
		pmd.pid = pktp_hdr->pth_pid;
		pmd.epid = pktp_hdr->pth_epid;
		pmd.svc = svc2str(pktp_hdr->pth_svc);
		pmd.dir = (pktp_hdr->pth_flags & PTH_FLAG_DIR_IN) ? "in" :
			(pktp_hdr->pth_flags & PTH_FLAG_DIR_OUT) ? "out" : "";
		
		match = evaluate_expression(pkt_meta_data_expression, &pmd);
		if (match == 0)
			packets_mtdt_fltr_drop++;
	}
	
	return (match);
}

#endif /* __APPLE__ */
