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
 * DHCPLease.c
 * - routines to handle the DHCP in-memory lease list and the lease
 *   stored in the filesystem
 */

/* 
 * Modification History
 *
 * June 11, 2009		Dieter Siegmund (dieter@apple.com)
 * - split out from ipconfigd.c
 */

#include <CoreFoundation/CFString.h>
#include <SystemConfiguration/SCValidation.h>
#include "DHCPLease.h"
#include "util.h"
#include "host_identifier.h"
#include "globals.h"
#include "cfutil.h"
#include "dhcp_thread.h"

#define DHCPCLIENT_LEASES_DIR		DHCPCLIENT_DIR "/leases"
#define DHCPCLIENT_LEASE_FILE_FMT	DHCPCLIENT_LEASES_DIR "/%s-%s"

static void
dhcp_lease_init()
{
    static int	done = 0;

    if (done != 0) {
	return;
    }
    if (create_path(DHCPCLIENT_LEASES_DIR, 0700) < 0) {
	my_log(LOG_DEBUG, "failed to create " 
	       DHCPCLIENT_LEASES_DIR ", %s (%d)", strerror(errno), errno);
	return;
    }
    done = 1;
    return;
}

/* required properties: */
#define kLeaseStartDate			CFSTR("LeaseStartDate")
#define kPacketData			CFSTR("PacketData")
#define kRouterHardwareAddress		CFSTR("RouterHardwareAddress")

/* informative properties: */
#define kLeaseLength			CFSTR("LeaseLength")
#define kIPAddress			CFSTR("IPAddress")
#define kRouterIPAddress		CFSTR("RouterIPAddress")

/*
 * Function: DHCPLeaseCreateWithDictionary
 * Purpose:
 *   Instantiate a new DHCPLease structure corresponding to the given
 *   dictionary.  Validates that required properties are present,
 *   returns NULL if those checks fail.
 */
static DHCPLeaseRef
DHCPLeaseCreateWithDictionary(CFDictionaryRef dict)
{
    CFDataRef			hwaddr_data;
    dhcp_lease_time_t		lease_time;
    DHCPLeaseRef		lease_p = NULL;
    CFDataRef			pkt_data;
    CFRange			pkt_data_range;
    struct in_addr *		router_p;
    CFDateRef			start_date;
    dhcp_lease_time_t		t1_time;
    dhcp_lease_time_t		t2_time;

    /* get the lease start time */
    start_date = CFDictionaryGetValue(dict, kLeaseStartDate);
    if (isA_CFDate(start_date) == NULL) {
	goto failed;
    }
    /* get the packet data */
    pkt_data = CFDictionaryGetValue(dict, kPacketData);
    if (isA_CFData(pkt_data) == NULL) {
	goto failed;
    }
    pkt_data_range.location = 0;
    pkt_data_range.length = CFDataGetLength(pkt_data);
    if (pkt_data_range.length < sizeof(struct dhcp)) {
	goto failed;
    }
    lease_p = (DHCPLeaseRef)malloc(sizeof(*lease_p) - 1
				   + pkt_data_range.length);
    bzero(lease_p, sizeof(*lease_p) - 1);

    /* copy the packet data */
    CFDataGetBytes(pkt_data, pkt_data_range, lease_p->pkt);
    lease_p->pkt_length = pkt_data_range.length;

    /* get the lease information and router IP address */
    lease_p->lease_start = (absolute_time_t)CFDateGetAbsoluteTime(start_date);
    { /* parse/retrieve options */
	dhcpol_t			options;
	
	(void)dhcpol_parse_packet(&options, (void *)lease_p->pkt,
				  pkt_data_range.length, NULL);
	dhcp_get_lease_from_options(&options, &lease_time, &t1_time, &t2_time);
	router_p = dhcp_get_router_from_options(&options, lease_p->our_ip);
	dhcpol_free(&options);
    }
    lease_p->lease_length = lease_time;

    /* get the IP address */
    /* ALIGN: lease_p->pkt is aligned, cast ok. */
    lease_p->our_ip = ((struct dhcp *)(void *)lease_p->pkt)->dp_yiaddr;

    /* get the router information */
    if (router_p != NULL) {
	CFRange		hwaddr_range;

	lease_p->router_ip = *router_p;
	/* get the router hardware address */
	hwaddr_data = CFDictionaryGetValue(dict, kRouterHardwareAddress);
	hwaddr_range.length = 0;
	if (isA_CFData(hwaddr_data) != NULL) {
	    hwaddr_range.length = CFDataGetLength(hwaddr_data);
	}
	if (hwaddr_range.length > 0) {
	    hwaddr_range.location = 0;
	    if (hwaddr_range.length > sizeof(lease_p->router_hwaddr)) {
		hwaddr_range.length = sizeof(lease_p->router_hwaddr);
	    }
	    lease_p->router_hwaddr_length = hwaddr_range.length;
	    CFDataGetBytes(hwaddr_data, hwaddr_range, lease_p->router_hwaddr);
	}
    }
    return (lease_p);

 failed:
    if (lease_p != NULL) {
	free(lease_p);
    }
    return (NULL);
}

