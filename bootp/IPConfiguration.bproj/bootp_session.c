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
 * bootp_session.c
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


#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <sys/filio.h>
#include <ctype.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/bootp.h>
#include <arpa/inet.h>
#include <net/if_types.h>
#include "dhcp_options.h"
#include "util.h"
#include <syslog.h>

#include "dhcplib.h"
#include "dynarray.h"
#include "bootp_session.h"
#include "bootp_transmit.h"
#include "ipconfigd_globals.h"

static void
bootp_session_read(void * arg1, void * arg2);

static int
S_get_bootp_socket(u_short client_port)
{
    struct sockaddr_in 		me;
    int 			status;
    int 			opt;
    int				sockfd;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
	perror("socket");
	return (-1);
    }
    bzero((char *)&me, sizeof(me));
    me.sin_family = AF_INET;
    me.sin_port = htons(client_port);
    me.sin_addr.s_addr = htonl(INADDR_ANY);
    
    status = bind(sockfd, (struct sockaddr *)&me, sizeof(me));
    if (status != 0) {
	my_log(LOG_ERR, "bootp_session: bind port %d failed",
	       client_port);
	goto failed;
    }
    opt = 1;
    status = setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &opt, 
			sizeof(opt));
    if (status < 0) {
	my_log(LOG_ERR, "setsockopt SO_BROADCAST");
	goto failed;
    }
    opt = 1;
    status = ioctl(sockfd, FIONBIO, &opt);
    if (status < 0) {
	my_log(LOG_ERR, "ioctl FIONBIO failed %s", strerror(errno));
	goto failed;
    }

    return sockfd;

 failed:
    if (sockfd >= 0)
	close(sockfd);
    return (-1);
}

bootp_client_t *
bootp_client_init(bootp_session_t * session)
{
    bootp_client_t *	client;

    client = malloc(sizeof(*client));
    if (client == NULL)
	return (NULL);
    bzero(client, sizeof(*client));

    if (dynarray_count(&session->clients) == 0) {
	session->sockfd = S_get_bootp_socket(session->client_port);
	if (session->sockfd >= 0) {
	    /* register as a reader */
	    session->callout = FDSet_add_callout(G_readers,
						 session->sockfd,
						 bootp_session_read,
						 session, NULL);
	}
	else {
	    free(client);
	    return (NULL);
	}
	    
    }
    if (dynarray_add(&session->clients, client) == FALSE) {
	free(client);
	return (NULL);
    }
    client->session = session;
    return (client);
}

void
bootp_client_free(bootp_client_t * * client_p)
{
    bootp_session_t * session;
    bootp_client_t * client = *client_p;
    int i;

    if (client == NULL)
	return;

    session = client->session;
    i = dynarray_index(&session->clients, client);
    if (i == -1)
	return;
    dynarray_remove(&session->clients, i, NULL);
    if (dynarray_count(&session->clients) == 0) {
	if (session->callout != NULL) {
	    /* this closes our socket */
	    FDSet_remove_callout(G_readers, &session->callout);
	}
	session->sockfd = -1;
    }
    free(client);
    *client_p = NULL;
    return;
}

void
bootp_client_enable_receive(bootp_client_t * client,
			    bootp_receive_func_t * func, 
			    void * arg1, void * arg2)
{
    client->receive = func;
    client->receive_arg1 = arg1;
    client->receive_arg2 = arg2;
    return;
}

void
bootp_client_disable_receive(bootp_client_t * client)

{
    client->receive = NULL;
    client->receive_arg1 = NULL;
    client->receive_arg2 = NULL;
    return;
}

int
bootp_client_transmit(bootp_client_t * client,
		      char * if_name, int hwtype, void * hwaddr, int hwlen,
		      struct in_addr dest_ip,
		      struct in_addr src_ip,
		      u_short dest_port,
		      u_short src_port,
		      void * data, int len)
{
    bootp_session_t *	session = client->session;
    return (bootp_transmit(session->sockfd, session->send_buf, 
			   if_name, hwtype, hwaddr, hwlen,
			   dest_ip, src_ip, dest_port, src_port, data, len));
}
		       


bootp_session_t *
bootp_session_init(u_short client_port)
{
    bootp_session_t * session = malloc(sizeof(*session));
    if (session == NULL)
	return (NULL);
    bzero(session, sizeof(*session));
    dynarray_init(&session->clients, free, NULL);
    session->sockfd = -1;
    session->client_port = client_port;
    return (session);
}

void
bootp_session_free(bootp_session_t * * session_p)
{
    bootp_session_t * session = *session_p;

    dynarray_free(&session->clients);
    if (session->callout != NULL) {
	FDSet_remove_callout(G_readers, &session->callout);
    }
    bzero(session, sizeof(*session));
    free(session);
    *session_p = NULL;
    return;
}

static void
bootp_session_deliver(bootp_session_t * session, char * data, int size)
{
    bootp_receive_data_t	event;
    int 			i;

    if (size < sizeof(struct dhcp)) {
	return;
    }

    if (session->debug) {
	printf("\nReceived packet of size %d\n", size);
	dhcp_print_packet((struct dhcp *)data, size);
    }

    bzero(&event, sizeof(event));
    event.data = (struct dhcp *)data;
    event.size = size;
    dhcpol_parse_packet(&event.options, (struct dhcp *)data, size, NULL);
    for (i = 0; i < dynarray_count(&session->clients); i++) {
	bootp_client_t *	client;

	client = dynarray_element(&session->clients, i);
	
	if (client->receive) {
	    (*client->receive)(client->receive_arg1, client->receive_arg2,
			       &event);
	}
    }
    dhcpol_free(&event.options); /* free malloc'd data */
    return;
}

static void
bootp_session_read(void * arg1, void * arg2)
{
    bootp_session_t * 		session = (bootp_session_t *)arg1;
    int 			n;
    struct sockaddr_in 		from;
    int 			fromlen;

    n = recvfrom(session->sockfd, session->receive_buf,
		 sizeof(session->receive_buf), 0,
		 (struct sockaddr *)&from, &fromlen);
    if (n < 0) {
	if (errno != EAGAIN) {
	    my_log(LOG_ERR, "bootp_session_read(): recvfrom %s", 
		   strerror(errno));
	}
    }
    else if (n > 0) {
	bootp_session_deliver(session, session->receive_buf, n);
    }
}

void
bootp_session_set_debug(bootp_session_t * session, int debug)
{
    session->debug = debug;
    return;
}
