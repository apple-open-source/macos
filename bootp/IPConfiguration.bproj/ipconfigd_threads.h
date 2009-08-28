
#ifndef _S_IPCONFIGD_THREADS_H
#define _S_IPCONFIGD_THREADS_H
/*
 * Copyright (c) 2000-2009 Apple Inc. All rights reserved.
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
#include "dhcp.h"
#include "ipconfig_types.h"
#include "globals.h"
#include "bootp_session.h"
#include "arp_session.h"
#include "timer.h"
#include "interfaces.h"
#include <TargetConditionals.h>

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
    IFEventID_sleep_e,			/* system will sleep */
    IFEventID_wake_e,			/* system has awoken */
    IFEventID_power_off_e,		/* system is powering off */
    IFEventID_last_e,
} IFEventID_t;

static __inline__ const char * 
IFEventID_names(IFEventID_t evid)
{
    static const char * names[] = {
	"START",
	"STOP",
	"TIMEOUT",
	"MEDIA",
	"DATA",
	"ARP",
	"CHANGE",
	"RENEW",
	"ARP COLLISION",
	"SLEEP",
	"WAKE",
	"POWER OFF",
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

#define RIFLAGS_IADDR_VALID	(uint32_t)0x1
#define RIFLAGS_HWADDR_VALID	(uint32_t)0x2
#define RIFLAGS_ARP_VERIFIED	(uint32_t)0x4
#define RIFLAGS_ALL_VALID	(RIFLAGS_IADDR_VALID | RIFLAGS_HWADDR_VALID | RIFLAGS_ARP_VERIFIED)

typedef struct {
    uint32_t			flags;
    struct in_addr		iaddr;
    uint8_t			hwaddr[MAX_LINK_ADDR_LEN];
} router_info_t;

struct ServiceState {
    IFState_t *			ifstate;
    ipconfig_method_t		method;
    ip_addr_mask_t		requested_ip;
    void *			serviceID;
    void *			parent_serviceID;
    void *			child_serviceID;
#if ! TARGET_OS_EMBEDDED
    void *			user_notification;
    void *			user_rls;
#endif /* ! TARGET_OS_EMBEDDED */
    struct completion_results	published;
    inet_addrinfo_t		info;
    router_info_t		router;
    boolean_t			free_in_progress;
    boolean_t			is_dynamic;
    void * 			private;
};

struct IFState {
    interface_t *		if_p;
    CFStringRef			ifname;
    link_status_t		link;
    dynarray_t			services;
    Service_t *			linklocal_service_p;
    boolean_t			startup_ready;
    boolean_t			free_in_progress;
    boolean_t			netboot;
    CFStringRef			ssid;
};

struct saved_pkt {
    dhcpol_t			options;
    uint8_t			pkt[1500];
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

extern ipconfig_status_t
validate_method_data_addresses(config_data_t * cfg, ipconfig_method_t method,
			       const char * ifname);

extern unsigned	
count_params(dhcpol_t * options, const uint8_t * tags, int size);

extern void *
find_option_with_length(dhcpol_t * options, dhcptag_t tag, int min_length);

extern char *	computer_name();

static __inline__ IFState_t *
service_ifstate(Service_t * service_p)
{
    return (service_p->ifstate);
}

static __inline__ void
service_set_requested_ip_addr(Service_t * service_p, struct in_addr ip)
{
    service_p->requested_ip.addr = ip;
    return;
}

static __inline__ struct in_addr
service_requested_ip_addr(Service_t * service_p)
{
    return (service_p->requested_ip.addr);
}

static __inline__ struct in_addr *
service_requested_ip_addr_ptr(Service_t * service_p)
{
    return (&service_p->requested_ip.addr);
}

static __inline__ void
service_set_requested_ip_mask(Service_t * service_p, struct in_addr mask)
{
    service_p->requested_ip.mask = mask;
    return;
}

static __inline__ struct in_addr
service_requested_ip_mask(Service_t * service_p)
{
    return (service_p->requested_ip.mask);
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
service_publish_success2(Service_t * service_p, void * pkt, int pkt_size,
			 absolute_time_t start);

void
service_publish_failure(Service_t * service_p, 
			ipconfig_status_t status, char * msg);

void
service_publish_failure_sync(Service_t * service_p, ipconfig_status_t status,
			     char * msg, boolean_t sync);

#if TARGET_OS_EMBEDDED
static __inline__ void
service_report_conflict(Service_t * service_p, struct in_addr * ip,
			const void * hwaddr, struct in_addr * server)
{
    /* nothing to do */
}

static __inline__ void
service_remove_conflict(Service_t * service_p)
{
    /* nothing to do */
}

#else /* TARGET_OS_EMBEDDED */
void
service_report_conflict(Service_t * service_p, struct in_addr * ip,
			const void * hwaddr, struct in_addr * server);

void
service_remove_conflict(Service_t * service_p);

#endif /* TARGET_OS_EMBDEDDED */

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

static __inline__ bool
service_is_address_set(Service_t * service_p)
{
    return (service_p->info.addr.s_addr
	    == service_requested_ip_addr_ptr(service_p)->s_addr);
}

Service_t *
service_parent_service(Service_t * service_p);

boolean_t
service_should_do_router_arp(Service_t * service_p);

void
linklocal_service_change(Service_t * parent_service_p, boolean_t no_allocate);

void
linklocal_set_needs_attention(void);

/**
 ** router_arp routines
 **/
static __inline__ boolean_t
service_router_is_hwaddr_valid(Service_t * service_p)
{
    return ((service_p->router.flags & RIFLAGS_HWADDR_VALID) != 0);
}

static __inline__ void
service_router_set_hwaddr_valid(Service_t * service_p)
{
    service_p->router.flags |= RIFLAGS_HWADDR_VALID;
    return;
}

static __inline__ void
service_router_clear_hwaddr_valid(Service_t * service_p)
{
    service_p->router.flags &= ~RIFLAGS_HWADDR_VALID;
    return;
}

static __inline__ boolean_t
service_router_is_iaddr_valid(Service_t * service_p)
{
    return ((service_p->router.flags & RIFLAGS_IADDR_VALID) != 0);
}

static __inline__ void
service_router_set_iaddr_valid(Service_t * service_p)
{
    service_p->router.flags |= RIFLAGS_IADDR_VALID;
    return;
}

static __inline__ void
service_router_clear_iaddr_valid(Service_t * service_p)
{
    service_p->router.flags &= ~RIFLAGS_IADDR_VALID;
    return;
}

static __inline__ boolean_t
service_router_is_arp_verified(Service_t * service_p)
{
    return ((service_p->router.flags & RIFLAGS_ARP_VERIFIED) != 0);
}

static __inline__ void
service_router_set_arp_verified(Service_t * service_p)
{
    service_p->router.flags |= RIFLAGS_ARP_VERIFIED;
    return;
}

static __inline__ void
service_router_clear_arp_verified(Service_t * service_p)
{
    service_p->router.flags &= ~RIFLAGS_ARP_VERIFIED;
    return;
}

static __inline__ void
service_router_clear(Service_t * service_p)
{
    service_p->router.flags = 0;
    return;
}

static __inline__ u_char *
service_router_hwaddr(Service_t * service_p)
{
    return (service_p->router.hwaddr);
}

static __inline__ int
service_router_hwaddr_size(Service_t * service_p)
{
    return (sizeof(service_p->router.hwaddr));
}

static __inline__ struct in_addr
service_router_iaddr(Service_t * service_p)
{
    return (service_p->router.iaddr);
}

static __inline__ void
service_router_set_iaddr(Service_t * service_p, struct in_addr iaddr)
{
    service_p->router.iaddr = iaddr;
    return;
}

static __inline__ boolean_t
service_router_all_valid(Service_t * service_p)
{
    return ((service_p->router.flags & RIFLAGS_ALL_VALID) == RIFLAGS_ALL_VALID);
}

static __inline__ void
service_router_set_all_valid(Service_t * service_p)
{
    service_p->router.flags = RIFLAGS_ALL_VALID;
    return;
}


typedef enum {
    router_arp_status_success_e = 0,
    router_arp_status_no_response_e = 1,
    router_arp_status_failed_e = 99,
} router_arp_status_t;

typedef void (service_resolve_router_callback_t)(Service_t * service_p,
						 router_arp_status_t status);

boolean_t
service_resolve_router(Service_t * service_p, arp_client_t * arp,
		       service_resolve_router_callback_t * callback_func,
		       struct in_addr our_ip);
boolean_t
service_update_router_address(Service_t * service_p, 
			      struct saved_pkt * saved_p);

struct in_addr *
get_router_from_options(dhcpol_t * options_p, struct in_addr our_ip);

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

ipconfig_status_t
failover_thread(Service_t * service_p, IFEventID_t evid, void * evdata);

void
netboot_addresses(struct in_addr * ip, struct in_addr * server_ip);

boolean_t
woke_from_hibernation(void);

/**
 ** DHCPLease, DHCPLeaseList
 **/
typedef struct {
    bool			tentative;
    bool			nak;
    struct in_addr		our_ip;
    absolute_time_t		lease_start;
    dhcp_lease_time_t		lease_length;
    struct in_addr		router_ip;
    uint8_t			router_hwaddr[MAX_LINK_ADDR_LEN];
    int				router_hwaddr_length;
    int				pkt_length;
    uint8_t			pkt[1];
} DHCPLease, * DHCPLeaseRef;

typedef dynarray_t DHCPLeaseList, * DHCPLeaseListRef;

void
DHCPLeaseSetNAK(DHCPLeaseRef lease_p, int nak);


void
DHCPLeaseListInit(DHCPLeaseListRef list_p);

void
DHCPLeaseListFree(DHCPLeaseListRef list_p);

void
DHCPLeaseListClear(DHCPLeaseListRef list_p,
		   const char * ifname,
		   uint8_t cid_type, const void * cid, int cid_length);
void
DHCPLeaseListRemoveLease(DHCPLeaseListRef list_p,
			 struct in_addr our_ip,
			 struct in_addr router_ip,
			 const uint8_t * router_hwaddr,
			 int router_hwaddr_length);
void
DHCPLeaseListUpdateLease(DHCPLeaseListRef list_p, struct in_addr our_ip,
			 struct in_addr router_ip,
			 const uint8_t * router_hwaddr,
			 int router_hwaddr_length,
			 absolute_time_t lease_start,
			 dhcp_lease_time_t lease_length,
			 const uint8_t * pkt, int pkt_length);
arp_address_info_t *
DHCPLeaseListCopyARPAddressInfo(DHCPLeaseListRef list_p, bool tentative_ok,
				int * ret_count);


void
DHCPLeaseListWrite(DHCPLeaseListRef list_p,
		   const char * ifname,
		   uint8_t cid_type, const void * cid, int cid_length);
void
DHCPLeaseListRead(DHCPLeaseListRef list_p,
		  const char * ifname,
		  uint8_t cid_type, const void * cid, int cid_length);

int
DHCPLeaseListFindLease(DHCPLeaseListRef list_p, struct in_addr our_ip,
		       struct in_addr router_ip,
		       const uint8_t * router_hwaddr, int router_hwaddr_length);

static __inline__ int
DHCPLeaseListCount(DHCPLeaseListRef list_p)
{
    return (dynarray_count(list_p));
}

static __inline__ DHCPLeaseRef
DHCPLeaseListElement(DHCPLeaseListRef list_p, int i)
{
    return (dynarray_element(list_p, i));
}

void
DHCPLeaseListRemoveAllButLastLease(DHCPLeaseListRef list_p);

/*
 * in dhcp.c
 */
void
dhcp_set_default_parameters(uint8_t * params, int n_params);

void
dhcp_set_additional_parameters(uint8_t * params, int n_params);

void
dhcp_get_lease_from_options(dhcpol_t * options, dhcp_lease_time_t * lease, 
			    dhcp_lease_time_t * t1, dhcp_lease_time_t * t2);

bool
dhcp_parameter_is_ok(uint8_t param);

/*
 * more globals
 */
extern bootp_session_t *	G_bootp_session;
extern arp_session_t *		G_arp_session;


#endif _S_IPCONFIGD_THREADS_H
