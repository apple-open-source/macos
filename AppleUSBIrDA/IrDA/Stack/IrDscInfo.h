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
	    void                GetNickname(UChar* name, int maxnamelen);

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
