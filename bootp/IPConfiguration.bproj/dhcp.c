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
#import <netinet/bootp.h>
#import <arpa/inet.h>
#import <syslog.h>

#import "rfc_options.h"
#import "dhcp_options.h"
#import "dhcp.h"
#import "interfaces.h"
#import "util.h"
#import <net/if_types.h>
#import "host_identifier.h"
#import "dhcplib.h"

#import "ipconfigd_threads.h"

extern char *  			ether_ntoa(struct ether_addr *e);

#import "dprintf.h"

#define SUGGESTED_LEASE_LENGTH		(60 * 60 * 24 * 30 * 3) /* 3 months */

#define MIN_LEASE_LENGTH		(60) 			/* 1 minute */

/* ad-hoc networking definitions */
#define AD_HOC_RANGE_START	((u_long)0xa9fe0000) /* 169.254.0.0 */
#define AD_HOC_RANGE_END	((u_long)0xa9feffff) /* 169.254.255.255 */
#define AD_HOC_FIRST_USEABLE	(AD_HOC_RANGE_START + 256) /* 169.254.1.0 */
#define AD_HOC_LAST_USEABLE	(AD_HOC_RANGE_END - 256) /* 169.254.254.255 */
#define AD_HOC_MASK		((u_long)0xffff0000) /* 255.255.0.0 */

#define	MAX_AD_HOC_TRIES	10

typedef struct {
    arp_client_t *	arp;
    bootp_client_t *	client;
    void *		client_id;
    int			client_id_len;
    boolean_t		gathering;
    u_char *		idstr;
    boolean_t		in_use;
    dhcp_time_secs_t	lease_start;
    dhcp_time_secs_t	lease_expiration;
    boolean_t		lease_is_infinite;
    dhcp_lease_t	lease_length;
    struct dhcp * 	request;
    int			request_size;
    struct saved_pkt	saved;
    dhcp_cstate_t	state;
    dhcp_time_secs_t	start_secs;
    dhcp_time_secs_t	t1;
    dhcp_time_secs_t	t2;
    timer_callout_t *	timer;
    int			try;
    char *		txbuf[sizeof(struct dhcp) + DHCP_MIN_OPTIONS_SIZE];
    u_long		xid;
    boolean_t		user_warned;
    dhcp_time_secs_t	wait_secs;
    struct {
	int		current;
	struct in_addr	probe;
	u_short		offset[MAX_AD_HOC_TRIES];
    } ad_hoc;
} IFState_dhcp_t;

typedef struct {
    arp_client_t *	arp;
    bootp_client_t *	client;
    boolean_t		gathering;
    u_char *		idstr;
    struct in_addr	our_ip;
    struct in_addr	our_mask;
    struct dhcp * 	request;
    int			request_size;
    struct saved_pkt	saved;
    dhcp_time_secs_t	start_secs;
    timer_callout_t *	timer;
    int			try;
    char *		txbuf[sizeof(struct dhcp) + DHCP_MIN_OPTIONS_SIZE];
    u_long		xid;
    dhcp_time_secs_t	wait_secs;
} IFState_inform_t;

static void
dhcp_init(IFState_t * ifstate, IFEventID_t event_id, void * event_data);

static void
dhcp_init_reboot(IFState_t * ifstate, IFEventID_t event_id, void * event_data);

static void
dhcp_select(IFState_t * ifstate, IFEventID_t event_id, void * event_data);

static void
dhcp_bound(IFState_t * ifstate, IFEventID_t event_id, void * event_data);

static void
dhcp_ad_hoc(IFState_t * ifstate, IFEventID_t event_id, void * event_data);

static void
dhcp_renew_rebind(IFState_t * ifstate, IFEventID_t event_id, void * event_data);

static void
dhcp_unbound(IFState_t * ifstate, IFEventID_t event_id, void * event_data);

static void
dhcp_decline(IFState_t * ifstate, IFEventID_t event_id, void * event_data);

static void
dhcp_release(IFState_t * ifstate);

static __inline__ boolean_t
get_server_identifier(dhcpol_t * options, struct in_addr * server_ip)
{
    struct in_addr * 	ipaddr_p;

    ipaddr_p = (struct in_addr *) 
	dhcpol_find(options, dhcptag_server_identifier_e, NULL, NULL);
    if (ipaddr_p)
	*server_ip = *ipaddr_p;
    return (ipaddr_p != NULL);
}

static __inline__ boolean_t
get_lease(dhcpol_t * options, dhcp_lease_t * lease_time)
{
    dhcp_lease_t *	lease_p;

    lease_p = (dhcp_lease_t *)
	dhcpol_find(options, dhcptag_lease_time_e, NULL, NULL);
    if (lease_p) {
	*lease_time = dhcp_lease_ntoh(*lease_p);
    }
    return (lease_p != NULL);
}

static u_char dhcp_static_default_params[] = {
    dhcptag_subnet_mask_e, 
    dhcptag_router_e,
    dhcptag_domain_name_server_e,
    dhcptag_domain_name_e,
    dhcptag_netinfo_server_address_e,
    dhcptag_netinfo_server_tag_e,
    dhcptag_slp_directory_agent_e,
    dhcptag_slp_service_scope_e,
};
static int	n_dhcp_static_default_params 
	= sizeof(dhcp_static_default_params) / sizeof(dhcp_static_default_params[0]);

static u_char * dhcp_default_params = dhcp_static_default_params;
static int	n_dhcp_default_params 
	= sizeof(dhcp_static_default_params)  / sizeof(dhcp_static_default_params[0]);

static u_char * dhcp_params = dhcp_static_default_params;
static int	n_dhcp_params 
	= sizeof(dhcp_static_default_params)  / sizeof(dhcp_static_default_params[0]);

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
	dhcp_default_params = dhcp_static_default_params;
	n_dhcp_default_params = n_dhcp_static_default_params;
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

