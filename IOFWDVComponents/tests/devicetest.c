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
#include <QuickTime/QuickTime.h>

#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iokit/IOKitLib.h>
#include <iokit/avc/IOFireWireAVCConsts.h>

#include <DVComponentGlue/IsochronousDataHandler.h>
#include <DVComponentGlue/DeviceControl.h>

static QTAtomSpec videoConfig;
static IDHDeviceID deviceID;
static IDHNotificationID notificationID;

static void printP(const char *s)
{
    int len = *s++;
    while(len--)
        printf("%c", *s++);
}

static void print4(const char *s, UInt32 val)
{
    printf("%s'%c%c%c%c'(0x%x)", s, val>>24, val>>16, val>>8, val, val);
}

static OSStatus notificationProc(IDHGenericEvent* event, void* userData)
{
    ComponentInstance	theInst = userData;
    printf("Got notification for device 0x%x, notification 0x%x, event 0x%x, userdata 0x%x\n",
        event->eventHeader.deviceID, event->eventHeader.notificationID, event->eventHeader.event,
        userData);
        
        // Reenable notification
   IDHNotifyMeWhen(theInst, event->eventHeader.notificationID, kIDHEventEveryEvent);

    return noErr;
}

static void doControlTest(ComponentInstance theInst, QTAtomSpec *currentIsochConfig, UInt8 op1, UInt8 op2)
{
        //Component control;
        ComponentInstance controlInst;
        ComponentResult result;
        IDHDeviceStatus			devStatus;
        DVCTransactionParams 	pParams;
        char					in[4], out[16];
        //char					in[44], out[44];
        int i;

        result = IDHGetDeviceControl(theInst, &controlInst);
        if(result)
                goto Exit;
        //controlInst = OpenComponent(control);
        // get the local node's fw ref id
        result = IDHGetDeviceStatus( theInst, currentIsochConfig, &devStatus);
        if(result)
                goto Exit;
        //result = FWClockPrivSetFWReferenceID(clockInst, (FWReferenceID) devStatus.localNodeID );
        //if(result)
        //	goto Exit;

        // set the clock's fw id
        //clockInst = OpenDefaultComponent(clockComponentType, systemMicrosecondClock);

        if(!controlInst)
                goto Exit;

#if 1
        // fill up the avc frame
        in[0]	= kAVCStatusInquiryCommand; //kAVCControlCommand;
        in[1] 	= 0x20;						// for now
        in[2] 	= op1;
        in[3] 	= op2;
#else
        // fill up the avc frame
        in[0]	= 0x00;
        in[1] 	= 0x58;						// for now
        in[2] 	= 0x50;
        in[3] 	= 0x00;
        
        in[4]	= 0xff; 
        in[5] 	= 0x00;						// for now
        in[6] 	= 0x00;
        in[7] 	= 0x00;
        
        in[8]	= 0xff; 
        in[9]	= 0xff; 
        in[10]	= 0xff; 
        in[11]	= 0xff; 
     
        in[12] 	= 0x00;
        in[13] 	= 0x00;
        in[14] 	= 0x00;
        in[15] 	= 0x1b;
        
        in[16] 	= 0x5c;
        in[17] 	= 0x44;
        in[18] 	= 0x43;
        in[19] 	= 0x49;
        
        in[20] 	= 0x4d;
        in[21] 	= 0x5c;
        in[22] 	= 0x31;
        in[23] 	= 0x30;
        
        in[24] 	= 0x31;
        in[25] 	= 0x43;
        in[26] 	= 0x41;
        in[27] 	= 0x4e;
        
        in[28] 	= 0x4f;
        in[29] 	= 0x4e;
        in[30] 	= 0x5c;
        in[31] 	= 0x41;
        
        in[32] 	= 0x55;
        in[33] 	= 0x54;
        in[34] 	= 0x5f;
        in[35] 	= 0x30;
        
        in[36] 	= 0x31;
        in[37] 	= 0x30;
        in[38] 	= 0x33;
        in[39] 	= 0x2e;
        
        in[40] 	= 0x4a;
        in[41] 	= 0x50;
        in[42] 	= 0x47;
        in[43] 	= 0x00;
        
                   
#endif

        // fill up the transaction parameter block
    pParams.commandBufferPtr = in;
    pParams.commandLength = sizeof(in);
    pParams.responseBufferPtr = out;
    pParams.responseBufferSize = sizeof(out);
    pParams.responseHandler = NULL;

    do {
        for(i=0; i<sizeof(out); i++)
                out[i] = 0;
        result = DeviceControlDoAVCTransaction( controlInst, &pParams);
        if(result == kIOReturnOffline) {
            printf("offline!!\n");
            sleep(1);
            continue;
        }
        if(result)
            goto Exit;
        printf("Received %d bytes:", pParams.responseBufferSize);
        for(i=0; i<sizeof(out); i++)
                printf("%d(0x%x) ", out[i], out[i]);
        printf("\n");
    } while (1); //(result != kIOReturnSuccess);
    
    //sleep(10);

Exit:
        if(result != noErr)
                printf("Control error %d(%x)\n", result, result);
}

