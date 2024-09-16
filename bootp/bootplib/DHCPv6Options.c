/*
 * Copyright (c) 2009-2024 Apple Inc. All rights reserved.
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
 * DHCPv6Options.c
 * - definitions and API's to handle DHCPv6 options
 */

/* 
 * Modification History
 *
 * September 18, 2009		Dieter Siegmund (dieter@apple.com)
 * - created
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <mach/boolean.h>
#include <string.h>
#include <errno.h>
#include "DHCPv6.h"
#include "DHCPv6Options.h"
#include "ptrlist.h"
#include "util.h"
#include "DNSNameList.h"
#include "cfutil.h"
#include <SystemConfiguration/SCPrivate.h>

#ifndef my_log
#define my_log	SC_log
#endif

#define kDHCPv6OPTIONSTR_DNS_SERVERS		"DNS_SERVERS"
#define kDHCPv6OPTIONSTR_DOMAIN_LIST		"DOMAIN_LIST"
#define kDHCPv6OPTIONSTR_POSIX_TIMEZONE		"POSIX_TIMEZONE"
#define kDHCPv6OPTIONSTR_TZDB_TIMEZONE		"TZDB_TIMEZONE"
#define kDHCPv6OPTIONSTR_CAPTIVE_PORTAL_URL	"CAPTIVE_PORTAL_URL"

STATIC void
DHCPv6OptionIA_NAPrintLevelToString(CFMutableStringRef str,
				    const DHCPv6OptionIA_NARef ia_na,
				    int ia_na_len, int level);

STATIC void
DHCPv6OptionIAADDRPrintLevelToString(CFMutableStringRef str,
				     DHCPv6OptionIAADDRRef ia_addr,
				     int ia_addr_len, int level);

STATIC void
DHCPv6OptionIA_PDPrintLevelToString(CFMutableStringRef str,
				    const DHCPv6OptionIA_PDRef ia_pd,
				    int ia_pd_len, int level);

STATIC void
DHCPv6OptionIAPREFIXPrintLevelToString(CFMutableStringRef str,
				     DHCPv6OptionIAPREFIXRef ia_prefix,
				     int ia_prefix_len, int level);

STATIC void
DHCPv6OptionSTATUS_CODEPrintToString(CFMutableStringRef str,
				     DHCPv6OptionSTATUS_CODERef status_p,
				     int status_len);

PRIVATE_EXTERN DHCPv6OptionType
DHCPv6OptionCodeGetType(DHCPv6OptionCode option_code)
{
    DHCPv6OptionType	type = kDHCPv6OptionTypeUnknown;

    switch (option_code) {
    case kDHCPv6OPTION_CLIENTID:
    case kDHCPv6OPTION_SERVERID:
	type = kDHCPv6OptionTypeDUID;
	break;
    case kDHCPv6OPTION_ORO:
	type = kDHCPv6OptionTypeUInt16;
	break;
    case kDHCPv6OPTION_ELAPSED_TIME:
	type = kDHCPv6OptionTypeUInt16;
	break;
    case kDHCPv6OPTION_UNICAST:
	type = kDHCPv6OptionTypeIPv6Address;
	break;
    case kDHCPv6OPTION_RAPID_COMMIT:
	type = kDHCPv6OptionTypeNone; /* i.e. no data */
	break;
    case kDHCPv6OPTION_SIP_SERVER_A:
    case kDHCPv6OPTION_DNS_SERVERS:
	type = kDHCPv6OptionTypeIPv6Address;
	break;
    case kDHCPv6OPTION_SIP_SERVER_D:
    case kDHCPv6OPTION_DOMAIN_LIST:
	type = kDHCPv6OptionTypeDNSNameList;
	break;
    case kDHCPv6OPTION_IA_NA:
	type = kDHCPv6OptionTypeIA_NA;
	break;
    case kDHCPv6OPTION_IA_PD:
	type = kDHCPv6OptionTypeIA_PD;
	break;
    case kDHCPv6OPTION_IAADDR:
	type = kDHCPv6OptionTypeIAADDR;
	break;
    case kDHCPv6OPTION_IAPREFIX:
	type = kDHCPv6OptionTypeIAPREFIX;
	break;
    case kDHCPv6OPTION_STATUS_CODE:
	type = kDHCPv6OptionTypeStatusCode;
	break;
    case kDHCPv6OPTION_POSIX_TIMEZONE:
    case kDHCPv6OPTION_TZDB_TIMEZONE:
    case kDHCPv6OPTION_CAPTIVE_PORTAL_URL:
	type = kDHCPv6OptionTypeString;
	break;
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
    default:
	break;
    }
    return (type);
}

typedef struct {
    DHCPv6OptionCode	code;
    const char *	name;
} OptionCodeName, *OptionCodeNameRef;

STATIC OptionCodeName option_code_names[] = {
    {
	kDHCPv6OPTION_DNS_SERVERS,
	kDHCPv6OPTIONSTR_DNS_SERVERS
    },
    {
	kDHCPv6OPTION_DOMAIN_LIST,
	kDHCPv6OPTIONSTR_DOMAIN_LIST
    },
    {
	kDHCPv6OPTION_POSIX_TIMEZONE,
	kDHCPv6OPTIONSTR_POSIX_TIMEZONE
    },
    {
	kDHCPv6OPTION_TZDB_TIMEZONE,
	kDHCPv6OPTIONSTR_TZDB_TIMEZONE
    },
    {
	kDHCPv6OPTION_CAPTIVE_PORTAL_URL,
	kDHCPv6OPTIONSTR_CAPTIVE_PORTAL_URL
    },
};

PRIVATE_EXTERN DHCPv6OptionCode
DHCPv6OptionNameGetCode(const char * name)
{
    int			i;
    OptionCodeNameRef	scan;

    for (i = 0, scan = option_code_names;
	 i < countof(option_code_names);
	 i++, scan++) {
	if (!strcasecmp(name, scan->name)) {
	    return (scan->code);
	}
    }
    return (kDHCPv6OPTION_NONE);
}

