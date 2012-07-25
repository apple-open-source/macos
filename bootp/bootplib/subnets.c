/*
 * Copyright (c) 1998-2008, 2011 Apple Inc. All rights reserved.
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
 * subnets.c
 * - API's to access DHCP server subnet information
 */

/*
 * Modification History:
 * 
 * June 23, 2006	Dieter Siegmund (dieter@apple.com)
 * - initial revision (based on subnetDescr.m)
 */

#include <ctype.h>
#include <pwd.h>
#include <netdb.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <machine/endian.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFData.h>
#include "dynarray.h"
#include "subnets.h"
#include "util.h"
#include "dhcp_options.h"
#include "cfutil.h"
#include "DNSNameList.h"

#include <SystemConfiguration/SCValidation.h>
static bool 	S_use_syslog;
static int 	S_log_level;

/* default lease values */
#define DEFAULT_LEASE_MIN	((dhcp_lease_time_t)60 * 60)	/* one hour */
#define DEFAULT_LEASE_MAX	((dhcp_lease_time_t)60 * 60 * 24) /* one day */

struct _SubnetList {
    dynarray_t		list;
};

typedef struct _OptionTLV {
    int			tag;
    int			length;
    const void *	value;
} OptionTLV, * OptionTLVRef;

typedef struct ip_range {
    struct in_addr	start;
    struct in_addr	end;
} ip_range_t;

struct _Subnet {
    const char *	name;
    struct in_addr	net_address;
    struct in_addr	net_mask;
    ip_range_t		net_range;
    bool		allocate;	/* TRUE means this is an IP pool */
    dhcp_lease_time_t	lease_min;
    dhcp_lease_time_t	lease_max;
    struct in_addr	nextip; 	/* to try to allocate */
    const char *	supernet;
    OptionTLVRef	options;
    int			options_count;
};

typedef struct _Subnet Subnet;

static int
ip_range_compare(ip_range_t * a_p, ip_range_t * b_p, bool * overlap)
{
    in_addr_t		b_start = iptohl(b_p->start);
    in_addr_t		b_end = iptohl(b_p->end);
    in_addr_t		a_start = iptohl(a_p->start);
    in_addr_t		a_end = iptohl(a_p->end);
    int			result = 0;
    
    *overlap = TRUE;
    if (a_start == b_start) {
	result = 0;
    }
    else if (a_start < b_start) {
	result = -1;
	if (a_end < b_start)
	    *overlap = FALSE;
    }
    else {
	result = 1;
	if (b_end < a_start)
	    *overlap = FALSE;
    }
    return (result);
}


#ifdef DEBUG
/* 
 * Function: S_timestamp
 *
 * Purpose:
 *   printf a timestamped event message
 */
static void
S_timestamp(char * msg)
{
    static struct timeval	tvp = {0,0};
    struct timeval		tv;

    gettimeofday(&tv, 0);
    if (tvp.tv_sec) {
	int sec, usec;
#define USECS_PER_SEC	1000000
	sec = tv.tv_sec - tvp.tv_sec;
	usec = tv.tv_usec - tvp.tv_usec;
	if (usec < 0) {
	    usec += USECS_PER_SEC;
	    sec--;
	}
	printf("%d.%06d (%d.%06d): %s\n", 
	       tv.tv_sec, tv.tv_usec, sec, usec, msg);
    }
    else 
	printf("%d.%06d (%d.%06d): %s\n", 
	       tv.tv_sec, tv.tv_usec, 0, 0, msg);
    tvp = tv;
}
#endif /* DEBUG */

static void
SubnetPrint(SubnetRef subnet)
{
    printf("Subnet '%s'", 
	   (subnet->name != NULL) 
	   ? subnet->name : inet_nettoa(subnet->net_address, subnet->net_mask));
    if (subnet->supernet != NULL) {
	printf(": supernet %s\n", subnet->supernet);
    }
    else {
	printf("\n");
    }
    printf("\tNetwork: %s", inet_ntoa(subnet->net_address));
    printf("/%s\n", inet_ntoa(subnet->net_mask));
    printf("\tRange: %s..", inet_ntoa(subnet->net_range.start));
    printf("%s\n", inet_ntoa(subnet->net_range.end));
    printf("\tAllocate: %s\n", (subnet->allocate) ? "yes" : "no");
    if (subnet->allocate) {
	printf("\tLease Min: %d   Lease Max: %d\n", 
	       subnet->lease_min, subnet->lease_max);
    }
    if (subnet->options_count != 0) {
	int 	i;

	printf("\tOptions:\n");
	printf("\t%6s %6s   %s\n", "Code", "Length", "Data");
	for (i = 0; i < subnet->options_count; i++) {
	    printf("\t%6d %6d   ",
		   subnet->options[i].tag, subnet->options[i].length);
	    print_bytes((void *)subnet->options[i].value, 
			subnet->options[i].length);
	    printf("\n");
	}
    }
    return;
}

