
#ifndef _S_BOOTP_SESSION_H
#define _S_BOOTP_SESSION_H

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
 * bootp_session.h
 * - maintain BOOTP client socket session
 * - maintain list of BOOTP clients
 * - distribute packet reception to enabled clients
 */

/* 
 * Modification History
 *
 * May 10, 2000		Dieter Siegmund (dieter@apple.com)
 * - created
 */

#include "FDSet.h"

/*
 * Type: bootp_receive_func_t
 * Purpose:
 *   Called to deliver data to the client.  The first two args are
 *   supplied by the client, the third is a pointer to a bootp_data_t.
 */
typedef void (bootp_receive_func_t)(void * arg1, void * arg2, void * arg3);

typedef struct {
    dynarray_t			clients;
    int				sockfd;
    u_short			client_port;
    char 			receive_buf[2048];
    char 			send_buf[2048];
    int 			ip_id;
    FDSet_t *			readers;
    FDCallout_t *		callout;
    int				debug;
} bootp_session_t;

typedef struct {
    bootp_session_t *		session; /* pointer to parent */
    bootp_receive_func_t *	receive;
    void *			receive_arg1;
    void *			receive_arg2;
} bootp_client_t;

typedef struct {
    struct dhcp  *		data;
    int				size;
    dhcpol_t			options;
} bootp_receive_data_t;

bootp_client_t *
bootp_client_init(bootp_session_t * slist);

void
bootp_client_free(bootp_client_t * * session);

void
bootp_client_enable_receive(bootp_client_t * client,
			    bootp_receive_func_t * func, 
			    void * arg1, void * arg2);

void
bootp_client_disable_receive(bootp_client_t * client);

int
bootp_client_transmit(bootp_client_t * client,
		      char * if_name, 
		      int hwtype, void * hwaddr, int hwlen,
		      struct in_addr dest_ip,
		      struct in_addr src_ip,
		      u_short dest_port,
		      u_short src_port,
		      void * data, int len);

bootp_session_t * 
bootp_session_init(u_short client_port);

void
bootp_session_set_debug(bootp_session_t * slist, int debug);

void
bootp_session_free(bootp_session_t * * slist);

#endif _S_BOOTP_SESSION_H
