/*
 * Copyright (c) 2009-2022 Apple Inc. All rights reserved.
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
 * DHCPv6Client.c
 * - API's to instantiate and interact with DHCPv6Client
 */

/* 
 * Modification History
 *
 * September 22, 2009		Dieter Siegmund (dieter@apple.com)
 * - created
 *
 * May 14, 2010			Dieter Siegmund (dieter@apple.com)
 * - implemented stateful support
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <mach/boolean.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include "DHCPv6.h"
#include "DHCPv6Client.h"
#include "DHCPv6Options.h"
#include "DHCPv6Socket.h"
#include "DHCPDUIDIAID.h"
#include "timer.h"
#include "ipconfigd_threads.h"
#include "ipconfigd_types.h"
#include "interfaces.h"
#include "cfutil.h"
#include "DNSNameList.h"
#include "symbol_scope.h"

typedef struct {
    DHCPv6OptionCode	code;
    int			min_length;
} DHCPv6OptionInfo;

typedef const DHCPv6OptionInfo * DHCPv6OptionInfoRef;

STATIC const DHCPv6OptionInfo IA_NA_OptionInfo = {
   .code = kDHCPv6OPTION_IA_NA,
   .min_length = DHCPv6OptionIA_NA_MIN_LENGTH
};

STATIC const DHCPv6OptionInfo IAADDR_OptionInfo = {
   .code = kDHCPv6OPTION_IAADDR,
   .min_length = DHCPv6OptionIAADDR_MIN_LENGTH
};

STATIC const DHCPv6OptionInfo IA_PD_OptionInfo = {
   .code = kDHCPv6OPTION_IA_PD,
   .min_length = DHCPv6OptionIA_PD_MIN_LENGTH
};

STATIC const DHCPv6OptionInfo IAPREFIX_OptionInfo = {
   .code = kDHCPv6OPTION_IAPREFIX,
   .min_length = DHCPv6OptionIAPREFIX_MIN_LENGTH
};

#if TEST_DHCPV6_CLIENT
#include <SystemConfiguration/SCPrivate.h>
#undef my_log
#define my_log(pri, format, ...)	do {		\
	struct timeval	tv;				\
	struct tm       tm;				\
	time_t		t;				\
							\
	(void)gettimeofday(&tv, NULL);					\
	t = tv.tv_sec;							\
	(void)localtime_r(&t, &tm);					\
									\
	SCPrint(TRUE, stdout,						\
		CFSTR("%04d/%02d/%02d %2d:%02d:%02d.%06d " format "\n"), \
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,		\
		tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_usec,		\
		## __VA_ARGS__ );					\
    } while (0)
#endif

typedef void
(DHCPv6ClientEventFunc)(DHCPv6ClientRef client, IFEventID_t event_id, 
			void * event_data);
typedef DHCPv6ClientEventFunc * DHCPv6ClientEventFuncRef;

typedef enum {
    kDHCPv6ClientStateInactive = 0,
    kDHCPv6ClientStateSolicit,
    kDHCPv6ClientStateRequest,
    kDHCPv6ClientStateBound,
    kDHCPv6ClientStateRenew,
    kDHCPv6ClientStateRebind,
    kDHCPv6ClientStateConfirm,
    kDHCPv6ClientStateRelease,
    kDHCPv6ClientStateUnbound,
    kDHCPv6ClientStateDecline,
    kDHCPv6ClientStateInform,
    kDHCPv6ClientStateInformComplete,
} DHCPv6ClientState;

STATIC const char *
DHCPv6ClientStateGetName(DHCPv6ClientState cstate)
{
    STATIC const char * names[] = {
	"Inactive",
	"Solicit",
	"Request",
	"Bound",
	"Renew",
	"Rebind",
	"Confirm",
	"Release",
	"Unbound",
	"Decline",
	"Inform",
	"InformComplete",
    };
    if (cstate >= 0 && cstate < countof(names)) {
	return (names[cstate]);
    }
    return ("<unknown>");
}

INLINE bool
S_dhcp_state_is_bound_renew_or_rebind(DHCPv6ClientState state)
{
    bool	ret;

    switch (state) {
    case kDHCPv6ClientStateBound:
    case kDHCPv6ClientStateRenew:
    case kDHCPv6ClientStateRebind:
	ret = true;
	break;
    default:
	ret = false;
	break;
    }
    return (ret);
}

STATIC const char *
DHCPv6ClientModeGetName(DHCPv6ClientMode mode)
{
    STATIC const char * names[] = {
	"None",
	"Stateless",
	"Stateful",
	"PrefixDelegation"
    };
    if (mode >= 0 && mode < countof(names)) {
	return (names[mode]);
    }
    return ("<unknown>");
}

STATIC const char *
DHCPv6ClientModeGetShortName(DHCPv6ClientMode mode)
{
    STATIC const char * names[] = {
	"None",
	"IR",
	"NA",
	"PD"
    };
    if (mode >= 0 && mode < countof(names)) {
	return (names[mode]);
    }
    return ("<unknown>");
}

typedef struct {
    CFAbsoluteTime		start;

    /* these times are all relative to start */
    uint32_t			t1;
    uint32_t			t2;
    uint32_t			valid_lifetime;
    uint32_t			preferred_lifetime;
    bool			valid;
    /* if this is a Wi-Fi network, remember the SSID */
    CFStringRef			ssid;
} lease_info, * lease_info_t;

typedef struct {
    DHCPv6PacketRef		pkt;
    int				pkt_len;
    DHCPv6OptionListRef		options;
} dhcpv6_info, * dhcpv6_info_t;

typedef union {
    DHCPv6OptionIA_NARef	ia_na;		/* points to saved */
    DHCPv6OptionIA_PDRef	ia_pd;		/* points to saved */
    const uint8_t *		ptr;
} IA_NA_PD_U, * IA_NA_PD_URef;

typedef union {
    DHCPv6OptionIAADDRRef	iaaddr;		/* points to saved */
    DHCPv6OptionIAPREFIXRef	iaprefix;	/* points to saved */
    const uint8_t *		ptr;
} ADDR_PREFIX_U, * ADDR_PREFIX_URef;

struct DHCPv6Client {
    char			description[32];
    CFRunLoopSourceRef		callback_rls;
    DHCPv6ClientNotificationCallBack callback;
    void *			callback_arg;
    struct in6_addr		our_ip;
    uint8_t			our_prefix_length;
    struct in6_addr		delegated_prefix;
    uint8_t			delegated_prefix_length;
    struct in6_addr		requested_prefix;
    uint8_t			requested_prefix_length;
    DHCPv6ClientMode		mode;
    DHCPv6ClientState		cstate;
    DHCPv6SocketRef		sock;
    ServiceRef			service_p;
    timer_callout_t *		timer;
    DHCPv6TransactionID		transaction_id;
    int				try;
    CFAbsoluteTime		start_time;
    CFTimeInterval		retransmit_time;
    dhcpv6_info			saved;
    lease_info			lease;
    bool			saved_verified;
    bool			private_address;
    CFDataRef			duid;
    CFAbsoluteTime		renew_rebind_time;
    DHCPDUIDRef			server_id; 	/* points to saved */
    IA_NA_PD_U			ia_na_pd;
    ADDR_PREFIX_U		addr_prefix;
};

static inline DHCPv6OptionIA_NARef
DHCPv6ClientGetIA_NA(DHCPv6ClientRef client)
{
    return (client->ia_na_pd.ia_na);
}

static inline DHCPv6OptionIA_PDRef
DHCPv6ClientGetIA_PD(DHCPv6ClientRef client)
{
    return (client->ia_na_pd.ia_pd);
}

static inline DHCPv6OptionIAADDRRef
DHCPv6ClientGetIAADDR(DHCPv6ClientRef client)
{
    return (client->addr_prefix.iaaddr);
}

static inline DHCPv6OptionIAPREFIXRef
DHCPv6ClientGetIAPREFIX(DHCPv6ClientRef client)
{
    return (client->addr_prefix.iaprefix);
}

STATIC DHCPv6ClientEventFunc	DHCPv6Client_Bound;
STATIC DHCPv6ClientEventFunc	DHCPv6Client_Unbound;
STATIC DHCPv6ClientEventFunc	DHCPv6Client_Solicit;
STATIC DHCPv6ClientEventFunc	DHCPv6Client_Request;
STATIC DHCPv6ClientEventFunc	DHCPv6Client_RenewRebind;

INLINE interface_t *
DHCPv6ClientGetInterface(DHCPv6ClientRef client)
{
    return (DHCPv6SocketGetInterface(client->sock));
}

STATIC bool
DHCPv6ClientVerifyModeIsNone(DHCPv6ClientRef client, const char * func)
{
    interface_t *	if_p = DHCPv6ClientGetInterface(client);

    if (client->mode != kDHCPv6ClientModeNone) {
	my_log(LOG_NOTICE, "%s(%s): mode is %s, ignoring",
	       func, if_name(if_p), DHCPv6ClientModeGetName(client->mode));
	return (false);
    }
    return (true);
}

STATIC const uint16_t	DHCPv6RequestedOptionsStatic[] = {
    kDHCPv6OPTION_DNS_SERVERS,
    kDHCPv6OPTION_DOMAIN_LIST,
    kDHCPv6OPTION_CAPTIVE_PORTAL_URL
};

STATIC void
DHCPv6ClientSetDescription(DHCPv6ClientRef client)
{
    interface_t * 		if_p = service_interface(client->service_p);

    if (client->mode == kDHCPv6ClientModeNone) {
	snprintf(client->description, sizeof(client->description),
		 "DHCPv6 %s", if_name(if_p));
    }
    else {
	snprintf(client->description, sizeof(client->description),
		 "DHCPv6-%s %s",
		 DHCPv6ClientModeGetShortName(client->mode),
		 if_name(if_p));
    }
    return;
}

STATIC const char *
DHCPv6ClientGetDescription(DHCPv6ClientRef client)
{
    return (client->description);
}

#define kDHCPv6RequestedOptionsStaticCount 	(sizeof(DHCPv6RequestedOptionsStatic) / sizeof(DHCPv6RequestedOptionsStatic[0]))

STATIC uint16_t *	DHCPv6RequestedOptionsDefault = (uint16_t *)DHCPv6RequestedOptionsStatic;
STATIC int		DHCPv6RequestedOptionsDefaultCount = kDHCPv6RequestedOptionsStaticCount;

STATIC uint16_t *	DHCPv6RequestedOptions = (uint16_t *)DHCPv6RequestedOptionsStatic;
STATIC int		DHCPv6RequestedOptionsCount =  kDHCPv6RequestedOptionsStaticCount;

STATIC int
S_get_prefix_length(const struct in6_addr * addr, int if_index)
{
    int	prefix_length;

    prefix_length = inet6_get_prefix_length(addr, if_index);
    if (prefix_length == 0) {
#define DHCPV6_PREFIX_LENGTH		128
	prefix_length = DHCPV6_PREFIX_LENGTH;
    }
    return (prefix_length);
}

PRIVATE_EXTERN void
DHCPv6ClientSetRequestedOptions(uint16_t * requested_options,
				int requested_options_count)
{
    if (requested_options != NULL && requested_options_count != 0) {
	DHCPv6RequestedOptionsDefault = requested_options;
	DHCPv6RequestedOptionsDefaultCount = requested_options_count;
    }
    else {
	DHCPv6RequestedOptionsDefault 
	    = (uint16_t *)DHCPv6RequestedOptionsStatic;
	DHCPv6RequestedOptionsDefaultCount 
	    = kDHCPv6RequestedOptionsStaticCount;
    }
    DHCPv6RequestedOptions = DHCPv6RequestedOptionsDefault;
    DHCPv6RequestedOptionsCount = DHCPv6RequestedOptionsDefaultCount;
    return;
}

PRIVATE_EXTERN bool
DHCPv6ClientOptionIsOK(int option)
{
    int i;

    switch (option) {
    case kDHCPv6OPTION_CLIENTID:
    case kDHCPv6OPTION_SERVERID:
    case kDHCPv6OPTION_ORO:
    case kDHCPv6OPTION_ELAPSED_TIME:
    case kDHCPv6OPTION_UNICAST:
    case kDHCPv6OPTION_RAPID_COMMIT:
    case kDHCPv6OPTION_IA_NA:
    case kDHCPv6OPTION_IAADDR:
    case kDHCPv6OPTION_STATUS_CODE:
    case kDHCPv6OPTION_IA_TA:
    case kDHCPv6OPTION_PREFERENCE:
    case kDHCPv6OPTION_RELAY_MSG:
    case kDHCPv6OPTION_AUTH:
    case kDHCPv6OPTION_USER_CLASS:
    case kDHCPv6OPTION_VENDOR_CLASS:
    case kDHCPv6OPTION_VENDOR_OPTS:
    case kDHCPv6OPTION_INTERFACE_ID:
    case kDHCPv6OPTION_RECONF_MSG:
    case kDHCPv6OPTION_RECONF_ACCEPT:
    case kDHCPv6OPTION_IA_PD:
    case kDHCPv6OPTION_IAPREFIX:
	return (true);
    default:
	break;
    }
    for (i = 0; i < DHCPv6RequestedOptionsCount; i++) {
	if (DHCPv6RequestedOptions[i] == option) {
	    return (true);
	}
    }
    return (false);
}

STATIC double
random_double_in_range(double bottom, double top)
{
    double		r = (double)arc4random() / (double)UINT32_MAX;
    
    return (bottom + (top - bottom) * r);
}

STATIC void
DHCPv6ClientLogAddressOrPrefix(DHCPv6ClientRef client,
			       DHCPv6MessageType msg_type,
			       bool is_addr,
			       ADDR_PREFIX_U * apu)
{
    const uint8_t *		addr;
    char 			ntopbuf[INET6_ADDRSTRLEN];
    DHCPv6OptionCode		option_code;
    uint32_t			preferred;
    char			prefix_length_buf[8];
    uint32_t			valid;

    if (is_addr) {
	option_code = kDHCPv6OPTION_IAADDR;
	addr = DHCPv6OptionIAADDRGetAddress(apu->iaaddr);
	preferred = DHCPv6OptionIAADDRGetPreferredLifetime(apu->iaaddr);
	valid = DHCPv6OptionIAADDRGetValidLifetime(apu->iaaddr);
	prefix_length_buf[0] = '\0';
    }
    else {
	uint8_t		prefix_length;

	option_code = kDHCPv6OPTION_IAPREFIX;
	addr = DHCPv6OptionIAPREFIXGetPrefix(apu->iaprefix);
	prefix_length = DHCPv6OptionIAPREFIXGetPrefixLength(apu->iaprefix);
	preferred = DHCPv6OptionIAPREFIXGetPreferredLifetime(apu->iaprefix);
	valid = DHCPv6OptionIAPREFIXGetValidLifetime(apu->iaprefix);
	snprintf(prefix_length_buf, sizeof(prefix_length_buf),
		 "/%d", prefix_length);
    }
    my_log(LOG_INFO,
	   "%s: %s Received %s (try=%d) %s %s%s Preferred %d Valid=%d",
	   DHCPv6ClientGetDescription(client),
	   DHCPv6ClientStateGetName(client->cstate),
	   DHCPv6MessageTypeName(msg_type),
	   client->try,
	   DHCPv6OptionCodeGetName(option_code),
	   inet_ntop(AF_INET6, addr, ntopbuf, sizeof(ntopbuf)),
	   prefix_length_buf,
	   preferred,
	   valid);
}

STATIC bool
DHCPv6ClientUsePrivateAddress(DHCPv6ClientRef client)
{
    return (client->private_address);
}

