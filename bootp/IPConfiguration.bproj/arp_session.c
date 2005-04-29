/*
 * Copyright (c) 2000 - 2004 Apple Computer, Inc. All rights reserved.
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

/*
 * arp_session.c
 */
/* 
 * Modification History
 *
 * May 11, 2000		Dieter Siegmund (dieter@apple.com)
 * - created
 *
 * March 21, 2001	Dieter Siegmund (dieter@apple.com)
 * - process multiple ARP responses from one bpf read instead of
 *   assuming there's just one response per read
 *
 * June 16, 2003	Dieter Siegmund (dieter@apple.com)
 * - added support for firewire
 *
 * December 22, 2003	Dieter Siegmund (dieter@apple.com)
 * - handle multiple arp client probe requests over multiple
 *   interfaces concurrently
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <net/if_arp.h>
#include <net/firewire.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <net/if_types.h>
#include <net/bpf.h>

#include "util.h"
#include <syslog.h>
#include "bpflib.h"
#include "util.h"
#include "dprintf.h"
#include "dynarray.h"
#include "timer.h"
#include "FDSet.h"
#include "ipconfigd_globals.h"
#include "arp_session.h"
#include "ioregpath.h"

#ifndef IFT_IEEE8023ADLAG
#define IFT_IEEE8023ADLAG 0x88		/* IEEE802.3ad Link Aggregate */
#endif IFT_IEEE8023ADLAG

struct firewire_arp {
    struct arphdr 	fw_hdr;	/* fixed-size header */
    uint8_t		arp_sha[FIREWIRE_ADDR_LEN];
    uint8_t		arp_spa[4];
    uint8_t		arp_tpa[4];
};

struct probe_info {
    struct timeval		retry_interval;
    int				probe_count;
    int				gratuitous_count;
};

struct arp_session {
    FDSet_t *			readers;
    int				debug;
    struct probe_info		default_probe_info;
    arp_our_address_func_t *	is_our_address;
    dynarray_t			if_sessions;
#ifdef TEST_ARP_SESSION
    int				next_client_index;
#endif TEST_ARP_SESSION
};

struct arp_if_session {
    arp_session_t *		session;
    interface_t *		if_p;
    int				bpf_fd;
    int				bpf_fd_refcount;
    dynarray_t			clients;
    char *			receive_buf;
    int				receive_bufsize;
    FDCallout_t *		read_callout;
    struct firewire_address	fw_addr;
};

typedef struct arp_if_session arp_if_session_t;

typedef enum {
    arp_probe_result_none_e = 0,
    arp_probe_result_not_in_use_e = 1,
    arp_probe_result_in_use_e = 2,
    arp_probe_result_error_e = 3,
    arp_probe_result_unknown_e = 4,
} arp_probe_result_t;

struct arp_client {
#ifdef TEST_ARP_SESSION
    int				client_index; /* unique ID */
#endif TEST_ARP_SESSION
    arp_probe_result_t		probe_result;
    boolean_t			fd_open;
    arp_if_session_t *		if_session;
    arp_result_func_t *		func;
    void *			arg1;
    void *			arg2;
    struct in_addr		sender_ip;
    struct in_addr		target_ip;
    int				try;
    timer_callout_t *		timer_callout;
    char			in_use_hwaddr[MAX_LINK_ADDR_LEN];
    char			errmsg[128];
    struct probe_info		probe_info;
    boolean_t			probes_are_collisions;
};

#define ARP_PROBE_COUNT		3
#define ARP_GRATUITOUS_COUNT	1
#define ARP_RETRY_SECS		1
#define ARP_RETRY_USECS		0
#ifdef TEST_ARP_SESSION
#define my_log		arp_session_log
static void arp_session_log(int priority, const char * message, ...);
#endif TEST_ARP_SESSION

#include <CoreFoundation/CFDictionary.h>
#include <SystemConfiguration/SCValidation.h>

static Boolean
getFireWireAddress(const char * ifname, struct firewire_address * addr_p)
{
    CFDictionaryRef	dict = NULL;
    CFDataRef		data;
    Boolean		found = FALSE;

    dict = myIORegistryEntryBSDNameMatchingCopyValue(ifname, TRUE);
    if (dict == NULL) {
	return (FALSE);
    }
    data = CFDictionaryGetValue(dict, CFSTR("IOFWHWAddr"));
    if (isA_CFData(data) == NULL || CFDataGetLength(data) != sizeof(*addr_p)) {
	goto done;
    }
    CFDataGetBytes(data, CFRangeMake(0, sizeof(*addr_p)), (void *)addr_p);

    /* put it in network byte order */
    addr_p->unicastFifoHi = htons(addr_p->unicastFifoHi);
    addr_p->unicastFifoLo = htonl(addr_p->unicastFifoLo);
    found = TRUE;

 done:
    if (dict != NULL) {
	CFRelease(dict);
    }
    return (found);
}

/* forward-declarations: */

static arp_if_session_t *
arp_session_new_if_session(arp_session_t * session, interface_t * if_p);

static void
arp_client_free_element(void * arg);

static void
arp_client_retransmit(void * arg1, void * arg2, void * arg3);

static boolean_t
arp_client_open_fd(arp_client_t * client);

static void
arp_client_close_fd(arp_client_t * client);

static void
arp_if_session_free(arp_if_session_t * * if_session_p);

static void
arp_if_session_free_element(void * arg);

static void
arp_if_session_read(void * arg1, void * arg2);

static boolean_t
arp_is_our_address(interface_t * if_p,
		   int hwtype, void * hwaddr, int hwlen);

static __inline__ char *
arpop_name(u_int16_t op)
{
    switch (op) {
    case ARPOP_REQUEST:
	return "ARP REQUEST";
    case ARPOP_REPLY:
	return "ARP REPLY";
    case ARPOP_REVREQUEST:
	return "REVARP REQUEST";
    case ARPOP_REVREPLY:
	return "REVARP REPLY";
    default:
	break;
    }
    return ("<unknown>");
}

static void
dump_arp(struct arphdr * arp_p)

