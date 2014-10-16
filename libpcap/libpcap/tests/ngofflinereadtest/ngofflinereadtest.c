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


#include "pcap-ng.h"
#include <stdio.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>

struct section_info {
	struct section_info *next;
	struct pcapng_section_header_fields shb;
	pcap_t *pcap;
	u_int32_t if_count;
	struct interface_info *if_list;
};

struct interface_info {
	struct interface_info *next;
	struct pcapng_interface_description_fields idb;
	struct section_info *section_info;
	u_int32_t interface_id;
	char *if_name;
	char *if_desc;
};

struct section_info *section_list = NULL;
struct section_info *current_section = NULL;

int mode_raw = 0;
int mode_block = 0;

#define PAD32(x) (((x) + 3) & ~3)

void hex_and_ascii_print(const char *, const void *, size_t, const char *);

struct section_info *
new_section_info(pcap_t *pcap, struct pcapng_section_header_fields *shb)
{
	struct section_info *section_info = calloc(1, sizeof(struct section_info));
	
	if (section_info == NULL)
		return NULL;
	
	section_info->pcap = pcap;
	section_info->shb = *shb;
	
	if (section_list == NULL) {
		section_list = section_info;
	} else {
		section_info->next = section_list;
		section_list = section_info;
	}
	current_section = section_info;
	
	return section_info;
}


void
interface_option_iterator(pcapng_block_t block, struct pcapng_option_info *option_info, void *context)
{
	struct interface_info *interface_info = (struct interface_info *)context;
	
	switch (option_info->code) {
		case 0:
			break;
			
		case 1:
			break;
			
		case 2:
			interface_info->if_name = malloc(option_info->length + 1);
			if (interface_info->if_name == NULL)
				break;
			snprintf(interface_info->if_name, option_info->length + 1, "%s", option_info->value);
			break;
		case 3:
			interface_info->if_desc = malloc(option_info->length + 1);
			if (interface_info->if_desc == NULL)
				break;
			snprintf(interface_info->if_desc, option_info->length + 1, "%s", option_info->value);
			break;
		case 4:
			break;
		case 5:
			break;
		case 6:
			break;
		case 7:
			break;
		case 8:
			break;
		case 9:
			break;
		case 10:
			break;
		case 11:
			break;
		case 12:
			break;
		case 13:
			break;
		case 14:
			break;
		default:
			break;
	}
}

struct interface_info *
new_interface_info(struct section_info *section_info, pcapng_block_t block)
{
	struct interface_info *interface_info = calloc(1, sizeof(struct interface_info));
	
	if (interface_info == NULL)
		return NULL;
	
	interface_info->section_info = section_info;
	interface_info->interface_id = section_info->if_count;
	section_info->if_count++;
	if (section_info->if_list == NULL) {
		section_info->if_list = interface_info;
	} else {
		interface_info->next = section_info->if_list;
		section_info->if_list = interface_info;
	}
	(void) pcnapng_block_iterate_options(block,
										 interface_option_iterator,
										 interface_info);
	
	return interface_info;
}

struct interface_info *
find_interface_info_by_id(u_int16_t interface_id)
{
	struct interface_info *interface_info;
	
	if (current_section == NULL)
		return (NULL);
	
	if (interface_id + 1 > current_section->if_count)
		return (NULL);
	
	for (interface_info = current_section->if_list;
		 interface_info != NULL;
		 interface_info = interface_info->next) {
		if (interface_info->interface_id == interface_id)
			return (interface_info);
	}
	return (NULL);
}

