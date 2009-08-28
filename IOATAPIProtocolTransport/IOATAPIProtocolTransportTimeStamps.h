/*
 * Copyright (c) 2008 Apple Inc. All rights reserved.
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

#ifndef _IOKIT_IO_ATAPI_PROTOCOL_TRANSPORT_TIMESTAMPS_H_
#define _IOKIT_IO_ATAPI_PROTOCOL_TRANSPORT_TIMESTAMPS_H_

#include <IOKit/IOTypes.h>

#include <sys/kdebug.h>
#include <IOKit/IOTimeStamp.h>
#include <IOKit/scsi/IOSCSIArchitectureModelFamilyTimestamps.h>

#ifdef __cplusplus
extern "C" {
#endif


extern UInt32	gATAPIDebugFlags;


/* The trace codes consist of the following (see IOSCSIArchitectureModelFamilyTimestamps.h):
 *
 * ----------------------------------------------------------------------
 *|              |               |              |               |Func   |
 *| Class (8)    | SubClass (8)  | SAM Class(6) |  Code (8)     |Qual(2)|
 * ----------------------------------------------------------------------
 *
 * DBG_IOKIT(05h)  DBG_IOSAM(27h)	  (24h)
 *
 * See <sys/kdebug.h> and IOTimeStamp.h for more details.
 *
 */

// ATAPI tracepoints					0x05279000 - 0x052790FF
enum
{
	kATADeviceInfo				= 1,		/* 0x05279004 */
	kATASendSCSICommand			= 2,		/* 0x05279008 */
	kATASendSCSICommandFailed	= 3,		/* 0x0527900C */
	kATACompleteSCSICommand		= 4,		/* 0x05279010 */
	kATAAbort					= 5,		/* 0x05279014 */
	kATAReset					= 6,		/* 0x05279018 */
	kATAResetComplete			= 7, 		/* 0x0527901C */
	kATAHandlePowerOn			= 8,		/* 0x05279020 */
	kATAPowerOnReset			= 9,		/* 0x05279024 */
	kATAPowerOnNoReset			= 10,		/* 0x05279028 */
	kATAHandlePowerOff			= 11,		/* 0x0527902C */
	kATADriverPowerOff			= 12, 		/* 0x05279030 */
	kATAStartStatusPolling		= 13,		/* 0x05279034 */
	kATAStatusPoll				= 14,		/* 0x05279038 */
	kATAStopStatusPolling		= 15,		/* 0x0527903C */
	kATASendATASleepCmd			= 16,		/* 0x05279040 */
};

// Tracepoint macros.
#define ATAPI_TRACE(code)	( ( ( DBG_IOKIT & 0xFF ) << 24) | ( ( DBG_IOSAM & 0xFF ) << 16 ) | ( kSAMClassATAPI << 10 ) | ( ( code & 0xFF ) << 2 ) )

#ifdef __cplusplus
}
#endif


#endif	/* _IOKIT_IO_ATAPI_PROTOCOL_TRANSPORT_TIMESTAMPS_H_ */
