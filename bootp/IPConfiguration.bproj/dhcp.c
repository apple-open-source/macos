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
 * dhcp.c
 * - DHCP configuration threads
 * - contains dhcp_thread() and inform_thread()
 */
/* 
 * Modification History
 *
 * May 16, 2000		Dieter Siegmund (dieter@apple.com)
 * - reworked to fit within the new event-driven framework
 *
 * October 4, 2000	Dieter Siegmund (dieter@apple.com)
 * - added code to unpublish interface state if the link goes
 *   down and stays down for more than 4 seconds
 * - modified INFORM to process link change events as well
 *
 * February 1, 2002	Dieter Siegmund (dieter@apple.com)
 * - changes for NetBoot
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

#include "rfc_options.h"
#include "dhcp_options.h"
#include "dhcp.h"
#include "interfaces.h"
#include "util.h"
#include <net/if_types.h>
#include "host_identifier.h"
#include "dhcplib.h"

#include "ipconfigd_threads.h"

#include "dprintf.h"

#define SUGGESTED_LEASE_LENGTH		(60 * 60 * 24 * 30 * 3) /* 3 months */
#define DEFAULT_LEASE_LENGTH		(60 * 60)		/* 1 hour */
#define MIN_LEASE_LENGTH		(3) 			/* 3 seconds */

typedef struct {
    boolean_t			valid;
    absolute_time_t		expiration;
    time_interval_t		length;
    absolute_time_t		start;
    absolute_time_t		t1;
    absolute_time_t		t2;
    boolean_t			needs_write;
} lease_info_t;

#define RIFLAGS_IADDR_VALID	(uint32_t)0x1
#define RIFLAGS_HWADDR_VALID	(uint32_t)0x2
#define RIFLAGS_ARP_VERIFIED	(uint32_t)0x4
#define RIFLAGS_DISCOVERING	(uint32_t)0x1000

typedef struct {
    uint32_t			flags;
    int				state;
    struct in_addr		iaddr;
    u_char			hwaddr[MAX_LINK_ADDR_LEN];
} router_info_t;

typedef struct {
    arp_client_t *	arp;
    bootp_client_t *	client;
    void *		client_id;
    int			client_id_len;
    boolean_t		enable_arp_collision_detection;
    boolean_t		gathering;
    boolean_t		in_use;
    lease_info_t	lease;
    boolean_t		must_broadcast;
    struct dhcp * 	request;
    int			request_size;
    router_info_t	router;
    struct saved_pkt	saved;
    dhcp_cstate_t	state;
    absolute_time_t	start_secs;
    timer_callout_t *	timer;
    int			try;
    char		txbuf[DHCP_PACKET_MIN];
    u_long		xid;
    boolean_t		user_warned;
    time_interval_t	wait_secs;
} Service_dhcp_t;

typedef struct {
    arp_client_t *	arp;
    bootp_client_t *	client;
    boolean_t		gathering;
    struct in_addr	our_ip;
    struct in_addr	our_mask;
    struct dhcp * 	request;
    int			request_size;
    struct saved_pkt	saved;
    absolute_time_t	start_secs;
    timer_callout_t *	timer;
    int			try;
    char		txbuf[DHCP_PACKET_MIN];
    u_long		xid;
    time_interval_t	wait_secs;
} Service_inform_t;

static void
dhcp_init(Service_t * service_p, IFEventID_t event_id, void * event_data);

static void
dhcp_init_reboot(Service_t * service_p, IFEventID_t event_id, 
		 void * event_data);

static void
dhcp_arp_router(Service_t * service_p, IFEventID_t event_id, 
		void * event_data);

static void
dhcp_select(Service_t * service_p, IFEventID_t event_id, void * event_data);

static void
dhcp_bound(Service_t * service_p, IFEventID_t event_id, void * event_data);

static void
dhcp_renew_rebind(Service_t * service_p, IFEventID_t event_id, 
		  void * event_data);

static void
dhcp_unbound(Service_t * service_p, IFEventID_t event_id, void * event_data);

static void
dhcp_decline(Service_t * service_p, IFEventID_t event_id, void * event_data);

static void
dhcp_release(Service_t * service_p);

static void
dhcp_no_server(Service_t * service_p, IFEventID_t event_id, void * event_data);

static boolean_t
dhcp_check_lease(Service_t * service_p, absolute_time_t current_time);

static boolean_t
dhcp_update_router_address(Service_t * service_p);


static boolean_t
get_server_identifier(dhcpol_t * options, struct in_addr * server_ip)
{
    struct in_addr * 	ipaddr_p;
    int			len;

    ipaddr_p = (struct in_addr *) 
	dhcpol_find(options, dhcptag_server_identifier_e, &len, NULL);
    if (ipaddr_p != NULL && len >= 4) {
	*server_ip = *ipaddr_p;
    }
    return (ipaddr_p != NULL);
}

static void *
find_option_with_length(dhcpol_t * options, dhcptag_t tag, int min_length)
{
    int		real_length;
    void * 	val;

    val = dhcpol_find(options, tag, &real_length, NULL);
    if (val == NULL) {
	return (NULL);
    }
    if (real_length < min_length) {
	return (NULL);
    }
    return (val);
}

static void
get_lease(dhcpol_t * options, time_interval_t * lease, 
	  time_interval_t * t1, time_interval_t * t2)
{
    time_interval_t *	lease_opt;
    time_interval_t *	t1_opt;
    time_interval_t *	t2_opt;

    lease_opt = (time_interval_t *)
	find_option_with_length(options, dhcptag_lease_time_e, 4);
    t1_opt = (time_interval_t *)
	find_option_with_length(options, dhcptag_renewal_t1_time_value_e, 4);
    t2_opt = (time_interval_t *)
	find_option_with_length(options, dhcptag_rebinding_t2_time_value_e, 4);
    if (lease_opt != NULL) {
	*lease = ntohl(*lease_opt);
	if (*lease < MIN_LEASE_LENGTH) {
	    *lease = MIN_LEASE_LENGTH;
	}
    }
    if (t1_opt != NULL) {
	*t1 = ntohl(*t1_opt);
    }
    if (t2_opt != NULL) {
	*t2 = ntohl(*t2_opt);
    }
    if (lease_opt == NULL) {
	if (t1_opt != NULL) {
	    *lease = *t1;
	}
	else if (t2_opt != NULL) {
	    *lease = *t2;
	}
	else {
	    *lease = DEFAULT_LEASE_LENGTH;
	}
    }
    if (*lease == DHCP_INFINITE_LEASE) {
	*t1 = *t2 = 0;
    }
    else if (t1_opt == NULL || *t1 >= *lease
	     || t2_opt == NULL || *t2 >= *lease
	     || *t2 < *t1) {
	*t1 = (*lease) / 2;
	*t2 = (time_interval_t) ((double)(*lease) * 0.875);
    }
    return;
}

static const u_char dhcp_static_default_params[] = {
    dhcptag_subnet_mask_e, 
    dhcptag_router_e,
    dhcptag_domain_name_server_e,
    dhcptag_domain_name_e,
    dhcptag_netinfo_server_address_e,
    dhcptag_netinfo_server_tag_e,
    dhcptag_slp_directory_agent_e,
    dhcptag_slp_service_scope_e,
    dhcptag_ldap_url_e,
    dhcptag_proxy_auto_discovery_url_e,
};
#define	N_DHCP_STATIC_DEFAULT_PARAMS 	(sizeof(dhcp_static_default_params) / sizeof(dhcp_static_default_params[0]))

static u_char * dhcp_default_params = (u_char *)dhcp_static_default_params;
static int	n_dhcp_default_params = N_DHCP_STATIC_DEFAULT_PARAMS;

static u_char * dhcp_params = (u_char *)dhcp_static_default_params;
static int	n_dhcp_params = N_DHCP_STATIC_DEFAULT_PARAMS;

void
dhcp_set_default_parameters(u_char * params, int n_params)
{
    static boolean_t	done = FALSE;

    if (done) {
	return;
    }
    done = TRUE;
    if (params && n_params) {
	dhcp_default_params = params;
	n_dhcp_default_params = n_params;
    }
    else {
	dhcp_default_params = (u_char *)dhcp_static_default_params;
	n_dhcp_default_params = N_DHCP_STATIC_DEFAULT_PARAMS;
    }
    dhcp_params = dhcp_default_params;
    n_dhcp_params = n_dhcp_default_params;
    return;
}

static u_char *
S_merge_parameters(u_char * params, int n_params, int * n_ret)
{
    int		i;
    u_char *	ret = dhcp_default_params;
    u_char *	new = NULL;
    int		new_end = 0;

    *n_ret = n_dhcp_default_params;
    if (params == NULL || n_params == 0) {
	goto done;
    }
    /* allocate the worst case size ie. no duplicates */
    new = (u_char *)malloc(n_dhcp_default_params + n_params);
    if (new == NULL) {
	goto done;
    }
    bcopy(dhcp_default_params, new, n_dhcp_default_params);
    for (i = 0, new_end = n_dhcp_default_params; i < n_params; i++) {
	boolean_t	already_there = FALSE;
	int 		j;

	for (j = 0; j < new_end; j++) {
	    if (new[j] == params[i]) {
		/* already in requested parameters list, ignore it */
		already_there = TRUE;
		break;
	    }
	}
	if (already_there == FALSE) {
	    new[new_end++] = params[i];
	}
    }
    if (new_end > n_dhcp_default_params) {
	ret = new;
	*n_ret = new_end;
    }
    else {
	free(new);
	new = NULL;
    }
 done:
    return (ret);
}

static void
S_print_char_array(u_char * params, int n_params)
{
    int i;

    for (i = 0; i < n_params; i++) {
	if (i == 0)
	    printf("%d", params[i]);
	else
	    printf(", %d", params[i]);
    }
    return;
}

void
dhcp_set_additional_parameters(u_char * params, int n_params)
{
    if (dhcp_params && dhcp_params != dhcp_default_params) {
	free(dhcp_params);
    }
    dhcp_params = S_merge_parameters(params, n_params, &n_dhcp_params);
    if (params) {
	free(params);
    }
    if (G_debug) {
	printf("DHCP requested parameters = {");
	S_print_char_array(dhcp_params, n_dhcp_params);
	printf("}\n");
    }
}

static void
add_computer_name(dhcpoa_t * options_p)
{
    /* add the computer name as the host_name option */
    char *	name = computer_name();

    if (name) {
	if (dhcpoa_add(options_p, dhcptag_host_name_e, strlen(name), name)
	    != dhcpoa_success_e) {
	    my_log(LOG_INFO, "make_dhcp_request: couldn't add host_name, %s",
		   dhcpoa_err(options_p));
	}
    }
    return;
}

static struct dhcp * 
make_dhcp_request(struct dhcp * request, int pkt_size,
		  dhcp_msgtype_t msg, 
		  u_char * hwaddr, u_char hwtype, u_char hwlen, 
		  void * cid, int cid_len, boolean_t must_broadcast,
		  dhcpoa_t * options_p)
{
    char * 	buf = NULL;
    u_char 	cid_type = 0;

    /* if no client id was specified, use the hardware address */
    if (cid == NULL || cid_len == 0) {
	cid = hwaddr;
	cid_len = hwlen;
	cid_type = hwtype;
    }

    bzero(request, pkt_size);
    request->dp_htype = hwtype;
    request->dp_op = BOOTREQUEST;

    switch (hwtype) {
    default:
    case ARPHRD_ETHER:
	request->dp_hlen = hwlen;
	bcopy(hwaddr, request->dp_chaddr, hwlen);
	break;
    case ARPHRD_IEEE1394:
	request->dp_hlen = 0; /* RFC 2855 */
	if (cid == hwaddr) {
	    /* if client id is the hardware address, set the right type */
	    cid_type = ARPHRD_IEEE1394_EUI64;
	}
	break;
    }
    if (must_broadcast || G_must_broadcast) {
	request->dp_flags = htons(DHCP_FLAGS_BROADCAST);
    }
    bcopy(G_rfc_magic, request->dp_options, sizeof(G_rfc_magic));
    dhcpoa_init(options_p, request->dp_options + sizeof(G_rfc_magic),
		pkt_size - sizeof(struct dhcp) - sizeof(G_rfc_magic));
    
    /* make the request a dhcp message */
    if (dhcpoa_add_dhcpmsg(options_p, msg) != dhcpoa_success_e) {
	my_log(LOG_ERR,
	       "make_dhcp_request: couldn't add dhcp message tag %d, %s", msg,
	       dhcpoa_err(options_p));
	goto err;
    }

    if (msg != dhcp_msgtype_decline_e && msg != dhcp_msgtype_release_e) {
	u_int16_t	max_message_size = htons(1500); /* max receive size */

	/* add the list of required parameters */
	if (dhcpoa_add(options_p, dhcptag_parameter_request_list_e,
			n_dhcp_params, dhcp_params)
	    != dhcpoa_success_e) {
	    my_log(LOG_ERR, "make_dhcp_request: "
		   "couldn't add parameter request list, %s",
		   dhcpoa_err(options_p));
	    goto err;
	}
	/* add the max message size */
	if (dhcpoa_add(options_p, dhcptag_max_dhcp_message_size_e,
		       sizeof(max_message_size), &max_message_size)
	    != dhcpoa_success_e) {
	    my_log(LOG_ERR, "make_dhcp_request: "
		    "couldn't add max message size, %s",
		    dhcpoa_err(options_p));
	    goto err;
	}
    }

    /* add the client identifier to the request packet */
    buf = malloc(cid_len + 1);
    if (buf == NULL) {
	my_log(LOG_ERR, "make_dhcp_request: malloc failed, %s (%d)",
	       strerror(errno), errno);
	goto err;
    }
    *buf = cid_type;
    bcopy(cid, buf + 1, cid_len);
    if (dhcpoa_add(options_p, dhcptag_client_identifier_e, cid_len + 1, buf)
	!= dhcpoa_success_e) {
	free(buf);
	my_log(LOG_ERR, "make_dhcp_request: "
	       "couldn't add client identifier, %s",
	       dhcpoa_err(options_p));
	goto err;
    }
    free(buf);
    return (request);
  err:
    return (NULL);
}

