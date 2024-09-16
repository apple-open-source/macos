/*
 * Copyright (c) 2013-2023 Apple Inc. All rights reserved.
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

/*
 * Make "pcap.h" not include "pcap/bpf.h"; we are going to include the
 * native OS version, as we need "struct bpf_config" from it.
 */
#define PCAP_DONT_INCLUDE_PCAP_BPF_H

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/kern_event.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/uio.h>
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
#include <stddef.h>
#include <assert.h>

#include "pcap-int.h"
#include "pcap-util.h"
#include "pcap-pktap.h"

static int pcap_cleanup_pktap_interface_internal(const char *ifname, char *ebuf);

/*
 * We append the procname + PID to the description
 */
#define AUTO_CLONE_IF_DESCRIPTION "libpcap auto cloned device"
#define AUTO_CLONE_IF_DESC_LEN (sizeof(AUTO_CLONE_IF_DESCRIPTION) -1)

#define _CASSERT(x) _Static_assert(x, "compile-time assertion failed " #x)

_CASSERT(offsetof(struct bpf_hdr_ext, bh_tstamp) == offsetof(struct bpf_hdr, bh_tstamp));
_CASSERT(offsetof(struct bpf_hdr_ext, bh_caplen) == offsetof(struct bpf_hdr, bh_caplen));
_CASSERT(offsetof(struct bpf_hdr_ext, bh_datalen) == offsetof(struct bpf_hdr, bh_datalen));
_CASSERT(offsetof(struct bpf_hdr_ext, bh_hdrlen) == offsetof(struct bpf_hdr, bh_hdrlen));
_CASSERT(MAXIMUM_SNAPLEN == BPF_MAXBUFSIZE);


static int
pcap_get_if_attach_count(const char *ifname, char *errbuf)
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
		if (ioctl(fd, BIOCGIFATTACHCOUNT, &ifr) == -1) {
            snprintf(errbuf, PCAP_ERRBUF_SIZE, "ioctl BIOCGIFATTACHCOUNT %s failed - %s",
					 ifname, strerror(errno));
		} else {
            count = ifr.ifr_intval;
		}
		close(fd);
	}
	return (count);
}

static int
pcap_cleanup_pktap_interface_internal(const char *ifname, char *ebuf)
{
    int s = -1;
	struct if_descreq if_descreq;
    struct ifreq ifr;
	int status = 0;

	/*
	 * Destroy the pktap instance we created
	 */
	if (ifname != NULL) {
		s = socket(AF_INET, SOCK_DGRAM, 0);
		if (s == -1) {
			snprintf(ebuf, PCAP_ERRBUF_SIZE, "%s: socket failed - %s",
					__func__, strerror(errno));
			goto failed;
		} else {
            /*
             * Verify it's been cloned by libpcap
             */
            bzero(&if_descreq, sizeof(struct if_descreq));
            strlcpy(if_descreq.ifdr_name, ifname, sizeof(if_descreq.ifdr_name));
            if (ioctl(s, SIOCGIFDESC, &if_descreq) < 0) {
				snprintf(ebuf, PCAP_ERRBUF_SIZE, "%s: ioctl SIOCGIFDESC %s - %s",
						 __func__, ifname, strerror(errno));
				goto failed;
            }
			if (if_descreq.ifdr_len == 0) {
                goto done;
			}
            if (strncmp((char *)if_descreq.ifdr_desc, AUTO_CLONE_IF_DESCRIPTION,
						AUTO_CLONE_IF_DESC_LEN) != 0) {
                goto done;
			}
            /*
             * Verify the interface is not already attached to another BPF
             * (and yes, there's a race with this kind of check)
             */
			if (pcap_get_if_attach_count(ifname, ebuf) != 1) {
                goto done;
			}
            /*
             * Now we assume it's ours 
             */
			bzero(&ifr, sizeof(struct ifreq));
			strlcpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
			if (ioctl(s, SIOCIFDESTROY, &ifr) < 0) {
				snprintf(ebuf, PCAP_ERRBUF_SIZE, "%s: ioctl(SIOCIFDESTROY) fail - %s",
						__func__, strerror(errno));
				goto failed;
			}
		}
	}
done:
	if (s != -1) {
        close(s);
	}
	return (status);
failed:
	status = -1;
	goto done;
}

