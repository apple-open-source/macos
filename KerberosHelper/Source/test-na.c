/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
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

#include "NetworkAuthenticationHelper.h"
#include "KerberosHelper.h"

#include <err.h>

static void
lkdc_classic(void)
{
    CFArrayRef array;
    CFIndex n;
    NAHRef na;

    CFShow(CFSTR("lkdc_classic"));

    na = NAHCreate(NULL, CFSTR("localhost.local"), CFSTR("host"), NULL);
    if (na == NULL)
	errx(1, "NACreate");

    array = NAHGetSelections(na);

    for (n = 0; n < CFArrayGetCount(array); n++)
	CFShow(CFArrayGetValueAtIndex(array, n));

    CFRelease(na);
}

uint8_t token[] =
    "\x60\x66\x06\x06\x2b\x06\x01\x05\x05\x02\xa0\x5c"
    "\x30\x5a\xa0\x2c\x30\x2a\x06\x09\x2a\x86\x48\x82"
    "\xf7\x12\x01\x02\x02\x06\x09\x2a\x86\x48\x86\xf7"
    "\x12\x01\x02\x02\x06\x0a\x2b\x06\x01\x04\x01\x82"
    "\x37\x02\x02\x0a\x06\x06\x2b\x05\x01\x05\x02\x07"
    "\xa3\x2a\x30\x28\xa0\x26\x1b\x24\x6e\x6f\x74\x5f"
    "\x64\x65\x66\x69\x6e\x65\x64\x5f\x69\x6e\x5f\x52"
    "\x46\x43\x34\x31\x37\x38\x40\x70\x6c\x65\x61\x73"
    "\x65\x5f\x69\x67\x6e\x6f\x72\x65";

static void
lkdc_wellknown(void)
{
    CFMutableDictionaryRef info;
    CFDictionaryRef negToken;
    CFArrayRef array;
    CFDataRef serverToken;
    CFIndex n;
    NAHRef na;

    CFShow(CFSTR("lkdc_wellknown"));

    serverToken = CFDataCreateWithBytesNoCopy(NULL, 
					      token, sizeof(token) - 1,
					      kCFAllocatorNull);


    negToken = KRBDecodeNegTokenInit(NULL, serverToken);
    CFRelease(serverToken);

    info = CFDictionaryCreateMutable(NULL,
				     0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);

    CFDictionaryAddValue(info, kNAHNegTokenInit, negToken);
    CFRelease(negToken);


    na = NAHCreate(NULL, CFSTR("localhost.local"), kNAHServiceAFPServer, info);
    CFRelease(info);
    if (na == NULL)
	errx(1, "NACreate");

    array = NAHGetSelections(na);

    for (n = 0; n < CFArrayGetCount(array); n++)
	CFShow(CFArrayGetValueAtIndex(array, n));

    CFRelease(na);
}

static void
test_ntlm(void)
{
    CFMutableDictionaryRef info;
    CFDictionaryRef negToken;
    CFArrayRef array;
    CFIndex n;
    NAHRef na;

    CFShow(CFSTR("test_ntlm"));

    negToken = KRBCreateNegTokenLegacyNTLM(NULL);
    if (negToken == NULL)
	errx(1, "KRBCreateNegTokenLegacyNTLM");

    info = CFDictionaryCreateMutable(NULL,
				     0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);

    CFDictionaryAddValue(info, kNAHNegTokenInit, negToken);
    CFRelease(negToken);


    na = NAHCreate(NULL, CFSTR("localhost.local"), CFSTR("host"), info);
    CFRelease(info);
    if (na == NULL)
	errx(1, "NACreate");

    array = NAHGetSelections(na);

    for (n = 0; n < CFArrayGetCount(array); n++) {
	NAHSelectionRef s = (void *)CFArrayGetValueAtIndex(array, n);
	CFShow(s);

	if (CFBooleanGetValue(NAHSelectionGetInfoForKey(s, kNAHUseSPNEGO)))
	    errx(1, "not raw NTLM");
    }

    CFRelease(na);
}

