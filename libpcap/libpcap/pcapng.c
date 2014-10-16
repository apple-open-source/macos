/*
 * Copyright (c) 2012-2014 Apple Inc. All rights reserved.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <err.h>
#include <sysexits.h>
#include <unistd.h>
#include <sys/uio.h>
#include "pcapng_private.h"

#include "pcap-int.h"
#include "pcap-common.h"
#include "sf-pcap-ng.h"
#include "pcap-util.h"

/*
 * Block cursor - used when processing the contents of a block.
 * Contains a pointer into the data being processed and a count
 * of bytes remaining in the block.
 */
struct block_cursor {
	u_char		*data;
	size_t		data_remaining;
	bpf_u_int32	block_type;
};


#define PAD_32BIT(x) ((x + 3) & ~3)
#define PAD_64BIT(x) ((x + 7) & ~7)
#define PADDED_OPTION_LEN(x) ((x) ? PAD_32BIT(x) + sizeof(struct pcapng_option_header) : 0)


void *
pcap_ng_block_header_ptr(pcapng_block_t block)
{
	return (block->pcapng_bufptr);
}

void *
pcap_ng_block_fields_ptr(pcapng_block_t block)
{
	return (block->pcapng_bufptr +
		sizeof(struct pcapng_block_header));
}

void *
pcap_ng_block_data_ptr(pcapng_block_t block)
{
	if (block->pcapng_data_is_external)
		return (block->pcapng_data_ptr);
	else
		return (block->pcapng_bufptr +
			sizeof(struct pcapng_block_header) +
			block->pcapng_fields_len);
}

void *
pcap_ng_block_records_ptr(pcapng_block_t block)
{
	if (block->pcapng_block_type != PCAPNG_BT_NRB)
		return (NULL);
	
	if (block->pcapng_data_is_external)
		return (block->pcapng_bufptr +
			sizeof(struct pcapng_block_header) +
			block->pcapng_fields_len);
	else
		return (block->pcapng_bufptr +
			sizeof(struct pcapng_block_header) +
			block->pcapng_fields_len +
			block->pcapng_data_len);
}

void *
pcap_ng_block_options_ptr(pcapng_block_t block)
{
	if (block->pcapng_block_type == PCAPNG_BT_SPB)
		return (NULL);
	
	if (block->pcapng_data_is_external)
		return (block->pcapng_bufptr +
			sizeof(struct pcapng_block_header) +
			block->pcapng_fields_len +
			block->pcapng_records_len);
	else
		return (block->pcapng_bufptr +
			sizeof(struct pcapng_block_header) +
			block->pcapng_fields_len +
			block->pcapng_data_len +
			block->pcapng_records_len);
}

void *
pcap_ng_block_trailer_ptr(pcapng_block_t block)
{
	if (block->pcapng_data_is_external)
		return (block->pcapng_bufptr +
			sizeof(struct pcapng_block_header) +
			block->pcapng_fields_len +
			block->pcapng_records_len +
			block->pcapng_options_len);
	else
		return (block->pcapng_bufptr +
			sizeof(struct pcapng_block_header) +
			block->pcapng_fields_len +
			block->pcapng_data_len +
			block->pcapng_records_len +
			block->pcapng_options_len);
}

pcapng_block_t
pcap_ng_block_alloc(size_t len)
{
	size_t totallen;
	u_char *ptr;
	struct pcapng_block *block;
	
	/*
	 * The internal block structure is prepended
	 */
	totallen = PAD_64BIT(sizeof(struct pcapng_block)) + len;
	ptr = malloc(totallen);
	if (ptr == NULL)
		return (NULL);
	
	block = (struct pcapng_block *)ptr;
	bzero(block, sizeof(struct pcapng_block));
	
	block->pcapng_bufptr = ptr + PAD_64BIT(sizeof(struct pcapng_block));
	block->pcapng_buflen = len;
	
	
	return (block);
}

void
pcap_ng_free_block(pcapng_block_t block)
{
	free(block);
}

bpf_u_int32
pcap_ng_block_get_type(pcapng_block_t block)
{
	return (block->pcapng_block_type);
}

bpf_u_int32
pcap_ng_block_get_len(pcapng_block_t block)
{
	return (block->pcapng_block_len);
}

int
pcap_ng_block_is_swapped(pcapng_block_t block)
{
	return (block->pcapng_block_swapped);
}

int
pcapng_update_block_length(pcapng_block_t block)
{
	block->pcapng_block_len = sizeof(struct pcapng_block_header) +
		block->pcapng_fields_len +
		block->pcapng_data_len +
		block->pcapng_records_len +
		block->pcapng_options_len +
		sizeof(struct pcapng_block_trailer);
	
	if (block->pcapng_block_len > block->pcapng_buflen) {
		errx(EX_SOFTWARE, "%s block len %lu greater than buffer size %lu",
		     __func__, block->pcapng_block_len, block->pcapng_buflen);
		
	}
	
	return (0);
}

