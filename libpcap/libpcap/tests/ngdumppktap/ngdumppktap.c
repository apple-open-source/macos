/*
 * Copyright (c) 2018 Apple Inc. All rights reserved.
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

#include <net/bpf.h>
#include <net/pktap.h>

#include <assert.h>
#include <err.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include "pcap/pcap-ng.h"
#include "pcap-util.h"
#include "pcap-pktap.h"

bool capture_done = false;
bool cap_pktap_v2 = false;
bool dump_pktap_v1 = true;
bool dump_pktap_v2 = false;
const char *filename = "pktap.pcapng";
const char *ifname = "pktap";
bool truncation = false;
int max_count = INT_MAX;
int verbosity = 0;
size_t max_length = 0;
uint32_t snap_len = 0;
bool want_pktap = true;

struct dump_info {
	pcap_t *p_cap;
	
	char *ifname_v1;
	pcap_dumper_t *dumper_v1;
	char *filename_v1;
	int packet_count_v1;
	size_t total_pktap_header_space;
	size_t total_bpf_length_v1;

	char *ifname_v2;
	pcap_dumper_t *dumper_v2;
	char *filename_v2;
	int packet_count_v2;
	size_t total_pktap_v2_hdr_space;
	size_t total_bpf_length_v2;
};

#define MAX_PACKET_LEN 65535

static u_char temp_buffer[1024 * 1024];

static void
print_pktap_header(const struct pktap_header *pktp_hdr)
{
	uuid_string_t uuidstr, euuidstr;
	
	fprintf(stderr, "pth_length %u (sizeof(struct pktap_header) %lu)\n",
		     pktp_hdr->pth_length, sizeof(struct pktap_header));
	fprintf(stderr, "pth_type_next %u\n", pktp_hdr->pth_type_next);
	fprintf(stderr, "pth_dlt %u\n", pktp_hdr->pth_dlt);
	fprintf(stderr, "pth_ifname %s pth_iftype %d\n", pktp_hdr->pth_ifname, pktp_hdr->pth_iftype);
	fprintf(stderr, "pth_flags 0x%x\n", pktp_hdr->pth_flags);
	fprintf(stderr, "pth_protocol_family %u\n", pktp_hdr->pth_protocol_family);
	fprintf(stderr, "pth_frame_pre_length %u pth_frame_post_length %u\n",
		     pktp_hdr->pth_frame_pre_length, pktp_hdr->pth_frame_post_length);
	fprintf(stderr, "pth_svc %u\n", pktp_hdr->pth_svc);
	fprintf(stderr, "pth_flowid %u\n", pktp_hdr->pth_flowid);
	fprintf(stderr, "pth_ipproto %u\n", pktp_hdr->pth_ipproto);
	fprintf(stderr, "pth_pid %u pth_epid %u\n", pktp_hdr->pth_pid, pktp_hdr->pth_epid);
	fprintf(stderr, "pth_comm %s pth_ecomm %s\n", pktp_hdr->pth_comm, pktp_hdr->pth_ecomm);
	uuid_unparse(pktp_hdr->pth_uuid, uuidstr);
	uuid_unparse(pktp_hdr->pth_euuid, euuidstr);
	fprintf(stderr, "pth_uuid %s pth_euuid %s\n",
		     uuid_is_null(pktp_hdr->pth_uuid) ? "" : uuidstr,
		     uuid_is_null(pktp_hdr->pth_euuid) ? "" : euuidstr);
}

static void
print_pktap_v2_hdr(const struct pktap_v2_hdr *pktp_hdr)
{
	uuid_string_t uuidstr, euuidstr;
	char *ptr = (char *)pktp_hdr;
	
	fprintf(stderr, "pth2_length %u (sizeof(struct pktap_v2_hdr) %lu)\n",
		pktp_hdr->pth_length, sizeof(struct pktap_v2_hdr));
	fprintf(stderr, "pth2_dlt %u\n", pktp_hdr->pth_dlt);
	fprintf(stderr, "pth2_ifname %s pth2_iftype %d\n",
		pktp_hdr->pth_ifname_offset != 0 ? ptr + pktp_hdr->pth_ifname_offset : "",
		pktp_hdr->pth_iftype);
	fprintf(stderr, "pth2_flags 0x%x\n", pktp_hdr->pth_flags);
	fprintf(stderr, "pth2_protocol_family %u\n", pktp_hdr->pth_protocol_family);
	fprintf(stderr, "pth2_frame_pre_length %u pth2_frame_post_length %u\n",
		pktp_hdr->pth_frame_pre_length, pktp_hdr->pth_frame_post_length);
	fprintf(stderr, "pth2_svc %u\n", pktp_hdr->pth_svc);
	fprintf(stderr, "pth2_flowid %u\n", pktp_hdr->pth_flowid);
	fprintf(stderr, "pth2_ipproto %u\n", pktp_hdr->pth_ipproto);
	fprintf(stderr, "pth2_pid %u pth2_e_pid %u\n", pktp_hdr->pth_pid, pktp_hdr->pth_e_pid);

	fprintf(stderr, "pth2_comm %s pth2_ecomm %s\n",
		pktp_hdr->pth_comm_offset != 0 ? ptr + pktp_hdr->pth_comm_offset : "",
		pktp_hdr->pth_e_comm_offset != 0 ? ptr + pktp_hdr->pth_e_comm_offset : "");
	if (pktp_hdr->pth_uuid_offset != 0)
		uuid_unparse(*(const uuid_t *)(ptr + pktp_hdr->pth_uuid_offset), uuidstr);
	if (pktp_hdr->pth_e_uuid_offset != 0)
		uuid_unparse(*(const uuid_t *)(ptr + pktp_hdr->pth_e_uuid_offset), euuidstr);
	fprintf(stderr, "pth2_uuid %s pth2_euuid %s\n",
		pktp_hdr->pth_uuid_offset == 0 ? "" : uuidstr,
		pktp_hdr->pth_e_uuid_offset == 0 ? "" : euuidstr);
}
static size_t
convert_pktap_header_to_v2(const struct pktap_header *pktp_hdr, struct pktap_v2_hdr_space *pktap_v2_hdr_space)
{
	struct pktap_v2_hdr *pktap_v2_hdr;

	pktap_v2_hdr = &pktap_v2_hdr_space->pth_hdr;

	COPY_PKTAP_COMMON_FIELDS_TO_V2(pktap_v2_hdr, pktp_hdr);

	if (!uuid_is_null(pktp_hdr->pth_uuid)) {
		size_t len = sizeof(uuid_t);
		uint8_t *ptr;

		pktap_v2_hdr->pth_uuid_offset = pktap_v2_hdr->pth_length;
		ptr = ((uint8_t *)pktap_v2_hdr) + pktap_v2_hdr->pth_uuid_offset;
		uuid_copy(*(uuid_t *)ptr, pktp_hdr->pth_uuid);

		pktap_v2_hdr->pth_length += len;
		assert(pktap_v2_hdr->pth_length < sizeof(struct pktap_v2_hdr_space));
	}

	if (!uuid_is_null(pktp_hdr->pth_euuid)) {
		size_t len = sizeof(uuid_t);
		uint8_t *ptr;

		pktap_v2_hdr->pth_e_uuid_offset = pktap_v2_hdr->pth_length;
		ptr = ((uint8_t *)pktap_v2_hdr) + pktap_v2_hdr->pth_e_uuid_offset;
		uuid_copy(*(uuid_t *)ptr, pktp_hdr->pth_euuid);

		pktap_v2_hdr->pth_length += len;
		assert(pktap_v2_hdr->pth_length < sizeof(struct pktap_v2_hdr_space));
	}

	if (strlen(pktp_hdr->pth_ifname) > 0) {
		size_t len;
		uint8_t *ptr;

		pktap_v2_hdr->pth_ifname_offset = pktap_v2_hdr->pth_length;
		ptr = ((uint8_t *)pktap_v2_hdr) + pktap_v2_hdr->pth_ifname_offset;
		len = 1 + strlcpy((char *)ptr, pktp_hdr->pth_ifname, sizeof(pktap_v2_hdr_space->pth_ifname));

		pktap_v2_hdr->pth_length += len;
		assert(pktap_v2_hdr->pth_length < sizeof(struct pktap_v2_hdr_space));
	}

	if (strlen(pktp_hdr->pth_comm) > 0) {
		size_t len;
		uint8_t *ptr;

		pktap_v2_hdr->pth_comm_offset = pktap_v2_hdr->pth_length;
		ptr = ((uint8_t *)pktap_v2_hdr) + pktap_v2_hdr->pth_comm_offset;
		len = 1 + strlcpy((char *)ptr, pktp_hdr->pth_comm, sizeof(pktap_v2_hdr_space->pth_comm));

		pktap_v2_hdr->pth_length += len;
		assert(pktap_v2_hdr->pth_length < sizeof(struct pktap_v2_hdr_space));
	}

	if (strlen(pktp_hdr->pth_ecomm) > 0) {
		size_t len;
		uint8_t *ptr;

		pktap_v2_hdr->pth_e_comm_offset = pktap_v2_hdr->pth_length;
		ptr = ((uint8_t *)pktap_v2_hdr) + pktap_v2_hdr->pth_e_comm_offset;
		len = 1 + strlcpy((char *)ptr, pktp_hdr->pth_ecomm, sizeof(pktap_v2_hdr_space->pth_e_comm));

		pktap_v2_hdr->pth_length += len;
		assert(pktap_v2_hdr->pth_length < sizeof(struct pktap_v2_hdr_space));
	}

	return (pktap_v2_hdr->pth_length);
}

static size_t
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

	if (pktap_v2_hdr_src->pth_uuid_offset != 0)
		uuid_copy(pktap_header_dst->pth_uuid, (ptr + pktap_v2_hdr_src->pth_uuid_offset));

	if (pktap_v2_hdr_src->pth_e_uuid_offset != 0)
		uuid_copy(pktap_header_dst->pth_euuid, (ptr + pktap_v2_hdr_src->pth_e_uuid_offset));

	if (pktap_v2_hdr_src->pth_ifname_offset != 0)
		strlcpy(pktap_header_dst->pth_ifname, (char *)(ptr + pktap_v2_hdr_src->pth_ifname_offset), sizeof(pktap_header_dst->pth_ifname));

	if (pktap_v2_hdr_src->pth_comm_offset != 0)
		strlcpy(pktap_header_dst->pth_comm, (char *)(ptr + pktap_v2_hdr_src->pth_comm_offset), sizeof(pktap_header_dst->pth_comm));

	if (pktap_v2_hdr_src->pth_e_comm_offset != 0)
		strlcpy(pktap_header_dst->pth_ecomm, (char *)(ptr + pktap_v2_hdr_src->pth_e_comm_offset), sizeof(pktap_header_dst->pth_ecomm));

	return (sizeof(struct pktap_header));
}

static bool
save_pktap_v1(struct dump_info *dump_info, const struct pcap_pkthdr *h, const u_char *bytes)
{
	int status;
	struct pcap_pkthdr pcap_pkthdr;

	memcpy(&pcap_pkthdr, h, sizeof(struct pcap_pkthdr));

	if (snap_len > 0 && pcap_pkthdr.caplen > snap_len) {
		pcap_pkthdr.caplen = snap_len;
	}
	if (((struct pktap_v2_hdr *)bytes)->pth_flags & PTH_FLAG_V2_HDR) {
		const struct pktap_v2_hdr *pktap_v2_hdr = (struct pktap_v2_hdr *)bytes;
		struct pktap_header pktap_header_space;
		struct pktap_header pktap_header_space_bis;
		size_t v1_hdr_len;
		struct pktap_v2_hdr_space pktap_v2_hdr_space;
		size_t v2_hdr_len;

		memset(&pktap_header_space, 0, sizeof(struct pktap_header));
		memset(&pktap_header_space_bis, 0, sizeof(struct pktap_header));
		memset(&pktap_v2_hdr_space, 0, sizeof(struct pktap_v2_hdr_space));

		
		v1_hdr_len = convert_v2_to_pktap_header(pktap_v2_hdr, &pktap_header_space);
		assert(v1_hdr_len == sizeof(struct pktap_header));

		if (verbosity > 1) {
			print_pktap_v2_hdr(pktap_v2_hdr);
			print_pktap_header(&pktap_header_space);
		}
		v2_hdr_len = convert_pktap_header_to_v2(&pktap_header_space, &pktap_v2_hdr_space);

		/*
		 * The conversion is not symetrical because the strings in the pktap_v2_hdr
		 * are of variable length and null uuid and empty string are omitted
		 * Note that BPF may pass empty string and null uuid
		 * We can compare the conversion to pktap_header
		 */
		(void) convert_v2_to_pktap_header(&pktap_v2_hdr_space.pth_hdr, &pktap_header_space_bis);
		assert(memcmp(&pktap_header_space, &pktap_header_space_bis, v1_hdr_len) == 0);

		assert(pcap_pkthdr.caplen + sizeof(struct pktap_header) - pktap_v2_hdr->pth_length <= sizeof(temp_buffer));

		memcpy(temp_buffer, &pktap_header_space, sizeof(struct pktap_header));
		memcpy(temp_buffer + sizeof(struct pktap_header), bytes + pktap_v2_hdr->pth_length,
		       pcap_pkthdr.caplen - pktap_v2_hdr->pth_length);

		pcap_pkthdr.caplen += sizeof(struct pktap_header) - pktap_v2_hdr->pth_length;
		pcap_pkthdr.len += sizeof(struct pktap_header) - pktap_v2_hdr->pth_length;

		status = pcap_ng_dump_pktap(dump_info->p_cap, dump_info->dumper_v1,
					    &pcap_pkthdr, temp_buffer);
		if (status == 0) {
			warnx("%s: pcap_ng_dump_pktap: %s\n",
			      __func__, pcap_geterr(dump_info->p_cap));
			return (false);
		}
	} else if (((struct pktap_header *)bytes)->pth_type_next == PTH_TYPE_PACKET) {
		status = pcap_ng_dump_pktap(dump_info->p_cap, dump_info->dumper_v1,
					    &pcap_pkthdr, bytes);
		if (status == 0) {
			warnx("%s: pcap_ng_dump_pktap: %s\n",
			      __func__, pcap_geterr(dump_info->p_cap));
			return (false);
		}
	} else {
		warnx("%s: unkwnown pktap_header type\n",
		      __func__);
		return (false);
	}

	dump_info->packet_count_v1++;
	dump_info->total_bpf_length_v1 += sizeof(struct bpf_hdr) + pcap_pkthdr.caplen;
	dump_info->total_pktap_header_space += sizeof(struct pktap_header);
	
	if (verbosity > 0) {
		fprintf(stderr, "%s: v1 packet count: %d total bpf length: %lu\n",
			__func__, dump_info->packet_count_v1, dump_info->total_bpf_length_v1);
	}

	return (true);
}