/*
 * Function: verify_packet
 * Purpose:
 */
static boolean_t
verify_packet(bootp_receive_data_t * pkt, u_long xid, interface_t * if_p, 
	      dhcp_msgtype_t * msgtype_p, struct in_addr * server_ip,
	      boolean_t * is_dhcp)
{
    if (dhcp_packet_match((struct bootp *)pkt->data, xid, 
			  (u_char) if_link_arptype(if_p),
			  if_link_address(if_p),
			  if_link_length(if_p))) {
	/* 
	 * A BOOTP packet should be one that doesn't contain
	 * a dhcp message.  Unfortunately, some stupid BOOTP servers
	 * are unaware of DHCP and RFC-standard options, and simply 
         * echo back what we sent in the options area.  This is the 
	 * reason for checking for DISCOVER, REQUEST and INFORM: they are
	 * invalid responses in the DHCP protocol, so we assume that 
	 * the server is blindly echoing what we send.
	 */
	if (is_dhcp_packet(&pkt->options, msgtype_p) == FALSE
	    || *msgtype_p == dhcp_msgtype_discover_e
	    || *msgtype_p == dhcp_msgtype_request_e
	    || *msgtype_p == dhcp_msgtype_inform_e) {
	    /* BOOTP packet */
	    if (G_dhcp_accepts_bootp == FALSE) {
		return (FALSE);
	    }
	    *msgtype_p = dhcp_msgtype_none_e;
	    *is_dhcp = FALSE;
	    *server_ip = pkt->data->dp_siaddr;
	    return (TRUE);
	}
	*is_dhcp = TRUE;
	server_ip->s_addr = 0;
	(void)get_server_identifier(&pkt->options, server_ip);
	/* matching DHCP packet */
	return (TRUE);
    }
    return (FALSE);
}


/**
 **
 ** INFORM Functions
 ** 
 */

static void
inform_cancel_pending_events(Service_t * service_p)
{
    Service_inform_t *	inform = (Service_inform_t *)service_p->private;

    if (inform == NULL)
	return;
    if (inform->timer) {
	timer_cancel(inform->timer);
    }
    if (inform->client) {
	bootp_client_disable_receive(inform->client);
    }
    if (inform->arp) {
	arp_client_cancel_probe(inform->arp);
    }
    return;
}

static void
inform_inactive(Service_t * service_p)
{
    Service_inform_t *	inform = (Service_inform_t *)service_p->private;

    inform_cancel_pending_events(service_p);
    service_remove_address(service_p);
    dhcpol_free(&inform->saved.options);
    service_publish_failure(service_p, ipconfig_status_media_inactive_e,
			    NULL);
    return;
}

static void
inform_link_timer(void * arg0, void * arg1, void * arg2)
{
    inform_inactive((Service_t *) arg0);
    return;
}

static void
inform_failed(Service_t * service_p, ipconfig_status_t status, char * msg)
{
    inform_cancel_pending_events(service_p);
    service_publish_failure(service_p, status, msg);
    return;
}

static void
inform_success(Service_t * service_p)
{
    Service_inform_t *	inform = (Service_inform_t *)service_p->private;
    int 		len;
    void *		option;
	
    option = dhcpol_find(&inform->saved.options, dhcptag_subnet_mask_e,
			 &len, NULL);
    if (option != NULL && len >= 4) {
	inform->our_mask = *((struct in_addr *)option);
	
	/* reset the interface address with the new mask */
	(void)service_set_address(service_p, inform->our_ip, 
				  inform->our_mask, G_ip_zeroes);
    }
    inform_cancel_pending_events(service_p);

    service_publish_success(service_p, inform->saved.pkt, 
			    inform->saved.pkt_size);
    return;
}

