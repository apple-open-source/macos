//
// IrDA Types
//

#ifndef _IrDA_Types_h
#define _IrDA_Types_h

#include <IOKit/IOLib.h>
#include <libkern/c++/OSObject.h>

#include "IrDADebugging.h"

enum IrDAErrors {
    noErr = 0,
    kIrDAErrGeneric,
    kIrDAErrWrongState,
    kIrDAErrRequestCanceled,
    kIrDAErrNotConnected,
    kIrDAErrCancel,
    kIrDAErrRetry,
    kIrDAErrLinkBusy,
    kIrDAErrTimeout,
    kIrDAErrToolBusy,
    kIrDAErrNoMemory,
    kIrDAErrPacket,
    kIrDAErrResourceNotAvailable,
    kIrDAErrBadParameter,
    
    errElementSizeMismatch,
    errNoMemory,
    errRangeCheck,
    errBadArg,
    
    // Discovery errors
    errDiscoveryTooManySlots,
    errDiscoveryInConnection,
    
    // lap
    errConnectionAborted,
    
    // qos
    errIncompatibleRemote,
    
    // irlapconn (et al)
    errCancel
    
};

typedef UInt32      IrDAErr;
typedef SInt32      Size;
typedef UInt8       UByte;
typedef UInt8       UChar;
typedef UInt16      UShort;
typedef UInt32      ULong;
typedef SInt32      SLong;
typedef SLong       FastInt;
typedef SInt32      Long;

typedef SLong       ArrayIndex;
enum    IndexValues  { kEmptyIndex = -1 };

enum {
    kPosBeg = 0,            // matches lseek(2) for no good reason
    kPosCur = 1,
    kPosEnd = 2
};

#define nil 0
#define EOF (-1)
	    
#define ABS(a)    ( ((a) < 0) ? -(a) : (a) )
#define MAX(a, b) ( ((a) > (b)) ? (a) : (b) )
#define MIN(a, b) ( ((a) < (b)) ? (a) : (b) )
#define MINMAX(min, expr, max) ( MIN(MAX(min, expr), max) )
#define ODD(x)    ( (x) & 1 )
#define EVEN(x)   ( !((x) & 1) )

typedef SInt32  ArrayIndex;
typedef UInt32  BitRate;

// Time stuff
typedef SInt32  TTimeout;                   // ms if > 0, us if < 0
#define kMicroseconds -1
#define kMilliseconds  1
#define kSeconds      1000
#define k9600bps                                        9600
#define k19200bps                                       19200
#define k38400bps                                       38400
#define k57600bps                                       57600
#define k115200bps                                      115200
#define k576000bps                                      576000
#define k1Mbps                                          1152000
#define k4Mbs                                           4000000

    
#define BlockMove(src, dest, len)           bcopy(src, dest, len)
#define BlockMoveData(src, dest, len)       BlockMove(x, y, len)

inline Long Min(Long a, Long b)                 { return (a < b) ? a : b; }
inline Long Max(Long a, Long b)                 { return (a > b) ? a : b; }
inline Long MinMax(Long l, Long x, Long h)      { return Min(Max(l, x), h); }
inline ULong UMin(ULong a, ULong b)             { return (a < b) ? a : b; }
inline ULong UMax(ULong a, ULong b)             { return (a > b) ? a : b; }
inline ULong UMinMax(ULong l, ULong x, ULong h) { return UMin(UMax(l, x), h); }


#endif  // _IrDA_Types_h
