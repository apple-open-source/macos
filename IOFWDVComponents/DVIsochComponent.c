
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
#include <IOKit/iokitmig.h>
#include <mach/mach_port.h>

#include <IOKit/DV/IOFWDVClient.h>
#include <IOKit/IOMessage.h>

#define kDVRequestID 0
#define kIOPending 1
#define kIORunning 2
#define kMaxDeviceClients 4

#define DEBUG 0
//#define DRAWRINGBUFFERGRAPH
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define kDVDeviceInfo 			'ddin'	// DV device info
#define kMaxDevicesActive		64	// max devices on firewire bus
#define kMaxInstances			10	// max clients attached to a device
#define kMaxNotifications		100	// 100 notifications can be supported
#define kNTSCCompressedBufferSize	120000
#define	kPALCompressedBufferSize	144000
#define kIDHSeedAtomType		'seed'	// seed for device atom validity
#define kIDHDevicePresentType		'pres'	// is device present?
#define	kServiceTypeDVServices		'avc '
#define kTimeoutDuration		(1000 / 15) * durationMillisecond	// 1/15 second (2 frame times)

typedef struct ClientNotification {
        ComponentInstance 	ihc;
        IDHDeviceID			deviceID;
        IDHNotificationProc	notificationProc;
        IDHEvent			events;
        void				*userData;
} ClientNotification;

// describes a device on the firewire bus
typedef struct DeviceDescription {
    UInt64		fGUID;				// persistent unique ID for each device
    io_object_t		fID;				// IOKit ID for each device
    io_connect_t	fConnection;
    io_object_t		fNotification;			// Handle for messages from kernel driver
    IsochComponentGlobalsPtr fGlobals;
    UInt32 		fNumOutputFrames;
    vm_address_t 	*bufMem;			// Buffers shared with kernel driver
    IOFWDVSharedVars *fSharedVars;	// Structure shared with kernel driver
    vm_address_t 	fDCLBuffer;			// DCL Buffers shared with kernel driver
    IOFWDVWriteSharedData *fSharedWriteVars;	// For access to output DCL variables
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
    int			fBufferLocks[kDVNumOutputFrames];
    ComponentInstance	deviceControlInstance;		// device control component instance
    Component		clock;				// FireWire clock component, clients get their own instance, not sure why
    UInt32 		standard; 			// device standard
    UInt32 		fDVFormats; 		// DV formats supported by device
    QTAtomContainer	deviceContainer;		// contains device database
    SInt16		deviceControlCount;		// number of clients using device control
    SInt16		readLocks;			// num clients that have device open for reads
    SInt16		writeLocks;			// num clients that have device open for writes
    Boolean		active;				// device is currently online
    UInt8		fOutputMode;		// AVC output signal mode - NTSC/SDL etc.
    UInt8		fWaitingStartWrite;	// 1 = if we're filling up buffers before starting to write, 2 = need to retry start
    Boolean		fConnected;				// device is currently configured
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
    DeviceDescription	deviceDescription[kMaxDevicesActive];	// description of each device
    ClientNotification	clientNotification[kMaxNotifications];	// arbirtary # of notifications
    UInt32		nDevices;				// number of devices in the list
    UInt32		seed;					// keep track of device lists

    // X Stuff
    mach_port_t			fMasterDevicePort;		// Master port for IOKit
    IONotificationPortRef	fNotifyPort;			// Our IOKit notification port
    mach_port_t			fNotifyMachPort;		// notify port as a mach port
    io_iterator_t		fMatchEnumer;			// Iterator over matching devices
    pthread_t			fWorkThread;			// For notifications and I/O
    pthread_attr_t		fThreadAttr;			// Attributes of work thread
    
    pthread_mutex_t		fGlobalsMutex;			// lock this before updating globals
    pthread_cond_t		fSyncCond;				// To synchronize threads.
    
    UInt32				fGlobalsState;			// 0 = uninitted. 
    CFBundleRef			fBundleID;				// Of the DV component's bundle
            
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

typedef struct {
    UInt32					fRequest;
    IsochComponentInstancePtr fInstance;
    void *					fParams;
} DVRequest;

typedef struct {
    mach_msg_header_t	msgHdr;
    DVRequest		dvRequest;
} SendMsg;

typedef struct {
    mach_msg_header_t	msgHdr;
    union {
        OSNotificationHeader	notifyHeader;
        DVRequest		dvRequest;
        UInt8			grr[72];	// Enough for IOServiceInterestContent
    }body;
    mach_msg_trailer_t	trailer;
} ReceiveMsg;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* Function prototypes */

pascal ComponentResult
FWDVICodecComponentDispatch(ComponentParameters *params, char ** storage);
static pascal ComponentResult
FWDVIDHCloseDevice(IsochComponentInstancePtr ih);

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
        sprintf(buf, "%c%c%c%c", a, b, c, d);
    else
        //printf(" 0x%x ", (int)val);
        //syslog(LOG_INFO, " 0x%x ", (int)val);
        sprintf(buf, " 0x%x ", (int)val);
        
    return buf + strlen(buf);
}

