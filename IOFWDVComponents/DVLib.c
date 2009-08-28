/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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

#include "DVLib.h"
#include <pthread.h>
#include <mach/mach_port.h>
#include <mach/thread_act.h>
#include <mach/vm_map.h>
#include <mach/mach_time.h>

#include <syslog.h>	// Debug messages

#include <IOKit/IOMessage.h>

#include <IOKit/firewire/IOFireWireLibIsoch.h>
#include <IOKit/avc/IOFireWireAVCConsts.h>

#include <IOKit/pwr_mgt/IOPMLib.h>

#include "IsochronousDataHandler.h"

#define kDVRequestID 0

#define ALT_TIMING 1

//#define USE_P2P_CONNECTIONS_FOR_DV_READ 1

#define PAGE_SIZE 4096;
#define kNoPlug 0xdeadbeef

// Use Channel 62 to send DV, and 63 to receive (because camcorders love to send on 63).
#define kWriteChannel 62
#define kReadChannel 61

enum {
    kNoP2PConnection = 0,
    kWriteP2PConnection = 1,
    kReadP2PConnection = 2
};

// AY_TODO: Move the following to IOFireWireAVCConsts.h
#define kAVCSignalModeDVCPro50_525_60	0x74
#define kAVCSignalModeDVCPro50_625_50	0xF4
#define kAVCSignalModeDVCPro100_525_60	0x70
#define kAVCSignalModeDVCPro100_625_50	0xF0
#define kAVCSignalModeMask_DVCPro50		0x74
#define kAVCSignalModeMask_DVCPro100	0x70
#define kAVCSignalModeMask_HD			0x08

enum {
    kDVRunning = 0,
    kDVStopped = 1,
    kDVWriteOverrun = 2,
    kDVReadOverrun = 3,
    kDVStopping = 4
};

// Isoc packet header, 2nd quad
enum {
    kPALBit									= 0x800000,	// 1 bit
    kSTypeMask								= 0x7c0000,	// 5 bits
    kSTypeSD								= 0x00000,
    kSTypeSDL								= 0x40000,
    kSTypeHD								= 0x80000
};

enum {

    // DVRead
    kNumPacketsPerInputBuffer               = 100,
    kDVSDPayloadPacketSize                  = 480,
    kDVSDLPayloadPacketSize                 = 240,
    kDVHDPayloadPacketSize               	= 960,
	kDVCPro50PayloadPacketSize				= 960,
	kDVCPro100PayloadPacketSize				= 1920,

    kNumPingPongs                           = 8,		// Number of DCL blocks in read program
    kNumPacketsPerPingPong                  = 100,
    kNumDCLsPerPingPongPacket               = 1,
    kRecordNumDCLs                          =
        kNumPingPongs * (kNumPacketsPerPingPong * kNumDCLsPerPingPongPacket+3)+6,
    kMaxDCLSize                             = 32,
    kRecordDCLProgramSize                   = kMaxDCLSize * kRecordNumDCLs,
};

enum {
    kNTSCFrameRateNumerator                 = 2997,
    kNTSCFrameRateDenominator               = 100,
    kNTSCPlayFramePeriodNumerator           = kNTSCFrameRateDenominator,
    kNTSCPlayFramePeriodDenominator         = kNTSCFrameRateNumerator,
    kNTSCNumDataPacketsPerDVFrame           = 250,
    kNTSCNumDataPacketsPerGroup				= 125,

    kPALFrameRateNumerator                  = 25,
    kPALFrameRateDenominator                = 1,
    kPALPlayFramePeriodNumerator            = kPALFrameRateDenominator,
    kPALPlayFramePeriodDenominator          = kPALFrameRateNumerator,
    kPALNumDataPacketsPerDVFrame            = 300,
    kPALNumDataPacketsPerGroup				= 150,
    
    kDVPacketAlignSlop                      = 8,       // add 8 bytes to
    kDVPacketCIPSize                        = 8,
    kPlaySYTDelay                           = 3, 		// Sony camcorders send a delay of 2, but VX2000 wants 3 from us...
};

// Frame Sizes
enum {
    kFrameSize_SD525_60						= 120000,
    kFrameSize_DVCPro525_60					= 120000,
    kFrameSize_SD625_50						= 144000,
    kFrameSize_DVCPro625_50					= 144000,
    kFrameSize_SDL525_60					= 60000,
    kFrameSize_SDL625_50					= 72000,
    kFrameSize_DVCPro50_525_60				= 240000,
    kFrameSize_HD1125_60					= 240000,
    kFrameSize_DVCPro50_625_50				= 288000,
    kFrameSize_HD1250_50					= 288000,
    kFrameSize_DVCPro100_525_60				= 480000,
    kFrameSize_DVCPro100_625_50				= 576000
};

typedef struct {
    UInt32					fRequest;
    void *					fParams;
} DVReq;

typedef struct {
    mach_msg_header_t	msgHdr;
    DVReq		dvRequest;
} SendMsg;

typedef struct {
    mach_msg_header_t	msgHdr;
    DVReq			dvRequest;
    mach_msg_trailer_t	trailer;
} ReceiveMsg;


struct DVLocalOutStruct;
typedef struct DVLocalOutStruct DVLocalOut, *DVLocalOutPtr;

typedef struct _DVStreamStruct {
    IOFireWireLibDeviceRef pFWDevice;
    IOFireWireAVCLibProtocolInterface **fAVCProtoInterface;
    IOFireWireLibLocalIsochPortRef pFWLocalIsochPort;
    IOFireWireLibRemoteIsochPortRef pFWRemoteIsochPort;
	IOFireWireLibIsochChannelRef fIsochChannelRef;
    UInt32			fPlug;			// PCR plug register of local node
    DVThread *		fThread;
    DVFrameVars		fFrames;
    DCLCommandPtr	pDCLList;               // DCLs used for playing.
    UInt8 *         fDCLBuffers;       // Buffers to use to transfer packet data.
    UInt32 			fDCLBufferSize;	// Total allocation for output buffers
    UInt64			fChannelMask;	// Legal channels to use
    UInt8			fSignalMode;			// signal type
    UInt8			fIsocChannel;	// Channel to use
    UInt8			fMaxSpeed;		// Max bus speed for isoc channel
    DVDevice		*pDVDevice;
	UInt32			fDVFrameSize;	// Frame size based on current signal mode
} DVStream;

// structs

struct DVLocalOutStruct
{
    DVLocalOutPtr	pNextLocalData;
    DVLocalOutPtr	pPrevLocalData;
    DVGlobalOutPtr	pGlobalData;
    // Pointer to jump command to end of buffer group.
    DCLJumpPtr		pEndOfBufferGroupDCLJump;
	// Pointer to label command at start of buffer group.
    DCLLabelPtr		pStartOfBufferGroupDCLLabel;
	// Pointer to jump command to use to skip an empty packet.
    DCLJumpPtr		pBufferGroupSkipEmptyPacketDCLJump;
    // Label to jump to to skip an empty packet.
    DCLLabelPtr		pBufferGroupSkipEmptyPacketDCLLabel;
    // Label to jump to to not skip an empty packet.
    DCLLabelPtr		pBufferGroupDontSkipEmptyPacketDCLLabel;
    // Pointer to buffer group's time stamp.
    UInt32 *		pBufferGroupTimeStampPtr;
    // Pointer to first DCL command in buffer group.
    DCLCommandPtr	pFirstBufferGroupDCLCommand;
    // Pointer to last DCL command in buffer group.
    DCLCommandPtr	pLastBufferGroupDCLCommand;

    // DCL update list with just the buffer group's time stamp DCL.
    DCLCommandPtr	timeStampUpdateDCLList;
    // List of DCL commands that need updating for each run through buffer group.
    DCLCommandPtr *	bufferGroupUpdateDCLList;
    // Number of DCL commands in above list.
    UInt32			updateListSize;

    // Number of packets in this buffer group.
    UInt32			numPackets;
    UInt32				fBlockNum;
    // True if pBufferGroupSkipEmptyPacketDCLJump is set to skip an empty packet.
    bool			skippingEmptyPacket;
};

// DVGlobalOut

struct DVGlobalOutStruct {
    DVStream	fStreamVars;
    UInt8 *			  fDCLCommandPool;
    UInt32			  fTotalPool;
    UInt32			  fAllocatedPool;
    DVSharedVars      fSharedDCLVars;

    // output structures
    DVLocalOut		fLocalDataArray[kNumPlayBufferGroups];   // List of buffer group data records.
    UInt8 *           pEmptyTransmitBuffers;  // Buffer to use for empty transmit packet data.
    DCLLabelPtr       pUnderrunDCLLabel;      // Pointer to underrun label.
    UInt32            totalPackets;           // Total number of packets for playing.
    UInt32            activePackets;          // Total number of active packets for playing.
    UInt32            nominalFrameCycleTime;  // Nominal cycle time between frames.
    UInt32            nextSYT;                // Next SYT to use on frame boundary.
    UInt32            nextDBC;                // Next DBC to use for first packet of next buffer group.
    UInt32            numDataPacketsPerFrame; // number of data packets per frame (different for NTSC or PAL)
    UInt32			  numDataPacketsPerGroup; // Number of data packets in each group (different for NTSC or PAL)
    UInt32            playFramePeriodNumerator;
    UInt32            playFramePeriodDenominator;
    UInt32			  playFrameRateNumerator, playFrameRateDenominator;
    UInt32 fDataPacketSize;		// Data portion, in bytes
    UInt32 fDataQuadSize;		// Data portion, in quads
    UInt32 fAlignQuadSize;		// Packet size in quads, padded to 16 byte boundary.
    UInt32 fHeader0;			// First quad - sourceID, data size, seq. no.
    UInt32 fHeader1;			// Second quad - format, PAL/NTSC, SType, rsv, SYT
    // channel buffer structures
    UInt32            nextDataPacketNum;      // Data packet number for first data packet of next buffer group.
    UInt32 *          pImageBuffer;           // Buffer to hold image in.
    bool			fUpdateBuffers;			  // Our job to copy image data?
    bool pendingDVWriteUnderrunHandler;
    bool deferredDVWriteFree;
    bool dvWriteStopInProgress;
};

typedef struct DVLocalInStruct
{
    DVGlobalInPtr		pGlobalData;
    DCLCommandPtr		fFirstCmd;
    DCLLabelPtr			fStateLabel;
    DCLJumpPtr			fStateJmp;
    UInt32				fBlockNum;
    UInt32 *			fTimeStampPtr;
} DVLocalIn, *DVLocalInPtr;

struct DVGlobalInStruct
{
    DVStream	fStreamVars;
    DCLCommandPtr *ppUpdateDCLList;
    UInt32 packetCount;
    UInt32 fLastFrameTime;
    DVLocalIn fLocalDataArray[kNumPingPongs];
    DCLLabelPtr fTerminal;
    UInt8 *pImageBuffer;
    UInt8 lastSequenceCount;
    UInt8 fState;			// Current DCL block
    UInt8 fSynced;
    UInt8 fRestarted;
    bool pendingDVReadUnderrunHandler;
    bool deferredDVReadFree;
    bool dvReadStopInProgress;
};

static IOReturn buildWriteProgram(DVGlobalOutPtr pGlobalData);
static IOReturn allocateBuffers(DVGlobalOutPtr pGlobalData);
static void DVWritePoll(DVGlobalOutPtr globs);
static void DVReadPoll(DVGlobalInPtr globs);
static void closeStream(DVStream *stream);
static void doDVReadHandleInputUnderrun( DVGlobalInPtr pGlobalData );
static void doDVHandleOutputUnderrun(  DVGlobalOutPtr	pGlobalData );

#include <CoreFoundation/CFRuntime.h>

#if ( MAC_OS_X_VERSION_MIN_REQUIRED >= 1060 ) && defined(CF_USING_COLLECTABLE_MEMORY)
#define REGISTER_THREADS_WITH_GC 1
#else // MAC_OS_X_VERSION_MIN_REQUIRED < 1060
#define REGISTER_THREADS_WITH_GC 0
#endif // MAC_OS_X_VERSION_MIN_REQUIRED < 1060

#if REGISTER_THREADS_WITH_GC
#include <dlfcn.h>
typedef void (*ObjCRegisterThreadWithCollectorPtr)(void);
#endif REGISTER_THREADS_WITH_GC

UInt32  AddFWCycleTimeToFWCycleTime( UInt32 cycleTime1, UInt32 cycleTime2 )
{
    UInt32    secondCount,
              cycleCount,
              cycleOffset;
    UInt32    cycleTime;

    // Add cycle offsets.
    cycleOffset = (cycleTime1 & 0x0FFF) + (cycleTime2 & 0x0FFF);

    // Add cycle counts.
    cycleCount = (cycleTime1 & 0x01FFF000) + (cycleTime2 & 0x01FFF000);

    // Add any carry over from cycle offset to cycle count.
    if (cycleOffset > 3071)
    {
        cycleCount += 0x1000;
        cycleOffset -= 3072;
    }

    // Add secondCounts.
    secondCount = (cycleTime1 & 0xFE000000) + (cycleTime2 & 0xFE000000);

    // Add any carry over from cycle count to secondCount.
    if (cycleCount > (7999 << 12))
    {
        secondCount += 0x02000000;
        cycleCount -= (8000 << 12);
    }

    // Put everything together into cycle time.
    cycleTime = secondCount | cycleCount | cycleOffset;

    return (cycleTime);
}

UInt32 SubtractFWCycleTimeFromFWCycleTime( UInt32 cycleTime1, UInt32 cycleTime2)
{
    SInt32 secondCount,
           cycleCount,
           cycleOffset;
    UInt32 cycleTime;

    // Subtract cycle offsets.
    cycleOffset = (cycleTime1 & 0x0FFF) - (cycleTime2 & 0x0FFF);

    // Subtract cycle counts.
    cycleCount = (cycleTime1 & 0x01FFF000) - (cycleTime2 & 0x01FFF000);

    // Subtract any borrow over from cycle offset to cycle count.

    if (cycleOffset < 0)
    {
        cycleCount -= 0x1000;
        cycleOffset += 3072;
    }

    // Subtract secondCounts.
    secondCount = (cycleTime1 & 0xFE000000) - (cycleTime2 & 0xFE000000);

    // Subtract any borrow over from cycle count to secondCount.
    if (cycleCount < 0)
    {
        secondCount -= 0x02000000;
        cycleCount += (8000 << 12);
    }

    // Put everything together into cycle time.
    cycleTime = secondCount | cycleCount | cycleOffset;

    return (cycleTime);
}

static UInt32 ConvertFractionalSecondsToFWCycleTime( UInt32 secondsNumerator, UInt32 secondsDenominator )
{
    // float  fSecondCount;
    // float  fCycleCount;
    // float  fCycleOffset;
    // UInt32 iSecondsCount;
    // UInt32 iCycleCount;
    // UInt32 iCycleOffset;
    // UInt32 secondsCycleTime;

    UInt32 iSecondsCount2;
    UInt32 iCycleCount2;
    UInt32 iCycleOffset2;
    UInt32 mSecondCount;
    UInt32 mCycleCount;
    UInt32 secondsCycleTime2;

    // Convert fractional seconds into floating point and compute seconds count.
    // fSecondCount = ((float) secondsNumerator) / ((float) secondsDenominator);
    // iSecondsCount = (UInt32) fSecondCount;
    iSecondsCount2 = secondsNumerator / secondsDenominator;
    mSecondCount = secondsNumerator % secondsDenominator;

    // Subtract whole seconds out of fSecondCount and convert to cycle count.
    // fCycleCount = (fSecondCount - ((float) iSecondsCount)) * 8000.0;
    // iCycleCount = (UInt32) fCycleCount;
    iCycleCount2 = (mSecondCount * 8000) / secondsDenominator;
    mCycleCount = (mSecondCount * 8000) % secondsDenominator;

    // Subtract whole cycles out of fCycleCount and convert to cycle offset.
    // fCycleOffset = (fCycleCount - ((float) iCycleCount)) * 3072.0;
    // iCycleOffset = (UInt32) fCycleOffset;
    iCycleOffset2 = (mCycleCount * 3072) / secondsDenominator;

    // Convert to cycle timer format.
    // secondsCycleTime = (iSecondsCount << 25) | (iCycleCount << 12) | iCycleOffset;
    secondsCycleTime2 = (iSecondsCount2 << 25) | (iCycleCount2 << 12) | iCycleOffset2;

    return (secondsCycleTime2);
}

static IOReturn writePlug(IOFireWireAVCLibProtocolInterface **interface, UInt32 plug, UInt32 val)
{
    return (*interface)->updateOutputPlug(interface, plug, 
        (*interface)->readOutputPlug(interface, plug), val);
}

static void handlePCRLock(void *refcon, UInt32 generation, UInt16 nodeID, UInt32 plug,
                                                                    UInt32 oldVal, UInt32 newVal)
{
    //syslog(LOG_INFO, "Plug %d written to, old val 0x%x new val 0x%x\n", plug, oldVal, newVal);
}

static IOReturn writeDeviceOutputMCR(IOFireWireLibDeviceRef interface, UInt32 mask, UInt32 val)
{
    UInt32 oldVal, newVal;
	UInt32 oldValHost, newValHost;
    IOReturn err;
    FWAddress addr;
    io_object_t obj;
    
    addr.nodeID = 0;
    addr.addressHi = 0xffff;
    addr.addressLo = 0xf0000900;
    obj = (*interface)->GetDevice(interface);
    err = (*interface)->ReadQuadlet(interface, obj, &addr, &oldVal, false, 0);
	oldValHost = EndianU32_BtoN( oldVal );
    
    if(err == kIOReturnSuccess) {
        if( (oldValHost & mask) != val) {
            newValHost = (oldValHost & ~mask) | val;
			newVal = EndianU32_NtoB( newValHost );
            err = (*interface)->CompareSwap(interface, obj, &addr, oldVal, newVal, false, 0);
        }
    }
    return err;
}

static IOReturn MakeP2PConnectionForWrite(DVDevice *pDVDevice,UInt32 plug, UInt32 chan)
{
    IOReturn err;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: MakeP2PConnectionForWrite begin\n");
#endif
	
	err = (*pDVDevice->fAVCInterface)->makeP2PInputConnection(pDVDevice->fAVCInterface, plug, chan);
    if (err == kIOReturnSuccess)
    {
        pDVDevice->p2pConnected = kWriteP2PConnection;
        pDVDevice->p2pPlug = plug;
        pDVDevice->p2pChan = chan;
    }

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: MakeP2PConnectionForWrite end\n");
#endif
	
	return err;
}

static IOReturn BreakP2PConnectionForWrite(DVDevice *pDVDevice,UInt32 plug, UInt32 chan)
{
    IOReturn err;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: BreakP2PConnectionForWrite begin\n");
#endif
	
	err = (*pDVDevice->fAVCInterface)->breakP2PInputConnection(pDVDevice->fAVCInterface, plug);

	// Always clear the connected flag, even if there was an error.
    pDVDevice->p2pConnected = kNoP2PConnection;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: BreakP2PConnectionForWrite end\n");
#endif

	return err;
}

static IOReturn MakeP2PConnectionForRead(DVDevice *pDVDevice,UInt32 plug, UInt32 chan)
{
    IOReturn err;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: MakeP2PConnectionForRead begin\n");
#endif
	
	err = (*pDVDevice->fAVCInterface)->makeP2POutputConnection(pDVDevice->fAVCInterface, plug,chan,kFWSpeedInvalid);
    if (err == kIOReturnSuccess)
    {
        pDVDevice->p2pConnected = kReadP2PConnection;
        pDVDevice->p2pPlug = plug;
        pDVDevice->p2pChan = chan;
    }

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: MakeP2PConnectionForRead end\n");
#endif
	
	return err;
}

static IOReturn BreakP2PConnectionForRead(DVDevice *pDVDevice,UInt32 plug, UInt32 chan)
{
    IOReturn err;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: BreakP2PConnectionForRead begin\n");
#endif
	
	err = (*pDVDevice->fAVCInterface)->breakP2POutputConnection(pDVDevice->fAVCInterface, plug);

	// Always clear the connected flag, even if there was an error.
    pDVDevice->p2pConnected = kNoP2PConnection;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: BreakP2PConnectionForRead end\n");
#endif
	
    return err;
}

void AVCUnitMessageCallback(void * refCon, UInt32 type, void * arg )
{
    DVDevice *pDVDevice = (DVDevice*) refCon;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: AVCUnitMessageCallback begin, type = 0x%08X\n",type);
#endif
	
    // If this is a bus-reset notification, see if we have a p2p connection.
    // If so, restore the P2P connection, do on real time thread for safety.
    // Done by kernel now.
    // Callback the client's message notification handler
    if (pDVDevice->fThread->fDeviceMessage != NULL)
        pDVDevice->fThread->fDeviceMessage((void*)pDVDevice->deviceIndex,type,arg);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: AVCUnitMessageCallback end\n");
#endif
    
    return;
}