PRIVATE_EXTERN void
DHCPv6ClientSetUsePrivateAddress(DHCPv6ClientRef client,
				 bool use_private_address)
{
    if (!DHCPv6ClientVerifyModeIsNone(client, __func__)) {
	return;
    }
    client->private_address = use_private_address;
}

STATIC void
DHCPv6ClientSetSSID(DHCPv6ClientRef client, CFStringRef ssid)
{
    if (ssid != NULL) {
	CFRetain(ssid);
    }
    my_CFRelease(&client->lease.ssid);
    client->lease.ssid = ssid;
}

STATIC CFDataRef
DHCPv6ClientGetDUID(DHCPv6ClientRef client)
{
    STATIC CFDataRef	duid;

    if (DHCPv6ClientUsePrivateAddress(client)) {
	if (client->duid == NULL) {
	    interface_t * if_p;

	    if_p = DHCPv6ClientGetInterface(client);
	    client->duid = DHCPDUIDCopy(if_p);
	}
	return (client->duid);
    }
    if (duid == NULL) {
	duid = DHCPDUIDEstablishAndGet(G_dhcp_duid_type);
    }
    return (duid);
}

STATIC DHCPIAID
DHCPv6ClientGetIAID(DHCPv6ClientRef client)
{
    interface_t * if_p;

    if (DHCPv6ClientUsePrivateAddress(client)) {
	/* we have our own address space */
	return (0);
    }
    if_p = DHCPv6ClientGetInterface(client);
    return (DHCPIAIDGet(if_name(if_p)));
}

STATIC bool
S_insert_duid(DHCPv6ClientRef client, DHCPv6OptionAreaRef oa_p)
{
    CFDataRef			data;
    DHCPv6OptionErrorString 	err;

    data = DHCPv6ClientGetDUID(client);
    if (data == NULL) {
	return (false);
    }
    if (!DHCPv6OptionAreaAddOption(oa_p, kDHCPv6OPTION_CLIENTID,
				   (int)CFDataGetLength(data),
				   CFDataGetBytePtr(data),
				   &err)) {
	my_log(LOG_NOTICE, "DHCPv6Client: failed to add CLIENTID, %s",
	       err.str);
	return (false);
    }
    return (true);
}

STATIC bool
S_duid_matches(DHCPv6ClientRef client, DHCPv6OptionListRef options)
{
    CFDataRef		data;
    DHCPDUIDRef		duid;
    int			option_len;

    data = DHCPv6ClientGetDUID(client);
    duid = (DHCPDUIDRef)
	DHCPv6OptionListGetOptionDataAndLength(options,
					       kDHCPv6OPTION_CLIENTID,
					       &option_len, NULL);
    if (duid == NULL
	|| CFDataGetLength(data) != option_len
	|| bcmp(duid, CFDataGetBytePtr(data), option_len) != 0) {
	return (false);
    }
    return (true);
}

STATIC IA_NA_PD_U
get_ia_na_pd_code(DHCPv6ClientRef client, DHCPv6MessageType msg_type,
		  bool get_ia_na,
		  DHCPv6OptionListRef options, ADDR_PREFIX_URef ret_addr_prefix,
		  DHCPv6StatusCode * ret_code)
{
    DHCPv6OptionInfoRef 	addr_prefix_info;
    DHCPv6OptionErrorString 	err;
    const char *		msg;
    unsigned int		msg_length;
    DHCPv6OptionInfoRef 	na_pd_info;
    int				option_len;
    uint32_t			preferred;
    IA_NA_PD_U			ret_ia_na_pd;
    DHCPv6StatusCode		status_code = kDHCPv6StatusCodeSuccess;
    DHCPv6OptionListRef		sub_options = NULL;
    uint8_t *			sub_option_buffer;
    uint32_t			t1;
    uint32_t			t2;
    uint32_t			valid;

    ret_addr_prefix->ptr = NULL;
    if (get_ia_na) {
	na_pd_info = &IA_NA_OptionInfo;
	addr_prefix_info = &IAADDR_OptionInfo;
    }
    else {
	na_pd_info = &IA_PD_OptionInfo;
	addr_prefix_info = &IAPREFIX_OptionInfo;
    }

    /* look for the required option (IA_NA or IA_PD) */
    ret_ia_na_pd.ptr
	= DHCPv6OptionListGetOptionDataAndLength(options,
						 na_pd_info->code,
						 &option_len, NULL);
    if (ret_ia_na_pd.ptr == NULL
	|| option_len <= na_pd_info->min_length) {
	/* option not present */
	goto done;
    }
    if (get_ia_na) {
	t1 = DHCPv6OptionIA_NAGetT1(ret_ia_na_pd.ia_na);
	t2 = DHCPv6OptionIA_NAGetT2(ret_ia_na_pd.ia_na);
    }
    else {
	t1 = DHCPv6OptionIA_PDGetT1(ret_ia_na_pd.ia_pd);
	t2 = DHCPv6OptionIA_PDGetT2(ret_ia_na_pd.ia_pd);
    }
    if (t1 != 0 && t2 != 0) {
	if (t1 > t2) {
	    /* server is confused */
	    goto done;
	}
    }
    option_len -= na_pd_info->min_length;
    if (get_ia_na) {
	sub_option_buffer = ret_ia_na_pd.ia_na->options;
    }
    else {
	sub_option_buffer = ret_ia_na_pd.ia_pd->options;
    }
    sub_options = DHCPv6OptionListCreate(sub_option_buffer, option_len, &err);
    if (sub_options == NULL) {
	my_log(LOG_INFO,
	       "%s: %s %s contains no options",
	       DHCPv6ClientGetDescription(client),
	       DHCPv6OptionCodeGetName(na_pd_info->code),
	       DHCPv6MessageTypeName(msg_type));
	goto done;
    }
    if (!DHCPv6OptionListGetStatusCode(sub_options, &status_code,
				       &msg, &msg_length)) {
	/* ignore bad data */
	goto done;
    }
    if (status_code != kDHCPv6StatusCodeSuccess) {
	my_log(LOG_INFO,
	       "%s: %s Status %s '%.*s'",
	       DHCPv6ClientGetDescription(client),
	       DHCPv6OptionCodeGetName(na_pd_info->code),
	       DHCPv6StatusCodeGetName(status_code),
	       msg_length, msg);
    }

    /* find the first IAADDR/IAPREFIX with non-zero lifetime */
    for (int start_index = 0; true; start_index++) {
	ADDR_PREFIX_U	apu;

	apu.ptr =
	    DHCPv6OptionListGetOptionDataAndLength(sub_options,
						   addr_prefix_info->code,
						   &option_len, &start_index);
	if (apu.ptr == NULL
	    || option_len < addr_prefix_info->min_length) {
	    my_log(LOG_INFO,
		   "%s: %s %s contains no valid %s option",
		   DHCPv6ClientGetDescription(client),
		   DHCPv6MessageTypeName(msg_type),
		   DHCPv6OptionCodeGetName(na_pd_info->code),
		   DHCPv6OptionCodeGetName(addr_prefix_info->code));
	    /* missing/invalid IAADDR/IAPREFIX option */
	    break;
	}
	if (get_ia_na) {
		valid = DHCPv6OptionIAADDRGetValidLifetime(apu.iaaddr);
		preferred
		    = DHCPv6OptionIAADDRGetPreferredLifetime(apu.iaaddr);
	}
	else {
		valid = DHCPv6OptionIAPREFIXGetValidLifetime(apu.iaprefix);
		preferred
		    = DHCPv6OptionIAPREFIXGetPreferredLifetime(apu.iaprefix);
	}
	if (valid == 0 || preferred == 0) {
	    my_log(LOG_INFO,
		   "%s: %s %s has valid/preferred lifetime 0, skipping",
		   DHCPv6ClientGetDescription(client),
		   DHCPv6MessageTypeName(msg_type),
		   DHCPv6OptionCodeGetName(addr_prefix_info->code));
	}
	else if (preferred > valid) {
	    /* server is confused */
	    my_log(LOG_INFO,
		   "%s: %s %s preferred %d > valid lifetime %d",
		   DHCPv6ClientGetDescription(client),
		   DHCPv6MessageTypeName(msg_type),
		   DHCPv6OptionCodeGetName(addr_prefix_info->code),
		   preferred, valid);
	    break;
	}
	else {
	    ret_addr_prefix->ptr = apu.ptr;
	    break;
	}
    }

 done:
    /* if we didn't find a suitable IAADDR/IAPREFIX, ignore the IA_NA/IA_PD */
    if (ret_addr_prefix->ptr == NULL) {
	ret_ia_na_pd.ptr = NULL;
    }
    if (ret_code != NULL) {
	*ret_code = status_code;
    }
    if (sub_options != NULL) {
	DHCPv6OptionListRelease(&sub_options);
    }
    return (ret_ia_na_pd);
}

STATIC IA_NA_PD_U
get_ia_na_pd(DHCPv6ClientRef client, DHCPv6MessageType msg_type, bool get_ia_na,
	     DHCPv6OptionListRef options, ADDR_PREFIX_URef ret_addr_prefix)
{
    return (get_ia_na_pd_code(client, msg_type, get_ia_na,
			      options, ret_addr_prefix, NULL));
}

STATIC uint8_t
get_preference_value_from_options(DHCPv6OptionListRef options)
{
    int				option_len;
    DHCPv6OptionPREFERENCERef	pref;
    uint8_t			value = kDHCPv6OptionPREFERENCEMinValue;

    pref = (DHCPv6OptionPREFERENCERef)
	DHCPv6OptionListGetOptionDataAndLength(options,
					       kDHCPv6OPTION_PREFERENCE,
					       &option_len, NULL);
    if (pref != NULL 
	&& option_len >= DHCPv6OptionPREFERENCE_MIN_LENGTH) {
	value = pref->value;
    }
    return (value);
}

#define OUR_IA_NA_SIZE	(DHCPv6OptionIA_NA_MIN_LENGTH			\
			 + DHCPV6_OPTION_HEADER_SIZE			\
			 + DHCPv6OptionIAADDR_MIN_LENGTH)

STATIC bool
add_ia_na_option(DHCPv6ClientRef client,
		 bool is_solicit,
		 DHCPv6OptionAreaRef oa_p,
		 DHCPv6OptionErrorStringRef err_p)
{
    const uint8_t *		addr;
    char			buf[OUR_IA_NA_SIZE];
    DHCPv6OptionIA_NARef	ia_na;
    unsigned int		ia_na_size;

    ia_na = (DHCPv6OptionIA_NARef)buf;
    ia_na_size = DHCPv6OptionIA_NA_MIN_LENGTH;
    DHCPv6OptionIA_NASetIAID(ia_na, DHCPv6ClientGetIAID(client));
    DHCPv6OptionIA_NASetT1(ia_na, 0);
    DHCPv6OptionIA_NASetT2(ia_na, 0);
    if (!is_solicit) {
	DHCPv6OptionIAADDRRef	iaddr;
	DHCPv6OptionRef		option;

	ia_na_size = OUR_IA_NA_SIZE;
	option = (DHCPv6OptionRef)(buf + DHCPv6OptionIA_NA_MIN_LENGTH);
	DHCPv6OptionSetCode(option, kDHCPv6OPTION_IAADDR);
	DHCPv6OptionSetLength(option, DHCPv6OptionIAADDR_MIN_LENGTH);
	iaddr = (DHCPv6OptionIAADDRRef)
	    (((char *)option) + DHCPV6_OPTION_HEADER_SIZE);
	addr = DHCPv6OptionIAADDRGetAddress(DHCPv6ClientGetIAADDR(client));
	DHCPv6OptionIAADDRSetAddress(iaddr, addr);
	DHCPv6OptionIAADDRSetPreferredLifetime(iaddr, 0);
	DHCPv6OptionIAADDRSetValidLifetime(iaddr, 0);
    }
    return (DHCPv6OptionAreaAddOption(oa_p, kDHCPv6OPTION_IA_NA,
				      ia_na_size, ia_na, err_p));
}

#define OUR_IA_PD_SIZE	(DHCPv6OptionIA_PD_MIN_LENGTH		\
			 + DHCPV6_OPTION_HEADER_SIZE		\
			 + DHCPv6OptionIAPREFIX_MIN_LENGTH)

STATIC bool
add_ia_pd_option(DHCPv6ClientRef client,
		 bool is_solicit,
		 DHCPv6OptionAreaRef oa_p,
		 DHCPv6OptionErrorStringRef err_p)
{
    const uint8_t *		addr;
    char			buf[OUR_IA_PD_SIZE];
    DHCPv6OptionIA_PDRef	ia_pd;
    unsigned int		ia_pd_size;

    ia_pd = (DHCPv6OptionIA_PDRef)buf;
    ia_pd_size = DHCPv6OptionIA_PD_MIN_LENGTH;
    DHCPv6OptionIA_PDSetIAID(ia_pd, DHCPv6ClientGetIAID(client));
    DHCPv6OptionIA_PDSetT1(ia_pd, 0);
    DHCPv6OptionIA_PDSetT2(ia_pd, 0);

    /* need to add IAPREFIX option */
    if (!is_solicit || client->requested_prefix_length != 0) {
	DHCPv6OptionIAPREFIXRef	iaprefix;
	DHCPv6OptionRef		option;
	uint8_t			prefix_length;

	ia_pd_size = OUR_IA_PD_SIZE;
	option = (DHCPv6OptionRef)(buf + DHCPv6OptionIA_PD_MIN_LENGTH);
	DHCPv6OptionSetCode(option, kDHCPv6OPTION_IAPREFIX);
	DHCPv6OptionSetLength(option, DHCPv6OptionIAPREFIX_MIN_LENGTH);
	iaprefix = (DHCPv6OptionIAPREFIXRef)
	    (((char *)option) + DHCPV6_OPTION_HEADER_SIZE);
	if (is_solicit) {
	    addr = (const uint8_t *)&client->requested_prefix;
	    prefix_length = client->requested_prefix_length;
	}
	else {
	    DHCPv6OptionIAPREFIXRef saved_iaprefix;

	    saved_iaprefix = DHCPv6ClientGetIAPREFIX(client);
	    addr = DHCPv6OptionIAPREFIXGetPrefix(saved_iaprefix);
	    prefix_length = DHCPv6OptionIAPREFIXGetPrefixLength(saved_iaprefix);
	}
	DHCPv6OptionIAPREFIXSetPrefix(iaprefix, addr);
	DHCPv6OptionIAPREFIXSetPrefixLength(iaprefix, prefix_length);
	DHCPv6OptionIAPREFIXSetPreferredLifetime(iaprefix, 0);
	DHCPv6OptionIAPREFIXSetValidLifetime(iaprefix, 0);
    }
    return (DHCPv6OptionAreaAddOption(oa_p, kDHCPv6OPTION_IA_PD,
				      ia_pd_size, ia_pd, err_p));
}

/*
 * Function: option_data_get_length
 * Purpose:
 *   Given a pointer to the option data, return its length, which is stored
 *   in the previous 2 bytes.
 */
STATIC int
option_data_get_length(const void * option_data)
{
    const uint16_t *	len_p;

    len_p = (const uint16_t *)(option_data - sizeof(uint16_t));
    return (ntohs(*len_p));
}

STATIC CFTimeInterval
DHCPv6_RAND(void)
{
    return (random_double_in_range(-0.1, 0.1));
}

STATIC CFTimeInterval
DHCPv6SubsequentTimeout(CFTimeInterval RTprev, CFTimeInterval MRT)
{
    CFTimeInterval	RT;

    RT = 2 * RTprev + DHCPv6_RAND() * RTprev;
    if (MRT != 0 && RT > MRT) {
	RT = MRT + DHCPv6_RAND() * MRT;
    }
    return (RT);
}