PRIVATE_EXTERN const char * 
DHCPv6OptionCodeGetName(DHCPv6OptionCode code)
{
    const char * 	str;

    switch (code) {
    case kDHCPv6OPTION_CLIENTID:
	str = "CLIENTID";
	break;
    case kDHCPv6OPTION_SERVERID:
	str = "SERVERID";
	break;
    case kDHCPv6OPTION_IA_NA:
	str = "IA_NA";
	break;
    case kDHCPv6OPTION_IA_TA:
	str = "IA_TA";
	break;
    case kDHCPv6OPTION_IAADDR:
	str = "IAADDR";
	break;
    case kDHCPv6OPTION_ORO:
	str = "ORO";
	break;
    case kDHCPv6OPTION_PREFERENCE:
	str = "PREFERENCE";
	break;
    case kDHCPv6OPTION_ELAPSED_TIME:
	str = "ELAPSED_TIME";
	break;
    case kDHCPv6OPTION_RELAY_MSG:
	str = "RELAY_MSG";
	break;
    case kDHCPv6OPTION_AUTH:
	str = "AUTH";
	break;
    case kDHCPv6OPTION_UNICAST:
	str = "UNICAST";
	break;
    case kDHCPv6OPTION_STATUS_CODE:
	str = "STATUS_CODE";
	break;
    case kDHCPv6OPTION_RAPID_COMMIT:
	str = "RAPID_COMMIT";
	break;
    case kDHCPv6OPTION_USER_CLASS:
	str = "USER_CLASS";
	break;
    case kDHCPv6OPTION_VENDOR_CLASS:
	str = "VENDOR_CLASS";
	break;
    case kDHCPv6OPTION_VENDOR_OPTS:
	str = "VENDOR_OPTS";
	break;
    case kDHCPv6OPTION_INTERFACE_ID:
	str = "INTERFACE_ID";
	break;
    case kDHCPv6OPTION_RECONF_MSG:
	str = "RECONF_MSG";
	break;
    case kDHCPv6OPTION_RECONF_ACCEPT:
	str = "RECONF_ACCEPT";
	break;
    case kDHCPv6OPTION_SIP_SERVER_D:
	str = "SIP_SERVER_D";
	break;
    case kDHCPv6OPTION_SIP_SERVER_A:
	str = "SIP_SERVER_A";
	break;
    case kDHCPv6OPTION_DNS_SERVERS:
	str = kDHCPv6OPTIONSTR_DNS_SERVERS;
	break;
    case kDHCPv6OPTION_DOMAIN_LIST:
	str = kDHCPv6OPTIONSTR_DOMAIN_LIST;
	break;
    case kDHCPv6OPTION_POSIX_TIMEZONE:
	str = kDHCPv6OPTIONSTR_POSIX_TIMEZONE;
	break;
    case kDHCPv6OPTION_TZDB_TIMEZONE:
	str = kDHCPv6OPTIONSTR_TZDB_TIMEZONE;
	break;
    case kDHCPv6OPTION_CAPTIVE_PORTAL_URL:
	str = kDHCPv6OPTIONSTR_CAPTIVE_PORTAL_URL;
	break;
    case kDHCPv6OPTION_IA_PD:
	str = "IA_PD";
	break;
    case kDHCPv6OPTION_IAPREFIX:
	str = "IAPREFIX";
	break;
    default:
	str = "<unknown>";
	break;
    }
    return (str);
}

/**
 ** DHCPv6OptionArea
 **/
PRIVATE_EXTERN void
DHCPv6OptionAreaInit(DHCPv6OptionAreaRef oa_p, uint8_t * buf, int size)
{
    oa_p->buf = buf;
    oa_p->size = size;
    oa_p->used = 0;
    return;
}

PRIVATE_EXTERN int
DHCPv6OptionAreaGetUsedLength(DHCPv6OptionAreaRef oa_p)
{
    return (oa_p->used);
}

PRIVATE_EXTERN bool
DHCPv6OptionAreaAddOption(DHCPv6OptionAreaRef oa_p,
			  DHCPv6OptionCode option_code,
			  DHCPv6OptionLength option_len,
			  const void * option_data,
			  DHCPv6OptionErrorString * err_p)

{
    int			left = oa_p->size - oa_p->used;
    DHCPv6OptionRef	opt;
    int			required_space;

    required_space = offsetof(DHCPv6Option, data) + option_len;
    err_p->str[0] = '\0';
    if (left < required_space) {
	if (err_p != NULL) {
	    snprintf(err_p->str, sizeof(err_p->str), 
		     "No room for option %s (%d), %d < %d",
		     DHCPv6OptionCodeGetName(option_code),
		     option_code, left, required_space);
	}
	return (FALSE);
    }
    opt = (DHCPv6OptionRef)(oa_p->buf + oa_p->used);
    DHCPv6OptionSetCode(opt, option_code);
    DHCPv6OptionSetLength(opt, option_len);
    if (option_len > 0) {
	bcopy(option_data, opt->data, option_len);
    }
    oa_p->used += required_space;
    return (TRUE);
}

PRIVATE_EXTERN bool
DHCPv6OptionAreaAddOptionRequestOption(DHCPv6OptionAreaRef oa_p, 
				       const DHCPv6OptionCode * requested_opts,
				       int count,
				       DHCPv6OptionErrorString * err_p)

{
    int			i;
    uint16_t		oro[count];

    /* convert requested options to network byte order */
    for (i = 0; i < count; i++) {
	oro[i] = htons(requested_opts[i]);
    }
    return (DHCPv6OptionAreaAddOption(oa_p, kDHCPv6OPTION_ORO,
				      (int)sizeof(oro), oro, err_p));
}


/**
 ** DHCPv6OptionList
 **/
struct DHCPv6OptionList {
    ptrlist_t		list;
};
typedef struct DHCPv6OptionList DHCPv6OptionList;

