/*
 * Copyright (c) 1999-2021 Apple Inc. All rights reserved.
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
 * July 6, 2020 	Dieter Siegmund (dieter@apple.com)
 * - moved out of ipconfigd.c
 */
#include "wireless.h"
#include "util.h"
#include "cfutil.h"
#include "mylog.h"
#include "symbol_scope.h"

#ifndef TEST_WIRELESS
#include "my_darwin.h"
#endif

#include <CoreFoundation/CFRuntime.h>
#include <pthread.h>

struct WiFiInfo {
	CFRuntimeBase	cf_base;

	/*
	 * NOTE: if you add a field, add a line to initialize it.
	 */
	CFStringRef		ssid;
	CFStringRef		networkID;
	WiFiAuthType		auth_type;
	struct ether_addr	bssid;
};

/**
 ** CF object glue code
 **/
STATIC CFStringRef	__WiFiInfoCopyDebugDesc(CFTypeRef cf);
STATIC void		__WiFiInfoDeallocate(CFTypeRef cf);

STATIC CFTypeID __kWiFiInfoTypeID = _kCFRuntimeNotATypeID;

STATIC const CFRuntimeClass __WiFiInfoClass
= {
   0,	/* version */
   "WiFiInfo",			/* className */
   NULL,			/* init */
   NULL,			/* copy */
   __WiFiInfoDeallocate,	/* deallocate */
   NULL,			/* equal */
   NULL,			/* hash */
   NULL,			/* copyFormattingDesc */
   __WiFiInfoCopyDebugDesc	/* copyDebugDesc */
};

STATIC CFStringRef
__WiFiInfoCopyDebugDesc(CFTypeRef cf)
{
	CFAllocatorRef		allocator = CFGetAllocator(cf);
	WiFiAuthType		auth_type;
	char			bssid[LINK_ADDR_ETHER_STR_LEN];
	WiFiInfoRef		info_p = (WiFiInfoRef)cf;
	CFStringRef		networkID;
	CFMutableStringRef	str;

	str = CFStringCreateMutable(allocator, 0);
	auth_type = WiFiInfoGetAuthType(info_p);
	link_addr_to_string(bssid, sizeof(bssid),
			    (const uint8_t *)WiFiInfoGetBSSID(info_p),
			    ETHER_ADDR_LEN);
	STRING_APPEND(str,
		      "<WiFiInfo %p [%p]> { SSID \"%@\""
		      " BSSID %s Security %s",
		      cf, allocator,
		      WiFiInfoGetSSID(info_p),
		      bssid,
		      WiFiAuthTypeGetString(auth_type));
	networkID = WiFiInfoGetNetworkID(info_p);
	if (networkID != NULL) {
		STRING_APPEND(str, " NetworkID %@", networkID);
	}
	STRING_APPEND_STR(str, " }");
	return (str);
}


STATIC void
__WiFiInfoDeallocate(CFTypeRef cf)
{
	WiFiInfoRef 	info_p = (WiFiInfoRef)cf;

	my_CFRelease(&info_p->ssid);
	my_CFRelease(&info_p->networkID);
	return;
}

#ifndef NO_WIRELESS

STATIC void
__WiFiInfoInitialize(void)
{
	/* initialize runtime */
	__kWiFiInfoTypeID
		= _CFRuntimeRegisterClass(&__WiFiInfoClass);
	return;
}

STATIC void
__WiFiInfoRegisterClass(void)
{
	STATIC pthread_once_t	initialized = PTHREAD_ONCE_INIT;

	pthread_once(&initialized, __WiFiInfoInitialize);
	return;
}

STATIC WiFiInfoRef
__WiFiInfoAllocate(CFAllocatorRef allocator)
{
	WiFiInfoRef		info_p;

	__WiFiInfoRegisterClass();

	return ((WiFiInfoRef)
		_CFRuntimeCreateInstance(allocator,
					 __kWiFiInfoTypeID,
					 sizeof(*info_p) - sizeof(CFRuntimeBase),
					 NULL));
}