void
pcap_cleanup_pktap_interface(const char *ifname)
{
	char errbuf[PCAP_ERRBUF_SIZE];

	if (pcap_cleanup_pktap_interface_internal(ifname, errbuf) != 0) {
		fprintf(stderr, "%s\n", errbuf);
	}
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
	} else if (strncmp(device, DROPTAP_IFNAME, strlen(DROPTAP_IFNAME)) == 0) {
		if_prefix = DROPTAP_IFNAME;

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
		sscanf(ifname, DROPTAP_IFNAME "%d", &unit);
	} else {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "bad device name: %s",
				 pcap_strerror(errno));
		goto fail;
	}
	
	if (unit == -1) {
		int desclen;
		
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
			if (strncmp((const char *)if_descreq.ifdr_desc, AUTO_CLONE_IF_DESCRIPTION,
				    AUTO_CLONE_IF_DESC_LEN) != 0)
				continue;
			/*
			 * Verify the interface is not already attached to another BPF
			 * (and yes, there's a race with this kind of check)
			 */
			if (pcap_get_if_attach_count(ifnameindex->if_name, ebuf) != 0) {
				/*
				 * Ignore the error
				 */
				ebuf[0] = 0;
				continue;
			}
			/*
			 * Keep the name of the matching interface around
			 */
			strlcpy(ifname, ifnameindex->if_name, PKTAP_IFXNAMESIZE);
			
			foundmatch = 1;
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
		}
		
		/*
		 * Mark the interface as being created by libpcap along with
		 * the current process name + pid
		 */
		bzero(&if_descreq, sizeof(struct if_descreq));
		strlcpy(if_descreq.ifdr_name, ifname, sizeof(if_descreq.ifdr_name));
		
		desclen = snprintf((char *)if_descreq.ifdr_desc, sizeof (if_descreq.ifdr_desc),
			 "%s - %s.%d", AUTO_CLONE_IF_DESCRIPTION, getprogname(), getpid());
		if (desclen < sizeof(if_descreq.ifdr_desc))
			if_descreq.ifdr_len = desclen + 1;
		else
			if_descreq.ifdr_len = sizeof(if_descreq.ifdr_desc);
		
		if (ioctl(s, SIOCSIFDESC, &if_descreq) < 0) {
			snprintf(ebuf, PCAP_ERRBUF_SIZE, "ioctl(SIOCSIFDESC): %s",
					 pcap_strerror(errno));
			goto fail;
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
				/*
				 * filter_param_if_name is not a zero terminated string so
				 * do not use strlcpy(3)
				 */
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

void
pktap_cleanup(pcap_t *p)
{
	char errbuf[PCAP_ERRBUF_SIZE];

	if (p->cleanup_interface_op != NULL)
		p->cleanup_interface_op(p->opt.device, errbuf);

	p->pktap_cleanup_op(p);

	if (p->pktap_ifname != NULL) {
		free(p->pktap_ifname);
		p->pktap_ifname = NULL;
	}
}

static int
pktap_activate(pcap_t *p)
{
	int status = 0;

	free(p->opt.device);
	p->opt.device = p->pktap_ifname;
	p->pktap_ifname = NULL;

	/*
	 * Just like pcap_set_snaplen() turn invalid value into the max value
	 * The snapshot must be adjusted before calling the actual callback
	 */
	if (p->snapshot < sizeof(struct pktap_header)) {
		p->snapshot = MAXIMUM_SNAPLEN;
	}

	status = p->pktap_activate_op(p);
	if (status != 0)
		return (status);

	p->pktap_cleanup_op = p->cleanup_op;
	p->cleanup_op = pktap_cleanup;

    return (status);
}

pcap_t *
pktap_create(const char *device, char *ebuf, int *is_ours)
{
	pcap_t *p = NULL;
	char *ifname = NULL;

	/*
	 * By default, when device is NULL, we use a pktap
	 * to capture on physical interfaces (exclude loopback and
	 * virtual and tunner interfaces).
	 *
	 * To capture on all interfaces, device can be either "any",
	 * or "all" or even "pktap,all"
	 */
	if (device == NULL)
		device = "pktap";
	if (strncmp(device, PKTAP_IFNAME, strlen(PKTAP_IFNAME)) != 0 &&
		strncmp(device, IPTAP_IFNAME, strlen(IPTAP_IFNAME)) != 0 &&
		strncmp(device, DROPTAP_IFNAME, strlen(DROPTAP_IFNAME)) != 0 &&
		strcmp(device, "all") != 0 &&
		strcmp(device, "any") != 0) {
		*is_ours = 0;
		return (NULL);
	}
	*is_ours = 1;

	/*
	 * Create a regular BPF network interface.
	 */
	p = pcap_create_interface(ebuf, 0);
	if (p == NULL)
		goto failed;

	ifname = pcap_setup_pktap_interface(device, ebuf);
	if (ifname == NULL)
		goto failed;
	p->pktap_ifname = ifname;
	p->cleanup_interface_op = pcap_cleanup_pktap_interface_internal;

	p->pktap_activate_op = p->activate_op;
	p->activate_op = pktap_activate;


	return (p);

failed:
	if (p != NULL)
		pcap_close(p);
	if (ifname != NULL)
		pcap_cleanup_pktap_interface_internal(ifname, ebuf);

	return (NULL);
}

/*
 * Returns zero if the packet doesn't match, non-zero if it matches
 */
static int
pcap_filter_pktap(pcap_t *pcap, pcap_dumper_t *dumper, struct pcap_if_info *if_info,
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
		if_info = pcap_if_info_set_find_by_name(&dumper->dump_if_info_set, pktp_hdr->pth_ifname);
		/*
		 * New interface
		 */
		if (if_info == NULL) {
			if_info = pcap_if_info_set_add(&dumper->dump_if_info_set, pktp_hdr->pth_ifname, -1,
						       pktp_hdr->pth_dlt, pcap->snapshot,
						       pcap->filter_str, pcap->errbuf);
			if (if_info == NULL) {
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

/*
 * Add a section header block when needed
 */
int
pcap_ng_dump_shb_comment(pcap_t *pcap, pcap_dumper_t *dumper, const char *comment)
{
	pcapng_block_t block = NULL;
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
	
	if (dumper->dump_block == NULL) {
		/*
		 * The snaplen represent the maximum length of the data so
		 * 4 KBytes should be more than enough to fit the block header, fields,
		 * options and trailer.
		 */
		dumper->dump_block = pcap_ng_block_alloc(pcap->snapshot + 4096);
		if (dumper->dump_block == NULL) {
			snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
				 "%s: pcap_ng_block_alloc() failed ", __func__);
			return (0);
		}
	}
	block = dumper->dump_block;

	if (pcap->shb_added == 0 || dumper->shb_added == 0) {
		char buf[256];
		
		retval = pcap_ng_block_reset(block, PCAPNG_BT_SHB);
		if (retval != 0) {
			snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
				 "%s: pcap_ng_block_reset(PCAPNG_BT_SHB) failed", __func__);
			return (0);
		}
		
		retval = pcap_ng_block_add_option_with_string(block, PCAPNG_SHB_HARDWARE,
							      utsname.machine);
		if(retval != 0) {
			snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
				 "%s: pcap_ng_block_add_option_with_string(PCAPNG_SHB_HARDWARE) failed", __func__);
			return (0);
		}
		
		snprintf(buf, sizeof(buf), "%s", utsname.version);
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

		if (comment != NULL && *comment != 0) {
			retval = pcap_ng_block_add_option_with_string(block, PCAPNG_OPT_COMMENT, comment);
			if(retval != 0) {
				snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
					 "%s: pcap_ng_block_add_option_with_string(PCAPNG_OPT_COMMENT) failed", __func__);
				return (0);
			}
		}

		(void) pcap_ng_dump_block(dumper, block);
		
		pcap->shb_added = 1;
		dumper->shb_added = 1;
	}
	return (1);
}

int
pcap_ng_dump_shb(pcap_t *pcap, pcap_dumper_t *dumper)
{
	return pcap_ng_dump_shb_comment(pcap, dumper, NULL);
}

struct pcap_proc_info *
pcap_ng_dump_proc_info(pcap_t *pcap, pcap_dumper_t *dumper, pcapng_block_t block,
		       struct pcap_proc_info *proc_info)
{
	int retval;
	struct pcapng_process_information_fields *pib;

	/*
	 * We're done when the process info block has already been saved
	 */
	if (proc_info->proc_block_dumped != 0)
		return (proc_info);
	
	retval = pcap_ng_block_reset(block, PCAPNG_BT_PIB);
	if (retval != 0) {
		snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
			 "%s: pcap_ng_block_reset(PCAPNG_BT_PIB) failed", __func__);
		return (NULL);
	}
	pib = pcap_ng_get_process_information_fields(block);
	pib->process_id = proc_info->proc_pid;
	
	if (pcap_ng_block_add_option_with_string(block, PCAPNG_PIB_NAME, proc_info->proc_name) != 0) {
		snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
			 "%s: pcap_ng_block_add_option_with_string(PCAPNG_PIB_NAME, %s) failed",
			 __func__, proc_info->proc_name);
		return (NULL);
	}

	if (uuid_is_null(proc_info->proc_uuid) == 0) {
		if (pcap_ng_block_add_option_with_uuid(block, PCAPNG_PIB_UUID, proc_info->proc_uuid) != 0) {
			snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
				 "%s: pcap_ng_block_add_option_with_uuid(PCAPNG_PIB_UUID) failed",
				 __func__);
			return (NULL);
		}
	}

	(void) pcap_ng_dump_block(dumper, block);
	
	proc_info->proc_block_dumped = 1;
	proc_info->proc_dump_index = dumper->dump_proc_info_set.proc_dump_index++;
	
	return (proc_info);
}