static IOReturn getSignalMode(IOFireWireAVCLibUnitInterface **avc, UInt8 *mode)
{
    UInt32 size;
    UInt8 cmd[4],response[4];
    IOReturn res;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: getSignalMode begin\n");
#endif
	
    // build query Output Signal Mode command
    cmd[0] = kAVCStatusInquiryCommand;
    cmd[1] = IOAVCAddress(kAVCTapeRecorder, 0);
    cmd[2] = kAVCOutputSignalModeOpcode;
    cmd[3] = kAVCSignalModeDummyOperand;
    size = 4;
    res = (*avc)->AVCCommand(avc, cmd, 4, response, &size);
    if(res == kIOReturnSuccess) {
        *mode =  response[3];
    }

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: getSignalMode end\n");
#endif
	
	return res;
}

static bool isDVCPro(IOFireWireAVCLibUnitInterface **avc, UInt8 *pMode)
{
    UInt32 size;
    UInt8 cmd[10],response[10];
    IOReturn res;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: isDVCPro begin\n");
#endif
	
    // build query vender-dependent command (is DVCPro?).
    cmd[0] = kAVCStatusInquiryCommand;
    cmd[1] = kAVCUnitAddress;
    cmd[2] = kAVCVendorDependentOpcode;
    cmd[3] = 0;
    cmd[4] = 0x80;
    cmd[5] = 0x45;
    cmd[6] = 0x82;
    cmd[7] = 0x48;
    cmd[8] = 0xff;
    cmd[9] = 0xff;
    size = 10;
    res = (*avc)->AVCCommand(avc, cmd, 10, response, &size);

	// If it is DVCPro50, see if its 25 or 50
	if ((res == kIOReturnSuccess) && (response[0] == kAVCImplementedStatus))
	{
		cmd[0] = kAVCStatusInquiryCommand;
		cmd[1] = kAVCUnitAddress;
		cmd[2] = kAVCOutputPlugSignalFormatOpcode;
		cmd[3] = 0;
		cmd[4] = 0xFF;
		cmd[5] = 0xFF;
		cmd[6] = 0xFF;
		cmd[7] = 0xFF;
		size = 8;

		res = (*avc)->AVCCommand(avc, cmd, 8, response, &size);

		if (res == kIOReturnSuccess && response[0] == kAVCImplementedStatus)
			*pMode = response[5];
		else
			*pMode = 0x00;

#ifdef kIDH_Verbose_Debug_Logging
		syslog(LOG_INFO, "DVLib: isDVCPro end:true\n");
#endif
		
		return true;
	}
	else
	{

#ifdef kIDH_Verbose_Debug_Logging
		syslog(LOG_INFO, "DVLib: isDVCPro end:false\n");
#endif
		return false;
	}
}

static bool isSDL(IOFireWireAVCLibUnitInterface **avc, UInt8 signalMode)
{
    // Try setting input mode to SDL
    IOReturn res;
    bool hasSDL;
    
    UInt32 size;
    UInt8 cmd[4],response[4];

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: isSDL begin\n");
#endif
	
    cmd[0] = kAVCControlCommand;
    cmd[1] = IOAVCAddress(kAVCTapeRecorder, 0);
    cmd[2] = kAVCInputSignalModeOpcode;
    cmd[3] = (signalMode & ~kAVCSignalModeMask_STYPE) | kAVCSignalModeMask_SDL;
    size = 4;
    res = (*avc)->AVCCommand(avc, cmd, 4, response, &size);
    if(res != kIOReturnSuccess || response[0] != kAVCAcceptedStatus)
	{
#ifdef kIDH_Verbose_Debug_Logging
		syslog(LOG_INFO, "DVLib: isSDL end:false\n");
#endif
        return false;	// Failed to set to SDL
	}
        
    cmd[0] = kAVCStatusInquiryCommand;
    cmd[1] = IOAVCAddress(kAVCTapeRecorder, 0);
    cmd[2] = kAVCInputSignalModeOpcode;
    cmd[3] = kAVCSignalModeDummyOperand;
    size = 4;
    res = (*avc)->AVCCommand(avc, cmd, 4, response, &size);
    hasSDL = (response[3] & kAVCSignalModeMask_STYPE) == kAVCSignalModeMask_SDL;
    
    // Set back to original value
    cmd[0] = kAVCControlCommand;
    cmd[1] = IOAVCAddress(kAVCTapeRecorder, 0);
    cmd[2] = kAVCInputSignalModeOpcode;
    cmd[3] = signalMode;
    size = 4;
    res = (*avc)->AVCCommand(avc, cmd, 4, response, &size);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: isSDL end:%s\n",(hasSDL==true ? "true" : "false"));
#endif
    
    return hasSDL;
}

static bool isMPEG(IOFireWireAVCLibUnitInterface **avc)
{
    UInt32 size;
    UInt8 cmd[8],response[8];
    IOReturn res;

	cmd[0] = kAVCStatusInquiryCommand;
	cmd[1] = kAVCUnitAddress;
	cmd[2] = kAVCOutputPlugSignalFormatOpcode;
	cmd[3] = 0;
	cmd[4] = 0xFF;
	cmd[5] = 0xFF;
	cmd[6] = 0xFF;
	cmd[7] = 0xFF;
	size = 8;

	res = (*avc)->AVCCommand(avc, cmd, 8, response, &size);

	if ((res == kIOReturnSuccess) &&
	 (response[0] == kAVCImplementedStatus) &&
	 (response[4] == 0xA0))
		return true;
	else
		return false; 
}

static void deviceArrived(void *refcon, io_iterator_t iterator )
{
    io_object_t obj;
    DVThread * dvThread = (DVThread *)refcon;
	UInt8 dvcProMode;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: deviceArrived begin\n");
#endif
    
    //syslog(LOG_INFO,"deviceArrived(0x%x, 0x%x)\n", refcon, iterator);
    while(obj = IOIteratorNext(iterator)) {
        CFMutableDictionaryRef properties;
        CFNumberRef dataDesc;
        CFStringRef strDesc;
        kern_return_t err;
        UInt64 GUID;
        int refound = 0;
        int device;
        DVDevice *dev = NULL;
        
        //syslog(LOG_INFO, "object 0x%x arrived!\n", obj);
        err = IORegistryEntryCreateCFProperties(obj, &properties, kCFAllocatorDefault, kNilOptions);

        dataDesc = (CFNumberRef)CFDictionaryGetValue(properties, CFSTR("GUID"));
        CFNumberGetValue(dataDesc, kCFNumberSInt64Type, &GUID);
        for(device=0; device<dvThread->fNumDevices; device++) {
            if(GUID == dvThread->fDevices[device].fGUID) {
                refound = 1;
                dev = &dvThread->fDevices[device];
                break;
            }
        }
        if(!refound) {
            CFBooleanRef hasFCP;
            device = dvThread->fNumDevices;
            dvThread->fNumDevices++;
            dev = &dvThread->fDevices[device];
            strDesc = (CFStringRef)CFDictionaryGetValue(properties, CFSTR("FireWire Product Name"));
            if(strDesc) {
                dev->fName[0] = 0;
                CFStringGetCString(strDesc, dev->fName, sizeof(dev->fName), kCFStringEncodingMacRoman);
            }
            hasFCP = (CFBooleanRef)CFDictionaryGetValue(properties, CFSTR("supportsFCP"));
            dev->fSupportsFCP = true;
            if(hasFCP)
                dev->fSupportsFCP = CFBooleanGetValue(hasFCP);

            dev->fGUID = GUID;
            dev->fMaxSpeed = kFWSpeed100MBit;
            dev->fWriteChan = kWriteChannel;
            dev->fReadChan = kReadChannel;
       }
        CFRelease(properties);
        
        dev->fObject = obj;
        dev->fThread = dvThread;
        
        // Request notification of messages via AVC user client
        err = openAVCUnit(dev->fObject, &dev->fAVCInterface, dvThread);
        if(err == kIOReturnSuccess)
		{
            UInt8 mode, stype;

			// Exclude DVCProHD and MPEG devices from the IDH device list!
			if (dev->fSupportsFCP)
			{
				if(isDVCPro(dev->fAVCInterface,&dvcProMode))
				{
					if ((dvcProMode == kAVCSignalModeDVCPro100_525_60) || (dvcProMode == kAVCSignalModeDVCPro100_625_50))
					{
						// Terminate this device
						DVDeviceTerminate(dev);

						// Remove this device from the device list
						dvThread->fNumDevices--;

						continue;	// continue to next device!
					}
				}
				else if (isMPEG(dev->fAVCInterface))
				{
						// Terminate this device
						DVDeviceTerminate(dev);

						// Remove this device from the device list
						dvThread->fNumDevices--;

						continue;	// continue to next device!
				}
			}
			
            dev->deviceIndex = device+1;
            (*dev->fAVCInterface)->setMessageCallback(dev->fAVCInterface, (void *) dev, AVCUnitMessageCallback);

			// Determine mode(s) supported
            if(dev->fSupportsFCP)
			{
                err = getSignalMode(dev->fAVCInterface, &mode);
                if(err == kIOReturnSuccess)
				{
                    if(mode & kAVCSignalModeMask_50)
                        dev->standard = palIn;
					else
						dev->standard = ntscIn;

					// See if DVCPro25 type device
					stype = mode & kAVCSignalModeMask_STYPE;
					if(stype == kAVCSignalModeMask_DVCPro25)
					{
						dev->fDVFormats |= 1 << kIDHDVCPro_25;
					}
					else if(stype == kAVCSignalModeMask_DVCPro50)
					{
						dev->fDVFormats |= 1 << kIDHDVCPro_50;
						dev->fMaxSpeed = kFWSpeed400MBit;	// Default to 400 for DVCPro-50
					}
					else
					{
						// Ask device via vender-dependent command if it's a DVCPro device.
						if(isDVCPro(dev->fAVCInterface,&dvcProMode))
						{
							if((dvcProMode & kAVCSignalModeMask_STYPE) == kAVCSignalModeMask_DVCPro50)
							{
								dev->fDVFormats |= 1 << kIDHDVCPro_50;
								dev->fMaxSpeed = kFWSpeed400MBit;	// Default to 400 for DVCPro-50
							}
							else
								dev->fDVFormats |= 1 << kIDHDVCPro_25;
						}
					}

					// See if SDL type device
					if(stype == kAVCSignalModeMask_SDL)
						dev->fDVFormats |= 1 << kIDHDV_SDL;
					else
					{
						// Ask camera if it's SDL.
						if(isSDL(dev->fAVCInterface, mode))
							dev->fDVFormats |= 1 << kIDHDV_SDL;
					}
				}
				else
				{
					// Failed the signal mode command. Assume standard NTSC DV
					dev->fDVFormats = 1 << kIDHDV_SD;	// Standard DV
					dev->standard = ntscIn;
				}
			}
			else
			{
				// Assume NTSC, standard DV if device doesn't support AVC.
				dev->fDVFormats = 1 << kIDHDV_SD;	// Standard DV
				dev->standard = ntscIn; 			// device standard - NTSC/PAL
			}

            // Notify client
            (dvThread->fAddedFunc)(dvThread->fAddedRefCon, dev, device+1, refound);
		}
    }

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: deviceArrived end\n");
#endif
	
}

static OSStatus DVthreadExit(DVThread *dvThread, UInt32 params)
{

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVthreadExit begin\n");
#endif
	
    if(dvThread->fNotifySource)
        CFRunLoopSourceInvalidate(dvThread->fNotifySource);

    if(dvThread->fPowerNotifySource)
        CFRunLoopSourceInvalidate(dvThread->fPowerNotifySource);

	// we have to do this because CF sometimes adds it's own source to our run loop (?!)
	// which we don't (can't?) invalidate
	// this makes sure our thread will really exit..
	CFRunLoopStop(dvThread->fWorkLoop) ;
    dvThread->fTimerFunc = NULL;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVthreadExit end\n");
#endif
	
    return noErr;
}

static void *DVRTThreadStart(DVThread *dvThread)
{

#if REGISTER_THREADS_WITH_GC
	if (CF_USING_COLLECTABLE_MEMORY)
	{
		void *dlhandle = dlopen("/usr/lib/libobjc.dylib", RTLD_LAZY | RTLD_LOCAL);
		if( dlhandle != NULL ) {
			ObjCRegisterThreadWithCollectorPtr objcRegisterThreadWithCollector = dlsym( dlhandle, "objc_registerThreadWithCollector" );
			if( objcRegisterThreadWithCollector != NULL )
				objcRegisterThreadWithCollector();
			else syslog(LOG_INFO, "dlsym(objc_registerThreadWithCollector) failed");
			dlclose( dlhandle );
		}
		else syslog(LOG_INFO, "dlopen(/usr/lib/libobjc.dylib) failed");
	}
#endif	// REGISTER_THREADS_WITH_GC
	
    ReceiveMsg msg;
    kern_return_t err;
    int delay;
    int run = true;
    int i;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVRTThreadStart begin\n");
#endif
	
    deviceArrived(dvThread, dvThread->fMatchEnumer);
    // signal that we're about to start the mach loop
    DVSignalSync(&dvThread->fRequestSyncer, &dvThread->fSyncRequest, 1);

    delay = 12;	// DCL block size
    while(run) {
        int nextTick;
#if TIMING
        CFAbsoluteTime start;
        start = CFAbsoluteTimeGetCurrent();
#endif
        err = mach_msg(&msg.msgHdr, MACH_RCV_MSG | MACH_RCV_TIMEOUT,
        //err = mach_msg(&msg.msgHdr, MACH_RCV_MSG | MACH_RCV_TIMEOUT | MACH_RCV_LARGE,
                       0, sizeof(msg), dvThread->fRequestMachPort, delay, MACH_PORT_NULL);

#if TIMING
        DVLog(dvThread, 'mmsg', start, CFAbsoluteTimeGetCurrent());
#endif
        if(err == MACH_MSG_SUCCESS) {
            switch (msg.msgHdr.msgh_id) {
            case kDVRequestID:
                dvThread->fRequestResult = (dvThread->fRequestFunc)(dvThread->fRequestArg, dvThread->fRequestParam);
				if(dvThread->fRequestFunc == DVthreadExit)
                    run = false;               
                DVSignalSync(&dvThread->fRequestSyncer, &dvThread->fSyncRequest, (UInt32)dvThread->fRequestFunc);
            }
        }
        for(i=0; i<kDVMaxStreamsActive; i++) {
            if(dvThread->fInStreams[i])
                DVReadPoll(dvThread->fInStreams[i]);
            if(dvThread->fOutStreams[i])
                DVWritePoll(dvThread->fOutStreams[i]);
        }
        if(dvThread->fTimerFunc) {
            dvThread->fTimerFunc(NULL, dvThread->fTimerRefCon);
            delay = 12;	// DCL block size in milliseconds
            nextTick = (int)((dvThread->requestTimeoutTime-CFAbsoluteTimeGetCurrent())*1000.0);
            if(nextTick <= 0)
                nextTick = 1;
            if(nextTick < delay)
                delay = nextTick;
        }
        
    }

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVRTThreadStart end\n");
#endif

    return NULL;
}

static void *DVRLThreadStart(DVThread *thread)
{
#if REGISTER_THREADS_WITH_GC
	if (CF_USING_COLLECTABLE_MEMORY)
	{
		void *dlhandle = dlopen("/usr/lib/libobjc.dylib", RTLD_LAZY | RTLD_LOCAL);
		if( dlhandle != NULL ) {
			ObjCRegisterThreadWithCollectorPtr objcRegisterThreadWithCollector = dlsym( dlhandle, "objc_registerThreadWithCollector" );
			if( objcRegisterThreadWithCollector != NULL )
				objcRegisterThreadWithCollector();
			else syslog(LOG_INFO, "dlsym(objc_registerThreadWithCollector) failed");
			dlclose( dlhandle );
		}
		else syslog(LOG_INFO, "dlopen(/usr/lib/libobjc.dylib) failed");
	}
#endif	// REGISTER_THREADS_WITH_GC
	
    CFRunLoopRef loop;
    //syslog(LOG_INFO, "Starting thread: %p\n", thread);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVRLThreadStart begin\n");
#endif
	
     loop = CFRunLoopGetCurrent();
    //printf("Starting thread: %p, loop %p, notify retain %d, notify %p ioport %p, info %x\n",
    //    thread, loop, retain, thread->fNotifySource, thread->fNotifyPort, *((UInt32 *)thread->fNotifySource + 1));
    if(thread->fNotifySource)
        CFRunLoopAddSource(loop, thread->fNotifySource, kCFRunLoopDefaultMode);

    if(thread->fPowerNotifySource)
        CFRunLoopAddSource(loop, thread->fPowerNotifySource, kCFRunLoopDefaultMode);

	
    CFRetain(loop);
    thread->fWorkLoop = loop;
    
    // signal that we're about to start the runloop
    DVSignalSync(&thread->fRequestSyncer, &thread->fSyncRequest, 1);

    CFRunLoopRun();

    //printf("Exiting thread: %p, loop %p\n", thread, loop);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVRLThreadStart end\n");
#endif
	
    return NULL;
}

void
PowerManagementNotificationCallback(void * refcon,
									io_service_t service,
									natural_t messageType,
									void * messageArgument )
{
	DVThread *dvThread = (DVThread*) refcon;
	UInt32 i;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: PowerManagementNotificationCallback begin\n");
#endif
	
	// If we are waking from sleep, restart any running streams
	if (messageType == kIOMessageSystemHasPoweredOn)
	{
		// Find all active streams, and restart them
        for(i=0; i<kDVMaxStreamsActive; i++)
		{
            if(dvThread->fInStreams[i])
			{
				syslog(LOG_INFO, "DV PowerManagementNotificationCallback, Restarting input stream %d\n",i);
				if (dvThread->fInStreams[i]->dvReadStopInProgress == false)
				{
					dvThread->fInStreams[i]->pendingDVReadUnderrunHandler = true;
					DVRequest(dvThread->fInStreams[i]->fStreamVars.fThread, 
						doDVReadHandleInputUnderrun, 
						dvThread->fInStreams[i],
						0);
				}
			}

            if(dvThread->fOutStreams[i])
			{
				syslog(LOG_INFO, "DV PowerManagementNotificationCallback, Restarting output stream %d\n",i);
				if (dvThread->fOutStreams[i]->dvWriteStopInProgress == false)
				{
					dvThread->fOutStreams[i]->pendingDVWriteUnderrunHandler = true;
					DVRequest(dvThread->fOutStreams[i]->fStreamVars.fThread, 
						doDVHandleOutputUnderrun, 
						dvThread->fOutStreams[i],
						0);
				}
			}
        }
	}

	// Acknowledge the message
	IOAllowPowerChange (dvThread->fPowerNotifyConnect, (long) messageArgument);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: PowerManagementNotificationCallback end\n");
#endif
	
}

DVThread * DVCreateThread(DVDeviceArrivedFunc deviceAdded, void * addedRefCon,
    CFRunLoopTimerCallBack timerTick, void *timerRefCon, IOFWAVCMessageCallback deviceMessage)
{
    UInt32 i;
    IOReturn err;
	mach_port_t masterDevicePort;
    DVThread *dvThread;

    const UInt8 num = kAVCTapeRecorder;
    CFMutableDictionaryRef	dict = 0;
    CFNumberRef	tape;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVCreateThread begin\n");
#endif
	
    dict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    
    if(!dict)
        return nil;

    tape = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt8Type, &num);
    if(!tape)
        return nil;

    CFDictionarySetValue( dict, CFSTR(kIOProviderClassKey), CFSTR("IOFireWireAVCSubUnit") );
    CFDictionarySetValue( dict, CFSTR("SubUnit_Type"), tape);
    CFRelease(tape);
    
    if ((err = IOMasterPort(bootstrap_port, &masterDevicePort)) != KERN_SUCCESS) {

#ifdef kIDH_Verbose_Debug_Logging
		syslog(LOG_INFO, "DVLib: DVCreateThread end:failed to get master port\n");
#endif
		return NULL;
    }
    
    dvThread = malloc(sizeof(DVThread));
    bzero(dvThread, sizeof(DVThread));
    for(i = 0 ; i < kDVMaxDevicesActive ; i++){
        dvThread->fDevices[i].fOutPlug = kNoPlug;
    }
    pthread_mutex_init(&dvThread->fRequestSyncer.fMutex, NULL);
    pthread_cond_init(&dvThread->fRequestSyncer.fSyncCond, NULL);
    pthread_mutex_init(&dvThread->fRequestMutex, NULL);

    dvThread->fNotifyPort = IONotificationPortCreate(masterDevicePort);
    dvThread->fNotifySource = IONotificationPortGetRunLoopSource(dvThread->fNotifyPort);
    
    err = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &dvThread->fRequestMachPort);
    err = mach_port_insert_right(mach_task_self(), dvThread->fRequestMachPort, dvThread->fRequestMachPort,
                                    MACH_MSG_TYPE_MAKE_SEND);
    
    if(timerTick) {
        dvThread->fTimerFunc = timerTick;
        dvThread->fTimerRefCon = timerRefCon;
    }
    
    dvThread->fAddedRefCon = addedRefCon;
    dvThread->fAddedFunc = deviceAdded;
    dvThread->fDeviceMessage = deviceMessage;
    
    err = IOServiceAddMatchingNotification( dvThread->fNotifyPort,
            kIOMatchedNotification, dict,
            deviceArrived, dvThread, &dvThread->fMatchEnumer );

	// Register for system power notifications
	dvThread->fPowerNotifyConnect = IORegisterForSystemPower ( dvThread,
							&dvThread->fPowerNotifyPort,
							PowerManagementNotificationCallback,
							&dvThread->fPowerManagementNotifier);
    dvThread->fPowerNotifySource = IONotificationPortGetRunLoopSource(dvThread->fPowerNotifyPort);
	
