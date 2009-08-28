/*
    File:       IrQOS.c

    Contains:   Implementation of IrDA Quality Of Service class

*/


#include "IrQOS.h"
#include "CBufferSegment.h"
#include "IrDALog.h"
#include "AppleIrDA.h"

#if (hasTracing > 0 && hasIrQOSTracing > 0) 

enum IrLogCodes
{
    kLogNew = 1,
    kLogFree,
    
    kLogReset,
    kLogReset_Baud,
    kLogReset_Data,
    kLogReset_BOF,
    kLogReset_Disconnect,
    
    kLogSetBaud1,
    kLogSetBaud2,
    kLogSetData1,
    kLogSetData2,
    
    kLogSetWindow,
    kLogSetDisconnect,
    
    kLogGetBaud,
    kLogGetBaudFailed,
    kLogGetMaxTurn,
    kLogGetMaxTurnFailed,
    
    kLogGetDataSize,
    kLogGetDataSizeFailed,
    kLogGetWindowSize,
    kLogGetWindowSizeFailed,
    
    kLogGetBofs,
    kLogGetBofsFailed,
    kLogGetMin,
    kLogGetMinFailed,
    kLogGetDisconnect,
    kLogGetDisconnectFailed,
    
    kLogAddInfo,
    kLogAddInfoFailed,
    kLogExtract,
    kLogExtractData,
    kLogNegotiateBaud1,
    kLogNegotiateBaud2,
    
    kLogNormMinTurn,
    kLogNormMaxLine,
    kLogNormWindow,
    kLogNormData,
    kLogNormRequested,
    kLogNormSmallerWindow,
    kLogNormSmallerData
    
};

static
EventTraceCauseDesc IrLogEvents[] = {
    {kLogNew,               "IrQOS: new, obj="},
    {kLogFree,              "IrQOS: free, obj="},
    
    {kLogReset,             "IrQOS: reset"},
    {kLogReset_Baud,        "IrQOS: reset max turn=, baud bitmap="},
    {kLogReset_Data,        "IrQOS: reset datasize=, window bits="},
    {kLogReset_BOF,         "IrQOS: reset bof=, min turn="},
    {kLogReset_Disconnect,  "IrQOS: reset disconnect link="},
    
    {kLogSetBaud1,          "IrQOS: set baud, bps="},
    {kLogSetBaud2,          "IrQOS: set baud, result=, fBaudRate="},
    {kLogSetData1,          "IrQOS: set data, buffersize="},
    {kLogSetData2,          "IrQOS: set data, result=, fDataSize="},
    
    {kLogSetWindow,         "IrQOS: set windowsize, ct=, bitmap="},
    {kLogSetDisconnect,     "IrQOS: set disconnect threshold, seconds=, mask="},
    
    {kLogGetBaud,           "IrQOS: get baud, bitpos=, bps="},
    {kLogGetBaudFailed,     "IrQOS: get baud failed, logic error!"},
    {kLogGetMaxTurn,        "IrQOS: get max turnaround, bitpos=, ms="},
    {kLogGetMaxTurnFailed,  "IrQOS: get max failed, logic error!"},
    
    {kLogGetDataSize,       "IrQOS: get data size, size="},
    {kLogGetDataSizeFailed, "IrQOS: get data size failed, logic error"},
    {kLogGetWindowSize,     "IrQOS: get window size, count="},
    {kLogGetWindowSizeFailed,   "IrQOS: get window size logic error!"},
    
    {kLogGetBofs,               "IrQOS: get bofs adjusted for speed, ct="},
    {kLogGetBofsFailed,         "IrQOS: get bofs failed, logic error"},
    {kLogGetMin,                "IrQOS: get min turnaround, delay="},
    {kLogGetMinFailed,          "IrQOS: get min turnaround logic error!"},
    {kLogGetDisconnect,         "IrQOS: get disconnect time, ms="},
    {kLogGetDisconnectFailed,   "IrQOS: get disconnect time failed, logic error"},
    
    {kLogAddInfo,               "IrQOS: add info, buffersize="},
    {kLogAddInfoFailed,         "IrQOS: add info failed logic error!"},
    {kLogExtract,               "IrQOS: extract info from buffer, addr="},
    {kLogExtractData,           "IrQOS: extract info, id=, len | value="},
    {kLogNegotiateBaud1,        "IrQOS: negotiate speed, input=, input="},
    {kLogNegotiateBaud2,        "IrQOS: negotiate speed, result="},
    
    {kLogNormMinTurn,           "IrQOS: min turn, speedBit | minturnBit, turn in bytes="},
    {kLogNormMaxLine,           "IrQOS: max line capacity="},
    {kLogNormWindow,            "IrQOS: had to normalize window mask, old=, new="},
    {kLogNormData,              "IrQOS: had to normalize data size mask, old=, new="},
    {kLogNormRequested,         "IrQOS: requested data line capacity="},
    {kLogNormSmallerWindow,     "IrQOS: shrinking window size to fit, new="},
    {kLogNormSmallerData,       "IrQOS: shrinking data size to fit, new="}
};

