/*
 * Copyright (c) 1999, 2000 Apple Computer, Inc. All rights reserved.
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
 * bootp.c
 * - BOOTP configuration thread
 * - contains bootp_thread()
 * - configures an interface's address using BOOTP
 * - once the address is retrieved, the client probes the address using
 *   ARP to ensure that another client isn't already using the address
 */
/* 
 * Modification History
 *
 * May 9, 2000		Dieter Siegmund (dieter@apple.com)
 * - reworked to fit within the new event-driven framework
 *
 * October 4, 2000	Dieter Siegmund (dieter@apple.com)
 * - added code to unpublish interface state if the link goes
 *   down and stays down for more than 4 seconds
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
#import <sys/sockio.h>
#import <ctype.h>
#import <net/if.h>
#import <net/etherdefs.h>
#import <netinet/in.h>
#import <netinet/udp.h>
#import <netinet/in_systm.h>
#import <netinet/ip.h>
#import <netinet/bootp.h>
#import <arpa/inet.h>
#import <net/if_types.h>

#import "rfc_options.h"
#import "dhcp_options.h"
#import "dhcp.h"
#import "interfaces.h"
#import "util.h"
#import "ts_log.h"
#import "host_identifier.h"
#import "dhcplib.h"

#import "dprintf.h"

#import "ipconfigd_threads.h"

extern char *  			ether_ntoa(struct ether_addr *e);

typedef struct {
    boolean_t			gathering;
    struct bootp		request;
    struct saved_pkt		saved;
    long			start_secs;
    int				try;
    dhcp_time_secs_t		wait_secs;
    u_long			xid;
    timer_callout_t *		timer;
    arp_client_t *		arp;
    bootp_client_t *		client;
    boolean_t			user_warned;
} IFState_bootp_t;

/* tags_search: these are the tags we look for using BOOTP */
static u_char       	bootp_params[] = { 
    dhcptag_host_name_e,
    dhcptag_subnet_mask_e, 
    dhcptag_router_e,
    dhcptag_domain_name_server_e,
    dhcptag_domain_name_e,
};
int			n_bootp_params = sizeof(bootp_params) 
				        / sizeof(bootp_params[0]);
#define IDEAL_RATING	n_bootp_params

static void 
bootp_request(IFState_t * ifstate, IFEventID_t evid, void * event_data);

/*
 * Function: make_bootp_request
 * Purpose:
 *   Create a "blank" bootp packet.
 */
static void
make_bootp_request(struct bootp * pkt, 
		   u_char * hwaddr, u_char hwtype, u_char hwlen)
{
    bzero(pkt, sizeof (*pkt));
    pkt->bp_op = BOOTREQUEST;
    pkt->bp_htype = hwtype;
    pkt->bp_hlen = hwlen;
    if (G_must_broadcast)
	pkt->bp_unused = htons(DHCP_FLAGS_BROADCAST);
    bcopy(hwaddr, pkt->bp_chaddr, hwlen);
    bcopy(G_rfc_magic, pkt->bp_vend, sizeof(G_rfc_magic));
    pkt->bp_vend[4] = dhcptag_end_e;
    return;
}

static void
S_cancel_pending_events(IFState_t * ifstate)
{
    IFState_bootp_t *	bootp = (IFState_bootp_t *)ifstate->private;

    if (bootp == NULL)
	return;
    if (bootp->timer) {
	timer_cancel(bootp->timer);
    }
    if (bootp->client) {
	bootp_client_disable_receive(bootp->client);
    }
    if (bootp->arp) {
	arp_cancel_probe(bootp->arp);
    }
    return;
}

static void
bootp_success(IFState_t * ifstate)
{
    IFState_bootp_t *	bootp = (IFState_bootp_t *)ifstate->private;
    int			len;
    struct in_addr	mask = {0};
    void *		option;
    struct bootp *	reply = (struct bootp *)bootp->saved.pkt;

    S_cancel_pending_events(ifstate);
    option = dhcpol_find(&bootp->saved.options, dhcptag_subnet_mask_e,
			 &len, NULL);
    if (option)
	mask = *((struct in_addr *)option);

    if (bootp->saved.our_ip.s_addr
	&& reply->bp_yiaddr.s_addr != bootp->saved.our_ip.s_addr) {
	(void)inet_remove(ifstate, &bootp->saved.our_ip);
    }
    bootp->saved.our_ip = reply->bp_yiaddr;
    (void)inet_add(ifstate, &bootp->saved.our_ip, &mask, NULL);
    ifstate_publish_success(ifstate, bootp->saved.pkt, bootp->saved.pkt_size);
    return;
}