{
    int arphrd = ntohs(arp_p->ar_hrd);

    printf("\n");
    printf("%s type=0x%x proto=0x%x\n", arpop_name(ntohs(arp_p->ar_op)),
	   arphrd, ntohs(arp_p->ar_pro));

    switch (arphrd) {
    case ARPHRD_ETHER:
	{
	    struct ether_arp * earp = (struct ether_arp *)arp_p;
	    if (arp_p->ar_hln == sizeof(earp->arp_sha)) {
		printf("Sender H/W\t%s\n", 
		       ether_ntoa((const struct ether_addr *)earp->arp_sha));
		printf("Target H/W\t%s\n", 
		       ether_ntoa((const struct ether_addr *)earp->arp_tha));
	    }
	    printf("Sender IP\t%s\n", 
		   inet_ntoa(*((struct in_addr *)earp->arp_spa)));
	    printf("Target IP\t%s\n", 
		   inet_ntoa(*((struct in_addr *)earp->arp_tpa)));
	}
	break;
    case ARPHRD_IEEE1394:
	{
	    struct firewire_arp * farp = (struct firewire_arp *)arp_p;

	    if (arp_p->ar_hln == sizeof(farp->arp_sha)) {
		printf("Sender H/W\t" FWA_FORMAT "\n",
		       FWA_LIST(farp->arp_sha));
	    }
	    printf("Sender IP\t%s\n", 
		   inet_ntoa(*((struct in_addr *)farp->arp_spa)));
	    printf("Target IP\t%s\n", 
		   inet_ntoa(*((struct in_addr *)farp->arp_tpa)));
	}
	break;
    }
    fflush(stdout);
    return;
}

static void
arp_client_close_fd(arp_client_t * client)
{
    arp_if_session_t * 	if_session = client->if_session;

    if (client->fd_open == FALSE) {
	return;
    }
    if (if_session->bpf_fd_refcount <= 0) {
	my_log(LOG_INFO, "arp_client_close_fd(%s): bpf open fd count is %d",
	       if_name(if_session->if_p), if_session->bpf_fd_refcount);
	return;
    }
    if_session->bpf_fd_refcount--;
    my_log(LOG_DEBUG, "arp_client_close_fd(%s): bpf open fd count is %d",
	   if_name(if_session->if_p), if_session->bpf_fd_refcount);
    client->fd_open = FALSE;
    if (if_session->bpf_fd_refcount == 0) {
	my_log(LOG_DEBUG, "arp_client_close_fd(%s): closing bpf fd %d",
	       if_name(if_session->if_p), if_session->bpf_fd);
	if (if_session->read_callout != NULL) {
	    /* this closes the file descriptor */
	    FDSet_remove_callout(if_session->session->readers, 
				 &if_session->read_callout);
	}
	if_session->bpf_fd = -1;
	if (if_session->receive_buf != NULL) {
	    free(if_session->receive_buf);
	    if_session->receive_buf = NULL;
	}
    }
    return;
}

/*
 * Function: arp_client_callback
 *
 * Purpose:
 *   Call the supplied function with the appropriate result.
 */
static void
arp_client_callback(void * arg1, void * arg2, void * arg3)
{
    void *		c_arg1;
    void * 		c_arg2;
    arp_result_func_t *	func;
    arp_client_t * 	client = (arp_client_t *)arg1;
    arp_result_t	result;
    
    /* remember the client parameters, then clear them */
    c_arg1 = client->arg1;
    c_arg2 = client->arg2;
    func = client->func;
    client->func = client->arg1 = client->arg2 = NULL;

    arp_client_close_fd(client);
    timer_cancel(client->timer_callout);

    /* return the results */
    bzero(&result, sizeof(result));
    switch (client->probe_result) {
    default:
    case arp_probe_result_none_e:
	/* not possible */
	printf("No result for %s?\n", 
	       if_name(client->if_session->if_p));
	break;
    case arp_probe_result_error_e:
	result.error = TRUE;
	break;
    case arp_probe_result_not_in_use_e:
	break;
    case arp_probe_result_in_use_e:
	result.in_use = TRUE;
	result.hwaddr = client->in_use_hwaddr;
	break;
    }

    /* return the results to the client */
    (*func)(c_arg1, c_arg2, &result);
    return;
}

/*
 * Function: arp_is_our_hardware_address
 *
 * Purpose:
 *   Returns whether the given hardware address matches the given
 *   network interface.
 */
static boolean_t
arp_is_our_address(interface_t * if_p,
		   int hwtype, void * hwaddr, int hwlen)
{
    int		link_length = if_link_length(if_p);

    if (hwtype != if_link_arptype(if_p)) {
	return (FALSE);
    }
    switch (hwtype) {
    default:
    case ARPHRD_ETHER:
	if (hwlen != link_length) {
	    return (FALSE);
	}
	break;
    case ARPHRD_IEEE1394:
	if (hwlen != FIREWIRE_ADDR_LEN) {
	    return (FALSE);
	}
	/* only the first 8 bytes matter */
	link_length = FIREWIRE_EUI64_LEN;
	break;
    }
    if (bcmp(hwaddr, if_link_address(if_p), link_length) == 0) {
	return (TRUE);
    }
    return (FALSE);
}

/*
 * Function: arp_if_session_read
 * Purpose:
 *   Called when data is available on the bpf fd.
 *   Check the arp packet, and see if it matches
 *   any of the clients' probe criteria.  If it does,
 *   call the client with an in_use result structure.
 */
