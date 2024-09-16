/*
 * Copyright (c) 2012-2021 Apple Inc. All rights reserved.
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
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "extract.h"

#ifdef DLT_PKTAP
#ifdef __APPLE__
#include <net/pktap.h>
#include <stddef.h>
#include <strings.h>
#include <uuid/uuid.h>

/*
 * This data structure is obsolete but used by the upstream cde
 * and matches enough of the current definition of struct pktap_header
 * for basic checks
 */
typedef struct {
	nd_uint32_t	pkt_len;	/* length of pktap header */
	nd_uint32_t	pkt_rectype;	/* type of record */
	nd_uint32_t	pkt_dlt;	/* DLT type of this packet */
	char		pkt_ifname[24];	/* interface name */
	nd_uint32_t	pkt_flags;
	nd_uint32_t	pkt_pfamily;	/* "protocol family" */
	nd_uint32_t	pkt_llhdrlen;	/* link-layer header length? */
	nd_uint32_t	pkt_lltrlrlen;	/* link-layer trailer length? */
	nd_uint32_t	pkt_pid;	/* process ID */
	char		pkt_cmdname[20]; /* command name */
	nd_uint32_t	pkt_svc_class;	/* "service class" */
	nd_uint16_t	pkt_iftype;	/* "interface type" */
	nd_uint16_t	pkt_ifunit;	/* unit number of interface? */
	nd_uint32_t	pkt_epid;	/* "effective process ID" */
	char		pkt_ecmdname[20]; /* "effective command name" */
} pktap_header_t;

/*
 * Record types.
 */
#define PKT_REC_NONE	0	/* nothing follows the header */
#define PKT_REC_PACKET	1	/* a packet follows the header */

typedef struct {
	uint8_t                 pth_length;                     /* length of this header */
	uint8_t                 pth_uuid_offset;                /* max size: sizeof(uuid_t) */
	uint8_t                 pth_e_uuid_offset;              /* max size: sizeof(uuid_t) */
	uint8_t                 pth_ifname_offset;              /* max size: PKTAP_IFXNAMESIZE*/
	uint8_t                 pth_comm_offset;                /* max size: PKTAP_MAX_COMM_SIZE */
	uint8_t                 pth_e_comm_offset;              /* max size: PKTAP_MAX_COMM_SIZE */
	nd_uint16_t             pth_dlt;                        /* DLT of packet */
	nd_uint16_t             pth_frame_pre_length;
	nd_uint16_t             pth_frame_post_length;
	nd_uint16_t             pth_iftype;
	nd_uint16_t             pth_ipproto;
	nd_uint32_t             pth_protocol_family;
	nd_uint32_t             pth_svc;                        /* service class */
	nd_uint32_t             pth_flowid;
	nd_uint32_t             pth_pid;                        /* process ID */
	nd_uint32_t             pth_e_pid;                      /* effective process ID */
	nd_uint32_t             pth_flags;                      /* flags */
} pktap_v2_hdr_t;

static void
pktap_header_print(netdissect_options *ndo, const u_char *bp, u_int length)
{
	const pktap_header_t *hdr;
	uint32_t dlt, hdrlen;
	const char *dltname;

	hdr = (const pktap_header_t *)bp;

	dlt = GET_LE_U_4(hdr->pkt_dlt);
	hdrlen = GET_LE_U_4(hdr->pkt_len);
	dltname = pcap_datalink_val_to_name(dlt);
	if (!ndo->ndo_qflag) {
		ND_PRINT("DLT %s (%u) len %u",
			  (dltname != NULL ? dltname : "UNKNOWN"), dlt, hdrlen);
	} else {
		ND_PRINT("%s", (dltname != NULL ? dltname : "UNKNOWN"));
	}

	ND_PRINT(", length %u: ", length);
}

extern char *svc2str(uint32_t);

