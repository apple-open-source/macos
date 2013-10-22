/*
 * Copyright (c) 2013 Apple Inc. All rights reserved.
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

#ifdef HAVE_PKTAP_API

/*
 * Make "pcap.h" not include "pcap/bpf.h"; we are going to include the
 * native OS version, as we need "struct bpf_config" from it.
 */
#define PCAP_DONT_INCLUDE_PCAP_BPF_H

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/bpf.h>
#include <net/pktap.h>
#include <net/iptap.h>
#include <fcntl.h>
#include <errno.h>
#include <libproc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pcap-int.h"
#include "pcap-util.h"

static const char *auto_cloned_if_description = "libpcap auto cloned device";

static int
pcap_get_if_attach_count(const char *ifname)
{
	int fd;
	int n = 0;
	char device[sizeof "/dev/bpf0000000000"];
	struct ifreq ifr;
	int count = -1;
	
	/*
	 * Find an available device
	 */
	do {
		(void)snprintf(device, sizeof(device), "/dev/bpf%d", n++);

		fd = open(device, O_RDONLY);
	} while (fd < 0 && errno == EBUSY);

	if (fd >= 0) {
        bzero(&ifr, sizeof(ifr));
        
        strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
        if (ioctl(fd, BIOCGIFATTACHCOUNT, &ifr) == -1)
            fprintf(stderr, "ioctl BIOCGIFATTACHCOUNT");
        else
            count = ifr.ifr_intval;
		
		close(fd);
	}
	return (count);
}

void
pcap_cleanup_pktap_interface(const char *ifname)
{
    int s = -1;
	struct if_descreq if_descreq;
    struct ifreq ifr;

	/*
	 * Destroy the pktap instance we created
	 */
	if (ifname != NULL) {
		s = socket(AF_INET, SOCK_DGRAM, 0);
		if (s == -1)
			fprintf(stderr, "%s: socket fail - %s\n",
					__func__, strerror(errno));
		else {
            /*
             * Verify it's been cloned by libpcap
             */
            bzero(&if_descreq, sizeof(struct if_descreq));
            strlcpy(if_descreq.ifdr_name, ifname, sizeof(if_descreq.ifdr_name));
            if (ioctl(s, SIOCGIFDESC, &if_descreq) < 0) {
                goto done;
            }
            
            if (if_descreq.ifdr_len == 0)
                goto done;
            if (strcmp((char *)if_descreq.ifdr_desc, auto_cloned_if_description) != 0)
                goto done;

            /*
             * Verify the interface is not already attached to another BPF
             * (and yes, there's a race with this kind of check)
             */
            if (pcap_get_if_attach_count(ifname) != 1)
                goto done;
            
            /*
             * Now we assume it's ours 
             */
			bzero(&ifr, sizeof(struct ifreq));
			strncpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
			if (ioctl(s, SIOCIFDESTROY, &ifr) < 0)
				fprintf(stderr, "%s: ioctl(SIOCIFDESTROY) fail - %s\n",
						__func__, strerror(errno));
		}
	}
done:
    if (s != -1)
        close(s);
}

