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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <net/if_media.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>

#include "configthreads_common.h"
#include "globals.h"
#include "interfaces.h"
#include "ip6config_utils.h"

static boolean_t
if_gifmedia(int sockfd, char * name, boolean_t * status)
{
    struct ifmediareq 	ifmr;
    boolean_t 		valid = FALSE;

    *status = FALSE;
    (void) memset(&ifmr, 0, sizeof(ifmr));
    (void) strncpy(ifmr.ifm_name, name, sizeof(ifmr.ifm_name));

    if (ioctl(sockfd, SIOCGIFMEDIA, (caddr_t)&ifmr) >= 0
	&& ifmr.ifm_count > 0
	&& ifmr.ifm_status & IFM_AVALID) {
	valid = TRUE;
	if (ifmr.ifm_status & IFM_ACTIVE)
	    *status = TRUE;
    }
    return (valid);
}

static boolean_t
get_media_status(char * name, boolean_t * media_status)
{
    boolean_t	media_valid = FALSE;
    int		sockfd;

    if ((sockfd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
	my_log(LOG_INFO, "get_media_status (%s): socket failed, %s",
	       name, strerror(errno));
	return (FALSE);
    }
    media_valid = if_gifmedia(sockfd, name, media_status);
    close(sockfd);
    return (media_valid);
}

__private_extern__ Service_t *
IFState_service_with_ID(IFState_t * ifstate, CFStringRef serviceID)
{
    int		count;
    int		j;

    count = dynarray_count(&ifstate->services);
    for (j = 0; j < count; j++) {
	Service_t *	service_p = dynarray_element(&ifstate->services, j);

	if (CFEqual(serviceID, service_p->serviceID)) {
	    return (service_p);
	}
    }
    return (NULL);
}

__private_extern__ Service_t *
IFState_service_with_ip(IFState_t * ifstate, struct in6_addr * iaddr)
{
    int	count;
    int	i, j;

    count = dynarray_count(&ifstate->services);
    for (i = 0; i < count; i++) {
	Service_t *	service_p = dynarray_element(&ifstate->services, i);

	for (j = 0; j < service_p->info.addrs.n_addrs; j++) {
	    if (IN6_ARE_ADDR_EQUAL(&service_p->info.addrs.addr_list[j].addr, iaddr)) {
		return (service_p);
	    }
	}
    }
    return (NULL);
}

__private_extern__ void
IFState_services_free(IFState_t * ifstate)
{
    dynarray_free(&ifstate->services);
    if (ifstate->llocal_service) {
	Service_free(ifstate->llocal_service);
	ifstate->llocal_service = NULL;
    }
    dynarray_init(&ifstate->services, Service_free, NULL);
    return;
}

__private_extern__ void
IFState_service_free(IFState_t * ifstate, CFStringRef serviceID)
{
    int		count;
    int		j;

    count = dynarray_count(&ifstate->services);
    for (j = 0; j < count; j++) {
	Service_t *	service_p = dynarray_element(&ifstate->services, j);

	if (CFEqual(serviceID, service_p->serviceID)) {
	    dynarray_free_element(&ifstate->services, j);
	    return;
	}
    }
    return;
}

__private_extern__ ip6config_status_t
IFState_service_add(IFState_t * ifstate, CFStringRef serviceID,
		    ip6config_method_t method,
		    void * method_data)
{
    Service_t *		service_p = NULL;
    ip6config_status_t	status = ip6config_status_success_e;

    /* create linklocal service when first starting up */
    if (ifstate->llocal_service == NULL &&
	method != ip6config_method_6to4_e) {
	ifstate->llocal_service = Service_init(ifstate, NULL,
					       ip6config_method_linklocal_e,
					       NULL, &status);

	if (ifstate->llocal_service == NULL) {
	    my_log(LOG_DEBUG, "status from %s was %s",
		   ip6config_method_string(ip6config_method_linklocal_e),
		   ip6config_status_string(status));
	    return (status);
	}
    }

    /* try to configure the service */
    service_p = Service_init(ifstate, serviceID, method,
			     method_data, &status);
    if (service_p == NULL) {
	my_log(LOG_DEBUG, "status from %s was %s",
	       ip6config_method_string(method),
	       ip6config_status_string(status));
    }
    else {
	dynarray_add(&ifstate->services, service_p);
    }

    return (status);
}

__private_extern__ void
IFState_update_media_status(IFState_t * ifstate)
{
    char * 		ifname = if_name(ifstate->if_p);
    link_status_t	link = {FALSE, FALSE};

    link.valid = get_media_status(ifname, &link.active);
    if (link.valid == FALSE) {
	my_log(LOG_DEBUG, "IFState_update_media_status: %s link is unknown", ifname);
    }
    else {
	my_log(LOG_DEBUG, "%s link is %s", ifname, link.active ? "up" : "down");
    }
    ifstate->link = link;
    return;
}

static IFState_t *
IFState_init(interface_t * if_p)
{
    IFState_t * ifstate;

    ifstate = malloc(sizeof(*ifstate));
    if (ifstate == NULL) {
	my_log(LOG_ERR, "IFState_init: malloc ifstate failed");
	return (NULL);
    }

    bzero(ifstate, sizeof(*ifstate));
    ifstate->if_p = if_dup(if_p);
    ifstate->ifname = (void *) CFStringCreateWithCString(NULL, if_name(if_p),
							kCFStringEncodingMacRoman);
    IFState_update_media_status(ifstate);
    dynarray_init(&ifstate->services, Service_free, NULL);

    return (ifstate);
}

__private_extern__ void
IFState_free(void * arg)
{
    IFState_t *	ifstate = (IFState_t *)arg;

    SCLog(G_verbose, LOG_INFO, CFSTR("IFState_free(%s)"), if_name(ifstate->if_p));
    IFState_services_free(ifstate);
    my_CFRelease(&ifstate->ifname);
    if_free(&ifstate->if_p);
    free(ifstate);
    return;
}

__private_extern__ IFState_t *
IFStateList_ifstate_with_name(IFStateList_t * list, char * ifname, int * where)
{
    int count;
    int i;

    count = dynarray_count(list);
    for (i = 0; i < count; i++) {
	IFState_t *	element = dynarray_element(list, i);
	if (strcmp(if_name(element->if_p), ifname) == 0) {
	    if (where != NULL) {
		*where = i;
	    }
	    return (element);
	}
    }
    return (NULL);
}

__private_extern__ IFState_t *
IFStateList_ifstate_create(IFStateList_t * list, interface_t * if_p)
{
    IFState_t *   ifstate;

    ifstate = IFStateList_ifstate_with_name(list, if_name(if_p), NULL);
    if (ifstate == NULL) {
	ifstate = IFState_init(if_p);
	if (ifstate) {
	    dynarray_add(list, ifstate);
	}
    }
    return (ifstate);
}

__private_extern__ void
IFStateList_ifstate_free(IFStateList_t * list, char * ifname)
{
    IFState_t *	ifstate;
    int		where = -1;

    ifstate = IFStateList_ifstate_with_name(list, ifname, &where);
    if (ifstate == NULL) {
	return;
    }
    dynarray_free_element(list, where);
    return;
}

__private_extern__ IFState_t *
IFStateList_service_with_ID(IFStateList_t * list, CFStringRef serviceID,
			    Service_t * * ret_service)
{
    int count;
    int i;

    count = dynarray_count(list);
    for (i = 0; i < count; i++) {
	IFState_t *	ifstate = dynarray_element(list, i);
	Service_t *	service_p;

	service_p = IFState_service_with_ID(ifstate, serviceID);
	if (service_p) {
	    if (ret_service) {
		*ret_service = service_p;
	    }
	    return (ifstate);
	}
    }
    if (ret_service) {
	*ret_service = NULL;
    }
    return (NULL);
}

__private_extern__ IFState_t *
IFStateList_service_with_ip(IFStateList_t * list, struct in6_addr * iaddr,
			    Service_t * * ret_service)
{
    int count;
    int i;

    count = dynarray_count(list);
    for (i = 0; i < count; i++) {
	IFState_t *	ifstate = dynarray_element(list, i);
	Service_t *	service_p;

	service_p = IFState_service_with_ip(ifstate, iaddr);
	if (service_p) {
	    if (ret_service) {
		*ret_service = service_p;
	    }
	    return (ifstate);
	}
    }
    if (ret_service) {
	*ret_service = NULL;
    }
    return (NULL);
}
