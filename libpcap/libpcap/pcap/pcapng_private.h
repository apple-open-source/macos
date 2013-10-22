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

#ifndef libpcapng_pcapng_private_h
#define libpcapng_pcapng_private_h

#include <sys/queue.h>

#include "pcap-ng.h"

struct pcapng_block {
	u_char		*pcapng_bufptr;
	size_t		pcapng_buflen;
	int			pcapng_buf_is_external;
	
    uint32_t    pcapng_block_type;
    size_t		pcapng_block_len;
	int			pcapng_block_swapped;
		
	size_t		pcapng_fields_len;
	
	u_char		*pcapng_data_ptr;
	size_t		pcapng_data_len;
	u_int32_t	pcapng_cap_len;
	int			pcapng_data_is_external;
	
	size_t		pcapng_records_len;
	size_t		pcapng_options_len;
	
	union {
		struct pcapng_section_header_fields			_section_header;
		struct pcapng_interface_description_fields	_interface_description;
		struct pcapng_packet_fields					_packet;
		struct pcapng_simple_packet_fields			_simple_packet;
		struct pcapng_interface_statistics_fields	_interface_statistics;
		struct pcapng_enhanced_packet_fields		_enhanced_packet;
		struct pcapng_process_information_fields	_process_information;
	} block_fields_;
};

#define pcap_ng_shb_fields		block_fields_._section_header
#define pcap_ng_idb_fields		block_fields_._interface_description
#define pcap_ng_opb_fields		block_fields_._packet
#define pcap_ng_spb_fields		block_fields_._simple_packet
#define pcap_ng_isb_fields		block_fields_._interface_statistics
#define pcap_ng_epb_fields		block_fields_._enhanced_packet
#define pcap_ng_pib_fields		block_fields_._process_information

/* Representation of on file data structure items */
#define PCAPNG_BYTE_ORDER_MAGIC	0x1A2B3C4D
#define PCAPNG_MAJOR_VERSION	1
#define PCAPNG_MINOR_VERSION	0

#endif