static __inline__ void
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
		  void * cid, int cid_len,
		  dhcpoa_t * options_p)
{
    char * 	buf = NULL;
    u_char 	cid_type = 0;

    bzero(request, pkt_size);
    request->dp_op = BOOTREQUEST;
    request->dp_htype = hwtype;
    request->dp_hlen = hwlen;
    if (G_must_broadcast)
	request->dp_flags = htons(DHCP_FLAGS_BROADCAST);
    bcopy(hwaddr, request->dp_chaddr, hwlen);
    bcopy(G_rfc_magic, request->dp_options, sizeof(G_rfc_magic));
    dhcpoa_init(options_p, request->dp_options + sizeof(G_rfc_magic),
		DHCP_MIN_OPTIONS_SIZE - sizeof(G_rfc_magic));
    
    /* make the request a dhcp message */
    if (dhcpoa_add_dhcpmsg(options_p, msg) != dhcpoa_success_e) {
	my_log(LOG_ERR,
	       "make_dhcp_request: couldn't add dhcp message tag %d, %s", msg,
	       dhcpoa_err(options_p));
	goto err;
    }

    if (msg != dhcp_msgtype_decline_e && msg != dhcp_msgtype_release_e) {
	/* add the list of required parameters */
	if (dhcpoa_add(options_p, dhcptag_parameter_request_list_e,
			n_dhcp_params, dhcp_params)
	    != dhcpoa_success_e) {
	    my_log(LOG_ERR, "make_dhcp_request: "
		   "couldn't add parameter request list, %s",
		   dhcpoa_err(options_p));
	    goto err;
	}
    }

    /* if no client id was specified, use the hardware address */
    if (cid == NULL || cid_len == 0) {
	cid = hwaddr;
	cid_len = hwlen;
	cid_type = hwtype;
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

inet_addrinfo_t *
interface_is_ad_hoc(interface_t * if_p)
{
    const struct in_addr	ad_hoc_net = { htonl(AD_HOC_RANGE_START) };
    const struct in_addr	ad_hoc_mask = { htonl(AD_HOC_MASK) };
    int 			i;

    for (i = 0; i < if_inet_count(if_p); i++) {
	inet_addrinfo_t * info =  if_inet_addr_at(if_p, i);

	if (in_subnet(ad_hoc_net, ad_hoc_mask, info->addr))
	    return (info);
    }
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
inform_cancel_pending_events(IFState_t * ifstate)
{
    IFState_inform_t *	inform = (IFState_inform_t *)ifstate->private;

    if (inform == NULL)
	return;
    if (inform->timer) {
	timer_cancel(inform->timer);
    }
    if (inform->client) {
	bootp_client_disable_receive(inform->client);
    }
    if (inform->arp) {
	arp_cancel_probe(inform->arp);
    }
    return;
}

static void
inform_inactive(IFState_t * ifstate)
{
    IFState_inform_t *	inform = (IFState_inform_t *)ifstate->private;

    inform_cancel_pending_events(ifstate);
    ifstate_remove_addresses(ifstate);
    dhcpol_free(&inform->saved.options);
    ifstate_publish_failure(ifstate, ipconfig_status_media_inactive_e,
			    NULL);
    return;
}

static void
inform_link_timer(void * arg0, void * arg1, void * arg2)
{
    inform_inactive((IFState_t *) arg0);
    return;
}

static void
inform_failed(IFState_t * ifstate, ipconfig_status_t status, char * msg)
{
    inform_cancel_pending_events(ifstate);
    ifstate_publish_failure(ifstate, status, msg);
    return;
}

static void
inform_success(IFState_t * ifstate)
{
    IFState_inform_t *	inform = (IFState_inform_t *)ifstate->private;
    int 		len;
    void *		option;
	
    option = dhcpol_find(&inform->saved.options, dhcptag_subnet_mask_e,
			 &len, NULL);
    if (option) {
	inform->our_mask = *((struct in_addr *)option);
	
	/* reset the interface address with the new mask */
	(void)inet_add(ifstate, inform->our_ip, 
		       &inform->our_mask, NULL);
    }
    inform_cancel_pending_events(ifstate);

    ifstate_publish_success(ifstate, inform->saved.pkt, 
			    inform->saved.pkt_size);
    return;
}

static void
inform_request(IFState_t * ifstate, IFEventID_t event_id, void * event_data)
{
    IFState_inform_t *	inform = (IFState_inform_t *)ifstate->private;
    ipconfig_status_t	status = ipconfig_status_success_e;

    switch (event_id) {
      case IFEventID_start_e: {
	  dhcpoa_t		options;
	  
	  /* clean-up anything that might have come before */
	  inform_cancel_pending_events(ifstate);
	  inform->request = make_dhcp_request((struct dhcp *)inform->txbuf, 
					      sizeof(inform->txbuf),
					      dhcp_msgtype_inform_e,
					      if_link_address(ifstate->if_p), 
					      if_link_arptype(ifstate->if_p),
					      if_link_length(ifstate->if_p), 
					      NULL, 0,
					      &options);
	  if (inform->request == NULL) {
	      my_log(LOG_ERR, "INFORM %s: make_dhcp_request failed",
		     if_name(ifstate->if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  inform->request->dp_ciaddr = inform->our_ip;
	  add_computer_name(&options);
	  if (dhcpoa_add(&options, dhcptag_end_e, 0, 0)
	      != dhcpoa_success_e) {
	      my_log(LOG_ERR, "INFORM %s: failed to terminate options",
		     if_name(ifstate->if_p));
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
	  inform->wait_secs = INITIAL_WAIT_SECS;
	  bootp_client_enable_receive(inform->client,
				      (bootp_receive_func_t *)inform_request,
				      ifstate, (void *)IFEventID_data_e);
	  dhcpol_free(&inform->saved.options);
	  bzero(&inform->saved, sizeof(inform->saved));

	  /* FALL THROUGH */
      }
      case IFEventID_timeout_e: {
	  struct timeval 	tv;
	  if (inform->gathering == TRUE) {
	      /* done gathering */
	      inform_success(ifstate);
	      return;
	  }
	  inform->try++;
	  if (inform->try > 1) {
	      if (ifstate->link.valid && ifstate->link.active == FALSE) {
		  inform_inactive(ifstate);
		  break; /* out of switch */
	      }
	  }
	  if (inform->try > (G_max_retries + 1)) {
	      status = ipconfig_status_no_server_e;
	      goto error;
	  }
	  inform->request->dp_xid = htonl(++inform->xid);
#ifdef DEBUG
	  dhcp_print_packet(inform->request, inform->request_size);
#endif DEBUG
	  /* send the packet */
	  if (bootp_client_transmit(inform->client, if_name(ifstate->if_p),
				    inform->request->dp_htype, NULL, 0,
				    G_ip_broadcast, G_ip_zeroes,
				    G_server_port, G_client_port,
				    inform->request, 
				    inform->request_size) < 0) {
	      my_log(LOG_ERR, 
		     "INFORM %s: transmit failed", if_name(ifstate->if_p));
	  }
	  /* wait for responses */
	  tv.tv_sec = inform->wait_secs;
	  tv.tv_usec = random_range(0, USECS_PER_SEC - 1);
	  my_log(LOG_DEBUG, "INFORM %s: waiting at %d for %d.%06d", 
		 if_name(ifstate->if_p), 
		 timer_current_secs() - inform->start_secs,
		 tv.tv_sec, tv.tv_usec);
	  timer_set_relative(inform->timer, tv, 
			     (timer_func_t *)inform_request,
			     ifstate, (void *)IFEventID_timeout_e, NULL);
	  /* next time wait twice as long */
	  inform->wait_secs *= 2;
	  if (inform->wait_secs > MAX_WAIT_SECS) {
	      inform->wait_secs = MAX_WAIT_SECS;
	  }
	  break;
      }
      case IFEventID_data_e: {
	  boolean_t 		is_dhcp = TRUE;
	  bootp_receive_data_t *pkt = (bootp_receive_data_t *)event_data;
	  dhcp_msgtype_t	reply_msgtype = dhcp_msgtype_none_e;
	  struct in_addr	server_ip;

	  if (verify_packet(pkt, inform->xid, ifstate->if_p, &reply_msgtype,
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
		      inform_success(ifstate);
		      return;
		  }
		  if (inform->gathering == FALSE) {
		      struct timeval t = {0,0};
		      t.tv_sec = G_gather_secs;
		      my_log(LOG_DEBUG, "INFORM %s: gathering began at %d", 
			     if_name(ifstate->if_p), 
			     timer_current_secs() - inform->start_secs);
		      inform->gathering = TRUE;
		      timer_set_relative(inform->timer, t, 
					 (timer_func_t *)inform_request,
					 ifstate, (void *)IFEventID_timeout_e, 
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
    inform_failed(ifstate, status, NULL);
    return;
}

static void
inform_start(IFState_t * ifstate, IFEventID_t event_id, void * event_data)
{
    IFState_inform_t *	inform = (IFState_inform_t *)ifstate->private;

    switch (event_id) {
      case IFEventID_start_e: {
	  inform_cancel_pending_events(ifstate);

	  arp_probe(inform->arp, 
		    (arp_result_func_t *)inform_start, ifstate,
		    (void *)IFEventID_arp_e, G_ip_zeroes,
		    inform->our_ip);
	  break;
      }
      case IFEventID_arp_e: {
	  arp_result_t *	result = (arp_result_t *)event_data;

	  if (result->error) {
	      my_log(LOG_DEBUG, "INFORM %s: arp probe failed, %s", 
		     if_name(ifstate->if_p),
		     arp_client_errmsg(inform->arp));
	  }
	  else {
	      if (result->in_use) {
		  char	msg[128];

		  snprintf(msg, sizeof(msg),
			   IP_FORMAT " in use by " EA_FORMAT,
			   IP_LIST(&inform->our_ip), EA_LIST(result->hwaddr));
		  ifstate_tell_user(ifstate, msg);
		  my_log(LOG_ERR, "INFORM %s: %s", if_name(ifstate->if_p), 
			 msg);
		  (void)inet_remove(ifstate, inform->our_ip);
		  inform_failed(ifstate, ipconfig_status_address_in_use_e,
				msg);
		  break;
	      }
	  }
	  if (ifstate->link.valid == TRUE && ifstate->link.active == FALSE) {
	      inform_inactive(ifstate);
	      break;
	  }

	  /* set the primary address */
	  (void)inet_add(ifstate, inform->our_ip, 
			 &inform->our_mask, NULL);
	  inform_request(ifstate, IFEventID_start_e, 0);
	  break;
      }
      default: {
	  break;
      }
    }
    return;
}

ipconfig_status_t
inform_thread(IFState_t * ifstate, IFEventID_t event_id, void * event_data)
{
    IFState_inform_t *	inform = (IFState_inform_t *)ifstate->private;
    ipconfig_status_t	status = ipconfig_status_success_e;

    switch (event_id) {
      case IFEventID_start_e: {
	  start_event_data_t *    evdata = ((start_event_data_t *)event_data);
	  ipconfig_method_data_t *ipcfg = evdata->config.data;

	  if (if_flags(ifstate->if_p) & IFF_LOOPBACK) {
	      status = ipconfig_status_invalid_operation_e;
	      break;
	  }
	  if (inform) {
	      my_log(LOG_ERR, "INFORM %s: re-entering start state", 
		     if_name(ifstate->if_p));
	      status = ipconfig_status_internal_error_e;
	      break;
	  }
	  status = validate_method_data_addresses(&evdata->config,
						  ipconfig_method_inform_e,
						  if_name(ifstate->if_p));
	  if (status != ipconfig_status_success_e)
	      break;

	  inform = malloc(sizeof(*inform));
	  if (inform == NULL) {
	      my_log(LOG_ERR, "INFORM %s: malloc failed", 
		     if_name(ifstate->if_p));
	      status = ipconfig_status_allocation_failed_e;
	      break;
	  }
	  ifstate->private = inform;
	  bzero(inform, sizeof(*inform));
	  inform->idstr = identifierToString(if_link_arptype(ifstate->if_p), 
					   if_link_address(ifstate->if_p), 
					   if_link_length(ifstate->if_p));
	  dhcpol_init(&inform->saved.options);
	  inform->our_ip = ipcfg->ip[0].addr;
	  inform->our_mask = ipcfg->ip[0].mask;
	  inform->timer = timer_callout_init();
	  if (inform->timer == NULL) {
	      my_log(LOG_ERR, "INFORM %s: timer_callout_init failed", 
		     if_name(ifstate->if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  inform->client = bootp_client_init(G_bootp_session);
	  if (inform->client == NULL) {
	      my_log(LOG_ERR, "INFORM %s: bootp_client_init failed",
		     if_name(ifstate->if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  inform->arp = arp_client_init(G_arp_session, ifstate->if_p);
	  if (inform->arp == NULL) {
	      my_log(LOG_ERR, "INFORM %s: arp_client_init failed", 
		     if_name(ifstate->if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  my_log(LOG_DEBUG, "INFORM %s: id %s start", 
		 if_name(ifstate->if_p), inform->idstr);
	  inform->start_secs = timer_current_secs();
	  inform->xid = random();
	  dhcpol_init(&inform->saved.options);
	  inform_start(ifstate, IFEventID_start_e, NULL);
	  break;
      }
      case IFEventID_stop_e: {
      stop:
	  my_log(LOG_DEBUG, "INFORM %s: stop", if_name(ifstate->if_p));
	  if (inform == NULL) { /* already stopped */
	      break;
	  }
	  /* remove IP address(es) */
	  ifstate_remove_addresses(ifstate);

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
	  if (inform->idstr) {
	      free(inform->idstr);
	      inform->idstr = NULL;
	  }

	  dhcpol_free(&inform->saved.options);
	  if (inform)
	      free(inform);
	  ifstate->private = NULL;
	  break;
      }
      case IFEventID_change_e: {
	  change_event_data_t *   evdata = ((change_event_data_t *)event_data);
	  ipconfig_method_data_t *ipcfg = evdata->config.data;
	  ipconfig_status_t	  status;	

	  if (inform == NULL) {
	      my_log(LOG_DEBUG, "INFORM %s: private data is NULL", 
		     if_name(ifstate->if_p));
	      return (ipconfig_status_internal_error_e);
	  }
	  status = validate_method_data_addresses(&evdata->config,
						  ipconfig_method_inform_e,
						  if_name(ifstate->if_p));
	  if (status != ipconfig_status_success_e)
	      return (status);
	  evdata->needs_stop = FALSE;
	  if (ipcfg->ip[0].addr.s_addr != inform->our_ip.s_addr) {
	      evdata->needs_stop = TRUE;
	  }
	  else if (ipcfg->ip[0].mask.s_addr != 0
		   && ipcfg->ip[0].mask.s_addr != inform->our_mask.s_addr) {
	      inform->our_mask = ipcfg->ip[0].mask;
	      (void)inet_add(ifstate, inform->our_ip,
			     &inform->our_mask, NULL);
	  }
	  return (ipconfig_status_success_e);
      }
      case IFEventID_media_e: {
	  if (inform == NULL)
	      return (ipconfig_status_internal_error_e);

	  if (ifstate->link.valid == TRUE) {
	      if (ifstate->link.active == TRUE) {
		  inform_start(ifstate, IFEventID_start_e, 0);
	      }
	      else {
		  struct timeval tv;

		  /* if link goes down and stays down long enough, unpublish */
		  inform_cancel_pending_events(ifstate);
		  tv.tv_sec = G_link_inactive_secs;
		  tv.tv_usec = 0;
		  timer_set_relative(inform->timer, tv, 
				     (timer_func_t *)inform_link_timer,
				     ifstate, NULL, NULL);
	      }
	  }
	  break;
      }
      case IFEventID_renew_e: {
	  if (inform == NULL) {
	      return (ipconfig_status_internal_error_e);
	  }
	  if (ifstate->link.valid == TRUE && ifstate->link.active == TRUE) {
	      inform_start(ifstate, IFEventID_start_e, 0);
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
dhcp_cancel_pending_events(IFState_t * ifstate)
{
    IFState_dhcp_t *	dhcp = (IFState_dhcp_t *)ifstate->private;

    if (dhcp == NULL)
	return;
    if (dhcp->timer) {
	timer_cancel(dhcp->timer);
    }
    if (dhcp->client) {
	bootp_client_disable_receive(dhcp->client);
    }
    if (dhcp->arp) {
	arp_cancel_probe(dhcp->arp);
    }
    return;
}


static void
dhcp_failed(IFState_t * ifstate, ipconfig_status_t status, char * msg)
{
    IFState_dhcp_t *	dhcp = (IFState_dhcp_t *)ifstate->private;

    dhcp_cancel_pending_events(ifstate);

    inet_disable_autoaddr(ifstate);
    dhcpol_free(&dhcp->saved.options);
    if (dhcp->saved.our_ip.s_addr) {
	(void)inet_remove(ifstate, dhcp->saved.our_ip);
    }
    dhcp->saved.our_ip.s_addr = 0;
    ifstate_publish_failure(ifstate, status, msg);
    ((IFState_dhcp_t *)ifstate->private)->state = dhcp_cstate_none_e;
    return;
}

static void
dhcp_inactive(IFState_t * ifstate)
{
    IFState_dhcp_t * 	dhcp = (IFState_dhcp_t *)ifstate->private;

    dhcp_cancel_pending_events(ifstate);
    ifstate_remove_addresses(ifstate);
    inet_disable_autoaddr(ifstate);
    dhcpol_free(&dhcp->saved.options);
    ifstate_publish_failure(ifstate, ipconfig_status_media_inactive_e, NULL);
    return;
}

static void
dhcp_set_lease_params(IFState_t * ifstate, char * descr, boolean_t is_dhcp,
		      dhcp_lease_t lease)
{
    IFState_dhcp_t * dhcp = (IFState_dhcp_t *)ifstate->private;

    dhcp->lease_start = timer_current_secs();

    dhcp->lease_is_infinite = FALSE;

    if (is_dhcp == FALSE) {
	dhcp->lease_is_infinite = TRUE;
    }
    else {
	if (lease == DHCP_INFINITE_LEASE) {
	    dhcp->lease_is_infinite = TRUE;
	}
	else {
	    if (lease < MIN_LEASE_LENGTH) {
		lease = MIN_LEASE_LENGTH;
	    }
	    dhcp->lease_length = lease;
	}
    }

    if (dhcp->lease_is_infinite) {
	dhcp->lease_length = dhcp->t1 = dhcp->t2 = dhcp->lease_expiration = 0;
    }
    else {
	dhcp->lease_expiration = dhcp->lease_start + dhcp->lease_length;
	dhcp->t1 = dhcp->lease_start 
	    + (dhcp_lease_t) ((double)dhcp->lease_length * 0.5);
	dhcp->t2 = dhcp->lease_start
	    + (dhcp_lease_t) ((double)dhcp->lease_length * 0.875);
    }
    my_log(LOG_DEBUG, 
	   "DHCP %s: %s lease"
	   " start = 0x%x, t1 = 0x%x , t2 = 0x%x, expiration 0x%x", 
	   if_name(ifstate->if_p), descr, dhcp->lease_start, 
	   dhcp->t1, dhcp->t2, dhcp->lease_expiration);
    return;
}

static void
dhcp_link_timer(void * arg0, void * arg1, void * arg2)
{
    dhcp_inactive((IFState_t *)arg0);
    return;
}

ipconfig_status_t
dhcp_thread(IFState_t * ifstate, IFEventID_t event_id, void * event_data)
{
    dhcp_time_secs_t 	current_time = timer_current_secs();
    IFState_dhcp_t *	dhcp = (IFState_dhcp_t *)ifstate->private;
    ipconfig_status_t	status = ipconfig_status_success_e;

    switch (event_id) {
      case IFEventID_start_e: {
	  start_event_data_t *   	evdata = ((start_event_data_t *)event_data);
	  ipconfig_method_data_t*	ipcfg = evdata->config.data;
	  struct in_addr		our_ip;

	  if (if_flags(ifstate->if_p) & IFF_LOOPBACK) {
	      status = ipconfig_status_invalid_operation_e;
	      break;
	  }
	  if (dhcp) {
	      my_log(LOG_ERR, "DHCP %s: re-entering start state", 
		     if_name(ifstate->if_p));
	      status = ipconfig_status_internal_error_e;
	      break;
	  }

	  dhcp = malloc(sizeof(*dhcp));
	  if (dhcp == NULL) {
	      my_log(LOG_ERR, "DHCP %s: malloc failed", 
		     if_name(ifstate->if_p));
	      status = ipconfig_status_allocation_failed_e;
	      break;
	  }
	  bzero(dhcp, sizeof(*dhcp));
	  ifstate->private = dhcp;

	  dhcp->state = dhcp_cstate_none_e;
	  dhcp->timer = timer_callout_init();
	  if (dhcp->timer == NULL) {
	      my_log(LOG_ERR, "DHCP %s: timer_callout_init failed", 
		     if_name(ifstate->if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  (void)inet_enable_autoaddr(ifstate);
	  dhcp->client = bootp_client_init(G_bootp_session);
	  if (dhcp->client == NULL) {
	      my_log(LOG_ERR, "DHCP %s: bootp_client_init failed",
		     if_name(ifstate->if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  dhcp->arp = arp_client_init(G_arp_session, ifstate->if_p);
	  if (dhcp->arp == NULL) {
	      my_log(LOG_ERR, "DHCP %s: arp_client_init failed", 
		     if_name(ifstate->if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  if (ipcfg->n_dhcp_client_id) {
	      void * 		cid;

	      dhcp->client_id_len = ipcfg->n_dhcp_client_id;
	      dhcp->client_id = malloc(dhcp->client_id_len);
	      if (dhcp->client_id == NULL) {
		  my_log(LOG_ERR, "DHCP %s: malloc client ID failed", 
			 if_name(ifstate->if_p));
		  status = ipconfig_status_allocation_failed_e;
		  goto stop;
	      }
	      cid = ((void *)ipcfg->ip) + ipcfg->n_ip * sizeof(ipcfg->ip[0]);
	      bcopy(cid, dhcp->client_id, dhcp->client_id_len);
	  }

	  dhcp->idstr = identifierToString(if_link_arptype(ifstate->if_p), 
					   if_link_address(ifstate->if_p), 
					   if_link_length(ifstate->if_p));
	  if (dhcp->idstr == NULL) {
	      my_log(LOG_ERR, "DHCP %s: malloc device ID string failed", 
		     if_name(ifstate->if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto stop;
	  }
	  dhcpol_init(&dhcp->saved.options);
	  my_log(LOG_DEBUG, "DHCP %s: H/W %s start", 
		 if_name(ifstate->if_p), dhcp->idstr);
	  dhcp->start_secs = current_time;
	  dhcp->xid = random();
	  dhcpol_init(&dhcp->saved.options);
	  /* use the previous lease if it exists */
	  if (dhcp_lease_read(dhcp->idstr, &our_ip)) {
	      /* try the same address if we had a lease at some point */
	      dhcp_init_reboot(ifstate, IFEventID_start_e, &our_ip);
	      break;
	  }
	  dhcp_init(ifstate, IFEventID_start_e, NULL);
	  break;
      }
      case IFEventID_stop_e: {
      stop:
	  my_log(LOG_DEBUG, "DHCP %s: stop", if_name(ifstate->if_p));
	  if (dhcp == NULL) {
	      my_log(LOG_DEBUG, "DHCP %s: already stopped", 
		     if_name(ifstate->if_p));
	      status = ipconfig_status_internal_error_e; /* shouldn't happen */
	      break;
	  }
	  if (event_id == IFEventID_stop_e) {
	      (void)dhcp_release(ifstate);
	  }

	  /* remove IP address(es) */
	  ifstate_remove_addresses(ifstate);

	  inet_disable_autoaddr(ifstate);

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
	  if (dhcp->idstr) {
	      free(dhcp->idstr);
	      dhcp->idstr = NULL;
	  }
	  if (dhcp->client_id) {
	      free(dhcp->client_id);
	      dhcp->client_id = NULL;
	  }
	  dhcpol_free(&dhcp->saved.options);
	  if (dhcp)
	      free(dhcp);
	  ifstate->private = NULL;
	  break;
      }
      case IFEventID_change_e: {
	  void *			cid;
	  change_event_data_t *   	evdata = ((change_event_data_t *)event_data);
	  ipconfig_method_data_t *	ipcfg = evdata->config.data;

	  if (dhcp == NULL) {
	      my_log(LOG_DEBUG, "DHCP %s: private data is NULL", 
		     if_name(ifstate->if_p));
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
      case IFEventID_media_e: {
	  struct in_addr	our_ip;

	  if (dhcp == NULL)
	      return (ipconfig_status_internal_error_e);
	  our_ip = dhcp->saved.our_ip;
	  if (ifstate->link.valid == TRUE) {
	      if (ifstate->link.active == TRUE) {
		  dhcp->in_use = dhcp->user_warned = FALSE;
		  if (our_ip.s_addr) {
		      if (dhcp->lease_is_infinite 
			  || current_time < dhcp->lease_expiration
			  || dhcp->lease_expiration == 0) {
			  /* try same address if there's time left on the lease */
			  dhcp_init_reboot(ifstate, IFEventID_start_e, &our_ip);
			  break;
		      }
		      (void)inet_remove(ifstate, our_ip);
		  }
		  dhcp_init(ifstate, IFEventID_start_e, NULL);
	      }
	      else {
		  struct timeval tv;

		  /* if link goes down and stays down long enough, unpublish */
		  dhcp_cancel_pending_events(ifstate);
		  tv.tv_sec = G_link_inactive_secs;
		  tv.tv_usec = 0;
		  timer_set_relative(dhcp->timer, tv, 
				     (timer_func_t *)dhcp_link_timer,
				     ifstate, NULL, NULL);
	      }
	  }
	  break;
      }
      case IFEventID_renew_e: {
	  struct in_addr our_ip;

	  if (dhcp == NULL)
	      return (ipconfig_status_internal_error_e);

	  if (ifstate->link.valid == TRUE && ifstate->link.active == TRUE) {
	      dhcp->in_use = dhcp->user_warned = FALSE;
	      our_ip = dhcp->saved.our_ip;
	      if (our_ip.s_addr) {
		  if (dhcp->lease_is_infinite 
		      || current_time < dhcp->lease_expiration
		      || dhcp->lease_expiration == 0) {
		      /* try same address if there's time left on the lease */
		      dhcp_init_reboot(ifstate, IFEventID_start_e, &our_ip);
		      break;
		  }
		  (void)inet_remove(ifstate, our_ip);
	      }
	      dhcp_init(ifstate, IFEventID_start_e, NULL);
	  }
	  break;
      }
      default:
	  break;
    } /* switch (event_id) */
    return (status);
}

static void
dhcp_init(IFState_t * ifstate, IFEventID_t event_id, void * event_data)
{
    IFState_dhcp_t *	dhcp = (IFState_dhcp_t *)ifstate->private;
    ipconfig_status_t	status = ipconfig_status_success_e;
    struct timeval 	tv;

    switch (event_id) {
      case IFEventID_start_e: {
	  dhcp_lease_t 	lease_option = dhcp_lease_hton(SUGGESTED_LEASE_LENGTH);
	  dhcpoa_t	options;

	  dhcp->state = dhcp_cstate_init_e;

	  /* clean-up anything that might have come before */
	  dhcp_cancel_pending_events(ifstate);
	  
	  /* form the request */
	  dhcp->request = make_dhcp_request((struct dhcp *)dhcp->txbuf, 
					    sizeof(dhcp->txbuf),
					    dhcp_msgtype_discover_e,
					    if_link_address(ifstate->if_p), 
					    if_link_arptype(ifstate->if_p),
					    if_link_length(ifstate->if_p),
					    dhcp->client_id, dhcp->client_id_len,
					    &options);
	  if (dhcp->request == NULL) {
	      my_log(LOG_ERR, "DHCP %s: INIT make_dhcp_request failed",
		     if_name(ifstate->if_p));
	  }
	  if (dhcpoa_add(&options, dhcptag_lease_time_e,
			 sizeof(lease_option), &lease_option) 
	      != dhcpoa_success_e) {
	      my_log(LOG_ERR, "DHCP %s: INIT dhcpoa_add lease time failed, %s", 
		     if_name(ifstate->if_p), dhcpoa_err(&options));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  add_computer_name(&options);
	  if (dhcpoa_add(&options, dhcptag_end_e, 0, 0)
	      != dhcpoa_success_e) {
	      my_log(LOG_ERR, "DHCP %s: INIT failed to terminate options",
		     if_name(ifstate->if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  dhcp->wait_secs = INITIAL_WAIT_SECS;
	  dhcp->request_size = sizeof(*dhcp->request) + sizeof(G_rfc_magic) 
	      + dhcpoa_used(&options);
	  if (dhcp->request_size < sizeof(struct bootp)) {
	      /* pad out to BOOTP-sized packet */
	      dhcp->request_size = sizeof(struct bootp);
	  }
	  dhcp->try = 0;
	  dhcp->gathering = FALSE;
	  dhcpol_free(&dhcp->saved.options);
	  bzero(&dhcp->saved, sizeof(dhcp->saved));
	  (void)inet_enable_autoaddr(ifstate);
	  bootp_client_enable_receive(dhcp->client,
				      (bootp_receive_func_t *)dhcp_init, 
				      ifstate, (void *)IFEventID_data_e);
	  /* FALL THROUGH */
      }
      case IFEventID_timeout_e: {
	  if (dhcp->gathering == TRUE) {
	      /* done gathering */
	      if (dhcp->saved.is_dhcp) {
		  dhcp_select(ifstate, IFEventID_start_e, NULL);
		  break; /* out of switch */
	      }
	      dhcp_bound(ifstate, IFEventID_start_e, NULL);
	      break; /* out of switch */
	  }
	  dhcp->try++;
	  if (dhcp->try > 1) {
	      if (ifstate->link.valid && ifstate->link.active == FALSE) {
		  dhcp_inactive(ifstate);
		  break;
	      }
	  }
	  if (dhcp->try > (G_max_retries + 1)) {
	      /* find an ad hoc address */
	      dhcp_ad_hoc(ifstate, IFEventID_start_e, NULL);
	      break; /* out of switch */
	  }
	  dhcp->request->dp_xid = htonl(++dhcp->xid);
#ifdef DEBUG
	  dhcp_print_packet(dhcp->request, dhcp->request_size);
#endif DEBUG
	  /* send the packet */
	  if (bootp_client_transmit(dhcp->client, if_name(ifstate->if_p),
				    dhcp->request->dp_htype, NULL, 0,
				    G_ip_broadcast, G_ip_zeroes,
				    G_server_port, G_client_port,
				    dhcp->request, dhcp->request_size) < 0) {
	      my_log(LOG_ERR, 
		     "DHCP %s: INIT transmit failed", if_name(ifstate->if_p));
	  }
	  /* wait for responses */
	  tv.tv_sec = dhcp->wait_secs;
	  tv.tv_usec = random_range(0, USECS_PER_SEC - 1);
	  my_log(LOG_DEBUG, "DHCP %s: INIT waiting at %d for %d.%06d", 
		 if_name(ifstate->if_p), 
		 timer_current_secs() - dhcp->start_secs,
		 tv.tv_sec, tv.tv_usec);
	  timer_set_relative(dhcp->timer, tv, 
			     (timer_func_t *)dhcp_init,
			     ifstate, (void *)IFEventID_timeout_e, NULL);
	  /* next time wait twice as long */
	  dhcp->wait_secs *= 2;
	  if (dhcp->wait_secs > MAX_WAIT_SECS)
	      dhcp->wait_secs = MAX_WAIT_SECS;
	  break;
      }
      case IFEventID_data_e: {
	  boolean_t 		is_dhcp = TRUE;
	  dhcp_lease_t 		lease;
	  bootp_receive_data_t *pkt = (bootp_receive_data_t *)event_data;
	  dhcp_msgtype_t	reply_msgtype = dhcp_msgtype_none_e;
	  struct in_addr	server_ip;

	  if (verify_packet(pkt, dhcp->xid, ifstate->if_p, &reply_msgtype,
			    &server_ip, &is_dhcp) == FALSE
	      || server_ip.s_addr == 0
	      || ip_valid(pkt->data->dp_yiaddr) == FALSE) {
	      /* reject the packet */
	      break; /* out of switch */
	  }
	  if (is_dhcp == FALSE
	      || (reply_msgtype == dhcp_msgtype_offer_e
		  && get_lease(&pkt->options, &lease))) {
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
		  dhcp_set_lease_params(ifstate, "INIT", is_dhcp, lease);
		  if (is_dhcp && rating == n_dhcp_params) {
		      dhcp->state = dhcp_cstate_select_e;
		      dhcp_select(ifstate, IFEventID_start_e, NULL);
		      break; /* out of switch */
		  }
		  if (dhcp->gathering == FALSE) {
		      struct timeval t = {0,0};
		      t.tv_sec = G_gather_secs;
		      my_log(LOG_DEBUG, "DHCP %s: INIT gathering began at %d", 
			     if_name(ifstate->if_p), 
			     timer_current_secs() - dhcp->start_secs);
		      dhcp->gathering = TRUE;
		      timer_set_relative(dhcp->timer, t, 
					 (timer_func_t *)dhcp_init,
					 ifstate, (void *)IFEventID_timeout_e, 
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
    dhcp_failed(ifstate, status, "INIT");
    return;
}


static void
dhcp_init_reboot(IFState_t * ifstate, IFEventID_t evid, void * event_data)
{
    IFState_dhcp_t *	dhcp = (IFState_dhcp_t *)ifstate->private;
    ipconfig_status_t	status = ipconfig_status_success_e;
    struct timeval 	tv;

    switch (evid) {
      case IFEventID_start_e: {
	  struct in_addr our_ip = *((struct in_addr *)event_data);
	  dhcp_lease_t 	 lease_option = dhcp_lease_hton(SUGGESTED_LEASE_LENGTH);
	  dhcpoa_t	 options;

	  dhcp->state = dhcp_cstate_init_reboot_e;

	  /* clean-up anything that might have come before */
	  dhcp_cancel_pending_events(ifstate);

	  /* form the request */
	  dhcp->request = make_dhcp_request((struct dhcp *)dhcp->txbuf, 
					    sizeof(dhcp->txbuf), 
					    dhcp_msgtype_request_e,
					    if_link_address(ifstate->if_p), 
					    if_link_arptype(ifstate->if_p),
					    if_link_length(ifstate->if_p), 
					    dhcp->client_id, dhcp->client_id_len,
					    &options);
	  if (dhcp->request == NULL) {
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  if (dhcpoa_add(&options, dhcptag_requested_ip_address_e,
			 sizeof(our_ip), &our_ip) 
	      != dhcpoa_success_e) {
	      my_log(LOG_ERR, "DHCP %s: INIT-REBOOT add request ip failed, %s", 
		     if_name(ifstate->if_p), dhcpoa_err(&options));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  if (dhcpoa_add(&options, dhcptag_lease_time_e,
			 sizeof(lease_option), &lease_option) 
	      != dhcpoa_success_e) {
	      my_log(LOG_ERR, "DHCP %s: INIT-REBOOT add lease time failed, %s", 
		     if_name(ifstate->if_p), dhcpoa_err(&options));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  add_computer_name(&options);
	  if (dhcpoa_add(&options, dhcptag_end_e, 0, 0)
	      != dhcpoa_success_e) {
	      my_log(LOG_ERR, "DHCP %s: INIT-REBOOT failed to terminate options",
		     if_name(ifstate->if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  dhcp->wait_secs = INITIAL_WAIT_SECS;
	  dhcp->request_size = sizeof(*dhcp->request) + sizeof(G_rfc_magic) 
	      + dhcpoa_used(&options);
	  if (dhcp->request_size < sizeof(struct bootp)) {
	      /* pad out to BOOTP-sized packet */
	      dhcp->request_size = sizeof(struct bootp);
	  }
	  dhcp->try = 0;
	  dhcp->gathering = FALSE;
	  dhcpol_free(&dhcp->saved.options);
	  bzero(&dhcp->saved, sizeof(dhcp->saved));
	  dhcp->saved.our_ip = our_ip;
	  (void)inet_enable_autoaddr(ifstate);
	  bootp_client_enable_receive(dhcp->client,
				      (bootp_receive_func_t *)dhcp_init_reboot, 
				      ifstate, (void *)IFEventID_data_e);
	  /* FALL THROUGH */
      }
      case IFEventID_timeout_e: {
	  if (dhcp->gathering == TRUE) {
	      /* done gathering */
	      dhcp_bound(ifstate, IFEventID_start_e, NULL);
	      break; /* out of switch */
	  }
	  dhcp->try++;
	  if (dhcp->try > 1) {
	      if (ifstate->link.valid && ifstate->link.active == FALSE) {
		  dhcp_inactive(ifstate);
		  break;
	      }
	  }
#define INIT_REBOOT_TRIES	2
	  if (dhcp->try > INIT_REBOOT_TRIES) {
	      my_log(LOG_DEBUG, "DHCP %s: INIT-REBOOT timed out", 
		     if_name(ifstate->if_p));
	      /* go back to the INIT state */
	      dhcp_failed(ifstate, ipconfig_status_no_server_e, 
			  "INIT-REBOOT");
	      dhcp_init(ifstate, IFEventID_start_e, NULL);
	      break; /* ouf of case */
	  }
	  dhcp->request->dp_xid = htonl(++dhcp->xid);
#ifdef DEBUG
	  dhcp_print_packet(dhcp->request, dhcp->request_size);
#endif DEBUG
	  /* send the packet */
	  if (bootp_client_transmit(dhcp->client, if_name(ifstate->if_p),
				    dhcp->request->dp_htype, NULL, 0,
				    G_ip_broadcast, G_ip_zeroes,
				    G_server_port, G_client_port,
				    dhcp->request, dhcp->request_size) < 0) {
	      my_log(LOG_ERR, 
		     "DHCP %s: INIT-REBOT transmit failed", 
		     if_name(ifstate->if_p));
	  }
	  /* wait for responses */
	  tv.tv_sec = dhcp->wait_secs;
	  tv.tv_usec = random_range(0, USECS_PER_SEC - 1);
	  my_log(LOG_DEBUG, "DHCP %s: INIT-REBOOT waiting at %d for %d.%06d", 
		 if_name(ifstate->if_p), 
		 timer_current_secs() - dhcp->start_secs,
		 tv.tv_sec, tv.tv_usec);
	  timer_set_relative(dhcp->timer, tv, 
			     (timer_func_t *)dhcp_init_reboot,
			     ifstate, (void *)IFEventID_timeout_e, NULL);
	  /* next time wait twice as long */
	  dhcp->wait_secs *= 2;
	  if (dhcp->wait_secs > MAX_WAIT_SECS)
	      dhcp->wait_secs = MAX_WAIT_SECS;
	  break;
      }
      case IFEventID_data_e: {
	  boolean_t 		is_dhcp = TRUE;
	  dhcp_lease_t 		lease;
	  bootp_receive_data_t *pkt = (bootp_receive_data_t *)event_data;
	  dhcp_msgtype_t	reply_msgtype = dhcp_msgtype_none_e;
	  struct in_addr	server_ip;

	  if (verify_packet(pkt, dhcp->xid, ifstate->if_p, &reply_msgtype,
			    &server_ip, &is_dhcp) == FALSE) {
	      /* reject the packet */
	      break; /* out of switch */
	  }
	  if (reply_msgtype == dhcp_msgtype_nak_e) {
	      dhcp_unbound(ifstate, IFEventID_start_e, NULL);
	      break; /* out of switch */
	  }
	  if (server_ip.s_addr == 0
	      || pkt->data->dp_yiaddr.s_addr != dhcp->saved.our_ip.s_addr) {
	      /* reject the packet */
	      break; /* out of switch */
	  }
	  if (is_dhcp == FALSE  
	      || (reply_msgtype == dhcp_msgtype_ack_e
		  && get_lease(&pkt->options, &lease))) {
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
	  
		  /* need to check whether t1,t2 overrided by server XXX */
		  dhcp_set_lease_params(ifstate, "INIT-REBOOT", is_dhcp, 
					lease);

		  if (is_dhcp && rating == n_dhcp_params) {
		      dhcp_bound(ifstate, IFEventID_start_e, NULL);
		      break; /* out of switch */
		  }
		  if (dhcp->gathering == FALSE) {
		      struct timeval t = {0,0};
		      t.tv_sec = G_gather_secs;
		      my_log(LOG_DEBUG, 
			     "DHCP %s: INIT-REBOOT gathering began at %d", 
			     if_name(ifstate->if_p), 
			     timer_current_secs() - dhcp->start_secs);
		      dhcp->gathering = TRUE;
		      timer_set_relative(dhcp->timer, t, 
					 (timer_func_t *)dhcp_init_reboot,
					 ifstate, (void *)IFEventID_timeout_e, 
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
    dhcp_failed(ifstate, status, "INIT-REBOOT");
    return;
}

static void
dhcp_select(IFState_t * ifstate, IFEventID_t evid, void * event_data)
{
    IFState_dhcp_t *	dhcp = (IFState_dhcp_t *)ifstate->private;
    ipconfig_status_t	status = ipconfig_status_success_e;
    struct timeval 	tv;

    switch (evid) {
      case IFEventID_start_e: {
	  dhcpoa_t	 	options;

	  /* clean-up anything that might have come before */
	  dhcp_cancel_pending_events(ifstate);

	  dhcp->state = dhcp_cstate_select_e;

	  /* form the request */
	  dhcp->request = make_dhcp_request((struct dhcp *)dhcp->txbuf, 
					    sizeof(dhcp->txbuf),
					    dhcp_msgtype_request_e,
					    if_link_address(ifstate->if_p), 
					    if_link_arptype(ifstate->if_p),
					    if_link_length(ifstate->if_p), 
					    dhcp->client_id, dhcp->client_id_len,
					    &options);
	  if (dhcp->request == NULL) {
	      my_log(LOG_ERR, "DHCP %s: SELECT make_dhcp_request failed",
		     if_name(ifstate->if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  /* insert server identifier and requested ip address */
	  if (dhcpoa_add(&options, dhcptag_requested_ip_address_e,
			 sizeof(dhcp->saved.our_ip), &dhcp->saved.our_ip)
	      != dhcpoa_success_e) {
	      my_log(LOG_ERR, "DHCP %s: SELECT add requested ip failed, %s", 
		     if_name(ifstate->if_p), dhcpoa_err(&options));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  if (dhcpoa_add(&options, dhcptag_server_identifier_e,
			 sizeof(dhcp->saved.server_ip), &dhcp->saved.server_ip)
	      != dhcpoa_success_e) {
	      my_log(LOG_ERR, "DHCP %s: SELECT add server ip failed, %s", 
		     if_name(ifstate->if_p), dhcpoa_err(&options));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  add_computer_name(&options);
	  if (dhcpoa_add(&options, dhcptag_end_e, 0, 0)
	      != dhcpoa_success_e) {
	      my_log(LOG_ERR, "DHCP %s: SELECT failed to terminate options",
		     if_name(ifstate->if_p));
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
	  dhcp->wait_secs = INITIAL_WAIT_SECS;
	  bootp_client_enable_receive(dhcp->client,
				      (bootp_receive_func_t *)dhcp_select, 
				      ifstate, (void *)IFEventID_data_e);
      }
      case IFEventID_timeout_e: {
	  dhcp->try++;
#define SELECT_RETRIES	1
	  if (dhcp->try > (SELECT_RETRIES + 1)) {
	      my_log(LOG_DEBUG, "DHCP %s: SELECT timed out", 
		     if_name(ifstate->if_p), 
		     timer_current_secs() - dhcp->start_secs,
		     tv.tv_sec, tv.tv_usec);
	      /* go back to INIT and try again */
	      dhcp_init(ifstate, IFEventID_start_e, NULL);
	      break; /* out of switch */
	  }
	  dhcp->request->dp_xid = htonl(dhcp->xid);
#ifdef DEBUG
	  dhcp_print_packet(dhcp->request, dhcp->request_size);
#endif DEBUG
	  /* send the packet */
	  if (bootp_client_transmit(dhcp->client, if_name(ifstate->if_p),
				    dhcp->request->dp_htype, NULL, 0,
				    G_ip_broadcast, G_ip_zeroes,
				    G_server_port, G_client_port,
				    dhcp->request, dhcp->request_size) < 0) {
	      my_log(LOG_ERR, 
		     "DHCP %s: SELECT transmit failed", 
		     if_name(ifstate->if_p));
	  }
	  /* wait for responses */
	  tv.tv_sec = dhcp->wait_secs;
	  tv.tv_usec = random_range(0, USECS_PER_SEC - 1);
	  my_log(LOG_DEBUG, "DHCP %s: SELECT waiting at %d for %d.%06d", 
		 if_name(ifstate->if_p), 
		 timer_current_secs() - dhcp->start_secs,
		 tv.tv_sec, tv.tv_usec);
	  timer_set_relative(dhcp->timer, tv, 
			     (timer_func_t *)dhcp_select,
			     ifstate, (void *)IFEventID_timeout_e, NULL);
	  /* next time wait twice as long */
	  dhcp->wait_secs *= 2;
	  if (dhcp->wait_secs > MAX_WAIT_SECS)
	      dhcp->wait_secs = MAX_WAIT_SECS;
	  break;
      }
      case IFEventID_data_e: {
	  boolean_t 		is_dhcp = TRUE;
	  dhcp_lease_t 		lease = SUGGESTED_LEASE_LENGTH;
	  bootp_receive_data_t *pkt = (bootp_receive_data_t *)event_data;
	  dhcp_msgtype_t	reply_msgtype = dhcp_msgtype_none_e;
	  struct in_addr	server_ip;

	  if (verify_packet(pkt, dhcp->xid, ifstate->if_p, &reply_msgtype,
			    &server_ip, &is_dhcp) == FALSE
	      || server_ip.s_addr == 0
	      || is_dhcp == FALSE) {
	      /* reject the packet */
	      break; /* out of switch */
	  }

	  if (reply_msgtype == dhcp_msgtype_nak_e) {
	      /* clean-up anything that might have come before */
	      dhcp_cancel_pending_events(ifstate);

	      /* 
	       * wait to retry INIT just in case there's a misbehaving server
	       * and we get stuck in an INIT-SELECT-NAK infinite loop
	       */
	      tv.tv_sec = INITIAL_WAIT_SECS * 4;
	      tv.tv_usec = 0;
	      timer_set_relative(dhcp->timer, tv, 
				 (timer_func_t *)dhcp_init,
				 ifstate, (void *)IFEventID_start_e, NULL);
	      break; /* out of switch */
	  }
	  if (reply_msgtype != dhcp_msgtype_ack_e
	      || ip_valid(pkt->data->dp_yiaddr) == FALSE
	      || get_lease(&pkt->options, &lease) == FALSE) {
	      /* reject the packet */
	      break; /* out of switch */
	  }

	  /* need to check whether t1,t2 overrided by server XXX */
	  dhcp_set_lease_params(ifstate, "SELECT", is_dhcp, lease);

	  dhcpol_free(&dhcp->saved.options);
	  bcopy(pkt->data, dhcp->saved.pkt, pkt->size);
	  dhcp->saved.pkt_size = pkt->size;
	  dhcp->saved.rating = 0;
	  (void)dhcpol_parse_packet(&dhcp->saved.options, 
				    (void *)dhcp->saved.pkt, 
				    dhcp->saved.pkt_size, NULL);
	  dhcp->saved.our_ip = pkt->data->dp_yiaddr;
	  dhcp->saved.server_ip = server_ip;
	  dhcp->saved.is_dhcp = TRUE;
	  dhcp_bound(ifstate, IFEventID_start_e, NULL);
	  break;
      }
      default:
	  break;
    }
    return;

 error:
    dhcp_failed(ifstate, status, "SELECT");
    return;
}

static void
dhcp_bound(IFState_t * ifstate, IFEventID_t event_id, void * event_data)
{
    IFState_dhcp_t *	dhcp = (IFState_dhcp_t *)ifstate->private;
    int			len;
    struct in_addr	mask = {0};
    boolean_t		renewing = FALSE;
    void *		option;
    struct timeval 	tv = {0, 0};

    switch (event_id) {
      case IFEventID_start_e: {
	  if (dhcp->state == dhcp_cstate_renew_e
	      || dhcp->state == dhcp_cstate_rebind_e) {
	      renewing = TRUE;
	      dhcp->state = dhcp_cstate_bound_e;
	      break; /* out of switch */
	  }
	  dhcp->state = dhcp_cstate_bound_e;

	  /* clean-up anything that might have come before */
	  dhcp_cancel_pending_events(ifstate);

	  /* do an ARP probe of the supplied address */
	  arp_probe(dhcp->arp, 
		    (arp_result_func_t *)dhcp_bound, ifstate,
		    (void *)IFEventID_arp_e, G_ip_zeroes,
		    dhcp->saved.our_ip);
	  return;
	  break;
	}
	case IFEventID_arp_e: {
	  arp_result_t *	result = (arp_result_t *)event_data;

	  if (result->error) {
	      my_log(LOG_ERR, "DHCP %s: ARP probe failed, %s", 
		     if_name(ifstate->if_p),
		     arp_client_errmsg(dhcp->arp));
	      /* continue anyways */
	  }
	  else if (result->in_use) {
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
		      ifstate_tell_user(ifstate, msg);
		      dhcp->user_warned = TRUE;
		  }
	      }
	      dhcp->in_use = TRUE;
	      syslog(LOG_ERR, "DHCP %s: %s", if_name(ifstate->if_p), msg);
	      dhcp_lease_clear(dhcp->idstr);
	      ifstate_publish_failure(ifstate, 
				      ipconfig_status_address_in_use_e, msg);
	      if (dhcp->saved.is_dhcp) {
		  dhcp_decline(ifstate, IFEventID_start_e, NULL);
		  return;
	      }
	      dhcp_cancel_pending_events(ifstate);
	      (void)inet_disable_autoaddr(ifstate);
	      dhcp->saved.our_ip.s_addr = 0;
	      tv.tv_sec = 10; /* retry in a bit */
	      tv.tv_usec = 0;
	      timer_set_relative(dhcp->timer, tv, 
				 (timer_func_t *)dhcp_init,
				 ifstate, (void *)IFEventID_start_e, NULL);
	      return;
	  }
	  break;
	}
	default:
	  return;
    }

    /* don't update the lease file if we're renewing the lease */
    if (renewing == FALSE) {
	if (dhcp->saved.is_dhcp) {
	    /* only bother to save a lease if it came from a DHCP server */
	    (void)dhcp_lease_write(dhcp->idstr, dhcp->saved.our_ip);
	}
	else {
	    dhcp_lease_clear(dhcp->idstr);
	}
    }

    /* allow user warning to appear */
    dhcp->in_use = dhcp->user_warned = FALSE;

    /* set the interface's address and output the status */
    option = dhcpol_find(&dhcp->saved.options, dhcptag_subnet_mask_e, 
			 &len, NULL);
    if (option) {
	mask = *((struct in_addr *)option);
    }

    inet_disable_autoaddr(ifstate);
	      
    /* set our new address */
    (void)inet_add(ifstate, dhcp->saved.our_ip, &mask, NULL);
    ifstate_publish_success(ifstate, dhcp->saved.pkt, dhcp->saved.pkt_size);
    if (dhcp->lease_is_infinite == TRUE) {
	/* don't need to talk to server anymore */
	my_log(LOG_DEBUG, "DHCP %s: infinite lease", 
	       if_name(ifstate->if_p));
	/* clean-up anything that might have come before */
	dhcp_cancel_pending_events(ifstate);
	return;
    }

    dhcp_cancel_pending_events(ifstate);
    /* wake up in RENEW state at t1 */
    tv.tv_sec = dhcp->t1 - timer_current_secs();
    tv.tv_usec = 0;
    timer_set_relative(dhcp->timer, tv, 
		       (timer_func_t *)dhcp_renew_rebind,
		       ifstate, (void *)IFEventID_start_e, NULL);
    return;
}

static void
dhcp_ad_hoc(IFState_t * ifstate, IFEventID_t event_id, void * event_data)
{
    IFState_dhcp_t *	dhcp = (IFState_dhcp_t *)ifstate->private;
    struct timeval 	tv;
    
    switch (event_id) {
      case IFEventID_start_e: {
	  int			i;
	  long			range;

	  dhcp->state = dhcp_cstate_none_e;

	  /* clean-up anything that might have come before */
	  dhcp_cancel_pending_events(ifstate);
	  
	  range = (AD_HOC_LAST_USEABLE + 1) - AD_HOC_FIRST_USEABLE;
	      
	  if (interface_is_ad_hoc(ifstate->if_p)) {
	      ifstate_publish_success(ifstate, NULL, 0);
	      break; /* out of switch */
	  }

	  /* populate an array of unique random numbers */
	  for (i = 0; i < MAX_AD_HOC_TRIES; ) {
	      int		j;
	      long 		r;
	      
	      r = random_range(0, range);
	      for (j = 0; j < i; j++) {
		  if (dhcp->ad_hoc.offset[j] == r)
		      continue;
	      }
	      dhcp->ad_hoc.offset[i++] = r;
	  }
	  dhcp->ad_hoc.current = 0;
	  dhcp->ad_hoc.probe.s_addr 
	      = htonl(AD_HOC_FIRST_USEABLE 
		      + dhcp->ad_hoc.offset[dhcp->ad_hoc.current]);
	  my_log(LOG_DEBUG, "DHCP %s probing " IP_FORMAT, 
		 if_name(ifstate->if_p), IP_LIST(&dhcp->ad_hoc.probe));
	  arp_probe(dhcp->arp, 
		    (arp_result_func_t *)dhcp_ad_hoc, ifstate,
		    (void *)IFEventID_arp_e, G_ip_zeroes,
		    dhcp->ad_hoc.probe);
	  /* wait for the results */
	  return;
      }
      case IFEventID_arp_e: {
	  arp_result_t *	result = (arp_result_t *)event_data;

	  if (result->error) {
	      my_log(LOG_DEBUG, "DHCP %s: ARP probe failed, %s", 
		     if_name(ifstate->if_p),
		     arp_client_errmsg(dhcp->arp));
	  }
	  else if (result->in_use) {
	      my_log(LOG_DEBUG, "DHCP %s: IP address " 
		     IP_FORMAT " is in use by " EA_FORMAT, 
		     if_name(ifstate->if_p), 
		     IP_LIST(&dhcp->ad_hoc.probe),
		     EA_LIST(result->hwaddr));
	  }
	  else {
	      const struct in_addr ad_hoc_mask = { htonl(AD_HOC_MASK) };
	      /* ad-hoc IP address is not in use, so use it */
	      if (inet_add(ifstate, dhcp->ad_hoc.probe, &ad_hoc_mask, 
			   NULL) == EEXIST) {
		  /* some other interface is already ad hoc */
		  (void)inet_remove(ifstate, dhcp->ad_hoc.probe);
		  /* wake/unblock anyone waiting */
		  ifstate_publish_failure(ifstate, ipconfig_status_no_server_e,
					  NULL);
	      }
	      else {
		  ifstate_publish_success(ifstate, NULL, 0);
	      }
	      /* we're done */
	      break; /* out of switch */
	  }
	  /* try the next address */
	  dhcp->ad_hoc.current++;
	  if (dhcp->ad_hoc.current >= MAX_AD_HOC_TRIES) {
	      ifstate_publish_failure(ifstate, ipconfig_status_no_server_e,
				      NULL);
	      /* we're done */
	      break; /* out of switch */
	  }
	  dhcp->ad_hoc.probe.s_addr 
	      = htonl(AD_HOC_FIRST_USEABLE 
		      + dhcp->ad_hoc.offset[dhcp->ad_hoc.current]);
	  arp_probe(dhcp->arp, 
		    (arp_result_func_t *)dhcp_ad_hoc, ifstate,
		    (void *)IFEventID_arp_e, G_ip_zeroes,
		    dhcp->ad_hoc.probe);
	  my_log(LOG_DEBUG, "DHCP %s probing " IP_FORMAT, 
		 if_name(ifstate->if_p), IP_LIST(&dhcp->ad_hoc.probe));
	  /* wait for the results */
	  return;
      }
      default:
	  break;
    }

    /*  we're done trying to configure ad hoc networking */
    dhcp_cancel_pending_events(ifstate);
#define INIT_RETRY_INTERVAL_SECS      (5 * 60)
    tv.tv_sec = INIT_RETRY_INTERVAL_SECS;
    tv.tv_usec = 0;
    /* wake up in INIT state after a period of waiting */
    timer_set_relative(dhcp->timer, tv, 
		       (timer_func_t *)dhcp_init,
		       ifstate, (void *)IFEventID_start_e, NULL);
    return;
}

static void
dhcp_decline(IFState_t * ifstate, IFEventID_t event_id, void * event_data)
{
    IFState_dhcp_t *	dhcp = (IFState_dhcp_t *)ifstate->private;
    ipconfig_status_t	status = ipconfig_status_success_e;
    struct timeval 	tv;

    switch (event_id) {
      case IFEventID_start_e: {
	  dhcpoa_t	 	options;

	  /* clean-up anything that might have come before */
	  dhcp_cancel_pending_events(ifstate);

	  /* decline the address */
	  dhcp->state = dhcp_cstate_decline_e;
	  dhcp->request = make_dhcp_request((struct dhcp *)dhcp->txbuf, 
					    sizeof(dhcp->txbuf),
					    dhcp_msgtype_decline_e,
					    if_link_address(ifstate->if_p), 
					    if_link_arptype(ifstate->if_p),
					    if_link_length(ifstate->if_p), 
					    dhcp->client_id, dhcp->client_id_len,
					    &options);
	  if (dhcp->request == NULL) {
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  if (dhcpoa_add(&options, dhcptag_requested_ip_address_e,
			 sizeof(dhcp->saved.our_ip), &dhcp->saved.our_ip) 
	      != dhcpoa_success_e) {
	      my_log(LOG_ERR, "DHCP %s: DECLINE couldn't add our ip, %s",
		     if_name(ifstate->if_p), dhcpoa_err(&options));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  if (dhcpoa_add(&options, dhcptag_server_identifier_e,
			 sizeof(dhcp->saved.server_ip), &dhcp->saved.server_ip)
	      != dhcpoa_success_e) {
	      my_log(LOG_ERR, "DHCP %s: DECLINE couldn't add server ip, %s",
		     if_name(ifstate->if_p), dhcpoa_err(&options));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  if (dhcpoa_add(&options, dhcptag_end_e, 0, 0)
	      != dhcpoa_success_e) {
	      my_log(LOG_ERR, "DHCP %s: DECLINE failed to terminate options",
		     if_name(ifstate->if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  if (bootp_client_transmit(dhcp->client, if_name(ifstate->if_p),
				    dhcp->request->dp_htype, NULL, 0,
				    G_ip_broadcast, G_ip_zeroes,
				    G_server_port, G_client_port,
				    dhcp->request, dhcp->request_size) < 0) {
	      my_log(LOG_ERR, 
		     "DHCP %s: DECLINE transmit failed", 
		     if_name(ifstate->if_p));
	  }
	  (void)inet_remove(ifstate, dhcp->saved.our_ip);
	  dhcp->saved.our_ip.s_addr = 0;
	  inet_disable_autoaddr(ifstate);
	  tv.tv_sec = 10; /* retry in a bit */
	  tv.tv_usec = 0;
	  timer_set_relative(dhcp->timer, tv, 
			     (timer_func_t *)dhcp_init,
			     ifstate, (void *)IFEventID_start_e, NULL);
	  break;
      }
      default:
	  break;
    }
    return;
 error:
    dhcp_failed(ifstate, status, "DECLINE");
    return;
}

static void
dhcp_unbound(IFState_t * ifstate, IFEventID_t event_id, void * event_data)
{
    IFState_dhcp_t *	dhcp = (IFState_dhcp_t *)ifstate->private;
    struct timeval 	tv = {0,0};

    switch (event_id) {
      case IFEventID_start_e: {
	  /* clean-up anything that might have come before */
	  dhcp_cancel_pending_events(ifstate);

	  dhcp->state = dhcp_cstate_unbound_e;
	  /* stop using the IP address immediately */
	  (void)inet_remove(ifstate, dhcp->saved.our_ip);
	  dhcp->saved.our_ip.s_addr = 0;

	  dhcp_lease_clear(dhcp->idstr);

	  tv.tv_sec = 0;
	  tv.tv_usec = 1000;
	  timer_set_relative(dhcp->timer, tv, 
			     (timer_func_t *)dhcp_init,
			     ifstate, (void *)IFEventID_start_e, NULL);
	  break;
      }
      default:
	break;
    }
    return;
}

static void
dhcp_renew_rebind(IFState_t * ifstate, IFEventID_t event_id, void * event_data)
{
    dhcp_time_secs_t 	current_time = timer_current_secs();
    IFState_dhcp_t *	dhcp = (IFState_dhcp_t *)ifstate->private;
    ipconfig_status_t	status = ipconfig_status_success_e;
    struct timeval 	tv;

    switch (event_id) {
      case IFEventID_start_e: {
	  dhcp_lease_t 		lease_option;
	  dhcpoa_t	 	options;

	  /* clean-up anything that might have come before */
	  dhcp_cancel_pending_events(ifstate);

	  dhcp->state = dhcp_cstate_renew_e;
	  my_log(LOG_DEBUG, "DHCP %s: RENEW", if_name(ifstate->if_p));
	  dhcp->request = make_dhcp_request((struct dhcp *)dhcp->txbuf, 
					    sizeof(dhcp->txbuf),
					    dhcp_msgtype_request_e,
					    if_link_address(ifstate->if_p), 
					    if_link_arptype(ifstate->if_p),
					    if_link_length(ifstate->if_p), 
					    dhcp->client_id, dhcp->client_id_len,
					    &options);
	  if (dhcp->request == NULL) {
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  dhcp->try = 0;
	  dhcp->request->dp_ciaddr = dhcp->saved.our_ip;
	  lease_option = dhcp_lease_hton(SUGGESTED_LEASE_LENGTH);
	  if (dhcpoa_add(&options, dhcptag_lease_time_e, sizeof(lease_option), 
			 &lease_option) != dhcpoa_success_e) {
	      my_log(LOG_ERR, "DHCP %s: RENEW/REBIND couldn't add"
		     " lease time: %s", if_name(ifstate->if_p),
		     dhcpoa_err(&options));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  add_computer_name(&options);
	  if (dhcpoa_add(&options, dhcptag_end_e, 0, 0)
	      != dhcpoa_success_e) {
	      my_log(LOG_ERR, "DHCP %s: RENEW/REBIND failed to terminate options",
		     if_name(ifstate->if_p));
	      status = ipconfig_status_allocation_failed_e;
	      goto error;
	  }
	  /* enable packet reception */
	  bootp_client_enable_receive(dhcp->client,
				      (bootp_receive_func_t *)dhcp_renew_rebind,
				      ifstate, (void *)IFEventID_data_e);
	  /* FALL THROUGH */
      }
      case IFEventID_timeout_e: {
	  struct in_addr	dest_ip = {0};
	  dhcp_time_secs_t	wakeup_time = current_time;

	  if (current_time >= dhcp->lease_expiration) {
	      /* server did not respond */
	      ifstate_publish_failure(ifstate, 
				      ipconfig_status_server_not_responding_e, 
				      NULL);
	      dhcp_unbound(ifstate, IFEventID_start_e, NULL);
	      return;
	  }
	  if (current_time < dhcp->t2) {
	      dhcp->state = dhcp_cstate_renew_e;
	      wakeup_time = current_time + (dhcp->t2 - current_time) / 2;
	      dest_ip = dhcp->saved.server_ip;
	  }
	  else { /* rebind */
	      dhcp->state = dhcp_cstate_rebind_e;
	      wakeup_time = current_time 
		  + (dhcp->lease_expiration - current_time) / 2;
	      dest_ip = G_ip_broadcast;
	  }
	  dhcp->request->dp_xid = htonl(++dhcp->xid);
#ifdef DEBUG
	  dhcp_print_packet(dhcp->request, dhcp->request_size);
#endif DEBUG
	  /* send the packet */
	  if (bootp_client_transmit(dhcp->client, if_name(ifstate->if_p),
				    dhcp->request->dp_htype, NULL, 0,
				    dest_ip, dhcp->saved.our_ip,
				    G_server_port, G_client_port,
				    dhcp->request, dhcp->request_size) < 0) {
	      my_log(LOG_ERR, 
		     "DHCP %s: RENEW/REBIND transmit failed", 
		     if_name(ifstate->if_p));
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
		 if_name(ifstate->if_p), 
		 timer_current_secs() - dhcp->start_secs,
		 tv.tv_sec, tv.tv_usec);
	  timer_set_relative(dhcp->timer, tv, 
			     (timer_func_t *)dhcp_renew_rebind,
			     ifstate, (void *)IFEventID_timeout_e, NULL);
	  break;
      }
      case IFEventID_data_e: {
	  boolean_t 		is_dhcp = TRUE;
	  dhcp_lease_t 		lease = SUGGESTED_LEASE_LENGTH;
	  bootp_receive_data_t *pkt = (bootp_receive_data_t *)event_data;
	  dhcp_msgtype_t	reply_msgtype = dhcp_msgtype_none_e;
	  struct in_addr	server_ip;

	  if (verify_packet(pkt, dhcp->xid, ifstate->if_p, &reply_msgtype,
			    &server_ip, &is_dhcp) == FALSE
	      || is_dhcp == FALSE) {
	      /* reject the packet */
	      return;
	  }

	  if (reply_msgtype == dhcp_msgtype_nak_e) {
	      ifstate_publish_failure(ifstate, 
				      ipconfig_status_lease_terminated_e,
				      NULL);
	      dhcp_unbound(ifstate, IFEventID_start_e, NULL);
	      return;
	  }
	  if (reply_msgtype != dhcp_msgtype_ack_e
	      || server_ip.s_addr == 0
	      || ip_valid(pkt->data->dp_yiaddr) == FALSE
	      || get_lease(&pkt->options, &lease) == FALSE) {
	      /* reject the packet */
	      return;
	  }
	  
	  /* address has to match, otherwise start over */
	  if (pkt->data->dp_yiaddr.s_addr != dhcp->saved.our_ip.s_addr) {
	      ifstate_publish_failure(ifstate, 
				      ipconfig_status_server_error_e,
				      NULL);
	      dhcp_unbound(ifstate, IFEventID_start_e, NULL);
	      return;
	  }
	  
	  /* need to check whether t1,t2 overridden by server XXX */
	  dhcp_set_lease_params(ifstate, "RENEW/REBIND", is_dhcp, lease);

	  dhcpol_free(&dhcp->saved.options);
	  bcopy(pkt->data, dhcp->saved.pkt, pkt->size);
	  dhcp->saved.pkt_size = pkt->size;
	  dhcp->saved.rating = 0;
	  (void)dhcpol_parse_packet(&dhcp->saved.options, 
				    (void *)dhcp->saved.pkt, 
				    dhcp->saved.pkt_size, NULL);
	  dhcp->saved.server_ip = server_ip;
	  dhcp->saved.is_dhcp = TRUE;
	  dhcp_bound(ifstate, IFEventID_start_e, NULL);
	  break;
      }
      default:
	  return;
    }
    return;

 error:
    dhcp_failed(ifstate, status, "RENEW/REBIND");
    return;
}

static void
dhcp_release(IFState_t * ifstate)
{
    IFState_dhcp_t *	dhcp = (IFState_dhcp_t *)ifstate->private;
    dhcpoa_t	 	options;

    if (dhcp->saved.is_dhcp == FALSE || dhcp->saved.our_ip.s_addr == 0) {
	return;
    }

    /* clean-up anything that might have come before */
    dhcp_cancel_pending_events(ifstate);

    /* release the address */
    dhcp->request = make_dhcp_request((struct dhcp *)dhcp->txbuf, 
				      sizeof(dhcp->txbuf),
				      dhcp_msgtype_release_e,
				      if_link_address(ifstate->if_p), 
				      if_link_arptype(ifstate->if_p),
				      if_link_length(ifstate->if_p), 
				      dhcp->client_id, dhcp->client_id_len,
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
	       if_name(ifstate->if_p), dhcpoa_err(&options));
	return;
    }
    if (dhcpoa_add(&options, dhcptag_end_e, 0, 0)
	!= dhcpoa_success_e) {
	my_log(LOG_ERR, "DHCP %s: RELEASE failed to terminate options",
	       if_name(ifstate->if_p));
	return;
    }
    if (bootp_client_transmit(dhcp->client, if_name(ifstate->if_p),
			      dhcp->request->dp_htype, NULL, 0,
			      dhcp->saved.server_ip, dhcp->saved.our_ip,
			      G_server_port, G_client_port,
			      dhcp->request, dhcp->request_size) < 0) {
	my_log(LOG_ERR, 
	       "DHCP %s: RELEASE transmit failed", 
	       if_name(ifstate->if_p));
	return;
    }
    dhcp->saved.our_ip.s_addr = 0;
    dhcp_lease_clear(dhcp->idstr);
    return;
}