static void
zero_out_unused_pktap_header_fields_for_v2(struct pktap_header *pktp_hdr)
{
	pktp_hdr->pth_ifunit = 0;
	pktp_hdr->pth_tstamp.tv_sec = 0;
	pktp_hdr->pth_tstamp.tv_usec = 0;
}

static bool
save_pktap_v2(struct dump_info *dump_info, const struct pcap_pkthdr *h, const u_char *bytes)
{
	int status;
	struct pcap_pkthdr pcap_pkthdr;
	
	memcpy(&pcap_pkthdr, h, sizeof(struct pcap_pkthdr));
	
	if (snap_len > 0 && pcap_pkthdr.caplen > snap_len) {
		pcap_pkthdr.caplen = snap_len;
	}
	
	if (((struct pktap_v2_hdr *)bytes)->pth_flags & PTH_FLAG_V2_HDR) {
		const struct pktap_v2_hdr *pktap_v2_hdr = (struct pktap_v2_hdr *)bytes;
		struct pktap_header pktap_header_space;
		size_t v1_hdr_len;

		v1_hdr_len = convert_v2_to_pktap_header(pktap_v2_hdr, &pktap_header_space);
		assert(v1_hdr_len == sizeof(struct pktap_header));
		
		dump_info->total_pktap_v2_hdr_space += pktap_v2_hdr->pth_length;
		
		status = pcap_ng_dump_pktap_v2(dump_info->p_cap, dump_info->dumper_v2,
					       &pcap_pkthdr, bytes, NULL);
		if (status == 0) {
			warnx("%s: pcap_ng_dump_pktap_v2: %s\n",
			      __func__, pcap_geterr(dump_info->p_cap));
			pcap_breakloop(dump_info->p_cap);
		}
	} else if (((struct pktap_header *)bytes)->pth_type_next == PTH_TYPE_PACKET) {
		const struct pktap_header *pktp_hdr = (struct pktap_header *)bytes;
		struct pktap_header pktap_header_space;
		size_t v1_hdr_len;
		struct pktap_v2_hdr_space pktap_v2_hdr_space;
		size_t v2_hdr_len;
		size_t offset;
		u_char *new_bytes;
		struct pktap_header pktap_header_copy;
		
		memset(&pktap_header_space, 0, sizeof(struct pktap_header));
		memset(&pktap_header_copy, 0, sizeof(struct pktap_header));

		/*
		 * Zero out the fields in the pktap_header we do not care about
		 */
		memcpy(&pktap_header_copy, pktp_hdr, sizeof(struct pktap_header));
		zero_out_unused_pktap_header_fields_for_v2(&pktap_header_copy);
		
		v2_hdr_len = convert_pktap_header_to_v2(&pktap_header_copy, &pktap_v2_hdr_space);

		if (verbosity > 1) {
			print_pktap_header(&pktap_header_copy);
			print_pktap_v2_hdr(&pktap_v2_hdr_space.pth_hdr);
		}
		dump_info->total_pktap_v2_hdr_space += pktap_v2_hdr_space.pth_hdr.pth_length;

		v1_hdr_len = convert_v2_to_pktap_header(&pktap_v2_hdr_space.pth_hdr, &pktap_header_space);
		assert(v1_hdr_len == sizeof(struct pktap_header));
		assert(memcmp(&pktap_header_copy, &pktap_header_space, sizeof(struct pktap_header)) == 0);
		
		offset = sizeof(struct pktap_header) - v2_hdr_len;
		new_bytes = (u_char *)bytes + offset;
		memcpy(new_bytes, &pktap_v2_hdr_space, v2_hdr_len);
		
		pcap_pkthdr.caplen -= offset;
		pcap_pkthdr.len -= offset;
		status = pcap_ng_dump_pktap_v2(dump_info->p_cap, dump_info->dumper_v2,
					       &pcap_pkthdr, new_bytes, NULL);
		if (status == 0) {
			warnx("%s: pcap_ng_dump_pktap_v2: %s\n",
			      __func__, pcap_geterr(dump_info->p_cap));
			return (false);
		}
	} else {
		warnx("%s: unkwnown pktap header type\n",
		      __func__);
		return (false);
	}
	
	dump_info->packet_count_v2++;
	dump_info->total_bpf_length_v2 += sizeof(struct bpf_hdr) + pcap_pkthdr.caplen;
	
	if (verbosity > 0) {
		fprintf(stderr, "%s: v2 packet count: %d total bpf length: %lu\n",
			__func__, dump_info->packet_count_v2, dump_info->total_bpf_length_v2);
	}

	return (true);
}


