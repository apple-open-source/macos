#include <mach/message.h>
#include <mach/mach_error.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/iokitmig.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <syslog.h>	// Debug messages
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

#include <IOKit/DV/IOFWDVClient.h>
#include <IOKit/IOMessage.h>
#include "DVFamily.h"

#define kNTSCCompressedBufferSize	120000
#define	kPALCompressedBufferSize	144000

#define kMaxDevice 32
#define kMaxNotifications 64

typedef struct device_info_struct {
    io_connect_t fConnection;
    UInt64 fGUID;
    UInt32 fNumOutputFrames;
    UInt32 fOpens;
    UInt32 frameSize;
    vm_address_t bufMem[kDVNumOutputFrames+4];
    vm_size_t shmemSize[kDVNumOutputFrames+4];
    IOFWDVSharedVars *fSharedVars;	// Structure shared with kernel driver
    UInt8		fOutputMode;		// AVC output signal mode - NTSC/SDL etc.
    char fName[256];
} device_info;

// notification stuff
typedef struct DVNotificationEntryStruct {
	UInt32		wantedEvents;
	DVNotifyProc	notifyProc;
	void		*userRefCon;
        DVDeviceID	device;
} DVNotificationEntry, *DVNotificationEntryPtr;


typedef struct {
    mach_msg_header_t	msgHdr;
    union {
        OSNotificationHeader	notifyHeader;
        //DVRequest		dvRequest;
        UInt8			grr[72];	// Enough for IOServiceInterestContent
    } body;
    mach_msg_trailer_t	trailer;
} ReceiveMsg;

static mach_port_t fMasterDevicePort;
static IONotificationPortRef	sNotifyPort;			// Our IOKit notification port
static mach_port_t		sNotifyMachPort;		// notify port as a mach port
static io_iterator_t		sMatchEnumer;			// Iterator over matching devices
static io_iterator_t		sTermEnumer;			// Iterator over terminated devices

static UInt32 fNumDevices, fNumAlive;
static device_info devices[kMaxDevice];
static DVNotificationEntry sNotifications[kMaxNotifications];
static int inited = 0;

static int exec_cmd(UInt32 cmd, UInt32 dev)
{   
    int err; 
    unsigned int size = 0;
        
    err = io_connect_method_scalarI_scalarO(devices[dev].fConnection, cmd, NULL, 0,
        NULL, &size);
    return err;
}    

static int mapframes(int dev)
{
    int 			i;
    vm_size_t		sharedSize;
    kern_return_t 	err;

    // map frame buffers
    for (i = 0 ; i < devices[dev].fNumOutputFrames ; i++) {
        err = IOConnectMapMemory(devices[dev].fConnection,i,mach_task_self(),
            &devices[dev].bufMem[i],&devices[dev].shmemSize[i],kIOMapAnywhere);
        //syslog(LOG_INFO, "Mapped %d to 0x%x\n", i, devices[dev].bufMem[i]);
        if(err == kIOReturnSuccess)
            bzero((void *)devices[dev].bufMem[i], devices[dev].shmemSize[i]);
    }

    // Map status struct
    err = IOConnectMapMemory(devices[dev].fConnection,kDVNumOutputFrames+4,mach_task_self(),
                                (vm_address_t *)&devices[dev].fSharedVars,&sharedSize,kIOMapAnywhere);
    return err;
}

static void unmapframes(int dev)
{
    int i;
    kern_return_t err;

// unmapping is an insta-panic with not much evidence left in the wreckage, let's just leak instead.
return;

    // map frame buffers
    for (i = 0 ; i < kDVNumOutputFrames + 4 ; i++) {
  //      printf("Unmapping %d-0x%x\n", i, devices[dev].bufMem[i]);
        if(devices[dev].bufMem[i]) {
            err = IOConnectUnmapMemory(devices[dev].fConnection,i,mach_task_self(), NULL);
            //devices[dev].bufMem[i]);
     //       printf("err 0x%x\n", err);
            devices[dev].bufMem[i] = NULL;
        }
    }

    devices[dev].fSharedVars = NULL;    
}

static void postEvent( DVEventHeaderPtr event )
{
    DVNotificationEntryPtr note;
    int i;
    for(i=0; i<kMaxNotifications; i++) {
        note = &sNotifications[i];
        if(note->notifyProc != NULL && (note->wantedEvents & event->theEvent) &&
            (note->device == kEveryDVDeviceID || note->device == event->deviceID)) {
            event->notifID = (DVNotificationID)(i+1);
            note->notifyProc((DVEventRecordPtr)event, note->userRefCon);
        }
    }
}

