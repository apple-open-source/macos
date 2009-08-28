
#ifndef _CONFIGTHREADS_COMMON_H_
#define _CONFIGTHREADS_COMMON_H_
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

/*
 * configthreads_common.h
 * - definitions required by the configuration threads
 */

#include <SystemConfiguration/SystemConfiguration.h>

#include "interfaces.h"
#include "configthreads_types.h"


/*
 * Typedefs and Structures
 */
typedef struct ServiceState Service_t;
typedef dynarray_t IFStateList_t;
typedef struct IFState IFState_t;
typedef struct {
    CFStringRef			serviceID;
    ip6config_method_t		method;
    ip6config_method_data_t *	method_data;
} ServiceConfig_t;

typedef enum {
    IFEventID_start_e = 0,		/* start the configuration method */
    IFEventID_stop_e, 			/* stop/clean-up */
    IFEventID_timeout_e,
    IFEventID_media_e,			/* e.g. link status change */
    IFEventID_data_e,			/* data to process */
    IFEventID_change_e,			/* ask config method to change */
    IFEventID_state_change_e,		/* state address data has changed */
    IFEventID_ipv4_primary_change_e, /* primary ipv4 service has changed */
    IFEventID_last_e,
} IFEventID_t;

typedef ip6config_status_t (ip6config_func_t)(Service_t * service_p,
					      IFEventID_t evid, void * evdata);

typedef struct {
    boolean_t			valid;
    boolean_t			active;
} link_status_t;

typedef struct {
    ip6config_method_data_t *	data;
} config_data_t;

typedef struct {
    config_data_t		config;
} start_event_data_t;

typedef struct {
    config_data_t		config;
    boolean_t			needs_stop;
} change_event_data_t;

struct ServiceState {
    IFState_t *			ifstate;
    ip6config_method_t		method;
    void *			serviceID;
    void *			user_notification;
    void *			user_rls;
    inet6_addrinfo_t		info;
    void * 			private;
};

struct IFState {
    interface_t *		if_p;
    void *			ifname;
    link_status_t		link;
    dynarray_t			services;
    Service_t *			llocal_service;
};

/*
 * Inlines
 */
static __inline__ boolean_t
ip6config_method_is_dynamic(ip6config_method_t method)
{
    if (method == ip6config_method_automatic_e
	|| method == ip6config_method_rtadv_e
	|| method == ip6config_method_6to4_e) {
	return (TRUE);
    }
    return (FALSE);
}

static __inline__ boolean_t
ip6config_method_is_manual(ip6config_method_t method)
{
    if (method == ip6config_method_manual_e) {
	return (TRUE);
    }

    return (FALSE);
}

static __inline__ IFState_t *
service_ifstate(Service_t * service_p)
{
    return (service_p->ifstate);
}

static __inline__ interface_t *
service_interface(Service_t * service_p)
{
    return (service_p->ifstate->if_p);
}

static __inline__ link_status_t *
service_link_status(Service_t * service_p)
{
    return (&service_p->ifstate->link);
}

static __inline__ const char *
IFEventID_names(IFEventID_t evid)
{
    static const char * names[] = {
	"START",
	"STOP",
	"TIMEOUT",
	"MEDIA",
	"DATA",
	"CHANGE",
	"STATE_CHANGE",
	"IPV4_PRIMARY_CHANGE"
    };
    if (evid < IFEventID_start_e || evid >= IFEventID_last_e)
	return ("<unknown event>");
    return (names[evid]);
}


/*
 * IFState related (IFState.c)
 */

Service_t *		IFState_service_with_ID(IFState_t * ifstate, CFStringRef serviceID);
Service_t *		IFState_service_with_ip(IFState_t * ifstate, struct in6_addr * iaddr);
void			IFState_services_free(IFState_t * ifstate);
void			IFState_service_free(IFState_t * ifstate, CFStringRef serviceID);
ip6config_status_t	IFState_service_add(IFState_t * ifstate, CFStringRef serviceID,
					    ip6config_method_t method, void * method_data);
void			IFState_update_media_status(IFState_t * ifstate);
void			IFState_free(void * arg);
IFState_t *		IFStateList_ifstate_with_name(IFStateList_t * list, char * ifname, int * where);
IFState_t *		IFStateList_ifstate_create(IFStateList_t * list, interface_t * if_p);
void			IFStateList_ifstate_free(IFStateList_t * list, char * ifname);
IFState_t *		IFStateList_service_with_ID(IFStateList_t * list, CFStringRef serviceID,
						    Service_t * * ret_service);
IFState_t *		IFStateList_service_with_ip(IFStateList_t * list, struct in6_addr * iaddr,
						    Service_t * * ret_service);


#define PROP_SERVICEID		CFSTR("ServiceID")

/* LINK_INACTIVE_WAIT_SECS: Time to wait after the link goes
 *   inactive before unpublishing the interface state information
 */
#define LINK_INACTIVE_WAIT_SECS	1

/*
 * service related (service.c)
 */

CFDictionaryRef	my_SCDynamicStoreCopyValue(SCDynamicStoreRef session, CFStringRef key);

int		service_set_addresses(Service_t * service_p, ip6_addrinfo_list_t * addr_list);
int		service_set_address(Service_t * service_p, struct in6_addr * addr, int prefixLen, int flags);
int		service_remove_addresses(Service_t * service_p);

void		service_publish_success(Service_t * service_p);

void            service_publish_clear(Service_t * service_p);
void		service_publish_failure(Service_t * service_p, ip6config_status_t status, char * msg);

ip6config_status_t service_set_service(IFState_t * ifstate, ServiceConfig_t * config);
void		service_free_inactive_services(char * ifname, ServiceConfig_t * config_list,
					       int count);

void		service_config_list_free(ServiceConfig_t * * list_p_p, int count);
ServiceConfig_t * service_config_list_lookup_method(ServiceConfig_t * config_list,
						    int count, ip6config_method_t method,
						    ip6config_method_data_t * method_data);
ServiceConfig_t * service_config_list_init(SCDynamicStoreRef session, CFArrayRef all_ipv6,
					   char * ifname, int * count_p);

void		Service_free(void * arg);
Service_t *	Service_init(IFState_t * ifstate, CFStringRef serviceID,
			     ip6config_method_t method,
			     void * method_data, ip6config_status_t * status_p);
void		service_report_conflict(Service_t * service_p, struct in6_addr * ip6);


/*
 * interface configuration "threads"
 */
ip6config_status_t
manual_thread(Service_t * service_p, IFEventID_t evid, void * evdata);

ip6config_status_t
rtadv_thread(Service_t * service_p, IFEventID_t evid, void * evdata);

ip6config_status_t
stf_thread(Service_t * service_p, IFEventID_t evid, void * evdata);

ip6config_status_t
linklocal_thread(Service_t * llocal_service, IFEventID_t evid, void * event_data);

#endif _CONFIGTHREADS_COMMON_H_
