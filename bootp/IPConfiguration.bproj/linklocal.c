/*
 * Copyright (c) 1999-2001 Apple Computer, Inc. All rights reserved.
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
 * linklocal.c
 * - link-local address configuration thread
 * - contains linklocal_thread()
 */
/* 
 * Modification History
 *
 * September 27, 2001	Dieter Siegmund (dieter@apple.com)
 * - moved ad-hoc processing into its own service
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
#import <syslog.h>

#import "dhcp_options.h"
#import "dhcp.h"
#import "interfaces.h"
#import "util.h"
#import <net/if_types.h>
#import "host_identifier.h"
#import "dhcplib.h"

#import "ipconfigd_threads.h"

extern char *  			ether_ntoa(struct ether_addr *e);

#import "dprintf.h"

/* ad-hoc networking definitions */
#define LINKLOCAL_RANGE_START	((u_long)0xa9fe0000) /* 169.254.0.0 */
#define LINKLOCAL_RANGE_END	((u_long)0xa9feffff) /* 169.254.255.255 */
#define LINKLOCAL_FIRST_USEABLE	(LINKLOCAL_RANGE_START + 256) /* 169.254.1.0 */
#define LINKLOCAL_LAST_USEABLE	(LINKLOCAL_RANGE_END - 256) /* 169.254.254.255 */
#define LINKLOCAL_MASK		((u_long)0xffff0000) /* 255.255.0.0 */

#define	MAX_LINKLOCAL_TRIES	10

typedef struct {
    arp_client_t *	arp;
    timer_callout_t *	timer;
    int			current;
    struct in_addr	our_ip;
    struct in_addr	probe;
    u_short		offset[MAX_LINKLOCAL_TRIES];
} Service_linklocal_t;

struct in_addr
S_find_linklocal_address(interface_t * if_p)
{
    const struct in_addr	linklocal_net = {htonl(LINKLOCAL_RANGE_START)};
    const struct in_addr	linklocal_mask = {htonl(LINKLOCAL_MASK)};
    int				count = if_inet_count(if_p);
    int				i;

    for (i = 0; i < count; i++) {
	inet_addrinfo_t * 	info = if_inet_addr_at(if_p, i);

	if (in_subnet(linklocal_net, linklocal_mask, info->addr)) {
	    my_log(LOG_DEBUG, "LINKLOCAL %s: found address " IP_FORMAT,
		   if_name(if_p), IP_LIST(&info->addr));
	    return (info->addr);
	}
    }
    return (G_ip_zeroes);
}

void
linklocal_cancel_pending_events(Service_t * service_p)
{
    Service_linklocal_t * linklocal = (Service_linklocal_t *)service_p->private;

    if (linklocal == NULL)
	return;
    if (linklocal->timer) {
	timer_cancel(linklocal->timer);
    }
    if (linklocal->arp) {
	arp_cancel_probe(linklocal->arp);
    }
    return;
}


static void
linklocal_failed(Service_t * service_p, ipconfig_status_t status, char * msg)
{
    Service_linklocal_t * linklocal = (Service_linklocal_t *)service_p->private;

    linklocal_cancel_pending_events(service_p);
    service_remove_address(service_p);
    if (status != ipconfig_status_media_inactive_e) {
	linklocal->our_ip = G_ip_zeroes;
    }
    service_publish_failure(service_p, status, msg);
    return;
}

static void
linklocal_link_timer(void * arg0, void * arg1, void * arg2)
{
    linklocal_failed((Service_t *)arg0, ipconfig_status_media_inactive_e, NULL);
    return;
}

