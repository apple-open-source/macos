/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
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
 */

#import <stdlib.h>
#import <unistd.h>
#import <string.h>
#import <stdio.h>
#import <sys/types.h>
#import <sys/wait.h>
#import <sys/errno.h>
#import <sys/socket.h>
#import <sys/ioctl.h>
#import <sys/time.h>
#import <sys/sockio.h>
#import <net/if.h>
#import <net/etherdefs.h>
#import <netinet/in.h>
#import <netinet/udp.h>
#import <netinet/in_systm.h>
#import <netinet/ip.h>
#import <net/if_arp.h>
#import <net/etherdefs.h>
#import <netinet/if_ether.h>
#import <arpa/inet.h>
#import <net/if_types.h>
#import <net/bpf.h>
#import "util.h"
#import "ts_log.h"

#import "bpflib.h"
#import "util.h"
#import "dprintf.h"
#import "dynarray.h"
#import "timer.h"
#import "FDSet.h"
#import "ipconfigd_globals.h"
#import "arp_session.h"

extern char *  			ether_ntoa(struct ether_addr *e);
extern struct ether_addr *	ether_aton(char *);

/* local functions: */
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
dump_arp(struct ether_arp * earp)
{
    printf("\nGot ARP packet\n");
    printf("%s type=0x%x proto=0x%x\n", arpop_name(ntohs(earp->ea_hdr.ar_op)),
	   ntohs(earp->ea_hdr.ar_hrd), ntohs(earp->ea_hdr.ar_pro));

    if (earp->ea_hdr.ar_hln == sizeof(earp->arp_sha)) {
	printf("Sender H/W\t%s\n", 
	       ether_ntoa((struct ether_addr *)earp->arp_sha));
	printf("Target H/W\t%s\n", 
	       ether_ntoa((struct ether_addr *)earp->arp_tha));
    }
    printf("Sender IP\t%s\n", inet_ntoa(*((struct in_addr *)earp->arp_spa)));
    printf("Target IP\t%s\n", inet_ntoa(*((struct in_addr *)earp->arp_tpa)));
    return;
}

static struct ether_addr ether_broadcast = { 
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff} 
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
    arp_session_t * 	session = (arp_session_t *)arg1;
    arp_client_t *	client = session->pending_client;
    int			n;

    if (client == NULL) {
	ts_log(LOG_ERR, "arp_read: no pending clients?");
	arp_close_fd(session);
	arp_start(session);
	return;
    }
    n = read(session->bpf_fd, session->receive_buf, session->receive_bufsize);
    if (n < 0) {
	ts_log(LOG_ERR, "arp_read: read(%s) failed, %s (%d)", 
	       if_name, strerror(errno), errno);
	sprintf(client->errmsg, "arp_read: read(%s) failed, %s (%d)", 
	       if_name(client->if_p), strerror(errno), errno);
	goto failed;
    }
    if (n > 0) {
	void *			c_arg1;
	void *			c_arg2;
	struct bpf_hdr * 	bpf = (struct bpf_hdr *)session->receive_buf;
	struct ether_arp *	earp;
	arp_result_func_t *	func;
	char *			pkt_start;
	arp_result_t 		result;
	
	dprintf(("bpf read %d captured %d\n", n, bpf->bh_caplen));
	    
	pkt_start = session->receive_buf + bpf->bh_hdrlen;
	earp = (struct ether_arp *)(pkt_start + sizeof(struct ether_header));
	if (session->debug) {
	    dump_arp(earp);
	}
	if (ntohs(earp->ea_hdr.ar_op) != ARPOP_REPLY
	    || ntohs(earp->ea_hdr.ar_hrd) != ARPHRD_ETHER
	    || ntohs(earp->ea_hdr.ar_pro) != ETHERTYPE_IP) {
	    return;
	}
	
	if (client->target_ip.s_addr 
	    != ((struct in_addr *)earp->arp_spa)->s_addr)
	    return;
	if (bcmp(earp->arp_tha, if_link_address(client->if_p), 
		 sizeof(earp->arp_tha)))
	    return;
	if (client->sender_ip.s_addr 
	    != ((struct in_addr *)earp->arp_tpa)->s_addr)
	    return;
	
	/* clean-up before calling the client */
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
	result.hwtype = ntohs(earp->ea_hdr.ar_hrd);
	result.hwlen = earp->ea_hdr.ar_hln;
	result.hwaddr = earp->arp_sha;
	(*func)(c_arg1, c_arg2, &result);

	/* start any pending client */
	arp_start(session);
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
    int status;

    if (session->bpf_fd < 0) {
	session->bpf_fd = bpf_new();
	if (session->bpf_fd < 0) {
	    ts_log(LOG_ERR, "arp_open_fd: bpf_new(%s) failed, %s (%d)", 
		   if_name(client->if_p), strerror(errno), errno);
	    sprintf(client->errmsg, "arp_open_fd: bpf_new(%s) failed, %s (%d)", 
		   if_name(client->if_p), strerror(errno), errno);
	    goto failed;
	}

	/* don't wait for packets to be buffered */
	bpf_set_immediate(session->bpf_fd, 1);

	/* set the filter to return only ARP packets */
	status = bpf_arp_filter(session->bpf_fd);
	if (status < 0) {
	    ts_log(LOG_ERR, "arp_open_fd: bpf_arp_filter(%s) failed: %s (%d)", 
		   if_name(client->if_p), strerror(errno), errno);
	    sprintf(client->errmsg, 
		    "arp_open_fd: bpf_arp_filter(%s) failed: %s (%d)", 
		   if_name(client->if_p), strerror(errno), errno);
	    goto failed;
	}
	/* get the receive buffer size */
	status = bpf_get_blen(session->bpf_fd, &session->receive_bufsize);
	if (status < 0) {
	    ts_log(LOG_ERR, 
		   "arp_open_fd: bpf_get_blen(%s) failed, %s (%d)", 
		   if_name(client->if_p), strerror(errno), errno);
	    sprintf(client->errmsg,
		   "arp_open_fd: bpf_get_blen(%s) failed, %s (%d)", 
		   if_name(client->if_p), strerror(errno), errno);
	    goto failed;
	}
	session->receive_buf = malloc(session->receive_bufsize);
	if (session->receive_buf == NULL) {
	    ts_log(LOG_ERR, 
		   "arp_open_fd: malloc failed");
	    sprintf(client->errmsg, "arp_open_fd: malloc failed");
	    goto failed;
	}
	session->read_callout 
	    = FDSet_add_callout(G_readers, session->bpf_fd,
				arp_read, session, NULL);
    }
    /* associate it with the given interface */
    status = bpf_setif(session->bpf_fd, if_name(client->if_p));
    if (status < 0) {
	ts_log(LOG_ERR, "arp_open_fd: bpf_setif(%s) failed: %s (%d)", 
	       if_name, strerror(errno), errno);
	sprintf(client->errmsg, "arp_open_fd: bpf_setif(%s) failed: %s (%d)", 
	       if_name(client->if_p), strerror(errno), errno);
	goto failed;
    }
    return (1);
 failed:
    arp_close_fd(session);
    return (0);
}

