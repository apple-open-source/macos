/*
    File:       IrQOS.h

    Contains:   Methods for implementing IrQOS (IrDA Quality of Service)

*/


#ifndef __IRQOS_H
#define __IRQOS_H

#include "IrDATypes.h"

class CBufferSegment;
struct USBIrDAQoS;          // From AppleUSBIrDA.h

// Constants

enum QOSIdentifiers
{
    kQOSNumberOfIdentifiers = 0x07,
    kQOSType1IdentifierFlag = 0x80,

    kQOSBaudRateId          = 0x01,
    kQOSMaxTurnAroundTimeId = 0x82,
    kQOSDataSizeId          = 0x83,
    kQOSWindowSizeId        = 0x84,
    kQOSNumberOfExtraBOFsId = 0x85,
    kQOSMinTurnAroundTimeId = 0x86,
    kQOSLinkDiscThresholdId = 0x08
};

enum QOSBaudRates           // 0x01
{
    kQOS2400bps             = 0x0001,
    kQOS9600bps             = 0x0002,
    kQOS19200bps            = 0x0004,
    kQOS38400bps            = 0x0008,
    kQOS57600bps            = 0x0010,
    kQOS115200bps           = 0x0020,
    kQOS576000bps           = 0x0040,
    kQOS1Mbps               = 0x0080,
    kQOS4Mbps               = 0x0100,

    kQOSValidBaudRatesLow       = (kQOS1Mbps     | kQOS115200bps | kQOS57600bps | kQOS38400bps  |
				   kQOS19200bps  | kQOS9600bps   | kQOS2400bps),
    kQOSValidBaudRatesHigh      = (kQOS4Mbps )>>8,
    
    kQOSDefaultBaudRates        = (kQOS1Mbps     | kQOS115200bps | kQOS57600bps | 
				   kQOS38400bps  | kQOS19200bps  | kQOS9600bps)
};

enum QOSMaxTurnAroundTime   // 0x82
{
    kQOSMaxTurnTime500ms    = 0x01,
    kQOSMaxTurnTime250ms    = 0x02,
    kQOSMaxTurnTime100ms    = 0x04,
    kQOSMaxTurnTime50ms     = 0x08,
    kQOSValidMaxTurnTimes       = (kQOSMaxTurnTime500ms | kQOSMaxTurnTime250ms |
				   kQOSMaxTurnTime100ms | kQOSMaxTurnTime50ms),
    kQOSDefaultMaxTurnTime      = (kQOSMaxTurnTime500ms)
};

enum QOSDataSize            // 0x83
{
    kQOS64Bytes             = 0x01,
    kQOS128Bytes            = 0x02,
    kQOS256Bytes            = 0x04,
    kQOS512Bytes            = 0x08,
    kQOS1024Bytes           = 0x10,
    kQOS2048Bytes           = 0x20,
    kQOSValidDataSizes          = (kQOS2048Bytes | kQOS1024Bytes |
				   kQOS512Bytes  | kQOS256Bytes  |
				   kQOS128Bytes  | kQOS64Bytes),
    kQOSDefaultDataSizes        = kQOSValidDataSizes
};

enum QOSWindowSize          // 0x84
{
    kQOS1Frame              = 0x01,
    kQOS2Frames             = 0x02,
    kQOS3Frames             = 0x04,
    kQOS4Frames             = 0x08,
    kQOS5Frames             = 0x10,
    kQOS6Frames             = 0x20,
    kQOS7Frames             = 0x40,
    kQOSValidWindowSizes        = (kQOS7Frames | kQOS6Frames | kQOS5Frames | kQOS4Frames |
				   kQOS3Frames | kQOS2Frames | kQOS1Frame),

    kQOSDefaultWindowSize       = (kQOS1Frame)
};

enum QOSExtraBOFs           // 0x85
{
    kQOS48ExtraBOFs         = 0x01,
    kQOS24ExtraBOFs         = 0x02,
    kQOS12ExtraBOFs         = 0x04,
    kQOS6ExtraBOFs          = 0x08,
    kQOS3ExtraBOFs          = 0x10,
    kQOS2ExtraBOFs          = 0x20,
    kQOS1ExtraBOF           = 0x40,
    kQOSNoExtraBOFs         = 0x80,
    kQOSValidExtraBOFs          = (kQOSNoExtraBOFs | kQOS1ExtraBOF  | kQOS2ExtraBOFs  |
				   kQOS3ExtraBOFs  | kQOS6ExtraBOFs | kQOS12ExtraBOFs |
				   kQOS24ExtraBOFs | kQOS48ExtraBOFs),
    kQOSDefaultExtraBOFs        = (kQOS2ExtraBOFs)
};

