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
#import <arpa/inet.h>
#import <net/if_types.h>
#import <syslog.h>

#import "interfaces.h"
#import "util.h"

#import "dprintf.h"
#import "dhcp_options.h"
#import "ipconfigd_threads.h"

extern char *  			ether_ntoa(struct ether_addr *e);

typedef struct {
    arp_client_t *		arp;
    timer_callout_t *		timer;
    struct in_addr		our_ip;
    struct in_addr		our_mask;
} IFState_manual_t;

static void
manual_cancel_pending_events(IFState_t * ifstate)
{
    IFState_manual_t *	manual = (IFState_manual_t *)ifstate->private;

    if (manual == NULL)
	return;
    if (manual->timer) {
	timer_cancel(manual->timer);
    }
    if (manual->arp) {
	arp_cancel_probe(manual->arp);
    }
    return;
}

static void
manual_inactive(IFState_t * ifstate)
{
    ifstate_remove_addresses(ifstate);
    ifstate_publish_failure(ifstate, ipconfig_status_media_inactive_e,
			    NULL);
    return;
}

static void
manual_link_timer(void * arg0, void * arg1, void * arg2)
{
    manual_inactive((IFState_t *) arg0);
    return;
}

static void
manual_start(IFState_t * ifstate, IFEventID_t evid, void * event_data)
{
    IFState_manual_t *	manual = (IFState_manual_t *)ifstate->private;

    switch (evid) {
      case IFEventID_start_e: {
	  manual_cancel_pending_events(ifstate);
	  arp_probe(manual->arp, 
		    (arp_result_func_t *)manual_start, ifstate,
		    (void *)IFEventID_arp_e, G_ip_zeroes,
		    manual->our_ip);
	  break;
      }
      case IFEventID_arp_e: {
	  arp_result_t *	result = (arp_result_t *)event_data;

	  if (result->error) {
	      my_log(LOG_ERR, "MANUAL %s: arp probe failed, %s", 
		     if_name(ifstate->if_p),
		     arp_client_errmsg(manual->arp));
	      /* continue without it anyways */
	  }
	  else {
	      if (result->in_use) {
		  char	msg[128];

		  snprintf(msg, sizeof(msg), 
			   IP_FORMAT " in use by " EA_FORMAT,
			   IP_LIST(&manual->our_ip), 
			   EA_LIST(result->hwaddr));
		  ifstate_tell_user(ifstate, msg);
		  my_log(LOG_ERR, "MANUAL %s: %s", 
			 if_name(ifstate->if_p), msg);
		  ifstate_remove_addresses(ifstate);
		  (void)inet_remove(ifstate, manual->our_ip);
		  ifstate_publish_failure(ifstate, 
					  ipconfig_status_address_in_use_e,
					  msg);
		  break;
	      }
	  }
	  if (ifstate->link.valid == TRUE && ifstate->link.active == FALSE) {
	      manual_inactive(ifstate);
	      break;
	  }

	  /* set the new address */
	  (void)inet_add(ifstate, manual->our_ip, &manual->our_mask, NULL);
	  ifstate_publish_success(ifstate, NULL, 0);
	  break;
      }
      default: {
	  break;
      }
    }
    return;
}

ipconfig_status_t
manual_thread(IFState_t * ifstate, IFEventID_t evid, void * event_data)
{
    IFState_manual_t *	manual = (IFState_manual_t *)ifstate->private;
    ipconfig_status_t	status = ipconfig_status_success_e;

    switch (evid) {
      case IFEventID_start_e: {
	  start_event_data_t *    evdata = ((start_event_data_t *)event_data);
	  ipconfig_method_data_t *ipcfg = evdata->config.data;

	  if (manual) {
	      my_log(LOG_DEBUG, "MANUAL %s: re-entering start state", 
		     if_name(ifstate->if_p));
	      return (ipconfig_status_internal_error_e);
	  }
	  status = validate_method_data_addresses(&evdata->config,
						  ipconfig_method_manual_e,
						  if_name(ifstate->if_p));
	  if (status != ipconfig_status_success_e)
	      break;
	  manual = malloc(sizeof(*manual));
	  if (manual == NULL) {
	      my_log(LOG_ERR, "MANUAL %s: malloc failed", 
		     if_name(ifstate->if_p));
	      status = ipconfig_status_allocation_failed_e;
	      break;
	  }
	  ifstate->private = manual;
	  bzero(manual, sizeof(*manual));
	  manual->our_ip = ipcfg->ip[0].addr;
	  manual->our_mask = ipcfg->ip[0].mask;
	  if (if_flags(ifstate->if_p) & IFF_LOOPBACK) {
	      /* set the new address */
	      (void)inet_add(ifstate, manual->our_ip, 
			     &manual->our_mask, NULL);
	      ifstate_publish_success(ifstate, NULL, 0);
	      break;
	  }
	  manual->timer = timer_callout_init();
	  if (manual->timer == NULL) {
	      my_log(LOG_ERR, "MANUAL %s: timer_callout_init failed", 
		     if_name(ifstate->if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  manual->arp = arp_client_init(G_arp_session, ifstate->if_p);
	  if (manual->arp == NULL) {
	      my_log(LOG_ERR, "MANUAL %s: arp_client_init failed", 
		     if_name(ifstate->if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  my_log(LOG_DEBUG, "MANUAL %s: starting", if_name(ifstate->if_p));
	  manual_start(ifstate, IFEventID_start_e, NULL);
	  break;
      }
      stop:
      case IFEventID_stop_e: {
	  if (manual == NULL) {
	      break;
	  }

	  /* remove IP address(es) */
	  ifstate_remove_addresses(ifstate);

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
	  ifstate->private = NULL;
	  break;
      }
      case IFEventID_change_e: {
	  change_event_data_t *   evdata = ((change_event_data_t *)event_data);
	  ipconfig_method_data_t *ipcfg = evdata->config.data;

	  if (manual == NULL) {
	      my_log(LOG_DEBUG, "MANUAL %s: private data is NULL", 
		     if_name(ifstate->if_p));
	      status = ipconfig_status_internal_error_e;
	      break;
	  }
	  status = validate_method_data_addresses(&evdata->config,
						  ipconfig_method_manual_e,
						  if_name(ifstate->if_p));
	  if (status != ipconfig_status_success_e)
	      break;
	  evdata->needs_stop = FALSE;
	  if (ipcfg->ip[0].addr.s_addr != manual->our_ip.s_addr) {
	      evdata->needs_stop = TRUE;
	  }
	  else if (ipcfg->ip[0].mask.s_addr != manual->our_mask.s_addr) {
	      manual->our_mask = ipcfg->ip[0].mask;
	      (void)inet_add(ifstate, manual->our_ip, &manual->our_mask, 
			     NULL);
	      /* publish new mask */
	      ifstate_publish_success(ifstate, NULL, 0);
	  }
	  break;
      }
      case IFEventID_media_e: {
	  if (manual == NULL)
	      return (ipconfig_status_internal_error_e);
	  if (ifstate->link.valid == TRUE) {
	      if (ifstate->link.active == TRUE) {
		  manual_start(ifstate, IFEventID_start_e, NULL);
	      }
	      else {
		  struct timeval tv;

		  /* if link goes down and stays down long enough, unpublish */
		  manual_cancel_pending_events(ifstate);
		  tv.tv_sec = G_link_inactive_secs;
		  tv.tv_usec = 0;
		  timer_set_relative(manual->timer, tv, 
				     (timer_func_t *)manual_link_timer,
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
