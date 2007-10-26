/* 
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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

#include <stdlib.h>
#include <stdio.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/vm_map.h>
#include <servers/bootstrap.h>

#include "LKDCHelperMessage.h"

#include "LKDCHelper.h"

volatile int	LKDCLogLevel = ASL_LEVEL_DEBUG;

static mach_port_t LKDCGetHelperPort(int retry)
{
	static mach_port_t port = MACH_PORT_NULL;

	if (retry) {
		port = MACH_PORT_NULL;
	}

	if (port == MACH_PORT_NULL &&
	    BOOTSTRAP_SUCCESS != bootstrap_look_up(bootstrap_port, kLKDCHelperName, &port)) {
		LKDCLog("%s: cannot contact helper", __func__);
	}

	return port;
}


/* Ugly but handy. */
#define MACHRETRYLOOP_BEGIN(kr, retry, err, fin) for (;;) {
#define MACHRETRYLOOP_END(kr, retry, err, fin)				\
	if (KERN_SUCCESS == (kr))					\
		break;							\
	else if (MACH_SEND_INVALID_DEST == (kr) && 0 == (retry)++)	\
		continue;						\
	else {								\
		(err) = kLKDCHelperCommunicationFailed;			\
		LKDCLog("Mach communication failed: %s", mach_error_string(kr)); \
		goto fin;						\
	}								\
	}								\
	if (0 != (err)) {						\
		LKDCLog("%s", LKDCHelperError((err)));	\
		goto fin;						\
	}

void LKDCHelperExit ()
{
	kern_return_t kr = KERN_FAILURE;
	int retry = 0;
	int err = 0;
	
	MACHRETRYLOOP_BEGIN(kr, retry, err, fin);
	kr = request_LKDCHelperExit (LKDCGetHelperPort (retry));
	MACHRETRYLOOP_END(kr, retry, err, fin);
	
fin:
	return;
}

void LKDCDumpStatus (int logLevel)
{
	kern_return_t kr = KERN_FAILURE;
	int retry = 0;
	int err = 0;
	
	MACHRETRYLOOP_BEGIN(kr, retry, err, fin);
	kr = request_LKDCDumpStatus (LKDCGetHelperPort (retry), logLevel);
	MACHRETRYLOOP_END(kr, retry, err, fin);
	
fin:
	return;
}

void LKDCSetLogLevel (int logLevel)
{
	kern_return_t kr = KERN_FAILURE;
	int retry = 0;
	int err = 0;
	
	MACHRETRYLOOP_BEGIN(kr, retry, err, fin);
	kr = request_LKDCSetLogLevel (LKDCGetHelperPort (retry), logLevel);
	MACHRETRYLOOP_END(kr, retry, err, fin);
	
fin:
	return;
}

LKDCHelperErrorType LKDCGetLocalRealm (char **name)
{
	kern_return_t kr = KERN_FAILURE;
	int retry = 0;
	int err = 0;
	realmNameOut_t realm;

	LKDCLogEnter ();
	
	MACHRETRYLOOP_BEGIN(kr, retry, err, fin);
	kr = request_LKDCGetLocalRealm (LKDCGetHelperPort (retry), realm, &err);
	MACHRETRYLOOP_END(kr, retry, err, fin);

	if (NULL != realm && 0 == err) {
		LKDCLog ("Local realm = %s", realm);
		
		*name = strdup (realm);
	}
	
fin:

	LKDCLogExit (err);
	return err;
}

LKDCHelperErrorType LKDCDiscoverRealm (const char *hostname, char **realm)
{				   
	kern_return_t kr = KERN_FAILURE;
	int retry = 0;
	int err = 0;
	realmNameOut_t	returnedRealm;
	
	LKDCLogEnter ();
	
	if (NULL == realm) {
		LKDCLog ("No place to store discovered realm.");
		err = kLKDCHelperParameterError;
		goto fin;
	}

	MACHRETRYLOOP_BEGIN(kr, retry, err, fin);
	kr = request_LKDCDiscoverRealm (LKDCGetHelperPort (retry), hostname, returnedRealm, &err);
	MACHRETRYLOOP_END(kr, retry, err, fin);

	if (0 == err) {
		LKDCLog ("realm = %s", returnedRealm);
		
		*realm = strdup (returnedRealm);
	}

fin:
	LKDCLogExit (err);
	return err;
}


LKDCHelperErrorType LKDCFindKDCForRealm (const char *realm, char **hostname, uint16_t *port)
{
	kern_return_t kr = KERN_FAILURE;
	int	kdcport = 0;		/* MiG uses 32 bit types */
	int retry = 0;
	int err = 0;
	hostnameOut_t	returnedHostname;

	LKDCLogEnter ();
	
	if (NULL == hostname) {
		LKDCLog ("No place to store discovered KDC hostname.");
		err = kLKDCHelperParameterError;
		goto fin;
	}
	
	MACHRETRYLOOP_BEGIN(kr, retry, err, fin);
	kr = request_LKDCFindKDCForRealm (LKDCGetHelperPort (retry), realm, returnedHostname, &kdcport, &err);
	MACHRETRYLOOP_END(kr, retry, err, fin);
	
	if (0 == err) {
		*port = kdcport;
		
		LKDCLog ("KDC Hostname = %s:%u", returnedHostname, kdcport);
		
		*hostname = strdup (returnedHostname);
	}
	
fin:
	LKDCLogExit (err);
	return err;	
}