void
block_option_iterator(pcapng_block_t block, struct pcapng_option_info *option_info, void *context)
{
	printf("    block_type %u context %p option_code %u value_len %u value_ptr %p\n",
		   pcap_ng_block_get_type(block), context,
		   option_info->code, option_info->length,
		   option_info->value
		   );
	switch (option_info->code) {
		case 0:
			printf("      opt_endofopt\n");
			break;
			
		case 1:
			printf("      opt_comment: %-*s\n",
				   option_info->length, option_info->value);
			break;
			
		default:
			/*
			 * Each block type has its own option code space
			 */
			switch (pcap_ng_block_get_type(block)) {
				case PCAPNG_BT_SHB:
					switch (option_info->code) {
						case 2:
							printf("      shb_hardware: %-*s\n",
								   option_info->length, option_info->value);
							break;
						case 3:
							printf("      shb_os: %-*s\n",
								   option_info->length, option_info->value);
							break;
						case 4:
							printf("      shb_userappl: %-*s\n",
								   option_info->length, option_info->value);
							break;
						default:
							printf("      <unkown shb option>\n");
							break;
					}
					break;
					
				case PCAPNG_BT_IDB:
					switch (option_info->code) {
						case 2:
							printf("      if_name: %-*s\n",
								   option_info->length, option_info->value);
							break;
						case 3:
							printf("      if_desc: %-*s\n",
								   option_info->length, option_info->value);
							break;
						case 4:
							printf("      if_IPv4addr\n");
							break;
						case 5:
							printf("      if_IPv6addr\n");
							break;
						case 6:
							printf("      if_MACaddr\n");
							break;
						case 7:
							printf("      if_EUIaddr\n");
							break;
						case 8:
							printf("      if_speed\n");
							break;
						case 9:
							printf("      if_tsresol\n");
							break;
						case 10:
							printf("      if_tzone\n");
							break;
						case 11:
							printf("      if_filter %-*s\n",
								   option_info->length, option_info->value);
							break;
						case 12:
							printf("      if_os %-*s\n",
								   option_info->length, option_info->value);
							break;
						case 13:
							printf("      if_fcslen\n");
							break;
						case 14:
							printf("      if_tsoffset\n");
							break;
						default:
							printf("      <unkown idb option>\n");
							break;
					}
					break;
					
				case PCAPNG_BT_EPB:
					switch (option_info->code) {
						case 2:
							printf("      epb_flags\n");
							break;
						case 3:
							printf("      epb_hash\n");
							break;
						case 4:
							printf("      epb_dropcount\n");
							break;
						case PCAPNG_EPB_PIB_INDEX:
							printf("      epb_pib\n");
							break;
						case PCAPNG_EPB_SVC:
							printf("      epb_svc\n");
							break;
						default:
							printf("      <unkown epb option>\n");
							break;
					}
					break;
					
				case PCAPNG_BT_SPB:
					printf("      <invalid spb option>\n");
					break;
					
				case PCAPNG_BT_PB:
					switch (option_info->code) {
						case 2:
							printf("      pack_flags\n");
							break;
						case 3:
							printf("      pack_hash\n");
							break;
						default:
							printf("      <unkown pb option>\n");
							break;
					}
					break;
					
				case PCAPNG_BT_PIB: {
					switch (option_info->code) {
						case 2:
							printf("      proc_name\n");
							break;
						case 3:
							printf("      proc_path\n");
							break;
						default:
							printf("      <unkown pib option>\n");
							break;
					}
					break;
				}
				case PCAPNG_BT_ISB: {
					break;
				}
				case PCAPNG_BT_NRB: {
					break;
				}
				default:
					break;
			}
			break;
	}
	if (option_info->value) {
		hex_and_ascii_print("      ", option_info->value, option_info->length, "\n");
	}
}