static void RecordEventLogger(UInt32 a, UInt32 b, UInt32 c, UInt32 d)
{
    char buf[256];
    char *curr = buf;
    sprintf(buf, "0x%x:", pthread_self());
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

static int pKGSysCall_Start (int code, int param1, int param2, int param3, int param4, int param5, int param6 )
{
	return syscall ( SYS_kdebug_trace, code | kKernelTraceEnable, param1, param2, param3, param4, param5, param6 );
}

static int pKGSysCall_End (int code, int param1, int param2, int param3, int param4, int param5, int param6 )
{
	return syscall ( SYS_kdebug_trace, code | kKernelTraceDisable, param1, param2, param3, param4, param5, param6 );
}

static int pKGSysCall_Insert (int code, int param1, int param2, int param3, int param4, int param5, int param6 )
{
	return syscall ( SYS_kdebug_trace, code , param1, param2, param3, param4, param5, param6 );
}

static void dropMsg(DeviceDescription *deviceDescriptionPtr)
{
    pKGSysCall_Insert (kIOFWDVTrace, 'drop', deviceDescriptionPtr->fSharedVars->fDroppedFrames-
                deviceDescriptionPtr->fOldDrop, deviceDescriptionPtr->fSharedVars->fDroppedFrames, 0, 0, 0);
    syslog(LOG_INFO,"Just dropped %ld frames (total %ld)!\n",
        deviceDescriptionPtr->fSharedVars->fDroppedFrames-deviceDescriptionPtr->fOldDrop,
        deviceDescriptionPtr->fSharedVars->fDroppedFrames);
    deviceDescriptionPtr->fOldDrop = deviceDescriptionPtr->fSharedVars->fDroppedFrames;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static void signalSync(UInt32 *var, UInt32 val)
{
    pthread_mutex_lock(&globals.fGlobalsMutex);
    *var = val;
	pthread_mutex_unlock(&globals.fGlobalsMutex);
    pthread_cond_broadcast(&globals.fSyncCond);
}

static void waitSync(UInt32 *var)
{
    if(!*var) {
        pthread_mutex_lock(&globals.fGlobalsMutex);
        while(!*var) {
            pthread_cond_wait(&globals.fSyncCond, &globals.fGlobalsMutex);
        }
        pthread_mutex_unlock(&globals.fGlobalsMutex);
    }
}

OSErr findDeviceDescriptionforDevice( IsochComponentInstancePtr ih, UInt32 deviceID, DeviceDescription **deviceDescription)
{
    OSErr	result = noErr;
    *deviceDescription = (DeviceDescriptionPtr)deviceID;
    return result;
}

//еее do a super find atom someday
static OSErr findAtom( const QTAtomSpec *atomSpec, OSType theType, QTAtom *theAtom)
{
        OSErr result = noErr;
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

static OSErr getDeviceID( QTAtomSpec *configID, UInt32 *deviceID)
{
        OSErr result = noErr;
        QTAtom deviceAtom;

        *deviceID = nil;

        result = findAtom( configID, kIDHDeviceAtomType, &deviceAtom);
        FailWithVal( result != noErr, Exit, result);

        result = QTGetAtomTypeAndID( configID->container, deviceAtom, nil, (long *) deviceID);
        FailWithVal( result != noErr, Exit, result);

Exit:
        return result;
}

static OSErr closeDeviceControl( IsochComponentInstancePtr ih, DeviceDescriptionPtr deviceDescriptionPtr)
{
        OSErr 						result = noErr;

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

        return result;
}

OSErr checkSeed( IsochComponentGlobalsPtr gGlobals, QTAtomSpec *configID)
{
        QTAtom 		seedAtom;
        OSErr 		result = noErr;
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

static OSErr setupVideoAtoms( QTAtomContainer container, QTAtom isocAtom, UInt32 standard, Boolean isSDL, Boolean isDVCPro)
{
    OSErr 					result = noErr;
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
    return result;
}

static OSErr setup48kAudioAtoms( QTAtomContainer container, QTAtom isocAtom, UInt32 standard)
{
        OSErr 		err;
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

static OSErr setup32kAudioAtoms( QTAtomContainer container, QTAtom isocAtom, UInt32 standard)
{
        OSErr err;
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

static OSErr setup44kAudioAtoms( QTAtomContainer container, QTAtom isocAtom, UInt32 standard)
{
        OSErr err;
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
    CFDictionaryRef	properties;
    CFStringRef		dataDesc;
    OSStatus		result;
    Boolean			done;
    
    result = IORegistryEntryCreateCFProperties(pDeviceDescription->fID,
                                               &properties, kCFAllocatorDefault, kNilOptions);

    if(result != noErr)
        return result;
    dataDesc = (CFStringRef)CFDictionaryGetValue(properties, CFSTR("FireWire Product Name"));
    if(dataDesc) {
        done = CFStringGetPascalString(dataDesc, name, 255, kCFStringEncodingMacRoman);
    }
    else {
        // Look up vendor ID in resource list
        UInt32	vendorID = pDeviceDescription->fGUID >> 40;
        Handle	h;
        UInt32	*pGuid;
        UInt32	guidCount;
        UInt32	i, index;		
        Str255	cameraName = "\pDV"; // in case we don't find anything
        SInt16 	refNum = -1, localizedRefNum = -1;
        
        BlockMoveData(cameraName, name, cameraName[0] + 1);

		// read vendor id resource and look for matching guid
        if(pDeviceDescription->fGlobals->fBundleID == 0) {
            CFBundleRef myRef;
            myRef = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.IOFWDVComponents"));
            CFBundleOpenBundleResourceFiles(myRef, &refNum, &localizedRefNum);
            pDeviceDescription->fGlobals->fBundleID = myRef;
        }
		h = Get1Resource('vnid', -20775); // jkl, get resource assignment and constant
		result = ResError();
		if (h && (result == noErr)) {
			HLock(h);
	
			// first long is number of vendor id's stored in resource
			guidCount = **((UInt32 **) h);

			pGuid = *((UInt32 **) h);
			pGuid++;
	
			for (i = 0, index = 1; i < guidCount; i++, index++) {
				if (pGuid[i] == vendorID) {
					// found device, get its name
					GetIndString(cameraName, -20775, index);
					if (cameraName[0]) {
						BlockMoveData(cameraName, name, cameraName[0] + 1);
						BlockMoveData(" DV", name + name[0] + 1, 3);
						name[0] += 3;
						break;
					}
				}	
			}
			HUnlock(h);
			ReleaseResource(h);
        }
    }
    CFRelease(properties);
    return result;
}

static OSErr postEvent(IsochComponentGlobalsPtr g, IDHDeviceID deviceID, IDHEvent event)
{
        UInt32 i;
        IDHDeviceConnectionEvent connectionEvent;
        OSErr error = noErr;

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

static void deviceMessage(void * refcon, io_service_t service,
                          natural_t messageType, void *messageArgument)
{
    //syslog(LOG_INFO,"Got message: refcon %d, service 0x%x type 0x%x arg %p\n",
    //    refcon, service, messageType, messageArgument);
    if(messageType == kIOMessageServiceIsTerminated) {
        DeviceDescriptionPtr deviceDescriptionPtr = (DeviceDescriptionPtr)refcon;
        if(deviceDescriptionPtr->deviceControlInstance) {
            DeviceControlDisableAVCTransactions(deviceDescriptionPtr->deviceControlInstance);
            DeviceControlSetDeviceConnectionID(deviceDescriptionPtr->deviceControlInstance,
                                                                                kIDHInvalidDeviceID);
        }
        if(deviceDescriptionPtr->fConnection) {            
            // Close the connection
            IOServiceClose(deviceDescriptionPtr->fConnection);
            deviceDescriptionPtr->fConnection = NULL;
        }
        if(deviceDescriptionPtr->fNotification) {
            IOObjectRelease(deviceDescriptionPtr->fNotification);
            deviceDescriptionPtr->fNotification = NULL;
        }
        if(deviceDescriptionPtr->fID) {
            IOObjectRelease(deviceDescriptionPtr->fID);
            deviceDescriptionPtr->fID = NULL;
        }
        // post a DV event to let the curious know...
        postEvent(deviceDescriptionPtr->fGlobals, (IDHDeviceID)deviceDescriptionPtr, kIDHEventDeviceRemoved);
    }
}

//====================================================================================
//
// registerDevice()
//	Register the device and install the FireWire port callback routines and bus reset proc.
//
//====================================================================================
static OSStatus registerDevice(DeviceDescriptionPtr pDeviceDescription)
{
    OSStatus err = noErr;
    return err;
}

static OSStatus findBuffers(DeviceDescription *deviceDescriptionPtr)
{
    unsigned int 	size;
    int 		nFrames;
    vm_address_t 	*buffs;
    vm_size_t		shmemSize;
    int			i;
    OSStatus		result = noErr;

    size = 0;
    nFrames = io_connect_method_scalarI_scalarO(deviceDescriptionPtr->fConnection,
                    kDVGetNumOutputFrames, NULL, 0, NULL, &size);
    if(nFrames < 0)
        return nFrames;	// Probably an error code.
    
    buffs = (vm_address_t *)NewPtr(nFrames * sizeof(vm_address_t));
    if(!buffs)
        return MemError();

    for(i=0; i<nFrames; i++) {
        result = IOConnectMapMemory(deviceDescriptionPtr->fConnection,i,mach_task_self(),
                                    &buffs[i],&shmemSize,kIOMapAnywhere);
        FailIf(result != noErr, Exit);
        // Try to make sure VM isn't confused.
        bzero((void *)buffs[i], shmemSize);
    }

    // Get status vars.
    result = IOConnectMapMemory(deviceDescriptionPtr->fConnection,kDVNumOutputFrames+4,mach_task_self(),
                                (vm_address_t *)&deviceDescriptionPtr->fSharedVars,&shmemSize,kIOMapAnywhere);

    //printf("res: 0x%x mapped vars to 0x%x: %d bytes\n", (int)result,
    //    (int)deviceDescriptionPtr->fSharedVars, shmemSize);
    FailIf(result != noErr, Exit);

    deviceDescriptionPtr->fNumOutputFrames = nFrames;
    deviceDescriptionPtr->bufMem = buffs;

    deviceDescriptionPtr->fBufSize = (deviceDescriptionPtr->standard == ntscIn)?
                                    kNTSCCompressedBufferSize:kPALCompressedBufferSize;
    if((deviceDescriptionPtr->fOutputMode & 0x7c) == 4)
        deviceDescriptionPtr->fBufSize /= 2;	// SDL

Exit:
    return result;
}

static OSStatus enableRead(DeviceDescription *deviceDescriptionPtr)
{
    unsigned int 	size;
    OSStatus		result;

    findBuffers(deviceDescriptionPtr);
    size = 0;
    result = io_connect_method_scalarI_scalarO(deviceDescriptionPtr->fConnection,
                                               kDVReadStart, NULL, 0, NULL, &size);
    if(result == noErr) {
        deviceDescriptionPtr->fOldDrop = deviceDescriptionPtr->fSharedVars->fDroppedFrames;
    }
    return result;
}

static OSStatus disableRead(DeviceDescription *deviceDescriptionPtr)
{
    unsigned int 	size;
    OSStatus		result;

    size = 0;
    result = io_connect_method_scalarI_scalarO(deviceDescriptionPtr->fConnection,
                                               kDVReadStop, NULL, 0, NULL, &size);

    return result;
}

static OSStatus enableWrite(DeviceDescription *deviceDescriptionPtr)
{
    unsigned int 	size;
    OSStatus		result;
    vm_size_t		shmemSize;
    int				mode;
    
    findBuffers(deviceDescriptionPtr);
        
    size = 0;
    mode = deviceDescriptionPtr->fOutputMode;
    result = io_connect_method_scalarI_scalarO(deviceDescriptionPtr->fConnection,
                                            kDVSetWriteSignalMode, &mode, 1, NULL, &size);
    {
        int i;
        
        // Map the shared write data structure
        result = IOConnectMapMemory(deviceDescriptionPtr->fConnection,kDVNumOutputFrames+5,mach_task_self(),
                                    (vm_address_t *)&deviceDescriptionPtr->fSharedWriteVars,&shmemSize,kIOMapAnywhere);
        FailIf(result != noErr, Exit);
    
        deviceDescriptionPtr->fOldWriteTimeStamps =
            (UInt32 *)NewPtr(deviceDescriptionPtr->fSharedWriteVars->fNumGroups*sizeof(UInt32));
        for(i=0; i<deviceDescriptionPtr->fSharedWriteVars->fNumGroups; i++) {
            deviceDescriptionPtr->fOldWriteTimeStamps[i] = deviceDescriptionPtr->fSharedWriteVars->fGroupData[i].fTimeStamp;
        }
        deviceDescriptionPtr->fDCLReadPos = 0;
        deviceDescriptionPtr->fBufWritePos = 0;
        deviceDescriptionPtr->fFrameBufferPos = 0;
        deviceDescriptionPtr->fDCLSavedWritePos = NULL;
        deviceDescriptionPtr->fDCLSavedPacketNum = 0;
            
        // Map the shared DCL program data buffer
        result = IOConnectMapMemory(deviceDescriptionPtr->fConnection,kDVNumOutputFrames+6,mach_task_self(),
                                    &deviceDescriptionPtr->fDCLBuffer,&shmemSize,kIOMapAnywhere);
        FailIf(result != noErr, Exit);
    }

    deviceDescriptionPtr->fWaitingStartWrite = 1;
    deviceDescriptionPtr->fSharedVars->fReader = deviceDescriptionPtr->fSharedVars->fWriter = 0;
    deviceDescriptionPtr->fOldDrop = deviceDescriptionPtr->fSharedVars->fDroppedFrames;
    deviceDescriptionPtr->fOldRead = deviceDescriptionPtr->fSharedVars->fReader;
    
Exit:
    return result;
}

static OSStatus disableWrite(DeviceDescription *deviceDescriptionPtr)
{
    unsigned int 	size;
    OSStatus		result;

    size = 0;
    result = io_connect_method_scalarI_scalarO(deviceDescriptionPtr->fConnection,
                                               kDVWriteStop, NULL, 0, NULL, &size);

    if(deviceDescriptionPtr->fOldWriteTimeStamps) {
        DisposePtr((Ptr)deviceDescriptionPtr->fOldWriteTimeStamps);
        deviceDescriptionPtr->fOldWriteTimeStamps = NULL;
    }
    return result;
}

static OSStatus sendMsg(IsochComponentInstancePtr ih, UInt32 request, void *params)
{
    IsochComponentGlobalsPtr g = &globals;
    SendMsg msg;
    bzero( &msg, sizeof(msg));

    msg.msgHdr.msgh_remote_port	= g->fNotifyMachPort;
    msg.msgHdr.msgh_bits = MACH_MSGH_BITS(
                                            MACH_MSG_TYPE_COPY_SEND,
                                            MACH_MSG_TYPE_COPY_SEND );
    msg.msgHdr.msgh_size 	= sizeof(msg);
    msg.msgHdr.msgh_id		= kDVRequestID;
    msg.dvRequest.fRequest = request;
    msg.dvRequest.fInstance = ih;
    msg.dvRequest.fParams = params;

    return mach_msg(&msg.msgHdr, MACH_SEND_MSG,
                msg.msgHdr.msgh_size, 0, MACH_PORT_NULL, 0, MACH_PORT_NULL);

}

static OSStatus doAVCTransaction(DeviceDescriptionPtr deviceDescriptionPtr,
                DVCTransactionParams* inTransaction)
{
    IOReturn result = kIDHErrDeviceNotConfigured;
    int size = inTransaction->responseBufferSize;
    if(deviceDescriptionPtr->fConnected) {
        result = io_connect_method_structureI_structureO(
        deviceDescriptionPtr->fConnection, kAVCCommand,
        inTransaction->commandBufferPtr, inTransaction->commandLength,
        inTransaction->responseBufferPtr, &size);
    }
    if(noErr == result)
        inTransaction->responseBufferSize = size;
    return result;
}


static OSStatus addDevice(IsochComponentGlobalsPtr g, io_object_t obj, DeviceDescriptionPtr *deviceIDPtr)
{
    OSErr 			result = noErr;
    QTAtom 			deviceAtom, isocAtom;
    DeviceDescriptionPtr	deviceDescriptionPtr;
    IDHDeviceStatus		deviceStatus;
    UInt32			standard;
    Boolean			hasSDL, isDVCPro;
    UInt32			devIndex;
    UInt64			newGUID;
    ComponentDescription	clkDesc;

    CFDictionaryRef		properties;
    CFNumberRef			dataDesc;
    CFBooleanRef		ntscDesc;
    CFBooleanRef		sdlDesc;
    CFBooleanRef		dvcProDesc;
    
    RecordEventLogger( 'isoc', 'addv', (int)g, obj);
    //syslog(LOG_INFO,"addDevice: service 0x%x\n", obj);

    ++g->seed;
    result = IORegistryEntryCreateCFProperties(obj, &properties, kCFAllocatorDefault, kNilOptions);

    if(result != noErr)
        return result;
    
    dataDesc = (CFNumberRef)CFDictionaryGetValue(properties, CFSTR("GUID"));
    CFNumberGetValue(dataDesc, kCFNumberSInt64Type, &newGUID);

   // fprintf(stderr,"Found device GUID: 0x%x%08x\n",
   //         (int)(newGUID>>32), (int)(newGUID & 0xffffffff));

    ntscDesc = (CFBooleanRef)CFDictionaryGetValue(properties, CFSTR("NTSC"));
    standard = ntscIn;
    if(ntscDesc && ! CFBooleanGetValue(ntscDesc)) {
        standard = palIn;
    }
    
    sdlDesc = (CFBooleanRef)CFDictionaryGetValue(properties, CFSTR("SDL"));
    hasSDL = false;
    if(sdlDesc && CFBooleanGetValue(sdlDesc)) {
        hasSDL = true;
    }

    dvcProDesc = (CFBooleanRef)CFDictionaryGetValue(properties, CFSTR("DVCPro"));
    isDVCPro = false;
    if(dvcProDesc && CFBooleanGetValue(dvcProDesc)) {
        isDVCPro = true;
    }
    CFRelease(properties);

    // look for existing device
    for( devIndex=0; devIndex < g->nDevices; ++devIndex) {
        deviceDescriptionPtr = &g->deviceDescription[devIndex];
        if (newGUID == deviceDescriptionPtr->fGUID) {
            deviceDescriptionPtr->fID = obj;
            *deviceIDPtr = deviceDescriptionPtr;
            //syslog(LOG_INFO,"Found device, index %d\n", devIndex);
            
            // Get device back to old state
            if(deviceDescriptionPtr->fConnected) {
                result = IOServiceOpen(deviceDescriptionPtr->fID, mach_task_self(), 11,
                                    &deviceDescriptionPtr->fConnection);
                if (result != kIOReturnSuccess) {
                    return result;
                }
                
#if 0
                // If not real-time, Set buffer size, 10 frames seems enough.
                {
                    int numFrames;
                    unsigned int size;

                    numFrames = 10;
                    size = 0;
                    io_connect_method_scalarI_scalarO(deviceDescriptionPtr->fConnection,
                                    kDVSetNumOutputFrames, &numFrames, 1, NULL, &size);
                }
#endif                                
                if(deviceDescriptionPtr->readLocks) {
                    enableRead(deviceDescriptionPtr);
                }
                if(deviceDescriptionPtr->writeLocks) {
                    enableWrite(deviceDescriptionPtr);
                }
            }
        
            return noErr;	// All Done!
        }
    }

    do {
        Str255 cameraName;

        deviceDescriptionPtr = &g->deviceDescription[g->nDevices];
        deviceDescriptionPtr->fGlobals = g;
        deviceDescriptionPtr->fID = obj;
#ifdef DRAWRINGBUFFERGRAPH
		deviceDescriptionPtr->fScreenBaseAddr = 0;
#endif
        result = registerDevice(deviceDescriptionPtr);
        if(noErr != result)
            break;

        RecordEventLogger( 'isoc', 'updt', 'add ', 'reg ');

        // add its description
        g->nDevices++;

        //*deviceID = ih->nDevices;

        deviceDescriptionPtr->fGUID = newGUID;

        // add the device standard (PAL, NTSC)
        deviceDescriptionPtr->standard 				= standard;
        deviceDescriptionPtr->active 				= true;
        deviceDescriptionPtr->fDVFormats			= 1 << kIDHDV_SD;		// Assume everything can do DV-SD
        if(hasSDL)
            deviceDescriptionPtr->fDVFormats		|= 1 << kIDHDV_SDL;
        if(isDVCPro)
            deviceDescriptionPtr->fDVFormats		|= 1 << kIDHDVCPro_25;
        
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
                                0, 0, sizeof(newGUID), &newGUID, nil);
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
        deviceStatus.inputStandard		= standard;
        deviceStatus.inputFormat		= kIDHDV_SD;

        deviceStatus.outputFormats 		= deviceDescriptionPtr->fDVFormats;
        deviceStatus.deviceActive 		= false;

        result = QTInsertChild( deviceDescriptionPtr->deviceContainer, deviceAtom, kDVDeviceInfo,
                                0, 0, sizeof( IDHDeviceStatus), &deviceStatus, nil);
        if( result != noErr)break;

        // add isoch descriptions to structure
        result = QTInsertChild( deviceDescriptionPtr->deviceContainer, deviceAtom, kIDHIsochServiceAtomType,
                                0, 0, 0, nil, &isocAtom);
        if( result != noErr)break;

        // add the configs to the isoc atom
        result = setupVideoAtoms( deviceDescriptionPtr->deviceContainer, isocAtom, standard, false, isDVCPro);
        if( result != noErr)break;
        if(hasSDL) {
            result = setupVideoAtoms( deviceDescriptionPtr->deviceContainer, isocAtom, standard, true, isDVCPro);
            if( result != noErr)break;
        }
        
        if( standard == ntscIn)
        {
                result = setup48kAudioAtoms( deviceDescriptionPtr->deviceContainer, isocAtom, standard);
                if( result != noErr)break;

                result = setup32kAudioAtoms( deviceDescriptionPtr->deviceContainer, isocAtom, standard);
                if( result != noErr)break;
        }
        else	// PAL audio
        {
                result = setup44kAudioAtoms( deviceDescriptionPtr->deviceContainer, isocAtom, standard);
                if( result != noErr)break;
        }
        *deviceIDPtr = deviceDescriptionPtr;
        return noErr;
    } while(false);

    return result;
}

static void deviceArrived(void *refcon, io_iterator_t iterator )
{
    io_object_t 		obj;
    IsochComponentGlobalsPtr 	ih = (IsochComponentGlobalsPtr)refcon;
    
    while(obj = IOIteratorNext(iterator)) {
        OSStatus result;
        DeviceDescriptionPtr deviceDescriptionPtr;
        result = addDevice(ih, obj, &deviceDescriptionPtr);
        if(result == noErr) {
            postEvent(ih, (IDHDeviceID)deviceDescriptionPtr, kIDHEventDeviceAdded);
            // Request notification of messages
            IOServiceAddInterestNotification(ih->fNotifyPort, deviceDescriptionPtr->fID,
                                kIOGeneralInterest, deviceMessage, deviceDescriptionPtr,
                                &deviceDescriptionPtr->fNotification);
        }
    }
}

#define TIMING 0

/* raw read of the timebase register */
void clock_get_uptime( register AbsoluteTime *result)
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

#if TIMING
#define WATCHDOG 1
#if WATCHDOG
int RTThreadRunning;
AbsoluteTime startTime;
pthread_t	watchThread;			// Check for work thread running too long

static void watchdog_thread_start(IsochComponentGlobalsPtr g)
{
    double   divisor;
    unsigned int delta;
    unsigned int abs_to_ns_num;
    unsigned int abs_to_ns_denom;
    unsigned int proc_to_abs_num;
    unsigned int proc_to_abs_denom;
    unsigned long long start, stop;
    int elapsed_msecs;

    (void)MKGetTimeBaseInfo (&delta, &abs_to_ns_num, &abs_to_ns_denom,
            &proc_to_abs_num,  &proc_to_abs_denom);

    divisor = ((double)abs_to_ns_denom / (double)abs_to_ns_num) * 1000000;
    while (1) {
        if(RTThreadRunning) {
            AbsoluteTime now;
            clock_get_uptime(&now);
            start = (((unsigned long long)startTime.hi) << 32) |
                    (unsigned long long)((unsigned int)(startTime.lo));
    
            stop = (((unsigned long long)now.hi) << 32) |
                    (unsigned long long)((unsigned int)(now.lo));
    
            elapsed_msecs = (int)(((double)(stop - start)) / divisor);
            if(elapsed_msecs>100) {
                syslog(LOG_INFO, "Thread running for %d mSec\n", elapsed_msecs);
                *(int *)0 = 0xdeadbeef;	// Find out why!
            }
        }
        usleep(30000);
    }
}
#endif

#endif

#if 0
static void incrementWriteBuffer(DeviceDescription *deviceDescriptionPtr)
{
    deviceDescriptionPtr->fSharedVars->fWriter += 1;   // release buffer
    if(deviceDescriptionPtr->fWaitingStartWrite && deviceDescriptionPtr->fSharedVars->fWriter == 4) {
        unsigned int size = 0;
        io_connect_method_scalarI_scalarO(deviceDescriptionPtr->fConnection,
                                               kDVWriteStart, NULL, 0, NULL, &size);
        deviceDescriptionPtr->fWaitingStartWrite = 0;
    }
}
#endif

static void silenceFrame(DeviceDescription *deviceDescriptionPtr, UInt8* frame)
{
    UInt32    i,j,k,n;
    UInt8    *tPtr;
    
    // Get DSF flag in byte 3 of header (Blue Book p. 113)
    tPtr = frame;
    if ((tPtr[3] &= 0x80) == 0)
        n=10;                            // ntsc            
    else
        n=12;                            // pal
    
    if(deviceDescriptionPtr->fOutputMode & 4)	// SDL
        n /= 2;
    // Yet another attempt ...
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

static UInt8 *getNextFrame(DeviceDescription *deviceDescriptionPtr, IsochComponentInstancePtr client, int slack, int waiting)
{
    ComponentResult err = noErr;
    IDHParameterBlock *pb;
    
    pb = client->fActiveHead;
    if(pb && !waiting) {
        if(deviceDescriptionPtr->fWaitingStartWrite) {
            IOReturn res;
            unsigned int size = 0;
            res = io_connect_method_scalarI_scalarO(deviceDescriptionPtr->fConnection,
                                                kDVWriteStart, NULL, 0, NULL, &size);
            if(res == kIOReturnNoResources || res == kIOReturnNoSpace) {
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
            deviceDescriptionPtr->fSharedVars->fDroppedFrames++;
            dropMsg(deviceDescriptionPtr);
            silenceFrame(deviceDescriptionPtr, (UInt8 *)deviceDescriptionPtr->bufMem[0]);
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
        dclPtr = (UInt8 *)deviceDescriptionPtr->fDCLBuffer + deviceDescriptionPtr->fSharedWriteVars->fGroupData[group].fDataOffset;
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

static void processWrites(DeviceDescription *deviceDescriptionPtr)
{
    int i;
    int changed;
    int done;
    int pos;
    int emptyBlocks;
    int numGroups = deviceDescriptionPtr->fSharedWriteVars->fNumGroups;
    IsochComponentInstancePtr client;

    changed = 0;
    
    client = deviceDescriptionPtr->fOpenClients[0];

    // If we failed to start writing last time through, reset to try again
    if(deviceDescriptionPtr->fWaitingStartWrite == 2) {
        for(i=0; i<deviceDescriptionPtr->fSharedWriteVars->fNumGroups; i++) {
            deviceDescriptionPtr->fOldWriteTimeStamps[i] = deviceDescriptionPtr->fSharedWriteVars->fGroupData[i].fTimeStamp;
        }
        deviceDescriptionPtr->fDCLReadPos = 0;
        deviceDescriptionPtr->fBufWritePos = 0;
        deviceDescriptionPtr->fFrameBufferPos = 0;
        deviceDescriptionPtr->fDCLSavedWritePos = NULL;
        deviceDescriptionPtr->fDCLSavedPacketNum = 0;
        deviceDescriptionPtr->fWaitingStartWrite = 1;
    }
    // First find out where the hardware is
    pos = deviceDescriptionPtr->fDCLReadPos;
    for(i=pos; i< pos+numGroups; i++) {
        int mod = i % numGroups;
        if(deviceDescriptionPtr->fOldWriteTimeStamps[mod] != 
            deviceDescriptionPtr->fSharedWriteVars->fGroupData[mod].fTimeStamp) {
            deviceDescriptionPtr->fOldWriteTimeStamps[mod] = deviceDescriptionPtr->fSharedWriteVars->fGroupData[mod].fTimeStamp;
            deviceDescriptionPtr->fDCLReadPos = i;
        }
    }
    
    // We try to fill ~30ms of the DCL buffer
    emptyBlocks = numGroups-(deviceDescriptionPtr->fBufWritePos - deviceDescriptionPtr->fDCLReadPos);
    if(emptyBlocks >= numGroups && !deviceDescriptionPtr->fWaitingStartWrite) {
        syslog(LOG_INFO, "DCL wrap!!: %d\n", emptyBlocks);
    }

    if(emptyBlocks>3)
        emptyBlocks = 3;
        
    done = 0;
    for(i=0; i<emptyBlocks; i++) {
        int ok;
        ok = 
            fillDCLGroup(deviceDescriptionPtr, client, (deviceDescriptionPtr->fBufWritePos + i) % numGroups,
                                        deviceDescriptionPtr->fBufWritePos+done - deviceDescriptionPtr->fDCLReadPos);
        if(!ok)
            break;
        done += ok;
    }
    deviceDescriptionPtr->fBufWritePos += done;
}

static int processReads(DeviceDescription *deviceDescriptionPtr)
{
    int doneFrames = 0;
    int frameIndex;
    IsochComponentInstancePtr client;
    int i;
    frameIndex = deviceDescriptionPtr->fSharedVars->fReader % deviceDescriptionPtr->fNumOutputFrames;
    if(deviceDescriptionPtr->fSharedVars->fReader + 1 < 
                            deviceDescriptionPtr->fSharedVars->fWriter) {
        UInt8 *buffer = (UInt8 *)deviceDescriptionPtr->bufMem[frameIndex];
        UInt32 frameSize = deviceDescriptionPtr->fSharedVars->fFrameSize[frameIndex];
         // Lock buffer while we process stuff
        deviceDescriptionPtr->fBufferLocks[frameIndex]++;
        if(deviceDescriptionPtr->fOldDrop < deviceDescriptionPtr->fSharedVars->fDroppedFrames) {
            dropMsg(deviceDescriptionPtr);
        }
        for(i=0; i<kMaxDeviceClients; i++) {
            client = deviceDescriptionPtr->fOpenClients[i];
            if(client) {
                if(client->fClientIndex != i) {
                    syslog(LOG_INFO, "processReads: client %p of %p index is %d not %d\n",
                        client, deviceDescriptionPtr, client->fClientIndex, i);
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
                    if(client->fHead == NULL)
                        client->fTail = NULL;
                    pb->result = noErr;
                    
                    //syslog(LOG_INFO, "read callback, buffer = %p\n", pb->buffer);
                    //syslog(LOG_INFO, "pb %p = req %d actual %d\n",
                    //    pb, pb->requestedCount, pb->actualCount);

                    err = pb->completionProc((IDHGenericEvent *)pb, pb->refCon);
                    if(err != noErr) {
                        syslog(LOG_INFO, "read callback for pb %p returned error %d\n", pb, err);
                    }
                    doneFrames++;
                }
            }
        }
        // unlock buffer
        deviceDescriptionPtr->fBufferLocks[frameIndex]--;
        if(doneFrames && deviceDescriptionPtr->fBufferLocks[frameIndex] == 0)
            deviceDescriptionPtr->fSharedVars->fReader += 1;   // release buffer
    }

    return doneFrames;
}

static void queueWrite(IsochComponentInstancePtr client, IDHParameterBlock *pb)
{
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
}

void processOpen(IsochComponentInstancePtr ih, DeviceDescription *deviceDescriptionPtr, UInt32 permissions)
{
    ComponentResult result = noErr;
    int i;
    
    FailWithAction( deviceDescriptionPtr->active == false, result = kIDHErrDeviceDisconnected, Exit);

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
    ih->fSyncResult = result;
    signalSync(&ih->fSyncRequest, 1);
    
}

void processClose(IsochComponentInstancePtr ih, DeviceDescription *deviceDescriptionPtr)
{
    if( ih->permissions & kIDHOpenForReadTransactions)	// tear down read
    {
        if( --deviceDescriptionPtr->readLocks <= 0)	// is device completely freed by clients?
        {
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
    ih->fSyncResult = noErr;
    signalSync(&ih->fSyncRequest, 1);
}

static void processCancelPendingIO(IsochComponentInstancePtr client)
{
    while(client->fHead) {
        IDHParameterBlock *pb = client->fHead;
        //syslog(LOG_INFO, "Cancelling IO %p, buffer = %p\n", pb, pb->buffer);

        client->fHead = (IDHParameterBlock *)pb->reserved1;
        pb->reserved1 = NULL;
    }
    client->fTail = NULL;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
static void DVIsoch_thread_start(IsochComponentGlobalsPtr g)
{

    ReceiveMsg msg;
    kern_return_t err;
    int delay;

#if TIMING
    double   divisor;
    int worst_sched = 0;
    int worst_callback = 0;
    int worst_msg = 0;
    {
        unsigned int delta;
        unsigned int abs_to_ns_num;
        unsigned int abs_to_ns_denom;
        unsigned int proc_to_abs_num;
        unsigned int proc_to_abs_denom;
    
        (void)MKGetTimeBaseInfo (&delta, &abs_to_ns_num, &abs_to_ns_denom,
                &proc_to_abs_num,  &proc_to_abs_denom);
    
        divisor = ((double)abs_to_ns_denom / (double)abs_to_ns_num) * 1000000;
    }
#endif
    deviceArrived(g, g->fMatchEnumer);
    signalSync(&g->fGlobalsState, 1);


    delay = 15;	// 1/2 a frame
    while(true) {
        int dev;
 #if TIMING
        AbsoluteTime timestamp1;
        AbsoluteTime timestamp2;
        unsigned long long start, stop;
        int elapsed_msecs;
#endif
        DeviceDescription *deviceDescriptionPtr;
       // deviceDescriptionPtr = (DeviceDescription *)ih->deviceID;
       
        for(dev = 0; dev<g->nDevices; dev++) {
            deviceDescriptionPtr = &g->deviceDescription[dev];
    
            // Process I/O queues
            if(deviceDescriptionPtr) {
#if TIMING
                clock_get_uptime(&timestamp1);
#endif        
                if(deviceDescriptionPtr->writeLocks)
                    processWrites(deviceDescriptionPtr);
                if(deviceDescriptionPtr->readLocks)
                    processReads(deviceDescriptionPtr);
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
#if TIMING
        clock_get_uptime(&timestamp1);
#if WATCHDOG
        RTThreadRunning=0;
#endif
#endif        
        err = mach_msg(&msg.msgHdr, MACH_RCV_MSG | MACH_RCV_TIMEOUT,
        //err = mach_msg(&msg.msgHdr, MACH_RCV_MSG | MACH_RCV_TIMEOUT | MACH_RCV_LARGE,
                       0, sizeof(msg), g->fNotifyMachPort, delay, MACH_PORT_NULL);

#if TIMING
#if WATCHDOG
        clock_get_uptime(&startTime);
        RTThreadRunning=1;
#endif

        clock_get_uptime(&timestamp2);
		start = (((unsigned long long)timestamp1.hi) << 32) |
		          (unsigned long long)((unsigned int)(timestamp1.lo));

		stop = (((unsigned long long)timestamp2.hi) << 32) |
		         (unsigned long long)((unsigned int)(timestamp2.lo));

		elapsed_msecs = (int)(((double)(stop - start)) / divisor);
        if(elapsed_msecs > worst_sched) {
            pKGSysCall_Insert (kIOFWDVTrace, 'schd', elapsed_msecs, 0, 0, 0, 0);
            syslog(LOG_INFO, "schedule delay %d mSec\n",
                    elapsed_msecs);
            worst_sched = elapsed_msecs;
        }
#endif        
        if(err == MACH_MSG_SUCCESS) {
            IsochComponentInstancePtr client;
            switch (msg.msgHdr.msgh_id) {
            case kOSNotificationMessageID:
                IODispatchCalloutFromMessage(NULL, &msg.msgHdr, g->fNotifyPort);
                break;
            case kDVRequestID:
                client = msg.body.dvRequest.fInstance;
                deviceDescriptionPtr = (DeviceDescriptionPtr)client->deviceID;
                switch(msg.body.dvRequest.fRequest) {
                    case kComponentCloseSelect:
                        pthread_exit(NULL);
                        break;

                    case kIDHReadSelect:
                    {
                        IDHParameterBlock *pbNew = (IDHParameterBlock *)msg.body.dvRequest.fParams;
                        if(client) {
                            if(client->fTail == NULL)
                                client->fHead = pbNew;
                            else
                                client->fTail->reserved1 = (UInt32)pbNew;
                            client->fTail = pbNew;
                        }
                        pbNew->reserved1 = 0;
                        break;
                    }
                    case kIDHWriteSelect:
                    {
                        IDHParameterBlock *pbNew = (IDHParameterBlock *)msg.body.dvRequest.fParams;
                        if(client) {
                            queueWrite(client, pbNew);
                        }
                        break;
                    }
                    case kIDHCancelPendingIOSelect:
                        if(client) {
                            processCancelPendingIO(client);
                        }
                        client->fSyncResult = noErr;
                        signalSync(&client->fSyncRequest, 1);
                        break;
                    case kIDHCloseDeviceSelect:
                        processClose(client, deviceDescriptionPtr);
                        break;
                        
                    case kIDHOpenDeviceSelect:
                        processOpen(client, deviceDescriptionPtr, (UInt32)msg.body.dvRequest.fParams);
                        break;
                        
                    default:
                        printf("DVIsoch : Unknown DVRequest %ld, params %p\n", msg.body.dvRequest.fRequest, msg.body.dvRequest.fParams);
                       
                }
                break;
            default:
                printf("DVIsoch : Unexpected msg id %d\n", msg.msgHdr.msgh_id);

            }
#if TIMING
            clock_get_uptime(&timestamp1);
            start = (((unsigned long long)timestamp2.hi) << 32) |
                    (unsigned long long)((unsigned int)(timestamp2.lo));
    
            stop = (((unsigned long long)timestamp1.hi) << 32) |
                    (unsigned long long)((unsigned int)(timestamp1.lo));
    
            elapsed_msecs = (int)(((double)(stop - start)) / divisor);
            if(elapsed_msecs > worst_msg) {
                pKGSysCall_Insert (kIOFWDVTrace, 'msg ', elapsed_msecs, 0, 0, 0, 0);
                syslog(LOG_INFO, "message process delay %d mSec for message %d (%d)\n",
                        elapsed_msecs, msg.msgHdr.msgh_id, msg.body.dvRequest.fRequest);
                worst_msg = elapsed_msecs;
            }
#endif        
        }
        else if(err != MACH_RCV_TIMED_OUT) {
            printf("DVIsoch : Receive err 0x%x, size %d(max %ld)\n", err, msg.msgHdr.msgh_size, sizeof(msg));
        }
    }

}
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static pascal ComponentResult
FWDVComponentOpen(IsochComponentInstancePtr storage, ComponentInstance self)
{
    kern_return_t err = noErr;
    IsochComponentGlobalsPtr g = &globals;
    RecordEventLogger( 'isoc', 'open', 0, 0);
    if( nil == (storage = (IsochComponentInstancePtr)NewPtrClear(sizeof(IsochComponentInstance))))
        return(MemError());

    RecordEventLogger( 'isoc', 'ope2', (int)storage, 0);

    SetComponentInstanceStorage(self, (Handle) storage);

    // One-time initialization of globals
    if (!g->fMasterDevicePort) {
        policy_base_data_t thePolicyBase;
        host_priority_info_data_t	priorityDefaults;
        int count;
        
        err = IOMasterPort(bootstrap_port, &g->fMasterDevicePort);
        if (err != KERN_SUCCESS) {
            RecordEventLogger( 'isoc', 'opep', (int)err, 0);
            return err;
        }
    
        g->fNotifyPort = IONotificationPortCreate(g->fMasterDevicePort);
        g->fNotifyMachPort = IONotificationPortGetMachPort(g->fNotifyPort);
        err = IOServiceAddMatchingNotification( g->fNotifyPort,
                kIOMatchedNotification, IOServiceMatching( kDVKernelDriverName ),
                deviceArrived, g, &g->fMatchEnumer );
        if (err != kIOReturnSuccess) {
            return err;
        }
    
        err = mach_port_insert_right(mach_task_self(), g->fNotifyMachPort, g->fNotifyMachPort,
                                    MACH_MSG_TYPE_MAKE_SEND);
        FailWithVal( err != noErr, Exit, err);
    
        g->fGlobalsState = 0;
        err = pthread_mutex_init(&g->fGlobalsMutex, NULL);
        FailWithVal( err != noErr, Exit, err);
        err = pthread_cond_init(&g->fSyncCond, NULL);
        FailWithVal( err != noErr, Exit, err);
        
        err = pthread_attr_init(&g->fThreadAttr);
        FailWithVal( err != noErr, Exit, err);
        err = pthread_create(&g->fWorkThread, &g->fThreadAttr,
            (void *)DVIsoch_thread_start, g);
        FailWithVal( err != noErr, Exit, err);

#if 0
        // Set thread priority to default round-robin -
        count = HOST_PRIORITY_INFO_COUNT;
    
        // Get the recommended priority for running on this configuration
        err = host_info ( mach_host_self(),  HOST_PRIORITY_INFO, (host_info_t)&priorityDefaults, &count);
        FailWithVal( err != noErr, Exit, err);

        thePolicyBase.rr.base_priority = priorityDefaults.user_priority;	// = 31 in Puma
        //thePolicyBase.rr.base_priority = 63; //max allowable;
        thePolicyBase.rr.quantum = 10;		// recommended by Bohman.. to be revisited.
    
        // set this thread's current scheduling policy to be RR
        err = thread_policy (pthread_mach_thread_np(g->fWorkThread),
            POLICY_RR, (policy_base_t)&thePolicyBase.rr, POLICY_RR_BASE_COUNT, FALSE );
#endif
#if 1
        {
            double   mult;
            unsigned int delta;
            unsigned int abs_to_ns_num;
            unsigned int abs_to_ns_denom;
            unsigned int proc_to_abs_num;
            unsigned int proc_to_abs_denom;
            thread_time_constraint_policy_data_t constraints;
        
            (void)MKGetTimeBaseInfo (&delta, &abs_to_ns_num, &abs_to_ns_denom,
                    &proc_to_abs_num,  &proc_to_abs_denom);
        
            mult = ((double)abs_to_ns_denom / (double)abs_to_ns_num) * 1000000;
            constraints.period = 15*mult;
            constraints.computation = 2*mult;
            constraints.constraint = 30*mult;
            constraints.preemptible = TRUE;
            err = thread_policy_set(pthread_mach_thread_np(g->fWorkThread), THREAD_TIME_CONSTRAINT_POLICY,
                (thread_policy_t)&constraints, THREAD_TIME_CONSTRAINT_POLICY_COUNT);
#if WATCHDOG
            if(watchThread == 0) {
                err = pthread_create(&watchThread, &g->fThreadAttr,
                    (void *)watchdog_thread_start, g);
                constraints.period = 30*mult;
                constraints.computation = 1*mult;
                constraints.constraint = 30*mult;
                constraints.preemptible = TRUE;
                err = thread_policy_set(pthread_mach_thread_np(watchThread), THREAD_TIME_CONSTRAINT_POLICY,
                    (thread_policy_t)&constraints, THREAD_TIME_CONSTRAINT_POLICY_COUNT);
            }
#endif
        }
#endif
#if 0        
        // more than Window Manager, so moving windows etc. doesn't cause dropped frames.
        thePolicyBase.ts.base_priority = 72;
        //thePolicyBase.ts.base_priority = 127; // max allowable
        err = thread_policy(pthread_mach_thread_np(g->fWorkThread),
            POLICY_TIMESHARE, (policy_base_t)&thePolicyBase.ts,
            POLICY_TIMESHARE_BASE_COUNT, TRUE);
#endif    
        FailWithVal( err != noErr, Exit, err);
    }
    // Wait for work thread to finish initializing globals
    waitSync(&g->fGlobalsState);
    
    g->fNumInstances++;
    
Exit:

    return( err );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static pascal ComponentResult
FWDVComponentClose(IsochComponentInstancePtr ih, ComponentInstance self)
{
    int i;
    IsochComponentGlobalsPtr g = &globals;
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

    g->fNumInstances--;
    if(g->fNumInstances == 0) {
        if(g->fWorkThread) {
            void *status;
            int err;
            sendMsg(ih, kComponentCloseSelect, ih);
    
            err = pthread_join(g->fWorkThread, &status);
        }
    
        // Free all mach ports etc.
        for(i=0; i<g->nDevices; i++) {
            DeviceDescriptionPtr dev = &g->deviceDescription[i];
            if(dev->fConnection) {
                IOConnectRelease(dev->fConnection);
                dev->fConnection = NULL;
            }
            dev->fConnected = 0;
            if(dev->fNotification) {
                IOObjectRelease(dev->fNotification);
                dev->fNotification = NULL;
            }
            if(dev->fID) {
                IOObjectRelease(dev->fID);
                dev->fID = NULL;
            }
        }
    
        if(g->fMatchEnumer)
            IOObjectRelease(g->fMatchEnumer);
        if(g->fNotifyPort)
            IONotificationPortDestroy(g->fNotifyPort);
        g->fGlobalsState = 0;
        g->fMasterDevicePort = NULL;
    }
    
    DisposePtr((Ptr) ih);

    SetComponentInstanceStorage(self, (Handle) nil );
    RecordEventLogger( 'isoc', 'clos', 'end ', 0);
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
    OSErr 		result = noErr;
    QTAtomContainer	container = nil;
    int			devIndex;
    UInt32		version;
    IsochComponentGlobalsPtr g = &globals;

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
        for( devIndex=0; devIndex<kMaxDevicesActive; ++devIndex)
        {
            if( g->deviceDescription[devIndex].fID != 0 && g->deviceDescription[devIndex].active)
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
        
    return result;
}

static pascal ComponentResult
FWDVIDHSetDeviceConfiguration(IsochComponentInstancePtr ih,
                              const QTAtomSpec	*configID)
{
    OSErr 				result = noErr;
    QTAtomSpec			volatileAtomSpec;
    QTAtom				nameAtom;
    IDHDeviceID			previousDeviceID;
    DeviceDescription	*deviceDescriptionPtr;
    IsochComponentGlobalsPtr g = &globals;
    Boolean				isSDL;

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
    if(deviceDescriptionPtr->fConnection == NULL) {
        result = IOServiceOpen(deviceDescriptionPtr->fID, mach_task_self(), 11,
                            &deviceDescriptionPtr->fConnection);
        if (result != kIOReturnSuccess) {
            goto Exit;
        }
#if 0
        // If not real time, Set buffer size, 10 frames seems enough.
        {
            int				numFrames;
            unsigned int	size;

            numFrames = 10;
            size = 0;
            io_connect_method_scalarI_scalarO(deviceDescriptionPtr->fConnection,
                            kDVSetNumOutputFrames, &numFrames, 1, NULL, &size);
        }
#endif
        deviceDescriptionPtr->fConnected = 1;
    }
    FailWithAction( deviceDescriptionPtr->active == false, result = kIDHErrDeviceDisconnected, Exit);

    // if this client already has device control, free the previous connection
    if( previousDeviceID && previousDeviceID != ih->deviceID) {
        DeviceDescription *deviceDescriptionPtr2;

        result = findDeviceDescriptionforDevice( ih, previousDeviceID, &deviceDescriptionPtr2);
        FailWithVal( result != noErr, Exit, result);
        if(deviceDescriptionPtr2->fConnection) {
            IOConnectRelease(deviceDescriptionPtr2->fConnection);
            deviceDescriptionPtr2->fConnection = nil;
        }
        if(ih->hasDeviceControl) {
                result = closeDeviceControl( ih, deviceDescriptionPtr2);
                FailMessage( result != noErr);

                ih->hasDeviceControl = false;
        }
    }
    
    // Set output signal mode
    deviceDescriptionPtr->fOutputMode = 0;
    if(isSDL)
        deviceDescriptionPtr->fOutputMode |= 4;
    else if(deviceDescriptionPtr->fDVFormats & (1 << kIDHDVCPro_25))
        deviceDescriptionPtr->fOutputMode |= 0x78;
    if(deviceDescriptionPtr->standard != ntscIn)
        deviceDescriptionPtr->fOutputMode |= 0x80;
        
Exit:
    RecordEventLogger( 'isoc', 'set ', 'Exit', ih);
    return result;
}

static pascal ComponentResult
FWDVIDHOpenDevice(IsochComponentInstancePtr ih, UInt32 permissions)
{
    ComponentResult result = noErr;
    IsochComponentGlobalsPtr g = &globals;

    RecordEventLogger( 'open', ' dev', ih, permissions);
    
    FailWithAction( permissions == 0, result = paramErr, Exit);

    FailWithAction( ih->deviceID == nil, result = kIDHErrDeviceNotConfigured, Exit);

    if(globals.fWorkThread == pthread_self()) {
        DeviceDescription *deviceDescriptionPtr;
        result = findDeviceDescriptionforDevice( ih, ih->deviceID, &deviceDescriptionPtr);	// find the device
        FailWithVal( result != noErr, Exit, result);
        processOpen(ih, deviceDescriptionPtr, permissions);
    }
    else {
        ih->fSyncRequest = 0;
        result = sendMsg(ih, kIDHOpenDeviceSelect, (void *)permissions);
        waitSync(&ih->fSyncRequest);
    }
    result = ih->fSyncResult;
    FailWithVal( result != noErr, Exit, result);
    result = postEvent( g, ih->deviceID,
            (permissions & kIDHOpenForReadTransactions)?kIDHEventReadEnabled:kIDHEventWriteEnabled);
    FailWithVal( result != noErr, Exit, result);
#ifdef DRAWRINGBUFFERGRAPH
	SetUpBlitGlobals((DeviceDescription	*)ih->deviceID);
#endif	 

Exit:
    return result;
}

static pascal ComponentResult
FWDVIDHCloseDevice(IsochComponentInstancePtr ih)
{
    OSErr 			result = noErr;
    DeviceDescription	*deviceDescriptionPtr;
    IsochComponentGlobalsPtr g = &globals;

    RecordEventLogger( 'isoc', 'clos', ' dev', ih);

    FailWithAction( ih->deviceID == nil, result = kIDHErrDeviceNotConfigured, Exit);

    if( ih->permissions == 0)	// client has no open devices?
            goto Exit;

    result = findDeviceDescriptionforDevice( ih, ih->deviceID, &deviceDescriptionPtr);	// find the device
    FailWithVal( result != noErr, Exit, result);

    if(globals.fWorkThread == pthread_self()) {
        processClose(ih, deviceDescriptionPtr);
        result = noErr;
    }
    else {
        ih->fSyncRequest = 0;
        result = sendMsg(ih, kIDHCloseDeviceSelect, 0);
        waitSync(&ih->fSyncRequest);
        result = ih->fSyncResult;
        FailWithVal( result != noErr, Exit, result);
   }     
    result = postEvent( g, ih->deviceID,
            (ih->permissions & kIDHOpenForReadTransactions)?kIDHEventReadDisabled:kIDHEventWriteDisabled);
    FailWithVal( result != noErr, Exit, result);
        
Exit:
    ih->permissions = 0;	// make sure device is closed
    ih->fClientIndex = 0xdead;
    return result;
}

//еее USER BETTER KEEP HIS DEVICE LIST AROUND IF THIS IS TO MEAN ANYTHING
static pascal ComponentResult
FWDVIDHGetDeviceConfiguration(IsochComponentInstancePtr ih, QTAtomSpec *configID )
{
    OSErr 	result = noErr;

    RecordEventLogger( 'isoc', 'get ', 'dev ', 'conf');

    FailWithAction( configID == nil, result = paramErr, Exit);
    FailWithAction( ih->currentConfig.container == nil || ih->currentConfig.atom == nil,
            result = kIDHErrDeviceNotConfigured, Exit);

    *configID = ih->currentConfig;

Exit:
    return result;
}

static pascal ComponentResult
FWDVIDHGetDeviceStatus(IsochComponentInstancePtr ih, const QTAtomSpec *devSpec, IDHDeviceStatus *status )
{
        OSErr 			result = noErr;
        IDHDeviceID		deviceID = nil;
        QTAtom			deviceInfoAtom, deviceAtom;
        QTAtomSpec		volatileAtomSpec;
        DeviceDescription	*deviceDescriptionPtr;
        IsochComponentGlobalsPtr g = &globals;
        UInt8			inputFormat = kIDHDV_SD;
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
        {
            OSStatus err;
            DVCTransactionParams transaction;
            UInt8 out[8];
            UInt8 in[8];
            
            out[0] = 0x01; //kAVCStatusInquiryCommand
            out[1] = 0xff;	// Unit
            out[2] = 0x18; //kAVCOutputPlugSignalFormatOpcode
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
        status->version = 				0x200;
        status->physicallyConnected =	true;
        status->readEnabled = 			deviceDescriptionPtr->readLocks;
        status->writeEnabled = 			deviceDescriptionPtr->writeLocks;
        status->exclusiveAccess = 		0;//deviceDescriptionPtr->exclusiveAccess;
        status->currentBandwidth = 		0;
        status->currentChannel = 		0;

        //еее need to make this work with camera tracking
        status->deviceActive = 			deviceDescriptionPtr->active;
        status->inputStandard =			deviceDescriptionPtr->standard;
        status->inputFormat =			inputFormat;
        //status->outputFormats = 		deviceDescriptionPtr->fDVFormats;
// JKL *** what to to with this? does this mean deviceID, cameraFWClientID, or localNodeFWClientID
// Think this is for clock to set the localFWReferenceID
        status->localNodeID	= 		(PsuedoID) deviceDescriptionPtr->fID;

        result = QTSetAtomData( deviceDescriptionPtr->deviceContainer, deviceInfoAtom, sizeof( IDHDeviceStatus), status);
        FailWithVal( result != noErr, Exit, result);

Exit:
        return result;
}

static pascal ComponentResult 
FWDVIDHGetDeviceClock(IsochComponentInstancePtr ih, Component *clock )
{
    DeviceDescription	*deviceDescriptionPtr;
    OSErr 		result = noErr;

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
        OSErr 			result = noErr;
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

        FailWithAction( deviceDescriptionPtr->active == false, result = kIDHErrDeviceDisconnected, Exit);

        if( pb->completionProc == nil) // synchronous read
        {
            int frameIndex = deviceDescriptionPtr->fSharedVars->fReader % deviceDescriptionPtr->fNumOutputFrames;
            UInt8 *buffer = (UInt8 *)deviceDescriptionPtr->bufMem[frameIndex];
            UInt32 frameSize;

            // wait for writer
            // if (*devices[refNum].fReader + 1 >= *devices[refNum].fWriter) return -1;
            if( deviceDescriptionPtr->fOldDrop <
                deviceDescriptionPtr->fSharedVars->fDroppedFrames) {
                    dropMsg(deviceDescriptionPtr);
            }

            while (deviceDescriptionPtr->fSharedVars->fReader + 1 >= deviceDescriptionPtr->fSharedVars->fWriter)
            {
                usleep(15000);	// about 1/2 a frame
            }
            frameSize = deviceDescriptionPtr->fSharedVars->fFrameSize[frameIndex];
            pb->actualCount = frameSize;
            if(pb->buffer != nil) {
                // copy frame
                bcopy(buffer, pb->buffer, frameSize);
                deviceDescriptionPtr->fSharedVars->fReader += 1;   // release buffer         
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

            result = sendMsg(ih, kIDHReadSelect, pb);
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
    OSErr 		result = noErr;
    int			frameIndex;
    DeviceDescription	*deviceDescriptionPtr;

    RecordEventLogger( 'isoc', 'rele', 'ase ', 'buff');

    FailWithAction( pb == nil, result = paramErr, Exit);

    FailWithAction( ih->deviceID == nil, result = kIDHErrDeviceNotConfigured, Exit);

    result = findDeviceDescriptionforDevice( ih, ih->deviceID, &deviceDescriptionPtr);
    FailWithVal( result != noErr, Exit, result);
    frameIndex = deviceDescriptionPtr->fSharedVars->fReader % deviceDescriptionPtr->fNumOutputFrames;
    if(pb->buffer == (Ptr)deviceDescriptionPtr->bufMem[frameIndex]) {
        if(--deviceDescriptionPtr->fBufferLocks[frameIndex] == 0)
            deviceDescriptionPtr->fSharedVars->fReader += 1;   // release buffer, this isn't quite right if several buffers are held.
    }
Exit:
    return result;
}

static pascal ComponentResult
FWDVIDHCancelPendingIO(IsochComponentInstancePtr ih, IDHParameterBlock *pb)
{
    OSErr 		result = noErr;
    DeviceDescription	*deviceDescriptionPtr;

    RecordEventLogger( 'isoc', 'canc', 'elIO', 0);

    FailWithAction( pb == nil, result = paramErr, Exit);

    FailWithAction( ih->deviceID == nil, result = kIDHErrDeviceNotConfigured, Exit);

    result = findDeviceDescriptionforDevice( ih, ih->deviceID, &deviceDescriptionPtr);
    FailWithVal( result != noErr, Exit, result);

    if(globals.fWorkThread == pthread_self()) {
        processCancelPendingIO(ih);
        result = noErr;
    }
    else {
        ih->fSyncRequest = 0;
        result = sendMsg(ih, kIDHCancelPendingIOSelect, pb);
        waitSync(&ih->fSyncRequest);
        result = ih->fSyncResult;
    }
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
    OSErr 		result = noErr;
    DeviceDescription	*deviceDescriptionPtr;

    RecordEventLogger( 'isoc', 'writ', ih, pb);

    FailWithAction( pb == nil, result = paramErr, Exit);

    // check for illegal condition
    FailWithAction( pb->completionProc == nil && pb->buffer == nil, result = paramErr, Exit);

    FailWithAction( ih->deviceID == nil, result = kIDHErrDeviceNotConfigured, Exit);

    result = findDeviceDescriptionforDevice( ih, ih->deviceID, &deviceDescriptionPtr);
    FailWithVal( result != noErr, Exit, result);

    FailWithAction( deviceDescriptionPtr->active == false, result = kIDHErrDeviceDisconnected, Exit);

    if( pb->completionProc == nil) // synchronous write
    {
        pb->result = kIDHErrCompletionPending;

        result = sendMsg(ih, kIDHWriteSelect, pb);
        while (pb->result == kIDHErrCompletionPending)
        {
            usleep(15000);	// about 1/2 a frame
        }

    }
    else {
        if(globals.fWorkThread == pthread_self()) {
            queueWrite(ih, pb);
        }
        else
            result = sendMsg(ih, kIDHWriteSelect, pb);
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

    FailWithAction( deviceDescriptionPtr->active == false, result = kIDHErrDeviceDisconnected, Exit);

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
                (DeviceConnectionID) deviceDescriptionPtr->fConnection);
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
    OSErr 			result = noErr;
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
    OSErr 		result = noErr;
    DeviceDescription	*deviceDescriptionPtr;
    UInt32 		info[2];
    UInt64		cycles;
    int			size;

    FailWithAction( time == nil, result = paramErr, Exit);

    FailWithAction( ih->deviceID == nil, result = kIDHErrDeviceNotConfigured, Exit);

    result = findDeviceDescriptionforDevice( ih, ih->deviceID, &deviceDescriptionPtr);
    FailWithVal( result != noErr, Exit, result);

    FailWithAction( deviceDescriptionPtr->active == false, result = kIDHErrDeviceDisconnected, Exit);

    size = 2;
    result = io_connect_method_scalarI_scalarO(deviceDescriptionPtr->fConnection,
                                               kDVGetDeviceTime, NULL, 0, (int *)info, &size);
    
    cycles = info[0] * 8000;			// Seconds converted to cycles
    cycles += (info[1] >> 12) & 0x1fff;
    
    time->value.lo = cycles & 0xffffffff;
    time->value.hi = (cycles >> 32) & 0xffffffff;
    time->scale = 8000;
    
Exit:
    return result;
}

static pascal ComponentResult
FWDVIDHNewNotification(IsochComponentInstancePtr ihc, IDHDeviceID deviceID,
    IDHNotificationProc notificationProc, void* userData, IDHNotificationID* notificationID)
{
    UInt32	i;
    Boolean	addedClient = false;
    OSErr 	result = noErr;
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
    OSErr 		result = noErr;
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
    OSErr 		result = noErr;
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
    OSErr 		result = noErr;
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
    OSErr 		result = noErr;
    DeviceDescription	*deviceDescriptionPtr;

    RecordEventLogger( 'isoc', 'setf', 'ormt', format);
    FailWithAction( ih->deviceID == nil, result = kIDHErrDeviceNotConfigured, Exit);

    result = findDeviceDescriptionforDevice( ih, ih->deviceID, &deviceDescriptionPtr);
    FailWithVal( result != noErr, Exit, result);
    
    deviceDescriptionPtr->fOutputMode = format << 2;	// Get STYPE field into position
    if(deviceDescriptionPtr->standard != ntscIn)
        deviceDescriptionPtr->fOutputMode |= 0x80;	// PAL flag

Exit:	
    return result;	
}

static pascal ComponentResult
FWDVIDHGetFormat(IsochComponentInstancePtr ih, UInt32 *format)
{
    OSErr 		result = noErr;
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