#endif /* NO_WIRELESS */

#ifdef NO_WIRELESS
PRIVATE_EXTERN const char *
WiFiAuthTypeGetString(WiFiAuthType auth_type)
{
	return ("unknown");
}

PRIVATE_EXTERN WiFiInfoRef
WiFiInfoCopy(CFStringRef ifname)
{
	return (NULL);
}

#else /* NO_WIRELESS */

#include <Apple80211/Apple80211API.h>
#include <Kernel/IOKit/apple80211/apple80211_ioctl.h>

#define _CASSERT(x)	_Static_assert(x, "compile-time assertion failed")

_CASSERT(kWiFiAuthTypeUnknown == APPLE80211_AUTHTYPE_UNKNOWN);
_CASSERT(kWiFiAuthTypeNone == APPLE80211_AUTHTYPE_NONE);

PRIVATE_EXTERN const char *
WiFiAuthTypeGetString(WiFiAuthType auth_type)
{
	const char * str;

	switch (auth_type) {
	case APPLE80211_AUTHTYPE_NONE:
		str = "NONE";
		break;
	case APPLE80211_AUTHTYPE_WPA:
		str = "WPA";
		break;
	case APPLE80211_AUTHTYPE_WPA_PSK:
		str = "WPA_PSK";
		break;
	case APPLE80211_AUTHTYPE_WPA2:
		str = "WPA2";
		break;
	case APPLE80211_AUTHTYPE_WPA2_PSK:
		str = "WPA2_PSK";
		break;
	case APPLE80211_AUTHTYPE_FT_PSK:
		str = "FT_PSK";
		break;
	case APPLE80211_AUTHTYPE_LEAP:
		str = "LEAP";
		break;
	case APPLE80211_AUTHTYPE_8021X:
		str = "8021X";
		break;
	case APPLE80211_AUTHTYPE_FT_8021X:
		str = "FT_8021X";
		break;
	case APPLE80211_AUTHTYPE_WPS:
		str = "WPS";
		break;
	case APPLE80211_AUTHTYPE_WAPI:
		str = "WAPI";
		break;
	case APPLE80211_AUTHTYPE_SHA256_PSK:
		str = "SHA256_PSK";
		break;
	case APPLE80211_AUTHTYPE_SHA256_8021X:
		str = "SHA256_8021X";
		break;
	case APPLE80211_AUTHTYPE_WPA3_SAE:
		str = "WPA3_SAE";
		break;
	case APPLE80211_AUTHTYPE_FT_SAE:
		str = "FT_SAE";
		break;
	case APPLE80211_AUTHTYPE_SHA384_8021X:
		str = "SHA384_8021X";
		break;
	case APPLE80211_AUTHTYPE_SHA384_FT_8021X:
		str = "SHA384_FT_8021X";
		break;
	case APPLE80211_AUTHTYPE_UNKNOWN:
		str = "UNKNOWN";
		break;
	default:
		str = "UNRECOGNIZED";
		break;
	}
	return (str);
}

static WiFiAuthType
get_wifi_auth_type(Apple80211Ref wref)
{
	uint32_t		auth_type;
	CFNumberRef		auth_type_cf;
	WiFiAuthType		auth_type_ret = kWiFiAuthTypeUnknown;
	Apple80211Err 		error;
	CFDictionaryRef		info = NULL;

	error = Apple80211CopyValue(wref, APPLE80211_IOC_AUTH_TYPE, NULL, &info);
	if (error != kA11NoErr) {
		my_log(LOG_NOTICE,
		       "Apple80211CopyValue(APPLE80211_IOC_AUTH_TYPE) failed"
		       ", 0x%x", error);
		goto done;
	}
	if (info == NULL) {
		goto done;
	}
	auth_type_cf = CFDictionaryGetValue(info, APPLE80211KEY_AUTH_UPPER);
	if (auth_type_cf == NULL) {
		uint32_t		auth_type_lower;
		CFNumberRef		auth_type_lower_cf;

		auth_type_lower_cf
			= CFDictionaryGetValue(info, APPLE80211KEY_AUTH_LOWER);
		if (auth_type_lower_cf == NULL) {
			goto done;
		}
		if (!CFNumberGetValue(auth_type_lower_cf, kCFNumberSInt32Type,
				      &auth_type_lower)) {
			goto done;
		}
		if (auth_type_lower == APPLE80211_AUTHTYPE_OPEN) {
			auth_type = kWiFiAuthTypeNone;
		}
		else {
			goto done;
		}
	}
	else if (!CFNumberGetValue(auth_type_cf,
				   kCFNumberSInt32Type, &auth_type)) {
		goto done;
	}
	auth_type_ret = auth_type;

 done:
	my_CFRelease(&info);
	return (auth_type_ret);
}