int
pcap_ng_block_reset(pcapng_block_t block, bpf_u_int32 type)
{
	bzero(&block->block_fields_, sizeof(block->block_fields_));
	
	switch (type) {
		case PCAPNG_BT_SHB:
			block->pcapng_block_type = type;
			
			block->pcap_ng_shb_fields.byte_order_magic = PCAPNG_BYTE_ORDER_MAGIC;
			block->pcap_ng_shb_fields.major_version = PCAPNG_VERSION_MAJOR;
			block->pcap_ng_shb_fields.minor_version = PCAPNG_VERSION_MINOR;
			block->pcap_ng_shb_fields.section_length = (uint64_t)-1;
			
			block->pcapng_fields_len = sizeof(struct pcapng_section_header_fields);
			break;
		
		case PCAPNG_BT_IDB:
			block->pcapng_block_type = type;
			
			block->pcapng_fields_len = sizeof(struct pcapng_interface_description_fields);
			break;
		
		case PCAPNG_BT_PB:
			block->pcapng_block_type = type;
			
			block->pcapng_fields_len = sizeof(struct pcapng_packet_fields);
			
			break;
		
		case PCAPNG_BT_SPB:
			block->pcapng_block_type = type;
			
			block->pcapng_fields_len = sizeof(struct pcapng_simple_packet_fields);
			
			break;
		
		case PCAPNG_BT_NRB:
			block->pcapng_block_type = type;
			
			block->pcapng_fields_len = 0;
			break;
		
		case PCAPNG_BT_ISB:
			block->pcapng_block_type = type;
			
			block->pcapng_fields_len = sizeof(struct pcapng_interface_statistics_fields);
			break;
		
		case PCAPNG_BT_EPB:
			block->pcapng_block_type = type;
			
			block->pcapng_fields_len = sizeof(struct pcapng_enhanced_packet_fields);
			break;
		
		case PCAPNG_BT_PIB:
			block->pcapng_block_type = type;
			
			block->pcapng_fields_len = sizeof(struct pcapng_process_information_fields);
			break;
		
		default:
			return (PCAP_ERROR);
	}
	
	block->pcapng_data_ptr = NULL;
	block->pcapng_data_len = 0;
	block->pcapng_cap_len = 0;
	block->pcapng_data_is_external = 0;
	
	block->pcapng_records_len = 0;
	
	block->pcapng_options_len = 0;
	
	pcapng_update_block_length(block);
	
	return (0);
}

struct pcapng_section_header_fields *
pcap_ng_get_section_header_fields(pcapng_block_t block)
{
	if (block != NULL && block->pcapng_block_type == PCAPNG_BT_SHB)
		return &block->pcap_ng_shb_fields;
	else
		return NULL;
}

struct pcapng_interface_description_fields *
pcap_ng_get_interface_description_fields(pcapng_block_t block)
{
	if (block != NULL && block->pcapng_block_type == PCAPNG_BT_IDB)
		return &block->pcap_ng_idb_fields;
	else
		return NULL;
}

struct pcapng_enhanced_packet_fields *
pcap_ng_get_enhanced_packet_fields(pcapng_block_t block)
{
	if (block != NULL && block->pcapng_block_type == PCAPNG_BT_EPB)
		return &block->pcap_ng_epb_fields;
	else
		return NULL;
}

struct pcapng_simple_packet_fields *
pcap_ng_get_simple_packet_fields(pcapng_block_t block)
{
	if (block != NULL && block->pcapng_block_type == PCAPNG_BT_SPB)
		return &block->pcap_ng_spb_fields;
	else
		return NULL;
}

struct pcapng_packet_fields *
pcap_ng_get_packet_fields(pcapng_block_t block)
{
	if (block != NULL && block->pcapng_block_type == PCAPNG_BT_PB)
		return &block->pcap_ng_opb_fields;
	else
		return NULL;
}

struct pcapng_interface_statistics_fields *
pcap_ng_get_interface_statistics_fields(pcapng_block_t block)
{
	if (block != NULL && block->pcapng_block_type == PCAPNG_BT_ISB)
		return &block->pcap_ng_isb_fields;
	else
		return NULL;
}

struct pcapng_process_information_fields *
pcap_ng_get_process_information_fields(pcapng_block_t block)
{
	if (block != NULL && block->pcapng_block_type == PCAPNG_BT_PIB)
		return &block->pcap_ng_pib_fields;
	else
		return NULL;
}

int
pcap_ng_block_does_support_data(pcapng_block_t block)
{
	switch (block->pcapng_block_type) {
		case PCAPNG_BT_PB:
		case PCAPNG_BT_SPB:
		case PCAPNG_BT_EPB:
			return (1);
			/* NOT REACHED */
			
		default:
			break;
	}
	return (0);
}

void *
pcap_ng_block_packet_get_data_ptr(pcapng_block_t block)
{
	if (pcap_ng_block_does_support_data(block) == 0)
		return (NULL);
	
	return (block->pcapng_data_ptr);
}

bpf_u_int32
pcap_ng_block_packet_get_data_len(pcapng_block_t block)
{
	if (pcap_ng_block_does_support_data(block) == 0)
		return (0);
	
	return (block->pcapng_cap_len);
}