STATIC CFTimeInterval
DHCPv6InitialTimeout(CFTimeInterval IRT)
{
    return (IRT + DHCPv6_RAND() * IRT);
}

STATIC uint16_t
get_elapsed_time(DHCPv6ClientRef client)
{
    uint16_t	elapsed_time;

    if (client->try == 1) {
	elapsed_time = 0;
    }
    else {
	uint32_t	elapsed;

	/* elapsed time is in 1/100ths of a second */
	elapsed = (timer_get_current_time() - client->start_time) * 100;
#define MAX_ELAPSED	0xffff
	if (elapsed > MAX_ELAPSED) {
	    elapsed_time = MAX_ELAPSED;
	}
	else {
	    elapsed_time = htons(elapsed);
	}
    }
    return (elapsed_time);
}

/**
 ** DHCPv6Client routines
 **/

STATIC void
DHCPv6ClientSetNewTransactionID(DHCPv6ClientRef client)
{
    uint32_t	r = arc4random();

    /* only use the lower 24 bits */
#define LOWER_24_BITS	((uint32_t)0x00ffffff)
    client->transaction_id = (r & LOWER_24_BITS);
    return;
}

STATIC DHCPv6TransactionID
DHCPv6ClientGetTransactionID(DHCPv6ClientRef client)
{
    return (client->transaction_id);
}

PRIVATE_EXTERN bool
DHCPv6ClientIsActive(DHCPv6ClientRef client)
{
    return (DHCPv6SocketReceiveIsEnabled(client->sock));
}

PRIVATE_EXTERN bool
DHCPv6ClientHasDNS(DHCPv6ClientRef client, bool * search_available)
{
    const uint8_t *	search;
    int			search_len;
    const uint8_t *	servers;
    int			servers_len;

    *search_available = false;

    /* check for DNSServers, DNSDomainList options */
    if (client->saved.options == NULL) {
	return (false);
    }
    search = DHCPv6OptionListGetOptionDataAndLength(client->saved.options,
						    kDHCPv6OPTION_DOMAIN_LIST,
						    &search_len, NULL);
    if (search != NULL && search_len > 0) {
	*search_available = true;
    }
    servers = DHCPv6OptionListGetOptionDataAndLength(client->saved.options,
						     kDHCPv6OPTION_DNS_SERVERS,
						     &servers_len, NULL);
    return (servers != NULL && (servers_len / sizeof(struct in6_addr)) != 0);
}

STATIC void
DHCPv6ClientAddPacketDescription(DHCPv6ClientRef client,
				 CFMutableDictionaryRef summary)
{
    dhcpv6_info_t	info = &client->saved;
    CFMutableStringRef	str;

    if (!client->saved_verified || info->pkt == NULL || info->options == NULL) {
	return;
    }
    str = CFStringCreateMutable(NULL, 0);
    DHCPv6PacketPrintToString(str, info->pkt, info->pkt_len);
    DHCPv6OptionListPrintToString(str, info->options);
    CFDictionarySetValue(summary, CFSTR("Packet"), str);
    CFRelease(str);
}

PRIVATE_EXTERN void
DHCPv6ClientProvideSummary(DHCPv6ClientRef client,
			   CFMutableDictionaryRef summary)
{
    CFMutableDictionaryRef	dict;

    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    my_CFDictionarySetCString(dict, CFSTR("State"),
			      DHCPv6ClientStateGetName(client->cstate));
    my_CFDictionarySetCString(dict, CFSTR("Mode"),
			      DHCPv6ClientModeGetName(client->mode));
    if (client->lease.valid && client->saved.pkt_len != 0
	&& client->saved_verified) {
	switch (client->mode) {
	case kDHCPv6ClientModeStatefulAddress:
#define kStatefulAddress	CFSTR("StatefulAddress")
	    if (!IN6_IS_ADDR_UNSPECIFIED(&client->our_ip)) {
		my_CFDictionarySetIPv6AddressAsString(dict,
						      kStatefulAddress,
						      &client->our_ip);
	    }
	    break;
	case kDHCPv6ClientModeStatefulPrefix:
	    if (!IN6_IS_ADDR_UNSPECIFIED(&client->delegated_prefix)) {
#define kDelegatedPrefix	kSCPropNetIPv6DelegatedPrefix
#define kDelegatedPrefixLength	kSCPropNetIPv6DelegatedPrefixLength
		my_CFDictionarySetIPv6AddressAsString(dict,
						      kDelegatedPrefix,
						      &client->delegated_prefix);
		my_CFDictionarySetUInt64(dict,
					 kDelegatedPrefixLength,
					 client->delegated_prefix_length);
	    }
	    break;
	default:
	    break;
	}
	my_CFDictionarySetAbsoluteTime(dict,
				       kSCPropNetDHCPv6LeaseStartTime,
				       client->lease.start);
	if (client->lease.valid_lifetime == DHCP_INFINITE_LEASE) {
	    CFDictionarySetValue(dict, kLeaseIsInfinite, kCFBooleanTrue);
	}
	else {
	    my_CFDictionarySetAbsoluteTime(dict,
					   kSCPropNetDHCPv6LeaseExpirationTime,
					   client->lease.start
					   + client->lease.valid_lifetime);
	}
    }
    else {
	CFAbsoluteTime	current_time;

	switch (client->cstate) {
	case kDHCPv6ClientStateSolicit:
	case kDHCPv6ClientStateRequest:
	case kDHCPv6ClientStateConfirm:
	case kDHCPv6ClientStateInform:
	    /* we're trying, so give some idea of the elapsed time */
	    current_time = timer_get_current_time();
	    if (current_time > client->start_time) {
		CFTimeInterval	delta = current_time - client->start_time;

		my_CFDictionarySetUInt64(dict, CFSTR("ElapsedTime"),
					 delta);
	    }
	    break;
	default:
	    break;
	}
    }
    DHCPv6ClientAddPacketDescription(client, dict);
    CFDictionarySetValue(summary, CFSTR("DHCPv6"), dict);
    CFRelease(dict);
    return;
}

STATIC void
DHCPv6ClientSetState(DHCPv6ClientRef client, DHCPv6ClientState cstate)
{
    client->cstate = cstate;
    my_log(LOG_INFO, "%s: %s",
	   DHCPv6ClientGetDescription(client),
	   DHCPv6ClientStateGetName(cstate));
}

STATIC void
DHCPv6ClientRemoveAddress(DHCPv6ClientRef client, const char * label)
{
    interface_t *	if_p = DHCPv6ClientGetInterface(client);
    char 		ntopbuf[INET6_ADDRSTRLEN];
    int			s;

    if (IN6_IS_ADDR_UNSPECIFIED(&client->our_ip)) {
	return;
    }
    my_log(LOG_INFO,
	   "%s: %s: removing %s",
	   DHCPv6ClientGetDescription(client),
	   label,
	   inet_ntop(AF_INET6, &client->our_ip,
		     ntopbuf, sizeof(ntopbuf)));
    s = inet6_dgram_socket();
    if (s < 0) {
	my_log(LOG_NOTICE,
	       "DHCPv6ClientRemoveAddress(%s):socket() failed, %s (%d)",
	       if_name(if_p), strerror(errno), errno);
    }
    else {
	if (inet6_difaddr(s, if_name(if_p), &client->our_ip) < 0) {
	    my_log(LOG_INFO,
		   "DHCPv6ClientRemoveAddress(%s): remove %s failed, %s (%d)",
		   if_name(if_p),
		   inet_ntop(AF_INET6, &client->our_ip,
			     ntopbuf, sizeof(ntopbuf)),
		   strerror(errno), errno);
	}
	close(s);
    }
    bzero(&client->our_ip, sizeof(client->our_ip));
    client->our_prefix_length = 0;
    return;
}

STATIC void
DHCPv6ClientClearRetransmit(DHCPv6ClientRef client)
{
    client->try = 0;
    return;
}

STATIC CFTimeInterval
DHCPv6ClientNextRetransmit(DHCPv6ClientRef client,
			   CFTimeInterval IRT, CFTimeInterval MRT)
{
    client->try++;
    if (client->try == 1) {
	client->retransmit_time = DHCPv6InitialTimeout(IRT);
    }
    else {
	client->retransmit_time
	    = DHCPv6SubsequentTimeout(client->retransmit_time, MRT);
    }
    return (client->retransmit_time);
}

STATIC void
DHCPv6ClientPostNotification(DHCPv6ClientRef client)
{
    if (client->callback_rls != NULL) {
	CFRunLoopSourceSignal(client->callback_rls);
    }
    return;
}

STATIC void
DHCPv6ClientCancelPendingEvents(DHCPv6ClientRef client)
{
    DHCPv6SocketDisableReceive(client->sock);
    timer_cancel(client->timer);
    return;
}

STATIC void
DHCPv6ClientClearLease(DHCPv6ClientRef client)
{
    DHCPv6ClientSetSSID(client, NULL);
    bzero(&client->lease, sizeof(client->lease));
}

STATIC void
DHCPv6ClientClearPacket(DHCPv6ClientRef client)
{
    DHCPv6ClientClearLease(client);
    if (client->saved.pkt != NULL) {
	free(client->saved.pkt);
	client->saved.pkt = NULL;
	client->saved.pkt_len = 0;
    }
    DHCPv6OptionListRelease(&client->saved.options);
    client->server_id = NULL;
    client->ia_na_pd.ptr = NULL;
    client->addr_prefix.ptr = NULL;
    client->saved_verified = false;
    client->saved.pkt_len = 0;
    return;
}

STATIC void
DHCPv6ClientInactive(DHCPv6ClientRef client)
{
    DHCPv6ClientCancelPendingEvents(client);
    DHCPv6ClientClearPacket(client);
    DHCPv6ClientRemoveAddress(client, "Inactive");
    DHCPv6ClientPostNotification(client);
    return;
}

STATIC bool
DHCPv6ClientLeaseOnSameNetwork(DHCPv6ClientRef client)
{
    bool		same_network;

    if (!if_is_wireless(service_interface(client->service_p))) {
	same_network = true;
    }
    else {
	CFStringRef	ssid;

	ssid = ServiceGetSSID(client->service_p);
	if (ssid != NULL && client->lease.ssid != NULL) {
	    same_network = CFEqual(ssid, client->lease.ssid);
	}
	else {
	    same_network = false;
	}
	if (!same_network) {
	    my_log(LOG_INFO, "%s: SSID now %@ (was %@)",
		   DHCPv6ClientGetDescription(client),
		   ssid, client->lease.ssid);
	}
    }
    return (same_network);
}

STATIC bool
DHCPv6ClientLeaseStillValid(DHCPv6ClientRef client,
			    CFAbsoluteTime current_time)
{
    lease_info_t	lease_p = &client->lease;

    if (!lease_p->valid) {
	goto done;
    }
    if (lease_p->valid_lifetime == DHCP_INFINITE_LEASE) {
	goto done;
    }
    if (current_time < lease_p->start) {
	/* time went backwards */
	DHCPv6ClientClearPacket(client);
	lease_p->valid = false;
	my_log(LOG_INFO, "%s: lease no longer valid",
	       DHCPv6ClientGetDescription(client));

	goto done;
    }
    if ((current_time - lease_p->start) >= lease_p->valid_lifetime) {
	/* expired */
	my_log(LOG_INFO, "%s: lease has expired",
	       DHCPv6ClientGetDescription(client));
	DHCPv6ClientClearPacket(client);
	lease_p->valid = false;
    }

 done:
    return (lease_p->valid);
}

STATIC void
DHCPv6ClientSavePacket(DHCPv6ClientRef client, DHCPv6SocketReceiveDataRef data)
{
    CFAbsoluteTime		current_time = timer_get_current_time();
    DHCPv6OptionErrorString 	err;
    bool			get_ia_na;
    lease_info_t		lease_p = &client->lease;
    int				option_len;
    uint32_t			preferred_lifetime;
    CFStringRef			ssid;
    uint32_t			t1;
    uint32_t			t2;
    uint32_t			valid_lifetime;

    DHCPv6ClientClearPacket(client);
    ssid = ServiceGetSSID(client->service_p);
    DHCPv6ClientSetSSID(client, ssid);
    client->saved.pkt_len = data->pkt_len;
    client->saved.pkt = malloc(client->saved.pkt_len);
    bcopy(data->pkt, client->saved.pkt, client->saved.pkt_len);
    client->saved.options 
	= DHCPv6OptionListCreateWithPacket(client->saved.pkt,
					   client->saved.pkt_len, &err);
    client->server_id = (DHCPDUIDRef)
	DHCPv6OptionListGetOptionDataAndLength(client->saved.options,
					       kDHCPv6OPTION_SERVERID,
					       &option_len, NULL);
    switch (client->mode) {
    case kDHCPv6ClientModeStatefulAddress:
	get_ia_na = true;
	break;
    case kDHCPv6ClientModeStatefulPrefix:
	get_ia_na = false;
	break;
    default:
	goto done;
    }
    client->ia_na_pd
	= get_ia_na_pd(client, client->saved.pkt->msg_type, get_ia_na,
		       client->saved.options, &client->addr_prefix);
    if (client->ia_na_pd.ptr == NULL) {
	DHCPv6OptionInfoRef 	addr_prefix_info;
	DHCPv6OptionInfoRef 	na_pd_info;

	if (get_ia_na) {
	    na_pd_info = &IA_NA_OptionInfo;
	    addr_prefix_info = &IAADDR_OptionInfo;
	}
	else {
	    na_pd_info = &IA_PD_OptionInfo;
	    addr_prefix_info = &IAPREFIX_OptionInfo;
	}
	/* shouldn't happen */
	my_log(LOG_NOTICE, "%s: %s() failed to retrieve %s/%s",
	       DHCPv6ClientGetDescription(client),
	       __func__,
	       DHCPv6OptionCodeGetName(na_pd_info->code),
	       DHCPv6OptionCodeGetName(addr_prefix_info->code));
	return;
    }
    if (get_ia_na) {
	DHCPv6OptionIA_NARef	ia_na = DHCPv6ClientGetIA_NA(client);
	DHCPv6OptionIAADDRRef	iaaddr = DHCPv6ClientGetIAADDR(client);

	t1 = DHCPv6OptionIA_NAGetT1(ia_na);
	t2 = DHCPv6OptionIA_NAGetT2(ia_na);
	valid_lifetime = DHCPv6OptionIAADDRGetValidLifetime(iaaddr);
	preferred_lifetime = DHCPv6OptionIAADDRGetPreferredLifetime(iaaddr);
    }
    else {
	DHCPv6OptionIA_PDRef	ia_pd = DHCPv6ClientGetIA_PD(client);
	DHCPv6OptionIAPREFIXRef	iaprefix = DHCPv6ClientGetIAPREFIX(client);

	t1 = DHCPv6OptionIA_PDGetT1(ia_pd);
	t2 = DHCPv6OptionIA_PDGetT2(ia_pd);
	valid_lifetime = DHCPv6OptionIAPREFIXGetValidLifetime(iaprefix);
	preferred_lifetime = DHCPv6OptionIAPREFIXGetPreferredLifetime(iaprefix);
    }
    if (preferred_lifetime == 0) {
	preferred_lifetime = valid_lifetime;
    }
    if (t1 == 0 || t2 == 0) {
	if (preferred_lifetime == DHCP_INFINITE_LEASE) {
	    t1 = t2 = 0;
	}
	else {
	    t1 = preferred_lifetime * 0.5;
	    t2 = preferred_lifetime * 0.8;
	}
    }
    else if (t1 == DHCP_INFINITE_LEASE || t2 == DHCP_INFINITE_LEASE) {
	t1 = t2 = 0;
	preferred_lifetime = DHCP_INFINITE_LEASE;
	valid_lifetime = DHCP_INFINITE_LEASE;
    }
    lease_p->start = current_time;
    if (valid_lifetime == DHCP_INFINITE_LEASE) {
	lease_p->t1 = lease_p->t2 = 0;
	preferred_lifetime = DHCP_INFINITE_LEASE;
    }
    else {
	lease_p->t1 = t1;
	lease_p->t2 = t2;
    }
    lease_p->preferred_lifetime = preferred_lifetime;
    lease_p->valid_lifetime = valid_lifetime;

 done:
    client->saved_verified = true;
    return;
}

