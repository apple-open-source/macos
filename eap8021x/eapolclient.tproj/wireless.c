
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
#include <Apple80211/Apple80211API.h>
#include <Apple80211/Apple80211IE.h>
#include <CoreFoundation/CFString.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <net/ethernet.h>

#include <sys/socket.h>
#include <Kernel/IOKit/apple80211/apple80211_ioctl.h>

#include "wireless.h"

boolean_t
wireless_bind(const char * if_name, wireless_t * wref_p)
{
    Apple80211Err	error;
    boolean_t		found = FALSE;
    CFStringRef		if_name_cf;
    Apple80211Ref	wref;

    error = Apple80211Open(&wref);
    if (error != kA11NoErr) {
	fprintf(stderr, "Apple80211Open failed, %x\n", error);
	return (FALSE);
    }
    if_name_cf = CFStringCreateWithCString(NULL, if_name,
					   kCFStringEncodingASCII);
    error = Apple80211BindToInterface(wref, if_name_cf);
    CFRelease(if_name_cf);
    if (error == kA11NoErr) {
	*wref_p = (wireless_t *)wref;
	found = TRUE;
    }
    else {
	fprintf(stderr, "Apple80211BindToInterface %s failed, %x\n",
		if_name, error);
	Apple80211Close(wref);
	*wref_p = NULL;
    }
    return (found);
}


