
/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
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
 * wireless.c
 */

/* 
 * Modification History
 *
 * October 26, 2001	Dieter Siegmund (dieter@apple)
 * - created
 */

#ifndef NO_WIRELESS
#include <Apple80211/Wireless.h>
#include <Apple80211/Apple80211.h>
#include <CoreFoundation/CFString.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <net/ethernet.h>

#define WIRELESS_MODE_NORMAL	1
#define WIRELESS_MODE_AP	2
#define WIRELESS_MODE_IBSS	4

#include "wireless.h"

static boolean_t
S_wireless_find(const struct ether_addr * client_mac, WirelessRef * wref_p)
{
    WirelessError	error;
    boolean_t		found = FALSE;
    int			i;
    WirelessInfo2	winfo2;

#if 0
    /* this is what I should be able to code, but can't because of 3170019 */
    // for (i = 0; TRUE; i++) { 
#endif
    for (i = 0; i < 1; i++) {
	error = WirelessAttach(wref_p, i);
	if (error != errWirelessNoError) {
	    if (i == 0) {
		fprintf(stderr, "WirelessAttach failed, %x\n", error);
	    }
	    break;
	}
	error = WirelessGetInfo2(*wref_p, &winfo2);
	if (error != errWirelessNoError) {
	    fprintf(stderr, "WirelessGetInfo2 failed, %x\n", error);
	}
	else if (bcmp(winfo2.macAddress, client_mac, sizeof(*client_mac)) 
		 == 0) {
	    found = TRUE;
	    break;
	}
	WirelessDetach(*wref_p);
    }
    return (found);
}

boolean_t
wireless_find(const struct ether_addr * client_mac, wireless_t * wref_p)
{
    return (S_wireless_find(client_mac, (WirelessRef *)wref_p));
}

boolean_t
wireless_ap_mac(const wireless_t wref, struct ether_addr * AP_mac)
{
    WirelessError	error;
    struct ether_addr	no_ap = { {0x44, 0x44, 0x44, 0x44, 0x44, 0x44 } };
    WirelessInfo2	winfo2;
    boolean_t		valid = FALSE;

    error = WirelessGetInfo2(wref, &winfo2);
    if (error != errWirelessNoError) {
	fprintf(stderr, "WirelessGetInfo2 failed, %x\n", error);
	return (FALSE);
    }
    if (winfo2.info1.portType == WIRELESS_MODE_NORMAL
	&& bcmp(winfo2.info1.bssID, &no_ap, sizeof(no_ap)) != 0) {
	*AP_mac = *((struct ether_addr *)winfo2.info1.bssID);
	valid = TRUE;
    }
    return (valid);
}

boolean_t
wireless_set_key(const wireless_t wref, wirelessKeyType type, 
		 int index, const uint8_t * key, int key_length)
{
    WirelessError	error;
    boolean_t		ret = TRUE;

    error = WirelessSetKey((WirelessRef)wref, type, index, key_length, 
			   (uint8_t *)key);
    if (error != errWirelessNoError) {
	fprintf(stderr, "wireless_set_key: WirelessSetKey failed, %x\n",
		error);
	ret = FALSE;
    }
    return (ret);
}

boolean_t
wireless_set_wpa_session_key(const wireless_t wref, 
			     const uint8_t * key, int key_length)
{
    WirelessError	error;
    boolean_t		ret = TRUE;

    error = WirelessSetWPAKey((WirelessRef)wref, kWPAKeyTypeSession, 
			      key_length, (uint8_t *)key);
    if (error != errWirelessNoError) {
	fprintf(stderr, 
		"wireless_set_key: WirelessSetWPAKey session key failed, %x\n",
		error);
	ret = FALSE;
    }
    return (ret);
}

boolean_t
wireless_set_wpa_server_key(const wireless_t wref, 
			    const uint8_t * key, int key_length)
{
    WirelessError	error;
    boolean_t		ret = TRUE;

    error = WirelessSetWPAKey((WirelessRef)wref, kWPAKeyTypeServer, 
			      key_length, (uint8_t *)key);
    if (error != errWirelessNoError) {
	fprintf(stderr, 
		"wireless_set_key: WirelessSetWPAKey server key failed, %x\n",
		error);
	ret = FALSE;
    }
    return (ret);
}

void
wireless_free(wireless_t wref)
{
    WirelessDetach(wref);
    return;
}


#ifdef TEST_WIRELESS

void
wireless_disassociate(const wireless_t wref)
{
    WirelessError	error;
    error = WirelessDisassociate((WirelessRef)wref);
    if (error != errWirelessNoError) {
	fprintf(stderr, 
		"wireless_disassociate: WirelessDisassociate failed, %x\n",
		error);
    }
    return;
}

