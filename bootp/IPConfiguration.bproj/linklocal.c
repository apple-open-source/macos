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
 *
 * January 8, 2002	Dieter Siegmund (dieter@apple.com)
 * - added pseudo-link-local service support i.e. configure the
 *   subnet, but don't configure a link-local address
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
#include <ctype.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/bootp.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <net/if_types.h>

#include "dhcp_options.h"
#include "dhcp.h"
#include "interfaces.h"
#include "util.h"
#include "host_identifier.h"
#include "dhcplib.h"

#include "ipconfigd_threads.h"

#include "dprintf.h"

/* ad-hoc networking definitions */
#define LINKLOCAL_RANGE_START	((u_long)0xa9fe0000) /* 169.254.0.0 */
#define LINKLOCAL_RANGE_END	((u_long)0xa9feffff) /* 169.254.255.255 */
#define LINKLOCAL_FIRST_USEABLE	(LINKLOCAL_RANGE_START + 256) /* 169.254.1.0 */
#define LINKLOCAL_LAST_USEABLE	(LINKLOCAL_RANGE_END - 256) /* 169.254.254.255 */
#define LINKLOCAL_MASK		((u_long)0xffff0000) /* 255.255.0.0 */
#define LINKLOCAL_RANGE		((u_long)(LINKLOCAL_LAST_USEABLE + 1) \
					  - LINKLOCAL_FIRST_USEABLE)
#define	MAX_LINKLOCAL_INITIAL_TRIES	10

/*
 * LINKLOCAL_RETRY_TIME_SECS
 *   After we probe for MAX_LINKLOCAL_INITIAL_TRIES addresses and fail,
 *   wait this amount of time before trying the next one.  This avoids
 *   overwhelming the network with ARP probes in the worst case scenario.
 */
#define LINKLOCAL_RETRY_TIME_SECS	30

typedef struct {
    arp_client_t *	arp;	/* if NULL, we don't configure an address */
    timer_callout_t *	timer;
    int			current;
    struct in_addr	our_ip;
    struct in_addr	probe;
    boolean_t		allocate;
    boolean_t		enable_arp_collision_detection;
} Service_linklocal_t;

static struct in_addr	S_last_address;

static __inline__ boolean_t
in_addr_is_linklocal(struct in_addr iaddr)
{
    return (IN_LINKLOCAL(iptohl(iaddr)));
}

static __inline__ void
linklocal_subnet_remove()
{
    subnet_route_delete(G_ip_zeroes, hltoip(IN_LINKLOCALNETNUM),
			hltoip(IN_CLASSB_NET), NULL);
    return;
}

static __inline__ void
linklocal_subnet_set(Service_t * service_p)
{
    struct in_addr	iaddr = { 0 };

    subnet_route_delete(G_ip_zeroes, hltoip(IN_LINKLOCALNETNUM),
			hltoip(IN_CLASSB_NET), NULL);
    if (service_p->info.addr.s_addr != 0) {
	iaddr = service_p->info.addr;
    }
    else {
	Service_t * parent_service_p = service_parent_service(service_p);
	if (parent_service_p == NULL) {
	    return;
	}
	iaddr = parent_service_p->info.addr;
    }
    subnet_route_add(iaddr, hltoip(IN_LINKLOCALNETNUM),
		     hltoip(IN_CLASSB_NET), 
		     if_name(service_interface(service_p)));
    return;
}