bpf_u_int32
pcap_ng_block_packet_copy_data(pcapng_block_t block, const void *ptr,
                               bpf_u_int32 caplen)
{
	bpf_u_int32 padding_len = PAD_32BIT(caplen) - caplen;
	
	if (pcap_ng_block_does_support_data(block) == 0)
		return (PCAP_ERROR);
	
	if (block->pcapng_block_len + PAD_32BIT(caplen) > block->pcapng_buflen) {
		warnx("%s block len %lu greater than buffer size %lu",
			  __func__, block->pcapng_block_len, block->pcapng_buflen);
		return (PCAP_ERROR);
	}
	/*
	 * Move the name records and options if necessary
	 */
	if (block->pcapng_records_len > 0 || block->pcapng_options_len > 0) {
		u_char *tmp = pcap_ng_block_records_ptr(block) ?
			pcap_ng_block_records_ptr(block) :
			pcap_ng_block_options_ptr(block);
		size_t len = block->pcapng_records_len + block->pcapng_options_len;
		int32_t offset = PAD_32BIT(caplen) - block->pcapng_data_len;
		
		bcopy(tmp, tmp + offset, len);
	}
	
	/*
	 * TBD: if records or options exist, should move them or error out
	 */
	block->pcapng_data_is_external = 0;
	block->pcapng_data_ptr = pcap_ng_block_data_ptr(block);
	bcopy(ptr, block->pcapng_data_ptr, caplen);
	if (padding_len > 0)
		bzero(block->pcapng_data_ptr + caplen, padding_len);
	block->pcapng_cap_len = caplen;
	block->pcapng_data_len = PAD_32BIT(caplen);
	
	pcapng_update_block_length(block);
	
	return (0);
}

bpf_u_int32
pcap_ng_block_packet_set_data(pcapng_block_t block, const void *ptr,
                              bpf_u_int32 caplen)
{
	if (pcap_ng_block_does_support_data(block) == 0)
		return (PCAP_ERROR);
	
	block->pcapng_data_is_external = 1;
	block->pcapng_data_ptr = (u_char *)ptr;
	block->pcapng_cap_len = caplen;
	block->pcapng_data_len = PAD_32BIT(caplen);
	
	pcapng_update_block_length(block);
	
	return (0);
}

int
pcap_ng_block_add_option_with_value(pcapng_block_t block, u_short code,
                                    const void *value, u_short value_len)
{
	size_t optlen = sizeof(struct pcapng_option_header) + PAD_32BIT(value_len);
	struct pcapng_option_header *opt_header;
	bpf_u_int32 padding_len = PAD_32BIT(value_len) - value_len;
	u_char *buffer;
	u_char *block_option_ptr = pcap_ng_block_options_ptr(block);
	
	if (block_option_ptr == NULL) {
		warnx("%s options not supported for block type %u",
			  __func__, block->pcapng_block_type);
		return (PCAP_ERROR);
	}
	
	if (optlen + block->pcapng_block_len > block->pcapng_buflen) {
		warnx("%s block len %lu greater than buffer size %lu",
			  __func__, block->pcapng_block_len, block->pcapng_buflen);
		return (PCAP_ERROR);
	}
	
	opt_header = (struct pcapng_option_header *)(block_option_ptr + block->pcapng_options_len);
	/* Insert before the end of option */
	if (block->pcapng_options_len > 0)
		opt_header -= 1;
	opt_header->option_code = code;
	opt_header->option_length = value_len;
	
	buffer = (u_char *)(opt_header + 1);
	
	bcopy(value, buffer, value_len);
	
	if (padding_len > 0)
		bzero(buffer + value_len, padding_len);
	
	/* Add end of option when first option added */
	if (block->pcapng_options_len == 0)
		block->pcapng_options_len = sizeof(struct pcapng_option_header);
	
	block->pcapng_options_len += optlen;
	
	/* Set the end of option at the end of the options */
	opt_header = (struct pcapng_option_header *)(block_option_ptr + block->pcapng_options_len);
	opt_header -= 1;
	opt_header->option_code = PCAPNG_OPT_ENDOFOPT;
	opt_header->option_length = 0;
	
	pcapng_update_block_length(block);
	
	return (0);
}

int
pcap_ng_block_add_option_with_string(pcapng_block_t block, u_short code, const char *str)
{
	return (pcap_ng_block_add_option_with_value(block, code, str, strlen(str) + 1));
}

static struct pcapng_option_header *
get_opthdr_from_block_data(struct pcapng_option_header *opthdr, int swapped,
			   struct block_cursor *cursor, char *errbuf)
{
	struct pcapng_option_header *optp;
	
	optp = get_from_block_data(cursor, sizeof(*opthdr), errbuf);
	if (optp == NULL) {
		/*
		 * Option header is cut short.
		 */
		return (NULL);
	}
	*opthdr = *optp;
	/*
	 * Byte-swap it if necessary.
	 */
	if (swapped) {
		opthdr->option_code = SWAPSHORT(opthdr->option_code);
		opthdr->option_length = SWAPSHORT(opthdr->option_length);
	}
	
	return (opthdr);
}

static void *
get_optvalue_from_block_data(struct block_cursor *cursor,
			     struct pcapng_option_header *opthdr, char *errbuf)
{
	size_t padded_option_len;
	void *optvalue;
	
	/* Pad option length to 4-byte boundary */
	padded_option_len = opthdr->option_length;
	padded_option_len = ((padded_option_len + 3)/4)*4;
	
	optvalue = get_from_block_data(cursor, padded_option_len, errbuf);
	if (optvalue == NULL) {
		/*
		 * Option value is cut short.
		 */
		return (NULL);
	}
	
	return (optvalue);
}