static __inline__ const char *
SubnetGetSupernet(SubnetRef subnet)
{
    return (subnet->supernet);
}

static bool
SubnetIsAddressOnSubnet(SubnetRef subnet, struct in_addr addr)
{
    return (in_subnet(subnet->net_address, subnet->net_mask, addr));
}

static bool
SubnetIsAddressWithinRange(SubnetRef subnet, struct in_addr ipaddr)
{
    in_addr_t l;

    if (SubnetIsAddressOnSubnet(subnet, ipaddr) == FALSE) {
	return (FALSE);
    }
    l = iptohl(ipaddr);
    if (l < iptohl(subnet->net_range.start)
	|| l > iptohl(subnet->net_range.end)) {
	return (FALSE);
    }
    return (TRUE);
}

static __inline__ ip_range_t
SubnetGetRange(SubnetRef subnet)
{
    return (subnet->net_range);
}

static int
SubnetCompareWithSubnet(SubnetRef subnet1, SubnetRef subnet2, bool * overlap)
{
    return (ip_range_compare(&subnet1->net_range, &subnet2->net_range, overlap));
}

static bool
SubnetSameSupernetAsSubnet(SubnetRef subnet1, SubnetRef subnet2)
{
    if (subnet1->supernet == NULL || subnet2->supernet == NULL) {
	return (FALSE);
    }
    if (strcmp(subnet1->supernet, subnet2->supernet) == 0) {
	return (TRUE);
    }
    return (FALSE);
}

bool
SubnetDoesAllocate(SubnetRef subnet)
{
    return (subnet->allocate);
}

static bool
SubnetAcquireAddress(SubnetRef subnet,
		     SubnetIsAddressInUseFuncRef func, void * arg,
		     struct in_addr * ret_addr)
{
    in_addr_t 	end;
    in_addr_t 	i;

    if (SubnetDoesAllocate(subnet) == FALSE) {
	return (FALSE);
    }
    end = iptohl(subnet->net_range.end);
    i = iptohl(subnet->nextip);
    if (i == (end + 1)) { /* previously exhausted ip range */
	i = iptohl(subnet->net_range.start);
    }
    for (; i <= end; i++) {
	if (func == NULL
	    || (*func)(arg, hltoip(i)) == FALSE) {
	    *ret_addr = hltoip(i);
	    subnet->nextip = hltoip(i);
	    return (TRUE);
	}
    }
    subnet->nextip = hltoip(end + 1);
    return (FALSE);
}

static const char *
SubnetGetName(SubnetRef subnet)
{
    return (subnet->name);
}

static bool
S_get_plist_boolean(CFDictionaryRef plist, CFStringRef key, bool d)
{
    CFBooleanRef	b;
    boolean_t 		ret = d;

    b = CFDictionaryGetValue(plist, key);
    if (isA_CFBoolean(b) != NULL) {
	ret = CFBooleanGetValue(b);
    }
    return (ret);
}

static char *
myCFStringCreateCStringWithRange(CFStringRef cfstr,
				 CFRange range, CFStringEncoding encoding)
{
    CFIndex	len = 0;
    char *	str;

    CFStringGetBytes(cfstr, range, encoding, 0, FALSE, NULL, 0, &len);
    if (len == 0) {
	return (NULL);
    }
    str = malloc(len + 1);
    CFStringGetBytes(cfstr, range, encoding, 0, FALSE, (UInt8 *)str, len, &len);
    str[len] = '\0';
    return (str);
}

static char *
myCFStringCreateCString(CFStringRef cfstr, CFStringEncoding encoding)
{
    CFRange	range;
    range = CFRangeMake(0, CFStringGetLength(cfstr));
    return (myCFStringCreateCStringWithRange(cfstr, range, encoding));
}

static CFDataRef
myCFDataCreateNumericType(dhcptype_t type, uint32_t l)
{
    uint8_t		b;
    CFDataRef		data = NULL;
    uint16_t		s;
    
    switch (type) {
    case dhcptype_bool_e:
	b = (l != 0) ? 1 : 0;
	data = CFDataCreate(NULL, &b, sizeof(b));
	break;
    case dhcptype_uint8_e:
	b = l;
	data = CFDataCreate(NULL, &b, sizeof(b));
	break;
    case dhcptype_uint16_e:
	s = htons(l);
	data = CFDataCreate(NULL, (UInt8 *)&s, sizeof(s));
	break;
    case dhcptype_int32_e:
    case dhcptype_uint32_e:
	l = htonl(l);
	data = CFDataCreate(NULL, (UInt8 *)&l, sizeof(l));
	break;
    default:
	break;
    }
    return (data);
}

