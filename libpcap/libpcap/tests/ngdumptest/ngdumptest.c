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

#include <stdio.h>
#include <err.h>
#include <sysexits.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "bpf.h"

#include "pcap-ng.h"

enum {
	SIMPLE_PACKET = 0,
	ENHANCED_PACKET = 1,
	OBSOLETE_PACKET = 2
};

const char *if_name = NULL;
int type_of_packet = ENHANCED_PACKET;
const char *proc_name = NULL;
uint32_t proc_pid = 0;
int32_t proc_index = -1;
const char *comment = NULL;
pcap_dumper_t *dumper = NULL;
size_t ext_len = 65536;
void *ext_buffer = NULL;
int first_comment = 0;
unsigned char *packet_data = NULL;
size_t packet_length = 0;
unsigned long num_data_blocks = 1;
int copy_data_buffer = 0;

void hex_and_ascii_print(const char *, const void *, unsigned int , const char *);

void
help(const char *str)
{
	printf("# usage: %s options...\n", str);
	printf("# options in order of dependency:\n");
	printf(" %-20s # %s\n", "-4 ipv4:name[:name...]", "IPv4 name resolution record");
	printf(" %-20s # %s\n", "-6 ipv6:name[:name...]", "IPv6 name resolution record");
	printf(" %-20s # %s\n", "-C", "copy data packet from data buffer");
	printf(" %-20s # %s\n", "-c string", "comment option");
	printf(" %-20s # %s\n", "-D length", "packet data length");
	printf(" %-20s # %s\n", "-d string", "packet data as a string");
	printf(" %-20s # %s\n", "-f", "first comment option");
	printf(" %-20s # %s\n", "-i name", "interface name");
	printf(" %-20s # %s\n", "-n num_data", "number of data blocks");
	printf(" %-20s # %s\n", "-p name:pid", "process name and pid");
	printf(" %-20s # %s\n", "-e", "enhanced packet block (default)");
	printf(" %-20s # %s\n", "-o", "(obsolete) packet block -- not implemented");
	printf(" %-20s # %s\n", "-s", "simple packet block");
	printf(" %-20s # %s\n", "-w name", "packet capture file name");
	printf(" %-20s # %s\n", "-x [buffer_length]", "externalize in buffer of given length");
}

void
write_block(pcapng_block_t block)
{
	bpf_u_int32 n;
	
	if (dumper != NULL) {
		n = pcap_ng_dump_block(dumper, block);
		if (n != pcap_ng_block_get_len(block))
			printf("%s: block len %u != pcap_ng_block_get_len() %u\n",
				   __func__, pcap_ng_block_get_len(block), n);
	}
	if (ext_buffer != NULL) {
		n = pcap_ng_externalize_block(ext_buffer, ext_len, block);
		hex_and_ascii_print("", ext_buffer, pcap_ng_block_get_len(block), "");
		if (n != pcap_ng_block_get_len(block))
			printf("%s: block len %u != pcap_ng_externalize_block() %u\n",
				   __func__, pcap_ng_block_get_len(block), n);
	}
}

pcapng_block_t
make_interface_description_block(pcapng_block_t block, const char *name)
{
	struct pcapng_interface_description_fields *idb_fields;

	pcap_ng_block_reset(block, PCAPNG_BT_IDB);
	
	if (first_comment && comment)
		pcap_ng_block_add_option_with_string(block, PCAPNG_OPT_COMMENT, comment);
	
	idb_fields = pcap_ng_get_interface_description_fields(block);
	idb_fields->linktype = DLT_RAW;
	idb_fields->snaplen = 65536;
	
	if (!first_comment && comment)
		pcap_ng_block_add_option_with_string(block, PCAPNG_OPT_COMMENT, comment);
	
	pcap_ng_block_add_option_with_string(block, PCAPNG_IF_NAME, name);
	
	write_block(block);
	
	return (block);
}

pcapng_block_t
make_process_information_block(pcapng_block_t block, const char *name, uint32_t pid)
{
	struct pcapng_process_information_fields *pib_fields;

	pcap_ng_block_reset(block, PCAPNG_BT_PIB);

	if (first_comment && comment)
		pcap_ng_block_add_option_with_string(block, PCAPNG_OPT_COMMENT, comment);

	pib_fields = pcap_ng_get_process_information_fields(block);
	pib_fields->process_id = pid;
	
	if (!first_comment && comment)
		pcap_ng_block_add_option_with_string(block, PCAPNG_OPT_COMMENT, comment);
	
	pcap_ng_block_add_option_with_string(block, PCAPNG_PIB_NAME, name);
	
	write_block(block);
	
	return (block);
}

