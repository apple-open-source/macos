/*
 * Copyright (c) 2009-2011 Apple Inc. All rights reserved.
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

PRIVATE_EXTERN DHCPv6OptionType
DHCPv6OptionCodeGetType(int option_code)
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
    case kDHCPv6OPTION_DNS_SERVERS:
	type = kDHCPv6OptionTypeIPv6Address;
	break;
    case kDHCPv6OPTION_DOMAIN_LIST:
	type = kDHCPv6OptionTypeDNSNameList;
	break;
    case kDHCPv6OPTION_IA_NA:
	type = kDHCPv6OptionTypeIA_NA;
	break;
    case kDHCPv6OPTION_IAADDR:
	type = kDHCPv6OptionTypeIAADDR;
	break;
    case kDHCPv6OPTION_STATUS_CODE:
	type = kDHCPv6OptionTypeStatusCode;
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

PRIVATE_EXTERN const char * 
DHCPv6OptionCodeGetName(int code)
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
    case kDHCPv6OPTION_DNS_SERVERS:
	str = "DNS_SERVERS";
	break;
    case kDHCPv6OPTION_DOMAIN_LIST:
	str = "DOMAIN_LIST";
	break;
    default:
	str = "<unknown>";
	break;
    }
    return (str);
}

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
DHCPv6OptionAreaAddOption(DHCPv6OptionAreaRef oa_p, int option_code, 
			  int option_len, const void * option_data,
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
				       const uint16_t * requested_options, 
				       int count,
				       DHCPv6OptionErrorString * err_p)

{
    int			i;
    uint16_t		oro[count];

    /* convert requested options to network byte order */
    for (i = 0; i < count; i++) {
	oro[i] = htons(requested_options[i]);
    }
    return (DHCPv6OptionAreaAddOption(oa_p, kDHCPv6OPTION_ORO,
				      sizeof(oro), oro, err_p));
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
	DHCPv6OptionRef	option = (DHCPv6OptionRef)scan;
	int		option_code;
	int		option_len;
	int		option_need;

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
    ptrlist_dup(&ret->list, &options.list);
    ptrlist_free(&options.list);
    return (ret);
}