static struct pcap_proc_info *
pcap_ng_dump_proc(pcap_t *pcap, pcap_dumper_t *dumper, pcapng_block_t block,
		  pid_t pid, const char *pcomm, const uuid_t uu)
{
	struct pcap_proc_info *proc_info;
	
	/*
	 * Add a process info block if needed
	 */
	proc_info = pcap_proc_info_set_find_uuid(&dumper->dump_proc_info_set, pid, pcomm, uu);
	if (proc_info == NULL) {
		proc_info = pcap_proc_info_set_add_uuid(&dumper->dump_proc_info_set, pid, pcomm,
							uu, pcap->errbuf);
		if (proc_info == NULL) {
			return (NULL);
		}
	}
	
	proc_info = pcap_ng_dump_proc_info(pcap, dumper, block, proc_info);
	
	return (proc_info);
}

int
pcap_ng_dump_kern_event(pcap_t *pcap, pcap_dumper_t *dumper,
		       struct kern_event_msg *kev, struct timeval *ts)
{
	int retval;
	pcapng_block_t block = NULL;
	struct pcapng_os_event_fields *osev_fields;

	if (pcap_ng_dump_shb(pcap, dumper) == 0)
		return (0);
	
	block = dumper->dump_block;

	retval = pcap_ng_block_reset(block, PCAPNG_BT_OSEV);
	if (retval != 0) {
		snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
			 "%s: pcap_ng_block_reset(PCAPNG_BT_OSEV) failed", __func__);
		return (0);
	}
	osev_fields = pcap_ng_get_os_event_fields(block);
	osev_fields->type = PCAPNG_OSEV_KEV;
	osev_fields->timestamp_high = (u_int32_t)ts->tv_sec;
	osev_fields->timestamp_low = ts->tv_usec;
	osev_fields->len = kev->total_size;
	pcap_ng_block_packet_set_data(block, kev, kev->total_size);

	(void) pcap_ng_dump_block(dumper, block);
	
	return (1);
}

struct pcap_if_info *
pcap_ng_dump_if_info(pcap_t *pcap, pcap_dumper_t *dumper, pcapng_block_t block,
		     struct pcap_if_info *if_info)
{
	int retval;
	struct pcapng_interface_description_fields *idb = NULL;
	
	/*
	 * We're done when the interface block has already been saved
	 */
	if (if_info->if_block_dumped != 0) {
		return (if_info);
	}
	
	retval = pcap_ng_block_reset(block, PCAPNG_BT_IDB);
	if (retval != 0) {
		snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
			 "%s: pcap_ng_block_reset(PCAPNG_BT_IDB) failed", __func__);
		return (0);
	}
	idb = pcap_ng_get_interface_description_fields(block);
	idb->idb_linktype = dlt_to_linktype(if_info->if_linktype);
	idb->idb_snaplen = if_info->if_snaplen;
	
	if (pcap_ng_block_add_option_with_string(block, PCAPNG_IF_NAME,
						 if_info->if_name) != 0) {
		snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
			 "%s: pcap_ng_block_add_option_with_string(PCAPNG_IF_NAME, %s) failed",
			 __func__, if_info->if_name);
		return (0);
	}
	
	(void) pcap_ng_dump_block(dumper, block);
	
	if_info->if_block_dumped = 1;
	if_info->if_dump_id = dumper->dump_if_info_set.if_dump_id++;
	
	return (if_info);
}

/*
 * To minimize memory allocation we use a single block object that
 * we reuse by calling pcap_ng_block_reset()
 */
