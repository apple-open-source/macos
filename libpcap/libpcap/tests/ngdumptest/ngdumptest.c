/*
 * Copyright (c) 2012-2018 Apple Inc. All rights reserved.
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
#include <sys/kern_event.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <uuid/uuid.h>
#include <net/pktap.h>

#include "bpf.h"

#include "pcap-ng.h"
#include "pcap-util.h"

enum {
	SIMPLE_PACKET = 0,
	ENHANCED_PACKET = 1,
	OBSOLETE_PACKET = 2,
	PKTAP_PACKET = 3
};

const char *if_name = NULL;
int type_of_packet = ENHANCED_PACKET;
char *proc_name = NULL;
uint32_t proc_pid = -1;
uuid_t proc_uuid;
int32_t proc_index = -1;
const char *comment = NULL;
pcap_t *pcap = NULL;
pcap_dumper_t *dumper = NULL;
size_t ext_len = 65536;
void *ext_buffer = NULL;
int first_comment = 0;
unsigned char *packet_data = NULL;
size_t packet_length = 0;
unsigned long num_data_blocks = 1;
int copy_data_buffer = 0;
int verbose = 0;

void hex_and_ascii_print(const char *, const void *, size_t, const char *);

void
help(const char *str)
{
	printf("# usage: %s options...\n", str);
	printf("# options in order of dependency:\n");
	printf(" %-36s # %s\n", "-4 ipv4:name[:name...]", "IPv4 name resolution record");
	printf(" %-36s # %s\n", "-6 ipv6:name[:name...]", "IPv6 name resolution record");
	printf(" %-36s # %s\n", "-C", "copy data packet from data buffer");
	printf(" %-36s # %s\n", "-c string", "comment option");
	printf(" %-36s # %s\n", "-D length", "packet data length");
	printf(" %-36s # %s\n", "-d string", "packet data as a string");
	printf(" %-36s # %s\n", "-f", "first comment option");
	printf(" %-36s # %s\n", "-k len", "kernel event of given len");
	printf(" %-36s # %s\n", "-i name", "interface name");
	printf(" %-36s # %s\n", "-n num_data", "number of data blocks");
	printf(" %-36s # %s\n", "-p name:pid:uuid", "process name, pid and uuid");
	printf(" %-36s # %s\n", "-t (simple|enhanced|obsolote|pktap)", "type of packet");
	printf(" %-36s # %s\n", " ", "note obsolete is not implemented");
	printf(" %-36s # %s\n", "-w name", "packet capture file name");
	printf(" %-36s # %s\n", "-x [buffer_length]", "externalize in buffer of given length");
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
		hex_and_ascii_print("", ext_buffer, pcap_ng_block_get_len(block), "\n");
		if (n != pcap_ng_block_get_len(block))
			printf("%s: block len %u != pcap_ng_externalize_block() %u\n",
				   __func__, pcap_ng_block_get_len(block), n);
	}
}

void
make_interface_description_block(const char *name)
{
	struct pcapng_interface_description_fields *idb_fields;
	pcapng_block_t block = pcap_ng_block_alloc(pcap_ng_block_size_max());

	if (verbose)
		printf("%s\n", __func__);

	pcap_ng_block_reset(block, PCAPNG_BT_IDB);

	if (first_comment && comment)
		pcap_ng_block_add_option_with_string(block, PCAPNG_OPT_COMMENT, comment);
	
	idb_fields = pcap_ng_get_interface_description_fields(block);
	idb_fields->idb_linktype = DLT_RAW;
	idb_fields->idb_snaplen = 65536;
	
	if (!first_comment && comment)
		pcap_ng_block_add_option_with_string(block, PCAPNG_OPT_COMMENT, comment);
	
	pcap_ng_block_add_option_with_string(block, PCAPNG_IF_NAME, name);
	
	write_block(block);

	pcap_ng_free_block(block);

}

void
make_process_information_block(const char *name, uint32_t pid, const uuid_t uu)
{
	struct pcapng_process_information_fields *pib_fields;
	pcapng_block_t block = pcap_ng_block_alloc(pcap_ng_block_size_max());

	if (verbose)
		printf("%s\n", __func__);

	pcap_ng_block_reset(block, PCAPNG_BT_PIB);

	if (first_comment && comment)
		pcap_ng_block_add_option_with_string(block, PCAPNG_OPT_COMMENT, comment);

	pib_fields = pcap_ng_get_process_information_fields(block);
	pib_fields->process_id = pid;
	
	if (!first_comment && comment)
		pcap_ng_block_add_option_with_string(block, PCAPNG_OPT_COMMENT, comment);
	
	if (name != NULL)
		pcap_ng_block_add_option_with_string(block, PCAPNG_PIB_NAME, name);
	
	if (uuid_is_null(uu) == 0)
		pcap_ng_block_add_option_with_uuid(block, PCAPNG_PIB_UUID, uu);
	
	write_block(block);

	pcap_ng_free_block(block);
}

void
make_kern_event_block(struct kern_event_msg *event)
{
	struct pcapng_os_event_fields *osev_fields;
	struct timeval ts;
	pcapng_block_t block = pcap_ng_block_alloc(pcap_ng_block_size_max());

	if (verbose)
		printf("%s\n", __func__);

	gettimeofday(&ts, NULL);
	
	pcap_ng_block_reset(block, PCAPNG_BT_OSEV);
	
	if (first_comment && comment)
		pcap_ng_block_add_option_with_string(block, PCAPNG_OPT_COMMENT, comment);
	
	osev_fields = pcap_ng_get_os_event_fields(block);
	osev_fields->type = PCAPNG_OSEV_KEV;
	osev_fields->timestamp_high = (bpf_u_int32)ts.tv_sec;
	osev_fields->timestamp_low = ts.tv_usec;
	osev_fields->len = event->total_size;

	if (copy_data_buffer)
		pcap_ng_block_packet_copy_data(block, event, event->total_size);
	else
		pcap_ng_block_packet_set_data(block, event, event->total_size);

	if (!first_comment && comment)
		pcap_ng_block_add_option_with_string(block, PCAPNG_OPT_COMMENT, comment);
	
	write_block(block);
	
	pcap_ng_free_block(block);
}

void
make_data_block(const void *data, size_t len)
{
	unsigned int i;
	
	for (i = 0; i < num_data_blocks; i++) {
		if (verbose)
			printf("%s\n", __func__);
		
		switch (type_of_packet) {
			case SIMPLE_PACKET: {
				pcapng_block_t block = pcap_ng_block_alloc(pcap_ng_block_size_max());

				pcap_ng_block_reset(block, PCAPNG_BT_SPB);
				
				if (first_comment && comment)
					pcap_ng_block_add_option_with_string(block, PCAPNG_OPT_COMMENT, comment);
				
				if (copy_data_buffer)
					pcap_ng_block_packet_copy_data(block, data, (bpf_u_int32)len);
				else
					pcap_ng_block_packet_set_data(block, data, (bpf_u_int32)len);
				
				if (!first_comment && comment)
					pcap_ng_block_add_option_with_string(block, PCAPNG_OPT_COMMENT, comment);
				
				write_block(block);

				pcap_ng_free_block(block);
				break;
			}
			case ENHANCED_PACKET: {
				struct pcapng_enhanced_packet_fields *epb_fields;
				uint32_t pktflags = PCAPNG_PBF_DIR_OUTBOUND | PCAPNG_PBF_DIR_INBOUND;
				uint32_t pmdflags = PCAPNG_EPB_PMDF_NEW_FLOW | PCAPNG_EPB_PMDF_REXMIT |
					PCAPNG_EPB_PMDF_KEEP_ALIVE | PCAPNG_EPB_PMDF_SOCKET | PCAPNG_EPB_PMDF_NEXUS_CHANNEL;

				pcapng_block_t block = pcap_ng_block_alloc(pcap_ng_block_size_max());
				
				pcap_ng_block_reset(block, PCAPNG_BT_EPB);
				
				if (first_comment && comment)
					pcap_ng_block_add_option_with_string(block, PCAPNG_OPT_COMMENT, comment);
				
				epb_fields = pcap_ng_get_enhanced_packet_fields(block);
				epb_fields->caplen = (bpf_u_int32)len;
				epb_fields->len = (bpf_u_int32)len;
				epb_fields->interface_id = 0;
				epb_fields->timestamp_high = 10000;
				epb_fields->timestamp_low = 2000;
				
				if (copy_data_buffer)
					pcap_ng_block_packet_copy_data(block, data, (bpf_u_int32)len);
				else
					pcap_ng_block_packet_set_data(block, data, (bpf_u_int32)len);
								
				if (proc_name != NULL) {
					pcap_ng_block_add_option_with_value(block, PCAPNG_EPB_PIB_INDEX, &proc_index, sizeof(proc_index));
				}
				if (!first_comment && comment)
					pcap_ng_block_add_option_with_string(block, PCAPNG_OPT_COMMENT, comment);

				pcap_ng_block_add_option_with_value(block, PCAPNG_EPB_FLAGS , &pktflags, 4);

				pcap_ng_block_add_option_with_value(block, PCAPNG_EPB_PMD_FLAGS, &pmdflags, 4);
				
				write_block(block);

				pcap_ng_free_block(block);
				break;
			}
			case PKTAP_PACKET: {
				struct pcap_pkthdr h;
				u_char *buffer = malloc(sizeof(struct pktap_header) + len);
				struct pktap_header *pktp_hdr;
				
				bzero(&h, sizeof(struct pcap_pkthdr));
				h.ts.tv_sec = 10000;
				h.ts.tv_usec = 2000;
				h.caplen = (bpf_u_int32) (sizeof(struct pktap_header) + len);
				h.len = (bpf_u_int32) (sizeof(struct pktap_header) + len);
				
				pktp_hdr = (struct pktap_header *)buffer;
				bzero(pktp_hdr, sizeof(struct pktap_header));
				bcopy(data, pktp_hdr + 1, len);
				pktp_hdr->pth_length = sizeof(struct pktap_header);
				snprintf(pktp_hdr->pth_ifname, sizeof(pktp_hdr->pth_ifname), "%s", if_name);
				pktp_hdr->pth_pid = proc_pid;
				pktp_hdr->pth_epid = -1;
				if (proc_name != NULL) {
					snprintf(pktp_hdr->pth_comm, sizeof(pktp_hdr->pth_comm), "%s", proc_name);
				}
				uuid_copy(pktp_hdr->pth_uuid, proc_uuid);

				if (comment != NULL) {
					if (pcap_ng_dump_pktap_comment(pcap, dumper, &h, buffer, comment) == 0)
						warnx("pcap_ng_dump_pktap() error %s\n", pcap_geterr(pcap));
				} else {
					if (pcap_ng_dump_pktap(pcap, dumper, &h, buffer) == 0)
						warnx("pcap_ng_dump_pktap() error %s\n", pcap_geterr(pcap));
				}
				break;
			}
			default:
				break;
		}
	}
}

void
make_section_header_block()
{
	pcapng_block_t block = pcap_ng_block_alloc(pcap_ng_block_size_max());
	
	if (verbose)
		printf("%s\n", __func__);

	pcap_ng_block_reset(block, PCAPNG_BT_SHB);
	
	pcap_ng_block_add_option_with_string(block, PCAPNG_OPT_COMMENT,
					     "section header block");
	
	write_block(block);
	
	pcap_ng_free_block(block);
}

void
make_name_resolution_record(int af, void *addr, char **names)
{
	pcapng_block_t block = pcap_ng_block_alloc(pcap_ng_block_size_max());

	if (verbose)
		printf("%s\n", __func__);

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
	
	pcap_ng_free_block(block);
}

int
main(int argc, char * const argv[])
{
	int ch;
	const char *file_name = NULL;
	int kevid = 0;
	
    if (argc == 1) {
        help(argv[0]);
        return (0);
    }
    
	/*
	 * Loop through argument to build PCAP-NG block
	 * Optionally write to file
	 */
	while ((ch = getopt(argc, argv, "4:6:Cc:D:d:fk:hi:n:p:t:w:xv")) != -1) {
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
				
				make_data_block(packet_data, packet_length);
				free(packet_data);
				break;
			}
			case 'd':
				packet_length  = strlen(optarg);
				packet_data = (unsigned char *)optarg;
				make_data_block(packet_data, packet_length);
				break;
				
			case 'f':
				first_comment = 1;
				break;

			case 'k': {
				struct kern_event_msg *kevmsg = NULL;
				u_long len;
				
				len = strtoul(optarg, NULL, 0);
				if (len < sizeof(struct kern_event_msg)) {
					fprintf(stderr, "malformed arguments for option 'n'\n");
					help(argv[0]);
					return (0);
				}
				kevmsg = (struct kern_event_msg *)calloc(1, len);
				kevmsg->total_size = (u_int32_t)len;
				kevmsg->vendor_code = KEV_VENDOR_APPLE;
				kevmsg->kev_class = KEV_NETWORK_CLASS;
				kevmsg->kev_subclass = KEV_DL_SUBCLASS;
				kevmsg->id = kevid++;
				kevmsg->event_code = KEV_DL_LINK_QUALITY_METRIC_CHANGED;

				make_kern_event_block(kevmsg);
				free(kevmsg);
				break;
			}
			case 'h':
				help(argv[0]);
				return (0);
				
			case 'i':
				if_name = optarg;
				
				make_interface_description_block(if_name);
				
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

				if (af == AF_INET)
					make_name_resolution_record(af,  &ina, names);
				else
					make_name_resolution_record(af, &in6a, names);
				break;
			}
				
			case 'n': {
				num_data_blocks = strtoul(optarg, NULL, 0);
				break;
			}
				
			case 'p': {
				char *ptr;
				char *tofree;
				
				tofree = strdup(optarg);
				if (tofree == NULL)
					errx(1, "### strdup() failed");
				ptr = tofree;
				
				if (proc_name != NULL) {
					free(proc_name);
					proc_name = NULL;
				}
				proc_pid = -1;
				uuid_clear(proc_uuid);
				
				do {
					char *str;

					if ((str = strsep(&ptr, ":")) == NULL)
						errx(1, "-p argument missing");
					
					if (*str != 0)
						proc_name = strdup(str);
					
					if ((str = strsep(&ptr, ":")) == NULL)
						break;
					if (*str != 0) {
						char *ep;

						proc_pid = (uint32_t)strtoul(str, &ep, 0);
						if (*ep || ep == optarg) {
							fprintf(stderr, "malformed pid for option 'p'\n");
							help(argv[0]);
							exit (0);
						}
					}
					if ((str = strsep(&ptr, ":")) == NULL)
						break;
					if (*str != 0) {
						if (uuid_parse(str, proc_uuid) != 0) {
							fprintf(stderr, "malformed uuid for option 'p'\n");
							help(argv[0]);
							return (0);
						}
					}
				} while (0);
				free(proc_name);
				free(tofree);

				proc_index += 1;
				
				make_process_information_block(proc_name, proc_pid, proc_uuid);
				
				break;
			}
			case 't':
				if (*optarg == 's')
					type_of_packet = SIMPLE_PACKET;
				else if (*optarg == 'e')
					type_of_packet = ENHANCED_PACKET;
				else if (*optarg == 'o')
					type_of_packet = OBSOLETE_PACKET;
				else if (*optarg == 'p')
					type_of_packet = PKTAP_PACKET;
				break;

			case 'v':
				verbose++;
				break;

			case 'w':
				file_name = optarg;
				
				if (dumper != NULL)
					pcap_ng_dump_close(dumper);
				
				if (pcap == NULL) {
					pcap = pcap_open_dead(DLT_PCAPNG, 65536);
					if (pcap == NULL)
						err(EX_OSERR, "pcap_open_dead(DLT_PCAPNG, 65536) failed\n");
				}
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