PRIVATE_EXTERN DHCPv6OptionListRef
DHCPv6OptionListCreateWithPacket(const DHCPv6PacketRef pkt, int pkt_len,
				 DHCPv6OptionErrorString * err_p)
{
    int		option_len;

    if (pkt_len < DHCPV6_PACKET_HEADER_LENGTH) {
	return (NULL);
    }
    option_len = pkt_len - DHCPV6_PACKET_HEADER_LENGTH;
    return (DHCPv6OptionListCreate(pkt->options, option_len, err_p));
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

PRIVATE_EXTERN void
DHCPv6OptionListFPrintLevel(FILE * file, DHCPv6OptionListRef options, int level)
{
    int 	count = DHCPv6OptionListGetCount(options);
    int		i;
    int		lev;

    
    fprintf(file, "Options[%d] = {\n", count);
    for (i = 0; i < count; i++) {
	DHCPDUIDRef		duid;
	DHCPv6OptionRef		option;
	int			option_code;
	int			option_len;
	const uint8_t *		option_data;
	DHCPv6OptionType	type;

	option = DHCPv6OptionListGetOptionAtIndex(options, i);	
	option_code = DHCPv6OptionGetCode(option);
	option_len = DHCPv6OptionGetLength(option);
	option_data = DHCPv6OptionGetData(option);
	for (lev = 0; lev < level; lev++) {
	    fprintf(file, "  ");
	}
	fprintf(file, "  %s (%d) Length %d",
		DHCPv6OptionCodeGetName(option_code), option_code, option_len);
	type = DHCPv6OptionCodeGetType(option_code);
	switch (type) {
	case kDHCPv6OptionTypeNone:
	    break;
	case kDHCPv6OptionTypeDUID:
	    fprintf(file, " ");
	    duid = (DHCPDUIDRef)option_data;
	    DHCPDUIDFPrint(file, duid, option_len);
	    fprintf(file, "\n");
	    break;
	case kDHCPv6OptionTypeUInt16: {
	    int		j;
	    void *	scan;	

	    scan = (void *)option_data;
	    fprintf(file, ": ");
	    for (j = 0; j < option_len / sizeof(uint16_t); 
		j++, scan += sizeof(uint16_t)) {
		uint16_t	val;
	
		val = net_uint16_get(scan);

		if (option_code == kDHCPv6OPTION_ORO) {
		    fprintf(file, "%s%s (%d)", (j == 0) ? "" : ", ", 
			   DHCPv6OptionCodeGetName(val), val);
		}
		else {
		    fprintf(file, "%s%d", (j == 0) ? "" : ", ", val);
		}
	    }
	    fprintf(file, "\n");
	    break;
	}
	case kDHCPv6OptionTypeUInt32: {
	    int		j;
	    void *	scan;

	    scan = (void *)option_data;
	    fprintf(file, ": ");
	    for (j = 0; j < option_len / sizeof(uint32_t); 
		j++, scan += sizeof(uint32_t)) {
		fprintf(file, "%s%d", (j == 0) ? "" : ", ", 
			net_uint32_get(scan));
	    }
	    fprintf(file, "\n");
	    break;
	}
	case kDHCPv6OptionTypeIPv6Address: {
	    int				j;
	    void *			scan;
	    char 			ntopbuf[INET6_ADDRSTRLEN];

	    scan = (void *)option_data;
	    for (j = 0; j < option_len / sizeof(struct in6_addr); 
		 j++, scan += sizeof(struct in6_addr)) {
		fprintf(file, " %s\n", 
		       inet_ntop(AF_INET6, scan, ntopbuf, sizeof(ntopbuf)));
	    }
	    break;
	}
	case kDHCPv6OptionTypeDNSNameList: {
	    int			j;	
	    const char * *	list;
	    int			list_count;

	    list = DNSNameListCreate(option_data, option_len, &list_count);
	    if (list == NULL) {
		fprintf(file, " Invalid");
		goto print_option_data;
	    }
	    fprintf(file, ": ");
	    for (j = 0; j < list_count; j++) {
		fprintf(file, "%s%s", (j == 0) ? "" : ", ", list[j]);
	    }
	    free(list);
	    fprintf(file, "\n");
	    break;
	}
	case kDHCPv6OptionTypeIA_NA:
	    DHCPv6OptionIA_NAFPrintLevel(file,
					 (DHCPv6OptionIA_NARef)option_data, 
					 option_len, level);
	    break;

	case kDHCPv6OptionTypeIAADDR:
	    DHCPv6OptionIAADDRFPrintLevel(file,
					  (DHCPv6OptionIAADDRRef)option_data,
					  option_len, level);
	    break;

	case kDHCPv6OptionTypeStatusCode:
	    DHCPv6OptionSTATUS_CODEFPrint(file, (DHCPv6OptionSTATUS_CODERef)
					  option_data, option_len);
	    break;

	print_option_data:
	default:
	case kDHCPv6OptionTypeUnknown:
	    if (option_len != 0) {
		fprintf(file, " Data ");
		fprint_bytes(file,
			     (void *)DHCPv6OptionGetData(option), option_len);
	    }
	    fprintf(file, "\n");
	    break;
	}
    }
    for (lev = 0; lev < level; lev++) {
	fprintf(file, "  ");
    }
    fprintf(file, "}\n");
    return;
}

PRIVATE_EXTERN void
DHCPv6OptionListFPrint(FILE * file, DHCPv6OptionListRef options)
{
    DHCPv6OptionListFPrintLevel(file, options, 0);
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
				       int option_code, int * ret_length,
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
 ** DHCPv6OptionIA_NA
 **/
PRIVATE_EXTERN void
DHCPv6OptionIA_NAFPrintLevel(FILE * file, const DHCPv6OptionIA_NARef ia_na,
			     int ia_na_len, int level)
{
    int		option_len;

    if (ia_na_len < DHCPv6OptionIA_NA_MIN_LENGTH) {
	fprintf(file, " IA_NA option is too short %d < %d\n", 
		ia_na_len, DHCPv6OptionIA_NA_MIN_LENGTH);
	return;
    }
    option_len = ia_na_len - DHCPv6OptionIA_NA_MIN_LENGTH;
    fprintf(file, " IA_NA IAID=%d T1=%d T2=%d",
	    DHCPv6OptionIA_NAGetIAID(ia_na),
	    DHCPv6OptionIA_NAGetT1(ia_na),
	    DHCPv6OptionIA_NAGetT2(ia_na));

    if (option_len == 0) {
	fprintf(file, "\n");
    }
    else {
	DHCPv6OptionErrorString 	err;
	DHCPv6OptionListRef		options;

	options = DHCPv6OptionListCreate(ia_na->options, option_len, &err);
	if (options == NULL) {
	    fprintf(file, " options invalid:\n\t%s\n",
		    err.str);
	}
	else {
	    fprintf(file, " ");
	    /* possibly recurse */
	    DHCPv6OptionListFPrintLevel(file, options, level + 1);
	}
	DHCPv6OptionListRelease(&options);
    }
    return;
}

PRIVATE_EXTERN void
DHCPv6OptionIA_NAFPrint(FILE * file, const DHCPv6OptionIA_NARef ia_na, 
			int ia_na_len)
{
    DHCPv6OptionIA_NAFPrintLevel(file, ia_na, ia_na_len, 0);
    return;
}

/**
 ** DHCPv6OptionIAADDR
 **/
PRIVATE_EXTERN void
DHCPv6OptionIAADDRFPrintLevel(FILE * file, DHCPv6OptionIAADDRRef ia_addr,
			      int ia_addr_len, int level)
{
    char 	ntopbuf[INET6_ADDRSTRLEN];
    int		option_len;


    if (ia_addr_len < DHCPv6OptionIAADDR_MIN_LENGTH) {
	fprintf(file, " IAADDR option is too short %d < %d\n", 
		ia_addr_len, DHCPv6OptionIAADDR_MIN_LENGTH);
	return;
    }
    option_len = ia_addr_len - DHCPv6OptionIAADDR_MIN_LENGTH;
    fprintf(file, " IAADDR %s Preferred %d Valid=%d",
	    inet_ntop(AF_INET6,
		      DHCPv6OptionIAADDRGetAddress(ia_addr),
		      ntopbuf, sizeof(ntopbuf)),
	    DHCPv6OptionIAADDRGetPreferredLifetime(ia_addr),
	    DHCPv6OptionIAADDRGetValidLifetime(ia_addr));
    if (option_len == 0) {
	fprintf(file, "\n");
    }
    else {
	DHCPv6OptionErrorString 	err;
	DHCPv6OptionListRef		options;

	options = DHCPv6OptionListCreate(ia_addr->options, option_len, &err);
	if (options == NULL) {
	    fprintf(file, " options invalid:\n\t%s\n",
		    err.str);
	}
	else {
	    fprintf(file, " ");
	    /* possibly recurse */
	    DHCPv6OptionListFPrintLevel(file, options, level + 1);
	}
	DHCPv6OptionListRelease(&options);
    }
    return;
}

PRIVATE_EXTERN void
DHCPv6OptionIAADDRFPrint(FILE * file, DHCPv6OptionIAADDRRef ia_addr,
			 int ia_addr_len)
{
    DHCPv6OptionIAADDRFPrintLevel(file, ia_addr, ia_addr_len, 0);
    return;
}

/**
 ** DHCPv6StatusCode
 **/
PRIVATE_EXTERN const char * 
DHCPv6StatusCodeGetName(int code)
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
    default:
	str = "<unknown>";
	break;
    }
    return (str);
}