#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVCreateThread end\n");
#endif
	
	return dvThread;
}

static void setThreadPriority(pthread_t thread)
{
    double   mult;
    thread_time_constraint_policy_data_t constraints;
    kern_return_t result;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: setThreadPriority begin\n");
#endif

	// use mach_timebase_info to get abs to ns conversion parameters
	mach_timebase_info_data_t tTBI;
	mach_timebase_info(&tTBI);

    // Set thread to Real Time
	mult = ((double)tTBI.denom / (double)tTBI.numer) * 1000000;
    constraints.period = 12*mult;
    constraints.computation = 2*mult;
    constraints.constraint = 24*mult;
    constraints.preemptible = TRUE;
    result = thread_policy_set(pthread_mach_thread_np(thread), THREAD_TIME_CONSTRAINT_POLICY,
        (thread_policy_t)&constraints, THREAD_TIME_CONSTRAINT_POLICY_COUNT);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: setThreadPriority end\n");
#endif
	
}

void DVSetTimeoutTime(DVThread * dvThread, CFAbsoluteTime fireDate)
{
    dvThread->setTimeoutTime = CFAbsoluteTimeGetCurrent();
    dvThread->requestTimeoutTime = fireDate;
}

void DVRunThread(DVThread * dvThread)
{
    pthread_attr_t threadAttr;			// Attributes of work thread
    pthread_t thread;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVRunThread begin\n");
#endif
    
    // Start each thread, wait for first to start before setting up second.
    dvThread->fSyncRequest = 0;
    pthread_attr_init(&threadAttr);
    //pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_DETACHED);
    pthread_create(&thread, &threadAttr, DVRLThreadStart, dvThread);
    dvThread->fRLThread = thread;
    DVWaitSync(&dvThread->fRequestSyncer, &dvThread->fSyncRequest);

    dvThread->fSyncRequest = 0;
    pthread_create(&thread, &threadAttr, DVRTThreadStart, dvThread);
    dvThread->fRTThread = thread;
    setThreadPriority(thread);
    DVWaitSync(&dvThread->fRequestSyncer, &dvThread->fSyncRequest);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVRunThread end\n");
#endif
	
}

void DVFreeThread(DVThread * dvThread)
{

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVFreeThread begin\n");
#endif
	
    DVRequest(dvThread, DVthreadExit, dvThread, 0);
    pthread_join(dvThread->fRTThread, NULL);
    pthread_join(dvThread->fRLThread, NULL);
    
    //printf("Workloop retain %d\n", CFGetRetainCount(dvThread->fWorkLoop));
    CFRelease(dvThread->fWorkLoop);
    
    //CFRunLoopRemoveSource(dvThread->fWorkLoop, dvThread->fNotifySource, kCFRunLoopDefaultMode);
    //CFRunLoopRemoveSource(dvThread->fWorkLoop, dvThread->fRequestSource, kCFRunLoopDefaultMode);
   	//printf("after thread exit, notify , request retains are %d, %d\n",
    //    CFGetRetainCount(dvThread->fNotifySource), CFGetRetainCount(dvThread->fRequestSource));
    if(dvThread->fMatchEnumer)
        IOObjectRelease(dvThread->fMatchEnumer);
    if(dvThread->fNotifyPort) {
        CFMachPortRef hack;
        CFMachPortContext 	context;
        Boolean		shouldFreeInfo;

        context.version = 1;
        context.info = (void *) dvThread->fNotifyPort;
        context.retain = NULL;
        context.release = NULL;
        context.copyDescription = NULL;


        hack = CFMachPortCreateWithPort(NULL, IONotificationPortGetMachPort(dvThread->fNotifyPort),
            NULL, &context, &shouldFreeInfo);
        CFMachPortInvalidate(hack);
        IONotificationPortDestroy(dvThread->fNotifyPort);
        //printf("hack port retain %d\n", CFGetRetainCount(hack));
        CFRelease(hack);


	}

	IONotificationPortDestroy(dvThread->fPowerNotifyPort);
	
    mach_port_destroy(mach_task_self(), dvThread->fRequestMachPort);
    
    //printf("after IONotificationPortDestroy, notify retain is %d\n", CFGetRetainCount(dvThread->fNotifySource));

    pthread_mutex_destroy(&dvThread->fRequestSyncer.fMutex);
    pthread_cond_destroy(&dvThread->fRequestSyncer.fSyncCond);
    pthread_mutex_destroy(&dvThread->fRequestMutex);

	IODeregisterForSystemPower(&dvThread->fPowerManagementNotifier);
	IOServiceClose(dvThread->fPowerNotifyConnect);
    memset(dvThread, 0xde, sizeof(DVThread));
    free(dvThread);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVFreeThread end\n");
#endif
	
}

void DVSignalSync(ThreadSyncer *sync, UInt32 *var, UInt32 val)
{
#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVSignalSync begin\n");
#endif
	
    pthread_mutex_lock(&sync->fMutex);
    *var = val;
	pthread_mutex_unlock(&sync->fMutex);
    pthread_cond_broadcast(&sync->fSyncCond);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVSignalSync end\n");
#endif
}

void DVWaitSync(ThreadSyncer *sync, UInt32 *var)
{

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVWaitSync begin\n");
#endif
	
    //if(!*var)
    {
        pthread_mutex_lock(&sync->fMutex);
        while(!*var) {
            pthread_cond_wait(&sync->fSyncCond, &sync->fMutex);
        }
        pthread_mutex_unlock(&sync->fMutex);
    }

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVWaitSync end\n");
#endif
	
}

void DVLock(ThreadSyncer *sync)
{

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVLock begin\n");
#endif
	
    pthread_mutex_lock(&sync->fMutex);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVLock end\n");
#endif
}

void DVUnlock(ThreadSyncer *sync)
{

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVUnlock begin\n");
#endif
	
    pthread_mutex_unlock(&sync->fMutex);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVUnlock end\n");
#endif
}

static IOReturn isochPortGetSupported(
	IOFireWireLibIsochPortRef			interface,
	IOFWSpeed*							outMaxSpeed,
	UInt64*								outChanSupported)
{
    DVStream *stream;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: isochPortGetSupported begin\n");
#endif
	
    stream = (DVStream *)((*interface)->GetRefCon(interface));

    if(*outMaxSpeed > stream->fMaxSpeed)
        *outMaxSpeed = stream->fMaxSpeed;
    *outChanSupported = stream->fChannelMask;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: isochPortGetSupported end\n");
#endif
	
	return kIOReturnSuccess;
}

static IOReturn isochPortAllocate(
    IOFireWireLibIsochPortRef		interface,
	IOFWSpeed						maxSpeed,
	UInt32							channel)
{
    DVStream *stream;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: isochPortAllocate begin\n");
#endif
	
    stream = (DVStream *)((*interface)->GetRefCon(interface));
    //printf("using channel %d\n", channel);
    stream->fIsocChannel = channel;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: isochPortAllocate end\n");
#endif
	
	return kIOReturnSuccess;
}

IOReturn openFireWireUnit(IOFireWireAVCLibUnitInterface **avcInterface, IOFireWireSessionRef session, IOFireWireLibDeviceRef *retInterface, DVThread *thread)
{
    IOFireWireLibDeviceRef	resultInterface;
    IOReturn				err = kIOReturnNoMemory;
    int						opened = false;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: openFireWireUnit begin\n");
#endif
	
    do {
        resultInterface = (*avcInterface)->getAncestorInterface(avcInterface, "IOFireWireUnit",
            CFUUIDGetUUIDBytes(kIOFireWireLibTypeID), CFUUIDGetUUIDBytes(kIOFireWireUnitInterfaceID_v3));
        if(!resultInterface)
            break;
            
        if(session)
            err = (*resultInterface)->OpenWithSessionRef(resultInterface, session);
        else
            err = (*resultInterface)->Open(resultInterface);
        if(err)
            break;
        opened = true;
        //err = (*resultInterface)->AddCallbackDispatcherToRunLoop(resultInterface, workLoop );
        err = (*resultInterface)->AddIsochCallbackDispatcherToRunLoop(resultInterface, thread->fWorkLoop);
    } while (false);
    
    if(!err)
        *retInterface = resultInterface;
    else {
        if(opened)
            (*resultInterface)->Close(resultInterface);
        if(resultInterface)
            (*resultInterface)->Release(resultInterface);
    }

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: openFireWireUnit end\n");
#endif
	
    return err;
}

IOReturn openAVCUnit(io_object_t obj, IOFireWireAVCLibUnitInterface ***retInterface, DVThread *thread)
{
    IOCFPlugInInterface** 	theCFPlugInInterface;
    IOFireWireAVCLibUnitInterface	**resultInterface = 0 ;
    SInt32					theScore ;
    IOReturn				err;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: openAVCUnit begin\n");
#endif

	err = IOCreatePlugInInterfaceForService(
                    obj,
                    kIOFireWireAVCLibUnitTypeID,
                    kIOCFPlugInInterfaceID,		//interfaceType,
                    & theCFPlugInInterface, 
                    & theScore);
    if (!err) {
        HRESULT comErr;
        comErr = (*theCFPlugInInterface)->QueryInterface(
                                            theCFPlugInInterface, 
                                            CFUUIDGetUUIDBytes(kIOFireWireAVCLibUnitInterfaceID), 
                                            (void**) & resultInterface);
        if (comErr == S_OK) {
            err = (*resultInterface)->addCallbackDispatcherToRunLoop(resultInterface, thread->fWorkLoop );
        }
        else
            err = comErr;
       (*theCFPlugInInterface)->Release(theCFPlugInInterface);	// Leave just one reference.
    }
    
    if(!err)
        *retInterface = resultInterface;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: openAVCUnit end\n");
#endif
	
    return err;
}

IOReturn openAVCProto(IOFireWireAVCLibUnitInterface **avcInterface, IOFireWireAVCLibProtocolInterface ***retInterface, DVThread *thread)
{
    IOFireWireAVCLibProtocolInterface **resultInterface;
    IOReturn				err = noErr;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: openAVCProto begin\n");
#endif
	
    do {
        resultInterface = (*avcInterface)->getProtocolInterface(avcInterface,
            CFUUIDGetUUIDBytes(kIOFireWireAVCLibProtocolTypeID),
            CFUUIDGetUUIDBytes(kIOFireWireAVCLibProtocolInterfaceID));
        if(!resultInterface)
            break;
        err = (*resultInterface)->addCallbackDispatcherToRunLoop(resultInterface, thread->fWorkLoop);
    } while (false);
    
    if(!err)
        *retInterface = resultInterface;
    else {
        if(resultInterface)
            (*resultInterface)->Release(resultInterface);
    }
        

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: openAVCProto end\n");
#endif
	
	return err;
}

void DVDeviceTerminate(DVDevice *dev)
{

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVDeviceTerminate begin\n");
#endif
	
    DVDeviceClose(dev);
    if(dev->fAVCInterface)
	{
		// Remove runloop source for this interface
		(*dev->fAVCInterface)->removeCallbackDispatcherFromRunLoop(dev->fAVCInterface);
		
		// Crasher Fix: If we are on a thread other than the RL thread,
		// we need to make sure the RL thread is idle before
		// releasing the AVC interface
		if (dev->fThread->fWorkLoop != CFRunLoopGetCurrent())
		{
			// Endless loop here until the run-loop thread is idle
			while (CFRunLoopIsWaiting(dev->fThread->fWorkLoop) == false )
				usleep(1000); // sleep for a millisecond	
		}
		
        (*dev->fAVCInterface)->Release(dev->fAVCInterface);
        dev->fAVCInterface = NULL;
    }
    if(dev->fObject) {
        IOObjectRelease(dev->fObject);
        dev->fObject = NULL;
    }

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVDeviceTerminate end\n");
#endif
	
}

IOReturn DVDeviceOpen(DVThread *dvThread, DVDevice *device)
{
    IOReturn err = noErr;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVDeviceOpen begin\n");
#endif
	
    if(!device->fAVCInterface)
        return kIOReturnNoMemory;

    do {
        err = (*device->fAVCInterface)->open(device->fAVCInterface);

        if(err != kIOReturnSuccess) break;
        err = openFireWireUnit(device->fAVCInterface, (*device->fAVCInterface)->getSessionRef(device->fAVCInterface),
                    &device->fDevInterface, dvThread);
        if(err != kIOReturnSuccess) break;

        err = openAVCProto(device->fAVCInterface, &device->fAVCProtoInterface, dvThread);
        if(err != kIOReturnSuccess) break;
        err = (*device->fAVCProtoInterface)->allocateOutputPlug(device->fAVCProtoInterface,
                                                    device, handlePCRLock, &device->fOutPlug);
        if(err != kIOReturnSuccess) break;
        
		err = writePlug(device->fAVCProtoInterface, device->fOutPlug, 122 << kIOFWPCROutputPayloadPhase);
        if(err != kIOReturnSuccess) break;
    } while (0);
    if(err != kIOReturnSuccess)
        DVDeviceClose(device);
        
#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVDeviceOpen end\n");
#endif
	
	return err;
}

static IOReturn doDVDeviceClose(DVDevice *dev)
{

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: doDVDeviceClose begin\n");
#endif
	
	if(dev->fDevInterface) {
        UInt32 ref;
        (*dev->fDevInterface)->Close(dev->fDevInterface);
		
		// Remove isoch callback runloop source
		(*dev->fDevInterface)->RemoveIsochCallbackDispatcherFromRunLoop(dev->fDevInterface);

        ref = (*dev->fDevInterface)->Release(dev->fDevInterface);
        //syslog(LOG_INFO, "DVCloseDriver FW refcount was %d\n", ref);
        dev->fDevInterface = NULL;
    }
    
    if(dev->fAVCProtoInterface)
	{
        UInt32 ref;
        if(dev->fOutPlug != kNoPlug) {
            (*dev->fAVCProtoInterface)->freeOutputPlug(dev->fAVCProtoInterface, dev->fOutPlug);
            dev->fOutPlug = kNoPlug; 
        }
		
		// Remove callback runloop source
		(*dev->fAVCProtoInterface)->removeCallbackDispatcherFromRunLoop(dev->fAVCProtoInterface);

        ref = (*dev->fAVCProtoInterface)->Release(dev->fAVCProtoInterface);
        //syslog(LOG_INFO, "DVCloseDriver AVCproto refcount was %d\n", ref);
        dev->fAVCProtoInterface = NULL;
    }

    if(dev->fAVCInterface) {
        (*dev->fAVCInterface)->close(dev->fAVCInterface);
    }

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: doDVDeviceClose end\n");
#endif
	
	return kIOReturnSuccess;
}

void DVDeviceClose(DVDevice *dev)
{

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVDeviceClose begin\n");
#endif
	
	DVRequest(dev->fThread, doDVDeviceClose, dev, 0);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVDeviceClose end\n");
#endif
	
}

IOReturn DVRequest(DVThread *thread, IOReturn (*func)(void *arg, UInt32 param), void *arg, UInt32 param)
{
    IOReturn result;

    //printf("Doing request %p\n", func);
    if(thread->fRTThread != pthread_self()) {
        
        pthread_mutex_lock(&thread->fRequestMutex);
        thread->fSyncRequest = 0;
        thread->fRequestFunc = func;
        thread->fRequestArg = arg;
        thread->fRequestParam = param;

        {
            SendMsg msg;
            bzero( &msg, sizeof(msg));
        
            msg.msgHdr.msgh_remote_port	= thread->fRequestMachPort;
            msg.msgHdr.msgh_bits = MACH_MSGH_BITS(
                                                    MACH_MSG_TYPE_COPY_SEND,
                                                    MACH_MSG_TYPE_COPY_SEND );
            msg.msgHdr.msgh_size 	= sizeof(msg);
            msg.msgHdr.msgh_id		= kDVRequestID;
        
            mach_msg(&msg.msgHdr, MACH_SEND_MSG,
                        msg.msgHdr.msgh_size, 0, MACH_PORT_NULL, 0, MACH_PORT_NULL);
        }
    
        DVWaitSync(&thread->fRequestSyncer, &thread->fSyncRequest);
        result = thread->fRequestResult;
        pthread_mutex_unlock(&thread->fRequestMutex);
    }
    else
        result = (*func)(arg, param);

    return result;
}

static void initStream(DVStream *stream, DVDevice *device, UInt32 plug, UInt32 channel, DVThread *thread)
{

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: initStream begin\n");
#endif
	
	stream->pFWDevice = device->fDevInterface;
    stream->pDVDevice = device;
    stream->fAVCProtoInterface = device->fAVCProtoInterface;
    stream->fPlug = plug;
    stream->fIsocChannel = channel;
    stream->fMaxSpeed = device->fMaxSpeed;
    stream->fThread = thread;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: initStream end\n");
#endif
	
}

static IOReturn openStream(DVStream *stream, bool forWrite, UInt32 packetSize)
{
    IOReturn err;
    IOFireWireLibIsochPortRef talker, listener;
    IOVirtualRange bufRange;
    bool allocBandwidth;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: openStream begin\n");
#endif
	
    do {
        
        if(forWrite) {
            // always allocate bandwidth
            allocBandwidth = true;
        }
        else {
            // Figure out if the device is already tranmitting, in which case use that channel and don't
            // allocate bandwidth
            UInt32 plugVal, plugValHost;
            io_object_t obj;
            FWAddress addr;
            UInt32 size;
            // Use any channel not already in use, or the channel that the camcorder is already using
            addr.nodeID = 0;
            addr.addressHi = 0xffff;
            addr.addressLo = 0xf0000904;
            size = 4;
            obj = (*stream->pFWDevice)->GetDevice(stream->pFWDevice);
            err = (*stream->pFWDevice)->ReadQuadlet(stream->pFWDevice, obj, &addr, &plugVal, false, 0);
			plugValHost = EndianU32_BtoN( plugVal );

            if(plugValHost & (kIOFWPCRBroadcast | kIOFWPCRP2PCount)) {
                UInt32 chan = (plugValHost & kIOFWPCRChannel)>>kIOFWPCRChannelPhase;
                //printf("Already transmitting on channel %x\n", chan);
                stream->fChannelMask = 1ULL << (63-chan);
                allocBandwidth = false;
            }
            else {
#ifdef USE_P2P_CONNECTIONS_FOR_DV_READ            
                stream->fChannelMask = ~1ULL;
                allocBandwidth = true;
#else
                stream->fChannelMask = 1ULL;	// Assume the camera will use channel 63
                allocBandwidth = false;
#endif                
            }
        }
        stream->fIsochChannelRef = (*stream->pFWDevice)->CreateIsochChannel(stream->pFWDevice, allocBandwidth, packetSize,
            stream->fMaxSpeed, CFUUIDGetUUIDBytes(kIOFireWireIsochChannelInterfaceID));
        if (NULL == stream->fIsochChannelRef) {
            err = memFullErr;
            break;
        }
        
        bufRange.address = (IOVirtualAddress)stream->fDCLBuffers;
        bufRange.length = stream->fDCLBufferSize;
        
        // Add local node as talker or listener
        if(forWrite) {
            stream->pFWLocalIsochPort = (*stream->pFWDevice)->CreateLocalIsochPort(stream->pFWDevice, 1 /*inTalking*/, 
                            stream->pDCLList, kFWDCLCycleEvent, 0, 0x0000f000, nil, 0, &bufRange, 1,
                                            CFUUIDGetUUIDBytes(kIOFireWireLocalIsochPortInterfaceID));
                                            
            // Use any available channel
            stream->fChannelMask = ~1ULL;
        }
        else {
            stream->pFWLocalIsochPort = (*stream->pFWDevice)->CreateLocalIsochPort(stream->pFWDevice, 0 /*inTalking*/, 
                            stream->pDCLList, 0, 0, 0, nil, 0, &bufRange, 1,
                                            CFUUIDGetUUIDBytes(kIOFireWireLocalIsochPortInterfaceID));
        }
		if (!stream->pFWLocalIsochPort) {
			err = memFullErr;
            break;
        }

		stream->pFWRemoteIsochPort = (*stream->pFWDevice)->CreateRemoteIsochPort(stream->pFWDevice, 0,										CFUUIDGetUUIDBytes(kIOFireWireRemoteIsochPortInterfaceID) );
		
        (*stream->pFWRemoteIsochPort)->SetRefCon( stream->pFWRemoteIsochPort, stream);
        (*stream->pFWRemoteIsochPort)->SetGetSupportedHandler( stream->pFWRemoteIsochPort, &isochPortGetSupported);
        (*stream->pFWRemoteIsochPort)->SetAllocatePortHandler( stream->pFWRemoteIsochPort, &isochPortAllocate);
        
        if(forWrite) {
            talker = (IOFireWireLibIsochPortRef) stream->pFWLocalIsochPort;
            listener = (IOFireWireLibIsochPortRef)stream->pFWRemoteIsochPort;
        }
        else {
            listener = (IOFireWireLibIsochPortRef) stream->pFWLocalIsochPort;
            talker = (IOFireWireLibIsochPortRef)stream->pFWRemoteIsochPort;
        }
		err = (*stream->fIsochChannelRef)->SetTalker( stream->fIsochChannelRef, talker);
        if(err)
            break;
		err = (*stream->fIsochChannelRef)->AddListener( stream->fIsochChannelRef, listener);
        if(err)
            break;


		// Initialize isochronous channel.

        (*stream->fIsochChannelRef)->TurnOnNotification(stream->fIsochChannelRef);
		err = (*stream->fIsochChannelRef)->AllocateChannel(stream->fIsochChannelRef);
        if(err)
            break;
        if(forWrite) {
            // set our output plug broadcast bit, channel number and bandwidth usage.
            err = writePlug(stream->fAVCProtoInterface, stream->fPlug,
                kIOFWPCROnline | kIOFWPCRBroadcast | (1 << kIOFWPCRP2PCountPhase) |
                (stream->fIsocChannel<<kIOFWPCRChannelPhase) | 
                (15 << kIOFWPCROutputOverheadPhase) | (122 << kIOFWPCROutputPayloadPhase));
                
            if(err)
                break;
            err = MakeP2PConnectionForWrite(stream->pDVDevice,0,stream->fIsocChannel);
        }
        else
        {
#ifdef USE_P2P_CONNECTIONS_FOR_DV_READ            
            err = MakeP2PConnectionForRead(stream->pDVDevice,0,stream->fIsocChannel);
#endif            
        }   
        
        err = (*stream->fIsochChannelRef)->Start(stream->fIsochChannelRef);
        if(err)
            break;

        stream->fFrames.fStatus = kDVRunning;

    } while (false);

	// If we got any errors, call closeStream now to cleanup
	if(err)
		closeStream(stream);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: openStream end\n");
#endif
	
    return err;
}