pcapng_block_t
make_data_block(pcapng_block_t block, const void *data, size_t len)
{
	unsigned int i;
	
	for (i = 0; i < num_data_blocks; i++) {
		switch (type_of_packet) {
			case SIMPLE_PACKET: {
				pcap_ng_block_reset(block, PCAPNG_BT_SPB);
				
				if (first_comment && comment)
					pcap_ng_block_add_option_with_string(block, PCAPNG_OPT_COMMENT, comment);
				
				if (copy_data_buffer)
					pcap_ng_block_packet_copy_data(block, data, len);
				else
					pcap_ng_block_packet_set_data(block, data, len);
				
				if (!first_comment && comment)
					pcap_ng_block_add_option_with_string(block, PCAPNG_OPT_COMMENT, comment);
				
				write_block(block);
				break;
			}
			case ENHANCED_PACKET: {
				struct pcapng_enhanced_packet_fields *epb_fields;
				
				pcap_ng_block_reset(block, PCAPNG_BT_EPB);
				
				if (first_comment && comment)
					pcap_ng_block_add_option_with_string(block, PCAPNG_OPT_COMMENT, comment);
				
				epb_fields = pcap_ng_get_enhanced_packet_fields(block);
				epb_fields->caplen = len;
				epb_fields->len = len;
				epb_fields->interface_id = 0;
				epb_fields->timestamp_high = 10000;
				epb_fields->timestamp_low = 2000;
				
				if (copy_data_buffer)
					pcap_ng_block_packet_copy_data(block, data, len);
				else
					pcap_ng_block_packet_set_data(block, data, len);
								
				if (proc_name != NULL) {
					pcap_ng_block_add_option_with_value(block, PCAPNG_EPB_PIB_INDEX, &proc_index, sizeof(proc_index));
				}
				if (!first_comment && comment)
					pcap_ng_block_add_option_with_string(block, PCAPNG_OPT_COMMENT, comment);
				
				write_block(block);
				break;
			}
			default:
				break;
		}
	}
	return (block);
}

void
make_section_header_block()
{
	pcapng_block_t block = pcap_ng_block_alloc(65536);

	pcap_ng_block_reset(block, PCAPNG_BT_SHB);

	pcap_ng_block_add_option_with_string(block, PCAPNG_OPT_COMMENT,
												  "section header block");
	
	write_block(block);
	
	pcap_ng_free_block(block);

	return;
}

pcapng_block_t
make_name_resolution_record(pcapng_block_t block, int af, void *addr, char **names)
{
	pcap_ng_block_reset(block, PCAPNG_BT_NRB);

	if (first_comment && comment)
		pcap_ng_block_add_option_with_string(block, PCAPNG_OPT_COMMENT, comment);

	if (af == AF_INET) {
		pcap_ng_block_add_name_record_with_ip4(block, (struct in_addr *)addr, (const char **)names);
	} else {
		pcap_ng_block_add_name_record_with_ip6(block, (struct in6_addr *)addr, (const char **)names);
	}
	
	if (!first_comment && comment)
		pcap_ng_block_add_option_with_string(block, PCAPNG_OPT_COMMENT, comment);

	write_block(block);
	
	return (block);
}