#define DEBUG 1
#ifdef DEBUG
void
print_pktap_header_debug(struct netdissect_options *ndo, struct pktap_header *pktp_hdr)
{
	ND_PRINT("pth_length %u (sizeof(struct pktap_header)  %lu)",
		   pktp_hdr->pth_length, sizeof(struct pktap_header));
	ND_PRINT(" type_next %u", pktp_hdr->pth_type_next);
	ND_PRINT(" dlt %u", pktp_hdr->pth_dlt);
	ND_PRINT(" ifname %s", pktp_hdr->pth_ifname);
	ND_PRINT(" flags 0x%x", pktp_hdr->pth_flags);
	ND_PRINT(" protocol_family %u", pktp_hdr->pth_protocol_family);
	ND_PRINT(" frame_pre_length %u", pktp_hdr->pth_frame_pre_length);
	ND_PRINT(" frame_post_length %u", pktp_hdr->pth_frame_post_length);
	ND_PRINT(" iftype %u\n", pktp_hdr->pth_iftype);
	ND_PRINT(" flowid 0x%x\n", pktp_hdr->pth_flowid);
}
#endif /* DEBUG */

static void
pktap_print_procinfo(struct netdissect_options *ndo, const char *label, const char **pprsep, char *comm, pid_t pid, uuid_t uu)
{
	if ((ndo->ndo_kflag & (PRMD_PNAME | PRMD_PID | PRMD_PUUID)) &&
	    (comm[0] != 0 || pid != -1 || uuid_is_null(uu) == 0)) {
		const char *semicolumn = "";
		
		ND_PRINT("%s%s ", *pprsep, label);
		
		if ((ndo->ndo_kflag & PRMD_PNAME)) {
			ND_PRINT("%s%s",
				  semicolumn,
				  comm[0] != 0 ? comm : "");
			semicolumn = ":";
		}
		if ((ndo->ndo_kflag & PRMD_PID)) {
			if (pid != -1)
				ND_PRINT("%s%u",
					  semicolumn, pid);
			else
				ND_PRINT("%s",
					  semicolumn);
			semicolumn = ":";
		}
		if ((ndo->ndo_kflag & PRMD_PUUID)) {
			if (uuid_is_null(uu) == 0) {
				uuid_string_t uuid_str;
				
				uuid_unparse_lower(uu, uuid_str);
				
				ND_PRINT("%s%s",
					  semicolumn, uuid_str);
			} else {
				ND_PRINT("%s",
					  semicolumn);
			}
		}
		*pprsep = ", ";
	}
}