enum QOSMinTurnAroundTime   // 0x86
{
    kQOSMinTurnTime10ms     = 0x01,
    kQOSMinTurnTime5ms      = 0x02,
    kQOSMinTurnTime1ms      = 0x04,
    kQOSMinTurnTime500us    = 0x08,
    kQOSMinTurnTime100us    = 0x10,
    kQOSMinTurnTime50us     = 0x20,
    kQOSMinTurnTime10us     = 0x40,
    kQOSMinTurnTimeNone     = 0x80,
    kQOSValidMinTurnTimes       = (kQOSMinTurnTimeNone  | kQOSMinTurnTime10us  |
				   kQOSMinTurnTime50us  | kQOSMinTurnTime100us |
				   kQOSMinTurnTime500us | kQOSMinTurnTime1ms   |
				   kQOSMinTurnTime5ms   | kQOSMinTurnTime10ms),
    kQOSDefaultMinTurnTime      = (kQOSMinTurnTime5ms)
};

enum QOSLinkDiscThreshold   // 0x08
{
    kQOSDiscAfter3secs      = 0x01,
    kQOSDiscAfter8secs      = 0x02,
    kQOSDiscAfter12secs     = 0x04,
    kQOSDiscAfter16secs     = 0x08,
    kQOSDiscAfter20secs     = 0x10,
    kQOSDiscAfter25secs     = 0x20,
    kQOSDiscAfter30secs     = 0x40,
    kQOSDiscAfter40secs     = 0x80,
    kQOSValidDiscThresholds     = (kQOSDiscAfter40secs | kQOSDiscAfter30secs |
				   kQOSDiscAfter25secs | kQOSDiscAfter20secs |
				   kQOSDiscAfter16secs | kQOSDiscAfter12secs |
				   kQOSDiscAfter8secs  | kQOSDiscAfter3secs),
    kQOSDefaultDiscThresholds   = (kQOSDiscAfter40secs | kQOSDiscAfter30secs |
				   kQOSDiscAfter25secs | kQOSDiscAfter20secs |
				   kQOSDiscAfter16secs | kQOSDiscAfter12secs |
				   kQOSDiscAfter8secs  | kQOSDiscAfter3secs)
};

typedef struct
{
    UInt16      baudRate;
    
    UInt8       maxTurnTime,
		dataSizes,
		windowSize,
		extraBOFs,
		minTurnTime,
		discThresholds;
		
} QoSData, *QoSPtr, **QoSHandle;
		
// Classes

// --------------------------------------------------------------------------------------------------------------------
//                      TIrQOS
// --------------------------------------------------------------------------------------------------------------------

class TIrQOS : public OSObject
{
    OSDeclareDefaultStructors(TIrQOS);

    public:
	    static TIrQOS * tIrQOS(USBIrDAQoS *qos);
	    bool init(USBIrDAQoS *qos);
	    void free();

	    // Set all values to defaults
	    void            Reset();

	    // Specify QOS parms (obtained via options)
	    IrDAErr         SetBaudRate(BitRate bitsPerSec);
	    IrDAErr         SetDataSize(ULong bufferSize);
	    IrDAErr         SetWindowSize(ULong numFrames);
	    IrDAErr         SetLinkDiscThresholdTime(TTimeout linkDiscThresholdTime);

	    // Obtain QOS parms (probably returned to client via options)
	    BitRate         GetBaudRate();
	    TTimeout        GetMaxTurnAroundTime();
	    ULong           GetDataSize();
	    ULong           GetWindowSize();
	    ULong           GetExtraBOFs();
	    TTimeout        GetMinTurnAroundTime();
	    TTimeout        GetLinkDiscThresholdTime();

	    ULong           AddInfoToBuffer(UByte* buffer, ULong maxBytes);
	    IrDAErr         ExtractInfoFromBuffer(CBufferSegment* buffer);

	    IrDAErr         NegotiateWith(TIrQOS* peerDeviceQOS);

    private:

	    IrDAErr         NormalizeInfo();
	    ULong           HighestBitOn(UByte aByte);
	    
	    UInt16          fBaudRate;
	    UByte           fMaxTurnAroundTime;
	    UByte           fDataSize;
	    UByte           fWindowSize;
	    UByte           fNumExtraBOFs;
	    UByte           fMinTurnAroundTime;
	    UByte           fLinkDiscThreshold;
	    
	    USBIrDAQoS      *fDeviceQOS;        // qos values from usb
};

#endif // __IRQOS_H