int
pcap_ng_dump_pktap_comment(pcap_t *pcap, pcap_dumper_t *dumper,
			   const struct pcap_pkthdr *h, const u_char *sp,
			   const char *comment)
{
	pcapng_block_t block = NULL;
	struct pktap_header *pktp_hdr;
	const u_char *pkt_data;
	struct pcap_if_info *if_info = NULL;
	struct pcapng_enhanced_packet_fields *epb;
	uint64_t ts;
	struct pcap_proc_info *proc_info = NULL;
	struct pcap_proc_info *e_proc_info = NULL;
	uint32_t pktflags = 0;
	uint32_t pmdflags = 0;
	int retval;
	
	pktp_hdr = (struct pktap_header *)sp;
	
	if (h->len < sizeof(struct pktap_header) ||
	    h->caplen < sizeof(struct pktap_header) ||
	    pktp_hdr->pth_length > h->caplen) {
		snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
			 "%s: Packet too short", __func__);
		return (0);
	}
	
	if (pcap_ng_dump_shb(pcap, dumper) == 0)
		return (0);
	
	block = dumper->dump_block;
	
	/*
	 * Add an interface info block for a new interface before filtering
	 */
	if_info = pcap_if_info_set_find_by_name(&dumper->dump_if_info_set, pktp_hdr->pth_ifname);
	if (if_info == NULL) {
		if_info = pcap_if_info_set_add(&dumper->dump_if_info_set, pktp_hdr->pth_ifname, -1,
					       pktp_hdr->pth_dlt, pcap->snapshot,
					       pcap->filter_str, pcap->errbuf);
		if (if_info == NULL) {
			return (0);
		}
	}
	
	/*
	 * Check the packet matches the filter
	 */
	if (pcap_filter_pktap(pcap, dumper, if_info, h, sp) == 0)
		return (0);
	
	/*
	 * Dump the interface info block (if needed)
	 */
	if_info = pcap_ng_dump_if_info(pcap, dumper, block, if_info);
	if (if_info == NULL) {
		return (0);
	}
	
	if ((pktp_hdr->pth_pid != -1 && pktp_hdr->pth_pid != 0) ||
	    pktp_hdr->pth_comm[0] != 0 || uuid_is_null(pktp_hdr->pth_uuid) == 0) {
		proc_info = pcap_ng_dump_proc(pcap, dumper, block,
					      pktp_hdr->pth_pid, pktp_hdr->pth_comm, pktp_hdr->pth_uuid);
		if (proc_info == NULL)
			return (0);
	}
	if ((pktp_hdr->pth_epid != -1 && pktp_hdr->pth_epid != 0) ||
	    pktp_hdr->pth_ecomm[0] != 0 || uuid_is_null(pktp_hdr->pth_euuid) == 0) {
		e_proc_info = pcap_ng_dump_proc(pcap, dumper, block,
						pktp_hdr->pth_epid, pktp_hdr->pth_ecomm, pktp_hdr->pth_euuid);
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
	if (pktp_hdr->pth_type_next == PTH_TYPE_DROP) {
		pkt_data = sp + DROPTAP_HDR_SIZE((struct droptap_header *)sp);
	} else {
		pkt_data = sp + pktp_hdr->pth_length;
	}
	epb = pcap_ng_get_enhanced_packet_fields(block);
	epb->caplen = h->caplen - pktp_hdr->pth_length;
	epb->interface_id = if_info->if_dump_id;
	epb->len = h->len - pktp_hdr->pth_length;
	/* Microsecond resolution */
	ts = ((uint64_t)h->ts.tv_sec) * 1000000 + (uint64_t)h->ts.tv_usec;
	epb->timestamp_high = ts >> 32;
	epb->timestamp_low  = ts & 0xffffffff;
	
	pcap_ng_block_packet_set_data(block, pkt_data, epb->caplen);
	
	if (proc_info != NULL) {
		pcap_ng_block_add_option_with_value(block, PCAPNG_EPB_PIB_INDEX, &proc_info->proc_dump_index, 4);
	}
	if (e_proc_info != NULL) {
		pcap_ng_block_add_option_with_value(block, PCAPNG_EPB_E_PIB_INDEX, &e_proc_info->proc_dump_index, 4);
	}
	if ((pktp_hdr->pth_flags & PTH_FLAG_DIR_IN)) {
		pktflags = PCAPNG_PBF_DIR_INBOUND;
	} else if ((pktp_hdr->pth_flags & PTH_FLAG_DIR_OUT)) {
		pktflags = PCAPNG_PBF_DIR_OUTBOUND;
	}
	if (pktflags != 0) {
		pcap_ng_block_add_option_with_value(block, PCAPNG_EPB_FLAGS , &pktflags, 4);
	}
	if (pktp_hdr->pth_svc != -1) {
		pcap_ng_block_add_option_with_value(block, PCAPNG_EPB_SVC , &pktp_hdr->pth_svc, 4);
	}
	if (pktp_hdr->pth_flowid != 0) {
		pcap_ng_block_add_option_with_value(block, PCAPNG_EPB_FLOW_ID , &pktp_hdr->pth_flowid, 4);
	}
	if (pktp_hdr->pth_trace_tag != 0) {
		pcap_ng_block_add_option_with_value(block, PCAPNG_EPB_TRACE_TAG , &pktp_hdr->pth_trace_tag, 2);
	}
	if (pktp_hdr->pth_type_next == PTH_TYPE_DROP) {
		pcap_ng_block_add_option_with_value(block, PCAPNG_EPB_DROP_REASON, &((struct droptap_header *)pktp_hdr)->dth_dropreason, 4);
		/*
		 * dth_dropfunc_size is 0 if we did not store function name and
		 * line number.
		 */
		if (((struct droptap_header *)pktp_hdr)->dth_dropfunc_size > 0) {
			pcap_ng_block_add_option_with_value(block, PCAPNG_EPB_DROP_LINE, &((struct droptap_header *)pktp_hdr)->dth_dropline, 2);
			pcap_ng_block_add_option_with_string(block, PCAPNG_EPB_DROP_FUNC, ((struct droptap_header *)pktp_hdr)->dth_dropfunc);
		}
	}
	if (comment != NULL && *comment != 0) {
		pcap_ng_block_add_option_with_string(block, PCAPNG_OPT_COMMENT, comment);
	}
	if (pktp_hdr->pth_flags & PTH_FLAG_NEW_FLOW) {
		pmdflags |= PCAPNG_EPB_PMDF_NEW_FLOW;
	}
	if (pktp_hdr->pth_flags & PTH_FLAG_REXMIT) {
		pmdflags |= PCAPNG_EPB_PMDF_REXMIT;
	}
	if (pktp_hdr->pth_flags & PTH_FLAG_KEEP_ALIVE) {
		pmdflags |= PCAPNG_EPB_PMDF_KEEP_ALIVE;
	}
	if (pktp_hdr->pth_flags & PTH_FLAG_SOCKET) {
		pmdflags |= PCAPNG_EPB_PMDF_SOCKET;
	}
	if (pktp_hdr->pth_flags & PTH_FLAG_NEXUS_CHAN) {
		pmdflags |= PCAPNG_EPB_PMDF_NEXUS_CHANNEL;
	}
	if (pktp_hdr->pth_flags & PTH_FLAG_WAKE_PKT) {
		pmdflags |= PCAPNG_EPB_PMDF_WAKE_PKT;
	}
	if (pmdflags != 0) {
		pcap_ng_block_add_option_with_value(block, PCAPNG_EPB_PMD_FLAGS, &pmdflags, 4);
	}

	(void) pcap_ng_dump_block(dumper, block);
	
	return (1);
}

int
pcap_ng_dump_pktap(pcap_t *pcap, pcap_dumper_t *dumper,
		   const struct pcap_pkthdr *h, const u_char *sp)
{
	return (pcap_ng_dump_pktap_comment(pcap, dumper, h, sp, NULL));
}

int
pcap_ng_dump_decryption_secrets(pcap_t *pcap, pcap_dumper_t *dumper,
				const uint32_t type, const size_t len, const uint8_t *sp)
{
	int retval;
	pcapng_block_t block = NULL;
	struct pcapng_decryption_secrets_fields *dsb_fields;

	if (len > UINT32_MAX) {
		return 0;
	}

	if (pcap_ng_dump_shb(pcap, dumper) == 0)
		return (0);

	block = dumper->dump_block;

	retval = pcap_ng_block_reset(block, PCAPNG_BT_DSB);
	if (retval != 0) {
		snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
			 "%s: pcap_ng_block_reset(PCAPNG_BT_DSB) failed", __func__);
		return (0);
	}
	dsb_fields = pcap_ng_get_decryption_secrets_fields(block);
	dsb_fields->secrets_type = type;
	dsb_fields->secrets_length = (uint32_t)len;
	pcap_ng_block_packet_set_data(block, sp, (uint32_t)len);

	(void) pcap_ng_dump_block(dumper, block);

	return (1);
}

int
pcap_apple_set_exthdr(pcap_t *p, int v)
{
	int status = -1;

	if (ioctl(p->fd, BIOCSEXTHDR, (caddr_t)&v) < 0) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "BIOCSEXTHDR: %s",
			 pcap_strerror(errno));
		status = PCAP_ERROR;
	} else {
		p->extendedhdr = v;
		status = 0;
	}

	return (status);
}

int
pcap_set_want_pktap(pcap_t *p, int v)
{
	p->wantpktap = !!v;

	return (0);
}

int
pcap_set_truncation_mode(pcap_t *p, bool on)
{
	int status = PCAP_ERROR;

	p->truncation = on;
	status = 0;

	return (status);
}

int
pcap_set_pktap_hdr_v2(pcap_t *p, bool on)
{
	int status = PCAP_ERROR;

	p->pktaphdrv2 = on;
	status = 0;

	return (status);
}