#define XTRACE(x, y, z) IrDALogAdd( x, y, (uintptr_t)z & 0xffff, IrLogEvents, true )

#else
#define XTRACE(x,y,z)   ((void)0)
#endif


#define kMicroseconds -1

static const ULong IrBaudRateTable[] = {    k9600bps,   k19200bps,  k38400bps,  k57600bps, 
					    k115200bps, k576000bps, k1Mbps,     k4Mbs       };

static const UByte IrExtraBOFsTable[8] = {48, 24, 12, 6, 3, 2, 1, 0};

static const TTimeout IrMaxTurnTimeTable[4] = {500 * kMilliseconds,
					250 * kMilliseconds,
					100 * kMilliseconds,
					 50 * kMilliseconds};

static const TTimeout IrMinTurnTimeTable[8] = { 10 * kMilliseconds,
					  5 * kMilliseconds,
					  1 * kMilliseconds,
					500 * kMicroseconds,
					100 * kMicroseconds,
					 50 * kMicroseconds,
					 10 * kMicroseconds,
					  0};

static const TTimeout IrLinkDiscThreshold[8] = {  3 * kSeconds,
						  8 * kSeconds,
						 12 * kSeconds,
						 16 * kSeconds,
						 20 * kSeconds,
						 25 * kSeconds,
						 30 * kSeconds,
						 40 * kSeconds};
						 
// The following tables are from 6.6.11 of IrLAP 1.1
    
//                                         10ms  5ms  1ms  .5ms .1ms .05ms .01ms 0ms
const UInt16 IrMinTurnInBytesTable[8][8] = {{ 10,    5,   1,   0,   0,   0,    0,   0},     //   9600 bps
					    { 20,   10,   2,   1,   0,   0,    0,   0},     //  19200 bps
					   {  40,   20,   4,   2,   0,   0,    0,   0},     //  38400 bps
					   {  58,   29,   6,   3,   1,   0,    0,   0},     //  57600 bps
					   { 115,   58,  12,   6,   1,   1,    0,   0},     // 115200 bps
					   { 720,  360,  72,  36,   7,   4,    2,   0},     // 576000 bps
					  { 1440,  720, 144,  72,  14,   7,    4,   0},     //  1.152 Mbps
					  { 5000, 2500, 500, 250,  50,   25,   5,   0}};    //      4 Mbps
					  
// This is the max line capacity table for baud rates below 115.2k (500ms maxTurn assumed)
const ULong IrMaxLineCapacityTable1[4] = {400, 800, 1600, 2360};

// This is the max line capacity table for 115.2k (based on maxTurn of 500/250/100/50 ms) 
// Includes 20% overhead for transpearency, 
const ULong IrMaxLineCapacityTable2[4] = {4800, 2400, 960, 480};

// FIR capacity with no overhead for bit stuffing

const ULong IrMaxLineCapacityTable576[4]    = { 28800,   11520,  5760,  2880 };
const ULong IrMaxLineCapacityTable1Mbps[4]  = { 57600,   28800, 11520,  5760 };
const ULong IrMaxLineCapacityTable4Mbps[4]  = { 200000, 100000, 40000, 20000 };

//--------------------------------------------------------------------------------
#define super OSObject
    OSDefineMetaClassAndStructors(TIrQOS, OSObject);
//--------------------------------------------------------------------------------


