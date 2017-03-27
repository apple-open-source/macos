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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <netdissect-stdinc.h>

#include <pcap.h>

#include "netdissect.h"

#ifdef DLT_PKTAP

#include <net/pktap.h>
#include <strings.h>
#include <uuid/uuid.h>

extern char *svc2str(uint32_t);

#define DEBUG 1
#ifdef DEBUG
void
print_pktap_header(struct netdissect_options *ndo, struct pktap_header *pktp_hdr)
{
	ND_PRINT((ndo, "pth_length %u (sizeof(struct pktap_header)  %lu)\n",
		   pktp_hdr->pth_length, sizeof(struct pktap_header)));
	ND_PRINT((ndo, "pth_type_next %u\n", pktp_hdr->pth_type_next));
	ND_PRINT((ndo, "pth_dlt %u\n", pktp_hdr->pth_dlt));
	ND_PRINT((ndo, "pth_ifname %s\n", pktp_hdr->pth_ifname));
	ND_PRINT((ndo, "pth_flags 0x%x\n", pktp_hdr->pth_flags));
	ND_PRINT((ndo, "pth_protocol_family %u\n", pktp_hdr->pth_protocol_family));
	ND_PRINT((ndo, "pth_frame_pre_length %u\n", pktp_hdr->pth_frame_pre_length));
	ND_PRINT((ndo, "pth_frame_post_length %u\n", pktp_hdr->pth_frame_post_length));
	ND_PRINT((ndo, "pth_pid %d\n", pktp_hdr->pth_pid));
	ND_PRINT((ndo, "pth_comm %s\n", pktp_hdr->pth_comm));
	ND_PRINT((ndo, "pth_svc %u\n", pktp_hdr->pth_svc));
	ND_PRINT((ndo, "pth_epid %d\n", pktp_hdr->pth_epid));
	ND_PRINT((ndo, "pth_ecomm %s\n", pktp_hdr->pth_ecomm));
}
#endif /* DEBUG */

static void
pktap_print_procinfo(struct netdissect_options *ndo, const char *label, const char **pprsep, char *comm, pid_t pid, uuid_t uu)
{
	if ((ndo->ndo_kflag & (PRMD_PNAME | PRMD_PID | PRMD_PUUID)) &&
	    (comm[0] != 0 || pid != -1 || uuid_is_null(uu) == 0)) {
		const char *semicolumn = "";
		
		ND_PRINT((ndo, "%s%s ", *pprsep, label));
		
		if ((ndo->ndo_kflag & PRMD_PNAME)) {
			ND_PRINT((ndo, "%s%s",
				  semicolumn,
				  comm[0] != 0 ? comm : ""));
			semicolumn = ":";
		}
		if ((ndo->ndo_kflag & PRMD_PID)) {
			if (pid != -1)
				ND_PRINT((ndo, "%s%u",
					  semicolumn, pid));
			else
				ND_PRINT((ndo, "%s",
					  semicolumn));
			semicolumn = ":";
		}
		if ((ndo->ndo_kflag & PRMD_PUUID)) {
			if (uuid_is_null(uu) == 0) {
				uuid_string_t uuid_str;
				
				uuid_unparse_lower(uu, uuid_str);
				
				ND_PRINT((ndo, "%s%s",
					  semicolumn, uuid_str));
			} else {
				ND_PRINT((ndo, "%s",
					  semicolumn));
			}
		}
		*pprsep = ", ";
	}
}

u_int
pktap_if_print(struct netdissect_options *ndo, const struct pcap_pkthdr *h,
			   const u_char *p)
{
	struct pktap_header *pktp_hdr;
	uint32_t dlt;
	if_printer printer;
	struct pcap_pkthdr tmp_hdr;
	
	pktp_hdr = (struct pktap_header *)p;

	if (h->len < sizeof(struct pktap_header) ||
		h->caplen < sizeof(struct pktap_header) ||
		pktp_hdr->pth_length > h->caplen) {
		ND_PRINT((ndo, "[|pktap]"));
		return sizeof(struct pktap_header);
	}
	
#ifdef DEBUG
	if (ndo->ndo_eflag > 1)
		print_pktap_header(ndo, pktp_hdr);
#endif
	
	if (ndo->ndo_kflag != PRMD_NONE) {
		const char *prsep = "";
		
		ND_PRINT((ndo, "("));
		
		if (ndo->ndo_kflag & PRMD_IF) {
			ND_PRINT((ndo, "%s", pktp_hdr->pth_ifname));
			prsep = ", ";
		}
		
		pktap_print_procinfo(ndo, "proc", &prsep, pktp_hdr->pth_comm, pktp_hdr->pth_pid, pktp_hdr->pth_uuid);

		pktap_print_procinfo(ndo, "eproc", &prsep, pktp_hdr->pth_ecomm, pktp_hdr->pth_epid, pktp_hdr->pth_euuid);


		if ((ndo->ndo_kflag & PRMD_SVC) && pktp_hdr->pth_svc != -1) {
			ND_PRINT((ndo, "%ssvc %s",
				  prsep,
				  svc2str(pktp_hdr->pth_svc)));
			prsep = ", ";
		}
		if (ndo->ndo_kflag & PRMD_DIR) {
			if ((pktp_hdr->pth_flags & PTH_FLAG_DIR_IN)) {
				ND_PRINT((ndo, "%sin",
					  prsep));
				prsep = ", ";
			} else if ((pktp_hdr->pth_flags & PTH_FLAG_DIR_OUT)) {
				ND_PRINT((ndo, "%sout",
					  prsep));
				prsep = ", ";
			}
		}
		ND_PRINT((ndo, ") "));
	}

	/*
	 * Compensate for the pktap header
	 */
	bcopy(h, &tmp_hdr, sizeof(struct pcap_pkthdr));
	tmp_hdr.caplen -= pktp_hdr->pth_length;
	tmp_hdr.len -= pktp_hdr->pth_length;
	p += pktp_hdr->pth_length;

	dlt = pktp_hdr->pth_dlt;

	if ((printer = lookup_printer(dlt)) != NULL) {
		printer(ndo, &tmp_hdr, p);
	} else {
		if (!ndo->ndo_suppress_default_print)
			ndo->ndo_default_print(ndo, p,tmp_hdr.caplen);
	}
	
	return sizeof(struct pktap_header);
}

#endif /* DLT_PKTAP */