void
read_callback(u_char *user, const struct pcap_pkthdr *hdr, const u_char *bytes)
{
	pcap_t *pcap = (pcap_t *)user;
	struct pcapng_option_info option_info;
	u_char *optptr = NULL;
	
	/* Access the raw block */
	if (mode_raw) {
		struct pcapng_block_header *block_header = (struct pcapng_block_header*)bytes;
		
		printf("raw  hdr caplen %u len %u\n", hdr->caplen, hdr->len);
		
		printf("#\n# user %p hdr.caplen %u hdr.len %u block_header.blocktype 0x%x block_header.totallength %u\n",
			   user, hdr->caplen, hdr->len,
			   block_header->block_type, block_header->total_length);

		hex_and_ascii_print("", bytes, block_header->total_length, "\n");

		switch (block_header->block_type) {
			case PCAPNG_BT_SHB: {
				struct pcapng_section_header_fields *shb = (struct pcapng_section_header_fields *)(block_header + 1);
				printf("# Section Header Block\n");
				printf("  byte_order_magic 0x%x major_version %u minor_version %u section_length %llu\n",
					   shb->byte_order_magic, shb->major_version, shb->minor_version, shb->section_length);
				
				hex_and_ascii_print("", shb, sizeof(struct pcapng_section_header_fields), "\n");
								
				optptr = (u_char *)(shb + 1);
				
				break;
			}
			case PCAPNG_BT_IDB: {
				struct pcapng_interface_description_fields *idb = (struct pcapng_interface_description_fields *)(block_header + 1);
				printf("# Interface Description Block\n");
				printf("  linktype %u reserved %u snaplen %u\n",
					   idb->linktype, idb->reserved, idb->snaplen);
				
				hex_and_ascii_print("", idb, sizeof(struct pcapng_interface_description_fields), "\n");
				optptr = (u_char *)(idb + 1);

				break;
			}	
			case PCAPNG_BT_EPB: {
				struct pcapng_enhanced_packet_fields *epb = (struct pcapng_enhanced_packet_fields *)(block_header + 1);
				printf("# Enhanced Packet Block\n");
				printf("  interface_id %u timestamp_high %u timestamp_low %u caplen %u len %u\n",
					   epb->interface_id, epb->timestamp_high, epb->timestamp_low, epb->caplen, epb->len);
				
				hex_and_ascii_print("", epb, sizeof(struct pcapng_enhanced_packet_fields), "\n");
				hex_and_ascii_print("", epb + 1, epb->caplen, "\n");

				optptr = (u_char *)(epb + 1);
				optptr += PAD32(epb->caplen);

				break;
			}
			case PCAPNG_BT_SPB: {
				struct pcapng_simple_packet_fields *spb = (struct pcapng_simple_packet_fields *)(block_header + 1);
				printf("# Simple Packet Block\n");
				printf("  len %u\n",
					   spb->len);

				hex_and_ascii_print("", spb, sizeof(struct pcapng_simple_packet_fields), "\n");
				hex_and_ascii_print("", spb + 1, spb->len, "\n");
				break;
			}
			case PCAPNG_BT_PB: {
				struct pcapng_packet_fields *pb = (struct pcapng_packet_fields *)(block_header + 1);
				printf("# Packet Block\n");
				printf("  interface_id %u drops_count %u timestamp_high %u timestamp_low %u caplen %u len %u\n",
					   pb->interface_id, pb->drops_count, pb->timestamp_high, pb->timestamp_low, pb->caplen, pb->len);
				
				hex_and_ascii_print("", pb, sizeof(struct pcapng_packet_fields), "\n");

				hex_and_ascii_print("", pb + 1, pb->caplen, "\n");

				break;
			}
			case PCAPNG_BT_PIB: {
				struct pcapng_process_information_fields *pib =  (struct pcapng_process_information_fields *)(block_header + 1);
				printf("# Process Information Block\n");
				printf("  process_id %u\n",
					   pib->process_id);
				hex_and_ascii_print("", pib, sizeof(struct pcapng_process_information_fields), "\n");
				break;
			}
			case PCAPNG_BT_ISB: {
				printf("# Interface Statistics Block\n");
				break;
			}
			case PCAPNG_BT_NRB: {
				printf("# Name Record Block\n");
				break;
			}
			default:
				printf("# Unknown Block\n");
				break;
		}
		if (optptr) {
			size_t optlen = block_header->total_length - (optptr - bytes);
			
			hex_and_ascii_print("", optptr, optlen, "\n");
		}
		
	}
	/* Create block object */
	if (mode_block) {
		pcapng_block_t block = pcap_ng_block_alloc_with_raw_block(pcap, (u_char *)bytes);
		
		if (block == NULL) {
			printf("  pcap_ng_block_alloc_with_raw_block() failed: %s\n", pcap_geterr(pcap));
			return;
		}
		
		switch (pcap_ng_block_get_type(block)) {
			case PCAPNG_BT_SHB: {
				struct pcapng_section_header_fields *shb = pcap_ng_get_section_header_fields(block);
				printf("# Section Header Block\n");
				printf("  byte_order_magic 0x%x major_version %u minor_version %u section_length %llu\n",
					   shb->byte_order_magic, shb->major_version, shb->minor_version, shb->section_length);
				
				(void)new_section_info(pcap, shb);
				break;
			}
			case PCAPNG_BT_IDB: {
				struct pcapng_interface_description_fields *idb = pcap_ng_get_interface_description_fields(block);
				printf("# Interface Description Block\n");
				printf("  linktype %u reserved %u snaplen %u\n",
					   idb->linktype, idb->reserved, idb->snaplen);
				if (pcap_ng_block_get_option(block, PCAPNG_IF_NAME, &option_info) == 1)
					if (option_info.value)
						printf("  interface name: %s\n", option_info.value);
								
				(void)new_interface_info(current_section, block);
				break;
			}
			case PCAPNG_BT_EPB: {
				struct pcapng_enhanced_packet_fields *epb = pcap_ng_get_enhanced_packet_fields(block);
				printf("# Enhanced Packet Block\n");
				printf("  interface_id %u timestamp_high %u timestamp_low %u caplen %u len %u\n",
					   epb->interface_id, epb->timestamp_high, epb->timestamp_low, epb->caplen, epb->len);
				break;
			}
			case PCAPNG_BT_SPB: {
				struct pcapng_simple_packet_fields *spb = pcap_ng_get_simple_packet_fields(block);
				printf("# Simple Packet Block\n");
				printf("  len %u\n",
					   spb->len);
				break;
			}
			case PCAPNG_BT_PB: {
				struct pcapng_packet_fields *pb = pcap_ng_get_packet_fields(block);
				printf("# Packet Block\n");
				printf("  interface_id %u drops_count %u timestamp_high %u timestamp_low %u caplen %u len %u\n",
					   pb->interface_id, pb->drops_count, pb->timestamp_high, pb->timestamp_low, pb->caplen, pb->len);
				break;
			}
			case PCAPNG_BT_PIB: {
				struct pcapng_process_information_fields *pib = pcap_ng_get_process_information_fields(block);
				printf("# Process Information Block\n");
				printf("  process_id %u\n",
					   pib->process_id);
				
				if (pcap_ng_block_get_option(block, PCAPNG_PIB_NAME, &option_info) == 1)
					if (option_info.value)
						printf("  process name: %s\n", option_info.value);
				break;
			}
			case PCAPNG_BT_ISB: {
				printf("# Interface Statistics Block\n");
				break;
			}
			case PCAPNG_BT_NRB: {
				printf("# Name Record Block\n");
				break;
			}
			default:
				printf("# Unknown Block\n");
				break;
		}
		pcnapng_block_iterate_options(block, block_option_iterator, NULL);
	}
}