int
pcap_set_compression(pcap_t *p, int v)
{
	int status = PCAP_ERROR;

	if (v >= 0 && v <= 2) {
		p->compression_mode = v;
		status = 0;
	}

	return status;
}

int
pcap_set_head_drop(pcap_t *p, int v)
{
	p->head_drop = !!v;

	return 0;
}

int
pcap_get_head_drop(pcap_t *p)
{
	return p->head_drop;
}

int
pcap_get_compression_stats(pcap_t *p, void *bufp, size_t buflen)
{
	if (p->compression_mode == 1) {
		struct bpf_comp_stats bcs;

		if (ioctl(p->fd, BIOCGHDRCOMPSTATS, (caddr_t)&bcs) < 0) {
			pcap_fmt_errmsg_for_errno(p->errbuf, PCAP_ERRBUF_SIZE,
									  errno, "BIOCGHDRCOMPSTATS");
			return PCAP_ERROR;
		}

		(void)snprintf(bufp, buflen,
					   "K tot_rd %llu tot_sz %llu tot_hdr_sz %llu no_prfx_cnt %llu prfx_cnt %llu tot_prfx_sz %llu max_prfx_sz %llu",
					   bcs.bcs_total_read,
					   bcs.bcs_total_size,
					   bcs.bcs_total_hdr_size,
					   bcs.bcs_count_no_common_prefix,
					   bcs.bcs_count_compressed_prefix,
					   bcs.bcs_total_compressed_prefix_size,
					   bcs.bcs_max_compressed_prefix_size);
		return 0;
	} else if (p->compression_mode == 2) {
		(void)snprintf(bufp, buflen,
					   "U tot_rd %llu tot_sz %llu tot_hdr_sz %llu no_prfx_cnt %llu prfx_cnt %llu tot_prfx_sz %llu max_prfx_sz %u",
					   p->total_read,
					   p->total_size,
					   p->total_hdr_size,
					   p->count_no_common_prefix,
					   p->count_common_prefix,
					   p->total_common_prefix_size,
					   p->max_common_prefix_size);
		return 0;
	}
	return -1;
}

static char *
pcap_svc2str(uint32_t svc)
{
	static char svcstr[10];

	switch (svc) {
		case SO_TC_BK_SYS:
			return "BK_SYS";
		case SO_TC_BK:
			return "BK";
		case SO_TC_BE:
			return "BE";
		case SO_TC_RD:
			return "RD";
		case SO_TC_OAM:
			return "OAM";
		case SO_TC_AV:
			return "AV";
		case SO_TC_RV:
			return "RV";
		case SO_TC_VI:
			return "VI";
		case SO_TC_VO:
			return "VO";
		case SO_TC_CTL:
			return "CTL";
		case SO_TC_NETSVC_SIG:
			return "SIG";
		default:
			snprintf(svcstr, sizeof(svcstr), "%u", svc);
			return svcstr;
	}
}

void
pcap_read_bpf_header(pcap_t *p, u_char *bp, struct pcap_pkthdr *pkthdr)
{
	struct bpf_hdr_ext *bhep = ((struct bpf_hdr_ext *)bp);
	char tmpbuf[100];
	int tlen;
	char *strsep = "";

	pkthdr->comment[0] = 0;

	if (p->extendedhdr == 0) {
		return;
	}

	if (bhep->bh_comm[0] != 0) {
		bzero(&tmpbuf, sizeof (tmpbuf));
		tlen = snprintf(tmpbuf, sizeof (tmpbuf),
				"%sproc %s:%d", strsep, bhep->bh_comm, bhep->bh_pid);
		if (tlen > 0) {
			strlcat(pkthdr->comment, tmpbuf, sizeof (pkthdr->comment));
			strsep = ", ";
		}
	}

	bzero(&tmpbuf, sizeof (tmpbuf));
	tlen = snprintf(tmpbuf, sizeof (tmpbuf),
			"%ssvc %s", strsep, pcap_svc2str(bhep->bh_svc));
	if (tlen > 0) {
		strlcat(pkthdr->comment, tmpbuf, sizeof (pkthdr->comment));
		strsep = ", ";
	}

	if (bhep->bh_pktflags != 0) {
		if (bhep->bh_pktflags & BPF_PKTFLAGS_TCP_REXMT) {
			bzero(&tmpbuf, sizeof (tmpbuf));
			tlen = snprintf(tmpbuf, sizeof (tmpbuf),
					"%sre", strsep);
			if (tlen > 0) {
				strlcat(pkthdr->comment, tmpbuf, sizeof (pkthdr->comment));
				strsep = ", ";
			}
		}
		if (bhep->bh_pktflags & BPF_PKTFLAGS_START_SEQ) {
			bzero(&tmpbuf, sizeof (tmpbuf));
			tlen = snprintf(tmpbuf, sizeof (tmpbuf),
					"%sst", strsep);
			if (tlen > 0) {
				strlcat(pkthdr->comment, tmpbuf, sizeof (pkthdr->comment));
				strsep = ", ";
			}
		}
		if (bhep->bh_pktflags & BPF_PKTFLAGS_LAST_PKT) {
			bzero(&tmpbuf, sizeof (tmpbuf));
			tlen = snprintf(tmpbuf, sizeof (tmpbuf),
					"%slp", strsep);
			if (tlen > 0) {
				strlcat(pkthdr->comment, tmpbuf, sizeof (pkthdr->comment));
				strsep = ", ";
			}
		}

		if (bhep->bh_pktflags & BPF_PKTFLAGS_WAKE_PKT) {
			bzero(&tmpbuf, sizeof (tmpbuf));
			tlen = snprintf(tmpbuf, sizeof (tmpbuf),
					"%swk", strsep);
			if (tlen > 0) {
				strlcat(pkthdr->comment, tmpbuf, sizeof (pkthdr->comment));
				strsep = ", ";
			}
		}
	}

	if (bhep->bh_unsent_bytes > 0) {
		bzero(&tmpbuf, sizeof (tmpbuf));
		tlen = snprintf(tmpbuf, sizeof (tmpbuf),
				"%sunsent %u", strsep, bhep->bh_unsent_bytes);
		if (tlen > 0) {
			strlcat(pkthdr->comment, tmpbuf, sizeof (pkthdr->comment));
			strsep = ", ";
		}
	}

#define BPF_HDR_EXT_FLAGS_DIR_MASK (BPF_HDR_EXT_FLAGS_DIR_IN | BPF_HDR_EXT_FLAGS_DIR_OUT)
	if ((bhep->bh_flags & BPF_HDR_EXT_FLAGS_DIR_MASK) == BPF_HDR_EXT_FLAGS_DIR_IN) {
		bzero(&tmpbuf, sizeof (tmpbuf));
		tlen = snprintf(tmpbuf, sizeof (tmpbuf),
				"%sin", strsep);
		if (tlen > 0) {
			strlcat(pkthdr->comment, tmpbuf, sizeof (pkthdr->comment));
			strsep = ", ";
		}
	} else {
		bzero(&tmpbuf, sizeof (tmpbuf));
		tlen = snprintf(tmpbuf, sizeof (tmpbuf),
				"%sout", strsep);
		if (tlen > 0) {
			strlcat(pkthdr->comment, tmpbuf, sizeof (pkthdr->comment));
			strsep = ", ";
		}
	}

	if (bhep->bh_flowid > 0) {
		bzero(&tmpbuf, sizeof (tmpbuf));
		tlen = snprintf(tmpbuf, sizeof (tmpbuf),
				"%sflowid 0x%x", strsep, bhep->bh_flowid);
		if (tlen > 0) {
			strlcat(pkthdr->comment, tmpbuf, sizeof (pkthdr->comment));
			strsep = ", ";
		}
	}
#if BPF_HDR_EXT_HAS_TRACE_TAG
	if (bhep->bh_trace_tag > 0) {
		bzero(&tmpbuf, sizeof (tmpbuf));
		tlen = snprintf(tmpbuf, sizeof (tmpbuf),
				"%sttag 0x%x", strsep, bhep->bh_trace_tag);
		if (tlen > 0) {
			strlcat(pkthdr->comment, tmpbuf, sizeof (pkthdr->comment));
			strsep = ", ";
		}
	}
#endif /* BPF_HDR_EXT_HAS_TRACE_TAG */
}

