/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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

#include <Carbon/Carbon.h>

typedef struct IsochComponentInstance IsochComponentInstance, *IsochComponentInstancePtr;
typedef struct IsochComponentGlobals IsochComponentGlobals, *IsochComponentGlobalsPtr;

#define CALLCOMPONENT_BASENAME() IDHDV
#define CALLCOMPONENT_GLOBALS()	IsochComponentInstancePtr storage

#define IDH_BASENAME()	FWDV
#define IDH_GLOBALS()	IsochComponentInstancePtr storage
#include "IsochronousDataHandler.k.h"

#include "DeviceControlPriv.h"

#include "DVVers.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // usleep()
#include <pthread.h>
#include <syslog.h>	// Debug messages

// Timebase stuff
#include <mach/clock.h>
#include <mach/clock_types.h>
// Trace stuff
#include <sys/syscall.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

#include <IOKit/IOMessage.h>
#include <IOKit/firewire/IOFireWireFamilyCommon.h>
#include <IOKit/avc/IOFireWireAVCConsts.h>

#include "DVLib.h"

#define kDVRequestID 0
#define kIOPending 1
#define kIORunning 2
#define kMaxDeviceClients 4

#define kDCLBlockTime (100.0/8000.0)

#define DEBUG 0
//#define DRAWRINGBUFFERGRAPH
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define kDVDeviceInfo 			'ddin'	// DV device info
#define kMaxInstances			10	// max clients attached to a device
#define kMaxNotifications		100	// 100 notifications can be supported
#define kNTSCCompressedBufferSize	120000
#define	kPALCompressedBufferSize	144000
#define kIDHSeedAtomType		'seed'	// seed for device atom validity
#define kIDHDevicePresentType		'pres'	// is device present?
#define	kServiceTypeDVServices		'avc '
#define kTimeoutDuration		(1000 / 15) * durationMillisecond	// 1/15 second (2 frame times)

typedef OSStatus (*RequestFunc)(IsochComponentInstancePtr ih, void *params);

typedef struct ClientNotification {
        ComponentInstance 	ihc;
        IDHDeviceID			deviceID;
        IDHNotificationProc	notificationProc;
        IDHEvent			events;
        void				*userData;
} ClientNotification;

// describes a device on the firewire bus
typedef struct DeviceDescription {
    DVDevice 		*fDevice;
    IsochComponentGlobalsPtr fGlobals;
    UInt32 		fNumOutputFrames;
    UInt8* bufMem[kDVMaxFrames];
    DVFrameVars *fReadSharedVars;	// Structure shared with isoc program thread
    DVFrameVars *fWriteSharedVars;	// Structure shared with isoc program thread
    DVGlobalOutPtr fWrite;
    DVGlobalInPtr fRead;
    UInt8 * 	fDCLBuffer;			// DCL Buffers shared with kernel driver
    DVSharedVars *fSharedWriteVars;	// For access to output DCL variables
    UInt32 *	fOldWriteTimeStamps;
    int			fDCLReadPos;		// Which block the DCL write program will process next
    int			fBufWritePos;		// Next block to write data into
    int			fFrameBufferPos;	// Current offset into current frame buffer
    int			fDCLSavedPacketNum; // Current packet in DCL buffer when we're waiting for a frame
    UInt8 *		fDCLSavedWritePos;	// Current position in DCL buffer when we're waiting for the next frame
    int			fBufSize;			// NTSC or PAL frame
    int			fOldDrop;			// Dropped before
    int			fOldRead;			// Where DV driver was before
    IsochComponentInstancePtr	fOpenClients[kMaxDeviceClients];	// component instances doing I/O to/from device
    int			fBufferLocks[kDVMaxFrames];
    ComponentInstance	deviceControlInstance;		// device control component instance
    Component		clock;				// FireWire clock component, clients get their own instance, not sure why
    QTAtomContainer	deviceContainer;		// contains device database
    SInt16		deviceControlCount;		// number of clients using device control
    SInt16		readLocks;			// num clients that have device open for reads
    SInt16		writeLocks;			// num clients that have device open for writes
    Boolean		fActive;			// device is currently online
    UInt8		fOutputMode;		// AVC output signal mode - NTSC/SDL etc.
    UInt8		fWaitingStartWrite;	// 1 = if we're filling up buffers before starting to write, 2 = need to retry start
    UInt8		fConnected;			// Number of times device has been selected (via IDHSetDeviceConfiguration)
    UInt8		fReadStage;
    UInt8		fNoAVC;				// True if device doesn't do AVC commands
#ifdef DRAWRINGBUFFERGRAPH
	Ptr		fScreenBaseAddr;
	UInt32	fPixDepth;
	UInt32	fRowBytes;
#endif 	 	
} DeviceDescription, *DeviceDescriptionPtr;

#ifdef DRAWRINGBUFFERGRAPH
static void	SetUpBlitGlobals(DeviceDescriptionPtr ddp);
static void	BlitBufferGraph(DeviceDescriptionPtr ddp);
#endif 	 	

struct IsochComponentGlobals
{
    // Globals
    UInt32		useCMP;					// CMP off by default, turn on by adding 'ucmp' resource
    DeviceDescription	deviceDescription[kDVMaxDevicesActive];	// description of each device
    ClientNotification	clientNotification[kMaxNotifications];	// arbirtary # of notifications
    UInt32		nDevices;				// number of devices in the list
    UInt32		seed;					// keep track of device lists

    // X Stuff
    DVThread *			fDVThread;
    CFBundleRef			fBundleID;				// Of the DV component's bundle
    UInt16				fBundleResRef;			// ref for the resource file
            
    int					fNumInstances;				   
};