void
DHCPLeaseSetNAK(DHCPLeaseRef lease_p, int nak)
{
    lease_p->nak = nak;
    return;
}

/*
 * Function: DHCPLeaseCreate
 * Purpose:
 *   Instantiate a new DHCPLease structure corresponding to the given
 *   information.
 */
static DHCPLeaseRef
DHCPLeaseCreate(struct in_addr our_ip, struct in_addr router_ip,
		const uint8_t * router_hwaddr, int router_hwaddr_length,
		absolute_time_t lease_start, 
		dhcp_lease_time_t lease_length,
		const uint8_t * pkt, int pkt_size)
{
    DHCPLeaseRef		lease_p = NULL;
    int				lease_data_length;

    lease_data_length = offsetof(DHCPLease, pkt) + pkt_size;
    lease_p = (DHCPLeaseRef)malloc(lease_data_length);
    bzero(lease_p, lease_data_length);
    lease_p->our_ip = our_ip;
    lease_p->router_ip = router_ip;
    lease_p->lease_start = lease_start;
    lease_p->lease_length = lease_length;
    bcopy(pkt, lease_p->pkt, pkt_size);
    lease_p->pkt_length = pkt_size;
    if (router_hwaddr != NULL && router_hwaddr_length != 0) {
	if (router_hwaddr_length > sizeof(lease_p->router_hwaddr)) {
	    router_hwaddr_length = sizeof(lease_p->router_hwaddr);
	}
	lease_p->router_hwaddr_length = router_hwaddr_length;
	bcopy(router_hwaddr, lease_p->router_hwaddr, router_hwaddr_length);
    }
    return (lease_p);
}

static CFDictionaryRef
DHCPLeaseCopyDictionary(DHCPLeaseRef lease_p)
{
    CFDataRef			data;
    CFDateRef			date;
    CFMutableDictionaryRef	dict;
    CFNumberRef			num;
    CFStringRef			str;

    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    /* set the IP address */
    str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT),
				   IP_LIST(&lease_p->our_ip));
    CFDictionarySetValue(dict, kIPAddress, str);
    CFRelease(str);


    /* set the lease start date */
    date = CFDateCreate(NULL, lease_p->lease_start);
    CFDictionarySetValue(dict, kLeaseStartDate, date);
    CFRelease(date);

    /* set the lease length */
    num = CFNumberCreate(NULL, kCFNumberSInt32Type, &lease_p->lease_length);
    CFDictionarySetValue(dict, kLeaseLength, num);
    CFRelease(num);

    /* set the packet data */
    data = CFDataCreateWithBytesNoCopy(NULL, lease_p->pkt, lease_p->pkt_length,
				       kCFAllocatorNull);
    CFDictionarySetValue(dict, kPacketData, data);
    CFRelease(data);

    if (lease_p->router_ip.s_addr != 0) {
	/* set the router IP address */
	str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT),
				       IP_LIST(&lease_p->router_ip));
	CFDictionarySetValue(dict, kRouterIPAddress, str);
	CFRelease(str);

	if (lease_p->router_hwaddr_length > 0) {
	    /* set the router hardware address */
	    data = CFDataCreateWithBytesNoCopy(NULL, lease_p->router_hwaddr,
					       lease_p->router_hwaddr_length,
					       kCFAllocatorNull);
	    CFDictionarySetValue(dict, kRouterHardwareAddress, data);
	    CFRelease(data);
	}
    }
    return (dict);
}