static CFDataRef
myCFDataCreateWithTagAndCFType(dhcptag_t tag, CFTypeRef value,
			       char * err)
{
    CFDataRef			data = NULL;
    dhcptype_t			type;
    const dhcptag_info_t *	tag_info;
    const dhcptype_info_t *	type_info = NULL;

    if (err != NULL) {
	*err = '\0';
    }
    if (isA_CFData(value) != NULL) {
	return (CFRetain(value));
    }
    tag_info = dhcptag_info(tag);
    if (tag_info == NULL) {
	if (err != NULL) {
	    strcpy(err, "Unknown tag");
	}
	goto done;
    }
    type_info = dhcptype_info(tag_info->type);
    if (type_info == NULL) {
	if (err != NULL) {
	    sprintf(err, "Unknown type %d", tag_info->type);
	}
	goto done;
    }
    type = tag_info->type;
    if (isA_CFArray(value) != NULL) {
	if (CFArrayGetCount(value) == 0) {
	    /* zero length array */
	    if (err != NULL) {
		strcpy(err, "Empty array");
	    }
	    goto done;
	}
	if (type_info->string_list == FALSE) {
	    /* just grab the first element */
	    value = CFArrayGetValueAtIndex(value, 0);
	} 
    }
    if (isA_CFString(value) != NULL) {
	struct in_addr		ip;
	uint32_t		l;

	if (type_info->multiple_of != dhcptype_none_e) {
	    if (type == dhcptype_ip_pairs_e) {
		if (err != NULL) {
		    strcpy(err, "Type requires IP address pairs");
		}
		/* need 2 or more values */
		goto done;
	    }
	    /* use the base type */
	    type = type_info->multiple_of;
	}
	switch (type) {
	case dhcptype_string_e:
	    data = CFStringCreateExternalRepresentation(NULL,
							value,
							kCFStringEncodingUTF8,
							0);
	    break;
	case dhcptype_ip_e:
	    if (my_CFStringToIPAddress(value, &ip) == FALSE) {
		if (err != NULL) {
		    strcpy(err, "Invalid IP address");
		}
		goto done;
	    }
	    data = CFDataCreate(NULL, (UInt8 *)&ip, sizeof(ip));
	    break;
	case dhcptype_bool_e:
	case dhcptype_uint8_e:
	case dhcptype_uint16_e:
	case dhcptype_int32_e:
	case dhcptype_uint32_e:
	    if (my_CFStringToNumber(value, &l) == FALSE) {
		if (err != NULL) {
		    strcpy(err, "Invalid IP number");
		}
		goto done;
	    }
	    data = myCFDataCreateNumericType(type, l);
	    if (data == NULL) {
		if (err != NULL) {
		    strcpy(err, "Failed to convert to numeric");
		}
	    }
	    break;
	case dhcptype_dns_namelist_e: {
	    uint8_t *	encoded;
	    int		encoded_length;
	    char *	str;

	    str = myCFStringCreateCString(value, kCFStringEncodingUTF8);
	    if (str == NULL) {
		if (err != NULL) {
		    strcpy(err, "Failed to convert to string");
		}
		goto done;
	    }
	    encoded = DNSNameListBufferCreate((const char * *)&str, 1, 
					      NULL, &encoded_length);
	    free(str);
	    if (encoded == NULL) {
		if (err != NULL) {
		    strcpy(err, "Failed to encode DNS search");
		}
		goto done;
	    }
	    data = CFDataCreate(NULL, encoded, encoded_length);
	    free(encoded);
	    break;
	}
	default:
	    if (err != NULL) {
		sprintf(err, "Failed to convert from string to %s",
			type_info->name);
	    }
	    /* can't convert from string */
	    goto done;
	}
    }
    else if (isA_CFNumber(value) != NULL || isA_CFBoolean(value) != NULL) {
	uint32_t		l;
	
	if (type_info->multiple_of != dhcptype_none_e) {
	    if (type == dhcptype_ip_pairs_e) {
		if (err != NULL) {
		    strcpy(err, "Type requires pairs of IP address values");
		}
		/* need 2 or more values */
		goto done;
	    }
	    /* use the base type */
	    type = type_info->multiple_of;
	}
	if (isA_CFBoolean(value) != NULL) {
	    l = (CFEqual(value, kCFBooleanTrue)) ? 1 : 0;
	}
	else if (CFNumberGetValue(value, kCFNumberSInt32Type, &l) 
		 == FALSE) {
	    if (err != NULL) {
		strcpy(err, "Failed to convert to numeric");
	    }
	    goto done;
	}
	data = myCFDataCreateNumericType(type, l);
	if (data == NULL) {
	    goto done;
	}
    }
    else if (isA_CFArray(value) != NULL) {
	int			i;
	int			count = CFArrayGetCount(value);

	switch (type) {
	case dhcptype_ip_pairs_e:
	    if (count & 1) {
		/* must appear in pairs */
		if (err != NULL) {
		    strcpy(err, "Type requires pairs of IP address values");
		}
		goto done;
	    }
	    /* FALL THROUGH */
	case dhcptype_ip_mult_e: {
	    CFMutableDataRef	d;
	    struct in_addr	ip;

	    d = CFDataCreateMutable(NULL, count * sizeof(ip));
	    for (i = 0; i < count; i++) {
		CFStringRef	element = CFArrayGetValueAtIndex(value, i);

		if (isA_CFString(element) == NULL) {
		    if (err != NULL) {
			strcpy(err, "Can't convert non-string to IP address");
		    }
		    /* each element needs to be a string */
		    CFRelease(d);
		    goto done;
		}
		if (my_CFStringToIPAddress(element, &ip) == FALSE) {
		    if (err != NULL) {
			strcpy(err, "Invalid IP address");
		    }
		    CFRelease(d);
		    goto done;
		}
		CFDataAppendBytes(d, (UInt8 *)&ip, sizeof(ip));
	    }
	    data = CFDataCreateCopy(NULL, d);
	    CFRelease(d);
	    break;
	}
	case dhcptype_uint8_mult_e:
	case dhcptype_uint16_mult_e: {
	    CFMutableDataRef	d;
	    uint32_t		l;

	    d = CFDataCreateMutable(NULL, count * type_info->size);
	    for (i = 0; i < count; i++) {
		CFTypeRef	element = CFArrayGetValueAtIndex(value, i);

		if (my_CFTypeToNumber(element, &l) == FALSE) {
		    if (err != NULL) {
			strcpy(err, "Invalid number");
		    }
		    CFRelease(d);
		    goto done;
		}
		if (type_info->size == 1) {
		    uint8_t	b = l;
		    CFDataAppendBytes(d, &b, sizeof(b));
		}
		else {
		    uint16_t	s = htons(l);
		    CFDataAppendBytes(d, (UInt8 *)&s, sizeof(s));
		}
	    }
	    data = CFDataCreateCopy(NULL, d);
	    CFRelease(d);
	    break;
	}
	case dhcptype_dns_namelist_e: {
	    uint8_t *	encoded;
	    int		encoded_length;
	    char * *	strlist;
	    int		strlist_count;
	    int		strlist_size;

	    /* find out how much space to allocate */
	    if (my_CFStringArrayToCStringArray(value, NULL, &strlist_size,
					       &strlist_count) == FALSE) {
		if (err != NULL) {
		    strcpy(err, "Could not convert DNS search to string list");
		}
		goto done;
	    }
	    /* allocate and populate */
	    strlist = (char * *)malloc(strlist_size);
	    if (my_CFStringArrayToCStringArray(value,
					       (char *)strlist, &strlist_size,
					       &strlist_count) == FALSE) {
		free(strlist);
		goto done;
	    }
	    encoded = DNSNameListBufferCreate((const char * *)strlist,
					      strlist_count,
					      NULL, &encoded_length);
	    free(strlist);
	    if (encoded == NULL) {
		if (err != NULL) {
		    strcpy(err, "Failed to encode DNS search");
		}
		goto done;
	    }
	    data = CFDataCreate(NULL, encoded, encoded_length);
	    free(encoded);
	    break;
	}
	default:
	    /* conversion is not possible */
	    if (err != NULL) {
		sprintf(err, "Can't convert array to %s", type_info->name);
	    }
	    goto done;
	}
    }
 done:
    return (data);
}

