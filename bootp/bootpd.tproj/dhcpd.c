/*
 * Copyright (c) 1999-2014 Apple Inc. All rights reserved.
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
 * dhcpd.c
 * - DHCP server
 */

/* 
 * Modification History
 * June 17, 1998 	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/time.h>
#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/bootp.h>
#include <netinet/if_ether.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <mach/boolean.h>
#include <notify.h>
#include "util.h"
#include "netinfo.h"
#include "dhcp.h"
#include "rfc_options.h"
#include "dhcp_options.h"
#include "host_identifier.h"
#include "hostlist.h"
#include "interfaces.h"
#include "dhcpd.h"
#include "NICache.h"
#include "NICachePrivate.h"
#include "dhcplib.h"
#include "bootpd.h"
#include "subnets.h"
#include "bootpdfile.h"
#include "bootplookup.h"
#include "nbo.h"


typedef long			dhcp_time_secs_t;
#define DHCP_INFINITE_TIME	((dhcp_time_secs_t)-1)

#define MAX_RETRY	5

static boolean_t	S_extend_leases = TRUE;

typedef struct {
    PLCache_t		list;
} DHCPLeases_t;

static DHCPLeases_t	S_leases;

void
DHCPLeases_free(DHCPLeases_t * leases)
{
    PLCache_free(&leases->list);
    bzero(leases, sizeof(*leases));
}

#define DHCP_LEASES_FILE		"/var/db/dhcpd_leases"
boolean_t
DHCPLeases_init(DHCPLeases_t * leases)
{
    bzero(leases, sizeof(*leases));
    PLCache_init(&leases->list);
#define ARBITRARILY_LARGE_NUMBER	(100 * 1024 * 1024)
    PLCache_set_max(&leases->list, ARBITRARILY_LARGE_NUMBER);
    if (PLCache_read(&leases->list, DHCP_LEASES_FILE) == FALSE) {
	goto failed;
    }
    return (TRUE);
 failed:
    DHCPLeases_free(leases);
    return (FALSE);
}

static boolean_t S_remove_host(PLCacheEntry_t * * entry);

boolean_t
DHCPLeases_reclaim(DHCPLeases_t * leases, interface_t * if_p, 
		   struct in_addr giaddr, struct timeval * time_in_p,
		   struct in_addr * client_ip)
{
    PLCacheEntry_t *	scan;

    for (scan = leases->list.tail; scan; scan = scan->prev) {
	dhcp_time_secs_t	expiry = 0;
	struct in_addr		iaddr;
	int			ip_index;
	ni_namelist *		ip_nl_p;
	int			lease_index;

	/* check the IP address */
	ip_index = (int)ni_proplist_match(scan->pl, NIPROP_IPADDR, NULL);
	if (ip_index == NI_INDEX_NULL) {
	    continue;
	}
	ip_nl_p = &scan->pl.nipl_val[ip_index].nip_val;
	if (ip_nl_p->ninl_len == 0) {
	    continue;
	}
	if (inet_aton(ip_nl_p->ninl_val[0], &iaddr) == 0) {
	    continue;
	}
	if (!ip_address_reachable(iaddr, giaddr, if_p)) {
	    /* not applicable to this network */
	    continue;
	}
	/* check the lease expiration */
	lease_index = (int)ni_proplist_match(scan->pl, NIPROP_DHCP_LEASE, NULL);
	if (lease_index != NI_INDEX_NULL) {
	    ni_namelist *		lease_nl_p;
	    long			val;

	    lease_nl_p = &scan->pl.nipl_val[lease_index].nip_val;
	    if (lease_nl_p->ninl_len == 0) {
		continue;
	    }
	    val = strtol(lease_nl_p->ninl_val[0], NULL, 0);
	    if (val == LONG_MAX && errno == ERANGE) {
		continue;
	    }
	    expiry = (dhcp_time_secs_t)val;
	}
	if (lease_index == NI_INDEX_NULL || time_in_p->tv_sec > expiry) {
	    if (S_remove_host(&scan)) {
		my_log(LOG_DEBUG, "dhcp: reclaimed address %s",
		       inet_ntoa(iaddr));
		*client_ip = iaddr;
		return (TRUE);
	    }
	}
    }
    return (FALSE);
}


int
dhcp_max_message_size(dhcpol_t * options) 
{
    u_char * 	opt;
    int 	opt_len;
    int		val = DHCP_PACKET_MIN;

    opt = dhcpol_find(options, dhcptag_max_dhcp_message_size_e,
		      &opt_len, NULL);
    if (opt != NULL && opt_len == 2) {
	u_int16_t sval = net_uint16_get(opt);
	if (sval > DHCP_PACKET_MIN) {
	    val = sval;
	}
    }
    return (val);
}

void
dhcp_init()
{
    static boolean_t 	first = TRUE;

    if (first == TRUE) {
	if (DHCPLeases_init(&S_leases) == FALSE) {
	    return;
	}
	first = FALSE;
    } 
    else {
	DHCPLeases_t new_leases;

	my_log(LOG_INFO, "dhcp: re-reading lease list");
	if (DHCPLeases_init(&new_leases) == TRUE) {
	    DHCPLeases_free(&S_leases);
	    S_leases = new_leases;
	}
    }
    return;
}

boolean_t
DHCPLeases_ip_in_use(DHCPLeases_t * leases, struct in_addr ip)
{
    PLCacheEntry_t * entry = PLCache_lookup_ip(&leases->list, ip);
    return (entry != NULL);
}