//--------------------------------------------------------------------------------
//      TIrQOS
//--------------------------------------------------------------------------------
/*static*/
TIrQOS *
TIrQOS::tIrQOS(USBIrDAQoS *qos)
{
    TIrQOS *obj = new TIrQOS;
    
    XTRACE(kLogNew, 0, obj);
    
    if (obj && !obj->init(qos)) {
	obj->release();
	obj = nil;
    }
    return obj;
}

//--------------------------------------------------------------------------------
//      init
//--------------------------------------------------------------------------------
bool TIrQOS::init(USBIrDAQoS *qos)
{
    fDeviceQOS = qos;       // save the qos from usb
    
    Reset();                // reset to initial values
    
    return super::init();   // done
    
} // TIrQOS::init


//--------------------------------------------------------------------------------
//      Free
//--------------------------------------------------------------------------------
void TIrQOS::free(void)
{
    XTRACE(kLogFree, 0, this);     // only reason we have a free
    super::free();                                      // is for the log
}


//--------------------------------------------------------------------------------
//      Reset
//--------------------------------------------------------------------------------
void TIrQOS::Reset()
{
#ifdef THROTTLE_SPEED
    int TESTING_ONLY_BAUD_THROTTLED;
    UInt16 THROTTLE = THROTTLE_SPEED;       // 0x3 = 9600 or slower, 0x7 = 19.2, 0f=38.4
#endif

    XTRACE(kLogReset, 0, this);
    
    // todo - could init from kext prefs file instead of compiled-in

    fBaudRate           = kQOSDefaultBaudRates;
    fMaxTurnAroundTime  = kQOSDefaultMaxTurnTime;
    fDataSize           = kQOSDefaultDataSizes;
    fWindowSize         = kQOSDefaultWindowSize;
    fNumExtraBOFs       = kQOSDefaultExtraBOFs;
    fMinTurnAroundTime  = kQOSDefaultMinTurnTime;
    fLinkDiscThreshold  = kQOSDefaultDiscThresholds;
    
    if (fDeviceQOS) {
	fDataSize           = fDeviceQOS->datasize;     // bytes per frame supported
	fWindowSize         = fDeviceQOS->windowsize;   // 1 thru 7 frames per ack
	fMinTurnAroundTime  = fDeviceQOS->minturn;      // min turnaround time
	fBaudRate           = fDeviceQOS->baud1 << 8 | fDeviceQOS->baud2;       // 16 bits of baud
#ifdef THROTTLE_SPEED
	{                                           // if want to slow down the connection
	    if (fBaudRate & THROTTLE) {             // if a valid result, then throttle back for testing
		fBaudRate &= THROTTLE;
		IOLog("Baud throttled, bitmap now set to 0x%x\n", fBaudRate);
	    }
	    else
		IOLog("baud not throttled, would have been zero, 0x%x, 0x%x\n", fBaudRate, THROTTLE);
	}
#endif
	fNumExtraBOFs       = fDeviceQOS->bofs;         // number of bofs the pod wants
    }
    
    // Log all the new values
    XTRACE(kLogReset_Baud, fMaxTurnAroundTime, fBaudRate);
    XTRACE(kLogReset_Data, fDataSize, fWindowSize);
    XTRACE(kLogReset_BOF,  fNumExtraBOFs, fMinTurnAroundTime);
    XTRACE(kLogReset_Disconnect, 0, fLinkDiscThreshold);

} // TIrQOS::Reset


//--------------------------------------------------------------------------------
//      SetBaudRate
//--------------------------------------------------------------------------------
IrDAErr TIrQOS::SetBaudRate(BitRate bitsPerSec)
{
    ULong newBaudRate = 0;
    IrDAErr result;

    switch(bitsPerSec) {
	// NOTE: Each case falls thru on purpose to build the bit mask
	case k4Mbs:
	    newBaudRate |= kQOS4Mbps;       
	
	case k1Mbps:
	    newBaudRate |= kQOS1Mbps;       
	
	case k576000bps:
	    newBaudRate |= kQOS576000bps;       
	
	case k115200bps:
	    newBaudRate |= kQOS115200bps;

	case k57600bps:
	    newBaudRate |= kQOS57600bps;

	case k38400bps:
	    newBaudRate |= kQOS38400bps;

	case k19200bps:
	    newBaudRate |= kQOS19200bps;

	case k9600bps:
	    newBaudRate |= kQOS9600bps;

	    fBaudRate = (UByte)newBaudRate;
	    result = noErr;
	    break;

	default:
	    result = errBadArg;
	    break;
    }

    XTRACE(kLogSetBaud1, bitsPerSec >> 16, bitsPerSec);
    XTRACE(kLogSetBaud2, result, fBaudRate);
    
    return result;

} // TIrQOS::SetBaudRate



