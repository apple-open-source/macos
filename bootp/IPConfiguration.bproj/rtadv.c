/*
 * Copyright (c) 2003-2024 Apple Inc. All rights reserved.
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
 * rtadv.c
 * - IPv6 Router Advertisement
 * - sends router solicitation requests, and waits for responses
 * - reads router advertisement messages, and from it, gleans the source
 *   IPv6 address to use as the Router
 */

/* 
 * Modification History
 *
 * October 6, 2009		Dieter Siegmund (dieter@apple.com)
 * - added support for DHCPv6
 */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/param.h>
#include <ctype.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#define KERNEL_PRIVATE
#include <netinet6/in6_var.h>
#undef KERNEL_PRIVATE
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <CoreFoundation/CFSocket.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include "cfutil.h"
#include "ipconfigd_threads.h"
#include "FDSet.h"
#include "globals.h"
#include "timer.h"
#include "ifutil.h"
#include "rtutil.h"
#include "sysconfig.h"
#include "util.h"
#include "symbol_scope.h"
#include "RouterAdvertisement.h"
#include "DHCPv6Client.h"
#include "RTADVSocket.h"
#include "DNSNameList.h"
#include "IPv6AWDReport.h"
#include "PvDInfoRequest.h"
#include "PvDInfoContext.h"

typedef enum {
      rtadv_state_inactive_e = 0,
      rtadv_state_solicit_e,
      rtadv_state_acquired_e,
      rtadv_state_restart_e,
} rtadv_state_t;

STATIC const char *
rtadv_state_get_string(rtadv_state_t state)
{
    const char *	str[] = {
	"Inactive",
	"Solicit",
	"Acquired",
	"Restart",
    };
    if (state < countof(str)) {
	return (str[state]);
    }
    return ("<unknown>");
}

typedef struct {
    RTADVSocketRef		sock;
    timer_callout_t *		timer;
    RouterAdvertisementRef	ra;
    DHCPv6ClientRef		dhcp_client;
    rtadv_state_t		state;
    int				try;
    absolute_time_t		start;
    absolute_time_t		dhcpv6_complete;
    boolean_t			success_report_submitted;
    boolean_t			lladdr_ok; /* ok to send link-layer address */
    boolean_t			renew;
    uint32_t			restart_count;
    boolean_t			has_autoconf_address;
    boolean_t			pref64_configured;
    boolean_t			router_lifetime_zero;
    CFStringRef			nat64_prefix;
    PvDInfoRequestRef		pvd_request;
    PvDInfoContext		pvd_context;
    uint8_t			clat46_partial; /* 2 .. 5 */
} Service_rtadv_t;

STATIC void
rtadv_address_changed(ServiceRef service_p);
STATIC void
rtadv_pvd_additional_info_schedule_fetch(ServiceRef service_p);

STATIC void
rtadv_pvd_additional_info_request_callback(ServiceRef service_p,
					   PvDInfoRequestRef pvd_request)
{
    Service_rtadv_t *	rtadv = NULL;
    CFDictionaryRef	addinfo = NULL;
    PvDInfoRequestState	completion_status = UINT32_MAX;

    my_log(LOG_DEBUG, "%s", __func__);
    if (pvd_request == NULL) {
	my_log(LOG_DEBUG,
	       "ignoring pvd info request callback: no longer active");
	goto done;
    }
    if (service_p == NULL) {
	my_log(LOG_DEBUG,
	       "ignoring pvd info request callback: no parent service");
	goto done;
    }
    rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    completion_status = PvDInfoRequestGetCompletionStatus(pvd_request);
    if (completion_status == kPvDInfoRequestStateObtained) {
	/* request completed in success state */
	addinfo = PvDInfoRequestCopyAdditionalInformation(pvd_request);
	PvDInfoContextSetAdditionalInformation(&rtadv->pvd_context,
					       addinfo);
	PvDInfoContextSetIsOk(&rtadv->pvd_context, true);
	PvDInfoContextSetLastFetchedDateToNow(&rtadv->pvd_context);
	PvDInfoContextCalculateEffectiveExpiration(&rtadv->pvd_context);
    } else if (completion_status == kPvDInfoRequestStateIdle) {
	/* request couldn't complete due to no internet connection */
	/* allow immediate retry whenever next RA comes */
	PvDInfoContextFlush(&rtadv->pvd_context, false);
    } else {
	/* request completed in fail state */
	PvDInfoContextSetIsOk(&rtadv->pvd_context, false);
	PvDInfoContextFlush(&rtadv->pvd_context, true);
	PvDInfoContextSetLastFetchedDateToNow(&rtadv->pvd_context);
    }
    rtadv_address_changed(service_p);

done:
    my_CFRelease(&rtadv->pvd_request);
    my_CFRelease(&addinfo);
    return;
}

STATIC uint64_t
_calculate_randomized_backoff(uint16_t delay)
{
    uint32_t ms_delay = UINT32_MAX;
    uint8_t uint8_delay = 0x0f; // only the lower 4 bits matter for PvD Delay
    uint8_t delay_capped = 0;

    if (delay > (uint16_t)uint8_delay) {
	my_log(LOG_ERR, "%s: can't have delay value greater than 15", __func__);
	goto done;
    }
    uint8_delay &= (uint8_t)delay;
    // Note: RFC8801 allows max delay is 2^(10+15) milliseconds,
    // so max delay 2^25 ms = 2^15 seconds (???)
    /* cap backoff at 16 seconds max, so around 2^14 milliseconds */
    delay_capped = ((uint8_delay > 4U) ? 4U : uint8_delay);
    ms_delay = arc4random_uniform(1U << (10U + delay_capped));
    my_log(LOG_INFO,
	    "%s: delaying PvD Additional Info fetch by %u milliseconds",
	    __func__, ms_delay);

done:
    return (ms_delay == UINT32_MAX ? UINT64_MAX : (uint64_t)ms_delay);
}

STATIC void
rtadv_pvd_flush(Service_rtadv_t * rtadv)
{
    /* cleanup and release all retained PvD info */
    if (rtadv->pvd_request != NULL) {
	PvDInfoRequestCancel(rtadv->pvd_request);
	my_CFRelease(&rtadv->pvd_request);
    }
    PvDInfoContextFlush(&rtadv->pvd_context, false);
}

STATIC void
_new_pvd_info_request(ServiceRef service_p,
		      CFStringRef pvdid,
		      uint16_t seqnr,
		      uint64_t ms_delay)
{
    Service_rtadv_t * rtadv = NULL;
    RouterAdvertisementRef ra = NULL;
    dispatch_block_t completion = NULL;
    CFArrayRef ipv6_prefixes = NULL;
    const char *ifname = NULL;
    bool success = false;

    my_log(LOG_DEBUG, "%s", __func__);
    rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    ra = rtadv->ra;

    /* cleanup old request if there's one in-flight */
    rtadv_pvd_flush(rtadv);

    /* make and save new context */
    PvDInfoContextSetPvDID(&rtadv->pvd_context, pvdid);
    PvDInfoContextSetSequenceNumber(&rtadv->pvd_context, seqnr);
    ipv6_prefixes = RouterAdvertisementCopyPrefixes(ra);
    if (ipv6_prefixes == NULL) {
	my_log(LOG_ERR, "%s: couldn't copy prefixes from RA", __func__);
	goto done;
    }
    PvDInfoContextSetPrefixes(&rtadv->pvd_context, ipv6_prefixes);
    ifname = if_name(service_interface(service_p));
    PvDInfoContextSetInterfaceName(&rtadv->pvd_context, ifname);

    /* make new request and resume */
    rtadv->pvd_request = PvDInfoRequestCreate(pvdid, ipv6_prefixes, ifname,
					      ms_delay);
    if (rtadv->pvd_request == NULL) {
	my_log(LOG_ERR, "%s: couldn't create pvd info request",
	       __func__);
	goto done;
    }
    completion = ^{
	rtadv_pvd_additional_info_request_callback(service_p,
						   rtadv->pvd_request);
    };
    PvDInfoRequestSetCompletionHandler(rtadv->pvd_request, completion,
				       IPConfigurationAgentQueue());
    my_log(LOG_INFO,
	   "%s: requesting PvD Additional Information fetch via if '%s' "
	   "for PvD ID '%@' and IPv6 Prefixes %@",
	   __func__, ifname, pvdid, ipv6_prefixes);
    PvDInfoRequestResume(rtadv->pvd_request);
    success = true;

done:
    if (!success) {
	PvDInfoContextFlush(&rtadv->pvd_context, false);
    }
    my_CFRelease(&ipv6_prefixes);
    return;
}