static void
S_generate_lease_change_notification(void)
{
    notify_post(DHCPD_LEASES_NOTIFICATION_KEY);
    return;
}

static boolean_t
S_remove_host(PLCacheEntry_t * * entry)
{
    PLCacheEntry_t *	ent = *entry;

    PLCache_remove(&S_leases.list, ent);
    PLCacheEntry_free(ent);
    *entry = NULL;
    PLCache_write(&S_leases.list, DHCP_LEASES_FILE);
    S_generate_lease_change_notification();
    return (TRUE);
}

static __inline__ boolean_t
S_commit_mods()
{
    return (PLCache_write(&S_leases.list, DHCP_LEASES_FILE));
}

static __inline__ char *
S_lease_propval(ni_proplist * pl_p)
{
    return (ni_valforprop(pl_p, NIPROP_DHCP_LEASE));
}

#define LEASE_FORMAT	"0x%lx"

static void
S_set_lease(ni_proplist * pl_p, dhcp_time_secs_t lease_time_expiry, 
	    boolean_t * mod)
{
    char buf[32];

    snprintf(buf, sizeof(buf), LEASE_FORMAT, lease_time_expiry);
    ni_set_prop(pl_p, NIPROP_DHCP_LEASE, buf, mod);
    return;
}

static dhcp_time_secs_t
S_lease_time_expiry(ni_proplist * pl_p)
{
    dhcp_time_secs_t 	expiry = DHCP_INFINITE_TIME;
    ni_name 		str = S_lease_propval(pl_p);
    long		val;

    if (str) {
	val = strtol(str, NULL, 0);
	if (val == LONG_MAX && errno == ERANGE) {
	    my_log(LOG_INFO, "S_lease_time_expiry: lease '%s' bad", str);
	    return (0);
	}
	expiry = (dhcp_time_secs_t)val;
    }
    return (expiry);
    
}

struct dhcp * 
make_dhcp_reply(struct dhcp * reply, int pkt_size, 
		struct in_addr server_id, dhcp_msgtype_t msg, 
		struct dhcp * request, dhcpoa_t * options)
{
    *reply = *request;
    reply->dp_hops = 0;
    reply->dp_secs = 0;
    reply->dp_op = BOOTREPLY;
    bcopy(rfc_magic, reply->dp_options, sizeof(rfc_magic));
    dhcpoa_init(options, reply->dp_options + sizeof(rfc_magic),
		pkt_size - sizeof(struct dhcp) - sizeof(rfc_magic));
    /* make the reply a dhcp message */
    if (dhcpoa_add_dhcpmsg(options, msg) != dhcpoa_success_e) {
	my_log(LOG_INFO, 
	       "make_dhcp_reply: couldn't add dhcp message tag %d: %s", msg,
	       dhcpoa_err(options));
	goto err;
    }
    /* add our server identifier */
    if (dhcpoa_add(options, dhcptag_server_identifier_e,
		   sizeof(server_id), &server_id) != dhcpoa_success_e) {
	my_log(LOG_INFO, 
	       "make_dhcp_reply: couldn't add server identifier tag: %s",
	       dhcpoa_err(options));
	goto err;
    }
    return (reply);
  err:
    return (NULL);
}

static struct dhcp * 
make_dhcp_nak(struct dhcp * reply, int pkt_size, 
	      struct in_addr server_id, dhcp_msgtype_t * msg_p, 
	      const char * nak_msg, struct dhcp * request, 
	      dhcpoa_t * options)
{
    struct dhcp * r;

    if (debug)
	printf("sending a NAK: '%s'\n", nak_msg);

    r = make_dhcp_reply(reply, pkt_size, server_id, dhcp_msgtype_nak_e, 
			request, options);
    if (r == NULL)
	return (NULL);

    r->dp_ciaddr.s_addr = 0;
    r->dp_yiaddr.s_addr = 0;

    if (nak_msg) {
	if (dhcpoa_add(options, dhcptag_message_e, (int)strlen(nak_msg),
		       nak_msg) != dhcpoa_success_e) {
	    my_log(LOG_INFO, "dhcpd: couldn't add NAK message type: %s",
		   dhcpoa_err(options));
	    goto err;
	}
    }
    if (dhcpoa_add(options, dhcptag_end_e, 0, 0) != dhcpoa_success_e) {
	my_log(LOG_INFO, "dhcpd: couldn't add end tag: %s",
	       dhcpoa_err(options));
	goto err;
    }
    *msg_p = dhcp_msgtype_nak_e;
    return (r);
 err:
    return (NULL);
}

static struct hosts *		S_pending_hosts = NULL;

#define DEFAULT_PENDING_SECS	60

static bool
S_ipinuse(void * arg, struct in_addr ip)
{
    struct hosts * 	hp;
    struct timeval * 	time_in_p = (struct timeval *)arg;

    if (bootp_getbyip_file(ip, NULL, NULL)
#if !TARGET_OS_EMBEDDED
	|| ((use_open_directory == TRUE)
	    && bootp_getbyip_ds(ip, NULL, NULL))
#endif /* !TARGET_OS_EMBEDDED */
        ) {
	return (TRUE);
    }

    if (DHCPLeases_ip_in_use(&S_leases, ip) == TRUE) {
	return (TRUE);
    }
    hp = hostbyip(S_pending_hosts, ip);
    if (hp) {
	u_long pending_secs = time_in_p->tv_sec - hp->tv.tv_sec;

	if (pending_secs < DEFAULT_PENDING_SECS) {
	    my_log(LOG_DEBUG, "dhcpd: %s will remain pending %d secs",
		   inet_ntoa(ip), DEFAULT_PENDING_SECS - pending_secs);
	    return (TRUE);
	}
	hostfree(&S_pending_hosts, hp); /* remove it from the list */
	return (FALSE);
    }
    
    return (FALSE);
}