/*
 * Returns zero if the packet doesn't match, non-zero if it matches
 */
static int
pcap_filter_pktap_v2(pcap_t *pcap, pcap_dumper_t *dumper, struct pcap_if_info *if_info,
		     const struct pcap_pkthdr *h, const u_char *sp)
{
	struct pktap_v2_hdr *pktap_v2_hdr;
	const u_char *pkt_data;
	int match = 0;
	const char *ifname;
	
	pktap_v2_hdr = (struct pktap_v2_hdr *)sp;
	
	if (h->len < sizeof(struct pktap_v2_hdr) ||
	    h->caplen < sizeof(struct pktap_v2_hdr) ||
	    pktap_v2_hdr->pth_length > h->caplen) {
		snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
			 "%s: Packet too short", __func__);
		return (0);
	}

	if (pktap_v2_hdr->pth_ifname_offset == 0) {
		return (0);
	}
	ifname = ((char *) pktap_v2_hdr) + pktap_v2_hdr->pth_ifname_offset;
		
	if (if_info == NULL) {
		if_info = pcap_if_info_set_find_by_name(&dumper->dump_if_info_set, ifname);
		/*
		 * New interface
		 */
		if (if_info == NULL) {
			if_info = pcap_if_info_set_add(&dumper->dump_if_info_set, ifname, -1,
						       pktap_v2_hdr->pth_dlt, pcap->snapshot,
						       pcap->filter_str, pcap->errbuf);
			if (if_info == NULL) {
				return (0);
			}
		}
	}
	
	if (if_info->if_filter_program.bf_insns == NULL) {
		match = 1;
	} else {
		/*
		 * The actual data packet is past the packet tap header
		 */
		struct pcap_pkthdr tmp_hdr;
		
		bcopy(h, &tmp_hdr, sizeof(struct pcap_pkthdr));
		
		tmp_hdr.caplen -= pktap_v2_hdr->pth_length;
		tmp_hdr.len -= pktap_v2_hdr->pth_length;
		
		pkt_data = sp + pktap_v2_hdr->pth_length;
		
		match = pcap_offline_filter(&if_info->if_filter_program, &tmp_hdr, pkt_data);
	}
	
	return (match);
}