STATIC void
rtadv_pvd_additional_info_schedule_fetch(ServiceRef service_p)
{
    Service_rtadv_t * 		rtadv = NULL;
    RouterAdvertisementRef 	ra = NULL;
    RA_PvDFlagsDelay 		flags = { 0 };
    const uint8_t * 		pvd_id = NULL;
    size_t 			pvd_id_length = 0;
    CFStringRef 		pvdid = NULL;
    CFStringRef			old_pvdid = NULL;
    bool			updated_pvd = false;
    uint16_t 			seqnr = 0;
    uint16_t 			old_seqnr = 0;
    uint64_t			ms_delay = UINT64_MAX;
    CFAbsoluteTime 		expiration = 0;
    CFAbsoluteTime 		current_time = 0;
    bool			new_request = false;
    const char *		reason = NULL;

    rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    /*
     * only deals with current RA if there's a PvD option
     * and if said option's H-flag is set
     */
    ra = rtadv->ra;
    pvd_id = RouterAdvertisementGetPvD(ra, &pvd_id_length, &seqnr, &flags);
    if (!flags.http) {
	PvDInfoContextFlush(&rtadv->pvd_context, true);
	goto done;
    }
    pvdid = DNSNameStringCreate(pvd_id, pvd_id_length);
    if (pvdid == NULL) {
	CFMutableStringRef bytes_str = NULL;

	bytes_str = CFStringCreateMutable(NULL, 0);
	print_data_cfstr(bytes_str, pvd_id, pvd_id_length);
	my_log(LOG_ERR, "%s: failed to create pvd id from raw str:\n%@",
	       __func__, bytes_str);
	my_CFRelease(&bytes_str);
	PvDInfoContextFlush(&rtadv->pvd_context, true);
	goto done;
    }
    old_pvdid = PvDInfoContextGetPvDID(&rtadv->pvd_context);
    if (old_pvdid == NULL) {
	new_request = true;
	goto done;
    }
    updated_pvd = (CFStringCompare(pvdid, old_pvdid, kCFCompareCaseInsensitive)
		   != kCFCompareEqualTo);
    if (updated_pvd) {
	/* new PvD: toss old, fetch new */
	PvDInfoContextFlush(&rtadv->pvd_context, false);
	new_request = true;
	goto done;
    }
    /* same PvD */
    if (!PvDInfoContextFetchAllowed(&rtadv->pvd_context)) {
	/* fetching failed before, mustn't retry */
	my_log(LOG_NOTICE, "%s: not allowed to fetch info for pvdid '%@'",
	       __func__, pvdid);
	goto done;
    } else if (!PvDInfoContextCanRefetch(&rtadv->pvd_context)) {
	/* can't refetch within 10 seconds */
	my_log(LOG_DEBUG, "%s: hasn't been %d seconds since last fetch",
	       __func__, kPvDInfoClientRefetchSamePvDIDMinWaitSeconds);
	goto done;
    }
    /* this checks for deprecation by new seqnr or expired info */
    old_seqnr = PvDInfoContextGetSequenceNumber(&rtadv->pvd_context);
    expiration = PvDInfoContextGetEffectiveExpirationTime(&rtadv->pvd_context);
    current_time = CFAbsoluteTimeGetCurrent();
    if (seqnr != old_seqnr) {
	my_log(LOG_DEBUG, "%s: got new seqnr != old seqnr", __func__);
	/*
	 * pvd-aware host must delay new fetch to mitigate pvd server
	 * overloading from synchronized requests of different network hosts
	 * when a deprecation event happens by new seqnr being issued
	 */
	ms_delay = _calculate_randomized_backoff(flags.delay);
	if (ms_delay == UINT64_MAX) {
	    /* bad delay value, toss info */
	    PvDInfoContextFlush(&rtadv->pvd_context, true);
	    goto done;
	}
	new_request = true;
	reason = "new sequence number";
    } else if (expiration != 0 && (expiration < current_time)) {
	my_log(LOG_DEBUG, "%s: pvd addinfo has expired, need new fetch",
	       __func__);
	new_request = true;
	reason = "expired";
    }
    if (!new_request) {
	/* info is up to date, no need for new fetch, ignore this new RA */
	my_log(LOG_DEBUG, "%s: no need to refetch", __func__);
	goto done;
    }
    /* deprecation */
    my_log(LOG_INFO, "%s: deprecating info for PvD ID '%@' with reason '%s'",
	   __func__, pvdid, reason);
    PvDInfoContextFlush(&rtadv->pvd_context, false);

done:
    if (new_request) {
	_new_pvd_info_request(service_p, pvdid, seqnr, ms_delay);
    }
    if (flags.http) {
	my_log(LOG_DEBUG, "%s: %sscheduled", __func__, new_request ? "" : "not ");
    }
    my_CFRelease(&pvdid);
    return;
}


#define CLAT46_ADDRESS_START	2
#define CLAT46_ADDRESS_COUNT	4
#define CLAT46_ADDRESS_END	(CLAT46_ADDRESS_START + CLAT46_ADDRESS_COUNT - 1)

STATIC uint16_t S_clat46_address_use_count[CLAT46_ADDRESS_COUNT];

/*
 * Function: S_clat46_address_allocate
 * Purpose:
 *   Try to find a CLAT46 IPv4 address that isn't already in active use by
 *   some other service.
 *
 *   The service continuity prefix (192.0.0.0/29) has only 8 IPv4 addresses,
 *   two of which are reserved: 192.0.0.0 and 192.0.0.7. 192.0.0.1 is
 *   is reserved for the router. Since terminusd uses 192.0.0.6, that leaves
 *   192.0.0.2 through 192.0.0.5 (four addresses) that are available.
 *
 *   Use `S_clat46_address_use_count` to store the number of references
 *   to the particular index. Try to find an index that isn't in use.
 *   If all are in use, use index zero (192.0.0.2).
 *
 * Returns:
 *   The partial CLAT46 address to use i.e. a value between 2 and 5.
 */
STATIC uint8_t
S_clat46_address_allocate(const char * ifname)
{
    bool	found_free = false;
    uint8_t	partial_addr;
    uint8_t	which = 0;

    for (uint8_t i = 0; i < CLAT46_ADDRESS_COUNT; i++) {
	if (S_clat46_address_use_count[i] == 0) {
	    which = i;
	    break;
	}
    }
    partial_addr = which + CLAT46_ADDRESS_START;
    if (S_clat46_address_use_count[which] != 0) {
	my_log(LOG_NOTICE,
	       "%s: CLAT46 address space exhausted, re-using 192.0.0.%d",
	       ifname, partial_addr);
    }
    else {
	my_log(LOG_NOTICE,
	       "%s: CLAT46 192.0.0.%d allocated",
	       ifname, partial_addr);
    }
    S_clat46_address_use_count[which]++;
    return (partial_addr);
}

/*
 * Function: S_clat46_address_release
 * Purpose:
 *   Release the specific CLAT46 `partial_address`, a value between 2 and 5.
 */
STATIC void
S_clat46_address_release(const char * ifname, uint8_t partial_addr)
{
    uint16_t	use_count;
    uint8_t	which;

    if (partial_addr < CLAT46_ADDRESS_START
	|| partial_addr > CLAT46_ADDRESS_END) {
	my_log(LOG_NOTICE,
	       "%s: CLAT46 partial address %d is invalid",
	       ifname, partial_addr);
	return;
    }
    which = partial_addr - CLAT46_ADDRESS_START;
    use_count = S_clat46_address_use_count[which];
    if (use_count == 0) {
	my_log(LOG_NOTICE,
	       "%s: CLAT46 192.0.0.%d is already released!",
	       ifname, partial_addr);
	return;
    }
    S_clat46_address_use_count[which]--;
    if (use_count == 1) {
	my_log(LOG_NOTICE, "%s: CLAT46 192.0.0.%d released",
	       ifname, partial_addr);
    }
    else {
	my_log(LOG_NOTICE, "%s: CLAT46 192.0.0.%d use count %d",
	       ifname, partial_addr, S_clat46_address_use_count[which]);
    }
}

STATIC struct in_addr
S_make_clat46_address(uint8_t index)
{
    struct in_addr	clat46_address;

    /* CLAT46 IPv4 address: 192.0.0.<index> */
    clat46_address.s_addr = htonl(IN_SERVICE_CONTINUITY + index);
    return (clat46_address);
}

STATIC struct in_addr
S_get_clat46_router(void)
{
    /* CLAT46 IPv4 address: 192.0.0.1 */
    return S_make_clat46_address(1);
}

STATIC void
insert_additional_routes(CFMutableDictionaryRef dict, struct in_addr addr)
{
    CFDictionaryRef	route_dict;

    route_dict = route_dict_create(&addr, &G_ip_broadcast, NULL);
    my_CFDictionarySetTypeAsArrayValue(dict,
				       kSCPropNetIPv4AdditionalRoutes,
				       route_dict);
    CFRelease(route_dict);
}

STATIC CFDictionaryRef
S_ipv4_clat46_dict_copy(CFStringRef ifname, uint8_t clat46_partial)
{
    struct in_addr 		clat46_address;
    CFMutableDictionaryRef	ipv4_dict;

    clat46_address = S_make_clat46_address(clat46_partial);
    ipv4_dict = CFDictionaryCreateMutable(NULL, 0,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks);
    /* Addresses */
    my_CFDictionarySetIPAddressAsArrayValue(ipv4_dict,
					    kSCPropNetIPv4Addresses,
					    clat46_address);
    /* Router */
    my_CFDictionarySetIPAddressAsString(ipv4_dict,
					kSCPropNetIPv4Router,
					S_get_clat46_router());

    /* InterfaceName */
    CFDictionarySetValue(ipv4_dict, kSCPropInterfaceName, ifname);

    /* CLAT46 */
    CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4CLAT46, kCFBooleanTrue);

    /* AdditionalRoutes */
    insert_additional_routes(ipv4_dict, clat46_address);

    return (ipv4_dict);
}


