/*
 * Copyright (c) 1999 - 2006 Apple Computer, Inc. All rights reserved.
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
 * failover.c
 * - failover configuration thread failover_thread()
 * - keeps an IP address assigned on the interface, as long as no one
 *   else is using it
 */
/* 
 * Modification History
 *
 * May 26, 2006		Dieter Siegmund (dieter@apple.com)
 * - created (from manual.c)
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
    uint32_t			address_timeout_secs;
    bool			address_is_verified;
    int				failover_start_delay;
    int				failover_probe_delay;
} Service_failover_t;

static void
failover_start(Service_t * service_p, IFEventID_t evid, void * event_data);

static void
failover_cancel_pending_events(Service_t * service_p)
{
    Service_failover_t *	failover = (Service_failover_t *)service_p->private;

    if (failover == NULL)
	return;
    if (failover->timer) {
	timer_cancel(failover->timer);
    }
    if (failover->arp) {
	arp_client_cancel(failover->arp);
    }
    return;
}

static void
failover_inactive(Service_t * service_p)
{
    failover_cancel_pending_events(service_p);
    service_remove_address(service_p);
    service_publish_failure(service_p, ipconfig_status_media_inactive_e,
			    NULL);
    return;
}

static void
failover_timed_out(Service_t * service_p)
{
    my_log(LOG_DEBUG, "FAILOVER %s: address timer fired", 
	   if_name(service_interface(service_p)));
    failover_cancel_pending_events(service_p);
    service_remove_address(service_p);
    service_publish_failure(service_p, ipconfig_status_address_timed_out_e,
			    NULL);
    return;
}

static void
failover_link_timer(void * arg0, void * arg1, void * arg2)
{
    failover_inactive((Service_t *) arg0);
    return;
}

static void
failover_start(Service_t * service_p, IFEventID_t evid, void * event_data)
{
    Service_failover_t * failover = (Service_failover_t *)service_p->private;
    interface_t *	if_p = service_interface(service_p);
    struct timeval	tv;

    switch (evid) {
      case IFEventID_start_e: {
	  failover->address_is_verified = FALSE;
	  failover_cancel_pending_events(service_p);
	  tv.tv_sec = random_range(0, 4);
	  tv.tv_usec = random_range(0, USECS_PER_SEC - 1);
	  timer_set_relative(failover->timer, tv, 
			     (timer_func_t *)failover_start,
			     service_p, (void *)IFEventID_timeout_e, NULL);
	  break;
      }
      case IFEventID_timeout_e: {
	  arp_client_probe(failover->arp, 
			   (arp_result_func_t *)failover_start, service_p,
			   (void *)IFEventID_arp_e, G_ip_zeroes,
			   service_requested_ip_addr(service_p));
	  break;
      }
      case IFEventID_arp_e: {
	  arp_result_t *	result = (arp_result_t *)event_data;

	  if (result->error) {
	      my_log(LOG_ERR, "FAILOVER %s: arp probe failed, %s", 
		     if_name(if_p), arp_client_errmsg(failover->arp));
	      break;
	  }
	  else {
	      if (result->in_use) {
		  char	msg[128];

		  snprintf(msg, sizeof(msg), 
			   IP_FORMAT " in use by " EA_FORMAT,
			   IP_LIST(service_requested_ip_addr_ptr(service_p)),
			   EA_LIST(result->addr.target_hardware));
		  my_log(LOG_NOTICE, "FAILOVER %s: %s", 
			 if_name(if_p), msg);
		  service_remove_address(service_p);
		  service_publish_failure(service_p, 
					  ipconfig_status_address_in_use_e,
					  msg);
		  tv.tv_sec = 10;
		  tv.tv_usec = 0;
		  timer_set_relative(failover->timer, tv, 
				     (timer_func_t *)failover_start,
				     service_p, IFEventID_start_e, NULL);
		  break;
	      }
	  }
	  if (service_link_status(service_p)->valid == TRUE 
	      && service_link_status(service_p)->active == FALSE) {
	      failover_inactive(service_p);
	      break;
	  }

	  /* set the new address */
	  (void)service_set_address(service_p,
				    service_requested_ip_addr(service_p),
				    service_requested_ip_mask(service_p),
				    G_ip_zeroes);
	  service_publish_success(service_p, NULL, 0);
	  failover->address_is_verified = TRUE;
	  if (failover->address_timeout_secs != 0) {
	      tv.tv_sec = failover->address_timeout_secs;
	      tv.tv_usec = 0;
	      timer_set_relative(failover->timer, tv, 
				 (timer_func_t *)failover_timed_out,
				 service_p, IFEventID_start_e, NULL);
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
failover_thread(Service_t * service_p, IFEventID_t evid, void * event_data)
{
    interface_t *	if_p = service_interface(service_p);
    Service_failover_t *	failover = (Service_failover_t *)service_p->private;
    ipconfig_status_t	status = ipconfig_status_success_e;

    switch (evid) {
      case IFEventID_start_e: {
	  start_event_data_t *    evdata = ((start_event_data_t *)event_data);
	  ipconfig_method_data_t *ipcfg = evdata->config.data;

	  if (failover) {
	      my_log(LOG_DEBUG, "FAILOVER %s: re-entering start state", 
		     if_name(if_p));
	      return (ipconfig_status_internal_error_e);
	  }
	  if (if_flags(if_p) & IFF_LOOPBACK) {
	      return (ipconfig_status_invalid_parameter_e);
	  }
	  status = validate_method_data_addresses(&evdata->config,
						  ipconfig_method_failover_e,
						  if_name(if_p));
	  if (status != ipconfig_status_success_e)
	      break;
	  failover = malloc(sizeof(*failover));
	  if (failover == NULL) {
	      my_log(LOG_ERR, "FAILOVER %s: malloc failed", 
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      break;
	  }
	  service_p->private = failover;
	  bzero(failover, sizeof(*failover));
	  service_set_requested_ip_addr(service_p, ipcfg->ip[0].addr);
	  service_set_requested_ip_mask(service_p, ipcfg->ip[0].mask);
	  failover->timer = timer_callout_init();
	  if (failover->timer == NULL) {
	      my_log(LOG_ERR, "FAILOVER %s: timer_callout_init failed", 
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  failover->arp = arp_client_init(G_arp_session, if_p);
	  if (failover->arp == NULL) {
	      my_log(LOG_INFO, "FAILOVER %s: arp_client_init failed", 
		     if_name(if_p));
	      goto stop;
	  }
	  failover->address_timeout_secs = ipcfg->u.failover_timeout;
	  my_log(LOG_DEBUG, "FAILOVER %s: starting", 
		 if_name(if_p));
	  failover_start(service_p, IFEventID_start_e, NULL);
	  break;
      }
      stop:
      case IFEventID_stop_e: {
	  my_log(LOG_DEBUG, "FAILOVER %s: stop", if_name(if_p));

	  if (failover == NULL) {
	      break;
	  }

	  /* remove IP address */
	  service_remove_address(service_p);

	  /* clean-up resources */
	  if (failover->arp) {
	      arp_client_free(&failover->arp);
	  }
	  if (failover->timer) {
	      timer_callout_free(&failover->timer);
	  }
	  if (failover) {
	      free(failover);
	  }
	  service_p->private = NULL;
	  break;
      }
      case IFEventID_change_e: {
	  change_event_data_t *   evdata = ((change_event_data_t *)event_data);
	  ipconfig_method_data_t *ipcfg = evdata->config.data;

	  if (failover == NULL) {
	      my_log(LOG_DEBUG, "FAILOVER %s: private data is NULL", 
		     if_name(if_p));
	      status = ipconfig_status_internal_error_e;
	      break;
	  }
	  status = validate_method_data_addresses(&evdata->config,
						  ipconfig_method_failover_e,
						  if_name(if_p));
	  if (status != ipconfig_status_success_e)
	      break;
	  evdata->needs_stop = FALSE;
	  if ((ipcfg->ip[0].addr.s_addr
	       != service_requested_ip_addr(service_p).s_addr)
	      || (ipcfg->ip[0].mask.s_addr
		  != service_requested_ip_mask(service_p).s_addr)) {
	      evdata->needs_stop = TRUE;
	  }
	  else {
	      failover->address_timeout_secs = ipcfg->u.failover_timeout;
	      if (service_is_address_set(service_p)
		  && failover->address_is_verified) {
		  if (failover->address_timeout_secs != 0) {
		      struct timeval	tv;

		      tv.tv_sec = failover->address_timeout_secs;
		      tv.tv_usec = 0;
		      timer_set_relative(failover->timer, tv, 
					 (timer_func_t *)failover_timed_out,
					 service_p, IFEventID_start_e, NULL);
		  }
		  else {
		      timer_cancel(failover->timer);
		  }
	      }
	      else {
		  failover_start(service_p, IFEventID_start_e, NULL);
	      }
	  }
	  break;
      }
      case IFEventID_arp_collision_e: {
	  arp_collision_data_t *	arpc;
	  char				msg[128];

	  arpc = (arp_collision_data_t *)event_data;

	  if (failover == NULL) {
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
	  my_log(LOG_NOTICE, "FAILOVER %s: %s", 
		 if_name(if_p), msg);
	  service_remove_address(service_p);
	  service_publish_failure(service_p, 
				  ipconfig_status_address_in_use_e,
				  msg);
	  failover_start(service_p, IFEventID_start_e, NULL);
	  break;
      }
      case IFEventID_media_e: {
	  if (failover == NULL)
	      return (ipconfig_status_internal_error_e);
	  if (service_link_status(service_p)->valid == TRUE) {
	      if (service_link_status(service_p)->active == TRUE) {
		  failover_start(service_p, IFEventID_start_e, NULL);
	      }
	      else {
		  struct timeval tv;

		  failover->address_is_verified = FALSE;

		  /* if link goes down and stays down long enough, unpublish */
		  failover_cancel_pending_events(service_p);
		  tv.tv_sec = G_link_inactive_secs;
		  tv.tv_usec = 0;
		  timer_set_relative(failover->timer, tv, 
				     (timer_func_t *)failover_link_timer,
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