static void
bootp_failed(IFState_t * ifstate, ipconfig_status_t status, char * msg)
{
    IFState_bootp_t *	bootp = (IFState_bootp_t *)ifstate->private;
    struct timeval	tv;

    S_cancel_pending_events(ifstate);
    dhcpol_free(&bootp->saved.options);

    if (bootp->saved.our_ip.s_addr) {
	(void)inet_remove(ifstate, &bootp->saved.our_ip);
    }
    bootp->saved.our_ip.s_addr = 0;

    /* remove the all-zeroes IP address */
    (void)inet_remove(ifstate, &G_ip_zeroes);
    ifstate_publish_failure(ifstate, status, msg);

    /* retry BOOTP again in a bit */
#define RETRY_INTERVAL_SECS      (2 * 60)
    tv.tv_sec = RETRY_INTERVAL_SECS;
    tv.tv_usec = 0;
    timer_set_relative(bootp->timer, tv, 
		       (timer_func_t *)bootp_request,
		       ifstate, (void *)IFEventID_start_e, NULL);

    return;
}

static void
bootp_arp_probe(IFState_t * ifstate,  IFEventID_t evid, void * event_data)
{
    IFState_bootp_t *	bootp = (IFState_bootp_t *)ifstate->private;

    switch (evid) {
      case IFEventID_start_e: {
	  struct bootp *	reply = (struct bootp *)bootp->saved.pkt;

	  ts_log(LOG_DEBUG, "BOOTP %s: ended at %d", if_name(ifstate->if_p), 
		 timer_current_secs() - bootp->start_secs);
	  /* don't need the all-zeroes address anymore */
	  (void)inet_remove(ifstate, &G_ip_zeroes);
	  bootp_client_disable_receive(bootp->client);
	  timer_cancel(bootp->timer);
	  arp_cancel_probe(bootp->arp);
	  arp_probe(bootp->arp, 
		    (arp_result_func_t *)bootp_arp_probe, ifstate,
		    (void *)IFEventID_arp_e, G_ip_zeroes,
		    reply->bp_yiaddr);
	  return;
	  break;
      }
      case IFEventID_arp_e: {
	  arp_result_t *	result = (arp_result_t *)event_data;

	  if (result->error) {
	      ts_log(LOG_ERR, "BOOTP %s: arp probe failed, %s", 
		     if_name(ifstate->if_p),
		     arp_client_errmsg(bootp->arp));
	      /* continue without it anyways */
	  }
	  else {
	      struct bootp *	reply = (struct bootp *)bootp->saved.pkt;
	      if (result->in_use) {
		  char	msg[128];
		  
		  snprintf(msg, sizeof(msg),
			   IP_FORMAT " in use by " 
			   EA_FORMAT ", BOOTP Server " IP_FORMAT,
			   IP_LIST(&reply->bp_yiaddr),
			   EA_LIST(result->hwaddr),
			   IP_LIST(&reply->bp_siaddr));
		  if (bootp->user_warned == FALSE) {
		      ifstate_tell_user(ifstate, msg);
		      bootp->user_warned = TRUE;
		  }
		  syslog(LOG_ERR, "BOOTP %s: %s", if_name(ifstate->if_p), msg);
		  bootp_failed(ifstate, ipconfig_status_address_in_use_e,
			       msg);
		  break;
	      }
	  }
	  bootp_success(ifstate);
	  break;
      }
      default:
	  break;
    }
    return;
}

