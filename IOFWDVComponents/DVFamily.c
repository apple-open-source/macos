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
#include <mach/message.h>
#include <mach/mach_error.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <syslog.h>	// Debug messages
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

#include <IOKit/IOMessage.h>

#include <IOKit/firewire/IOFireWireLib.h>
#include <IOKit/avc/IOFireWireAVCConsts.h>
#include "DVLib.h"

#include "DVFamily.h"

#define kNTSCCompressedBufferSize	120000
#define	kPALCompressedBufferSize	144000

#define kMaxNotifications 64

typedef struct device_info_struct {
    DVDevice *fDevice;
    UInt32 fNumOutputFrames;
    UInt32 fOpens;
    UInt32 frameSize;
    UInt8* bufMem[kDVMaxFrames];
    DVFrameVars *fReadSharedVars;	// Structure shared with isoc program thread
    DVFrameVars *fWriteSharedVars;	// Structure shared with isoc program thread
    DVGlobalOutPtr fWrite;
    DVGlobalInPtr fRead;
    UInt8		fOutputMode;		// AVC output signal mode - NTSC/SDL etc.
} device_info;

// notification stuff
typedef struct DVNotificationEntryStruct {
	UInt32		wantedEvents;
	DVNotifyProc	notifyProc;
	void		*userRefCon;
        DVDeviceID	device;
} DVNotificationEntry, *DVNotificationEntryPtr;


static DVThread *sThread;
static UInt32 fNumDevices, fNumAlive;
static device_info devices[kDVMaxDevicesActive];
static DVNotificationEntry sNotifications[kMaxNotifications];
static int inited = 0;

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

static void deviceMessage(void * refcon, UInt32 messageType, void *messageArgument)
{
    DVDeviceRefNum refNum = (DVDeviceRefNum)refcon;
    
	device_info *dev = &devices[refNum];
    //syslog(LOG_INFO,"Got message: refcon %d, type 0x%x arg %p\n",
    //    refcon, messageType, messageArgument);
    
    switch(messageType) {
        case kIOMessageServiceIsTerminated:
            //syslog(LOG_INFO, "Terminating device %d\n", refNum);
            break;

        case kIOFWMessageServiceIsRequestingClose:
            if(dev->fOpens > 0) {
                //syslog(LOG_INFO, "Force closing device %d\n", refNum);
                if(dev->fWrite)
                    DVDisableWrite(refNum);
                if(dev->fRead)
                    DVDisableRead(refNum);
                dev->fOpens = 1;
                DVCloseDriver(refNum);
            }
            break;
            
        case kIOMessageServiceWasClosed:
            //syslog(LOG_INFO, "device %d closing\n", refNum);
            break;
        default:
            break;
    }
    
}

static void deviceArrived(void *refcon, DVDevice *device, UInt32 index, UInt32 refound)
{
    //syslog(LOG_INFO,"deviceArrived(0x%x, 0x%x)\n", refcon, index);
    fNumAlive++;
    devices[index].fDevice = device;
    if(!refound)
        fNumDevices++;
    //syslog(LOG_INFO, "Found a device, GUID: 0x%x%08x name: %s\n",
    //        (UInt32)(GUID>>32), (UInt32)(GUID & 0xffffffff), devices[device].fName);
        
    {
        // post a DV event to let the curious know...
        DVConnectionEvent theEvent;
        theEvent.eventHeader.deviceID	= (DVDeviceID) index;
        theEvent.eventHeader.theEvent 	= kDVDeviceAdded;
        postEvent( &theEvent.eventHeader );
    }
}