struct IsochComponentInstance
{
    // Instance variables
    ComponentInstance	self;
    QTAtomSpec		currentConfig;				// current device config set by client
    IDHDeviceID		deviceID;					// current deviceID
    UInt16			fClientIndex;				// into device client list
    long		permissions;					// open permissions
    IDHParameterBlock *	fHead;					// Of I/O queue
    IDHParameterBlock *	fTail;					// Of I/O queue
    IDHParameterBlock *	fActiveHead;			// Of I/Os referencing the ring buffer
    IDHParameterBlock *	fActiveTail;			// Of I/O referencing the ring buffer
    UInt32			fSyncRequest;				// Flag to indicate completion of synchronous operation
    ComponentResult	fSyncResult;				// Result of sync operation
    Boolean			hasDeviceControl;			// does this client have device control?
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* Function prototypes */

pascal ComponentResult
FWDVICodecComponentDispatch(ComponentParameters *params, char ** storage);
static pascal ComponentResult
FWDVIDHCloseDevice(IsochComponentInstancePtr ih);

static OSStatus doAVCTransaction(DeviceDescriptionPtr deviceDescriptionPtr,
                DVCTransactionParams* inTransaction);

static OSStatus disableRead(DeviceDescription *deviceDescriptionPtr);
static void disableWrite(DeviceDescription *deviceDescriptionPtr);

/* Globals */
static IsochComponentGlobals globals;


/* ---------------------- Debug Stuff -------------------------- */
#ifdef DEBUG
#define FailMessage(cond)		assert (!(cond))
#else
#define FailMessage(cond)		{}
#endif
#define FailWithVal(cond, handler,num)	\
        if (cond) {			\
                goto handler;		\
        }
#define FailWithAction(cond, action, handler)	\
        if (cond) {				\
                { action; }			\
                goto handler;			\
        }

#define FailIf(cond, handler)	\
        if (cond) {		\
            FailMessage(false);	\
                goto handler;	\
        }

#if DEBUG
static char * print4(UInt32 val, char *buf)
{
    char a, b, c, d;
    a = val>>24;
    b = val>>16;
    c = val>>8;
    d = val;

    if(a >= ' ' && b >= ' ' && c >= ' ' && d >= ' ')
        //printf("%c%c%c%c", a, b, c, d);
        //syslog(LOG_INFO, "%c%c%c%c", a, b, c, d);
        snprintf(buf, 8, "%c%c%c%c", a, b, c, d);
    else
        //printf(" 0x%x ", (int)val);
        //syslog(LOG_INFO, " 0x%x ", (int)val);
        snprintf(buf, 8, " 0x%x ", (int)val);
        
    return buf + strlen(buf);
}

static void RecordEventLogger(UInt32 a, UInt32 b, UInt32 c, UInt32 d)
{
    char buf[256];
    char *curr = buf;
    snprintf(buf, 11, "0x%x:", (unsigned int) pthread_self());
    curr = buf + strlen(buf);
    if(a)
    curr = print4(a, curr);
    if(b)
    curr = print4(b, curr);
    if(c)
    curr = print4(c, curr);
    if(d)
    curr = print4(d, curr);
    printf("%s\n", buf);
    syslog(LOG_INFO, buf);
}
#else
#define RecordEventLogger(a, b, c, d)
#endif

//masks for SYS_kdebug_trace code 
#define kKernelTraceEnable	(1 << 0 )
#define kKernelTraceDisable	(1 << 1 )
#define kIOFWDVTrace 0x08001000

#if 0
static int pKGSysCall_Start (int code, int param1, int param2, int param3, int param4, int param5, int param6 )
{
	return syscall ( SYS_kdebug_trace, code | kKernelTraceEnable, param1, param2, param3, param4, param5, param6 );
}

static int pKGSysCall_End (int code, int param1, int param2, int param3, int param4, int param5, int param6 )
{
	return syscall ( SYS_kdebug_trace, code | kKernelTraceDisable, param1, param2, param3, param4, param5, param6 );
}
#endif

static int pKGSysCall_Insert (int code, int param1, int param2, int param3, int param4, int param5, int param6 )
{
	return syscall ( SYS_kdebug_trace, code , param1, param2, param3, param4, param5, param6 );
}

static void dropMsg(DeviceDescription *deviceDescriptionPtr, UInt32 dropped)
{
    IsochComponentGlobalsPtr g = deviceDescriptionPtr->fGlobals;
    UInt32 i;
    IDHDeviceFrameDroppedEvent dropEvent;
    pKGSysCall_Insert (kIOFWDVTrace, 'drop', dropped-deviceDescriptionPtr->fOldDrop, dropped, 0, 0, 0);

    for (i = 0; i < kMaxNotifications ; ++i)
    {
        ClientNotification* clientNotification = &g->clientNotification[i];
        IDHDeviceID clientDeviceID = clientNotification->deviceID;
        IDHEvent wantedEvents = clientNotification->events;
        if (kIDHEventFrameDropped & wantedEvents)
        {
            // Send notification if the device matches the client's desired device
            // or client specified all devices.
            if ((kIDHDeviceIDEveryDevice == clientDeviceID) ||
                    ((IDHDeviceID)deviceDescriptionPtr == clientDeviceID) )
            {
                // we currently only support a one-shot notification, like clock callbacks
                clientNotification->events = 0;
                
                dropEvent.eventHeader.event = kIDHEventFrameDropped;
                dropEvent.eventHeader.deviceID = (IDHDeviceID)deviceDescriptionPtr;
                dropEvent.eventHeader.notificationID =
                                (UInt32)clientNotification;
                dropEvent.totalDropped = dropped;
                dropEvent.newlyDropped = dropped - deviceDescriptionPtr->fOldDrop;
                
                (*clientNotification->notificationProc)
                                ((IDHGenericEvent*)&dropEvent,
                                clientNotification->userData);
            }
        }
    }
    
    // syslog(LOG_INFO,"Just dropped %ld frames (total %ld)!\n", dropped-deviceDescriptionPtr->fOldDrop, dropped);

    deviceDescriptionPtr->fOldDrop = dropped;
}

static inline UInt32 cycles(UInt32 cycleReg)
{
#if 0
    UInt32 result;
    result = ((cycleReg & 0xFE000000) >> 25) * 8000;
    result += (cycleReg & 0x01FFF000) >> 12;
    return result;
#else
	return (cycleReg & 0x01FFF000) >> 12;
#endif
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSStatus findDeviceDescriptionforDevice( IsochComponentInstancePtr ih, UInt32 deviceID, DeviceDescription **deviceDescription)
{
    OSStatus	result = noErr;
    *deviceDescription = (DeviceDescriptionPtr)deviceID;
    return result;
}

//еее do a super find atom someday
static OSStatus findAtom( const QTAtomSpec *atomSpec, OSType theType, QTAtom *theAtom)
{
        OSStatus result = noErr;
        OSType type;
        QTAtom atom;

        atom = atomSpec->atom;

        result = QTGetAtomTypeAndID( atomSpec->container, atom, (long *) &type, nil);
        FailWithVal( result != noErr, Exit, result);

        while( type != kIDHDeviceAtomType && type != theType)
        {
                atom = QTGetAtomParent( atomSpec->container, atom); // should be isoch atom
                FailWithAction( atom == nil || atom == -1, result = kIDHErrDeviceList, Exit);

                result = QTGetAtomTypeAndID( atomSpec->container, atom, (long *) &type, nil);
                FailWithVal( result != noErr, Exit, result);
        }

        if( theType == type)
        {
                *theAtom = atom;
                result = noErr;
        }
        else
        {
                *theAtom = nil;
                result = kIDHErrDeviceList;
        }

Exit:
        return result;
}

static OSStatus getDeviceID( QTAtomSpec *configID, UInt32 *deviceID)
{
        OSStatus result = noErr;
        QTAtom deviceAtom;

        *deviceID = nil;

        result = findAtom( configID, kIDHDeviceAtomType, &deviceAtom);
        FailWithVal( result != noErr, Exit, result);

        result = QTGetAtomTypeAndID( configID->container, deviceAtom, nil, (long *) deviceID);
        FailWithVal( result != noErr, Exit, result);

Exit:
        return result;
}

static OSStatus closeDeviceControl( IsochComponentInstancePtr ih, DeviceDescriptionPtr deviceDescriptionPtr)
{
        OSStatus 						result = noErr;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: closeDeviceControl begin\n");
#endif
	
        if( deviceDescriptionPtr->deviceControlInstance)
        {
                if( --deviceDescriptionPtr->deviceControlCount <= 0)
                {
                        deviceDescriptionPtr->deviceControlCount = 0;

                        result = DeviceControlDisableAVCTransactions(deviceDescriptionPtr->deviceControlInstance);
                        result = DeviceControlSetDeviceConnectionID(deviceDescriptionPtr->deviceControlInstance,
                                                                                            kIDHInvalidDeviceID);

                        CloseComponent(deviceDescriptionPtr->deviceControlInstance);

                        deviceDescriptionPtr->deviceControlInstance = nil;
                }
        }

#ifdef kIDH_Verbose_Debug_Logging
		syslog(LOG_INFO, "IDH: closeDeviceControl end\n");
#endif
		
        return result;
}

OSStatus checkSeed( IsochComponentGlobalsPtr gGlobals, QTAtomSpec *configID)
{
        QTAtom 		seedAtom;
        OSStatus 		result = noErr;
        UInt32		seed;

        // look for device in device container
        seedAtom = QTFindChildByIndex( configID->container, kParentAtomIsContainer, kIDHSeedAtomType, 1, nil);
        FailWithAction( seedAtom == nil, result = kIDHErrDeviceList, Exit);

        QTLockContainer( configID->container);

        // get the value of the devicePresent atom
        QTCopyAtomDataToPtr( configID->container, seedAtom, true, sizeof( seed), &seed, nil);

        QTUnlockContainer( configID->container);

        // seed has expired?
        if( seed != gGlobals->seed)
        {
                result = kIDHErrDeviceList;
                goto Exit;
        }

Exit:
        return result;
}

static OSStatus setupVideoAtoms( QTAtomContainer container, QTAtom isocAtom, UInt32 standard, Boolean isSDL, Boolean isDVCPro)
{
    OSStatus 					result = noErr;
    QTAtom 					configAtom;
    OSType 					type;
    long 					size;
    float 					interval;
    long					direction;
    IDHDimension 			dimension;
    IDHResolution 			resolution;
    Fixed 					refresh;
    OSType 					pixel;
    OSType 					decoType;
    Component 				decoComponent;
    ComponentDescription 	compDescrip;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: setupVideoAtoms begin\n");
#endif
	
    // create a vide NTSC mode
    result = QTInsertChild( container, isocAtom, kIDHIsochModeAtomType,
                    0, 0, 0, nil, &configAtom);
    FailWithVal( result != noErr, Exit, result);

    type = kIDHVideoMediaAtomType;
    result = QTInsertChild( container, configAtom, kIDHIsochMediaType,
                    0, 0, sizeof( type), &type, nil);
    FailWithVal( result != noErr, Exit, result);

    if(isSDL) {
        result = QTInsertChild( container, configAtom, kIDHNameAtomType,
                        0, 0, 7, "\pDV-SDL", nil);
    }
    else if(isDVCPro) {
        result = QTInsertChild( container, configAtom, kIDHNameAtomType,
                        0, 0, 7, "\pDVCPro", nil);
    }
    else {
        result = QTInsertChild( container, configAtom, kIDHNameAtomType,
                        0, 0, 3, "\pDV", nil);
    }
    FailWithVal( result != noErr, Exit, result);

    type = (standard == ntscIn)?'DVC ':'DVCP';
    result = QTInsertChild( container, configAtom, kIDHDataTypeAtomType,
                    0, 0, sizeof( type), &type, nil);
    FailWithVal( result != noErr, Exit, result);

    size = (standard == ntscIn)?kNTSCCompressedBufferSize:kPALCompressedBufferSize;
    if(isSDL)
        size /= 2;
    result = QTInsertChild( container, configAtom, kIDHDataSizeAtomType,
                    0, 0, sizeof( size), &size, nil);
    FailWithVal( result != noErr, Exit, result);

    result = QTInsertChild( container, configAtom, kIDHDataBufferSizeAtomType,
                    0, 0, sizeof( size), &size, nil);
    FailWithVal( result != noErr, Exit, result);

    interval = 29.97;
    result = QTInsertChild( container, configAtom, kIDHDataIntervalAtomType,
                    0, 0, sizeof( interval), &interval, nil);
    FailWithVal( result != noErr, Exit, result);

    direction = kIDHDataTypeIsInputAndOutput;
    result = QTInsertChild( container, configAtom, kIDHDataIODirectionAtomType,
                    0, 0, sizeof( direction), &direction, nil);
    FailWithVal( result != noErr, Exit, result);

    dimension.x = 720; dimension.y = (standard == ntscIn)?480:576;
    result = QTInsertChild( container, configAtom, kIDHVideoDimensionsAtomType,
                    0, 0, sizeof( dimension), &dimension, nil);
    FailWithVal( result != noErr, Exit, result);

    resolution.x = 72 << 16; resolution.y = 72 << 16;
    result = QTInsertChild( container, configAtom, kIDHVideoResolutionAtomType,
                    0, 0, sizeof( resolution), &resolution, nil);
    FailWithVal( result != noErr, Exit, result);

    refresh = (29 << 16) + 97; //еее
    result = QTInsertChild( container, configAtom, kIDHVideoRefreshRateAtomType,
                    0, 0, sizeof( refresh), &refresh, nil);
    FailWithVal( result != noErr, Exit, result);

    pixel = 'dv  '; //еее
    result = QTInsertChild( container, configAtom, kIDHVideoPixelTypeAtomType,
                    0, 0, sizeof( pixel), &pixel, nil);
    FailWithVal( result != noErr, Exit, result);

//еее	kIDHVideoDecompressorsAtomType		= FOUR_CHAR_CODE('deco'),

    decoType = (standard == ntscIn)?'dvc ':'dvcp';
    result = QTInsertChild( container, configAtom, kIDHVideoDecompressorTypeAtomType,
                    0, 0, sizeof( decoType), &decoType, nil);
    FailWithVal( result != noErr, Exit, result);

    compDescrip.componentType = 'imdc';
    compDescrip.componentSubType = decoType;
    compDescrip.componentManufacturer = 0;
    compDescrip.componentFlags = 0;
    compDescrip.componentFlagsMask = 0;

    decoComponent = FindNextComponent( nil, &compDescrip);
    result = QTInsertChild( container, configAtom, kIDHVideoDecompressorComponentAtomType,
                    0, 0, sizeof( decoComponent), &decoComponent, nil);
    FailWithVal( result != noErr, Exit, result);

Exit:

#ifdef kIDH_Verbose_Debug_Logging
		syslog(LOG_INFO, "IDH: setupVideoAtoms end\n");
#endif
	
	return result;
}

static OSStatus setup48kAudioAtoms( QTAtomContainer container, QTAtom isocAtom, UInt32 standard)
{
        OSStatus 		err;
        QTAtom 		configAtom;

        StringPtr 	name;
        OSType 		type;
        long 		size;
        Fixed 		rate;
        float 		interval;
        long 		direction;

        // create a vide NTSC mode
        err = QTInsertChild( container, isocAtom, kIDHIsochModeAtomType,
                        0, 0, 0, nil, &configAtom);
        FailWithVal( err != noErr, Exit, err);

        type = kIDHSoundMediaAtomType;
        err = QTInsertChild( container, configAtom, kIDHIsochMediaType,
                        0, 0, sizeof( type), &type, nil);
        FailWithVal( err != noErr, Exit, err);

        name = "\pDV-48khz";
        err = QTInsertChild( container, configAtom, kIDHNameAtomType,
                        0, 0, name[0]+1, name, nil);
        FailWithVal( err != noErr, Exit, err);

        type = 'DV48';
        err = QTInsertChild( container, configAtom, kIDHDataTypeAtomType,
                        0, 0, sizeof( type), &type, nil);
        FailWithVal( err != noErr, Exit, err);

        size = (standard == ntscIn)?kNTSCCompressedBufferSize:kPALCompressedBufferSize;
        err = QTInsertChild( container, configAtom, kIDHDataSizeAtomType,
                        0, 0, sizeof( size), &size, nil);
        FailWithVal( err != noErr, Exit, err);

        size = (standard == ntscIn)?kNTSCCompressedBufferSize:kPALCompressedBufferSize;
        err = QTInsertChild( container, configAtom, kIDHDataBufferSizeAtomType,
                        0, 0, sizeof( size), &size, nil);
        FailWithVal( err != noErr, Exit, err);

        interval = 29.97;
        err = QTInsertChild( container, configAtom, kIDHDataIntervalAtomType,
                        0, 0, sizeof( interval), &interval, nil);
        FailWithVal( err != noErr, Exit, err);

        direction = kIDHDataTypeIsInputAndOutput;
        err = QTInsertChild( container, configAtom, kIDHDataIODirectionAtomType,
                        0, 0, sizeof( direction), &direction, nil);
        FailWithVal( err != noErr, Exit, err);

        size = 2;
        err = QTInsertChild( container, configAtom, kIDHSoundChannelCountAtomType,
                        0, 0, sizeof( size), &size, nil);
        FailWithVal( err != noErr, Exit, err);

        size = 2;
        err = QTInsertChild( container, configAtom, kIDHSoundSampleSizeAtomType,
                        0, 0, sizeof( size), &size, nil);
        FailWithVal( err != noErr, Exit, err);

        rate = rate44khz;
        err = QTInsertChild( container, configAtom, kIDHSoundSampleRateAtomType,
                        0, 0, sizeof( rate), &rate, nil);
        FailWithVal( err != noErr, Exit, err);

Exit:
        return err;
} // sound 1 config

static OSStatus setup32kAudioAtoms( QTAtomContainer container, QTAtom isocAtom, UInt32 standard)
{
        OSStatus err;
        QTAtom configAtom;

        StringPtr 	name;
        OSType 		type;
        long 		size;
        Fixed 		rate;
        float 		interval;
        long 		direction;

        // create a vide NTSC mode
        err = QTInsertChild( container, isocAtom, kIDHIsochModeAtomType,
                        0, 0, 0, nil, &configAtom);
        FailWithVal( err != noErr, Exit, err);

        type = kIDHSoundMediaAtomType;
        err = QTInsertChild( container, configAtom, kIDHIsochMediaType,
                        0, 0, sizeof( type), &type, nil);
        FailWithVal( err != noErr, Exit, err);

        name = "\pDV-32khz";
        err = QTInsertChild( container, configAtom, kIDHNameAtomType,
                        0, 0, name[0]+1, name, nil);
        FailWithVal( err != noErr, Exit, err);

        type = 'DV32';
        err = QTInsertChild( container, configAtom, kIDHDataTypeAtomType,
                        0, 0, sizeof( type), &type, nil);
        FailWithVal( err != noErr, Exit, err);

        size = (standard == ntscIn)?kNTSCCompressedBufferSize:kPALCompressedBufferSize;
        err = QTInsertChild( container, configAtom, kIDHDataSizeAtomType,
                        0, 0, sizeof( size), &size, nil);
        FailWithVal( err != noErr, Exit, err);

        size = (standard == ntscIn)?kNTSCCompressedBufferSize:kPALCompressedBufferSize;
        err = QTInsertChild( container, configAtom, kIDHDataBufferSizeAtomType,
                        0, 0, sizeof( size), &size, nil);
        FailWithVal( err != noErr, Exit, err);

        interval = 29.97;
        err = QTInsertChild( container, configAtom, kIDHDataIntervalAtomType,
                        0, 0, sizeof( interval), &interval, nil);
        FailWithVal( err != noErr, Exit, err);

        direction = kIDHDataTypeIsInputAndOutput;
        err = QTInsertChild( container, configAtom, kIDHDataIODirectionAtomType,
                        0, 0, sizeof( direction), &direction, nil);
        FailWithVal( err != noErr, Exit, err);

        size = 4;
        err = QTInsertChild( container, configAtom, kIDHSoundChannelCountAtomType,
                        0, 0, sizeof( size), &size, nil);
        FailWithVal( err != noErr, Exit, err);

        size = 2;
        err = QTInsertChild( container, configAtom, kIDHSoundSampleSizeAtomType,
                        0, 0, sizeof( size), &size, nil);
        FailWithVal( err != noErr, Exit, err);

        rate = 32000 << 16;
        err = QTInsertChild( container, configAtom, kIDHSoundSampleRateAtomType,
                        0, 0, sizeof( rate), &rate, nil);
        FailWithVal( err != noErr, Exit, err);

Exit:
        return err;
} // sound 2 config

static OSStatus setup44kAudioAtoms( QTAtomContainer container, QTAtom isocAtom, UInt32 standard)
{
        OSStatus err;
        QTAtom configAtom;

        StringPtr 	name;
        OSType 		type;
        long 		size;
        Fixed 		rate;
        float 		interval;
        long 		direction;

        // create a vide NTSC mode
        err = QTInsertChild( container, isocAtom, kIDHIsochModeAtomType,
                        0, 0, 0, nil, &configAtom);
        FailWithVal( err != noErr, Exit, err);

        type = kIDHSoundMediaAtomType;
        err = QTInsertChild( container, configAtom, kIDHIsochMediaType,
                        0, 0, sizeof( type), &type, nil);
        FailWithVal( err != noErr, Exit, err);

        name = "\pDV-44khz";
        err = QTInsertChild( container, configAtom, kIDHNameAtomType,
                        0, 0, name[0]+1, name, nil);
        FailWithVal( err != noErr, Exit, err);

        type = 'DV44';
        err = QTInsertChild( container, configAtom, kIDHDataTypeAtomType,
                        0, 0, sizeof( type), &type, nil);
        FailWithVal( err != noErr, Exit, err);

        size = (standard == ntscIn)?kNTSCCompressedBufferSize:kPALCompressedBufferSize;
        err = QTInsertChild( container, configAtom, kIDHDataSizeAtomType,
                        0, 0, sizeof( size), &size, nil);
        FailWithVal( err != noErr, Exit, err);

        size = (standard == ntscIn)?kNTSCCompressedBufferSize:kPALCompressedBufferSize;
        err = QTInsertChild( container, configAtom, kIDHDataBufferSizeAtomType,
                        0, 0, sizeof( size), &size, nil);
        FailWithVal( err != noErr, Exit, err);

        interval = 29.97;
        err = QTInsertChild( container, configAtom, kIDHDataIntervalAtomType,
                        0, 0, sizeof( interval), &interval, nil);
        FailWithVal( err != noErr, Exit, err);

        direction = kIDHDataTypeIsInputAndOutput;
        err = QTInsertChild( container, configAtom, kIDHDataIODirectionAtomType,
                        0, 0, sizeof( direction), &direction, nil);
        FailWithVal( err != noErr, Exit, err);

        size = 4;
        err = QTInsertChild( container, configAtom, kIDHSoundChannelCountAtomType,
                        0, 0, sizeof( size), &size, nil);
        FailWithVal( err != noErr, Exit, err);

        size = 2;
        err = QTInsertChild( container, configAtom, kIDHSoundSampleSizeAtomType,
                        0, 0, sizeof( size), &size, nil);
        FailWithVal( err != noErr, Exit, err);

        rate = 44100 << 16;
        err = QTInsertChild( container, configAtom, kIDHSoundSampleRateAtomType,
                        0, 0, sizeof( rate), &rate, nil);
        FailWithVal( err != noErr, Exit, err);

Exit:
        return err;
} // sound 3 config

static OSStatus cameraNameLookup(DeviceDescriptionPtr pDeviceDescription, UInt8 *name)
{
    OSStatus	result	= noErr;
    int 		len 	= strlen(pDeviceDescription->fDevice->fName);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: cameraNameLookup begin\n");
#endif
	
	if(len) 
	{
        if(len>255)
            len = 255;
        name[0] = len;
        BlockMoveData(pDeviceDescription->fDevice->fName, name+1, len);
    }
    else
	{
        // Look up vendor ID in resource list
		UInt32		vendorID	= pDeviceDescription->fDevice->fGUID >> 40;
		CFStringRef cameraName	= CFStringCreateWithCString(kCFAllocatorDefault, "DV", CFStringGetSystemEncoding());
        SInt16		refNum		= -1, localizedRefNum = -1;
		Boolean		stringFound	= false;

		CFBundleRef myRef = 0;

		// read vendor id resource and look for matching guid
		if(pDeviceDescription->fGlobals->fBundleID == 0) 
		{
			myRef = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.IOFWDVComponents"));
			CFBundleOpenBundleResourceFiles(myRef, &refNum, &localizedRefNum);
			pDeviceDescription->fGlobals->fBundleID = myRef;
			pDeviceDescription->fGlobals->fBundleResRef = localizedRefNum;
		}
		else
		{
			myRef = pDeviceDescription->fGlobals->fBundleID;
		}
		
		if( myRef )
		{
			// form the key
			CFStringRef keyRef = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("VENDOR_%08X"), vendorID);
			
			if( keyRef )
			{
				// lookup the value
				CFStringRef localizedString = CFBundleCopyLocalizedString ( myRef,  keyRef, cameraName, NULL );

				if ( localizedString )
				{
					CFMutableArrayRef names = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

					if (localizedString && CFStringGetLength(localizedString) > 0) 
					{
						CFArrayAppendValue(names, (const void *)localizedString);
					}
					
					if (cameraName && CFStringGetLength(cameraName) > 0) 
					{
						CFArrayAppendValue(names, (const void *)cameraName);
					}
					
					CFStringRef vendorAndCameraName = CFStringCreateByCombiningStrings(kCFAllocatorDefault, names, CFSTR(" "));
					
					if ( vendorAndCameraName )
					{
						stringFound = CFStringGetPascalString ( vendorAndCameraName, name, sizeof(name), CFStringGetSystemEncoding());
						CFRelease( vendorAndCameraName );
					}
					
					CFRelease(names); 
					CFRelease( localizedString );
				}
				
				CFRelease( keyRef );
			}
		}

		if(stringFound == false)
		{
			CFStringGetPascalString ( CFSTR("DV"), name, sizeof(name), CFStringGetSystemEncoding());
		}

		CFRelease(cameraName);
	}

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: cameraNameLookup end\n");
#endif
	
	return result;
}

static OSStatus postEvent(IsochComponentGlobalsPtr g, IDHDeviceID deviceID, IDHEvent event)
{
        UInt32 i;
        IDHDeviceConnectionEvent connectionEvent;
        OSStatus error = noErr;

        // We now have two broad classifications of events - ones that need to be
        // reported ASAP, which are stream related:
        //
        // 		kIDHPrivateEventReadComplete
        //		kIDHPrivateEventWriteComplete
        //
        // and ones that are device management related, whose notifications will
        // probably generate massive amounts of task-level only Toolbox calls:
        //
        //		kIDHEventDeviceAdded
        //		kIDHEventDeviceRemoved
        //		kIDHEventReadEnabled
        //		kIDHEventReadDisabled
        //		kIDHEventWriteEnabled
        //		kIDHEventWriteDisabled
        //
        // kIDHPrivateEventReadComplete and kIDHPrivateEventWriteComplete are posted to a secondary
        // interrupt handler. All other events are handled immediately.


        RecordEventLogger( 'isoc', 'post', deviceID, event);

        for (i = 0; i < kMaxNotifications ; ++i)
        {
                ClientNotification* clientNotification = &g->clientNotification[i];
                IDHDeviceID clientDeviceID = clientNotification->deviceID;
                IDHEvent wantedEvents = clientNotification->events;
                if (event & wantedEvents)
                {
                        // Send notification if the device matches the client's desired device
                        // or client specified all devices.
                        if ((kIDHDeviceIDEveryDevice == clientDeviceID) ||
                                (deviceID == clientDeviceID) )
                        {
                                // we currently only support a one-shot notification, like clock callbacks
                                clientNotification->events = 0;

                                switch(event)
                                {
                                        case kIDHEventDeviceChanged:
                                        case kIDHEventDeviceAdded:
                                        case kIDHEventDeviceRemoved:
                                        case kIDHEventReadEnabled:
                                        case kIDHEventReadDisabled:
                                        case kIDHEventWriteEnabled:
                                        case kIDHEventWriteDisabled:

                                                connectionEvent.eventHeader.event = event;
                                                connectionEvent.eventHeader.deviceID = deviceID;
                                                connectionEvent.eventHeader.notificationID =
                                                                (UInt32)clientNotification;

                                                (*clientNotification->notificationProc)
                                                                ((IDHGenericEvent*)&connectionEvent,
                                                                clientNotification->userData);
                                                break;

                                        default:
                                                RecordEventLogger( 'isoc', 'post', '????', event);
                                                break;
                                }

                        }
                }
        }

        RecordEventLogger( 'isoc', 'post', 'end ', event);
        return error;

}

static void deviceMessage(void * refcon, UInt32 messageType, void *messageArgument)
{

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: deviceMessage begin\n");
#endif
	
	// refcon is 1-based device index.
    DeviceDescriptionPtr deviceDescriptionPtr = &globals.deviceDescription[(int)refcon-1];
    //syslog(LOG_INFO,"Got message: refcon %d, type 0x%x arg %p\n",
    //    refcon, messageType, messageArgument);
    if(messageType == kIOFWMessageServiceIsRequestingClose) {
        //syslog(LOG_INFO,"Device # %d, ptr %p requesting close\n",
        //	refcon-1, deviceDescriptionPtr);
        deviceDescriptionPtr->fActive = false;
        if(deviceDescriptionPtr->deviceControlInstance) {
            DeviceControlDisableAVCTransactions(deviceDescriptionPtr->deviceControlInstance);
            DeviceControlSetDeviceConnectionID(deviceDescriptionPtr->deviceControlInstance,
                                                                                kIDHInvalidDeviceID);
        }
        if(deviceDescriptionPtr->fDevice) {

			if(deviceDescriptionPtr->fRead != NULL)
				disableRead(deviceDescriptionPtr);
			if(deviceDescriptionPtr->fWrite != NULL)
				disableWrite(deviceDescriptionPtr);
			
            DVDeviceTerminate(deviceDescriptionPtr->fDevice);
            deviceDescriptionPtr->fDevice = NULL;
        }
        // post a DV event to let the curious know...
        postEvent(deviceDescriptionPtr->fGlobals, (IDHDeviceID)deviceDescriptionPtr, kIDHEventDeviceRemoved);
    }

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: deviceMessage end\n");
#endif
}


static OSStatus enableRead(DeviceDescription *deviceDescriptionPtr)
{
    OSStatus		result;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: enableRead begin\n");
#endif
	
    deviceDescriptionPtr->fNumOutputFrames = 4;
    deviceDescriptionPtr->fRead = DVAllocRead(deviceDescriptionPtr->fDevice, globals.fDVThread);

	// Get the current device's output-signal mode, and set the signal mode approprietely
	{
		OSStatus err;
		DVCTransactionParams transaction;
		UInt8 out[8];
		UInt8 in[8];

		out[0] = kAVCStatusInquiryCommand;
		out[1] = kAVCUnitAddress;
		out[2] = kAVCOutputPlugSignalFormatOpcode;
		out[3] = 0;	// Plug
		out[4] = out[5] = out[6] = out[7] = 0xff;
		transaction.commandBufferPtr = out;
		transaction.commandLength = sizeof(out);
		transaction.responseBufferPtr = in;
		transaction.responseBufferSize = sizeof(in);
		transaction.responseHandler = NULL;

		err = doAVCTransaction(deviceDescriptionPtr, &transaction);
		if(err == noErr && in[0] == 0xc)
		{
			// Set the mode based on the response from this command
			result = DVReadSetSignalMode(deviceDescriptionPtr->fRead, in[5]);
		}
	}
	
	result = DVReadAllocFrames(deviceDescriptionPtr->fRead, deviceDescriptionPtr->fNumOutputFrames, 
        &deviceDescriptionPtr->fReadSharedVars, deviceDescriptionPtr->bufMem);
    if(result == noErr)
        result = DVReadStart(deviceDescriptionPtr->fRead);
    if(result == noErr) {
        deviceDescriptionPtr->fOldDrop = deviceDescriptionPtr->fReadSharedVars->fDroppedFrames;
        DVSetTimeoutTime(deviceDescriptionPtr->fGlobals->fDVThread, CFAbsoluteTimeGetCurrent() + kDCLBlockTime);
    }

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: enableRead end\n");
#endif
	
	return result;
}

static OSStatus disableRead(DeviceDescription *deviceDescriptionPtr)
{

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: disableRead begin\n");
#endif
	
    if(deviceDescriptionPtr->fRead) {
        DVReadStop(deviceDescriptionPtr->fRead);
        DVReadFreeFrames(deviceDescriptionPtr->fRead);
        DVReadFree(deviceDescriptionPtr->fRead);
        deviceDescriptionPtr->fRead = NULL;
    }

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: disableRead end\n");
#endif
	
	return noErr;
}

static OSStatus enableWrite(DeviceDescription *deviceDescriptionPtr)
{
    OSStatus		result;
    int i;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: enableWrite begin\n");
#endif
	
    deviceDescriptionPtr->fNumOutputFrames = 4;
    deviceDescriptionPtr->fWrite = DVAllocWrite(deviceDescriptionPtr->fDevice,
                                                        deviceDescriptionPtr->fGlobals->fDVThread);
    result = DVWriteSetSignalMode(deviceDescriptionPtr->fWrite, deviceDescriptionPtr->fOutputMode);
    result = DVWriteAllocFrames(deviceDescriptionPtr->fWrite, deviceDescriptionPtr->fNumOutputFrames, 
            &deviceDescriptionPtr->fWriteSharedVars, deviceDescriptionPtr->bufMem);
    deviceDescriptionPtr->fDCLBuffer = DVWriteGetDCLBuffer(deviceDescriptionPtr->fWrite,
                                                            &deviceDescriptionPtr->fSharedWriteVars);
    deviceDescriptionPtr->fBufSize = (deviceDescriptionPtr->fDevice->standard == ntscIn)?
                                    kNTSCCompressedBufferSize:kPALCompressedBufferSize;
    if((deviceDescriptionPtr->fOutputMode & 0x7c) == 4)
        deviceDescriptionPtr->fBufSize /= 2;	// SDL
	else if((deviceDescriptionPtr->fOutputMode & 0x7c) == 0x74)
        deviceDescriptionPtr->fBufSize *= 2;	// DVCPro50

    deviceDescriptionPtr->fOldWriteTimeStamps =
        (UInt32 *)NewPtr(deviceDescriptionPtr->fSharedWriteVars->fNumGroups*sizeof(UInt32));
    for(i=0; i<deviceDescriptionPtr->fSharedWriteVars->fNumGroups; i++) {
        deviceDescriptionPtr->fOldWriteTimeStamps[i] = deviceDescriptionPtr->fSharedWriteVars->fTimeStampPtrs[i];
    }
    deviceDescriptionPtr->fDCLReadPos = 0;
    deviceDescriptionPtr->fBufWritePos = 0;
    deviceDescriptionPtr->fFrameBufferPos = 0;
    deviceDescriptionPtr->fDCLSavedWritePos = NULL;
    deviceDescriptionPtr->fDCLSavedPacketNum = 0;
            
    deviceDescriptionPtr->fWaitingStartWrite = 1;
    deviceDescriptionPtr->fWriteSharedVars->fReader = deviceDescriptionPtr->fWriteSharedVars->fWriter = 0;
    deviceDescriptionPtr->fOldDrop = deviceDescriptionPtr->fWriteSharedVars->fDroppedFrames;

    DVSetTimeoutTime(deviceDescriptionPtr->fGlobals->fDVThread, CFAbsoluteTimeGetCurrent() + kDCLBlockTime);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: enableWrite end\n");
#endif
	
    return result;
}

static void disableWrite(DeviceDescription *deviceDescriptionPtr)
{
#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: disableWrite begin\n");
#endif
	
	if (deviceDescriptionPtr->fWrite)
	{
		DVWriteStop(deviceDescriptionPtr->fWrite);
		DVWriteFreeFrames(deviceDescriptionPtr->fWrite);
		DVWriteFree(deviceDescriptionPtr->fWrite);
		deviceDescriptionPtr->fWrite = NULL;
		if(deviceDescriptionPtr->fOldWriteTimeStamps)
		{
			DisposePtr((Ptr)deviceDescriptionPtr->fOldWriteTimeStamps);
			deviceDescriptionPtr->fOldWriteTimeStamps = NULL;
		}
	}
	else
	{
		syslog(LOG_INFO, "IDH: disableWrite called with deviceDescriptionPtr->fWrite == NULL\n");
	}

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: disableWrite end\n");
#endif
	
}

static OSStatus sendMsg(IsochComponentInstancePtr ih, RequestFunc request, void *params)
{
    return DVRequest(globals.fDVThread, request, ih, (UInt32)params);
}

static OSStatus doAVCTransaction(DeviceDescriptionPtr deviceDescriptionPtr,
                DVCTransactionParams* inTransaction)
{
    IOReturn result = kIDHErrDeviceNotConfigured;
    if(deviceDescriptionPtr->fConnected) {
        if(!deviceDescriptionPtr->fActive)
            return kIDHErrDeviceDisconnected;
        if(deviceDescriptionPtr->fNoAVC > 1)
            return kIOReturnTimeout;
        else {
            result = (*deviceDescriptionPtr->fDevice->fAVCInterface)->AVCCommand(deviceDescriptionPtr->fDevice->fAVCInterface,
                inTransaction->commandBufferPtr, inTransaction->commandLength,
                inTransaction->responseBufferPtr, &inTransaction->responseBufferSize);
                
            if(result == kIOReturnTimeout)
                deviceDescriptionPtr->fNoAVC++;
            else
                deviceDescriptionPtr->fNoAVC = 0;
        }
    }
    return result;
}


static void deviceArrived(void *refcon, DVDevice *device, UInt32 index, UInt32 refound)
{
    IsochComponentGlobalsPtr g = (IsochComponentGlobalsPtr)refcon;
    OSStatus 			result = noErr;
    QTAtom 			deviceAtom, isocAtom;
    DeviceDescriptionPtr	deviceDescriptionPtr;
    IDHDeviceStatus		deviceStatus;
    ComponentDescription	clkDesc;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: deviceArrived begin\n");
#endif
	
    RecordEventLogger( 'isoc', 'addv', (int)g, index);
    //syslog(LOG_INFO,"deviceArrived: device 0x%x\n", index);

    ++g->seed;
    
    // device index is 1-based.
    deviceDescriptionPtr = &g->deviceDescription[index-1];

    do {
        // look for existing device
        if( refound) {
            // Get device back to old state, set fActive to true after everything is restored, not before!
            deviceDescriptionPtr->fDevice = device;
            if(deviceDescriptionPtr->fConnected) {
                IOReturn result;
                result = DVDeviceOpen(g->fDVThread, deviceDescriptionPtr->fDevice);
                if(result == kIOReturnSuccess) {
                    if(deviceDescriptionPtr->readLocks) {
                        enableRead(deviceDescriptionPtr);
                    }
                    if(deviceDescriptionPtr->writeLocks) {
                        enableWrite(deviceDescriptionPtr);
                    }
                }
            }
            deviceDescriptionPtr->fActive = true;
        }
        else {
            Str255 cameraName;
    
            bzero(deviceDescriptionPtr, sizeof(*deviceDescriptionPtr));
            deviceDescriptionPtr->fActive = true;
            deviceDescriptionPtr->fGlobals = g;
            deviceDescriptionPtr->fDevice = device;
#ifdef DRAWRINGBUFFERGRAPH
            deviceDescriptionPtr->fScreenBaseAddr = 0;
#endif
            RecordEventLogger( 'isoc', 'updt', 'add ', 'reg ');
    
            // add its description
            g->nDevices++;
    
            //*deviceID = ih->nDevices;
    
            
            // find clock component
            // wouldn't it be better for us to open an instance on OpenDevice, set FWClockPrivLocalReference, etc.
            clkDesc.componentType 			= clockComponentType;
            clkDesc.componentSubType 		= 'fwcy';
            clkDesc.componentManufacturer 		= 'appl';
            clkDesc.componentFlags 			= 0L;
            clkDesc.componentFlagsMask 		= 0L;
    
            deviceDescriptionPtr->clock = FindNextComponent( deviceDescriptionPtr->clock, &clkDesc);	// Look for FireWire clock component
            FailMessage( deviceDescriptionPtr->clock == nil);
            // create device description atom structure
            result = QTNewAtomContainer( &deviceDescriptionPtr->deviceContainer);
            if( result != noErr)break;
    
            // add a device atom
            result = QTInsertChild( deviceDescriptionPtr->deviceContainer, kParentAtomIsContainer, kIDHDeviceAtomType,
                                    (long) deviceDescriptionPtr, 0, 0, nil, &deviceAtom);
            if( result != noErr)break;
    
            // add the unique 64 bit FireWire GUID id to device atom
            result = QTInsertChild( deviceDescriptionPtr->deviceContainer, deviceAtom, kIDHUniqueIDType,
                                    0, 0, sizeof(device->fGUID), &device->fGUID, nil);
            if( result != noErr)break;
    
            result = cameraNameLookup(deviceDescriptionPtr, cameraName);
            if( result != noErr)break;
            result = QTInsertChild( deviceDescriptionPtr->deviceContainer, deviceAtom, kIDHNameAtomType,
                                    0, 0, cameraName[0] + 1, cameraName, nil);
    
            // add the IDH unique id to device atom
            result = QTInsertChild( deviceDescriptionPtr->deviceContainer, deviceAtom, kIDHDeviceIDType,
                                    0, 0, sizeof(deviceDescriptionPtr), &deviceDescriptionPtr, nil);
            if( result != noErr)break;
    
            // create a device status structure and add it to the device atom
            deviceStatus.version 		= 0x200;
            deviceStatus.physicallyConnected 	= true;
            deviceStatus.readEnabled 		= false;
            deviceStatus.writeEnabled 		= false;
            deviceStatus.exclusiveAccess 	= false;
            deviceStatus.currentBandwidth 	= 0;
            deviceStatus.currentChannel 	= 0;
            deviceStatus.inputStandard		= deviceDescriptionPtr->fDevice->standard;
            deviceStatus.inputFormat		= kIDHDV_SD;
    
            deviceStatus.outputFormats 		= deviceDescriptionPtr->fDevice->fDVFormats;
            deviceStatus.deviceActive 		= false;
    
            result = QTInsertChild( deviceDescriptionPtr->deviceContainer, deviceAtom, kDVDeviceInfo,
                                    0, 0, sizeof( IDHDeviceStatus), &deviceStatus, nil);
            if( result != noErr)break;
    
            // add isoch descriptions to structure
            result = QTInsertChild( deviceDescriptionPtr->deviceContainer, deviceAtom, kIDHIsochServiceAtomType,
                                    0, 0, 0, nil, &isocAtom);
            if( result != noErr)break;
    
            // add the configs to the isoc atom
            result = setupVideoAtoms( deviceDescriptionPtr->deviceContainer, isocAtom,
                deviceDescriptionPtr->fDevice->standard, false,
                deviceDescriptionPtr->fDevice->fDVFormats & (1 << kIDHDVCPro_25));
            if( result != noErr)break;
            if(deviceDescriptionPtr->fDevice->fDVFormats & (1 << kIDHDV_SDL)) {
                result = setupVideoAtoms( deviceDescriptionPtr->deviceContainer, isocAtom,
                 deviceDescriptionPtr->fDevice->standard, true,
                 deviceDescriptionPtr->fDevice->fDVFormats & (1 << kIDHDVCPro_25));
                if( result != noErr)break;
            }
            
            if( deviceDescriptionPtr->fDevice->standard == ntscIn)
            {
                    result = setup48kAudioAtoms( deviceDescriptionPtr->deviceContainer, isocAtom,
                     ntscIn);
                    if( result != noErr)break;
    
                    result = setup32kAudioAtoms( deviceDescriptionPtr->deviceContainer, isocAtom,
                     ntscIn);
                    if( result != noErr)break;
            }
            else	// PAL audio
            {
                    result = setup44kAudioAtoms( deviceDescriptionPtr->deviceContainer, isocAtom,
                     deviceDescriptionPtr->fDevice->standard);
                    if( result != noErr)break;
            }

			// Set output signal mode, default to standard DV
			deviceDescriptionPtr->fOutputMode = 0;
			if(deviceDescriptionPtr->fDevice->standard != ntscIn)
				deviceDescriptionPtr->fOutputMode |= 0x80;
		}
    } while(false);

    postEvent(g, (IDHDeviceID)deviceDescriptionPtr, kIDHEventDeviceAdded);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: deviceArrived end\n");
#endif
	
}

static UInt8 *getNextFrame(DeviceDescription *deviceDescriptionPtr, IsochComponentInstancePtr client, int slack, int waiting)
{
    ComponentResult err = noErr;
    IDHParameterBlock *pb;
    
    pb = client->fActiveHead;
    if(pb && !waiting) {
        if(deviceDescriptionPtr->fWaitingStartWrite) {
            IOReturn res;
            res = DVWriteStart(deviceDescriptionPtr->fWrite);
            //if(res == kIOReturnNoResources || res == kIOReturnNoSpace || res == kIOFireWireBusReset) {
            if(res != kIOReturnSuccess) {
                //syslog(LOG_INFO, "start write returned %x\n", res);
                err = kIDHErrDeviceCantWrite;
                deviceDescriptionPtr->fWaitingStartWrite = 2;
            }
            else
                deviceDescriptionPtr->fWaitingStartWrite = 0;
        }

        client->fActiveHead = (IDHParameterBlock *)pb->reserved1;
        if(client->fActiveHead == NULL)
            client->fActiveTail = NULL;
        pb->actualCount = deviceDescriptionPtr->fBufSize;
        pb->result = err;

        //syslog(LOG_INFO, "write callback, buffer = %p\n", pb->buffer);
        //syslog(LOG_INFO, "pb %p = req %d actual %d\n",
        //    pb, pb->requestedCount, pb->actualCount);
        if(pb->completionProc) {
            err = pb->completionProc((IDHGenericEvent *)pb, pb->refCon);
            if(err != noErr) {
                syslog(LOG_INFO, "write callback for pb %p returned error %d\n", pb, err);
            }
        }
    }
    if(client->fActiveHead) {
        pb = client->fActiveHead;
        // We have to copy the buffer because QuickTime only uses one buffer to send data to us,
        // so if they drop a frame we don't have anything to send (because they are overwriting the
        // buffer with new data)
        bcopy(pb->buffer, (UInt8 *)deviceDescriptionPtr->bufMem[0], deviceDescriptionPtr->fBufSize);
    }
    else {
        //syslog(LOG_INFO, "Waiting for next IDHWrite, %d DCL blocks free\n", slack);
        if(slack < 3) {
            // No new frame to write, re-use the last one, after muting the audio
            deviceDescriptionPtr->fWriteSharedVars->fDroppedFrames++;
            dropMsg(deviceDescriptionPtr, deviceDescriptionPtr->fWriteSharedVars->fDroppedFrames);
            DVSilenceFrame(deviceDescriptionPtr->fOutputMode, (UInt8 *)deviceDescriptionPtr->bufMem[0]);
        }
        else {
            return NULL;	// OK to wait for next IDHWrite
        }
    }
    deviceDescriptionPtr->fFrameBufferPos = 0;
    return (UInt8 *)deviceDescriptionPtr->bufMem[0];
}

static int fillDCLGroup(DeviceDescription *deviceDescriptionPtr, IsochComponentInstancePtr client, int group, int slack)
{
    UInt8 *dclPtr;
    UInt8 *dataPtr;
    UInt32 dataSize = deviceDescriptionPtr->fSharedWriteVars->fPacketDataSize;
    UInt32 packetSize = deviceDescriptionPtr->fSharedWriteVars->fAlignedPacketSize;
    int i;
    int start;
    // If we're waiting for a new frame, restore the current DCL pointer
	if(deviceDescriptionPtr->fDCLSavedWritePos != NULL) {
        dclPtr = deviceDescriptionPtr->fDCLSavedWritePos;
        dataPtr = getNextFrame(deviceDescriptionPtr, client, slack, true);
        //syslog(LOG_INFO, "Waiting for next frame, new frame = 0x%x\n", dataPtr);
        if(dataPtr == NULL)
            return 0;	// Waiting for next frame.
        deviceDescriptionPtr->fDCLSavedWritePos = NULL;	// Got a frame, carry on
        start = deviceDescriptionPtr->fDCLSavedPacketNum;
        deviceDescriptionPtr->fDCLSavedPacketNum = 0;
    }
    else {
        dclPtr = deviceDescriptionPtr->fDCLBuffer + deviceDescriptionPtr->fSharedWriteVars->fDataOffset[group];
        start = 0;
    }
    
    if(deviceDescriptionPtr->fWaitingStartWrite) {
        if(!client->fActiveHead) {
            return 0;	// No data, not running yet.
        }
        else {
            bcopy(client->fActiveHead->buffer, (UInt8 *)deviceDescriptionPtr->bufMem[0],
                                        deviceDescriptionPtr->fBufSize);
        }
    }
    dataPtr = (UInt8 *)deviceDescriptionPtr->bufMem[0];
        
    dataPtr += deviceDescriptionPtr->fFrameBufferPos;
    
    for(i=start; i<deviceDescriptionPtr->fSharedWriteVars->fGroupSize; i++) {
        int pageOffset;
        // check for buffer crossing page
        pageOffset = (int) (dclPtr + packetSize) & 0x0fff;
        if (pageOffset < packetSize && pageOffset > 0) {
            // if it does, increment buffer pointer
            // and lop off page rollover to start at next page
            dclPtr += packetSize;
            dclPtr = (UInt8 *)((int)dclPtr & 0xfffff000);
        }
        bcopy(dataPtr, dclPtr+8, dataSize);
        dataPtr += dataSize;
        deviceDescriptionPtr->fFrameBufferPos += dataSize;
        dclPtr += packetSize;
        
        if(deviceDescriptionPtr->fFrameBufferPos >= deviceDescriptionPtr->fBufSize) {
            dataPtr = getNextFrame(deviceDescriptionPtr, client, slack, false);
            if(dataPtr == NULL) {
                deviceDescriptionPtr->fDCLSavedWritePos = dclPtr;
                deviceDescriptionPtr->fDCLSavedPacketNum = i+1;
                return 0;	// Waiting for next frame.
            }
            deviceDescriptionPtr->fFrameBufferPos = 0;
            deviceDescriptionPtr->fDCLSavedWritePos = NULL;
            deviceDescriptionPtr->fDCLSavedPacketNum = 0;
        }
    }
    return 1;
}

static CFAbsoluteTime processWrites(DeviceDescription *deviceDescriptionPtr)
{
    int i;
    int changed;
    int done;
    int emptyBlocks, ahead;
    int numGroups = deviceDescriptionPtr->fSharedWriteVars->fNumGroups;
    IsochComponentInstancePtr client;
    int pos;
    changed = 0;
    
    client = deviceDescriptionPtr->fOpenClients[0];

    // If we failed to start writing last time through, reset to try again
    if(deviceDescriptionPtr->fWaitingStartWrite == 2) {
        for(i=0; i<deviceDescriptionPtr->fSharedWriteVars->fNumGroups; i++) {
            deviceDescriptionPtr->fOldWriteTimeStamps[i] = deviceDescriptionPtr->fSharedWriteVars->fTimeStampPtrs[i];
        }
        //deviceDescriptionPtr->fDCLReadPos = 0;
        deviceDescriptionPtr->fBufWritePos = 0;
        deviceDescriptionPtr->fFrameBufferPos = 0;
        deviceDescriptionPtr->fDCLSavedWritePos = NULL;
        deviceDescriptionPtr->fDCLSavedPacketNum = 0;
        deviceDescriptionPtr->fWaitingStartWrite = 1;
    }
    // First find out where the hardware is
    pos = deviceDescriptionPtr->fSharedWriteVars->fDMAPos;
    ahead = deviceDescriptionPtr->fBufWritePos - pos;
    if(ahead < 0)
        ahead += numGroups;
    emptyBlocks = 4-ahead;
    //printf("pos %d wripos %d ahead %d\n", pos, deviceDescriptionPtr->fBufWritePos, ahead);

    if(emptyBlocks>3)
        emptyBlocks = 3;
        
    done = 0;
    for(i=0; i<emptyBlocks; i++) {
        int ok;
        ok = 
            fillDCLGroup(deviceDescriptionPtr, client, (deviceDescriptionPtr->fBufWritePos + i) % numGroups,
                            ahead+done);
        if(!ok)
            break;
        done += ok;
    }
    deviceDescriptionPtr->fBufWritePos += done;
    deviceDescriptionPtr->fBufWritePos %= numGroups;
    return CFAbsoluteTimeGetCurrent() + kDCLBlockTime;
}

static CFAbsoluteTime processReads(DeviceDescription *deviceDescriptionPtr)
{
    int doneFrames = 0;
    int frameIndex;
    IsochComponentInstancePtr client;
    int i;
    CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
    UInt32 cycleTimeNow;
    SInt32 frameDiff;
    
    (*deviceDescriptionPtr->fDevice->fDevInterface)->
        GetCycleTime(deviceDescriptionPtr->fDevice->fDevInterface, &cycleTimeNow);

    frameIndex = deviceDescriptionPtr->fReadSharedVars->fReader % deviceDescriptionPtr->fNumOutputFrames;
    if(deviceDescriptionPtr->fReadSharedVars->fReader < 
                            deviceDescriptionPtr->fReadSharedVars->fWriter) {
        UInt8 *buffer = deviceDescriptionPtr->bufMem[frameIndex];
        UInt32 frameSize = deviceDescriptionPtr->fReadSharedVars->fFrameSize[frameIndex];
        UInt32 frameTime = cycles(deviceDescriptionPtr->fReadSharedVars->fFrameTime[frameIndex]);

        // Drop out if too soon
        //printf("Cycle now %x frame %x\n", cycles(cycleTimeNow), frameTime);
        frameDiff = cycles(cycleTimeNow) - frameTime;
        if(frameDiff < 0)
            frameDiff += 8000;
            
        frameDiff = 200 - frameDiff;
        if(frameDiff > 0) {
            //printf("Delaying for %d cycles\n", frameDiff);
            return now + frameDiff/8000.0;
        }

         // Lock buffer while we process stuff
        deviceDescriptionPtr->fBufferLocks[frameIndex]++;
        if(deviceDescriptionPtr->fOldDrop < deviceDescriptionPtr->fReadSharedVars->fDroppedFrames) {
            dropMsg(deviceDescriptionPtr, deviceDescriptionPtr->fReadSharedVars->fDroppedFrames);
        }
        for(i=0; i<kMaxDeviceClients; i++) {
            client = deviceDescriptionPtr->fOpenClients[i];
            if(client) {
                if(client->fClientIndex != i) {
                    syslog(LOG_INFO, "processReads: client %p of %p index is %d not %d\n",
                        client, deviceDescriptionPtr, client->fClientIndex, i);
                }
                if(deviceDescriptionPtr->fReadStage == 1) {
                    syslog(LOG_INFO, "client %d open, head/tail is %p/%p\n", i, client->fHead, client->fTail);
                }
                if(client->fHead) {
                    IDHParameterBlock *pb = client->fHead;
                    OSStatus err;
                    
                    pb->actualCount = frameSize;

                    if(pb->buffer != nil) {
                        // copy frame
                        bcopy(buffer, pb->buffer, frameSize);
                    }
                    else {
                        deviceDescriptionPtr->fBufferLocks[frameIndex]++;
                        pb->buffer = buffer;
                    }
                    client->fHead = (IDHParameterBlock *)pb->reserved1;
                    pb->reserved1 = frameTime;
                    if(client->fHead == NULL)
                        client->fTail = NULL;
                    pb->result = noErr;
                    
                    //syslog(LOG_INFO, "read callback, buffer = %p\n", pb->buffer);
                    //syslog(LOG_INFO, "pb %p = req %d actual %d\n",
                    //    pb, pb->requestedCount, pb->actualCount);
                    {
#ifdef TIMING
                        CFAbsoluteTime cstart, cend;
                        cstart = CFAbsoluteTimeGetCurrent();
#endif
                        err = pb->completionProc((IDHGenericEvent *)pb, pb->refCon);
#ifdef TIMING
                        cend = CFAbsoluteTimeGetCurrent();
                        if(cend-cstart > 0.05)
                            syslog(LOG_INFO, "read callback took %8.3f to %8.3f\n", cstart, cend);
#endif
                        if(err != noErr) {
                            syslog(LOG_INFO, "read callback for pb %p returned error %d\n", pb, err);
                        }
                    }
                    doneFrames++;
                }
            }
        }
        // unlock buffer
        deviceDescriptionPtr->fBufferLocks[frameIndex]--;
        if(doneFrames && deviceDescriptionPtr->fBufferLocks[frameIndex] == 0)
            deviceDescriptionPtr->fReadSharedVars->fReader += 1;   // release buffer
    }

    return now+kDCLBlockTime;
}

static ComponentResult queueWrite(IsochComponentInstancePtr client, void *params)
{
    IDHParameterBlock *pb = (IDHParameterBlock *)params;
    
    if(pb->buffer) {
        if(client->fActiveTail == NULL) {
            client->fActiveHead = pb;
        }
        else
            client->fActiveTail->reserved1 = (UInt32)pb;
        client->fActiveTail = pb; 
    }
    else {
        if(client->fTail == NULL)
            client->fHead = pb;
        else
            client->fTail->reserved1 = (UInt32)pb;
        client->fTail = pb;
    }
    pb->reserved1 = 0;
    return noErr;
}

static ComponentResult queueRead(IsochComponentInstancePtr client, void *params)
{
    IDHParameterBlock *pbNew = (IDHParameterBlock *)params;
    if(client->fTail == NULL)
        client->fHead = pbNew;
    else
        client->fTail->reserved1 = (UInt32)pbNew;
    client->fTail = pbNew;
    pbNew->reserved1 = 0;

    return noErr;
}

ComponentResult processOpen(IsochComponentInstancePtr ih, void *params)
{
    UInt32 permissions = (UInt32)params;
    ComponentResult result = noErr;
    int i;

    DeviceDescription *deviceDescriptionPtr;
    result = findDeviceDescriptionforDevice( ih, ih->deviceID, &deviceDescriptionPtr);	// find the device
    FailWithVal( result != noErr, Exit, result);
    
    FailWithAction( deviceDescriptionPtr->fActive == false, result = kIDHErrDeviceDisconnected, Exit);

    // trying to reopen for read or write?
    if( ih->permissions & kIDHOpenForReadTransactions && permissions & kIDHOpenForReadTransactions)
            goto Exit;	// don't do anything

    if( ih->permissions & kIDHOpenForWriteTransactions && permissions & kIDHOpenForWriteTransactions)
            goto Exit;	// don't do anything

    // check for switching directions before opening
    FailWithAction( ih->permissions & kIDHOpenForReadTransactions && permissions & kIDHOpenForWriteTransactions,
            result = kIDHErrDeviceInUse, Exit);

    FailWithAction( ih->permissions & kIDHOpenForWriteTransactions && permissions & kIDHOpenForReadTransactions,
            result = kIDHErrDeviceInUse, Exit);

    // if user is opening for read, make sure device isn't already opened for writes
    if( permissions & kIDHOpenForReadTransactions)
    {
        FailWithAction( deviceDescriptionPtr->writeLocks, result = kIDHErrDeviceInUse, Exit);

        if( deviceDescriptionPtr->readLocks == 0)
        {
            result = enableRead(deviceDescriptionPtr);
            FailWithVal( result != noErr, Exit, result);
        }

        ++deviceDescriptionPtr->readLocks;	// keep track of read count
        //syslog(LOG_INFO, "ih %p dev %p Opened for read, count %d\n",
        //    ih, deviceDescriptionPtr, deviceDescriptionPtr->readLocks);
        RecordEventLogger( 'open', ' Now', ih, deviceDescriptionPtr->readLocks);
    }

    // if user is opening for write, make sure device isn't already opened
    if( permissions & kIDHOpenForWriteTransactions)
    {
        FailWithAction( deviceDescriptionPtr->readLocks || deviceDescriptionPtr->writeLocks,
                        result = kIDHErrDeviceInUse, Exit);

        result = enableWrite(deviceDescriptionPtr);
        FailWithVal( result != noErr, Exit, result);

        deviceDescriptionPtr->writeLocks = 1;	// keep track of write count
    }

//        if( permissions & kIDHOpenWithExclusiveAccess)
//                deviceDescriptionPtr->exclusiveAccess = true;

    ih->permissions = permissions;	// save the permissions
    // Setup device->component pointer
    for(i=0; i<kMaxDeviceClients; i++) {
        if(!deviceDescriptionPtr->fOpenClients[i]) {
            deviceDescriptionPtr->fOpenClients[i] = ih;
            ih->fClientIndex = i;
            break;
        }
    }

Exit:
    return result;
}

static ComponentResult processClose(IsochComponentInstancePtr ih, void *junk)
{
    ComponentResult result = noErr;
    DeviceDescription *deviceDescriptionPtr;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: processClose begin\n");
#endif
	
    result = findDeviceDescriptionforDevice( ih, ih->deviceID, &deviceDescriptionPtr);	// find the device
    FailWithVal( result != noErr, Exit, result);
    if( ih->permissions & kIDHOpenForReadTransactions)	// tear down read
    {
        //syslog(LOG_INFO, "ih %p dev %p Closing from read, count %d\n",
        //    ih, deviceDescriptionPtr, deviceDescriptionPtr->readLocks);
        if( --deviceDescriptionPtr->readLocks <= 0)	// is device completely freed by clients?
        {
            if(deviceDescriptionPtr->fActive)
                disableRead(deviceDescriptionPtr);
            //    volatile int *fWriter,*fReader,*fDrop,*fStatus;

            //if(deviceDescriptionPtr->fSharedVars) {
                // printf("fWriter: %ld", deviceDescriptionPtr->fSharedVars->fWriter);
                //  printf(" fReader: %ld", deviceDescriptionPtr->fSharedVars->fReader);
                //  printf(" fDrop: %ld", deviceDescriptionPtr->fSharedVars->fDroppedFrames);
                //  printf(" fStatus: %ld", deviceDescriptionPtr->fSharedVars->fStatus);
                //  printf("\n");
            //}
            
            //deviceDescriptionPtr->exclusiveAccess = false;
        }
    }

    if( ih->permissions & kIDHOpenForWriteTransactions) // tear down write
    {
        if( --deviceDescriptionPtr->writeLocks <= 0)	// is device completely freed by clients?
        {
            if(deviceDescriptionPtr->fActive)
                disableWrite(deviceDescriptionPtr);
            //if(deviceDescriptionPtr->fSharedVars) {
            //      printf("fWriter: %ld", deviceDescriptionPtr->fSharedVars->fWriter);
            //     printf(" fReader: %ld", deviceDescriptionPtr->fSharedVars->fReader);
            //      printf(" fDrop: %ld", deviceDescriptionPtr->fSharedVars->fDroppedFrames);
            //       printf(" fStatus: %ld", deviceDescriptionPtr->fSharedVars->fStatus);
            //       printf("\n");
            //}
            
            //deviceDescriptionPtr->exclusiveAccess = false;
        }
    }

    deviceDescriptionPtr->fOpenClients[ih->fClientIndex] = nil;
Exit:

#ifdef kIDH_Verbose_Debug_Logging
syslog(LOG_INFO, "IDH: processClose end\n");
#endif

return result;

}

static ComponentResult processCancelPendingIO(IsochComponentInstancePtr client, void *params)
{
    while(client->fHead) {
        IDHParameterBlock *pb = client->fHead;
        //syslog(LOG_INFO, "Cancelling IO %p, buffer = %p\n", pb, pb->buffer);

        client->fHead = (IDHParameterBlock *)pb->reserved1;
        pb->reserved1 = NULL;
    }
    client->fTail = NULL;
    
    return noErr;
}

#if TIMING
/* raw read of the timebase register */
static void clock_get_uptime( register AbsoluteTime *result)
{
#ifdef __ppc__

        register UInt32  hic;
        do {
	  asm volatile("  mftbu %0" : "=r" (result->hi));
	  asm volatile("  mftb  %0" : "=r" (result->lo));
	  asm volatile("  mftbu %0" : "=r" (hic));
        } while (hic != result->hi);

#else
        result->lo = 0;
        result->hi = 0;
#endif /* __ppc__ */

}
#endif

static void timerTick(CFRunLoopTimerRef timer, void *info)
{
    IsochComponentGlobalsPtr g = (IsochComponentGlobalsPtr)info;
    int dev;
    CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
    CFAbsoluteTime fireDate = now + 100.0;
#ifdef TIMING
    if(now > g->fDVThread->requestTimeoutTime + .03) {
        pKGSysCall_Insert (kIOFWDVTrace, 'late', (UInt32)((now-g->fDVThread->requestTimeoutTime)*1000), 0, 0, 0, 0);
        syslog(LOG_INFO, "Late timer tick, @ %8.3f, wanted %8.3f\n", now, g->fDVThread->requestTimeoutTime);
        DVDumpLog(g->fDVThread);
    }
#endif
    for(dev = 0; dev<g->nDevices; dev++) {
        DeviceDescription *deviceDescriptionPtr;
        deviceDescriptionPtr = &g->deviceDescription[dev];

        // Process I/O queues
        if(deviceDescriptionPtr && deviceDescriptionPtr->fActive) {
#if TIMING
            clock_get_uptime(&timestamp1);
#endif
            if(deviceDescriptionPtr->fRead) {
                fireDate = CFAbsoluteTimeGetCurrent() + kDCLBlockTime;
            }
    
            if(deviceDescriptionPtr->writeLocks) {
                CFAbsoluteTime writeDate;
                writeDate = processWrites(deviceDescriptionPtr);
                if(writeDate < fireDate)
                    fireDate = writeDate;
            }
            if(deviceDescriptionPtr->readLocks) {
                CFAbsoluteTime readDate;
                readDate = processReads(deviceDescriptionPtr);
                if(readDate < fireDate)
                    fireDate = readDate;
            }
#if TIMING
            clock_get_uptime(&timestamp2);
            start = (((unsigned long long)timestamp1.hi) << 32) |
                    (unsigned long long)((unsigned int)(timestamp1.lo));
    
            stop = (((unsigned long long)timestamp2.hi) << 32) |
                    (unsigned long long)((unsigned int)(timestamp2.lo));
    
            elapsed_msecs = (int)(((double)(stop - start)) / divisor);
            if(elapsed_msecs > worst_callback) {
                pKGSysCall_Insert (kIOFWDVTrace, 'call', elapsed_msecs, 0, 0, 0, 0);
                syslog(LOG_INFO, "callback delay %d mSec\n",
                        elapsed_msecs);
                worst_callback = elapsed_msecs;
            }
#endif        
        }
    }
    DVSetTimeoutTime(g->fDVThread, fireDate);
#if TIMING
    DVLog(g->fDVThread, 'timr', now, CFAbsoluteTimeGetCurrent());
#endif
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static pascal ComponentResult
FWDVComponentOpen(IsochComponentInstancePtr storage, ComponentInstance self)
{
    IsochComponentGlobalsPtr g = &globals;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: FWDVComponentOpen begin\n");
#endif
	
    RecordEventLogger( 'isoc', 'open', g->fNumInstances, 0);
    if( nil == (storage = (IsochComponentInstancePtr)NewPtrClear(sizeof(IsochComponentInstance))))
        return(MemError());

    RecordEventLogger( 'isoc', 'ope2', (int)storage, 0);
    //syslog(LOG_INFO, "%x: FWDVComponentOpen count %d\n", pthread_self(), g->fNumInstances);

    SetComponentInstanceStorage(self, (Handle) storage);

    // One-time initialization of globals
    if (!g->fDVThread) {
        g->fDVThread = DVCreateThread(deviceArrived, g, timerTick, g, deviceMessage);
        if (!g->fDVThread)
            return(MemError());
        g->nDevices=0;    
        DVRunThread(g->fDVThread);
    }
    
    g->fNumInstances++;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: FWDVComponentOpen fNumInstances: %d\n",g->fNumInstances);
#endif
	
#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: FWDVComponentOpen end\n");
#endif
	
    return(noErr);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static pascal ComponentResult
FWDVComponentClose(IsochComponentInstancePtr ih, ComponentInstance self)
{
    int i;
    IsochComponentGlobalsPtr g = &globals;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: FWDVComponentClose begin\n");
#endif
	
    RecordEventLogger( 'isoc', 'clos', ih, self);
    if( !ih)
        return( noErr );

    if(ih->hasDeviceControl) {
        DeviceDescriptionPtr deviceDescriptionPtr;
        if(findDeviceDescriptionforDevice( ih, ih->deviceID, &deviceDescriptionPtr) == noErr)
        	closeDeviceControl( ih, deviceDescriptionPtr);
    }
    
    if( ih->permissions )
        FWDVIDHCloseDevice(ih);

    //syslog(LOG_INFO, "%x: FWDVComponentClose count %d\n", pthread_self(), g->fNumInstances);
    g->fNumInstances--;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: FWDVComponentClose fNumInstances: %d\n",g->fNumInstances);
#endif
	
    if(g->fNumInstances == 0) {
    
        // Free all mach ports etc.
        for(i=0; i<g->nDevices; i++) {
            DeviceDescriptionPtr dev = &g->deviceDescription[i];
            dev->fConnected = 0;
            if(dev->fDevice) {
                DVDeviceTerminate(dev->fDevice);
                dev->fDevice = NULL;
            }
            if(dev->deviceContainer) {
                QTDisposeAtomContainer(dev->deviceContainer);
                dev->deviceContainer = NULL;
            }
        }
        if(g->fDVThread) {
            //syslog(LOG_INFO, "%x: Calling DVFreeThread\n", pthread_self());
            DVFreeThread(g->fDVThread);
            g->fDVThread = NULL;
        }
    }
    
    DisposePtr((Ptr) ih);

    SetComponentInstanceStorage(self, (Handle) nil );
    RecordEventLogger( 'isoc', 'clos', 'end ', 0);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: FWDVComponentClose end\n");
#endif
	
	return( noErr );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static pascal ComponentResult
FWDVComponentVersion(IsochComponentInstancePtr storage)
{
    RecordEventLogger( 'isoc', 'vers', 0, 0);
    return (DVVersion << 16) | DVRevision;
}

static pascal ComponentResult
FWDVComponentRegister(IsochComponentInstancePtr storage)
{
    // need to re-register with each source type?
    RecordEventLogger( 'isoc', 'reg ', 0, 0);
    return( noErr );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
static pascal ComponentResult
FWDVIDHGetDeviceList(IsochComponentInstancePtr storage,
                     QTAtomContainer *deviceList )
{
    OSStatus 		result = noErr;
    QTAtomContainer	container = nil;
    int			devIndex;
    UInt32		version;
    IsochComponentGlobalsPtr g = &globals;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: FWDVIDHGetDeviceList begin\n");
#endif

    RecordEventLogger( 'isoc', 'get ', 'dev ', 'list');
    do {
        // create device atom list now
        result = QTNewAtomContainer( &container);
        if(result != noErr)
            break;

        // save seed
        result = QTInsertChild( container, kParentAtomIsContainer, kIDHSeedAtomType, 0, 0,
                                sizeof( g->seed), &g->seed, nil);
        if(result != noErr)
            break;

        version = (DVVersion << 24) | (DVRevision << 16) | DVBuildNumber;

        // save isoch version
        result = QTInsertChild( container, kParentAtomIsContainer, kIDHIsochVersionAtomType, 0, 0,
                                sizeof( UInt32), &version, nil);
        if(result != noErr)
            break;

        // save useCMP value
        result = QTInsertChild( container, kParentAtomIsContainer, kIDHUseCMPAtomType, 0, 0,
                                sizeof( UInt32), &g->useCMP, nil);
        if(result != noErr)
            break;

        // save each active device
        for( devIndex=0; devIndex<kDVMaxDevicesActive; ++devIndex)
        {
            if( g->deviceDescription[devIndex].fDevice != 0 && g->deviceDescription[devIndex].fActive)
            {
                result = QTInsertChildren( container, kParentAtomIsContainer,
                                                  g->deviceDescription[devIndex].deviceContainer);
                if(result != noErr)
                    break;
            }
        }

        *deviceList = container;
        return noErr;
    } while (false);
    
    if(container) {
        QTRemoveChildren( container, kParentAtomIsContainer);
        QTDisposeAtomContainer( container);
    }

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: FWDVIDHGetDeviceList end\n");
#endif
	
	return result;
}

static pascal ComponentResult
FWDVIDHSetDeviceConfiguration(IsochComponentInstancePtr ih,
                              const QTAtomSpec	*configID)
{
    OSStatus 				result = noErr;
    QTAtomSpec			volatileAtomSpec;
    QTAtom				nameAtom;
    IDHDeviceID			previousDeviceID;
    DeviceDescription	*deviceDescriptionPtr;
    IsochComponentGlobalsPtr g = &globals;
    Boolean				isSDL;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: FWDVIDHSetDeviceConfiguration begin\n");
#endif
	
    RecordEventLogger( 'isoc', 'set ', 'conf', ih);
    
    FailWithAction( configID == nil, result = paramErr, Exit);

    FailWithAction( configID->container == nil, result = paramErr, Exit);

    // if the client is setting to the same config, then we are ok
    if( configID->container == ih->currentConfig.container &&
            configID->atom == ih->currentConfig.atom)
        goto Exit;

    // device already in use, please close device first
    FailWithAction( ih->permissions != 0, result = kIDHErrDeviceInUse, Exit);

    previousDeviceID = ih->deviceID;

    volatileAtomSpec = *configID;
    result = getDeviceID( &volatileAtomSpec, &ih->deviceID);
    FailWithVal( result != noErr, Exit, result);

    result = checkSeed( g, &volatileAtomSpec);
    FailWithVal( result != noErr, Exit, result);

    ih->currentConfig = *configID;

    nameAtom = QTFindChildByIndex( configID->container, configID->atom, kIDHNameAtomType, 1, nil);
    if( nameAtom != nil) {
        Str255 name;
        long size;
        QTCopyAtomDataToPtr( configID->container, nameAtom, true, 255, name, &size);
        isSDL = !strncmp(name, "\pDV-SDL", *name);
    }
    else
        isSDL = false;
    
    result = findDeviceDescriptionforDevice( ih, ih->deviceID, &deviceDescriptionPtr);
    FailWithVal( result != noErr, Exit, result);

    // Open device
    if(deviceDescriptionPtr->fDevice->fDevInterface == NULL) {
        result = DVDeviceOpen(g->fDVThread, deviceDescriptionPtr->fDevice);
        if (result != kIOReturnSuccess) {
            goto Exit;
        }
        deviceDescriptionPtr->fConnected = 1;
    }
    else
        deviceDescriptionPtr->fConnected++;
    
    FailWithAction( deviceDescriptionPtr->fActive == false, result = kIDHErrDeviceDisconnected, Exit);

    // if this client had a previous connection, close that and if it had device control,
    // close that too.
    if( previousDeviceID && previousDeviceID != ih->deviceID) {
        DeviceDescription *deviceDescriptionPtr2;

        result = findDeviceDescriptionforDevice( ih, previousDeviceID, &deviceDescriptionPtr2);
        FailWithVal( result != noErr, Exit, result);
        if(deviceDescriptionPtr2->fConnected) {
            deviceDescriptionPtr2->fConnected--;
            // Close device if nobody else has it open and it hasn't been unplugged already
            if(deviceDescriptionPtr2->fConnected == 0 && deviceDescriptionPtr2->fActive &&
                deviceDescriptionPtr2->fDevice->fDevInterface) {
                DVDeviceClose(deviceDescriptionPtr2->fDevice);
            }
        }
        if(ih->hasDeviceControl) {
                result = closeDeviceControl( ih, deviceDescriptionPtr2);
                FailMessage( result != noErr);

                ih->hasDeviceControl = false;
        }
    }
            
Exit:
    RecordEventLogger( 'isoc', 'set ', 'Exit', ih);

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: FWDVIDHSetDeviceConfiguration end\n");
#endif
	
    return result;
}

static pascal ComponentResult
FWDVIDHOpenDevice(IsochComponentInstancePtr ih, UInt32 permissions)
{
    ComponentResult result = noErr;
    IsochComponentGlobalsPtr g = &globals;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: FWDVIDHOpenDevice begin\n");
#endif
	
    RecordEventLogger( 'open', ' dev', ih, permissions);
    
    FailWithAction( permissions == 0, result = paramErr, Exit);

    FailWithAction( ih->deviceID == nil, result = kIDHErrDeviceNotConfigured, Exit);

	{
		DeviceDescription* description ;
		FailWithAction( noErr != findDeviceDescriptionforDevice( ih, ih->deviceID, & description ), result = kIDHErrDeviceNotConfigured, Exit ) ;
	
		FailWithAction( description == nil, result = kIDHErrDeviceNotConfigured, Exit ) ;
		FailWithAction( description->fDevice == nil, result = kIDHErrDeviceNotConfigured, Exit ) ;
		FailWithAction( description->fDevice->fDevInterface == nil, result = kIDHErrDeviceNotConfigured, Exit ) ;
		FailWithAction( description->fDevice->fAVCInterface == nil, result = kIDHErrDeviceNotConfigured, Exit ) ;
	}
	
    result = sendMsg(ih, processOpen, (void *)permissions);

    FailWithVal( result != noErr, Exit, result);
    result = postEvent( g, ih->deviceID,
            (permissions & kIDHOpenForReadTransactions)?kIDHEventReadEnabled:kIDHEventWriteEnabled);
    FailWithVal( result != noErr, Exit, result);
#ifdef DRAWRINGBUFFERGRAPH
	SetUpBlitGlobals((DeviceDescription	*)ih->deviceID);
#endif	 

Exit:

#ifdef kIDH_Verbose_Debug_Logging
		syslog(LOG_INFO, "IDH: FWDVIDHOpenDevice end\n");
#endif
	
	return result;
}

static pascal ComponentResult
FWDVIDHCloseDevice(IsochComponentInstancePtr ih)
{
    OSStatus 			result = noErr;
    IsochComponentGlobalsPtr g = &globals;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: FWDVIDHCloseDevice begin\n");
#endif
	
    RecordEventLogger( 'isoc', 'clos', ' dev', ih);

    FailWithAction( ih->deviceID == nil, result = kIDHErrDeviceNotConfigured, Exit);

    if( ih->permissions == 0)	// client has no open devices?
            goto Exit;

    //syslog(LOG_INFO, "%x: FWDVIDHCloseDevice\n", pthread_self());
    result = sendMsg(ih, processClose, 0);
    FailWithVal( result != noErr, Exit, result);
    result = postEvent( g, ih->deviceID,
            (ih->permissions & kIDHOpenForReadTransactions)?kIDHEventReadDisabled:kIDHEventWriteDisabled);
    FailWithVal( result != noErr, Exit, result);
        
Exit:
    ih->permissions = 0;	// make sure device is closed
    ih->fClientIndex = 0xdead;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: FWDVIDHCloseDevice end\n");
#endif
	
    return result;
}

//еее USER BETTER KEEP HIS DEVICE LIST AROUND IF THIS IS TO MEAN ANYTHING
static pascal ComponentResult
FWDVIDHGetDeviceConfiguration(IsochComponentInstancePtr ih, QTAtomSpec *configID )
{
    OSStatus 	result = noErr;

#ifdef kIDH_Verbose_Debug_Logging
	syslog(LOG_INFO, "IDH: FWDVIDHGetDeviceConfiguration begin\n");
#endif
	
    RecordEventLogger( 'isoc', 'get ', 'dev ', 'conf');

    FailWithAction( configID == nil, result = paramErr, Exit);
    FailWithAction( ih->currentConfig.container == nil || ih->currentConfig.atom == nil,
            result = kIDHErrDeviceNotConfigured, Exit);

    *configID = ih->currentConfig;

Exit:

#ifdef kIDH_Verbose_Debug_Logging
		syslog(LOG_INFO, "IDH: FWDVIDHGetDeviceConfiguration end\n");
#endif
	
	return result;
}

static pascal ComponentResult
FWDVIDHGetDeviceStatus(IsochComponentInstancePtr ih, const QTAtomSpec *devSpec, IDHDeviceStatus *status )
{
        OSStatus 			result = noErr;
        IDHDeviceID		deviceID = nil;
        QTAtom			deviceInfoAtom, deviceAtom;
        QTAtomSpec		volatileAtomSpec;
        DeviceDescription	*deviceDescriptionPtr;
        IsochComponentGlobalsPtr g = &globals;
        UInt8			inputFormat = kIDHDV_SD;

#ifdef kIDH_Verbose_Debug_Logging
		syslog(LOG_INFO, "IDH: FWDVIDHGetDeviceStatus begin\n");
#endif
		
		RecordEventLogger( 'isoc', 'get ', 'stat', ih);
        FailWithAction( devSpec == nil, result = paramErr, Exit);
        FailWithAction( status == nil, result = paramErr, Exit);

        volatileAtomSpec = *devSpec;

        result = checkSeed( g, &volatileAtomSpec);
        if( result != noErr)
                goto Exit;

        result = getDeviceID( &volatileAtomSpec, &deviceID);
        FailWithVal( result != noErr, Exit, result);

        result = findDeviceDescriptionforDevice( ih, deviceID, &deviceDescriptionPtr);	// find description for this device
        FailWithVal( result != noErr, Exit, result);

        deviceAtom = QTFindChildByIndex( deviceDescriptionPtr->deviceContainer, kParentAtomIsContainer, kIDHDeviceAtomType, 1, nil);
        FailWithAction( deviceAtom == nil, result = kIDHErrDeviceList, Exit);

        // find device status for this device
        deviceInfoAtom = QTFindChildByIndex( deviceDescriptionPtr->deviceContainer, deviceAtom, kDVDeviceInfo, 1, nil);
        FailWithAction( deviceInfoAtom == nil, result = kIDHErrDeviceList, Exit);

        // Ask device what it's currently configured to transmit
        // Need to ask info for plug 0 for Panasonic AJ250, which always says 'DV' for kAVCOutputSignalModeOpcode,
        // even when sending DVCPro25.
#if 1
        {
            OSStatus err;
            DVCTransactionParams transaction;
            UInt8 out[8];
            UInt8 in[8];
            
            out[0] = kAVCStatusInquiryCommand;
            out[1] = kAVCUnitAddress;
            out[2] = kAVCOutputPlugSignalFormatOpcode;
            out[3] = 0;	// Plug
            out[4] = out[5] = out[6] = out[7] = 0xff;
            transaction.commandBufferPtr = out;
            transaction.commandLength = sizeof(out);
            transaction.responseBufferPtr = in;
            transaction.responseBufferSize = sizeof(in);
            transaction.responseHandler = NULL;
            
            err = doAVCTransaction(deviceDescriptionPtr, &transaction);
            if(err == noErr && in[0] == 0xc) {
                inputFormat = (in[5] >> 2) & 0x1f; // Fish out STYPE field
            }
        }
#else
        {
            OSStatus err;
            DVCTransactionParams transaction;
            UInt8 out[4];
            UInt8 in[4];
            
            out[0] = kAVCStatusInquiryCommand;
            out[1] = IOAVCAddress(kAVCTapeRecorder, 0);
            out[2] = kAVCOutputSignalModeOpcode;
            out[3] = kAVCSignalModeDummyOperand;

            transaction.commandBufferPtr = out;
            transaction.commandLength = sizeof(out);
            transaction.responseBufferPtr = in;
            transaction.responseBufferSize = sizeof(in);
            transaction.responseHandler = NULL;
            
            err = doAVCTransaction(deviceDescriptionPtr, &transaction);
            if(err == noErr && in[0] == 0xc) {
                inputFormat = (in[3] >> 2) & 0x1f; // Fish out STYPE field
            }
        }
#endif
        //status->version = 				0x200;
        status->physicallyConnected =	true;
        status->readEnabled = 			deviceDescriptionPtr->readLocks;
        status->writeEnabled = 			deviceDescriptionPtr->writeLocks;
        status->exclusiveAccess = 		0;//deviceDescriptionPtr->exclusiveAccess;
        status->currentBandwidth = 		0;
        status->currentChannel = 		0;

        //еее need to make this work with camera tracking
        status->deviceActive = 			deviceDescriptionPtr->fActive;
        status->inputFormat =			inputFormat;
        if(deviceDescriptionPtr->fActive) {
            status->inputStandard =			deviceDescriptionPtr->fDevice->standard;
        
            // Does caller want extended status?
            if(status->version == 0x200)
                status->outputFormats = 	deviceDescriptionPtr->fDevice->fDVFormats;
        }
// JKL *** what to to with this? does this mean deviceID, cameraFWClientID, or localNodeFWClientID
// Think this is for clock to set the localFWReferenceID
        status->localNodeID	= 		(PsuedoID) deviceDescriptionPtr->fDevice;

        result = QTSetAtomData( deviceDescriptionPtr->deviceContainer, deviceInfoAtom, sizeof( IDHDeviceStatus), status);
        FailWithVal( result != noErr, Exit, result);

Exit:

#ifdef kIDH_Verbose_Debug_Logging
			syslog(LOG_INFO, "IDH: FWDVIDHGetDeviceStatus end\n");
#endif
		
        return result;
}

static pascal ComponentResult 
FWDVIDHGetDeviceClock(IsochComponentInstancePtr ih, Component *clock )
{
    DeviceDescription	*deviceDescriptionPtr;
    OSStatus 		result = noErr;

    RecordEventLogger( 'isoc', 'get ', 'clok', 0);

    FailWithAction( clock == nil, result = paramErr, Exit);

    FailWithAction( ih->deviceID == nil, result = kIDHErrDeviceNotConfigured, Exit);

    result = findDeviceDescriptionforDevice( ih, ih->deviceID, &deviceDescriptionPtr);
    FailWithVal( result != noErr, Exit, result);

//	FailWithAction( deviceDescriptionPtr->active == false, result = kIDHErrDeviceDisconnected, Exit);
    
    *clock = deviceDescriptionPtr->clock;
    
Exit:
    return result;
}											

// can be called synchronously at task level only
static pascal ComponentResult
FWDVIDHRead(IsochComponentInstancePtr ih, IDHParameterBlock *pb)
{
        OSStatus 			result = noErr;
        DeviceDescription	*deviceDescriptionPtr;

        RecordEventLogger( 'isoc', 'read', (unsigned long) ih, (unsigned long) pb );

        //syslog(LOG_INFO, "FWDVIDHRead, completion = %p buffer = %p\n", pb->completionProc, pb->buffer);
        //syslog(LOG_INFO, "pb %p = req %d actual %d, result %d\n",
        //    pb, pb->requestedCount, pb->actualCount, pb->result);

        FailWithAction( pb == nil, result = paramErr, Exit);

        // can't do sync reads in I/O thread
        //FailWithAction( pb->completionProc == nil && CurrentExecutionLevel() != kTaskLevel, result = paramErr, Exit);

        FailWithAction( ih->deviceID == nil, result = kIDHErrDeviceNotConfigured, Exit);

        FailWithAction( !(ih->permissions & kIDHOpenForReadTransactions), result = kIDHErrDeviceNotOpened, Exit);
        
        result = findDeviceDescriptionforDevice( ih, ih->deviceID, &deviceDescriptionPtr);
        FailWithVal( result != noErr, Exit, result);

        FailWithAction( deviceDescriptionPtr->fActive == false, result = kIDHErrDeviceDisconnected, Exit);

        if( pb->completionProc == nil) // synchronous read
        {
            int frameIndex = deviceDescriptionPtr->fReadSharedVars->fReader % deviceDescriptionPtr->fNumOutputFrames;
            UInt8 *buffer = (UInt8 *)deviceDescriptionPtr->bufMem[frameIndex];
            UInt32 frameSize;

            // wait for writer
            // if (*devices[refNum].fReader + 1 >= *devices[refNum].fWriter) return -1;
            if( deviceDescriptionPtr->fOldDrop <
                deviceDescriptionPtr->fReadSharedVars->fDroppedFrames) {
                    dropMsg(deviceDescriptionPtr, deviceDescriptionPtr->fReadSharedVars->fDroppedFrames);
            }

            while (deviceDescriptionPtr->fReadSharedVars->fReader >= deviceDescriptionPtr->fReadSharedVars->fWriter)
            {
                usleep(12500);	// time DCL block takes to run
            }
            frameSize = deviceDescriptionPtr->fReadSharedVars->fFrameSize[frameIndex];
            pb->actualCount = frameSize;
            pb->reserved1 = cycles(deviceDescriptionPtr->fReadSharedVars->fFrameTime[frameIndex]);
            if(pb->buffer != nil) {
                // copy frame
                bcopy(buffer, pb->buffer, frameSize);
                deviceDescriptionPtr->fReadSharedVars->fReader += 1;   // release buffer         
            }
            else
                pb->buffer = buffer;
            pb->result = noErr;
        }
        else {
            pb->result = kIDHErrCompletionPending;
            //syslog(LOG_INFO, "FWDVIDHRead end, completion = %p buffer = %p\n", pb->completionProc, pb->buffer);
            //syslog(LOG_INFO, "pb %p = req %d actual %d, result %d\n",
            //    pb, pb->requestedCount, pb->actualCount, pb->result);

            result = sendMsg(ih, queueRead, pb);
        }

Exit:
#ifdef DRAWRINGBUFFERGRAPH
	BlitBufferGraph(deviceDescriptionPtr);
#endif	 
        return result;
}

static pascal ComponentResult
FWDVIDHReleaseBuffer(IsochComponentInstancePtr ih, IDHParameterBlock *pb)
{
    OSStatus 		result = noErr;
    int			frameIndex;
    DeviceDescription	*deviceDescriptionPtr;

    RecordEventLogger( 'isoc', 'rele', 'ase ', 'buff');

    FailWithAction( pb == nil, result = paramErr, Exit);

    FailWithAction( ih->deviceID == nil, result = kIDHErrDeviceNotConfigured, Exit);

    result = findDeviceDescriptionforDevice( ih, ih->deviceID, &deviceDescriptionPtr);
    FailWithVal( result != noErr, Exit, result);
    frameIndex = deviceDescriptionPtr->fReadSharedVars->fReader % deviceDescriptionPtr->fNumOutputFrames;
    if(pb->buffer == (Ptr)deviceDescriptionPtr->bufMem[frameIndex]) {
        if(--deviceDescriptionPtr->fBufferLocks[frameIndex] == 0)
            deviceDescriptionPtr->fReadSharedVars->fReader += 1;   // release buffer, this isn't quite right if several buffers are held.
    }
Exit:
    return result;
}

static pascal ComponentResult
FWDVIDHCancelPendingIO(IsochComponentInstancePtr ih, IDHParameterBlock *pb)
{
    OSStatus 		result = noErr;
    DeviceDescription	*deviceDescriptionPtr;

    RecordEventLogger( 'isoc', 'canc', 'elIO', 0);

    FailWithAction( pb == nil, result = paramErr, Exit);

    FailWithAction( ih->deviceID == nil, result = kIDHErrDeviceNotConfigured, Exit);

    result = findDeviceDescriptionforDevice( ih, ih->deviceID, &deviceDescriptionPtr);
    FailWithVal( result != noErr, Exit, result);

    result = sendMsg(ih, processCancelPendingIO, pb);

Exit:
#ifdef DRAWRINGBUFFERGRAPH
	BlitBufferGraph(deviceDescriptionPtr);
#endif	 
    return result;
}

// can be called synchronously at task level only
static pascal ComponentResult
FWDVIDHWrite(IsochComponentInstancePtr ih, IDHParameterBlock *pb)
{
    OSStatus 		result = noErr;
    DeviceDescription	*deviceDescriptionPtr;

    RecordEventLogger( 'isoc', 'writ', ih, pb);

    FailWithAction( pb == nil, result = paramErr, Exit);

    // check for illegal condition
    FailWithAction( pb->completionProc == nil && pb->buffer == nil, result = paramErr, Exit);

    FailWithAction( ih->deviceID == nil, result = kIDHErrDeviceNotConfigured, Exit);

    result = findDeviceDescriptionforDevice( ih, ih->deviceID, &deviceDescriptionPtr);
    FailWithVal( result != noErr, Exit, result);

    FailWithAction( deviceDescriptionPtr->fActive == false, result = kIDHErrDeviceDisconnected, Exit);

    if( pb->completionProc == nil) // synchronous write
    {
        pb->result = kIDHErrCompletionPending;

        result = sendMsg(ih, queueWrite, pb);
        while (pb->result == kIDHErrCompletionPending)
        {
            usleep(12500);	// time DCL block takes to run
        }
    }
    else {
        result = sendMsg(ih, queueWrite, pb);
    }


Exit:
#ifdef DRAWRINGBUFFERGRAPH
	BlitBufferGraph(deviceDescriptionPtr);
#endif	 
    return result;
}

static pascal ComponentResult
FWDVIDHGetDeviceControl(IsochComponentInstancePtr ih, ComponentInstance *deviceControl)
{
    ComponentResult				result = noErr;
    DeviceDescription			*deviceDescriptionPtr;

    RecordEventLogger( 'isoc', 'get ', 'dev ', 'ctrl');

    if ( deviceControl == nil )
            return(paramErr);

    FailWithAction( ih->deviceID == nil, result = kIDHErrDeviceNotConfigured, Exit);

    result = findDeviceDescriptionforDevice( ih, ih->deviceID, &deviceDescriptionPtr);
    FailWithVal( result != noErr, Exit, result);

    FailWithAction( deviceDescriptionPtr->fActive == false, result = kIDHErrDeviceDisconnected, Exit);

    // =======================================================================
    // All should be good here from a device stand point, now open the device
    // control component.  If the devc is not nil either we didn't release it
    // from close or the same client is calling open again.

    if ( deviceDescriptionPtr->deviceControlInstance == nil)
    {
        ComponentDescription	devcDesc;
        Component		devcComp;
        ComponentInstance	devc;

        devcDesc.componentType 		= kDeviceControlComponentType;
        devcDesc.componentSubType 	= kDeviceControlSubtypeFWDV;
        devcDesc.componentManufacturer 	= 0;
        devcDesc.componentFlags 	= 0L;
        devcDesc.componentFlagsMask 	= 0L;

        devcComp = nil;
        devcComp = FindNextComponent( devcComp, &devcDesc);
        FailMessage( devcComp == nil);
        if ( devcComp )
        {
            result = OpenAComponent(devcComp, &devc);
            FailWithVal( result != noErr, Exit, result);

            result = DeviceControlSetDeviceConnectionID(devc, 
                (DeviceConnectionID) deviceDescriptionPtr->fDevice->fAVCInterface);
            FailWithVal( result != noErr, Exit, result);
            result = DeviceControlEnableAVCTransactions(devc);
            FailWithVal( result != noErr, Exit, result);

            deviceDescriptionPtr->deviceControlInstance = devc;
        }
    }

    if( ih->hasDeviceControl == false && deviceDescriptionPtr->deviceControlInstance != nil)
    {
            ih->hasDeviceControl = true;
            ++deviceDescriptionPtr->deviceControlCount;
    }


    *deviceControl = deviceDescriptionPtr->deviceControlInstance;

    FailMessage( *deviceControl == nil);

Exit:
    return(result);
}

static pascal ComponentResult
FWDVIDHUpdateDeviceList(IsochComponentInstancePtr ih, QTAtomContainer *deviceList )
{
    OSStatus 			result = noErr;
    short			nDVDevices, i;
    QTAtomSpec		atomSpec;
    IsochComponentGlobalsPtr g = &globals;

    RecordEventLogger( 'isoc', 'updt', 'dev ', 'list');

    FailWithAction( deviceList == nil, result = paramErr, Exit);

    atomSpec.container = *deviceList;

    result = checkSeed( g, &atomSpec);	// make sure the container is current
    if( result != noErr)
            goto Exit;

    // check for useCMP value changing
    if (result == noErr)
    {
            QTAtom	useCMPAtom;
            UInt32	useCMPValue;

            useCMPAtom = QTFindChildByIndex(*deviceList, kParentAtomIsContainer, kIDHUseCMPAtomType, 1, nil);
            if (useCMPAtom)
            {
                    QTLockContainer(*deviceList);
                    result = QTCopyAtomDataToPtr(*deviceList, useCMPAtom, true, 4, &useCMPValue, nil);
                    QTUnlockContainer(*deviceList);

                    if (result == noErr)
                            g->useCMP = useCMPValue;

            }
    }

    // move all volatile atoms here
    nDVDevices = QTCountChildrenOfType( *deviceList, kParentAtomIsContainer, kIDHDeviceAtomType);
    for( i=0; i<nDVDevices; ++i)
    {
            QTAtom 				deviceAtomNew, nameAtomNew, deviceIDAtom;
            QTAtom 				deviceAtomOld, nameAtomOld;
            DeviceDescription	*deviceDescriptionPtr;
            IDHDeviceID			deviceID;
            UInt8				newName[256];
            SInt32				actualSize;

            // get the client supplied atoms
            deviceAtomNew = QTFindChildByIndex( *deviceList, kParentAtomIsContainer, kIDHDeviceAtomType, i + 1, nil);
            FailIf( deviceAtomNew == nil, Exit);

            nameAtomNew = QTFindChildByIndex( *deviceList, deviceAtomNew, kIDHNameAtomType, 1, nil);
            FailIf( nameAtomNew == nil, Exit);

            deviceIDAtom = QTFindChildByIndex( *deviceList, deviceAtomNew, kIDHDeviceIDType, 1, nil);
            FailIf( deviceIDAtom == nil, Exit);

            QTLockContainer( *deviceList);

            QTCopyAtomDataToPtr( *deviceList, deviceIDAtom, true, sizeof( IDHDeviceID), &deviceID, nil);

            QTUnlockContainer( *deviceList);

            // find the local copy of this device container
            result = findDeviceDescriptionforDevice( ih, deviceID, &deviceDescriptionPtr);
            FailWithVal( result != noErr, Exit, result);

            deviceAtomOld = QTFindChildByIndex( deviceDescriptionPtr->deviceContainer, kParentAtomIsContainer, kIDHDeviceAtomType, 1, nil);
            FailIf( deviceAtomOld == nil, Exit);

            nameAtomOld = QTFindChildByIndex( deviceDescriptionPtr->deviceContainer, deviceAtomOld, kIDHNameAtomType, 1, nil);
            FailIf( nameAtomOld == nil, Exit);

            // get new name
            QTLockContainer( *deviceList);
            result = QTCopyAtomDataToPtr(*deviceList, nameAtomNew, true, 256, newName, &actualSize);
            QTUnlockContainer( *deviceList);
            FailWithVal( result != noErr, Exit, result);

            // update prefs file with new name
            // JKL, don't want to do this for now since FCP might be using this routine to toggle CMP
//		updateCameraName(&deviceDescriptionPtr->uniqueID, newName);

            // copy the new data into the current atom
            result = QTReplaceAtom( deviceDescriptionPtr->deviceContainer, nameAtomOld, *deviceList, nameAtomNew);
            FailWithVal( result != noErr, Exit, result);
    }

Exit:
    return result;
}																		

static pascal ComponentResult
FWDVIDHGetDeviceTime(IsochComponentInstancePtr ih, TimeRecord *time)
{
    OSStatus 		result;
    DeviceDescription	*deviceDescriptionPtr;
    UInt32 		busTime, cycleTime;
    UInt64		cycles;

    FailWithAction( time == nil, result = paramErr, Exit);

    FailWithAction( ih->deviceID == nil, result = kIDHErrDeviceNotConfigured, Exit);

    result = findDeviceDescriptionforDevice( ih, ih->deviceID, &deviceDescriptionPtr);
    FailWithVal( result != noErr, Exit, result);

    FailWithAction( deviceDescriptionPtr->fActive == false, result = kIDHErrDeviceDisconnected, Exit);

     result = (*deviceDescriptionPtr->fDevice->fDevInterface)->
        GetBusCycleTime(deviceDescriptionPtr->fDevice->fDevInterface, &busTime, &cycleTime);
    
    if(result == noErr) {
        cycles = busTime * 8000;				// Seconds converted to cycles
        cycles += (cycleTime >> 12) & 0x1fff;	// Extract cycle count from cyletime register
        
        time->value.lo = cycles & 0xffffffff;
        time->value.hi = (cycles >> 32) & 0xffffffff;
        time->scale = 8000;
    }
Exit:
    return result;
}

static pascal ComponentResult
FWDVIDHNewNotification(IsochComponentInstancePtr ihc, IDHDeviceID deviceID,
    IDHNotificationProc notificationProc, void* userData, IDHNotificationID* notificationID)
{
    UInt32	i;
    Boolean	addedClient = false;
    OSStatus 	result = noErr;
    IsochComponentGlobalsPtr g = &globals;
    
    RecordEventLogger( 'isoc', 'new ', 'noti', 'fy  ');

    FailWithAction( notificationProc == nil, result = paramErr, Exit);
    FailWithAction( notificationID == nil, result = paramErr, Exit);

    i = 0;
    while (i < kMaxNotifications)
    {
        if (0 == g->clientNotification[i].deviceID)
        {
            g->clientNotification[i].deviceID = deviceID;
            g->clientNotification[i].notificationProc = notificationProc;
            g->clientNotification[i].events = 0;
            g->clientNotification[i].userData = userData;
            *notificationID = (UInt32)&g->clientNotification[i];
            addedClient = true;
            break;
        }
        
        ++i;
    }
    
    if (!addedClient)				// List is full. Unable to add addtional clients
            result = paramErr;
    
Exit:
    return result;	
}

static pascal ComponentResult
FWDVIDHNotifyMeWhen(ComponentInstance idh, IDHNotificationID notificationID, IDHEvent events)
{
    OSStatus 		result = noErr;
    ClientNotification*	clientNotification = (ClientNotification*)notificationID;
    
    RecordEventLogger( 'isoc', 'noti', 'when', events);

    FailWithAction( clientNotification == nil, result = paramErr, Exit);

    clientNotification->events = events;
    
Exit:	
    return result;
}

static pascal ComponentResult
FWDVIDHCancelNotification(ComponentInstance idh, IDHNotificationID notificationID)
{
    OSStatus 		result = noErr;
    ClientNotification*	clientNotification = (ClientNotification*)notificationID;
    
    RecordEventLogger( 'isoc', 'canc', 'el  ', 'noti');

    FailWithAction( clientNotification == nil, result = paramErr, Exit);
    
    clientNotification->events = 0;
	
Exit:	
    return result;	
}

static pascal ComponentResult
FWDVIDHDisposeNotification(ComponentInstance idh, IDHNotificationID notificationID)
{
    OSStatus 		result = noErr;
    ClientNotification*	clientNotification = (ClientNotification*)notificationID;

    RecordEventLogger( 'isoc', 'disp', 'ose ', 'noti');

    FailWithAction( clientNotification == nil, result = paramErr, Exit);

    clientNotification->deviceID = 0;
    
Exit:	
    return result;	
}

static pascal ComponentResult
FWDVIDHSetFormat(IsochComponentInstancePtr ih, UInt32 format)
{
    OSStatus 		result = noErr;
    DeviceDescription	*deviceDescriptionPtr;

    RecordEventLogger( 'isoc', 'setf', 'ormt', format);
    FailWithAction( ih->deviceID == nil, result = kIDHErrDeviceNotConfigured, Exit);

    result = findDeviceDescriptionforDevice( ih, ih->deviceID, &deviceDescriptionPtr);
    FailWithVal( result != noErr, Exit, result);
    
    deviceDescriptionPtr->fOutputMode = format << 2;	// Get STYPE field into position
    if(deviceDescriptionPtr->fDevice->standard != ntscIn)
        deviceDescriptionPtr->fOutputMode |= 0x80;	// PAL flag

Exit:	
    return result;	
}

static pascal ComponentResult
FWDVIDHGetFormat(IsochComponentInstancePtr ih, UInt32 *format)
{
    OSStatus 		result = noErr;
    DeviceDescription	*deviceDescriptionPtr;

    RecordEventLogger( 'isoc', 'getf', 'ormt', format);
    FailWithAction( ih->deviceID == nil, result = kIDHErrDeviceNotConfigured, Exit);

    result = findDeviceDescriptionforDevice( ih, ih->deviceID, &deviceDescriptionPtr);
    FailWithVal( result != noErr, Exit, result);

    *format = (deviceDescriptionPtr->fOutputMode >> 2) & 0x1f;	// Return just STYPE field, in bottom bits
    
Exit:	
    return result;	
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define DoCDispatchWS(x,p,s)		\
    case k ## x ## Select:		\
        /* printf("FWDV" #x "{"); */	\
        { ComponentResult err;		\
            err = CallComponentFunctionWithStorageProcInfo( s, p, (ProcPtr) FWDV ## x,	\
                                                uppCall ## x ## ProcInfo );		\
        /* printf("%ld}\n", err); */	\
        return err;	}

#define DoDispatchWS(x,p,s)		\
    case k ## x ## Select:		\
        /* printf("FWDV" #x "{"); */	\
        { ComponentResult err;		\
            err = CallComponentFunctionWithStorageProcInfo( s, p, (ProcPtr) FWDV ## x,	\
                                                upp ## x ## ProcInfo );			\
        /* printf("%ld}\n", err); */ 	\
        return err;	}


static pascal ComponentResult
FWDVComponentCanDo(IsochComponentInstancePtr storage, short selector)
{
    ComponentResult result;
    RecordEventLogger( 'isoc', 'cand', 0, 0);

    switch(selector) {
        /* Standard selectors */
        case kComponentOpenSelect:
        case kComponentCloseSelect:
        case kComponentCanDoSelect:
        case kComponentVersionSelect:

        /* IDH selectors */
        case kIDHGetDeviceListSelect:
        case kIDHGetDeviceConfigurationSelect:
        case kIDHSetDeviceConfigurationSelect:
        case kIDHGetDeviceStatusSelect:
        case kIDHGetDeviceClockSelect:
        case kIDHOpenDeviceSelect:
        case kIDHCloseDeviceSelect:
        case kIDHReadSelect:
        case kIDHWriteSelect:
        case kIDHNewNotificationSelect:
        case kIDHNotifyMeWhenSelect:
        case kIDHCancelNotificationSelect:
        case kIDHDisposeNotificationSelect:
        case kIDHReleaseBufferSelect:
        case kIDHCancelPendingIOSelect:
        case kIDHGetDeviceControlSelect:
        case kIDHUpdateDeviceListSelect:
        case kIDHGetDeviceTimeSelect:
        case kIDHSetFormatSelect:
        case kIDHGetFormatSelect:
            return(true);

        default:
            RecordEventLogger( 'isoc', 'cant', selector, 0);
            result = false;
            return (result);
    }
}


pascal ComponentResult
FWDVICodecComponentDispatch(ComponentParameters *params, char ** storage)
{
    ComponentResult result;

    /*	If the selector is less than zero, it's a Component manager selector.	*/

    if ( params->what < 0  ) {
        switch ( params->what ) {
            DoCDispatchWS( ComponentOpen, params, storage );
            DoCDispatchWS( ComponentClose, params, storage );
            DoCDispatchWS( ComponentRegister, params, storage );
            DoCDispatchWS( ComponentCanDo, params, storage );
            DoCDispatchWS( ComponentVersion, params, storage );

            default :
                return (paramErr);
        }
    }

    /*
     *	Here we dispatch the rest of our calls. We use the magic thing manager routine which
     *	calls our subroutines with the proper parameters. The prototypes are in Image Codec.h.
     */
    switch ( params->what ) {
        DoDispatchWS( IDHGetDeviceList, params, storage );
        DoDispatchWS( IDHGetDeviceConfiguration, params, storage );
        DoDispatchWS( IDHSetDeviceConfiguration, params, storage );
        DoDispatchWS( IDHGetDeviceStatus, params, storage );
        DoDispatchWS( IDHGetDeviceClock, params, storage );
        DoDispatchWS( IDHOpenDevice, params, storage );
        DoDispatchWS( IDHCloseDevice, params, storage );
        DoDispatchWS( IDHRead, params, storage );
        DoDispatchWS( IDHWrite, params, storage );
        DoDispatchWS( IDHReleaseBuffer, params, storage );
        DoDispatchWS( IDHCancelPendingIO, params, storage );
        DoDispatchWS( IDHGetDeviceControl, params, storage );
        DoDispatchWS( IDHUpdateDeviceList, params, storage );
        DoDispatchWS( IDHGetDeviceTime, params, storage );
        DoDispatchWS( IDHNewNotification, params, storage );
        DoDispatchWS( IDHNotifyMeWhen, params, storage );
        DoDispatchWS( IDHCancelNotification, params, storage );
        DoDispatchWS( IDHDisposeNotification, params, storage );
        DoDispatchWS( IDHSetFormat, params, storage );
        DoDispatchWS( IDHGetFormat, params, storage );
               
    default:
        {
            int len = params->paramSize/4;
            int i;
            printf("IDH unimp:%d %d ", params->what, params->paramSize);
            for(i=0; i<len; i++)
                printf("0x%lx ", params->params[i]);
            printf("\n");
            result = paramErr;
            return(result);
        }
    }
}

#ifdef DRAWRINGBUFFERGRAPH

static void SetUpBlitGlobals(DeviceDescriptionPtr ddp)
{
	GDHandle gdh =  GetMainDevice();
if (gdh)
	{
	ddp->fScreenBaseAddr = gdh[0]->gdPMap[0]->baseAddr;
	ddp->fRowBytes = gdh[0]->gdPMap[0]->rowBytes & 0x3FFF;
	ddp->fPixDepth = gdh[0]->gdPMap[0]->pixelSize;
	}
}

#define spacerColour 0xFFFF
#define unusedColour 0x0000
#define emptyColour (16<<10) + (16<<5) +(16)
#define hasDataColour (0<<10) + (31<<5) +(0)
#define readColour (31<<10) + (31<<5) +(0)
#define writeColour (0<<10) + (31<<5) +(31)
#define collisionColour (31<<10) + (0<<5) +(0)

static void BlitBufferGraph(DeviceDescriptionPtr ddp)
{
	short * line0Ptr = nil;
	short * line00Ptr = nil;
	short * line1Ptr = nil;
	short * line2Ptr = nil;
	short * line3Ptr = nil;
	short buffCol = 0;
	long flags =0;
	int x,i,read,write;
	if (ddp->fPixDepth != 16 || ddp->fScreenBaseAddr ==0 || ddp->fRowBytes ==0)
		return;
	line00Ptr =(short*) (ddp->fScreenBaseAddr + ddp->fRowBytes*18-ddp->fRowBytes/4);
	line0Ptr =(short*) (ddp->fScreenBaseAddr + ddp->fRowBytes*19-ddp->fRowBytes/4);
	line1Ptr =(short*) (ddp->fScreenBaseAddr + ddp->fRowBytes*16-ddp->fRowBytes/4);
	line2Ptr =(short*) (ddp->fScreenBaseAddr + ddp->fRowBytes*17-ddp->fRowBytes/4);
	line3Ptr =(short*) (ddp->fScreenBaseAddr + ddp->fRowBytes*15-ddp->fRowBytes/4);
	read = ddp->fSharedVars->fReader % ddp->fNumOutputFrames;
	write = ddp->fSharedVars->fWriter % ddp->fNumOutputFrames;
	for(x =0;x < ddp->fNumOutputFrames; x++)
		{
		*line1Ptr++ = (short)spacerColour;
		*line2Ptr++ = (short)spacerColour;
		*line0Ptr++ = (short)spacerColour;
		*line00Ptr++ = (short)spacerColour;
		
		for(i=0;i<ddp->fBufferLocks[x];i++) {
			short* beardPtr = line0Ptr+ddp->fRowBytes*i*2;
			*beardPtr++ = unusedColour;
			*beardPtr++ = unusedColour;
			*beardPtr++ = unusedColour;
			*beardPtr++ = unusedColour;
		}
		
		if (ddp->fBufferLocks[x]) {
			buffCol = hasDataColour;
		} else {
			buffCol = emptyColour;
		}
		*line1Ptr++ = buffCol;
		*line1Ptr++ = buffCol;
		*line1Ptr++ = buffCol;
		*line1Ptr++ = buffCol;
		
		*line2Ptr++ = buffCol;
		*line2Ptr++ = buffCol;
		*line2Ptr++ = buffCol;
		*line2Ptr++ = buffCol;
		
		buffCol = (short)spacerColour;
		if (read == x)
			buffCol = readColour;
		if (write == x)
			{
			if (buffCol == readColour)
				buffCol = collisionColour;
			else
				buffCol = writeColour;
			}
		*line00Ptr++ = buffCol;
		*line00Ptr++ = buffCol;
		*line00Ptr++ = buffCol;
		*line00Ptr++ = buffCol;

		*line0Ptr++ = buffCol;
		*line0Ptr++ = buffCol;
		*line0Ptr++ = buffCol;
		*line0Ptr++ = buffCol;
		}
		
	
}
#endif

