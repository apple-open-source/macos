/*
 * Copyright (c) 2012-2023 Apple Inc. All rights reserved.
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

#include <stdbool.h>
#include <pcap/pcap.h>
#include <uuid/uuid.h>
#include <os/availability.h>

#ifndef __PCAPNG_BLOCK_T__
#define __PCAPNG_BLOCK_T__
typedef struct pcapng_block * pcapng_block_t;
#endif /* __PCAPNG_BLOCK_T__ */

#ifdef __cplusplus
extern "C" {
#endif

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
int pcap_ng_dump_shb(pcap_t *, pcap_dumper_t *);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
int pcap_ng_dump_shb_comment(pcap_t *, pcap_dumper_t *, const char *);

struct pcap_if_info {
	int if_id;
	int if_dump_id; /* may be different from if_id because of filtering */
	char *if_name;
	int if_linktype;
	int if_snaplen;
	struct bpf_program if_filter_program;
	int if_block_dumped;
};
SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern struct pcap_if_info * pcap_find_if_info_by_name(pcap_t *, const char *);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern struct pcap_if_info * pcap_find_if_info_by_id(pcap_t *, int);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern struct pcap_if_info * pcap_add_if_info(pcap_t *, const char *, int, int, int);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern void pcap_free_if_info(pcap_t *, struct pcap_if_info *);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern void pcap_clear_if_infos(pcap_t *);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern int pcap_set_filter_info(pcap_t *, const char *, int, bpf_u_int32);

struct pcap_proc_info {
	uint32_t proc_index;
	uint32_t proc_dump_index;  /* may be different from proc_index because of filtering */
	uint32_t proc_pid;
	char *proc_name;
	int proc_block_dumped;
	uuid_t proc_uuid;
};
SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern struct pcap_proc_info * pcap_find_proc_info(pcap_t *, uint32_t, const char *);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern struct pcap_proc_info * pcap_find_proc_info_uuid(pcap_t *, uint32_t, const char *, const uuid_t);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern struct pcap_proc_info * pcap_find_proc_info_by_index(pcap_t *, uint32_t);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern struct pcap_proc_info * pcap_add_proc_info(pcap_t *, uint32_t, const char *);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern struct pcap_proc_info * pcap_add_proc_info_uuid(pcap_t *, uint32_t, const char *, const uuid_t);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern void pcap_free_proc_info(pcap_t *, struct pcap_proc_info *);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern void pcap_clear_proc_infos(pcap_t *);


struct pcap_if_info_set {
	int if_info_count;
	struct pcap_if_info **if_infos;
	int if_dump_id;
};

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern void pcap_if_info_set_clear(struct pcap_if_info_set *if_info_set);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern struct pcap_if_info * pcap_if_info_set_find_by_name(struct pcap_if_info_set *if_info_set, const char *name);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern struct pcap_if_info * pcap_if_info_set_find_by_id(struct pcap_if_info_set *if_info_set, int if_id);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern void pcap_if_info_set_free(struct pcap_if_info_set *if_info_set, struct pcap_if_info *if_info);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern struct pcap_if_info * pcap_if_info_set_add(struct pcap_if_info_set *if_info_set, const char *name,
					   int if_id, int linktype, int snaplen,
					   const char *filter_str, char *errbuf);

struct pcap_proc_info_set {
	int proc_info_count;
	struct pcap_proc_info **proc_infos;
	int proc_dump_index;
	
};

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern void pcap_proc_info_set_clear(struct pcap_proc_info_set *proc_info_set);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern struct pcap_proc_info * pcap_proc_info_set_find(struct pcap_proc_info_set *proc_info_set,
						uint32_t pid, const char *name);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern struct pcap_proc_info * pcap_proc_info_set_find_uuid(struct pcap_proc_info_set *proc_info_set,
						     uint32_t pid, const char *name, const uuid_t uu);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern struct pcap_proc_info * pcap_proc_info_set_find_by_index(struct pcap_proc_info_set *proc_info_set,
							 uint32_t index);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern void pcap_proc_info_set_free(struct pcap_proc_info_set *proc_info_set,
			     struct pcap_proc_info *proc_info);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern struct pcap_proc_info * pcap_proc_info_set_add_uuid(struct pcap_proc_info_set *proc_info_set,
						    uint32_t pid, const char *name, const uuid_t uu, char *errbuf);

/*
 * To reset information that are specific to each section.
 * Should be called when adding a new section header block.
 */
SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern void pcap_ng_init_section_info(pcap_t *);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern char * pcap_setup_pktap_interface(const char *, char *);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern void pcap_cleanup_pktap_interface(const char *);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern int pcap_ng_dump_pktap(pcap_t *, pcap_dumper_t *, const struct pcap_pkthdr *, const u_char *);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern int pcap_ng_dump_pktap_comment(pcap_t *, pcap_dumper_t *, const struct pcap_pkthdr *, const u_char *, const char *);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern int pcap_ng_dump_pktap_v2(pcap_t *, pcap_dumper_t *, const struct pcap_pkthdr *, const u_char *, const char *);

struct kern_event_msg;
SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern int pcap_ng_dump_kern_event(pcap_t *, pcap_dumper_t *,
				   struct kern_event_msg *, struct timeval *);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern struct pcap_if_info *pcap_ng_dump_if_info(pcap_t *, pcap_dumper_t *, pcapng_block_t,
						 struct pcap_if_info *);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern struct pcap_proc_info *pcap_ng_dump_proc_info(pcap_t *, pcap_dumper_t *, pcapng_block_t,
						 struct pcap_proc_info *);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern void pcap_ng_dump_init_section_info(pcap_dumper_t *);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern void pcap_read_bpf_header(pcap_t *p, u_char *bp, struct pcap_pkthdr *pkthdr);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern int pcap_set_truncation_mode(pcap_t *p, bool on);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern int pcap_set_pktap_hdr_v2(pcap_t *p, bool on);

#define HAS_PCAP_SET_COMPRESSION 1
SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern int pcap_set_compression(pcap_t *, int);

#define HAS_PCAP_HEAD_DROP 1
SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
	extern int pcap_set_head_drop(pcap_t *, int);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
extern int pcap_get_head_drop(pcap_t *);

SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
extern int pcap_get_compression_stats(pcap_t *, void *, size_t);

/*
 * To access DLT_PKPTAP, pcap_set_want_pktap() must be called before pcap_activate()
 */
SPI_AVAILABLE(macos(10.8), ios(5.0), tvos(9.0), watchos(1.0), bridgeos(1.0))
extern int pcap_set_want_pktap(pcap_t *, int);

/*
 * Settings to be able to write more than the interface MTU
 */
#define HAS_PCAP_MAX_WRITE_SIZE 1

SPI_AVAILABLE(macos(14.1), ios(17.1), tvos(17.1), watchos(10.1), bridgeos(8.1))
extern int pcap_set_max_write_size(pcap_t *, u_int);

SPI_AVAILABLE(macos(14.1), ios(17.1), tvos(17.1), watchos(10.1), bridgeos(8.1))
extern int pcap_get_max_write_size(pcap_t *, u_int *);

/*
 * Settings to be able to write more than the interface MTU
 */
#define HAS_PCAP_SEND_MULTIPLE 1

struct pcap_pkt_hdr_priv {
	bpf_u_int32       pcap_priv_hdr_len;      /* length of this structure (allows for extension) */
	bpf_u_int32       pcap_priv_flags;        /* none currently defined, pass 0 */
	bpf_u_int32       pcap_priv_len;          /* length of this packet data in the iovecs */
	bpf_u_int32       pcap_priv_iov_count;    /* number of elements in pcap_priv_iov */
	struct iovec      *pcap_priv_iov_array;	/* scatter gather array of the data */
};

SPI_AVAILABLE(macos(14.1), ios(17.1), tvos(17.1), watchos(10.1), bridgeos(8.1))
extern int pcap_set_send_multiple(pcap_t *, int);

SPI_AVAILABLE(macos(14.1), ios(17.1), tvos(17.1), watchos(10.1), bridgeos(8.1))
extern int pcap_get_send_multiple(pcap_t *, int *);

/*
 * Returns the number of packet sent or PCAP_ERROR
 */
SPI_AVAILABLE(macos(14.1), ios(17.1), tvos(17.1), watchos(10.1), bridgeos(8.1))
extern int pcap_sendpacket_multiple(pcap_t *pcap, const u_int count, const struct pcap_pkt_hdr_priv *pcap_pkt_array);

#ifdef __cplusplus
}
#endif

#endif