static void
DHCPLeasePrint(DHCPLeaseRef lease_p)
{
    printf("IP " IP_FORMAT " Start %d Length", 
	   IP_LIST(&lease_p->our_ip), (int)lease_p->lease_start);
    if (lease_p->lease_length == DHCP_INFINITE_LEASE) {
	printf(" infinite");
    }
    else {
	printf(" %d", (int)lease_p->lease_length);
    }

    if (lease_p->router_ip.s_addr != 0) {
	printf(" Router IP " IP_FORMAT, IP_LIST(&lease_p->router_ip));
	if (lease_p->router_hwaddr_length > 0) {
	    char	link_string[MAX_LINK_ADDR_LEN * 3];

	    link_addr_to_string(link_string, sizeof(link_string),
				lease_p->router_hwaddr,
				lease_p->router_hwaddr_length);
	    printf(" MAC %s", link_string);
	}
    }
    printf("\n");
}

static void
DHCPLeaseListPrint(DHCPLeaseListRef list_p)
{
    int		count;
    int		i;
    
    count = dynarray_count(list_p);
    printf("There are %d leases\n", count);
    for (i = 0; i < count; i++) {
	DHCPLeaseRef	lease_p = dynarray_element(list_p, i);
	printf("%d. ", i + 1);
	DHCPLeasePrint(lease_p);
    }
    return;
}

static bool
DHCPLeaseListGetPath(const char * ifname,
		     uint8_t cid_type, const void * cid, int cid_length,
		     char * filename, int filename_size)
{
    char *			idstr;
    char			idstr_scratch[128];
    
    idstr = identifierToStringWithBuffer(cid_type, cid, cid_length,
					 idstr_scratch, sizeof(idstr_scratch));
    if (idstr == NULL) {
	return (FALSE);
    }
    snprintf(filename, filename_size, DHCPCLIENT_LEASE_FILE_FMT, ifname,
	     idstr);
    if (idstr != idstr_scratch) {
	free(idstr);
    }
    return (TRUE);
}

void
DHCPLeaseListInit(DHCPLeaseListRef list_p)
{
    dynarray_init(list_p, free, NULL);
    return;
}

void
DHCPLeaseListFree(DHCPLeaseListRef list_p)
{
    dynarray_free(list_p);
}

/*
 * Function: DHCPLeaseListRemoveStaleLeases
 * Purpose:
 *   Scans the list of leases removing any that are no longer valid.
 */
static void
DHCPLeaseListRemoveStaleLeases(DHCPLeaseListRef list_p)
{
    int				count;
    absolute_time_t 		current_time;
    int				i;

    count = dynarray_count(list_p);
    if (count == 0) {
	return;
    }
    current_time = timer_current_secs();
    i = 0;
    while (i < count) {
	DHCPLeaseRef	lease_p = dynarray_element(list_p, i);

	/* check the lease expiration */
	if (lease_p->lease_length != DHCP_INFINITE_LEASE
	    && current_time >= (lease_p->lease_start + lease_p->lease_length)) {
	    /* lease is expired */
	    if (G_IPConfiguration_verbose) {
		my_log(LOG_NOTICE, "Removing Stale Lease "
		       IP_FORMAT " Router " IP_FORMAT,
		       IP_LIST(&lease_p->our_ip),
		       IP_LIST(&lease_p->router_ip));
	    }
	    dynarray_free_element(list_p, i);
	    count--;
	}
	else {
	    i++;
	}
    }
    return;
}

/* 
 * Function: DHCPLeaseListRead
 *
 * Purpose:
 *   Read the single DHCP lease for the given interface/client_id into a
 *   DHCPLeaseList structure.  This lease is marked as "tentative" because
 *   we have no idea whether the lease is still good or not, since another
 *   version of the OS (or another OS) could have had additional communication
 *   with the DHCP server, invalidating our notion of the lease.  It also
 *   affords a simple, self-cleansing mechanism to clear out the set of
 *   leases we keep track.
 */