//--------------------------------------------------------------------------------
//      SetDataSize
//--------------------------------------------------------------------------------
IrDAErr TIrQOS::SetDataSize(ULong bufferSize)
{
    ULong newDataSize = 0;
    IrDAErr result;

    switch(bufferSize) {
	// NOTE: Each case falls thru on purpose to build the bit mask
	case 2048:
	    newDataSize |= kQOS2048Bytes;

	case 1024:
	    newDataSize |= kQOS1024Bytes;

	case 512:
	    newDataSize |= kQOS512Bytes;

	case 256:
	    newDataSize |= kQOS256Bytes;

	case 128:
	    newDataSize |= kQOS128Bytes;

	case 64:
	    newDataSize |= kQOS64Bytes;

	    fDataSize = (UByte)newDataSize;
	    result = noErr;
	    break;

	default:
	    result = errBadArg;
	    break;
    }

    XTRACE(kLogSetData1, bufferSize >> 16, bufferSize);
    XTRACE(kLogSetData2, result, fDataSize);
    return result;

} // TIrQOS::SetDataSize


//--------------------------------------------------------------------------------
//      SetWindowSize
//--------------------------------------------------------------------------------
IrDAErr TIrQOS::SetWindowSize(ULong numFrames)
{
    // Only acceptible values are 1 thru 7
    if (--numFrames >= 7) return errBadArg;

    fWindowSize = (UByte)((kQOSValidWindowSizes >> (6 - numFrames)) & kQOSValidWindowSizes);

    XTRACE(kLogSetWindow, numFrames, fWindowSize);
    return noErr;

} // TIrQOS::SetWindowSize



//--------------------------------------------------------------------------------
//      SetLinkDiscThresholdTime
//--------------------------------------------------------------------------------
IrDAErr TIrQOS::SetLinkDiscThresholdTime(TTimeout linkDiscThresholdTime)
{
    ULong newLinkDiscThreshold = 0;
    IrDAErr result;

    switch(linkDiscThresholdTime / kSeconds) {
	// NOTE: Each case falls thru on purpose to build the bit mask
	case 40:
	    newLinkDiscThreshold |= kQOSDiscAfter40secs;

	case 30:
	    newLinkDiscThreshold |= kQOSDiscAfter30secs;

	case 25:
	    newLinkDiscThreshold |= kQOSDiscAfter25secs;

	case 20:
	    newLinkDiscThreshold |= kQOSDiscAfter20secs;

	case 16:
	    newLinkDiscThreshold |= kQOSDiscAfter16secs;

	case 12:
	    newLinkDiscThreshold |= kQOSDiscAfter12secs;

	case 8:
	    newLinkDiscThreshold |= kQOSDiscAfter8secs;

	case 3:
	    newLinkDiscThreshold |= kQOSDiscAfter3secs;

	    fLinkDiscThreshold = (UByte)newLinkDiscThreshold;
	    result = noErr;
	    break;

	default:
	    result = errBadArg;
	    break;
    }

    XTRACE(kLogSetDisconnect, linkDiscThresholdTime / kSeconds, fLinkDiscThreshold);
    return result;

} // TIrQOS::SetLinkDiscThresholdTime


//--------------------------------------------------------------------------------
//      GetBaudRate
//--------------------------------------------------------------------------------
BitRate TIrQOS::GetBaudRate()
{
    ULong bitPos;

    // note: the -1 and +7 are due to 2400 baud not being in our table
    
    if( fBaudRate>>8 & kQOSValidBaudRatesHigh ) {
	bitPos = HighestBitOn(fBaudRate>>8 ) + 7;   // Look at next byte
    }
    else {
	bitPos = HighestBitOn(fBaudRate) - 1;       // Look at the low byte
    }
    require(bitPos < sizeof(IrBaudRateTable) / sizeof(IrBaudRateTable[0]), Fail);
    
    XTRACE(kLogGetBaud, bitPos, IrBaudRateTable[bitPos]);
    
    return IrBaudRateTable[bitPos];

Fail:
    XTRACE(kLogGetBaudFailed, 0xffff, 9600);
    return k9600bps;        // better than crashing, but not much

} // TIrQOS::GetBaudRate