char *
pcap_setup_pktap_interface(const char *device, char *ebuf)
{
	struct ifreq ifr;
	int s = -1;
	struct if_nameindex *ifnameindices = NULL, *ifnameindex;
	int foundmatch = 0;
	struct if_descreq if_descreq;
	char *pktap_param = NULL;
	int unit = -1;
	const char *if_prefix = NULL;
	char *ifname = NULL;
	
	ifname = calloc(1, PKTAP_IFXNAMESIZE);
	if (ifname == NULL) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "malloc(): %s",
				 pcap_strerror(errno));
		goto fail;
	}
	
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "socket(): %s",
				 pcap_strerror(errno));
		goto fail;
	}
	
	/*
	 * Use a pktap interface to tap on multiple physical interfaces
	 */
	if (strncmp(device, PKTAP_IFNAME, strlen(PKTAP_IFNAME)) == 0) {
		size_t tocopy;
		
		if_prefix = PKTAP_IFNAME;
		/*
		 * The comma marks the optional paramaters
		 */
		pktap_param = strchr(device, ',');
		
		/*
		 * Copy the interface name
		 */
		if (pktap_param != NULL)
			tocopy = pktap_param - device;
		else
			tocopy = strlen(device);
		if (tocopy + 1 > PKTAP_IFXNAMESIZE) {
			snprintf(ebuf, PCAP_ERRBUF_SIZE, "device name too long: %s",
					 pcap_strerror(errno));
			goto fail;
		}
		bcopy(device, ifname, tocopy);
		ifname[tocopy] = 0;
		
		/*
		 * Create a device instance when no unit number is specified
		 */
		sscanf(ifname, PKTAP_IFNAME "%d", &unit);
	} else if (strcmp(device, "all") == 0 || strcmp(device, "any") == 0) {
		if_prefix = PKTAP_IFNAME;
		pktap_param = "all";
		unit = -1;
	} else if (strncmp(device, IPTAP_IFNAME, strlen(IPTAP_IFNAME)) == 0) {
		if_prefix = IPTAP_IFNAME;
		
		/*
		 * Copy the interface name
		 */
		if (strlcpy(ifname, device, PKTAP_IFXNAMESIZE) >= PKTAP_IFXNAMESIZE) {
			snprintf(ebuf, PCAP_ERRBUF_SIZE, "device name too long: %s",
					 pcap_strerror(errno));
			goto fail;
		}
		/*
		 * Create a device instance when no unit number is specified
		 */
		sscanf(ifname, IPTAP_IFNAME "%d", &unit);
	} else {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "bad device name: %s",
				 pcap_strerror(errno));
		goto fail;
	}
	
	if (unit == -1) {
		/*
		 * Check if there is a pktap that was created by libpcap as it was
		 * most likely leaked by a previous crash
		 */
		if ((ifnameindices = if_nameindex()) == NULL) {
			snprintf(ebuf, PCAP_ERRBUF_SIZE, "if_nameindex: %s",
					 pcap_strerror(errno));
			goto fail;
		}
		for (ifnameindex = ifnameindices; ifnameindex->if_index != 0; ifnameindex++) {
			if (strncmp(ifnameindex->if_name, if_prefix, strlen(if_prefix)) != 0)
				continue;
			
			bzero(&if_descreq, sizeof(struct if_descreq));
			strlcpy(if_descreq.ifdr_name, ifnameindex->if_name, sizeof(if_descreq.ifdr_name));
			if (ioctl(s, SIOCGIFDESC, &if_descreq) < 0) {
				snprintf(ebuf, PCAP_ERRBUF_SIZE, "ioctl(SIOCGIFDESC): %s",
						 pcap_strerror(errno));
				goto fail;
			}
			
			if (if_descreq.ifdr_len == 0)
				continue;
			if (strcmp((char *)if_descreq.ifdr_desc, auto_cloned_if_description) != 0)
				continue;
			/*
			 * Verify the interface is not already attached to another BPF
			 * (and yes, there's a race with this kind of check)
			 */
			if (pcap_get_if_attach_count(ifnameindex->if_name) != 0)
				continue;
			
			/*
			 * Keep the name of the matching interface around
			 */
			strlcpy(ifname, ifnameindex->if_name, PKTAP_IFXNAMESIZE);
			
			foundmatch = 1;
			break;
		}
		
		if (foundmatch == 0) {
			/*
			 * We're creating a new instance of a pktap that should be destroyed
			 * before exiting
			 *
			 * Note: we may leak the interface when exiting abnormaly, by
			 * crashing or by not calling pcap_close()
			 */
			memset(&ifr, 0, sizeof(ifr));
			(void) strlcpy(ifr.ifr_name, if_prefix, sizeof(ifr.ifr_name));
			if (ioctl(s, SIOCIFCREATE, &ifr) < 0) {
				snprintf(ebuf, PCAP_ERRBUF_SIZE, "ioctl(SIOCIFCREATE): %s",
						 pcap_strerror(errno));
				goto fail;
			}
			snprintf(ifname, PKTAP_IFXNAMESIZE, "%s", ifr.ifr_name);
			
			/*
			 * Mark the interface as being created by tcpdump
			 */
			bzero(&if_descreq, sizeof(struct if_descreq));
			strncpy(if_descreq.ifdr_name, ifname, sizeof(if_descreq.ifdr_name));
			if_descreq.ifdr_len = strlen(auto_cloned_if_description);
			strncpy((char *)if_descreq.ifdr_desc, auto_cloned_if_description,
					sizeof (if_descreq.ifdr_desc));
			if (ioctl(s, SIOCSIFDESC, &if_descreq) < 0) {
				snprintf(ebuf, PCAP_ERRBUF_SIZE, "ioctl(SIOCSIFDESC): %s",
						 pcap_strerror(errno));
				goto fail;
			}
		}
	}
	
	if (pktap_param != NULL) {
		int num_filter_entries = 0;
		struct pktap_filter pktap_if_filter[PKTAP_MAX_FILTERS];
		
		bzero(pktap_if_filter, sizeof(pktap_if_filter));
		
		/*
		 * The comma separated parameters is a list of interfaces for
		 * pktap to filter on
		 */
		while (*pktap_param != '\0') {
			char *end_ptr;
			struct pktap_filter entry;
			size_t len;
			
			/* This makes sure the strings are zero terminated */
			bzero(&entry, sizeof(struct pktap_filter));
			
			if (*pktap_param == ',') {
				pktap_param++;
				continue;
			}
			if (num_filter_entries >= PKTAP_MAX_FILTERS) {
				snprintf(ebuf, PCAP_ERRBUF_SIZE,
						 "Too many pktap parameters, max is %u", PKTAP_MAX_FILTERS);
				goto fail;
			}
			
			end_ptr = strchr(pktap_param, ',');
			if (end_ptr == NULL)
				len = strlen(pktap_param);
			else
				len = end_ptr - pktap_param;
			
			if (len > sizeof(entry.filter_param_if_name) - 1) {
				snprintf(ebuf, PCAP_ERRBUF_SIZE,
						 "Interface name too big for filter");
				goto fail;
			}
			
			if (strcmp(pktap_param, "all") == 0 || strcmp(pktap_param, "any") == 0) {
				entry.filter_op = PKTAP_FILTER_OP_PASS;
				entry.filter_param = PKTAP_FILTER_PARAM_IF_TYPE;
				entry.filter_param_if_type = 0;
			} else {
				entry.filter_op = PKTAP_FILTER_OP_PASS;
				entry.filter_param = PKTAP_FILTER_PARAM_IF_NAME;
				strncpy(entry.filter_param_if_name, pktap_param,
						MIN(sizeof(entry.filter_param_if_name), len));
			}
			pktap_if_filter[num_filter_entries] = entry;
			num_filter_entries++;
			pktap_param += len;
		}
		
		if (num_filter_entries > 0) {
			struct ifdrv ifdr;
			
			bzero(&ifdr, sizeof(struct ifdrv));
			snprintf(ifdr.ifd_name, sizeof(ifdr.ifd_name), "%s", ifname);
			ifdr.ifd_cmd = PKTP_CMD_FILTER_SET;
			ifdr.ifd_len = sizeof(pktap_if_filter);
			ifdr.ifd_data = &pktap_if_filter[0];
			
			if (ioctl(s, SIOCSDRVSPEC, &ifdr) == -1) {
				snprintf(ebuf, PCAP_ERRBUF_SIZE, "ioctl(SIOCSDRVSPEC): %s",
						 pcap_strerror(errno));
				goto fail;
			}
		}
	}