void
DHCPLeaseListRead(DHCPLeaseListRef list_p,
		  const char * ifname,
		  uint8_t cid_type, const void * cid, int cid_length)
{
    char			filename[PATH_MAX];
    CFDictionaryRef		lease_dict = NULL;
    DHCPLeaseRef		lease_p;
    struct in_addr		lease_ip;

    DHCPLeaseListInit(list_p);
    if (DHCPLeaseListGetPath(ifname, cid_type, cid, cid_length,
			     filename, sizeof(filename)) == FALSE) {
	goto done;
    }
    lease_dict = my_CFPropertyListCreateFromFile(filename);
    if (isA_CFDictionary(lease_dict) == NULL) {
	goto done;
    }
    lease_p = DHCPLeaseCreateWithDictionary(lease_dict);
    if (lease_p == NULL) {
	goto done;
    }
    lease_p->tentative = TRUE;
    dynarray_add(list_p, lease_p);
    if (G_debug) {
	DHCPLeaseListPrint(list_p);
    }
    lease_ip = lease_p->our_ip;
    DHCPLeaseListRemoveStaleLeases(list_p);
    if (DHCPLeaseListCount(list_p) == 0) {
	remove_unused_ip(ifname, lease_ip);
    }

 done:
    my_CFRelease(&lease_dict);
    return;
}

/* 
 * Function: DHCPLeaseListWrite
 *
 * Purpose:
 *   Write the last DHCP lease in the list for the given interface/client_id.
 *   We only save the last (current) lease.  See the comments for 
 *   DHCPLeaseListRead above for more information.
 */
void
DHCPLeaseListWrite(DHCPLeaseListRef list_p,
		   const char * ifname,
		   uint8_t cid_type, const void * cid, int cid_length)
{
    int			count;
    char		filename[PATH_MAX];
    CFDictionaryRef	lease_dict;
    DHCPLeaseRef	lease_p;
    
    if (DHCPLeaseListGetPath(ifname, cid_type, cid, cid_length,
			     filename, sizeof(filename)) == FALSE) {
	return;
    }
    DHCPLeaseListRemoveStaleLeases(list_p);
    count = dynarray_count(list_p);
    if (count == 0) {
	unlink(filename);
	return;
    }
    lease_p = dynarray_element(list_p, count - 1);
    lease_dict = DHCPLeaseCopyDictionary(lease_p);
    dhcp_lease_init();
    if (my_CFPropertyListWriteFile(lease_dict, filename) < 0) {
	/*
	 * An ENOENT error is expected on a read-only filesystem.  All 
	 * other errors should be reported.
	 */
	if (errno != ENOENT) {
	    my_log(LOG_NOTICE, "my_CFPropertyListWriteFile(%s) failed, %s", 
		   filename, strerror(errno));
	}
    }
    my_CFRelease(&lease_dict);
    return;
}

/*
 * Function: DHCPLeaseListCopyARPAddressInfo
 * Purpose:
 *   Returns a list of arp_address_info_t's corresponding to each
 *   discoverable lease.
 */
arp_address_info_t *
DHCPLeaseListCopyARPAddressInfo(DHCPLeaseListRef list_p, bool tentative_ok,
				int * ret_count)
{
    int				arp_info_count;
    arp_address_info_t *	arp_info_p;
    int				count;
    int				i;
    arp_address_info_t *	info_p;

    DHCPLeaseListRemoveStaleLeases(list_p);
    count = dynarray_count(list_p);
    if (count == 0) {
	*ret_count = 0;
	return (NULL);
    }
    arp_info_p = (arp_address_info_t *)malloc(sizeof(*arp_info_p) * count);
    arp_info_count = 0;
    info_p = arp_info_p;
    for (i = 0; i < count; i++) {
	DHCPLeaseRef	lease_p = dynarray_element(list_p, i);

	if (lease_p->router_ip.s_addr == 0
	    || lease_p->router_hwaddr_length == 0) {
	    /* can't use this with ARP discovery */
	    if (G_IPConfiguration_verbose) {
		my_log(LOG_NOTICE, "ignoring lease for " IP_FORMAT,
		       IP_LIST(&lease_p->our_ip));
	    }
	    continue;
	}
	if (lease_p->tentative && tentative_ok == FALSE) {
	    /* ignore tentative lease */
	    continue;
	}
	info_p->sender_ip = lease_p->our_ip;
	info_p->target_ip = lease_p->router_ip;
	bcopy(lease_p->router_hwaddr, info_p->target_hardware,
	      lease_p->router_hwaddr_length);
	arp_info_count++;
	info_p++;
    }
    if (arp_info_count == 0) {
	free(arp_info_p);
	arp_info_p = NULL;
    }
    *ret_count = arp_info_count;
    return (arp_info_p);
}

/*
 * Function: DHCPLeaseListFindLease
 * Purpose:
 *   Find a lease corresponding to the supplied information.
 */