#define kOptionTag	CFSTR("Tag")
#define kOptionData	CFSTR("Data")

static CFArrayRef
createOptionsDataArrayFromDictionary(CFDictionaryRef plist, int * ret_space)
{
    CFMutableArrayRef	array;
    int			count;
    int			i;
    const void * * 	keys;
    int			space;
    const void * *	values;

    *ret_space = 0;
    count = CFDictionaryGetCount(plist);
    if (count == 0) {
	return (NULL);
    }
    keys = malloc(count * sizeof(*keys));
    values = malloc(count * sizeof(*values));
    CFDictionaryGetKeysAndValues(plist, keys, values);

    array = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks);
    space = 0;
    for (i = 0; i < count; i++) {
	char			err[256];
	char *			option_name;
	CFRange			range;
	dhcptag_t		tag;
	CFDataRef		this_data;
	CFStringRef		this_key = keys[i];
	CFStringRef		this_value = values[i];

	if (CFStringHasPrefix(this_key, CFSTR("dhcp_")) == FALSE) {
	    /* not a DHCP option */
	    continue;
	}
	range = CFRangeMake(5, CFStringGetLength(this_key));
	option_name = myCFStringCreateCStringWithRange(this_key, range,
						       kCFStringEncodingASCII);
	if (option_name == NULL) {
	    continue;
	}
	tag = dhcptag_with_name(option_name);
	if (tag == -1) {
	    if (S_use_syslog) {
		syslog(S_log_level,
		       "subnets: unrecognized option '%s'", option_name);
	    }
	    else {
		fprintf(stderr, 
			"subnets: unrecognized option '%s'\n", option_name);
	    }
	    goto loop_done;
	}
	if (tag == dhcptag_subnet_mask_e) {
	    /* redundant, subnet mask comes from net_mask property */
	    goto loop_done;
	}
	this_data = myCFDataCreateWithTagAndCFType(tag, (CFTypeRef)this_value,
						   err);
	if (this_data != NULL) {
	    CFMutableDictionaryRef	dict;
	    CFNumberRef			num;

	    space += CFDataGetLength(this_data);
	    dict = CFDictionaryCreateMutable(NULL, 0,
					     &kCFTypeDictionaryKeyCallBacks,
					     &kCFTypeDictionaryValueCallBacks);
	    num = CFNumberCreate(NULL, kCFNumberIntType, &tag);
	    CFDictionarySetValue(dict, kOptionTag, num);
	    CFRelease(num);
	    CFDictionarySetValue(dict, kOptionData, this_data);
	    CFRelease(this_data);
	    CFArrayAppendValue(array, dict);
	    CFRelease(dict);
	}
	else {
	    if (S_use_syslog) {
		syslog(S_log_level, 
		       "subnets: Failed to convert '%s': %s",
		       option_name, err);
	    }
	    else {
		fprintf(stderr, 
			"subnets: Failed to convert '%s': %s\n",
			option_name, err);
	    }
	}
    loop_done:
	free(option_name);
    }
    free(keys);
    free(values);
    if (CFArrayGetCount(array) == 0) {
	CFRelease(array);
	array = NULL;
    }
    else {
	*ret_space = space;
    }
    return (array);
}