STATIC bool
DHCPv6OptionListParse(DHCPv6OptionListRef options,
		      const uint8_t * buf, int buf_size,
		      DHCPv6OptionErrorString * err_p)
{
    int			left;
    const uint8_t *	scan;
    
    ptrlist_init(&options->list);
    for (left = buf_size, scan = buf; TRUE; ) {
	DHCPv6OptionRef		option = (DHCPv6OptionRef)scan;
	DHCPv6OptionCode 	option_code;
	DHCPv6OptionLength 	option_len;
	int			option_need;

	if (left < DHCPV6_OPTION_HEADER_SIZE) {
	    if (left == 0) {
		/* we're done */
		break;
	    }
	    if (err_p != NULL) {
		snprintf(err_p->str, sizeof(err_p->str),
			 "truncated buffer at offset %d\n",
			 (int)(scan - buf));
	    }
	    goto failed;
	}
	option_code = DHCPv6OptionGetCode(option);
	option_len = DHCPv6OptionGetLength(option);
	option_need = DHCPV6_OPTION_HEADER_SIZE + option_len;
	if (left < option_need) {
	    if (err_p != NULL) {
		snprintf(err_p->str, sizeof(err_p->str), 
			 "truncated option %s (%d) at offset %d,"
			 " left %d < need %d",
			 DHCPv6OptionCodeGetName(option_code), option_code,
			 (int)(scan - buf), left, option_need);
	    }
	    goto failed;
	}
	ptrlist_add(&options->list, (void *)scan);
	scan += option_need;
	left -= option_need;
    }
    return (TRUE);
    
 failed:
    ptrlist_free(&options->list);
    return (NULL);
}

PRIVATE_EXTERN DHCPv6OptionListRef
DHCPv6OptionListCreate(const uint8_t * buf, int buf_size,
		       DHCPv6OptionErrorString * err_p)
{
    DHCPv6OptionList	options;
    DHCPv6OptionListRef	ret;

    if (DHCPv6OptionListParse(&options, buf, buf_size, err_p) == FALSE) {
	return (NULL);
    }
    ret = (DHCPv6OptionListRef)malloc(sizeof(*ret));
    *ret = options;
    return (ret);
}

PRIVATE_EXTERN DHCPv6OptionListRef
DHCPv6OptionListCreateWithPacket(const DHCPv6PacketRef pkt, int pkt_len,
				 DHCPv6OptionErrorString * err_p)
{
    int		options_length;

    if (pkt_len < DHCPV6_PACKET_HEADER_LENGTH) {
	return (NULL);
    }
    options_length = pkt_len - DHCPV6_PACKET_HEADER_LENGTH;
    return (DHCPv6OptionListCreate(pkt->options, options_length, err_p));
}

PRIVATE_EXTERN void
DHCPv6OptionListRelease(DHCPv6OptionListRef * options_p)
{
    DHCPv6OptionListRef		options = *options_p;

    if (options == NULL) {
	return;
    }
    *options_p = NULL;
    ptrlist_free(&options->list);
    free(options);
    return;
}

STATIC void
DHCPv6OptionPrintToString(CFMutableStringRef str,
			  DHCPv6OptionCode option_code,
			  DHCPv6OptionLength option_length,
			  const uint8_t * option_data,
			  int level)
{
    DHCPDUIDRef		duid;
    DHCPv6OptionType	type;

    STRING_APPEND(str, "%s (%d) Length %d: ",
		  DHCPv6OptionCodeGetName(option_code),
		  option_code, option_length);
    type = DHCPv6OptionCodeGetType(option_code);
    switch (type) {
    case kDHCPv6OptionTypeNone:
	break;
    case kDHCPv6OptionTypeDUID:
	duid = (DHCPDUIDRef)option_data;
	DHCPDUIDPrintToString(str, duid, option_length);
	STRING_APPEND(str, "\n");
	break;
    case kDHCPv6OptionTypeUInt16: {
	int		j;
	void *	scan;	
	
	scan = (void *)option_data;
	for (j = 0; j < option_length / sizeof(uint16_t); 
	     j++, scan += sizeof(uint16_t)) {
	    uint16_t	val;
	    
	    val = net_uint16_get(scan);
	    if (option_code == kDHCPv6OPTION_ORO) {
		STRING_APPEND(str, "%s%s (%d)", (j == 0) ? "" : ", ", 
			      DHCPv6OptionCodeGetName(val), val);
	    }
	    else {
		STRING_APPEND(str, "%s%d", (j == 0) ? "" : ", ", val);
	    }
	}
	STRING_APPEND(str, "\n");
	break;
    }
    case kDHCPv6OptionTypeUInt32: {
	int		j;
	void *	scan;
	
	scan = (void *)option_data;
	for (j = 0; j < option_length / sizeof(uint32_t); 
	     j++, scan += sizeof(uint32_t)) {
	    STRING_APPEND(str, "%s%d", (j == 0) ? "" : ", ",
			  net_uint32_get(scan));
	}
	STRING_APPEND(str, "\n");
	break;
    }
    case kDHCPv6OptionTypeIPv6Address: {
	int				j;
	void *			scan;
	char 			ntopbuf[INET6_ADDRSTRLEN];

	scan = (void *)option_data;
	for (j = 0; j < option_length / sizeof(struct in6_addr); 
	     j++, scan += sizeof(struct in6_addr)) {
	    STRING_APPEND(str, "%s%s",
			  (j == 0) ? "" : ", ",
			  inet_ntop(AF_INET6, scan, ntopbuf, sizeof(ntopbuf)));
	}
	STRING_APPEND(str, "\n");
	break;
    }
    case kDHCPv6OptionTypeDNSNameList: {
	int			j;	
	const char * *	list;
	int			list_count;

	list = DNSNameListCreate(option_data, option_length, &list_count);
	if (list == NULL) {
	    STRING_APPEND(str, " Invalid");
	    goto print_option_data;
	}
	for (j = 0; j < list_count; j++) {
	    STRING_APPEND(str, "%s%s", (j == 0) ? "" : ", ", list[j]);
	}
	free(list);
	STRING_APPEND(str, "\n");
	break;
    }
    case kDHCPv6OptionTypeString: {
	CFStringRef string = CFStringCreateWithBytes(kCFAllocatorDefault, option_data, (CFIndex)option_length, kCFStringEncodingUTF8, FALSE);
	STRING_APPEND(str, " %@\n", string);
	my_CFRelease(&string);
	break;
    }
    case kDHCPv6OptionTypeIA_NA:
	DHCPv6OptionIA_NAPrintLevelToString(str,
					    (DHCPv6OptionIA_NARef)
					    option_data, 
					    option_length, level);
	break;

    case kDHCPv6OptionTypeIAADDR:
	DHCPv6OptionIAADDRPrintLevelToString(str,
					     (DHCPv6OptionIAADDRRef)
					     option_data,
					     option_length, level);
	break;

    case kDHCPv6OptionTypeIA_PD:
	DHCPv6OptionIA_PDPrintLevelToString(str,
					    (DHCPv6OptionIA_PDRef)
					    option_data,
					    option_length, level);
	break;

    case kDHCPv6OptionTypeIAPREFIX:
	DHCPv6OptionIAPREFIXPrintLevelToString(str,
					       (DHCPv6OptionIAPREFIXRef)
					       option_data,
					       option_length, level);
	break;

    case kDHCPv6OptionTypeStatusCode:
	DHCPv6OptionSTATUS_CODEPrintToString(str,
					     (DHCPv6OptionSTATUS_CODERef)
					     option_data, option_length);
	break;

    print_option_data:
    default:
    case kDHCPv6OptionTypeUnknown:
	if (option_length != 0) {
	    STRING_APPEND(str, " Data ");
	    print_bytes_cfstr(str,
			      (void *)option_data,
			      option_length);
	}
	STRING_APPEND(str, "\n");
	break;
    }
    return;
}