static void
arp_if_session_read(void * arg1, void * arg2)
{
    arp_client_t *	client;
    int			client_count;
    boolean_t		debug;
    char		errmsg[128];
    int			hwlen = 0;
    int			hwtype;
    int			i;
    arp_if_session_t * 	if_session = (arp_if_session_t *)arg1;
    int			link_header_size;
    int			link_arp_size;
    int			n;
    char *		offset;
    struct timeval 	t = {0, 1};

    errmsg[0] = '\0';

    if (if_session->bpf_fd_refcount == 0) {
	my_log(LOG_ERR, "arp_if_session_read: no pending clients?");
	return;
    }

    debug = if_session->session->debug;
    client_count = dynarray_count(&if_session->clients);

    hwtype = if_link_arptype(if_session->if_p);
    switch (hwtype) {
    default:
	/* default clause will never match */
    case ARPHRD_ETHER:
	link_header_size = sizeof(struct ether_header);
	link_arp_size = sizeof(struct ether_arp);
	hwlen = ETHER_ADDR_LEN;
	break;
    case ARPHRD_IEEE1394:
	link_header_size = sizeof(struct firewire_header);
	link_arp_size = sizeof(struct firewire_arp);
	hwlen = FIREWIRE_ADDR_LEN;
	break;
    }
    n = read(if_session->bpf_fd, if_session->receive_buf, 
	     if_session->receive_bufsize);
    if (n < 0) {
	if (errno == EAGAIN) {
	    return;
	}
	my_log(LOG_ERR, "arp_if_session_read: read(%s) failed, %s (%d)", 
	       if_name(if_session->if_p), strerror(errno), errno);
	snprintf(errmsg, sizeof(errmsg),
		 "arp_if_session_read: read(%s) failed, %s (%d)", 
		 if_name(if_session->if_p), strerror(errno), errno);
	goto failed;
    }
    for (offset = if_session->receive_buf; n > 0; ) {
	struct arphdr *		arp_p;
	struct bpf_hdr * 	bpf = (struct bpf_hdr *)offset;
	void *			hwaddr;
	short			op;
	char *			pkt_start;
	struct in_addr *	source_ip_p;
	struct in_addr *	target_ip_p;
	int			skip;
	
	dprintf(("bpf remaining %d header %d captured %d\n", n, 
		 bpf->bh_hdrlen, bpf->bh_caplen));
	pkt_start = offset + bpf->bh_hdrlen;
	arp_p = (struct arphdr *)(pkt_start + link_header_size);
	if (debug) {
	    dump_arp(arp_p);
	}
	op = ntohs(arp_p->ar_op);
	if (bpf->bh_caplen < (link_header_size + link_arp_size)
	    || arp_p->ar_hln != hwlen
	    || (op != ARPOP_REPLY && op != ARPOP_REQUEST)
	    || ntohs(arp_p->ar_hrd) != hwtype
	    || ntohs(arp_p->ar_pro) != ETHERTYPE_IP) {
	    goto next_packet;
	}
	switch (hwtype) {
	default:
	case ARPHRD_ETHER:
	    {
		struct ether_arp * earp;
		
		earp = (struct ether_arp *)arp_p;
		source_ip_p = (struct in_addr *)earp->arp_spa;
		target_ip_p = (struct in_addr *)earp->arp_tpa;
		hwaddr = earp->arp_sha;
	    }
	    break;
	case ARPHRD_IEEE1394:
	    {
		struct firewire_arp * farp;

		farp = (struct firewire_arp *)arp_p;
		source_ip_p = (struct in_addr *)farp->arp_spa;
		target_ip_p = (struct in_addr *)farp->arp_tpa;
		hwaddr = farp->arp_sha;
	    }
	    break;
	}
	/* don't report conflicts against our own h/w addresses */
	if ((*if_session->session->is_our_address)(if_session->if_p,
						   hwtype,
						   hwaddr,
						   hwlen)) {
	    goto next_packet;
	}
	for (i = 0; i < client_count; i++) {
	    arp_client_t *	client;

	    client = dynarray_element(&if_session->clients, i);
	    if (client->func == NULL) {
		continue;
	    }
	    /* IP is in use by some other host */
	    if (client->target_ip.s_addr == source_ip_p->s_addr
		|| (client->probes_are_collisions
		    && op == ARPOP_REQUEST
		    && source_ip_p->s_addr == 0
		    && client->target_ip.s_addr == target_ip_p->s_addr)) {
		bcopy(hwaddr, client->in_use_hwaddr, hwlen);
		/* use a callback to avoid re-entrancy */
		client->probe_result = arp_probe_result_in_use_e;
		timer_set_relative(client->timer_callout, t, 
				   (timer_func_t *)arp_client_callback,
				   client, NULL, NULL);
	    }
	}
    next_packet:
	skip = BPF_WORDALIGN(bpf->bh_caplen + bpf->bh_hdrlen);
	if (skip == 0) {
	    break;
	}
	offset += skip;
	n -= skip;
    }
    return;
 failed:
    for (i = 0; i < client_count; i++) {
	client = dynarray_element(&if_session->clients, i);
	if (client->func == NULL) {
	    continue;
	}
	strncpy(client->errmsg, errmsg, sizeof(client->errmsg));
	/* report back an error to the caller */
	client->probe_result = arp_probe_result_error_e;
	timer_set_relative(client->timer_callout, t, 
			   (timer_func_t *)arp_client_callback,
			   client, NULL, NULL);
    }
    return;
}

static boolean_t
arp_client_open_fd(arp_client_t * client)
{
    arp_if_session_t *	if_session = client->if_session;
    int 		opt;
    int 		status;

    if (client->fd_open) {
	return (TRUE);
    }
    if_session->bpf_fd_refcount++;
    my_log(LOG_DEBUG, "arp_client_open_fd (%s): refcount %d", 
	   if_name(if_session->if_p), if_session->bpf_fd_refcount);
    client->fd_open = TRUE;
    if (if_session->bpf_fd_refcount > 1) {
	/* already open */
	return (TRUE);
    }
    if_session->bpf_fd = bpf_new();
    if (if_session->bpf_fd < 0) {
	my_log(LOG_ERR, "arp_client_open_fd: bpf_new(%s) failed, %s (%d)", 
	       if_name(if_session->if_p), strerror(errno), errno);
	snprintf(client->errmsg, sizeof(client->errmsg),
		 "arp_client_open_fd: bpf_new(%s) failed, %s (%d)", 
		 if_name(if_session->if_p), strerror(errno), errno);
	goto failed;
    }
    opt = 1;
    status = ioctl(if_session->bpf_fd, FIONBIO, &opt);
    if (status < 0) {
	my_log(LOG_ERR, "ioctl FIONBIO failed %s", strerror(errno));
	goto failed;
    }

    /* associate it with the given interface */
    status = bpf_setif(if_session->bpf_fd, if_name(if_session->if_p));
    if (status < 0) {
	my_log(LOG_ERR, "arp_client_open_fd: bpf_setif(%s) failed: %s (%d)", 
	       if_name(if_session->if_p), strerror(errno), errno);
	snprintf(client->errmsg, sizeof(client->errmsg),
		 "arp_client_open_fd: bpf_setif(%s) failed: %s (%d)", 
		 if_name(if_session->if_p), strerror(errno), errno);
	goto failed;
    }

    /* don't wait for packets to be buffered */
    bpf_set_immediate(if_session->bpf_fd, 1);

    /* set the filter to return only ARP packets */
    switch (if_link_type(if_session->if_p)) {
    default:
    case IFT_ETHER:
	status = bpf_arp_filter(if_session->bpf_fd, 12, ETHERTYPE_ARP,
				sizeof(struct ether_arp) 
				+ sizeof(struct ether_header));
	break;
    case IFT_IEEE1394:
	status = bpf_arp_filter(if_session->bpf_fd, 16, ETHERTYPE_ARP,
				sizeof(struct firewire_arp) 
				+ sizeof(struct firewire_header));
	break;
    }
    if (status < 0) {
	my_log(LOG_ERR, 
	       "arp_client_open_fd: bpf_arp_filter(%s) failed: %s (%d)", 
	       if_name(if_session->if_p), strerror(errno), errno);
	snprintf(client->errmsg, sizeof(client->errmsg),
		 "arp_client_open_fd: bpf_arp_filter(%s) failed: %s (%d)", 
		 if_name(if_session->if_p), strerror(errno), errno);
	goto failed;
    }
    /* get the receive buffer size */
    status = bpf_get_blen(if_session->bpf_fd, &if_session->receive_bufsize);
    if (status < 0) {
	my_log(LOG_ERR, 
	       "arp_client_open_fd: bpf_get_blen(%s) failed, %s (%d)", 
	       if_name(if_session->if_p), strerror(errno), errno);
	snprintf(client->errmsg, sizeof(client->errmsg),
		 "arp_client_open_fd: bpf_get_blen(%s) failed, %s (%d)", 
		 if_name(if_session->if_p), strerror(errno), errno);
	goto failed;
    }
    if_session->receive_buf = malloc(if_session->receive_bufsize);
    if_session->read_callout 
	= FDSet_add_callout(if_session->session->readers, if_session->bpf_fd,
			    arp_if_session_read, if_session, NULL);
    if (if_session->read_callout == NULL) {
	close(if_session->bpf_fd);
	if_session->bpf_fd = -1;
	goto failed;
    }
    my_log(LOG_DEBUG, "arp_client_open_fd (%s): opened bpf fd %d\n",
	   if_name(if_session->if_p), if_session->bpf_fd);
    return (TRUE);
 failed:
    arp_client_close_fd(client);
    return (FALSE);
}

