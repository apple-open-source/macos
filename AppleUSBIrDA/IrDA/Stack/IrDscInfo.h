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
    File:       IrDscInfo.h

    Contains:   Methods for implementing IrDscInfo (IrLMP's XID discovery/device info)


*/


#ifndef __IRDSCINFO_H
#define __IRDSCINFO_H

#include "IrDATypes.h"


// Constants

// Max nickname length (must be shorter if more than one service hints byte)
#define kMaxNicknameLen     21

enum DevInfoHints
{
    kDevInfoHintPnPSupport  = 0x00000001,
    kDevInfoHintPDA         = 0x00000002,
    kDevInfoHintComputer    = 0x00000004,
    kDevInfoHintPrinter     = 0x00000008,
    kDevInfoHintModem       = 0x00000010,
    kDevInfoHintFAX         = 0x00000020,
    kDevInfoHintLANAccess   = 0x00000040,
    kDevInfoHintExtension1  = 0x00000080,   // Not a real hint, an implementation gizmo

    kDevInfoHintTelephony   = 0x00000100,
    kDevInfoHintFileServer  = 0x00000200,
    kDevInfoHintIrCOMM      = 0x00000400,
    kDevInfoHintReserved1   = 0x00000800,
    kDevInfoHintReserved2   = 0x00001000,
    kDevInfoHintReserved3   = 0x00002000,
    kDevInfoHintReserved4   = 0x00004000,
    kDevInfoHintExtension2  = 0x00008000,   // Not a real hint, an implementation gizmo

    kDevInfoHintExtension3  = 0x00800000    // Not a real hint, an implementation gizmo
};

#define kHintCount  32

class CBufferSegment;

// Classes

// --------------------------------------------------------------------------------------------------------------------
//                      TIrDscInfo
// --------------------------------------------------------------------------------------------------------------------

class TIrDscInfo : public OSObject
{
	    OSDeclareDefaultStructors(TIrDscInfo);
public:
	    static TIrDscInfo * tIrDscInfo();
	    bool    init(void);
	    void    free(void);
    

	    // Specify DeviceInfo fields
	    void                SetVersion(UByte version)           { fVersion = version;   }
	    void                SetServiceHints(ULong hintBits);
	    void                RemoveServiceHints( ULong hintBits );
	    void                SetDeviceAddr(ULong address)        { fDevAddr = address;   }
	    void                SetCharacterSet(UByte charset)      { fCharset = charset;   }
	    IrDAErr             SetNickname(const char* name);

	    // Obtain DeviceInfo fields
	    UByte               GetVersion()                        { return fVersion;      }
	    UByte               GetCharacterSet()                   { return fCharset;      }
	    ULong               GetServiceHints()                   { return fHints;        }
	    ULong               GetDeviceAddr()                     { return fDevAddr;      }
	    void                GetNickname(UChar* name);

	    // Put/Get DevInfo part of the discovery info (service hints, char set, nickname)
	    ULong               AddDevInfoToBuffer(UByte* buffer, ULong maxBytes);
	    IrDAErr             ExtractDevInfoFromBuffer(CBufferSegment* buffer);
    
    private:

	    ULong               fDevAddr;
	    ULong               fHints;
	    UByte               fVersion;       // Of IrLAP supported
	    UByte               fCharset;
	    UByte               fNickname[kMaxNicknameLen+1];
	    UByte               fHintCount[kHintCount];
};

#endif // __IRDSCINFO_H