static OptionTLVRef
copyOptionsDataArrayToOptionTLVList(CFArrayRef option_list, 
				    OptionTLVRef list, int buf_space)
{
    int			count;
    int			i;
    char *		start_options;

    count = CFArrayGetCount(option_list);
    if (buf_space < (sizeof(OptionTLV) * count)) {
	/* internal error */
	if (S_use_syslog) {
	    syslog(S_log_level,
		   "copyOptionsDataArrayToOptionTLVList %d < %d",
		   buf_space, (int)sizeof(OptionTLV) * count);
	}
	else {
	    fprintf(stderr, "copyOptionsDataArrayToOptionTLVList %d < %d\n",
		    buf_space, (int)sizeof(OptionTLV) * count);
	}
	return (NULL);
    }
    buf_space -= sizeof(OptionTLV) * count;
    start_options = (char *)list + sizeof(OptionTLV) * count;
    for (i = 0; i < count; i++) {
	CFDataRef	data;
	CFDictionaryRef	dict = CFArrayGetValueAtIndex(option_list, i);
	int		this_length;
	int		tag;

	data = CFDictionaryGetValue(dict, kOptionData);
	(void)CFNumberGetValue(CFDictionaryGetValue(dict, kOptionTag),
			       kCFNumberIntType, &tag);
	this_length = CFDataGetLength(data);
	list[i].tag = tag;
	list[i].length = this_length;
	list[i].value = start_options;
	if (buf_space < this_length) {
	    if (S_use_syslog) {
		syslog(S_log_level,
		       "copyOptionsDataArrayToOptionTLVList option %d < %d",
		       buf_space, this_length);
	    }
	    else {
		fprintf(stderr,
			"copyOptionsDataArrayToOptionTLVList option %d < %d\n",
			buf_space, this_length);
	    }
	    return (NULL);
	}
	memcpy(start_options, CFDataGetBytePtr(data), this_length);
	start_options += this_length;
	buf_space -= this_length;
    }
    return (list);
}

static void
SubnetSetLeaseMaxMin(SubnetRef subnet, CFDictionaryRef plist)
{
    uint32_t		l;
    CFTypeRef		value;

    value = CFDictionaryGetValue(plist, CFSTR(SUBNET_PROP_LEASE_MAX));
    if (my_CFTypeToNumber(value, &l)) {
	subnet->lease_max = l;
    }
    else {
	subnet->lease_max = DEFAULT_LEASE_MAX;
    }
    value = CFDictionaryGetValue(plist, CFSTR(SUBNET_PROP_LEASE_MIN));
    if (my_CFTypeToNumber(value, &l)) {
	subnet->lease_min = l;
    }
    else {
	subnet->lease_min = DEFAULT_LEASE_MIN;
    }
    if (subnet->lease_min <= 0) {
	subnet->lease_min = DEFAULT_LEASE_MIN;
    }
    if (subnet->lease_max < subnet->lease_min) {
	subnet->lease_max = subnet->lease_min;
    }
    return;
}

dhcp_lease_time_t
SubnetGetMaxLease(SubnetRef subnet)
{
    return (subnet->lease_max);
}

dhcp_lease_time_t
SubnetGetMinLease(SubnetRef subnet)
{
    return (subnet->lease_min);
}