STATIC boolean_t
rtadv_set_clat46_address(ServiceRef service_p)
{
    struct in_addr	addr;
    interface_t *	if_p = service_interface(service_p);
    uint8_t		partial_addr = 0;
    int			ret = 0;
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    int			s;
    boolean_t		success = FALSE;

    if (service_clat46_is_active(service_p)) {
	return (TRUE);
    }
    s = inet_dgram_socket();
    if (s < 0) {
	my_log(LOG_ERR, "socket failed, %s (%d)",
	       strerror(errno), errno);
	goto done;
    }
    interface_set_noarp(if_name(if_p), TRUE);
    partial_addr = S_clat46_address_allocate(if_name(if_p));
    addr = S_make_clat46_address(partial_addr);
    ret = inet_aifaddr(s, if_name(if_p), addr, &G_ip_broadcast, &addr);
    if (ret == 0) {
	uint64_t	eflags = 0;

	(void)interface_get_eflags(s, if_name(if_p), &eflags);
	if ((eflags & IFEF_CLAT46) != 0
	    || inet6_clat46_start(if_name(if_p)) == 0) {
	    my_log(LOG_NOTICE,
		   "RTADV %s: CLAT46 enabled using address " IP_FORMAT,
		   if_name(if_p), IP_LIST(&addr));
	    service_clat46_set_is_active(service_p, true);
	    success = TRUE;
	}
	else {
	    my_log(LOG_ERR,
		   "RTADV %s: failed to enable CLAT46",
		   if_name(if_p));
	    (void)inet_difaddr(s, if_name(if_p), addr);
	    ServiceDetachIPv4(service_p);
	}
	flush_routes(if_link_index(if_p), G_ip_zeroes, addr);
    }
    else {
	my_log(LOG_NOTICE,
	       "RTADV %s: set CLAT46 address " IP_FORMAT " failed, %s (%d)",
	       if_name(if_p), IP_LIST(&addr), strerror(ret), ret);
    }
    close(s);

 done:
    if (success) {
	rtadv->clat46_partial = partial_addr;
    }
    else if (partial_addr != 0) {
	S_clat46_address_release(if_name(if_p), partial_addr);
    }
    return (success);
}

STATIC void
remove_all_clat46_addresses(int s, interface_t * if_p,
			    uint8_t partial_addr)
{
    for (uint8_t i = CLAT46_ADDRESS_START; i <= CLAT46_ADDRESS_END; i++) {
	struct in_addr	addr;

	if (i == partial_addr) {
	    /* skip it, we've already done it */
	    continue;
	}
	addr = S_make_clat46_address(i);
	if (inet_difaddr(s, if_name(if_p), addr) == 0) {
	    my_log(LOG_NOTICE,
		   "RTADV %s: removed CLAT46 address " IP_FORMAT,
		   if_name(if_p), IP_LIST(&addr));
	    flush_routes(if_link_index(if_p), G_ip_zeroes, addr);
	}
    }
    return;
}

STATIC void
rtadv_remove_clat46_address_only(ServiceRef service_p, boolean_t all)
{
    struct in_addr	addr;
    uint64_t		eflags = 0;
    interface_t *	if_p = service_interface(service_p);
    uint8_t		partial_addr;
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    int			s;

    s = inet_dgram_socket();
    if (s < 0) {
	my_log(LOG_ERR, "socket failed, %s (%d)",
	       strerror(errno), errno);
	return;
    }
    /* re-enable ARP */
    interface_set_noarp(if_name(if_p), FALSE);

    /* if we've assigned a CLAT46 IPv4 address, remove it now */
    partial_addr = rtadv->clat46_partial;
    if (partial_addr != 0) {
	S_clat46_address_release(if_name(if_p), partial_addr);
	rtadv->clat46_partial = 0;
	addr = S_make_clat46_address(partial_addr);
	if (inet_difaddr(s, if_name(if_p), addr) == 0) {
	    my_log(LOG_NOTICE,
		   "RTADV %s: removed CLAT46 address " IP_FORMAT,
		   if_name(if_p), IP_LIST(&addr));
	}
	else {
	    int	error = errno;

	    my_log(LOG_NOTICE,
		   "RTADV %s: remove CLAT46 address "
		   IP_FORMAT " failed, %s (%d)",
		   if_name(if_p), IP_LIST(&addr), strerror(error), error);
	}
	flush_routes(if_link_index(if_p), G_ip_zeroes, addr);
    }
    if (all) {
	/* cleanup any addresses that could have been assigned previously */
	remove_all_clat46_addresses(s, if_p, partial_addr);
    }
    (void)interface_get_eflags(s, if_name(if_p), &eflags);
    if ((eflags & IFEF_CLAT46) != 0) {
	inet6_clat46_stop(if_name(if_p));
    }
    close(s);
    ServiceDetachIPv4(service_p);
    return;
}

STATIC void
rtadv_remove_clat46_address(ServiceRef service_p, boolean_t all)
{
    rtadv_remove_clat46_address_only(service_p, all);
    service_clat46_set_is_active(service_p, false);
}

STATIC void
rtadv_set_nat64_prefixlist(ServiceRef service_p, CFStringRef nat64_prefix)
{
    interface_t *	if_p = service_interface(service_p);
    struct in6_addr	prefix;
    int			prefix_count;
    uint8_t		prefix_length;
    uint16_t		prefix_lifetime;
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    boolean_t		success;

    if (rtadv->ra == NULL
	|| !RouterAdvertisementGetPREF64(rtadv->ra,
					 &prefix,
					 &prefix_length,
					 &prefix_lifetime)
	|| prefix_lifetime == 0) {
	prefix_count = 0;
	my_CFRelease(&rtadv->nat64_prefix);
	if (!rtadv->pref64_configured) {
	    /* we didn't configure pref64 prefix, nothing to remove */
	    return;
	}
    }
    else {
	if (nat64_prefix == NULL) {
	    /* should not happen */
	}
	else {
	    if (rtadv->nat64_prefix != NULL
		&& CFEqual(nat64_prefix, rtadv->nat64_prefix)) {
		/* same prefix, nothing to do */
		return;
	    }
	    CFRetain(nat64_prefix);
	    my_CFRelease(&rtadv->nat64_prefix);
	    rtadv->nat64_prefix = nat64_prefix;
	}
	prefix_count = 1;
    }
    /* set or clear the nat64 prefixlist */
    success = inet6_set_nat64_prefixlist(if_name(if_p), &prefix, &prefix_length,
					 prefix_count);
    rtadv->pref64_configured = (prefix_count != 0) && success;
    return;
}

STATIC void
rtadv_cancel_pending_events(ServiceRef service_p)
{
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);

    timer_cancel(rtadv->timer);
    if (rtadv->sock != NULL) {
	RTADVSocketDisableReceive(rtadv->sock);
    }
    return;
}

STATIC boolean_t
rtadv_router_lladdr_collision(ServiceRef service_p)
{
    boolean_t			collision = FALSE;
    interface_t *		if_p = service_interface(service_p);
    struct in6_addr		linklocal_addr;
    Service_rtadv_t *		rtadv;
    const struct in6_addr *	router_p;

    if (if_ift_type(if_p) != IFT_CELLULAR) {
	/* don't care except for cellular */
	goto done;
    }
    rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    if (rtadv->ra == NULL) {
	goto done;
    }
    if (!inet6_get_linklocal_address(if_link_index(if_p), &linklocal_addr)) {
	/* couldn't get our link-local address */
	goto done;
    }
    router_p = RouterAdvertisementGetSourceIPAddress(rtadv->ra);
    if (IN6_ARE_ADDR_EQUAL(router_p, &linklocal_addr)) {
	collision = TRUE;
    }
 done:
    return (collision);
}