STATIC DHCPv6PacketRef
DHCPv6ClientMakePacket(DHCPv6ClientRef client,
		       DHCPv6MessageType message_type,
		       char * buf, int buf_size,
		       DHCPv6OptionAreaRef oa_p)
{
    uint16_t			elapsed_time;
    DHCPv6OptionErrorString 	err;
    DHCPv6PacketRef		pkt;

    pkt = (DHCPv6PacketRef)buf;
    DHCPv6PacketSetMessageType(pkt, message_type);
    DHCPv6PacketSetTransactionID(pkt, DHCPv6ClientGetTransactionID(client));
    DHCPv6OptionAreaInit(oa_p, pkt->options, 
			 buf_size - DHCPV6_PACKET_HEADER_LENGTH);
    if (!S_insert_duid(client, oa_p)) {
	return (NULL);
    }
    if (client->mode != kDHCPv6ClientModeStatefulPrefix
	&& !DHCPv6OptionAreaAddOptionRequestOption(oa_p,
						   DHCPv6RequestedOptions,
						   DHCPv6RequestedOptionsCount,
						   &err)) {
	my_log(LOG_NOTICE, "DHCPv6Client: failed to add ORO, %s",
	       err.str);
	return (NULL);
    }
    elapsed_time = get_elapsed_time(client);
    if (!DHCPv6OptionAreaAddOption(oa_p, kDHCPv6OPTION_ELAPSED_TIME,
				   sizeof(elapsed_time), &elapsed_time,
				   &err)) {
	my_log(LOG_NOTICE, "DHCPv6Client: failed to add ELAPSED_TIME, %s",
	       err.str);
	return (NULL);
    }
    return (pkt);
}


STATIC void
DHCPv6ClientSendInform(DHCPv6ClientRef client)
{
    char			buf[1500];
    int				error;
    DHCPv6OptionArea		oa;
    DHCPv6PacketRef		pkt;

    pkt = DHCPv6ClientMakePacket(client, kDHCPv6MessageINFORMATION_REQUEST,
				 buf, sizeof(buf), &oa);
    if (pkt == NULL) {
	return;
    }
    error = DHCPv6SocketTransmit(client->sock, pkt,
				 DHCPV6_PACKET_HEADER_LENGTH 
				 + DHCPv6OptionAreaGetUsedLength(&oa));
    switch (error) {
    case 0:
    case ENXIO:
    case ENETDOWN:
	break;
    default:
	my_log(LOG_NOTICE,
	       "%s: SendInformRequest transmit failed, %s",
	       DHCPv6ClientGetDescription(client),
	       strerror(error));
	break;
    }
    return;
}

STATIC void
DHCPv6ClientSendSolicit(DHCPv6ClientRef client)
{
    char			buf[1500];
    DHCPv6OptionErrorString 	err;
    int				error;
    DHCPv6OptionArea		oa;
    DHCPv6PacketRef		pkt;

    pkt = DHCPv6ClientMakePacket(client, kDHCPv6MessageSOLICIT,
				 buf, sizeof(buf), &oa);
    if (pkt == NULL) {
	return;
    }
    switch (client->mode) {
    case kDHCPv6ClientModeStatefulAddress:
	if (!add_ia_na_option(client, true, &oa, &err)) {
	    my_log(LOG_NOTICE, "DHCPv6Client: failed to add IA_NA, %s",
		   err.str);
	    return;
	}
	break;
    case kDHCPv6ClientModeStatefulPrefix:
	if (!add_ia_pd_option(client, true, &oa, &err)) {
	    my_log(LOG_NOTICE, "DHCPv6Client: failed to add IA_PD, %s",
		   err.str);
	    return;
	}
	break;
    default:
	/* can't happen */
	return;
    }
    error = DHCPv6SocketTransmit(client->sock, pkt,
				 DHCPV6_PACKET_HEADER_LENGTH 
				 + DHCPv6OptionAreaGetUsedLength(&oa));
    switch (error) {
    case 0:
    case ENXIO:
    case ENETDOWN:
	break;
    default:
	my_log(LOG_NOTICE, "%s: SendSolicit transmit failed, %s",
	       DHCPv6ClientGetDescription(client),
	       strerror(error));
	break;
    }
    return;
}

STATIC void
DHCPv6ClientSendPacket(DHCPv6ClientRef client)
{
    char			buf[1500];
    DHCPv6OptionErrorString 	err;
    int				error;
    DHCPv6MessageType		message_type;
    DHCPv6OptionArea		oa;
    DHCPv6PacketRef		pkt;

    if (client->server_id == NULL) {
	my_log(LOG_NOTICE, "%s: %s() NULL server_id",
	       DHCPv6ClientGetDescription(client),
	       __func__);
	return;
    }
    if (client->ia_na_pd.ptr == NULL) {
	my_log(LOG_NOTICE, "%s: %s() NULL IA_NA/IA_PD",
	       DHCPv6ClientGetDescription(client),
	       __func__);
	return;
    }
    switch (client->cstate) {
    case kDHCPv6ClientStateRequest:
	message_type = kDHCPv6MessageREQUEST;
	break;
    case kDHCPv6ClientStateRenew:
	message_type = kDHCPv6MessageRENEW;
	break;
    case kDHCPv6ClientStateRebind:
	message_type = kDHCPv6MessageREBIND;
	break;
    case kDHCPv6ClientStateRelease:
	message_type = kDHCPv6MessageRELEASE;
	break;
    case kDHCPv6ClientStateConfirm:
	/*
	 * REBIND in Confirm State
	 *
	 * Send a REBIND message instead of CONFIRM when trying to confirm the
	 * prefix is still valid.
	 *
	 * See RFC 8415:
	 * 18.2.
	 * ... Rebind if it has delegated prefixes...
	 *
	 * 18.2.12
	 * If the client has any valid delegated prefixes obtained from the DHCP
	 * server, the client MUST initiate a Rebind/Reply message exchange as
	 * described in Section 18.2.5, with the exception that the
	 * retransmission parameters should be set as for the Confirm message
	 * (see Section 18.2.3).
	 */
	message_type = (client->mode == kDHCPv6ClientModeStatefulPrefix)
	    ? kDHCPv6MessageREBIND : kDHCPv6MessageCONFIRM;
	break;
    case kDHCPv6ClientStateDecline:
	message_type = kDHCPv6MessageDECLINE;
	break;
    default:
	my_log(LOG_NOTICE,
	       "%s: SendPacket doesn't know %s",
	       DHCPv6ClientGetDescription(client),
	       DHCPv6ClientStateGetName(client->cstate));
	return;
    }
    pkt = DHCPv6ClientMakePacket(client, message_type,
				 buf, sizeof(buf), &oa);
    if (pkt == NULL) {
	return;
    }
    switch (message_type) {
    case kDHCPv6MessageREBIND:
    case kDHCPv6MessageCONFIRM:
	break;
    default:
	if (!DHCPv6OptionAreaAddOption(&oa, kDHCPv6OPTION_SERVERID,
				       option_data_get_length(client->server_id),
				       client->server_id, &err)) {
	    my_log(LOG_NOTICE,
		   "%s: %s failed to add SERVERID, %s",
		   DHCPv6ClientGetDescription(client),
		   DHCPv6ClientStateGetName(client->cstate), err.str);
	    return;
	}
	break;
    }
    if (client->mode == kDHCPv6ClientModeStatefulAddress) {
	if (!add_ia_na_option(client, false, &oa, &err)) {
	    my_log(LOG_NOTICE, "DHCPv6Client: failed to add IA_NA, %s",
		   err.str);
	    return;
	}
    }
    else {
	if (!add_ia_pd_option(client, false, &oa, &err)) {
	    my_log(LOG_NOTICE, "DHCPv6Client: failed to add IA_NA, %s",
		   err.str);
	    return;
	}
    }
    error = DHCPv6SocketTransmit(client->sock, pkt,
				 DHCPV6_PACKET_HEADER_LENGTH 
				 + DHCPv6OptionAreaGetUsedLength(&oa));
    switch (error) {
    case 0:
    case ENXIO:
    case ENETDOWN:
	break;
    default:
	my_log(LOG_NOTICE, "%s: SendPacket transmit failed, %s",
	       DHCPv6ClientGetDescription(client),
	       strerror(error));
	break;
    }
    return;
}

STATIC void
DHCPv6Client_InformComplete(DHCPv6ClientRef client, IFEventID_t event_id,
			    void * event_data)
{
    switch (event_id) {
    case IFEventID_start_e:
	DHCPv6ClientSetState(client, kDHCPv6ClientStateInformComplete);
	DHCPv6ClientCancelPendingEvents(client);
	break;
    default:
	break;
    }
}

STATIC void
DHCPv6Client_Inform(DHCPv6ClientRef client, IFEventID_t event_id, 
		   void * event_data)
{
    interface_t *	if_p = DHCPv6ClientGetInterface(client);

    switch (event_id) {
    case IFEventID_start_e:
	DHCPv6ClientSetState(client, kDHCPv6ClientStateInform);
	DHCPv6ClientClearPacket(client);
	DHCPv6ClientClearRetransmit(client);
	DHCPv6ClientSetNewTransactionID(client);
	DHCPv6SocketEnableReceive(client->sock,
				  DHCPv6ClientGetTransactionID(client),
				  (DHCPv6SocketReceiveFuncPtr)
				  DHCPv6Client_Inform,
				  client, (void *)IFEventID_data_e);

	if (if_ift_type(if_p) != IFT_CELLULAR) {
	    timer_callout_set(client->timer,
			      random_double_in_range(0, DHCPv6_INF_MAX_DELAY),
			      (timer_func_t *)DHCPv6Client_Inform, client,
			      (void *)IFEventID_timeout_e, NULL);
	    break;
	}
	/* FALL THROUGH */
    case IFEventID_timeout_e:
	if (client->try == 0) {
	    client->start_time = timer_get_current_time();
	}
	else {
	    link_status_t	link_status;

	    link_status = if_get_link_status(if_p);
	    if (link_status.valid && !link_status.active) {
		DHCPv6ClientInactive(client);
		break;
	    }
	}
	timer_callout_set(client->timer,
			  DHCPv6ClientNextRetransmit(client,
						     DHCPv6_INF_TIMEOUT,
						     DHCPv6_INF_MAX_RT),
			  (timer_func_t *)DHCPv6Client_Inform, 
			  client, (void *)IFEventID_timeout_e, NULL);
	my_log(LOG_INFO, "%s: Inform Transmit (try=%d)",
	       DHCPv6ClientGetDescription(client),
	       client->try);
	DHCPv6ClientSendInform(client);
	break;
    case IFEventID_data_e: {
	DHCPv6SocketReceiveDataRef 	data;
	int				option_len;
	DHCPDUIDRef			server_id;

	data = (DHCPv6SocketReceiveDataRef)event_data;
	if (data->pkt->msg_type != kDHCPv6MessageREPLY
	    || (!S_duid_matches(client, data->options))) {
	    /* not a match */
	    break;
	}
	server_id = (DHCPDUIDRef)
	    DHCPv6OptionListGetOptionDataAndLength(data->options,
						   kDHCPv6OPTION_SERVERID,
						   &option_len, NULL);
	if (server_id == NULL
	    || !DHCPDUIDIsValid(server_id, option_len)) {
	    /* missing/invalid DUID */
	    break;
	}
	my_log(LOG_INFO,
	       "%s: %s Received (try=%d)",
	       DHCPv6ClientGetDescription(client),
	       DHCPv6MessageTypeName(data->pkt->msg_type),
	       client->try);
	DHCPv6ClientSavePacket(client, data);
	DHCPv6ClientPostNotification(client);
	DHCPv6Client_InformComplete(client, IFEventID_start_e, NULL);
	break;
    }
    default:
	break;
    }
    return;
}

STATIC void
DHCPv6Client_Release(DHCPv6ClientRef client, IFEventID_t event_id, 
		     void * event_data)
{
    switch (event_id) {
    case IFEventID_start_e:
	DHCPv6ClientSetState(client, kDHCPv6ClientStateRelease);
	DHCPv6ClientRemoveAddress(client, "Release");
	DHCPv6ClientCancelPendingEvents(client);
	DHCPv6ClientClearRetransmit(client);
	DHCPv6ClientSetNewTransactionID(client);
	my_log(LOG_INFO, "%s: Release Transmit",
	       DHCPv6ClientGetDescription(client));
	DHCPv6ClientSendPacket(client);
	/*
	 * We're supposed to wait for a Reply.  Unfortunately, that's not
	 * possible because the code that invokes us expects the Stop
	 * event to be synchronous.
	 */
	break;
    default:
	break;
    }
    return;
}

STATIC void
DHCPv6Client_Decline(DHCPv6ClientRef client, IFEventID_t event_id, 
		     void * event_data)
{
    switch (event_id) {
    case IFEventID_start_e:
	DHCPv6ClientSetState(client, kDHCPv6ClientStateDecline);
	DHCPv6ClientRemoveAddress(client, "Decline");
	DHCPv6ClientCancelPendingEvents(client);
	DHCPv6ClientClearLease(client);
	client->saved_verified = false;
	DHCPv6ClientPostNotification(client);
	DHCPv6ClientClearRetransmit(client);
	DHCPv6ClientSetNewTransactionID(client);
	DHCPv6SocketEnableReceive(client->sock,
				  DHCPv6ClientGetTransactionID(client),
				  (DHCPv6SocketReceiveFuncPtr)
				  DHCPv6Client_Decline,
				  client, (void *)IFEventID_data_e);
	/* FALL THROUGH */
    case IFEventID_timeout_e:
	if (client->try >= DHCPv6_DEC_MAX_RC) {
	    /* go back to Solicit */
	    DHCPv6Client_Solicit(client, IFEventID_start_e, NULL);
	    return;
	}
	timer_callout_set(client->timer,
			  DHCPv6ClientNextRetransmit(client,
						     DHCPv6_DEC_TIMEOUT, 
						     0),
			  (timer_func_t *)DHCPv6Client_Decline, 
			  client, (void *)IFEventID_timeout_e, NULL);
	my_log(LOG_INFO,
	       "%s: Decline Transmit (try=%d)",
	       DHCPv6ClientGetDescription(client),
	       client->try);
	DHCPv6ClientSendPacket(client);
	break;

    case IFEventID_data_e: {
	DHCPv6SocketReceiveDataRef 	data;
	int				option_len;
	DHCPDUIDRef			server_id;

	data = (DHCPv6SocketReceiveDataRef)event_data;
	if (data->pkt->msg_type != kDHCPv6MessageREPLY
	    || (!S_duid_matches(client, data->options))) {
	    /* not a match */
	    break;
	}
	server_id = (DHCPDUIDRef)
	    DHCPv6OptionListGetOptionDataAndLength(data->options,
						   kDHCPv6OPTION_SERVERID,
						   &option_len, NULL);
	if (server_id == NULL
	    || !DHCPDUIDIsValid(server_id, option_len)) {
	    /* missing/invalid DUID */
	    break;
	}
	my_log(LOG_INFO,
	       "%s: %s Received (try=%d)",
	       DHCPv6ClientGetDescription(client),
	       DHCPv6MessageTypeName(data->pkt->msg_type),
	       client->try);

	/* back to Solicit */
	DHCPv6Client_Solicit(client, IFEventID_start_e, NULL);
	break;
    }
    default:
	break;
    }
    return;
}