const char *
SubnetGetOptionPtrAndLength(SubnetRef subnet, dhcptag_t tag, 
			    int * option_length)
{
    int			i;
    OptionTLVRef	scan;

    if (tag == dhcptag_subnet_mask_e) {
	*option_length = sizeof(subnet->net_mask);
	return ((const char *)&subnet->net_mask);
    }

    for (i = 0, scan = subnet->options; i < subnet->options_count;
	 i++, scan++) {
	if (scan->tag == tag) {
	    *option_length = scan->length;
	    return (scan->value);
	}
    }
    return (NULL);
}

struct in_addr
SubnetGetMask(SubnetRef subnet)
{
    return (subnet->net_mask);
}

static SubnetRef
SubnetCreateWithDictionary(CFDictionaryRef plist, char * err)
{
    struct in_addr	net_address;
    struct in_addr	net_mask;
    ip_range_t		net_range;
    CFArrayRef		net_range_prop;
    CFStringRef		name_prop;
    int			name_space = 0;
    char *		offset;
    CFArrayRef 		option_list = NULL;
    int			option_space = 0;
    CFStringRef		prop_val;
    SubnetRef		subnet = NULL;
    CFStringRef		supernet_prop;
    int			supernet_space = 0;
    int			tail_space = 0;

    if (isA_CFDictionary(plist) == NULL) {
	if (err != NULL) {
	    strcpy(err, "subnet element is not a dictionary");
	    return (NULL);
	}
    }
    if (err != NULL) {
	err[0] = '\0';
    }

    /* name */
    name_prop = CFDictionaryGetValue(plist, CFSTR(SUBNET_PROP_NAME));
    if (name_prop != NULL) {
	if (isA_CFString(name_prop) == NULL) {
	    if (err != NULL) {
		strcpy(err, "Invalid '" SUBNET_PROP_NAME "' property");
	    }
	    goto failed;
	}
	name_space 
	    = my_CFStringToCStringAndLength(name_prop, NULL, 0);
	if (name_space <= 1) {
	    name_space = 0;
	}
	else {
	    tail_space += name_space;
	}
    }

    /* supernet */
    supernet_prop = CFDictionaryGetValue(plist, CFSTR(SUBNET_PROP_SUPERNET));
    if (supernet_prop != NULL) {
	if (isA_CFString(supernet_prop) == NULL) {
	    if (err != NULL) {
		strcpy(err, "Invalid '" SUBNET_PROP_SUPERNET "' property");
	    }
	    goto failed;
	}
	supernet_space 
	    = my_CFStringToCStringAndLength(supernet_prop, NULL, 0);
	if (supernet_space <= 1) {
	    supernet_space = 0;
	}
	else {
	    tail_space += supernet_space;
	}
    }

    /* get the net address */
    prop_val = CFDictionaryGetValue(plist, CFSTR(SUBNET_PROP_NET_ADDRESS));
    if (isA_CFString(prop_val) == NULL
	|| my_CFStringToIPAddress(prop_val, &net_address) == FALSE) {
	if (err != NULL) {
	    strcpy(err,
		   "Invalid/missing '" SUBNET_PROP_NET_ADDRESS "' property");
	}
	goto failed;
    }
	
    /* get the net mask */
    prop_val = CFDictionaryGetValue(plist, CFSTR(SUBNET_PROP_NET_MASK));
    if (isA_CFString(prop_val) == NULL
	|| my_CFStringToIPAddress(prop_val, &net_mask) == FALSE) {
	if (err != NULL) {
	    strcpy(err,
		   "Invalid/missing '" SUBNET_PROP_NET_MASK "' property");
	}
	goto failed;
    }

    /* get the ip range */
    net_range_prop = CFDictionaryGetValue(plist, CFSTR(SUBNET_PROP_NET_RANGE));
    if (isA_CFArray(net_range_prop) == NULL) {
	if (err != NULL) {
	    strcpy(err,
		   "Invalid/missing '" SUBNET_PROP_NET_RANGE "' property");
	}
	goto failed;
    }
    if (CFArrayGetCount(net_range_prop) != 2) {
	if (err != NULL) {
	    strcpy(err,
		   "'" SUBNET_PROP_NET_RANGE "' property must specify 2 values");
	}
	goto failed;
    }
    prop_val = CFArrayGetValueAtIndex(net_range_prop, 0);
    if (isA_CFString(prop_val) == NULL
	|| my_CFStringToIPAddress(prop_val, &net_range.start) == FALSE) {
	if (err != NULL) {
	    strcpy(err, "Invalid '" SUBNET_PROP_NET_RANGE "' range start");
	}
	goto failed;
    }
    prop_val = CFArrayGetValueAtIndex(net_range_prop, 1);
    if (isA_CFString(prop_val) == NULL
	|| my_CFStringToIPAddress(prop_val, &net_range.end) == FALSE) {
	if (err != NULL) {
	    strcpy(err, "Invalid '" SUBNET_PROP_NET_RANGE "' range end");
	}
	goto failed;
    }
    if (in_subnet(net_address, net_mask, net_range.start) == FALSE) {
	if (err != NULL) {
	    strcpy(err, "'" SUBNET_PROP_NET_RANGE "' start not within subnet");
	}
	goto failed;
    }
    if (in_subnet(net_address, net_mask, net_range.end) == FALSE) {
	if (err != NULL) {
	    strcpy(err, "'" SUBNET_PROP_NET_RANGE "' end not within subnet");
	}
	goto failed;
    }
    if (name_space == 0) {
	name_prop = NULL;
	name_space = strlen(inet_nettoa(net_address, net_mask)) + 1;
	tail_space += name_space;
    }

    option_list = createOptionsDataArrayFromDictionary(plist, &option_space);
    if (option_list != NULL) {
	option_space = roundup(option_space, sizeof(char *))
	    + CFArrayGetCount(option_list) * sizeof(OptionTLV);
	tail_space += option_space;
    }
    subnet = malloc(sizeof(*subnet) + tail_space);
    bzero(subnet, sizeof(*subnet));
    SubnetSetLeaseMaxMin(subnet, plist);
    subnet->net_address = net_address;
    subnet->net_mask = net_mask;
    subnet->net_range = net_range;
    subnet->allocate = S_get_plist_boolean(plist, CFSTR("allocate"), FALSE);

    offset = (char *)(subnet + 1);

    /* copy the options */
    if (option_list != NULL) {
	subnet->options_count = CFArrayGetCount(option_list);
	/* ALIGN: offset aligned (from malloc), cast safe. */
	subnet->options = 
	    copyOptionsDataArrayToOptionTLVList(option_list, (void *)offset,
						option_space);
	offset += option_space;
	CFRelease(option_list);
    }

    /* copy the name */
    subnet->name = offset;
    if (name_prop != NULL) {
	my_CFStringToCStringAndLength(name_prop, offset, name_space);
    }
    else {
	strncpy(offset, inet_nettoa(net_address, net_mask), name_space);
    }
    offset += name_space;

    /* copy the supernet */
    if (supernet_space != 0) {
	my_CFStringToCStringAndLength(supernet_prop, offset, supernet_space);
	subnet->supernet = offset;
	/* offset += supernet_space; */
    }
    subnet->nextip = net_range.start;

 failed:
    return (subnet);
}