STATIC void
rtadv_submit_awd_report(ServiceRef service_p, boolean_t success)
{
    inet6_addrlist_t	addrs;
    CFStringRef		apn_name = NULL;
    boolean_t		autoconf_active = FALSE;
    boolean_t		dns_complete = FALSE;
    bool		has_search = FALSE;
    int			i;
    interface_t *	if_p = service_interface(service_p);
    int			prefix_count = 0;
    IPv6AWDReportRef	report;
    int			router_count = 0;
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    struct in6_ifstat	stats;
    inet6_addrinfo_t *	scan;
    InterfaceType	type;

    if (success) {
	if (rtadv->success_report_submitted) {
	    return;
	}
	if (rtadv->ra == NULL) {
	    my_log(LOG_NOTICE, "RTADV %s: success yet ra is NULL?",
		   if_name(if_p));
	    return;
	}
    }
    switch (if_ift_type(if_p)) {
    case IFT_CELLULAR:
	type = kInterfaceTypeCellular;
	apn_name = ServiceGetAPNName(service_p);
	break;
    case IFT_ETHER:
	type = if_is_wireless(if_p) ? kInterfaceTypeWiFi : kInterfaceTypeWired;
	break;
    default:
	type = kInterfaceTypeOther;
	break;
    }

    switch (G_awd_interface_types) {
    case kIPConfigurationInterfaceTypesCellular:
	if (type != kInterfaceTypeCellular) {
	    /* ignore non-cellular interface */
	    return;
	}
	break;
    case kIPConfigurationInterfaceTypesAll:
	break;
    default:
	/* don't generate awd reports */
	return;
    }

    report = IPv6AWDReportCreate(type);
    if (report == NULL) {
	return;
    }

    /* get the addresses and set whether flags are set or not */
    inet6_addrlist_copy(&addrs, if_link_index(if_p));
    for (i = 0, scan = addrs.list; i < addrs.count; i++, scan++) {
	if (IN6_IS_ADDR_LINKLOCAL(&scan->addr)) {
	    if ((scan->addr_flags & IN6_IFF_DUPLICATED) != 0) {
		IPv6AWDReportSetLinkLocalAddressDuplicated(report);
	    }
	}
	else if ((scan->addr_flags & IN6_IFF_AUTOCONF) != 0) {
	    IPv6AWDReportSetAutoconfAddressAcquired(report);
	    autoconf_active = TRUE;
	    if ((scan->addr_flags & IN6_IFF_DUPLICATED) != 0) {
		IPv6AWDReportSetAutoconfAddressDuplicated(report);
	    }
	    if ((scan->addr_flags & IN6_IFF_DEPRECATED) != 0) {
		IPv6AWDReportSetAutoconfAddressDeprecated(report);
	    }
	    if ((scan->addr_flags & IN6_IFF_DETACHED) != 0) {
		IPv6AWDReportSetAutoconfAddressDetached(report);
	    }
	}
    }
    inet6_addrlist_free(&addrs);

    if (apn_name != NULL) {
	IPv6AWDReportSetAPNName(report, apn_name);
    }
    if (autoconf_active && rtadv->ra != NULL) {
	uint32_t	prefix_preferred_lifetime;
	uint32_t	prefix_valid_lifetime;
	uint32_t	router_lifetime;

	router_lifetime = RouterAdvertisementGetRouterLifetime(rtadv->ra);
	prefix_preferred_lifetime
	    = RouterAdvertisementGetPrefixLifetimes(rtadv->ra,
						    &prefix_valid_lifetime);
	IPv6AWDReportSetRouterLifetime(report, router_lifetime);
	IPv6AWDReportSetPrefixPreferredLifetime(report,
						prefix_preferred_lifetime);
	IPv6AWDReportSetPrefixValidLifetime(report, prefix_valid_lifetime);

	/* set cellular-specific properties */
	if (type == kInterfaceTypeCellular) {
	    if (router_lifetime != ROUTER_LIFETIME_MAXIMUM) {
		IPv6AWDReportSetRouterLifetimeNotMaximum(report);
	    }
	    if (prefix_valid_lifetime != ND6_INFINITE_LIFETIME) {
		IPv6AWDReportSetPrefixLifetimeNotInfinite(report);
	    }
	}
    }
    /* 464XLAT */
    if (service_clat46_is_configured(service_p)) {
	IPv6AWDReportSetXLAT464Enabled(report);
    }

    /* DNS options from RA */
    if (rtadv->ra != NULL
	&& RouterAdvertisementGetRDNSS(rtadv->ra, NULL, NULL) != NULL) {
	IPv6AWDReportSetAutoconfRDNSS(report);
	dns_complete = TRUE;
	if (RouterAdvertisementGetDNSSL(rtadv->ra, NULL, NULL) != NULL) {
	    IPv6AWDReportSetAutoconfDNSSL(report);
	}
    }

    /* DNS options from DHCPv6 */
    if (rtadv->dhcp_client != NULL
	&& DHCPv6ClientHasDNS(rtadv->dhcp_client, &has_search)) {
	dns_complete = TRUE;
	IPv6AWDReportSetDHCPv6DNSServers(report);
	if (has_search) {
	    IPv6AWDReportSetDHCPv6DNSDomainList(report);
	}
    }

    if (success) {
	/* success report */
	absolute_time_t	complete;
	CFTimeInterval	delta;

	complete = RouterAdvertisementGetReceiveTime(rtadv->ra);
	if (complete > rtadv->start) {
	    delta = complete - rtadv->start;
	    IPv6AWDReportSetAutoconfAddressAcquisitionSeconds(report, delta);
	}
	if (rtadv->dhcpv6_complete != 0
	    && rtadv->dhcpv6_complete > rtadv->start) {
	    delta = rtadv->dhcpv6_complete - rtadv->start;
	    IPv6AWDReportSetDHCPv6AddressAcquisitionSeconds(report, delta);
	}
	if (dns_complete) {
	    absolute_time_t	now = timer_get_current_time();

	    if (now > rtadv->start) {
		delta = now - rtadv->start;
		IPv6AWDReportSetDNSConfigurationAcquisitionSeconds(report,
								   delta);
	    }
	}
	rtadv->success_report_submitted = TRUE;
    }
    else {
	/* failure report */
	if (rtadv->router_lifetime_zero) {
	    IPv6AWDReportSetRouterLifetimeZero(report);
	}
#if 0
	/* TBD metric to handle */
	IPv6AWDReportSetControlQueueUnsentCount(report, count);
#endif
	if (service_plat_discovery_failed(service_p)) {
	    IPv6AWDReportSetXLAT464PLATDiscoveryFailed(report);
	}
	rtadv->success_report_submitted = FALSE;
    }
    if (rtadv_router_lladdr_collision(service_p)) {
	IPv6AWDReportSetRouterSourceAddressCollision(report);
    }

    if (rtadv->try >= MAX_RTR_SOLICITATIONS) {
	IPv6AWDReportSetRouterSolicitationCount(report, MAX_RTR_SOLICITATIONS);
    }
    else {
	IPv6AWDReportSetRouterSolicitationCount(report, rtadv->try);
    }
    if (inet6_ifstat(if_name(if_p), &stats) == 0) {
	if (stats.ifs6_pfx_expiry_cnt != 0) {
	    IPv6AWDReportSetExpiredPrefixCount(report,
					       stats.ifs6_pfx_expiry_cnt);
	}
	if (stats.ifs6_defrtr_expiry_cnt != 0) {
	    IPv6AWDReportSetExpiredDefaultRouterCount(report,
						      stats.ifs6_defrtr_expiry_cnt);
	}
    }
    router_count = inet6_router_and_prefix_count(if_link_index(if_p),
						 &prefix_count);
    if (router_count > 0) {
	IPv6AWDReportSetDefaultRouterCount(report, router_count);
	IPv6AWDReportSetPrefixCount(report, prefix_count);
    }
    if (rtadv->restart_count != 0) {
	IPv6AWDReportSetAutoconfRestarted(report);
    }
#if 0
    /* TBD metric to handle */
    IPv6AWDReportSetManualAddressConfigured(report); /* need in manual_v6.c */
#endif

    IPv6AWDReportSubmit(report);
    my_log(LOG_NOTICE, "%s: submitted AWD %s report %@", if_name(if_p),
	   success ? "success" : "failure", report);

    CFRelease(report);
    return;
}

STATIC void
rtadv_submit_awd_success_report(ServiceRef service_p)
{
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);

    if (rtadv->dhcp_client != NULL
	&& DHCPv6ClientIsActive(rtadv->dhcp_client)) {
	/* waiting for DHCPv6 to complete */
	return;
    }
    rtadv_submit_awd_report(service_p, TRUE);
}

STATIC void
rtadv_set_state(ServiceRef service_p, rtadv_state_t state)
{
    interface_t *	if_p = service_interface(service_p);
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);

    my_log(LOG_NOTICE, "RTADV %s: %s", if_name(if_p),
	   rtadv_state_get_string(state));
    rtadv->state = state;
}

STATIC void
rtadv_failed(ServiceRef service_p, ipconfig_status_t status)
{
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);

    rtadv_set_state(service_p, rtadv_state_inactive_e);
    rtadv->try = 0;
    my_CFRelease(&rtadv->ra);
    rtadv_pvd_flush(rtadv);
    rtadv_cancel_pending_events(service_p);
    inet6_rtadv_disable(if_name(service_interface(service_p)));
    rtadv_set_nat64_prefixlist(service_p, NULL);
    rtadv_remove_clat46_address_only(service_p, FALSE);
    service_publish_failure(service_p, status);
    /* clear CLAT46 *after* unpublishing to ensure IPv4 gets cleared too */
    service_clat46_set_is_active(service_p, false);
    rtadv->router_lifetime_zero = FALSE;
    ServiceSetBusy(service_p, FALSE);
    return;
}

STATIC void
rtadv_inactive(ServiceRef service_p)
{
    interface_t *	if_p = service_interface(service_p);
    Service_rtadv_t *	rtadv;

    rtadv_set_state(service_p, rtadv_state_inactive_e);
    rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    inet6_flush_prefixes(if_name(if_p));
    inet6_flush_routes(if_name(if_p));
    rtadv_failed(service_p, ipconfig_status_media_inactive_e);
    rtadv->restart_count = 0;
    return;
}

static void
rtadv_log_ra(const char * ifname, RouterAdvertisementRef ra)
{
    CFStringRef	description;

    description = RouterAdvertisementCopyDescription(ra);
    my_log(LOG_INFO, "RTADV %s: received RA %@", ifname, description);
    CFRelease(description);
}

