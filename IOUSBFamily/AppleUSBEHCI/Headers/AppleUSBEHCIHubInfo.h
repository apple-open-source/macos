/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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


#ifndef _APPLEEHCIHUBINFO_H
#define _APPLEEHCIHUBINFO_H

// this structure is used to monitor the hubs which are attached. there will
// be an instance of this structure for every high speed hub with a FS/LS
// device attached to it. If the hub is in single TT mode, then there will
// just be one instance on port 0. If the hub is in multi-TT mode, then there
// will be that instance AND an instance for each active port
// note that this is NOT an OSObject because the overhead (data + vtable) 
// would be too high.

typedef struct AppleUSBEHCIHubInfo AppleUSBEHCIHubInfo, *AppleUSBEHCIHubInfoPtr;

struct AppleUSBEHCIHubInfo
{
    AppleUSBEHCIHubInfoPtr	next;
    UInt32			flags;
    UInt16			bandwidthAvailable;
    UInt8			hubAddr;
    UInt8			hubPort;
};

enum
{
    kUSBEHCIFlagsMuliTT		= 0x0001
};

#endif
