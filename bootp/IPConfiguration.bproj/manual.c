/*
 * Copyright (c) 1999 - 2004 Apple Computer, Inc. All rights reserved.
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
 * manual.c
 * - manual configuration thread manual_thread()
 * - probes the address using ARP to ensure that another client 
 *   isn't already using the address
 */
/* 
 * Modification History
 *
 * May 24, 2000		Dieter Siegmund (dieter@apple.com)
 * - created
 *
 * October 4, 2000	Dieter Siegmund (dieter@apple.com)
 * - added code to unpublish interface state if the link goes
 *   down and stays down for more than 4 seconds
 * - added more code to re-arp when the link comes back
 * - forgo configuring the address if the link state is inactive
 *   after we ARP
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
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <net/if_types.h>
#include <syslog.h>

#include "interfaces.h"
#include "util.h"

#include "dprintf.h"
#include "dhcp_options.h"
#include "ipconfigd_threads.h"

typedef struct {
    arp_client_t *		arp;
    timer_callout_t *		timer;
    boolean_t			resolve_router_timed_out;
    boolean_t			ignore_link_status;
    boolean_t			user_warned;
} Service_manual_t;

static void
manual_resolve_router_callback(Service_t * service_p, router_arp_status_t status);

static void
manual_resolve_router_retry(void * arg0, void * arg1, void * arg2)
{
    Service_t * 	service_p = (Service_t *)arg0;
    Service_manual_t *	manual = (Service_manual_t *)service_p->private;

    service_resolve_router(service_p, manual->arp,
			   manual_resolve_router_callback,
			   service_requested_ip_addr(service_p));
    return;
}

static void
manual_resolve_router_callback(Service_t * service_p,
			       router_arp_status_t status)
{
    interface_t *	if_p = service_interface(service_p);
    Service_manual_t *	manual = (Service_manual_t *)service_p->private;
    struct timeval	tv;

    switch (status) {
    case router_arp_status_no_response_e:
	/* try again in 60 seconds */
	tv.tv_sec = 60;
	tv.tv_usec = 0;
	timer_set_relative(manual->timer, tv, 
			   (timer_func_t *)manual_resolve_router_retry,
			   service_p, NULL, NULL);
	if (manual->resolve_router_timed_out) {
	    break;
	}
	manual->resolve_router_timed_out = TRUE;
	/* publish what we have so far */
	service_publish_success(service_p, NULL, 0);
	break;
    case router_arp_status_success_e:
	manual->resolve_router_timed_out = FALSE;
	service_publish_success(service_p, NULL, 0);
	break;
    case router_arp_status_failed_e:
	my_log(LOG_ERR, "MANUAL %s: router arp resolution failed, %s", 
	       if_name(if_p), arp_client_errmsg(manual->arp));
	break;
    default:
	break;
    }
    return;
}

static void
manual_cancel_pending_events(Service_t * service_p)
{
    Service_manual_t *	manual = (Service_manual_t *)service_p->private;

    if (manual == NULL)
	return;
    if (manual->timer) {
	timer_cancel(manual->timer);
    }
    if (manual->arp) {
	arp_client_cancel(manual->arp);
    }
    return;
}

static void
manual_inactive(Service_t * service_p)
{
    manual_cancel_pending_events(service_p);
    service_remove_address(service_p);
    service_publish_failure(service_p, ipconfig_status_media_inactive_e,
			    NULL);
    return;
}

static void
manual_link_timer(void * arg0, void * arg1, void * arg2)
{
    manual_inactive((Service_t *) arg0);
    return;
}