STATIC void
rtadv_acquired(ServiceRef service_p, IFEventID_t event_id, void * event_data)
{
    absolute_time_t		earliest_expiration = 0;
    absolute_time_t		pvd_info_expiration = 0;
    interface_t *		if_p = service_interface(service_p);
    absolute_time_t		now;
    RouterAdvertisementRef	ra;
    Service_rtadv_t *		rtadv;

    rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    switch (event_id) {
    case IFEventID_start_e:
	/* no more retries */
	rtadv_set_state(service_p, rtadv_state_acquired_e);
	timer_cancel(rtadv->timer);
	RTADVSocketEnableReceive(rtadv->sock,
				 (RTADVSocketReceiveFuncPtr)rtadv_acquired,
				 service_p, (void *)IFEventID_data_e);
	/* FALL THROUGH */
    case IFEventID_data_e:
	ra = (RouterAdvertisementRef)event_data;
	rtadv_log_ra(if_name(if_p), ra);
	if (rtadv->ra != NULL) {
	    const struct in6_addr *	current;
	    const struct in6_addr *	this;

	    /* XXX only track state of one router right now */
	    current = RouterAdvertisementGetSourceIPAddress(rtadv->ra);
	    this = RouterAdvertisementGetSourceIPAddress(ra);
	    if (!IN6_ARE_ADDR_EQUAL(current, this)) {
		my_log(LOG_INFO, "RTADV %s: ignoring RA (not from %@)",
		       if_name(if_p),
		       RouterAdvertisementGetSourceIPAddressAsString(rtadv->ra));
		break;
	    }
	}
	else if (RouterAdvertisementGetRouterLifetime(ra) == 0) {
	    rtadv->router_lifetime_zero = TRUE;
	    my_log(LOG_INFO,
		   "RTADV %s: ignoring RA (lifetime zero)", if_name(if_p));
	    break;
	}

	/* toss old state */
	timer_cancel(rtadv->timer);
	my_CFRelease(&rtadv->ra);

	/* abandon RA if the lifetime is now zero */
	if (RouterAdvertisementGetRouterLifetime(ra) == 0) {
	    /* router is no longer eligible to be primary */
	    my_log(LOG_NOTICE,
		   "RTADV %s: router lifetime became zero",
		   if_name(if_p));
	}
	else {
	    /* save the new RA */
	    CFRetain(ra);
	    rtadv->ra = ra;
	}

	/* kick off DHCP if needed */
	if (rtadv->dhcp_client != NULL) {
	    uint8_t	flags = 0;

	    if (rtadv->ra != NULL) {
		flags = RouterAdvertisementGetFlags(rtadv->ra);
	    }
	    if ((flags & (ND_RA_FLAG_MANAGED | ND_RA_FLAG_OTHER)) != 0) {
		DHCPv6ClientMode	expected_mode;
		DHCPv6ClientMode	mode;

		expected_mode = (flags & ND_RA_FLAG_MANAGED) != 0
		    ? kDHCPv6ClientModeStatefulAddress
		    : kDHCPv6ClientModeStateless;
		mode = DHCPv6ClientGetMode(rtadv->dhcp_client);
		if (mode != expected_mode) {
		    bool	privacy_required;

		    privacy_required
			= ServiceIsPrivacyRequired(service_p, NULL);
		    DHCPv6ClientStop(rtadv->dhcp_client);
		    DHCPv6ClientSetUsePrivateAddress(rtadv->dhcp_client,
						     privacy_required);
		    DHCPv6ClientSetMode(rtadv->dhcp_client, expected_mode);
		    DHCPv6ClientStart(rtadv->dhcp_client);
		}
	    }
	    else if (DHCPv6ClientIsActive(rtadv->dhcp_client)) {
		DHCPv6ClientStop(rtadv->dhcp_client);
	    }
	}
	if (rtadv->ra != NULL) {
	    /* if needed, set a DNS expiration timer */
	    now = RouterAdvertisementGetReceiveTime(rtadv->ra);
	    earliest_expiration
	    = RouterAdvertisementGetDNSExpirationTime(rtadv->ra, now,
							  NULL, NULL);
	}
	/* DNS may have been updated, force publish */
	rtadv_address_changed(service_p);
	    
	/* starts PvD Additional Info fetching if needed */
	if (rtadv->ra != NULL) {
	    rtadv_pvd_additional_info_schedule_fetch(service_p);
	}

	/* pick earliest of DNS and PvD expirations for timer */
	if (PvDInfoContextIsOk(&rtadv->pvd_context)) {
	    pvd_info_expiration
	    = PvDInfoContextGetEffectiveExpirationTime(&rtadv->pvd_context);

	    if (pvd_info_expiration != 0
		&& pvd_info_expiration < earliest_expiration) {
		earliest_expiration = pvd_info_expiration;
	    }
	}
	if (earliest_expiration != 0) {
	    CFDateRef	date;
	    
	    date = CFDateCreate(NULL, earliest_expiration);
	    my_log(LOG_INFO, "RTADV %s: DNS expiration time %@",
		   if_name(if_p), date);
	    CFRelease(date);
	    timer_callout_set_absolute(rtadv->timer,
				       earliest_expiration,
				       (timer_func_t *)rtadv_acquired,
				       service_p,
				       (void *)IFEventID_timeout_e,
				       NULL);
	}
	break;
    case IFEventID_timeout_e:
	if (rtadv->ra == NULL) {
	    break;
	}
	
	/* if PvD expired, deprecate info, schedule new fetch */
	if (PvDInfoContextIsOk(&rtadv->pvd_context)) {
	    pvd_info_expiration
	    = PvDInfoContextGetEffectiveExpirationTime(&rtadv->pvd_context);
	    now = timer_get_current_time();
	    
	    if (pvd_info_expiration != 0 && pvd_info_expiration < now) {
		rtadv_pvd_additional_info_schedule_fetch(service_p);
	    }
	}
	    
	/* DNS or PvD expired, force publish */
	rtadv_address_changed(service_p);

	/* check again, rtadv->ra could have been released */
	if (rtadv->ra == NULL) {
	    break;
	}

	/* check if we need to set another timer */
	now = timer_get_current_time();
	earliest_expiration
	    = RouterAdvertisementGetDNSExpirationTime(rtadv->ra, now,
						      NULL, NULL);
	if (PvDInfoContextIsOk(&rtadv->pvd_context)) {
	    pvd_info_expiration
		= PvDInfoContextGetEffectiveExpirationTime(&rtadv->pvd_context);

	    if (pvd_info_expiration != 0
		&& pvd_info_expiration < earliest_expiration) {
		earliest_expiration = pvd_info_expiration;
	    }
	}

	/* pick earliest of DNS and PvD expirations for timer */
	if (earliest_expiration != 0) {
	    CFDateRef	date;
	    
	    date = CFDateCreate(NULL, earliest_expiration);
	    my_log(LOG_INFO, "RTADV %s: DNS expiration time %@",
		   if_name(if_p), date);
	    CFRelease(date);
	    timer_callout_set_absolute(rtadv->timer,
				       earliest_expiration,
				       (timer_func_t *)rtadv_acquired,
				       service_p,
				       (void *)IFEventID_timeout_e,
				       NULL);
	}
	break;
    default:
	break;
    }
}

STATIC void
rtadv_solicit(ServiceRef service_p, IFEventID_t event_id, void * event_data)
{
    int			error;
    interface_t *	if_p = service_interface(service_p);
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    struct timeval	tv;

    switch (event_id) {
    case IFEventID_start_e:
	rtadv_set_state(service_p, rtadv_state_solicit_e);
	service_plat_discovery_clear(service_p);
	ServiceSetBusy(service_p, TRUE);
	rtadv->start = timer_get_current_time();
	rtadv->dhcpv6_complete = 0;
	rtadv->success_report_submitted = FALSE;
	rtadv->router_lifetime_zero = FALSE;
	ServiceUnpublishCLAT46(service_p);
	rtadv_remove_clat46_address(service_p, TRUE);
	rtadv_cancel_pending_events(service_p);
	RTADVSocketEnableReceive(rtadv->sock,
				 (RTADVSocketReceiveFuncPtr)rtadv_solicit,
				 service_p, (void *)IFEventID_data_e);
	if (inet6_rtadv_enable(if_name(if_p),
			       ServiceIsCGAEnabled(service_p)) != 0) {
	    rtadv_failed(service_p, ipconfig_status_internal_error_e);
	    return;
	}
	rtadv->try = 0;
	my_CFRelease(&rtadv->ra);
	rtadv->has_autoconf_address = FALSE;

	/* FALL THROUGH */

    case IFEventID_timeout_e:
	rtadv->try++;
	if (rtadv->try > 1) {
	    link_status_t	link_status = service_link_status(service_p);

	    if (link_status.valid == TRUE
		&& link_status.active == FALSE) {
		rtadv_inactive(service_p);
		return;
	    }
	    if (service_interface_ipv4_published(service_p)) {
		/* don't keep the interface busy if IPv4 is ready to go */
		my_log(LOG_INFO,
		       "RTADV %s: IPv4 is ready, release busy",
		       if_name(if_p));
		ServiceSetBusy(service_p, FALSE);
	    }
	}
	if (rtadv->try > MAX_RTR_SOLICITATIONS) {
	    if (rtadv->try == (MAX_RTR_SOLICITATIONS + 1)) {
		/* set a timer that fires if we don't get an address */
#define ADDRESS_ACQUISITION_FAILURE_TIMEOUT	20
		tv.tv_sec = ADDRESS_ACQUISITION_FAILURE_TIMEOUT;
		tv.tv_usec = 0;
		timer_set_relative(rtadv->timer, tv,
				   (timer_func_t *)rtadv_solicit,
				   service_p,
				   (void *)IFEventID_timeout_e, NULL);
	    }
	    else if (rtadv->try == (MAX_RTR_SOLICITATIONS + 2)) {
		/* generate a symptom if we don't get an address */
		ServiceGenerateFailureSymptom(service_p);
		ServiceSetBusy(service_p, FALSE);
	    }
	    /* now we just wait to see if something comes in */
	    return;
	}
	my_log(LOG_NOTICE,
	       "RTADV %s: sending Router Solicitation (%d of %d)",
	       if_name(if_p), rtadv->try, MAX_RTR_SOLICITATIONS);
	error = RTADVSocketSendSolicitation(rtadv->sock,
					    rtadv->lladdr_ok);
	switch (error) {
	case 0:
	case ENXIO:
	case ENETDOWN:
	case EADDRNOTAVAIL:
	    break;
	default:
	    my_log(LOG_ERR, "RTADV %s: send Router Solicitation: failed, %s",
		   if_name(if_p), strerror(error));
	    break;
	}
	
	/* set timer values and wait for responses */
	tv.tv_sec = RTR_SOLICITATION_INTERVAL;

	/* don't add random fuzz for cellular rdar://problem/31542146 */
	tv.tv_usec = (if_ift_type(if_p) == IFT_CELLULAR)
	    ? 0
	    : (suseconds_t)random_range(0, USECS_PER_SEC - 1);
	timer_set_relative(rtadv->timer, tv,
			   (timer_func_t *)rtadv_solicit,
			   service_p, (void *)IFEventID_timeout_e, NULL);
	break;

    case IFEventID_data_e: {
	RouterAdvertisementRef ra;

	ra = (RouterAdvertisementRef)event_data;
	if (RouterAdvertisementGetRouterLifetime(ra) != 0) {
		rtadv_acquired(service_p, IFEventID_start_e, event_data);
	}
	break;
    }
    default:
	break;
    }
    return;
}