static void closeStream(DVStream *stream)
{
    IOReturn err;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: closeStream begin\n");
#endif
	
    stream->fFrames.fStatus = kDVStopped;
    if(stream->fIsochChannelRef) {
        (*stream->fIsochChannelRef)->TurnOffNotification(stream->fIsochChannelRef);
        //syslog(LOG_INFO, "Stopping stream %p\n", stream);
        err = (*stream->fIsochChannelRef)->Stop(stream->fIsochChannelRef);
        //syslog(LOG_INFO, "Stopped stream, err %x\n", err);
        //syslog(LOG_INFO, "ReleaseChannel of stream %p\n", stream);
        err = (*stream->fIsochChannelRef)->ReleaseChannel(stream->fIsochChannelRef);
        //syslog(LOG_INFO, "releaseChannelled, err %x\n", err);
        //syslog(LOG_INFO, "Releaseing channel of stream %p\n", stream);
        (*stream->fIsochChannelRef)->Release(stream->fIsochChannelRef);
        stream->fIsochChannelRef = NULL;
    }
    if(stream->pFWLocalIsochPort) {
        (*stream->pFWLocalIsochPort)->Release(stream->pFWLocalIsochPort);
        stream->pFWLocalIsochPort = NULL;
    }
    if(stream->pFWRemoteIsochPort) {
        (*stream->pFWRemoteIsochPort)->Release(stream->pFWRemoteIsochPort);
        stream->pFWRemoteIsochPort = NULL;
    }
    
    // Run the runloop for .1 secs to pick up stray DCL callbacks
    // But we don't want to run the other runloop sources... 
    //CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, false);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: closeStream end\n");
#endif
	
}

static IOReturn DVAllocFrames(DVFrameVars *pFrameData, UInt32 numFrames, UInt32 frameSize,
        DVFrameVars **frameVars, UInt8 **frames)
{
    int i;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVAllocFrames begin\n");
#endif
	
    pFrameData->fNumFrames = numFrames;
    pFrameData->fFrames = malloc(numFrames*frameSize);
    pFrameData->fReader = 0;
    pFrameData->fWriter = 0;
    pFrameData->fDroppedFrames = 0;
    pFrameData->fStatus = 0;
    for(i=0; i<numFrames; i++) {
        frames[i] = pFrameData->fFrames + i*frameSize;
    }
    *frameVars = pFrameData;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVAllocFrames end\n");
#endif
	
    return kIOReturnSuccess;
}

static void DVFreeFrames(DVFrameVars *pFrameData)
{

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVFreeFrames begin\n");
#endif
	
    if(!pFrameData->fFrames)
	{
#ifdef kIDH_Verbose_Debug_Logging
		syslog(LOG_INFO, "DVLib: DVFreeFrames end:no frames\n");
#endif
        return;
	}

    free(pFrameData->fFrames);
    pFrameData->fFrames = NULL;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVFreeFrames end\n");
#endif
}

static void DVGetNextFullOutputFrame(DVFrameVars *pFrameData, UInt8** ppFrame, UInt32 frameSize )
{
    if(NULL == *ppFrame) {
        *ppFrame = pFrameData->fFrames;
    }
    else {
        if (pFrameData->fReader + 1 < pFrameData->fWriter) {
            pFrameData->fReader++;
        }
        else {
            //syslog(LOG_INFO, "DVGetNextFullOutputFrame: dropping frame: reader %d writer %d dropped %d\n",
                //pFrameData->fReader, pFrameData->fWriter,
                //pFrameData->fDroppedFrames);
            pFrameData->fDroppedFrames++;
        }
        *ppFrame = pFrameData->fFrames + 
            frameSize*(pFrameData->fReader % pFrameData->fNumFrames);
    }
}

void DVSetInputFrameSizeAndMode(DVFrameVars *pFrameData, UInt32 bytes, UInt8 mode, UInt32 frameTime )
{
    int index = pFrameData->fWriter % pFrameData->fNumFrames;
    int i;
    pFrameData->fFrameSize[index] = bytes;
    pFrameData->fFrameStandard[index] = mode;
    pFrameData->fFrameTime[index] = frameTime;
    pFrameData->fFrameStatus[index] = kReady;
    
    // find next free frame
    for(i=pFrameData->fWriter + 1; i < pFrameData->fReader + pFrameData->fNumFrames; i++) {
        if(pFrameData->fFrameStatus[i % pFrameData->fNumFrames] != kReading)
            break;
        //syslog(LOG_INFO, "Skipping frame %d (%d) state %d\n",
        //    i, i % pFrameData->fNumFrames, pFrameData->fFrameStatus[i % pFrameData->fNumFrames]);
    }
    if (i< pFrameData->fReader + pFrameData->fNumFrames)
        pFrameData->fWriter = i;
    else {
        pFrameData->fDroppedFrames++;
        //(LOG_INFO, "Dropping frame on input, dropped %d @ %d\n",
        //    pFrameData->fDroppedFrames, pFrameData->fWriter);
    }
}

void DVGetNextEmptyInputFrame(DVFrameVars *pFrameData,  UInt8** ppFrame, UInt32 frameSize )
{
    int index = pFrameData->fWriter % pFrameData->fNumFrames;
    *ppFrame = pFrameData->fFrames + frameSize*index;
	pFrameData->fFrameStatus[index] = kWriting;
}

static UInt32 getEmptyPacketsPerGroup(DVGlobalOutPtr pGlobalData, UInt32 numDataPacketsPerPlayBufferGroup)
{
    // Compute the number of data packets per empty packet.
    // If the frame rate is expressed as n/d, the number of data packets per buffer group
    // expressed as A, and the number of data packets per frame as C, then the number of
    // empty packets per buffer group B should be
    //
    // B = int (8000*d/n*A/C - A + 1)
    // B = A*((8000*d)/(n*c) - 1) + 1
    
	//
    // in order to ensure that the frame rate may be maintained by periodically reducing
    // the number of empty packets in a buffer group by 1.
    //

	UInt32 numEmptyPacketsPerPlayBufferGroup;
    UInt32 A1, C1, d1, n1;
                    
    A1 = numDataPacketsPerPlayBufferGroup;
    C1 = pGlobalData->numDataPacketsPerFrame;
    n1 = pGlobalData->playFrameRateNumerator;
    d1 = pGlobalData->playFrameRateDenominator;
#if ALT_TIMING
	{
		UInt32 d2 = C1*n1;
		UInt32 n2 = (8000 * d1 * A1) - (d2 * A1) + d2;
		numEmptyPacketsPerPlayBufferGroup = n2 / d2;
	}
#else
    numEmptyPacketsPerPlayBufferGroup = (8000 * d1 * A1 + (n1 * C1)/2) / (n1 * C1) - A1;
#endif
    
    return numEmptyPacketsPerPlayBufferGroup;
}

static void FreeDCLCommandPool(DVGlobalOutPtr pGlobalData)
{

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: FreeDCLCommandPool begin\n");
#endif
	
    if( pGlobalData->fDCLCommandPool != NULL ) {
        free(pGlobalData->fDCLCommandPool);
        pGlobalData->fDCLCommandPool = NULL;
    }

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: FreeDCLCommandPool end\n");
#endif
	
}

static IOReturn AllocateDCLCommandPool(DVGlobalOutPtr pGlobalData, UInt32 total )
{       
    UInt8 * pDCLCommandPool;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: AllocateDCLCommandPool begin\n");
#endif
	
    // Allocate DCL command pool record.
    pDCLCommandPool = malloc(total);
    if (pDCLCommandPool == NULL)
    {
        // syslog(LOG_INFO, "AllocateDCLCommandPool: IOMalloc: pDCLCommandPool failed\n");
#ifdef kIDH_Verbose_Debug_Logging
		syslog(LOG_INFO, "DVLib: AllocateDCLCommandPool end:no pool\n");
#endif
        return kIOReturnNoMemory;
    }
    else
    {
        pGlobalData->fTotalPool = total;
        pGlobalData->fAllocatedPool = 0;
        pGlobalData->fDCLCommandPool = pDCLCommandPool;
    }

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: AllocateDCLCommandPool end\n");
#endif
	
    return kIOReturnSuccess;
}

static DCLCommandPtr AllocateDCLCommand(DVGlobalOutPtr pGlobalData, UInt32 dclSize )
{
    DCLCommandPtr pDCLCommand;

    if(pGlobalData->fAllocatedPool + dclSize <= pGlobalData->fTotalPool) {
        pDCLCommand = (DCLCommandPtr)(pGlobalData->fDCLCommandPool + pGlobalData->fAllocatedPool);
        pGlobalData->fAllocatedPool += dclSize;
    }
    else {
        syslog(LOG_INFO, "Trying to allocated DCL command size %d, no space left\n", dclSize);
        pDCLCommand = NULL;
    }

    return (pDCLCommand);
}

static DVLocalOutPtr DVAllocatePlayBufferGroup(DVGlobalOutPtr pGlobalData, int num)
{
    DVLocalOutPtr pLocalData,
                  pPrevLocalData,
                  pNextLocalData;

    // Allocate buffer group data record.
    pLocalData = &pGlobalData->fLocalDataArray[num];          
    pLocalData->pGlobalData = pGlobalData; 
    pLocalData->fBlockNum = num;
    
    // Insert buffer group data record into list.
    if(num == 0) {
        pPrevLocalData = &pGlobalData->fLocalDataArray[kNumPlayBufferGroups-1];
    }
    else
        pPrevLocalData = &pGlobalData->fLocalDataArray[num-1];
        
    if(num == kNumPlayBufferGroups-1)
        pNextLocalData = &pGlobalData->fLocalDataArray[0];
    else
        pNextLocalData = &pGlobalData->fLocalDataArray[num+1];
        
    pLocalData->pNextLocalData = pNextLocalData;
    pLocalData->pPrevLocalData = pPrevLocalData;
    return (pLocalData);
}

static void DVDeallocatePlayBufferGroup( DVLocalOutPtr pLocalData )
{      
    if ( pLocalData != NULL )
    {
        if ( pLocalData->bufferGroupUpdateDCLList != NULL )
            free(pLocalData->bufferGroupUpdateDCLList);
   }
}               

static IOReturn  DVCreatePlayBufferGroupUpdateList( DVLocalOutPtr pLocalData)
{
    DCLCommandPtr     pDCLCommand,
                      pLastDCLCommand;
    DCLCommandPtr     *updateDCLList,
                      *pUpdateDCLListEntry;
    UInt32            opcode;
    UInt32            updateListSize;
    IOReturn          error = 0;

    // Loop through all DCL commands in buffer group and count all send packet DCL
    // commands.
    pDCLCommand = pLocalData->pFirstBufferGroupDCLCommand;
    pLastDCLCommand = pLocalData->pLastBufferGroupDCLCommand;
    updateListSize = 0;
    while (pDCLCommand != pLastDCLCommand)
    {
        opcode = pDCLCommand->opcode & ~kFWDCLOpFlagMask;
        if ((opcode == kDCLSendPacketStartOp) || (opcode == kDCLSendPacketOp))
            updateListSize++;

        pDCLCommand = pDCLCommand->pNextDCLCommand;
    }
    opcode = pDCLCommand->opcode & ~kFWDCLOpFlagMask;
    if ((opcode == kDCLSendPacketStartOp) || (opcode == kDCLSendPacketOp))
        updateListSize++;

    // Allocate update list.
    updateDCLList = (DCLCommandPtr *)malloc( updateListSize * sizeof (DCLCommandPtr) );
    if (updateDCLList == NULL)
    {
        // syslog(LOG_INFO, "DVCreatePlayBufferGroupUpdateList: IOMalloc: updateDCLList\n");
        error = kIOReturnNoMemory;
    }
    else
    {
        bzero( updateDCLList, updateListSize * sizeof (DCLCommandPtr) );
    }

    // Loop through all DCL commands in buffer group and add all send packet DCL
    // commands to update list.
    if (error == 0)
    {
        pDCLCommand = pLocalData->pFirstBufferGroupDCLCommand;
        pLastDCLCommand = pLocalData->pLastBufferGroupDCLCommand;
        pUpdateDCLListEntry = updateDCLList;

        while (pDCLCommand != pLastDCLCommand)
        {
            opcode = pDCLCommand->opcode & ~kFWDCLOpFlagMask;
            if ((opcode == kDCLSendPacketStartOp) || (opcode == kDCLSendPacketOp))
                *pUpdateDCLListEntry++ = pDCLCommand;

            pDCLCommand = pDCLCommand->pNextDCLCommand;
        }

        opcode = pDCLCommand->opcode & ~kFWDCLOpFlagMask;
        if ((opcode == kDCLSendPacketStartOp) || (opcode == kDCLSendPacketOp))
            *pUpdateDCLListEntry++ = pDCLCommand;
    }

    // Save update list.
    if (error == 0)
    {
        pLocalData->bufferGroupUpdateDCLList = updateDCLList;
        pLocalData->updateListSize = updateListSize;
    }
    else
    {
        pLocalData->bufferGroupUpdateDCLList = NULL;
        pLocalData->updateListSize = 0;
    }

    return ( error );
}

static void ModifyDCLJump(IOFireWireLibLocalIsochPortRef port, DCLJumpPtr pDCLJump, DCLLabelPtr pDCLLabel)
{
    // Send notification to DCL compiler.
    if (port) {
        (*port)->ModifyJumpDCL( port, pDCLJump, pDCLLabel);
    }
}

void DVSilenceFrame(UInt8 mode, UInt8* frame)
{
    UInt32    i,j,k,n;
    UInt8    *tPtr;
	UInt8    sType = ((mode & 0x7C) >> 2);
    
    //syslog(LOG_INFO, "silencing frame %p\n", frame);
    
    // Get DSF flag in byte 3 of header (Blue Book p. 113)
    tPtr = frame;
    if ((tPtr[3] & 0x80) == 0)
        n=10;                            // ntsc            
    else
        n=12;                            // pal

	if (sType == 1)
		n /= 2;							//  SDL
	else if (sType == 0x1D)
		n *= 2;							// DVCPro-50

    // Mute all the audio samples
    
    for (i=0;i<n;i++)
    {
        for (j=0;j<9;j++)
        {
            tPtr = frame + (i * 12000) + ((j * 16 + 6) * 80) + 8;
            for (k=0;k<72;k++)
                *tPtr++ = 0x0;
        }
    }
}


static void DVUpdateOutputBuffers( DVLocalOutPtr pLocalData )
{
    DCLCommandPtr         pCurrentDCLCommand;
    DCLTransferPacketPtr  pDCLTransferPacket;
    DVLocalOutPtr		pPrevLocalData;
    DVGlobalOutPtr        pGlobalData;
    UInt16                localNodeID;
    UInt32				  shiftedNodeID;	// Poistioned for ORing into header0
    UInt32                nominalFrameCycleTime;
    UInt32                syt;
    UInt32                *pBuffer, *pImageBuffer, *pLastImageBuffer;
    UInt32                packetNum, dataPacketNum, numPackets;
    UInt32                dbc;
//    static UInt16       lastFrameSequence = 0;
//    UInt16              currentFrameSequence;
	UInt8 stype;
    
    // Get driver data and first DCL command.
    pGlobalData = pLocalData->pGlobalData;
    
    pCurrentDCLCommand = pLocalData->pFirstBufferGroupDCLCommand;
    nominalFrameCycleTime = pGlobalData->nominalFrameCycleTime;

    // Get data for previous buffer group.
    pPrevLocalData = pLocalData->pPrevLocalData;
    syt = pGlobalData->nextSYT;
    dbc = pGlobalData->nextDBC;
    dataPacketNum = pGlobalData->nextDataPacketNum;

    // Get local node ID.

    (*pGlobalData->fStreamVars.pFWDevice)->GetLocalNodeID(pGlobalData->fStreamVars.pFWDevice, &localNodeID);
    localNodeID &= 0x3f;
    shiftedNodeID = (UInt32)localNodeID << 24;
    
    // Get first send packet command for this buffer group.
    while (pCurrentDCLCommand->opcode != kDCLSendPacketStartOp)
        pCurrentDCLCommand = pCurrentDCLCommand->pNextDCLCommand;
    pDCLTransferPacket = (DCLTransferPacketPtr) pCurrentDCLCommand;

    // Update the packet buffers.
    numPackets = pLocalData->numPackets;

    if(pGlobalData->fUpdateBuffers) {
        // Get the next frame to output
        if( pGlobalData->pImageBuffer == NULL ) {
            DVGetNextFullOutputFrame(&pGlobalData->fStreamVars.fFrames,
							(UInt8 **)&(pGlobalData->pImageBuffer),
							 pGlobalData->fStreamVars.fDVFrameSize);
        }
    }
    pImageBuffer = ( pGlobalData->pImageBuffer + (pGlobalData->fDataQuadSize * dataPacketNum) );
    for( packetNum = 0; packetNum < numPackets; packetNum++)
    {
        // Set up packet header.
        pBuffer = (UInt32 *) pDCLTransferPacket->buffer;

        pBuffer[0] = EndianU32_NtoB( pGlobalData->fHeader0 | (dbc & 0xFF) | shiftedNodeID );
        pBuffer[1] = EndianU32_NtoB( pGlobalData->fHeader1 | 0xFFFF );

        // if not an empty packet
        if (pDCLTransferPacket->size > kDVPacketCIPSize)
        {
            // Set SYT field if this is the first data packet in the frame.
            if (dataPacketNum == 0)
            {
                pBuffer[1] = EndianU32_NtoB( pGlobalData->fHeader1 | (syt & 0xFFFF) );
                syt = AddFWCycleTimeToFWCycleTime(syt, pGlobalData->nominalFrameCycleTime);
            }
        
            // Copy data into packet.
            if(pGlobalData->fUpdateBuffers) {
                bcopy(pImageBuffer, (void *)((UInt32)(pDCLTransferPacket->buffer) + kDVPacketCIPSize),
                        pGlobalData->fDataPacketSize);
                pImageBuffer += pGlobalData->fDataQuadSize;
            }

			// Increment dbc based on stream type
			// TODO: This will need to change to support 2x,4x modes on some signal types
			stype = pGlobalData->fStreamVars.fSignalMode & kAVCSignalModeMask_STYPE;
			switch (stype)
			{
				case kAVCSignalModeMask_DVCPro50:
					dbc += 2;	// DBC increments by two for each packet
					break;
		
				case kAVCSignalModeMask_DVCPro100:
					dbc += 4;	// DBC increments by four for each packet
					break;

				case kAVCSignalModeMask_SDL:
				case kAVCSignalModeMask_DVCPro25:
				case kAVCSignalModeMask_HD:
				default:	// SD video stream
					dbc += 1;	// DBC increments by one for each packet
					break;
			};
            dataPacketNum++;
        
            // check if frame is done
            if (dataPacketNum == pGlobalData->numDataPacketsPerFrame )
            {
                // syslog(LOG_INFO, "frame done\n");
                // DVCIsochCompleteEvent    theEvent;
                
                // post a DV event to let the curious know...
                // theEvent.eventHeader.deviceID     = gpDVFWData->deviceID;
                // theEvent.eventHeader.theEvent     = kDVIsochWriteComplete;
                // theEvent.pFrameBuffer            = (Ptr) pImageBuffer;
                // theEvent.fwCycleTime            = syt;
                // FIXME: DVCPostEvent( (DVCEventRecordPtr) &theEvent );
                // syslog(LOG_INFO, "DVCPostEvent\n");
                
                // pImageBuffer = (UInt32 *) pDVCDriverData->imageBuffer;
                dataPacketNum = 0;
                //pDVCDriverData->playData.imageBuffer = GetNextOutputFrame();
                
                if(pGlobalData->fUpdateBuffers) {
                    pLastImageBuffer = pGlobalData->pImageBuffer;
                    
                    DVGetNextFullOutputFrame(&pGlobalData->fStreamVars.fFrames,
								(UInt8 **)&(pGlobalData->pImageBuffer),
								 pGlobalData->fStreamVars.fDVFrameSize);
                    pImageBuffer = pGlobalData->pImageBuffer;
        
                    // Mute the audio on repeating frames, based on repeating frame sequences
                    if (pImageBuffer == pLastImageBuffer)
                        DVSilenceFrame(pGlobalData->fStreamVars.fSignalMode, (UInt8 *)pImageBuffer);
                }	// End if(pGlobalData->fUpdateBuffers)
            } // end if end frame
        } // end if empty
        
        // Find next send packet start command.
        pCurrentDCLCommand = pCurrentDCLCommand->pNextDCLCommand;
        while (pCurrentDCLCommand != NULL)
        {
            if (pCurrentDCLCommand->opcode != kDCLSendPacketStartOp)
                pCurrentDCLCommand = pCurrentDCLCommand->pNextDCLCommand;
            else
                break;
        }
        pDCLTransferPacket = (DCLTransferPacketPtr) pCurrentDCLCommand;
    }
    pGlobalData->nextSYT = syt;
    pGlobalData->nextDBC = dbc;
    pGlobalData->nextDataPacketNum = dataPacketNum;

}