STATIC void
DHCPv6OptionListPrintLevelToString(CFMutableStringRef str,
				   DHCPv6OptionListRef options,
				   int level)
{
    int 	count = DHCPv6OptionListGetCount(options);
    int		i;
    int		lev;

    STRING_APPEND(str, "Options[%d] = {\n", count);
    for (i = 0; i < count; i++) {
	DHCPv6OptionRef		option;
	DHCPv6OptionCode	option_code;
	DHCPv6OptionLength	option_length;
	const uint8_t *		option_data;

	option = DHCPv6OptionListGetOptionAtIndex(options, i);	
	option_code = DHCPv6OptionGetCode(option);
	option_length = DHCPv6OptionGetLength(option);
	option_data = DHCPv6OptionGetData(option);
	for (lev = 0; lev < level; lev++) {
	    STRING_APPEND(str, "  ");
	}
	STRING_APPEND(str, "  ");
	DHCPv6OptionPrintToString(str, option_code, option_length, option_data,
				  level);
    }
    for (lev = 0; lev < level; lev++) {
	STRING_APPEND(str, "  ");
    }
    STRING_APPEND(str, "}\n");
    return;
}

PRIVATE_EXTERN void
DHCPv6OptionListPrintToString(CFMutableStringRef str,
			      DHCPv6OptionListRef options)
{
    DHCPv6OptionListPrintLevelToString(str, options, 0);
    return;
}

void
DHCPv6OptionListFPrint(FILE * file, DHCPv6OptionListRef options)
{
    CFMutableStringRef	str;

    str = CFStringCreateMutable(NULL, 0);
    DHCPv6OptionListPrintLevelToString(str, options, 0);
    SCPrint(TRUE, file, CFSTR("%@\n"), str);
    CFRelease(str);
    return;
}

STATIC void
DHCPv6OptionsBufferPrintLevelToString(CFMutableStringRef str,
				      const uint8_t * buf, int buf_length,
				      int level)
{
    DHCPv6OptionErrorString 	err;
    DHCPv6OptionListRef		options;

    options = DHCPv6OptionListCreate(buf, buf_length, &err);
    if (options == NULL) {
	STRING_APPEND(str, " options invalid:\n\t%s\n",
		      err.str);
    }
    else {
	STRING_APPEND(str, " ");
	/* possibly recurse */
	DHCPv6OptionListPrintLevelToString(str, options, level + 1);
    }
    DHCPv6OptionListRelease(&options);
}

PRIVATE_EXTERN int
DHCPv6OptionListGetCount(DHCPv6OptionListRef options)
{
    return (ptrlist_count(&options->list));
}

PRIVATE_EXTERN DHCPv6OptionRef
DHCPv6OptionListGetOptionAtIndex(DHCPv6OptionListRef options, int i)
{
    return ((DHCPv6OptionRef)ptrlist_element(&options->list, i));
}

PRIVATE_EXTERN const uint8_t *
DHCPv6OptionListGetOptionDataAndLength(DHCPv6OptionListRef options,
				       DHCPv6OptionCode option_code,
				       int * ret_length,
				       int * start_index)
{
    int		count = DHCPv6OptionListGetCount(options);
    int		i = 0;

    if (start_index != NULL) {
	i = *start_index;
    }
    for (; i < count; i++) {
	DHCPv6OptionRef		option;

	option = DHCPv6OptionListGetOptionAtIndex(options, i);
	if (DHCPv6OptionGetCode(option) == option_code) {
	    if (start_index != NULL) {
		*start_index = i + 1;
	    }
	    *ret_length = DHCPv6OptionGetLength(option);
	    return (DHCPv6OptionGetData(option));
	}
    }
    return (NULL);
}


/**
 ** DHCPv6OptionsDictionary
 **/
STATIC CFNumberRef
DHCPv6OptionCodeCreateCFNumber(DHCPv6OptionCode code)
{
    return (CFNumberCreate(NULL, kCFNumberSInt16Type, &code));
}

STATIC CFDataRef
DHCPv6OptionDataCreate(DHCPv6OptionCode option_code, CFTypeRef value)
{
    CFDataRef		data = NULL;
    DHCPv6OptionType	option_type;

    option_type = DHCPv6OptionCodeGetType(option_code);
    if (isA_CFData(value) != NULL) {
	data = CFRetain(value);
    }
    else if (isA_CFString(value) != NULL) {
	CFStringRef	str = (CFStringRef)value;

	switch (option_type) {
	case kDHCPv6OptionTypeIPv6Address: {
	    struct in6_addr	ip6addr;

	    if (!my_CFStringToIPv6Address(str, &ip6addr)) {
		my_log(LOG_NOTICE, "invalid IPv6 address '%@'", str);
		break;
	    }
	    data = CFDataCreate(NULL, (UInt8 *)&ip6addr, sizeof(ip6addr));
	    break;
	}
	case kDHCPv6OptionTypeDNSNameList:
	    data = DNSNameListDataCreateWithString(str);
	    break;
	case kDHCPv6OptionTypeString:
	default:
	    data = my_CFStringCreateData(str);
	    break;
	}
    }
    else if (isA_CFArray(value) != NULL) {
	CFArrayRef	list = (CFArrayRef)value;

	switch (option_type) {
	case kDHCPv6OptionTypeIPv6Address: {
	    struct in6_addr *	ip6addrs;
	    int			ip6addrs_count;

	    ip6addrs = my_CFArrayToIPv6Addresses(list, &ip6addrs_count);
	    if (ip6addrs == NULL) {
		my_log(LOG_NOTICE, "invalid IPv6 address '%@'", list);
		break;
	    }
	    data = CFDataCreate(NULL, (UInt8 *)ip6addrs,
				ip6addrs_count * sizeof(*ip6addrs));
	    free(ip6addrs);
	    break;
	}
	case kDHCPv6OptionTypeDNSNameList:
	    data = DNSNameListDataCreateWithArray(list, FALSE);
	    break;
	default:
	    break;
	}
    }
    return (data);
}