static void
manual_start(Service_t * service_p, IFEventID_t evid, void * event_data)
{
    interface_t *	if_p = service_interface(service_p);
    Service_manual_t *	manual = (Service_manual_t *)service_p->private;

    switch (evid) {
      case IFEventID_start_e: {
	  if (manual->arp == NULL) {
	      /* if the link is up, just assign the IP */
	      if (manual->ignore_link_status == FALSE
		  && service_link_status(service_p)->valid == TRUE 
		  && service_link_status(service_p)->active == FALSE) {
		  manual_inactive(service_p);
		  break;
	      }
	      (void)service_set_address(service_p,
					service_requested_ip_addr(service_p),
					service_requested_ip_mask(service_p),
					G_ip_zeroes);
	      service_publish_success(service_p, NULL, 0);
	      break;
	  }
	  manual_cancel_pending_events(service_p);
	  arp_client_probe(manual->arp,
			   (arp_result_func_t *)manual_start, service_p,
			   (void *)IFEventID_arp_e, G_ip_zeroes,
			   service_requested_ip_addr(service_p));
	  break;
      }
      case IFEventID_arp_e: {
	  arp_result_t *	result = (arp_result_t *)event_data;

	  if (result->error) {
	      my_log(LOG_ERR, "MANUAL %s: arp probe failed, %s", 
		     if_name(if_p), arp_client_errmsg(manual->arp));
	      break;
	  }
	  else {
	      if (result->in_use) {
		  char			msg[128];
		  struct timeval	tv;

		  snprintf(msg, sizeof(msg), 
			   IP_FORMAT " in use by " EA_FORMAT,
			   IP_LIST(service_requested_ip_addr_ptr(service_p)),
			   EA_LIST(result->addr.target_hardware));
		  if (manual->user_warned == FALSE) {
		      manual->user_warned = TRUE;
		      service_report_conflict(service_p,
					      service_requested_ip_addr_ptr(service_p),
					      result->addr.target_hardware,
					      NULL);
		  }
		  my_log(LOG_ERR, "MANUAL %s: %s", 
			 if_name(if_p), msg);
		  service_remove_address(service_p);
		  service_publish_failure(service_p, 
					  ipconfig_status_address_in_use_e,
					  msg);
		  if (G_manual_conflict_retry_interval_secs > 0) {
		      /* try again in a bit */
		      tv.tv_sec = G_manual_conflict_retry_interval_secs;
		      tv.tv_usec = 0;
		      timer_set_relative(manual->timer, tv, 
					 (timer_func_t *)manual_start,
					 service_p, IFEventID_start_e, NULL);
		  }
		  break;
	      }
	  }
	  if (manual->ignore_link_status == FALSE
	      && service_link_status(service_p)->valid == TRUE 
	      && service_link_status(service_p)->active == FALSE) {
	      manual_inactive(service_p);
	      break;
	  }

	  /* set the new address */
	  (void)service_set_address(service_p,
				    service_requested_ip_addr(service_p),
				    service_requested_ip_mask(service_p),
				    G_ip_zeroes);
	  service_remove_conflict(service_p);
	  if (service_router_is_iaddr_valid(service_p)
	      && service_resolve_router(service_p, manual->arp,
					manual_resolve_router_callback,
					service_requested_ip_addr(service_p))) {
	      /* router ARP resolution started */
	      manual->resolve_router_timed_out = FALSE;
	  }
	  else {
	      service_publish_success(service_p, NULL, 0);
	  }
	  break;
      }
      default: {
	  break;
      }
    }
    return;
}