int
DHCPLeaseListFindLease(DHCPLeaseListRef list_p, struct in_addr our_ip,
		       struct in_addr router_ip,
		       const uint8_t * router_hwaddr, int router_hwaddr_length)
{
    int			count;
    int			i;
    bool		private_ip = ip_is_private(our_ip);

    count = dynarray_count(list_p);
    for (i = 0; i < count; i++) {
	DHCPLeaseRef	lease_p = dynarray_element(list_p, i);

	if (lease_p->our_ip.s_addr != our_ip.s_addr) {
	    /* IP doesn't match */
	    continue;
	}
	if (private_ip == FALSE) {
	    /* lease for public IP is unique */
	    return (i);
	}
	if (lease_p->router_ip.s_addr != router_ip.s_addr) {
	    /* router IP doesn't match (or one is set the other isn't)*/
	    continue;
	}
	if (router_ip.s_addr == 0) {
	    /* found lease with no router information */
	    return (i);
	}
	if (lease_p->router_hwaddr_length != router_hwaddr_length) {
	    /* one has router hwaddr, other doesn't */
	    continue;
	}
	if (router_hwaddr == NULL || router_hwaddr_length == 0) {
	    /* found lease with router IP but no router hwaddr */
	    return (i);
	}
	if (bcmp(lease_p->router_hwaddr, router_hwaddr, router_hwaddr_length)
	    == 0) {
	    /* exact match on IP, router IP, router hwaddr */
	    return (i);
	}
    }
    return (-1);
}

/*
 * Function: DHCPLeaseShouldBeRemoved
 * Purpose:
 *   Given an existing lease entry 'existing_p' and the one to be added 'new_p',
 *   determine whether the existing lease entry should be removed.
 *   
 *   The criteria for removing the lease entry:
 *   1) No router information is specified.  This entry is useless for lease
 *      detection.   If such a lease exists, it only makes sense to have
 *      one of them, the most recently used lease.  It will be added to the
 *      end of the list in that case.
 *   2) Lease was NAK'd.  The DHCP server NAK'd this lease and it's for the
 *      same network i.e. same router MAC.
 *   3) Lease is for a public IP and is the same as the new lease.
 *   4) Lease has the same router IP/MAC address as an existing lease.  We
 *      can only sensibly have a single lease for a particular network, so
 *      eliminate redundant ones.
 */
static boolean_t
DHCPLeaseShouldBeRemoved(DHCPLeaseRef existing_p, DHCPLeaseRef new_p, 
			 boolean_t private_ip)
{
    if (existing_p->router_ip.s_addr == 0
	|| existing_p->router_hwaddr_length == 0) {
	if (G_IPConfiguration_verbose) {
	    my_log(LOG_NOTICE,
		   "Removing lease with no router for IP address "
		   IP_FORMAT, IP_LIST(&existing_p->our_ip));
	}
	return (TRUE);
    }
    if (existing_p->nak) {
	boolean_t		ignore = FALSE;

	existing_p->nak = FALSE;
	if (ip_is_private(existing_p->our_ip) != private_ip) {
	    /* one IP is private, the other is public, ignore NAK */
	    ignore = TRUE;
	}
	else if (new_p->router_hwaddr_length != 0
		 && bcmp(existing_p->router_hwaddr, new_p->router_hwaddr,
			 existing_p->router_hwaddr_length) != 0) {
	    /* router MAC on NAK'd lease is different, so ignore NAK */
	    ignore = TRUE;
	}
	if (ignore) {
	    if (G_IPConfiguration_verbose) {
		my_log(LOG_NOTICE, "Ignoring NAK on IP address "
		       IP_FORMAT, IP_LIST(&existing_p->our_ip));
	    }
	}
	else {
	    if (G_IPConfiguration_verbose) {
		my_log(LOG_NOTICE, "Removing NAK'd lease for IP address "
		       IP_FORMAT, IP_LIST(&existing_p->our_ip));
	    }
	    return (TRUE);
	}
    }
    if (private_ip == FALSE
	&& new_p->our_ip.s_addr == existing_p->our_ip.s_addr) {
	/* public IP's are the same, remove it */
	if (G_IPConfiguration_verbose) {
	    my_log(LOG_NOTICE, "Removing lease for public IP address "
		   IP_FORMAT, IP_LIST(&existing_p->our_ip));
	}
	return (TRUE);
    }
    if (new_p->router_ip.s_addr == 0
	|| new_p->router_hwaddr_length == 0) {
	/* new lease doesn't have a router */
	return (FALSE);
    }
    if (bcmp(new_p->router_hwaddr, existing_p->router_hwaddr, 
	     new_p->router_hwaddr_length) == 0) {
	if (G_IPConfiguration_verbose) {
	    if (new_p->our_ip.s_addr == existing_p->our_ip.s_addr) {
		my_log(LOG_NOTICE,
		       "Removing lease with same router for IP address "
		       IP_FORMAT, IP_LIST(&existing_p->our_ip));
	    }
	    else {
		my_log(LOG_NOTICE,
		       "Removing lease with same router, old IP "
		       IP_FORMAT " new IP " IP_FORMAT,
		       IP_LIST(&existing_p->our_ip),
		       IP_LIST(&new_p->our_ip));
	    }
	}
	return (TRUE);
    }
    return (FALSE);
}

