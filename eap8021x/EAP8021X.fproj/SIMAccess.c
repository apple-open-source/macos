/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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
 * Modification History
 *
 * January 15, 2009	Dieter Siegmund (dieter@apple.com)
 * - created
 */

/*
 * SIMAccess.c
 * - API's to access the SIM
 */


#include "SIMAccess.h"
#include <TargetConditionals.h>

#if TARGET_OS_EMBEDDED
#include <CoreFoundation/CFRunLoop.h>
#include <CoreFoundation/CFMachPort.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFData.h>
#include <CoreTelephony/CTServerConnectionPriv.h>
#include <syslog.h>
#include "symbol_scope.h"

#define kSIMAccessRunLoopMode	CFSTR("com.apple.eap.eapsim.SIMAccess")

enum {
    kRequestResultUnknown	= 0,
    kRequestResultSuccess	= 1,
    kRequestResultFailure	= 2
};
typedef int	RequestResult;

typedef struct {
    CTServerConnectionRef	serv;
    int				result;
} ServerConnection, * ServerConnectionRef;

STATIC void
ServerConnectionCallback(CTServerConnectionRef connection, 
			 CFStringRef notification, 
			 CFDictionaryRef notification_info,
			 void * info)
{
    ServerConnectionRef	conn_p  = (ServerConnectionRef)info;

    if (notification == NULL) {
	return;
    }
    if (CFEqual(notification, kCTSIMSupportSIMAuthenticationInfoNotification)) {
	conn_p->result = kRequestResultSuccess;
    }
    else if (CFEqual(notification,
		     kCTSIMSupportSIMAuthenticationFailedNotification)) {
	conn_p->result = kRequestResultFailure;
    }
    return;

}

STATIC void
ServerConnectionInit(ServerConnectionRef conn_p)
{
    bzero(conn_p, sizeof(*conn_p));
    return;
}

STATIC void
ServerConnectionClose(ServerConnectionRef conn_p)
{
    if (conn_p->serv != NULL) {
        _CTServerConnectionUnregisterForNotification(conn_p->serv,
						     kCTSIMSupportSIMAuthenticationInfoNotification);
        _CTServerConnectionUnregisterForNotification(conn_p->serv,
						     kCTSIMSupportSIMAuthenticationFailedNotification);
        CFRelease(conn_p->serv);
    }
    ServerConnectionInit(conn_p);
    return;
}

STATIC CFStringRef
serverConnectionCopyDescription(const void *info)
{
    return (CFRetain(CFSTR("SIMAccess serv")));
}

STATIC bool
ServerConnectionOpen(ServerConnectionRef conn_p)
{
    _CTServerConnectionContext	ctx = {	0, NULL, NULL, NULL, 
					serverConnectionCopyDescription };
    ctx.info = conn_p;
    conn_p->serv
	= _CTServerConnectionCreateWithIdentifier(NULL,
						  CFSTR("EAP-SIM SIMAccess"),
						  ServerConnectionCallback,
						  &ctx);
    if (conn_p->serv == NULL) {
	fprintf(stderr, "_CTServerConnectionCreateWithIdentifier failed\n");
	goto failed;
    }
    _CTServerConnectionRegisterForNotification(conn_p->serv,
					       kCTSIMSupportSIMAuthenticationInfoNotification);
    _CTServerConnectionRegisterForNotification(conn_p->serv,
					       kCTSIMSupportSIMAuthenticationFailedNotification);
    _CTServerConnectionAddToRunLoop(conn_p->serv, CFRunLoopGetCurrent(),
				    kSIMAccessRunLoopMode);
    return (true);

 failed:
    ServerConnectionClose(conn_p);
    return (false);
}


STATIC CTError
ServerConnectionStartRequest(ServerConnectionRef conn_p, const uint8_t * rand_p)
{
    CTError			ct_error;
    CFDataRef			rand;

    rand = CFDataCreate(NULL, rand_p, SIM_RAND_SIZE);
    if (rand == NULL) {
	ct_error.domain = kCTErrorDomainPOSIX;
	ct_error.error = ENOMEM;
	return (ct_error);
    }
    ct_error = _CTServerConnectionSIMAuthenticate(conn_p->serv, rand);
    if (ct_error.domain != kCTErrorDomainNoError) {
	fprintf(stderr, "_CTServerConnectionSIMAuthenticate failed, %d/%d\n",
		(int)ct_error.domain, (int)ct_error.error);
    }
    CFRelease(rand);
    return (ct_error);
}


STATIC SInt32
ServerConnectionRun(ServerConnectionRef conn_p, CFTimeInterval seconds)
{
    return (CFRunLoopRunInMode(kSIMAccessRunLoopMode, seconds, true));
}