static char			link_broadcast[8] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff 
};

static int
arp_client_transmit(arp_client_t * client, boolean_t send_gratuitous)
{
    arp_if_session_t *		if_session = client->if_session;
    struct arphdr *		hdr;
    int 			status = 0;
    char			txbuf[128];
    int				size;

    bzero(txbuf, sizeof(txbuf));

    /* fill in the ethernet header */
    switch (if_link_arptype(if_session->if_p)) {
    case ARPHRD_ETHER:
	{
	    struct ether_header *	eh_p;
	    struct ether_arp *		earp;

	    /* fill in the ethernet header */
	    eh_p = (struct ether_header *)txbuf;
	    bcopy(link_broadcast, eh_p->ether_dhost,
		  sizeof(eh_p->ether_dhost));
	    eh_p->ether_type = htons(ETHERTYPE_ARP);
	    /* fill in the arp packet contents */
	    earp = (struct ether_arp *)(txbuf + sizeof(*eh_p));
	    hdr = &earp->ea_hdr;
	    hdr->ar_hrd = htons(ARPHRD_ETHER);
	    hdr->ar_pro = htons(ETHERTYPE_IP);
	    hdr->ar_hln = sizeof(earp->arp_sha);;
	    hdr->ar_pln = sizeof(struct in_addr);
	    hdr->ar_op = htons(ARPOP_REQUEST);
	    bcopy(if_link_address(if_session->if_p), earp->arp_sha,
		  sizeof(earp->arp_sha));
	    if (send_gratuitous == TRUE
		&& client->sender_ip.s_addr == 0) {
		*((struct in_addr *)earp->arp_spa) = client->target_ip;
	    }
	    else {
		*((struct in_addr *)earp->arp_spa) = client->sender_ip;
	    }
	    *((struct in_addr *)earp->arp_tpa) = client->target_ip;
	    size = sizeof(*eh_p) + sizeof(*earp);
	}
	break;
    case ARPHRD_IEEE1394:
	{
	    struct firewire_header *	fh_p;
	    struct firewire_arp *	farp;

	    /* fill in the firewire header */
	    fh_p = (struct firewire_header *)txbuf;
	    bcopy(link_broadcast, fh_p->firewire_dhost,
		  sizeof(fh_p->firewire_dhost));
	    fh_p->firewire_type = htons(ETHERTYPE_ARP);

	    /* fill in the arp packet contents */
	    farp = (struct firewire_arp *)(txbuf + sizeof(*fh_p));
	    hdr = &farp->fw_hdr;
	    hdr->ar_hrd = htons(ARPHRD_IEEE1394);
	    hdr->ar_pro = htons(ETHERTYPE_IP);
	    hdr->ar_hln = sizeof(farp->arp_sha);;
	    hdr->ar_pln = sizeof(struct in_addr);
	    hdr->ar_op = htons(ARPOP_REQUEST);
	    bcopy(&if_session->fw_addr, farp->arp_sha,
		  sizeof(farp->arp_sha));
	    if (send_gratuitous == TRUE
		&& client->sender_ip.s_addr == 0) {
		*((struct in_addr *)farp->arp_spa) = client->target_ip;
	    }
	    else {
		*((struct in_addr *)farp->arp_spa) = client->sender_ip;
	    }
	    *((struct in_addr *)farp->arp_tpa) = client->target_ip;
	    size = sizeof(*fh_p) + sizeof(*farp);
	}
	break;
    default:
	snprintf(client->errmsg, sizeof(client->errmsg),
		 "arp_client_transmit(%s): "
		 "interface hardware type not yet known", 
		 if_name(if_session->if_p));
	return (0);
	break;
    }

    status = bpf_write(if_session->bpf_fd, txbuf, size);
    if (status < 0) {
	my_log(LOG_ERR, "arp_client_transmit(%s) failed, %s (%d)", 
	       if_name(if_session->if_p), strerror(errno), errno);
	snprintf(client->errmsg, sizeof(client->errmsg),
		 "arp_client_transmit(%s) failed, %s (%d)", 
		 if_name(if_session->if_p), strerror(errno), errno);
	return (0);
    }
    return (1);
}

/*
 * Function: arp_client_retransmit
 *
 * Purpose:
 *   Transmit an ARP packet with timeout retry.
 *   Uses callback to invoke arp_client_report_error if the transmit failed.
 *   When we've tried often enough, call the client function with a result
 *   structure indicating no errors and the IP is not in use.
 */
static void
arp_client_retransmit(void * arg1, void * arg2, void * arg3)
{
    arp_client_t * 	client = (arp_client_t *)arg1;
    struct probe_info * probe_info = &client->probe_info;
    struct timeval 	t = {0,1};
    int			tries_left;

    tries_left = (probe_info->probe_count + probe_info->gratuitous_count)
	- client->try;
    if (tries_left <= 0) {
	/* not in use */
	client->probe_result = arp_probe_result_not_in_use_e;
	timer_set_relative(client->timer_callout, t, 
			   (timer_func_t *)arp_client_callback,
			   client, NULL, NULL);
	return;
    }
    client->try++;
    if (arp_client_transmit(client, 
			    (tries_left == probe_info->gratuitous_count))) {
	timer_set_relative(client->timer_callout, probe_info->retry_interval,
			   (timer_func_t *)arp_client_retransmit,
			   client, NULL, NULL);
    }
    else {
	/* report back an error to the caller */
	client->probe_result = arp_probe_result_error_e;
	timer_set_relative(client->timer_callout, t, 
			   (timer_func_t *)arp_client_callback,
			   client, NULL, NULL);
    }
    return;
}

