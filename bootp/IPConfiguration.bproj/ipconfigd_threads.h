
#ifndef _S_IPCONFIGD_THREADS_H
#define _S_IPCONFIGD_THREADS_H
/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
 * ipconfigd_threads.h
 * - definitions required by the configuration threads
 */
/* 
 * Modification History
 *
 * May 10, 2000		Dieter Siegmund (dieter@apple.com)
 * - created
 */

#include <mach/boolean.h>
#include "ipconfig_types.h"
#include "ipconfigd_globals.h"
#include "bootp_session.h"
#include "arp_session.h"
#include "timer.h"
#include "interfaces.h"

typedef enum {
    IFEventID_start_e = 0,		/* start the configuration method */
    IFEventID_stop_e, 			/* stop/clean-up */
    IFEventID_timeout_e,
    IFEventID_media_e,			/* e.g. link status change */
    IFEventID_data_e,			/* server data to process */
    IFEventID_arp_e,			/* ARP check results */
    IFEventID_change_e,			/* ask config method to change */
    IFEventID_renew_e,			/* ask config method to renew */
    IFEventID_arp_collision_e,		/* there was an ARP collision */
    IFEventID_last_e,
} IFEventID_t;

static __inline__ const unsigned char * 
IFEventID_names(IFEventID_t evid)
{
    static const unsigned char * names[] = {
	"START",
	"STOP",
	"TIMEOUT",
	"MEDIA",
	"DATA",
	"ARP",
	"CHANGE",
	"RENEW",
	"ARP COLLISION",
    };
    if (evid < IFEventID_start_e || evid >= IFEventID_last_e)
	return ("<unknown event>");
    return (names[evid]);
}

typedef struct ServiceState Service_t;
typedef struct IFState IFState_t;
typedef ipconfig_status_t (ipconfig_func_t)(Service_t * service_p, 
					    IFEventID_t evid, void * evdata);
struct completion_results {
    ipconfig_status_t		status;
    void *			pkt;
    int				pkt_size;
    dhcpol_t			options;
    boolean_t			ready;
    char *			msg;
};

typedef struct {
    boolean_t			valid;
    boolean_t			active;
} link_status_t;

struct ServiceState {
    IFState_t *			ifstate;
    ipconfig_method_t		method;
    void *			serviceID;
    void *			parent_serviceID;
    void *			child_serviceID;
    void *			user_notification;
    void *			user_rls;
    struct completion_results	published;
    inet_addrinfo_t		info;
    boolean_t			free_in_progress;
    void * 			private;
};

struct IFState {
    interface_t *		if_p;
    void *			ifname;
    link_status_t		link;
    dynarray_t			services;
    boolean_t			startup_ready;
    boolean_t			free_in_progress;
    boolean_t			netboot;
};

struct saved_pkt {
    dhcpol_t			options;
    char			pkt[1500];
    int				pkt_size;
    unsigned 			rating;
    boolean_t			is_dhcp;
    struct in_addr		our_ip;
    struct in_addr		server_ip;
};

typedef struct {
    ipconfig_method_data_t *	data;
    unsigned int		data_len;
} config_data_t;

typedef struct {
    config_data_t		config;
} start_event_data_t;

typedef struct {
    config_data_t		config;
    boolean_t			needs_stop;
} change_event_data_t;

typedef struct {
    struct in_addr		ip_addr;
    void *			hwaddr;
    int				hwlen;
} arp_collision_data_t;

extern struct ether_addr *ether_aton(char *);

/*
 * Function: ip_valid
 * Purpose:
 *   Perform some cursory checks on the IP address
 *   supplied by the server
 */
static __inline__ boolean_t
ip_valid(struct in_addr ip)
{
    if (ip.s_addr == 0
	|| ip.s_addr == INADDR_BROADCAST)
	return (FALSE);
    return (TRUE);
}

static __inline__ ipconfig_status_t
validate_method_data_addresses(config_data_t * cfg, ipconfig_method_t method,
			       char * ifname)
{
    if (cfg->data_len < sizeof(ipconfig_method_data_t)
	+ sizeof(struct in_addr) * 2) {
	my_log(LOG_DEBUG, "%s %s: method data too short (%d bytes)",
	       ipconfig_method_string(method), ifname, cfg->data_len);
	return (ipconfig_status_invalid_parameter_e);
    }
    if (cfg->data->n_ip == 0) {
	my_log(LOG_DEBUG, "%s %s: no IP addresses specified", 
	       ipconfig_method_string(method), ifname);
	return (ipconfig_status_invalid_parameter_e);
    }
    if (ip_valid(cfg->data->ip[0].addr) == FALSE) {
	my_log(LOG_DEBUG, "%s %s: invalid IP %s", 
	       ipconfig_method_string(method), ifname,
	       inet_ntoa(cfg->data->ip[0].addr));
	return (ipconfig_status_invalid_parameter_e);
    }
    return (ipconfig_status_success_e);
}

extern unsigned	count_params(dhcpol_t * options, u_char * tags, int size);

extern char *	computer_name();

static __inline__ IFState_t *
service_ifstate(Service_t * service_p)
{
    return (service_p->ifstate);
}

int
service_enable_autoaddr(Service_t * service_p);

int
service_disable_autoaddr(Service_t * service_p);

int
service_set_address(Service_t * service_p, struct in_addr ip, 
		    struct in_addr mask, struct in_addr  broadcast);

int
service_remove_address(Service_t * service_p);

void
service_publish_success(Service_t * service_p, void * pkt, int pkt_size);

void
service_publish_failure(Service_t * service_p, 
			ipconfig_status_t status, char * msg);

void
service_publish_failure_sync(Service_t * service_p, ipconfig_status_t status,
			     char * msg, boolean_t sync);


void
service_report_conflict(Service_t * service_p, struct in_addr * ip,
			void * hwaddr, struct in_addr * server);

void
service_tell_user(Service_t * service_p, char * msg);

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

Service_t *
service_parent_service(Service_t * service_p);

void
linklocal_service_change(Service_t * parent_service_p, boolean_t no_allocate);

/* 
 * interface configuration "threads" 
 */
ipconfig_status_t
bootp_thread(Service_t * service_p, IFEventID_t evid, void * evdata);

ipconfig_status_t
dhcp_thread(Service_t * service_p, IFEventID_t evid, void * evdata);

ipconfig_status_t
manual_thread(Service_t * service_p, IFEventID_t evid, void * evdata);

ipconfig_status_t
inform_thread(Service_t * service_p, IFEventID_t evid, void * evdata);

ipconfig_status_t
linklocal_thread(Service_t * service_p, IFEventID_t evid, void * evdata);

/*
 * DHCP lease information
 */
boolean_t
dhcp_lease_read(const char *, struct in_addr *);

boolean_t
dhcp_lease_write(const char *, struct in_addr);

void
dhcp_lease_clear(const char *);

void
netboot_addresses(struct in_addr * ip, struct in_addr * server_ip);

/*
 * in dhcp.c
 */
void
dhcp_set_default_parameters(u_char * params, int n_params);

void
dhcp_set_additional_parameters(u_char * params, int n_params);

/* 
 * routing table
 */
boolean_t
subnet_route_add(struct in_addr gateway, struct in_addr netaddr, 
		 struct in_addr netmask, char * ifname);

boolean_t
subnet_route_delete(struct in_addr gateway, struct in_addr netaddr, 
		    struct in_addr netmask, char * ifname);

/*
 * more globals
 */
extern bootp_session_t *	G_bootp_session;
extern arp_session_t *		G_arp_session;


#endif _S_IPCONFIGD_THREADS_H