STATIC void
DHCPv6Client_RenewRebind(DHCPv6ClientRef client, IFEventID_t event_id, 
			 void * event_data)
{
    CFAbsoluteTime		current_time = timer_get_current_time();

    switch (event_id) {
    case IFEventID_start_e:
	DHCPv6ClientSetState(client, kDHCPv6ClientStateRenew);
	DHCPv6ClientCancelPendingEvents(client);
	DHCPv6ClientClearRetransmit(client);
	client->start_time = current_time;
	DHCPv6ClientSetNewTransactionID(client);
	DHCPv6SocketEnableReceive(client->sock,
				  DHCPv6ClientGetTransactionID(client),
				  (DHCPv6SocketReceiveFuncPtr)
				  DHCPv6Client_RenewRebind,
				  client, (void *)IFEventID_data_e);
	/* FALL THROUGH */
    case IFEventID_timeout_e: {
	CFTimeInterval	time_since_start;
	CFTimeInterval	wait_time;

	if (!DHCPv6ClientLeaseStillValid(client, current_time)) {
	    DHCPv6Client_Unbound(client, IFEventID_start_e, NULL);
	    return;
	}
	time_since_start = current_time - client->lease.start;
	if (((uint32_t)time_since_start) < client->lease.t2) {
	    CFTimeInterval	time_until_t2;

	    /* Renew (before T2) */
	    wait_time = DHCPv6ClientNextRetransmit(client,
						   DHCPv6_REN_TIMEOUT,
						   DHCPv6_REN_MAX_RT);
	    time_until_t2 = client->lease.t2 - (uint32_t)time_since_start;
	    if (wait_time > time_until_t2) {
		wait_time = time_until_t2;
	    }
	}
	else {
	    CFTimeInterval	time_until_expiration;

	    /* Rebind (T2 or later) */
	    if (client->cstate != kDHCPv6ClientStateRebind) {
		/* switch to Rebind */
		DHCPv6ClientSetNewTransactionID(client);
		DHCPv6SocketEnableReceive(client->sock,
					  DHCPv6ClientGetTransactionID(client),
					  (DHCPv6SocketReceiveFuncPtr)
					  DHCPv6Client_RenewRebind,
					  client, (void *)IFEventID_data_e);
		client->start_time = current_time;
		DHCPv6ClientSetState(client, kDHCPv6ClientStateRebind);
		DHCPv6ClientClearRetransmit(client);
	    }
	    wait_time = DHCPv6ClientNextRetransmit(client,
						   DHCPv6_REB_TIMEOUT,
						   DHCPv6_REB_MAX_RT);
	    time_until_expiration 
		= client->lease.valid_lifetime - (uint32_t)time_since_start;
	    if (wait_time > time_until_expiration) {
		wait_time = time_until_expiration;
	    }
	}
	client->renew_rebind_time = current_time + wait_time;
	timer_callout_set(client->timer,
			  wait_time,
			  (timer_func_t *)DHCPv6Client_RenewRebind, 
			  client, (void *)IFEventID_timeout_e, NULL);
	my_log(LOG_INFO,
	       "%s: %s Transmit (try=%d) (wait_time=%lu)",
	       DHCPv6ClientGetDescription(client),
	       DHCPv6ClientStateGetName(client->cstate),
	       client->try, (unsigned long)wait_time);
	DHCPv6ClientSendPacket(client);
	break;
    }
    case IFEventID_data_e: {
	ADDR_PREFIX_U			addr_prefix;
	DHCPv6StatusCode		code;
	DHCPv6SocketReceiveDataRef 	data;
	bool				get_ia_na;
	IA_NA_PD_U			ia_na_pd;
	const char *			msg;
	unsigned int			msg_length;
	int				option_len;
	DHCPDUIDRef			server_id;

	data = (DHCPv6SocketReceiveDataRef)event_data;
	if (data->pkt->msg_type != kDHCPv6MessageREPLY
	    || (!S_duid_matches(client, data->options))) {
	    /* not a match */
	    break;
	}
	server_id = (DHCPDUIDRef)
	    DHCPv6OptionListGetOptionDataAndLength(data->options,
						   kDHCPv6OPTION_SERVERID,
						   &option_len, NULL);
	if (server_id == NULL
	    || !DHCPDUIDIsValid(server_id, option_len)) {
	    /* missing/invalid DUID */
	    break;
	}
	if (!DHCPv6OptionListGetStatusCode(data->options, &code,
					   &msg, &msg_length)) {
	    /* ignore bad data */
	    break;
	}
	if (code != kDHCPv6StatusCodeSuccess) {
	    my_log(LOG_NOTICE,
		   "%s: %s %s %.*s",
		   DHCPv6ClientGetDescription(client),
		   DHCPv6MessageTypeName(data->pkt->msg_type),
		   DHCPv6StatusCodeGetName(code),
		   msg_length, msg);
	    /* XXX check for a specific value maybe? */
	    DHCPv6Client_Unbound(client, IFEventID_start_e, NULL);
	    return;
	}
	get_ia_na = (client->mode == kDHCPv6ClientModeStatefulAddress);
	ia_na_pd
	    = get_ia_na_pd(client, data->pkt->msg_type, get_ia_na,
			   data->options, &addr_prefix);
	if (ia_na_pd.ptr == NULL) {
	    DHCPv6Client_Unbound(client, IFEventID_start_e, NULL);
	    break;
	}
	DHCPv6ClientLogAddressOrPrefix(client, data->pkt->msg_type,
				       get_ia_na, &addr_prefix);
	DHCPv6ClientSavePacket(client, data);
	DHCPv6Client_Bound(client, IFEventID_start_e, NULL);
	break;
    }
    default:
	break;
    }
    return;
}

STATIC void
DHCPv6Client_Confirm(DHCPv6ClientRef client, IFEventID_t event_id, 
		     void * event_data)
{
    CFAbsoluteTime		current_time = timer_get_current_time();
    interface_t *		if_p = DHCPv6ClientGetInterface(client);

    switch (event_id) {
    case IFEventID_start_e:
	DHCPv6ClientSetState(client, kDHCPv6ClientStateConfirm);
	DHCPv6ClientCancelPendingEvents(client);
	DHCPv6ClientClearRetransmit(client);
	client->saved_verified = false;
	DHCPv6ClientSetNewTransactionID(client);
	DHCPv6SocketEnableReceive(client->sock,
				  DHCPv6ClientGetTransactionID(client),
				  (DHCPv6SocketReceiveFuncPtr)
				  DHCPv6Client_Confirm,
				  client, (void *)IFEventID_data_e);
	timer_callout_set(client->timer,
			  random_double_in_range(0, DHCPv6_CNF_MAX_DELAY),
			  (timer_func_t *)DHCPv6Client_Confirm, client,
			  (void *)IFEventID_timeout_e, NULL);
	break;
    case IFEventID_timeout_e:
	if (client->try == 0) {
	    client->start_time = current_time;
	}
	else {
	    bool		done = false;
	    link_status_t	link_status;

	    link_status = if_get_link_status(if_p);
	    if (link_status_is_inactive(&link_status)) {
		DHCPv6ClientInactive(client);
		break;
	    }
	    if (current_time > client->start_time) {
		if ((current_time - client->start_time) >= DHCPv6_CNF_MAX_RD) {
		    done = true;
		}
	    }
	    else {
		done = true;
	    }
	    if (done) {
		if (DHCPv6ClientLeaseStillValid(client, current_time)) {
		    DHCPv6Client_Bound(client, IFEventID_start_e, NULL);
		    return;
		}
		DHCPv6Client_Solicit(client, IFEventID_start_e, NULL);
		return;
	    }
	}
	timer_callout_set(client->timer,
			  DHCPv6ClientNextRetransmit(client,
						     DHCPv6_CNF_TIMEOUT,
						     DHCPv6_CNF_MAX_RT),
			  (timer_func_t *)DHCPv6Client_Confirm,
			  client, (void *)IFEventID_timeout_e, NULL);
	my_log(LOG_INFO,
	       "%s: Confirm Transmit (try=%d)",
	       DHCPv6ClientGetDescription(client),
	       client->try);
	DHCPv6ClientSendPacket(client);
	break;
    case IFEventID_data_e: {
	DHCPv6StatusCode		code;
	DHCPv6SocketReceiveDataRef 	data;
	int				option_len;
	DHCPDUIDRef			server_id;
	const char *			msg;
	unsigned int			msg_length;

	data = (DHCPv6SocketReceiveDataRef)event_data;
	if (data->pkt->msg_type != kDHCPv6MessageREPLY
	    || !S_duid_matches(client, data->options)) {
	    /* not a match */
	    break;
	}
	server_id = (DHCPDUIDRef)
	    DHCPv6OptionListGetOptionDataAndLength(data->options,
						   kDHCPv6OPTION_SERVERID,
						   &option_len, NULL);
	if (server_id == NULL
	    || !DHCPDUIDIsValid(server_id, option_len)) {
	    /* missing/invalid DUID */
	    break;
	}
	if (!DHCPv6OptionListGetStatusCode(data->options, &code,
					   &msg, &msg_length)) {
	    /* ignore bad data */
	    break;
	}
	if (code != kDHCPv6StatusCodeSuccess) {
	    my_log(LOG_NOTICE,
		   "%s: %s %s '%.*s'",
		   DHCPv6ClientGetDescription(client),
		   DHCPv6MessageTypeName(data->pkt->msg_type),
		   DHCPv6StatusCodeGetName(code),
		   msg_length, msg);
	    DHCPv6Client_Unbound(client, IFEventID_start_e, NULL);
	    return;
	}
	my_log(LOG_INFO,
	       "%s: %s Received (try=%d)",
	       DHCPv6ClientGetDescription(client),
	       DHCPv6MessageTypeName(data->pkt->msg_type),
	       client->try);
	if (client->mode == kDHCPv6ClientModeStatefulPrefix) {
	    ADDR_PREFIX_U	addr_prefix;
	    IA_NA_PD_U		ia_na_pd;

	    /*
	     * We sent a REBIND, so we need to save the packet. See comment
	     * "REBIND in Confirm State" in DHCPv6ClientSendPacket() above.
	     */
	    ia_na_pd
		= get_ia_na_pd(client, data->pkt->msg_type, false,
			       data->options, &addr_prefix);
	    if (ia_na_pd.ptr != NULL) {
		DHCPv6ClientLogAddressOrPrefix(client, data->pkt->msg_type,
					       false, &addr_prefix);
		DHCPv6ClientSavePacket(client, data);
	    }
	}
	DHCPv6Client_Bound(client, IFEventID_start_e, NULL);
	break;
    }
    default:
	break;
    }
}

STATIC void
DHCPv6ClientHandleAddressChanged(DHCPv6ClientRef client,
				 inet6_addrlist_t * addr_list_p)
{
    int				i;
    inet6_addrinfo_t *		scan;

    if (addr_list_p == NULL || addr_list_p->count == 0) {
	/* no addresses configured, nothing to do */
	return;
    }
    if (client->cstate != kDHCPv6ClientStateBound) {
	return;
    }
    for (i = 0, scan = addr_list_p->list; 
	 i < addr_list_p->count; i++, scan++) {
	if (IN6_ARE_ADDR_EQUAL(&client->our_ip, &scan->addr)) {
	    /* someone else is using this address, decline it */
	    if ((scan->addr_flags & IN6_IFF_DUPLICATED) != 0) {
		DHCPv6Client_Decline(client, IFEventID_start_e, NULL);
		return;
	    }
	    if ((scan->addr_flags & IN6_IFF_TENTATIVE) != 0) {
		my_log(LOG_INFO, "address is still tentative");
		/* address is still tentative */
		break;
	    }
	    /* notify that we're ready */
	    DHCPv6ClientPostNotification(client);
	    break;
	}
    }
    return;
}

STATIC void
DHCPv6ClientSimulateAddressChanged(DHCPv6ClientRef client)
{
    inet6_addrlist_t	addr_list;
    interface_t *	if_p = DHCPv6ClientGetInterface(client);

    inet6_addrlist_copy(&addr_list, if_link_index(if_p));
    DHCPv6ClientHandleAddressChanged(client, &addr_list);
    inet6_addrlist_free(&addr_list);
    return;
}

/*
 * Function: S_time_in_future
 * Purpose:
 *   Returns whether the given time is in the future by at least the
 *   time interval specified by 'time_interval'.
 */
INLINE bool
S_time_in_future(CFAbsoluteTime current_time,
		 CFAbsoluteTime the_time,
		 CFTimeInterval time_interval)
{
    return (current_time < the_time
	    && (the_time - current_time) >= time_interval);
}

STATIC void
DHCPv6ClientHandleWake(DHCPv6ClientRef client,
		       void * event_data)
{
    interface_t *	if_p;
    link_event_data_t	link_event;
    link_status_t *	link_status_p;
    bool		wait_for_link_active;

    /*
     * While asleep, we could have switched networks without knowing it.
     * Unless we know with some confidence that we're on the same network,
     * we need to remove the IP address from the interface.
     *
     * We remove the IP address if any of the following are true:
     * - we're not connected to a network (link status is inactive)
     * - we're on a different Wi-Fi network (the SSID changed)
     * - we're not on the same ethernet network
     */
    if_p = DHCPv6ClientGetInterface(client);
    link_event = (link_event_data_t)event_data;
    link_status_p = &link_event->link_status;
    wait_for_link_active = link_status_is_inactive(link_status_p);
    if (wait_for_link_active
	|| (if_is_wireless(if_p)
	    && link_event->info == kLinkInfoNetworkChanged)
	|| (!if_is_wireless(if_p)
	    && !link_status_p->wake_on_same_network)) {
	DHCPv6ClientRemoveAddress(client, "Wake");
	if (wait_for_link_active) {
	    return;
	}
	if (client->cstate != kDHCPv6ClientStateSolicit) {
	    DHCPv6Client_Solicit(client, IFEventID_start_e, NULL);
	}
    }
    else {
	CFAbsoluteTime		current_time = timer_get_current_time();

	if (!DHCPv6ClientLeaseStillValid(client, current_time)) {
	    if (client->cstate != kDHCPv6ClientStateSolicit) {
		DHCPv6Client_Unbound(client, IFEventID_start_e, NULL);
	    }
	    return;
	}
	/*
	 * If we're not in bound, renew, or rebind states, or the BSSID
	 * has changed, enter the Confirm state.
	 */
	if (!S_dhcp_state_is_bound_renew_or_rebind(client->cstate)
	    || link_event->info == kLinkInfoBSSIDChanged) {
	    DHCPv6Client_Confirm(client, IFEventID_start_e, NULL);
	    return;
	}

	/* If an infinite lease, no need to do any maintenance */
	if (client->lease.valid_lifetime == DHCP_INFINITE_LEASE) {
	    return;
	}

	/*
	 * Check the timer we had scheduled. If it is sufficiently in the
	 * future, schedule a new timer to wakeup in RENEW/REBIND then.
	 * Otherwise, enter RENEW/REBIND now.
	 *
	 * Note that re-scheduling a timer at wake is important because
	 * timers stop counting down while the system is asleep.
	 */
	if (S_time_in_future(current_time, client->renew_rebind_time,
			     G_wake_skew_secs)) {
	    CFAbsoluteTime	delta;

	    delta = client->renew_rebind_time - current_time;
	    my_log(LOG_INFO,
		   "%s: wake: calculated new timer (%lu secs)",
		   DHCPv6ClientGetDescription(client),
		   (unsigned long)delta);
	    timer_callout_set(client->timer, delta,
			      (timer_func_t *)DHCPv6Client_RenewRebind,
			      client, (void *)IFEventID_start_e, NULL);
	}
	else {
	    my_log(LOG_INFO,
		   "%s: wake: need to renew/rebind",
		   DHCPv6ClientGetDescription(client));
	    DHCPv6Client_RenewRebind(client, IFEventID_start_e, NULL);
	}
    }
    return;
}