static void
linklocal_init(Service_t * service_p, IFEventID_t event_id, void * event_data)
{
    Service_linklocal_t * linklocal = (Service_linklocal_t *)service_p->private;
    interface_t *	  if_p = service_interface(service_p);
    
    switch (event_id) {
      case IFEventID_start_e: {
	  int			i;
	  long			range;

	  /* clean-up anything that might have come before */
	  linklocal_cancel_pending_events(service_p);
	  
	  range = (LINKLOCAL_LAST_USEABLE + 1) - LINKLOCAL_FIRST_USEABLE;

	  /* populate an array of unique random numbers */
	  for (i = 0; i < MAX_LINKLOCAL_TRIES; ) {
	      int		j;
	      long 		r;
	      
	      r = random_range(0, range);
	      for (j = 0; j < i; j++) {
		  if (linklocal->offset[j] == r)
		      continue;
	      }
	      linklocal->offset[i++] = r;
	  }
	  linklocal->current = 0;
	  if (linklocal->our_ip.s_addr) {
	      /* try to keep the same address */
	      linklocal->probe = linklocal->our_ip;
	  }
	  else {
	      linklocal->probe.s_addr 
		  = htonl(LINKLOCAL_FIRST_USEABLE 
			  + linklocal->offset[linklocal->current]);
	  }
	  my_log(LOG_DEBUG, "LINKLOCAL %s: probing " IP_FORMAT, 
		 if_name(if_p), IP_LIST(&linklocal->probe));
	  arp_probe(linklocal->arp, 
		    (arp_result_func_t *)linklocal_init, service_p,
		    (void *)IFEventID_arp_e, G_ip_zeroes,
		    linklocal->probe);
	  /* wait for the results */
	  return;
      }
      case IFEventID_arp_e: {
	  arp_result_t *	result = (arp_result_t *)event_data;

	  if (result->error) {
	      my_log(LOG_DEBUG, "LINKLOCAL %s: ARP probe failed, %s", 
		     if_name(if_p),
		     arp_client_errmsg(linklocal->arp));
	  }
	  else if (result->in_use) {
	      my_log(LOG_DEBUG, "LINKLOCAL %s: IP address " 
		     IP_FORMAT " is in use by " EA_FORMAT, 
		     if_name(if_p), 
		     IP_LIST(&linklocal->probe),
		     EA_LIST(result->hwaddr));
	      if (linklocal->our_ip.s_addr == linklocal->probe.s_addr) {
		  linklocal->our_ip = G_ip_zeroes;
		  (void)service_remove_address(service_p);
		  service_publish_failure(service_p, 
					  ipconfig_status_address_in_use_e,
					  NULL);
	      }
	  }
	  else {
	      const struct in_addr linklocal_mask = { htonl(LINKLOCAL_MASK) };

	      if (service_link_status(service_p)->valid == TRUE 
		  && service_link_status(service_p)->active == FALSE) {
		  linklocal_failed(service_p,
				   ipconfig_status_media_inactive_e, NULL);
		  break;
	      }

	      /* ad-hoc IP address is not in use, so use it */
	      if (service_set_address(service_p, linklocal->probe, 
				      linklocal_mask, 
				      G_ip_zeroes) == EEXIST) {
		  /* some other interface is already ad hoc */
		  (void)service_remove_address(service_p);
		  service_publish_failure(service_p, 
					  ipconfig_status_address_in_use_e,
					  NULL);
	      }
	      else {
		  linklocal_cancel_pending_events(service_p);
		  linklocal->our_ip = linklocal->probe;
		  service_publish_success(service_p, NULL, 0);
	      }
	      /* we're done */
	      break; /* out of switch */
	  }
	  /* try the next address */
	  linklocal->current++;
	  if (linklocal->current >= MAX_LINKLOCAL_TRIES) {
	      service_publish_failure(service_p,
				      ipconfig_status_address_in_use_e,
				      NULL);
	      /* we're done */
	      break; /* out of switch */
	  }
	  linklocal->probe.s_addr 
	      = htonl(LINKLOCAL_FIRST_USEABLE 
		      + linklocal->offset[linklocal->current]);
	  arp_probe(linklocal->arp, 
		    (arp_result_func_t *)linklocal_init, service_p,
		    (void *)IFEventID_arp_e, G_ip_zeroes,
		    linklocal->probe);
	  my_log(LOG_DEBUG, "LINKLOCAL %s probing " IP_FORMAT, 
		 if_name(if_p), IP_LIST(&linklocal->probe));
	  /* wait for the results */
	  return;
      }
      default:
	  break;
    }

    return;
}