static void 
bootp_request(IFState_t * ifstate, IFEventID_t evid, void * event_data)
{
    IFState_bootp_t *	bootp = (IFState_bootp_t *)ifstate->private;
    struct timeval 	tv;

    switch (evid) {
      case IFEventID_start_e: 
	  ts_log(LOG_DEBUG, "BOOTP %s: starting", if_name(ifstate->if_p));
	  (void)inet_add(ifstate, &G_ip_zeroes, NULL, NULL);
	  S_cancel_pending_events(ifstate);
	  bootp->start_secs = timer_current_secs();
	  bootp->wait_secs = INITIAL_WAIT_SECS;
	  bootp->gathering = FALSE;
	  bootp->saved.pkt_size = 0;
	  bootp->saved.rating = 0;
	  dhcpol_free(&bootp->saved.options);
	  make_bootp_request(&bootp->request, if_link_address(ifstate->if_p), 
			     if_link_arptype(ifstate->if_p),
			     if_link_length(ifstate->if_p));
	  bootp->try = 0;
	  bootp_client_enable_receive(bootp->client,
				      (bootp_receive_func_t *)bootp_request, 
				      ifstate, (void *)IFEventID_data_e);
	  /* FALL THROUGH */
      case IFEventID_timeout_e:
	  if (bootp->gathering == TRUE) {
	      bootp_arp_probe(ifstate, IFEventID_start_e, NULL);
	      break;
	  }
	  bootp->try++;
	  if (bootp->try > 1) {
	      if (ifstate->link.valid && ifstate->link.active == FALSE) {
		  bootp_failed(ifstate, ipconfig_status_media_inactive_e,
			       NULL);
		  break;
	      }
	  }
	  if (bootp->try > (G_max_retries + 1)) {
	      bootp_failed(ifstate, ipconfig_status_no_server_e, NULL);
	      break;
	  }
	  bootp->request.bp_secs 
	    = htons((u_short) timer_current_secs() -
					  bootp->start_secs);
	  bootp->request.bp_xid = htonl(++bootp->xid);
	  /* send the packet */
	  if (bootp_client_transmit(bootp->client, if_name(ifstate->if_p),
				    bootp->request.bp_htype, NULL, 0,
				    G_ip_broadcast, G_ip_zeroes,
				    G_server_port, G_client_port,
				    &bootp->request, 
				    sizeof(bootp->request)) < 0) {
	      ts_log(LOG_ERR, 
		     "BOOTP %s: transmit failed", if_name(ifstate->if_p));
	  }
	  /* wait for responses */
	  tv.tv_sec = bootp->wait_secs;
	  tv.tv_usec = random_range(0, USECS_PER_SEC - 1);
	  ts_log(LOG_DEBUG, "BOOTP %s: waiting at %d for %d.%06d", 
		 if_name(ifstate->if_p), 
		 timer_current_secs() - bootp->start_secs,
		 tv.tv_sec, tv.tv_usec);
	  timer_set_relative(bootp->timer, tv, 
			     (timer_func_t *)bootp_request,
			     ifstate, (void *)IFEventID_timeout_e, NULL);
	  /* next time wait twice as long */
	  bootp->wait_secs = tv.tv_sec * 2;
	  if (bootp->wait_secs > MAX_WAIT_SECS)
	      bootp->wait_secs = MAX_WAIT_SECS;
	  break;

      case IFEventID_data_e: {
	  bootp_receive_data_t *pkt = (bootp_receive_data_t *)event_data;
	  unsigned 		rating;
	  struct bootp *	reply;

	  reply = (struct bootp *)pkt->data;
	  if ((ip_valid(reply->bp_yiaddr) == FALSE
	       && ip_valid(reply->bp_ciaddr) == FALSE)
	      || dhcp_packet_match(reply, bootp->xid,
				   (u_char) if_link_arptype(ifstate->if_p),
				   if_link_address(ifstate->if_p),
				   if_link_length(ifstate->if_p)) == FALSE) {
	      /* not an interesting packet, drop the packet */
	      break; /* out of case */
	  }
	  rating = count_params(&pkt->options, bootp_params, n_bootp_params);
	  if (bootp->saved.pkt_size == 0
	      || rating > bootp->saved.rating) {
	      dhcpol_free(&bootp->saved.options);
	      bcopy(pkt->data, bootp->saved.pkt, pkt->size);
	      bootp->saved.pkt_size = pkt->size;
	      bootp->saved.rating = rating;
	      dhcpol_parse_packet(&bootp->saved.options, 
				  (void *)bootp->saved.pkt, 
				  bootp->saved.pkt_size,
				  NULL);
	      if (rating == IDEAL_RATING) {
		  bootp_arp_probe(ifstate, IFEventID_start_e, NULL);
		  break;
	      }
	      if (bootp->gathering == FALSE) {
		  struct timeval t = {0,0};
		  t.tv_sec = G_gather_secs;
		  ts_log(LOG_DEBUG, "BOOTP %s: gathering began at %d", 
			 if_name(ifstate->if_p), 
			 timer_current_secs() - bootp->start_secs);
		  bootp->gathering = TRUE;
		  timer_set_relative(bootp->timer, t, 
				     (timer_func_t *)bootp_request,
				     ifstate, (void *)IFEventID_timeout_e, 
				     NULL);
	      }
	  }
	  break;
      }
      default:
	  break;
    }
    return;
}