/* 
 * Function: DHCPLeaseListUpdateLease
 *
 * Purpose:
 *   Update the lease entry for the given lease in the in-memory lease database.
 */
void
DHCPLeaseListUpdateLease(DHCPLeaseListRef list_p, struct in_addr our_ip,
			 struct in_addr router_ip,
			 const uint8_t * router_hwaddr,
			 int router_hwaddr_length,
			 absolute_time_t lease_start,
			 dhcp_lease_time_t lease_length,
			 const uint8_t * pkt, int pkt_size)
{
    int			count;
    int			i;
    DHCPLeaseRef	lease_p;
    boolean_t		private_ip = ip_is_private(our_ip);

    lease_p = DHCPLeaseCreate(our_ip, router_ip,
			      router_hwaddr, router_hwaddr_length,
			      lease_start, lease_length, pkt, pkt_size);
    /* scan lease list to eliminate NAK'd, incomplete, and duplicate leases */
    count = dynarray_count(list_p);
    for (i = 0; i < count; i++) {
	DHCPLeaseRef	scan_p = dynarray_element(list_p, i);

	if (DHCPLeaseShouldBeRemoved(scan_p, lease_p, private_ip)) {
	    dynarray_free_element(list_p, i);
	    i--;
	    count--;
	}
    }
    if (G_IPConfiguration_verbose) {
	my_log(LOG_NOTICE, "Saving lease for " IP_FORMAT,
	       IP_LIST(&lease_p->our_ip));
    }
    dynarray_add(list_p, lease_p);
    return;
}

/* 
 * Function: DHCPLeaseListRemoveLease
 *
 * Purpose:
 *   Remove the lease entry for the given lease.
 */
void
DHCPLeaseListRemoveLease(DHCPLeaseListRef list_p,
			 struct in_addr our_ip,
			 struct in_addr router_ip,
			 const uint8_t * router_hwaddr,
			 int router_hwaddr_length)
{
    int		where;

    /* remove the old information if it's there */
    where = DHCPLeaseListFindLease(list_p, our_ip, router_ip,
				   router_hwaddr, router_hwaddr_length);
    if (where != -1) {
	if (G_IPConfiguration_verbose) {
	    DHCPLeaseRef lease_p = DHCPLeaseListElement(list_p, where);

	    my_log(LOG_NOTICE, "Removing lease for " IP_FORMAT,
		   IP_LIST(&lease_p->our_ip));
	}
	dynarray_free_element(list_p, where);
    }
    return;
}

/* 
 * Function: DHCPLeaseListRemoveAllButLastLease
 * Purpose:
 *   Remove all leases except the last one (the most recently used one),
 *   and mark it tentative.
 */
void
DHCPLeaseListRemoveAllButLastLease(DHCPLeaseListRef list_p)
{
    int			count;
    int			i;
    DHCPLeaseRef	lease_p;

    count = DHCPLeaseListCount(list_p);
    if (count == 0) {
	return;
    }
    for (i = 0; i < (count - 1); i++) {
	if (G_IPConfiguration_verbose) {
	    lease_p = DHCPLeaseListElement(list_p, 0);	    
	    my_log(LOG_NOTICE, "Removing lease #%d for IP address "
		   IP_FORMAT, i + 1, IP_LIST(&lease_p->our_ip));
	}
	dynarray_free_element(list_p, 0);
    }
    lease_p = DHCPLeaseListElement(list_p, 0);
    lease_p->tentative = TRUE;
    return;
}