#define DHCPD_CREATOR		"dhcpd"

static char *
S_get_hostname(void * hostname_opt, int hostname_opt_len)
{

    if (hostname_opt && hostname_opt_len > 0) {
      	int		i;
	char * 		h = (char *)hostname_opt;
	char * 		hostname = malloc(hostname_opt_len + 1);

	for (i = 0; i < hostname_opt_len; i++) {
	    char 	ch = h[i];
	    if (ch == 0 || ch == '\n') {
		ch = '.';
	    }
	    hostname[i] = ch;
	}
	hostname[hostname_opt_len] = '\0';
	return (hostname);
    }
    return (NULL);
}

static boolean_t
S_create_host(char * idstr, char * hwstr,
	      struct in_addr iaddr, void * hostname_opt, int hostname_opt_len,
	      dhcp_time_secs_t lease_time_expiry)
{
    char		lease_str[128];
    ni_proplist 	pl;


    /* add DHCP-specific properties */
    NI_INIT(&pl);
    if (hostname_opt) {
	char *	h;
	h = S_get_hostname(hostname_opt, hostname_opt_len);
	
	ni_proplist_addprop(&pl, NIPROP_NAME, (ni_name)h);
	free(h);
    }
    ni_proplist_addprop(&pl, NIPROP_IPADDR,
			(ni_name) inet_ntoa(iaddr));
    ni_proplist_addprop(&pl, NIPROP_HWADDR,
			(ni_name) hwstr);
    ni_proplist_addprop(&pl, NIPROP_IDENTIFIER,
			(ni_name) idstr);
    snprintf(lease_str, sizeof(lease_str), LEASE_FORMAT, lease_time_expiry);
    ni_proplist_addprop(&pl, NIPROP_DHCP_LEASE, (ni_name)lease_str);

    PLCache_add(&S_leases.list, PLCacheEntry_create(pl));
    PLCache_write(&S_leases.list, DHCP_LEASES_FILE);
    ni_proplist_free(&pl);
    S_generate_lease_change_notification();
    return (TRUE);
}

typedef enum {
    dhcp_binding_none_e = 0,
    dhcp_binding_permanent_e,
    dhcp_binding_temporary_e,
} dhcp_binding_t;

static SubnetRef
acquire_ip(struct in_addr giaddr, interface_t * if_p,
	   struct timeval * time_in_p, struct in_addr * iaddr_p)
{
    SubnetRef 	subnet = NULL;

    if (subnets == NULL) {
	return (NULL);
    }
    if (giaddr.s_addr) {
	*iaddr_p = giaddr;
	subnet = SubnetListAcquireAddress(subnets, iaddr_p, S_ipinuse,
					  time_in_p);
    }
    else {
	int 			i;
	inet_addrinfo_t *	info;

	for (i = 0; i < if_inet_count(if_p); i++) {
	    info = if_inet_addr_at(if_p, i);
	    *iaddr_p = info->netaddr;
	    subnet = SubnetListAcquireAddress(subnets, iaddr_p, S_ipinuse,
					      time_in_p);
	    if (subnet != NULL) {
		break;
	    }
	}
    }
    return (subnet);
}

boolean_t
dhcp_bootp_allocate(char * idstr, char * hwstr, struct dhcp * rq,
		    interface_t * if_p, struct timeval * time_in_p,
		    struct in_addr * iaddr_p, SubnetRef * subnet_p)
{
    PLCacheEntry_t * 	entry = NULL;
    struct in_addr 	iaddr;
    dhcp_time_secs_t	lease_time_expiry = 0;
    subnet_match_args_t	match;
    dhcp_lease_time_t	max_lease;
    boolean_t		modified = FALSE;
    SubnetRef		subnet = NULL;

    bzero(&match, sizeof(match));
    match.if_p = if_p;
    match.giaddr = rq->dp_giaddr;
    match.has_binding = FALSE;

    if (bootp_getbyhw_file(rq->dp_htype, rq->dp_chaddr, rq->dp_hlen,
			   subnet_match, &match, &iaddr, NULL, NULL)
#if !TARGET_OS_EMBEDDED
	|| ((use_open_directory == TRUE)
	    && bootp_getbyhw_ds(rq->dp_htype, rq->dp_chaddr, rq->dp_hlen,
				subnet_match, &match, &iaddr, NULL, NULL))
#endif /* !TARGET_OS_EMBEDDED */
	) {

	/* infinite lease */
	*iaddr_p = iaddr;
	if (subnets != NULL) {
	    /* try exact */
	    subnet = SubnetListGetSubnetForAddress(subnets, iaddr, TRUE);
	    if (subnet == NULL) {
		/* try any */
		subnet = SubnetListGetSubnetForAddress(subnets, iaddr, FALSE);
	    }
	}
	*subnet_p = subnet;
	return (TRUE);
    }