static void deviceArrived(void *refcon, io_iterator_t iterator )
{
    io_object_t obj;
    //syslog(LOG_INFO,"deviceArrived(0x%x, 0x%x)\n", refcon, iterator);
    while(obj = IOIteratorNext(iterator)) {
        CFMutableDictionaryRef properties;
        CFNumberRef dataDesc;
        CFStringRef strDesc;
        kern_return_t err;
        UInt64 GUID;
        int refound = 0;
        int device;
 
        // syslog(LOG_INFO, "object 0x%x arrived!\n", obj);
        err = IORegistryEntryCreateCFProperties(obj, &properties, kCFAllocatorDefault, kNilOptions);

        dataDesc = (CFNumberRef)CFDictionaryGetValue(properties, CFSTR("GUID"));
        CFNumberGetValue(dataDesc, kCFNumberSInt64Type, &GUID);
        for(device=1; device<kMaxDevice; device++) {
            if(GUID == devices[device].fGUID) {
                refound = 1;
                break;
            }
        }
        if(!refound) {
            device = fNumDevices+1;
            strDesc = (CFStringRef)CFDictionaryGetValue(properties, CFSTR("FireWire Product Name"));
            if(strDesc) {
                devices[device].fName[0] = 0;
                CFStringGetCString(strDesc, devices[device].fName,
                    sizeof(devices[device].fName), kCFStringEncodingMacRoman);
            }
        }
        CFRelease(properties);
        if ((err = IOServiceOpen(obj, mach_task_self(), 11,
             &devices[device].fConnection)) != kIOReturnSuccess) {
             fprintf(stderr,"DVFamily : IOServiceOpen failed: 0x%x\n", err);
             continue; 
        }
        devices[device].fGUID = GUID;
        devices[device].fOpens = 0;
        fNumAlive++;
        if(!refound)
            fNumDevices++;
        //syslog(LOG_INFO, "Found a device, GUID: 0x%x%08x name: %s\n",
        //        (UInt32)(GUID>>32), (UInt32)(GUID & 0xffffffff), devices[device].fName);
            
        {
            // post a DV event to let the curious know...
            DVConnectionEvent theEvent;
            theEvent.eventHeader.deviceID	= (DVDeviceID) device;
            theEvent.eventHeader.theEvent 	= kDVDeviceAdded;
            postEvent( &theEvent.eventHeader );
        }
    }
}

static void deviceRemoved(void *refcon, io_iterator_t iterator )
{
    io_object_t obj;
    while(obj = IOIteratorNext(iterator)) {
        CFMutableDictionaryRef properties;
        CFNumberRef dataDesc;
         kern_return_t err;
        UInt64 GUID;
        int device;
 
      //  syslog(LOG_INFO, "object 0x%x departed!\n", obj);
        err = IORegistryEntryCreateCFProperties(obj, &properties, kCFAllocatorDefault, kNilOptions);

        dataDesc = (CFNumberRef)CFDictionaryGetValue(properties, CFSTR("GUID"));
        CFNumberGetValue(dataDesc, kCFNumberSInt64Type, &GUID);
     //   syslog(LOG_INFO, "device gone, GUID: 0x%x%08x\n",
     //           (UInt32)(GUID>>32), (UInt32)(GUID & 0xffffffff));
        CFRelease(properties);
        for(device=1; device<kMaxDevice; device++) {
            device_info *dev = devices+device;
            if(GUID == dev->fGUID) {
                if(dev->fConnection) {
                    DVConnectionEvent theEvent;
                    
                    // Close the connection
                    IOServiceClose(dev->fConnection);
                    dev->fConnection = NULL;
                    fNumAlive--;
        
                    // post a DV event to let the curious know...
                    theEvent.eventHeader.deviceID	= (DVDeviceID) (dev-devices);
                    theEvent.eventHeader.theEvent 	= kDVDeviceRemoved;
                    postEvent( &theEvent.eventHeader );
                }
                break;
            }
        }
        IOObjectRelease(obj);
    }
}