static void DVHandleOutput(DVLocalOutPtr pLocalData)
{
    DVGlobalOutPtr     pGlobalData;
    DVLocalOutPtr      pPrevLocalData;
    UInt32             nominalFrameCycleTime;
    UInt32             fractionalFrameCycleCount,
                       fractionalFrameCycleOffset;
    SInt32             timeDrift;
    UInt32             cycleDrift;
    UInt32             projectedTimeStamp,
                       projectedSYT;
#if TIMING
    CFAbsoluteTime cstart, cend;
    cstart = CFAbsoluteTimeGetCurrent();
#endif

    //syslog(LOG_INFO, "DVHandleOutput: 0x%x\n", pDCLCommandPtr);
    pPrevLocalData = pLocalData->pPrevLocalData;
    pGlobalData = pLocalData->pGlobalData;
    nominalFrameCycleTime = pGlobalData->nominalFrameCycleTime;

    // Undo skipping empty packet if we're currently skipping a packet.
    if (pLocalData->skippingEmptyPacket)
    {
        ModifyDCLJump(pGlobalData->fStreamVars.pFWLocalIsochPort,
            pLocalData->pBufferGroupSkipEmptyPacketDCLJump, pLocalData->pBufferGroupDontSkipEmptyPacketDCLLabel);
        pGlobalData->activePackets++;
        pLocalData->skippingEmptyPacket = false;
    }

    // Compute time drift.

    // Compute the projected time stamp value for the first packet of the current
    // buffer group the next time this proc is called for the current buffer group.

    // Start at time stamp of first packet in next buffer group to be sent.
    projectedTimeStamp = *pLocalData->pBufferGroupTimeStampPtr;	// Time last packet in group was sent last time round
    projectedTimeStamp = AddFWCycleTimeToFWCycleTime(projectedTimeStamp, 1 << 12);

    // Add the total number of cycles for all active buffer group packets.
    projectedTimeStamp = AddFWCycleTimeToFWCycleTime(projectedTimeStamp, pGlobalData->activePackets << 12);

    // Subtract the number of cycles for all packets in the current buffer group.
    projectedTimeStamp = SubtractFWCycleTimeFromFWCycleTime(projectedTimeStamp, pLocalData->numPackets << 12);

    // Compute the projected SYT value for the first packet of the current buffer group
    // the next time this proc is called for the current buffer group.

    // Start with the SYT value to use for the first packet of the next frame.
    projectedSYT = pGlobalData->nextSYT;

    // Subtract the SYT offset between frames if we aren't at the start of a frame
    if(pGlobalData->nextDataPacketNum != 0) {
        projectedSYT = SubtractFWCycleTimeFromFWCycleTime(projectedSYT, nominalFrameCycleTime);

        // Add the fraction of the SYT offset between the start of the frame and the
        // first data packet for the current buffer group.
        fractionalFrameCycleOffset =
            ((nominalFrameCycleTime & 0x0FFF) * pGlobalData->nextDataPacketNum) /
            pGlobalData->numDataPacketsPerFrame;
    
        fractionalFrameCycleCount =
            ((nominalFrameCycleTime & 0x01FFF000) * pGlobalData->nextDataPacketNum) /
            pGlobalData->numDataPacketsPerFrame;
        fractionalFrameCycleCount =
            (fractionalFrameCycleCount & 0x01FFF000) +
            (((fractionalFrameCycleCount & 0x0FFF) * 3072) / 4096);
    
        projectedSYT = AddFWCycleTimeToFWCycleTime (projectedSYT, fractionalFrameCycleOffset);
        projectedSYT = AddFWCycleTimeToFWCycleTime (projectedSYT, fractionalFrameCycleCount);
    }
    
    // The time drift is the difference between the projected time stamp and SYT.
    // We must convert the time drift to cycles.
    cycleDrift = AddFWCycleTimeToFWCycleTime(projectedTimeStamp, kPlaySYTDelay << 12);
    cycleDrift = SubtractFWCycleTimeFromFWCycleTime(cycleDrift, projectedSYT);
//syslog(LOG_INFO, "time drift %x group %p\n", cycleDrift >> 12, pLocalData );
    timeDrift = (cycleDrift >> 12) & 0x000F;
    // Skip an empty packet if we're drifting.
    // Only consider positive drifting.
    if ((timeDrift > 0) && (timeDrift < 0x0008))
    {
        ModifyDCLJump(pGlobalData->fStreamVars.pFWLocalIsochPort,
            pLocalData->pBufferGroupSkipEmptyPacketDCLJump, pLocalData->pBufferGroupSkipEmptyPacketDCLLabel);
        pGlobalData->activePackets--;
        pLocalData->skippingEmptyPacket = true;
    }

    DVUpdateOutputBuffers( pLocalData );

    // Update DCL jumps to call underrun proc after this buffer group.
    //zzz check errors.
    ModifyDCLJump (pGlobalData->fStreamVars.pFWLocalIsochPort,
        pLocalData->pEndOfBufferGroupDCLJump, pGlobalData->pUnderrunDCLLabel);
    ModifyDCLJump (pGlobalData->fStreamVars.pFWLocalIsochPort,
        pPrevLocalData->pEndOfBufferGroupDCLJump, pLocalData->pStartOfBufferGroupDCLLabel);
    pGlobalData->fSharedDCLVars.fDMAPos = pLocalData->fBlockNum;
#if TIMING
    cend = CFAbsoluteTimeGetCurrent();
    DVLog(pGlobalData->fStreamVars.fThread, 'isoc', cstart, cend);
#endif
}

static void DVWritePoll(DVGlobalOutPtr globs)
{
    int i, pos;
    pos = globs->fSharedDCLVars.fDMAPos;
    for(i=pos; i<kNumPlayBufferGroups; i++)
        if(*globs->fLocalDataArray[i].pBufferGroupTimeStampPtr != 0xffffffff) {
            DVHandleOutput(&globs->fLocalDataArray[i]);
            *globs->fLocalDataArray[i].pBufferGroupTimeStampPtr = 0xffffffff;
        }
            
    for(i=0; i<pos; i++)
        if(*globs->fLocalDataArray[i].pBufferGroupTimeStampPtr != 0xffffffff) {
            DVHandleOutput(&globs->fLocalDataArray[i]);
            *globs->fLocalDataArray[i].pBufferGroupTimeStampPtr = 0xffffffff;
        }
}


static void doDVHandleOutputUnderrun(  DVGlobalOutPtr	pGlobalData )
{   
    IOReturn		err;
    // FIXME

    syslog(LOG_INFO, "DVHandleOutputUnderrun: 0x%p\n", pGlobalData);

    DVStream *stream;

    if ((pGlobalData->pendingDVWriteUnderrunHandler == true) && (pGlobalData->deferredDVWriteFree == true))
    {
	// Free the globalout data struct
	free(pGlobalData);
	return;
    }
    pGlobalData->pendingDVWriteUnderrunHandler = false;


    stream = &pGlobalData->fStreamVars;

	// See if stream still open. If not, we're done!
	if (stream->fIsochChannelRef == NULL)
		return;
	
	closeStream(&pGlobalData->fStreamVars);

	FreeDCLCommandPool(pGlobalData);
    
	BreakP2PConnectionForWrite(pGlobalData->fStreamVars.pDVDevice,0,pGlobalData->fStreamVars.fIsocChannel);
	
    err = buildWriteProgram(pGlobalData);
    if(err != kIOReturnSuccess)
        syslog(LOG_INFO, "DVHandleOutputUnderrun: buildWriteProgram returned %x\n", err);
    
	err = DVWriteStart(pGlobalData);
}

static void DVHandleOutputUnderrun( DCLCommandPtr pDCLCommandPtr )
{
    DVGlobalOutPtr pGlobalData;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVHandleOutputUnderrun begin\n");
#endif
	
    pGlobalData = (DVGlobalOutPtr)((DCLCallProcPtr)pDCLCommandPtr)->procData;
    if (pGlobalData->dvWriteStopInProgress == false)
    {
        pGlobalData->pendingDVWriteUnderrunHandler = true;
        DVRequest(pGlobalData->fStreamVars.fThread, doDVHandleOutputUnderrun, pGlobalData, 0);
    }

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVHandleOutputUnderrun end\n");
#endif
}


static void DVDisposeDCLOutput( DVGlobalOutPtr pOutputData )
{       
    DVLocalOutPtr  pLocalData, pNextLocalData;
    UInt32         bufferGroupNum;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVDisposeDCLOutput begin\n");
#endif
	
    // syslog(LOG_INFO, "DVDisposeDCLOutput\n");
    if( pOutputData != NULL )
    {
        // Deallocate play buffer group data records.
        // and update lists associated with them
        pLocalData = &pOutputData->fLocalDataArray[0];
        for (bufferGroupNum = 0; bufferGroupNum < kNumPlayBufferGroups; bufferGroupNum++)
        {
            if( pLocalData != NULL )
            { 
                pNextLocalData = pLocalData->pNextLocalData;
                DVDeallocatePlayBufferGroup (pLocalData);
                pLocalData = pNextLocalData; 
            }
        }
        
        FreeDCLCommandPool(pOutputData);
        
        /* don't dispose of the frame buffer here.. wait since its shared
        DVReleaseFrameIO();
        */

        if( pOutputData->fStreamVars.fDCLBuffers != NULL)
        {
            //free( pOutputData->pTransmitBuffers); //pOutputData->fDCLBufferSize );
            vm_deallocate(mach_task_self(), (vm_address_t)pOutputData->fStreamVars.fDCLBuffers,
                pOutputData->fStreamVars.fDCLBufferSize);
        }
        //free( pOutputData); //, sizeof(DVGlobalOut) );
    }

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVDisposeDCLOutput end\n");
#endif
	
}

static IOReturn allocateBuffers(DVGlobalOutPtr pGlobalData)
{
    UInt32			numDataPacketsPerPage;
    UInt32			numEmptyPackets;
    UInt32			pageSize;
    UInt32			emptySize;		// Space used by empty packer headers.
    UInt32			transmitBuffersSize;

    IOReturn		res;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: allocateBuffers begin\n");
#endif
	
	// Setup CIP header static bits, plus packet size based on signal mode
	UInt8 stype = pGlobalData->fStreamVars.fSignalMode & kAVCSignalModeMask_STYPE;
	switch (stype)
	{
		case kAVCSignalModeMask_SDL:
			pGlobalData->fHeader0 = 0x003c0000;
			pGlobalData->fHeader1 = 0x80040000;
			pGlobalData->fDataPacketSize = kDVSDLPayloadPacketSize;			// Data portion, in bytes
			break;
			
		case kAVCSignalModeMask_DVCPro25:
			pGlobalData->fHeader0 = 0x00780000;
			pGlobalData->fHeader1 = 0x80780000;
			pGlobalData->fDataPacketSize = kDVSDPayloadPacketSize;			// Data portion, in bytes
			break;
			
		case kAVCSignalModeMask_DVCPro50:
			pGlobalData->fHeader0 = 0x00784000;
			pGlobalData->fHeader1 = 0x80740000;
			pGlobalData->fDataPacketSize = kDVCPro50PayloadPacketSize;			// Data portion, in bytes
			break;
		
		case kAVCSignalModeMask_DVCPro100:
			pGlobalData->fHeader0 = 0x00788000;
			pGlobalData->fHeader1 = 0x80700000;
			pGlobalData->fDataPacketSize = kDVCPro50PayloadPacketSize;			// Data portion, in bytes		
			break;

		case kAVCSignalModeMask_HD:
			pGlobalData->fHeader0 = 0x00F00000;
			pGlobalData->fHeader1 = 0x80080000;
			pGlobalData->fDataPacketSize = kDVCPro50PayloadPacketSize;			// Data portion, in bytes		
			break;
			
		default:	// Must be SD video stream
			pGlobalData->fHeader0 = 0x00780000;
			pGlobalData->fHeader1 = 0x80000000;
			pGlobalData->fDataPacketSize = kDVSDPayloadPacketSize;			// Data portion, in bytes
			break;
	};
	
    pGlobalData->fDataQuadSize = pGlobalData->fDataPacketSize/4;	// Data portion, in quads
    pGlobalData->fAlignQuadSize = (pGlobalData->fDataPacketSize + kDVPacketCIPSize + 15)/16;
    pGlobalData->fAlignQuadSize *= 4;					// Packet size padded out to 16 byte boundary, in quads
    pGlobalData->fSharedDCLVars.fAlignedPacketSize = 4*pGlobalData->fAlignQuadSize; //Packet size in bytes
    pGlobalData->fSharedDCLVars.fPacketDataSize = pGlobalData->fDataPacketSize;
    
    ////////////////////////////////////////////////
    //
    // set timing values for appropriate video system
    //
    if( !(pGlobalData->fStreamVars.fSignalMode & kAVCSignalModeMask_50) )
    {
        //syslog(LOG_INFO, "NTSC output\n");
        pGlobalData->playFramePeriodNumerator = kNTSCPlayFramePeriodNumerator;
        pGlobalData->playFramePeriodDenominator = kNTSCPlayFramePeriodDenominator;
        
        pGlobalData->playFrameRateNumerator = kNTSCFrameRateNumerator;
        pGlobalData->playFrameRateDenominator = kNTSCFrameRateDenominator;
        
        pGlobalData->numDataPacketsPerFrame = kNTSCNumDataPacketsPerDVFrame;
        pGlobalData->numDataPacketsPerGroup = kNTSCNumDataPacketsPerGroup;
    }
    else
    {
        //syslog(LOG_INFO, "PAL output\n");
        pGlobalData->fHeader1 |= kPALBit;
        pGlobalData->playFramePeriodNumerator = kPALPlayFramePeriodNumerator;
        pGlobalData->playFramePeriodDenominator = kPALPlayFramePeriodDenominator;
        
        pGlobalData->playFrameRateNumerator = kPALFrameRateNumerator;
        pGlobalData->playFrameRateDenominator = kPALFrameRateDenominator;
        
        pGlobalData->numDataPacketsPerFrame = kPALNumDataPacketsPerDVFrame;
        pGlobalData->numDataPacketsPerGroup = kPALNumDataPacketsPerGroup;
    }
    
    // Compute nominal frame period cycle time.
    pGlobalData->nominalFrameCycleTime = ConvertFractionalSecondsToFWCycleTime
        (pGlobalData->playFramePeriodNumerator, pGlobalData->playFramePeriodDenominator);

    pGlobalData->fSharedDCLVars.fNumGroups = kNumPlayBufferGroups;
    pGlobalData->fSharedDCLVars.fGroupSize = pGlobalData->numDataPacketsPerGroup;
    
    pageSize = PAGE_SIZE;
    numDataPacketsPerPage = pageSize / 
                (pGlobalData->fDataPacketSize + kDVPacketCIPSize + kDVPacketAlignSlop);
#if ALT_TIMING
    numEmptyPackets = getEmptyPacketsPerGroup(pGlobalData, pGlobalData->numDataPacketsPerGroup) * kNumPlayBufferGroups;
#else
    numEmptyPackets = 
        getEmptyPacketsPerGroup(pGlobalData, pGlobalData->numDataPacketsPerGroup * kNumPlayBufferGroups) + kNumPlayBufferGroups/2;
#endif

    transmitBuffersSize = pGlobalData->numDataPacketsPerGroup * kNumPlayBufferGroups * pageSize;
    transmitBuffersSize /= numDataPacketsPerPage;
    // add pages for empty packets and time stamps
    emptySize = numEmptyPackets * (kDVPacketCIPSize + kDVPacketAlignSlop);
    // Allocate.
    //syslog(LOG_INFO, "DVWrite: IOMalloc: pGlobalData->pTransmitBuffers size %d, empty %d\n",transmitBuffersSize, emptySize);
    //pGlobalData->pTransmitBuffers = (UInt8 *)malloc( transmitBuffersSize + emptySize + pageSize );
    pGlobalData->fStreamVars.fDCLBufferSize =
                    transmitBuffersSize + emptySize + sizeof(UInt32)*kNumPlayBufferGroups;
    vm_allocate(mach_task_self(), (vm_address_t *)&pGlobalData->fStreamVars.fDCLBuffers,
       pGlobalData->fStreamVars.fDCLBufferSize, VM_FLAGS_ANYWHERE);
    //syslog(LOG_INFO, "DCL buffers at %p\n", pGlobalData->fStreamVars.fDCLBuffers);
    if( pGlobalData->fStreamVars.fDCLBuffers == NULL ) {
        res = kIOReturnNoMemory;
        goto bail;
    }
    bzero( pGlobalData->fStreamVars.fDCLBuffers, pGlobalData->fStreamVars.fDCLBufferSize );
    pGlobalData->pEmptyTransmitBuffers = pGlobalData->fStreamVars.fDCLBuffers + transmitBuffersSize;
    pGlobalData->fSharedDCLVars.fTimeStampPtrs = (UInt32 *)(pGlobalData->pEmptyTransmitBuffers + emptySize);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: allocateBuffers end\n");
#endif
	
	return kIOReturnSuccess;
    
bail:
    DVDisposeDCLOutput( pGlobalData );

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: allocateBuffers end:fail\n");
#endif
	
    return res;
}