//--------------------------------------------------------------------------------
//      GetMaxTurnAroundTime
//--------------------------------------------------------------------------------
TTimeout TIrQOS::GetMaxTurnAroundTime()
{
    ULong bitPos = HighestBitOn(fMaxTurnAroundTime);
    require(bitPos < sizeof(IrMaxTurnTimeTable) / sizeof(IrMaxTurnTimeTable[0]), Fail);
    
    XTRACE(kLogGetMaxTurn, bitPos, IrMaxTurnTimeTable[bitPos]);
    
    return IrMaxTurnTimeTable[bitPos];

Fail:
    XTRACE(kLogGetMaxTurnFailed, 0xffff, 500);
    
    return (500 * kMilliseconds);

} // TIrQOS::GetMaxTurnAroundTime


//--------------------------------------------------------------------------------
//      GetDataSize
//--------------------------------------------------------------------------------
ULong TIrQOS::GetDataSize()
{
    ULong size;
    
    require(fDataSize, Fail);
    
    size = 64 * (1 << HighestBitOn(fDataSize));
    
    XTRACE(kLogGetDataSize, 0, size);
    
    return size;

Fail:
    XTRACE(kLogGetDataSizeFailed, 0xffff, 64);
    return kQOS64Bytes;

} // TIrQOS::GetDataSize


//--------------------------------------------------------------------------------
//      GetWindowSize
//--------------------------------------------------------------------------------
ULong TIrQOS::GetWindowSize()
{
    ULong size;
    
    require(fWindowSize, Fail);
    
    size = HighestBitOn(fWindowSize) + 1;
    
    XTRACE(kLogGetWindowSize, 0, size);
    
    return size;
    
Fail:
    XTRACE(kLogGetWindowSizeFailed, 0xffff, 1);
    return 1;

} // TIrQOS::GetWindowSize


//--------------------------------------------------------------------------------
//      GetExtraBOFs
//--------------------------------------------------------------------------------
ULong TIrQOS::GetExtraBOFs()
{
    ULong result;
    ULong bofs;
    
    require(fNumExtraBOFs, Fail);
    
    bofs = IrExtraBOFsTable[HighestBitOn(fNumExtraBOFs)];       // map to actual bof count
    
    result = (bofs * GetBaudRate()) / 115200;                   // normalize per 115k bps base
    
    XTRACE(kLogGetBofs, 0, result);
    
    // Return extra BOFs adjusted for the actual baud rate
    return result;

Fail:
    XTRACE(kLogGetBofsFailed, 0xffff, 48);
    return 48;

} // TIrQOS::GetExtraBOFs


//--------------------------------------------------------------------------------
//      GetMinTurnAroundTime
//--------------------------------------------------------------------------------
TTimeout TIrQOS::GetMinTurnAroundTime()
{
    ULong bitpos;
    TTimeout result;
    
    require(fMinTurnAroundTime, Fail);
    
    bitpos = HighestBitOn(fMinTurnAroundTime);
    result = IrMinTurnTimeTable[bitpos];
    XTRACE(kLogGetMin, result >> 16, result);
    return result;
    
Fail:
    XTRACE(kLogGetMinFailed, 0, IrMinTurnTimeTable[0]);
    return IrMinTurnTimeTable[0];

} // TIrQOS::GetMinTurnAroundTime


//--------------------------------------------------------------------------------
//      GetLinkDiscThresholdTime
//--------------------------------------------------------------------------------
TTimeout TIrQOS::GetLinkDiscThresholdTime()
{
    TTimeout result;
    ULong bitpos;
    
    require(fLinkDiscThreshold, Fail);
    
    bitpos = HighestBitOn(fLinkDiscThreshold);
    result = IrLinkDiscThreshold[bitpos];
    XTRACE(kLogGetDisconnect, result >> 16, result);
    
    return result;
    
Fail:
    XTRACE(kLogGetDisconnectFailed, 0xffff, IrLinkDiscThreshold[0]);
    return IrLinkDiscThreshold[0];

} // TIrQOS::GetLinkDiscThresholdTime


