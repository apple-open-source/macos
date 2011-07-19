/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

#include <IOKit/IOKitLib.h>
#include <IOKit/firewire/IOFireWireLib.h>
#include <IOKit/avc/IOFireWireAVCLib.h>

// Turn on logging of when events happen, and log dump when something takes too long
//#define TIMING 1

//#define kIDH_Verbose_Debug_Logging 1

#define kDVMaxDevicesActive		64	// max devices on firewire bus
#define kDVMaxStreamsActive		64	// max isoc channels on fw bus

#define kLogSize 20
typedef struct _Log  {
    UInt32 tag;
    CFAbsoluteTime start;
    CFAbsoluteTime end;
} Log;


// pthread stuff
typedef struct _syncStruct
{
    pthread_mutex_t		fMutex;			// lock this before updating globals
    pthread_cond_t		fSyncCond;		// To synchronize threads.
} ThreadSyncer;

typedef struct _DVThread DVThread;
struct DVGlobalOutStruct;
typedef struct DVGlobalOutStruct DVGlobalOut, *DVGlobalOutPtr;

struct DVGlobalInStruct;
typedef struct DVGlobalInStruct DVGlobalIn, *DVGlobalInPtr;

// device structure
typedef struct _DVDevice
{
    DVThread *fThread;
    io_object_t fObject;
	IOFireWireLibDeviceRef	fDevInterface;
    IOFireWireAVCLibUnitInterface **fAVCInterface;
    IOFireWireAVCLibProtocolInterface **fAVCProtoInterface;
    UInt64 		fGUID;
    char 		fName[256];
    UInt32 		fOutPlug;
    UInt32 		fDVFormats; 		// DV formats supported by device
    UInt8 		standard; 			// device standard - NTSC/PAL
    UInt8		fWriteChan;			// Channel the Mac writes to the device on
    UInt8		fReadChan;			// Channel the Mac reads from the device on
    UInt8		fMaxSpeed;			// Max bus speed for isoc channel
    bool		fSupportsFCP;		// Does device support AVC commands using the FCP protocol?
    UInt8		p2pConnected;
    UInt32		p2pPlug;
    UInt32		p2pChan;
    UInt32		deviceIndex;		// 1-based device index for message callback refcon
} DVDevice;

// callback functions

typedef void (*DVDeviceArrivedFunc)(void *refcon, DVDevice *device, UInt32 index, UInt32 refound);

struct _DVThread
{
    pthread_t				fRTThread;			// Real Time thread, avoid CF.
    pthread_t				fRLThread;			// Run-loop thread for device messages
	IONotificationPortRef	fNotifyPort;		// Our IOKit notification port
	CFRunLoopSourceRef		fNotifySource;		// notify port as a runloop source
#if 0
	CFRunLoopSourceRef		fRequestSource;		// runloop source to ask workthread to do stuff
	CFRunLoopTimerRef		fTimerSource;
#else
    mach_port_t				fRequestMachPort;		// for requests to real time thread
    CFRunLoopTimerCallBack	fTimerFunc;				// Fake CR timer callbacks on RT thread
    void *					fTimerRefCon;
#endif
	CFRunLoopRef			fWorkLoop;
	io_iterator_t			fMatchEnumer;			// Iterator over matching devices
	io_iterator_t			fTermEnumer;			// Iterator over terminated devices
    DVDeviceArrivedFunc		fAddedFunc;
    void * 					fAddedRefCon;
    IOServiceMatchingCallback	fRemovedFunc;
    void * 					fRemovedRefCon;
    IOFWAVCMessageCallback	fDeviceMessage;
    pthread_mutex_t			fRequestMutex;
	ThreadSyncer			fRequestSyncer;
    UInt32					fSyncRequest;		// Flag to indicate completion of synchronous operation
    IOReturn 				(*fRequestFunc)(void *arg, UInt32 param);
    void *					fRequestArg;
    UInt32					fRequestParam;
    IOReturn				fRequestResult;
    UInt32					fNumDevices;
    DVDevice				fDevices[kDVMaxDevicesActive];
    CFAbsoluteTime			setTimeoutTime;
    CFAbsoluteTime			requestTimeoutTime;
    DVGlobalOut	*			fOutStreams[kDVMaxStreamsActive];
    DVGlobalIn	*			fInStreams[kDVMaxStreamsActive];

    UInt32					fLogPos;
    Log						fLog[kLogSize];

	io_object_t             fPowerManagementNotifier;
	IONotificationPortRef	fPowerNotifyPort;
	CFRunLoopSourceRef		fPowerNotifySource;
	io_connect_t			fPowerNotifyConnect;
	bool					fRunLoopIsRunning;
};


enum {
    kNumPlayBufferGroups	= 6,	// Blocks of packet send ops in output program
    kDVMaxFrames			= 20	// Max number of DV frames in ring buffer
};