static arp_client_t *
arp_if_session_new_client(arp_if_session_t * if_session)
{
    arp_client_t *		client;

    client = malloc(sizeof(*client));
    if (client == NULL) {
	return (NULL);
    }
    bzero(client, sizeof(*client));
    if (dynarray_add(&if_session->clients, client) == FALSE) {
	free(client);
	return (NULL);
    }
#ifdef TEST_ARP_SESSION
    client->client_index = if_session->session->next_client_index++;
#endif TEST_ARP_SESSION
    client->if_session = if_session;
    client->probe_info = if_session->session->default_probe_info;
    client->timer_callout = timer_callout_init();
    return (client);

}

static arp_client_t *
arp_session_new_client(arp_session_t * session, interface_t * if_p)
{
    arp_if_session_t *		if_session;

    if_session = arp_session_new_if_session(session, if_p);
    if (if_session == NULL) {
	return (NULL);
    }
    return (arp_if_session_new_client(if_session));
}

#ifdef TEST_ARP_SESSION
static arp_client_t *
arp_session_find_client_with_index(arp_session_t * session, int index)
{
    int		if_sessions_count;
    int		i;
    int		j;

    if_sessions_count = dynarray_count(&session->if_sessions);
    for (i = 0; i < if_sessions_count; i++) {
	int			clients_count;
	arp_if_session_t *	if_session;

	if_session = dynarray_element(&session->if_sessions, i);
	clients_count = dynarray_count(&if_session->clients);
	for (j = 0; j < clients_count; j++) {
	    arp_client_t *	client;

	    client = dynarray_element(&if_session->clients, j);
	    if (client->client_index == index) {
		return (client);
	    }
	}
    }
    return (NULL);
}
#endif TEST_ARP_SESSION

void
arp_client_set_probe_info(arp_client_t * client, 
			  const struct timeval * retry_interval,
			  const int * probe_count, 
			  const int * gratuitous_count)
{
    struct probe_info *		probe_info = &client->probe_info;

    if (retry_interval != NULL) {
	probe_info->retry_interval = *retry_interval;
    }
    if (probe_count != NULL) {
	probe_info->probe_count = *probe_count;
    }
    if (gratuitous_count != NULL) {
	probe_info->gratuitous_count = *gratuitous_count;
    }
    return;
}

void 
arp_client_restore_default_probe_info(arp_client_t * client)
{
    client->probe_info = client->if_session->session->default_probe_info;
    return;
}


arp_client_t *
arp_client_init(arp_session_t * session, interface_t * if_p)
{
    return (arp_session_new_client(session, if_p));
}

static void
arp_client_free_element(void * arg)
{
    arp_client_t * 	client = (arp_client_t *)arg;

    arp_client_cancel_probe(client);
    timer_callout_free(&client->timer_callout);
    free(client);
    return;
}

void
arp_client_free(arp_client_t * * client_p)
{
    arp_client_t * 		client = NULL;
    int 			i;
    arp_if_session_t * 		if_session;

    if (client_p != NULL) {
	client = *client_p;
    }
    if (client == NULL) {
	return;
    }

    /* remove from list of clients */
    if_session = client->if_session;
    i = dynarray_index(&if_session->clients, client);
    if (i != -1) {
	dynarray_remove(&if_session->clients, i, NULL);
    }
    else {
	my_log(LOG_ERR, "arp_client_free(%s) not in list?",
	       if_name(if_session->if_p));
    }

    /* free resources */
    arp_client_free_element(client);
    *client_p = NULL;

    /* if we're the last client, if_session can go too */
    if (dynarray_count(&if_session->clients) == 0) {
	arp_if_session_free(&if_session);
    }
    return;
}

void
arp_client_set_probes_are_collisions(arp_client_t * client, 
				     boolean_t probes_are_collisions)
{
    client->probes_are_collisions = probes_are_collisions;
    return;
}

void
arp_client_probe(arp_client_t * client,
		 arp_result_func_t * func, void * arg1, void * arg2,
		 struct in_addr sender_ip, struct in_addr target_ip)
{
    
    arp_if_session_t * 	if_session = client->if_session;
    struct timeval 	t = {0, 1};

    if (if_link_type(if_session->if_p) == IFT_IEEE1394) {
	/* copy in the latest firewire address */
	if (getFireWireAddress(if_name(if_session->if_p), 
			       &if_session->fw_addr) == FALSE) {
	    my_log(LOG_INFO, 
		   "arp_probe(%s): could not retrieve firewire address",
		   if_name(if_session->if_p));
	}
    }
    client->sender_ip = sender_ip;
    client->target_ip = target_ip;
    client->func = func;
    client->arg1 = arg1;
    client->arg2 = arg2;
    client->errmsg[0] = '\0';
    client->try = 0;
    if (!arp_client_open_fd(client)) {
	/* report back an error to the caller */
	client->probe_result = arp_probe_result_error_e;
	timer_set_relative(client->timer_callout, t, 
			   (timer_func_t *)arp_client_callback,
			   client, NULL, NULL);
	return;
    }
    client->probe_result = arp_probe_result_unknown_e;
    arp_client_retransmit(client, NULL, NULL);
    return;
}

const char *
arp_client_errmsg(arp_client_t * client)
{
    return ((const char *)client->errmsg);
}

void
arp_client_cancel_probe(arp_client_t * client)
{
    client->errmsg[0] = '\0';
    client->func = client->arg1 = client->arg2 = NULL;
    client->probe_result = arp_probe_result_none_e;
    arp_client_close_fd(client);
    timer_cancel(client->timer_callout);
    return;
}

arp_session_t *
arp_session_init(FDSet_t * readers,
		 arp_our_address_func_t * func, 
		 const struct timeval * tv_p,
		 const int * probe_count, 
		 const int * gratuitous_count)
{
    arp_session_t * 	session;

    session = malloc(sizeof(*session));
    if (session == NULL) {
	return (NULL);
    }
    bzero(session, sizeof(*session));
    dynarray_init(&session->if_sessions, arp_if_session_free_element, NULL);
    session->readers = readers;
    if (func == NULL) {
	session->is_our_address = arp_is_our_address;
    }
    else {
	session->is_our_address = func;
    }
    if (tv_p) {
	session->default_probe_info.retry_interval = *tv_p;
    }
    else {
	session->default_probe_info.retry_interval.tv_sec = ARP_RETRY_SECS;
	session->default_probe_info.retry_interval.tv_usec = ARP_RETRY_USECS;
    }
    if (probe_count) {
	session->default_probe_info.probe_count = *probe_count;
    }
    else {
	session->default_probe_info.probe_count = ARP_PROBE_COUNT;
    }
    if (gratuitous_count) {
	session->default_probe_info.gratuitous_count = *gratuitous_count;
    }
    else {
	session->default_probe_info.gratuitous_count = ARP_GRATUITOUS_COUNT;
    }
#ifdef TEST_ARP_SESSION
    session->next_client_index = 1;
#endif TEST_ARP_SESSION
    return (session);
}