int
pcap_ng_block_get_option(pcapng_block_t block, u_short code, struct pcapng_option_info *option_info)
{
	struct pcapng_option_header opthdr;
	int swapped;
	int num_of_options = 0;
	struct block_cursor cursor;
	
	if (option_info == NULL)
		return (PCAP_ERROR);
	if (block->pcapng_options_len == 0)
		goto done;
	
	swapped = block->pcapng_block_swapped;
	
	cursor.block_type = block->pcapng_block_type;
	cursor.data = pcap_ng_block_options_ptr(block);
	cursor.data_remaining = block->pcapng_options_len;
	
	while (get_opthdr_from_block_data(&opthdr, swapped, &cursor, NULL)) {
		void *value = get_optvalue_from_block_data(&cursor, &opthdr, NULL);
		
		/*
		 * If option is cut short we cannot parse it, give up
		 */
		if (opthdr.option_length != 0 && value == NULL)
			break;
		
		if (code == opthdr.option_code) {
			option_info->code = opthdr.option_code;
			option_info->length = opthdr.option_length;
			option_info->value = value;
			
			num_of_options = 1;
			break;
		}
		/*
		 * Detect end of option delimiter
		 */
		if (opthdr.option_code == PCAPNG_OPT_ENDOFOPT)
			break;
	}
	
done:
	return (num_of_options);
}

int
pcnapng_block_iterate_options(pcapng_block_t block,
                              pcapng_option_iterator_func opt_iterator_func,
                              void *context)
{
	struct pcapng_option_header opthdr;
	int swapped;
	int num_of_options = 0;
	struct block_cursor cursor;

	if (block == NULL || opt_iterator_func == NULL)
		return (PCAP_ERROR);
	swapped = block->pcapng_block_swapped;

	cursor.block_type = block->pcapng_block_type;
	cursor.data = pcap_ng_block_options_ptr(block);
	cursor.data_remaining = block->pcapng_options_len;

	while (get_opthdr_from_block_data(&opthdr, swapped, &cursor, NULL)) {
		void *value = get_optvalue_from_block_data(&cursor, &opthdr, NULL);
		struct pcapng_option_info option_info;

		/*
		 * If option is cut short we cannot parse it, give up
		 */
		if (opthdr.option_length != 0 && value == NULL)
			break;
		
		option_info.code = opthdr.option_code;
		option_info.length = opthdr.option_length;
		option_info.value = value;
			
		num_of_options++;
		
		opt_iterator_func(block, &option_info, context);

		/*
		 * Detect end of option delimiter
		 */
		if (opthdr.option_code == PCAPNG_OPT_ENDOFOPT)
			break;
	}
	
done:
	return (num_of_options);
}

int
pcnapng_block_iterate_name_records(pcapng_block_t block,
                                   pcapng_name_record_iterator_func record_iterator_func,
                                   void *context)
{
	struct pcapng_record_header recordhdr;
	int swapped;
	int num_of_records = 0;
	struct block_cursor cursor;

	if (block == NULL || record_iterator_func == NULL)
		return (PCAP_ERROR);
	swapped = block->pcapng_block_swapped;

	cursor.block_type = block->pcapng_block_type;
	cursor.data = pcap_ng_block_records_ptr(block);
	cursor.data_remaining = block->pcapng_records_len;
	
	/*
	 * Note that we take advantage of the fact that name record headers
	 * have the same layout as option headers
	 */
	while (get_opthdr_from_block_data((struct pcapng_option_header *)&recordhdr,
									  swapped, &cursor, NULL)) {
		struct pcapng_name_record_info record_info;
		void *value =
			get_optvalue_from_block_data(&cursor,
			                             (struct pcapng_option_header *)&recordhdr,
			                             NULL);

		/*
		 * If record is cut short we cannot parse it, give up
		 */
		if (recordhdr.record_length != 0 && value == NULL)
			break;
		
		record_info.code = recordhdr.record_type;
		record_info.length = recordhdr.record_length;
		record_info.value = value;

		num_of_records++;

		/*
		 * Detect end of option delimiter
		 */
		if (record_info.code == PCAPNG_NRES_ENDOFRECORD)
			break;
	}
	
done:
	return (num_of_records);
}

int
pcap_ng_block_add_name_record_common(pcapng_block_t block, uint32_t type,
                                     size_t addrlen, void *addr, const char **names)
{
	size_t names_len = 0;
	int i;
	const char *p;
	size_t record_len = 0;
	struct pcapng_record_header	*record_hdr;
	size_t padding_len;
	u_char *buffer;
	size_t offset;
	u_char *block_records_ptr = pcap_ng_block_records_ptr(block);

	if (block_records_ptr == NULL)
		return (PCAP_ERROR);
	
	for (i = 0; ; i++) {
		p = names[i];
		if (p == NULL || *p == 0)
			break;
		names_len += strlen(p) + 1;
	}
	
	record_len = sizeof(struct pcapng_record_header) + addrlen + PAD_32BIT(names_len);
	if (record_len + block->pcapng_block_len > block->pcapng_buflen) {
		warnx("%s block len %lu greater than buffer size %lu",
		      __func__, block->pcapng_block_len, block->pcapng_buflen);
		return (PCAP_ERROR);
	}
	
	/*
	 * Move the options if necessary
	 */
	if (block->pcapng_options_len > 0) {
		u_char *tmp = pcap_ng_block_options_ptr(block);
		
		bcopy(tmp, tmp + record_len, block->pcapng_options_len);
	}
	
	padding_len = PAD_32BIT(names_len) - names_len;
	
	record_hdr = (struct pcapng_record_header*)(block_records_ptr + block->pcapng_records_len);
	if (block->pcapng_records_len > 0)
		record_hdr -= 1;
	record_hdr->record_type = type;
	record_hdr->record_length = addrlen + PAD_32BIT(names_len);
	
	buffer = (u_char *)(record_hdr + 1);
	bcopy(addr, buffer, addrlen);
	offset = addrlen;
	for (i = 0; ; i++) {
		p = names[i];
		if (p == NULL || *p == 0)
			break;
		u_short slen = strlen(p) + 1;
		bcopy(p, buffer, slen);
		offset += slen;
	}
	if (padding_len > 0)
		bzero(buffer + offset, padding_len);
	
	block->pcapng_records_len += record_len;
	
	pcapng_update_block_length(block);

	return (0);
}