int
pcap_ng_dump_pktap_v2(pcap_t *pcap, pcap_dumper_t *dumper,
		      const struct pcap_pkthdr *h, const u_char *sp,
		      const char *comment)
{
	pcapng_block_t block = NULL;
	struct pktap_v2_hdr *pktap_v2_hdr;
	const u_char *pkt_data;
	struct pcap_if_info *if_info = NULL;
	struct pcapng_enhanced_packet_fields *epb;
	uint64_t ts;
	struct pcap_proc_info *proc_info = NULL;
	struct pcap_proc_info *e_proc_info = NULL;
	uint32_t pktflags = 0;
	uint32_t pmdflags = 0;
	int retval;
	const char *ifname = NULL;
	const char *comm = NULL;
	const uuid_t *uuid = NULL;
	const char *e_comm = NULL;
	const uuid_t *e_uuid = NULL;
	
	pktap_v2_hdr = (struct pktap_v2_hdr *)sp;
	
	if (h->len < sizeof(struct pktap_v2_hdr) ||
	    h->caplen < sizeof(struct pktap_v2_hdr) ||
	    pktap_v2_hdr->pth_length > h->caplen) {
		snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
			 "%s: Packet too short", __func__);
		return (0);
	}
	
	if (pktap_v2_hdr->pth_ifname_offset == 0) {
		snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
			 "%s: No ifame", __func__);
		return (0);
	}
	ifname = ((char *) pktap_v2_hdr) + pktap_v2_hdr->pth_ifname_offset;
	
	if (pcap_ng_dump_shb(pcap, dumper) == 0)
		return (0);
	
	block = dumper->dump_block;
	
	/*
	 * Add an interface info block for a new interface before filtering
	 */
	if_info = pcap_if_info_set_find_by_name(&dumper->dump_if_info_set, ifname);
	if (if_info == NULL) {
		if_info = pcap_if_info_set_add(&dumper->dump_if_info_set, ifname, -1,
					       pktap_v2_hdr->pth_dlt, pcap->snapshot,
					       pcap->filter_str, pcap->errbuf);
		if (if_info == NULL) {
			return (0);
		}
	}
	
	/*
	 * Check the packet matches the filter
	 */
	if (pcap_filter_pktap_v2(pcap, dumper, if_info, h, sp) == 0)
		return (0);
	
	/*
	 * Dump the interface info block (if needed)
	 */
	if_info = pcap_ng_dump_if_info(pcap, dumper, block, if_info);
	if (if_info == NULL) {
		return (0);
	}
	
	if (pktap_v2_hdr->pth_comm_offset != 0)
		comm = ((char *) pktap_v2_hdr) + pktap_v2_hdr->pth_comm_offset;
	if (pktap_v2_hdr->pth_uuid_offset != 0)
		uuid = (uuid_t *)(((char *) pktap_v2_hdr) + pktap_v2_hdr->pth_uuid_offset);
	if ((pktap_v2_hdr->pth_pid != 0 && pktap_v2_hdr->pth_pid != -1) ||
	    comm != NULL || uuid != NULL) {
		proc_info = pcap_ng_dump_proc(pcap, dumper, block,
					      pktap_v2_hdr->pth_pid,
					      comm,
					      *uuid);
		if (proc_info == NULL)
			return (0);
	}
	
	if (pktap_v2_hdr->pth_e_comm_offset != 0)
		e_comm = ((char *) pktap_v2_hdr) + pktap_v2_hdr->pth_e_comm_offset;
	if (pktap_v2_hdr->pth_e_uuid_offset != 0)
		e_uuid = (uuid_t *)(((char *) pktap_v2_hdr) + pktap_v2_hdr->pth_e_uuid_offset);
	
	if ((pktap_v2_hdr->pth_e_pid != 0 && pktap_v2_hdr->pth_e_pid != -1) ||
	    e_comm != NULL || e_uuid != NULL) {
		e_proc_info = pcap_ng_dump_proc(pcap, dumper, block,
						pktap_v2_hdr->pth_e_pid,
						e_comm,
						*e_uuid);
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
	pkt_data = sp + pktap_v2_hdr->pth_length;
	epb = pcap_ng_get_enhanced_packet_fields(block);
	epb->caplen = h->caplen - pktap_v2_hdr->pth_length;
	epb->interface_id = if_info->if_dump_id;
	epb->len = h->len - pktap_v2_hdr->pth_length;
	/* Microsecond resolution */
	ts = ((uint64_t)h->ts.tv_sec) * 1000000 + (uint64_t)h->ts.tv_usec;
	epb->timestamp_high = ts >> 32;
	epb->timestamp_low  = ts & 0xffffffff;
	
	pcap_ng_block_packet_set_data(block, pkt_data, epb->caplen);
	
	if (proc_info != NULL) {
		pcap_ng_block_add_option_with_value(block, PCAPNG_EPB_PIB_INDEX, &proc_info->proc_dump_index, 4);
	}
	if (e_proc_info != NULL) {
		pcap_ng_block_add_option_with_value(block, PCAPNG_EPB_E_PIB_INDEX, &e_proc_info->proc_dump_index, 4);
	}
	if ((pktap_v2_hdr->pth_flags & PTH_FLAG_DIR_IN)) {
		pktflags = PCAPNG_PBF_DIR_INBOUND;
	} else if ((pktap_v2_hdr->pth_flags & PTH_FLAG_DIR_OUT)) {
		pktflags = PCAPNG_PBF_DIR_OUTBOUND;
	}
	if (pktflags != 0) {
		pcap_ng_block_add_option_with_value(block, PCAPNG_EPB_FLAGS , &pktflags, 4);
	}
	if (pktap_v2_hdr->pth_svc != (uint16_t)-1) {
		uint32_t svc = pktap_v2_hdr->pth_svc;
		pcap_ng_block_add_option_with_value(block, PCAPNG_EPB_SVC , &svc, 4);
	}
	if (pktap_v2_hdr->pth_flowid != 0) {
		pcap_ng_block_add_option_with_value(block, PCAPNG_EPB_FLOW_ID , &pktap_v2_hdr->pth_flowid, 4);
	}
	if (comment != NULL && *comment != 0) {
		pcap_ng_block_add_option_with_string(block, PCAPNG_OPT_COMMENT, comment);
	}
	if (pktap_v2_hdr->pth_flags & PTH_FLAG_NEW_FLOW) {
		pmdflags |= PCAPNG_EPB_PMDF_NEW_FLOW;
	}
	if (pktap_v2_hdr->pth_flags & PTH_FLAG_REXMIT) {
		pmdflags |= PCAPNG_EPB_PMDF_REXMIT;
	}
	if (pktap_v2_hdr->pth_flags & PTH_FLAG_KEEP_ALIVE) {
		pmdflags |= PCAPNG_EPB_PMDF_KEEP_ALIVE;
	}
	if (pktap_v2_hdr->pth_flags & PTH_FLAG_SOCKET) {
		pmdflags |= PCAPNG_EPB_PMDF_SOCKET;
	}
	if (pktap_v2_hdr->pth_flags & PTH_FLAG_NEXUS_CHAN) {
		pmdflags |= PCAPNG_EPB_PMDF_NEXUS_CHANNEL;
	}
	if (pktap_v2_hdr->pth_flags & PTH_FLAG_WAKE_PKT) {
		pmdflags |= PCAPNG_EPB_PMDF_WAKE_PKT;
	}
	if (pmdflags != 0) {
		pcap_ng_block_add_option_with_value(block, PCAPNG_EPB_PMD_FLAGS, &pmdflags, 4);
	}

	(void) pcap_ng_dump_block(dumper, block);
	
	return (1);
}

void
pcap_ng_dump_init_section_info(pcap_dumper_t *dumper)
{
	dumper->shb_added = 0;
	pcap_if_info_set_clear(&dumper->dump_if_info_set);
	pcap_proc_info_set_clear(&dumper->dump_proc_info_set);
}

int
pcap_set_max_write_size(pcap_t *pcap, u_int max_write_size)
{
	if (ioctl(pcap->fd, BIOCSWRITEMAX, &max_write_size) != 0) {
		snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
			 "%s: BIOCSWRITEMAX errno %d", __func__, errno);
		return PCAP_ERROR;
	}
	return 0;
}

int
pcap_get_max_write_size(pcap_t *pcap, u_int *max_write_size_ptr)
{
	if (ioctl(pcap->fd, BIOCGWRITEMAX, max_write_size_ptr) != 0) {
		snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
			 "%s: BIOCGWRITEMAX errno %d", __func__, errno);
		return PCAP_ERROR;
	}
	return 0;
}

int
pcap_set_send_multiple(pcap_t *pcap, int value)
{
	if (ioctl(pcap->fd, BIOCSBATCHWRITE, &value) != 0) {
		snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
			 "%s: BIOCSBATCHWRITE errno %d", __func__, errno);
		return PCAP_ERROR;
	} else {
		pcap->send_multiple = value == 0 ? 0 : 1;
	}
	return 0;
}

int
pcap_get_send_multiple(pcap_t *pcap, int *value_ptr)
{
	if (ioctl(pcap->fd, BIOCGBATCHWRITE, value_ptr) != 0) {
		snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
			 "%s: BIOCGBATCHWRITE errno %d", __func__, errno);
		return PCAP_ERROR;
	}
	return 0;
}

#define PCAP_SEND_MULTIPLE_MAX 256
#define PCAP_PRIV_IOVLEN_MAX 128