/*
    The write program is built of kNumPlayBufferGroups blocks of kDCLSendPacketStartOps,
    kNumDataPacketsPerPlayBufferGroup of the ops in each block send empty DV packets (just the CIP header) so that the timing
    comes out right.
    In addition, the callproc (DVHandleOutput) adjusts the blocks to skip an empty packet when the Mac is sending data too slowly.
    
    LoopDCL:
    SetTagSyncBits(tag=1, sync=0)
    
    kNumPlayBufferGroups*
        SendPacketStart (full packet/empty packet)
        or, for first empty packet in group:
            Jump	(initially to SendEmptyLabel, altered to SkipEmptyLabel when falling behind)
            SendEmptyLabel:
            SendPacketStart (empty packet)
            SkipEmptyLabel:
        Jump (initially to EndOfGroup, except last group jumps to Underrun)
        EndOfGroup:
        TimeStamp
        UpdateDCLList(TimeStamp)
        CallProc(DVHandleOutput)
        UpdateDCLList(All the SendPackets)
        
    Jump (LoopDCL)
    
    Underrun:
    CallProc(DVHandleOutputUnderrun)
*/
static IOReturn buildWriteProgram(DVGlobalOutPtr pGlobalData)
{

    UInt32                        numEmptyPacketsInPlayBufferGroup;

    DCLCommandPtr                pDCLCommand;
    DCLCommandPtr                pFirstBufferGroupDCLCommand;
    DCLLabelPtr                    pUnderrunDCLLabel,
                                pBufferGroupDCLLabel,
                                pDCLLabel;
    DCLTransferPacketPtr        pDCLTransferPacket;
    DCLCallProcPtr                pDCLCallProc;
    DCLSetTagSyncBitsPtr        pDCLSetTagSyncBits;
    DCLJumpPtr                    pDCLJump,
                                pBufferGroupDCLJump;
    DCLLabelPtr                    pBufferGroupSkipEmptyPacketDCLLabel;
    DCLUpdateDCLListPtr            pDCLUpdateDCLList;
    DCLPtrTimeStampPtr			pDCLTimeStamp;

    DVLocalOutPtr                pPlayBufferGroupData;
    UInt32 *			pTransmitBuffer;
    UInt8 *				pEmptyTransmitBuffer;
    volatile UInt32 *	pTimeStampPtr;
    UInt32				bufferGroupNum;
    UInt32				dataPacketNum;
    UInt32				numPackets;
    UInt32              emptyPacketNumerator;
    UInt32				pageOffset;
    IOReturn			res;

    UInt32			 totalDCLSize;
    UInt32			totalEmpty, emptySoFar;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: buildWriteProgram begin\n");
#endif
	
#if ALT_TIMING    
	totalEmpty = getEmptyPacketsPerGroup(pGlobalData, pGlobalData->numDataPacketsPerGroup) * kNumPlayBufferGroups;
#else
    totalEmpty = getEmptyPacketsPerGroup(pGlobalData, pGlobalData->numDataPacketsPerGroup * kNumPlayBufferGroups) + kNumPlayBufferGroups/2;
#endif

    // syslog(LOG_INFO, "total empty %d\n", totalEmpty);
    emptySoFar = 0;
    pTransmitBuffer = (UInt32 *) pGlobalData->fStreamVars.fDCLBuffers;
    pEmptyTransmitBuffer = pGlobalData->pEmptyTransmitBuffers;
    pTimeStampPtr = pGlobalData->fSharedDCLVars.fTimeStampPtrs;
    
    /////////////////////////////////////////
    // Start Up DCL Allocation Engine
    // Allocate DCL command pool.
    //
    // DCLs:
    // Start Label + SetTagSyncBits +  DCLJump + DCLLabel + DCLCallProc
    //    kNumPlayBufferGroups * ( (kNumDataPacketsPerPlayBufferGroup + numEmptyPacketsPerPlayBufferGroup) * DCLTransferPacket + 3 * DCLLabel + 2 * DCLJump + DCLPtrTimeStamp + DCLUpdateDCLList + DCLCallProc + DCLUpdateDCLList)
    
    totalDCLSize = 2 * sizeof(DCLLabel) + sizeof(DCLSetTagSyncBits) + sizeof(DCLJump) + sizeof(DCLCallProc) + 
    kNumPlayBufferGroups * (pGlobalData->numDataPacketsPerGroup * sizeof(DCLTransferPacket) + 3*sizeof(DCLLabel) + 2*sizeof(DCLJump) + sizeof(DCLPtrTimeStamp) + 2*sizeof(DCLUpdateDCLList) + sizeof(DCLCallProc) + totalEmpty * sizeof(DCLTransferPacket));
    
    res = AllocateDCLCommandPool(pGlobalData, totalDCLSize);
    if (res)
        goto bail;
 
    ////////////////////////////////////
    // Actually Create DCL Program
    //
    
    // Initialize total packet count.
    pGlobalData->totalPackets = 0;
    pGlobalData->activePackets = 0;

    // Set isoch packet tag bits to the way DV likes 'em
    pDCLSetTagSyncBits = (DCLSetTagSyncBitsPtr) AllocateDCLCommand (pGlobalData, sizeof (DCLSetTagSyncBits));
    pDCLCommand = (DCLCommandPtr) pDCLSetTagSyncBits;
    pDCLSetTagSyncBits->opcode = kDCLSetTagSyncBitsOp;
    pDCLSetTagSyncBits->tagBits = 1;
    pDCLSetTagSyncBits->syncBits = 0;

	// Set the pointer to the start of this DCL program
	pGlobalData->fStreamVars.pDCLList = pDCLCommand;

    for (bufferGroupNum = 0; bufferGroupNum < kNumPlayBufferGroups; bufferGroupNum++)
    {
        // Allocate a buffer group data record.
        pPlayBufferGroupData = DVAllocatePlayBufferGroup( pGlobalData, bufferGroupNum);
		
        // Initialize for loop.
        dataPacketNum = 0;
        numPackets = 0;
        emptyPacketNumerator = 0;
#if ALT_TIMING
        numEmptyPacketsInPlayBufferGroup = totalEmpty/kNumPlayBufferGroups;
#else
        numEmptyPacketsInPlayBufferGroup = (totalEmpty*(bufferGroupNum+1)+kNumPlayBufferGroups/2)/kNumPlayBufferGroups - emptySoFar;
#endif
        emptySoFar += numEmptyPacketsInPlayBufferGroup;
        pFirstBufferGroupDCLCommand = NULL;
        pBufferGroupSkipEmptyPacketDCLLabel = NULL;
        pGlobalData->fSharedDCLVars.fDataOffset[bufferGroupNum] =
                                (UInt8*)pTransmitBuffer - pGlobalData->fStreamVars.fDCLBuffers;

        // Create label for start of buffer group.
        pBufferGroupDCLLabel = (DCLLabelPtr) AllocateDCLCommand (pGlobalData, sizeof (DCLLabel));
        pPlayBufferGroupData->pStartOfBufferGroupDCLLabel = pBufferGroupDCLLabel;
        pDCLCommand->pNextDCLCommand = (DCLCommandPtr) pBufferGroupDCLLabel;
        pDCLCommand = (DCLCommandPtr) pBufferGroupDCLLabel;
        pBufferGroupDCLLabel->opcode = kDCLLabelOp;

		while (dataPacketNum < pGlobalData->numDataPacketsPerGroup)
        {
            // Send a packet: CIP header + payload.
            pDCLTransferPacket = (DCLTransferPacketPtr) AllocateDCLCommand (pGlobalData, sizeof (DCLTransferPacket));
            pDCLCommand->pNextDCLCommand = (DCLCommandPtr) pDCLTransferPacket;
            pDCLCommand = (DCLCommandPtr) pDCLTransferPacket;
            pDCLTransferPacket->opcode = kDCLSendPacketStartOp;
            pDCLTransferPacket->size = pGlobalData->fDataPacketSize + kDVPacketCIPSize;

            // check for buffer crossing page
            pageOffset = (UInt32) (pTransmitBuffer + pGlobalData->fAlignQuadSize) & 0x0fff;
            if (pageOffset < (4*pGlobalData->fAlignQuadSize) && pageOffset > 0)
            {
                // if it does, increment buffer pointer
                // and lop off page rollover to start at next page
                pTransmitBuffer += pGlobalData->fAlignQuadSize;
                pTransmitBuffer = (UInt32 *)((UInt32)pTransmitBuffer & 0xfffff000);
            }

            pDCLTransferPacket->buffer = (UInt8 *) pTransmitBuffer;
            // increment by multiple of 16 to maintain cache alignment
            pTransmitBuffer += pGlobalData->fAlignQuadSize;

            // Save first data packet DCL command.
            if (pFirstBufferGroupDCLCommand == NULL)
                pFirstBufferGroupDCLCommand = (DCLCommandPtr) pDCLCommand;

            dataPacketNum++;
            numPackets++;
            emptyPacketNumerator += numEmptyPacketsInPlayBufferGroup;

            if (emptyPacketNumerator >= pGlobalData->numDataPacketsPerGroup)
            {
                // Add skip jump if this is the first empty packet in the buffer group.
                if (pBufferGroupSkipEmptyPacketDCLLabel == NULL)
                {
                    pDCLJump = (DCLJumpPtr) AllocateDCLCommand (pGlobalData, sizeof (DCLJump));
                    pPlayBufferGroupData->pBufferGroupSkipEmptyPacketDCLJump = pDCLJump;
                    pDCLCommand->pNextDCLCommand = (DCLCommandPtr) pDCLJump;
                    pDCLCommand = (DCLCommandPtr) pDCLJump;
                    pDCLJump->opcode = kDCLJumpOp | kFWDCLOpDynamicFlag;

                    pDCLLabel = (DCLLabelPtr) AllocateDCLCommand (pGlobalData, sizeof (DCLLabel));
                    pPlayBufferGroupData->pBufferGroupDontSkipEmptyPacketDCLLabel = pDCLLabel;
                    pDCLCommand->pNextDCLCommand = (DCLCommandPtr) pDCLLabel;
                    pDCLCommand = (DCLCommandPtr) pDCLLabel;
                    pDCLLabel->opcode = kDCLLabelOp;
                    pDCLJump->pJumpDCLLabel = pDCLLabel;
                }

                // Send a packet.
                // Just CIP header.
                pDCLTransferPacket = (DCLTransferPacketPtr) AllocateDCLCommand (pGlobalData, sizeof (DCLTransferPacket));
                pDCLCommand->pNextDCLCommand = (DCLCommandPtr) pDCLTransferPacket;
                pDCLCommand = (DCLCommandPtr) pDCLTransferPacket;
                pDCLTransferPacket->opcode = kDCLSendPacketStartOp;
                pDCLTransferPacket->buffer = pEmptyTransmitBuffer;
                pDCLTransferPacket->size = kDVPacketCIPSize;

                // increment 16 bytes to maintain alignment
                pEmptyTransmitBuffer += kDVPacketCIPSize+kDVPacketAlignSlop;
                numPackets++;
                emptyPacketNumerator -= pGlobalData->numDataPacketsPerGroup;

                // Add skip jump label if this is the first empty packet in the
                // buffer group.
                if (pBufferGroupSkipEmptyPacketDCLLabel == NULL)
                {
                    // Add skip label.
                    pDCLLabel = (DCLLabelPtr) AllocateDCLCommand (pGlobalData, sizeof (DCLLabel));
                    pBufferGroupSkipEmptyPacketDCLLabel = pDCLLabel;
                    pPlayBufferGroupData->pBufferGroupSkipEmptyPacketDCLLabel = pBufferGroupSkipEmptyPacketDCLLabel;
                    pDCLCommand->pNextDCLCommand = (DCLCommandPtr) pDCLLabel;
                    pDCLCommand = (DCLCommandPtr) pDCLLabel;
                    pDCLLabel->opcode = kDCLLabelOp;
                }
            }
        }
         // Save number of packets in this buffer group, DCL update list size, and last
        // DCL command.
        pPlayBufferGroupData->numPackets = numPackets;
        pPlayBufferGroupData->pFirstBufferGroupDCLCommand = pFirstBufferGroupDCLCommand;
        pPlayBufferGroupData->pLastBufferGroupDCLCommand = (DCLCommandPtr) pDCLCommand;

        // Create buffer group update list.
        DVCreatePlayBufferGroupUpdateList( pPlayBufferGroupData );

        // Update total packet count.
        pGlobalData->totalPackets += numPackets;
        pGlobalData->activePackets += numPackets;
        // work out if we should start by skipping this empty packet.
		
#if ALT_TIMING
	{
		UInt32 nominalProgramCycleTime = 0;
		UInt32 nominalActivePackets = 0;
		SInt32 cycleDrift = 0;
				
		nominalProgramCycleTime = ConvertFractionalSecondsToFWCycleTime( pGlobalData->playFramePeriodNumerator*(kNumPlayBufferGroups/2), 
																		 pGlobalData->playFramePeriodDenominator );
		nominalActivePackets = ((nominalProgramCycleTime & 0x01FFF000) >> 12);
		cycleDrift = pGlobalData->activePackets - ((nominalActivePackets*(bufferGroupNum+1)) / kNumPlayBufferGroups);
	
//		syslog(LOG_INFO, "Group %d, active %d, nominal %d, cycleDrift %d\n", bufferGroupNum+1, pGlobalData->activePackets-numPackets, ((nominalActivePackets*(bufferGroupNum)) / kNumPlayBufferGroups), cycleDrift);
       
		if(cycleDrift > 0) {
            pGlobalData->activePackets--;
            pPlayBufferGroupData->pBufferGroupSkipEmptyPacketDCLJump->pJumpDCLLabel = pPlayBufferGroupData->pBufferGroupSkipEmptyPacketDCLLabel;
            pPlayBufferGroupData->skippingEmptyPacket = true;
        }
        else
            pPlayBufferGroupData->skippingEmptyPacket = false;
	}
#else
		emptyError = getEmptyPacketsPerGroup(pGlobalData, pGlobalData->numDataPacketsPerGroup*(bufferGroupNum+1));
        emptyError = pGlobalData->activePackets - pGlobalData->numDataPacketsPerGroup*(bufferGroupNum+1) - emptyError;

        //syslog(LOG_INFO, "Group %d, %d empty packets, Current error %d\n", bufferGroupNum, numEmptyPacketsInPlayBufferGroup, emptyError);
        if(emptyError > 0) {
            pGlobalData->activePackets--;
            pPlayBufferGroupData->pBufferGroupSkipEmptyPacketDCLJump->pJumpDCLLabel = pPlayBufferGroupData->pBufferGroupSkipEmptyPacketDCLLabel;
            pPlayBufferGroupData->skippingEmptyPacket = true;
        }
        else
            pPlayBufferGroupData->skippingEmptyPacket = false;
  #endif

        // Get time stamp at end of buffer group.
        pDCLTimeStamp = (DCLPtrTimeStampPtr) AllocateDCLCommand (pGlobalData, sizeof (DCLPtrTimeStamp));
        pDCLCommand->pNextDCLCommand = (DCLCommandPtr) pDCLTimeStamp;
        pDCLCommand = (DCLCommandPtr) pDCLTimeStamp;
        pDCLTimeStamp->opcode = kDCLPtrTimeStampOp;
        *pTimeStampPtr = 0xffffffff;	// Init to impossible time stamp
        pDCLTimeStamp->timeStampPtr = pTimeStampPtr++;

        pPlayBufferGroupData->pBufferGroupTimeStampPtr = pDCLTimeStamp->timeStampPtr;
        pPlayBufferGroupData->timeStampUpdateDCLList = (DCLCommandPtr) pDCLTimeStamp;

        // Create update DCL list to update time stamp.
        pDCLUpdateDCLList = (DCLUpdateDCLListPtr) AllocateDCLCommand (pGlobalData, sizeof (DCLUpdateDCLList));
        pDCLCommand->pNextDCLCommand = (DCLCommandPtr) pDCLUpdateDCLList;
        pDCLCommand = (DCLCommandPtr) pDCLUpdateDCLList;
        pDCLUpdateDCLList->opcode = kDCLUpdateDCLListOp;
        pDCLUpdateDCLList->dclCommandList = &(pPlayBufferGroupData->timeStampUpdateDCLList);
        pDCLUpdateDCLList->numDCLCommands = 1;

#if 0
		// TODO: Merge this update DCL list with the timestamp update list, above!!!!!
        // Create update DCL list to update buffers.
        pDCLUpdateDCLList = (DCLUpdateDCLListPtr) AllocateDCLCommand (pGlobalData, sizeof (DCLUpdateDCLList));
        pDCLCommand->pNextDCLCommand = (DCLCommandPtr) pDCLUpdateDCLList;
        pDCLCommand = (DCLCommandPtr) pDCLUpdateDCLList;
        pDCLUpdateDCLList->opcode = kDCLUpdateDCLListOp;
        pDCLUpdateDCLList->dclCommandList = pPlayBufferGroupData->bufferGroupUpdateDCLList;
        pDCLUpdateDCLList->numDCLCommands = pPlayBufferGroupData->updateListSize;
#endif		
		
        // Create end of buffer group jump.
        pBufferGroupDCLJump = (DCLJumpPtr) AllocateDCLCommand (pGlobalData, sizeof (DCLJump));
        pPlayBufferGroupData->pEndOfBufferGroupDCLJump = pBufferGroupDCLJump;
        pDCLCommand->pNextDCLCommand = (DCLCommandPtr) pBufferGroupDCLJump;
        pDCLCommand = (DCLCommandPtr) pBufferGroupDCLJump;
        pBufferGroupDCLJump->opcode = kDCLJumpOp | kFWDCLOpDynamicFlag;
        pBufferGroupDCLJump->pJumpDCLLabel = nil; // For now, this will be updated later!
    }

    // Create label for underrun.
    pUnderrunDCLLabel = (DCLLabelPtr) AllocateDCLCommand (pGlobalData, sizeof (DCLLabel));
    pGlobalData->pUnderrunDCLLabel = pUnderrunDCLLabel;
    pDCLCommand->pNextDCLCommand = (DCLCommandPtr) pUnderrunDCLLabel;
    pDCLCommand = (DCLCommandPtr) pUnderrunDCLLabel;
    pUnderrunDCLLabel->opcode = kDCLLabelOp;

	// Send a garbage packet (just CIP only!). Required for a valid DCL program!
	pDCLTransferPacket = (DCLTransferPacketPtr) AllocateDCLCommand (pGlobalData, sizeof (DCLTransferPacket));
	pDCLCommand->pNextDCLCommand = (DCLCommandPtr) pDCLTransferPacket;
	pDCLCommand = (DCLCommandPtr) pDCLTransferPacket;
	pDCLTransferPacket->opcode = kDCLSendPacketStartOp;
	pDCLTransferPacket->buffer = pGlobalData->pEmptyTransmitBuffers;
	pDCLTransferPacket->size = kDVPacketCIPSize;
	
    // Call underrun proc.
    // This is the last command.
    pDCLCallProc = (DCLCallProcPtr) AllocateDCLCommand (pGlobalData, sizeof (DCLCallProc));
    pDCLCommand->pNextDCLCommand = (DCLCommandPtr) pDCLCallProc;
    pDCLCallProc->pNextDCLCommand = NULL;
    pDCLCallProc->opcode = kDCLCallProcOp;
    pDCLCallProc->proc = DVHandleOutputUnderrun;
    pDCLCallProc->procData = (UInt32) pGlobalData;

	// Fix up jump targets
    for (bufferGroupNum = 0; bufferGroupNum < kNumPlayBufferGroups; bufferGroupNum++)
    {
		pPlayBufferGroupData = &pGlobalData->fLocalDataArray[bufferGroupNum];

		if (bufferGroupNum == (kNumPlayBufferGroups-1))
			 pPlayBufferGroupData->pEndOfBufferGroupDCLJump->pJumpDCLLabel =
				 pGlobalData->pUnderrunDCLLabel;
		else
			 pPlayBufferGroupData->pEndOfBufferGroupDCLJump->pJumpDCLLabel =
				pGlobalData->fLocalDataArray[bufferGroupNum+1].pStartOfBufferGroupDCLLabel;
	}

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: buildWriteProgram end\n");
#endif
    return kIOReturnSuccess;
    
bail:

#ifdef kIDH_Verbose_Debug_Logging
		syslog(LOG_INFO, "DVLib: buildWriteProgram end:fail\n");
#endif
	
    return res;
}

DVGlobalOutPtr DVAllocWrite(DVDevice *device, DVThread *thread)
{
    DVGlobalOutPtr globs;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVAllocWrite begin\n");
#endif
	
    globs = malloc(sizeof(DVGlobalOut));
    if(!globs)
        return NULL;
    bzero(globs, sizeof(DVGlobalOut));
    initStream(&globs->fStreamVars, device, device->fOutPlug, device->fWriteChan, thread);
    globs->fUpdateBuffers = 1;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVAllocWrite end\n");
#endif
	
    return globs;
}