boolean_t
wireless_ap_mac(const wireless_t wref, struct ether_addr * AP_mac)
{
    const struct ether_addr	no_ap = { {0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

    if (Apple80211Get((Apple80211Ref)wref, APPLE80211_IOC_BSSID, 0, 
		      AP_mac, sizeof(*AP_mac)) != kA11NoErr) {
	return (FALSE);
    }
    /* Work around for Atheros: <rdar://problem/5255595> */
    if (memcmp(AP_mac, &no_ap, sizeof(no_ap)) == 0) {
	return (FALSE);
    }
    return (TRUE);
}

boolean_t
wireless_set_key(const wireless_t wref, wirelessKeyType type, 
		 int index, const uint8_t * key, int key_length)
{
    struct apple80211_key 	akey;

    bzero(&akey, sizeof(akey));
    akey.version = APPLE80211_VERSION;

    /* validate the type */
    switch (type) {
    case kKeyTypeIndexedTx:
	akey.key_flags = (APPLE80211_KEY_FLAG_TX
			  | APPLE80211_KEY_FLAG_UNICAST);
	break;
    case kKeyTypeIndexedRx:
	akey.key_flags = (APPLE80211_KEY_FLAG_RX
			  | APPLE80211_KEY_FLAG_UNICAST);
	break;
    case kKeyTypeMulticast:
	akey.key_flags = APPLE80211_KEY_FLAG_MULTICAST;
	break;
    case kKeyTypeDefault:
	akey.key_flags = APPLE80211_KEY_FLAG_UNICAST;
	break;
    default:
	return (FALSE);
    }

    /* validate the length */
    switch (key_length) {
    case 5:
	akey.key_cipher_type = APPLE80211_CIPHER_WEP_40;
	break;
    case 13:
	akey.key_cipher_type = APPLE80211_CIPHER_WEP_104;
	break;
    default:
	return (FALSE);
    }

    /* set the key */
    memcpy(akey.key, key, key_length);
    akey.key_len = (uint32_t)key_length;
    akey.key_index = (uint16_t)index;
    return (Apple80211Set((Apple80211Ref)wref, APPLE80211_IOC_CIPHER_KEY, 0, 
			  &akey, sizeof(akey)) == kA11NoErr);
}

boolean_t
wireless_set_wpa_session_key(const wireless_t wref, 
			     const uint8_t * key, int key_length)
{
    struct apple80211_key 	akey;

    /* validate the key_length */
    if (key_length > 32 || key_length < 0) {
	return (FALSE);
    }
    bzero(&akey, sizeof(akey));
    akey.version = APPLE80211_VERSION;
    if (key_length != 0) {
	/* copy the session key, zero-padding to 32 bytes */
	memcpy(akey.key, key, key_length);
	key_length = 32;
    }
    akey.key_len = (u_int32_t)key_length;
    akey.key_cipher_type = APPLE80211_CIPHER_PMK;
    return (Apple80211Set((Apple80211Ref)wref, APPLE80211_IOC_CIPHER_KEY, 0, 
			  &akey, sizeof(akey)) == kA11NoErr);
}

void
wireless_free(wireless_t wref)
{
    Apple80211Close((Apple80211Ref)wref);
    return;
}


#ifdef TEST_WIRELESS

#include <EAP8021X/LinkAddresses.h>
#include "myCFUtil.h"

void
wireless_disassociate(const wireless_t wref)
{
    Apple80211Err	error;
    error = Apple80211Disassociate( (Apple80211Ref)wref );
    if (error != kA11NoErr) {
	fprintf(stderr, 
		"wireless_disassociate: WirelessDisassociate failed, %x\n",
		error);
    }
    return;
}

static char *
wireless_first(wireless_t * wref_p)
{
    int			count;
    Apple80211Err	error;
    int			i;
    CFArrayRef		if_name_list;
    char *		ret_name = NULL;
    Apple80211Ref	wref;

    error = Apple80211Open(&wref);
    if (error != kA11NoErr) {
	fprintf(stderr, "Apple80211Open failed, %x\n", error);
	return (NULL);
    }
    error = Apple80211GetIfListCopy(wref, &if_name_list);
    if (error != kA11NoErr) {
	fprintf(stderr, "Apple80211GetIfListCopy failed, %x\n", error);
	goto done;
    }
    count = CFArrayGetCount(if_name_list);
    if (count > 0) {
	CFStringRef	if_name;

	if_name = CFArrayGetValueAtIndex(if_name_list, 0);
	error = Apple80211BindToInterface(wref, if_name);
	if (error != kA11NoErr) {
	    fprintf(stderr, "Apple80211BindToInterface failed, %x\n",
		    error);
	}
	else {
	    ret_name = my_CFStringToCString(if_name, kCFStringEncodingASCII);
	}
    }
    CFRelease(if_name_list);

 done:
    if (ret_name == NULL) {
	Apple80211Close(wref);
	*wref_p = NULL;
    }
    else {
	*wref_p = wref;
    }
    return (ret_name);
}

static boolean_t
wireless_join(wireless_t wref, CFDataRef ssid)
{
    Apple80211Err 		error;
    boolean_t			ret = FALSE;
    CFMutableDictionaryRef 	scan_args = NULL;
    CFArrayRef 			scan_result = NULL;
    CFStringRef 		ssid_str;
	
    ssid_str = CFStringCreateWithBytes(NULL, CFDataGetBytePtr(ssid),
				       CFDataGetLength(ssid),
				       kCFStringEncodingUTF8, FALSE);
    scan_args = CFDictionaryCreateMutable(NULL, 0,
					 &kCFTypeDictionaryKeyCallBacks,
					 &kCFTypeDictionaryValueCallBacks );
    CFDictionarySetValue(scan_args, APPLE80211KEY_SCAN_SSID, ssid_str);
    CFRelease(ssid_str);
    error = Apple80211Scan((Apple80211Ref)wref, &scan_result, scan_args);
    CFRelease(scan_args);
    if (error != kA11NoErr) {
	fprintf(stderr, "Apple80211Scan failed, %d\n", error);
	return (FALSE);
    }
    if (CFArrayGetCount(scan_result) > 0) {
	CFDictionaryRef 	scan_dict;

	scan_dict = CFArrayGetValueAtIndex(scan_result, 0);
	error = Apple80211Associate((Apple80211Ref)wref, scan_dict, NULL);
	if (error == kA11NoErr) {
	    ret = TRUE;
	}
	else {
	    fprintf(stderr, "Apple80211Associate failed, %d\n", error);
	    CFShow(scan_dict);
	}
    }
    if (scan_result != NULL) {
	CFRelease(scan_result);
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

static boolean_t
get_mac_address(const char * if_name, struct sockaddr_dl * ret)
{
    struct sockaddr_dl *	dl_p;
    boolean_t			found = FALSE;
    LinkAddressesRef		list;

    list = LinkAddresses_create();
    if (list != NULL) {
	dl_p = LinkAddresses_lookup(list, (char *)if_name);
	if (dl_p != NULL) {
	    *ret = *dl_p;
	    found = TRUE;
	}
	LinkAddresses_free(&list);
    }
    return (found);
}

int
main(int argc, char * argv[])
{
    struct ether_addr	AP_mac;
    int			ch;
    boolean_t		disassociate = FALSE;
    const char *	if_name = NULL;
    boolean_t		has_wireless;
    const char *	key_str = NULL;
    const char *	network = NULL;
    struct sockaddr_dl	w;
    wireless_t		wref;

    while ((ch =  getopt(argc, argv, "dhHi:k:x:")) != EOF) {
	switch ((char)ch) {
	case 'h':
	case 'H':
	    fprintf(stderr,
		    "usage: wireless [ -i <interface> ] ( -d | -k | -x <ssid> )\n");
	    exit(0);
	    break;
	case 'x':		/* join network */
	    network = optarg;
	    break;
	case 'k':		/* set the wireless key */
	    key_str = optarg;
	    break;
	case 'd':
	    disassociate = TRUE;
	    break;
	case 'i':		/* specify the interface */
	    if_name = optarg;
	    break;
	default:
	    break;
	}
    }

    if (if_name != NULL) {
	if (wireless_bind(if_name, &wref) == FALSE) {
	    printf("interface '%s' is not present or not AirPort\n",
		   if_name);
	    exit(1);
	}
    }
    else if ((if_name = wireless_first(&wref)) == NULL) {
	printf("no AirPort card\n");
	exit(0);
    }
    get_mac_address(if_name, &w);
    printf("AirPort %.*s %s\n", w.sdl_nlen, w.sdl_data,
	   ether_ntoa((struct ether_addr *)(w.sdl_data + w.sdl_nlen)));

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
	fprintf(stderr, "attempting to join network '%s'\n", network);
	if (wireless_join(wref, ssid) == FALSE) {
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
