/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef __SCCLOG__
#define __SCCLOG__

enum
{
	kSerialOut = 0,
	kSerialIn = 1,
	kSerialOther = 2
};

#if USE_ELG
    void AllocateEventLog(UInt32 size);
    void EvLog(UInt32 a, UInt32 b, UInt32 ascii, char* str);
#endif

#if LOG_DATA
    UInt8 Asciify(UInt8 i);
    void SerialLogData(UInt8 Dir, UInt32 Count, char *buf);
#endif

#endif