/**
 ** SubnetList
 **/

void
SubnetListLogErrors(int level)
{
    S_use_syslog = TRUE;
    S_log_level = level;
    return;
}

static __inline__ int
SubnetListCount(SubnetListRef subnets)
{
    return (dynarray_count(&subnets->list));
}

static __inline__ SubnetRef
SubnetListElement(SubnetListRef subnets, int i)
{
    return (dynarray_element(&subnets->list, i));
}

static bool
SubnetListAddSubnet(SubnetListRef subnets, SubnetRef new_entry)
{
    int		count;
    int 	i;

    count = SubnetListCount(subnets);
    for (i = 0; i < count; i++) {
	int 		c;
	SubnetRef	entry = SubnetListElement(subnets, i);
	bool		overlap = FALSE;

	c = SubnetCompareWithSubnet(new_entry, entry, &overlap);
	if (overlap) {
	    if (S_use_syslog) {
		syslog(S_log_level,
		       "subnets: net_range in '%s' overlaps with subnet '%s'",
		       SubnetGetName(new_entry),
		       SubnetGetName(entry));
	    }
	    else {
		fprintf(stderr, 
			"subnets: net_range in '%s' overlaps with '%s'\n",
			SubnetGetName(new_entry),
			SubnetGetName(entry));
	    }
	    return (FALSE);
	}
	if (c < 0) {
	    dynarray_insert(&subnets->list, new_entry, i);
	    return (TRUE);
	}
    }

    /* append to end */
    dynarray_add(&subnets->list, new_entry);
    return (TRUE);
}


static void
SubnetFree(SubnetRef subnet)
{
    free(subnet);
    return;
}

SubnetListRef
SubnetListCreateWithArray(CFArrayRef list)
{
    char			err[256];
    int				count;
    int				i;
    SubnetListRef		subnets = NULL;

    if (isA_CFArray(list) == NULL) {
	if (S_use_syslog) {
	    syslog(S_log_level,
		   "subnets: type is not an array");
	}
	else {
	    fprintf(stderr, "subnets: type is not an array\n");
	}
	return (NULL);
    }
    subnets = (SubnetListRef)malloc(sizeof(*subnets));
    if (subnets == NULL) {
	return (NULL);
    }
    bzero(subnets, sizeof(*subnets));
    dynarray_init(&subnets->list, (void *)SubnetFree, NULL);
    count = CFArrayGetCount(list);
    for (i = 0; i < count; i++) {
	CFDictionaryRef	dict = CFArrayGetValueAtIndex(list, i);
	SubnetRef	entry;

	entry = SubnetCreateWithDictionary(dict, err);
	if (entry == NULL) {
	    if (S_use_syslog) {
		syslog(S_log_level,
		       "subnets: create failed, %s", err);
	    }
	    else {
		fprintf(stderr, "subnets: create failed, %s\n", err);
	    }
	    goto failed;
	}
	if (SubnetListAddSubnet(subnets, entry) == FALSE) {
	    SubnetFree(entry);
	    goto failed;
	}
    }
    return (subnets);

 failed:
    SubnetListFree(&subnets);
    return (NULL);
}