PRIVATE_EXTERN void 
DHCPv6OptionSTATUS_CODEFPrint(FILE * file, DHCPv6OptionSTATUS_CODERef status_p,
			      int status_len)
{
    uint16_t		code;
    int			message_len;

    if (status_len < DHCPv6OptionSTATUS_CODE_MIN_LENGTH) {
	fprintf(file, " STATUS_CODE option is too short %d < %d\n", 
		status_len, DHCPv6OptionSTATUS_CODE_MIN_LENGTH);
	return;
    }
    code = DHCPv6OptionSTATUS_CODEGetCode(status_p);
    message_len = status_len - DHCPv6OptionSTATUS_CODE_MIN_LENGTH;
    if (message_len) {
	fprintf(file, " STATUS_CODE %s (%d) '%.*s'\n",
		DHCPv6StatusCodeGetName(code), code,
		message_len, status_p->message);
    }
    else {
	fprintf(file, " STATUS_CODE %s (%d)\n",
		DHCPv6StatusCodeGetName(code), code);
    }
    return;
}

#if TEST_DHCPV6_OPTIONS
/* type = 1, hw = 1, time = aabbccdd, linklayer_address = 0x000102030405 */
#define DUID_LLT_1	0x00, 0x01, 0x00, 0x01, 0xaa, 0xbb, 0xcc, 0xdd, \
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05