IOReturn DVWriteSetSignalMode(DVGlobalOutPtr globs, UInt8 mode)
{
    globs->fStreamVars.fSignalMode = mode;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVWriteSetSignalMode begin\n");
#endif
	
	switch (mode)
	{
		// NTSC SD or DVCPro25
		case kAVCSignalModeSD525_60:
		case kAVCSignalModeDVCPro525_60:
			globs->fStreamVars.fDVFrameSize = kFrameSize_SD525_60;
			break;

		// PAL SD or DVCPro25
		case kAVCSignalModeSD625_50:
		case kAVCSignalModeDVCPro625_50:
			globs->fStreamVars.fDVFrameSize = kFrameSize_SD625_50;
			break;

		// NTSC SDL
		case kAVCSignalModeSDL525_60: 
			globs->fStreamVars.fDVFrameSize = kFrameSize_SDL525_60;
			break;

		// PAL SDL
		case kAVCSignalModeSDL625_50:
			globs->fStreamVars.fDVFrameSize = kFrameSize_SDL625_50;
			break;

		// NTSC DVCPro50 or HD
		case kAVCSignalModeDVCPro50_525_60:
		case kAVCSignalModeHD1125_60:
			globs->fStreamVars.fDVFrameSize = kFrameSize_DVCPro50_525_60;
			break;

		// PAL DVCPro50 or HD
		case kAVCSignalModeDVCPro50_625_50:
		case kAVCSignalModeHD1250_50:
			globs->fStreamVars.fDVFrameSize = kFrameSize_DVCPro50_625_50;
			break;

		// NTSC DVCPro100
		case kAVCSignalModeDVCPro100_525_60:
			globs->fStreamVars.fDVFrameSize = kFrameSize_DVCPro100_525_60;
			break;

		// PAL DVCPro100
		case kAVCSignalModeDVCPro100_625_50:
			globs->fStreamVars.fDVFrameSize = kFrameSize_DVCPro100_625_50;
			break;

		default:
			globs->fStreamVars.fDVFrameSize = kFrameSize_SD625_50;
			break;
	};

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVWriteSetSignalMode end\n");
#endif
	
    return kIOReturnSuccess;
}

IOReturn DVWriteAllocFrames(DVGlobalOutPtr pGlobalData, UInt32 numFrames, 
    DVFrameVars **frameVars, UInt8 **frames)
{
    IOReturn err;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVWriteAllocFrames begin\n");
#endif
	
   do {
        err = DVAllocFrames(&pGlobalData->fStreamVars.fFrames,
							numFrames,
							pGlobalData->fStreamVars.fDVFrameSize,
							frameVars,
							frames);
        if(err != kIOReturnSuccess)
            break;
        err = allocateBuffers(pGlobalData);
         if(err != kIOReturnSuccess)
            break;
            
        err = buildWriteProgram(pGlobalData);
    } while (0);

#ifdef kIDH_Verbose_Debug_Logging
   syslog(LOG_INFO, "DVLib: DVWriteAllocFrames end\n");
#endif
   
   return err;
}

UInt8 * DVWriteGetDCLBuffer(DVGlobalOutPtr pGlobalData, DVSharedVars **varPtr)
{
    // Caller is now responsible for filling up the DCL buffers
    pGlobalData->fUpdateBuffers = 0;
    *varPtr = &pGlobalData->fSharedDCLVars;
    return pGlobalData->fStreamVars.fDCLBuffers;
}

static IOReturn doDVWriteStart(DVGlobalOutPtr pGlobalData)
{
    IOReturn err;
    DVLocalOutPtr	pPlayBufferGroupData;
    UInt32			bufferGroupNum;
    int i;
    DVThread *		dvThread = pGlobalData->fStreamVars.fThread;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: doDVWriteStart begin\n");
#endif
	
    do {        
 
        // Set up all of the buffer groups.
        //syslog(LOG_INFO, "DVWrite: Setup all of the buffer groups\n");
    
        pGlobalData->nextSYT = kPlaySYTDelay<<12;
        pGlobalData->nextDBC = 0;
        pGlobalData->nextDataPacketNum = 0;
        pGlobalData->pImageBuffer = NULL;
        pGlobalData->fSharedDCLVars.fDMAPos = 0;
        
        pGlobalData->pendingDVWriteUnderrunHandler = false;
        pGlobalData->deferredDVWriteFree = false;
        pGlobalData->dvWriteStopInProgress = false;

        pPlayBufferGroupData = &pGlobalData->fLocalDataArray[0];
        for (bufferGroupNum = 0; bufferGroupNum < kNumPlayBufferGroups; bufferGroupNum++)
        {
            DVUpdateOutputBuffers( pPlayBufferGroupData);
            pPlayBufferGroupData = pPlayBufferGroupData->pNextLocalData;
        }
        err = openStream(&pGlobalData->fStreamVars, true, pGlobalData->fDataPacketSize + kDVPacketCIPSize);

        for(i=0; i<kDVMaxStreamsActive; i++) {
            if(dvThread->fOutStreams[i] == NULL) {
                dvThread->fOutStreams[i] = pGlobalData;
                break;
            }
            else if(dvThread->fOutStreams[i] == pGlobalData) {
                break;	// Happens when restarting.
            }
        }
        
    } while (0);
    //syslog(LOG_INFO, "doDVWriteStart exit, err %x\n", err);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: doDVWriteStart end\n");
#endif
	
	return err;
}

IOReturn DVWriteStart(DVGlobalOutPtr pGlobalData)
{
    IOReturn err;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVWriteStart begin\n");
#endif
	
    err = DVRequest(pGlobalData->fStreamVars.fThread, doDVWriteStart, pGlobalData, 0);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVWriteStart end\n");
#endif
	
	return err;
}

static void doDVWriteStop(DVGlobalOutPtr pGlobalData)
{
    int i;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: doDVWriteStop begin\n");
#endif
	
    pGlobalData->dvWriteStopInProgress = true;

    DVThread *		dvThread = pGlobalData->fStreamVars.fThread;
    
    for(i=0; i<kDVMaxStreamsActive; i++) {
        if(dvThread->fOutStreams[i] == pGlobalData) {
            dvThread->fOutStreams[i] = NULL;
            break;
        }
    }
    closeStream(&pGlobalData->fStreamVars);

    BreakP2PConnectionForWrite(pGlobalData->fStreamVars.pDVDevice,0,pGlobalData->fStreamVars.fIsocChannel);

    DVDisposeDCLOutput(pGlobalData);

    pGlobalData->dvWriteStopInProgress = false;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: doDVWriteStop end\n");
#endif
	
}

void DVWriteStop(DVGlobalOutPtr pGlobalData)
{
#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVWriteStop begin\n");
#endif
	
    DVRequest(pGlobalData->fStreamVars.fThread, doDVWriteStop, pGlobalData, 0);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVWriteStop end\n");
#endif
	
}

void DVWriteFreeFrames(DVGlobalOutPtr globs)
{

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVWriteFreeFrames begin\n");
#endif
	
    DVFreeFrames(&globs->fStreamVars.fFrames);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVWriteFreeFrames end\n");
#endif
	
}

void DVWriteFree(DVGlobalOutPtr globs)
{

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVWriteFree begin\n");
#endif
	
    if (globs->pendingDVWriteUnderrunHandler == true)
	globs->deferredDVWriteFree = true;
    else
	free(globs);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVWriteFree end\n");
#endif
	
}

DVGlobalInPtr DVAllocRead(DVDevice *device, DVThread *thread)
{
    DVGlobalInPtr globs;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVAllocRead begin\n");
#endif
	
    globs = malloc(sizeof(DVGlobalIn));
    if(!globs)
        return NULL;
    bzero(globs, sizeof(DVGlobalIn));
    initStream(&globs->fStreamVars, device, kNoPlug, device->fReadChan, thread);

	// Set the initial read signal mode for standard DV.
	// This can be overriden later by a call to DVReadSetSignalMode
	if (device->standard == 0)
		DVReadSetSignalMode(globs,0x00);	// NTSC-DV
	else
		DVReadSetSignalMode(globs,0x80);	// PAL-DV
	
    return globs;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVAllocRead end\n");
#endif
	
}

IOReturn DVReadSetSignalMode(DVGlobalInPtr globs, UInt8 mode)
{

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVReadSetSignalMode begin\n");
#endif
	
    globs->fStreamVars.fSignalMode = mode;
	
    switch (mode)
	{
		// NTSC SD or DVCPro25
		case kAVCSignalModeSD525_60:
		case kAVCSignalModeDVCPro525_60:
			globs->fStreamVars.fDVFrameSize = kFrameSize_SD525_60;
			break;

		// PAL SD or DVCPro25
		case kAVCSignalModeSD625_50:
		case kAVCSignalModeDVCPro625_50:
			globs->fStreamVars.fDVFrameSize = kFrameSize_SD625_50;
			break;

		// NTSC SDL
		case kAVCSignalModeSDL525_60:
			// override SDL modes to SD
			globs->fStreamVars.fSignalMode = kAVCSignalModeSD525_60;
			globs->fStreamVars.fDVFrameSize = kFrameSize_SD525_60;
			break;

		// PAL SDL
		case kAVCSignalModeSDL625_50:
			// override SDL modes to SD
			globs->fStreamVars.fSignalMode = kAVCSignalModeSD625_50;
			globs->fStreamVars.fDVFrameSize = kFrameSize_SD625_50;
			break;

		// NTSC DVCPro50 or HD
		case kAVCSignalModeDVCPro50_525_60:
		case kAVCSignalModeHD1125_60:
			globs->fStreamVars.fDVFrameSize = kFrameSize_DVCPro50_525_60;
			break;

		// PAL DVCPro50 or HD
		case kAVCSignalModeDVCPro50_625_50:
		case kAVCSignalModeHD1250_50:
			globs->fStreamVars.fDVFrameSize = kFrameSize_DVCPro50_625_50;
			break;

		// NTSC DVCPro100
		case kAVCSignalModeDVCPro100_525_60:
			globs->fStreamVars.fDVFrameSize = kFrameSize_DVCPro100_525_60;
			break;

		// PAL DVCPro100
		case kAVCSignalModeDVCPro100_625_50:
			globs->fStreamVars.fDVFrameSize = kFrameSize_DVCPro100_625_50;
			break;

		default:
			// override the specified mode if it's not one of our supported modes.
			globs->fStreamVars.fSignalMode = kAVCSignalModeSD625_50;
			globs->fStreamVars.fDVFrameSize = kFrameSize_SD625_50;
			break;
	};
    

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVReadSetSignalMode end\n");
#endif
	
	return kIOReturnSuccess;
}

IOReturn DVReadAllocFrames(DVGlobalInPtr globs, UInt32 numFrames, 
    DVFrameVars **frameVars, UInt8 **frames)
{
	IOReturn err;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVReadAllocFrames begin\n");
#endif
	
    err = DVAllocFrames(&globs->fStreamVars.fFrames,
							numFrames,
							globs->fStreamVars.fDVFrameSize,
							frameVars,
							frames);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVReadAllocFrames end\n");
#endif
	
	return err;
}

static void doDVReadHandleInputUnderrun( DVGlobalInPtr pGlobalData )
{
	UInt32 timeNow, lastFrameTime;
    DVStream *stream;
	int	pingPongNum;


    if ((pGlobalData->pendingDVReadUnderrunHandler == true) && (pGlobalData->deferredDVReadFree == true))
    {
	// Free the globalin data struct
	free(pGlobalData);
	return;
    }
    pGlobalData->pendingDVReadUnderrunHandler = false;
    
    stream = &pGlobalData->fStreamVars;

	// See if stream still open. If not, we're done!
	if (stream->fIsochChannelRef == NULL)
		return;
		
    (*stream->pFWDevice)->
        GetCycleTime(stream->pFWDevice, &timeNow);
    syslog(LOG_INFO, "At %8.3f Req time %8.3f, now %8.3f\n",
        stream->fThread->setTimeoutTime, stream->fThread->requestTimeoutTime, CFAbsoluteTimeGetCurrent());
    syslog(LOG_INFO, "DVReadHandleInputUnderrun: 0x%p, last block = %d, status %d, writer %d reader %d timeNow %x\n",
        pGlobalData, pGlobalData->fState, stream->fFrames.fStatus,
        stream->fFrames.fWriter, stream->fFrames.fReader, timeNow);

    lastFrameTime = stream->fFrames.fFrameTime[(stream->fFrames.fWriter-1)%stream->fFrames.fNumFrames];
	
	// Stop
	(*stream->fIsochChannelRef)->Stop(stream->fIsochChannelRef);

	// Fixup DCL program jumps and timestamps
	for (pingPongNum = 0; pingPongNum < kNumPingPongs; pingPongNum++)
    {
		if (pingPongNum < (kNumPingPongs-1))
			ModifyDCLJump(pGlobalData->fStreamVars.pFWLocalIsochPort,
				pGlobalData->fLocalDataArray[pingPongNum].fStateJmp, pGlobalData->fLocalDataArray[pingPongNum+1].fStateLabel);
		else
			ModifyDCLJump(pGlobalData->fStreamVars.pFWLocalIsochPort,
				 pGlobalData->fLocalDataArray[pingPongNum].fStateJmp, pGlobalData->fTerminal);

		*pGlobalData->fLocalDataArray[pingPongNum].fTimeStampPtr = 0xffffffff;
	}

	// Reset some vars
	pGlobalData->fState = 0;
    pGlobalData->fSynced = 0;
    pGlobalData->fRestarted = true;
    pGlobalData->fLastFrameTime = lastFrameTime;
	
	// Restart
	(*stream->fIsochChannelRef)->Start(stream->fIsochChannelRef);
}

static void DVReadHandleInputUnderrun( DCLCommandPtr pDCLCommandPtr )
{
    DVGlobalInPtr pGlobalData;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVReadHandleInputUnderrun begin\n");
#endif
	
    pGlobalData = (DVGlobalInPtr)((DCLCallProcPtr)pDCLCommandPtr)->procData;
    if (pGlobalData->dvReadStopInProgress == false)
    {
        pGlobalData->pendingDVReadUnderrunHandler = true;
        DVRequest(pGlobalData->fStreamVars.fThread, doDVReadHandleInputUnderrun, pGlobalData, 0);
    }

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVReadHandleInputUnderrun end\n");
#endif
	
}

static void DVStorePackets(DVLocalInPtr pLocalData)
{
    DVGlobalInPtr pGlobalData;
    DCLCommandPtr pCurrentCmd;
    DCLTransferPacketPtr pDCLTransferPacket;
    UInt8 * pPacketBuffer;
    UInt32 packetHeader, packetSize, packetNum, packetPerFrame;
    bool vSyncDetected;
    UInt8 currentSequenceCount;
    int prevBlock;
	UInt8 fn;
	UInt8 stype;
	UInt32 actualModeFrameSize;
	short syncData;
	UInt32 cipHeader;

#if TIMING
    CFAbsoluteTime cstart, cend;
    cstart = CFAbsoluteTimeGetCurrent();
#endif

    pGlobalData = pLocalData->pGlobalData;
    
    //printf("Timestamp %p = %x\n", pLocalData->fTimeStampPtr, *pLocalData->fTimeStampPtr);
    
    // Get info from ping pong data.
    pCurrentCmd = pLocalData->fFirstCmd;

    // How many packets we talkin'?
    packetPerFrame = (pGlobalData->fStreamVars.fSignalMode & kAVCSignalModeMask_50) ?
                            kPALNumDataPacketsPerDVFrame : kNTSCNumDataPacketsPerDVFrame;
    for ( packetNum = 0; packetNum < kNumPacketsPerInputBuffer; packetNum++ )
    {
        // compute size of transfer
        pDCLTransferPacket = (DCLTransferPacketPtr) pCurrentCmd;
        pPacketBuffer = (UInt8*)pDCLTransferPacket->buffer;
        packetHeader = *((UInt32*) pPacketBuffer);
        pPacketBuffer += 4; // 4 byte 1394 header
        packetSize = (packetHeader & kFWIsochDataLength) >> kFWIsochDataLengthPhase;
#if 1
		// Calculate fn
		fn = ((pPacketBuffer[2] & 0xC0) >> 6);
		if (fn == 0)
			fn = 1;
		else
			fn = 1 << fn;

        // Check for corrupt packets, otherwise we may die horribly later in bcopy()
        if(packetSize < 8) {
            syslog(LOG_INFO, "DVStorePackets: size %d header 0x%x\n", packetSize, packetHeader);
            packetSize = 8;
        }
        else
		{
			// Check for packet size not matching CIP header specified packet size
			if(packetSize > 8 && packetSize != (pPacketBuffer[1]*4*fn) + 8) {
				syslog(LOG_INFO, "DVStorePackets: size %d header 0x%x\n", packetSize, packetHeader);
				packetSize = 8;
			}

			// Check to make sure the signal mode in the CIP header is what we're expecting
			cipHeader = *(UInt32 *)(pPacketBuffer+4);
			cipHeader = EndianU32_BtoN( cipHeader );
			if (pGlobalData->fStreamVars.fSignalMode != ((cipHeader >> 16) & 0xff))
			{
				// CIP DV-mode doesn't match the configured mode! To prevent a crash, we
				// should only store packets if the CIP DV-mode frame-size will
				// fit into our allocated frame-buffers!

				if (((cipHeader >> 16) & 0xff) & kAVCSignalModeMask_50)
					actualModeFrameSize = kPALNumDataPacketsPerDVFrame * (packetSize-8);
				else
					actualModeFrameSize = kNTSCNumDataPacketsPerDVFrame * (packetSize-8);
				
				if (actualModeFrameSize >  pGlobalData->fStreamVars.fDVFrameSize)
				{
					syslog(LOG_INFO, "DVStorePackets (received frame too large for frame-buffer): expected DV mode: %d, actual DV mode: %d\n", 
						pGlobalData->fStreamVars.fSignalMode, (cipHeader >> 16) & 0xff);
					packetSize = 8;
				}
			}
        }
#endif
        // skip empty packets
        if( packetSize > 8 ) {
            // get current data block sequence counter value and increment saved value
            currentSequenceCount = pPacketBuffer[3];

			// Increment lastSequenceCount based on stream type
			// TODO: This will need to change to support 2x,4x modes on some signal types
			stype = pGlobalData->fStreamVars.fSignalMode & kAVCSignalModeMask_STYPE;
			switch (stype)
			{
				case kAVCSignalModeMask_DVCPro50:
					pGlobalData->lastSequenceCount += 2;
					break;
		
				case kAVCSignalModeMask_DVCPro100:
					pGlobalData->lastSequenceCount += 4;
					break;

				case kAVCSignalModeMask_SDL:
				case kAVCSignalModeMask_DVCPro25:
				case kAVCSignalModeMask_HD:
				default:	// SD video stream
					pGlobalData->lastSequenceCount += 1;
					break;
			};

            // Want size minus CIP header
            packetSize -= 8;
            // detect vSync

           syncData = *(short *)(pPacketBuffer + 8);			  syncData = EndianS16_BtoN(syncData);

		  vSyncDetected = ((syncData & 0xE0F8 ) == 0x0000 );
            if( vSyncDetected ) {
                // Calculate when Sync arrived.
                UInt32 frameEnd = SubtractFWCycleTimeFromFWCycleTime(*pLocalData->fTimeStampPtr, (kNumPacketsPerInputBuffer - packetNum) << 12);
                UInt32 cip2 = *(UInt32 *)(pPacketBuffer+4);
				cip2 = EndianU32_BtoN( cip2 );
                pGlobalData->fStreamVars.fSignalMode = (cip2 >> 16) & 0xff;
                packetPerFrame = (pGlobalData->fStreamVars.fSignalMode & kAVCSignalModeMask_50) ?
                                        kPALNumDataPacketsPerDVFrame : kNTSCNumDataPacketsPerDVFrame;
                // if we got our frameSync at the right time  
                if( pGlobalData->packetCount == packetPerFrame ) {
                    //printf("Frame received @ %x (w%d r%d)\n", frameEnd, pGlobalData->fStreamVars.fFrames.fWriter, pGlobalData->fStreamVars.fFrames.fReader);
                    
                    // Set amount read
                    DVSetInputFrameSizeAndMode(&pGlobalData->fStreamVars.fFrames, packetPerFrame * packetSize,
                                                                    pGlobalData->fStreamVars.fSignalMode, frameEnd);
                }
                else {
                    //printf("Sync detected @ %x\n", frameEnd);
                    if(pGlobalData->fRestarted) {
                        // Calculate how many frames missed.
                        // Fiddly mess from OHCI, have 16 bits from the middle of the cycle register (3 second bits + 13 cyle bits)
                        UInt32 lastFrameTime;
                        SInt32 cycleDiff, secsDiff;
                        UInt32 dropped;

                        lastFrameTime = pGlobalData->fLastFrameTime;
                        cycleDiff = ((frameEnd & 0x01FFF000) - (lastFrameTime & 0x01FFF000));
                        if(cycleDiff < 0) {
                            cycleDiff += 8000 << 12;
                            frameEnd -= 0x02000000;
                        }
                        secsDiff = (frameEnd & 0x0e000000) - (lastFrameTime & 0x0e000000);
                        if(secsDiff < 0)
                            secsDiff += 0x10000000;
                        secsDiff >>= 25;
                        cycleDiff >>= 12;
                        cycleDiff += secsDiff * 8000;
                        if(pGlobalData->fStreamVars.fSignalMode & kAVCSignalModeMask_50)
                            dropped = (cycleDiff * kPALFrameRateNumerator + (4000*kPALFrameRateDenominator)) / (8000*kPALFrameRateDenominator);
                        else
                            dropped = (cycleDiff * kNTSCFrameRateNumerator + (4000*kNTSCFrameRateDenominator)) / (8000*kNTSCFrameRateDenominator);
                        //printf("At sync, dropped %d frames\n", dropped);
                        pGlobalData->fStreamVars.fFrames.fDroppedFrames += dropped;
                        pGlobalData->fRestarted = false;
                    }
                }
                
                // start a new frame
                pGlobalData->packetCount = 0;
                pGlobalData->lastSequenceCount = currentSequenceCount;
                DVGetNextEmptyInputFrame(&pGlobalData->fStreamVars.fFrames, 
											&(pGlobalData->pImageBuffer),
											pGlobalData->fStreamVars.fDVFrameSize);
                //printf("Filling frame %p (w%d r%d)\n",
                //    pGlobalData->pImageBuffer, pGlobalData->fStreamVars.fFrames.fWriter, pGlobalData->fStreamVars.fFrames.fReader);
                    
                pGlobalData->fSynced = true;
            }

            if(pGlobalData->fSynced) {
                // skip over CIP header
                pPacketBuffer += 8;  // 8 bytes
        
                if (currentSequenceCount == pGlobalData->lastSequenceCount && pGlobalData->packetCount < packetPerFrame) {
                    // store the packet
                    bcopy( pPacketBuffer, (void *)((UInt32) pGlobalData->pImageBuffer + (pGlobalData->packetCount * packetSize)), packetSize );
                    pGlobalData->packetCount++;
                }
                else {
                    // IOLog("DVStorePacket: dropped frame: packet out of sequence\n");
                    // packet out of sequence or too many packets between vSync detection, start new frame
                    //printf("Lost sync: %d-%d; %d-%d\n",
                    //    currentSequenceCount, pGlobalData->lastSequenceCount, pGlobalData->packetCount, packetPerFrame);
                    pGlobalData->packetCount = 0;
                    pGlobalData->fSynced = false;
                }
            }
            // Set last count to current count to resynch counts if bad sequence
            pGlobalData->lastSequenceCount = currentSequenceCount;
        }
        // update for next packet
        pCurrentCmd = pCurrentCmd->pNextDCLCommand;
    }
    
    
    // This block is ready for reuse, link previous block to this, this one to terminator
    
    pGlobalData->fState = pLocalData->fBlockNum;

    if(pLocalData->fBlockNum == 0)
        prevBlock = kNumPingPongs-1;
    else
        prevBlock = pLocalData->fBlockNum-1;
        
    ModifyDCLJump(pGlobalData->fStreamVars.pFWLocalIsochPort,
            pGlobalData->fLocalDataArray[prevBlock].fStateJmp, pLocalData->fStateLabel);
    ModifyDCLJump(pGlobalData->fStreamVars.pFWLocalIsochPort, pLocalData->fStateJmp, pGlobalData->fTerminal);

#if TIMING
    cend = CFAbsoluteTimeGetCurrent();
    DVLog(pGlobalData->fStreamVars.fThread, 'isoc', cstart, cend);
#endif
}

