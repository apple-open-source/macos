/*
 * Copyright (c) 2007 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 * 
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */


#ifndef _PMConnection_h_
#define _PMConnection_h_


#define LOG_SLEEPSERVICES 1
/*
 * Struct for gSleepService 
 */
typedef struct {
    int                         notifyToken;
    CFStringRef                 uuid;
    long                        capTime;
} SleepServiceStruct;


__private_extern__ void PMConnection_prime(void);

__private_extern__ bool PMConnectionHandleDeadName(mach_port_t deadPort);

// PMAssertions.c calls into this when a PreventSystemSleep assertion is taken
__private_extern__ IOReturn _unclamp_silent_running(void);

__private_extern__ io_connect_t getRootDomainConnect();
__private_extern__ bool isA_BTMtnceWake();
__private_extern__ bool isA_SleepSrvcWake();
__private_extern__ void set_SleepSrvcWake();
__private_extern__ void cancelPowerNapStates( );

__private_extern__ void InternalEvalConnections(void);
#endif