STATIC CTError
ServerConnectionGetKcSRES(ServerConnectionRef conn_p,
			  uint8_t * kc_p, uint8_t * sres_p)
{
    CTError		ct_error;
    CFDataRef		data;
    CFDictionaryRef	dict = NULL;
    CFRange		range;

    ct_error = _CTServerConnectionCopyAuthenticationInfo(conn_p->serv,
							 &dict);
    if (ct_error.domain != kCTErrorDomainNoError) {
	fprintf(stderr,
		"_CTServerConnectionCopyAuthenticationInfo failed %d/%d\n",
		(int)ct_error.domain, (int)ct_error.error);
	return (ct_error);
    }

    /* get the Kc bytes */
    data = CFDictionaryGetValue(dict, 
				kCTSimSupportSIMAuthenticationKc);
    if (CFDataGetLength(data) != SIM_KC_SIZE) {
	fprintf(stderr, "bogus Kc value\n");
	goto invalid;
    }
    range = CFRangeMake(0, SIM_KC_SIZE);
    CFDataGetBytes(data, range, kc_p);

    /* get the SRES bytes */
    data = CFDictionaryGetValue(dict, 
				kCTSimSupportSIMAuthenticationSres);
    if (CFDataGetLength(data) != SIM_SRES_SIZE) {
	fprintf(stderr, "bogus SRES value\n");
	goto invalid;
    }
    range = CFRangeMake(0, SIM_SRES_SIZE);
    CFDataGetBytes(data, range, sres_p);

    CFRelease(dict);
    return (ct_error);
    
 invalid:
    if (dict != NULL) {
	CFRelease(dict);
    }
    ct_error.domain = kCTErrorDomainPOSIX;
    ct_error.error = EINVAL;
    return (ct_error);
}

__private_extern__ bool
SIMProcessRAND(const uint8_t * rand_p, int count,
	       uint8_t * kc_p, uint8_t * sres_p)
{
    int			failed_count = 0;
    int			i;
    bool		ret = false;
    ServerConnection	conn;

    ServerConnectionInit(&conn);
    if (ServerConnectionOpen(&conn) == false) {
	syslog(LOG_NOTICE, "SIMProcessRAND: ServerConnectionOpen failed");
	return (false);
    }
    for (i = 0; i < count; i++) {
	CTError		ct_error;

	while (1) {
	    ct_error = ServerConnectionStartRequest(&conn,
						    rand_p + SIM_RAND_SIZE * i);
	    if (ct_error.domain == kCTErrorDomainNoError) {
		failed_count = 0;
		break;
	    }
	    failed_count++;
#define N_ATTEMPTS	3
	    if (failed_count > N_ATTEMPTS) {
		syslog(LOG_NOTICE, "Number of failed attempts %d > %d",
			failed_count, N_ATTEMPTS);
		goto failed;
	    }
	    ServerConnectionClose(&conn);
	    if (ServerConnectionOpen(&conn) == false) {
		syslog(LOG_NOTICE, 
			"SIMProcessRAND: ServerConnectionOpen failed,"
			" failed_count = %d", failed_count);
		goto failed;
	    }
	}
	conn.result = kRequestResultUnknown;
	while (1) {
	    SInt32	runloop_result;
	    bool	keep_going = FALSE;

#define WAIT_TIME_SECONDS	60
	    runloop_result = ServerConnectionRun(&conn, WAIT_TIME_SECONDS);
	    if (conn.result != kRequestResultUnknown) {
		break;
	    }
	    switch (runloop_result) {
	    case kCFRunLoopRunHandledSource:
		keep_going = TRUE;
		break;
	    case kCFRunLoopRunFinished:
	    case kCFRunLoopRunStopped:
		syslog(LOG_NOTICE,
		       "SIMProcessRAND: RunLoop finished/stopped"
		       " before results ready");
		break;
	    case kCFRunLoopRunTimedOut:
		syslog(LOG_NOTICE,
		       "SIMProcessRAND: RunLoop timed out after 60 seconds"); 
		break;
	    default:
		syslog(LOG_NOTICE,
		       "SIMProcessRAND: RunLoop returned unexpected value %d",
		       (int)runloop_result);
		break;
	    }
	    if (keep_going == FALSE) {
		break;
	    }
	}
	if (conn.result == kRequestResultSuccess) {
	    ct_error = ServerConnectionGetKcSRES(&conn,
						 kc_p + SIM_KC_SIZE * i,
						 sres_p + SIM_SRES_SIZE * i);
	}
	else {
	    syslog(LOG_NOTICE, "SIMProcessRAND: Could not access SIM");
	    goto failed;
	}
    }
    ret = true;

 failed:
    ServerConnectionClose(&conn);
    return (ret);
}

#else /* TARGET_OS_EMBEDDED */

__private_extern__ bool
SIMProcessRAND(const uint8_t * rand_p, int count,
	       uint8_t * kc_p, uint8_t * sres_p)
{
    return (false);
}

#endif /* TARGET_OS_EMBEDDED */

#ifdef TEST_SIMACCESS
#define N_TRIPLETS	3

#include "printdata.h"

int
main()
{
    static const uint8_t	rand[SIM_RAND_SIZE * N_TRIPLETS] = { 
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,

	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,

	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f
    };
    static uint8_t		kc[SIM_KC_SIZE * N_TRIPLETS];
    static uint8_t		sres[SIM_SRES_SIZE * N_TRIPLETS];

    if (SIMProcessRAND(rand, N_TRIPLETS, kc, sres) == false) {
	fprintf(stderr, "SIMProcessRAND failed\n");
	exit(1);
    }
    printf("Kc\n");
    print_data(kc, sizeof(kc));
    printf("SRES\n");
    print_data(sres, sizeof(sres));
    exit(0);
    return (0);
}

#endif /* TEST_SIMACCESS */
