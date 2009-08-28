/*
 * Copyright (c) 1998-2009 Apple Inc. All rights reserved.
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


#ifndef _IOKIT_IO_FIREWIRE_SERIAL_BUS_PROTOCOL_TRANSPORT_TIMESTAMPS_H_
#define _IOKIT_IO_FIREWIRE_SERIAL_BUS_PROTOCOL_TRANSPORT_TIMESTAMPS_H_

#include <IOKit/IOTypes.h>

#include <sys/kdebug.h>
#include <IOKit/IOTimeStamp.h>
#include <IOKit/scsi/IOSCSIArchitectureModelFamilyTimestamps.h>

#ifdef __cplusplus
extern "C" {
#endif




/* The trace codes consist of the following (see IOSCSIArchitectureModelFamilyTimestamps.h):
 *
 * ----------------------------------------------------------------------
 *|              |               |              |               |Func   |
 *| Class (8)    | SubClass (8)  | SAM Class(6) |  Code (8)     |Qual(2)|
 * ----------------------------------------------------------------------
 *
 * DBG_IOKIT(05h)  DBG_IOSAM(27h)	  (20h)
 *
 * See <sys/kdebug.h> and IOTimeStamp.h for more details.
 *
 */

// FireWire tracepoints								0x05278000 - 0x052783FF
enum
{
	kGUID								= 1,		/* 0x05278004 */
	kLoginRequest						= 2,		/* 0x05278008 */
	kLoginCompletion					= 3,		/* 0x0527800C */
	kLoginLost							= 4,		/* 0x05278010 */
	kLoginResumed						= 5,		/* 0x05278014 */
	kSendSCSICommand1					= 6,		/* 0x05278018 */
	kSendSCSICommand2					= 7,		/* 0x0527801C */
	kSCSICommandSenseData				= 8,		/* 0x05278020 */
	kCompleteSCSICommand				= 9,		/* 0x05278024 */
	kSubmitOrb							= 10,		/* 0x05278028 */
	kStatusNotify						= 11,		/* 0x0527802C */
	kFetchAgentReset					= 12,		/* 0x05278030 */
	kFetchAgentResetComplete			= 13,		/* 0x05278034 */
	kLogicalUnitReset					= 14,		/* 0x05278038 */
	kLogicalUnitResetComplete			= 15		/* 0x0527803C */
};

// Tracepoint macros.
#define FW_TRACE(code)	( ( ( DBG_IOKIT & 0xFF ) << 24) | ( ( DBG_IOSAM & 0xFF ) << 16 ) | ( kSAMClassFireWire << 10 ) | ( ( code & 0xFF ) << 2 ) )

#ifdef __cplusplus
}
#endif


#endif	/* _IOKIT_IO_FIREWIRE_SERIAL_BUS_PROTOCOL_TRANSPORT_TIMESTAMPS_H_ */