#define	SWAPLONG(y) \
((((y)&0xff)<<24) | (((y)&0xff00)<<8) | (((y)&0xff0000)>>8) | (((y)>>24)&0xff))
#define	SWAPSHORT(y) \
( (((y)&0xff)<<8) | ((u_short)((y)&0xff00)>>8) )
#define	SWAPLONGLONG(y) \
(SWAPLONG((unsigned long)(y)) << 32 | SWAPLONG((unsigned long)((y) >> 32)))

void
test_pcap_ng_fopen_offline(const char *filename, char *errbuf)
{
	FILE *fp = fopen(filename, "r");
	off_t offset;
	
	if (fp == NULL) {
		warn("fopen(%s) failed", filename);
		return;
	}
	offset = ftello(fp);
	pcap_t *pcap = pcap_ng_fopen_offline(fp, errbuf);
	if (pcap == NULL) {
		warnx("pcap_ng_fopen_offline(%s) failed: %s\n",
		      filename, errbuf);
		if (ftello(fp) != offset)
			warnx("pcap_ng_fopen_offline(%s) ftello(fp) (%llu) != offset (%llu)",
			      filename, ftello(fp), offset);
	} else {
		pcap_close(pcap);
	}
	fclose(fp);
	
}


int main(int argc, const char * argv[])
{
	int i;
	char errbuf[PCAP_ERRBUF_SIZE];
	
#if 0
	u_int64_t	section_length;
	
	section_length = 0x0004000300020001;
	printf("section_length %llx\n", section_length);
	
	u_int32_t lowlong = (u_int32_t)section_length;
	printf("lowlong %x\n", lowlong);
	
	u_int32_t hilong = (u_int32_t)(section_length >> 32);
	printf("hilong %x\n", hilong);
	
	u_int64_t swapped = SWAPLONGLONG(section_length);
	printf("swapped %llx\n", swapped);
	
	int nums[11] = { 0, 1, 2, 3, 4, 5, -1, -2, -3, -4, -5 };
	for (i = 0; i < 11; i++) {
		printf("PCAPNG_ROUNDUP32(%d): %d\n", nums[i], PCAPNG_ROUNDUP32(nums[i]));
	}
#endif
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-raw") == 0) {
			mode_raw = 1;
			continue;
		}
		if (strcmp(argv[i], "-block") == 0) {
			mode_block = 1;
			continue;
		}
		if (mode_block == 0 && mode_raw == 0)
			mode_block = 1;
		
		printf("#\n# opening %s\n#\n", argv[i]);
		
		pcap_t *pcap = pcap_ng_open_offline(argv[i], errbuf);
		if (pcap == NULL) {
			warnx("pcap_ng_open_offline(%s) failed: %s\n",
				  argv[i], errbuf);
			test_pcap_ng_fopen_offline(argv[i], errbuf);
			continue;
		}
		int result = pcap_dispatch(pcap, -1, read_callback, (u_char *)pcap);
		if (result < 0) {
			warnx("pcap_dispatch failed: %s\n",
				  pcap_statustostr(result));
		} else {
			printf("# read %d packets\n", result);
		}
		pcap_close(pcap);
	}
	return 0;
}

