/*
 * Copyright (c) 2012-2013 Apple Inc. All rights reserved.
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

#ifndef libpcap_pcap_ng_h
#define libpcap_pcap_ng_h

#include <pcap/pcap.h>

#ifdef PRIVATE

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A useful and efficient macro to round up a number to a multiple of 4
 */
#define PCAPNG_ROUNDUP32(x) (((x) + 3) & ~3)

/*
 * Pcapng blocks share a similar format:
 * - a block header composed of the block type and the block length
 * - a set of fixed fields specific to the block type
 * - for some block types a list of records
 * - a list of option
 * - a block trailer that repeats the block length
 */

/*
 * Common part at the beginning of all blocks.
 */
struct pcapng_block_header {
	bpf_u_int32	block_type;
	bpf_u_int32	total_length;
};

/*
 * Common trailer at the end of all blocks.
 */
struct pcapng_block_trailer {
	bpf_u_int32	total_length;
};

/*
 * Option header.
 */
struct pcapng_option_header {
	u_short		option_code;
	u_short		option_length;  /* Actual length of option value, not rounded up */
	/* Followed by option value that is aligned to 32 bits */
};

/*
 * Common options that may appear within most types of blocks
 */
#define PCAPNG_OPT_ENDOFOPT	0	/* Zero length option to mark the end of list of options */
#define PCAPNG_OPT_COMMENT	1	/* UTF-8 string */

/*
 * The pcap_ng_xxx_fields structures describe for each type of block
 * the part of the block body that follows the common block header.
 *
 * When using the raw block APIs and format, numbers are in the byte order of the host
 * that created the blokck.
 *
 * When using the high level API, numbers are in the local host byte order.
 *
 * Note that addresses and port are always in network byte order.
 */

/*
 * Section Header Block.
 */
#define PCAPNG_BT_SHB			0x0A0D0D0A

struct pcapng_section_header_fields {
	bpf_u_int32	byte_order_magic;
	u_short		major_version;
	u_short		minor_version;
	u_int64_t	section_length;     /* 0xFFFFFFFFFFFFFFFF means length not specified */
	/* followed by options and trailer */
};

/*
 * Byte-order magic value.
 */
#define PCAPNG_BYTE_ORDER_MAGIC	0x1A2B3C4D

/*
 * Current version number.
 * If major_version isn't PCAPNG_VERSION_MAJOR,
 * that means that this code can't read the file.
 */
#define PCAPNG_VERSION_MAJOR	1
#define PCAPNG_VERSION_MINOR	0

/*
 * Option codes for Section Header Block
 */
#define PCAPNG_SHB_HARDWARE     0x00000002	/* UTF-8 string */
#define PCAPNG_SHB_OS           0x00000003	/* UTF-8 string */
#define PCAPNG_SHB_USERAPPL     0x00000004	/* UTF-8 string */

/*
 * Interface Description Block.
 *
 * Integer values are in the local host byte order
 */
#define PCAPNG_BT_IDB			0x00000001

struct pcapng_interface_description_fields {
	u_short		linktype;
	u_short		reserved;
	bpf_u_int32	snaplen;
	/* followed by options and trailer */
};

/*
 * Options in the IDB.
 */
#define PCAPNG_IF_NAME			2	/* UTF-8 string with the interface name  */
#define PCAPNG_IF_DESCRIPTION	3	/* UTF-8 string with the interface description */
#define PCAPNG_IF_IPV4ADDR		4	/* 8 bytes long IPv4 address and netmask (may be repeated) */
#define PCAPNG_IF_IPV6ADDR		5	/* 17 bytes long IPv6 address and prefix length (may be repeated) */
#define PCAPNG_IF_MACADDR		6	/* 6 bytes long interface's MAC address */
#define PCAPNG_IF_EUIADDR		7	/* 8 bytes long interface's EUI address */
#define PCAPNG_IF_SPEED			8	/* 64 bits number for the interface's speed, in bits/s */
#define PCAPNG_IF_TSRESOL		9	/* 8 bits number with interface's time stamp resolution */
#define PCAPNG_IF_TZONE			10	/* interface's time zone */
#define PCAPNG_IF_FILTER		11	/* variable length filter used when capturing on interface */
#define PCAPNG_IF_OS			12	/* UTF-8 string with the OS on which the interface was installed */
#define PCAPNG_IF_FCSLEN		13	/* 8 bits number with the FCS length for this interface */
#define PCAPNG_IF_TSOFFSET		14	/* 64 bits number offset to add to get absolute time stamps  */