void
arp_session_free(arp_session_t * * session_p)
{
    arp_session_t * session = *session_p;

    dynarray_free(&session->if_sessions);
    bzero(session, sizeof(*session));
    free(session);
    *session_p = NULL;
    return;
}

static arp_if_session_t *
arp_session_find_if_session(arp_session_t * session, const char * ifn)
{
    int			count;
    int			i;

    count = dynarray_count(&session->if_sessions);
    for (i = 0; i < count; i++) {
	arp_if_session_t * 	if_session;

	if_session = dynarray_element(&session->if_sessions, i);
	if (strcmp(if_name(if_session->if_p), ifn) == 0) {
	    return (if_session);
	}
    }
    return (NULL);
}

static void
arp_if_session_free_element(void * arg)
{
    arp_if_session_t * if_session = (arp_if_session_t *)arg;

    /* free all of the clients, close file descriptor */
    dynarray_free(&if_session->clients);

    free(if_session);
    return;
}

static void
arp_if_session_free(arp_if_session_t * * if_session_p)
{
    arp_if_session_t * 		if_session = NULL;
    int 			i;
    arp_session_t * 		session;

    if (if_session_p != NULL) {
	if_session = *if_session_p;
    }
    if (if_session == NULL) {
	return;
    }

    /* remove from the list of if_sessions */
    session = if_session->session;
    i = dynarray_index(&session->if_sessions, if_session);
    if (i != -1) {
	dynarray_remove(&session->if_sessions, i, NULL);
    }
    else {
	my_log(LOG_ERR, "arp_if_session_free(%s) not in list?",
	       if_name(if_session->if_p));
    }

    /* release resources */
    arp_if_session_free_element(if_session);

    *if_session_p = NULL;
    return;
}

static arp_if_session_t *
arp_session_new_if_session(arp_session_t * session, interface_t * if_p)
{
    struct firewire_address	fw_addr;
    arp_if_session_t * 		if_session;

    if_session = arp_session_find_if_session(session, if_name(if_p));
    if (if_session != NULL) {
	return (if_session);
    }
    switch (if_ift_type(if_p)) {
    case IFT_L2VLAN:
    case IFT_IEEE8023ADLAG:
    case IFT_ETHER:
	break;
    case IFT_IEEE1394:
	/* copy in the firewire address */
	if (getFireWireAddress(if_name(if_p), &fw_addr) == FALSE) {
	    my_log(LOG_INFO, 
		   "arp_client_init(%s): could not retrieve firewire address",
		   if_name(if_p));
	    return (NULL);
	}
	break;
    default:
	my_log(LOG_INFO, "arp_client_init(%s): unsupported network type",
	       if_name(if_p));
	return (NULL);
    }
    if_session = (arp_if_session_t *)malloc(sizeof(*if_session));
    bzero(if_session, sizeof(*if_session));
    if_session->bpf_fd = -1;
    dynarray_init(&if_session->clients, arp_client_free_element, NULL);
    if (if_link_type(if_p) == IFT_IEEE1394) {
	/* copy in the fw address */
	if_session->fw_addr = fw_addr;
    }
    if_session->if_p = if_p;
    if_session->session = session;
    dynarray_add(&session->if_sessions, if_session);
    return (if_session);
}

void
arp_session_set_debug(arp_session_t * session, int debug)
{
    session->debug = debug;
    return;
}

#ifdef TEST_ARP_SESSION
#include <stdarg.h>
typedef boolean_t		func_t(int argc, const char * * argv);
typedef func_t * 		funcptr_t;

static arp_session_t *		S_arp_session;
static boolean_t 		S_debug = FALSE;
static func_t			S_do_probe;
static func_t			S_cancel_probe;
static func_t			S_toggle_debug;
static func_t			S_new_client;
static func_t			S_free_client;
static func_t			S_list;
static func_t			S_quit;
static func_t			S_client_params;
static interface_list_t *	S_interfaces;

static void
arp_session_log(int priority, const char * message, ...)
{
    va_list 		ap;

    if (priority == LOG_DEBUG) {
	if (S_arp_session->debug == FALSE)
	    return;
    }
    va_start(ap, message);
    vfprintf(stderr, message, ap);
    fprintf(stderr, "\n");
    fflush(stderr);
    return;
}

static const struct command_info {
    char *	command;
    funcptr_t	func;
    int		argc;
    char *	usage;
    int		display;
} commands[] = {
    { "new", S_new_client, 1, "<ifname>", 1 },
    { "free", S_free_client, 1, "<client_index>", 1 },
    { "probe", S_do_probe, 3, "<client_index> <sender_ip> <target_ip>", 1 },
    { "cancel", S_cancel_probe, 1, "<client_index>", 1 },
    { "params", S_client_params, 1, "<client_index> [ default | <interval> <probes> [ <gratuitous> ] ]", 1 },
    { "debug", S_toggle_debug, 0, NULL, 1 },
    { "list", S_list, 0, NULL, 1 },
    { "quit", S_quit, 0, NULL, 1 },
    { NULL, NULL, 0 }
};

struct arg_info {
    char * *	argv;
    int		argc;
    int		argv_size;
};

static void
arg_info_init(struct arg_info * args)
{
    args->argv = NULL;
    args->argv_size = 0;
    args->argc = 0;
    return;
}

static void
arg_info_free(struct arg_info * args)
{
    if (args->argv != NULL) {
	free(args->argv);
    }
    arg_info_init(args);
    return;
}

static void
arg_info_print(struct arg_info * args)
{
    int	i;

    for (i = 0; i < args->argc; i++) {
	printf("%2d. '%s'\n", i, args->argv[i]);
    }
    return;
}

static void
arg_info_add(struct arg_info * args, char * new_arg)
{
    if (args->argv == NULL) {
	args->argv_size = 6;
	args->argv = (char * *)malloc(sizeof(*args->argv) * args->argv_size);
    }
    else if (args->argc == args->argv_size) {
	args->argv_size *= 2;
	args->argv = (char * *)realloc(args->argv, 
				       sizeof(*args->argv) * args->argv_size);
    }
    args->argv[args->argc++] = new_arg;
    return;
}

void
my_CFRelease(void * t)
{
    void * * obj = (void * *)t;
    if (obj && *obj) {
	CFRelease(*obj);
	*obj = NULL;
    }
    return;
}