static void
kerberos_classic(CFStringRef hostname,
		 CFStringRef service,
		 CFStringRef user,
		 CFStringRef password)
{
    CFMutableDictionaryRef info;
    CFDictionaryRef negToken;
    CFArrayRef array;
    CFIndex n;
    NAHRef na;

    CFShow(CFSTR("test KRBCreateNegTokenLegacyKerberos"));

    negToken = KRBCreateNegTokenLegacyKerberos(NULL);
    if (negToken == NULL)
	errx(1, "KRBCreateNegTokenLegacyKerberos");

    CFShow(CFSTR("kerberos_classic"));

    info = CFDictionaryCreateMutable(NULL,
				     0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);

    CFDictionaryAddValue(info, kNAHUserName, user);
    CFDictionaryAddValue(info, kNAHPassword, password);

    CFDictionaryAddValue(info, kNAHNegTokenInit, negToken);
    CFRelease(negToken);


    na = NAHCreate(NULL, hostname, service, info);
    if (na == NULL)
	errx(1, "NAHCreate");

    array = NAHGetSelections(na);

    for (n = 0; n < CFArrayGetCount(array); n++) {
	NAHSelectionRef s = (void *)CFArrayGetValueAtIndex(array, n);
	CFErrorRef error = NULL;
	Boolean ret;

	ret = NAHSelectionAcquireCredential(s, NULL, &error);
	printf("NAHSelectionAcquireCredential: %s\n", ret ? "yes" : "no");
	CFShow(s);
	if (error)
	    CFShow(error);

	if (ret) {
	    CFDictionaryRef authinfo = NAHSelectionCopyAuthInfo(s);
	    if (authinfo) {
		CFShow(CFSTR("Authinfo"));
		CFShow(authinfo);
		CFRelease(authinfo);
	    }

	    /* if NAHCopyReferenceKey returns NULL, refcounting not supported */

	    CFStringRef refinfo = NAHCopyReferenceKey(s);
		
	    CFStringRef label = CFSTR("fs:/testing");

	    NAHAddReferenceAndLabel(s, label);
	    
	    NAHCredRemoveReference(refinfo);
		
	    NAHCredAddReference(refinfo);
	    NAHCredRemoveReference(refinfo);
	    
	    if (refinfo)
		CFRelease(refinfo);

	    /* should delete */
	    NAHFindByLabelAndRelease(label);
	}
    }

    CFRelease(na);

    CFRelease(info);
}

/*
 *
 */

static void
check_valid(bool expected, bool found, const char *check)
{
    if (expected ^ found)
	errx(1, "%s doesn't have expected result: %s %sfound",
	     check, check, found ? "" : "not ");
}