/*
 * The following options are experimental Apple additions
 */
#define PCAPNG_IF_E_IF_ID		0x8001 /* Interface index of the effective interface */
	
/*
 * Packet Block.
 *
 * This block type is obsolete and should not be used to create new capture files.
 * Use instead Simple Packet Block or Enhanced Packet Block.
 */
#define PCAPNG_BT_PB			0x00000002

struct pcapng_packet_fields {
	u_short		interface_id;
	u_short		drops_count;
	bpf_u_int32	timestamp_high;
	bpf_u_int32	timestamp_low;
	bpf_u_int32	caplen;
	bpf_u_int32	len;
	/* followed by packet data, options, and trailer */
};

#define PCAPNG_PACK_FLAGS       2	/* 32 bits flags word containing link-layer information */
#define PCAPNG_PACK_HASH        3	/* Variable length */

/*
 * Simple Packet Block.
 */
#define PCAPNG_BT_SPB			0x00000003

struct pcapng_simple_packet_fields {
	bpf_u_int32	len;
	/* followed by packet data and trailer */
};

/*
 * Name Resolution Block
 *
 * Block body has no fields and is made of list of name records followed by options
 */
#define PCAPNG_BT_NRB               0x00000004  /* Name Resolution Block */

/*
 * Record header
 * Looks very much like an option header
 */
struct pcapng_record_header {
	u_short		record_type;
	u_short		record_length;  /* Actual length of record value, not rounded up */
	/* Followed by record value that is aligned to 32 bits */
};

/*
 * Name Resolution Record
 */
#define PCAPNG_NRES_ENDOFRECORD     0	/* Zero length record to mark end of list of records */
#define PCAPNG_NRES_IP4RECORD       1	/* Variable: 4 bytes IPv4 address followed by zero-terminated strings */
#define PCAPNG_NRES_IP6RECORD       2	/* Variable: 16 bytes IPv6 address followed by zero-terminated strings */

/*
 * Options for Name Resolution Block
 */
#define PCAPNG_NS_DNSNAME       2	/* UTF-8 string with the name of the DNS server */
#define PCAPNG_NS_DNSIP4ADDR    3	/* 4 bytes IPv4 address of the DNS server */
#define PCAPNG_NS_DNSIP6ADDR    4	/* 16 bytes IPv6 address of the DNS server */


/*
 * Interface Statistics Block
 */
#define PCAPNG_BT_ISB               0x00000005

struct pcapng_interface_statistics_fields {
	u_short			interface_id;
	bpf_u_int32		timestamp_high;
	bpf_u_int32		timestamp_low;
};

/*
 * Options for Interface Statistics Block
 */
#define PCAPNG_ISB_STARTTIIME   2	/* 64 bits timestamp in same format as timestamp in packets */
#define PCAPNG_ISB_ENDTIME      3	/* 64 bits timestamp in same format as timestamp in packets */
#define PCAPNG_ISB_IFRECV       4	/* 64 bits number of packet received during capture */
#define PCAPNG_ISB_IFDROP       5	/* 64 bits number of packet dropped due to lack of resources */
#define PCAPNG_ISB_FILTERACCEPT 6	/* 64 bits number of packet accepted by filter */
#define PCAPNG_ISB_OSDROP       7	/* 64 bits number of packet dropped by OS */
#define PCAPNG_ISB_USRDELIV     8	/* 64 bits number of packets delivered to the user */

/*
 * Enhanced Packet Block.
 */
#define PCAPNG_BT_EPB			0x00000006