static SInt32 init(void)
{
    fNumDevices = 0;
    fNumAlive = 0;
    
   // sThread = DVCreateThread(IOServiceMatching( kDVKernelDriverName ), deviceArrived, (void *)12345, deviceRemoved, (void *)12346);
    sThread = DVCreateThread(deviceArrived, (void *)12345, nil, nil, deviceMessage);
    DVRunThread(sThread);
    
    //syslog(LOG_INFO, "workloop is: %p\n", workLoop);

    inited = 1;
    //syslog(LOG_INFO, "Initted\n");
    
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
    IOReturn err;
    device_info *dev = &devices[refNum];
    //syslog(LOG_INFO, "DVDoAVCTransaction, open %d, interface %p\n", dev->fOpens, dev->fDevice.fAVCInterface);
    
    if(!dev->fDevice->fSupportsFCP) {
        return( -4162 ); // timeoutErr
    }
    
	err = (*dev->fDevice->fAVCInterface)->AVCCommand(dev->fDevice->fAVCInterface,
                                    pParams->commandBufferPtr, pParams->commandLength,
                                    pParams->responseBufferPtr, &pParams->responseBufferSize);
    //syslog(LOG_INFO, "DVDoAVCTransaction returns %d: %x %x %x %x\n",
    //    pParams->responseBufferSize, pParams->responseBufferPtr[0], pParams->responseBufferPtr[1],
    //                                    pParams->responseBufferPtr[2], pParams->responseBufferPtr[3]);

    //if(err)
    //    syslog(LOG_INFO, "DVDoAVCTransaction(), err 0x%x\n", err);
    return err;
}

///////////////////////////////////////////////////////////////////////
// device management
///////////////////////////////////////////////////////////////////////

UInt32 DVCountDevices( void )
{
    if(!inited)
        init();
    // Return total number of devices, not just the number currently connected.
    //syslog(LOG_INFO, "DVCountDevices() = %d\n", fNumDevices);
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
    strcpy(str, devices[deviceID].fDevice->fName);
    return noErr;
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
    IOReturn err = kIOReturnSuccess; 
    device_info *dev = &devices[deviceID];
//syslog(LOG_INFO, "DVOpenDriver(0x%x, 0x%x)\n", deviceID, pRefNum);
    *pRefNum = deviceID;

    if(dev->fOpens > 0) {
        dev->fOpens++;
        return noErr;
        //return kAlreadyEnabledErr;
    }
        
    do {
        if(dev->fDevice->fObject == 0) {
            err = kDVDisconnectedErr;
            break;
        }
        //err = DVDeviceOpen(sThread, dev->fDevice);
        //if(err != kIOReturnSuccess) break;
        
        dev->fNumOutputFrames = 5;
        dev->fOpens++;
    } while (0);
    
    if(err != kIOReturnSuccess) {
        syslog(LOG_INFO, "error opening DV device: %x", err);
        dev->fOpens = 1;
        DVCloseDriver(deviceID);
    }
    return err; // FIXME
}

OSErr DVCloseDriver( DVDeviceRefNum refNum )
{
    device_info *dev = &devices[refNum];
//syslog(LOG_INFO, "DVCloseDriver(0x%x), opens = %d\n", refNum, devices[refNum].fOpens);
    if(dev->fOpens > 0) {
        dev->fOpens--;
        if(dev->fOpens == 0) {
        }
    }
    return noErr;
}

OSErr DVGetDeviceStandard(DVDeviceRefNum refNum, UInt32 * pStandard )
{
    AVCCTSFrameStruct		avcFrame;
    AVCTransactionParams	transactionParams;
    UInt8					responseBuffer[ 16 ];
    OSStatus				theErr = noErr;
    UInt32					currentSignal, AVCStatus;
    device_info *			dev = &devices[refNum];

//syslog(LOG_INFO, "DVGetDeviceStandard(0x%x)\n", refNum);
    if(!devices[refNum].fDevice->fSupportsFCP) {
            *pStandard = kNTSCStandard;
            devices[refNum].frameSize = kNTSCCompressedBufferSize;
            devices[refNum].fOutputMode = kAVCSignalModeSD525_60;
            return( theErr );
    }

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
    
	theErr = (*dev->fDevice->fAVCInterface)->AVCCommand(dev->fDevice->fAVCInterface,
                                    transactionParams.commandBufferPtr, transactionParams.commandLength,
                                    transactionParams.responseBufferPtr, &transactionParams.responseBufferSize);
    if(theErr) {
        //syslog(LOG_INFO, "DVGetDeviceStandard(), err 0x%x\n", theErr);
        if(theErr == kIOReturnTimeout) {
            *pStandard = kNTSCStandard;
            return noErr;
        }
        return theErr;
    }
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
            syslog(LOG_INFO, "DVGetDeviceStandard(), err 0x%x\n", kUnknownStandardErr);
            return( kUnknownStandardErr ); // how should I handle this?
    }
}