static int
arp_link_length(interface_t * if_p)
{
    int len;

    if (if_link_arptype(if_p) == ARPHRD_IEEE1394) {
	len = sizeof(struct firewire_eui64);
    }
    else {
	len = ETHER_ADDR_LEN;
    }
    return (len);
}


static void
arp_test(void * arg1, void * arg2, void * arg3)
{
    arp_client_t *	client = (arp_client_t *)arg1;
    arp_result_t *	result = (arp_result_t *)arg3;

    if (result->error) {
	printf("ARP probe failed: '%s'\n",
	       client->errmsg);
    }
    else if (result->in_use) {
	int i;
	int len = arp_link_length(client->if_session->if_p);
	const u_char * addr = result->hwaddr;

	printf("ip address " IP_FORMAT " in use by",
	       IP_LIST(&client->target_ip));
	for (i = 0; i < len; i++) {
	    printf("%c%02x", i == 0 ? ' ' : ':', addr[i]);
	}
	printf("\n");
    }
    else {
	printf("ip address " IP_FORMAT " is not in use\n", 
	       IP_LIST(&client->target_ip));
    }
}

static boolean_t
S_toggle_debug(int argc, const char * * argv)
{
    S_debug = !S_debug;
    arp_session_set_debug(S_arp_session, S_debug);
    if (S_debug) {
	printf("debug mode enabled\n");
    }
    else {
	printf("debug mode disabled\n");
    }
    return (TRUE);
}

static boolean_t
S_quit(int argc, const char * * argv)
{
    exit(0);
    return (TRUE);
}

static boolean_t
S_new_client(int argc, const char * * argv)
{
    arp_client_t *	client;
    interface_t *	if_p;

    if_p = ifl_find_name(S_interfaces, argv[1]);
    if (if_p == NULL) {
	fprintf(stderr, "interface %s does not exist\n", argv[1]);
	goto done;
    }
    client = arp_client_init(S_arp_session, if_p);
    if (client == NULL) {
	fprintf(stderr, "Could not create a new client over %s\n", 
		if_name(if_p));
	goto done;
    }
    printf("%d\n", client->client_index);
 done:
    return (client != NULL);
}

static boolean_t
get_int_param(const char * arg, int * ret_int)
{
    char *	endptr;
    int		val;
    
    val = strtol(arg, &endptr, 0);
    if (endptr == arg || (val == 0 && errno != 0)) {
	return (FALSE);
    }
    *ret_int = val;
    return (TRUE);
}

static boolean_t
get_client_index(const char * arg, int * client_index)
{
    if (get_int_param(arg, client_index)) {
	return (TRUE);
    }
    if (strcmp(arg, ".") == 0) {
	*client_index = S_arp_session->next_client_index - 1;
	return (TRUE);
    }
    return (FALSE);
}

static boolean_t
S_do_probe(int argc, const char * * argv)
{
    arp_client_t *	client;
    int			client_index;
    struct in_addr 	sender_ip;
    struct in_addr     	target_ip;

    if (get_client_index(argv[1], &client_index) == FALSE) {
	fprintf(stderr, "invalid client index\n");
	goto done;
    }
    client = arp_session_find_client_with_index(S_arp_session, client_index);
    if (client == NULL) {
	fprintf(stderr, "no such client index\n");
	goto done;
    }
    if (inet_aton(argv[2], &sender_ip) == 0) {
	fprintf(stderr, "invalid sender ip address %s\n", argv[2]);
	client = NULL;
	goto done;
    }
    if (inet_aton(argv[3], &target_ip) == 0) {
	fprintf(stderr, "invalid target ip address %s\n", argv[3]);
	client = NULL;
	goto done;
    }
    arp_client_probe(client, arp_test, client, NULL, sender_ip, target_ip);
 done:
    return (client != NULL);
}

static boolean_t
S_free_client(int argc, const char * * argv)
{
    arp_client_t *	client;
    int			client_index;

    if (get_client_index(argv[1], &client_index) == FALSE) {
	fprintf(stderr, "invalid client index\n");
	goto done;
    }
    client = arp_session_find_client_with_index(S_arp_session, client_index);
    if (client == NULL) {
	fprintf(stderr, "no such client index\n");
	goto done;
    }
    arp_client_free(&client);
 done:
    return (client != NULL);
}

static boolean_t
S_cancel_probe(int argc, const char * * argv)
{
    arp_client_t *	client;
    int			client_index;
    boolean_t		error = FALSE;

    if (get_client_index(argv[1], &client_index) == FALSE) {
	fprintf(stderr, "invalid client index\n");
	goto done;
    }
    client = arp_session_find_client_with_index(S_arp_session, client_index);
    if (client == NULL) {
	fprintf(stderr, "no such client index\n");
	goto done;
    }
    arp_client_cancel_probe(client);

 done:
    return (client != NULL);
}

static boolean_t
S_client_params(int argc, const char * * argv)
{
    arp_client_t *	client;
    int			client_index;
    boolean_t		error = FALSE;
    struct probe_info *	probe_info;

    if (get_client_index(argv[1], &client_index) == FALSE) {
	fprintf(stderr, "invalid client index\n");
	goto done;
    }
    client = arp_session_find_client_with_index(S_arp_session, client_index);
    if (client == NULL) {
	fprintf(stderr, "no such client index\n");
	goto done;
    }
    if (argc == 2) {
	/* display only */
    }
    else if (strcmp(argv[2], "default") == 0) {
	struct probe_info *	probe_info;

	if (argc != 3) {
	    fprintf(stderr, "Too many parameters specified\n");
	    client = NULL;
	    goto done;
	}
	client->probe_info = client->if_session->session->default_probe_info;
    }
    else if (argc < 4) {
	fprintf(stderr, "insufficient args\n");
	client = NULL;
	goto done;
    }
    else {
	char *		endptr;
	float		interval;
	int		gratuitous_count = 0;
	int		probe_count;
	struct timeval	tv;

	if (argc > 5) {
	    fprintf(stderr, "Too many parameters specified\n");
	    client = NULL;
	    goto done;
	}
	interval = strtof(argv[2], &endptr);
	if (endptr == argv[2] || (interval == 0 && errno != 0)) {
	    fprintf(stderr, "Invalid probe interval specified\n");
	    client = NULL;
	    goto done;
	}
	tv.tv_sec = (int)interval;
	tv.tv_usec = (interval - tv.tv_sec) * 1000 * 1000;
	if (get_int_param(argv[3], &probe_count) == FALSE) {
	    fprintf(stderr, "Invalid probe count specified\n");
	    client = NULL;
	    goto done;
	}
	if (argc == 5) {
	    if (get_int_param(argv[4], &gratuitous_count) == FALSE) {
		fprintf(stderr, "Invalid gratuitous count specified\n");
		client = NULL;
		goto done;
	    }
	}
	arp_client_set_probe_info(client, &tv, &probe_count,
				  &gratuitous_count);
    }
    probe_info = &client->probe_info;
    printf("Probe interval %d.%06d probes %d gratuitous %d\n",
	   probe_info->retry_interval.tv_sec, 
	   probe_info->retry_interval.tv_usec, 
	   probe_info->probe_count, probe_info->gratuitous_count);

 done:
    return (client != NULL);
}
    