void
print_pktap_header(struct netdissect_options *ndo, struct pktap_header *pktp_hdr)
{
	if (ndo->ndo_eflag > 2) {
		print_pktap_header_debug(ndo, pktp_hdr);
	}

	if (ndo->ndo_kflag != PRMD_NONE && ndo->ndo_kflag != PRMD_VERBOSE) {
		const char *prsep = "";

		ND_PRINT("(");

		if (ndo->ndo_kflag & PRMD_IF) {
			ND_PRINT("%s", pktp_hdr->pth_ifname);
			prsep = ", ";
		}

		pktap_print_procinfo(ndo, "proc", &prsep, pktp_hdr->pth_comm, pktp_hdr->pth_pid, pktp_hdr->pth_uuid);

		pktap_print_procinfo(ndo, "eproc", &prsep, pktp_hdr->pth_ecomm, pktp_hdr->pth_epid, pktp_hdr->pth_euuid);


		if ((ndo->ndo_kflag & PRMD_SVC) && pktp_hdr->pth_svc != -1) {
			ND_PRINT("%s" "svc %s",
				  prsep,
				  svc2str(pktp_hdr->pth_svc));
			prsep = ", ";
		}
		if (ndo->ndo_kflag & PRMD_DIR) {
			if ((pktp_hdr->pth_flags & PTH_FLAG_DIR_IN)) {
				ND_PRINT("%s" "in",
					  prsep);
				prsep = ", ";
			} else if ((pktp_hdr->pth_flags & PTH_FLAG_DIR_OUT)) {
				ND_PRINT("%s" "out",
					  prsep);
				prsep = ", ";
			}
		}
		if (ndo->ndo_kflag & PRMD_FLAGS) {
			if ((pktp_hdr->pth_flags & PTH_FLAG_NEW_FLOW)) {
				ND_PRINT("%s" "nf",
					  prsep);
				prsep = ", ";
			}
			if ((pktp_hdr->pth_flags & PTH_FLAG_KEEP_ALIVE)) {
				ND_PRINT("%s" "ka",
					  prsep);
				prsep = ", ";
			}
			if ((pktp_hdr->pth_flags & PTH_FLAG_REXMIT)) {
				ND_PRINT("%s" "re",
					  prsep);
				prsep = ", ";
			}
			if ((pktp_hdr->pth_flags & PTH_FLAG_SOCKET)) {
				ND_PRINT("%s" "so",
					  prsep);
				prsep = ", ";
			}
			if ((pktp_hdr->pth_flags & PTH_FLAG_NEXUS_CHAN)) {
				ND_PRINT("%s" "ch",
					  prsep);
				prsep = ", ";
			}
			if ((pktp_hdr->pth_flags & PTH_FLAG_WAKE_PKT)) {
				ND_PRINT("%s" "wk",
					  prsep);
				prsep = ", ";
			}
		}
		if (ndo->ndo_kflag & PRMD_FLOWID) {
			ND_PRINT("%s" "flowid 0x%x",
				  prsep,
				  pktp_hdr->pth_flowid);
			prsep = ", ";
		}
#ifdef PKTAP_HAS_TRACE_TAG
		if (ndo->ndo_kflag & PRMD_TRACETAG) {
			ND_PRINT("%s" "ttag 0x%x",
				  prsep,
				  pktp_hdr->pth_trace_tag);
			prsep = ", ";
		}
#endif /* PKTAP_HAS_TRACE_TAG */
		if (ndo->ndo_kflag & PRMD_DLT) {
			ND_PRINT("%s" "dlt 0x%x",
				  prsep,
				  pktp_hdr->pth_dlt);
			prsep = ", ";
		}
		if (pktp_hdr->pth_type_next == PTH_TYPE_DROP) {
			struct droptap_header *dtap_hdr = (struct droptap_header *)pktp_hdr;
			ND_PRINT("%s" "%s",
				prsep,
				drop_reason_str(dtap_hdr->dth_dropreason));
			prsep = ", ";
			if (dtap_hdr->dth_dropfunc_size > 0) {
				ND_PRINT("%s" "%s:%u",
					prsep,
					dtap_hdr->dth_dropfunc,
					dtap_hdr->dth_dropline);
				prsep = ", ";
			}
		}
		ND_PRINT(") ");
	}
}

static void
convert_v2_to_pktap_header(const struct pktap_v2_hdr *pktap_v2_hdr_src, struct pktap_header *pktap_header_dst)
{
	uint8_t *ptr = (uint8_t *)pktap_v2_hdr_src;

	pktap_header_dst->pth_length = sizeof(struct pktap_header);
	pktap_header_dst->pth_type_next = PTH_TYPE_PACKET;
	pktap_header_dst->pth_dlt = pktap_v2_hdr_src->pth_dlt;
	pktap_header_dst->pth_frame_pre_length = pktap_v2_hdr_src->pth_frame_pre_length;
	pktap_header_dst->pth_frame_pre_length = pktap_v2_hdr_src->pth_frame_pre_length;
	pktap_header_dst->pth_iftype = pktap_v2_hdr_src->pth_iftype;
	pktap_header_dst->pth_ipproto =  pktap_v2_hdr_src->pth_ipproto;
	pktap_header_dst->pth_protocol_family = pktap_v2_hdr_src->pth_protocol_family;
	pktap_header_dst->pth_svc = pktap_v2_hdr_src->pth_svc;
	pktap_header_dst->pth_flowid = pktap_v2_hdr_src->pth_flowid;
	pktap_header_dst->pth_pid = pktap_v2_hdr_src->pth_pid;
	pktap_header_dst->pth_epid = pktap_v2_hdr_src->pth_e_pid;
	pktap_header_dst->pth_flags = pktap_v2_hdr_src->pth_flags;
	pktap_header_dst->pth_flags &= ~PTH_FLAG_V2_HDR;

	if (pktap_v2_hdr_src->pth_uuid_offset != 0) {
		uuid_copy(pktap_header_dst->pth_uuid, (ptr + pktap_v2_hdr_src->pth_uuid_offset));
	} else {
		uuid_clear(pktap_header_dst->pth_uuid);
	}

	if (pktap_v2_hdr_src->pth_e_uuid_offset != 0) {
		uuid_copy(pktap_header_dst->pth_euuid, (ptr + pktap_v2_hdr_src->pth_e_uuid_offset));
	} else {
		uuid_clear(pktap_header_dst->pth_euuid);
	}

	if (pktap_v2_hdr_src->pth_ifname_offset != 0) {
		strlcpy(pktap_header_dst->pth_ifname, (char *)(ptr + pktap_v2_hdr_src->pth_ifname_offset), sizeof(pktap_header_dst->pth_ifname));
	} else {
		pktap_header_dst->pth_ifname[0] = 0;
	}

	if (pktap_v2_hdr_src->pth_comm_offset != 0) {
		strlcpy(pktap_header_dst->pth_comm, (char *)(ptr + pktap_v2_hdr_src->pth_comm_offset), sizeof(pktap_header_dst->pth_comm));
	} else {
		pktap_header_dst->pth_comm[0] = 0;
	}

	if (pktap_v2_hdr_src->pth_e_comm_offset != 0) {
		strlcpy(pktap_header_dst->pth_ecomm, (char *)(ptr + pktap_v2_hdr_src->pth_e_comm_offset), sizeof(pktap_header_dst->pth_ecomm));
	} else {
		pktap_header_dst->pth_ecomm[0] = 0;
	}
}