struct pcapng_enhanced_packet_fields {
	bpf_u_int32	interface_id;
	bpf_u_int32	timestamp_high;
	bpf_u_int32	timestamp_low;
	bpf_u_int32	caplen;
	bpf_u_int32	len;
	/* followed by packet data, options, and trailer */
};

#define PCAPNG_EPB_FLAGS        2	/* 32 bits flags word containing link-layer information */
#define PCAPNG_EPB_HASH         3	/* Variable length */
#define PCAPNG_EPB_DROPCOUNT    4	/* 64 bits number of packets lost between this packet and the preceding one */

/*
 * The following options are experimental Apple additions
 */
#define PCAPNG_EPB_PIB_INDEX	0x8001	/* 32 bits number of process information block within the section */
#define PCAPNG_EPB_SVC			0x8002	/* 32 bits number with type of service code */
#define PCAPNG_EPB_E_PIB_INDEX	0x8003	/* 32 bits number of the effective process information block */

/*
 * Process Information Block
 *
 * NOTE: Experimental, this block type is not standardized
 */
#define PCAPNG_BT_PIB			0x80000001

struct pcapng_process_information_fields {
	bpf_u_int32	process_id;				/* As reported by OS, may wrap */
	/* followed by options and trailer */
};

#define PCAPNG_PIB_NAME			2	/* UTF-8 string with name of process */
#define PCAPNG_PIB_PATH			3	/* UTF-8 string with path of process */

/*
 * To open for reading a file in pcap-ng file format
 */
pcap_t *pcap_ng_fopen_offline(FILE *, char *);
pcap_t *pcap_ng_open_offline(const char *, char *);

/* 
 * Open for writing a capture file -- a "savefile" in pcap-ng file format
 */
pcap_dumper_t *pcap_ng_dump_open(pcap_t *, const char *);
pcap_dumper_t *pcap_ng_dump_fopen(pcap_t *, FILE *);

/*
 * Close a "savefile" being written to
 */
void pcap_ng_dump_close(pcap_dumper_t *);

/*
 * Write a packet to of a save file
 * This assume the packet are all of the same link type
 * pcap_ng_dump() is obsolete
 */
void pcap_ng_dump(u_char *, const struct pcap_pkthdr *, const u_char *);

/*
 * Opaque type for internalized pcap-ng blocks
 * 
 * Internalized pcap-ng blocks provide a convenient way to 
 * read and write pcap-ng blocks by hidding most of the detail 
 * of the block format
 */
typedef struct pcapng_block * pcapng_block_t;

/*
 * Allocate an internalized pcap-ng block data structure.
 * This allocate a work buffer of the given size to 
 * hold raw data block content.
 * The size should be large enough to hold the largest 
 * expected block size.
 */
pcapng_block_t pcap_ng_block_alloc(size_t );
	
/*
 * To intialize or reuse a existing internalized pcap-ng block.
 * Re-using pcapng_block_t is more efficient than using  
 * pcap_ng_block_alloc() for each block. 
 */
int pcap_ng_block_reset(pcapng_block_t , bpf_u_int32 );

/*
 * Free the memory associated internalized pcap-ng block
 */
void pcap_ng_free_block(pcapng_block_t);
	
/*
 * Write a internalized pcap-ng block into a savefile
 */
bpf_u_int32 pcap_ng_dump_block(pcap_dumper_t *, pcapng_block_t);

/*
 * Write a internalized pcap-ng block into a memory buffer
 */
bpf_u_int32 pcap_ng_externalize_block(void *, size_t , pcapng_block_t );

/*
 * To allocate or initialize a raw block read from pcap-ng file
 */
pcapng_block_t pcap_ng_block_alloc_with_raw_block(pcap_t *, u_char *);
int pcap_ng_block_init_with_raw_block(pcapng_block_t block, pcap_t *p, u_char *);

/*
 * Essential accessors
 */
bpf_u_int32 pcap_ng_block_get_type(pcapng_block_t);
bpf_u_int32 pcap_ng_block_get_len(pcapng_block_t);
int pcap_ng_block_is_swapped(pcapng_block_t);