int
pcap_ng_block_add_name_record_with_ip4(pcapng_block_t block,
                                       struct in_addr *in4,
                                       const char **names)
{
	if (block->pcapng_block_type != PCAPNG_BT_NRB)
		return (PCAP_ERROR);
	
	return pcap_ng_block_add_name_record_common(block,
	                                            PCAPNG_NRES_IP4RECORD,
	                                            sizeof(struct in_addr),
	                                            in4,
	                                            names);
}

int
pcap_ng_block_add_name_record_with_ip6(pcapng_block_t block,
                                       struct in6_addr *in6,
                                       const char **names)
{
	if (block->pcapng_block_type != PCAPNG_BT_NRB)
		return (PCAP_ERROR);
	
	return pcap_ng_block_add_name_record_common(block,
	                                            PCAPNG_NRES_IP4RECORD,
	                                            sizeof(struct in6_addr),
	                                            in6,
	                                            names);
}

bpf_u_int32
pcap_ng_externalize_block(void *buffer, size_t buflen, pcapng_block_t block)
{
	struct pcapng_block_header block_header;
	struct pcapng_block_trailer block_trailer;
	bpf_u_int32 bytes_written = 0;
	u_char *ptr;
	
	if (buffer == NULL || buflen < block->pcapng_block_len)
		return (0);
	
	ptr = buffer;
	block_header.block_type = block->pcapng_block_type;
	block_header.total_length = block->pcapng_block_len;
	bcopy(&block_header, ptr + bytes_written, sizeof(struct pcapng_block_header));
	bytes_written += sizeof(struct pcapng_block_header);
	
	switch (block->pcapng_block_type) {
		case PCAPNG_BT_SHB:
		case PCAPNG_BT_IDB:
		case PCAPNG_BT_PB:
		case PCAPNG_BT_SPB:
		case PCAPNG_BT_NRB:
		case PCAPNG_BT_ISB:
		case PCAPNG_BT_EPB:
		case PCAPNG_BT_PIB:
			if (block->pcapng_block_type == PCAPNG_BT_PB) {
				if(block->pcap_ng_opb_fields.caplen == 0)
					block->pcap_ng_opb_fields.caplen = block->pcapng_cap_len;
				if(block->pcap_ng_opb_fields.len == 0)
					block->pcap_ng_opb_fields.len = block->pcapng_cap_len;
			}
			if (block->pcapng_block_type == PCAPNG_BT_SPB) {
				if(block->pcap_ng_spb_fields.len == 0)
					block->pcap_ng_spb_fields.len = block->pcapng_cap_len;
			}
			if (block->pcapng_block_type == PCAPNG_BT_EPB) {
				if(block->pcap_ng_epb_fields.caplen == 0)
					block->pcap_ng_epb_fields.caplen = block->pcapng_cap_len;
				if(block->pcap_ng_epb_fields.len == 0)
					block->pcap_ng_epb_fields.len = block->pcapng_cap_len;
			}
			
			if (block->pcapng_fields_len > 0) {
				bcopy(&block->pcap_ng_shb_fields, ptr + bytes_written, block->pcapng_fields_len);
				bytes_written += block->pcapng_fields_len;
			}
			break;
		default:
			/* Unknown block */
			return (0);
			break;
	}
	
	
	if (block->pcapng_data_len > 0) {
		bpf_u_int32 padding_len = PAD_32BIT(block->pcapng_cap_len) - block->pcapng_cap_len;
				
		bcopy(block->pcapng_data_ptr, ptr + bytes_written, block->pcapng_cap_len);
		bytes_written += block->pcapng_cap_len;
		
		if (padding_len > 0) {
			bzero(ptr + bytes_written, padding_len);
			bytes_written += padding_len;
		}
	}
	
	if (block->pcapng_records_len > 0) {
		bcopy(pcap_ng_block_records_ptr(block), ptr + bytes_written, block->pcapng_records_len);
		bytes_written += block->pcapng_records_len;
	}
	if (block->pcapng_options_len > 0) {
		bcopy(pcap_ng_block_options_ptr(block), ptr + bytes_written, block->pcapng_options_len);
		bytes_written += block->pcapng_options_len;
	}

	block_trailer.total_length = block->pcapng_block_len;
	bcopy(&block_trailer, ptr + bytes_written, bytes_written);
	bytes_written += sizeof(struct pcapng_block_trailer);		
		
	return (bytes_written);
}