static void
bootp_link_timer(void * arg0, void * arg1, void * arg2)
{
    IFState_t * ifstate = (IFState_t *) arg0;

    if (ifstate == NULL)
	return;
    
    (void)inet_remove(ifstate, &G_ip_zeroes);
    ifstate_publish_failure(ifstate, ipconfig_status_media_inactive_e,
			    NULL);
    return;
}

ipconfig_status_t
bootp_thread(IFState_t * ifstate, IFEventID_t evid, void * event_data)
{
    IFState_bootp_t *	bootp = (IFState_bootp_t *)ifstate->private;
    ipconfig_status_t	status = ipconfig_status_success_e;

    switch (evid) {
      case IFEventID_start_e: 
	  if (bootp) {
	      ts_log(LOG_ERR, "BOOTP %s: re-entering start state", 
		     if_name(ifstate->if_p));
	      return (ipconfig_status_internal_error_e);
	  }
	  bootp = malloc(sizeof(*bootp));
	  if (bootp == NULL) {
	      ts_log(LOG_ERR, "BOOTP %s: malloc failed", 
		     if_name(ifstate->if_p));
	      return (ipconfig_status_allocation_failed_e);
	  }
	  ifstate->private = bootp;
	  bzero(bootp, sizeof(*bootp));
	  dhcpol_init(&bootp->saved.options);
	  bootp->xid = random();
	  bootp->timer = timer_callout_init();
	  if (bootp->timer == NULL) {
	      ts_log(LOG_ERR, "BOOTP %s: timer_callout_init failed", 
		     if_name(ifstate->if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  (void)inet_add(ifstate, &G_ip_zeroes, NULL, NULL);
	  bootp->client = bootp_client_init(G_bootp_session);
	  if (bootp->client == NULL) {
	      ts_log(LOG_ERR, "BOOTP %s: bootp_client_init failed",
		     if_name(ifstate->if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  bootp->arp = arp_client_init(G_arp_session, ifstate->if_p);
	  if (bootp->arp == NULL) {
	      ts_log(LOG_ERR, "BOOTP %s: arp_client_init failed", 
		     if_name(ifstate->if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  bootp_request(ifstate, IFEventID_start_e, NULL);
	  break;
      case IFEventID_stop_e: {
	  inet_addrinfo_t * info;
      stop:
	  ts_log(LOG_DEBUG, "BOOTP %s: stop", if_name(ifstate->if_p));

	  if (bootp == NULL) { /* already stopped */
	      ts_log(LOG_DEBUG, "DHCP %s: already stopped", 
		     if_name(ifstate->if_p));
	      status = ipconfig_status_internal_error_e; /* shouldn't happen */
	      break;
	  }

	  /* remove IP address(es) */
	  while (info = if_inet_addr_at(ifstate->if_p, 
					ifstate->our_addrs_start)) {
	      ts_log(LOG_DEBUG, "BOOTP %s: removing %s",
		     if_name(ifstate->if_p), inet_ntoa(info->addr));
	      inet_remove(ifstate, &info->addr);
	  }

	  /* clean-up resources */
	  if (bootp->timer) {
	      timer_callout_free(&bootp->timer);
	  }
	  if (bootp->client) {
	      bootp_client_free(&bootp->client);
	  }
	  if (bootp->arp) {
	      arp_client_free(&bootp->arp);
	  }
	  dhcpol_free(&bootp->saved.options);
	  if (bootp)
	      free(bootp);
	  ifstate->private = NULL;
	  break;
      }
      case IFEventID_media_e: {
	  if (bootp == NULL) {
	      status = ipconfig_status_internal_error_e;
	      break;
	  }
	  if (ifstate->link.valid == TRUE) {
	      if (ifstate->link.active == TRUE) {
		  /* confirm an address, get a new one, or timeout */
		  bootp->user_warned = FALSE;
		  bootp_request(ifstate, IFEventID_start_e, NULL);
	      }
	      else {
		  struct timeval tv;

		  /* if link goes down and stays down long enough, unpublish */
		  S_cancel_pending_events(ifstate);
		  tv.tv_sec = G_link_inactive_secs;
		  tv.tv_usec = 0;
		  timer_set_relative(bootp->timer, tv, 
				     (timer_func_t *)bootp_link_timer,
				     ifstate, NULL, NULL);
	      }
	  }
	  break;
      }
      default:
	  break;
    } /* switch */
    return (status);
}
