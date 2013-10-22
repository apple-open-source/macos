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

#include <tcpdump-stdinc.h>

#include <pcap.h>

#include "interface.h"

#ifdef DLT_PKTAP

#include <net/pktap.h>
#include <strings.h>

extern char *svc2str(uint32_t);

#define DEBUG 1
#ifdef DEBUG
void
print_pktap_header(struct pktap_header *pktp_hdr)
{
	printf("pth_length %u (sizeof(struct pktap_header)  %lu)\n",
		   pktp_hdr->pth_length, sizeof(struct pktap_header));
	printf("pth_type_next %u\n", pktp_hdr->pth_type_next);
	printf("pth_dlt %u\n", pktp_hdr->pth_dlt);
	printf("pth_ifname %s\n", pktp_hdr->pth_ifname);
	printf("pth_flags 0x%x\n", pktp_hdr->pth_flags);
	printf("pth_protocol_family %u\n", pktp_hdr->pth_protocol_family);
	printf("pth_frame_pre_length %u\n", pktp_hdr->pth_frame_pre_length);
	printf("pth_frame_post_length %u\n", pktp_hdr->pth_frame_post_length);
	printf("pth_pid %d\n", pktp_hdr->pth_pid);
	printf("pth_comm %s\n", pktp_hdr->pth_comm);
	printf("pth_svc %u\n", pktp_hdr->pth_svc);
	printf("pth_epid %d\n", pktp_hdr->pth_epid);
	printf("pth_ecomm %s\n", pktp_hdr->pth_ecomm);
}
#endif /* DEBUG */

u_int
pktap_if_print(struct netdissect_options *ndo, const struct pcap_pkthdr *h,
			   const u_char *p)
{
	struct pktap_header *pktp_hdr;
	uint32_t dlt;
	if_ndo_printer ndo_printer;
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
	if (eflag > 1)
		print_pktap_header(pktp_hdr);
#endif
	
	if (kflag != PRMD_NONE) {
		const char *prsep = "";
		
		ND_PRINT((ndo, "("));
	
		if (kflag & PRMD_IF) {
			ND_PRINT((ndo, "%s", pktp_hdr->pth_ifname));
			prsep = ", ";
		}
		if (pktp_hdr->pth_pid != -1) {
			switch ((kflag & (PRMD_PNAME |PRMD_PID))) {
				case (PRMD_PNAME |PRMD_PID):
					ND_PRINT((ndo, "%sproc %s:%u",
							  prsep,
							  pktp_hdr->pth_comm, pktp_hdr->pth_pid));
					prsep = ", ";
                    if ((pktp_hdr->pth_flags & PTH_FLAG_PROC_DELEGATED)) {
                        ND_PRINT((ndo, "%seproc %s:%u",
                                  prsep,
                                  pktp_hdr->pth_ecomm, pktp_hdr->pth_epid));
                        prsep = ", ";
                    }
					break;
				case PRMD_PNAME:
					ND_PRINT((ndo, "%sproc %s",
							  prsep,
							  pktp_hdr->pth_comm));
					prsep = ", ";
                    if ((pktp_hdr->pth_flags & PTH_FLAG_PROC_DELEGATED)) {
                        ND_PRINT((ndo, "%seproc %s",
                                  prsep,
                                  pktp_hdr->pth_ecomm));
                        prsep = ", ";
                    }
					break;
					
				case PRMD_PID:
					ND_PRINT((ndo, "%sproc %u",
							  prsep,
							  pktp_hdr->pth_pid));
					prsep = ", ";
                    if ((pktp_hdr->pth_flags & PTH_FLAG_PROC_DELEGATED)) {
                        ND_PRINT((ndo, "%seproc %u",
                                  prsep,
                                  pktp_hdr->pth_epid));
                        prsep = ", ";
                    }
					break;
					
				default:
					break;
			}
		}
		if ((kflag & PRMD_SVC) && pktp_hdr->pth_svc != -1) {
			ND_PRINT((ndo, "%ssvc %s",
					  prsep,
					  svc2str(pktp_hdr->pth_svc)));
			prsep = ", ";
		}
		if (kflag & PRMD_DIR) {
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
		printer(&tmp_hdr, p);
	} else if ((ndo_printer = lookup_ndo_printer(dlt)) != NULL) {
		ndo_printer(ndo, &tmp_hdr, p);
	} else {
		if (!ndo->ndo_suppress_default_print)
			ndo->ndo_default_print(ndo, p,tmp_hdr.caplen);
	}
	
	return sizeof(struct pktap_header);
}

#endif /* DLT_PKTAP */