STATIC CFStringRef
copy_networkID(Apple80211Ref wref)
{
	Apple80211Err		error;
	CFMutableStringRef 	networkID;

	networkID = CFStringCreateMutable(NULL, 0);
	error = Apple80211Get(wref,
			      APPLE80211_IOC_COLOCATED_NETWORK_SCOPE_ID,
			      0,
			      (void *)networkID,
			      sizeof(networkID));
	if (error != kA11NoErr) {
		my_log(LOG_DEBUG,
		       "Apple80211Get(APPLE80211_IOC_COLOCATED_NETWORK_SCOPE_ID)"
		       "failed, 0x%x", error);
		my_CFRelease(&networkID);
	}
	return (networkID);
}

PRIVATE_EXTERN WiFiInfoRef
WiFiInfoCopy(CFStringRef ifname)
{
	struct ether_addr 	bssid;
	Apple80211Err		error;
	WiFiInfoRef		info_p = NULL;
	CFMutableDataRef	ssid;
	Apple80211Ref		wref = NULL;

	error = Apple80211Open(&wref);
	if (error != kA11NoErr) {
		my_log(LOG_NOTICE, "Apple80211Open failed, 0x%x", error);
		goto done;
	}
	error = Apple80211BindToInterface(wref, ifname);
	if (error != kA11NoErr) {
		goto done;
	}
	ssid = CFDataCreateMutable(NULL, 0);
	if ((Apple80211Get(wref, APPLE80211_IOC_SSID, 0, ssid, 0) == kA11NoErr)
	    && (Apple80211Get(wref, APPLE80211_IOC_BSSID, 0,
			      &bssid, sizeof(bssid)) == kA11NoErr)) {
		/* we have both the SSID and BSSID */
		info_p = __WiFiInfoAllocate(NULL);
		info_p->ssid = my_CFStringCreateWithData(ssid);
		info_p->bssid = bssid;
		info_p->auth_type = get_wifi_auth_type(wref);
		info_p->networkID = copy_networkID(wref);
	}
	CFRelease(ssid);

 done:
	if (wref != NULL) {
		Apple80211Close(wref);
	}
	return (info_p);
}

#endif /* NO_WIRELESS */

PRIVATE_EXTERN CFStringRef
WiFiInfoGetSSID(WiFiInfoRef w)
{
	return (w->ssid);
}

PRIVATE_EXTERN const struct ether_addr *
WiFiInfoGetBSSID(WiFiInfoRef w)
{
	return (&w->bssid);
}

PRIVATE_EXTERN WiFiAuthType
WiFiInfoGetAuthType(WiFiInfoRef w)
{
	return (w->auth_type);
}

PRIVATE_EXTERN CFStringRef
WiFiInfoGetNetworkID(WiFiInfoRef w)
{
	return (w->networkID);
}

PRIVATE_EXTERN const char *
WiFiInfoComparisonResultGetString(WiFiInfoComparisonResult result)
{
	static const char * str[] = {
				     "unknown",
				     "same network",
				     "different networks",
				     "different BSSIDs",
	};

	if (result < countof(str)) {
		return (str[result]);
	}
	return ("<invalid>");
}

