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

#import <mach/boolean.h>
#import "ipconfig_types.h"
#import "ipconfigd_globals.h"
#import "bootp_session.h"
#import "arp_session.h"
#import "timer.h"
#import "interfaces.h"

#define MAX_RETRIES			2
#define INITIAL_WAIT_SECS		4
#define MAX_WAIT_SECS			60
#define RAND_SECS			1

/*
 * Define: GATHER_TIME_SECS
 * Purpose:
 *   Time to wait for the ideal packet after receiving 
 *   the first acceptable packet.
 */ 
#define GATHER_TIME_SECS		2

/* 
 * Define: LINK_INACTIVE_WAIT_SECS
 * Purpose:
 *   Time to wait after the link goes inactive before unpublishing 
 *   the interface state information
 */
#define LINK_INACTIVE_WAIT_SECS		4


typedef enum {
    IFEventID_start_e = 0,		/* start the configuration method */
    IFEventID_stop_e, 			/* stop/clean-up */
    IFEventID_timeout_e,
    IFEventID_media_e,			/* e.g. link status change */
    IFEventID_data_e,			/* server data to process */
    IFEventID_arp_e,			/* ARP check results */
    IFEventID_change_e,			/* ask config method to change */
    IFEventID_renew_e,			/* ask config method to renew */
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
    };
    if (evid < IFEventID_start_e || evid >= IFEventID_last_e)
	return ("<unknown event>");
    return (names[evid]);
}

typedef struct IFState IFState_t;
typedef ipconfig_status_t (ipconfig_func_t)(IFState_t * ifstate, 
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

struct IFState {
    ipconfig_method_t		method;
    interface_t *		if_p;
    void *			ifname;
    void *			serviceID;
    void *			user_notification;
    void *			user_rls;
    int				our_addrs_start;
    struct completion_results	published;
    link_status_t		link;
    void * 			private;
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

int
inet_enable_autoaddr(IFState_t * ifstate);

int
inet_disable_autoaddr(IFState_t * ifstate);

int
inet_add(IFState_t * ifstate, const struct in_addr ip, 
	 const struct in_addr * mask, const struct in_addr * broadcast);

int
inet_remove(IFState_t * ifstate, struct in_addr ip);

void
ifstate_publish_success(IFState_t * ifstate, void * pkt, int pkt_size);

void
ifstate_publish_failure(IFState_t * ifstate, 
			ipconfig_status_t status, char * msg);

void
ifstate_remove_addresses(IFState_t * ifstate);

void
ifstate_tell_user(IFState_t * ifstate, char * msg);

/* 
 * interface configuration "threads" 
 */
ipconfig_status_t
bootp_thread(IFState_t * ifstate, IFEventID_t evid, void * evdata);

ipconfig_status_t
dhcp_thread(IFState_t * ifstate, IFEventID_t evid, void * evdata);

ipconfig_status_t
manual_thread(IFState_t * ifstate, IFEventID_t evid, void * evdata);

ipconfig_status_t
inform_thread(IFState_t * ifstate, IFEventID_t evid, void * evdata);

/*
 * DHCP lease information
 */
boolean_t
dhcp_lease_read(char * idstr, struct in_addr * ip);

boolean_t
dhcp_lease_write(char * idstr, struct in_addr ip);

void
dhcp_lease_clear(char * idstr);

/*
 * in dhcp.c
 */
inet_addrinfo_t *
interface_is_ad_hoc(interface_t * if_p);

void
dhcp_set_default_parameters(u_char * params, int n_params);

void
dhcp_set_additional_parameters(u_char * params, int n_params);


extern bootp_session_t *	G_bootp_session;
extern arp_session_t *		G_arp_session;