static boolean_t
S_wireless_first(WirelessRef * wref_p, struct ether_addr * client_mac)
{
    WirelessError	error = errWirelessNoError;
    WirelessInfo2	winfo2;

    error = WirelessAttach(wref_p, 0);
    if (error != errWirelessNoError) {
	return (FALSE);
    }
    error = WirelessGetInfo2(*wref_p, &winfo2);
    if (error != errWirelessNoError) {
	fprintf(stderr, "WirelessGetInfo2 failed, %x\n", error);
	WirelessDetach(*wref_p);
	return (FALSE);
    }
    *client_mac = *((struct ether_addr *)winfo2.macAddress);
    return (TRUE);
}

static boolean_t
wireless_first(wireless_t * wref_p, struct ether_addr * client_mac)
{
    return (S_wireless_first((WirelessRef *)wref_p, client_mac));
}

static boolean_t
wireless_join(wireless_t wref, CFDataRef ssid, WirelessJoinType join_type)
{
    WirelessError	error;
    boolean_t		ret = TRUE;

    error = WirelessAssociate((WirelessRef)wref, join_type, ssid, NULL);
    if (error != errWirelessNoError) {
	fprintf(stderr, "wireless_join: WirelessAssociate failed, %x\n",
		error);
	ret = FALSE;
    }
    return (ret);
    
}

static void
hexstrtobin(const char * hexstr, int hexlen, uint8_t * bin, int bin_len)
{
    int		i;
    int		j;
    char	tmp[3];

    tmp[2] = '\0';
    for (i = 0, j = 0; i < hexlen && j < bin_len; i += 2, j++) {
	tmp[0] = hexstr[i];
	tmp[1] = hexstr[i + 1];
	bin[j] = (uint8_t)strtoul(tmp, NULL, 16);
    }
    return;
}

int
main(int argc, char * argv[])
{
    struct ether_addr	AP_mac;
    int			ch;
    boolean_t		has_wireless;
    boolean_t		disassociate = FALSE;
    const char *	key_str = NULL;
    const char *	network = NULL;
    struct ether_addr	wireless_mac;
    wireless_t		wref;

    if (wireless_first(&wref, &wireless_mac) == FALSE) {
	printf("no AirPort card\n");
	exit(0);
    }
    printf("AirPort %s\n", ether_ntoa(&wireless_mac));
    while ((ch =  getopt(argc, argv, "dk:x:")) != EOF) {
	switch ((char)ch) {
	case 'x':		/* join 802.1x network */
	    network = optarg;
	    break;
	case 'k':		/* set the wireless key */
	    key_str = optarg;
	    break;
	case 'd':
	    disassociate = TRUE;
	    break;
	default:
	    break;
	}
    }

    if (wireless_ap_mac(wref, &AP_mac) == FALSE) {
	printf("Not associated\n");
    }
    else {
	printf("Access Point %s\n", ether_ntoa(&AP_mac));
	if (disassociate) {
	    wireless_disassociate(wref);
	    goto done;
	}
    }
    if (key_str) {
	uint8_t	key[13];
	int	key_len;
	int	hex_len = strlen(key_str);
	
	if (hex_len & 0x1) {
	    fprintf(stderr, "invalid key, odd number of hex bytes\n");
	    exit(1);
	}
	key_len = hex_len / 2;
	
	switch (key_len) {
	case 5:
	case 13:
	    hexstrtobin(key_str, hex_len, key, key_len);
	    if (wireless_set_key(wref, 0, 0, key, key_len)
		== FALSE) {
		fprintf(stderr, "wireless_set_key failed\n");
	    }
	    break;
	default:
	    fprintf(stderr, 
		    "invalid key length %d,"
		    " must be 5 or 13 hex bytes\n", key_len);
	    exit(1);
	    break;
	}
    }
    else if (network != NULL) {
	CFDataRef	ssid;
	ssid = CFDataCreateWithBytesNoCopy(NULL, (const UInt8 *)network,
					   strlen(network), kCFAllocatorNull);
	fprintf(stderr, "attempting to join 802.1x network '%s'\n", network);
	if (wireless_join(wref, ssid, eJoinWPA_Unspecified/* eJoin8021X */) == FALSE) {
	    fprintf(stderr, "wireless_join failed\n");
	}
    }
 done:
    wireless_free(wref);
    exit(0);
    return (0);
}

#endif TEST_WIRELESS
#endif NO_WIRELESS