INLINE bool
WiFiInfoNetworkIDsAreEqual(WiFiInfoRef info1, WiFiInfoRef info2)
{
	CFStringRef	networkID1 = WiFiInfoGetNetworkID(info1);
	CFStringRef	networkID2 = WiFiInfoGetNetworkID(info2);

	return (my_CFStringEqual(networkID1, networkID2));
}

INLINE bool
WiFiInfoSSIDsAreEqual(WiFiInfoRef info1, WiFiInfoRef info2)
{
	return (CFEqual(WiFiInfoGetSSID(info1),
			WiFiInfoGetSSID(info2)));
}

INLINE bool
WiFiInfoBSSIDsAreEqual(WiFiInfoRef info1, WiFiInfoRef info2)
{
	return (bcmp(WiFiInfoGetBSSID(info1),
		     WiFiInfoGetBSSID(info2),
		     sizeof(info1->bssid)) == 0);
}

PRIVATE_EXTERN WiFiInfoComparisonResult
WiFiInfoCompare(WiFiInfoRef info1, WiFiInfoRef info2)
{
	WiFiInfoComparisonResult	result;

	if (info1 == NULL || info2 == NULL) {
		if (info1 == info2) {
			/* both are NULL, can't say whether they are equal */
			result = kWiFiInfoComparisonResultUnknown;
		}
		else {
			result = kWiFiInfoComparisonResultNetworkChanged;
		}
	}
	else if (WiFiInfoSSIDsAreEqual(info1, info2)
		 || WiFiInfoNetworkIDsAreEqual(info1, info2)) {
		/* SSIDs or NetworkIDs match */
		if (!WiFiInfoBSSIDsAreEqual(info1, info2)) {
			result = kWiFiInfoComparisonResultBSSIDChanged;
		}
		else {
			result = kWiFiInfoComparisonResultSameNetwork;
		}
	}
	else {
		/* different networks */
		result = kWiFiInfoComparisonResultNetworkChanged;
	}
	return (result);
}

#if TEST_WIRELESS

STATIC WiFiInfoRef
WiFiInfoCreate(CFStringRef ssid, CFStringRef networkID,
	       const struct ether_addr * bssid)
{
	WiFiInfoRef		info_p;

	info_p = __WiFiInfoAllocate(NULL);
	CFRetain(ssid);
	if (networkID != NULL) {
		CFRetain(networkID);
	}
	info_p->ssid = ssid;
	info_p->networkID = networkID;
	info_p->bssid = *bssid;
	return (info_p);
}

#include <SystemConfiguration/SCPrivate.h>
#include <sysexits.h>

static void
usage(const char * progname)
{
	fprintf(stderr,
		"usage: %s ( -i <interface> | -t )\n", progname);
	exit(EX_USAGE);
	return;
}

const struct ether_addr ether_addr_one = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x1 };
const struct ether_addr ether_addr_two = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x2 };
const struct ether_addr ether_addr_three = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x3 };

static void
compare_info(const char * test, WiFiInfoRef info1, WiFiInfoRef info2,
	     WiFiInfoComparisonResult expected_result)
{
	WiFiInfoComparisonResult	result;

	result = WiFiInfoCompare(info1, info2);
	SCPrint(TRUE, stderr, CFSTR("%s: %@, %@\n"),
		WiFiInfoComparisonResultGetString(result),
		info1,
		info2);
	if (result != expected_result) {
		fprintf(stderr, "%s: FAIL\n", test);
		exit(1);
	}
	printf("%s: PASS\n", test);
	return;
}