cleanup:
	if (ifnameindices != NULL)
		if_freenameindex(ifnameindices);
	if (s != -1)
		close(s);
	return (ifname);
		
fail:
	if (ifname != NULL) {
		free(ifname);
		ifname = NULL;
	}
	goto cleanup;;
}

pcap_t *
pcap_create_pktap_device(const char *device, char *ebuf)
{
	pcap_t *p = NULL;
	char *ifname = NULL;
	
	ifname = pcap_setup_pktap_interface(device, ebuf);
	if (ifname != NULL) {
		p = pcap_create_common(ifname, ebuf);
		if (p != NULL)
			p->cleanup_interface_op = pcap_cleanup_pktap_interface;
		else
			pcap_cleanup_pktap_interface(ifname);
		free(ifname);
	}
	
	return (p);
}

/*
 * Returns zero if the packet doesn't match, non-zero if it matches
 */
static int
pcap_filter_pktap(pcap_t *pcap, struct pcap_if_info *if_info,
				  const struct pcap_pkthdr *h, const u_char *sp)
{
	struct pktap_header *pktp_hdr;
	const u_char *pkt_data;
	int match = 0;
	
	pktp_hdr = (struct pktap_header *)sp;
	
	if (h->len < sizeof(struct pktap_header) ||
		h->caplen < sizeof(struct pktap_header) ||
		pktp_hdr->pth_length > h->caplen) {
		snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
				 "%s: Packet too short", __func__);
		return (0);
	}
	
	if (if_info == NULL) {
		if_info = pcap_find_if_info_by_name(pcap, pktp_hdr->pth_ifname);
		/*
		 * New interface
		 */
		if (if_info == NULL) {
			if_info = pcap_add_if_info(pcap, pktp_hdr->pth_ifname, -1,
											pktp_hdr->pth_dlt, pcap->snapshot);
			if (if_info == NULL) {
				snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
						 "%s: pcap_add_if_info(%s) failed",
						 __func__, pktp_hdr->pth_ifname);
				return (0);
			}
		}
	}
	
	if (if_info->if_filter_program.bf_insns == NULL)
		match = 1;
	else {
		/*
		 * The actual data packet is past the packet tap header
		 */
		struct pcap_pkthdr tmp_hdr;
        
		bcopy(h, &tmp_hdr, sizeof(struct pcap_pkthdr));
        
		tmp_hdr.caplen -= pktp_hdr->pth_length;
		tmp_hdr.len -= pktp_hdr->pth_length;
		
		pkt_data = sp + pktp_hdr->pth_length;
        
		match = pcap_offline_filter(&if_info->if_filter_program, &tmp_hdr, pkt_data);
		
	}
	
	return (match);
}