STATIC bool
DHCPv6ClientBoundAddress(DHCPv6ClientRef client,
			 uint32_t valid_lifetime,
			 uint32_t preferred_lifetime)
{
    DHCPv6OptionIAADDRRef	iaaddr;
    interface_t *		if_p = DHCPv6ClientGetInterface(client);
    char 			ntopbuf[INET6_ADDRSTRLEN];
    struct in6_addr		our_ip;
    int				prefix_length;
    int				s;

    iaaddr = DHCPv6ClientGetIAADDR(client);
    bcopy((void *)DHCPv6OptionIAADDRGetAddress(iaaddr), &our_ip, sizeof(our_ip));
    s = inet6_dgram_socket();
    if (s < 0) {
	my_log(LOG_NOTICE,
	       "%s(%s): socket() failed, %s (%d)",
	       __func__, if_name(if_p), strerror(errno), errno);
	return (false);
    }

    /* if the address has changed, remove the old first */
    if (!IN6_IS_ADDR_UNSPECIFIED(&client->our_ip)
	&& !IN6_ARE_ADDR_EQUAL(&client->our_ip, &our_ip)) {
	inet_ntop(AF_INET6, &client->our_ip, ntopbuf, sizeof(ntopbuf));
	if (inet6_difaddr(s, if_name(if_p), &client->our_ip) < 0) {
	    my_log(LOG_NOTICE,
		   "%s(%s): remove %s failed, %s (%d)",
		   __func__, if_name(if_p), ntopbuf,
		   strerror(errno), errno);
	}
	else {
	    my_log(LOG_NOTICE, "%s(%s): removed %s", __func__,
		   if_name(if_p), ntopbuf);
	}
    }
    prefix_length = S_get_prefix_length(&our_ip, if_link_index(if_p));
    inet_ntop(AF_INET6, &our_ip, ntopbuf, sizeof(ntopbuf));
    if (inet6_aifaddr(s, if_name(if_p), &our_ip, NULL,
		      prefix_length, IN6_IFF_DYNAMIC,
		      valid_lifetime, preferred_lifetime) < 0) {
	my_log(LOG_NOTICE,
	       "%s(%s): adding %s failed, %s (%d)", __func__,
	       if_name(if_p), ntopbuf, strerror(errno), errno);
    }
    else {
	my_log(LOG_NOTICE,
	       "%s: set address %s/%d valid %d preferred %d",
	       DHCPv6ClientGetDescription(client),
	       ntopbuf, prefix_length,
	       valid_lifetime, preferred_lifetime);
    }
    /* notify that we're ready */
    DHCPv6ClientPostNotification(client);
    client->our_ip = our_ip;
    client->our_prefix_length = prefix_length;

    /* and see what addresses are there now */
    DHCPv6ClientSimulateAddressChanged(client);
    close(s);
    return (true);
}

STATIC bool
DHCPv6ClientBoundPrefix(DHCPv6ClientRef client,
			uint32_t valid_lifetime,
			uint32_t preferred_lifetime)
{
    DHCPv6OptionIAPREFIXRef	iaprefix;
    const char *		label;
    char 			ntopbuf[INET6_ADDRSTRLEN];
    struct in6_addr		prefix;
    uint8_t			prefix_length;

    iaprefix = DHCPv6ClientGetIAPREFIX(client);
    bcopy((void *)DHCPv6OptionIAPREFIXGetPrefix(iaprefix),
	  &prefix, sizeof(prefix));
    prefix_length = DHCPv6OptionIAPREFIXGetPrefixLength(iaprefix);
    if (IN6_IS_ADDR_UNSPECIFIED(&client->delegated_prefix)) {
	label = "New";
    }
    else if (!IN6_ARE_ADDR_EQUAL(&client->delegated_prefix, &prefix)) {
	label = "Changed";
    }
    else {
	label = "Maintained";
    }
    client->delegated_prefix = prefix;
    client->delegated_prefix_length = prefix_length;
    inet_ntop(AF_INET6, &client->delegated_prefix, ntopbuf, sizeof(ntopbuf));
    my_log(LOG_NOTICE,
	   "%s: %s prefix %s/%d valid %d preferred %d",
	   DHCPv6ClientGetDescription(client),
	   label, ntopbuf, client->delegated_prefix_length,
	   valid_lifetime, preferred_lifetime);
    DHCPv6ClientPostNotification(client);
    return (true);
}

STATIC void
DHCPv6Client_Bound(DHCPv6ClientRef client, IFEventID_t event_id,
		   void * event_data)
{
    switch (event_id) {
    case IFEventID_start_e: {
	CFAbsoluteTime		current_time = timer_get_current_time();
	uint32_t		preferred_lifetime;
	CFTimeInterval		time_since_start = 0;
	uint32_t		valid_lifetime;

	DHCPv6ClientSetState(client, kDHCPv6ClientStateBound);
	client->lease.valid = true;
	client->saved_verified = true;
	DHCPv6ClientCancelPendingEvents(client);
	valid_lifetime = client->lease.valid_lifetime;
	preferred_lifetime = client->lease.preferred_lifetime;
	if (valid_lifetime != DHCP_INFINITE_LEASE) {
	    if (current_time < client->lease.start) {
		/* time went backwards? */
		DHCPv6Client_Unbound(client, IFEventID_start_e, NULL);
		return;
	    }
	    time_since_start = current_time - client->lease.start;
	    if (((uint32_t)time_since_start) >= valid_lifetime) {
		/* expired */
		DHCPv6Client_Unbound(client, IFEventID_start_e, NULL);
		return;
	    }
	    /* reduce the time left by the amount that's elapsed already */
	    valid_lifetime -= (uint32_t)time_since_start;
	    if (((uint32_t)time_since_start) < preferred_lifetime) {
		preferred_lifetime -= (uint32_t)time_since_start;
	    }
	    else {
		preferred_lifetime = 0; /* XXX really? */
	    }
	}
	if (client->mode == kDHCPv6ClientModeStatefulAddress) {
	    if (!DHCPv6ClientBoundAddress(client, valid_lifetime,
					  preferred_lifetime)) {
		break;
	    }
	}
	else {
	    if (!DHCPv6ClientBoundPrefix(client, valid_lifetime,
					 preferred_lifetime)) {
		break;
	    }
	}
	/* set a timer to start in Renew */
	if (valid_lifetime != DHCP_INFINITE_LEASE) {
	    uint32_t	t1 = client->lease.t1;

	    if (((uint32_t)time_since_start) < t1) {
		t1 -= (uint32_t)time_since_start;
	    }
	    else {
		t1 = 10; /* wakeup in 10 seconds */
	    }
	    client->renew_rebind_time = current_time + t1;
	    timer_callout_set(client->timer, t1,
			      (timer_func_t *)DHCPv6Client_RenewRebind,
			      client, (void *)IFEventID_start_e, NULL);
	}
	break;
    }
    default:
	break;
    }
}

STATIC void
DHCPv6Client_Unbound(DHCPv6ClientRef client, IFEventID_t event_id, 
		     void * event_data)
{
    switch (event_id) {
    case IFEventID_start_e:
	DHCPv6ClientSetState(client, kDHCPv6ClientStateUnbound);
	DHCPv6ClientCancelPendingEvents(client);
	DHCPv6ClientRemoveAddress(client, "Unbound");
	DHCPv6ClientClearPacket(client);
	DHCPv6ClientPostNotification(client);
	DHCPv6Client_Solicit(client, IFEventID_start_e, NULL);
	break;
    default:
	break;
    }
}

STATIC void
DHCPv6Client_Request(DHCPv6ClientRef client, IFEventID_t event_id, 
		     void * event_data)
{
    switch (event_id) {
    case IFEventID_start_e:
	DHCPv6ClientSetState(client, kDHCPv6ClientStateRequest);
	DHCPv6ClientClearRetransmit(client);
	DHCPv6ClientSetNewTransactionID(client);
	client->start_time = timer_get_current_time();
	DHCPv6SocketEnableReceive(client->sock,
				  DHCPv6ClientGetTransactionID(client),
				  (DHCPv6SocketReceiveFuncPtr)
				  DHCPv6Client_Request,
				  client, (void *)IFEventID_data_e);
	/* FALL THROUGH */
    case IFEventID_timeout_e: {
	if (client->try >= DHCPv6_REQ_MAX_RC) {
	    /* go back to Solicit */
	    DHCPv6Client_Solicit(client, IFEventID_start_e, NULL);
	    return;
	}
	timer_callout_set(client->timer,
			  DHCPv6ClientNextRetransmit(client,
						     DHCPv6_REQ_TIMEOUT,
						     DHCPv6_REQ_MAX_RT),
			  (timer_func_t *)DHCPv6Client_Request, 
			  client, (void *)IFEventID_timeout_e, NULL);
	my_log(LOG_INFO,
	       "%s: Request Transmit (try=%d)",
	       DHCPv6ClientGetDescription(client),
	       client->try);
	DHCPv6ClientSendPacket(client);
	break;
    }
    case IFEventID_data_e: {
	ADDR_PREFIX_U			addr_prefix;
	DHCPv6StatusCode		code;
	DHCPv6SocketReceiveDataRef 	data;
	bool				get_ia_na;
	IA_NA_PD_U			ia_na_pd;
	int				option_len;
	const char *			msg;
	unsigned int			msg_length;
	DHCPDUIDRef			server_id;

	data = (DHCPv6SocketReceiveDataRef)event_data;
	if (data->pkt->msg_type != kDHCPv6MessageREPLY
	    || !S_duid_matches(client, data->options)) {
	    /* not a match */
	    break;
	}
	server_id = (DHCPDUIDRef)
	    DHCPv6OptionListGetOptionDataAndLength(data->options,
						   kDHCPv6OPTION_SERVERID,
						   &option_len, NULL);
	if (server_id == NULL
	    || !DHCPDUIDIsValid(server_id, option_len)) {
	    /* missing/invalid DUID */
	    break;
	}
	if (!DHCPv6OptionListGetStatusCode(data->options, &code,
					   &msg, &msg_length)) {
	    /* ignore bad data */
	    break;
	}
	if (code != kDHCPv6StatusCodeSuccess) {
	    my_log(LOG_NOTICE,
		   "%s: %s %s '%.*s'",
		   DHCPv6ClientGetDescription(client),
		   DHCPv6MessageTypeName(data->pkt->msg_type),
		   DHCPv6StatusCodeGetName(code),
		   msg_length, msg);
	}
	if (code == kDHCPv6StatusCodeNoAddrsAvail
	    || code == kDHCPv6StatusCodeNoPrefixAvail) {
	    /* must ignore it */
	    break;
	}
	get_ia_na = (client->mode == kDHCPv6ClientModeStatefulAddress);
	ia_na_pd
	    = get_ia_na_pd_code(client, data->pkt->msg_type, get_ia_na,
				data->options, &addr_prefix, &code);
	if (code == kDHCPv6StatusCodeNotOnLink) {
	    /* go back to Solicit */
	    my_log(LOG_NOTICE,
		   "%s: NotOnLink",
		   DHCPv6ClientGetDescription(client));
	    DHCPv6Client_Solicit(client, IFEventID_start_e, NULL);
	    return;
	}
	if (ia_na_pd.ptr == NULL) {
	    /* no binding */
	    break;
	}
	DHCPv6ClientLogAddressOrPrefix(client, data->pkt->msg_type,
				       get_ia_na, &addr_prefix);
	DHCPv6ClientSavePacket(client, data);
	DHCPv6Client_Bound(client, IFEventID_start_e, NULL);
	break;
    }
    default:
	break;
    }
    return;
}

STATIC void
DHCPv6Client_Solicit(DHCPv6ClientRef client, IFEventID_t event_id, 
		     void * event_data)
{
    interface_t *	if_p = DHCPv6ClientGetInterface(client);

    switch (event_id) {
    case IFEventID_start_e:
	DHCPv6ClientSetState(client, kDHCPv6ClientStateSolicit);
	DHCPv6ClientClearRetransmit(client);
	DHCPv6ClientClearPacket(client);
	DHCPv6ClientSetNewTransactionID(client);
	DHCPv6SocketEnableReceive(client->sock,
				  DHCPv6ClientGetTransactionID(client),
				  (DHCPv6SocketReceiveFuncPtr)
				  DHCPv6Client_Solicit,
				  client, (void *)IFEventID_data_e);
	timer_callout_set(client->timer,
			  random_double_in_range(0, DHCPv6_SOL_MAX_DELAY),
			  (timer_func_t *)DHCPv6Client_Solicit, client,
			  (void *)IFEventID_timeout_e, NULL);
	break;
    case IFEventID_timeout_e: {
	if (client->try == 0) {
	    client->start_time = timer_get_current_time();
	}
	else {
	    link_status_t	link_status;

	    link_status = if_get_link_status(if_p);
	    if (link_status_is_inactive(&link_status)) {
		DHCPv6ClientInactive(client);
		break;
	    }
	}
	/* we received a response after waiting */
	if (client->saved.pkt_len != 0) {
	    DHCPv6Client_Request(client, IFEventID_start_e, NULL);
	    return;
	}
	timer_callout_set(client->timer,
			  DHCPv6ClientNextRetransmit(client,
						     DHCPv6_SOL_TIMEOUT,
						     DHCPv6_SOL_MAX_RT),
			  (timer_func_t *)DHCPv6Client_Solicit, 
			  client, (void *)IFEventID_timeout_e, NULL);
	my_log(LOG_INFO,
	       "%s: Solicit Transmit (try=%d)",
	       DHCPv6ClientGetDescription(client),
	       client->try);
	DHCPv6ClientSendSolicit(client);
#define GENERATE_SYMPTOM_AT_TRY		6
	if (client->mode == kDHCPv6ClientModeStatefulAddress
	    && client->try >= GENERATE_SYMPTOM_AT_TRY) {
	    /*
	     * We generally don't want to be calling the provided callback
	     * directly because of re-entrancy issues: the callback could call
	     * us, and we could call them, and enter an endless loop.
	     * This call is safe because we're running as a result of our timer
	     * and the client callback code isn't going to call back into us.
	     */
	    (*client->callback)(client, client->callback_arg,
				kDHCPv6ClientNotificationTypeGenerateSymptom);
	}
	break;
    }
    case IFEventID_data_e: {
	ADDR_PREFIX_U			addr_prefix;
	DHCPv6StatusCode		code;
	bool				get_ia_na;
	DHCPv6SocketReceiveDataRef 	data;
	IA_NA_PD_U			ia_na_pd;
	const char *			msg;
	unsigned int			msg_length;
	int				option_len;
	uint8_t				pref;
	DHCPDUIDRef			server_id;
	CFMutableStringRef		str;

	data = (DHCPv6SocketReceiveDataRef)event_data;
	if (data->pkt->msg_type != kDHCPv6MessageADVERTISE
	    || !S_duid_matches(client, data->options)) {
	    /* not a match */
	    break;
	}
	server_id = (DHCPDUIDRef)
	    DHCPv6OptionListGetOptionDataAndLength(data->options,
						   kDHCPv6OPTION_SERVERID,
						   &option_len, NULL);
	if (server_id == NULL
	    || !DHCPDUIDIsValid(server_id, option_len)) {
	    /* missing/invalid DUID */
	    break;
	}
	if (!DHCPv6OptionListGetStatusCode(data->options, &code,
					   &msg, &msg_length)) {
	    /* ignore bad data */
	    break;
	}
	if (code != kDHCPv6StatusCodeSuccess) {
	    my_log(LOG_NOTICE, "%s: %s %s '%.*s'",
		   DHCPv6ClientGetDescription(client),
		   DHCPv6MessageTypeName(data->pkt->msg_type),
		   DHCPv6StatusCodeGetName(code),
		   msg_length, msg);
	}
	if (code == kDHCPv6StatusCodeNoAddrsAvail
	    || code == kDHCPv6StatusCodeNoPrefixAvail) {
	    /* must ignore it */
	    break;
	}
	switch (client->mode) {
	case kDHCPv6ClientModeStatefulAddress:
	    get_ia_na = true;
	    break;
	case kDHCPv6ClientModeStatefulPrefix:
	    get_ia_na = false;
	    break;
	default:
	    goto done;
	}
	ia_na_pd = get_ia_na_pd(client, data->pkt->msg_type, get_ia_na,
				data->options, &addr_prefix);
	if (ia_na_pd.ptr == NULL) {
	    goto done;
	}
	DHCPv6ClientLogAddressOrPrefix(client, data->pkt->msg_type,
				       get_ia_na, &addr_prefix);

	/* check for a server preference value */
	pref = get_preference_value_from_options(data->options);

	/* if this response is "better" than one we saved, use it */
	if (client->saved.options != NULL) {
	    uint8_t		saved_pref;

	    saved_pref
		= get_preference_value_from_options(client->saved.options);
	    if (saved_pref >= pref) {
		/* saved packet is still "better" */
		break;
	    }
	}
	str = CFStringCreateMutable(NULL, 0);
	DHCPDUIDPrintToString(str, server_id, option_data_get_length(server_id));
	my_log(LOG_INFO,
	       "%s: Saving Advertise from %@",
	       DHCPv6ClientGetDescription(client),
	       str);
	CFRelease(str);
	DHCPv6ClientSavePacket(client, data);
	if (client->try > 1 || pref == kDHCPv6OptionPREFERENCEMaxValue) {
	    /* already waited, or preference is max, move to Request */
	    DHCPv6Client_Request(client, IFEventID_start_e, NULL);
	    break;
	}
	break;
    }
    default:
	break;
    }

 done:
    return;
}