//--------------------------------------------------------------------------------
//      AddInfoToBuffer
//--------------------------------------------------------------------------------
ULong TIrQOS::AddInfoToBuffer(UByte* buffer, ULong maxBytes)
{
    // Make sure my parms are consistent before sending them out
    IrDAErr result;
    
    XTRACE(kLogAddInfo, 0, maxBytes);
    
    result = NormalizeInfo();
    require(result == noErr, Bogus);
    require(maxBytes >= (kQOSNumberOfIdentifiers * 3 + 1), Bogus);      // Add the extra byte for the baud rate
    
    // Add baud rate
    *buffer++ = kQOSBaudRateId;
    *buffer++ = 2;
    *buffer++ = ( UInt8 )fBaudRate;                 // Low byte goes out first (2400 - 1Mbps)
    *buffer++ = ( UInt8 )( fBaudRate >> 8 );        // High byte second (4Mbps+)

    // Add max turn around time
    *buffer++ = kQOSMaxTurnAroundTimeId;
    *buffer++ = 1;
    *buffer++ = fMaxTurnAroundTime;

    // Add data size
    *buffer++ = kQOSDataSizeId;
    *buffer++ = 1;
    *buffer++ = fDataSize;

    // Add window size
    *buffer++ = kQOSWindowSizeId;
    *buffer++ = 1;
    *buffer++ = fWindowSize;

    // Add num extra BOFs
    *buffer++ = kQOSNumberOfExtraBOFsId;
    *buffer++ = 1;
    *buffer++ = fNumExtraBOFs;

    // Add min turn around time
    *buffer++ = kQOSMinTurnAroundTimeId;
    *buffer++ = 1;
    *buffer++ = fMinTurnAroundTime;

    // Add link disconnect/threshold
    *buffer++ = kQOSLinkDiscThresholdId;
    *buffer++ = 1;
    *buffer++ = fLinkDiscThreshold;
    
    return kQOSNumberOfIdentifiers * 3 + 1;

Bogus:
    XTRACE(kLogAddInfoFailed, 0xffff, 0);
    
    return 0;

} // TIrQOS::AddInfoToBuffer