static struct pcap_proc_info *
pcap_ng_dump_proc_info(pcap_t *pcap, pcap_dumper_t *dumper, pcapng_block_t block,
					   pid_t pid, char *pcomm)
{
	int retval;
	struct pcap_proc_info *proc_info;
	
	proc_info = pcap_find_proc_info(pcap, pid, pcomm);
	if (proc_info == NULL) {
		struct pcapng_process_information_fields *pib = NULL;
		
		proc_info = pcap_add_proc_info(pcap, pid, pcomm);
		if (proc_info == NULL) {
			snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
					 "%s: allocate_proc_info(%s) failed",
					 __func__, pcomm);
			return (NULL);
		}
		retval = pcap_ng_block_reset(block, PCAPNG_BT_PIB);
		if (retval != 0) {
			snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
					 "%s: pcap_ng_block_reset(PCAPNG_BT_PIB) failed", __func__);
			return (NULL);
		}
		pib = pcap_ng_get_process_information_fields(block);
		pib->process_id = pid;
		
		if (pcap_ng_block_add_option_with_string(block, PCAPNG_PIB_NAME, pcomm) != 0) {
			snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
					 "%s: pcap_ng_block_add_option_with_string(PCAPNG_PIB_NAME, %s) failed",
					 __func__, pcomm);
			return (NULL);
		}
		
		pcap_ng_dump_block(dumper, block);
	}
	
	return (proc_info);
}

/*
 * To minimize memory allocation we use a single block object that
 * we reuse by calling pcap_ng_block_reset()
 */
