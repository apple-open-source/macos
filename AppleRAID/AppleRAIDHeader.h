/*
 * Copyright (c) 2001-2002 Apple Computer, Inc. All rights reserved.
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
 *  DRI: Josh de Cesare
 *
 */


#ifndef _APPLERAIDHEADER_H
#define _APPLERAIDHEADER_H

#define kAppleRAIDSetNameKey		("AppleRAID-SetName")
#define kAppleRAIDSetUniqueNameKey	("AppleRAID-SetUniqueName")
#define kAppleRAIDSliceNumberKey	("AppleRAID-SliceNumber")
#define kAppleRAIDSequenceNumberKey	("AppleRAID-SequenceNumber")

#define kAppleRAIDLevelName		("AppleRAID-LevelName")
#define kAppleRAIDLevelNameStripe	("Stripe")
#define kAppleRAIDLevelNameMirror	("Mirror")
#define kAppleRAIDLevelNameConcat	("Concat")

#define kAppleRAIDStatus		("AppleRAID-Status")
#define kAppleRAIDStatusForming		("Forming")
#define kAppleRAIDStatusRunning		("Running")
#define kAppleRAIDStatusStopped		("Stopped")
#define kAppleRAIDStatusDegraded	("Degraded")
#define kAppleRAIDStatusFailed		("Failed")

#define kAppleRAIDSignature		("AppleRAIDHeader")

enum {
    kAppleRAIDStripe		= 0x00000000,
    kAppleRAIDMirror		= 0x00000001,
    kAppleRAIDConcat		= 0x00000100,
    
    kAppleRAIDHeaderV1_0_0	= 0x00010000,
    
    kAppleRAIDHeaderSize	= 0x1000,
    kAppleRAIDMaxOFPath		= 0x200
};

struct AppleRAIDHeader {
    char	raidSignature[16];		// 0x0000 - kAppleRAIDSignature
    UInt32	raidHeaderSize;			// 0x0010 - Defaults to kAppleRAIDHeaderSize
    UInt32	raidHeaderVersion;		// 0x0014 - kAppleRAIDHeaderV1_0_0
    UInt32	raidHeaderSequence;		// 0x0018 - 0 slice is bad, >0 slice could be good
    UInt32	raidLevel;			// 0x001C - one of kAppleRAIDStripe, kAppleRAIDMirror or kAppleRAIDConcat
    UInt32	raidUUID[4];			// 0x0020 - 128 bit univeral unique identifier
    char	raidSetName[32];		// 0x0030 - Null Terminated 31 Character UTF8 String
    UInt32	raidSliceCount;			// 0x0050 - Number of slices in set
    UInt32	raidSliceNumber;		// 0x0054 - 0 <= raidSliceNumber < raidSliceCount
    UInt32	raidChunkSize;			// 0x0058 - Usually 32 KB
    UInt32	raidChunkCount;			// 0x005C - Number of full chunks in set
    UInt32	reserved1[104];			// 0x0060 - reservered init to zero, but preserve on update
    char	raidOFPaths[0];			// 0x0200 - Allow kAppleRAIDMaxOFPath for each slice
                                                //        - Zero fill to size of header
};
typedef struct AppleRAIDHeader AppleRAIDHeader;

#endif /* ! _APPLERAIDHEADER_H */