enum {
    kEmpty = 0,
    kReading = 1,
    kWriting = 2,
    kReady = 3,
    
};

typedef struct _DVSharedVars {
    UInt32 					fNumGroups;		// Number of blocks in DCL program
    UInt32 					fGroupSize;		// Number of data packets in DCL group
    UInt32					fAlignedPacketSize;	// Total packet size (offset from one to next)
    UInt32					fPacketDataSize;	// Size of DV data in packet
    UInt32					fDMAPos;			// Which block DMA has just done
    volatile UInt32	*		fTimeStampPtrs;		// Pointer to array of timestamps, taken at end of block transmit
    UInt32					fDataOffset[kNumPlayBufferGroups];	// into DCL data buffer
} DVSharedVars;

typedef struct _DVFrameVars {
    volatile UInt32 fReader;
    volatile UInt32 fWriter;
    volatile UInt32 fDroppedFrames;
    volatile UInt32 fStatus;
    volatile UInt32 fFrameSize[kDVMaxFrames];
    volatile UInt8 fFrameStandard[kDVMaxFrames];
    volatile UInt32 fFrameTime[kDVMaxFrames];
    volatile UInt8 fFrameStatus[kDVMaxFrames];
    UInt32			  fNumFrames;
    UInt8 *			  fFrames;
} DVFrameVars;


extern DVThread * DVCreateThread(DVDeviceArrivedFunc deviceAdded, void * addedRefCon,
    CFRunLoopTimerCallBack timerTick, void *timerRefCon, IOFWAVCMessageCallback deviceMessage);
extern void DVRunThread(DVThread *thread);
extern void DVFreeThread(DVThread *thread);
extern void DVSetTimeoutTime(DVThread * dvThread, CFAbsoluteTime fireDate);

extern void DVSignalSync(ThreadSyncer *sync, UInt32 *var, UInt32 val);
extern void DVWaitSync(ThreadSyncer *sync, UInt32 *var);
extern void DVLock(ThreadSyncer *sync);
extern void DVUnlock(ThreadSyncer *sync);
extern IOReturn DVRequest(DVThread *thread, IOReturn (*func)(void *arg, UInt32 param), void *arg, UInt32 param);

extern IOReturn openFireWireUnit(IOFireWireAVCLibUnitInterface **avcInterface, IOFireWireSessionRef session, IOFireWireLibDeviceRef *retInterface, DVThread *thread);
extern IOReturn openAVCUnit(io_object_t obj, IOFireWireAVCLibUnitInterface ***retInterface, DVThread *thread);
extern IOReturn openAVCProto(IOFireWireAVCLibUnitInterface **avcInterface, IOFireWireAVCLibProtocolInterface ***retInterface, DVThread *thread);

extern IOReturn DVDeviceInit(DVThread *thread, DVDevice *device,
        io_object_t obj, IOServiceInterestCallback deviceMessage, void * refCon);
extern void DVDeviceTerminate(DVDevice *device);
extern IOReturn DVDeviceOpen(DVThread *thread, DVDevice *device);
extern void DVDeviceClose(DVDevice *device);

extern DVGlobalOutPtr DVAllocWrite(DVDevice *device, DVThread *thread);
extern IOReturn DVWriteSetSignalMode(DVGlobalOutPtr globs, UInt8 mode);
extern IOReturn DVWriteAllocFrames(DVGlobalOutPtr globs, UInt32 numFrames, 
        DVFrameVars **frameVars, UInt8 **frames);
extern UInt8 * DVWriteGetDCLBuffer(DVGlobalOutPtr globs, DVSharedVars **varPtr);
extern IOReturn DVWriteStart(DVGlobalOutPtr globs);
extern void DVWriteStop(DVGlobalOutPtr globs);
extern void DVWriteFreeFrames(DVGlobalOutPtr globs);
extern void DVWriteFree(DVGlobalOutPtr globs);

extern DVGlobalInPtr DVAllocRead(DVDevice *device, DVThread *thread);
extern IOReturn DVReadSetSignalMode(DVGlobalInPtr globs, UInt8 mode);
extern IOReturn DVReadAllocFrames(DVGlobalInPtr globs, UInt32 numFrames, 
        DVFrameVars **frameVars, UInt8 **frames);
extern IOReturn DVReadStart(DVGlobalInPtr globs);
extern void DVReadStop(DVGlobalInPtr globs);
extern void DVReadFreeFrames(DVGlobalInPtr globs);
extern void DVReadFree(DVGlobalInPtr globs);

extern void DVSilenceFrame(UInt8 mode, UInt8* frame);

extern void DVLog(DVThread *thread, UInt32 tag, CFAbsoluteTime start, CFAbsoluteTime end);
extern void DVDumpLog(DVThread *thread);