PRIVATE_EXTERN CFDataRef
DHCPv6OptionsDictionaryGetOption(CFDictionaryRef dict, DHCPv6OptionCode code)
{
    CFDataRef		data = NULL;
    CFNumberRef		num;

    num = DHCPv6OptionCodeCreateCFNumber(code);
    if (num != NULL) {
	data = CFDictionaryGetValue(dict, num);
	CFRelease(num);
    }
    return (data);
}

PRIVATE_EXTERN CFDictionaryRef
DHCPv6OptionsDictionaryCreate(CFDictionaryRef dict)
{
    CFIndex			count;
    const void * * 		keys;
    CFMutableDictionaryRef	options;
    const void * *		values;

    count = CFDictionaryGetCount(dict);
    if (count == 0) {
	return (NULL);
    }
    options = CFDictionaryCreateMutable(NULL, count,
					&kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks);
    keys = malloc(count * sizeof(*keys) * 2);
    values = keys + count;
    CFDictionaryGetKeysAndValues(dict, keys, values);
    for (CFIndex i = 0; i < count; i++) {
	CFDataRef		data;
	char *			option_name;
	DHCPv6OptionCode	option_code;
	CFRange			range;
	CFStringRef		this_key = keys[i];
	CFTypeRef		this_value = values[i];

#define OPTION_PREFIX		"dhcp_"
#define OPTION_PREFIX_LENGTH	(sizeof(OPTION_PREFIX) - 1)
	if (CFStringHasPrefix(this_key, CFSTR(OPTION_PREFIX)) == FALSE) {
	    /* not a DHCP option */
	    continue;
	}
	range = CFRangeMake(OPTION_PREFIX_LENGTH,
			    CFStringGetLength(this_key) - OPTION_PREFIX_LENGTH);
	option_name = my_CFStringToCStringWithRange(this_key, range,
						    kCFStringEncodingUTF8);
	if (option_name == NULL) {
	    continue;
	}
	option_code = DHCPv6OptionNameGetCode(option_name);
	if (option_code == kDHCPv6OPTION_NONE && isdigit(option_name[0])) {
	    option_code = strtoul(option_name, NULL, 0);
	}
	if (option_code == kDHCPv6OPTION_NONE) {
	    /* we don't understand this option */
	    my_log(LOG_NOTICE, "Ignoring unsupported option '%@'",
		   this_key);
	    goto loop_done;
	}
	data = DHCPv6OptionDataCreate(option_code, this_value);
	if (data == NULL) {
	    my_log(LOG_NOTICE, "Failed to handle '%@'",
		   this_key);
	}
	else {
	    CFNumberRef		num;

	    num = DHCPv6OptionCodeCreateCFNumber(option_code);
	    CFDictionarySetValue(options, num, data);
	    CFRelease(num);
	    CFRelease(data);
	}

    loop_done:
	free(option_name);
    }
    free(keys);
    if (CFDictionaryGetCount(options) == 0) {
	my_CFRelease(&options);
    }
    return (options);
}

PRIVATE_EXTERN void
DHCPv6OptionsDictionaryPrintToString(CFMutableStringRef str,
				     CFDictionaryRef dict)
{
    CFIndex			count;
    const void * * 		keys;
    const void * *		values;

    count = CFDictionaryGetCount(dict);
    if (count == 0) {
	return;
    }
    keys = malloc(count * sizeof(*keys) * 2);
    values = keys + count;
    CFDictionaryGetKeysAndValues(dict, keys, values);
    for (CFIndex i = 0; i < count; i++) {
	CFNumberRef		key = keys[i];
	DHCPv6OptionCode 	option_code;
	DHCPv6OptionLength 	option_length;
	const uint8_t *		option_data;
	CFDataRef		value = values[i];

	if (isA_CFNumber(key) == NULL) {
	    STRING_APPEND(str, "[%d] key %@ not a number\n",
			  (int)i, key);
	    continue;
	}
	CFNumberGetValue(key, kCFNumberSInt16Type, &option_code);
	option_length = CFDataGetLength(value);
	option_data = CFDataGetBytePtr(value);
	DHCPv6OptionPrintToString(str, option_code, option_length, option_data,
				  0);
    }
    free(keys);
    return;
}

/**
 ** DHCPv6OptionIA_NA
 **/
STATIC void
DHCPv6OptionIA_NAPrintLevelToString(CFMutableStringRef str,
				    const DHCPv6OptionIA_NARef ia_na,
				    int ia_na_len, int level)
{
    int		options_length;

    if (ia_na_len < DHCPv6OptionIA_NA_MIN_LENGTH) {
	STRING_APPEND(str, " IA_NA option is too short %d < %d\n", 
		      ia_na_len, DHCPv6OptionIA_NA_MIN_LENGTH);
	return;
    }
    options_length = ia_na_len - DHCPv6OptionIA_NA_MIN_LENGTH;
    STRING_APPEND(str, " IA_NA IAID=%d T1=%d T2=%d",
		  DHCPv6OptionIA_NAGetIAID(ia_na),
		  DHCPv6OptionIA_NAGetT1(ia_na),
		  DHCPv6OptionIA_NAGetT2(ia_na));

    if (options_length == 0) {
	STRING_APPEND(str, "\n");
    }
    else {
	DHCPv6OptionErrorString 	err;
	DHCPv6OptionListRef		options;

	options = DHCPv6OptionListCreate(ia_na->options, options_length, &err);
	if (options == NULL) {
	    STRING_APPEND(str, " options invalid:\n\t%s\n",
			  err.str);
	}
	else {
	    STRING_APPEND(str, " ");
	    /* possibly recurse */
	    DHCPv6OptionListPrintLevelToString(str, options, level + 1);
	}
	DHCPv6OptionListRelease(&options);
    }
    return;
}

/**
 ** DHCPv6OptionIAADDR
 **/