#define DUID_EN_1	0x00, 0x02, 0x01, 0x02, 0x03, 0x04, 0xde, 0xad, \
	0xbe, 0xef

#define DUID_LL_1	0x00, 0x03, 0x00, 0x01, \
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05

#define DUID_LLT_UNDEF6_1 0x00, 0x06, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, \
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05

#define ORO_1 0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04

#define DNS_SERVER_1 0x20, 0x02, 0x45, 0xb5, 0xea, 0x61, 0x00, 0x00, 0x2, 0x1f, 0xf3, 0xff, 0xfe, 0x43, 0x1a, 0xbf

#define DNS_NAME_SEARCH_1 \
    4, 'e', 'u', 'r', 'o', 5, 'a', 'p', 'p', 'l', 'e', 3, 'c', 'o', 'm', 0, \
9, 'm', 'a', 'r', 'k', 'e', 't', 'i', 'n', 'g', 5, 'a', 'p', 'p', 'l', 'e', 3, 'c', 'o', 'm', 0, \
 11, 'e', 'n', 'g', 'i', 'n', 'e', 'e', 'r', 'i', 'n', 'g', 5, 'a', 'p', 'p', 'l', 'e', 3, 'c', 'o', 'm', 0


const uint8_t test_buf1[] = {
    0x00, 0x03, 0x00, 0x02, 0x01, 0x02,
    0x00, 0x01, 0x00, 0x0e, DUID_LLT_1 ,
    0x00, 0x01, 0x00, 0xa, DUID_EN_1 ,
    0x00, 0x02, 0x00, 0xa, DUID_LL_1 ,
    0x00, 0x06, 0x00, 0x08, ORO_1 ,
    0x00, 0x18, 0x00, 0x3c, DNS_NAME_SEARCH_1 ,
    0x00, 0x18, 0x00, 0x03, 0x1, 'a', 0,
    0x00, 0x02, 0x00, 0x0e, DUID_LLT_UNDEF6_1 ,
    0x00, 0x17, 0x00, 0x10, DNS_SERVER_1 ,
};

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

int
main(int argc, char * argv[])
{
    if (run_tests(tests, argc > 1) == FALSE) {
	fprintf(stderr, "TEST FAILED\n");
	exit(1);
    }
    exit(0);
    return (0);
}
#endif /* TEST_DHCPV6_OPTIONS */