#define my_assert(x) _Static_assert(x, #x)

void
print_pktap_v2_header_debug(struct netdissect_options *ndo, const struct pktap_v2_hdr *pktp_hdr)
{
	uuid_string_t uuidstr, euuidstr;
	char *ptr = (char *)pktp_hdr;

	my_assert(sizeof(struct pktap_v2_hdr) == sizeof(pktap_v2_hdr_t));
	my_assert(sizeof(struct pktap_v2_hdr) == sizeof(pktap_v2_hdr_t));

	ND_PRINT("pth2_length %u (sizeof(struct pktap_v2_hdr) %lu sizeof(pktap_v2_hdr_t) %lu) ",
		pktp_hdr->pth_length, sizeof(struct pktap_v2_hdr), sizeof(pktap_v2_hdr_t));
	ND_PRINT("dlt %u ", pktp_hdr->pth_dlt);
	ND_PRINT("ifname %s iftype %d ",
		pktp_hdr->pth_ifname_offset != 0 ? ptr + pktp_hdr->pth_ifname_offset : "",
		pktp_hdr->pth_iftype);
	ND_PRINT("flags 0x%x ", pktp_hdr->pth_flags);
	ND_PRINT("protocol_family %u ", pktp_hdr->pth_protocol_family);
	ND_PRINT("frame_pre_length %u frame_post_length %u ",
		pktp_hdr->pth_frame_pre_length, pktp_hdr->pth_frame_post_length);
	ND_PRINT("svc %u ", pktp_hdr->pth_svc);
	ND_PRINT("flowid %u ", pktp_hdr->pth_flowid);
	ND_PRINT("ipproto %u ", pktp_hdr->pth_ipproto);
	ND_PRINT("pid %u e_pid %u ", pktp_hdr->pth_pid, pktp_hdr->pth_e_pid);

	ND_PRINT("comm %s ecomm %s ",
		pktp_hdr->pth_comm_offset != 0 ? ptr + pktp_hdr->pth_comm_offset : "",
		pktp_hdr->pth_e_comm_offset != 0 ? ptr + pktp_hdr->pth_e_comm_offset : "");
	if (pktp_hdr->pth_uuid_offset != 0)
		uuid_unparse(*(const uuid_t *)(ptr + pktp_hdr->pth_uuid_offset), uuidstr);
	if (pktp_hdr->pth_e_uuid_offset != 0)
		uuid_unparse(*(const uuid_t *)(ptr + pktp_hdr->pth_e_uuid_offset), euuidstr);
	ND_PRINT("pth2_uuid %s euuid %s\n",
		pktp_hdr->pth_uuid_offset == 0 ? "" : uuidstr,
		pktp_hdr->pth_e_uuid_offset == 0 ? "" : euuidstr);
}