static int
arp_transmit(arp_session_t * session, arp_client_t * client)
{
    struct ether_header*eh_p;
    struct ether_arp *	earp;
    struct arphdr *	hdr;
    int 		status = 0;
    char		txbuf[128];

    if (!arp_open_fd(session, client))
	return (0); /* error */

    bzero(txbuf, sizeof(txbuf));

    /* fill in the ethernet header */
    eh_p = (struct ether_header *)txbuf;
    bcopy(&ether_broadcast, eh_p->ether_dhost,
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
    *((struct in_addr *)earp->arp_spa) = client->sender_ip;
    *((struct in_addr *)earp->arp_tpa) = client->target_ip;

    status = bpf_write(session->bpf_fd, txbuf, sizeof(*eh_p) + sizeof(*earp));
    if (status < 0) {
	ts_log(LOG_ERR, "arp_transmit(%s) failed, %s (%d)", 
	       if_name, strerror(errno), errno);
	sprintf(client->errmsg, "arp_transmit(%s) failed, %s (%d)", 
	       if_name(client->if_p), strerror(errno), errno);
	return (0);
    }
    return (1);
}

/*
 * Function: arp_retransmit
 *
 * Purpose:
 *   Transmit an ARP packet up to ARP_PROBE_TRIES times.
 *   Calls arp_error if the transmit failed.
 *   When we've tried ARP_PROBE_TRIES times, call the
 *   client function with a result structure indicating
 *   no errors and the IP is not in use.
 */
static void
arp_retransmit(void * arg1, void * arg2, void * arg3)
{
    arp_client_t * 	client;
    arp_session_t * 	session = (arp_session_t *)arg1;

    client = session->pending_client;
    if (client == NULL)
	return;

    if (session->pending_client_try == ARP_PROBE_TRIES) {
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

    if (arp_transmit(session, client)) {
	struct timeval t;

	t.tv_sec = ARP_PROBE_TRY_INTERVAL_SECS;
	t.tv_usec = ARP_PROBE_TRY_INTERVAL_USECS;
	timer_set_relative(session->timer_callout, t, 
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

    if (session->pending_client)
	return; /* busy */

    for (i = 0; i < dynarray_count(&session->clients); i++) {
	arp_client_t * 	client;
	client = dynarray_element(&session->clients, i);
	if (client->func) {
	    next_client = client;
	    break;
	}
    }
    if (next_client == NULL) {
	arp_close_fd(session);
	timer_cancel(session->timer_callout);
	return; /* no applicable clients */
    }
    session->pending_client = next_client;
    session->pending_client_try = 0;
    arp_retransmit(session, NULL, NULL);
    return;
}

arp_client_t *
arp_client_init(arp_session_t * session, interface_t * if_p)
{
    arp_client_t *	client;

    if (if_link_arptype(if_p) != ARPHRD_ETHER) {
	ts_log(LOG_DEBUG, "arp_client_init(%s): not ethernet",
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
arp_session_init()
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
	printf("ip address " IP_FORMAT " is in use by " EA_FORMAT "\n", 
	       IP_LIST(&client->target_ip), 
	       EA_LIST((char *)result->hwaddr));
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
	printf("usage: arpcheck <ifname> <sender ip> <target ip>\n");
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

    p = arp_session_init();
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
