/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

struct firewire_arp {
    struct arphdr 	fw_hdr;	/* fixed-size header */
    uint8_t		arp_sha[FIREWIRE_ADDR_LEN];
    uint8_t		arp_spa[4];
    uint8_t		arp_tpa[4];
};

struct arp_session {
    int				bpf_fd;
    char *			receive_buf;
    int				receive_bufsize;
    dynarray_t			clients;
    FDSet_t *			readers;
    FDCallout_t *		read_callout;
    arp_client_t *		pending_client;
    int				pending_client_try;
    timer_callout_t *		timer_callout;
    arp_our_address_func_t *	is_our_address;
    int				debug;
    struct timeval		retry;
    int				probe_count;
    int				gratuitous_count;
};

struct arp_client {
    arp_session_t *		session;
    interface_t *		if_p;
    arp_result_func_t *		func;
    void *			arg1;
    void *			arg2;
    struct in_addr		sender_ip;
    struct in_addr		target_ip;
    char			errmsg[128];
    struct firewire_address	fw_addr;
};

#define ARP_PROBE_COUNT		3
#define ARP_GRATUITOUS_COUNT	1
#define ARP_RETRY_SECS		1
#define ARP_RETRY_USECS		0

#ifdef TEST_ARP_SESSION
#define my_log		syslog
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

static int
arp_open_fd(arp_session_t * session, arp_client_t * client);

static void
arp_close_fd(arp_session_t * session);

static void
arp_start(arp_session_t * session);

static void
arp_retransmit(void * arg1, void * arg2, void * arg3);

static void
arp_read(void * arg1, void * arg2);

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

static char	link_broadcast[8] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff 
};

#ifdef TEST_ARP_SESSION
static FDSet_t *	G_readers = NULL;
#endif TEST_ARP_SESSION

static void
arp_close_fd(arp_session_t * session)
{
    if (session->read_callout) {
	/* this closes the file descriptor */
	FDSet_remove_callout(G_readers, &session->read_callout);
    }
    session->bpf_fd = -1;
    if (session->receive_buf) {
	free(session->receive_buf);
	session->receive_buf = NULL;
    }
    return;
}

/*
 * Function: arp_error
 *
 * Purpose:
 *   Calls a pending client with an error result structure.
 *
 *   This function is called from two places, arp_read and
 *   arp_retransmit.
 */