void
SubnetListFree(SubnetListRef * subnets_p)
{
    SubnetListRef	subnets;

    if (subnets_p == NULL) {
	return;
    }
    subnets = *subnets_p;
    if (subnets == NULL) {
	return;
    }
    dynarray_free(&subnets->list);
    free(subnets);
    *subnets_p = NULL;
    return;
}

void
SubnetListPrint(SubnetListRef subnets)
{
    int		count;
    int		i;

    count = SubnetListCount(subnets);
    printf("There are %d entries\n", count);
    for (i = 0; i < count; i++) {
	SubnetRef	entry = SubnetListElement(subnets, i);

	printf("%2d. ", i + 1);
	SubnetPrint(entry);
    }
    return;
}

/*
 * Function: SubnetListAcquireAddress
 *
 * Purpose:
 *   Get a new ip address on the same subnet or "supernet" as *addr.  The new ip
 *   address is returned in addr.
 */
SubnetRef
SubnetListAcquireAddress(SubnetListRef subnets, struct in_addr * addr,
			 SubnetIsAddressInUseFuncRef func, void * arg)
{
    int			count;
    SubnetRef		entry;
    int 		i;
    struct in_addr	subnet_address = *addr;

    entry = SubnetListGetSubnetForAddress(subnets, subnet_address, FALSE);
    if (entry == NULL) {
	return (NULL);
    }

    count = SubnetListCount(subnets);
    for (i = 0; i < count; i++) {
	SubnetRef	this_entry = SubnetListElement(subnets, i);

	if (this_entry == entry
	    || SubnetIsAddressOnSubnet(this_entry, subnet_address)
	    || SubnetSameSupernetAsSubnet(this_entry, entry)) {
	    if (SubnetAcquireAddress(this_entry, func, arg, addr)) {
		return (this_entry);
	    }
	}
    }
    return (NULL);
}

SubnetRef
SubnetListGetSubnetForAddress(SubnetListRef subnets, struct in_addr addr,
			      bool in_range)
{
    bool		(*func)(SubnetRef subnet, struct in_addr addr);
    int			count;
    int 		i;

    count = SubnetListCount(subnets);
    func = in_range ? SubnetIsAddressWithinRange : SubnetIsAddressOnSubnet;
    for (i = 0; i < count; i++) {
	SubnetRef	entry = SubnetListElement(subnets, i);

	if ((*func)(entry, addr)) {
	    return (entry);
	}
    }
    return (NULL);
}

bool
SubnetListAreAddressesOnSameSupernet(SubnetListRef subnets,
				     struct in_addr addr1,
				     struct in_addr addr2)
{
    SubnetRef		subnet1;
    SubnetRef		subnet2;

    subnet1 = SubnetListGetSubnetForAddress(subnets, addr1, FALSE);
    subnet2 = SubnetListGetSubnetForAddress(subnets, addr2, FALSE);
    if (subnet1 == NULL || subnet2 == NULL) {
	/* don't know anything about one or both of the addresses */
	return (FALSE);
    }
    if (subnet1 == subnet2) {
	return (TRUE);
    }
    return (SubnetSameSupernetAsSubnet(subnet1, subnet2));
}



#ifdef TEST_SUBNETS

int
main(int argc, const char * argv[])
{
    CFArrayRef		list;
    SubnetListRef	subnets;
    CFDictionaryRef	plist;

    if (argc != 2) {
	fprintf(stderr, "Usage: subnets <file>\n");
	exit(1);
    }
    plist = my_CFPropertyListCreateFromFile(argv[1]);
    if (isA_CFDictionary(plist) == NULL) {
	fprintf(stderr, "failed to load %s\n", argv[1]);
	exit(1);
    }
    list = CFDictionaryGetValue(plist, CFSTR("Subnets"));
    subnets = SubnetListCreateWithArray(list);
    if (subnets != NULL) {
	struct in_addr	ip;
	SubnetRef	subnet;

	inet_aton("17.202.42.110", &ip);
	SubnetListPrint(subnets);

	subnet = SubnetListAcquireAddress(subnets, &ip, NULL, NULL);
	if (subnet != NULL) {
	    SubnetPrint(subnet);
	    printf("Allocated %s\n", inet_ntoa(ip));
	}
	SubnetListFree(&subnets);
    }
    else {
	fprintf(stderr, "SubnetListCreateWithArray failed\n");
    }
    exit(0);
}
#endif /* TEST_SUBNETS */