//--------------------------------------------------------------------------------
//      ExtractInfoFromBuffer
//--------------------------------------------------------------------------------
IrDAErr TIrQOS::ExtractInfoFromBuffer(CBufferSegment* buffer)
{
    ULong value;
    UByte idLenVal[3];      // id byte, length byte, value byte
    
    XTRACE(kLogExtract, 0, buffer);

    // Preset fields to default values (in case some values are not provided)
    fBaudRate           = kQOS9600bps;
    fMaxTurnAroundTime  = kQOSMaxTurnTime500ms;
    fDataSize           = kQOS64Bytes;
    fWindowSize         = kQOS1Frame;
    fNumExtraBOFs       = kQOS48ExtraBOFs;
    fMinTurnAroundTime  = kQOSMinTurnTime10ms;
    fLinkDiscThreshold  = kQOSDefaultDiscThresholds;

    // Basically, repeat: read id byte, read len byte, read len value bytes, update value.

    while (true) {
	// Need another 3 bytes minimum to continue
	if (buffer->Getn(&idLenVal[0], sizeof(idLenVal)) != sizeof(idLenVal)) break;
	
	XTRACE(kLogExtractData, idLenVal[0], (idLenVal[1] << 8) | idLenVal[2]);

	// Put value into a local (compiler wasn't smart enough, so I'm helping it)
	value = idLenVal[2];

	// Assign the value to the appropriate field.  Mask off unknown bits first.
	// If value after masking is 0, then don't change the field - leave as default.
	switch(idLenVal[0]) {
	    case kQOSBaudRateId:
		// Baud rate is the only field that can have more than one byte in the value
		// The low order byte is sent first (< 4Mbps), followed by the high order byte (4Mbps or greater) 
		if (value & kQOSValidBaudRatesLow) {
		    fBaudRate = (UByte)(value & kQOSValidBaudRatesLow);
		}
		if( idLenVal[1] == 2 ) {                // Check if 4 Mbps or greater field was sent
		    value = buffer->Peek();             // Get it out of the buffer, but don't advance
		    value &= kQOSValidBaudRatesHigh;    // that's done later. Mask off unused bits
		    fBaudRate += value << 8;            // Put it into the high byte of the baud rate member
		}
		break;

	    case kQOSMaxTurnAroundTimeId:
		if (value & kQOSValidMaxTurnTimes) {
		    fMaxTurnAroundTime = (UByte)(value & kQOSValidMaxTurnTimes);
		}
		break;

	    case kQOSDataSizeId:
		if (value & kQOSValidDataSizes) {
		    fDataSize = (UByte)(value & kQOSValidDataSizes);
		}
		break;

	    case kQOSWindowSizeId:
		if (value & kQOSValidWindowSizes) {
		    fWindowSize = (UByte)(value & kQOSValidWindowSizes);
		}
		break;

	    case kQOSNumberOfExtraBOFsId:
		if (value & kQOSValidExtraBOFs) {
		    fNumExtraBOFs = (UByte)(value & kQOSValidExtraBOFs);
		}
		break;

	    case kQOSMinTurnAroundTimeId:
		if (value & kQOSValidMinTurnTimes) {
		    fMinTurnAroundTime = (UByte)(value & kQOSValidMinTurnTimes);
		}
		break;

	    case kQOSLinkDiscThresholdId:
		if (value & kQOSValidDiscThresholds) {
		    fLinkDiscThreshold = (UByte)(value & kQOSValidDiscThresholds);
		}
		break;

	    default:
		// Ignore other negotiation parameters I don't understand
		break;
	}

	// If length is something other than 1 then skip additional value fields
	if (idLenVal[1] > 1) {
	    buffer->Seek(idLenVal[1] - 1, kPosCur);
	}
    }

    return noErr;

} // TIrQOS::ExtractInfoFromBuffer


//--------------------------------------------------------------------------------
//      NegotiateWith
//--------------------------------------------------------------------------------
IrDAErr TIrQOS::NegotiateWith(TIrQOS* peerDeviceQOS)
{
    require(peerDeviceQOS, Fail);
    
    XTRACE(kLogNegotiateBaud1, peerDeviceQOS->fBaudRate, fBaudRate);
    XTRACE(kLogNegotiateBaud2, 0, fBaudRate & peerDeviceQOS->fBaudRate);
    
    // Baud rate is intersection of my values and peer devices values
    fBaudRate &= peerDeviceQOS->fBaudRate;

    // Link disconnect/threshold is intersection of my and peer devices values
    fLinkDiscThreshold &= peerDeviceQOS->fLinkDiscThreshold;

    // Can't connect if no agreement on baud rate and/or link disconnect threshold
    if ((fBaudRate == 0) || (fLinkDiscThreshold == 0)) {
	return errIncompatibleRemote;
    }

    // Make sure that my parms are still consistent in case baud rate changed
    return NormalizeInfo();

Fail:
    return errIncompatibleRemote;

} // TIrQOS::NegotiateWith