    match.has_binding = FALSE;
    entry = PLCache_lookup_identifier(&S_leases.list, idstr,
				      subnet_match, &match, &iaddr,
				      NULL);
    if (entry) {
	if (subnets != NULL) {
	    subnet = SubnetListGetSubnetForAddress(subnets, iaddr, TRUE);
	}
	if (subnet != NULL) {
	    max_lease = SubnetGetMaxLease(subnet);
	    lease_time_expiry = max_lease + time_in_p->tv_sec;
	    S_set_lease(&entry->pl, lease_time_expiry, &modified);

	    PLCache_make_head(&S_leases.list, entry);
	    *iaddr_p = iaddr;
	    *subnet_p = subnet;
	    if (modified && S_commit_mods() == FALSE) {
		return (FALSE);
	    }
	    return (TRUE);
	}
	/* remove the old binding, it's not valid */
	PLCache_remove(&S_leases.list, entry);
	modified = TRUE;
	entry = NULL;
    }

    subnet = acquire_ip(rq->dp_giaddr, if_p, time_in_p, &iaddr);
    if (subnet == NULL) {
	if (DHCPLeases_reclaim(&S_leases, if_p, rq->dp_giaddr, 
			       time_in_p, &iaddr)) {
	    subnet = SubnetListGetSubnetForAddress(subnets, iaddr, TRUE);
	}
	if (subnet == NULL) {
	    if (debug) {
		printf("no ip addresses\n");
	    }
	    if (modified) {
		S_commit_mods();
	    }
	    return (FALSE);
	}
    }
    *subnet_p = subnet;
    *iaddr_p = iaddr;
    max_lease = SubnetGetMaxLease(subnet);
    lease_time_expiry = max_lease + time_in_p->tv_sec;
    if (S_create_host(idstr, hwstr,
		      iaddr, NULL, 0, lease_time_expiry) == FALSE) {
	return (FALSE);
    }
    return (TRUE);
}