static void
unit_test(void)
{
	WiFiInfoRef	info1;
	WiFiInfoRef	info2;
	WiFiInfoRef	info3;
	WiFiInfoRef	info4;
	WiFiInfoRef	info5;
	WiFiInfoRef	info6;

	info1 = WiFiInfoCreate(CFSTR("NetworkOne"), NULL, &ether_addr_one);
	info2 = WiFiInfoCreate(CFSTR("NetworkTwo"), NULL, &ether_addr_two);
	info3 = WiFiInfoCreate(CFSTR("NetworkOne"), NULL, &ether_addr_two);
	info4 = WiFiInfoCreate(CFSTR("NetworkFour"),
			       CFSTR("NetworkIDOne"),
			       &ether_addr_one);
	info5 = WiFiInfoCreate(CFSTR("NetworkFive"),
			       CFSTR("NetworkIDOne"),
			       &ether_addr_two);
	info6 = WiFiInfoCreate(CFSTR("NetworkSix"),
			       CFSTR("NetworkIDTwo"),
			       &ether_addr_three);

	/* NULL against NULL */
	compare_info("NULL networks", NULL, NULL,
		     kWiFiInfoComparisonResultUnknown);
	printf("\n");

	/* one NULL, other non-NULL */
	compare_info("NULL, non-NULL", NULL, info1,
		     kWiFiInfoComparisonResultNetworkChanged);
	printf("\n");
	compare_info("non-NULL, NULL", info1, NULL,
		     kWiFiInfoComparisonResultNetworkChanged);
	printf("\n");

	/* different SSIDs */
	compare_info("Different SSIDs", info1, info2,
		     kWiFiInfoComparisonResultNetworkChanged);
	printf("\n");

	/* different BSSIDs */
	compare_info("Roam", info1, info3,
		     kWiFiInfoComparisonResultBSSIDChanged);
	printf("\n");
	compare_info("Same NetworkID Roam", info4, info5,
		     kWiFiInfoComparisonResultBSSIDChanged);
	printf("\n");

	/* different SSID, NetworkIDs */
	compare_info("Different SSID/NetworkID", info5, info6,
		     kWiFiInfoComparisonResultNetworkChanged);
	printf("\n");

	/* different SSIDs, one NULL NetworkID */
	compare_info("Different SSID, one NetworkID NULL",
		     info1, info4, kWiFiInfoComparisonResultNetworkChanged);

	my_CFRelease(&info1);
	my_CFRelease(&info2);
	my_CFRelease(&info3);
	my_CFRelease(&info4);
	my_CFRelease(&info5);
	my_CFRelease(&info6);
	return;
}

int
main(int argc, char * argv[])
{
	int			ch;
	CFStringRef		ifname = NULL;
	const char *		progname = argv[0];
	bool			run_unit_test = false;

	while ((ch = getopt(argc, argv, "hi:t")) != EOF) {
		switch (ch) {
		case 'h':
			usage(progname);
			break;
		case 'i':
			if (ifname != NULL) {
				fprintf(stderr, "-i specified multiple times\n");
				usage(progname);
			}
			ifname = CFStringCreateWithCString(NULL, optarg,
							   kCFStringEncodingUTF8);
			break;
		case 't':
			run_unit_test = true;
			break;
		default:
			break;
		}
	}
	if (ifname != NULL) {
		WiFiInfoRef		info_p;

		info_p = WiFiInfoCopy(ifname);
		if (info_p != NULL) {
			WiFiAuthType	auth_type;
			char		bssid[LINK_ADDR_ETHER_STR_LEN];

			link_addr_to_string(bssid, sizeof(bssid),
					    WiFiInfoGetBSSID(info_p),
					    ETHER_ADDR_LEN);
			auth_type = WiFiInfoGetAuthType(info_p);
			SCPrint(TRUE, stdout,
				CFSTR("%@: SSID %@ BSSID %s Security %s"
				      " NetworkID %@\n"),
				ifname, WiFiInfoGetSSID(info_p),
				bssid,
				WiFiAuthTypeGetString(auth_type),
				WiFiInfoGetNetworkID(info_p));
			SCPrint(TRUE, stdout,
				CFSTR("%@: %@\n"), ifname, info_p);
		}
		else {
			printf("not associated\n");
		}
		my_CFRelease(&info_p);
	}
	else if (run_unit_test) {
		unit_test();
	}
	else {
		usage(progname);
	}
	exit(0);
	return (0);
}

#endif /* TEST_WIRELESS */