static void
arp_error(void * arg1, void * arg2, void * arg3)
{
    void *		c_arg1;
    void * 		c_arg2;
    arp_client_t * 	client;
    arp_result_func_t *	func;
    arp_session_t * 	session = (arp_session_t *)arg1;
    arp_result_t	result;
    
    client = session->pending_client;
    if (client == NULL) {
	return;
    }

    /* remember the client parameters */
    c_arg1 = client->arg1;
    c_arg2 = client->arg2;
    func = client->func;
    bzero(&result, sizeof(result));
    result.ip_address = client->target_ip;
    result.error = TRUE;

    /* clean-up, and start another client */
    session->pending_client = NULL;
    client->func = client->arg1 = client->arg2 = NULL;
    arp_start(session);

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
 * Function: arp_read
 * Purpose:
 *   Called when data is available on the bpf fd.
 *   Check the arp packet, and see if it matches
 *   the client's probe criteria.  If it does,
 *   call the client with an in_use result structure.
 */
static void
arp_read(void * arg1, void * arg2)
{
    arp_client_t *	client;
    int			hwlen;
    int			hwtype;
    int			link_header_size;
    int			link_arp_size;
    int			n;
    char *		offset;
    arp_session_t * 	session = (arp_session_t *)arg1;

    client = session->pending_client;
    if (client == NULL) {
	my_log(LOG_ERR, "arp_read: no pending clients?");
	arp_start(session);
	return;
    }
    hwtype = if_link_arptype(client->if_p);
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
    n = read(session->bpf_fd, session->receive_buf, session->receive_bufsize);
    if (n < 0) {
	if (errno == EAGAIN) {
	    return;
	}
	my_log(LOG_ERR, "arp_read: read(%s) failed, %s (%d)", 
	       if_name(client->if_p), strerror(errno), errno);
	snprintf(client->errmsg, sizeof(client->errmsg),
		 "arp_read: read(%s) failed, %s (%d)", 
	       if_name(client->if_p), strerror(errno), errno);
	goto failed;
    }
    for (offset = session->receive_buf; n > 0; ) {
	struct arphdr *		arp_p;
	struct bpf_hdr * 	bpf = (struct bpf_hdr *)offset;
	void *			c_arg1;
	void *			c_arg2;
	arp_result_func_t *	func;
	void *			hwaddr;
	short			op;
	char *			pkt_start;
	arp_result_t 		result;
	struct in_addr *	source_ip_p;
	struct in_addr *	target_ip_p;
	int			skip;
	
	dprintf(("bpf remaining %d header %d captured %d\n", n, 
		 bpf->bh_hdrlen, bpf->bh_caplen));
	pkt_start = offset + bpf->bh_hdrlen;
	arp_p = (struct arphdr *)(pkt_start + link_header_size);
	if (session->debug) {
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

	if ((client->target_ip.s_addr == source_ip_p->s_addr
	     || (op == ARPOP_REQUEST
		 && source_ip_p->s_addr == 0
		 && client->target_ip.s_addr == target_ip_p->s_addr))
	    && (*session->is_our_address)(client->if_p, 
					  hwtype,
					  hwaddr,
					  hwlen) == FALSE) {
	    /* IP is in use by some other host */
	    timer_cancel(session->timer_callout);
	    func = client->func;
	    c_arg1 = client->arg1;
	    c_arg2 = client->arg2;
	    client->func = client->arg1 = client->arg2 = NULL;
	    session->pending_client = NULL;
	    
	    /* Address is in use, fill in the structure and return it */
	    bzero(&result, sizeof(result));
	    result.in_use = TRUE;
	    result.ip_address = client->target_ip;
	    result.hwtype = hwtype;
	    result.hwlen = hwlen;
	    result.hwaddr = hwaddr;
	    (*func)(c_arg1, c_arg2, &result);
	    
	    /* start any pending client */
	    arp_start(session);
	    break;
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
    {
	/* report back an error to the caller */
	struct timeval t;
	t.tv_sec = 0;
	t.tv_usec = 100;
	timer_set_relative(session->timer_callout, t, 
			   (timer_func_t *)arp_error,
			   session, NULL, NULL);
    }
    return;
}

static int
arp_open_fd(arp_session_t * session, arp_client_t * client)
{
    int opt;
    int status;

    if (session->bpf_fd != -1) {
	/* this can't happen */
	arp_close_fd(session);
	printf("arp_open_fd called with fd already open\n");
    }
    session->bpf_fd = bpf_new();
    if (session->bpf_fd < 0) {
	my_log(LOG_ERR, "arp_open_fd: bpf_new(%s) failed, %s (%d)", 
	       if_name(client->if_p), strerror(errno), errno);
	snprintf(client->errmsg, sizeof(client->errmsg),
		 "arp_open_fd: bpf_new(%s) failed, %s (%d)", 
		 if_name(client->if_p), strerror(errno), errno);
	goto failed;
    }
    opt = 1;
    status = ioctl(session->bpf_fd, FIONBIO, &opt);
    if (status < 0) {
	my_log(LOG_ERR, "ioctl FIONBIO failed %s", strerror(errno));
	goto failed;
    }

    /* associate it with the given interface */
    status = bpf_setif(session->bpf_fd, if_name(client->if_p));
    if (status < 0) {
	my_log(LOG_ERR, "arp_open_fd: bpf_setif(%s) failed: %s (%d)", 
	       if_name(client->if_p), strerror(errno), errno);
	snprintf(client->errmsg, sizeof(client->errmsg),
		 "arp_open_fd: bpf_setif(%s) failed: %s (%d)", 
		 if_name(client->if_p), strerror(errno), errno);
	goto failed;
    }

    /* don't wait for packets to be buffered */
    bpf_set_immediate(session->bpf_fd, 1);

    /* set the filter to return only ARP packets */
    switch (if_link_type(client->if_p)) {
    default:
    case IFT_ETHER:
	status = bpf_arp_filter(session->bpf_fd, 12, ETHERTYPE_ARP,
				sizeof(struct ether_arp) 
				+ sizeof(struct ether_header));
	break;
    case IFT_IEEE1394:
	status = bpf_arp_filter(session->bpf_fd, 16, ETHERTYPE_ARP,
				sizeof(struct firewire_arp) 
				+ sizeof(struct firewire_header));
	break;
    }
    if (status < 0) {
	my_log(LOG_ERR, "arp_open_fd: bpf_arp_filter(%s) failed: %s (%d)", 
	       if_name(client->if_p), strerror(errno), errno);
	snprintf(client->errmsg, sizeof(client->errmsg),
		 "arp_open_fd: bpf_arp_filter(%s) failed: %s (%d)", 
		 if_name(client->if_p), strerror(errno), errno);
	goto failed;
    }
    /* get the receive buffer size */
    status = bpf_get_blen(session->bpf_fd, &session->receive_bufsize);
    if (status < 0) {
	my_log(LOG_ERR, 
	       "arp_open_fd: bpf_get_blen(%s) failed, %s (%d)", 
	       if_name(client->if_p), strerror(errno), errno);
	snprintf(client->errmsg, sizeof(client->errmsg),
		 "arp_open_fd: bpf_get_blen(%s) failed, %s (%d)", 
		 if_name(client->if_p), strerror(errno), errno);
	goto failed;
    }
    session->receive_buf = malloc(session->receive_bufsize);
    if (session->receive_buf == NULL) {
	my_log(LOG_ERR, 
	       "arp_open_fd: malloc failed");
	snprintf(client->errmsg, sizeof(client->errmsg),
		 "arp_open_fd: malloc failed");
	goto failed;
    }
    session->read_callout 
	= FDSet_add_callout(G_readers, session->bpf_fd,
			    arp_read, session, NULL);
    if (session->read_callout == NULL) {
	close(session->bpf_fd);
	session->bpf_fd = -1;
	goto failed;
    }
    return (1);
 failed:
    arp_close_fd(session);
    return (0);
}

static int
arp_transmit(arp_session_t * session, arp_client_t * client, 
	     boolean_t send_gratuitous)
{
    struct arphdr *		hdr;
    int 			status = 0;
    char			txbuf[128];
    int				size;

    bzero(txbuf, sizeof(txbuf));

    /* fill in the ethernet header */
    switch (if_link_arptype(client->if_p)) {
    default:
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
	    bcopy(if_link_address(client->if_p), earp->arp_sha,
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
	    bcopy(&client->fw_addr, farp->arp_sha,
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
    }

    status = bpf_write(session->bpf_fd, txbuf, size);
    if (status < 0) {
	my_log(LOG_ERR, "arp_transmit(%s) failed, %s (%d)", 
	       if_name(client->if_p), strerror(errno), errno);
	snprintf(client->errmsg, sizeof(client->errmsg),
		 "arp_transmit(%s) failed, %s (%d)", 
		 if_name(client->if_p), strerror(errno), errno);
	return (0);
    }
    return (1);
}

/*
 * Function: arp_retransmit
 *
 * Purpose:
 *   Transmit an ARP packet with timeout retry.
 *   Calls arp_error if the transmit failed.
 *   When we've tried often enough, call the
 *   client function with a result structure indicating
 *   no errors and the IP is not in use.
 */
static void
arp_retransmit(void * arg1, void * arg2, void * arg3)
{
    arp_client_t * 	client;
    arp_session_t * 	session = (arp_session_t *)arg1;
    int			tries_left;

    client = session->pending_client;
    if (client == NULL)
	return;

    tries_left = (session->probe_count + session->gratuitous_count)
	- session->pending_client_try;

    if (tries_left <= 0) {
	void *			c_arg1 = client->arg1;
	void *			c_arg2 = client->arg2;
	arp_result_func_t *	func = client->func;
	arp_result_t		result;

	client->func = client->arg1 = client->arg2 = NULL;
	session->pending_client = NULL;
	arp_start(session);

	bzero(&result, sizeof(result));
	result.ip_address = client->target_ip;
	(*func)(c_arg1, c_arg2, &result);
	return;
    }
    session->pending_client_try++;

    if (arp_transmit(session, client, 
		     (tries_left == session->gratuitous_count))) {
	timer_set_relative(session->timer_callout, session->retry, 
			   (timer_func_t *)arp_retransmit,
			   session, NULL, NULL);
    }
    else {
	/* report back an error to the caller */
	struct timeval t;
	t.tv_sec = 0;
	t.tv_usec = 100; 
	timer_set_relative(session->timer_callout, t, 
			   (timer_func_t *)arp_error,
			   session, NULL, NULL);
    }
    return;
}

/*
 * Function: arp_start
 * Purpose:
 *   Find the next client and start an arp probe.
 *   If no clients are ready, close our bpf fd
 *   and cancel any timeouts.
 */
static void
arp_start(arp_session_t * session)
{
    arp_client_t * 	next_client = NULL;
    int 		i;

    if (session->pending_client) {
	return; /* busy */
    }

    arp_close_fd(session);
    for (i = 0; i < dynarray_count(&session->clients); i++) {
	arp_client_t * 	client;
	client = dynarray_element(&session->clients, i);
	if (client->func) {
	    next_client = client;
	    break;
	}
    }
    if (next_client == NULL) {
	timer_cancel(session->timer_callout);
	return; /* no applicable clients */
    }
    session->pending_client = next_client;
    session->pending_client_try = 0;

    if (!arp_open_fd(session, next_client)) {
	/* report back an error to the caller */
	struct timeval t;
	t.tv_sec = 0;
	t.tv_usec = 100; 
	timer_set_relative(session->timer_callout, t, 
			   (timer_func_t *)arp_error,
			   session, NULL, NULL);
	return;
    }
    arp_retransmit(session, NULL, NULL);
    return;
}

arp_client_t *
arp_client_init(arp_session_t * session, interface_t * if_p)
{
    arp_client_t *		client;
    struct firewire_address	fw_addr;

    switch (if_link_type(if_p)) {
    case IFT_ETHER:
	break;
    case IFT_IEEE1394:
	/* copy in the firewire address */
	if (getFireWireAddress(if_name(if_p), &fw_addr) == FALSE) {
	    my_log(LOG_DEBUG, 
		   "arp_client_init(%s): could not retrieve firewire address",
		   if_name(if_p));
	    return (NULL);
	}
	break;
    default:
	my_log(LOG_DEBUG, "arp_client_init(%s): unsupported network type",
	       if_name(if_p));
	return (NULL);
    }
    client = malloc(sizeof(*client));
    if (client == NULL) {
	return (NULL);
    }
    bzero(client, sizeof(*client));
    client->if_p = if_p;
    if (dynarray_add(&session->clients, client) == FALSE) {
	free(client);
	return (NULL);
    }
    client->session = session;

    if (if_link_type(if_p) == IFT_IEEE1394) {
	/* copy in the hw address */
	client->fw_addr = fw_addr;
    }
    return (client);
}

void
arp_client_free(arp_client_t * * client_p)
{
    arp_session_t * session;
    arp_client_t * client = *client_p;
    int i;

    if (client == NULL)
	return;

    session = client->session;
    arp_cancel_probe(client);

    i = dynarray_index(&session->clients, client);
    if (i == -1) {
	return;
    }
    dynarray_remove(&session->clients, i, NULL);
    free(client);
    *client_p = NULL;
    return;
}

void
arp_probe(arp_client_t * client,
	  arp_result_func_t * func, void * arg1, void * arg2,
	  struct in_addr sender_ip, struct in_addr target_ip)
{
    arp_session_t * session = client->session;
    client->sender_ip = sender_ip;
    client->target_ip = target_ip;
    client->func = func;
    client->arg1 = arg1;
    client->arg2 = arg2;
    client->errmsg[0] = '\0';
    arp_start(session);
    return;
}

char *
arp_client_errmsg(arp_client_t * client)
{
    return (client->errmsg);
}

void
arp_cancel_probe(arp_client_t * client)
{
    arp_session_t * session = client->session;
    client->errmsg[0] = '\0';
    client->func = client->arg1 = client->arg2 = NULL;
    if (client == session->pending_client) {
	session->pending_client = NULL;
	arp_start(session);
    }
    return;
}

arp_session_t *
arp_session_init(arp_our_address_func_t * func, struct timeval * tv_p,
		 int * probe_count, int * gratuitous_count)
{
    arp_session_t * 	session;
    timer_callout_t *	timer;

    timer = timer_callout_init();
    if (timer == NULL)
	return (NULL);
    session = malloc(sizeof(*session));
    if (session == NULL) {
	timer_callout_free(&timer);
	return (NULL);
    }
    bzero(session, sizeof(*session));
    dynarray_init(&session->clients, free, NULL);
    session->bpf_fd = -1;
    session->timer_callout = timer;
    if (func == NULL) {
	session->is_our_address = arp_is_our_address;
    }
    else {
	session->is_our_address = func;
    }
    if (tv_p) {
	session->retry = *tv_p;
    }
    else {
	session->retry.tv_sec = ARP_RETRY_SECS;
	session->retry.tv_usec = ARP_RETRY_USECS;
    }
    if (probe_count) {
	session->probe_count = *probe_count;
    }
    else {
	session->probe_count = ARP_PROBE_COUNT;
    }
    if (gratuitous_count) {
	session->gratuitous_count = *gratuitous_count;
    }
    else {
	session->gratuitous_count = ARP_GRATUITOUS_COUNT;
    }
    return (session);
}

void
arp_session_free(arp_session_t * * session_p)
{
    arp_session_t * session = *session_p;

    dynarray_free(&session->clients);

    arp_close_fd(session);

    /* cancel the timer callout */
    if (session->timer_callout) {
	timer_callout_free(&session->timer_callout);
    }
    bzero(session, sizeof(*session));
    free(session);
    *session_p = NULL;
    return;
}

void
arp_session_set_debug(arp_session_t * session, int debug)
{
    session->debug = debug;
    return;
}

#ifdef TEST_ARP_SESSION

void
arp_test(void * arg1, void * arg2, void * arg3)
{
    arp_client_t *	client = (arp_client_t *)arg1;
    arp_result_t *	result = (arp_result_t *)arg3;

    if (result->error) {
	printf("ARP probe failed: '%s'\n",
	       client->errmsg);
	exit(1);
    }
    if (result->in_use) {
	int i;
	int len;
	u_char * addr = result->hwaddr;

	if (result->hwtype == ARPHRD_IEEE1394) {
	    len = sizeof(struct firewire_eui64);
	}
	else {
	    len = result->hwlen;
	}

	printf("ip address " IP_FORMAT " is in use by",
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
    exit(0);
}

int
main(int argc, char * argv[])
{
    interface_t *	if_p;
    interface_list_t *	list_p;
    arp_session_t *	p;
    arp_client_t *	c;
    struct in_addr 	sender_ip;
    struct in_addr     	target_ip;

    if (argc < 4) {
	printf("usage: arptest <ifname> <sender ip> <target ip>\n");
	exit(1);
    }

    list_p = ifl_init(FALSE);
    if (list_p == NULL) {
	fprintf(stderr, "couldn't get interface list\n");
	exit(1);
    }

    if_p = ifl_find_name(list_p, argv[1]);
    if (if_p == NULL) {
	fprintf(stderr, "interface %s does not exist\n", argv[1]);
	exit(1);
    }

    G_readers = FDSet_init();
    if (G_readers == NULL) {
	fprintf(stderr, "FDSet_init failed\n");
	exit(1);
    }

    {
	int gratuitous = 0;
	p = arp_session_init(NULL, NULL, NULL, &gratuitous);
    }
    if (p == NULL) {
	fprintf(stderr, "arp_session_init failed\n");
	exit(1);
    }
    arp_session_set_debug(p, 1);
    if (inet_aton(argv[2], &sender_ip) == 0) {
	fprintf(stderr, "invalid ip address %s\n", argv[3]);
	exit(1);
    }
    if (inet_aton(argv[3], &target_ip) == 0) {
	fprintf(stderr, "invalid ip address %s\n", argv[4]);
	exit(1);
    }
    
    c = arp_client_init(p, if_p);
    if (c == NULL) {
	fprintf(stderr, "arp_client_init %s\n", if_name(if_p));
	exit(1);
    }
    arp_probe(c, arp_test, c, NULL, sender_ip, target_ip);
    CFRunLoopRun();
    exit(0);
}
#endif TEST_ARP_SESSION