static void OpenDV()
{
    ComponentInstance theInst;
    ComponentResult version;
    QTAtomContainer deviceList = NULL;
    short nDVDevices, i, j;
    QTAtom deviceAtom;
    UInt32 cmpFlag;
    UInt32 isoversion;
    long size;
    OSStatus err;
    
    theInst = OpenDefaultComponent('ihlr', 'dv  ');
    printf("Instance is 0x%x\n", theInst);
        if(theInst == NULL)
                return;

    version = CallComponentVersion(theInst);
    printf("Version is 0x%x\n", version);

// Ask for notifications for what's happening - ask for EVERYTHING!!
    err = IDHNewNotification(theInst, kIDHDeviceIDEveryDevice, notificationProc, theInst, &notificationID);
    if( err != noErr)
            goto error;

    err = IDHNotifyMeWhen(theInst, notificationID, kIDHEventEveryEvent);
    if( err != noErr)
            goto error;

    do {
        err = IDHGetDeviceList( theInst, &deviceList);
        if( err != noErr)
                goto error;

        nDVDevices = QTCountChildrenOfType( deviceList, kParentAtomIsContainer, kIDHDeviceAtomType);
        if(nDVDevices > 0)
            break;
        printf("Waiting for a camera...\n");
        sleep(1);
    } while(true);

    
    QTLockContainer( deviceList);
    // find the cmp atom
    deviceAtom = QTFindChildByIndex( deviceList, kParentAtomIsContainer, kIDHUseCMPAtomType, 1, nil);
    if( deviceAtom == nil)
            goto error;

    // get the value of the cmp atom
    QTCopyAtomDataToPtr( deviceList, deviceAtom, true, sizeof( cmpFlag), &cmpFlag, &size);

    // find the version atom
    deviceAtom = QTFindChildByIndex( deviceList, kParentAtomIsContainer, kIDHIsochVersionAtomType, 1, nil);
    if( deviceAtom == nil)
            goto error;

    // get the value of the version atom
    QTCopyAtomDataToPtr( deviceList, deviceAtom, true, sizeof( isoversion), &isoversion, &size);

    printf("Version 0x%x. %d DV devices, use CMP flag is %d\n", isoversion, nDVDevices, cmpFlag);

    for( i=0; i<nDVDevices; ++i)
    {
            QTAtom isochAtom, dataAtom;
            UInt32 test[2];
            int nConfigs;
            char cameraName[256];
            IDHDeviceStatus deviceStatus;

            // get the atom to this device
            deviceAtom = QTFindChildByIndex( deviceList, kParentAtomIsContainer, kIDHDeviceAtomType, i + 1, nil);
            if( deviceAtom == nil)
                    goto error;

            printf("device %d ", i);

            dataAtom = QTFindChildByIndex( deviceList, deviceAtom, kIDHUniqueIDType, 1, nil);
            if( dataAtom == nil)
                    goto error;
            QTCopyAtomDataToPtr( deviceList, dataAtom, true, sizeof( test), test, &size);
            printf("guid 0x%x%08x ", test[0], test[1]);

            dataAtom = QTFindChildByIndex( deviceList, deviceAtom, kIDHNameAtomType, 1, nil);
            if( dataAtom == nil)
                    goto error;
            QTCopyAtomDataToPtr( deviceList, dataAtom, true, 255, cameraName, &size);
            cameraName[size] = 0;
            printf("%s ", cameraName+1);

            dataAtom = QTFindChildByIndex( deviceList, deviceAtom, kIDHDeviceIDType, 1, nil);
            if( dataAtom == nil)
                    goto error;
            QTCopyAtomDataToPtr( deviceList, dataAtom, true, sizeof( deviceID), &deviceID, &size);
            printf("deviceID 0x%x ", deviceID);

            dataAtom = QTFindChildByIndex( deviceList, deviceAtom, 'ddin', 1, nil);
            if( dataAtom == nil)
                    goto error;
            QTCopyAtomDataToPtr( deviceList, dataAtom, true, sizeof( deviceStatus), &deviceStatus, &size);
            printf("\ndevice status:\n");
            printf("version %d\n", deviceStatus.version);
            printf("physicallyConnected %d\n", deviceStatus.physicallyConnected);
            printf("readEnabled %d ", deviceStatus.readEnabled);
            printf("writeEnabled %d ", deviceStatus.writeEnabled);
            printf("exclusiveAccess %d\n", deviceStatus.exclusiveAccess);
            printf("currentBandwidth %d ", deviceStatus.currentBandwidth);
            printf("currentChannel %d ", deviceStatus.currentChannel);
            printf("inputStandard %d ", deviceStatus.inputStandard);
            printf("deviceActive %d\n", deviceStatus.deviceActive);
                
            // find the isoch characteristics for this device
            isochAtom = QTFindChildByIndex( deviceList, deviceAtom, kIDHIsochServiceAtomType, 1, nil);
            if( isochAtom == nil)
                    goto error;

            // how many configs exist for this device
            nConfigs = QTCountChildrenOfType( deviceList, isochAtom, kIDHIsochModeAtomType);
            printf("\n%d configs:\n", nConfigs);

            videoConfig.atom = nil;	// start with no selected config

            // process each config
            for( j=0; j<nConfigs; ++j)
            {
                    OSType mediaType;
                    QTAtom configAtom, mediaAtom;

                    // get this configs atom
                    configAtom = QTFindChildByIndex( deviceList, isochAtom, kIDHIsochModeAtomType, j + 1, nil);
                    if( configAtom == nil)
                            goto error;

                    printf("Config %d",j);
                    // find the media type atom
                    mediaAtom = QTFindChildByIndex( deviceList, configAtom, kIDHIsochMediaType, 1, nil);
                    if( mediaAtom == nil)
                            goto error;

                    // get the value of the mediaType atom
                    QTCopyAtomDataToPtr( deviceList, mediaAtom, true, sizeof( mediaType), &mediaType, &size);
                    print4(" Media type:", mediaType);

                    // is this config an video config?
                    if( mediaType == kIDHVideoMediaAtomType)	// found video device
                    {
                            videoConfig.container = deviceList;	// save this config
                            videoConfig.atom = configAtom;
                            break;
                    }
                    printf("\n");
            }
            printf("-----\n");

    }

    if( videoConfig.atom == nil)	// no good configs found
            goto error;

    printf("setting config\n");
    // set isoch to use this config
    err = IDHSetDeviceConfiguration( theInst, &videoConfig);
    if( err != noErr)
            goto error;
#if 1
    doControlTest(theInst, &videoConfig,
        //0xc3, //kAVCPlayOpcode
        //0x75 //kAVCPlayForward
        0xd0,	// Transport State
        0x7f
    );
#else
    {
        TimeRecord time1, time2;
        do {
            err = IDHGetDeviceTime(theInst, &time1);
            err = IDHGetDeviceTime(theInst, &time2);
            if(time2.value.lo > time1.value.lo+1) {
                //printf("read device time1, scale: %d, time 0x%x:0x%x\n",
               //     time1.scale, time1.value.hi, time1.value.lo);
                //printf("read device time2, scale: %d, time 0x%x:0x%x\n",
                //    time2.scale, time2.value.hi, time2.value.lo);
                    
                printf("Diff is %d\n", time2.value.lo-time1.value.lo);
            }
        } while (1);
    }
#endif
error:
    if( err != noErr)
        printf("error %d(0x%x)\n", err, err);
    if(deviceList) {
        QTUnlockContainer( deviceList);
        QTDisposeAtomContainer(deviceList);
    }

    CloseComponent(theInst);

}