static void
readcallback(u_char *user, const struct pcap_pkthdr *h, const u_char *bytes)
{
	struct dump_info *dump_info = (struct dump_info *) user;
	bool v1_done = false;
	bool v2_done = false;

	if (dump_pktap_v1) {
		if ((max_count != 0 && dump_info->packet_count_v1 > max_count) ||
		      (max_length != 0 && dump_info->total_bpf_length_v1 >= max_length)) {
			v1_done = true;
		} else if (!save_pktap_v1(dump_info, h, bytes)) {
			v1_done = true;
			v2_done = true;
		}
	} else {
		v1_done = true;
	}

	if (dump_pktap_v2) {
		if ((max_count != 0 && dump_info->packet_count_v2 > max_count) ||
		    (max_length != 0 && dump_info->total_bpf_length_v2 >= max_length)) {
			v2_done = true;
		} else if (!save_pktap_v2(dump_info, h, bytes)) {
			v2_done = true;
		}
	} else {
		v2_done = true;
	}

	if (capture_done || (v1_done && v2_done)) {
		if (verbosity > 0) {
			fprintf(stderr, "%s: done\n",
				__func__);
		}
		pcap_breakloop(dump_info->p_cap);
	}
}

static void
usage(void)
{
#define OPTION_FMT " %-20s %s\n"
	printf("# usage: %s [-c 1|2] [-d 1|-1|2|-2] [-f filename] [-h] [-i ifname] [-l max_length] [-n max_count] [-s snap_len] [-t 0|1] [-v]\n",
	       getprogname());
	printf(OPTION_FMT, "-c 1|2", "capture version 1 'struct pktap_header' or version 2 'struct pktap_hdr_v2' (v1 by default)");
	printf(OPTION_FMT, "-d 1|-1|2|-2", "dump using version 1 pcap_ng_dump_pktap() and/or version 2 pcap_ng_dump_pktap_v2() (v1 by default)");
	printf(OPTION_FMT, "-f filename", "file name for dump file (to be prefixed with 'v1_' or 'v2'");
	printf(OPTION_FMT, "-h", "display this help");
	printf(OPTION_FMT, "-i ifname", "capture interface name (pktap by default)");
	printf(OPTION_FMT, "-l max_length", "maximum length of packet captured (unlimited by default");
	printf(OPTION_FMT, "-n max_count", "maximum number of packet to capture (unlimited by default");
	printf(OPTION_FMT, "-s snap_len", "snap len (unlimited by default)");
	printf(OPTION_FMT, "-t 0|1", "truncation mode (off by default)");
	printf(OPTION_FMT, "-v", "increase versbosity");
}