bpf_u_int32
pcap_ng_dump_block(pcap_dumper_t *p, pcapng_block_t block)
{
	struct pcapng_block_header *block_header;
	struct pcapng_block_trailer *block_trailer;
	bpf_u_int32 bytes_written = 0;
	struct iovec iov[4];
	int iovcnt;
	char data_padding[3] = { 0, 0, 0 };
	
	block_header = (struct pcapng_block_header *)pcap_ng_block_header_ptr(block);
	block_header->block_type = block->pcapng_block_type;
	block_header->total_length = block->pcapng_block_len;
	
	switch (block->pcapng_block_type) {
		case PCAPNG_BT_SHB:
		case PCAPNG_BT_IDB:
		case PCAPNG_BT_PB:
		case PCAPNG_BT_SPB:
		case PCAPNG_BT_NRB:
		case PCAPNG_BT_ISB:
		case PCAPNG_BT_EPB:
		case PCAPNG_BT_PIB:
			if (block->pcapng_block_type == PCAPNG_BT_PB) {
				if(block->pcap_ng_opb_fields.caplen == 0)
					block->pcap_ng_opb_fields.caplen = block->pcapng_cap_len;
				if(block->pcap_ng_opb_fields.len == 0)
					block->pcap_ng_opb_fields.len = block->pcapng_cap_len;
			}
			if (block->pcapng_block_type == PCAPNG_BT_SPB) {
				if(block->pcap_ng_spb_fields.len == 0)
					block->pcap_ng_spb_fields.len = block->pcapng_cap_len;
			}
			if (block->pcapng_block_type == PCAPNG_BT_EPB) {
				if(block->pcap_ng_epb_fields.caplen == 0)
					block->pcap_ng_epb_fields.caplen = block->pcapng_cap_len;
				if(block->pcap_ng_epb_fields.len == 0)
					block->pcap_ng_epb_fields.len = block->pcapng_cap_len;
			}
			
			if (block->pcapng_fields_len > 0)
				bcopy(&block->pcap_ng_shb_fields, pcap_ng_block_fields_ptr(block), block->pcapng_fields_len);
			break;
		default:
			/* Unknown block */
			return (0);
			break;
	}
	
	block_trailer = pcap_ng_block_trailer_ptr(block);
	block_trailer->total_length = block_header->total_length;
	
	iovcnt = 0;
	iov[iovcnt].iov_len = sizeof(struct pcapng_block_header) + block->pcapng_fields_len;
	iov[iovcnt].iov_base = block->pcapng_bufptr;
	iovcnt++;
	
	if (block->pcapng_data_len > 0) {
		bpf_u_int32 padding_len = PAD_32BIT(block->pcapng_cap_len) - block->pcapng_cap_len;
		
		iov[iovcnt].iov_len = block->pcapng_cap_len;
		iov[iovcnt].iov_base = block->pcapng_data_ptr;
		iovcnt++;
		
		/* This is suboptimal... */
		if (padding_len > 0) {
			iov[iovcnt].iov_len = padding_len;
			iov[iovcnt].iov_base = data_padding;
			iovcnt++;
		}
	}
	/*
	 * The name records, options and block trailer are contiguous
	 */
	iov[iovcnt].iov_len = block->pcapng_records_len +
		block->pcapng_options_len +
		sizeof(struct pcapng_block_trailer);
	if (block->pcapng_records_len > 0)
		iov[iovcnt].iov_base = pcap_ng_block_records_ptr(block);
	else if (block->pcapng_options_len > 0)
		iov[iovcnt].iov_base = pcap_ng_block_options_ptr(block);
	else
		iov[iovcnt].iov_base = block_trailer;
	iovcnt++;

	bytes_written += writev(((FILE *)p)->_file, iov, iovcnt);
	
	return (bytes_written);
}

