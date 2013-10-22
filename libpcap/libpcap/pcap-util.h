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

#ifndef libpcap_pcap_util_h
#define libpcap_pcap_util_h

#ifdef PRIVATE

#include <pcap/pcap.h>

struct pcap_if_info {
	int if_id;
	char *if_name;
	u_short if_linktype;
	u_short if_snaplen;
	struct bpf_program if_filter_program;
	int if_block_added;
};
extern struct pcap_if_info * pcap_find_if_info_by_name(pcap_t *, const char *);
extern struct pcap_if_info * pcap_find_if_info_by_id(pcap_t *, int);
extern struct pcap_if_info * pcap_add_if_info(pcap_t *, const char *, int, int, int);
extern void pcap_free_if_info(pcap_t *, struct pcap_if_info *);
extern void pcap_clear_if_infos(pcap_t *);

int pcap_set_filter_info(pcap_t *, const char *, int, bpf_u_int32);

struct pcap_proc_info {
	uint32_t proc_index;
	uint32_t proc_pid;
	char *proc_name;
};
extern struct pcap_proc_info * pcap_find_proc_info(pcap_t *, uint32_t , const char *);
extern struct pcap_proc_info * pcap_find_proc_info_by_index(pcap_t *, uint32_t);
extern struct pcap_proc_info * pcap_add_proc_info(pcap_t *, uint32_t , const char *);
extern void pcap_free_proc_info(pcap_t *, struct pcap_proc_info *);
extern void pcap_clear_proc_infos(pcap_t *);

/*
 * To reset information that are specific to each section.
 * Should be called when adding a new section header block.
 */
extern void pcap_ng_init_section_info(pcap_t *);

extern char * pcap_setup_pktap_interface(const char *, char *);
extern void pcap_cleanup_pktap_interface(const char *);

extern int pcap_ng_dump_pktap(pcap_t *, pcap_dumper_t *, const struct pcap_pkthdr *, const u_char *);

#endif /* PRIVATE */

#endif