PRIVATE_EXTERN DHCPv6ClientMode
DHCPv6ClientGetMode(DHCPv6ClientRef client)
{
    return (client->mode);
}

PRIVATE_EXTERN DHCPv6ClientRef
DHCPv6ClientCreate(ServiceRef service_p)
{
    DHCPv6ClientRef		client;
    interface_t * 		if_p = service_interface(service_p);
    char			timer_name[32];

    client = (DHCPv6ClientRef)malloc(sizeof(*client));
    bzero(client, sizeof(*client));
    client->service_p = service_p;
    client->sock = DHCPv6SocketCreate(if_p);
    snprintf(timer_name, sizeof(timer_name),
	     "DHCPv6-%s", if_name(if_p));
    client->timer = timer_callout_init(timer_name);
    DHCPv6ClientSetDescription(client);
    return (client);
}

STATIC void
DHCPv6ClientStartInternal(DHCPv6ClientRef client)
{
    CFAbsoluteTime		current_time;
    interface_t *		if_p = DHCPv6ClientGetInterface(client);

    switch (client->mode) {
    case kDHCPv6ClientModeStateless:
	DHCPv6Client_Inform(client, IFEventID_start_e, NULL);
	break;
    case kDHCPv6ClientModeStatefulPrefix:
    case kDHCPv6ClientModeStatefulAddress:
	my_log(LOG_NOTICE, "%s: %s()",
	       DHCPv6ClientGetDescription(client), __func__);
	current_time = timer_get_current_time();
	if (DHCPv6ClientLeaseStillValid(client, current_time)
	    && DHCPv6ClientLeaseOnSameNetwork(client)) {
	    my_log(LOG_NOTICE, "%s: %s() CONFIRM",
		   DHCPv6ClientGetDescription(client), __func__);
	    DHCPv6Client_Confirm(client, IFEventID_start_e, NULL);
	}
	else {
	    my_log(LOG_NOTICE, "%s: %s() SOLICIT",
		   DHCPv6ClientGetDescription(client), __func__);
	    DHCPv6Client_Solicit(client, IFEventID_start_e, NULL);
	}
	break;
    default:
	/* must specify a mode */
	my_log(LOG_NOTICE, "%s(%s): no/invalid mode specified",
	       __func__, if_name(if_p));
	return;
    }
    return;
}

PRIVATE_EXTERN void
DHCPv6ClientStart(DHCPv6ClientRef client)
{
    interface_t *		if_p = DHCPv6ClientGetInterface(client);

    my_log(LOG_NOTICE, "%s(%s): %s using %s address",
	   __func__, if_name(if_p),
	   DHCPv6ClientModeGetName(client->mode),
	   DHCPv6ClientUsePrivateAddress(client) ? "private" : "permanent");
    DHCPv6ClientStartInternal(client);
}

PRIVATE_EXTERN void
DHCPv6ClientSetMode(DHCPv6ClientRef client, DHCPv6ClientMode mode)
{
    if (client->mode == mode) {
	/* already in the right mode */
	return;
    }
    if (DHCPv6ClientVerifyModeIsNone(client, __func__)) {
	client->mode = mode;
	DHCPv6ClientSetDescription(client);
    }
    return;
}

PRIVATE_EXTERN void
DHCPv6ClientDiscardInformation(DHCPv6ClientRef client)
{
    if (DHCPv6ClientVerifyModeIsNone(client, __func__)) {
	DHCPv6ClientClearPacket(client);
    }
    return;
}

STATIC void
DHCPv6ClientSetRequestedPrefixLength(DHCPv6ClientRef client,
				     uint8_t prefix_length)
{
    if (client->mode != kDHCPv6ClientModeStatefulPrefix) {
	return;
    }
    client->requested_prefix_length = prefix_length;
    return;
}

STATIC void
DHCPv6ClientSetRequestedPrefix(DHCPv6ClientRef client,
			       const struct in6_addr * prefix)
{
    if (client->mode != kDHCPv6ClientModeStatefulPrefix) {
	return;
    }
    client->requested_prefix = *prefix;
    return;
}

STATIC uint8_t
DHCPv6ClientGetRequestedPrefixLength(DHCPv6ClientRef client)
{
    return (client->requested_prefix_length);
}

PRIVATE_EXTERN void
DHCPv6ClientStop(DHCPv6ClientRef client)
{
    /* remove the IP address */
    DHCPv6ClientRemoveAddress(client, "Stop");
    DHCPv6ClientCancelPendingEvents(client);
    bzero(&client->delegated_prefix, sizeof(client->delegated_prefix));
    client->delegated_prefix_length = 0;
    client->saved_verified = false;
    DHCPv6ClientSetState(client, kDHCPv6ClientStateInactive);
    client->mode = kDHCPv6ClientModeNone;
    my_CFRelease(&client->duid);
    DHCPv6ClientPostNotification(client);
    return;
}

PRIVATE_EXTERN void
DHCPv6ClientRelease(DHCPv6ClientRef * client_p)
{
    DHCPv6ClientRef	client = *client_p;
    CFAbsoluteTime	current_time;

    if (client == NULL) {
	return;
    }
    *client_p = NULL;
    current_time = timer_get_current_time();
    if (DHCPv6ClientLeaseStillValid(client, current_time)) {
	DHCPv6Client_Release(client, IFEventID_start_e, NULL);
    }
    if (client->timer != NULL) {
	timer_callout_free(&client->timer);
    }
    DHCPv6SocketRelease(&client->sock);
    DHCPv6ClientClearPacket(client);
    DHCPv6ClientSetNotificationCallBack(client, NULL, NULL);
    DHCPv6OptionListRelease(&client->saved.options);
    my_CFRelease(&client->duid);
    free(client);
    return;
}

PRIVATE_EXTERN bool
DHCPv6ClientGetInfo(DHCPv6ClientRef client, ipv6_info_t * info_p)
{
    if (client->saved.options == NULL || !client->saved_verified
	|| client->saved.pkt_len == 0) {
	info_p->pkt = NULL;
	info_p->pkt_len = 0;
	info_p->options = NULL;
	info_p->is_stateful = FALSE;
	return (false);
    }
    info_p->pkt = client->saved.pkt;
    info_p->pkt_len = client->saved.pkt_len;
    info_p->options = client->saved.options;
    switch (client->mode) {
    case kDHCPv6ClientModeStatefulPrefix:
	info_p->prefix = client->delegated_prefix;
	info_p->prefix_length = client->delegated_prefix_length;
	info_p->prefix_valid_lifetime = client->lease.valid_lifetime;
	info_p->prefix_preferred_lifetime = client->lease.preferred_lifetime;
	/* FALL THROUGH */
    case kDHCPv6ClientModeStatefulAddress:
	info_p->is_stateful = TRUE;
	info_p->lease_start = client->lease.start;
	info_p->lease_expiration = 0;
	if (client->lease.valid_lifetime != DHCP_INFINITE_LEASE) {
	    info_p->lease_expiration = client->lease.start
		+ client->lease.valid_lifetime;
	}
	break;
    default:
	break;
    }
    return (true);
}

PRIVATE_EXTERN void
DHCPv6ClientCopyAddresses(DHCPv6ClientRef client, 
			  inet6_addrlist_t * addr_list_p)
{
    if (IN6_IS_ADDR_UNSPECIFIED(&client->our_ip)) {
	inet6_addrlist_init(addr_list_p);
	return;
    }
    addr_list_p->list = addr_list_p->list_static;
    addr_list_p->count = 1;
    addr_list_p->list[0].addr = client->our_ip;
    addr_list_p->list[0].prefix_length = client->our_prefix_length;
    addr_list_p->list[0].addr_flags = 0;
    return;
}

STATIC void 
DHCPv6ClientDeliverNotification(void * info)
{
    DHCPv6ClientRef	client = (DHCPv6ClientRef)info;

    if (client->callback == NULL) {
	/* this can't really happen */
	my_log(LOG_NOTICE,
	       "DHCPv6Client: runloop source signaled but callback is NULL");
	return;
    }
    (*client->callback)(client, client->callback_arg,
			kDHCPv6ClientNotificationTypeStatusChanged);
    return;
}

PRIVATE_EXTERN void
DHCPv6ClientSetNotificationCallBack(DHCPv6ClientRef client, 
				    DHCPv6ClientNotificationCallBack callback,
				    void * callback_arg)
{
    client->callback = callback;
    client->callback_arg = callback_arg;
    if (callback == NULL) {
	if (client->callback_rls != NULL) {
	    CFRunLoopSourceInvalidate(client->callback_rls);
	    my_CFRelease(&client->callback_rls);
	}
    }
    else if (client->callback_rls == NULL) {
	CFRunLoopSourceContext 	context;

	bzero(&context, sizeof(context));
	context.info = (void *)client;
	context.perform = DHCPv6ClientDeliverNotification;
	client->callback_rls = CFRunLoopSourceCreate(NULL, 0, &context);
	CFRunLoopAddSource(CFRunLoopGetCurrent(), client->callback_rls,
			   kCFRunLoopDefaultMode);
    }
    return;
}

STATIC void
DHCPv6ClientHandleLinkStatusRenew(DHCPv6ClientRef client,
				  void * event_data)
{
    link_event_data_t	link_event = (link_event_data_t)event_data;
    link_status_t *	link_status_p = &link_event->link_status;

    if (link_status_is_active(link_status_p)) {
	my_log(LOG_NOTICE, "%s: %s() link is active",
	       DHCPv6ClientGetDescription(client), __func__);
	DHCPv6ClientStartInternal(client);
    }
    else {
	my_log(LOG_NOTICE, "%s: %s() link is inactive",
	       DHCPv6ClientGetDescription(client), __func__);
    }
    return;
}

STATIC void
DHCPv6ClientHandleRoam(DHCPv6ClientRef client)
{
    CFAbsoluteTime	current_time;

    /* we roamed, confirm the address if necessary */
    my_log(LOG_NOTICE,
	   "%s: %s() state is %s",
	   DHCPv6ClientGetDescription(client),
	   __func__,
	   DHCPv6ClientStateGetName(client->cstate));
    current_time = timer_get_current_time();
    if (DHCPv6ClientLeaseStillValid(client, current_time)
	&& S_dhcp_state_is_bound_renew_or_rebind(client->cstate)) {
	DHCPv6Client_Confirm(client, IFEventID_start_e, NULL);
    }
    return;
}

void
DHCPv6ClientHandleEvent(DHCPv6ClientRef client, IFEventID_t event_ID,
			void * event_data)
{
    if (DHCPv6ClientGetMode(client) != kDHCPv6ClientModeStatefulAddress) {
	/* not stateful, ignore */
	return;
    }

    switch (event_ID) {
    case IFEventID_ipv6_address_changed_e:
	DHCPv6ClientHandleAddressChanged(client,
					 (inet6_addrlist_t *)event_data);
	break;
    case IFEventID_wake_e: {
	DHCPv6ClientHandleWake(client, event_data);
	break;
    }
    case IFEventID_renew_e:
    case IFEventID_link_status_changed_e:
	DHCPv6ClientHandleLinkStatusRenew(client, event_data);
	break;
    case IFEventID_bssid_changed_e:
	DHCPv6ClientHandleRoam(client);
	break;
    default:
	break;
    }
    return;
}

STATIC void
dhcpv6_pd_notify(DHCPv6ClientRef client, void * callback_arg,
		 DHCPv6ClientNotificationType type)
{
#pragma unused(callback_arg)
#pragma unused(type)
    ipv6_info_t			info;
    CFMutableDictionaryRef	summary;

    summary = CFDictionaryCreateMutable(NULL, 0,
					&kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks);
    DHCPv6ClientProvideSummary(client, summary);
    my_log(LOG_NOTICE, "%s: %s() %@",
	   DHCPv6ClientGetDescription(client),
	   __func__, summary);
    CFRelease(summary);
    bzero(&info, sizeof(info));
    if (DHCPv6ClientGetInfo(client, &info)) {
	ServicePublishSuccessIPv6(client->service_p,
				  NULL, 0,
				  NULL, 0,
				  &info,
				  NULL);
    }
    else if (ServiceIsPublished(client->service_p)) {
	service_publish_failure(client->service_p,
				ipconfig_status_media_inactive_e);
    }
    return;
}


STATIC void
dhcpv6_pd_start(ServiceRef service_p, void * event_data)
{
    DHCPv6ClientRef		client;
    interface_t *		if_p = service_interface(service_p);
    link_status_t		link_status;
    ipconfig_method_data_t	method_data;

    client = DHCPv6ClientCreate(service_p);
    ServiceSetPrivate(service_p, client);
    DHCPv6ClientSetMode(client, kDHCPv6ClientModeStatefulPrefix);
    my_log(LOG_NOTICE, "%s: start",
	   DHCPv6ClientGetDescription(client));

    /* check for prefix/prefix_length */
    method_data = (ipconfig_method_data_t)event_data;
    if (method_data != NULL) {
	const struct in6_addr *	prefix;
	uint8_t			prefix_length;

	prefix = &method_data->dhcpv6_pd.requested_prefix;
	prefix_length = method_data->dhcpv6_pd.requested_prefix_length;
	if (prefix_length != 0) {
	    DHCPv6ClientSetRequestedPrefixLength(client, prefix_length);
	    DHCPv6ClientSetRequestedPrefix(client, prefix);
	}
    }
    DHCPv6ClientSetNotificationCallBack(client, dhcpv6_pd_notify, NULL);
    link_status = if_get_link_status(if_p);
    if (link_status_is_active(&link_status)) {
	DHCPv6ClientStart(client);
    }
    return;
}