/*
 * Provide access to field of the block header in the native host byte order
 */
struct pcapng_section_header_fields *pcap_ng_get_section_header_fields(pcapng_block_t );
struct pcapng_interface_description_fields *pcap_ng_get_interface_description_fields(pcapng_block_t );
struct pcapng_enhanced_packet_fields *pcap_ng_get_enhanced_packet_fields(pcapng_block_t );
struct pcapng_simple_packet_fields *pcap_ng_get_simple_packet_fields(pcapng_block_t );
struct pcapng_packet_fields *pcap_ng_get_packet_fields(pcapng_block_t );
struct pcapng_process_information_fields *pcap_ng_get_process_information_fields(pcapng_block_t );

/*
 * Set the packet data to the passed buffer by copying into the internal block buffer
 */
bpf_u_int32 pcap_ng_block_packet_copy_data(pcapng_block_t , const void *, bpf_u_int32 );
	
/*
 * Set the packet data by referencing an external buffer.
 */
bpf_u_int32 pcap_ng_block_packet_set_data(pcapng_block_t block, const void *, bpf_u_int32 );
	
/*
 * Return the first byte of the packet data (if any, or NULL otherwise)
 */
void *pcap_ng_block_packet_get_data_ptr(pcapng_block_t);
	
/*
 * Returns the length of the packet data
 */
bpf_u_int32 pcap_ng_block_packet_get_data_len(pcapng_block_t);
	
/*
 * Returns zero if the block does not support packet data
 */
int pcap_ng_block_does_support_data(pcapng_block_t);

/*
 * Add a option with the given code and value
 */
int pcap_ng_block_add_option_with_value(pcapng_block_t , u_short ,
										const void *, u_short );
int	pcap_ng_block_add_option_with_string(pcapng_block_t , u_short , const char *);

/*
 * To access option of an internalized block
 * The fields code and length are in the natural host byte order
 * The value content may be byte swapped if the block was read from a savefile
 */
struct pcapng_option_info {
	u_short code;
	u_short length;
	void *value;
};

/*
 * Get an option of the give code
 * This should be used for otions that may appear at most once in a block as
 * this returns the first option with the given code
 * Returns zero their is no option with that code in the block
 */
int pcap_ng_block_get_option(pcapng_block_t block, u_short code, struct pcapng_option_info *option_info);
	
/*
 * To walk the list of options in a block.
 */
typedef void (*pcapng_option_iterator_func)(pcapng_block_t ,
											struct pcapng_option_info *,
											void *);
int pcnapng_block_iterate_options(pcapng_block_t block,
								  pcapng_option_iterator_func opt_iterator_func,
								  void *context);
int pcap_ng_block_iterate_options(pcapng_block_t block,
                                  pcapng_option_iterator_func opt_iterator_func,
                                  void *context);

/*
 * To access name records
 * The fields code and length are in the natural host byte order
 */

struct pcapng_name_record_info {
	u_short code;
	u_short length;
	void *value;
};
typedef void (*pcapng_name_record_iterator_func)(pcapng_block_t ,
												struct pcapng_name_record_info *,
												void * );

int pcnapng_block_iterate_name_records(pcapng_block_t ,
									   pcapng_name_record_iterator_func ,
									   void *);
int pcap_ng_block_iterate_name_records(pcapng_block_t ,
                                       pcapng_name_record_iterator_func ,
                                       void *);
struct in_addr;
struct in6_addr;

int pcap_ng_block_add_name_record_with_ip4(pcapng_block_t, struct in_addr *, const char **);
int pcap_ng_block_add_name_record_with_ip6(pcapng_block_t, struct in6_addr *, const char **);
	
/*
 * To map between DLT and Link Type
 */
int dlt_to_linktype(int );
int linktype_to_dlt(int );

#ifdef __cplusplus
}
#endif

#endif /* PRIVATE */

#endif /* libpcap_pcap_ng_h */
