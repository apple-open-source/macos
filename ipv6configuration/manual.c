/*
 * Copyright (c) 2003-2008 Apple Inc. All rights reserved.
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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <sys/time.h>
#include <ctype.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <syslog.h>

#include "globals.h"
#include "timer.h"
#include "configthreads_common.h"
#include "ip6config_utils.h"

typedef struct {
	struct in6_addr	our_addr;
	int		our_prefixlen;
	int		our_flags;
	timer_callout_t *timer;
} Service_manual_t;

static void
manual_cancel_pending_events(Service_t * service_p)
{
    Service_manual_t *	manual = (Service_manual_t *)service_p->private;

    if (manual == NULL)
	return;
    if (manual->timer) {
	timer_cancel(manual->timer);
    }
    return;
}

static void
manual_inactive(Service_t * service_p)
{
    service_remove_addresses(service_p);
    service_publish_failure(service_p, ip6config_status_media_inactive_e,
			    NULL);
    return;
}

static void
manual_link_timer(void * arg0, void * arg1, void * arg2)
{
    manual_inactive((Service_t *) arg0);
    return;
}

static ip6config_status_t
validate_method_data_addresses(config_data_t * cfg, ip6config_method_t method,
			       char * ifname)
{
    int		i;
    char	str[INET6_ADDRSTRLEN];

    if (cfg->data->n_ip6 < 1) {
	my_log(LOG_DEBUG, "%s %s: no IP6 addresses specified",
	       ip6config_method_string(method), ifname);
	return (ip6config_status_invalid_parameter_e);
    }
    for (i = 0; i < cfg->data->n_ip6; i++) {
	if (ip_valid(&cfg->data->ip6[i].addr) == FALSE) {
	    my_log(LOG_DEBUG, "%s %s: invalid IP6 %s",
		   ip6config_method_string(method), ifname,
		   inet_ntop(AF_INET6, &cfg->data->ip6[i].addr, str, sizeof(str)));
	    return (ip6config_status_invalid_parameter_e);
	}
    }
    return (ip6config_status_success_e);
}

static void
manual_start(Service_t * service_p)
{
    Service_manual_t *	manual = (Service_manual_t *)service_p->private;

    manual_cancel_pending_events(service_p);

    if (service_link_status(service_p)->valid == TRUE
	&& service_link_status(service_p)->active == FALSE) {
	manual_inactive(service_p);
	return;
    }

    (void)service_set_address(service_p, &manual->our_addr,
			      manual->our_prefixlen,
			      manual->our_flags);

    return;
}

__private_extern__ ip6config_status_t
manual_thread(Service_t * service_p, IFEventID_t evid, void * event_data)
{
    interface_t *	if_p = service_interface(service_p);
    Service_manual_t *	manual = (Service_manual_t *)service_p->private;
    ip6config_status_t	status = ip6config_status_success_e;

    switch (evid) {
	case IFEventID_start_e: {
	    start_event_data_t *    evdata = ((start_event_data_t *)event_data);
	    ip6config_method_data_t *ipcfg = evdata->config.data;

	    my_log(LOG_DEBUG, "MANUAL_THREAD %s: STARTING", if_name(if_p));

	    if (manual) {
		my_log(LOG_DEBUG, "MANUAL_THREAD %s: re-entering start state",
		       if_name(if_p));
		status = ip6config_status_internal_error_e;
		break;
	    }

	    status = validate_method_data_addresses(&evdata->config,
						    ip6config_method_manual_e, if_name(if_p));
	    if (status != ip6config_status_success_e) {
		my_log(LOG_DEBUG, "MANUAL_THREAD: validate_method_data_addresses returned error");
		break;
	    }

	    manual = calloc(1, sizeof(*manual));
	    if (manual == NULL) {
		my_log(LOG_ERR, "MANUAL_THREAD %s: calloc failed",
		       if_name(if_p));
		status = ip6config_status_allocation_failed_e;
		break;
	    }

	    service_p->private = manual;

	    /* only one manual address per service */
	    memcpy(&manual->our_addr, &ipcfg->ip6[0].addr, sizeof(struct in6_addr));
	    manual->our_prefixlen = ipcfg->ip6[0].prefixLen;
	    manual->our_flags = ipcfg->ip6[0].flags;

	    if (if_flags(if_p) & IFF_LOOPBACK) {
		/* set the new address */
		(void)service_set_address(service_p, &manual->our_addr,
					  manual->our_prefixlen,
					  manual->our_flags);
		service_publish_success(service_p);
		break;
	    }

	    manual->timer = timer_callout_init();
	    if (manual->timer == NULL) {
		my_log(LOG_ERR, "MANUAL_THREAD %s: timer_callout_init failed",
		       if_name(if_p));
		status = ip6config_status_allocation_failed_e;
		goto stop;
	    }

	    my_log(LOG_DEBUG, "MANUAL_THREAD %s: starting", if_name(if_p));
	    manual_start(service_p);
	    break;
	}
     stop:
	case IFEventID_stop_e: {
	    my_log(LOG_DEBUG, "MANUAL_THREAD %s: STOPPING", if_name(if_p));

	    if (manual == NULL) {
		my_log(LOG_DEBUG, "MANUAL %s: private data is NULL",
		       if_name(if_p));
		status = ip6config_status_internal_error_e;
		break;
	    }

	    /* remove IP6 address */
	    service_remove_addresses(service_p);

	    /* clean-up resources */
	    if (manual->timer)
		timer_callout_free(&manual->timer);

	    free(manual);
	    service_p->private = NULL;
	    break;
	}
	case IFEventID_change_e: {
	    change_event_data_t *	evdata = ((change_event_data_t *)event_data);
	    ip6config_method_data_t *	ipcfg = evdata->config.data;

	    my_log(LOG_DEBUG, "MANUAL_THREAD %s: CHANGING", if_name(if_p));

	    if (manual == NULL) {
		my_log(LOG_DEBUG, "MANUAL %s: private data is NULL",
		       if_name(if_p));
		status = ip6config_status_internal_error_e;
		break;
	    }

	    status = validate_method_data_addresses(&evdata->config,
					    ip6config_method_manual_e, if_name(if_p));
	    if (status != ip6config_status_success_e)
		break;

		evdata->needs_stop = FALSE;

		if (!IN6_ARE_ADDR_EQUAL(&ipcfg->ip6[0].addr, &manual->our_addr) ||
		    (ipcfg->ip6[0].prefixLen - manual->our_prefixlen != 0)) {
		    evdata->needs_stop = TRUE;
		    break;
		}
		break;
       }
	case IFEventID_state_change_e: {
	    int	i, j;
	    ip6_addrinfo_list_t * 	ip6_addrs = ((ip6_addrinfo_list_t *)event_data);
	    ip6_addrinfo_list_t *	addrslist_p = &service_p->info.addrs;

	    my_log(LOG_DEBUG, "MANUAL_THREAD %s: STATE CHANGE", if_name(if_p));

	    if (manual == NULL) {
		my_log(LOG_DEBUG, "MANUAL %s: private data is NULL",
		       if_name(if_p));
		status = ip6config_status_internal_error_e;
		break;
	    }

	    /* go through the address lists; autoconf or linklocal addrs are invalid */
	    for (i = 0; i < ip6_addrs->n_addrs; i++) {
		int	done = 0;
		ip6_addrinfo_t	*new_addr = ip6_addrs->addr_list + i;

		if (IN6_IS_ADDR_LINKLOCAL(&new_addr->addr)
		    || (new_addr->flags & IN6_IFF_AUTOCONF)) {
		    continue;
		}

		for (j = 0; j < addrslist_p->n_addrs; j++) {
		    if (IN6_ARE_ADDR_EQUAL(&new_addr->addr, &addrslist_p->addr_list[j].addr)) {
			/* check for duplicate */
			if (new_addr->flags & IN6_IFF_DUPLICATED) {
				char	msg[128];

				snprintf(msg, sizeof(msg),
					 IP6_FORMAT " is a duplicate address on interface %s",
					 IP6_LIST(&addrslist_p->addr_list[j].addr), if_name(if_p));
				my_log(LOG_ERR, "MANUAL %s: %s", if_name(if_p), msg);

				service_report_conflict(service_p, &addrslist_p->addr_list[j].addr);
				service_remove_addresses(service_p);
				service_publish_failure(service_p,
							ip6config_status_address_in_use_e,
							msg);
				break;
			}

			service_publish_success(service_p);
			done++;
			break;
		    }
		}
		if (done)
		    break;
	    }

	    break;
	}
	case IFEventID_media_e: {
	    if (manual == NULL)
		return (ip6config_status_internal_error_e);

	    if (service_link_status(service_p)->valid == TRUE) {
		if (service_link_status(service_p)->active == TRUE) {
		    manual_start(service_p);
		}
		else {
		    struct timeval tv;

		    /* if link goes down and stays down long enough, unpublish */
		    manual_cancel_pending_events(service_p);
		    tv.tv_sec = LINK_INACTIVE_WAIT_SECS;
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