struct in_addr
S_find_linklocal_address(interface_t * if_p)
{
    int				count = if_inet_count(if_p);
    int				i;

    if (S_last_address.s_addr != 0) {
	return (S_last_address);
    }
    for (i = 0; i < count; i++) {
	inet_addrinfo_t * 	info = if_inet_addr_at(if_p, i);

	if (in_addr_is_linklocal(info->addr)) {
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

    linklocal->enable_arp_collision_detection = FALSE;
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
	  linklocal->enable_arp_collision_detection = FALSE;

	  /* clean-up anything that might have come before */
	  linklocal_cancel_pending_events(service_p);
	  
	  linklocal->current = 0;
	  if (linklocal->our_ip.s_addr) {
	      /* try to keep the same address */
	      linklocal->probe = linklocal->our_ip;
	  }
	  else {
	      linklocal->probe.s_addr 
		  = htonl(LINKLOCAL_FIRST_USEABLE 
			  + random_range(0, LINKLOCAL_RANGE));
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
	      break;
	  }
	  else if (result->in_use) {
	      my_log(LOG_DEBUG, "LINKLOCAL %s: IP address " 
		     IP_FORMAT " is in use by " EA_FORMAT, 
		     if_name(if_p), 
		     IP_LIST(&linklocal->probe),
		     EA_LIST(result->hwaddr));
	      if (linklocal->our_ip.s_addr == linklocal->probe.s_addr) {
		  S_last_address = linklocal->our_ip = G_ip_zeroes;
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
	      (void)service_set_address(service_p, linklocal->probe, 
					linklocal_mask, G_ip_zeroes);
	      linklocal_cancel_pending_events(service_p);
	      S_last_address = linklocal->our_ip = linklocal->probe;
	      service_publish_success(service_p, NULL, 0);
	      linklocal->enable_arp_collision_detection = TRUE;
	      /* we're done */
	      break; /* out of switch */
	  }
	  linklocal->current++;
	  if (linklocal->current >= MAX_LINKLOCAL_INITIAL_TRIES) {
	      struct timeval tv;
	      /* initial tries threshold reached, try again after a timeout */
	      tv.tv_sec = LINKLOCAL_RETRY_TIME_SECS;
	      timer_set_relative(linklocal->timer, tv, 
				 (timer_func_t *)linklocal_init,
				 service_p, (void *)IFEventID_timeout_e, NULL);
	      /* don't fall through, wait for timer */
	      break;
	  }
	  /* FALL THROUGH */
      case IFEventID_timeout_e:
	  /* try the next address */
	  linklocal->probe.s_addr 
	      = htonl(LINKLOCAL_FIRST_USEABLE 
		      + random_range(0, LINKLOCAL_RANGE));
	  arp_probe(linklocal->arp, 
		    (arp_result_func_t *)linklocal_init, service_p,
		    (void *)IFEventID_arp_e, G_ip_zeroes,
		    linklocal->probe);
	  my_log(LOG_DEBUG, "LINKLOCAL %s probing " IP_FORMAT, 
		 if_name(if_p), IP_LIST(&linklocal->probe));
	  /* wait for the results */
	  break;
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
	  start_event_data_t *    evdata = ((start_event_data_t *)event_data);
	  ipconfig_method_data_t *ipcfg = NULL;

	  my_log(LOG_DEBUG, "LINKLOCAL %s: start", if_name(if_p));

	  if (evdata != NULL) {
	      ipcfg = evdata->config.data;
	  }
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
	  linklocal->allocate = TRUE;
	  /* ARP probes count as collisions for link-local address allocation */
	  arp_client_set_probes_are_collisions(linklocal->arp, TRUE);
	  if (ipcfg != NULL && ipcfg->reserved_0 == TRUE) {
	      /* don't allocate an IP address, just set the subnet */
	      linklocal->allocate = FALSE;
	      linklocal_subnet_set(service_p);
	      service_publish_failure(service_p, 
				      ipconfig_status_success_e, NULL);
	      break;
	  }
	  linklocal->our_ip = S_find_linklocal_address(if_p);
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
	  if (linklocal->allocate) {
	      service_remove_address(service_p);
	  }
	  else {
	      linklocal_subnet_remove();
	  }

	  /* clean-up the published state */
	  service_publish_failure(service_p, 
				  ipconfig_status_success_e, NULL);

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
	  change_event_data_t *   evdata = ((change_event_data_t *)event_data);
	  ipconfig_method_data_t *ipcfg = evdata->config.data;
	  boolean_t		  allocate = TRUE;

	  if (ipcfg != NULL && ipcfg->reserved_0 == TRUE) {
	      /* don't allocate an IP address, just set the subnet */
	      allocate = FALSE;
	  }
	  if (linklocal->allocate != allocate) {
	      linklocal->allocate = allocate;
	      if (allocate) {
		  linklocal->our_ip = S_find_linklocal_address(if_p);
		  linklocal_init(service_p, IFEventID_start_e, NULL);
	      }
	      else {
		  linklocal_failed(service_p, ipconfig_status_success_e, NULL);
		  linklocal_subnet_set(service_p);
	      }
	  }
	  break;
      }
      case IFEventID_arp_collision_e: {
	  arp_collision_data_t *	arpc;

	  arpc = (arp_collision_data_t *)event_data;
	  if (linklocal == NULL) {
	      return (ipconfig_status_internal_error_e);
	  }
	  if (linklocal->allocate == FALSE) {
	      break;
	  }
	  if (linklocal->enable_arp_collision_detection == FALSE
	      || arpc->ip_addr.s_addr != linklocal->our_ip.s_addr) {
	      break;
	  }
	  S_last_address = linklocal->our_ip = G_ip_zeroes;
	  (void)service_remove_address(service_p);
	  service_publish_failure(service_p, 
				  ipconfig_status_address_in_use_e,
				  NULL);
	  linklocal_init(service_p, IFEventID_start_e, NULL);
	  break;
      }
      case IFEventID_media_e: {
	  if (linklocal == NULL) {
	      return (ipconfig_status_internal_error_e);
	  }
	  if (linklocal->allocate == FALSE) {
	      break;
	  }
	  if (service_link_status(service_p)->valid == TRUE) {
	      if (service_link_status(service_p)->active == TRUE) {
		  linklocal_init(service_p, IFEventID_start_e, NULL);
	      }
	      else {
		  struct timeval tv;

		  linklocal->enable_arp_collision_detection = FALSE;

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