int
pcap_ng_dump_pktap(pcap_t *pcap, pcap_dumper_t *dumper,
				   const struct pcap_pkthdr *h, const u_char *sp)
{
	static pcapng_block_t block = NULL;
	struct pktap_header *pktp_hdr;
	const u_char *pkt_data;
	struct pcap_if_info *if_info = NULL;
	struct pcapng_enhanced_packet_fields *epb;
	uint64_t ts;
	struct pcap_proc_info *proc_info = NULL;
	struct pcap_proc_info *e_proc_info = NULL;
	uint32_t pktflags = 0;
	int retval;
	static struct utsname utsname;
	static struct proc_bsdshortinfo bsdinfo;
	static int info_done = 0;

	if (info_done == 0) {
		if (uname(&utsname) == -1) {
			snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
					 "%s: uname() failed", __func__);
			return (0);
		}
		if (proc_pidinfo(getpid(), PROC_PIDT_SHORTBSDINFO, 1, &bsdinfo, sizeof(bsdinfo)) < 0) {
			snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
					 "%s: proc_pidinfo(PROC_PIDT_SHORTBSDINFO) failed", __func__);
			return (0);
		}
		info_done = 1;
	}
    
	pktp_hdr = (struct pktap_header *)sp;
	
	if (h->len < sizeof(struct pktap_header) ||
		h->caplen < sizeof(struct pktap_header) ||
		pktp_hdr->pth_length > h->caplen) {
		snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
				 "%s: Packet too short", __func__);
		return (0);
	}
	
    if (block == NULL) {
        block = pcap_ng_block_alloc(65536);
        if (block == NULL) {
            snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
					 "%s: pcap_ng_block_alloc() failed ", __func__);
            return (0);
        }
    }
	
	/*
	 * Add a section header block when needed
	 */
	if (pcap->shb_added == 0) {
		char buf[256];
		
		retval = pcap_ng_block_reset(block, PCAPNG_BT_SHB);
		if (retval != 0) {
			snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
					 "%s: pcap_ng_block_reset(PCAPNG_BT_SHB) failed", __func__);
			return (0);
		}
		retval = pcap_ng_block_add_option_with_string(block, PCAPNG_OPT_COMMENT,
													  "section header block");
		if(retval != 0) {
			snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
					 "%s: pcap_ng_block_add_option_with_string(PCAPNG_OPT_COMMENT) failed", __func__);
			return (0);
		}
		
		retval = pcap_ng_block_add_option_with_string(block, PCAPNG_SHB_HARDWARE,
													  utsname.machine);
		if(retval != 0) {
			snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
					 "%s: pcap_ng_block_add_option_with_string(PCAPNG_SHB_HARDWARE) failed", __func__);
			return (0);
		}
		
		snprintf(buf, sizeof(buf), "%s %s", utsname.sysname, utsname.release);
		retval = pcap_ng_block_add_option_with_string(block, PCAPNG_SHB_OS, buf);
		if(retval != 0) {
			snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
					 "%s: pcap_ng_block_add_option_with_string(PCAPNG_SHB_OS) failed", __func__);
			return (0);
		}
		
		snprintf(buf, sizeof(buf), "%s (%s)", bsdinfo.pbsi_comm, pcap_lib_version());
		retval = pcap_ng_block_add_option_with_string(block, PCAPNG_SHB_USERAPPL, buf);
		if(retval != 0) {
			snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
					 "%s: pcap_ng_block_add_option_with_string(PCAPNG_SHB_USERAPPL) failed", __func__);
			return (0);
		}

		pcap_ng_dump_block(dumper, block);
		
		pcap->shb_added = 1;
	}
	
	/*
	 * Add an interface info block for a new interface
	 */
	if_info = pcap_find_if_info_by_name(pcap, pktp_hdr->pth_ifname);
	if (if_info == NULL) {
		if_info = pcap_add_if_info(pcap, pktp_hdr->pth_ifname, -1,
								   pktp_hdr->pth_dlt, pcap->snapshot);
		if (if_info == NULL) {
			snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
					 "%s: pcap_add_if_info(%s) failed",
					 __func__, pktp_hdr->pth_ifname);
			return (0);
		}
	}
	
	/*
	 * Need to add the interface info block to avoid reference to
	 * missing interfaces caused by filtering
	 */
	if (if_info->if_block_added == 0) {
		struct pcapng_interface_description_fields *idb = NULL;
		
		retval = pcap_ng_block_reset(block, PCAPNG_BT_IDB);
		if (retval != 0) {
			snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
					 "%s: pcap_ng_block_reset(PCAPNG_BT_IDB) failed", __func__);
			return (0);
		}
		idb = pcap_ng_get_interface_description_fields(block);
		idb->linktype = dlt_to_linktype(pktp_hdr->pth_dlt);
		idb->snaplen = pcap->snapshot;
		
		if (pcap_ng_block_add_option_with_string(block, PCAPNG_IF_NAME,
												 pktp_hdr->pth_ifname) != 0) {
			snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
					 "%s: pcap_ng_block_add_option_with_string(PCAPNG_IF_NAME, %s) failed",
					 __func__, pktp_hdr->pth_ifname);
			return (0);
		}
		
		pcap_ng_dump_block(dumper, block);
		
		if_info->if_block_added = 1;
	}

	/*
	 * Check the packet matches the filter
	 */
	if (pcap_filter_pktap(pcap, if_info, h, sp) == 0)
		return (0);
	
	if (pktp_hdr->pth_pid != -1 && pktp_hdr->pth_comm[0] != 0) {
		proc_info = pcap_ng_dump_proc_info(pcap, dumper, block,
										   pktp_hdr->pth_pid, pktp_hdr->pth_comm);
		if (proc_info == NULL)
			return (0);
	}
	if (pktp_hdr->pth_epid != -1 && pktp_hdr->pth_ecomm[0] != 0) {
		e_proc_info = pcap_ng_dump_proc_info(pcap, dumper, block,
										   pktp_hdr->pth_epid, pktp_hdr->pth_ecomm);
		if (e_proc_info == NULL)
			return (0);
	}
    
    retval = pcap_ng_block_reset(block, PCAPNG_BT_EPB);
    if (retval != 0) {
		snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
				 "%s: pcap_ng_block_reset(PCAPNG_BT_EPB) failed", __func__);
		return (0);
	}
	/*
	 * The actual data packet is past the packet tap header
	 */
	pkt_data = sp + pktp_hdr->pth_length;
	epb = pcap_ng_get_enhanced_packet_fields(block);
	epb->caplen = h->caplen - pktp_hdr->pth_length;
	epb->interface_id = if_info->if_id;
	epb->len = h->len - pktp_hdr->pth_length;
	/* Microsecond resolution */
	ts = ((uint64_t)h->ts.tv_sec) * 1000000 + (uint64_t)h->ts.tv_usec;
	epb->timestamp_high = ts >> 32;
	epb->timestamp_low  = ts & 0xffffffff;
	
	pcap_ng_block_packet_set_data(block, pkt_data, epb->caplen);
	
	if (proc_info != NULL)
		pcap_ng_block_add_option_with_value(block, PCAPNG_EPB_PIB_INDEX, &proc_info->proc_index, 4);
	if (e_proc_info != NULL)
		pcap_ng_block_add_option_with_value(block, PCAPNG_EPB_E_PIB_INDEX, &e_proc_info->proc_index, 4);
	
	if ((pktp_hdr->pth_flags & PTH_FLAG_DIR_IN))
		pktflags = 0x01;
	else if ((pktp_hdr->pth_flags & PTH_FLAG_DIR_OUT))
		pktflags = 0x02;
	if (pktflags != 0)
		pcap_ng_block_add_option_with_value(block, PCAPNG_EPB_FLAGS , &pktflags, 4);
	
	if (pktp_hdr->pth_svc != -1)
		pcap_ng_block_add_option_with_value(block, PCAPNG_EPB_SVC , &pktp_hdr->pth_svc, 4);
	
	pcap_ng_dump_block(dumper, block);
	
	return (1);
}

#endif /* HAVE_PKTAP_API */