static SInt32 init(void)
{
    UInt32 i;
    kern_return_t err;

    for(i = 0 ; i < kMaxDevice ; i++) devices[i].fGUID = 0;
    fNumDevices = 0;
    fNumAlive = 0;

    if ((err = IOMasterPort(bootstrap_port, &fMasterDevicePort)) !=
        KERN_SUCCESS) {
        fprintf(stderr,"DVFamily : IOMasterPort failed: %d\n", err);
        return err;
    }

    sNotifyPort = IONotificationPortCreate(fMasterDevicePort);
    sNotifyMachPort = IONotificationPortGetMachPort(sNotifyPort);

    
    if ((err = IOServiceAddMatchingNotification( sNotifyPort,
            kIOMatchedNotification, IOServiceMatching( kDVKernelDriverName ),
            deviceArrived, (void *)12345, &sMatchEnumer )) != kIOReturnSuccess) {
        return err;
    }
    
    if ((err = IOServiceAddMatchingNotification( sNotifyPort,
            kIOTerminatedNotification, IOServiceMatching( kDVKernelDriverName ),
            deviceRemoved, (void *)12346, &sTermEnumer )) != kIOReturnSuccess) {
        return err;
    }
    

    deviceArrived((void *)12345, sMatchEnumer);
    deviceRemoved((void *)12346, sTermEnumer);


    inited = 1;
    return noErr;
}

///////////////////////////////////////////////////////////////////////
// Notifications
//
///////////////////////////////////////////////////////////////////////
OSErr DVNewNotification( DVDeviceRefNum refNum, DVNotifyProc notifyProc,
                                    void *userData, DVNotificationID *pNotifyID )
{
    int i;
    for(i=0; i<kMaxNotifications; i++) {
        if(sNotifications[i].notifyProc == NULL) {
            sNotifications[i].wantedEvents = 0;
            sNotifications[i].notifyProc = notifyProc;
            sNotifications[i].userRefCon = userData;
            sNotifications[i].device = (DVDeviceID)refNum;
            *pNotifyID = (DVNotificationID)(i+1);
            return noErr; 
        }
    }
    return kDVNoNotificationsErr;
}
	
OSErr DVNotifyMeWhen( DVDeviceRefNum refNum, DVNotificationID notifyID, UInt32 events)
{
    int id = ((int)notifyID)-1;
    if(sNotifications[id].notifyProc != NULL) {
        sNotifications[id].wantedEvents = events;
        return noErr; 
    }
    else
        return paramErr;
}

OSErr DVCancelNotification( DVDeviceRefNum refNum, DVNotificationID notifyID )
{
    int id = ((int)notifyID)-1;
    if(sNotifications[id].notifyProc != NULL) {
        sNotifications[id].wantedEvents = 0;
        return noErr; 
    }
    else
        return paramErr;
}

OSErr DVDisposeNotification( DVDeviceRefNum refNum, DVNotificationID notifyID )
{
    int id = ((int)notifyID)-1;
    if(sNotifications[id].notifyProc != NULL) {
        sNotifications[id].wantedEvents = 0;
        sNotifications[id].notifyProc = NULL;
        sNotifications[id].userRefCon = NULL;
        return noErr; 
    }
    else
        return paramErr;
}


///////////////////////////////////////////////////////////////////////
// AVC
///////////////////////////////////////////////////////////////////////

OSErr DVDoAVCTransaction( DVDeviceRefNum refNum, AVCTransactionParamsPtr pParams )
{
    int err;
    mach_msg_type_number_t outputCnt = pParams->responseBufferSize;
    err = io_connect_method_structureI_structureO(
        devices[refNum].fConnection, kAVCCommand,
        pParams->commandBufferPtr, pParams->commandLength,
        pParams->responseBufferPtr, &outputCnt
    );
    pParams->responseBufferSize = outputCnt;
    // printf("DVDoAVCTransaction %d %d\n",pParams->commandLength,err);
    return err;
}

///////////////////////////////////////////////////////////////////////
// device management
///////////////////////////////////////////////////////////////////////

UInt32 DVCountDevices( void )
{
    if(!inited)
        init();
    else {
        ReceiveMsg msg;
        kern_return_t err;
        err = mach_msg(&msg.msgHdr, MACH_RCV_MSG | MACH_RCV_TIMEOUT,
                       0, sizeof(msg), sNotifyMachPort, 1 /* mSec timeout */, MACH_PORT_NULL);
        if(err == MACH_MSG_SUCCESS && msg.msgHdr.msgh_id == kOSNotificationMessageID)
            IODispatchCalloutFromMessage(NULL, &msg.msgHdr, sNotifyPort);
    }
    // Return total number of devices, not just the number currently connected.
    return fNumDevices;
}

OSErr DVGetIndDevice( DVDeviceID * pDVDevice, UInt32 index )
{
    *pDVDevice = index;
    return noErr;
}