ipconfig_status_t
manual_thread(Service_t * service_p, IFEventID_t evid, void * event_data)
{
    interface_t *	if_p = service_interface(service_p);
    Service_manual_t *	manual = (Service_manual_t *)service_p->private;
    ipconfig_status_t	status = ipconfig_status_success_e;

    switch (evid) {
      case IFEventID_start_e: {
	  start_event_data_t *    evdata = ((start_event_data_t *)event_data);
	  ipconfig_method_data_t *ipcfg = evdata->config.data;

	  if (manual) {
	      my_log(LOG_DEBUG, "MANUAL %s: re-entering start state", 
		     if_name(if_p));
	      return (ipconfig_status_internal_error_e);
	  }
	  status = validate_method_data_addresses(&evdata->config,
						  ipconfig_method_manual_e,
						  if_name(if_p));
	  if (status != ipconfig_status_success_e)
	      break;
	  manual = malloc(sizeof(*manual));
	  if (manual == NULL) {
	      my_log(LOG_ERR, "MANUAL %s: malloc failed", 
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      break;
	  }
	  service_p->private = manual;
	  bzero(manual, sizeof(*manual));
	  manual->ignore_link_status
	      = ((ipcfg->flags & ipconfig_method_data_flags_ignore_link_status_e)
		 != 0);
	  service_set_requested_ip_addr(service_p, ipcfg->ip[0].addr);
	  service_set_requested_ip_mask(service_p, ipcfg->ip[0].mask);
	  if (if_flags(if_p) & IFF_LOOPBACK) {
	      /* set the new address */
	      (void)service_set_address(service_p, 
					service_requested_ip_addr(service_p),
					service_requested_ip_mask(service_p),
					G_ip_zeroes);
	      service_publish_success(service_p, NULL, 0);
	      break;
	  }
	  if (ipcfg->u.manual_router.s_addr != 0
	      && ipcfg->u.manual_router.s_addr != ipcfg->ip[0].addr.s_addr) {
	      service_router_set_iaddr(service_p, ipcfg->u.manual_router);
	      service_router_set_iaddr_valid(service_p);
	  }
	  manual->timer = timer_callout_init();
	  if (manual->timer == NULL) {
	      my_log(LOG_ERR, "MANUAL %s: timer_callout_init failed", 
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  manual->arp = arp_client_init(G_arp_session, if_p);
	  if (manual->arp == NULL) {
	      my_log(LOG_INFO, "MANUAL %s: arp_client_init failed", 
		     if_name(if_p));
	  }
	  my_log(LOG_DEBUG, "MANUAL %s: starting", 
		 if_name(if_p));
	  manual_start(service_p, IFEventID_start_e, NULL);
	  break;
      }
      stop:
      case IFEventID_stop_e: {
	  my_log(LOG_DEBUG, "MANUAL %s: stop", if_name(if_p));
	  if (manual == NULL) {
	      break;
	  }

	  /* remove IP address */
	  service_remove_address(service_p);

	  /* clean-up resources */
	  if (manual->arp) {
	      arp_client_free(&manual->arp);
	  }
	  if (manual->timer) {
	      timer_callout_free(&manual->timer);
	  }
	  if (manual) {
	      free(manual);
	  }
	  service_p->private = NULL;
	  break;
      }
      case IFEventID_change_e: {
	  change_event_data_t *   evdata = ((change_event_data_t *)event_data);
	  ipconfig_method_data_t *ipcfg = evdata->config.data;
	  boolean_t		  ignore_link_status;

	  if (manual == NULL) {
	      my_log(LOG_DEBUG, "MANUAL %s: private data is NULL", 
		     if_name(if_p));
	      status = ipconfig_status_internal_error_e;
	      break;
	  }
	  status = validate_method_data_addresses(&evdata->config,
						  ipconfig_method_manual_e,
						  if_name(if_p));
	  if (status != ipconfig_status_success_e)
	      break;
	  evdata->needs_stop = FALSE;
	  ignore_link_status
	      = ((ipcfg->flags & ipconfig_method_data_flags_ignore_link_status_e)
		 != 0);
	  if (ipcfg->ip[0].addr.s_addr
	      != service_requested_ip_addr(service_p).s_addr
	      || (service_router_is_iaddr_valid(service_p)
		  && (ipcfg->u.manual_router.s_addr 
		      != service_router_iaddr(service_p).s_addr))
	      || (ignore_link_status != manual->ignore_link_status)) {
	      evdata->needs_stop = TRUE;
	  }
	  else if (ipcfg->ip[0].mask.s_addr
		   != service_requested_ip_mask(service_p).s_addr) {
	      service_set_requested_ip_mask(service_p, ipcfg->ip[0].mask);
	      (void)service_set_address(service_p, 
					service_requested_ip_addr(service_p),
					service_requested_ip_mask(service_p),
					G_ip_zeroes);
	      /* publish new mask */
	      service_publish_success(service_p, NULL, 0);
	  }
	  break;
      }
      case IFEventID_arp_collision_e: {
	  arp_collision_data_t *	arpc;
	  char				msg[128];

	  arpc = (arp_collision_data_t *)event_data;

	  if (manual == NULL) {
	      return (ipconfig_status_internal_error_e);
	  }
	  if (arpc->ip_addr.s_addr 
	      != service_requested_ip_addr(service_p).s_addr) {
	      break;
	  }
	  snprintf(msg, sizeof(msg), 
		   IP_FORMAT " in use by " EA_FORMAT,
		   IP_LIST(&arpc->ip_addr), 
		   EA_LIST(arpc->hwaddr));
	  if (manual->user_warned == FALSE) {
	      manual->user_warned = TRUE;
	      service_report_conflict(service_p,
				      &arpc->ip_addr,
				      arpc->hwaddr,
				      NULL);
	  }
	  my_log(LOG_ERR, "MANUAL %s: %s", 
		 if_name(if_p), msg);
	  break;
      }
      case IFEventID_media_e: {
	  if (manual == NULL)
	      return (ipconfig_status_internal_error_e);
	  if (manual->ignore_link_status) {
	      break;
	  }
	  manual->user_warned = FALSE;
	  if (service_link_status(service_p)->valid == TRUE) {
	      if (service_link_status(service_p)->active == TRUE) {
		  manual_start(service_p, IFEventID_start_e, NULL);
	      }
	      else {
		  struct timeval tv;

		  /* if link goes down and stays down long enough, unpublish */
		  manual_cancel_pending_events(service_p);
		  tv.tv_sec = G_link_inactive_secs;
		  tv.tv_usec = 0;
		  timer_set_relative(manual->timer, tv, 
				     (timer_func_t *)manual_link_timer,
				     service_p, NULL, NULL);
	      }
	  }
	  break;
      }
      default:
	  break;
    } /* switch */

    return (status);
}