void
parse_command_line(int argc, char * const argv[])
{
	int ch;

	if (argc == 1) {
		usage();
		exit(EX_OK);
	}
	while ((ch = getopt(argc, argv, "c:d:f:hi:l:n:qs:t:vw:")) != -1) {
		switch (ch) {
			case 'c':
				if (strcmp(optarg, "1") == 0) {
					cap_pktap_v2 = false;
				} else if (strcmp(optarg, "2") == 0) {
					cap_pktap_v2 = true;
				} else {
					warnx("bad parameter '%s' for option '-%c'", optarg, ch);
					usage();
					exit(EX_USAGE);
				}
				break;
				
			case 'd':
				if (strcmp(optarg, "1") == 0) {
					dump_pktap_v1 = true;
				} else if (strcmp(optarg, "-1") == 0) {
					dump_pktap_v1 = false;
				} else if (strcmp(optarg, "2") == 0) {
					dump_pktap_v2 = true;
				} else if (strcmp(optarg, "-2") == 0) {
					dump_pktap_v2 = false;
				} else {
					warnx("bad parameter '%s' for option '-%c'", optarg, ch);
					usage();
					exit(EX_USAGE);
				}
				break;
				
			case 'f':
				filename = optarg;
				break;
				
			case 'h':
				usage();
				exit(EX_OK);
				
			case 'i':
				ifname = optarg;
				break;
				
			case 'l':
				max_length = strtoul(optarg, NULL, 0);
				break;
				
			case 'n':
				max_count = atoi(optarg);
				break;

			case 'q':
				verbosity--;
				break;

			case 's':
				snap_len = (uint32_t) strtoul(optarg, NULL, 0);
				break;
				
			case 't':
				truncation = !!atoi(optarg);
				break;
				
			case 'v':
				verbosity++;
				break;
				
			case 'w':
				want_pktap = !!atoi(optarg);
				break;

			default:
				usage();
				exit(EX_USAGE);
		}
	}
}