OSErr DVSetDeviceName( DVDeviceID deviceID, char * str )
{
//printf("DVSetDeviceName(0x%x, %s)\n", deviceID, str);
    return noErr; // FIXME
}

OSErr DVGetDeviceName( DVDeviceID deviceID, char * str )
{
    strcpy(str, devices[deviceID].fName);
    return noErr; // FIXME
}

OSErr DVUnregisterClientApp( DVClientID dvClientID )
{
//printf("DVUnregisterClientApp(0x%x)\n", dvClientID);
    return noErr; // FIXME
}

OSErr DVRegisterClientApp( DVClientID *pDVClientID, UInt32 clientContextData )
{
//printf("DVRegisterClientApp(0x%x, 0x%x)\n", pDVClientID, clientContextData);
    *pDVClientID = (DVClientID)1;
    return noErr; // FIXME
}

OSStatus DVGetNextClientEvent( DVClientID dvClientID )
{
//printf("DVGetNextClientEvent(%d)\n", dvClientID);
    return noErr; // FIXME
}

OSErr DVOpenDriver( DVDeviceID deviceID, DVDeviceRefNum *pRefNum )
{
    IOReturn err; 
    device_info *dev = &devices[deviceID];
//printf("DVOpenDriver(0x%x, 0x%x)\n", deviceID, pRefNum);
    *pRefNum = deviceID;

    if(dev->fOpens > 0) {
        dev->fOpens++;
        return noErr;
    }
    
    do {
        // Get device standard
        UInt32 standard;
        err = DVGetDeviceStandard(*pRefNum, &standard );

        // allocate frame buffers
        dev->fNumOutputFrames = exec_cmd(kDVGetNumOutputFrames,*pRefNum);
        //syslog(LOG_INFO,"output frames %d\n",dev->fNumOutputFrames);
        err = exec_cmd(kDVReadStart,*pRefNum);
        if(err != kIOReturnSuccess) break;
        err = mapframes(*pRefNum);
        if(err != kIOReturnSuccess) break;
        err = exec_cmd(kDVReadStop,*pRefNum);
        if(err != kIOReturnSuccess) break;
        dev->fOpens++;
    } while (0);
    
    if(err != kIOReturnSuccess) {
        syslog(LOG_INFO, "error opening DV device: %x", err);
        DVCountDevices();	// Update device states
    }
    return err; // FIXME
}

OSErr DVCloseDriver( DVDeviceRefNum refNum )
{
//printf("DVCloseDriver(0x%x), opens = %d\n", refNum, devices[refNum].fOpens);
    devices[refNum].fOpens--;
    if(devices[refNum].fOpens == 0) {
        unmapframes(refNum);
        exec_cmd(kDVReadExit,refNum);
    }
    return noErr;
}

OSErr DVGetDeviceStandard(DVDeviceRefNum refNum, UInt32 * pStandard )
{
    AVCCTSFrameStruct      avcFrame;
    AVCTransactionParams   transactionParams;
    UInt8                  responseBuffer[ 16 ];
    OSErr                  theErr = noErr;
    UInt32                 currentSignal, AVCStatus;

    // fill up the avc frame
    avcFrame.cmdType_respCode  = kAVCStatusInquiryCommand;
    avcFrame.headerAddress     = 0x20;                        // for now
    avcFrame.opcode            = kAVCOutputSignalModeOpcode;
    avcFrame.operand[ 0 ]      = kAVCSignalModeDummyOperand;
    
    // fill up the transaction parameter block
    transactionParams.commandBufferPtr      = (Ptr) &avcFrame;
    transactionParams.commandLength         = 4;
    transactionParams.responseBufferPtr     = (Ptr) responseBuffer;
    transactionParams.responseBufferSize    = 4;
    transactionParams.responseHandler       = nil;
    
    theErr = DVDoAVCTransaction(refNum, &transactionParams );
    currentSignal = ((responseBuffer[ 2 ] << 8) | responseBuffer[ 3 ]);
    AVCStatus = responseBuffer[ 0 ];

    *pStandard = kUnknownStandard;
    switch (currentSignal & 0x000000ff) 
    {
        case kAVCSignalModeSD525_60:
        case kAVCSignalModeSDL525_60:
        case kAVCSignalModeHD1125_60: 
            devices[refNum].frameSize = kNTSCCompressedBufferSize;
            devices[refNum].fOutputMode = kAVCSignalModeSD525_60;
            
            *pStandard = kNTSCStandard;
            return( theErr );
    
        case kAVCSignalModeSD625_50: 
        case kAVCSignalModeSDL625_50: 
        case kAVCSignalModeHD1250_50: 
            devices[refNum].frameSize = kPALCompressedBufferSize;
            devices[refNum].fOutputMode = kAVCSignalModeSD625_50;
            
            *pStandard = kPALStandard;
            return( theErr );
    
        default:
            return( kUnknownStandardErr ); // how should I handle this?
    }
}