static void
dump_bytes(const unsigned char * buf, int buf_len)
{
    int i;

    for (i = 0; i < buf_len; i++) {
	printf("%c%02x", i == 0 ? ' ' : ':', buf[i]);
    }
    return;
}

static boolean_t
S_list(int argc, const char * * argv)
{
    int		if_sessions_count;
    int		i;
    int		j;

    if_sessions_count = dynarray_count(&S_arp_session->if_sessions);
    for (i = 0; i < if_sessions_count; i++) {
	int			clients_count;
	arp_if_session_t *	if_session;
	int			len;

	if_session = dynarray_element(&S_arp_session->if_sessions, i);
	clients_count = dynarray_count(&if_session->clients);
	len = arp_link_length(if_session->if_p);
	for (j = 0; j < clients_count; j++) {
	    arp_client_t *	client;

	    client = dynarray_element(&if_session->clients, j);
	    printf("%d. %s", client->client_index, if_name(if_session->if_p));
	    switch (client->probe_result) {
	    case arp_probe_result_none_e:
		printf(" idle");
		break;
	    case arp_probe_result_unknown_e:
		printf(" probing %s", inet_ntoa(client->target_ip));
		break;
	    case arp_probe_result_in_use_e:
		printf(" %s in use by", 
		       inet_ntoa(client->target_ip));
		dump_bytes(client->in_use_hwaddr, len);
		break;
	    case arp_probe_result_not_in_use_e:
		printf(" %s not in use", inet_ntoa(client->target_ip));
		break;
	    case arp_probe_result_error_e:
		printf(" %s error encountered", inet_ntoa(client->target_ip));
		break;
	    }
	    printf("\n");
	}
    }
    return (FALSE);
 done:
    return (TRUE);
}

static void
parse_command(char * buf, struct arg_info * args)
{
    char *		arg_start = NULL;
    int			i;
    char *		scan;

    for (scan = buf; *scan != '\0'; scan++) {
	char ch = *scan;

	switch (ch) {
	case ' ':
	case '\n':
	case '\t':
	    *scan = '\0';
	    if (arg_start != NULL) {
		arg_info_add(args, arg_start);
		arg_start = NULL;
	    }
	    break;
	default:
	    if (arg_start == NULL) {
		arg_start = scan;
	    }
	    break;
	}
    }
    if (arg_start != NULL) {
	arg_info_add(args, arg_start);
    }
    return;
}

void
usage()
{
    int i;

    fprintf(stderr, "Available commands: ");
    for (i = 0; commands[i].command; i++) {
	if (commands[i].display) {
	    fprintf(stderr, "%s%s",  i == 0 ? "" : ", ",
		    commands[i].command);
	}
    }
    fprintf(stderr, "\n");
    return;
}

static struct command_info *
S_lookup_command(char * cmd)
{
    int i;

    for (i = 0; commands[i].command; i++) {
	if (strcmp(cmd, commands[i].command) == 0) {
	    return commands + i;
	}
    }
    return (NULL);
}

static void
process_command(struct arg_info * args)
{
    boolean_t			ok = TRUE;
    struct command_info *	cmd_info;

    cmd_info = S_lookup_command(args->argv[0]);
    if (cmd_info != NULL) {
	if (cmd_info->argc >= args->argc) {
	    ok = FALSE;
	    if (cmd_info->display) {
		fprintf(stderr, "insufficient args\nusage:\n\t%s %s\n", 
			args->argv[0],
			cmd_info->usage ? cmd_info->usage : "");
	    }
	}
    }
    else {
	ok = FALSE;
	usage();
    }

    if (ok) {
	(*cmd_info->func)(args->argc, (const char * *)args->argv);
    }
    return;
}

static void
display_prompt(FILE * f)
{
    fprintf(f, "# ");
    fflush(f);
    return;
}

static void
user_input(CFSocketRef s, CFSocketCallBackType type, 
	   CFDataRef address, const void *data, void *info)
{
    struct arg_info	args;
    char 		choice[128];

    if (fgets(choice, sizeof(choice), stdin) == NULL) {
	exit(1);
    }
    arg_info_init(&args);
    parse_command(choice, &args);
    if (args.argv == NULL) {
	goto done;
    }
    process_command(&args);

 done:
    arg_info_free(&args);
    display_prompt(stdout);
    return;
}

static void
initialize_input()
{
    CFSocketContext	context = { 0, NULL, NULL, NULL, NULL };
    CFRunLoopSourceRef	rls = NULL;
    CFSocketRef		socket = NULL;

    /* context.info = NULL; */
    socket = CFSocketCreateWithNative(NULL, fileno(stdin), 
				      kCFSocketReadCallBack,
				      user_input, &context);
    if (socket == NULL) {
	fprintf(stderr, "CFSocketCreateWithNative failed\n");
	exit(1);
    }
    rls = CFSocketCreateRunLoopSource(NULL, socket, 0);
    if (rls == NULL) {
	fprintf(stderr, "CFSocketCreateRunLoopSource failed\n");
	exit(1);
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    my_CFRelease(&rls);
    my_CFRelease(&socket);
    display_prompt(stdout);
    return;
}

int
main(int argc, char * argv[])
{
    int			gratuitous;
    FDSet_t *		readers;

#if 0
    if (argc < 4) {
	printf("usage: arptest <ifname> <sender ip> <target ip>\n");
	exit(1);
    }
#endif 0
    S_interfaces = ifl_init(FALSE);
    if (S_interfaces == NULL) {
	fprintf(stderr, "couldn't get interface list\n");
	exit(1);
    }
    readers = FDSet_init();
    if (readers == NULL) {
	fprintf(stderr, "FDSet_init failed\n");
	exit(1);
    }
    gratuitous = 0;
    S_arp_session = arp_session_init(readers, NULL, NULL, NULL, &gratuitous);
    if (S_arp_session == NULL) {
	fprintf(stderr, "arp_session_init failed\n");
	exit(1);
    }
    initialize_input();
    CFRunLoopRun();
    exit(0);
}
#endif TEST_ARP_SESSION