static void
verify_result(bool expect_mech_kerberos,
	      bool expect_ntlm,
	      bool expect_iakerb,
	      bool expect_classic_kerberos,
	      bool expect_classic_lkdc,
	      bool expect_wlkdc,
	      bool expect_raw_kerberos, 
	      CFStringRef service,
	      CFStringRef hostname,
	      CFStringRef username,
	      CFStringRef password,
	      void *spnego,
	      size_t len,
	      CFDictionaryRef negToken)
{
    CFDataRef serverToken = CFDataCreate(NULL, spnego, len);
    CFMutableDictionaryRef info;
    NAHRef na;
    
    info = CFDictionaryCreateMutable(NULL,
				     0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    
    if (spnego) {
	negToken = KRBDecodeNegTokenInit(NULL, serverToken);
	CFRelease(serverToken);
	if (negToken == NULL)
	    errx(1, "failed to decode NegTokenInit");
	
	CFShow(negToken);

	CFDictionaryAddValue(info, kNAHNegTokenInit, negToken);
	CFRelease(negToken);
    } else if (negToken) {
	CFShow(negToken);
	CFDictionaryAddValue(info, kNAHNegTokenInit, negToken);
    }

    
    
    if (username)
	CFDictionaryAddValue(info, kNAHUserName, username);
    if (password)
	CFDictionaryAddValue(info, kNAHPassword, password);
    
    na = NAHCreate(NULL, hostname, service, info);
    if (info)
	CFRelease(info);
    if (na == NULL)
	errx(1, "NAHCreate");

    CFArrayRef r = NAHGetSelections(na);

    CFIndex n, num = CFArrayGetCount(r);

    bool found_wlkdc = false,
	found_classic_lkdc = false,
	found_kerberos = false,
	found_raw_kerberos = false,
	found_mech_kerberos = false,
	found_mech_ntlm = false,
	found_mech_iakerb = false;

    CFShow(hostname);

    for (n = 0; n < num; n++) {
	NAHSelectionRef s = (NAHSelectionRef)CFArrayGetValueAtIndex(r, n);
	CFStringRef name;
	CFRange range1, range2;
	
	CFShow(s);
	
	name = NAHSelectionGetInfoForKey(s, kNAHMechanism);
	if (CFStringCompare(name, CFSTR("Kerberos"), 0) == kCFCompareEqualTo) {
	    found_raw_kerberos = true;
	}

	name = NAHSelectionGetInfoForKey(s, kNAHInnerMechanism);
	if (CFStringCompare(name, CFSTR("Kerberos"), 0) == kCFCompareEqualTo) {
	    found_mech_kerberos = true;
	    
	    name = NAHSelectionGetInfoForKey(s, kNAHClientPrincipal);
	    
	    range1 = CFStringFind(name, CFSTR("WELLKNOWN:COM.APPLE.LKDC"), 0);
	    range2 = CFStringFind(name, CFSTR("@LKDC:SHA1"), 0);
	    if (range1.location != kCFNotFound)
		found_wlkdc = true;
	    else if (range2.location != kCFNotFound)
		found_classic_lkdc = true;
	    else
		found_kerberos = true;
	    
	} else if (CFStringCompare(name, CFSTR("IAKerb"), 0) == kCFCompareEqualTo) {
	    found_mech_iakerb = true;
	} else if (CFStringCompare(name, CFSTR("NTLM"), 0) == kCFCompareEqualTo) {
	    found_mech_ntlm = true;
	}
    }
    
    check_valid(expect_mech_kerberos, found_mech_kerberos, "kerberos mech");
    check_valid(expect_ntlm, found_mech_ntlm, "ntlm mech");
    check_valid(expect_iakerb, found_mech_iakerb, "iakerb");
    check_valid(expect_classic_kerberos, found_kerberos, "kerberos classic");
    check_valid(expect_classic_lkdc, found_classic_lkdc, "lkdc classic mech");
    check_valid(expect_wlkdc, found_wlkdc, "wellknown lkdc");
    check_valid(expect_raw_kerberos, found_raw_kerberos, "raw kerberos");

    
    CFRelease(na);
}


uint8_t win2k8[108] = 
    "\x60\x6a\x06\x06\x2b\x06\x01\x05\x05\x02\xa0\x60\x30\x5e\xa0\x30"
    "\x30\x2e\x06\x09\x2a\x86\x48\x82\xf7\x12\x01\x02\x02\x06\x09\x2a"
    "\x86\x48\x86\xf7\x12\x01\x02\x02\x06\x0a\x2a\x86\x48\x86\xf7\x12"
    "\x01\x02\x02\x03\x06\x0a\x2b\x06\x01\x04\x01\x82\x37\x02\x02\x0a"
    "\xa3\x2a\x30\x28\xa0\x26\x1b\x24\x6e\x6f\x74\x5f\x64\x65\x66\x69"
    "\x6e\x65\x64\x5f\x69\x6e\x5f\x52\x46\x43\x34\x31\x37\x38\x40\x70"
    "\x6c\x65\x61\x73\x65\x5f\x69\x67\x6e\x6f\x72\x65";

uint8_t snowleopard[] = 
    "\x60\x81\xa6\x06\x06\x2b\x06\x01\x05\x05\x02\xa0\x81\x9b\x30\x81"
    "\x98\xa0\x24\x30\x22\x06\x09\x2a\x86\x48\x86\xf7\x12\x01\x02\x02"
    "\x06\x09\x2a\x86\x48\x82\xf7\x12\x01\x02\x02\x06\x0a\x2b\x06\x01"
    "\x04\x01\x82\x37\x02\x02\x0a\xa3\x70\x30\x6e\xa0\x6c\x1b\x6a\x63"
    "\x69\x66\x73\x2f\x4c\x4b\x44\x43\x3a\x53\x48\x41\x31\x2e\x35\x38"
    "\x46\x43\x45\x33\x36\x44\x42\x44\x42\x44\x38\x36\x41\x45\x37\x30"
    "\x31\x30\x39\x33\x42\x34\x42\x31\x44\x37\x39\x41\x44\x44\x30\x37"
    "\x30\x35\x44\x30\x30\x42\x40\x4c\x4b\x44\x43\x3a\x53\x48\x41\x31"
    "\x2e\x35\x38\x46\x43\x45\x33\x36\x44\x42\x44\x42\x44\x38\x36\x41"
    "\x45\x37\x30\x31\x30\x39\x33\x42\x34\x42\x31\x44\x37\x39\x41\x44"
    "\x44\x30\x37\x30\x35\x44\x30\x30\x42";

uint8_t lion[] =
    "\x60\x7e\x06\x06\x2b\x06\x01\x05\x05\x02\xa0\x74\x30\x72\xa0\x44"
    "\x30\x42\x06\x09\x2a\x86\x48\x82\xf7\x12\x01\x02\x02\x06\x09\x2a"
    "\x86\x48\x86\xf7\x12\x01\x02\x02\x06\x06\x2a\x85\x70\x2b\x0e\x03"
    "\x06\x06\x2b\x06\x01\x05\x05\x0e\x06\x0a\x2b\x06\x01\x04\x01\x82"
    "\x37\x02\x02\x0a\x06\x06\x2b\x05\x01\x05\x02\x07\x06\x06\x2b\x06"
    "\x01\x05\x02\x05\xa3\x2a\x30\x28\xa0\x26\x1b\x24\x6e\x6f\x74\x5f"
    "\x64\x65\x66\x69\x6e\x65\x64\x5f\x69\x6e\x5f\x52\x46\x43\x34\x31"
    "\x37\x38\x40\x70\x6c\x65\x61\x73\x65\x5f\x69\x67\x6e\x6f\x72\x65";

int
main(int argc, char **argv)
{
    /* validate expected behaviors */
    
    system("/System/Library/PrivateFrameworks/Heimdal.framework/Helpers/gsstool destroy --all");

    /*
     * Basic tests
     */
    
    lkdc_classic();
    lkdc_wellknown();
    test_ntlm();

    kerberos_classic(CFSTR("host.ads.apple.com"), CFSTR("host"),
		     CFSTR("ktestuser"), CFSTR("foobar"));

    kerberos_classic(CFSTR("host.ads.apple.com"), CFSTR("host"),
		     CFSTR("ktestuser@ADS.APPLE.COM"), CFSTR("foobar"));

    system("/System/Library/PrivateFrameworks/Heimdal.framework/Helpers/gsstool destroy --all");

    /* windows 2008 server */
    verify_result(true, true, false, true, false, false, false, CFSTR("cifs"), CFSTR("host.ads.apple.com"), CFSTR("user"), CFSTR("password"), win2k8, sizeof(win2k8), NULL);
    verify_result(false, false, false, false, false, false, false, CFSTR("cifs"), CFSTR("host.ads.apple.com"), NULL, NULL, win2k8, sizeof(win2k8), NULL);
    
    /* snowleopard SMB */
    verify_result(true, true, false, true, false, false, false, CFSTR("cifs"), CFSTR("nutcracker.apple.com"), CFSTR("user"), CFSTR("password"), snowleopard, sizeof(snowleopard), NULL);
    verify_result(false, false, false, false, false, false, false, CFSTR("cifs"), CFSTR("nutcracker.apple.com"), NULL, NULL, snowleopard, sizeof(snowleopard), NULL);
    /* next two tests only really for N.N. when his laptop is turned on.... :-/ */
#if 0
    verify_result(true, true, false, false, true, false, false, CFSTR("cifs"), CFSTR("nutcracker.local"), CFSTR("user"), CFSTR("password"), snowleopard, sizeof(snowleopard), NULL);
    verify_result(true, true, false, false, true, false, false, CFSTR("cifs"), CFSTR("nutcracker.bitcollector.members.mac.com"), CFSTR("user"), CFSTR("password"), snowleopard, sizeof(snowleopard), NULL);
#endif

    /* snowleopard VNC */
    CFShow(CFSTR("VNC - snow leopard"));
    verify_result(true, false, false, true, false, true, false, CFSTR("vnc"), CFSTR("nutcracker.apple.com"), CFSTR("user"), CFSTR("password"), NULL, 0, NULL);
    verify_result(false, false, false, false, false, false, false, CFSTR("vnc"), CFSTR("nutcracker.apple.com"), NULL, NULL, NULL, 0, NULL);
#if 0
    verify_result(true, false, false, false, true, true, false, CFSTR("vnc"), CFSTR("nutcracker.local"), CFSTR("user"), CFSTR("password"), NULL, 0, NULL);
    verify_result(true, false, false, false, true, true, false, CFSTR("vnc"), CFSTR("nutcracker.bitcollector.members.mac.com"), CFSTR("user"), CFSTR("password"), NULL, 0, NULL);
#endif

    CFShow(CFSTR("afpserver - No initial packet - snow leopard"));
    verify_result(true, false, false, true, false, false, true, CFSTR("afpserver"), CFSTR("nutcracker.apple.com"), CFSTR("user"), CFSTR("password"), NULL, 0, NULL);
#if 0
    verify_result(true, false, false, true, true, false, false, CFSTR("afpserver"), CFSTR("nutcracker.local"), CFSTR("user"), CFSTR("password"), NULL, 0, NULL);
#endif
    verify_result(false, false, false, false, false, false, false, CFSTR("afpserver"), CFSTR("nutcracker.apple.com"), NULL, NULL, NULL, 0, NULL);
#if 0
    verify_result(false, false, false, false, true, false, false, CFSTR("afpserver"), CFSTR("nutcracker.local"), NULL, NULL, NULL, 0, NULL);
#endif

    CFShow(CFSTR("afpserver - Initial packet without IAKERB or SupportLKDC - 3rd party"));
    verify_result(true, false, false, true, false, false, true, CFSTR("afpserver"), CFSTR("nutcracker.apple.com"), CFSTR("user"), CFSTR("password"), snowleopard, sizeof(snowleopard), NULL);
    verify_result(false, false, false, false, false, false, false, CFSTR("afpserver"), CFSTR("nutcracker.apple.com"), NULL, NULL, snowleopard, sizeof(snowleopard), NULL);

    CFShow(CFSTR("afpserver - Initial packet with IAKERB or SupportLKDC - lion"));
    verify_result(true, false, true, true, false, false, false, CFSTR("afpserver"), CFSTR("nutcracker.apple.com"), CFSTR("user"), CFSTR("password"), lion, sizeof(lion), NULL);
    verify_result(false, false, false, false, false, false, false, CFSTR("afpserver"), CFSTR("nutcracker.apple.com"), NULL, NULL, lion, sizeof(lion), NULL);
#if 0
    verify_result(true, false, false, false, true, true, false, CFSTR("afpserver"), CFSTR("nutcracker.local"), CFSTR("user"), CFSTR("password"), NULL, 0, NULL);
    verify_result(true, false, false, false, true, true, false, CFSTR("afpserver"), CFSTR("nutcracker.bitcollector.members.mac.com"), CFSTR("user"), CFSTR("password"), NULL, 0, NULL);
#endif

    printf("PASS\n");
    
    return 0;
}