STATIC void
rtadv_flush(ServiceRef service_p)
{
    interface_t *	if_p = service_interface(service_p);
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);

    /* flush IPv6 configuration from the interface and unpublish */
    inet6_flush_prefixes(if_name(if_p));
    inet6_flush_routes(if_name(if_p));
    inet6_rtadv_disable(if_name(if_p));
    if (rtadv->dhcp_client != NULL) {
	DHCPv6ClientStop(rtadv->dhcp_client);
	DHCPv6ClientDiscardInformation(rtadv->dhcp_client);
    }
    rtadv_failed(service_p, ipconfig_status_network_changed_e);
    return;
}

STATIC void
rtadv_restart(ServiceRef service_p, IFEventID_t event_id, void * event_data)
{
    interface_t *	if_p = service_interface(service_p);
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    struct timeval	tv;

    switch (event_id) {
    case IFEventID_start_e:
	rtadv_set_state(service_p, rtadv_state_restart_e);
	/* wait a short time to avoid looping quickly */
	tv.tv_sec = 1;
	tv.tv_usec = (suseconds_t)random_range(0, USECS_PER_SEC - 1);
	timer_set_relative(rtadv->timer, tv,
			   (timer_func_t *)rtadv_restart,
			   service_p, (void *)IFEventID_timeout_e, NULL);
	break;

    case IFEventID_timeout_e:
	rtadv->restart_count++;
	my_log(LOG_NOTICE, "RTADV %s: restart count %u",
	       if_name(if_p), rtadv->restart_count);
	rtadv_solicit(service_p, IFEventID_start_e, NULL);
	break;
    default:
	break;
    }
    return;
}

STATIC void
rtadv_router_expired(ServiceRef service_p,
		     ipv6_router_prefix_counts_t * event)
{
    interface_t *	if_p = service_interface(service_p);

    my_log(LOG_NOTICE, "RTADV %s: router expired", if_name(if_p));

    /* generate metric when the router expires */
    rtadv_submit_awd_report(service_p, FALSE);

    if (event->router_count == 0) {
	/* no more default routers, start over */
	rtadv_restart(service_p, IFEventID_start_e, NULL);
    }
    else {
	/* the router expired but we still have a router */
	rtadv_solicit(service_p, IFEventID_start_e, NULL);
    }
    return;
}

STATIC CFStringRef
create_signature(RouterAdvertisementRef ra,
		 inet6_addrinfo_t * list_p, int list_count)
{
    const uint8_t *	lladdr;
    int			lladdr_length;
    struct in6_addr	netaddr;
    char 		ntopbuf[INET6_ADDRSTRLEN];
    CFMutableStringRef	sig_str;

    if (list_p == NULL || list_count == 0 || ra == NULL) {
	return (NULL);
    }

    lladdr = RouterAdvertisementGetSourceLinkAddress(ra, &lladdr_length);
    if (lladdr == NULL) {
	return (NULL);
    }
    netaddr = list_p[0].addr;
    in6_netaddr(&netaddr, list_p[0].prefix_length);
    sig_str = CFStringCreateMutable(NULL, 0);
    CFStringAppendFormat(sig_str, NULL,
			 CFSTR("IPv6.Prefix=%s/%d;IPv6.RouterHardwareAddress="),
			 inet_ntop(AF_INET6, &netaddr,
				   ntopbuf, sizeof(ntopbuf)),
			 list_p[0].prefix_length);
    my_CFStringAppendBytesAsHex(sig_str, lladdr, lladdr_length, ':');
    return (sig_str);
}

STATIC CFStringRef
copy_nat64_prefix(RouterAdvertisementRef ra)
{
    CFStringRef		prefix;
    uint16_t		prefix_lifetime;

    prefix = RouterAdvertisementCopyPREF64PrefixAndLifetime(ra,
							    &prefix_lifetime);
    if (prefix != NULL) {
	my_log(LOG_NOTICE, "PREF64 prefix %@ lifetime %ds",
	       prefix, prefix_lifetime);
	if (prefix_lifetime == 0) {
	    my_CFRelease(&prefix);
	}
    }
    return (prefix);
}


#ifndef IN6_IFF_SWIFTDAD
#define IN6_IFF_SWIFTDAD	0x0800  /* DAD with no delay */
#endif /* IN6_IFF_SWIFTDAD */

STATIC void
rtadv_trigger_dad(ServiceRef service_p, inet6_addrinfo_t * list, int count)
{
    int			i;
    interface_t *	if_p = service_interface(service_p);
    inet6_addrinfo_t *	scan;
    int			sockfd;

    sockfd = inet6_dgram_socket();
    if (sockfd < 0) {
	my_log(LOG_ERR,
	       "RTADV %s: failed to open socket, %s",
	       if_name(if_p), strerror(errno));
	return;
    }
    for (i = 0, scan = list; i < count; scan++, i++) {
	char 	ntopbuf[INET6_ADDRSTRLEN];

	if (inet6_aifaddr(sockfd, if_name(if_p),
			  &scan->addr, NULL, scan->prefix_length,
			  scan->addr_flags | IN6_IFF_SWIFTDAD,
			  scan->valid_lifetime,
			  scan->preferred_lifetime) < 0) {
	    my_log(LOG_ERR,
		   "RTADV %s: inet6_aifaddr(%s/%d) failed, %s",
		   if_name(if_p),
		   inet_ntop(AF_INET6, &scan->addr, ntopbuf, sizeof(ntopbuf)),
		   scan->prefix_length, strerror(errno));
	}
	else {
	    my_log(LOG_NOTICE,
		   "RTADV %s: Re-assigned %s/%d",
		   if_name(if_p),
		   inet_ntop(AF_INET6, &scan->addr, ntopbuf, sizeof(ntopbuf)),
		   scan->prefix_length);
	}
    }
    close(sockfd);
    return;
}