void DVReadPoll(DVGlobalInPtr globs)
{
    int i, pos;
    pos = globs->fState;
    for(i=pos; i<kNumPingPongs; i++)
        if(*globs->fLocalDataArray[i].fTimeStampPtr != 0xffffffff) {
            DVStorePackets(&globs->fLocalDataArray[i]);
            *globs->fLocalDataArray[i].fTimeStampPtr = 0xffffffff;
        }
            
    for(i=0; i<pos; i++)
        if(*globs->fLocalDataArray[i].fTimeStampPtr != 0xffffffff) {
            DVStorePackets(&globs->fLocalDataArray[i]);
            *globs->fLocalDataArray[i].fTimeStampPtr = 0xffffffff;
        }
}

IOReturn DVReadStart(DVGlobalInPtr globs)
{
    DCLCommandPtr	opcodes;
    UInt8 *			pingPongBuffer = NULL;
    UInt8 *			pingPongPtr;
    UInt8 *			pDCLCommand;
    DCLLabelPtr			pStartDCLLabel;
    DCLLabelPtr			pBlockDCLLabel;
    DCLLabelPtr			pUnderrunDCLLabel;
    DCLTransferPacketPtr	pDCLTransferPacket;
    DCLPtrTimeStampPtr	pDCLTimeStamp;
//    DCLCallProcPtr		pDCLPingPongProc;
    DCLCallProcPtr      pUnderrunDCLCallProc;
    DCLJumpPtr			pDCLPingPongLoop;
    int				pingPongNum, packetNum;
    UInt32			updateListSize;
    UInt32			bufferSize;
    DCLUpdateDCLListPtr		pDCLUpdateDCLList;
    DCLCommandPtr		*updateDCLList, *startUpdateDCLList;
    DVLocalInPtr		pLocalData;
    IOReturn			res;
    UInt32 *			timeStampPtr;
    int 				i;
	UInt32				packetBufferSize;
	UInt32 				alignedDVPacketSize;
	UInt32 				pingPongBufferSize;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVReadStart begin\n");
#endif
	
    //syslog(LOG_INFO, "DVReadStart() %p\n", globs);

    // init variables
    pingPongBuffer = NULL;

    globs->fStreamVars.pDCLList = NULL;
    globs->ppUpdateDCLList = NULL;
    globs->pImageBuffer = NULL;
    globs->fStreamVars.fDCLBuffers = NULL;
//    globs->fStreamVars.fSignalMode = kAVCSignalModeMask_50;	// initialize to bigger packets per frame (PAL)
    globs->packetCount = 0;
    globs->fState = 0;

    globs->pendingDVReadUnderrunHandler = false;
    globs->deferredDVReadFree = false;
    globs->dvReadStopInProgress = false;

	switch (globs->fStreamVars.fSignalMode)
	{

		case kAVCSignalModeSDL525_60:
		case kAVCSignalModeSDL625_50:
			packetBufferSize = 252;
			alignedDVPacketSize = 512;
			break;

		case kAVCSignalModeDVCPro50_525_60:
		case kAVCSignalModeHD1125_60:
		case kAVCSignalModeDVCPro50_625_50:
		case kAVCSignalModeHD1250_50:
			packetBufferSize = 972;
			alignedDVPacketSize = 1024;
			break;

		case kAVCSignalModeDVCPro100_525_60:
		case kAVCSignalModeDVCPro100_625_50:
			packetBufferSize = 1932;
			alignedDVPacketSize = 2048;
			break;

		case kAVCSignalModeSD525_60:
		case kAVCSignalModeDVCPro525_60:
		case kAVCSignalModeSD625_50:
		case kAVCSignalModeDVCPro625_50:
		default:
			packetBufferSize = 492;
			alignedDVPacketSize = 512;
			break;
	};

	pingPongBufferSize = kNumPingPongs * kNumPacketsPerPingPong * alignedDVPacketSize;
	
    // Create ping pong buffer, overrun buffer and time stamp buffer
    //zzz should allocate in initialization routine.
    bufferSize = pingPongBufferSize + alignedDVPacketSize + kNumPingPongs * sizeof(UInt32);
    vm_allocate(mach_task_self(), (vm_address_t *)&pingPongBuffer,
        bufferSize, VM_FLAGS_ANYWHERE);
    if (pingPongBuffer == NULL)
    {
        // syslog(LOG_INFO, "DVRead::Start : IOMalloc: pingPongBuffer failed\n");
        res = kIOReturnNoMemory;
        goto bail;
    }
    timeStampPtr = (UInt32 *)(pingPongBuffer + pingPongBufferSize + alignedDVPacketSize);
    globs->fStreamVars.fDCLBuffers = pingPongBuffer;
    globs->fStreamVars.fDCLBufferSize = bufferSize;
    bzero( pingPongBuffer, bufferSize );

    // Get pointer to start of DCL commands and update list.
    opcodes = (DCLCommandPtr)malloc(kRecordDCLProgramSize);
    globs->fStreamVars.pDCLList = opcodes;
    if (opcodes == NULL)
    {
        // syslog(LOG_INFO, "DVRead::Start : IOMalloc: opcodes failed\n");
        res = kIOReturnNoMemory;
        goto bail;
    }
    bzero( opcodes, kRecordDCLProgramSize );
    pDCLCommand = (UInt8 *)opcodes;
    updateDCLList = (DCLCommandPtr *)malloc(kRecordNumDCLs * sizeof(DCLCommandPtr));
    globs->ppUpdateDCLList = updateDCLList;
    if (updateDCLList == NULL)
    {
        // syslog(LOG_INFO, "DVRead::Start : IOMalloc: updateDCLList failed\n");
        res = kIOReturnNoMemory;
        goto bail;
    }
    bzero( updateDCLList, kRecordNumDCLs * sizeof(DCLCommandPtr));

    // Create label for start of loop.
    pStartDCLLabel = (DCLLabelPtr) pDCLCommand;
    pDCLCommand += sizeof (DCLLabel);
    pStartDCLLabel->pNextDCLCommand = (DCLCommandPtr) pDCLCommand;
    pStartDCLLabel->opcode = kDCLLabelOp;
    pingPongPtr = pingPongBuffer;

    // Create kNumPingPongs ping pong buffer lists of 100 packets each.
    for (pingPongNum = 0; pingPongNum < kNumPingPongs; pingPongNum++)
    {
            // Create the DCL input record record and fill it in.
            pLocalData = &globs->fLocalDataArray[pingPongNum];
            //pLocalData->fFirstCmd = (DCLCommandPtr) pDCLCommand;
            pLocalData->pGlobalData = globs;
            pLocalData->fBlockNum = pingPongNum;
            pLocalData->fTimeStampPtr = timeStampPtr;
            *timeStampPtr = 0xffffffff;
            startUpdateDCLList = updateDCLList;
            updateListSize = 0;
            // Create block start label 
            pBlockDCLLabel = (DCLLabelPtr) pDCLCommand;
            pDCLCommand += sizeof (DCLLabel);
            pBlockDCLLabel->pNextDCLCommand = (DCLCommandPtr) pDCLCommand;
            pBlockDCLLabel->opcode = kDCLLabelOp;
            pLocalData->fStateLabel = pBlockDCLLabel;

            pLocalData->fFirstCmd = (DCLCommandPtr) pDCLCommand;

            // Create transfer DCL for each packet.
            for (packetNum = 0; packetNum < kNumPacketsPerPingPong; packetNum++)
            {
                    // Receive one packet up to packetBufferSize bytes.
                    pDCLTransferPacket = (DCLTransferPacketPtr) pDCLCommand;
                    pDCLCommand += sizeof (DCLTransferPacket);
                    pDCLTransferPacket->pNextDCLCommand = (DCLCommandPtr) pDCLCommand;
                    pDCLTransferPacket->opcode = kDCLReceivePacketStartOp;
                    pDCLTransferPacket->buffer = pingPongPtr;
                    pDCLTransferPacket->size = packetBufferSize;

                    *updateDCLList++ = (DCLCommandPtr) pDCLTransferPacket;
                    updateListSize++;
                    pingPongPtr += alignedDVPacketSize;
            }

            // Create timestamp.                      
            pDCLTimeStamp = (DCLPtrTimeStampPtr) pDCLCommand;
            pDCLCommand += sizeof (DCLPtrTimeStamp);
            pDCLTimeStamp->pNextDCLCommand = (DCLCommandPtr) pDCLCommand;
            pDCLTimeStamp->opcode = kDCLPtrTimeStampOp;
            pDCLTimeStamp->timeStampPtr = timeStampPtr++;
            *updateDCLList++ = (DCLCommandPtr) pDCLTimeStamp;
            updateListSize++;
            // Create update DCL list.                      
            pDCLUpdateDCLList = (DCLUpdateDCLListPtr) pDCLCommand;
            pDCLCommand += sizeof (DCLUpdateDCLList);  
            pDCLUpdateDCLList->pNextDCLCommand = (DCLCommandPtr) pDCLCommand;
            pDCLUpdateDCLList->opcode = kDCLUpdateDCLListOp;
            pDCLUpdateDCLList->dclCommandList = startUpdateDCLList;
            pDCLUpdateDCLList->numDCLCommands = updateListSize;

            // Jump to next block (to terminator for last block)
            pDCLPingPongLoop = (DCLJumpPtr) pDCLCommand;
            pDCLCommand += sizeof (DCLJump);
            pDCLPingPongLoop->pNextDCLCommand = (DCLCommandPtr) pDCLCommand;
            pDCLPingPongLoop->opcode = kDCLJumpOp | kFWDCLOpDynamicFlag;
            pDCLPingPongLoop->pJumpDCLLabel = (DCLLabelPtr)pDCLCommand;
            pLocalData->fStateJmp = pDCLPingPongLoop;

    }

    // Create label for underrun.
    pUnderrunDCLLabel = (DCLLabelPtr) pDCLCommand;
    //pGlobalData->pUnderrunDCLLabel = pUnderrunDCLLabel;
    pDCLCommand += sizeof (DCLLabel);
    pUnderrunDCLLabel->pNextDCLCommand = (DCLCommandPtr) pDCLCommand;
    pUnderrunDCLLabel->opcode = kDCLLabelOp;
    globs->fTerminal = pUnderrunDCLLabel;
    
    // Create receive command for underrun packet
    pDCLTransferPacket = (DCLTransferPacketPtr) pDCLCommand;
    pDCLCommand += sizeof (DCLTransferPacket);
    pDCLTransferPacket->pNextDCLCommand = (DCLCommandPtr) pDCLCommand;
    pDCLTransferPacket->opcode = kDCLReceivePacketStartOp;
    pDCLTransferPacket->buffer = pingPongPtr;
    pDCLTransferPacket->size = packetBufferSize;
    
    // Call underrun proc.
    pUnderrunDCLCallProc = (DCLCallProcPtr) pDCLCommand;
    pDCLCommand += sizeof (DCLCallProc);
    pUnderrunDCLCallProc->pNextDCLCommand = (DCLCommandPtr) pDCLCommand;
    pUnderrunDCLCallProc->opcode = kDCLCallProcOp;
    pUnderrunDCLCallProc->proc = DVReadHandleInputUnderrun;
    pUnderrunDCLCallProc->procData = (UInt32)globs;

    // Create closed loop at end to flush isoc packets out of FIFO
    
    pUnderrunDCLLabel = (DCLLabelPtr) pDCLCommand;
    pDCLCommand += sizeof (DCLLabel);
    pUnderrunDCLLabel->pNextDCLCommand = (DCLCommandPtr) pDCLCommand;
    pUnderrunDCLLabel->opcode = kDCLLabelOp;
    
    // Create receive command for underrun packet
    pDCLTransferPacket = (DCLTransferPacketPtr) pDCLCommand;
    pDCLCommand += sizeof (DCLTransferPacket);
    pDCLTransferPacket->pNextDCLCommand = (DCLCommandPtr) pDCLCommand;
    pDCLTransferPacket->opcode = kDCLReceivePacketStartOp;
    pDCLTransferPacket->buffer = pingPongPtr;
    pDCLTransferPacket->size = packetBufferSize;

    // Loop back to keep dumping packets into the bucket
    // This is the last command.
    pDCLPingPongLoop = (DCLJumpPtr) pDCLCommand;
    pDCLPingPongLoop->pNextDCLCommand = NULL;
    pDCLPingPongLoop->opcode = kDCLJumpOp;
    pDCLPingPongLoop->pJumpDCLLabel = pUnderrunDCLLabel;
    
    // Initialize isochronous channel.
    res = openStream(&globs->fStreamVars, false, kDVSDPayloadPacketSize + kDVPacketCIPSize);
    if(res != kIOReturnSuccess)
        goto bail;

    for(i=0; i<kDVMaxStreamsActive; i++) {
        if(globs->fStreamVars.fThread->fInStreams[i] == NULL) {
            globs->fStreamVars.fThread->fInStreams[i] = globs;
            break;
        }
    }

     //syslog(LOG_INFO, "DVRead::Started()\n");

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVReadStart end\n");
#endif
	
    return kIOReturnSuccess;

bail:
    syslog(LOG_INFO, "DVRead::Start() failed: 0x%x\n", res);
    //Stop();

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVReadStart end:bail\n");
#endif
	
    return res;
}

static IOReturn doDVReadStop(DVGlobalInPtr pGlobalData)
{
    int i;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: doDVReadStop begin\n");
#endif
	
    pGlobalData->dvReadStopInProgress = true;
    
    //syslog(LOG_INFO, "doDVReadStop()0x%x\n", pGlobalData);
    for(i=0; i<kDVMaxStreamsActive; i++) {
        if(pGlobalData->fStreamVars.fThread->fInStreams[i] == pGlobalData) {
            pGlobalData->fStreamVars.fThread->fInStreams[i] = NULL;
            break;
        }
    }

#ifdef USE_P2P_CONNECTIONS_FOR_DV_READ            
    BreakP2PConnectionForRead(pGlobalData->fStreamVars.pDVDevice,0,pGlobalData->fStreamVars.fIsocChannel);
#endif
    
    closeStream(&pGlobalData->fStreamVars);
    if ( pGlobalData->ppUpdateDCLList) {
        free( pGlobalData->ppUpdateDCLList); //,kRecordNumDCLs * sizeof(DCLCommandPtr));
        pGlobalData->ppUpdateDCLList = NULL;
    }
    if ( pGlobalData->fStreamVars.pDCLList) {
        //bzero(pGlobalData->fStreamVars.pDCLList, kRecordDCLProgramSize);
        free( pGlobalData->fStreamVars.pDCLList); //, kRecordDCLProgramSize);
        pGlobalData->fStreamVars.pDCLList = NULL;
    }
    if ( pGlobalData->fStreamVars.fDCLBuffers) {
        vm_deallocate(mach_task_self(), (vm_address_t)pGlobalData->fStreamVars.fDCLBuffers,
                pGlobalData->fStreamVars.fDCLBufferSize);
        pGlobalData->fStreamVars.fDCLBuffers = NULL;
    }
    
    pGlobalData->dvReadStopInProgress = false;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: doDVReadStop end\n");
#endif
	
    return kIOReturnSuccess;
}

void DVReadStop(DVGlobalInPtr pGlobalData)
{
#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVReadStop begin\n");
#endif
	
    DVRequest(pGlobalData->fStreamVars.fThread, doDVReadStop, pGlobalData, 0);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVReadStop end\n");
#endif
	
}

void DVReadFreeFrames(DVGlobalInPtr globs)
{
#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVReadFreeFrames begin\n");
#endif
	
    DVFreeFrames(&globs->fStreamVars.fFrames);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVReadFreeFrames end\n");
#endif
	
}

void DVReadFree(DVGlobalInPtr globs)
{

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVReadFree begin\n");
#endif
	
    // Defer freeing of the globalin data struct
    // if we have a pending input underrun to deal with.
	
    if (globs->pendingDVReadUnderrunHandler == true)
	globs->deferredDVReadFree = true;
    else
	free(globs);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "DVLib: DVReadFree end\n");
#endif
	
}

void DVLog(DVThread *thread, UInt32 tag, CFAbsoluteTime start, CFAbsoluteTime end)
{
#if TIMING
    Log * log;
    
    log = &thread->fLog[thread->fLogPos];
    log->tag = tag;
    log->start = start;
    log->end = end;
    thread->fLogPos++;
    if(thread->fLogPos >= kLogSize)
        thread->fLogPos = 0;
#endif
}

void DVDumpLog(DVThread *thread)
{
#if TIMING
    Log * log;
    UInt32 tag;
    int i;
    
    for(i=thread->fLogPos; i<kLogSize; i++) {
        log = &thread->fLog[i];
        tag = log->tag;
        if(tag) {
            syslog(LOG_INFO, "%d %c%c%c%c %8.3f to %8.3f\n", i, tag>>24, tag>>16, tag>>8, tag, log->start, log->end);
        }
        else
            syslog(LOG_INFO, "%d %x %8.3f to %8.3f\n", i, tag, log->start, log->end);
    }
    
    for(i=0; i< thread->fLogPos; i++) {
        log = &thread->fLog[i];
        tag = log->tag;
        if(tag) {
            syslog(LOG_INFO, "%d %c%c%c%c %8.3f to %8.3f\n", i, tag>>24, tag>>16, tag>>8, tag, log->start, log->end);
        }
        else
            syslog(LOG_INFO, "%d %x %8.3f to %8.3f\n", i, tag, log->start, log->end);
    }
#endif
}