STATIC void
DHCPv6OptionIAADDRPrintLevelToString(CFMutableStringRef str,
				     DHCPv6OptionIAADDRRef ia_addr,
				     int ia_addr_len, int level)
{
    char 	ntopbuf[INET6_ADDRSTRLEN];
    int		options_length;


    if (ia_addr_len < DHCPv6OptionIAADDR_MIN_LENGTH) {
	STRING_APPEND(str, " IAADDR option is too short %d < %d\n", 
		      ia_addr_len, DHCPv6OptionIAADDR_MIN_LENGTH);
	return;
    }
    options_length = ia_addr_len - DHCPv6OptionIAADDR_MIN_LENGTH;
    STRING_APPEND(str, " IAADDR %s Preferred %d Valid=%d",
		  inet_ntop(AF_INET6,
			    DHCPv6OptionIAADDRGetAddress(ia_addr),
			    ntopbuf, sizeof(ntopbuf)),
		  DHCPv6OptionIAADDRGetPreferredLifetime(ia_addr),
		  DHCPv6OptionIAADDRGetValidLifetime(ia_addr));
    if (options_length == 0) {
	STRING_APPEND(str, "\n");
    }
    else {
	DHCPv6OptionsBufferPrintLevelToString(str,
					      ia_addr->options, options_length,
					      level);
    }
    return;
}

PRIVATE_EXTERN void
DHCPv6OptionIAADDRPrintToString(CFMutableStringRef str,
				DHCPv6OptionIAADDRRef ia_addr,
				int ia_addr_len)
{
    DHCPv6OptionIAADDRPrintLevelToString(str, ia_addr, ia_addr_len, 0);
    return;
}

/**
 ** DHCPv6OptionIA_PD
 **/
STATIC void
DHCPv6OptionIA_PDPrintLevelToString(CFMutableStringRef str,
				    const DHCPv6OptionIA_PDRef ia_pd,
				    int ia_pd_len, int level)
{
    int		options_length;

    if (ia_pd_len < DHCPv6OptionIA_PD_MIN_LENGTH) {
	STRING_APPEND(str, " IA_PD option is too short %d < %d\n",
		      ia_pd_len, DHCPv6OptionIA_PD_MIN_LENGTH);
	return;
    }
    options_length = ia_pd_len - DHCPv6OptionIA_PD_MIN_LENGTH;
    STRING_APPEND(str, " IA_PD IAID=%d T1=%d T2=%d",
		  DHCPv6OptionIA_PDGetIAID(ia_pd),
		  DHCPv6OptionIA_PDGetT1(ia_pd),
		  DHCPv6OptionIA_PDGetT2(ia_pd));

    if (options_length == 0) {
	STRING_APPEND(str, "\n");
    }
    else {
	DHCPv6OptionErrorString 	err;
	DHCPv6OptionListRef		options;

	options = DHCPv6OptionListCreate(ia_pd->options, options_length, &err);
	if (options == NULL) {
	    STRING_APPEND(str, " options invalid:\n\t%s\n",
			  err.str);
	}
	else {
	    STRING_APPEND(str, " ");
	    /* possibly recurse */
	    DHCPv6OptionListPrintLevelToString(str, options, level + 1);
	}
	DHCPv6OptionListRelease(&options);
    }
    return;
}

/**
 ** DHCPv6OptionIAPREFIX
 **/

STATIC void
DHCPv6OptionIAPREFIXPrintLevelToString(CFMutableStringRef str,
				       DHCPv6OptionIAPREFIXRef ia_prefix,
				       int ia_prefix_len, int level)
{
    char 	ntopbuf[INET6_ADDRSTRLEN];
    int		options_length;


    if (ia_prefix_len < DHCPv6OptionIAPREFIX_MIN_LENGTH) {
	STRING_APPEND(str, " IAPREFIX option is too short %d < %d\n",
		      ia_prefix_len, DHCPv6OptionIAPREFIX_MIN_LENGTH);
	return;
    }
    options_length = ia_prefix_len - DHCPv6OptionIAPREFIX_MIN_LENGTH;
    STRING_APPEND(str, " IAPREFIX %s/%d Preferred %d Valid=%d",
		  inet_ntop(AF_INET6,
			    DHCPv6OptionIAPREFIXGetPrefix(ia_prefix),
			    ntopbuf, sizeof(ntopbuf)),
		  DHCPv6OptionIAPREFIXGetPrefixLength(ia_prefix),
		  DHCPv6OptionIAPREFIXGetPreferredLifetime(ia_prefix),
		  DHCPv6OptionIAPREFIXGetValidLifetime(ia_prefix));
    if (options_length == 0) {
	STRING_APPEND(str, "\n");
    }
    else {
	DHCPv6OptionsBufferPrintLevelToString(str,
					      ia_prefix->options,
					      options_length,
					      level);
    }
    return;
}

PRIVATE_EXTERN void
DHCPv6OptionIAPREFIXPrintToString(CFMutableStringRef str,
				  DHCPv6OptionIAPREFIXRef ia_prefix,
				  int ia_prefix_len)
{
    DHCPv6OptionIAPREFIXPrintLevelToString(str, ia_prefix, ia_prefix_len, 0);
    return;
}

PRIVATE_EXTERN void
DHCPv6OptionIAADDRFPrint(FILE * file, DHCPv6OptionIAADDRRef ia_addr,
			 int ia_addr_len)
{
    CFMutableStringRef	str;

    str = CFStringCreateMutable(NULL, 0);
    DHCPv6OptionIAADDRPrintToString(str, ia_addr, ia_addr_len);
    SCPrint(TRUE, file, CFSTR("%@"), str);
    CFRelease(str);
    return;
}

/**
 ** DHCPv6StatusCode
 **/