void
print_pktap_v2_header(struct netdissect_options *ndo, const struct pktap_v2_hdr *pktap_v2_hdr)
{
	struct pktap_header pktp_hdr;

	convert_v2_to_pktap_header(pktap_v2_hdr, &pktp_hdr);

	print_pktap_header(ndo, &pktp_hdr);
}

void
pktap_v2_if_print(struct netdissect_options *ndo, const struct pcap_pkthdr *h,
	       const u_char *p)
{
	uint32_t dlt, hdrlen;
	u_int length = h->len;
	if_printer printer;
	struct pcap_pkthdr nhdr;
	const pktap_v2_hdr_t *hdr;

	ndo->ndo_protocol = "pktapv2";
	if (h->len < sizeof(struct pktap_v2_hdr)) {
		ND_PRINT(" (packet too short, %u < %zu)",
				 h->len, sizeof(struct pktap_v2_hdr));
		goto invalid;
	}
	hdr = (const pktap_v2_hdr_t *)p;
	hdrlen = hdr->pth_length;
	if (hdrlen < sizeof(struct pktap_v2_hdr)) {
		/*
		 * Claimed header length < structure length.
		 * XXX - does this just mean some fields aren't
		 * being supplied, or is it truly an error (i.e.,
		 * is the length supplied so that the header can
		 * be expanded in the future)?
		 */
		ND_PRINT(" (pkt_len too small, %u < %zu)",
				 hdrlen, sizeof(struct pktap_v2_hdr));
		goto invalid;
	}
	if (hdrlen > length) {
		ND_PRINT(" (pkt_len too big, %u > %u)",
				 hdrlen, length);
		goto invalid;
	}
	ND_TCHECK_LEN(p, hdrlen);

	const struct pktap_v2_hdr *pktap_v2_hdr = (const struct pktap_v2_hdr *)p;

	if (ndo->ndo_eflag > 1) {
		print_pktap_v2_header_debug(ndo, pktap_v2_hdr);
	}

	if (pktap_v2_hdr->pth_comm_offset > hdrlen) {
		ND_PRINT(" (pth_comm_offset too big, %u > %u)",
			 pktap_v2_hdr->pth_comm_offset, hdrlen);
		goto invalid;
	}
	if (pktap_v2_hdr->pth_e_comm_offset > hdrlen) {
		ND_PRINT(" (pth_e_comm_offset too big, %u > %u)",
			 pktap_v2_hdr->pth_e_comm_offset, hdrlen);
		goto invalid;
	}
	if (pktap_v2_hdr->pth_ifname_offset > hdrlen) {
		ND_PRINT(" (pth_ifname_offset too big, %u > %u)",
			 pktap_v2_hdr->pth_ifname_offset, hdrlen);
		goto invalid;
	}
	if (pktap_v2_hdr->pth_uuid_offset > hdrlen) {
		ND_PRINT(" (pth_uuid_offset too big, %u > %u)",
			 pktap_v2_hdr->pth_uuid_offset, hdrlen);
		goto invalid;
	}
	if (pktap_v2_hdr->pth_e_uuid_offset > hdrlen) {
		ND_PRINT(" (pth_e_uuid_offset too big, %u > %u)",
			 pktap_v2_hdr->pth_e_uuid_offset, hdrlen);
		goto invalid;
	}

	print_pktap_v2_header(ndo, pktap_v2_hdr);

	/*
	 * Compensate for the pktap header
	 */
	bcopy(h, &nhdr, sizeof(struct pcap_pkthdr));
	nhdr.caplen -= hdrlen;
	nhdr.len -= hdrlen;
	p += hdrlen;

	dlt = pktap_v2_hdr->pth_dlt;

	if ((printer = lookup_printer(dlt)) != NULL) {
		printer(ndo, &nhdr, p);
	} else {
		if (!ndo->ndo_suppress_default_print)
			ndo->ndo_default_print(ndo, p,nhdr.caplen);
	}
	ndo->ndo_ll_hdr_len += hdrlen;
	return;
invalid:
	nd_print_invalid(ndo);
}