int main(int argc, char **argv)
{
	UInt32 seed = GetComponentListModSeed();
	UInt32 num;
	Handle aName;
	ComponentDescription desc, aDesc;
	Component aComponent;

    int pos = 1;
            
	printf("Component seed is %d\n", seed);
        desc.componentType = 'ihlr';				/* A unique 4-byte code indentifying the command set */
	desc.componentSubType = 0;			/* Particular flavor of this instance */
	desc.componentManufacturer = 0;		/* Vendor indentification */
	desc.componentFlags = 0;				/* 8 each for Component,Type,SubType,Manuf/revision */
	desc.componentFlagsMask = 0;			/* Mask for specifying which flags to consider in search, zero during registration */

	num = CountComponents(&desc);
	printf("%d components match\n", num);
	
	aComponent = 0;
	aName = NewHandleClear(200);
	while (aComponent = FindNextComponent(aComponent, &desc)) {
		OSStatus oops;
		printf("Found component 0x%x:", aComponent);
		oops = GetComponentInfo(aComponent, &aDesc, aName,
                                         NULL, NULL);
        if(oops)
        	printf("GetComponentInfo() returned error %d\n", oops);
        else {
        	if(GetHandleSize(aName))
        		printP(*aName);
        	else
        		printf("Unnamed");
                print4(", Type ", aDesc.componentType);

                print4(", SubType ", aDesc.componentSubType);
                print4(", Manufacturer ", aDesc.componentManufacturer);
                printf("\n");
	}
        }

    num = 0;
    do {
        ComponentInstance theInst;

        theInst = OpenDefaultComponent('ihlr', 'dv  ');
        //printf("Instance is 0x%x\n", theInst);
        if(theInst == NULL)
                return;
        num++;
        usleep(50000);
        CloseComponent(theInst);
    } while (0);
	OpenDV();
	return 0;
}

