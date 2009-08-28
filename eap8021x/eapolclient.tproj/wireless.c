
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
 * wireless.c
 */

/* 
 * Modification History
 *
 * October 26, 2001	Dieter Siegmund (dieter@apple)
 * - created
 */

#include <TargetConditionals.h>

#ifndef TEST_WIRELESS
#if ! TARGET_OS_EMBEDDED
#include "my_darwin.h"
#endif /* TARGET_OS_EMBEDDED */
#endif TEST_WIRELESS

#include <SystemConfiguration/SCValidation.h>

#ifndef NO_WIRELESS
#include <Apple80211/Apple80211API.h>
#include <Apple80211/Apple80211IE.h>
#include <CoreFoundation/CFString.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <net/ethernet.h>
#include <sys/queue.h>

#include <sys/socket.h>
#include <Kernel/IOKit/apple80211/apple80211_ioctl.h>

#include "myCFUtil.h"
#include "wireless.h"

struct scanCallbackEntry_s;
typedef struct scanCallbackEntry_s scanCallbackEntry, * scanCallbackEntryRef;

typedef TAILQ_HEAD(scanCallbackHead_s, scanCallbackEntry_s) scanCallbackHead;

#if 0
static __inline
not_used() {};
/* this is here so that emacs indent doesn't get confused - what a pain*/
#endif 0
typedef struct scanCallbackHead_s * scanCallbackHeadRef;
static scanCallbackHead 	S_head = TAILQ_HEAD_INITIALIZER(S_head);
static scanCallbackHeadRef 	S_scanCallbackHead_p = &S_head;

enum {
    kScanCallbackStateNone	= 0,
    kScanCallbackStateStarted	= 1,
    kScanCallbackStateComplete	= 2
};

struct scanCallbackEntry_s {
    Apple80211Ref			wref;
    wireless_scan_callback_t		func;
    void *				arg;
    CFStringRef				ssid;
    uint32_t				state;
    TAILQ_ENTRY(scanCallbackEntry_s)	link;
};

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
	*wref_p = (wireless_t)wref;
	found = TRUE;
    }
    else {
#ifdef TEST_WIRELESS
	fprintf(stderr, "Apple80211BindToInterface %s failed, %x\n",
		if_name, error);
#endif /* TEST_WIRELESS */
	Apple80211Close(wref);
	*wref_p = NULL;
    }
    return (found);
}

#if TARGET_OS_EMBEDDED
static __inline__ boolean_t
get_ap_address(Apple80211Ref wref, struct ether_addr * AP_mac)
{
    CFMutableStringRef 		ea_str_cf;
    boolean_t			ret = FALSE;

    ea_str_cf = CFStringCreateMutable(NULL, 0);
    if (Apple80211Get(wref, APPLE80211_IOC_BSSID, 0, ea_str_cf, 0) 
	== kA11NoErr) {
	char * ea_str;

	ea_str = my_CFStringToCString(ea_str_cf, kCFStringEncodingASCII);
	if (ea_str != NULL) {
	    struct ether_addr * 	ret_ea = ether_aton(ea_str);
	    if (ret_ea != NULL) {
		*AP_mac = *ret_ea;
		ret = TRUE;
	    }
	    free(ea_str);
	}
    }
    CFRelease(ea_str_cf);
    return (ret);
}
#endif