///////////////////////////////////////////////////////////////////////
// readin'
///////////////////////////////////////////////////////////////////////

OSErr DVIsEnabled( DVDeviceRefNum refNum, Boolean *isEnabled)
{
    //syslog(LOG_INFO, "DVIsEnabled, returning %d\n", devices[refNum].fRead);
    *isEnabled = devices[refNum].fRead != 0;
    return noErr; // FIXME
}

OSErr DVEnableRead( DVDeviceRefNum refNum )
{
    OSErr err;
    device_info *dev = &devices[refNum];
    //syslog(LOG_INFO, "DVEnableRead entry\n");
    if(dev->fRead) {
        //syslog(LOG_INFO, "DVEnableRead, already enabled!\n");
        return noErr;
    }
    if(dev->fWrite) {
        //syslog(LOG_INFO, "DVEnableRead, already writing!\n");
        return kAlreadyEnabledErr;
    }
    do {
        err = DVDeviceOpen(sThread, dev->fDevice);
        if(err != kIOReturnSuccess) break;
        dev->fRead = DVAllocRead(dev->fDevice, sThread);
        err = DVReadAllocFrames(dev->fRead, dev->fNumOutputFrames, 
            &dev->fReadSharedVars, dev->bufMem);
        if(err != kIOReturnSuccess) break;
        err = DVReadStart(dev->fRead);
    } while (0);
    if(err) {
        syslog(LOG_INFO, "DVEnableRead(), err 0x%x\n", err);
        DVDisableRead(refNum);
    }
    return err;
}

OSErr DVDisableRead( DVDeviceRefNum refNum )
{
    device_info *dev = &devices[refNum];
    if(dev->fRead) {
        //syslog(LOG_INFO, "DVDisableRead\n");
        DVReadStop(dev->fRead);
        DVReadFreeFrames(dev->fRead);
        DVReadFree(dev->fRead);
        dev->fRead = NULL;
        DVDeviceClose(dev->fDevice);
    }
    return noErr;
}

OSErr DVReadFrame( DVDeviceRefNum refNum, Ptr *ppReadBuffer, UInt32 * pSize )
{
    device_info *dev = &devices[refNum];
    int index=0, i;
    
    // wait for writer
     for(i=dev->fReadSharedVars->fReader; i<dev->fReadSharedVars->fWriter; i++) {
        index = i % dev->fNumOutputFrames;
        if(dev->fReadSharedVars->fFrameStatus[index] == kReady)
            break;
    }
    if (i >= dev->fReadSharedVars->fWriter)
        return -1;
    // copy frame
    *ppReadBuffer = dev->bufMem[index];
    *pSize = dev->fReadSharedVars->fFrameSize[index];
    dev->fReadSharedVars->fFrameStatus[index] = kReading;
    //syslog(LOG_INFO,"DVReadFrame returning frame %d @ %x\n", index, *ppReadBuffer);
    dev->fReadSharedVars->fReader = i;
    return noErr; // FIXME
}

OSErr DVReleaseFrame( DVDeviceRefNum refNum, Ptr pReadBuffer )
{
    int i;
    device_info *dev = &devices[refNum];
    for(i=0; i<dev->fNumOutputFrames; i++) {
        if(pReadBuffer == dev->bufMem[i])
            break;
    }
    //syslog(LOG_INFO, "released frame %d=%p, fReader now %d\n",
    //        i, pReadBuffer, dev->fReadSharedVars->fReader);
    dev->fReadSharedVars->fFrameStatus[i] = kEmpty;
    return noErr; // FIXME
}