//--------------------------------------------------------------------------------
//      NormalizeInfo
//--------------------------------------------------------------------------------
IrDAErr TIrQOS::NormalizeInfo()
{
    IrDAErr result = noErr;
    ULong minTurnTimeInBytes;
    ULong maxLineCapacity;
    ULong extraBOFs;
    ULong maxWindowsBit;
    Boolean firSpeed = GetBaudRate() > k115200bps;
    
    require(fBaudRate, Bogus);
    require(fMinTurnAroundTime, Bogus);
    require(fMaxTurnAroundTime, Bogus);
    require(fWindowSize, Bogus);
    require(fDataSize, Bogus);

    // Lookup minimum turnaround time in bytes from table (based on baud rate)
    {
	ULong speed;    // bit position speed
	ULong minturn;  // bit position min turnaround
	speed = HighestBitOn(fBaudRate >> 1);           // shifting out 2400 bps and shifting in 4mbit
	minturn = HighestBitOn(fMinTurnAroundTime);
	minTurnTimeInBytes = IrMinTurnInBytesTable[speed][minturn]; // convert to byte count
	
	XTRACE(kLogNormMinTurn, (speed << 8) | minturn, minTurnTimeInBytes);
    }

    // Lookup maximum line capacity
    {
	ULong   maxTurn = HighestBitOn(fMaxTurnAroundTime);
	require(maxTurn < 4, Bogus);
	
	switch( GetBaudRate() )
	{
	    case k4Mbs:
		maxLineCapacity = IrMaxLineCapacityTable4Mbps[maxTurn];
		break;
		
	    case k1Mbps:
		maxLineCapacity = IrMaxLineCapacityTable1Mbps[maxTurn];
		break;
		
	    case k576000bps:
		maxLineCapacity = IrMaxLineCapacityTable576[maxTurn];
		break;
		
	    case k115200bps:
		maxLineCapacity = IrMaxLineCapacityTable2[maxTurn];
		break;
		
	    default: 
		maxLineCapacity = IrMaxLineCapacityTable1[HighestBitOn(fBaudRate>>1)];
	}
	XTRACE(kLogNormMaxLine, maxLineCapacity >> 16, maxLineCapacity);
    }   

    extraBOFs = GetExtraBOFs();                 // Don't need these for FIR 

    // make sure windowsize and datasize are proper bitmaps
    {
	UInt8 old_windowsize, old_datasize;         // debugging
	UInt8 bitpos;
	
	old_windowsize = fWindowSize;
	old_datasize   = fDataSize;
	
	bitpos = HighestBitOn(fWindowSize);         // All window sizes below maxWindow are valid
	if (bitpos > 0)                             // if max windowsize > 1
	    fWindowSize |= (1 << bitpos) -1;        // then turn on all the lower bits too
	
	bitpos = HighestBitOn(fDataSize);           // get max packet size
	if (bitpos > 0)                             // if more than min size (64 bytes)
	    fDataSize |= (1 << bitpos) -1;          // then turn on all the lower bits too
	    
	if (old_windowsize != fWindowSize)
	    XTRACE(kLogNormWindow, old_windowsize, fWindowSize);
	    
	if (old_datasize != fDataSize)
	    XTRACE(kLogNormData, old_datasize, fDataSize);
    }

    // Pare things down until they fit
    while (true) {
	ULong requestedLineCapacity;
	if( firSpeed )
	    requestedLineCapacity = ( ( GetDataSize() + 4 ) * GetWindowSize() ) + minTurnTimeInBytes;
	else
	    requestedLineCapacity = ((GetDataSize() + 6 + extraBOFs) * GetWindowSize()) + minTurnTimeInBytes;
	
	XTRACE(kLogNormRequested, requestedLineCapacity >> 16, requestedLineCapacity); 
	
	if (requestedLineCapacity < maxLineCapacity) break;

	// First decrement window (if more than one choice specified)
	maxWindowsBit = HighestBitOn(fWindowSize);
	if (maxWindowsBit != 0) {               // if more than a single window left
						// Turn off high bit (reduce window count by 1)
	    fWindowSize &= (UByte)(~(1 << maxWindowsBit));
	    XTRACE(kLogNormSmallerWindow, 0, fWindowSize);
	    require(fWindowSize, Bogus);            // sanity check, should never hit this
	}

	// If at only one window left, try decrementing buffer size instead
	else {
	    // Turn off high bit
	    fDataSize &= (UByte)(~(1 << HighestBitOn(fDataSize)));
	    XTRACE(kLogNormSmallerData, 0, fDataSize);
	    if (fDataSize == 0) {
		result = errIncompatibleRemote;
		break;
	    }
	}
    }

    return result;

Bogus:
    return errIncompatibleRemote;

} // TIrQOS::NormalizeInfo


//--------------------------------------------------------------------------------
//      HighestBitOn
//--------------------------------------------------------------------------------
ULong TIrQOS::HighestBitOn(UByte aByte)
{
    ULong bitPosition;
    UByte bitMask = 0x80;
    
    require(aByte != 0, Fail);

    for (bitPosition = 7, bitMask = 0x80; bitMask != 0; bitPosition--, bitMask >>= 1) {
	if ((aByte & bitMask) != 0) {
	    break;
	}
    }

    return bitPosition;

Fail:
    return (ULong)-1;

} // TIrQOS::HighestBitOn