static void
inform_request(Service_t * service_p, IFEventID_t event_id, void * event_data)
{
    absolute_time_t	current_time = timer_current_secs();
    interface_t *	if_p = service_interface(service_p);
    Service_inform_t *	inform = (Service_inform_t *)service_p->private;
    ipconfig_status_t	status = ipconfig_status_success_e;

    switch (event_id) {
      case IFEventID_start_e: {
	  dhcpoa_t		options;
	  
	  inform->start_secs = current_time;

	  /* clean-up anything that might have come before */
	  inform_cancel_pending_events(service_p);
	  inform->request = make_dhcp_request((struct dhcp *)inform->txbuf, 
					      sizeof(inform->txbuf),
					      dhcp_msgtype_inform_e,
					      if_link_address(if_p), 
					      if_link_arptype(if_p),
					      if_link_length(if_p), 
					      NULL, 0,
					      FALSE,
					      &options);
	  if (inform->request == NULL) {
	      my_log(LOG_ERR, "INFORM %s: make_dhcp_request failed",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  inform->request->dp_ciaddr = inform->our_ip;
	  add_computer_name(&options);
	  if (dhcpoa_add(&options, dhcptag_end_e, 0, 0)
	      != dhcpoa_success_e) {
	      my_log(LOG_ERR, "INFORM %s: failed to terminate options",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  inform->request_size = sizeof(*inform->request) + sizeof(G_rfc_magic) 
	      + dhcpoa_used(&options);
	  if (inform->request_size < sizeof(struct bootp)) {
	      /* pad out to BOOTP-sized packet */
	      inform->request_size = sizeof(struct bootp);
	  }
	  inform->try = 0;
	  inform->gathering = FALSE;
	  inform->wait_secs = G_initial_wait_secs;
	  bootp_client_enable_receive(inform->client,
				      (bootp_receive_func_t *)inform_request,
				      service_p, (void *)IFEventID_data_e);
	  inform->saved.rating = 0;
	  inform->xid++;
#if 0
	  dhcpol_free(&inform->saved.options);
	  bzero(&inform->saved, sizeof(inform->saved));
#endif 0

	  /* FALL THROUGH */
      }
      case IFEventID_timeout_e: {
	  struct timeval 	tv;
	  if (inform->gathering == TRUE) {
	      /* done gathering */
	      inform_success(service_p);
	      return;
	  }
	  inform->try++;
	  if (inform->try > 1) {
	      if (service_link_status(service_p)->valid 
		  && service_link_status(service_p)->active == FALSE) {
		  inform_inactive(service_p);
		  break; /* out of switch */
	      }
	  }
	  if (inform->try > (G_max_retries + 1)) {
	      inform_success(service_p);
	      break;
	  }
	  inform->request->dp_xid = htonl(inform->xid);
	  inform->request->dp_secs 
	      = htons((u_short)(current_time - inform->start_secs));

	  /* send the packet */
	  if (bootp_client_transmit(inform->client,
				    G_ip_broadcast, G_ip_zeroes,
				    G_server_port, G_client_port,
				    inform->request, 
				    inform->request_size) < 0) {
	      my_log(LOG_ERR, 
		     "INFORM %s: transmit failed", if_name(if_p));
	  }
	  /* wait for responses */
	  tv.tv_sec = inform->wait_secs;
	  tv.tv_usec = random_range(0, USECS_PER_SEC - 1);
	  my_log(LOG_DEBUG, "INFORM %s: waiting at %d for %d.%06d", 
		 if_name(if_p), 
		 current_time - inform->start_secs,
		 tv.tv_sec, tv.tv_usec);
	  timer_set_relative(inform->timer, tv, 
			     (timer_func_t *)inform_request,
			     service_p, (void *)IFEventID_timeout_e, NULL);
	  /* next time wait twice as long */
	  inform->wait_secs *= 2;
	  if (inform->wait_secs > G_max_wait_secs) {
	      inform->wait_secs = G_max_wait_secs;
	  }
	  break;
      }
      case IFEventID_data_e: {
	  boolean_t 		is_dhcp = TRUE;
	  bootp_receive_data_t *pkt = (bootp_receive_data_t *)event_data;
	  dhcp_msgtype_t	reply_msgtype = dhcp_msgtype_none_e;
	  struct in_addr	server_ip;

	  if (verify_packet(pkt, inform->xid, if_p, &reply_msgtype,
			    &server_ip, &is_dhcp) == FALSE) {
	      /* reject the packet */
	      break; /* out of switch */
	  }
	  if (is_dhcp == FALSE
	      || (reply_msgtype == dhcp_msgtype_ack_e)) {
	      int rating = 0;
	      
	      rating = count_params(&pkt->options, dhcp_params, n_dhcp_params);
	      /* 
	       * The new packet is "better" than the saved
	       * packet if:
	       * - there was no saved packet, or
	       * - the new packet is a DHCP packet and the saved
	       *   one is a BOOTP packet or a DHCP packet with
	       *   a lower rating, or
	       * - the new packet and the saved packet are both
	       *   BOOTP but the new one has a higher rating
	       * All this to allow BOOTP/DHCP interoperability
	       * ie. we accept a BOOTP response if it's
	       * the only one we've got.  We expect/favour a DHCP 
	       * response.
	       */
	      if (inform->saved.pkt_size == 0
		  || (is_dhcp == TRUE && (inform->saved.is_dhcp == FALSE 
					  || rating > inform->saved.rating))
		  || (is_dhcp == FALSE && inform->saved.is_dhcp == FALSE
		      && rating > inform->saved.rating)) {
		  dhcpol_free(&inform->saved.options);
		  bcopy(pkt->data, inform->saved.pkt, pkt->size);
		  inform->saved.pkt_size = pkt->size;
		  inform->saved.rating = rating;
		  (void)dhcpol_parse_packet(&inform->saved.options, 
					    (void *)inform->saved.pkt, 
					    inform->saved.pkt_size, NULL);
		  inform->saved.our_ip = pkt->data->dp_yiaddr;
		  inform->saved.server_ip = server_ip;
		  inform->saved.is_dhcp = is_dhcp;
		  if (is_dhcp && rating == n_dhcp_params) {
		      inform_success(service_p);
		      return;
		  }
		  if (inform->gathering == FALSE) {
		      struct timeval t = {0,0};
		      t.tv_sec = G_gather_secs;
		      my_log(LOG_DEBUG, "INFORM %s: gathering began at %d", 
			     if_name(if_p), 
			     current_time - inform->start_secs);
		      inform->gathering = TRUE;
		      timer_set_relative(inform->timer, t, 
					 (timer_func_t *)inform_request,
					 service_p, (void *)IFEventID_timeout_e,
					 NULL);
		  }
	      }
	  }
	  break;
      }
      default:
	  break;
    }
    return;
 error:
    inform_failed(service_p, status, NULL);
    return;

}

static void
inform_start(Service_t * service_p, IFEventID_t event_id, void * event_data)
{
    interface_t *	if_p = service_interface(service_p);
    Service_inform_t *	inform = (Service_inform_t *)service_p->private;

    switch (event_id) {
      case IFEventID_start_e: {
	  inform_cancel_pending_events(service_p);

	  arp_client_probe(inform->arp, 
			   (arp_result_func_t *)inform_start, service_p,
			   (void *)IFEventID_arp_e, G_ip_zeroes,
			   inform->our_ip);
	  break;
      }
      case IFEventID_arp_e: {
	  arp_result_t *	result = (arp_result_t *)event_data;

	  if (result->error) {
	      my_log(LOG_DEBUG, "INFORM %s: arp probe failed, %s", 
		     if_name(if_p),
		     arp_client_errmsg(inform->arp));
	      inform_failed(service_p, ipconfig_status_internal_error_e, NULL);
	      break;
	  }
	  else {
	      if (result->in_use) {
		  char	msg[128];

		  snprintf(msg, sizeof(msg),
			   IP_FORMAT " in use by " EA_FORMAT,
			   IP_LIST(&inform->our_ip), EA_LIST(result->hwaddr));
		  service_report_conflict(service_p,
					  &inform->our_ip,
					  result->hwaddr,
					  NULL);
		  my_log(LOG_ERR, "INFORM %s: %s", if_name(if_p), 
			 msg);
		  (void)service_remove_address(service_p);
		  inform_failed(service_p, ipconfig_status_address_in_use_e,
				msg);
		  break;
	      }
	  }
	  if (service_link_status(service_p)->valid == TRUE 
	      && service_link_status(service_p)->active == FALSE) {
	      inform_inactive(service_p);
	      break;
	  }

	  /* set the primary address */
	  (void)service_set_address(service_p, inform->our_ip, 
				    inform->our_mask, G_ip_zeroes);
	  inform_request(service_p, IFEventID_start_e, 0);
	  break;
      }
      default: {
	  break;
      }
    }
    return;
}

ipconfig_status_t
inform_thread(Service_t * service_p, IFEventID_t event_id, void * event_data)
{
    interface_t *	if_p = service_interface(service_p);
    Service_inform_t *	inform = (Service_inform_t *)service_p->private;
    ipconfig_status_t	status = ipconfig_status_success_e;

    switch (event_id) {
      case IFEventID_start_e: {
	  start_event_data_t *    evdata = ((start_event_data_t *)event_data);
	  ipconfig_method_data_t *ipcfg = evdata->config.data;

	  if (if_flags(if_p) & IFF_LOOPBACK) {
	      status = ipconfig_status_invalid_operation_e;
	      break;
	  }
	  if (inform) {
	      my_log(LOG_ERR, "INFORM %s: re-entering start state", 
		     if_name(if_p));
	      status = ipconfig_status_internal_error_e;
	      break;
	  }
	  status = validate_method_data_addresses(&evdata->config,
						  ipconfig_method_inform_e,
						  if_name(if_p));
	  if (status != ipconfig_status_success_e)
	      break;

	  inform = malloc(sizeof(*inform));
	  if (inform == NULL) {
	      my_log(LOG_ERR, "INFORM %s: malloc failed", 
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      break;
	  }
	  service_p->private = inform;
	  bzero(inform, sizeof(*inform));
	  dhcpol_init(&inform->saved.options);
	  inform->our_ip = ipcfg->ip[0].addr;
	  inform->our_mask = ipcfg->ip[0].mask;
	  inform->timer = timer_callout_init();
	  if (inform->timer == NULL) {
	      my_log(LOG_ERR, "INFORM %s: timer_callout_init failed", 
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  inform->client = bootp_client_init(G_bootp_session, if_p);
	  if (inform->client == NULL) {
	      my_log(LOG_ERR, "INFORM %s: bootp_client_init failed",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  inform->arp = arp_client_init(G_arp_session, if_p);
	  if (inform->arp == NULL) {
	      my_log(LOG_ERR, "INFORM %s: arp_client_init failed", 
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  my_log(LOG_DEBUG, "INFORM %s: start", if_name(if_p));
	  inform->xid = random();
	  inform_start(service_p, IFEventID_start_e, NULL);
	  break;
      }
      case IFEventID_stop_e: {
      stop:
	  my_log(LOG_DEBUG, "INFORM %s: stop", if_name(if_p));
	  if (inform == NULL) { /* already stopped */
	      break;
	  }
	  /* remove IP address */
	  service_remove_address(service_p);

	  /* clean-up resources */
	  if (inform->timer) {
	      timer_callout_free(&inform->timer);
	  }
	  if (inform->client) {
	      bootp_client_free(&inform->client);
	  }
	  if (inform->arp) {
	      arp_client_free(&inform->arp);
	  }
	  dhcpol_free(&inform->saved.options);
	  if (inform)
	      free(inform);
	  service_p->private = NULL;
	  break;
      }
      case IFEventID_change_e: {
	  change_event_data_t *   evdata = ((change_event_data_t *)event_data);
	  ipconfig_method_data_t *ipcfg = evdata->config.data;
	  ipconfig_status_t	  status;	

	  if (inform == NULL) {
	      my_log(LOG_DEBUG, "INFORM %s: private data is NULL", 
		     if_name(if_p));
	      return (ipconfig_status_internal_error_e);
	  }
	  status = validate_method_data_addresses(&evdata->config,
						  ipconfig_method_inform_e,
						  if_name(if_p));
	  if (status != ipconfig_status_success_e)
	      return (status);
	  evdata->needs_stop = FALSE;
	  if (ipcfg->ip[0].addr.s_addr != inform->our_ip.s_addr) {
	      evdata->needs_stop = TRUE;
	  }
	  else if (ipcfg->ip[0].mask.s_addr != 0
		   && ipcfg->ip[0].mask.s_addr != inform->our_mask.s_addr) {
	      inform->our_mask = ipcfg->ip[0].mask;
	      (void)service_set_address(service_p, inform->our_ip,
					inform->our_mask, G_ip_zeroes);
	  }
	  return (ipconfig_status_success_e);
      }
      case IFEventID_arp_collision_e: {
	  arp_collision_data_t *	arpc;
	  char				msg[128];

	  arpc = (arp_collision_data_t *)event_data;
	  if (inform == NULL) {
	      return (ipconfig_status_internal_error_e);
	  }
	  if (arpc->ip_addr.s_addr != inform->our_ip.s_addr) {
	      break;
	  }
	  snprintf(msg, sizeof(msg), 
		   IP_FORMAT " in use by " EA_FORMAT,
		   IP_LIST(&arpc->ip_addr), 
		   EA_LIST(arpc->hwaddr));
	  service_report_conflict(service_p,
				  &arpc->ip_addr,
				  arpc->hwaddr,
				  NULL);
	  my_log(LOG_ERR, "INFORM %s: %s", if_name(if_p), 
		 msg);
#if 0
	  service_remove_address(service_p);
	  inform_failed(service_p, ipconfig_status_address_in_use_e,
			msg);
#endif 0
	  break;
      }
      case IFEventID_renew_e:
      case IFEventID_media_e: {
	  if (inform == NULL)
	      return (ipconfig_status_internal_error_e);

	  if (service_link_status(service_p)->valid == TRUE) {
	      if (service_link_status(service_p)->active == TRUE) {
		  inform_start(service_p, IFEventID_start_e, 0);
	      }
	      else if (event_id == IFEventID_media_e) {
		  struct timeval tv;

		  /* if link goes down and stays down long enough, unpublish */
		  inform_cancel_pending_events(service_p);
		  tv.tv_sec = G_link_inactive_secs;
		  tv.tv_usec = 0;
		  timer_set_relative(inform->timer, tv, 
				     (timer_func_t *)inform_link_timer,
				     service_p, NULL, NULL);
	      }
	  }
	  break;
      }
      default:
	  break;
    } /* switch (event_id) */
    return (status);
}

/**
 **
 ** DHCP Functions
 ** 
 */

static void
dhcp_cancel_pending_events(Service_t * service_p)
{
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)service_p->private;

    if (dhcp == NULL)
	return;
    if (dhcp->timer) {
	timer_cancel(dhcp->timer);
    }
    if (dhcp->client) {
	bootp_client_disable_receive(dhcp->client);
    }
    if (dhcp->arp) {
	arp_client_cancel_probe(dhcp->arp);
    }
    return;
}


static void
dhcp_failed(Service_t * service_p, ipconfig_status_t status, char * msg,
	    boolean_t sync)
{
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)service_p->private;

    dhcp_cancel_pending_events(service_p);

    service_disable_autoaddr(service_p);
    dhcpol_free(&dhcp->saved.options);
    service_remove_address(service_p);
#if 0
    dhcp->saved.our_ip.s_addr = 0;
    dhcp->lease.valid = FALSE;
    dhcp->router.flags = 0;
#endif 0
    service_publish_failure_sync(service_p, status, msg, sync);
    dhcp->state = dhcp_cstate_none_e;
    return;
}

static void
dhcp_inactive(Service_t * service_p)
{
    Service_dhcp_t * 	dhcp = (Service_dhcp_t *)service_p->private;

    dhcp_cancel_pending_events(service_p);
    /*
     * Set the status here so that the link-local service will disappear 
     * when we call service_remove_address.
     */
    service_p->published.status = ipconfig_status_media_inactive_e;
    service_remove_address(service_p);
    service_disable_autoaddr(service_p);
    service_publish_failure(service_p, ipconfig_status_media_inactive_e, NULL);
    dhcp->state = dhcp_cstate_none_e;
    return;
}

static void
dhcp_set_lease_params(Service_t * service_p, char * descr, boolean_t is_dhcp,
		      absolute_time_t lease_start,
		      time_interval_t lease, time_interval_t t1,
		      time_interval_t t2)
{
    Service_dhcp_t * dhcp = (Service_dhcp_t *)service_p->private;
    interface_t *    if_p = service_interface(service_p);

    dhcp->lease.start = lease_start;
    if (is_dhcp == FALSE) {
	dhcp->lease.length = DHCP_INFINITE_LEASE;
    }
    else {
	dhcp->lease.length = lease;
    }

    if (dhcp->lease.length == DHCP_INFINITE_LEASE) {
	dhcp->lease.t1 = dhcp->lease.t2 = dhcp->lease.expiration = 0;
    }
    else {
	dhcp->lease.expiration = dhcp->lease.start + dhcp->lease.length;
	dhcp->lease.t1 = dhcp->lease.start + t1;
	dhcp->lease.t2 = dhcp->lease.start + t2;
    }
    my_log(LOG_DEBUG, 
	   "DHCP %s: %s lease"
	   " start = 0x%x, t1 = 0x%x , t2 = 0x%x, expiration 0x%x", 
	   if_name(if_p), descr, dhcp->lease.start, 
	   dhcp->lease.t1, dhcp->lease.t2, dhcp->lease.expiration);
    return;
}

static void
dhcp_link_timer(void * arg0, void * arg1, void * arg2)
{
    dhcp_inactive((Service_t *)arg0);
    return;
}

static void
get_client_id(Service_dhcp_t * dhcp, interface_t * if_p, const void * * cid_p, 
	      u_char * cid_type_p, int * cid_length_p)
{
    if (dhcp->client_id != NULL) {
	*cid_type_p = 0;
	*cid_p = dhcp->client_id;
	*cid_length_p = dhcp->client_id_len;
    }
    else {
	*cid_type_p = if_link_arptype(if_p);
	*cid_p = if_link_address(if_p);
	*cid_length_p = if_link_length(if_p);
    }
    return;
}

static boolean_t
_dhcp_lease_read(Service_t * service_p, absolute_time_t * lease_start,
		 char * pkt, int * pkt_size, char * router_hwaddr,
		 int * router_hwaddr_size)
{
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)service_p->private;
    interface_t *	if_p = service_interface(service_p);
    const void *	cid;
    u_char 		cid_type;
    int 		cid_length;

    get_client_id(dhcp, if_p, &cid, &cid_type, &cid_length);
    return (dhcp_lease_read(if_name(if_p), cid_type, cid, cid_length,
			    lease_start, pkt, pkt_size, router_hwaddr,
			    router_hwaddr_size));
}

static boolean_t
_dhcp_lease_write(Service_t * service_p, absolute_time_t lease_start,
		  const char * pkt, int pkt_size, const char * router_hwaddr,
		  int router_hwaddr_size)
{
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)service_p->private;
    interface_t *	if_p = service_interface(service_p);
    const void *	cid;
    u_char 		cid_type;
    int 		cid_length;

    get_client_id(dhcp, if_p, &cid, &cid_type, &cid_length);
    return (dhcp_lease_write(if_name(if_p), cid_type, cid, cid_length,
			     lease_start, pkt, pkt_size, router_hwaddr,
			     router_hwaddr_size));
}

static void
_dhcp_lease_clear(Service_t * service_p)
{
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)service_p->private;
    interface_t *	if_p = service_interface(service_p);
    const void *	cid;
    u_char 		cid_type;
    int 		cid_length;

    get_client_id(dhcp, if_p, &cid, &cid_type, &cid_length);
    dhcp_lease_clear(if_name(if_p), cid_type, cid, cid_length);
    return;
}

static boolean_t
recover_lease(Service_t * service_p, struct in_addr * our_ip)
{
    absolute_time_t 	current_time = timer_current_secs();
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)service_p->private;
    interface_t *	if_p = service_interface(service_p);
    time_interval_t	lease;
    absolute_time_t	lease_start;
    int			pkt_size = sizeof(dhcp->saved.pkt);
    int			router_hwaddr_size = sizeof(dhcp->router.hwaddr);
    time_interval_t	t1;
    time_interval_t	t2;

    if (_dhcp_lease_read(service_p, &lease_start, dhcp->saved.pkt, &pkt_size,
			 dhcp->router.hwaddr, &router_hwaddr_size) == FALSE) {
	goto failed;
    }
    dhcp->saved.pkt_size = pkt_size;
    (void)dhcpol_parse_packet(&dhcp->saved.options, 
			      (void *)dhcp->saved.pkt, 
			      dhcp->saved.pkt_size, NULL);
    get_lease(&dhcp->saved.options, &lease, &t1, &t2);
    dhcp_set_lease_params(service_p, "RECOVERED", 
			  TRUE, lease_start, lease, t1, t2);
    if (dhcp->lease.length != DHCP_INFINITE_LEASE
	&& current_time >= dhcp->lease.expiration) {
	my_log(LOG_DEBUG, 
	       "DHCP %s: lease is expired: current 0x%x >= expire 0x%x",
	       if_name(if_p), current_time, dhcp->lease.expiration);
	goto failed;
    }
    dhcp->lease.valid = TRUE;
    dhcp->saved.rating = 0;
    dhcp->saved.our_ip = ((struct dhcp *)dhcp->saved.pkt)->dp_yiaddr;
    get_server_identifier(&dhcp->saved.options, &dhcp->saved.server_ip);
    dhcp->saved.is_dhcp = TRUE;
    *our_ip = dhcp->saved.our_ip;
    (void)dhcp_update_router_address(service_p);
    if (router_hwaddr_size > 0) {
	dhcp->router.flags |= RIFLAGS_HWADDR_VALID;
    }
    my_log(LOG_DEBUG, "DHCP %s: recovered lease for IP " IP_FORMAT, 
	   if_name(if_p), IP_LIST(our_ip));
    return (TRUE);

 failed:
    return (FALSE);
}

static boolean_t
dhcp_check_lease(Service_t * service_p, absolute_time_t current_time)
{
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)service_p->private;

    if (dhcp->lease.valid) {
	if (dhcp->lease.length != DHCP_INFINITE_LEASE
	    && (current_time + 16) >= dhcp->lease.expiration) {
	    dhcp->lease.valid = FALSE;
	    dhcp->router.flags = 0;
	    (void)service_remove_address(service_p);
	}
    }
    return (dhcp->lease.valid);
}

ipconfig_status_t
dhcp_thread(Service_t * service_p, IFEventID_t event_id, void * event_data)
{
    absolute_time_t 	current_time = timer_current_secs();
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)service_p->private;
    interface_t *	if_p = service_interface(service_p);
    ipconfig_status_t	status = ipconfig_status_success_e;

    switch (event_id) {
      case IFEventID_start_e: {
	  start_event_data_t *   	evdata = ((start_event_data_t *)event_data);
	  ipconfig_method_data_t*	ipcfg = evdata->config.data;
	  struct in_addr		our_ip;

	  if (if_flags(if_p) & IFF_LOOPBACK) {
	      status = ipconfig_status_invalid_operation_e;
	      break;
	  }
	  if (dhcp) {
	      my_log(LOG_ERR, "DHCP %s: re-entering start state", 
		     if_name(if_p));
	      status = ipconfig_status_internal_error_e;
	      break;
	  }

	  dhcp = malloc(sizeof(*dhcp));
	  if (dhcp == NULL) {
	      my_log(LOG_ERR, "DHCP %s: malloc failed", 
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      break;
	  }
	  bzero(dhcp, sizeof(*dhcp));
	  dhcp->must_broadcast = (if_link_arptype(if_p) == ARPHRD_IEEE1394);
	  dhcpol_init(&dhcp->saved.options);
	  service_p->private = dhcp;

	  dhcp->lease.valid = FALSE;
	  dhcp->router.flags = 0;
	  dhcp->state = dhcp_cstate_none_e;
	  dhcp->timer = timer_callout_init();
	  if (dhcp->timer == NULL) {
	      my_log(LOG_ERR, "DHCP %s: timer_callout_init failed", 
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  (void)service_enable_autoaddr(service_p);
	  dhcp->client = bootp_client_init(G_bootp_session, if_p);
	  if (dhcp->client == NULL) {
	      my_log(LOG_ERR, "DHCP %s: bootp_client_init failed",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  dhcp->arp = arp_client_init(G_arp_session, if_p);
	  if (dhcp->arp == NULL) {
	      my_log(LOG_ERR, "DHCP %s: arp_client_init failed", 
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  if (ipcfg->n_dhcp_client_id) {
	      void * 		cid;

	      dhcp->client_id_len = ipcfg->n_dhcp_client_id;
	      dhcp->client_id = malloc(dhcp->client_id_len);
	      if (dhcp->client_id == NULL) {
		  my_log(LOG_ERR, "DHCP %s: malloc client ID failed", 
			 if_name(if_p));
		  status = ipconfig_status_allocation_failed_e;
		  goto stop;
	      }
	      cid = ((void *)ipcfg->ip) + ipcfg->n_ip * sizeof(ipcfg->ip[0]);
	      bcopy(cid, dhcp->client_id, dhcp->client_id_len);
	  }
	  my_log(LOG_DEBUG, "DHCP %s: start", if_name(if_p));
	  dhcp->xid = random();
	  if (service_ifstate(service_p)->netboot == TRUE
	      || recover_lease(service_p, &our_ip)) {
	      dhcp_init_reboot(service_p, IFEventID_start_e, &our_ip);
	  }
	  else {
	      dhcp_init(service_p, IFEventID_start_e, NULL);
	  }
	  break;
      }
      case IFEventID_stop_e: {
      stop:
	  my_log(LOG_DEBUG, "DHCP %s: stop", if_name(if_p));
	  if (dhcp == NULL) {
	      my_log(LOG_DEBUG, "DHCP %s: already stopped", 
		     if_name(if_p));
	      status = ipconfig_status_internal_error_e; /* shouldn't happen */
	      break;
	  }
	  if (event_id == IFEventID_stop_e) {
	      (void)dhcp_release(service_p);
	  }

	  /* remove IP address */
	  service_remove_address(service_p);

	  service_disable_autoaddr(service_p);

	  /* clean-up resources */
	  if (dhcp->timer) {
	      timer_callout_free(&dhcp->timer);
	  }
	  if (dhcp->client) {
	      bootp_client_free(&dhcp->client);
	  }
	  if (dhcp->arp) {
	      arp_client_free(&dhcp->arp);
	  }
	  if (dhcp->client_id) {
	      free(dhcp->client_id);
	      dhcp->client_id = NULL;
	  }
	  dhcpol_free(&dhcp->saved.options);
	  if (dhcp)
	      free(dhcp);
	  service_p->private = NULL;
	  break;
      }
      case IFEventID_change_e: {
	  void *			cid;
	  change_event_data_t *   	evdata = ((change_event_data_t *)event_data);
	  ipconfig_method_data_t *	ipcfg = evdata->config.data;

	  if (dhcp == NULL) {
	      my_log(LOG_DEBUG, "DHCP %s: private data is NULL", 
		     if_name(if_p));
	      return (ipconfig_status_internal_error_e);
	  }
	  evdata->needs_stop = FALSE;
	  cid = ((void *)ipcfg->ip) + ipcfg->n_ip * sizeof(ipcfg->ip[0]);
	  if (ipcfg->n_dhcp_client_id) {
	      if (dhcp->client_id == NULL 
		  || dhcp->client_id_len != ipcfg->n_dhcp_client_id
		  || bcmp(dhcp->client_id, cid, dhcp->client_id_len)) {
		  evdata->needs_stop = TRUE;
	      }
	  }
	  else {
	      if (dhcp->client_id != NULL) {
		  evdata->needs_stop = TRUE;
	      }
	  }
	  return (ipconfig_status_success_e);
      }
      case IFEventID_arp_collision_e: {
	  arp_collision_data_t *	arpc;
	  char				msg[128];
	  struct timeval 		tv;

	  arpc = (arp_collision_data_t *)event_data;

	  if (dhcp == NULL) {
	      return (ipconfig_status_internal_error_e);
	  }
	  if (dhcp->state != dhcp_cstate_bound_e
	      || dhcp->enable_arp_collision_detection == FALSE
	      || arpc->ip_addr.s_addr != dhcp->saved.our_ip.s_addr) {
	      break;
	  }
	  /* defend our address, don't just give it up XXX */
	  snprintf(msg, sizeof(msg),
		   IP_FORMAT " in use by " EA_FORMAT 
		   ", DHCP Server " 
		   IP_FORMAT, IP_LIST(&dhcp->saved.our_ip),
		   EA_LIST(arpc->hwaddr),
		   IP_LIST(&dhcp->saved.server_ip));
	  my_log(LOG_ERR, "DHCP %s: %s", if_name(if_p), msg);
	  service_report_conflict(service_p,
				  &dhcp->saved.our_ip,
				  arpc->hwaddr,
				  &dhcp->saved.server_ip);
	  _dhcp_lease_clear(service_p);
	  service_publish_failure(service_p, 
				  ipconfig_status_address_in_use_e, msg);
	  if (dhcp->saved.is_dhcp) {
	      dhcp_decline(service_p, IFEventID_start_e, NULL);
	      break;
	  }
	  dhcp_cancel_pending_events(service_p);
	  (void)service_disable_autoaddr(service_p);
	  dhcp->saved.our_ip.s_addr = 0;
	  dhcp->lease.valid = FALSE;
	  dhcp->router.flags = 0;
	  tv.tv_sec = 10; /* retry in a bit */
	  tv.tv_usec = 0;
	  timer_set_relative(dhcp->timer, tv, 
			     (timer_func_t *)dhcp_init,
			     service_p, (void *)IFEventID_start_e, NULL);
	  break;
      }
      case IFEventID_network_changed_e:
	  /* switched networks, remove the IP address to avoid IP collisions */
	  (void)service_remove_address(service_p);
      case IFEventID_renew_e:
      case IFEventID_media_e: {
	  struct in_addr	our_ip;

	  if (dhcp == NULL) {
	      return (ipconfig_status_internal_error_e);
	  }
	  our_ip = dhcp->saved.our_ip;
	  if (service_link_status(service_p)->valid == TRUE) {
	      dhcp->router.flags &= ~RIFLAGS_ARP_VERIFIED;
	      if (service_link_status(service_p)->active == TRUE) {
		  dhcp->in_use = dhcp->user_warned = FALSE;
		  if (dhcp_check_lease(service_p, current_time)) {
		      /* try same address */
		      if (dhcp->state != dhcp_cstate_init_reboot_e
			  || dhcp->try != 1) {
			  dhcp_init_reboot(service_p, IFEventID_start_e, 
					   &our_ip);
		      }
		      /* we're already in the init-reboot state */
		      break;
		  }
		  if (dhcp->state != dhcp_cstate_init_e
		      || dhcp->try != 1) {
		      dhcp_init(service_p, IFEventID_start_e, NULL);
		      break;
		  }
	      }
	      else if (event_id == IFEventID_media_e) {
		  struct timeval tv;
		  
		  /* ensure that we'll retry when the link goes back up */
		  dhcp->try = 0;
		  dhcp->state = dhcp_cstate_none_e;
		  dhcp->enable_arp_collision_detection = FALSE;

		  /* if link goes down and stays down long enough, unpublish */
		  dhcp_cancel_pending_events(service_p);
		  tv.tv_sec = G_link_inactive_secs;
		  tv.tv_usec = 0;
		  timer_set_relative(dhcp->timer, tv, 
				     (timer_func_t *)dhcp_link_timer,
				     service_p, NULL, NULL);
	      }
	  }
	  break;
      }
      case IFEventID_power_off_e:
      case IFEventID_sleep_e: {
	  if (dhcp == NULL) {
	      return (ipconfig_status_internal_error_e);
	  }
	  if (dhcp->lease.valid && dhcp->saved.is_dhcp) {
	      char *	router_hwaddr = NULL;
	      int 	router_hwaddr_size = 0;
	      
	      if (G_router_arp
		  && (dhcp->router.flags & RIFLAGS_HWADDR_VALID) != 0) {
		  /* we have the router h/w address, save it too */
		  router_hwaddr = dhcp->router.hwaddr;
		  router_hwaddr_size = if_link_length(if_p);
	      }
	      (void)_dhcp_lease_write(service_p, dhcp->lease.start,
				      dhcp->saved.pkt, dhcp->saved.pkt_size,
				      router_hwaddr, router_hwaddr_size);
	  }
	  break;
      }
      case IFEventID_wake_e: {
	  break;
      }
      default:
	  break;
    } /* switch (event_id) */
    return (status);
}

static void
dhcp_init(Service_t * service_p, IFEventID_t event_id, void * event_data)
{
    absolute_time_t 	current_time = timer_current_secs();
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)service_p->private;
    interface_t *	if_p = service_interface(service_p);
    ipconfig_status_t	status = ipconfig_status_success_e;
    struct timeval 	tv;

    switch (event_id) {
      case IFEventID_start_e: {
	  time_interval_t 	lease_option = htonl(SUGGESTED_LEASE_LENGTH);
	  dhcpoa_t		options;
	  dhcp_cstate_t		prev_state = dhcp->state;

	  my_log(LOG_DEBUG, "DHCP %s: INIT", if_name(if_p));

	  dhcp->state = dhcp_cstate_init_e;

	  /* clean-up anything that might have come before */
	  dhcp_cancel_pending_events(service_p);
	  
	  /* form the request */
	  dhcp->request = make_dhcp_request((struct dhcp *)dhcp->txbuf, 
					    sizeof(dhcp->txbuf),
					    dhcp_msgtype_discover_e,
					    if_link_address(if_p), 
					    if_link_arptype(if_p),
					    if_link_length(if_p),
					    dhcp->client_id, 
					    dhcp->client_id_len,
					    dhcp->must_broadcast,
					    &options);
	  if (dhcp->request == NULL) {
	      my_log(LOG_ERR, "DHCP %s: INIT make_dhcp_request failed",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  if (dhcpoa_add(&options, dhcptag_lease_time_e,
			 sizeof(lease_option), &lease_option) 
	      != dhcpoa_success_e) {
	      my_log(LOG_ERR, "DHCP %s: INIT dhcpoa_add lease time failed, %s", 
		     if_name(if_p), dhcpoa_err(&options));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  add_computer_name(&options);
	  if (dhcpoa_add(&options, dhcptag_end_e, 0, 0)
	      != dhcpoa_success_e) {
	      my_log(LOG_ERR, "DHCP %s: INIT failed to terminate options",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  dhcp->request_size = sizeof(*dhcp->request) + sizeof(G_rfc_magic) 
	      + dhcpoa_used(&options);
	  if (dhcp->request_size < sizeof(struct bootp)) {
	      /* pad out to BOOTP-sized packet */
	      dhcp->request_size = sizeof(struct bootp);
	  }
	  if (prev_state != dhcp_cstate_init_reboot_e) {
	      dhcp->wait_secs = G_initial_wait_secs;
	      dhcp->try = 0;
	      dhcp->start_secs = current_time;
	  }
	  dhcp->xid++;
	  dhcp->gathering = FALSE;
	  dhcp->saved.rating = 0;
	  (void)service_enable_autoaddr(service_p);
	  bootp_client_enable_receive(dhcp->client,
				      (bootp_receive_func_t *)dhcp_init, 
				      service_p, (void *)IFEventID_data_e);
	  /* FALL THROUGH */
      }
      case IFEventID_timeout_e: {
	  if (dhcp->gathering == TRUE) {
	      /* done gathering */
	      if (dhcp->saved.is_dhcp) {
		  dhcp_select(service_p, IFEventID_start_e, NULL);
		  break; /* out of switch */
	      }
	      dhcp_bound(service_p, IFEventID_start_e, NULL);
	      break; /* out of switch */
	  }
	  dhcp->try++;
	  if (dhcp->try > 1) {
	      if (service_link_status(service_p)->valid 
		  && service_link_status(service_p)->active == FALSE) {
		  dhcp_inactive(service_p);
		  break;
	      }
	  }
	  if (dhcp->try >= (G_dhcp_router_arp_at_retry_count + 1)) {
	      /* try to confirm the router's address */
	      if ((dhcp->router.flags & RIFLAGS_IADDR_VALID) != 0
		  && (dhcp->router.flags & RIFLAGS_ARP_VERIFIED) == 0) {
		  dhcp_arp_router(service_p, IFEventID_start_e, (void *)FALSE);
	      }
	      else if (dhcp_check_lease(service_p, current_time)
		       && (dhcp->router.flags & RIFLAGS_ARP_VERIFIED) != 0) {
		  /* lease is still valid, router is still available */
		  dhcp_bound(service_p, IFEventID_start_e, NULL);
		  break;
	      }
	  }
	  if (dhcp->try >= (G_dhcp_allocate_linklocal_at_retry_count + 1)) {
	      if (G_dhcp_failure_configures_linklocal) {
		  service_p->published.status = ipconfig_status_no_server_e;
		  linklocal_service_change(service_p, FALSE);
	      }
	  }
	  if (dhcp->try > (G_max_retries + 1)) {
	      /* no server responded */
	      dhcp_no_server(service_p, IFEventID_start_e, NULL);
	      break; /* out of switch */
	  }

	  dhcp->request->dp_xid = htonl(dhcp->xid);
	  dhcp->request->dp_secs 
	      = htons((u_short)(current_time - dhcp->start_secs));

	  /* send the packet */
	  if (bootp_client_transmit(dhcp->client,
				    G_ip_broadcast, G_ip_zeroes,
				    G_server_port, G_client_port,
				    dhcp->request, dhcp->request_size) < 0) {
	      my_log(LOG_ERR, 
		     "DHCP %s: INIT transmit failed", if_name(if_p));
	  }
	  /* wait for responses */
	  tv.tv_sec = dhcp->wait_secs;
	  tv.tv_usec = random_range(0, USECS_PER_SEC - 1);
	  my_log(LOG_DEBUG, "DHCP %s: INIT waiting at %d for %d.%06d", 
		 if_name(if_p), 
		 current_time - dhcp->start_secs,
		 tv.tv_sec, tv.tv_usec);
	  timer_set_relative(dhcp->timer, tv, 
			     (timer_func_t *)dhcp_init,
			     service_p, (void *)IFEventID_timeout_e, NULL);
	  /* next time wait twice as long */
	  dhcp->wait_secs *= 2;
	  if (dhcp->wait_secs > G_max_wait_secs)
	      dhcp->wait_secs = G_max_wait_secs;
	  break;
      }
      case IFEventID_data_e: {
	  boolean_t 		is_dhcp = TRUE;
	  time_interval_t 	lease;
	  bootp_receive_data_t *pkt = (bootp_receive_data_t *)event_data;
	  dhcp_msgtype_t	reply_msgtype = dhcp_msgtype_none_e;
	  struct in_addr	server_ip;
	  time_interval_t 	t1;
	  time_interval_t 	t2;

	  if (verify_packet(pkt, dhcp->xid, if_p, &reply_msgtype,
			    &server_ip, &is_dhcp) == FALSE
	      || server_ip.s_addr == 0
	      || ip_valid(pkt->data->dp_yiaddr) == FALSE) {
	      /* reject the packet */
	      break; /* out of switch */
	  }
	  if (is_dhcp == FALSE
	      || reply_msgtype == dhcp_msgtype_offer_e) {
	      int rating = 0;
	      
	      get_lease(&pkt->options, &lease, &t1, &t2);
	      rating = count_params(&pkt->options, dhcp_params, n_dhcp_params);
	      /* 
	       * The new packet is "better" than the saved
	       * packet if:
	       * - there was no saved packet, or
	       * - the new packet is a DHCP packet and the saved
	       *   one is a BOOTP packet or a DHCP packet with
	       *   a lower rating, or
	       * - the new packet and the saved packet are both
	       *   BOOTP but the new one has a higher rating
	       * All this to allow BOOTP/DHCP interoperability
	       * ie. we accept a BOOTP response if it's
	       * the only one we've got.  We expect/favour a DHCP 
	       * response.
	       */
	      if (dhcp->saved.pkt_size == 0
		  || (is_dhcp == TRUE && (dhcp->saved.is_dhcp == FALSE 
					  || rating > dhcp->saved.rating))
		  || (is_dhcp == FALSE && dhcp->saved.is_dhcp == FALSE
		      && rating > dhcp->saved.rating)) {
		  dhcpol_free(&dhcp->saved.options);
		  bcopy(pkt->data, dhcp->saved.pkt, pkt->size);
		  dhcp->saved.pkt_size = pkt->size;
		  dhcp->saved.rating = rating;
		  (void)dhcpol_parse_packet(&dhcp->saved.options, 
					    (void *)dhcp->saved.pkt, 
					    dhcp->saved.pkt_size, NULL);
		  dhcp->saved.our_ip = pkt->data->dp_yiaddr;
		  dhcp->saved.server_ip = server_ip;
		  dhcp->saved.is_dhcp = is_dhcp;
		  dhcp_set_lease_params(service_p, "INIT", is_dhcp, 
					current_time, lease, t1, t2);
		  if (is_dhcp && rating == n_dhcp_params) {
		      dhcp_select(service_p, IFEventID_start_e, NULL);
		      break; /* out of switch */
		  }
		  if (dhcp->gathering == FALSE) {
		      struct timeval t = {0,0};
		      t.tv_sec = G_gather_secs;
		      my_log(LOG_DEBUG, "DHCP %s: INIT gathering began at %d", 
			     if_name(if_p), 
			     current_time - dhcp->start_secs);
		      dhcp->gathering = TRUE;
		      timer_set_relative(dhcp->timer, t, 
					 (timer_func_t *)dhcp_init,
					 service_p, (void *)IFEventID_timeout_e, 
					 NULL);
		  }
	      }
	  }
	  break;
      }
      default:
	  break;
    }
    return;

 error:
    dhcp_failed(service_p, status, "INIT", TRUE);
    return;
}


static void
dhcp_init_reboot(Service_t * service_p, IFEventID_t evid, void * event_data)
{
    absolute_time_t 	current_time = timer_current_secs();
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)service_p->private;
    interface_t *	if_p = service_interface(service_p);
    struct in_addr	source_ip = G_ip_zeroes;
    ipconfig_status_t	status = ipconfig_status_success_e;
    struct timeval 	tv;
    struct in_addr 	server_ip = G_ip_broadcast;

    if (service_ifstate(service_p)->netboot == TRUE) {
	netboot_addresses(&source_ip, NULL);
    }

    switch (evid) {
      case IFEventID_start_e: {
	  struct in_addr 	our_ip;
	  time_interval_t 	lease_option = htonl(SUGGESTED_LEASE_LENGTH);
	  dhcpoa_t	 	options;

	  my_log(LOG_DEBUG, "DHCP %s: INIT-REBOOT", if_name(if_p));
	  dhcp->start_secs = current_time;
	  dhcp->try = 0;
	  dhcp->wait_secs = G_initial_wait_secs;
	  dhcp->state = dhcp_cstate_init_reboot_e;
	  if (source_ip.s_addr == 0) {
	      our_ip = *((struct in_addr *)event_data);
	  }
	  else {
	      our_ip = source_ip;
	  }

	  /* clean-up anything that might have come before */
	  dhcp_cancel_pending_events(service_p);

	  /* form the request */
	  dhcp->request = make_dhcp_request((struct dhcp *)dhcp->txbuf, 
					    sizeof(dhcp->txbuf), 
					    dhcp_msgtype_request_e,
					    if_link_address(if_p), 
					    if_link_arptype(if_p),
					    if_link_length(if_p), 
					    dhcp->client_id, 
					    dhcp->client_id_len,
					    dhcp->must_broadcast,
					    &options);
	  if (dhcp->request == NULL) {
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  if (dhcpoa_add(&options, dhcptag_requested_ip_address_e,
			 sizeof(our_ip), &our_ip) 
	      != dhcpoa_success_e) {
	      my_log(LOG_ERR, "DHCP %s: INIT-REBOOT add request ip failed, %s", 
		     if_name(if_p), dhcpoa_err(&options));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  if (dhcpoa_add(&options, dhcptag_lease_time_e,
			 sizeof(lease_option), &lease_option) 
	      != dhcpoa_success_e) {
	      my_log(LOG_ERR, "DHCP %s: INIT-REBOOT add lease time failed, %s", 
		     if_name(if_p), dhcpoa_err(&options));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  add_computer_name(&options);
	  if (dhcpoa_add(&options, dhcptag_end_e, 0, 0)
	      != dhcpoa_success_e) {
	      my_log(LOG_ERR, "DHCP %s: INIT-REBOOT failed to terminate options",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  dhcp->request_size = sizeof(*dhcp->request) + sizeof(G_rfc_magic) 
	      + dhcpoa_used(&options);
	  if (dhcp->request_size < sizeof(struct bootp)) {
	      /* pad out to BOOTP-sized packet */
	      dhcp->request_size = sizeof(struct bootp);
	  }
	  dhcp->gathering = FALSE;
	  dhcp->xid++;
	  dhcp->saved.our_ip = our_ip;
	  dhcp->saved.rating = 0;
	  (void)service_enable_autoaddr(service_p);
	  bootp_client_enable_receive(dhcp->client,
				      (bootp_receive_func_t *)dhcp_init_reboot, 
				      service_p, (void *)IFEventID_data_e);
	  /* FALL THROUGH */
      }
      case IFEventID_timeout_e: {
	  if (dhcp->gathering == TRUE) {
	      /* done gathering */
	      dhcp_bound(service_p, IFEventID_start_e, NULL);
	      break; /* out of switch */
	  }
	  dhcp->try++;
	  if (dhcp->try > 1) {
	      if (service_link_status(service_p)->valid 
		  && service_link_status(service_p)->active == FALSE) {
		  dhcp_inactive(service_p);
		  break;
	      }
	  }
	  if (dhcp->try >= (G_dhcp_router_arp_at_retry_count + 1)) {
	      /* try to confirm the router's address */
	      if ((dhcp->router.flags & RIFLAGS_IADDR_VALID) != 0
		  && (dhcp->router.flags & RIFLAGS_ARP_VERIFIED) == 0) {
		  dhcp_arp_router(service_p, IFEventID_start_e, (void *)FALSE);
	      }
	      else if (dhcp_check_lease(service_p, current_time)
		       && (dhcp->router.flags & RIFLAGS_ARP_VERIFIED) != 0) {
		  /* lease is still valid, router is still available */
		  dhcp_bound(service_p, IFEventID_start_e, NULL);
		  break;
	      }
	  }
	  if (dhcp->try >= (G_dhcp_allocate_linklocal_at_retry_count + 1)) {
	      if (G_dhcp_failure_configures_linklocal) {
		  service_p->published.status = ipconfig_status_no_server_e;
		  linklocal_service_change(service_p, FALSE);
	      }
	  }
	  if (dhcp->try > (G_dhcp_init_reboot_retry_count + 1)) {
	      my_log(LOG_DEBUG, "DHCP %s: INIT-REBOOT timed out", 
		     if_name(if_p));
	      (void)service_remove_address(service_p);
	      service_publish_failure_sync(service_p, 
					   ipconfig_status_no_server_e,
					   NULL, FALSE);
	      dhcp->try--;
	      /* go back to the INIT state */
	      dhcp_init(service_p, IFEventID_start_e, NULL);
	      break; /* ouf of case */
	  }

	  dhcp->request->dp_xid = htonl(dhcp->xid);
	  dhcp->request->dp_secs 
	      = htons((u_short)(current_time - dhcp->start_secs));
	  /* send the packet */
	  if (bootp_client_transmit(dhcp->client,
				    server_ip, source_ip,
				    G_server_port, G_client_port,
				    dhcp->request, dhcp->request_size) < 0) {
	      my_log(LOG_ERR, 
		     "DHCP %s: INIT-REBOOT transmit failed", 
		     if_name(if_p));
	  }
	  /* wait for responses */
	  tv.tv_sec = dhcp->wait_secs;
	  tv.tv_usec = random_range(0, USECS_PER_SEC - 1);
	  my_log(LOG_DEBUG, "DHCP %s: INIT-REBOOT waiting at %d for %d.%06d", 
		 if_name(if_p), 
		 current_time - dhcp->start_secs,
		 tv.tv_sec, tv.tv_usec);
	  timer_set_relative(dhcp->timer, tv, 
			     (timer_func_t *)dhcp_init_reboot,
			     service_p, (void *)IFEventID_timeout_e, NULL);
	  /* next time wait twice as long */
	  dhcp->wait_secs *= 2;
	  if (dhcp->wait_secs > G_max_wait_secs)
	      dhcp->wait_secs = G_max_wait_secs;
	  break;
      }
      case IFEventID_data_e: {
	  boolean_t 		is_dhcp = TRUE;
	  time_interval_t 		lease;
	  bootp_receive_data_t *pkt = (bootp_receive_data_t *)event_data;
	  dhcp_msgtype_t	reply_msgtype = dhcp_msgtype_none_e;
	  struct in_addr	server_ip;
	  time_interval_t 	t1;
	  time_interval_t 	t2;

	  if (verify_packet(pkt, dhcp->xid, if_p, &reply_msgtype,
			    &server_ip, &is_dhcp) == FALSE) {
	      /* reject the packet */
	      break; /* out of switch */
	  }
	  if (reply_msgtype == dhcp_msgtype_nak_e
	      && service_ifstate(service_p)->netboot == FALSE) {
	      service_publish_failure(service_p, 
				      ipconfig_status_lease_terminated_e,
				      NULL);
	      dhcp_unbound(service_p, IFEventID_start_e, NULL);
	      break; /* out of switch */
	  }
	  if (server_ip.s_addr == 0
	      || pkt->data->dp_yiaddr.s_addr != dhcp->saved.our_ip.s_addr) {
	      /* reject the packet */
	      break; /* out of switch */
	  }
	  if (is_dhcp == FALSE  
	      || reply_msgtype == dhcp_msgtype_ack_e) {
	      int rating = 0;
	      
	      get_lease(&pkt->options, &lease, &t1, &t2);

	      rating = count_params(&pkt->options, dhcp_params, n_dhcp_params);
	      /* 
	       * The new packet is "better" than the saved
	       * packet if:
	       * - there was no saved packet, or
	       * - the new packet is a DHCP packet and the saved
	       *   one is a BOOTP packet or a DHCP packet with
	       *   a lower rating, or
	       * - the new packet and the saved packet are both
	       *   BOOTP but the new one has a higher rating
	       * All this to allow BOOTP/DHCP interoperability
	       * ie. we accept a BOOTP response if it's
	       * the only one we've got.  We expect/favour a DHCP 
	       * response.
	       */
	      if (dhcp->saved.pkt_size == 0
		  || (is_dhcp == TRUE && (dhcp->saved.is_dhcp == FALSE 
					  || rating > dhcp->saved.rating))
		  || (is_dhcp == FALSE && dhcp->saved.is_dhcp == FALSE
		      && rating > dhcp->saved.rating)) {
		  dhcpol_free(&dhcp->saved.options);
		  bcopy(pkt->data, dhcp->saved.pkt, pkt->size);
		  dhcp->saved.pkt_size = pkt->size;
		  dhcp->saved.rating = rating;
		  (void)dhcpol_parse_packet(&dhcp->saved.options, 
					    (void *)dhcp->saved.pkt, 
					    dhcp->saved.pkt_size, NULL);
		  dhcp->saved.our_ip = pkt->data->dp_yiaddr;
		  dhcp->saved.server_ip = server_ip;
		  dhcp->saved.is_dhcp = is_dhcp;
		  dhcp_set_lease_params(service_p, "INIT-REBOOT", is_dhcp,
					current_time, lease, t1, t2);
		  if (is_dhcp && rating == n_dhcp_params) {
		      dhcp_bound(service_p, IFEventID_start_e, NULL);
		      break; /* out of switch */
		  }
		  if (dhcp->gathering == FALSE) {
		      struct timeval t = {0,0};
		      t.tv_sec = G_gather_secs;
		      my_log(LOG_DEBUG, 
			     "DHCP %s: INIT-REBOOT gathering began at %d", 
			     if_name(if_p), 
			     current_time - dhcp->start_secs);
		      dhcp->gathering = TRUE;
		      timer_set_relative(dhcp->timer, t, 
					 (timer_func_t *)dhcp_init_reboot,
					 service_p, 
					 (void *)IFEventID_timeout_e, 
					 NULL);
		  }
	      }
	  }
	  break;
      }
      default:
	  break; /* shouldn't happen */
    }
    return;

 error:
    dhcp_failed(service_p, status, "INIT-REBOOT", TRUE);
    return;
}

static void
dhcp_select(Service_t * service_p, IFEventID_t evid, void * event_data)
{
    absolute_time_t 	current_time = timer_current_secs();
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)service_p->private;
    interface_t *	if_p = service_interface(service_p);
    ipconfig_status_t	status = ipconfig_status_success_e;
    struct timeval 	tv;

    switch (evid) {
      case IFEventID_start_e: {
	  dhcpoa_t	 	options;

	  my_log(LOG_DEBUG, "DHCP %s: SELECT", if_name(if_p));
	  /* clean-up anything that might have come before */
	  dhcp_cancel_pending_events(service_p);

	  dhcp->state = dhcp_cstate_select_e;

	  /* form the request */
	  dhcp->request = make_dhcp_request((struct dhcp *)dhcp->txbuf, 
					    sizeof(dhcp->txbuf),
					    dhcp_msgtype_request_e,
					    if_link_address(if_p), 
					    if_link_arptype(if_p),
					    if_link_length(if_p), 
					    dhcp->client_id, 
					    dhcp->client_id_len,
					    dhcp->must_broadcast,
					    &options);
	  if (dhcp->request == NULL) {
	      my_log(LOG_ERR, "DHCP %s: SELECT make_dhcp_request failed",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  /* insert server identifier and requested ip address */
	  if (dhcpoa_add(&options, dhcptag_requested_ip_address_e,
			 sizeof(dhcp->saved.our_ip), &dhcp->saved.our_ip)
	      != dhcpoa_success_e) {
	      my_log(LOG_ERR, "DHCP %s: SELECT add requested ip failed, %s", 
		     if_name(if_p), dhcpoa_err(&options));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  if (dhcpoa_add(&options, dhcptag_server_identifier_e,
			 sizeof(dhcp->saved.server_ip), &dhcp->saved.server_ip)
	      != dhcpoa_success_e) {
	      my_log(LOG_ERR, "DHCP %s: SELECT add server ip failed, %s", 
		     if_name(if_p), dhcpoa_err(&options));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  add_computer_name(&options);
	  if (dhcpoa_add(&options, dhcptag_end_e, 0, 0)
	      != dhcpoa_success_e) {
	      my_log(LOG_ERR, "DHCP %s: SELECT failed to terminate options",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  dhcp->request_size = sizeof(*dhcp->request) + sizeof(G_rfc_magic) 
	      + dhcpoa_used(&options);
	  if (dhcp->request_size < sizeof(struct bootp)) {
	      /* pad out to BOOTP-sized packet */
	      dhcp->request_size = sizeof(struct bootp);
	  }
	  dhcp->try = 0;
	  dhcp->gathering = FALSE;
	  dhcp->wait_secs = G_initial_wait_secs;
	  bootp_client_enable_receive(dhcp->client,
				      (bootp_receive_func_t *)dhcp_select, 
				      service_p, (void *)IFEventID_data_e);
      }
      case IFEventID_timeout_e: {
	  dhcp->try++;
	  if (dhcp->try > (G_dhcp_select_retry_count + 1)) {
	      my_log(LOG_DEBUG, "DHCP %s: SELECT timed out", if_name(if_p));
	      /* go back to INIT and try again */
	      dhcp_init(service_p, IFEventID_start_e, NULL);
	      break; /* out of switch */
	  }
	  dhcp->request->dp_xid = htonl(dhcp->xid);
	  dhcp->request->dp_secs 
	      = htons((u_short)(current_time - dhcp->start_secs));

	  /* send the packet */
	  if (bootp_client_transmit(dhcp->client,
				    G_ip_broadcast, G_ip_zeroes,
				    G_server_port, G_client_port,
				    dhcp->request, dhcp->request_size) < 0) {
	      my_log(LOG_ERR, 
		     "DHCP %s: SELECT transmit failed", 
		     if_name(if_p));
	  }
	  /* wait for responses */
	  tv.tv_sec = dhcp->wait_secs;
	  tv.tv_usec = 0;
	  my_log(LOG_DEBUG, "DHCP %s: SELECT waiting at %d for %d.%06d", 
		 if_name(if_p), 
		 current_time - dhcp->start_secs,
		 tv.tv_sec, tv.tv_usec);
	  timer_set_relative(dhcp->timer, tv, 
			     (timer_func_t *)dhcp_select,
			     service_p, (void *)IFEventID_timeout_e, NULL);
	  /* next time wait twice as long */
	  dhcp->wait_secs *= 2;
	  if (dhcp->wait_secs > G_max_wait_secs)
	      dhcp->wait_secs = G_max_wait_secs;
	  break;
      }
      case IFEventID_data_e: {
	  boolean_t 		is_dhcp = TRUE;
	  time_interval_t 		lease = SUGGESTED_LEASE_LENGTH;
	  bootp_receive_data_t *pkt = (bootp_receive_data_t *)event_data;
	  dhcp_msgtype_t	reply_msgtype = dhcp_msgtype_none_e;
	  struct in_addr	server_ip = { 0 };
	  time_interval_t 	t1;
	  time_interval_t 	t2;

	  if (verify_packet(pkt, dhcp->xid, if_p, &reply_msgtype,
			    &server_ip, &is_dhcp) == FALSE
	      || is_dhcp == FALSE) {
	      /* reject the packet */
	      break; /* out of switch */
	  }

	  if (reply_msgtype == dhcp_msgtype_nak_e) {
	      if (server_ip.s_addr == 0 
		  || server_ip.s_addr != dhcp->saved.server_ip.s_addr) {
		  /* reject the packet */
		  break; /* out of switch */
	      }
	      /* clean-up anything that might have come before */
	      dhcp_cancel_pending_events(service_p);

	      /* 
	       * wait to retry INIT just in case there's a misbehaving server
	       * and we get stuck in an INIT-SELECT-NAK infinite loop
	       */
	      tv.tv_sec = 10;
	      tv.tv_usec = 0;
	      timer_set_relative(dhcp->timer, tv, 
				 (timer_func_t *)dhcp_init,
				 service_p, (void *)IFEventID_start_e, NULL);
	      break; /* out of switch */
	  }
	  if (reply_msgtype != dhcp_msgtype_ack_e
	      || ip_valid(pkt->data->dp_yiaddr) == FALSE) {
	      /* reject the packet */
	      break; /* out of switch */
	  }
	  get_lease(&pkt->options, &lease, &t1, &t2);
	  dhcp_set_lease_params(service_p, "SELECT", is_dhcp, 
				current_time, lease, t1, t2);
	  dhcpol_free(&dhcp->saved.options);
	  bcopy(pkt->data, dhcp->saved.pkt, pkt->size);
	  dhcp->saved.pkt_size = pkt->size;
	  dhcp->saved.rating = 0;
	  (void)dhcpol_parse_packet(&dhcp->saved.options, 
				    (void *)dhcp->saved.pkt, 
				    dhcp->saved.pkt_size, NULL);
	  dhcp->saved.our_ip = pkt->data->dp_yiaddr;
	  if (server_ip.s_addr != 0) {
	      dhcp->saved.server_ip = server_ip;
	  }
	  dhcp->saved.is_dhcp = TRUE;
	  dhcp_bound(service_p, IFEventID_start_e, NULL);
	  break;
      }
      default:
	  break;
    }
    return;

 error:
    dhcp_failed(service_p, status, "SELECT", TRUE);
    return;
}

static boolean_t
dhcp_update_router_address(Service_t * service_p)
{
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)service_p->private;
    struct in_addr *	router_p;

    dhcp->router.flags = 0;
    router_p = find_option_with_length(&dhcp->saved.options, 
				       dhcptag_router_e, 4);
    if (router_p == NULL) {
	return (FALSE);
    }
    if (router_p->s_addr == dhcp->saved.our_ip.s_addr) {
	/* proxy arp, use DNS server instead */
	router_p = find_option_with_length(&dhcp->saved.options, 
					   dhcptag_domain_name_server_e, 4);
	if (router_p == NULL) {
	    return (FALSE);
	}
    }
    dhcp->router.iaddr = *router_p;
    dhcp->router.flags = RIFLAGS_IADDR_VALID;
    return (TRUE);
}

static void
dhcp_arp_router(Service_t * service_p, IFEventID_t event_id, void * event_data)
{
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)service_p->private;
    interface_t *	if_p = service_interface(service_p);

    switch (event_id) {
      case IFEventID_start_e: {
	  boolean_t		discover = (boolean_t)event_data;
	  int			gratuitous_count;
	  int			probe_count;
	  struct timeval	tv;
	  boolean_t		use_real_ip;

	  if (G_router_arp == FALSE) {
	      /* don't ARP for the router */
	      break;
	  }
	  dhcp->router.flags &= ~RIFLAGS_ARP_VERIFIED;
	  if ((dhcp->router.flags & RIFLAGS_IADDR_VALID) == 0) {
	      my_log(LOG_DEBUG, 
		     "DHCP %s: ARP ROUTER: IP address missing",
		     if_name(if_p));
	      break;
	  }
	  if (discover == FALSE
	      && (dhcp->router.flags & RIFLAGS_HWADDR_VALID) == 0) {
	      my_log(LOG_DEBUG, 
		     "DHCP %s: ARP ROUTER: hardware address missing",
		     if_name(if_p));
	      break;
	  }
	  if (ip_is_private(dhcp->saved.our_ip) == FALSE) {
	      use_real_ip = TRUE;
	  }
	  else {
	      use_real_ip = discover;
	  }
	  my_log(LOG_DEBUG, "DHCP %s: ARP ROUTER "  IP_FORMAT " started", 
		 if_name(if_p), IP_LIST(&dhcp->router.iaddr));
	  gratuitous_count = 0;
	  tv.tv_sec = 0;
	  tv.tv_usec = 200 * 1000;
	  if (discover) {
	      dhcp->router.flags |= RIFLAGS_DISCOVERING;
	      probe_count = 10;
	  }
	  else {
	      probe_count = 3;
	  }
	  arp_client_set_probe_info(dhcp->arp, &tv,
				    &probe_count, &gratuitous_count);
	  arp_client_probe(dhcp->arp,
			   (arp_result_func_t *)dhcp_arp_router,
			   service_p,
			   (void *)IFEventID_arp_e, 
			   use_real_ip ? dhcp->saved.our_ip : G_ip_zeroes,
			   dhcp->router.iaddr);
	  break;
      }
      case IFEventID_arp_e: {
	  arp_result_t *	result = (arp_result_t *)event_data;

	  if (result->error) {
	      my_log(LOG_ERR, "DHCP %s: ARP ROUTER failed, %s", 
		     if_name(if_p),
		     arp_client_errmsg(dhcp->arp));
	      break;
	  }
	  if (result->in_use) {
	      if ((dhcp->router.flags & RIFLAGS_DISCOVERING) == 0
		  && bcmp(result->hwaddr, dhcp->router.hwaddr, 
			  if_link_length(if_p)) != 0) {
		  my_log(LOG_DEBUG, "DHCP %s: ARP ROUTER " IP_FORMAT 
			 ": hardware address doesn't match", if_name(if_p),
			 IP_LIST(&dhcp->router.iaddr));
		  dhcp->lease.valid = FALSE;
		  dhcp->router.flags = 0;
		  (void)service_remove_address(service_p);
	      }
	      else {
		  /* grab the latest router hardware address */
		  bcopy(result->hwaddr, dhcp->router.hwaddr, 
			if_link_length(if_p));
		  dhcp->router.flags = RIFLAGS_IADDR_VALID
		      | RIFLAGS_HWADDR_VALID | RIFLAGS_ARP_VERIFIED;
		  my_log(LOG_DEBUG, "DHCP %s: ARP ROUTER " IP_FORMAT 
			 ": response received", if_name(if_p),
			 IP_LIST(&dhcp->router.iaddr));
		  if (dhcp->lease.needs_write) {
		      dhcp->lease.needs_write = FALSE;
		      (void)_dhcp_lease_write(service_p, dhcp->lease.start,
					      dhcp->saved.pkt, 
					      dhcp->saved.pkt_size,
					      dhcp->router.hwaddr,
					      if_link_length(if_p));
		  }
	      }
	  }
	  else {
	      my_log(LOG_DEBUG, "DHCP %s: ARP router " IP_FORMAT 
		     ": no response", if_name(if_p),
		     IP_LIST(&dhcp->router.iaddr));
	      dhcp->router.flags &= ~RIFLAGS_DISCOVERING;
	  }
	  break;
      }
      default:
	  break;
    }
}

static void
dhcp_bound(Service_t * service_p, IFEventID_t event_id, void * event_data)
{
    absolute_time_t 	current_time = timer_current_secs();
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)service_p->private;
    boolean_t		do_arp = TRUE;
    interface_t *	if_p = service_interface(service_p);
    int			len;
    struct in_addr	mask = {0};
    void *		option;
    struct timeval 	tv = {0, 0};

    switch (event_id) {
      case IFEventID_start_e: {
	  my_log(LOG_DEBUG, "DHCP %s: BOUND", if_name(if_p));
	  dhcp->lease.needs_write = TRUE;
	  dhcp->enable_arp_collision_detection = FALSE;
	  if (dhcp->state == dhcp_cstate_renew_e
	      || dhcp->state == dhcp_cstate_rebind_e) {
	      dhcp->state = dhcp_cstate_bound_e;
	      dhcp->lease.needs_write = FALSE;
	      break; /* out of switch */
	  }

	  dhcp->state = dhcp_cstate_bound_e;
	  /* clean-up anything that might have come before */
	  dhcp_cancel_pending_events(service_p);

	  /* do an ARP probe of the supplied address */
	  if (service_ifstate(service_p)->netboot == TRUE
	      || service_p->info.addr.s_addr == dhcp->saved.our_ip.s_addr) {
	      /* no need to probe, we're already using it */
	      break;
	  }
	  arp_client_restore_default_probe_info(dhcp->arp);
	  arp_client_probe(dhcp->arp, 
			   (arp_result_func_t *)dhcp_bound, service_p,
			   (void *)IFEventID_arp_e, G_ip_zeroes,
			   dhcp->saved.our_ip);
	  return;
	  break;
	}
	case IFEventID_arp_e: {
	  arp_result_t *	result = (arp_result_t *)event_data;

	  if (result->error) {
	      my_log(LOG_ERR, "DHCP %s: ARP probe failed, %s", 
		     if_name(if_p),
		     arp_client_errmsg(dhcp->arp));
	      dhcp_failed(service_p, ipconfig_status_internal_error_e, NULL,
			  TRUE);
	      return;
	  }
	  if (result->in_use) {
	      char		msg[128];
	      struct timeval 	tv;

	      snprintf(msg, sizeof(msg),
		       IP_FORMAT " in use by " EA_FORMAT 
		       ", DHCP Server " 
		       IP_FORMAT, IP_LIST(&dhcp->saved.our_ip),
		       EA_LIST(result->hwaddr),
		       IP_LIST(&dhcp->saved.server_ip));
	      if (dhcp->in_use) {
		  if (dhcp->user_warned == FALSE) {
		      service_report_conflict(service_p,
					      &dhcp->saved.our_ip,
					      result->hwaddr,
					      &dhcp->saved.server_ip);
		      dhcp->user_warned = TRUE;
		  }
	      }
	      dhcp->in_use = TRUE;
	      my_log(LOG_ERR, "DHCP %s: %s", if_name(if_p), msg);
	      _dhcp_lease_clear(service_p);
	      dhcp->lease.valid = FALSE;
	      dhcp->router.flags = 0;
	      service_publish_failure(service_p, 
				      ipconfig_status_address_in_use_e, msg);
	      if (dhcp->saved.is_dhcp) {
		  dhcp_decline(service_p, IFEventID_start_e, NULL);
		  return;
	      }
	      dhcp_cancel_pending_events(service_p);
	      (void)service_disable_autoaddr(service_p);
	      dhcp->saved.our_ip.s_addr = 0;
	      tv.tv_sec = 10; /* retry in a bit */
	      tv.tv_usec = 0;
	      timer_set_relative(dhcp->timer, tv, 
				 (timer_func_t *)dhcp_init,
				 service_p, (void *)IFEventID_start_e, NULL);
	      return;
	  }
	  break;
	}
	default:
	  return;
    }

    /* the lease is valid */
    dhcp->lease.valid = TRUE;

    if (G_router_arp == FALSE
	&& dhcp->lease.needs_write && dhcp->saved.is_dhcp) {
	/* write the lease now since we won't ARP for the router */
	dhcp->lease.needs_write = FALSE;
	(void)_dhcp_lease_write(service_p, dhcp->lease.start,
				dhcp->saved.pkt, dhcp->saved.pkt_size,
				NULL, 0);
    }

    /* allow user warning to appear */
    dhcp->in_use = dhcp->user_warned = FALSE;

    /* set the interface's address and output the status */
    option = dhcpol_find(&dhcp->saved.options, dhcptag_subnet_mask_e, 
			 &len, NULL);
    if (option != NULL && len >= 4) {
	mask = *((struct in_addr *)option);
    }

    service_disable_autoaddr(service_p);

    /* set our new address */
    (void)service_set_address(service_p, dhcp->saved.our_ip, 
			      mask, G_ip_zeroes);
    service_publish_success2(service_p, dhcp->saved.pkt,
			     dhcp->saved.pkt_size, dhcp->lease.start);

    /* stop link local if necessary */
    if (G_dhcp_success_deconfigures_linklocal) {
	linklocal_service_change(service_p, TRUE);
    }

    /* allow us to be called in the event of a subsequent collision */
    dhcp->enable_arp_collision_detection = TRUE;

    /* clean-up anything that might have come before */
    dhcp_cancel_pending_events(service_p);

    if (dhcp->lease.length == DHCP_INFINITE_LEASE) {
	/* don't need to talk to server anymore */
	my_log(LOG_DEBUG, "DHCP %s: infinite lease", 
	       if_name(if_p));
    }
    else {
	if (dhcp->lease.t1 < current_time) {
	    /* t1 is already in the past, wake up in RENEW momentarily */
	    tv.tv_sec = 0;
	    tv.tv_usec = 1;
	    do_arp = FALSE;
	}
	else {
	    /* wake up in RENEW at t1 */
	    tv.tv_sec = dhcp->lease.t1 - current_time;
	    tv.tv_usec = 0;
	}
	timer_set_relative(dhcp->timer, tv, 
			   (timer_func_t *)dhcp_renew_rebind,
			   service_p, (void *)IFEventID_start_e, NULL);
    }

    /* get the router's MAC address */
    if (do_arp) {
	if (dhcp_update_router_address(service_p)) {
	    dhcp_arp_router(service_p, IFEventID_start_e, (void *)TRUE);
	}
    }

    return;
}

static void
dhcp_no_server(Service_t * service_p, IFEventID_t event_id, void * event_data)
{
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)service_p->private;
    struct timeval 	tv;
    
    switch (event_id) {
      case IFEventID_start_e: {
	  if (G_dhcp_failure_configures_linklocal) {
	      linklocal_service_change(service_p, FALSE);
	  }
	  dhcp_cancel_pending_events(service_p);
	  service_publish_failure(service_p, 
				  ipconfig_status_no_server_e,
				  NULL);
	  
#define INIT_RETRY_INTERVAL_SECS      (1 * 60)
	  tv.tv_sec = INIT_RETRY_INTERVAL_SECS;
	  tv.tv_usec = 0;
	  /* wake up in INIT state after a period of waiting */
	  timer_set_relative(dhcp->timer, tv, 
			     (timer_func_t *)dhcp_init,
			     service_p, (void *)IFEventID_start_e, NULL);
	  break;
      }
      default: {
	break;
      }
    }
    return;
}

static void
dhcp_decline(Service_t * service_p, IFEventID_t event_id, void * event_data)
{
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)service_p->private;
    interface_t *	if_p = service_interface(service_p);
    ipconfig_status_t	status = ipconfig_status_success_e;
    struct timeval 	tv;

    switch (event_id) {
      case IFEventID_start_e: {
	  dhcpoa_t	 	options;

	  my_log(LOG_DEBUG, "DHCP %s: DECLINE", if_name(if_p));

	  /* clean-up anything that might have come before */
	  dhcp_cancel_pending_events(service_p);

	  /* decline the address */
	  dhcp->state = dhcp_cstate_decline_e;
	  dhcp->request = make_dhcp_request((struct dhcp *)dhcp->txbuf, 
					    sizeof(dhcp->txbuf),
					    dhcp_msgtype_decline_e,
					    if_link_address(if_p), 
					    if_link_arptype(if_p),
					    if_link_length(if_p), 
					    dhcp->client_id, 
					    dhcp->client_id_len,
					    FALSE,
					    &options);
	  if (dhcp->request == NULL) {
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  if (dhcpoa_add(&options, dhcptag_requested_ip_address_e,
			 sizeof(dhcp->saved.our_ip), &dhcp->saved.our_ip) 
	      != dhcpoa_success_e) {
	      my_log(LOG_ERR, "DHCP %s: DECLINE couldn't add our ip, %s",
		     if_name(if_p), dhcpoa_err(&options));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  if (dhcpoa_add(&options, dhcptag_server_identifier_e,
			 sizeof(dhcp->saved.server_ip), &dhcp->saved.server_ip)
	      != dhcpoa_success_e) {
	      my_log(LOG_ERR, "DHCP %s: DECLINE couldn't add server ip, %s",
		     if_name(if_p), dhcpoa_err(&options));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  if (dhcpoa_add(&options, dhcptag_end_e, 0, 0)
	      != dhcpoa_success_e) {
	      my_log(LOG_ERR, "DHCP %s: DECLINE failed to terminate options",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  if (bootp_client_transmit(dhcp->client,
				    G_ip_broadcast, G_ip_zeroes,
				    G_server_port, G_client_port,
				    dhcp->request, dhcp->request_size) < 0) {
	      my_log(LOG_ERR, 
		     "DHCP %s: DECLINE transmit failed", 
		     if_name(if_p));
	  }
	  (void)service_remove_address(service_p);
	  dhcp->saved.our_ip.s_addr = 0;
	  service_disable_autoaddr(service_p);
	  tv.tv_sec = 10; /* retry in a bit */
	  tv.tv_usec = 0;
	  timer_set_relative(dhcp->timer, tv, 
			     (timer_func_t *)dhcp_init,
			     service_p, (void *)IFEventID_start_e, NULL);
	  break;
      }
      default:
	  break;
    }
    return;
 error:
    dhcp_failed(service_p, status, "DECLINE", TRUE);
    return;
}

static void
dhcp_unbound(Service_t * service_p, IFEventID_t event_id, void * event_data)
{
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)service_p->private;
    interface_t *	if_p = service_interface(service_p);
    struct timeval 	tv = {0,0};

    switch (event_id) {
      case IFEventID_start_e: {
	  my_log(LOG_DEBUG, "DHCP %s: UNBOUND", if_name(if_p));

	  /* clean-up anything that might have come before */
	  dhcp_cancel_pending_events(service_p);
	  dhcp->state = dhcp_cstate_unbound_e;

	  /* stop using the IP address immediately */
	  (void)service_remove_address(service_p);
	  dhcp->saved.our_ip.s_addr = 0;
	  dhcp->lease.valid = FALSE;
	  dhcp->router.flags = 0;
	  _dhcp_lease_clear(service_p);

	  tv.tv_sec = 0;
	  tv.tv_usec = 1000;
	  timer_set_relative(dhcp->timer, tv, 
			     (timer_func_t *)dhcp_init,
			     service_p, (void *)IFEventID_start_e, NULL);
	  break;
      }
      default:
	break;
    }
    return;
}

static void
dhcp_renew_rebind(Service_t * service_p, IFEventID_t event_id, void * event_data)
{
    absolute_time_t 	current_time = timer_current_secs();
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)service_p->private;
    interface_t *	if_p = service_interface(service_p);
    ipconfig_status_t	status = ipconfig_status_success_e;
    struct timeval 	tv;

    switch (event_id) {
      case IFEventID_start_e: {
	  time_interval_t 	lease_option;
	  dhcpoa_t	 	options;

	  /* clean-up anything that might have come before */
	  dhcp_cancel_pending_events(service_p);

	  dhcp->start_secs = current_time;

	  dhcp->state = dhcp_cstate_renew_e;
	  my_log(LOG_DEBUG, "DHCP %s: RENEW/REBIND", if_name(if_p));
	  dhcp->request = make_dhcp_request((struct dhcp *)dhcp->txbuf, 
					    sizeof(dhcp->txbuf),
					    dhcp_msgtype_request_e,
					    if_link_address(if_p), 
					    if_link_arptype(if_p),
					    if_link_length(if_p), 
					    dhcp->client_id, 
					    dhcp->client_id_len,
					    FALSE,
					    &options);
	  if (dhcp->request == NULL) {
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  dhcp->try = 0;
	  dhcp->request->dp_ciaddr = dhcp->saved.our_ip;
	  lease_option = htonl(SUGGESTED_LEASE_LENGTH);
	  if (dhcpoa_add(&options, dhcptag_lease_time_e, sizeof(lease_option), 
			 &lease_option) != dhcpoa_success_e) {
	      my_log(LOG_ERR, "DHCP %s: RENEW/REBIND couldn't add"
		     " lease time: %s", if_name(if_p),
		     dhcpoa_err(&options));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  add_computer_name(&options);
	  if (dhcpoa_add(&options, dhcptag_end_e, 0, 0)
	      != dhcpoa_success_e) {
	      my_log(LOG_ERR, "DHCP %s: RENEW/REBIND failed to terminate options",
		     if_name(if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  /* enable packet reception */
	  bootp_client_enable_receive(dhcp->client,
				      (bootp_receive_func_t *)dhcp_renew_rebind,
				      service_p, (void *)IFEventID_data_e);
	  /* FALL THROUGH */
      }
      case IFEventID_timeout_e: {
	  struct in_addr	dest_ip = {0};
	  absolute_time_t	wakeup_time;

	  if (current_time >= dhcp->lease.expiration) {
	      /* server did not respond */
	      service_publish_failure(service_p, 
				      ipconfig_status_server_not_responding_e, 
				      NULL);
	      dhcp_unbound(service_p, IFEventID_start_e, NULL);
	      return;
	  }
	  if (current_time < dhcp->lease.t2) {
	      dhcp->state = dhcp_cstate_renew_e;
	      wakeup_time = current_time + (dhcp->lease.t2 - current_time) / 2;
	      dest_ip = dhcp->saved.server_ip;
	  }
	  else { /* rebind */
	      dhcp->state = dhcp_cstate_rebind_e;
	      wakeup_time = current_time 
		  + (dhcp->lease.expiration - current_time) / 2;
	      dest_ip = G_ip_broadcast;
	  }
	  dhcp->request->dp_xid = htonl(++dhcp->xid);
	  dhcp->request->dp_secs 
	      = htons((u_short)(current_time - dhcp->start_secs));

	  /* send the packet */
	  if (bootp_client_transmit(dhcp->client,
				    dest_ip, dhcp->saved.our_ip,
				    G_server_port, G_client_port,
				    dhcp->request, dhcp->request_size) < 0) {
	      my_log(LOG_ERR, 
		     "DHCP %s: RENEW/REBIND transmit failed", 
		     if_name(if_p));
	  }
	  /* wait for responses */
#define RENEW_REBIND_MIN_WAIT_SECS	60
	  if ((wakeup_time - current_time) < RENEW_REBIND_MIN_WAIT_SECS) {
	      tv.tv_sec = RENEW_REBIND_MIN_WAIT_SECS;
	  }
	  else {
	      tv.tv_sec = wakeup_time - current_time;
	  }
	  tv.tv_usec = 0;
	  my_log(LOG_DEBUG, "DHCP %s: RENEW/REBIND waiting at %d for %d.%06d", 
		 if_name(if_p), 
		 current_time - dhcp->start_secs,
		 tv.tv_sec, tv.tv_usec);
	  timer_set_relative(dhcp->timer, tv, 
			     (timer_func_t *)dhcp_renew_rebind,
			     service_p, (void *)IFEventID_timeout_e, NULL);
	  break;
      }
      case IFEventID_data_e: {
	  boolean_t 		is_dhcp = TRUE;
	  time_interval_t	lease = SUGGESTED_LEASE_LENGTH;
	  bootp_receive_data_t *pkt = (bootp_receive_data_t *)event_data;
	  dhcp_msgtype_t	reply_msgtype = dhcp_msgtype_none_e;
	  struct in_addr	server_ip;
	  time_interval_t 	t1;
	  time_interval_t 	t2;

	  if (verify_packet(pkt, dhcp->xid, if_p, &reply_msgtype,
			    &server_ip, &is_dhcp) == FALSE
	      || is_dhcp == FALSE) {
	      /* reject the packet */
	      return;
	  }

	  if (reply_msgtype == dhcp_msgtype_nak_e) {
	      service_publish_failure(service_p, 
				      ipconfig_status_lease_terminated_e,
				      NULL);
	      dhcp_unbound(service_p, IFEventID_start_e, NULL);
	      return;
	  }
	  if (reply_msgtype != dhcp_msgtype_ack_e
	      || server_ip.s_addr == 0
	      || ip_valid(pkt->data->dp_yiaddr) == FALSE) {
	      /* reject the packet */
	      return;
	  }
	  get_lease(&pkt->options, &lease, &t1, &t2);

	  /* address has to match, otherwise start over */
	  if (pkt->data->dp_yiaddr.s_addr != dhcp->saved.our_ip.s_addr) {
	      service_publish_failure(service_p, 
				      ipconfig_status_server_error_e,
				      NULL);
	      dhcp_unbound(service_p, IFEventID_start_e, NULL);
	      return;
	  }
	  dhcp_set_lease_params(service_p, "RENEW/REBIND", is_dhcp,
				current_time, lease, t1, t2);
	  dhcpol_free(&dhcp->saved.options);
	  bcopy(pkt->data, dhcp->saved.pkt, pkt->size);
	  dhcp->saved.pkt_size = pkt->size;
	  dhcp->saved.rating = 0;
	  (void)dhcpol_parse_packet(&dhcp->saved.options, 
				    (void *)dhcp->saved.pkt, 
				    dhcp->saved.pkt_size, NULL);
	  dhcp->saved.server_ip = server_ip;
	  dhcp->saved.is_dhcp = TRUE;
	  dhcp_bound(service_p, IFEventID_start_e, NULL);
	  break;
      }
      default:
	  return;
    }
    return;

 error:
    dhcp_failed(service_p, status, "RENEW/REBIND", TRUE);
    return;
}

static void
dhcp_release(Service_t * service_p)
{
    interface_t *	if_p = service_interface(service_p);
    Service_dhcp_t *	dhcp = (Service_dhcp_t *)service_p->private;
    dhcpoa_t	 	options;

    if (dhcp->saved.is_dhcp == FALSE || dhcp->lease.valid == FALSE) {
	return;
    }

    my_log(LOG_DEBUG, "DHCP %s: RELEASE", if_name(if_p));
    _dhcp_lease_clear(service_p);
    dhcp->lease.valid = FALSE;
    dhcp->router.flags = 0;

    /* clean-up anything that might have come before */
    dhcp_cancel_pending_events(service_p);

    /* release the address */
    dhcp->request = make_dhcp_request((struct dhcp *)dhcp->txbuf, 
				      sizeof(dhcp->txbuf),
				      dhcp_msgtype_release_e,
				      if_link_address(if_p), 
				      if_link_arptype(if_p),
				      if_link_length(if_p), 
				      dhcp->client_id, dhcp->client_id_len,
				      FALSE,
				      &options);
    if (dhcp->request == NULL) {
	return;
    }
    dhcp->request->dp_xid = htonl(++dhcp->xid);
    dhcp->request->dp_ciaddr = dhcp->saved.our_ip;
    if (dhcpoa_add(&options, dhcptag_server_identifier_e,
		   sizeof(dhcp->saved.server_ip), &dhcp->saved.server_ip)
	!= dhcpoa_success_e) {
	my_log(LOG_ERR, "DHCP %s: RELEASE couldn't add server ip, %s",
	       if_name(if_p), dhcpoa_err(&options));
	return;
    }
    if (dhcpoa_add(&options, dhcptag_end_e, 0, 0)
	!= dhcpoa_success_e) {
	my_log(LOG_ERR, "DHCP %s: RELEASE failed to terminate options",
	       if_name(if_p));
	return;
    }
    if (bootp_client_transmit(dhcp->client,
			      dhcp->saved.server_ip, dhcp->saved.our_ip,
			      G_server_port, G_client_port,
			      dhcp->request, dhcp->request_size) < 0) {
	my_log(LOG_ERR, 
	       "DHCP %s: RELEASE transmit failed", 
	       if_name(if_p));
	return;
    }
    dhcp->saved.our_ip.s_addr = 0;
    return;
}