STATIC void
rtadv_address_changed_common(ServiceRef service_p,
			     inet6_addrlist_t * addr_list_p)
{
    interface_t *	if_p = service_interface(service_p);
    inet6_addrinfo_t *	linklocal;
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    boolean_t		try_was_zero = FALSE;

    my_log(LOG_DEBUG, "%s", __func__);
    linklocal = inet6_addrlist_get_linklocal(addr_list_p);
    if (linklocal == NULL) {
	/* no linklocal address assigned, nothing to do */
	my_log(LOG_INFO,
	       "RTADV %s: link-local address not present",
	       if_name(if_p));
	return;
    }
    if ((linklocal->addr_flags & IN6_IFF_NOTREADY) != 0) {
	if ((linklocal->addr_flags & IN6_IFF_DUPLICATED) != 0) {
	    /* address conflict occurred */
	    my_log(LOG_NOTICE,
		   "RTADV %s: link-local address is duplicated",
		   if_name(if_p));
	    return;
	}
	/* linklocal address isn't ready */
	my_log(LOG_INFO,
	       "RTADV %s: link-local address is not ready",
	       if_name(if_p));
	return;
    }
    rtadv->lladdr_ok = (linklocal->addr_flags & IN6_IFF_DADPROGRESS) == 0;
    my_log(LOG_INFO,
	   "RTADV %s: link-layer option in Router Solicitation is %sOK",
	   if_name(if_p), rtadv->lladdr_ok ? "" : "not ");
    if (rtadv->try == 0) {
	link_status_t	link_status = service_link_status(service_p);

	try_was_zero = TRUE;
	if (link_status.valid == FALSE
	    || link_status.active == TRUE) {
	    my_log(LOG_NOTICE,
		   "RTADV %s: link-local address is ready, starting",
		   if_name(if_p));
	    rtadv_solicit(service_p, IFEventID_start_e, NULL);
	}
    }
    if (rtadv->renew || !try_was_zero) {
	int			count;
	boolean_t		dhcp_has_address = FALSE;
	inet6_addrlist_t	dhcp_addr_list;
	uint32_t		autoconf_count = 0;
	uint32_t		deprecated_count = 0;
	uint32_t		detached_count = 0;
	int			i;
	ipv6_info_t		info;
	inet6_addrinfo_t	list[addr_list_p->count];
	const struct in6_addr *	router_p = NULL;
	int			router_count = 0;
	inet6_addrinfo_t *	scan;
	CFStringRef		signature = NULL;

	inet6_addrlist_init(&dhcp_addr_list);
	if (rtadv->dhcp_client != NULL) {
	    DHCPv6ClientCopyAddresses(rtadv->dhcp_client, &dhcp_addr_list);
	    dhcp_has_address = (dhcp_addr_list.count != 0);
	}

	/* only copy autoconf and DHCP addresses */
	for (i = 0, count = 0, scan = addr_list_p->list;
	     i < addr_list_p->count;
	     i++, scan++) {
	    boolean_t	autoconf_address = FALSE;

	    if ((scan->addr_flags & IN6_IFF_AUTOCONF) != 0) {
		autoconf_count++;
		autoconf_address = TRUE;
	    }
	    if ((scan->addr_flags & IN6_IFF_NOTREADY) != 0) {
		if ((scan->addr_flags & IN6_IFF_DUPLICATED) != 0) {
		    my_log(LOG_NOTICE, "RTADV %s: duplicated address",
			   if_name(if_p));
		}
		continue;
	    }
	    if (autoconf_address) {
		if ((scan->addr_flags & IN6_IFF_DEPRECATED) != 0) {
		    deprecated_count++;
		}
		if ((scan->addr_flags & IN6_IFF_DETACHED) != 0) {
		    detached_count++;
		}
	    }
	    if (autoconf_address
		|| inet6_addrlist_contains_address(&dhcp_addr_list, scan)) {
		list[count++] = *scan;
	    }
	}
	inet6_addrlist_free(&dhcp_addr_list);
	if (autoconf_count == 0 && rtadv->has_autoconf_address) {
	    my_log(LOG_NOTICE, "RTADV %s: autoconf addresses expired",
		   if_name(if_p));
	    rtadv_submit_awd_report(service_p, FALSE);
	    rtadv_restart(service_p, IFEventID_start_e, NULL);
	    return;
	}
	if (count == 0) {
	    return;
	}
	if (autoconf_count != 0
	    && (detached_count + deprecated_count) == autoconf_count) {
	    my_log(LOG_NOTICE,
		   "RTADV %s: all autoconf addresses detached/deprecated",
		   if_name(if_p));
	}
	rtadv->has_autoconf_address = (autoconf_count != 0);

	bzero(&info, sizeof(info));
	if (rtadv->ra != NULL) {
	    boolean_t	clat46_is_configured;
	    boolean_t	remove_clat46 = TRUE;
	    boolean_t	set_clat46 = FALSE;

	    /* fill in information from DHCPv6 */
	    if (rtadv->dhcp_client != NULL
		&& DHCPv6ClientGetInfo(rtadv->dhcp_client, &info)) {
		if (dhcp_has_address && rtadv->dhcpv6_complete == 0) {
		    rtadv->dhcpv6_complete = timer_get_current_time();
		}
	    }
	    /* this checks whether there's current pvd addinfo to publish */
	    if (PvDInfoContextIsOk(&rtadv->pvd_context)) {
		info.pvd_additional_info_dict
		= PvDInfoContextGetAdditionalInformation(&rtadv->pvd_context);
	    }
	    /* check whether to enable CLAT46 or not */
	    clat46_is_configured = service_clat46_is_configured(service_p);
	    info.ra = rtadv->ra;
	    info.nat64_prefix = copy_nat64_prefix(rtadv->ra);
	    if (info.nat64_prefix != NULL
		|| service_nat64_prefix_available(service_p)) {
		service_clat46_set_is_available(service_p, TRUE);
		if (info.nat64_prefix != NULL) {
		    /* we have PREF64, set it now */
		    rtadv_set_nat64_prefixlist(service_p, info.nat64_prefix);
		}
		if (!service_interface_ipv4_published(service_p)) {
		    /* no IPv4 service published, OK to set CLAT46 */
		    set_clat46 = TRUE;
		    remove_clat46 = FALSE;
		}
	    }
	    else {
		service_clat46_set_is_available(service_p, FALSE);
		if (clat46_is_configured || if_ift_type(if_p) != IFT_CELLULAR) {
		    info.perform_plat_discovery
			= !service_plat_discovery_complete(service_p);
		}
	    }
	    if (remove_clat46) {
		/* remove CLAT46 prefix and CLAT46 address */
		rtadv_set_nat64_prefixlist(service_p, NULL);
		if (rtadv->clat46_partial != 0) {
		    rtadv_remove_clat46_address(service_p, FALSE);
		}
	    }
	    else if (set_clat46) {
		CFStringRef	ifname;

		/* enable CLAT46 */
		ifname = ServiceGetInterfaceName(service_p);
		if (rtadv_set_clat46_address(service_p)) {
		    info.ipv4_dict
			= S_ipv4_clat46_dict_copy(ifname, rtadv->clat46_partial);
		}
	    }
	    router_p = RouterAdvertisementGetSourceIPAddress(rtadv->ra);
	    router_count = 1;
	    signature = create_signature(rtadv->ra, list, count);
	}
	ServicePublishSuccessIPv6(service_p, list, count,
				  router_p, router_count,
				  &info, signature);
	my_CFRelease(&signature);
	my_CFRelease(&info.ipv4_dict);
	my_CFRelease(&info.nat64_prefix);
	if (rtadv->ra != NULL) {
	    rtadv_submit_awd_success_report(service_p);
	    if (rtadv->dhcp_client == NULL
		|| !DHCPv6ClientIsActive(rtadv->dhcp_client)) {
		ServiceSetBusy(service_p, FALSE);
	    }
	    else {
		my_log(LOG_NOTICE, "%s: DHCPv6 client still active",
		       if_name(if_p));
	    }
	}
	if (rtadv->renew) {
	    /* re-assign address to trigger DAD */
	    rtadv_trigger_dad(service_p, list, count);
	    rtadv->renew = FALSE;
	}
    }
    return;
}

STATIC void
rtadv_address_changed(ServiceRef service_p)
{
    inet6_addrlist_t	addrs;

    inet6_addrlist_copy(&addrs,
			if_link_index(service_interface(service_p)));
    rtadv_address_changed_common(service_p, &addrs);
    inet6_addrlist_free(&addrs);
}

STATIC void
rtadv_dhcp_callback(DHCPv6ClientRef client, void * callback_arg,
		    DHCPv6ClientNotificationType type)
{
    ServiceRef		service_p = (ServiceRef)callback_arg;

    switch (type) {
    case kDHCPv6ClientNotificationTypeStatusChanged:
	rtadv_address_changed(service_p);
	break;
    case kDHCPv6ClientNotificationTypeGenerateSymptom:
	ServiceGenerateFailureSymptom(service_p);
	break;
    default:
	break;
    }
    return;
}

STATIC void
rtadv_init(ServiceRef service_p)
{
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);

    rtadv->try = 0;
    rtadv_address_changed(service_p);
    ServiceSetBusy(service_p, TRUE);
    return;
}

STATIC void
rtadv_provide_summary(ServiceRef service_p, CFMutableDictionaryRef summary)
{
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);

    CFMutableDictionaryRef	dict;

    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    my_CFDictionarySetCString(dict, CFSTR("State"),
			      rtadv_state_get_string(rtadv->state));
    if (rtadv->ra != NULL) {
	CFStringRef	str;

	str = RouterAdvertisementCopyDescription(rtadv->ra);
	if (str != NULL) {
	    CFDictionarySetValue(dict, CFSTR("RouterAdvertisement"), str);
	    CFRelease(str);
	}
    }
    if (service_clat46_is_configured(service_p)) {
	CFDictionarySetValue(dict, CFSTR("CLAT46Enabled"), kCFBooleanTrue);
    }
    if (service_clat46_is_active(service_p)) {
	CFDictionarySetValue(dict, CFSTR("CLAT46Active"), kCFBooleanTrue);
    }
    CFDictionarySetValue(summary, CFSTR("RTADV"), dict);
    CFRelease(dict);
    if (rtadv->dhcp_client != NULL) {
        DHCPv6ClientProvideSummary(rtadv->dhcp_client, summary);
    }
    return;
}

/*
 * Function: rtadv_handle_wake
 * Purpose:
 *   If DNS information is present in the RA and has not expired, set a new
 *   expiration timer. If the DNS information has expired, return `true`.
 *
 * Returns:
 *   `true` if we need to call `rtadv_init()`, false otherwise.
 *
 * Note:
 *   This function assumes that the link status is active.
 */
