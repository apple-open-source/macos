/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 
/*!
 *  @header SLPDefines
 */
 
#ifndef _SLPDefines_
#define _SLPDefines_
#pragma once

#define kSLPdPath	"/tmp/slp_ipc"

#define CONFIG_DA_HEART_BEAT		10800	// (3 hours) DA Heartbeat,
											// so that SAs passively detect
											// new DAs.

#define v2_Default_Scope			"DEFAULT"
#define kSLPPluginNotInitialized	-1

enum
{
	kLogOptionRegDereg			= 0x01000000,
	kLogOptionExpirations		= 0x02000000,
	kLogOptionServiceRequests	= 0x04000000,
	kLogOptionDAInfoRequests	= 0x08000000,
	kLogOptionRejections		= 0x10000000,
	kLogOptionRAdminInteraction	= 0x20000000,
	kLogOptionErrors			= 0x00000080,
	kLogOptionDebuggingMessages	= 0x40000000,
    kLogOptionAllMessages		= 0x80000000
};

#define CONFIG_INTERVAL_0			60		// Cache replies by XID.

#define CONFIG_INTERVAL_1			10800	// registration Lifetime,
											// (ie. 3 hours) 
											// after which ad expires
											
#define CONFIG_INTERVAL_2			1		// Retry multicast query 
											// until no new values arrive.
											
#define CONFIG_INTERVAL_3			30		// Max time to wait for a 

#define CONFIG_INTERVAL_4			3		// Wait to rgister on reboot

#define CONFIG_INTERVAL_5			3		// Retransmit DA discovery,
											// retry it 3 times
											
#define CONFIG_INTERVAL_6			5		// Give up on requests sent to DA

#define CONFIG_INTERVAL_7			15		// Give up on DA discovery

#define CONFIG_INTERVAL_8			15		// Give up on requests sent to SAs

#define CONFIG_INTERVAL_9			10800	// (3 hours) DA Heartbeat,
											// so that SAs passively detect
											// new DAs.
											
#define CONFIG_INTERVAL_10			RangedRdm(1,3)	
											// (should be random 1-3)
											// wait to register services on
											// passive DA discovery
											
#define CONFIG_INTERVAL_11			RangedRdm(1,3)
											// (should be random 1-3)
											// wait to register services on
											// active DA discovery.
											
#define CONFIG_INTERVAL_12			300		// (ie. 5 seconds)
											// DAs and SAs close idle connections
											
#endif