///////////////////////////////////////////////////////////////////////
// writin'
///////////////////////////////////////////////////////////////////////

OSErr DVEnableWrite( DVDeviceRefNum refNum )
{
    IOReturn err;
    device_info *dev = &devices[refNum];
        //syslog(LOG_INFO, "DVEnableWrite entry\n");

    if(dev->fWrite) {
        //syslog(LOG_INFO, "DVEnableWrite, already enabled!\n");
        return noErr;
    }
    if(dev->fRead) {
        //syslog(LOG_INFO, "DVEnableWrite, already reading!\n");
        return kAlreadyEnabledErr;
    }
    
    do {
        err = DVDeviceOpen(sThread, dev->fDevice);
        if(err != kIOReturnSuccess) break;
        dev->fWrite = DVAllocWrite(dev->fDevice, sThread);
        err = DVWriteSetSignalMode(dev->fWrite, dev->fOutputMode);
        if(err)
            break;
        err = DVWriteAllocFrames(dev->fWrite, dev->fNumOutputFrames, 
            &dev->fWriteSharedVars, dev->bufMem);
        if(err)
            break;
            
        err = DVWriteStart(dev->fWrite);
    } while (0);
    
    if(err) {
        syslog(LOG_INFO, "DVEnableWrite(), err 0x%x\n", err);
        DVDisableWrite(refNum);
    }
    return err;
}

OSErr DVDisableWrite( DVDeviceRefNum refNum )
{
    //OSErr err;
    device_info *dev = &devices[refNum];
    //syslog(LOG_INFO, "DVDisableWrite\n");
    if(dev->fWrite) {
        DVWriteStop(dev->fWrite);
        DVWriteFreeFrames(dev->fWrite);
        DVWriteFree(dev->fWrite);
        dev->fWrite = NULL;
        DVDeviceClose(dev->fDevice);
    }
    
    return noErr;
}

OSErr DVGetEmptyFrame( DVDeviceRefNum refNum, Ptr *ppEmptyFrameBuffer, UInt32 * pSize )
{
    // check for error
    if(!devices[refNum].fWrite) {
        syslog(LOG_INFO, "DVGetEmptyFrame, not writing!!\n");
        return kNotEnabledErr;
    }

    if (devices[refNum].fWriteSharedVars->fStatus == 2)
    {
        syslog(LOG_INFO, "DVGetEmptyFrame, status %d\n", devices[refNum].fWriteSharedVars->fStatus);
        return -2;
    }

    // wait for reader
    // if (*devices[refNum].fWriter + 2 >= *devices[refNum].fReader + devices[refNum].fNumOutputFrames) return -1;
    if (devices[refNum].fWriteSharedVars->fWriter + 1 >=
        devices[refNum].fWriteSharedVars->fReader + devices[refNum].fNumOutputFrames)
    {
        //syslog(LOG_INFO, "DVGetEmptyFrame, no frame available: %d-%d\n",
        //    devices[refNum].fWriteSharedVars->fWriter, devices[refNum].fWriteSharedVars->fReader);
        return -1;
    }
    // copy frame
    *ppEmptyFrameBuffer = devices[refNum].bufMem[devices[refNum].fWriteSharedVars->fWriter % devices[refNum].fNumOutputFrames];
    *pSize = devices[refNum].frameSize;

    return noErr; // FIXME
}

OSErr DVWriteFrame( DVDeviceRefNum refNum, Ptr pWriteBuffer )
{
    device_info *dev = &devices[refNum];
    if(!dev->fWrite) {
        syslog(LOG_INFO, "DVWriteFrame, not writing!!\n");
        return kNotEnabledErr;
    }
    dev->fWriteSharedVars->fWriter += 1;

    return noErr; // FIXME
}

OSErr DVSetWriteSignalMode( DVDeviceRefNum refNum, UInt8 mode)
{
    devices[refNum].fOutputMode = mode;

    return noErr;
}


UInt32 getNumDroppedFrames(DVDeviceRefNum refNum)
{
    return devices[refNum].fWriteSharedVars->fDroppedFrames;
}