///////////////////////////////////////////////////////////////////////
// readin'
///////////////////////////////////////////////////////////////////////

OSErr DVIsEnabled( DVDeviceRefNum refNum, Boolean *isEnabled)
{
    *isEnabled = true;
    return noErr; // FIXME
}

OSErr DVEnableRead( DVDeviceRefNum refNum )
{
    OSErr err;
    //printf("DVEnableRead(%d)\n", refNum);
    err = exec_cmd(kDVReadStart,refNum);
    // err = mapframes(refNum);
    return err; // FIXME
}

OSErr DVDisableRead( DVDeviceRefNum refNum )
{
    OSErr err;
    //printf("DVDisableRead(%d)\n", refNum);
    err = exec_cmd(kDVReadStop,refNum);
    return err; // FIXME
}

OSErr DVReadFrame( DVDeviceRefNum refNum, Ptr *ppReadBuffer, UInt32 * pSize )
{
    int index = devices[refNum].fSharedVars->fReader % devices[refNum].fNumOutputFrames;
    // wait for writer
    // if (*devices[refNum].fReader + 1 >= *devices[refNum].fWriter) return -1;
    if (devices[refNum].fSharedVars->fReader + 1 >= devices[refNum].fSharedVars->fWriter) return -1;

    // copy frame
    *ppReadBuffer = (Ptr)devices[refNum].bufMem[index];
    *pSize = devices[refNum].fSharedVars->fFrameSize[index];

    // fprintf(stderr,"DVReadFrame reader=%d\n",*devices[refNum].fReader % devices[refNum].fNumOutputFrames);
    return noErr; // FIXME
}

OSErr DVReleaseFrame( DVDeviceRefNum refNum, Ptr pReadBuffer )
{
    devices[refNum].fSharedVars->fReader += 1;
    return noErr; // FIXME
}

///////////////////////////////////////////////////////////////////////
// writin'
///////////////////////////////////////////////////////////////////////

OSErr DVEnableWrite( DVDeviceRefNum refNum )
{
    OSErr err;
    unsigned int 	size = 0;
    int mode = devices[refNum].fOutputMode;

    err = io_connect_method_scalarI_scalarO(devices[refNum].fConnection, kDVSetWriteSignalMode, &mode, 1,
        NULL, &size);

    err = exec_cmd(kDVWriteStart,refNum);
    // err = mapframes(refNum);
    return err; // FIXME
}

OSErr DVDisableWrite( DVDeviceRefNum refNum )
{
    OSErr err;
    err = exec_cmd(kDVWriteStop,refNum);
    return err; // FIXME
}

OSErr DVGetEmptyFrame( DVDeviceRefNum refNum, Ptr *ppEmptyFrameBuffer, UInt32 * pSize )
{
    // check for error
    if (devices[refNum].fSharedVars->fStatus == 2) return -2;

    // wait for reader
    // if (*devices[refNum].fWriter + 2 >= *devices[refNum].fReader + devices[refNum].fNumOutputFrames) return -1;
    if (devices[refNum].fSharedVars->fWriter + 1 >=
        devices[refNum].fSharedVars->fReader + devices[refNum].fNumOutputFrames) return -1;

    // copy frame
    *ppEmptyFrameBuffer = (Ptr)devices[refNum].bufMem[devices[refNum].fSharedVars->fWriter %
                                                            devices[refNum].fNumOutputFrames];
    *pSize = devices[refNum].frameSize;

    return noErr; // FIXME
}

OSErr DVWriteFrame( DVDeviceRefNum refNum, Ptr pWriteBuffer )
{
    devices[refNum].fSharedVars->fWriter += 1;

    return noErr; // FIXME
}

OSErr DVSetWriteSignalMode( DVDeviceRefNum refNum, UInt8 mode)
{
    devices[refNum].fOutputMode = mode;

    return noErr;
}


UInt32 getNumDroppedFrames(DVDeviceRefNum refNum)
{
    return devices[refNum].fSharedVars->fDroppedFrames;
}
