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
 * arp_session.h
 */

/* 
 * Modification History
 *
 * May 11, 2000		Dieter Siegmund (dieter@apple.com)
 * - created
 */

#import "FDSet.h"
#import "interfaces.h"

/*
 * Type: arp_result_func_t
 * Purpose:
 *   Called to send results back to the caller.  The first two args are
 *   supplied by the client, the third is a pointer to an arp_result_t.
 */
typedef void (arp_result_func_t)(void * arg1, void * arg2, void * arg3);

#define ARP_PROBE_TRIES				3
#define ARP_PROBE_TRY_INTERVAL_SECS		0
#define ARP_PROBE_TRY_INTERVAL_USECS		(700 * 1000)
	
typedef struct arp_client arp_client_t;

typedef struct {
    int				bpf_fd;
    char *			receive_buf;
    int				receive_bufsize;
    dynarray_t			clients;
    FDSet_t *			readers;
    FDCallout_t *		read_callout;
    arp_client_t *		pending_client;
    int				pending_client_try;
    timer_callout_t *		timer_callout;
    int				debug;
} arp_session_t;

struct arp_client 
{
    arp_session_t *		session;
    char			if_name[IFNAMSIZ + 1];
    interface_t *		if_p;
    arp_result_func_t *		func;
    void *			arg1;
    void *			arg2;
    struct in_addr		sender_ip;
    struct in_addr		target_ip;
    char			errmsg[1024];
};


typedef struct {
    boolean_t			error;
    boolean_t			in_use;
    struct in_addr		ip_address;
    int				hwtype;
    void *			hwaddr;
    int				hwlen;
} arp_result_t;

arp_session_t * 
arp_session_init();

void
arp_session_free(arp_session_t * * session_p);

void
arp_session_set_debug(arp_session_t * session, int debug);


/**
 ** arp client functions
 **/
arp_client_t *
arp_client_init(arp_session_t * session, interface_t * if_p);

void
arp_client_free(arp_client_t * * client_p);

char *
arp_client_errmsg(arp_client_t * client);

void
arp_probe(arp_client_t * client,
	  arp_result_func_t * func, void * arg1, void * arg2,
	  struct in_addr sender_ip, struct in_addr target_ip);

void
arp_cancel_probe(arp_client_t * client);