int
main(int argc, char * const argv[])
{
	int ch;
	const char *file_name = NULL;
	pcapng_block_t block;
	
	/*
	 * Loop through argument to build PCAP-NG block
	 * Optionally write to file
	 */
	while ((ch = getopt(argc, argv, "4:6:Cc:D:d:efhi:n:o:p:sw:x")) != -1) {
		switch (ch) {
			case 'C':
				copy_data_buffer = 1;
				break;
				
			case 'c':
				comment = optarg;
				break;
				
			case 'D': {
				int i;
				
				packet_length = strtoul(optarg, NULL, 0);
				packet_data = malloc(packet_length);
				for (i = 0; i < packet_length; i++) {
					packet_data[i] = i % 256;
				}
				
				block = pcap_ng_block_alloc(65536);
				make_data_block(block, packet_data, packet_length);
				pcap_ng_free_block(block);
				break;
			}
			case 'd':
				packet_length  = strlen(optarg);
				packet_data = (unsigned char *)optarg;
				block = pcap_ng_block_alloc(65536);
				make_data_block(block, packet_data, packet_length);
				pcap_ng_free_block(block);
				break;
				
			case 'e':
				type_of_packet = ENHANCED_PACKET;
				break;
				
			case 'f':
				first_comment = 1;
				break;

			case 'h':
				help(argv[0]);
				return (0);
				
			case 'i':
				if_name = optarg;
				
				block = pcap_ng_block_alloc(65536);
				make_interface_description_block(block, if_name);
				pcap_ng_free_block(block);
				
				break;
				
			case '4':
			case '6': {
				int af;
				int name_count = 0;
				char *ptr = strchr(optarg, ':');
				char **names = NULL;
				int i;
				struct in_addr ina;
				struct in6_addr in6a;
				int retval;
				
				
				if (ptr == NULL) {
					fprintf(stderr, "malformed arguments for option 'n'\n");
					help(argv[0]);
					return (0);
				}
				do {
					ptr++;
					name_count += 1;
				} while ((ptr = strchr(ptr, ':')) != NULL);
				
				names = calloc(name_count +1, sizeof(char *));
				
				ptr = strchr(optarg, ':');
				*ptr = '\0';
				
				if (ch == '4') {
					af = AF_INET;
					retval = inet_pton(af, optarg, &ina);
				} else {
					af = AF_INET6;
					retval = inet_pton(af, optarg, &in6a);
				}
				if (retval == 0)
					errx(1, "This is not an %s address: '%s'\n",
						 af == AF_INET ? "IPv4" : "IPv6",
						 optarg);
				else if (retval == -1)
					err(1, "inet_pton(%s) failed\n", optarg);
				
				for (i = 0; i < name_count; i++) {
					char *end;
					ptr++;
					end = strchr(ptr, ':');
					if (end != NULL)
						*end = '\0';
					
					names[i] = strdup(ptr);
					if (end != NULL)
						ptr = end;
				}

				block = pcap_ng_block_alloc(65536);
				if (af == AF_INET)
					make_name_resolution_record(block, af,  &ina, names);
				else
					make_name_resolution_record(block, af, &in6a, names);
				pcap_ng_free_block(block);
				break;
			}
				
			case 'n': {
				num_data_blocks = strtoul(optarg, NULL, 0);
				break;
			}
				
			case 'o':
				type_of_packet = OBSOLETE_PACKET;
				break;
				
			case 'p': {
				char *ptr = strchr(optarg, ':');
				char *endptr;
				
				if (ptr == NULL) {
					fprintf(stderr, "malformed arguments for option 'p'\n");
					help(argv[0]);
					return (0);
				}
				proc_name = strndup(optarg, (ptr - optarg));
				ptr += 1;
				if (*ptr == '\0') {
					fprintf(stderr, "malformed arguments for option 'p'\n");
					help(argv[0]);
					return (0);
				}
				proc_pid = (uint32_t)strtoul(ptr, &endptr, 0);
				if (endptr != NULL && *endptr != '\0') {
					fprintf(stderr, "malformed arguments for option 'p'\n");
					help(argv[0]);
					return (0);
				}
				proc_index += 1;
				
				block = pcap_ng_block_alloc(65536);
				make_process_information_block(block, proc_name, proc_pid);
				pcap_ng_free_block(block);
				break;
			}
			case 's':
				type_of_packet = SIMPLE_PACKET;
				break;
				
			case 'w':
				file_name = optarg;
				
				if (dumper != NULL)
					pcap_ng_dump_close(dumper);
				
				pcap_t *pcap = pcap_open_dead(DLT_PCAPNG, 65536);
				if (pcap == NULL)
					err(EX_OSERR, "pcap_open_dead(DLT_PCAPNG, 65536) failed\n");
				
				dumper = pcap_ng_dump_open(pcap, file_name);
				if (pcap == NULL)
					err(EX_OSERR,  "pcap_ng_dump_open(%s) failed\n", file_name);

				
				make_section_header_block();
				break;

			case 'x':
				if (optind < argc) {
					if (argv[optind][0] != '-') {
						char *endptr;
						unsigned long num = strtoul(argv[optind], &endptr, 0);
						if (endptr != NULL && *endptr == '\0') {
							optind++;
							ext_len = num;
						}
					}
				}
				ext_buffer = malloc(ext_len);
				if (ext_buffer == NULL)
					errx(EX_OSERR, "malloc(%lu) failed", ext_len);
				break;
			default:
				help(argv[0]);
				return (0);
		}
	}
		
	if (dumper != NULL)
		pcap_ng_dump_close(dumper);
	
	return (0);
}