PRIVATE_EXTERN const char * 
DHCPv6StatusCodeGetName(DHCPv6StatusCode code)
{
    const char *	str;

    switch (code) {
    case kDHCPv6StatusCodeSuccess:
	str = "Success";
	break;
    case kDHCPv6StatusCodeFailure:
	str = "Failure";
	break;
    case kDHCPv6StatusCodeNoAddrsAvail:
	str = "NoAddrsAvail";
	break;
    case kDHCPv6StatusCodeNoBinding:
	str = "NoBinding";
	break;
    case kDHCPv6StatusCodeNotOnLink:
	str = "NotOnLink";
	break;
    case kDHCPv6StatusCodeUseMulticast:
	str = "UseMulticast";
	break;
    case kDHCPv6StatusCodeNoPrefixAvail:
	str = "NoPrefixAvail";
	break;
    case kDHCPv6StatusCodeUnknownQueryType:
	str = "UnknownQueryType";
	break;
    case kDHCPv6StatusCodeMalformedQuery:
	str = "MalformedQuery";
	break;
    case kDHCPv6StatusCodeNotConfigured:
	str = "NotConfigured";
	break;
    case kDHCPv6StatusCodeNotAllowed:
	str = "NotAllowed";
	break;
    case kDHCPv6StatusCodeQueryTerminated:
	str = "QueryTerminated";
	break;
    case kDHCPv6StatusCodeDataMissing:
	str = "DataMissing";
	break;
    case kDHCPv6StatusCodeCatchUpComplete:
	str = "CatchUpComplete";
	break;
    case kDHCPv6StatusCodeNotSupported:
	str = "NotSupported";
	break;
    case kDHCPv6StatusCodeTLSConnectionRefused:
	str = "TLSConnectionRefused";
	break;
    case kDHCPv6StatusCodeAddressInUse:
	str = "AddressInUse";
	break;
    case kDHCPv6StatusCodeConfigurationConflict:
	str = "ConfigurationConflict";
	break;
    case kDHCPv6StatusCodeMissingBindingInformation:
	str = "MissingBindingInformation";
	break;
    case kDHCPv6StatusCodeOutdatedBindingInformation:
	str = "OutdatedBindingInformation";
	break;
    case kDHCPv6StatusCodeServerShuttingDown:
	str = "ServerShuttingDown";
	break;
    case kDHCPv6StatusCodeDNSUpdateNotSupported:
	str = "DNSUpdateNotSupported";
	break;
    case kDHCPv6StatusCodeExcessiveTimeSkew:
	str = "ExcessiveTimeSkew";
	break;
    default:
	str = "<unknown>";
	break;
    }
    return (str);
}

STATIC void
DHCPv6OptionSTATUS_CODEPrintToString(CFMutableStringRef str,
				     DHCPv6OptionSTATUS_CODERef status_p,
				     int status_len)
{
    uint16_t		code;
    int			message_len;

    if (status_len < DHCPv6OptionSTATUS_CODE_MIN_LENGTH) {
	STRING_APPEND(str, " STATUS_CODE option is too short %d < %d\n", 
		      status_len, DHCPv6OptionSTATUS_CODE_MIN_LENGTH);
	return;
    }
    code = DHCPv6OptionSTATUS_CODEGetCode(status_p);
    message_len = status_len - DHCPv6OptionSTATUS_CODE_MIN_LENGTH;
    if (message_len) {
	STRING_APPEND(str, " STATUS_CODE %s (%d) '%.*s'\n",
		      DHCPv6StatusCodeGetName(code), code,
		      message_len, status_p->message);
    }
    else {
	STRING_APPEND(str, " STATUS_CODE %s (%d)\n",
		      DHCPv6StatusCodeGetName(code), code);
    }
    return;
}

PRIVATE_EXTERN bool
DHCPv6OptionListGetStatusCode(DHCPv6OptionListRef options,
			      DHCPv6StatusCode * ret_code,
			      const char * * ret_msg,
			      unsigned int * ret_msg_length)
{
    DHCPv6StatusCode		code = kDHCPv6StatusCodeSuccess;
    int				option_len;
    const char *		msg = NULL;
    unsigned int		msg_length = 0;
    DHCPv6OptionSTATUS_CODERef	status_code;
    bool			success = true;

    status_code = (DHCPv6OptionSTATUS_CODERef)
	DHCPv6OptionListGetOptionDataAndLength(options,
					       kDHCPv6OPTION_STATUS_CODE,
					       &option_len, NULL);
    if (status_code != NULL) {
	if (option_len >= DHCPv6OptionSTATUS_CODE_MIN_LENGTH) {
	    code = DHCPv6OptionSTATUS_CODEGetCode(status_code);
	    msg = (const char *)
		DHCPv6OptionSTATUS_CODEGetMessage(status_code);
	    msg_length = option_len - DHCPv6OptionSTATUS_CODE_MIN_LENGTH;
	}
	else {
	    my_log(LOG_NOTICE,
		   "DHCPv6 StatusCode option too short %d < %d",
		   option_len, DHCPv6OptionSTATUS_CODE_MIN_LENGTH);
	    code = kDHCPv6StatusCodeFailure;
	    success = false;
	}
    }
    *ret_code = code;
    if (ret_msg_length != NULL) {
	*ret_msg_length = msg_length;
    }
    if (ret_msg != NULL) {
	*ret_msg = msg;
    }
    return (success);
}

#if TEST_DHCPV6_OPTIONS

#define IA_ADDR_1 \
	0x00, 0x05, 0x00, 0x18,						\
	0xfc, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,			\
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,			\
	0x00, 0x00, 0x00, 0x8e,						\
	0x00, 0x00, 0x01, 0xff

#define IA_NA_1	0x00, 0x00, 0x00, 0x01,					\
	0xff, 0xff, 0xff, 0xff,						\
	0xff, 0xff, 0xff, 0xff,						\
	IA_ADDR_1

/* type = 1, hw = 1, time = aabbccdd, linklayer_address = 0x000102030405 */
#define DUID_LLT_1	0x00, 0x01, 0x00, 0x01, 0xaa, 0xbb, 0xcc, 0xdd, \
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05

#define DUID_EN_1	0x00, 0x02, 0x01, 0x02, 0x03, 0x04, 0xde, 0xad, \
	0xbe, 0xef

#define DUID_LL_1	0x00, 0x03, 0x00, 0x01, \
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05

#define DUID_LLT_UNDEF6_1 0x00, 0x06, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, \
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05

#define DUID_UUID_1	0x00, 0x04,		\
	0x00, 0x01, 0x02, 0x03,			\
	0x04, 0x05, 0x06, 0x07,			\
	0x08, 0x09, 0x0a, 0x0b,			\
	0x0c, 0x0d, 0x0e, 0x0f

#define ORO_1 0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04

#define DNS_SERVER_1 0x20, 0x02, 0x45, 0xb5, 0xea, 0x61, 0x00, 0x00, 0x2, 0x1f, 0xf3, 0xff, 0xfe, 0x43, 0x1a, 0xbf