STATIC bool
rtadv_handle_wake(ServiceRef service_p)
{
    CFAbsoluteTime	earliest_expiration;
    CFAbsoluteTime	pvd_info_expiration;
    bool		has_dns = false;
    bool		dns_has_expired = false;
    interface_t *	if_p = service_interface(service_p);
    bool		need_init = false;
    CFAbsoluteTime	now;
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);

    /* get the DNS expiration time */
    if (rtadv->ra == NULL) {
	/* no RA, no DNS */
	goto done;
    }
    now = timer_get_current_time();
    earliest_expiration
	= RouterAdvertisementGetDNSExpirationTime(rtadv->ra, now,
						  &has_dns, &dns_has_expired);
    if (!has_dns || !PvDInfoContextIsOk(&rtadv->pvd_context)) {
	/* there's no DNS nor PvD, nothing to wake for */
	goto done;
    }
    if (dns_has_expired) {
	/* DNS has expired */
	need_init = true;
	my_log(LOG_NOTICE, "RTADV %s: DNS expired", if_name(if_p));
	timer_cancel(rtadv->timer);
	goto done;
    }
    /* pick earliest of DNS or PvD expirations for new timer */
    pvd_info_expiration
    = PvDInfoContextGetEffectiveExpirationTime(&rtadv->pvd_context);
    if (pvd_info_expiration != 0
	&& pvd_info_expiration < earliest_expiration) {
	earliest_expiration = pvd_info_expiration;
	/* pvd info has expired, schedule new fetch */
	rtadv_pvd_additional_info_schedule_fetch(service_p);
    }
    /* set a new expiration timer */
    if (earliest_expiration != 0) {
	CFDateRef		date;
	
	date = CFDateCreate(NULL, earliest_expiration);
	my_log(LOG_INFO, "%s %s: DNS expiration time %@",
	       __func__, if_name(if_p), date);
	CFRelease(date);
	timer_callout_set_absolute(rtadv->timer,
				   earliest_expiration,
				   (timer_func_t *)rtadv_acquired,
				   service_p,
				   (void *)IFEventID_timeout_e,
				   NULL);
    }

 done:
    return (need_init);
}

STATIC void
rtadv_start(ServiceRef service_p, __unused void * arg2, __unused void * arg3)
{
    interface_t *	if_p = service_interface(service_p);

    my_log(LOG_NOTICE, "RTADV %s: start", if_name(if_p));
    rtadv_init(service_p);
}

STATIC void
rtadv_schedule_start(ServiceRef service_p)
{
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    struct timeval	tv;

    tv.tv_sec = 0;
    tv.tv_usec = 0;
    timer_set_relative(rtadv->timer, tv,
		       (timer_func_t *)rtadv_start,
		       service_p, NULL, NULL);
}

PRIVATE_EXTERN ipconfig_status_t
rtadv_thread(ServiceRef service_p, IFEventID_t evid, void * event_data)
{
    interface_t *	if_p = service_interface(service_p);
    Service_rtadv_t *	rtadv = (Service_rtadv_t *)ServiceGetPrivate(service_p);
    ipconfig_status_t	status = ipconfig_status_success_e;
    char		timer_name[32];

    if (evid != IFEventID_start_e && rtadv == NULL) {
	my_log(LOG_NOTICE, "RTADV %s: is NULL", if_name(if_p));
	return (ipconfig_status_internal_error_e);
    }

    switch (evid) {
    case IFEventID_start_e:
	if (if_flags(if_p) & IFF_LOOPBACK) {
	    status = ipconfig_status_invalid_operation_e;
	    break;
	}
	if (rtadv != NULL) {
	    my_log(LOG_NOTICE, "RTADV %s: re-entering start state",
		   if_name(if_p));
	    status = ipconfig_status_internal_error_e;
	    break;
	}
	rtadv = malloc(sizeof(*rtadv));
	if (rtadv == NULL) {
	    my_log(LOG_NOTICE, "RTADV %s: malloc failed", if_name(if_p));
	    status = ipconfig_status_allocation_failed_e;
	    break;
	}
	bzero(rtadv, sizeof(*rtadv));
	ServiceSetPrivate(service_p, rtadv);
	snprintf(timer_name, sizeof(timer_name), "rtadv-%s",
		 if_name(if_p));
	rtadv->timer = timer_callout_init(timer_name);
	if (rtadv->timer == NULL) {
	    my_log(LOG_NOTICE, "RTADV %s: timer_callout_init failed",
		   if_name(if_p));
	    status = ipconfig_status_allocation_failed_e;
	    goto stop;
	}
	rtadv->sock = RTADVSocketCreate(if_p);
	if (rtadv->sock == NULL) {
	    my_log(LOG_NOTICE, "RTADV %s: RTADVSocketCreate failed",
		   if_name(if_p));
	    status = ipconfig_status_allocation_failed_e;
	    goto stop;
	}
	if (G_dhcpv6_enabled) {
	    if (service_disable_dhcpv6(service_p)) {
		my_log(LOG_NOTICE, "RTADV %s: DHCPv6 client is disabled",
		       if_name(if_p));
	    }
	    else {
		rtadv->dhcp_client = DHCPv6ClientCreate(service_p);
		DHCPv6ClientSetNotificationCallBack(rtadv->dhcp_client,
						    rtadv_dhcp_callback,
						    service_p);
	    }
	}
	/* clear out prefix (in case of crash) */
	rtadv_set_nat64_prefixlist(service_p, NULL);
	rtadv_schedule_start(service_p);
	break;

    stop:
    case IFEventID_stop_e:
	my_log(LOG_NOTICE, "RTADV %s: stop", if_name(if_p));

	/* generate/submit AWD report */
	if (!ServiceIsPublished(service_p)) {
	    rtadv_submit_awd_report(service_p, FALSE);
	}

	/* common clean-up */
	rtadv_failed(service_p, ipconfig_status_resource_unavailable_e);

	/* close/release the RTADV socket */
	RTADVSocketRelease(&rtadv->sock);

	/* stop DHCPv6 client */
	DHCPv6ClientRelease(&rtadv->dhcp_client);

	/* clean-up resources */
	if (rtadv->timer) {
	    timer_callout_free(&rtadv->timer);
	}
	inet6_flush_prefixes(if_name(if_p));
	inet6_flush_routes(if_name(if_p));
	ServiceSetPrivate(service_p, NULL);
	free(rtadv);
	break;

    case IFEventID_ipv6_address_changed_e:
	if (rtadv->dhcp_client != NULL) {
	    DHCPv6ClientHandleEvent(rtadv->dhcp_client, evid, event_data);
	}
	rtadv_address_changed_common(service_p, event_data);
	break;

    case IFEventID_wake_e:
    case IFEventID_renew_e:
    case IFEventID_link_status_changed_e: {
	link_event_data_t	link_event = (link_event_data_t)event_data;
	link_status_t *		link_status_p;

	link_status_p = &link_event->link_status;
	if (link_status_is_active(link_status_p)) {
	    boolean_t		need_init = FALSE;
	    boolean_t		network_changed;

	    network_changed = (link_event->info == kLinkInfoNetworkChanged);
	    if (network_changed) {
		rtadv->restart_count = 0;
		rtadv_flush(service_p);
		need_init = TRUE;
	    }
	    else if (evid == IFEventID_renew_e) {
		need_init = TRUE;
	    }
	    else {
		if (evid == IFEventID_wake_e) {
		    need_init = rtadv_handle_wake(service_p);
		}
		else {
		    need_init = TRUE;
		}
		if (rtadv->try == 1 && rtadv->ra == NULL) {
		    /* we're already on it, don't call init (debounce) */
		    need_init = FALSE;
		}
	    }
	    if (evid == IFEventID_renew_e
		&& if_ift_type(if_p) == IFT_CELLULAR) {
		rtadv->renew = TRUE;
	    }
	    if (need_init) {
		rtadv_init(service_p);
	    }
	}
	else {
	    rtadv->try = 0;
	}
	if (rtadv->dhcp_client != NULL) {
	    DHCPv6ClientHandleEvent(rtadv->dhcp_client, evid, event_data);
	}
	break;
    }
    case IFEventID_bssid_changed_e:
	if (rtadv->dhcp_client != NULL) {
	    DHCPv6ClientHandleEvent(rtadv->dhcp_client, evid, event_data);
	}
	break;

    case IFEventID_link_timer_expired_e:
	rtadv_inactive(service_p);
	if (rtadv->dhcp_client != NULL) {
	    DHCPv6ClientStop(rtadv->dhcp_client);
	}
	break;

    case IFEventID_get_ipv6_info_e: {
	ipv6_info_t *		info_p = (ipv6_info_t *)event_data;

	if (rtadv->dhcp_client != NULL) {
	    (void)DHCPv6ClientGetInfo(rtadv->dhcp_client, info_p);
	}
	if (rtadv->ra != NULL) {
	    info_p->ra = rtadv->ra;
	}
	break;
    }
    case IFEventID_ipv6_router_expired_e:
	rtadv_router_expired(service_p,
			     (ipv6_router_prefix_counts_t *)event_data);
	break;

    case IFEventID_plat_discovery_complete_e: {
	boolean_t	success;

	if (!service_clat46_is_configured(service_p)) {
	    rtadv_address_changed(service_p);
	    break;
	}
	if (event_data != NULL) {
	    success = *((boolean_t *)event_data);
	}
	else {
	    success = FALSE;
	}
	if (success) {
	    rtadv_address_changed(service_p);
	}
	else {
	    /* generate failure metric */
	    my_log(LOG_NOTICE, "RTADV %s: PLAT discovery failed",
		   if_name(if_p));
	    rtadv_submit_awd_report(service_p, FALSE);
	}
	break;
    }
    case IFEventID_ipv4_publish_e:
	rtadv_address_changed(service_p);
	break;
    case IFEventID_provide_summary_e:
	rtadv_provide_summary(service_p, (CFMutableDictionaryRef)event_data);
	break;
    default:
	break;
    } /* switch */

    return (status);
}
