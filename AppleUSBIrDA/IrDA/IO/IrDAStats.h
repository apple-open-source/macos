/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
/*
    File:       IrDAStats.h

    Contains:   Status and statistics of interest to outside clients

*/
#ifndef __IrDAStats__
#define __IrDAStats__

// hmm
// These are defined in CarbonCore/MacTypes.h JRW

#ifndef __MACTYPES__

typedef unsigned char       UInt8;
typedef unsigned short int  UInt16;
typedef unsigned long int   UInt32;

#endif

enum {                              // lap connection state
    kIrDAStatusIdle,                // idle
    kIrDAStatusDiscoverActive,      // looking for peer
    kIrDAStatusConnected,           // connected
    kIrDAStatusBrokenConnection,    // still connected, but beam blocked
    kIrDAStatusInvalid,             // Invalid Status (Use by UI)
    kIrDAStatusOff                  // We have been powered down
};

typedef struct
{
    UInt8   connectionState;    // see enum
    UInt32  connectionSpeed;    // in bps
    UInt8   nickName[22];       // Nickname of peer (from discovery).  valid if connected

    UInt32  dataPacketsIn;      // total packets read
    UInt32  dataPacketsOut;     // total packets written
    UInt32  crcErrors;          // packets read with CRC errors (if available) 
    UInt32  ioErrors;           // packets read with other errors (if available)

    UInt32  recTimeout;         // number of recv timeouts
    UInt32  xmitTimeout;        // number of transmit timeouts (if implemented)

    UInt32  iFrameRec;          // Info frames received (data carrying packets)
    UInt32  iFrameSent;         // Info frames sent (data carrying packets)

    UInt32  uFrameRec;          // U frames received
    UInt32  uFrameSent;         // U frames sent

    UInt32  dropped;            // input packet dropped for (one of several) reasons
    UInt32  resent;             // count of our packets that we have resent

    UInt32  rrRec;              // count of RR packets read
    UInt32  rrSent;             // count of RR packets written
	    
    UInt16  rnrRec;             // count of receiver-not-ready packets read
    UInt16  rnrSent;            // count of receiver-not-ready packets sent
	    
    UInt8   rejRec;             // number of reject packets received
    UInt8   rejSent;            // number of reject packets sent
	    
    UInt8   srejRec;            // ?
    UInt8   srejSent;
			
    UInt32  protcolErrs;        // ?
} IrDAStatus;


#endif  // __IrDAStats__