void
dhcp_request(request_t * request, dhcp_msgtype_t msgtype,
	     boolean_t dhcp_allocate)
{
    dhcp_binding_t	binding = dhcp_binding_none_e;
    char		cid_type;
    int			cid_len;
    void *		cid;
    PLCacheEntry_t * 	entry = NULL;
    boolean_t		has_binding = FALSE;
    void *		hostname_opt = NULL;
    int			hostname_opt_len = 0;
    char *		hostname = NULL;
    char *		hwstr = NULL;
    char *		idstr = NULL;
    struct in_addr	iaddr;
    dhcp_lease_time_t	lease = 0;
    dhcp_time_secs_t	lease_time_expiry = 0;
    int			len;
    int			max_packet;
    dhcp_lease_time_t	min_lease;
    dhcp_lease_time_t	max_lease;
    boolean_t		modified = FALSE;
    dhcpoa_t		options;
    boolean_t		orphan = FALSE;
    struct dhcp *	reply = NULL;
    dhcp_msgtype_t	reply_msgtype = dhcp_msgtype_none_e;
    struct dhcp *	rq = request->pkt;
    char		scratch_idstr[128];
    char		scratch_hwstr[sizeof(rq->dp_chaddr) * 3];
    SubnetRef 		subnet = NULL;
    dhcp_lease_time_t *	suggested_lease = NULL;
    dhcp_cstate_t	state = dhcp_cstate_none_e;
    uint32_t		txbuf[ETHERMTU / sizeof(uint32_t)];
    boolean_t		use_broadcast = FALSE;

    iaddr.s_addr = 0;
    max_packet = dhcp_max_message_size(request->options_p);
    if (max_packet > sizeof(txbuf)) {
	max_packet = sizeof(txbuf);
    }
    /* need to exclude the IP/UDP header from what we send back */
    max_packet -= DHCP_PACKET_OVERHEAD;

    /* check for a client identifier */
    cid = dhcpol_find(request->options_p, dhcptag_client_identifier_e, 
		      &cid_len, NULL);
    if (cid != NULL) {
	if (cid_len > 1) {
	    /* use the client identifier as provided */
	    cid_type = *((char *)cid);
	    cid_len--;
	    cid++;
	}
	else {
	    cid = NULL;
	}
    }
    if (cid == NULL
	|| (dhcp_ignore_client_identifier && rq->dp_hlen != 0)) {
	/* use the hardware address as the identifier */
	cid = rq->dp_chaddr;
	cid_type = rq->dp_htype;
	cid_len = rq->dp_hlen;
    }
    if (cid_len == 0) {
	goto no_reply;
    }
    idstr = identifierToStringWithBuffer(cid_type, cid, cid_len,
					 scratch_idstr, sizeof(scratch_idstr));
    if (idstr == NULL) {
	goto no_reply;
    }
    if (cid_type == 0) {
	hwstr = identifierToStringWithBuffer(rq->dp_htype, rq->dp_chaddr, 
					     rq->dp_hlen, scratch_hwstr,
					     sizeof(scratch_hwstr));
	if (hwstr == NULL) {
	    goto no_reply;
	}
    }
    else {
	hwstr = idstr;
    }

    hostname_opt = dhcpol_find(request->options_p, dhcptag_host_name_e,
			       &hostname_opt_len, NULL);
    if (hostname_opt && hostname_opt_len) {
	my_log(LOG_INFO, "DHCP %s [%s]: %s <%.*s>", 
	       dhcp_msgtype_names(msgtype), if_name(request->if_p), idstr,
	       hostname_opt_len, hostname_opt);
    }
    else {
	my_log(LOG_INFO, "DHCP %s [%s]: %s", 
	       dhcp_msgtype_names(msgtype), if_name(request->if_p), idstr);
    }

    suggested_lease = 
	(dhcp_lease_time_t *)dhcpol_find(request->options_p,
					 dhcptag_lease_time_e,
					 &len, NULL);
    if (cid_type != 0) { 
	subnet_match_args_t	match;

	bzero(&match, sizeof(match));
	match.if_p = request->if_p;
	match.giaddr = rq->dp_giaddr;
	match.ciaddr = rq->dp_ciaddr;
	match.has_binding = FALSE;

	if (bootp_getbyhw_file(cid_type, cid, cid_len,
			       subnet_match, &match, &iaddr,
			       &hostname, NULL)
#if !TARGET_OS_EMBEDDED
	    || ((use_open_directory == TRUE)
		&& bootp_getbyhw_ds(cid_type, cid, cid_len,
				    subnet_match, &match, &iaddr,
				    &hostname, NULL))
#endif /* !TARGET_OS_EMBEDDED */
	    ) {
	    binding = dhcp_binding_permanent_e;
	    lease_time_expiry = DHCP_INFINITE_TIME;
	}
	if (match.has_binding == TRUE) {
	    has_binding = TRUE;
	}
    }

    if (binding == dhcp_binding_none_e) {
	boolean_t		some_binding = FALSE;
	subnet_match_args_t	match;

	bzero(&match, sizeof(match));
	match.if_p = request->if_p;
	match.giaddr = rq->dp_giaddr;
	match.ciaddr = rq->dp_ciaddr;

	/* no permanent netinfo binding: check for a lease */
	entry = PLCache_lookup_identifier(&S_leases.list, idstr,
					  subnet_match, &match, &iaddr,
					  &some_binding);
	if (some_binding == TRUE) {
	    has_binding = TRUE;
	}
	if (entry != NULL) {
	    if (subnets != NULL) {
		subnet = SubnetListGetSubnetForAddress(subnets, iaddr, TRUE);
	    }
	    if (subnet == NULL || SubnetDoesAllocate(subnet) == FALSE) {
		S_remove_host(&entry);
		my_log(LOG_INFO, "dhcpd: removing %s binding for %s",
		       idstr, inet_ntoa(iaddr));
		orphan = TRUE;
		entry = NULL;
	    }
	    else {
		binding = dhcp_binding_temporary_e;
		lease_time_expiry = S_lease_time_expiry(&entry->pl);
		PLCache_make_head(&S_leases.list, entry);
	    }
	}
    }
    if (binding != dhcp_binding_none_e) {
	/* client is already bound on this subnet */
	if (lease_time_expiry == DHCP_INFINITE_TIME) {
	    /* permanent entry */
	    lease = DHCP_INFINITE_LEASE;
	}
	else {
	    max_lease = SubnetGetMaxLease(subnet);
	    min_lease = SubnetGetMinLease(subnet);
	    if (suggested_lease) {
		lease = dhcp_lease_ntoh(*suggested_lease);
		if (lease > max_lease)
		    lease = max_lease;
		else if (lease < min_lease)
		    lease = min_lease;
	    }
	    else if ((request->time_in_p->tv_sec + min_lease) 
		     >= lease_time_expiry) {
		/* expired lease: give it the default lease */
		lease = min_lease;
	    }
	    else { /* give the host the remaining time on the lease */
		lease = (dhcp_lease_time_t)
		    (lease_time_expiry - request->time_in_p->tv_sec);
	    }
	}
    }

    switch (msgtype) {
      case dhcp_msgtype_discover_e: {
	  state = dhcp_cstate_init_e;

	  { /* delete the pending host entry */
	      struct hosts *	hp;
	      hp = hostbyaddr(S_pending_hosts, cid_type, cid, cid_len,
			      NULL, NULL);
	      if (hp)
		  hostfree(&S_pending_hosts, hp);
	  }

	  if (binding != dhcp_binding_none_e) {
	      /* client is already bound on this subnet */
	  }
	  else if (dhcp_allocate == FALSE) {
	      /* NetBoot 1.0 enabled, but DHCP is not */
	      goto no_reply;
	  }
	  else { /* find an ip address */
	      /* allocate a new ip address */
	      subnet = acquire_ip(rq->dp_giaddr, 
				  request->if_p, request->time_in_p, &iaddr);
	      if (subnet == NULL) {
		  if (DHCPLeases_reclaim(&S_leases, request->if_p, 
					 rq->dp_giaddr, 
					 request->time_in_p, &iaddr)) {
		      if (subnets != NULL) {
			  subnet = SubnetListGetSubnetForAddress(subnets, iaddr,
								 TRUE);
		      }
		  }
		  if (subnet == NULL) {
		      if (debug) {
			  printf("no ip addresses\n");
		      }
		      goto no_reply; /* out of ip addresses */
		  }
	      }
	      max_lease = SubnetGetMaxLease(subnet);
	      min_lease = SubnetGetMinLease(subnet);
	      if (suggested_lease) {
		  lease = dhcp_lease_ntoh(*suggested_lease);
		  if (lease > max_lease)
		      lease = max_lease;
		  else if (lease < min_lease)
		      lease = min_lease;
	      }
	      else {
		  lease = min_lease;
	      }
	  }
	  { /* keep track of this offer in the pending hosts list */
	      struct hosts *	hp;

	      hp = hostadd(&S_pending_hosts, request->time_in_p, 
			   cid_type, cid, cid_len,
			   &iaddr, NULL, NULL);
	      if (hp == NULL)
		  goto no_reply;
	      hp->lease = lease;
	  }
	  /*
	   * allow for drift between server/client clocks by offering
	   * a lease shorter than the recorded value
	   */
	  if (lease == DHCP_INFINITE_LEASE)
	      lease = dhcp_lease_hton(lease);
	  else
	      lease = dhcp_lease_hton(lease_prorate(lease));

	  /* form a reply */
	  reply = make_dhcp_reply((struct dhcp *)txbuf, max_packet,
				  if_inet_addr(request->if_p), 
				  reply_msgtype = dhcp_msgtype_offer_e,
				  rq, &options);
	  if (reply == NULL)
	      goto no_reply;
	  reply->dp_ciaddr.s_addr = 0;
	  reply->dp_yiaddr = iaddr;
	  if (dhcpoa_add(&options, dhcptag_lease_time_e, sizeof(lease),
			 &lease) != dhcpoa_success_e) {
	      my_log(LOG_INFO, "dhcpd: couldn't add lease time tag: %s",
		     dhcpoa_err(&options));
	      goto no_reply;
	  }
	  break;
      }
      case dhcp_msgtype_request_e: {
	  const char * 		nak = NULL;
	  int			optlen;
	  struct in_addr * 	req_ip;
	  struct in_addr * 	server_id;

	  server_id = (struct in_addr *)
	      dhcpol_find(request->options_p, dhcptag_server_identifier_e,
			  &optlen, NULL);
	  req_ip = (struct in_addr *)
	      dhcpol_find(request->options_p, dhcptag_requested_ip_address_e,
			  &optlen, NULL);
	  if (server_id) { /* SELECT */
	      struct hosts *	hp = hostbyaddr(S_pending_hosts, cid_type,
						cid, cid_len,
						NULL, FALSE);
	      if (debug)
		  printf("SELECT\n");
	      state = dhcp_cstate_select_e;

	      if (server_id->s_addr != if_inet_addr(request->if_p).s_addr) {
		  if (debug)
		      printf("client selected %s\n", inet_ntoa(*server_id));
		  /* clean up */
		  if (hp) {
		      hostfree(&S_pending_hosts, hp);
		  }
		  
		  if (binding == dhcp_binding_temporary_e) {
		      S_remove_host(&entry);
		  }
		  if (detect_other_dhcp_server(request->if_p)) {
		      my_log(LOG_INFO, 
			     "dhcpd: detected another DHCP server %s,"
			     " disabling DHCP on %s",
			     inet_ntoa(*server_id),
			     if_name(request->if_p));
		      disable_dhcp_on_interface(request->if_p);
		  }
		  goto no_reply;
	      }
	      if (binding == dhcp_binding_none_e && hp == NULL) {
		  goto no_reply;
	      }
	      
	      if (hp) {
		  iaddr = hp->iaddr;
		  if (hp->lease == DHCP_INFINITE_LEASE)
		      lease_time_expiry = DHCP_INFINITE_LEASE;
		  else {
		      lease_time_expiry 
			  = hp->lease + request->time_in_p->tv_sec;
		  }
		  lease = (dhcp_lease_time_t)hp->lease;
	      }
	      else {
		  /* this case only happens if the client sends 
		   * a REQUEST without sending a DISCOVER first
		   * but we have a binding 
		   */

		  /* iaddr, lease_time_expiry, lease
		   * are all set above 
		   */
	      }
	      if (req_ip == NULL
		  || req_ip->s_addr != iaddr.s_addr) {
		  if (req_ip == NULL) {
		      my_log(LOG_INFO,
			     "dhcpd: host %s sends SELECT without"
			     " Requested IP option", idstr);
		  }
		  else {
		      my_log(LOG_INFO, 
			     "dhcpd: host %s sends SELECT with wrong"
			     " IP address %s, should be " IP_FORMAT,
			     idstr, inet_ntoa(*req_ip), IP_LIST(&iaddr));
		  }
		  use_broadcast = TRUE;
		  reply = make_dhcp_nak((struct dhcp *)txbuf, max_packet,
					if_inet_addr(request->if_p), 
					&reply_msgtype, 
					"protocol error in SELECT state",
					rq, &options);
		  if (reply)
		      goto reply;
		  goto no_reply;
	      }
	      if (binding != dhcp_binding_none_e) {
		  if (binding == dhcp_binding_temporary_e) {
		      if (hostname_opt && hostname_opt_len > 0) {
			  char *	h;

			  h = S_get_hostname(hostname_opt, 
					     hostname_opt_len);
			  ni_set_prop(&entry->pl, NIPROP_NAME, h, &modified);
			  free(h);
		      }
		      S_set_lease(&entry->pl, lease_time_expiry, &modified);
		  }
	      }
	      else { /* create a new host entry */
		  if (subnets != NULL) {
		      subnet = SubnetListGetSubnetForAddress(subnets, iaddr,
							     TRUE);
		  }
		  if (subnet == NULL
		      || S_create_host(idstr, hwstr, iaddr, 
				       hostname_opt, hostname_opt_len,
				       lease_time_expiry) == FALSE) {
		      reply = make_dhcp_nak((struct dhcp *)txbuf, 
					    max_packet,
					    if_inet_addr(request->if_p), 
					    &reply_msgtype, 
					    "unexpected server failure",
					    rq, &options);
		      if (reply)
			  goto reply;
		      goto no_reply;
		  }
	      }
	  } /* select */
	  else /* init-reboot/renew/rebind */ {
	      if (req_ip) { /* init-reboot */
		  if (debug) 
		      printf("init-reboot\n");
		  state = dhcp_cstate_init_reboot_e;
		  if (binding == dhcp_binding_none_e) {
		      if (orphan == FALSE) {
			  my_log(LOG_DEBUG, "dhcpd: INIT-REBOOT host "
				 "%s binding for %s with another server",
				 idstr, inet_ntoa(*req_ip));
			  goto no_reply;
		      }
		      nak = "requested address no longer available";
		      use_broadcast = TRUE;
		      goto send_nak;
		  }
		  if (req_ip->s_addr != iaddr.s_addr) {
		      nak = "requested address incorrect";
		      use_broadcast = TRUE;
		      goto send_nak;
		  }
	      } /* init-reboot */
	      else if (rq->dp_ciaddr.s_addr) { /* renew/rebind */
		  if (debug) {
		      if (request->dstaddr_p == NULL 
			  || ntohl(request->dstaddr_p->s_addr) 
			  == INADDR_BROADCAST)
			  printf("rebind\n");
		      else
			  printf("renew\n");
		  }
		  if (binding == dhcp_binding_none_e) {
		      if (orphan == FALSE) {
			  if (debug) {
			      if (has_binding)
				  printf("Client binding is not applicable\n");
			      else
				  printf("No binding for client\n");
			  }
			  goto no_reply;
		      }
		      nak = "requested address no longer available";
		      use_broadcast = TRUE;
		      goto send_nak;
		  }
		  if (request->dstaddr_p == NULL
		      || ntohl(request->dstaddr_p->s_addr) == INADDR_BROADCAST
		      || rq->dp_giaddr.s_addr) { /* REBIND */
		      state = dhcp_cstate_rebind_e;
		      if (rq->dp_ciaddr.s_addr != iaddr.s_addr) {
			  if (debug)
			      printf("Incorrect ciaddr " IP_FORMAT 
				     " should be " IP_FORMAT "\n",
				     IP_LIST(&rq->dp_ciaddr), 
				     IP_LIST(&iaddr));
			  goto no_reply;
		      }
		  }
		  else { /* RENEW */
		      state = dhcp_cstate_renew_e;
		      if (rq->dp_ciaddr.s_addr != iaddr.s_addr) {
			  my_log(LOG_INFO, 
				 "dhcpd: client ciaddr=%s should use "
				 IP_FORMAT, inet_ntoa(rq->dp_ciaddr), 
				 IP_LIST(&iaddr));
			  iaddr = rq->dp_ciaddr; /* trust it anyways */
		      }
		  }
	      } /* renew/rebind */
	      else {
		  my_log(LOG_DEBUG,
			 "dhcpd: host %s in unknown state", idstr);
		  goto no_reply;
	      }

	      if (binding == dhcp_binding_permanent_e) {
		  lease = DHCP_INFINITE_LEASE;
	      }
	      else {
		  if (hostname_opt && hostname_opt_len > 0) {
		      char * h;
		      h = S_get_hostname(hostname_opt, hostname_opt_len);
		      ni_set_prop(&entry->pl, NIPROP_NAME, h, &modified);
		      free(h);
		  }
		  max_lease = SubnetGetMaxLease(subnet);
		  min_lease = SubnetGetMaxLease(subnet);
		  if (suggested_lease) {
		      lease = dhcp_lease_ntoh(*suggested_lease);
		      if (lease > max_lease)
			  lease = max_lease;
		      else if (lease < min_lease)
			  lease = min_lease;
		  }
		  else if (S_extend_leases) {
		      /* automatically extend the lease */
		      lease = min_lease;
		      my_log(LOG_DEBUG, 
			     "dhcpd: %s lease extended to %s client",
			     inet_ntoa(iaddr), dhcp_cstate_str(state));
		  }
		  else {
		      if (request->time_in_p->tv_sec >= lease_time_expiry) {
			  /* send a nak */
			  nak = "lease expired";
			  goto send_nak;
		      }
		      /* give the host the remaining time on the lease */
		      lease = (dhcp_lease_time_t)
			  (lease_time_expiry - request->time_in_p->tv_sec);
		  }
		  if (lease == DHCP_INFINITE_LEASE) {
		      lease_time_expiry = DHCP_INFINITE_TIME;
		  }
		  else {
		      lease_time_expiry = lease + request->time_in_p->tv_sec;
		  }
		  S_set_lease(&entry->pl, lease_time_expiry, &modified);
	      }
	  } /* init-reboot/renew/rebind */
      send_nak:
	  if (nak) {
	      reply = make_dhcp_nak((struct dhcp *)txbuf, max_packet,
				    if_inet_addr(request->if_p), 
				    &reply_msgtype, nak,
				    rq, &options);
	      if (reply)
		  goto reply;
	      goto no_reply;
	  }
	  /*
	   * allow for drift between server/client clocks by offering
	   * a lease shorter than the recorded value
	   */
	  if (lease == DHCP_INFINITE_LEASE)
	      lease = dhcp_lease_hton(lease);
	  else
	      lease = dhcp_lease_hton(lease_prorate(lease));

	  reply = make_dhcp_reply((struct dhcp *)txbuf, max_packet,
				  if_inet_addr(request->if_p),
				  reply_msgtype = dhcp_msgtype_ack_e,
				  rq, &options);
	  reply->dp_yiaddr = iaddr;
	  if (dhcpoa_add(&options, dhcptag_lease_time_e,
			 sizeof(lease), &lease) != dhcpoa_success_e) {
	      my_log(LOG_INFO, "dhcpd: couldn't add lease time tag: %s",
		     dhcpoa_err(&options));
	      goto no_reply;
	  }
	  break;
      }
      case dhcp_msgtype_decline_e: {
	  int			optlen;
	  struct in_addr * 	req_ip;
	  struct in_addr * 	server_id;

	  server_id = (struct in_addr *)
	      dhcpol_find(request->options_p, dhcptag_server_identifier_e,
			  &optlen, NULL);
	  req_ip = (struct in_addr *)
	      dhcpol_find(request->options_p, dhcptag_requested_ip_address_e,
			  &optlen, NULL);
	  if (server_id == NULL || req_ip == NULL) {
	      goto no_reply;
	  }
	  if (server_id->s_addr != if_inet_addr(request->if_p).s_addr) {
	      my_log(LOG_DEBUG, "dhcpd: host %s "
		     "declines IP %s from server " IP_FORMAT,
		     idstr, inet_ntoa(*req_ip), IP_LIST(server_id));
	      goto no_reply;
	  }

	  if (binding == dhcp_binding_temporary_e
	      && iaddr.s_addr == req_ip->s_addr) {
	      ni_delete_prop(&entry->pl, NIPROP_IDENTIFIER, &modified);
	      S_set_lease(&entry->pl, 
			  request->time_in_p->tv_sec + DHCP_DECLINE_WAIT_SECS,
			  &modified);
	      ni_set_prop(&entry->pl, NIPROP_DHCP_DECLINED, 
			  idstr, &modified);
	      my_log(LOG_INFO, "dhcpd: IP %s declined by %s",
		     inet_ntoa(iaddr), idstr);
	      if (debug) {
		  printf("marking host %s as declined\n", inet_ntoa(iaddr));
	      }
	  }
	  break;
      }
      case dhcp_msgtype_release_e: {
	  if (binding == dhcp_binding_temporary_e) {
	      if (debug) {
		  printf("%s released by client, setting expiration to now\n", 
			 inet_ntoa(iaddr));
	      }
	      /* set the lease expiration time to now */
	      S_set_lease(&entry->pl, request->time_in_p->tv_sec, &modified);
	  }
	  break;
      }
      case dhcp_msgtype_inform_e: {
	  iaddr = rq->dp_ciaddr;
	  reply = make_dhcp_reply((struct dhcp *)txbuf, max_packet,
				  if_inet_addr(request->if_p),
				  reply_msgtype = dhcp_msgtype_ack_e,
				  rq, &options);
	  if (reply)
	      goto reply;
	  goto no_reply;
      }
      default: {
	  if (debug) {
	      printf("unknown message ignored\n");
	  }
	  break;
      }
    }

  reply:
    if (debug)
	printf("state=%s\n", dhcp_cstate_str(state));
    if (binding == dhcp_binding_temporary_e && modified) {
	if (S_commit_mods() == FALSE)
	    goto no_reply;
    }
    { /* check the seconds field */
	u_int16_t	secs;
	
	secs = (u_int16_t)ntohs(rq->dp_secs);
	if (secs < reply_threshold_seconds) {
	    if (debug) {
		printf("rp->dp_secs %d < threshold %d\n",
		       secs, reply_threshold_seconds);
	    }
	    goto no_reply;
	}
	
    }
    if (reply) {
	if (reply_msgtype == dhcp_msgtype_ack_e || 
	    reply_msgtype == dhcp_msgtype_offer_e) {
	    int			num_params;
	    const uint8_t *	params;

	    params = (const uint8_t *)
		dhcpol_find(request->options_p, 
			    dhcptag_parameter_request_list_e,
			    &num_params, NULL);

	    bzero(reply->dp_file, sizeof(reply->dp_file));

	    reply->dp_siaddr = if_inet_addr(request->if_p);
	    strlcpy((char *)reply->dp_sname, server_name,
		    sizeof(reply->dp_sname));

	    /* add the client-specified parameters */
	    if (params != NULL)
		(void)add_subnet_options(hostname, iaddr, 
					 request->if_p, 
					 &options, params, num_params);
	    /* terminate the options */
	    if (dhcpoa_add(&options, dhcptag_end_e, 0, NULL)
		!= dhcpoa_success_e) {
		my_log(LOG_INFO, "couldn't add end tag: %s",
		       dhcpoa_err(&options));
		goto no_reply;
	    }
	}
	{
	    int size = sizeof(struct dhcp) + sizeof(rfc_magic) 
		+ dhcpoa_used(&options);
	    
	    if (size < sizeof(struct bootp)) {
		/* pad out to BOOTP-sized packet */
		size = sizeof(struct bootp);
	    }
	    if (debug) {
		printf("\nSending: DHCP %s (size %d)\n", 
		       dhcp_msgtype_names(reply_msgtype), size);
	    }
	    if (sendreply(request->if_p, (struct bootp *)reply, size, 
			  use_broadcast, &iaddr)) {
		if (hostname == NULL && entry != NULL) {
		    hostname = ni_valforprop(&entry->pl, NIPROP_NAME);
		    if (hostname != NULL)
			hostname = strdup(hostname);
		}
		my_log(LOG_INFO, "%s sent %s %s pktsize %d",
		       dhcp_msgtype_names(reply_msgtype),
		       (hostname != NULL) 
		       ? hostname : (char *)"<no hostname>", 
		       inet_ntoa(iaddr), size);
	    }
	}
    }
 no_reply:
    if (hostname != NULL)
	free(hostname);
    if (idstr != scratch_idstr)
	free(idstr);
    if (hwstr != NULL && hwstr != idstr && hwstr != scratch_hwstr)
	free(hwstr);
    return;
}