void
pktap_if_print(struct netdissect_options *ndo, const struct pcap_pkthdr *h,
			   const u_char *p)
{
	uint32_t dlt, hdrlen, rectype;
	u_int caplen = h->caplen;
	u_int length = h->len;
	if_printer printer;
	const pktap_header_t *hdr;
	struct pcap_pkthdr nhdr;

	if (ndo->ndo_pktapv2) {
		pktap_v2_if_print(ndo, h, p);
		return;
	}

	ndo->ndo_protocol = "pktap";
	if (h->len < sizeof(pktap_header_t)) {
		ND_PRINT(" (packet too short, %u < %zu)",
				 h->len, sizeof(pktap_header_t));
		goto invalid;
	}

	hdr = (const pktap_header_t *)p;
	dlt = GET_LE_U_4(hdr->pkt_dlt);
	hdrlen = GET_LE_U_4(hdr->pkt_len);
	if (hdrlen < sizeof(pktap_header_t)) {
		/*
		 * Claimed header length < structure length.
		 * XXX - does this just mean some fields aren't
		 * being supplied, or is it truly an error (i.e.,
		 * is the length supplied so that the header can
		 * be expanded in the future)?
		 */
		ND_PRINT(" (pkt_len too small, %u < %zu)",
				 hdrlen, sizeof(pktap_header_t));
		goto invalid;
	}
	if (hdrlen > length) {
		ND_PRINT(" (pkt_len too big, %u > %u)",
				 hdrlen, length);
		goto invalid;
	}
	ND_TCHECK_LEN(p, hdrlen);

	if (hdrlen < sizeof(struct pktap_header)) {
		if (ndo->ndo_eflag)
			pktap_header_print(ndo, p, length);

		length -= hdrlen;
		caplen -= hdrlen;
		p += hdrlen;

		rectype = GET_LE_U_4(hdr->pkt_rectype);
		switch (rectype) {

		case PKT_REC_NONE:
			ND_PRINT("no data");
			break;

		case PKT_REC_PACKET:
			printer = lookup_printer(dlt);
			if (printer != NULL) {
				nhdr = *h;
				nhdr.caplen = caplen;
				nhdr.len = length;
				printer(ndo, &nhdr, p);
				hdrlen += ndo->ndo_ll_hdr_len;
			} else {
				if (!ndo->ndo_eflag)
					pktap_header_print(ndo, (const u_char *)hdr,
							length + hdrlen);

				if (!ndo->ndo_suppress_default_print)
					ND_DEFAULTPRINT(p, caplen);
			}
			break;
		}

		ndo->ndo_ll_hdr_len += hdrlen;
		return;
	}

	struct pktap_header *pktp_hdr = (struct pktap_header *)p;

	print_pktap_header(ndo, pktp_hdr);

	/*
	 * Compensate for the pktap header
	 */
	bcopy(h, &nhdr, sizeof(struct pcap_pkthdr));
	nhdr.caplen -= pktp_hdr->pth_length;
	nhdr.len -= pktp_hdr->pth_length;
	if (pktp_hdr->pth_type_next == PTH_TYPE_DROP) {
		p += DROPTAP_HDR_SIZE((struct droptap_header *)p);
	} else {
		p += pktp_hdr->pth_length;
	}
	dlt = pktp_hdr->pth_dlt;

	if (pktp_hdr->pth_type_next == PTH_TYPE_PACKET ||
	    pktp_hdr->pth_type_next == PTH_TYPE_DROP) {
		if ((printer = lookup_printer(dlt)) != NULL) {
			printer(ndo, &nhdr, p);
		} else {
			if (!ndo->ndo_suppress_default_print)
				ndo->ndo_default_print(ndo, p,nhdr.caplen);
		}
	}
	ndo->ndo_ll_hdr_len += pktp_hdr->pth_length;
	return;
invalid:
	nd_print_invalid(ndo);
}
#endif /* __APPLE__ */
#endif /* DLT_PKTAP */