int
pcap_ng_block_internalize_common(pcapng_block_t *pblock, pcap_t *p, u_char *raw_block)
{
	pcapng_block_t block = NULL;
	struct pcapng_block_header bh = *(struct pcapng_block_header *)raw_block;
	struct block_cursor cursor;
	int swapped = 0;
	
	if (pblock == NULL || raw_block == NULL)
		return (PCAP_ERROR);
	
	if (p != NULL)
		swapped = p->swapped;
	
	if (swapped) {
		bh.block_type = SWAPLONG(bh.block_type);
		bh.total_length = SWAPLONG(bh.total_length);
	}
	
	switch (bh.block_type) {
		case PCAPNG_BT_SHB:
		    pcap_ng_init_section_info(p);
		    break;
		case PCAPNG_BT_IDB:
		case PCAPNG_BT_PB:
		case PCAPNG_BT_SPB:
		case PCAPNG_BT_NRB:
		case PCAPNG_BT_ISB:
		case PCAPNG_BT_EPB:
		case PCAPNG_BT_PIB:
			break;
		default:
			(void) snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
			                "%s: Unknown block type length %u",
			                __func__, bh.block_type);
			goto fail;
	}
	/* Check the length is reasonable, limit to 1 MBytes */
	if (bh.total_length > 1024 * 1024) {
		(void) snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
		                "%s: Block total length %u is greater than 16 MB",
		                __func__, bh.total_length);
		goto fail;
	}
	
	/*
	 * Some ntar files from wireshark.org do not round up the total block length to
	 * a multiple of 4 bytes -- they must ignore the 32 bit alignment of the block body!
	 */
	bh.total_length = PAD_32BIT(bh.total_length);
	
	if (*pblock == NULL) {
		block = pcap_ng_block_alloc(bh.total_length);
		if (block == NULL) {
			(void) snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
			                "%s: Unknown block type %u",
			                __func__, bh.block_type);
			goto fail;
		}
	} else {
		block = *pblock;
	}
	block->pcapng_bufptr = raw_block;
	block->pcapng_buflen = bh.total_length;
	block->pcapng_buf_is_external = 1;
	pcap_ng_block_reset(block, bh.block_type);
	block->pcapng_block_len = bh.total_length;
	block->pcapng_block_swapped = swapped;
	
	cursor.data = raw_block + sizeof(struct pcapng_block_header);
	cursor.data_remaining = bh.total_length -
		sizeof(struct pcapng_block_header) -
		sizeof(struct pcapng_block_trailer);
	cursor.block_type = bh.block_type;
	
	switch (bh.block_type) {
		case PCAPNG_BT_SHB: {
			struct pcapng_section_header_fields *shbp = pcap_ng_get_section_header_fields(block);
			struct pcapng_section_header_fields *rawshb;
			
			rawshb = get_from_block_data(&cursor, sizeof(struct pcapng_section_header_fields), p->errbuf);
			if (rawshb == NULL)
				goto fail;

			shbp->byte_order_magic = rawshb->byte_order_magic;
			shbp->major_version = rawshb->major_version;
			shbp->minor_version = rawshb->minor_version;
			shbp->section_length = rawshb->section_length;
			if (swapped) {
				shbp->byte_order_magic = SWAPLONG(shbp->byte_order_magic);
				shbp->major_version = SWAPSHORT(shbp->major_version);
				shbp->minor_version = SWAPSHORT(shbp->minor_version);
				shbp->section_length = SWAPLONGLONG(shbp->section_length);
			}
			
			break;
		}
		case PCAPNG_BT_IDB: {
			struct pcapng_interface_description_fields *idbp = pcap_ng_get_interface_description_fields(block);
			struct pcapng_interface_description_fields *rawidb;
			
			rawidb = get_from_block_data(&cursor, sizeof(struct pcapng_interface_description_fields), p->errbuf);
			if (rawidb == NULL)
				goto fail;
			
			idbp->linktype = rawidb->linktype;
			idbp->reserved = rawidb->reserved;
			idbp->snaplen = rawidb->snaplen;
			if (swapped) {
				idbp->linktype = SWAPSHORT(idbp->linktype);
				idbp->reserved = SWAPSHORT(idbp->reserved);
				idbp->snaplen = SWAPLONG(idbp->snaplen);
			}
			
			break;
		}
		case PCAPNG_BT_ISB: {
			struct pcapng_interface_statistics_fields *isbp = pcap_ng_get_interface_statistics_fields(block);
			struct pcapng_interface_statistics_fields *rawisb;
			
			rawisb = get_from_block_data(&cursor, sizeof(struct pcapng_interface_statistics_fields), p->errbuf);
			if (rawisb == NULL)
				goto fail;
			
			isbp->interface_id = rawisb->interface_id;
			isbp->timestamp_high = rawisb->timestamp_high;
			isbp->timestamp_low = rawisb->timestamp_low;
			if (swapped) {
				isbp->interface_id = SWAPSHORT(isbp->interface_id);
				isbp->timestamp_high = SWAPLONG(isbp->timestamp_high);
				isbp->timestamp_low = SWAPLONG(isbp->timestamp_low);
			}

			break;
		}
		case PCAPNG_BT_EPB: {
			struct pcapng_enhanced_packet_fields *epbp = pcap_ng_get_enhanced_packet_fields(block);
			struct pcapng_enhanced_packet_fields *rawepb;
			void *data;
			
			rawepb = get_from_block_data(&cursor, sizeof(struct pcapng_enhanced_packet_fields), p->errbuf);
			if (rawepb == NULL)
				goto fail;
			
			epbp->interface_id = rawepb->interface_id;
			epbp->timestamp_high = rawepb->timestamp_high;
			epbp->timestamp_low = rawepb->timestamp_low;
			epbp->caplen = rawepb->caplen;
			epbp->len = rawepb->len;
			if (swapped) {
				epbp->interface_id = SWAPLONG(epbp->interface_id);
				epbp->timestamp_high = SWAPLONG(epbp->timestamp_high);
				epbp->timestamp_low = SWAPLONG(epbp->timestamp_low);
				epbp->caplen = SWAPLONG(epbp->caplen);
				epbp->len = SWAPLONG(epbp->len);
			}
			data = get_from_block_data(&cursor, PAD_32BIT(epbp->caplen), p->errbuf);
			if (data == NULL)
				goto fail;
			block->pcapng_data_is_external = 0;
			block->pcapng_data_ptr = (u_char *)data;
			block->pcapng_cap_len = epbp->caplen;
			block->pcapng_data_len = PAD_32BIT(epbp->caplen);
			
			break;
		}
		case PCAPNG_BT_SPB: {
			struct pcapng_simple_packet_fields *spbp = pcap_ng_get_simple_packet_fields(block);
			struct pcapng_simple_packet_fields *rawspb;
			void *data;
			uint32_t caplen;
			
			rawspb = get_from_block_data(&cursor, sizeof(struct pcapng_simple_packet_fields), p->errbuf);
			if (rawspb == NULL)
				goto fail;
			
			spbp->len = rawspb->len;
			if (swapped) {
				spbp->len = SWAPLONG(spbp->len);
			}
			caplen = bh.total_length - sizeof(struct pcapng_simple_packet_fields) -
			sizeof(struct pcapng_block_header) - sizeof(struct pcapng_block_trailer);
			if (caplen > spbp->len)
				caplen = spbp->len;
			data = get_from_block_data(&cursor, PAD_32BIT(caplen), p->errbuf);
			if (data == NULL)
				goto fail;
			block->pcapng_data_is_external = 0;
			block->pcapng_data_ptr = (u_char *)data;
			block->pcapng_cap_len = caplen;
			block->pcapng_data_len = PAD_32BIT(caplen);
						
			break;
		}
		case PCAPNG_BT_PB: {
			struct pcapng_packet_fields *pbp = pcap_ng_get_packet_fields(block);
			struct pcapng_packet_fields *rawpb;
			void *data;
			
			rawpb = get_from_block_data(&cursor, sizeof(struct pcapng_packet_fields), p->errbuf);
			if (rawpb == NULL)
				goto fail;
			
			pbp->interface_id = rawpb->interface_id;
			pbp->drops_count = rawpb->drops_count;
			pbp->timestamp_high = rawpb->timestamp_high;
			pbp->timestamp_low = rawpb->timestamp_low;
			pbp->caplen = rawpb->caplen;
			pbp->len = rawpb->len;
			if (swapped) {
				/* these were written in opposite byte order */
				pbp->interface_id = SWAPSHORT(pbp->interface_id);
				pbp->drops_count = SWAPSHORT(pbp->drops_count);
				pbp->timestamp_high = SWAPLONG(pbp->timestamp_high);
				pbp->timestamp_low = SWAPLONG(pbp->timestamp_low);
				pbp->caplen = SWAPLONG(pbp->caplen);
				pbp->len = SWAPLONG(pbp->len);
			}
			
			data = get_from_block_data(&cursor, PAD_32BIT(pbp->caplen), p->errbuf);
			if (data == NULL)
				goto fail;
			block->pcapng_data_is_external = 0;
			block->pcapng_data_ptr = (u_char *)data;
			block->pcapng_cap_len = pbp->caplen;
			block->pcapng_data_len = PAD_32BIT(pbp->caplen);
						
			break;
		}
		case PCAPNG_BT_PIB: {
			struct pcapng_process_information_fields *pibp = pcap_ng_get_process_information_fields(block);
			struct pcapng_process_information_fields *rawpib;
			
			rawpib = get_from_block_data(&cursor, sizeof(struct pcapng_process_information_fields), p->errbuf);
			if (rawpib == NULL)
				goto fail;
			
			pibp->process_id = rawpib->process_id;
			if (swapped) {
				pibp->process_id = SWAPSHORT(rawpib->process_id);
			}
			break;
		}
		case PCAPNG_BT_NRB: {
			struct pcapng_record_header *rh = (struct pcapng_record_header *)(block + 1);

			while (1) {
				size_t record_len;
				
				rh = get_from_block_data(&cursor, sizeof(struct pcapng_record_header), p->errbuf);
				if (rh == NULL)
					goto fail;
				
				if (swapped)
					record_len = SWAPSHORT(rh->record_length);
				else
					record_len = rh->record_length;
				
				if (get_from_block_data(&cursor, PCAPNG_ROUNDUP32(record_len), p->errbuf) == NULL)
					goto fail;

				block->pcapng_records_len += sizeof(struct pcapng_record_header) +
					PCAPNG_ROUNDUP32(record_len);
				
				if (rh->record_type == PCAPNG_NRES_ENDOFRECORD)
					break;
			}			
			break;
		}
		default:
			goto fail;
	}
	
	/*
	 * Finally compute the length of the options as options come last in blocks
	 */
	while (1) {
		size_t optlen;
		struct pcapng_option_header *opt;

		opt = get_from_block_data(&cursor, sizeof(struct pcapng_option_header), p->errbuf);
		/* 
		 * No, or no more options
		 */
		if (opt == NULL)
			break;
		
		if (swapped)
			optlen = SWAPSHORT(opt->option_length);
		else
			optlen = opt->option_length;

		if (get_from_block_data(&cursor, PCAPNG_ROUNDUP32(optlen), p->errbuf) == NULL)
			goto fail;

		block->pcapng_options_len += sizeof(struct pcapng_option_header) + PCAPNG_ROUNDUP32(optlen);

		if (opt->option_code == PCAPNG_OPT_ENDOFOPT)
			break;
	}
				
	/* Success */
	if (*pblock == NULL)
		*pblock = block;
	return (0);
	
fail:
	if (*pblock == NULL && block != NULL)
		pcap_ng_free_block(block);
	return (PCAP_ERROR);
}

int
pcap_ng_block_init_with_raw_block(pcapng_block_t block, pcap_t *p, u_char *raw_block)
{
	return pcap_ng_block_internalize_common(&block, p, raw_block);
}

pcapng_block_t
pcap_ng_block_alloc_with_raw_block(pcap_t *p, u_char *raw_block)
{
	pcapng_block_t block = NULL;
	
	if (pcap_ng_block_internalize_common(&block, p, raw_block) == 0)
		return (block);
	else
		return (NULL);
}