boolean_t
wireless_ap_mac(wireless_t wref, struct ether_addr * AP_mac)
{
    const struct ether_addr	no_ap = { {0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };


#if TARGET_OS_EMBEDDED
    if (get_ap_address((Apple80211Ref)wref, AP_mac) == FALSE) {
	return (FALSE);
    }
#else
    if (Apple80211Get((Apple80211Ref)wref, APPLE80211_IOC_BSSID, 0, 
		      AP_mac, sizeof(*AP_mac)) != kA11NoErr) {
	return (FALSE);
    }
#endif

    /* Work around for Atheros: <rdar://problem/5255595> */
    if (memcmp(AP_mac, &no_ap, sizeof(no_ap)) == 0) {
	return (FALSE);
    }
    return (TRUE);
}

boolean_t
wireless_set_key(wireless_t wref, wirelessKeyType type, 
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
wireless_is_wpa_enterprise(wireless_t wref)
{
    Apple80211Err 	error;
    CFDictionaryRef	info = NULL;
    boolean_t		is_wpa_enterprise = FALSE;
    CFNumberRef		upper_cf;
    uint32_t		upper;	

    error = Apple80211CopyValue((Apple80211Ref)wref, APPLE80211_IOC_AUTH_TYPE,
				NULL, &info);
    if (error != kA11NoErr) {
	fprintf(stderr,
		"Apple80211CopyValue(APPLE80211_IOC_AUTH_TYPE) failed, 0x%x\n",
		error);
	goto done;
    }
    if (info == NULL) {
	goto done;
    }
    upper_cf = CFDictionaryGetValue(info, APPLE80211KEY_AUTH_UPPER);
    if (upper_cf == NULL) {
	goto done;
    }
    if (CFNumberGetValue(upper_cf, kCFNumberSInt32Type, &upper) == FALSE) {
	goto done;
    }
    switch (upper) {
    case APPLE80211_AUTHTYPE_WPA:
    case APPLE80211_AUTHTYPE_WPA2:
	is_wpa_enterprise = TRUE;
	break;
    default:
	break;
    }

 done:
    my_CFRelease(&info);
    return (is_wpa_enterprise);
}

boolean_t
wireless_set_wpa_pmk(wireless_t wref, 
		     const struct ether_addr * bssid,
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
    if (bssid != NULL) {
	akey.key_cipher_type = APPLE80211_CIPHER_PMKSA;
	akey.key_ea = *bssid;
    }
    else {
	akey.key_cipher_type = APPLE80211_CIPHER_PMK;
    }
    return (Apple80211Set((Apple80211Ref)wref, APPLE80211_IOC_CIPHER_KEY, 0, 
			  &akey, sizeof(akey)) == kA11NoErr);
}

void
wireless_free(wireless_t wref)
{
    wireless_scan_cancel(wref);
    Apple80211Close((Apple80211Ref)wref);
    return;
}

static CFDataRef
wireless_copy_ssid_data(wireless_t wref)
{
    CFMutableDataRef	ssid;

    ssid = CFDataCreateMutable(kCFAllocatorDefault, 0);
    if (Apple80211Get((Apple80211Ref)wref, APPLE80211_IOC_SSID, 0, 
		      ssid, 0) != kA11NoErr) {
	CFRelease(ssid);
	return (NULL);
    }
    return ((CFDataRef)ssid);
}

static CFStringRef
ssid_string_from_data(CFDataRef data)
{
    CFStringRef		ssid_str;

    ssid_str = CFStringCreateWithBytes(NULL,
				       CFDataGetBytePtr(data),
				       CFDataGetLength(data),
				       kCFStringEncodingUTF8,
				       FALSE);
    if (ssid_str == NULL) {
	ssid_str = CFStringCreateWithBytes(NULL,
					   CFDataGetBytePtr(data),
					   CFDataGetLength(data),
					   kCFStringEncodingMacRoman,
					   FALSE);
    }
    return (ssid_str);
}

CFStringRef
wireless_copy_ssid_string(wireless_t wref)
{
    CFDataRef		ssid;
    CFStringRef		ssid_str;

    ssid = wireless_copy_ssid_data(wref);
    if (ssid == NULL) {
	return (NULL);
    }
    ssid_str = ssid_string_from_data(ssid);
    CFRelease(ssid);
    return (ssid_str);
}

/**
 ** scanning
 **/
static CFDataRef
bssid_string_to_data(CFStringRef bssid)
{
    struct ether_addr * 	eaddr;
    char			val[32];

    if (CFStringGetCString(bssid, val, sizeof(val), kCFStringEncodingASCII)
	== FALSE) {
	return (NULL);
    }
    eaddr = ether_aton(val);
    if (eaddr == NULL) {
	return (NULL);
    }
    return (CFDataCreate(NULL, (const UInt8 *)eaddr, sizeof(*eaddr)));
}

static CFArrayRef
copy_bssid_list_from_scan(CFArrayRef scan_result, CFStringRef ssid)
{
    CFMutableArrayRef		bssid_list = NULL;
    int				count;
    int				i;
    CFRange			range;

    count = CFArrayGetCount(scan_result);
    if (count == 0) {
	goto failed;
    }
    bssid_list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    range.location = 0;
    range.length = 0;
    for (i = 0; i < count; i++) {
	CFStringRef		bssid;
	CFDataRef		bssid_data;
	CFDictionaryRef		dict = CFArrayGetValueAtIndex(scan_result, i);
	CFStringRef		this_ssid;

	this_ssid = CFDictionaryGetValue(dict, APPLE80211KEY_SSID_STR);
	if (this_ssid == NULL || !CFEqual(this_ssid, ssid)) {
	    continue;
	}
	bssid = CFDictionaryGetValue(dict, APPLE80211KEY_BSSID);
	if (isA_CFString(bssid) == NULL) {
	    continue;
	}
	bssid_data = bssid_string_to_data(bssid);
	if (bssid_data != NULL) {
	    if (!CFArrayContainsValue(bssid_list, range, bssid_data)) {
		CFArrayAppendValue(bssid_list, bssid_data);
		range.length++;
	    }
	    CFRelease(bssid_data);
	}
    }
    if (CFArrayGetCount(bssid_list) == 0) {
	CFRelease(bssid_list);
	bssid_list = NULL;
    }
 failed:
    return (bssid_list);
}

CFDictionaryRef
make_scan_args(CFStringRef ssid, int num_scans)
{
    uint32_t			n;
    CFMutableDictionaryRef 	scan_args;
    CFNumberRef			val;

    scan_args = CFDictionaryCreateMutable(NULL, 0,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(scan_args, APPLE80211KEY_SCAN_SSID, ssid);
    CFDictionarySetValue(scan_args, APPLE80211KEY_SCAN_MERGE, 
			 kCFBooleanFalse);
    n = num_scans;
    val = CFNumberCreate(NULL, kCFNumberSInt32Type, &n);
    CFDictionarySetValue(scan_args, APPLE80211KEY_SCAN_NUM_SCANS, val);
    CFRelease(val);

    n = APPLE80211_MODE_AUTO;
    val = CFNumberCreate(NULL, kCFNumberSInt32Type, &n);
    CFDictionarySetValue(scan_args, APPLE80211KEY_SCAN_PHY_MODE, val);
    CFRelease(val);

    n = APPLE80211_SCAN_TYPE_ACTIVE;
    val = CFNumberCreate(NULL, kCFNumberSInt32Type, &n);
    CFDictionarySetValue(scan_args, APPLE80211KEY_SCAN_TYPE, val);
    CFRelease(val);
    return (scan_args);
}

static scanCallbackEntryRef
scanCallbackEntryFind(Apple80211Ref wref)
{
    scanCallbackEntryRef	scan;

    TAILQ_FOREACH(scan, S_scanCallbackHead_p, link) {
	if (scan->wref == wref) {
	    return (scan);
	}
    }
    return (NULL);
}

#ifdef APPLE80211KEY_SCAN_BSSID_LIST
static void
scanCallbackEvent(Apple80211Err err, Apple80211Ref wref,
		  UInt32 event, void * eventData, UInt32 eventDataLen,
		  void * context)
#else /* APPLE80211KEY_SCAN_BSSID_LIST */
static void
scanCallbackEvent(Apple80211Err err, Apple80211Ref wref,
		  UInt8 event, void * eventData, void * context)
#endif /* APPLE80211KEY_SCAN_BSSID_LIST */
{
    void *			arg;
    CFArrayRef			bssid_list;
    scanCallbackEntryRef	callback;
    wireless_scan_callback_t	func;

    switch (event) {
    case APPLE80211_M_SCAN_DONE:
	callback = scanCallbackEntryFind(wref);
	if (callback == NULL) {
	    fprintf(stderr, "%s: no callback?\n",
		    __FUNCTION__);
	    break;
	}
	func = callback->func;
	arg = callback->arg;
	callback->state = kScanCallbackStateComplete;
	bssid_list = copy_bssid_list_from_scan(eventData, callback->ssid);
	(*func)((wireless_t)wref, bssid_list, arg);
	if (bssid_list != NULL) {
	    CFRelease(bssid_list);
	}
	break;
    default:
	fprintf(stderr, "%s: unexpected event %d",
		__FUNCTION__, (int)event);
	break;
    }
    return;
}

void
wireless_scan_cancel(wireless_t this_wref)
{
    scanCallbackEntryRef	callback;
    Apple80211Ref 		wref = (Apple80211Ref)this_wref;

    callback = scanCallbackEntryFind(wref);
    if (callback == NULL) {
	return;
    }
    TAILQ_REMOVE(S_scanCallbackHead_p, callback, link);
    /* (void)Apple80211StopMonitoringEvent(wref, APPLE80211_M_SCAN_DONE); */
    (void)Apple80211EventMonitoringHalt(wref);
    CFRelease(callback->ssid);
    free(callback);
    return;
}

boolean_t
wireless_scan(wireless_t this_wref, CFStringRef ssid, int num_scans,
	      wireless_scan_callback_t func, void * arg)
{
    scanCallbackEntryRef	callback;
    Apple80211Err		error;
    bool			ret = TRUE;
    CFDictionaryRef	 	scan_args;
    Apple80211Ref 		wref = (Apple80211Ref)this_wref;

    if (ssid == NULL || func == NULL) {
	fprintf(stderr, "%s: ssid and/or func NULL\n",
		    __FUNCTION__);
	return (FALSE);
    }
    callback = scanCallbackEntryFind(wref);
    if (callback == NULL) {
#ifdef APPLE80211KEY_SCAN_BSSID_LIST
	error = Apple80211EventMonitoringInit(wref, scanCallbackEvent,
					      NULL, CFRunLoopGetCurrent());
#else /* APPLE80211KEY_SCAN_BSSID_LIST */
	error = Apple80211EventMonitoringInit(wref, scanCallbackEvent,
					      NULL);
#endif /* APPLE80211KEY_SCAN_BSSID_LIST */
	if (error != kA11NoErr) {
	    fprintf(stderr,
		    "%s: Apple80211EventMonitoringInit failed, %d\n",
		    __FUNCTION__,
		    error);
	    ret = FALSE;
	    goto done;
	}
	error = Apple80211StartMonitoringEvent(wref, APPLE80211_M_SCAN_DONE);
	if (error != kA11NoErr) {
	    (void)Apple80211EventMonitoringHalt(wref);
	    fprintf(stderr,
		    "%s: Apple80211StartMonitoringEvent failed, %d\n", 
		    __FUNCTION__,
		    error);
	    ret = FALSE;
	    goto done;
	}
	/* create a callback entry, and add it to the list */
	callback = (scanCallbackEntryRef)malloc(sizeof(*callback));
	bzero(callback, sizeof(*callback));
	callback->wref = wref;
	callback->ssid = CFRetain(ssid);
	TAILQ_INSERT_TAIL(S_scanCallbackHead_p, callback, link);
    }
    else if (!CFEqual(callback->ssid, ssid)) {
	CFRetain(ssid);
	CFRelease(callback->ssid);
	callback->ssid = ssid;
	callback->state = kScanCallbackStateNone;
    }
    callback->func = func;
    callback->arg = arg;
    if (callback->state == kScanCallbackStateStarted) {
	/* there's already a scan for this SSID pending */
	fprintf(stderr, 
		"%s: scan already in progress\n",
		__FUNCTION__);
	goto done;
    }
    scan_args = make_scan_args(ssid, num_scans);
    error = Apple80211ScanAsync((Apple80211Ref)wref, scan_args);
    CFRelease(scan_args);
    if (error != kA11NoErr) {
	fprintf(stderr,
		"%s: Apple80211ScanAsync failed, %d\n", 
		__FUNCTION__, error);
	goto done;
    }
    callback->state = kScanCallbackStateStarted;

 done:
    return (ret);
}

#ifdef TEST_WIRELESS

#include <EAP8021X/LinkAddresses.h>

static CFArrayRef
wireless_scan_ssid(wireless_t wref, CFStringRef ssid)
{
    CFArrayRef			bssid_list = NULL;
    Apple80211Err		error;
    CFDictionaryRef	 	scan_args;
    CFArrayRef 			scan_result = NULL;

    scan_args = make_scan_args(ssid, 1);
    error = Apple80211Scan((Apple80211Ref)wref, &scan_result, scan_args);
    CFRelease(scan_args);
    if (error != kA11NoErr) {
	fprintf(stderr, "Apple80211Scan failed, %d\n", error);
	goto failed;
    }
    bssid_list = copy_bssid_list_from_scan(scan_result, ssid);
    if (bssid_list == NULL) {
	fprintf(stderr, "No scan results\n");
    }
    else {
	CFShow(bssid_list);
    }
    fflush(stdout);
    fflush(stderr);

 failed:
    my_CFRelease(&scan_result);
    return (bssid_list);

}

static bool
wireless_async_scan_ssid(wireless_t this_wref, CFStringRef ssid);

static void
async_scan_callback(wireless_t wref, CFArrayRef bssid_list, void * arg)
{
    if (bssid_list == NULL) {
	fprintf(stderr, "No scan results\n");
    }
    else {
	CFShow(bssid_list);
    }
    return;
}

static bool
wireless_async_scan_ssid(wireless_t this_wref, CFStringRef ssid)
{
    return (wireless_scan(this_wref, ssid, 1, async_scan_callback, (void *)ssid));
}

void
wireless_disassociate(wireless_t wref)
{
#if TARGET_OS_EMBEDDED
    Apple80211Set((Apple80211Ref)wref, APPLE80211_IOC_DISASSOCIATE, 0, 0, 0);
#else
    Apple80211Err	error;

    error = Apple80211Disassociate( (Apple80211Ref)wref );
    if (error != kA11NoErr) {
	fprintf(stderr, 
		"wireless_disassociate: WirelessDisassociate failed, %x\n",
		error);
    }
#endif
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
	*wref_p = (wireless_t)wref;
    }
    return (ret_name);
}

static boolean_t
wireless_join(wireless_t wref, CFStringRef ssid)
{
    Apple80211Err 		error;
    boolean_t			ret = FALSE;
    CFMutableDictionaryRef 	scan_args = NULL;
    CFArrayRef 			scan_result = NULL;
	
    scan_args = CFDictionaryCreateMutable(NULL, 0,
					 &kCFTypeDictionaryKeyCallBacks,
					 &kCFTypeDictionaryValueCallBacks );
    CFDictionarySetValue(scan_args, APPLE80211KEY_SCAN_SSID, ssid);
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
	if (error != kA11NoErr) {
	    error = Apple80211Associate((Apple80211Ref)wref,
					scan_dict, CFSTR("1234567890"));
	}
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

#if 0
#include <sys/sockio.h>

/* get the ssid without linking against AirPort framework */
static CFStringRef
wireless_ssid_ioc(const char * if_name)
{
    struct apple80211req	req;
    int				s;
    uint8_t			ssid[APPLE80211_MAX_SSID_LEN];
    CFStringRef			ssid_str = NULL;
    
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == -1) {
	return (NULL);
    }
    bzero(&req, sizeof(req));
    req.req_len = APPLE80211_MAX_SSID_LEN;
    req.req_data = ssid;
    req.req_type = APPLE80211_IOC_SSID;
    strlcpy(req.req_if_name, if_name, sizeof(req.req_if_name));

    if (ioctl(s, SIOCGA80211, (char *)&req) < 0) {
	perror("SIOCGA80211");
	goto done;
    }
    ssid_str = CFStringCreateWithBytes(NULL, ssid, req.req_len,
				       kCFStringEncodingUTF8,
				       FALSE);
    if (ssid_str == NULL) {
	ssid_str = CFStringCreateWithBytes(NULL, ssid, req.req_len,
					   kCFStringEncodingMacRoman,
					   FALSE);
    }
    
 done:
    close(s);
    return (ssid_str);
}
#endif 0

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

struct ssid_wref {
    CFStringRef		ssid;
    wireless_t		wref;
};

static void
before_blocking(CFRunLoopObserverRef observer, 
		CFRunLoopActivity activity, void *info)
{
    scanCallbackEntryRef	callback;
    struct ssid_wref *		ref = (struct ssid_wref *)info;

    callback = scanCallbackEntryFind((Apple80211Ref)ref->wref);
    if (callback->state == kScanCallbackStateComplete) {
	printf("scan is complete, starting another scan\n");
	wireless_scan(ref->wref, ref->ssid, 1, async_scan_callback,
		      (void *)ref->ssid);
    }
    else {
	printf("before blocking\n");
    }
    return;
}

int
main(int argc, char * argv[])
{
    int			a_flag = 0;
    struct ether_addr	AP_mac;
    int			ch;
    boolean_t		disassociate = FALSE;
    const char *	if_name = NULL;
    boolean_t		has_wireless;
    const char *	key_str = NULL;
    const char *	network = NULL;
    int			scan_current_ssid = FALSE;
    struct sockaddr_dl	w;
    wireless_t		wref;

    while ((ch =  getopt(argc, argv, "adhHi:k:sx:")) != EOF) {
	switch ((char)ch) {
	case 'a':
	    a_flag = 1;
	    break;
	case 'h':
	case 'H':
	    fprintf(stderr,
		    "usage: wireless [ -i <interface> ] ( -d | -k | -x <ssid> | -s [ -a ])\n");
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
	case 's':
	    scan_current_ssid = TRUE;
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
    printf("AirPort: %s %s\n", if_name,
	   ether_ntoa((struct ether_addr *)(w.sdl_data + w.sdl_nlen)));

    if (wireless_ap_mac(wref, &AP_mac) == FALSE) {
	printf("Not associated\n");
    }
    else {
	CFStringRef	ssid;

	printf("Access Point: %s\n", ether_ntoa(&AP_mac));
	ssid = wireless_copy_ssid_string(wref);
	if (ssid != NULL) {
	    printf("SSID: ");
	    fflush(stdout);
	    CFShow(ssid);
	    fflush(stderr);
	}
	if (wireless_is_wpa_enterprise(wref) == TRUE) {
	    printf("WPA Enterprise\n");
	}
	else {
	    printf("Not WPA Enterprise\n");
	}
	if (disassociate) {
	    wireless_disassociate(wref);
	    goto done;
	}
	else if (scan_current_ssid) {
	    if (a_flag) {
		if (wireless_async_scan_ssid(wref, ssid)) {
		    CFRunLoopObserverContext	context 
			= { 0, NULL, NULL, NULL, NULL };
		    CFRunLoopObserverRef	observer;
		    struct ssid_wref		ref;


		    ref.ssid = ssid;
		    ref.wref = wref;
		    context.info = &ref;
		    observer 
			= CFRunLoopObserverCreate(NULL,
						  kCFRunLoopBeforeWaiting,
						  TRUE, 0, before_blocking,
						  &context);
		    if (observer != NULL) {
			CFRunLoopAddObserver(CFRunLoopGetCurrent(), observer, 
					     kCFRunLoopDefaultMode);
		    }
		    else {
			fprintf(stderr, "start_initialization: "
				"CFRunLoopObserverCreate failed!");
		    }
		    CFRunLoopRun();
		}
		else {
		    exit(1);
		}
	    }
	    else {
		wireless_scan_ssid(wref, ssid);
	    }
	}
	my_CFRelease(&ssid);
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
	CFDataRef	data;
	CFStringRef	ssid_str;

	data = CFDataCreateWithBytesNoCopy(NULL, (const UInt8 *)network,
					   strlen(network), kCFAllocatorNull);
	ssid_str = ssid_string_from_data(data);
	fprintf(stderr, "attempting to join network '%s'\n", network);
	if (wireless_join(wref, ssid_str) == FALSE) {
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