void
signal_handler(int sig)
{
	capture_done = true;
}

int
main(int argc, char * const argv[]) {
	char ebuf[PCAP_ERRBUF_SIZE];
	int status;
	struct dump_info dump_info = {};

	parse_command_line(argc, argv);
	
	dump_info.ifname_v1 = pcap_setup_pktap_interface(ifname, ebuf);
	if (dump_info.ifname_v1 == NULL) {
		errx(EX_OSERR, "pcap_setup_pktap_interface(%s) fail - %s",
		     ifname, ebuf);
	}
	dump_info.ifname_v2 = pcap_setup_pktap_interface(ifname, ebuf);
	if (dump_info.ifname_v2 == NULL) {
		errx(EX_OSERR, "pcap_setup_pktap_interface(pktap) fail - %s",
		     ebuf);
	}

	dump_info.p_cap = pcap_create(ifname, ebuf);
	if (dump_info.p_cap == NULL) {
		errx(EX_OSERR, "pcap_create(%s) fail - %s",
		     ifname, ebuf);
	}
	status = pcap_set_timeout(dump_info.p_cap, 1000);
	if (status != 0) {
		errx(EX_OSERR, "pcap_create(%s) fail - %s",
		     ifname, pcap_statustostr(status));
	}

	/*
	 * Must be called before pcap_activate()
	 */
	pcap_set_want_pktap(dump_info.p_cap, want_pktap);
	pcap_set_pktap_hdr_v2(dump_info.p_cap, cap_pktap_v2);
	pcap_set_truncation_mode(dump_info.p_cap, truncation);
	if (snap_len > 0)
		pcap_set_snaplen(dump_info.p_cap, snap_len);
	
	status = pcap_activate(dump_info.p_cap);
	if (status < 0) {
		if (status == PCAP_ERROR) {
			errx(EX_OSERR, "pcap_activate(%s) fail - %s",
			     ifname, pcap_geterr(dump_info.p_cap));
		} else {
			errx(EX_OSERR, "pcap_activate(%s) fail - %s %s",
			     ifname, pcap_statustostr(status), pcap_geterr(dump_info.p_cap));
		}
	}

	if (verbosity) {
		int dlt = pcap_datalink(dump_info.p_cap);
		const char *dltname = pcap_datalink_val_to_name(dlt);
		int n_dlts, i;
		int *dlts = 0;

		fprintf(stderr, "wantpktap %d\n", want_pktap);

		fprintf(stderr, "pcap_datalink: %d name: %s\n", dlt, dltname);

		n_dlts = pcap_list_datalinks(dump_info.p_cap, &dlts);

		for (i = 0; i < n_dlts; i++) {
			dltname = pcap_datalink_val_to_name(dlts[i]);
			fprintf(stderr, "pcap_list_datalinks[%d]: %d name: %s\n", i, dlts[i], dltname);
		}
		pcap_free_datalinks(dlts);
	}

	if (dump_pktap_v1) {
		dump_info.filename_v1 = malloc(PATH_MAX);
		snprintf(dump_info.filename_v1, PATH_MAX, "v1_%s", filename);
		dump_info.dumper_v1 = pcap_ng_dump_open(dump_info.p_cap, dump_info.filename_v1);
		if (dump_info.dumper_v1 == NULL) {
			errx(EX_OSERR, "pcap_ng_dump_open(%s) fail - %s",
			     dump_info.filename_v1, pcap_geterr(dump_info.p_cap));
		}
	}

	if (dump_pktap_v2) {
		dump_info.filename_v2 = malloc(PATH_MAX);
		snprintf(dump_info.filename_v2, PATH_MAX, "v2_%s", filename);
		dump_info.dumper_v2 = pcap_ng_dump_open(dump_info.p_cap, dump_info.filename_v2);
		if (dump_info.dumper_v2 == NULL) {
			errx(EX_OSERR, "pcap_ng_dump_open(%s) fail - %s",
			     dump_info.filename_v2, pcap_geterr(dump_info.p_cap));
		}
	}

	/* to stop the capture */
	signal(SIGINT, signal_handler);

	while (true) {
		status = pcap_dispatch(dump_info.p_cap, -1, readcallback, (u_char *) &dump_info);
		if (status == -1) {
			errx(EX_OSERR, "pcap_dispatch(%s) fail - %s",
			     ifname, pcap_geterr(dump_info.p_cap));
		} else if (status == -2) {
			/* pcap_breakloop() called */
			break;
		}
	}

	if (dump_pktap_v1) {
		pcap_dump_close(dump_info.dumper_v1);
		fprintf(stderr, "v1 file: %s  packet count: %d total bpf length: %lu pktap header space %lu\n",
			dump_info.filename_v1, dump_info.packet_count_v1,
			dump_info.total_bpf_length_v1, dump_info.total_pktap_header_space);
	}
	if (dump_pktap_v2) {
		pcap_dump_close(dump_info.dumper_v2);
		fprintf(stderr, "v2 file: %s  packet count: %d total bpf length: %lu pktap v2 hdr space %lu\n",
			dump_info.filename_v2, dump_info.packet_count_v2,
			dump_info.total_bpf_length_v2, dump_info.total_pktap_v2_hdr_space);
	}
	pcap_close(dump_info.p_cap);

	exit(EX_OK);
}