#define DNS_NAME_SEARCH_1						\
    4, 'e', 'u', 'r', 'o', 5, 'a', 'p', 'p', 'l', 'e', 3, 'c', 'o', 'm', 0, \
	9, 'm', 'a', 'r', 'k', 'e', 't', 'i', 'n', 'g', 5, 'a', 'p', 'p', 'l', 'e', 3, 'c', 'o', 'm', 0, \
	11, 'e', 'n', 'g', 'i', 'n', 'e', 'e', 'r', 'i', 'n', 'g', 5, 'a', 'p', 'p', 'l', 'e', 3, 'c', 'o', 'm', 0



/* test_buf1 parses OK */
const uint8_t test_buf1[] = {
    0x00, 0x03, 0x00, 0x28, IA_NA_1 ,
    0x00, 0x01, 0x00, 0x0e, DUID_LLT_1 ,
    0x00, 0x01, 0x00, 0xa, DUID_EN_1 ,
    0x00, 0x02, 0x00, 0xa, DUID_LL_1 ,
    0x00, 0x06, 0x00, 0x08, ORO_1 ,
    0x00, 0x18, 0x00, 0x3c, DNS_NAME_SEARCH_1 ,
    0x00, 0x18, 0x00, 0x03, 0x1, 'a', 0,
    0x00, 0x02, 0x00, 0x0e, DUID_LLT_UNDEF6_1 ,
    0x00, 0x17, 0x00, 0x10, DNS_SERVER_1 ,
};

/* test_buf2 parses at a high level OK, but IA_NA is too short. */
const uint8_t test_buf2[] = {
    0x00, 0x03, 0x00, 0x02, 0x01, 0x02, /* truncated IA_NA(3) */
    0x00, 0x02, 0x00, 0x12, DUID_UUID_1 ,
};

/* test_bad_buf1 does not parse OK */
const uint8_t test_bad_buf1[] = {
    0x00, 0x01, 0x01, 0x00, 0x01, 0x02,
};

struct testbuf {
    const uint8_t *	buf;
    int			size;
    bool		good;
};
struct testbuf tests[] = {
    { test_buf1, sizeof(test_buf1), TRUE },
    { test_buf2, sizeof(test_buf2), TRUE },
    { test_bad_buf1, sizeof(test_bad_buf1), FALSE },
    { NULL, 0 }
};

STATIC bool
run_tests(struct testbuf * list, bool verbose)
{
    int				i;
    struct testbuf *		scan;
    
    for (i = 0, scan = list; scan->buf != NULL; scan++, i++) {
	DHCPv6OptionErrorString 	err;
	DHCPv6OptionListRef		options;

	options = DHCPv6OptionListCreate(scan->buf, scan->size, &err);
	if (options != NULL) {
	    if (scan->good == FALSE) {
		fprintf(stderr, "test %d succeeded unexpectedly\n", i);
		return (FALSE);
	    }
	    printf("test %d SUCCESS\n", i);
	    if (verbose) {
		DHCPv6OptionListFPrint(stdout, options);
	    }
	    DHCPv6OptionListRelease(&options);
	}
	else {
	    if (scan->good == TRUE) {
		printf("test %d FAILURE: %s\n", i,
		       err.str);
		return (FALSE);
	    }
	    printf("test %d SUCCESS\n", i);
	    if (verbose) {
		fprintf(stderr, "\tParse failed, %s\n", err.str);
	    }

	}
    }
    return (TRUE);
}

STATIC bool
run_config_tests(bool verbose)
{
    CFIndex 		count;
    CFDictionaryRef	dict;
    CFArrayRef		tests;

#define OPTIONS_TEST_DATA	"DHCPv6OptionsTestData.plist"
    dict = my_CFPropertyListCreateFromFile(OPTIONS_TEST_DATA);
    if (dict == NULL) {
	fprintf(stderr, "Can't load '%s'\n",
		OPTIONS_TEST_DATA);
	return (FALSE);
    }
    if (isA_CFDictionary(dict) == NULL) {
	fprintf(stderr, "Not a dictionary '%s'\n",
		OPTIONS_TEST_DATA);
	return (FALSE);
    }
#define ktests 		"tests"
    tests = CFDictionaryGetValue(dict, CFSTR(ktests));
    if (isA_CFArray(tests) == NULL) {
	fprintf(stderr, "Missing/invalid '%s' array\n",
		ktests);
	return (FALSE);
    }
    count = CFArrayGetCount(tests);
    for (CFIndex i = 0; i < count; i++) {
	CFDictionaryRef		config;
	Boolean			expect_success = TRUE;
	CFBooleanRef		expect_success_cf;
	CFDictionaryRef		options;
	Boolean			test_success = TRUE;

	config = CFArrayGetValueAtIndex(tests, i);
	if (isA_CFDictionary(config) == NULL) {
	    fprintf(stderr, "'%s' array contains non dictionary\n",
		    ktests);
	    return (FALSE);
	}
	expect_success_cf
	    = CFDictionaryGetValue(config, CFSTR("expect_success"));
	if (isA_CFBoolean(expect_success_cf)) {
	    expect_success = CFBooleanGetValue(expect_success_cf);
	}
	options = DHCPv6OptionsDictionaryCreate(config);
	if (options == NULL) {
	    if (expect_success) {
		test_success = FALSE;
	    }
	}
	else {
	    if (!expect_success) {
		test_success = FALSE;
	    }
	}
	printf("Config test %d [%s]\n", (int)i,
	       test_success ? "SUCCESS" : "FAILURE");
	if (verbose) {
	    if (options != NULL) {
		CFMutableStringRef	str;

		str = CFStringCreateMutable(NULL, 0);
		DHCPv6OptionsDictionaryPrintToString(str, options);
		SCPrint(TRUE, stdout,
			CFSTR("%@\n"), str);
		my_CFRelease(&str);
	    }
	    else {
		SCPrint(TRUE, stdout,
			CFSTR("can't parse config %@\n"), config);

	    }
	}

	my_CFRelease(&options);
    }
    return (TRUE);
}

int
main(int argc, char * argv[])
{
    if (run_tests(tests, argc > 1) == FALSE) {
	fprintf(stderr, "TEST FAILED\n");
	exit(1);
    }
    run_config_tests(argc > 1);
    exit(0);
    return (0);
}
#endif /* TEST_DHCPV6_OPTIONS */