ipconfig_status_t
linklocal_thread(Service_t * service_p, IFEventID_t event_id, void * event_data)
{
    Service_linklocal_t *	linklocal;
    interface_t *		if_p = service_interface(service_p);
    ipconfig_status_t		status = ipconfig_status_success_e;

    linklocal = (Service_linklocal_t *)service_p->private;

    switch (event_id) {
      case IFEventID_start_e: {
	  if (if_flags(if_p) & IFF_LOOPBACK) {
	      status = ipconfig_status_invalid_operation_e;
	      break;
	  }
	  if (linklocal) {
	      my_log(LOG_ERR, "LINKLOCAL %s: re-entering start state", 
		     if_name(if_p));
	      status = ipconfig_status_internal_error_e;
	      break;
	  }
	  linklocal = malloc(sizeof(*linklocal));
	  if (linklocal == NULL) {
	      my_log(LOG_ERR, "LINKLOCAL %s: malloc failed", 
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      break;
	  }
	  bzero(linklocal, sizeof(*linklocal));
	  service_p->private = linklocal;

	  linklocal->our_ip = S_find_linklocal_address(if_p);
	  linklocal->timer = timer_callout_init();
	  if (linklocal->timer == NULL) {
	      my_log(LOG_ERR, "LINKLOCAL %s: timer_callout_init failed", 
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  linklocal->arp = arp_client_init(G_arp_session, if_p);
	  if (linklocal->arp == NULL) {
	      my_log(LOG_ERR, "LINKLOCAL %s: arp_client_init failed", 
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  my_log(LOG_DEBUG, "LINKLOCAL %s: start", if_name(if_p));
	  linklocal_init(service_p, IFEventID_start_e, NULL);
	  break;
      }
      case IFEventID_stop_e: {
      stop:
	  my_log(LOG_DEBUG, "LINKLOCAL %s: stop", if_name(if_p));
	  if (linklocal == NULL) {
	      my_log(LOG_DEBUG, "LINKLOCAL %s: already stopped", 
		     if_name(if_p));
	      status = ipconfig_status_internal_error_e; /* shouldn't happen */
	      break;
	  }
	  /* remove IP address(es) */
	  service_remove_address(service_p);

	  /* clean-up resources */
	  if (linklocal->timer) {
	      timer_callout_free(&linklocal->timer);
	  }
	  if (linklocal->arp) {
	      arp_client_free(&linklocal->arp);
	  }
	  if (linklocal) {
	      free(linklocal);
	  }
	  service_p->private = NULL;
	  break;
      }
      case IFEventID_change_e: {
	  break;
      }
      case IFEventID_media_e: {
	  if (linklocal == NULL) {
	      return (ipconfig_status_internal_error_e);
	  }
	  if (service_link_status(service_p)->valid == TRUE) {
	      if (service_link_status(service_p)->active == TRUE) {
		  linklocal_init(service_p, IFEventID_start_e, NULL);
	      }
	      else {
		  struct timeval tv;

		  /* if link goes down and stays down long enough, unpublish */
		  linklocal_cancel_pending_events(service_p);
		  tv.tv_sec = G_link_inactive_secs;
		  tv.tv_usec = 0;
		  timer_set_relative(linklocal->timer, tv, 
				     (timer_func_t *)linklocal_link_timer,
				     service_p, NULL, NULL);
	      }
	  }
	  break;
      }
      case IFEventID_renew_e: {
	  break;
      }
      default:
	  break;
    } /* switch (event_id) */
    return (status);
}