PRIVATE_EXTERN ipconfig_status_t
dhcpv6_pd_thread(ServiceRef service_p, IFEventID_t event_id, void * event_data)
{
    DHCPv6ClientRef	client = (DHCPv6ClientRef)ServiceGetPrivate(service_p);
    interface_t *	if_p = service_interface(service_p);
    ipconfig_status_t	status = ipconfig_status_success_e;


    switch (event_id) {
      case IFEventID_start_e:
	  if (client != NULL) {
	      my_log(LOG_NOTICE, "%s: re-entering start state",
		     DHCPv6ClientGetDescription(client));
	      status = ipconfig_status_internal_error_e;
	      break;
	  }
	  dhcpv6_pd_start(service_p, event_data);
	  break;
      case IFEventID_stop_e:
	  my_log(LOG_NOTICE, "DHCPv6 %s: stop",
		 if_name(if_p));
	  if (client == NULL) {
	      my_log(LOG_NOTICE, "DHCPv6 %s: already stopped",
		     if_name(if_p));
	      status = ipconfig_status_internal_error_e; /* shouldn't happen */
	      break;
	  }
	  DHCPv6ClientRelease(&client);
	  ServiceSetPrivate(service_p, NULL);
	  break;
      case IFEventID_change_e: {
	  change_event_data_t *		change_event;
	  ipconfig_method_data_t	method_data;

	  change_event = (change_event_data_t *)event_data;
	  method_data = change_event->method_data;
	  change_event->needs_stop = FALSE;
	  if (method_data != NULL) {
	      ipconfig_method_data_dhcpv6_pd_t dhcpv6_pd;

	      dhcpv6_pd = &method_data->dhcpv6_pd;
	      if (DHCPv6ClientGetRequestedPrefixLength(client)
		  != dhcpv6_pd->requested_prefix_length) {
		  change_event->needs_stop = TRUE;
	      }
	      else if (!IN6_ARE_ADDR_EQUAL(&dhcpv6_pd->requested_prefix,
					   &client->requested_prefix)) {
		  change_event->needs_stop = TRUE;
	      }
	  }
	  return (ipconfig_status_success_e);
      }
      case IFEventID_renew_e:
      case IFEventID_link_status_changed_e:
	  my_log(LOG_NOTICE, "%s: %s() link status changed",
		 DHCPv6ClientGetDescription(client), __func__);
	  DHCPv6ClientSetMode(client, kDHCPv6ClientModeStatefulPrefix);
	  DHCPv6ClientHandleLinkStatusRenew(client, event_data);
	  break;
      case IFEventID_link_timer_expired_e:
	  DHCPv6ClientStop(client);
	  break;
      case IFEventID_wake_e:
	  DHCPv6ClientHandleWake(client, event_data);
	  break;
      case IFEventID_get_ipv6_info_e:
	  DHCPv6ClientGetInfo(client, (ipv6_info_t *)event_data);
	  break;
      case IFEventID_bssid_changed_e:
	  DHCPv6ClientHandleRoam(client);
	  break;
      case IFEventID_provide_summary_e:
	  DHCPv6ClientProvideSummary(client, (CFMutableDictionaryRef)event_data);
	  break;
      default:
	  break;
    } /* switch (event_id) */
    return (status);
}

#if TEST_DHCPV6_CLIENT
#include "sysconfig.h"
#include "wireless.h"
#include <SystemConfiguration/SCPrivate.h>

boolean_t G_is_netboot;
DHCPDUIDType G_dhcp_duid_type;
Boolean G_IPConfiguration_verbose = TRUE;
int  G_wake_skew_secs = 30;

struct ServiceInfo {
    interface_t	*	if_p;
    WiFiInfoRef		wifi_info_p;
    void *		priv;
    ipconfig_status_t	status;
    boolean_t		ready;
};

void *
ServiceGetPrivate(ServiceRef service_p)
{
    return (service_p->priv);
}

void
ServiceSetPrivate(ServiceRef service_p, void * priv)
{
    service_p->priv = priv;
}

interface_t *
service_interface(ServiceRef service_p)
{
    return (service_p->if_p);
}

CFStringRef
ServiceGetSSID(ServiceRef service_p)
{
    if (service_p->wifi_info_p == NULL) {
	return (NULL);
    }
    return (WiFiInfoGetSSID(service_p->wifi_info_p));
}

void
ServicePublishSuccessIPv6(ServiceRef service_p,
			  inet6_addrinfo_t * addresses, int addresses_count,
			  const struct in6_addr * router, int router_count,
			  ipv6_info_t * ipv6_info_p,
			  CFStringRef signature)
{
#pragma unused(addresses)
#pragma unused(addresses_count)
#pragma unused(router)
#pragma unused(router_count)
#pragma unused(ipv6_info_p)
#pragma unused(signature)
    service_p->status = ipconfig_status_success_e;
    service_p->ready = TRUE;
}

void
service_publish_failure(ServiceRef service_p, ipconfig_status_t status)
{
#pragma unused(status)
    service_p->status = status;
    service_p->ready = FALSE;
}

boolean_t
ServiceIsPublished(ServiceRef service_p)
{
    return (service_p->ready && service_p->status == ipconfig_status_success_e);
}

STATIC void
DHCPv6ClientSendBadOptions(DHCPv6ClientRef client)
{
    char			buf[1500];
    int				error;
    interface_t *		if_p = DHCPv6ClientGetInterface(client);
    DHCPv6OptionArea		oa;
    DHCPv6OptionRef		opt;
    DHCPv6PacketRef		pkt;
    int				pkt_len;

    pkt = DHCPv6ClientMakePacket(client, kDHCPv6MessageINFORMATION_REQUEST,
				 buf, sizeof(buf), &oa);
    if (pkt == NULL) {
	return;
    }

    opt = (DHCPv6OptionRef)(oa.buf + oa.used);
    DHCPv6OptionSetCode(opt, kDHCPv6OPTION_SERVERID);
    DHCPv6OptionSetLength(opt, 64); /* pretend that it's longer */
    opt->data[0] = 'X';
    opt->data[1] = 'X';
    opt->data[2] = 'X';
    opt->data[3] = 'X';
    oa.used += 8; /* only put in 8 bytes */
    pkt_len = DHCPV6_PACKET_HEADER_LENGTH + DHCPv6OptionAreaGetUsedLength(&oa);

    for (int i = 0; i < (1024 * 1024); i++) {
	error = DHCPv6SocketTransmit(client->sock, pkt, pkt_len);
	switch (error) {
	case 0:
	    break;
	case ENXIO:
	case ENETDOWN:
	    fprintf(stderr, "DHCPv6SocketTransmit failed, %d (%s)\n",
		    error, strerror(error));
	    return;
	default:
	    printf("send failed, waiting a bit\n");
	    my_log(LOG_NOTICE, "DHCPv6 %s: SendBadOptions transmit failed, %s",
		   if_name(if_p), strerror(error));
	    usleep(1000);
	    break;
	}
    }
    return;
}

STATIC void
client_notification(DHCPv6ClientRef client,
		    void * callback_arg,
		    DHCPv6ClientNotificationType type)
{
    ipv6_info_t	info;

    if (!DHCPv6ClientGetInfo(client, &info)) {
	printf("DHCPv6 updated: no info\n");
    }
    else {
	printf("DHCPv6 updated\n");
	DHCPv6OptionListFPrint(stdout, info.options);
    }
    return;
}

interface_list_t *
get_interface_list(void)
{
    STATIC interface_list_t *	S_interfaces;

    if (S_interfaces == NULL) {
	S_interfaces = ifl_init();
    }
    return (S_interfaces);
}

STATIC DHCPv6ClientMode	S_mode;

STATIC void
handle_change(SCDynamicStoreRef session, CFArrayRef changes, void * arg)
{
    int			count;
    DHCPv6ClientRef	client = (DHCPv6ClientRef)arg;
    int			i;
    interface_t *	if_p = DHCPv6ClientGetInterface(client);


    if (changes == NULL || (count = CFArrayGetCount(changes)) == 0) {
	return;
    }

    for (i = 0; i < count; i++) {
	CFStringRef	key = CFArrayGetValueAtIndex(changes, i);

	if (CFStringHasSuffix(key, kSCEntNetLink)) {
	    CFBooleanRef	active = kCFBooleanTrue;
	    CFDictionaryRef	dict;

	    dict = SCDynamicStoreCopyValue(session, key);
	    if (dict != NULL) {
		if (CFDictionaryGetValue(dict, kSCPropNetLinkDetaching)) {
		    my_log(LOG_NOTICE, "%s detaching - exiting",
			   if_name(if_p));
		    exit(0);
		}
		active = CFDictionaryGetValue(dict, kSCPropNetLinkActive);
	    }
	    if (CFEqual(active, kCFBooleanTrue)) {
		DHCPv6ClientSetMode(client, S_mode);
		DHCPv6ClientStart(client);
	    }
	    else {
		DHCPv6ClientStop(client);
	    }
	}
	else if (CFStringHasSuffix(key, kSCEntNetIPv6)) {
	    inet6_addrlist_t 	addr_list;

	    /* get the addresses from the interface and deliver the event */
	    inet6_addrlist_copy(&addr_list, if_link_index(if_p));
	    DHCPv6ClientHandleEvent(client,
				    IFEventID_ipv6_address_changed_e,
				    &addr_list);
	    inet6_addrlist_free(&addr_list);
	}
    }
    return;
}

STATIC void
notification_init(DHCPv6ClientRef client)
{
    CFArrayRef			array;
    SCDynamicStoreContext	context;
    CFStringRef			ifname_cf;
    const void *		keys[2];
    CFRunLoopSourceRef		rls;
    SCDynamicStoreRef		store;

    bzero(&context, sizeof(context));
    context.info = client;
    store = SCDynamicStoreCreate(NULL, CFSTR("DHCPv6Client"),
				 handle_change, &context);
    if (store == NULL) {
	my_log(LOG_NOTICE, "SCDynamicStoreCreate failed: %s",
	       SCErrorString(SCError()));
	return;
    }
    ifname_cf
	= CFStringCreateWithCString(NULL,
				    if_name(DHCPv6ClientGetInterface(client)),
				    kCFStringEncodingUTF8);
    keys[0] = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							    kSCDynamicStoreDomainState,
							    ifname_cf,
							    kSCEntNetIPv6);
    keys[1] = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							    kSCDynamicStoreDomainState,
							    ifname_cf,
							    kSCEntNetLink);
    CFRelease(ifname_cf);
    array = CFArrayCreate(NULL, (const void **)keys, 2, &kCFTypeArrayCallBacks);
    SCDynamicStoreSetNotificationKeys(store, array, NULL);
    CFRelease(array);
    rls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);
    return;
}

#define kOptStateless		"stateless"
#define kOptStatefulAddress	"stateful-address"
#define kOptStatefulPrefix	"stateful-prefix"

static void
usage(const char * progname)
{
    fprintf(stderr,
	    "usage: %s -i <ifname> [ -m <mode> ]"
	    " [ -b ] [ -P ] [ -p <prefix_length> ]\n"
	    "\t<mode> is one of '"
	    kOptStateless "', '" kOptStatefulAddress "', "
	    kOptStatefulPrefix "\n",
	    progname);
    exit(1);
}

STATIC bool
get_mode_from_string(const char * str, DHCPv6ClientMode * ret_mode)
{
    DHCPv6ClientMode	mode;
    bool		success = true;

    if (strcmp(str, kOptStateless) == 0) {
	mode = kDHCPv6ClientModeStateless;
    }
    else if (strcmp(str, kOptStatefulAddress) == 0) {
	mode = kDHCPv6ClientModeStatefulAddress;
    }
    else if (strcmp(str, kOptStatefulPrefix) == 0) {
	mode = kDHCPv6ClientModeStatefulPrefix;
    }
    else {
	mode = kDHCPv6ClientModeNone;
	success = false;
    }
    *ret_mode = mode;
    return (success);
}

int
main(int argc, char * argv[])
{
    int 			ch;
    DHCPv6ClientRef		client;
    interface_t *		if_p;
    const char *		ifname = NULL;
    interface_list_t *		interfaces = NULL;
    int				prefix_length = 0;
    const char *		progname = argv[0];
    bool			send_bad_options = false;
    struct ServiceInfo		service;
    bool			use_privacy = false;

    S_mode = kDHCPv6ClientModeStateless;
    while ((ch = getopt(argc, argv, "bi:m:Pp:")) != -1) {
	switch (ch) {
	case 'b':
	    send_bad_options = true;
	    break;
	case 'i':
	    ifname = optarg;
	    break;
	case 'm':
	    if (!get_mode_from_string(optarg, &S_mode)) {
		fprintf(stderr, "Bad mode '%s'\n", optarg);
		usage(progname);
	    }
	    break;
	case 'P':
	    use_privacy = true;
	    break;
	case 'p':
	    prefix_length = strtoul(optarg, NULL, 0);
	    break;
	default:
	    usage(progname);
	    break;
	}
    }
    if (ifname == NULL) {
	usage(progname);
    }
    argc -= optind;
    argv += optind;

    if (argc > 0) {
	fprintf(stderr, "Extra arguments provided\n");
	usage(progname);
    }
    interfaces = get_interface_list();
    if (interfaces == NULL) {
	fprintf(stderr, "failed to get interface list\n");
	exit(2);
    }
    if_p = ifl_find_name(interfaces, ifname);
    if (if_p == NULL) {
	fprintf(stderr, "No such interface '%s'\n", ifname);
	exit(2);
    }
    DHCPv6SocketSetVerbose(true);
    bzero(&service, sizeof(service));
    service.if_p = if_p;
    if (if_is_wireless(if_p)) {
	CFStringRef	ifname_cf;

	ifname_cf = CFStringCreateWithCString(NULL, if_name(if_p),
					      kCFStringEncodingUTF8);
	service.wifi_info_p = WiFiInfoCopy(ifname_cf);
	CFRelease(ifname_cf);
    }
    client = DHCPv6ClientCreate(&service);
    if (client == NULL) {
	fprintf(stderr, "DHCPv6ClientCreate(%s) failed\n", ifname);
	exit(2);
    }
    if (send_bad_options) {
	printf("Sending bad options\n");
	DHCPv6ClientSendBadOptions(client);
    }
    else {
	notification_init(client);
	DHCPv6ClientSetUsePrivateAddress(client, use_privacy);
	DHCPv6ClientSetNotificationCallBack(client, client_notification, NULL);
	DHCPv6ClientSetMode(client, S_mode);
	switch (S_mode) {
	case kDHCPv6ClientModeStatefulPrefix:
	    printf("Starting %s /%d%s\n",
		   DHCPv6ClientModeGetName(S_mode),
		   prefix_length,
		   use_privacy ? " [private address]" : "");
	    DHCPv6ClientSetRequestedPrefixLength(client, prefix_length);
	    break;
	default:
	    printf("Starting %s%s\n",
		   DHCPv6ClientModeGetName(S_mode),
		   use_privacy ? " [private address]" : "");
	    break;
	}
	DHCPv6ClientStart(client);
	CFRunLoopRun();
    }
    exit(0);
    return (0);
}
#endif /* TEST_DHCPV6_CLIENT */