static int
send_packets_one_by_one(pcap_t *pcap, const u_int pkt_count, const struct pcap_pkt_hdr_priv *pcap_pkt_array)
{
	int num_send = 0;

	for (u_int i = 0; i < pkt_count; i++) {
		uint32_t n = pcap_pkt_array[i].pcap_priv_iov_count;

		if (n > PCAP_PRIV_IOVLEN_MAX) {
			snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
				 "pcap_sendpacket_multiple: pcap_priv_iov_count %u greater than max %d",
				 n, PCAP_PRIV_IOVLEN_MAX);
			return PCAP_ERROR;
		}
	}

	for (u_int i = 0; i < pkt_count; i++) {
		ssize_t ret = writev(pcap->fd, pcap_pkt_array[i].pcap_priv_iov_array, pcap_pkt_array[i].pcap_priv_iov_count);
		if (ret < 0) {
			pcap_fmt_errmsg_for_errno(pcap->errbuf, PCAP_ERRBUF_SIZE,
				errno, "pcap_sendpacket_multiple: writev %d failed: %s", i, pcap->errbuf);
			break;
		}
		num_send++;
	}
	return num_send;
}

int
pcap_sendpacket_multiple(pcap_t *pcap, const u_int pkt_count, const struct pcap_pkt_hdr_priv *pcap_pkt_array)
{
	if (pkt_count > PCAP_SEND_MULTIPLE_MAX) {
		snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
				 "pcap_sendpacket_multiple: count %u greater than max %d",
				 pkt_count, PCAP_SEND_MULTIPLE_MAX);
		return PCAP_ERROR;
	}

	/*
	 * Be lenient and allow the SPI to be called when send_multiple is not set
	 */
	if (pcap->send_multiple == 0) {
		return send_packets_one_by_one(pcap, pkt_count, pcap_pkt_array);
	}

	/*
	 * Allocate buffers large enough for the packet count
	 */
	if (pcap->send_bpfhdr_count < pkt_count) {
		if (pcap->send_bpfhdr_array != NULL) {
			free(pcap->send_bpfhdr_array);
			pcap->send_bpfhdr_array = NULL;
		}
		pcap->send_bpfhdr_count = 0;

		pcap->send_bpfhdr_array = calloc(pkt_count, sizeof(struct bpf_hdr));
		if (pcap->send_bpfhdr_array == NULL) {
			snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
				 "pcap_sendpacket_multiple: calloc bpf_hdr array errno %d", errno);
			return PCAP_ERROR;
		}
		pcap->send_bpfhdr_count = pkt_count;
	}

	/*
	 * Make sure we have enough iovecs
	 */
	u_int num_iov_needed = 0;
	for (u_int i = 0; i < pkt_count; i++) {
		uint32_t n = pcap_pkt_array[i].pcap_priv_iov_count;

		if (n > PCAP_PRIV_IOVLEN_MAX) {
			snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
				 "pcap_sendpacket_multiple: pcap_priv_iov_count %u greater that max %d",
				 n, PCAP_PRIV_IOVLEN_MAX);
			return PCAP_ERROR;
		}
		num_iov_needed += n;
	}
	/*
	 * We need 2 more iovecs for each packet because of BPF word alignment:
	 * - one iovec is for the BPF header
	 * - another iovec is for padding if needed
	 */
	num_iov_needed += 2 * pkt_count;

	if (pcap->send_iovec_count < num_iov_needed) {
		if (pcap->send_iovec_array != NULL) {
			free(pcap->send_iovec_array);
			pcap->send_iovec_array = NULL;
		}
		pcap->send_iovec_count = 0;

		pcap->send_iovec_array = calloc(3 * num_iov_needed, sizeof(struct iovec));
		if (pcap->send_iovec_array == NULL) {
			snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
					 "pcap_sendpacket_multiple: calloc iovec array errno %d", errno);
			free(pcap->send_bpfhdr_array);
			pcap->send_bpfhdr_array = NULL;
			pcap->send_bpfhdr_count = 0;
			return PCAP_ERROR;
		}
		pcap->send_iovec_count = num_iov_needed;
	}

	/*
	 * We use several iovecs for each packet:
	 * - the first iovec is for the BPF header
	 * - the followings iovecs are for the data
	 * - the optional last iovec is for padding if needed
	 */
	int iovcnt = 0;
	for (u_int i = 0; i < pkt_count; i++) {
		size_t padding_len;
		char padding[BPF_ALIGNMENT];
		struct bpf_hdr *bpfhdr = &pcap->send_bpfhdr_array[i];

		bpfhdr->bh_hdrlen = offsetof(struct bpf_hdr, bh_hdrlen) + sizeof(bpfhdr->bh_hdrlen);
		bpfhdr->bh_caplen = pcap_pkt_array[i].pcap_priv_len;
		bpfhdr->bh_datalen = bpfhdr->bh_caplen;

		pcap->send_iovec_array[iovcnt].iov_base = bpfhdr;
		pcap->send_iovec_array[iovcnt].iov_len = bpfhdr->bh_hdrlen;
		iovcnt += 1;

		for (uint32_t j = 0; j < pcap_pkt_array[i].pcap_priv_iov_count; j++) {
			if (j >= pcap->send_iovec_count) {
				snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
						 "pcap_sendpacket_multiple: calloc iovec array errno %d", errno);
			}
			pcap->send_iovec_array[iovcnt] = pcap_pkt_array[i].pcap_priv_iov_array[j];
			iovcnt += 1;
		}

		padding_len = BPF_ALIGNMENT - (bpfhdr->bh_hdrlen + bpfhdr->bh_caplen) % BPF_ALIGNMENT;
		if (padding_len < BPF_ALIGNMENT) {
			pcap->send_iovec_array[iovcnt].iov_base = padding;
			pcap->send_iovec_array[iovcnt].iov_len = padding_len;
			iovcnt += 1;
		}
	}

	ssize_t retval = writev(pcap->fd, pcap->send_iovec_array, iovcnt);
	if (retval < 0) {
		snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
			 "pcap_sendpacket_multiple: writev failed errno %d", errno);
		return PCAP_ERROR;
	}
	return pkt_count;
}

int
pcap_get_divert_input(pcap_t *pcap, int *value)
{
#ifdef BIOCGDVRTIN
	if (ioctl(pcap->fd, BIOCGDVRTIN, &value) != 0) {
		snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
			 "%s: BIOCGDVRTIN errno %d", __func__, errno);
		return PCAP_ERROR;
	}
	return 0;
#else  /* BIOCGDVRTIN */
	snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
		 "%s: BIOCGDVRTIN not supported", __func__);
	return PCAP_ERROR;
#endif /* BIOCGDVRTIN */
}

int
pcap_set_divert_input(pcap_t *pcap, int value)
{
#ifdef BIOCSDVRTIN
	if (ioctl(pcap->fd, BIOCSDVRTIN, &value) != 0) {
		snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
				 "%s: BIOCSDVRTIN %d errno %d", __func__, value, errno);
		return PCAP_ERROR;
	}
	return 0;
#else  /* BIOCGDVRTIN */
	snprintf(pcap->errbuf, PCAP_ERRBUF_SIZE,
		 "%s: BIOCSDVRTIN not supported", __func__);
	return PCAP_ERROR;
#endif /* BIOCGDVRTIN */
}
