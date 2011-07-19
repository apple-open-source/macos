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
/*
 cc -framework QuickTime -I/System/Library/Extensions/IOFWDV.kext/Headers -I/System/Library/Frameworks/QuickTime.framework/Headers -I/System/Library/Frameworks/Carbon.framework/Headers testcomp.c /System/Library/Extensions/IOFWDV.kext/libIOFWDV.a -o testcomp
 */

#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iokit/IOKitLib.h>

//#define TARGET_CARBON 1
#include <Carbon/Carbon.h>

//#include <Carbon/MacMemory.h>
//#include <Carbon/Components.h>

#include "IsochronousDataHandler.h"
#include "DeviceControl.h"

static int done = 0;
static int file = 0;
static     QTAtomSpec videoConfig;

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

// called when a new isoch read is received
static OSStatus DVIsochComponentReadCallback( IDHGenericEvent *eventRecord, void *userData)
{
        OSErr 					result = noErr;
        IDHParameterBlock		*pb = (IDHParameterBlock *) eventRecord;

#if 1
        ComponentInstance	theInst = userData;

        if(file)
            write(file, pb->buffer, pb->actualCount);
        
        result = IDHReleaseBuffer( theInst, pb);
        // fill out structure
        pb->buffer 			= NULL;
        pb->requestedCount	= 120000;
        pb->actualCount 	= 0;
        pb->completionProc 	= DVIsochComponentReadCallback;
        // do another read
        result = IDHRead( theInst, pb);
        if( result != noErr) {
            printf("IDHRead error %d\n", result);
        }
#else
        printf("read complete for block 0x%x, refcon 0x%x\n", pb, userData);
#endif
        done++;
        return result;
}

// called when a new isoch read is received
static OSStatus DVIsochComponentWriteCallback( IDHGenericEvent *eventRecord, void *userData)
{
        OSErr 					result = noErr;
        IDHParameterBlock		*pb = (IDHParameterBlock *) eventRecord;

#if 1
        ComponentInstance	theInst = userData;

        if(file) {
            int len;
            len = read(file, pb->buffer, 120000);
            if(len < 120000)
                return result;
        }
#if WRITEBUFF
#else
        pb->buffer = nil;
#endif
        // fill out structure
        pb->requestedCount	= 120000;
        pb->actualCount 	= 0;
        pb->completionProc 	= DVIsochComponentWriteCallback;
        // do another write
        result = IDHWrite( theInst, pb);
        if( result != noErr) {
            printf("IDHWrite error %d\n", result);
        }
#else
        printf("write complete for block 0x%x, refcon 0x%x\n", pb, userData);
#endif
        done++;
        return result;
}

static void doControlTest(ComponentInstance theInst, QTAtomSpec *currentIsochConfig)
{
        //Component control;
        ComponentInstance controlInst;
        ComponentResult result;
        IDHDeviceStatus			devStatus;
        DVCTransactionParams 	pParams;
        char					in[4], out[16];
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


        // fill up the avc frame
        in[0]	= 0x00; //kAVCControlCommand;
        in[1] 	= 0x20;						// for now
        in[2] 	= 0xc3; //kAVCPlayOpcode;
        in[3] 	= 0x75; //kAVCPlayForward;

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
    } while(result != kIOReturnSuccess);
    
    //sleep(10);
    CallComponentClose(controlInst, 0);

Exit:
        if(result != noErr)
                printf("Control error %d(%x)\n", result, result);
}

static OSErr doReadTest(ComponentInstance theInst)
{
    Ptr myBuffer;
    IDHParameterBlock isochParamBlock;
    OSErr err;
    
    // open the DV device for reading
    err = IDHOpenDevice( theInst, kIDHOpenForReadTransactions);
    if( err != noErr)
            goto error;

    printf("Opened device\n");
    doControlTest(theInst, &videoConfig);

    file = open("/tmp/dump.rawdv", O_CREAT | O_WRONLY | O_TRUNC, 0666);
#if 0
    {
        int i;
        // we are doing isoch reads with only one buffer at a time
        //myBuffer = NewPtrClear(120000);

        for(i=0; i<1000; i++) {

           // isochParamBlock.buffer 		= myBuffer;
            isochParamBlock.buffer 		= nil;
            isochParamBlock.requestedCount	= 120000;	// NTSC buffer size
            isochParamBlock.actualCount 	= 0;
            isochParamBlock.refCon		= (void *)0x12345678;


            isochParamBlock.completionProc 	= 0;

            err = IDHRead( theInst, &isochParamBlock);
            if( err != noErr)
                    goto error;
            write(file, isochParamBlock.buffer, 120000);
            err = IDHReleaseBuffer( theInst, &isochParamBlock);
            if( err != noErr)
                    goto error;
        }

    }
#else
    isochParamBlock.buffer 		= nil;
    isochParamBlock.requestedCount	= 120000;	// NTSC buffer size
    isochParamBlock.actualCount 	= 0;
    isochParamBlock.refCon		= (void *)theInst;


    isochParamBlock.completionProc 	= DVIsochComponentReadCallback;

    err = IDHRead( theInst, &isochParamBlock);
    if( err != noErr)
            goto error;
    printf("Issued read\n");

    while(!done)
            sleep(1);
    sleep(10);
    printf("Did %d frames\n", done);
//    err = IDHReleaseBuffer( theInst, &isochParamBlock);
#endif
    // close the DV device
    err = IDHCloseDevice( theInst);
    if( err != noErr)
            goto error;

    printf("Closed device\n");

    printf("Read %d bytes\n", isochParamBlock.actualCount);
    if(isochParamBlock.actualCount)
    {
            int i,j;
            UInt8 *p = (UInt8 *)isochParamBlock.buffer;
            for(i=0; i<100; i++) {
                    printf("%d: ", i*40);
                    for(j=0; j<40; j++)
                            printf("%2x ",*p++);
                    printf("\n");
            }
    }
error:
    return err;
}

static OSErr doWriteTest(ComponentInstance theInst)
{
    Ptr myBuffer;
    IDHParameterBlock isochParamBlock;
    OSErr err;

    // open the DV device for writing
    err = IDHOpenDevice( theInst, kIDHOpenForWriteTransactions);
    if( err != noErr)
            goto error;

    printf("Opened device\n");

    myBuffer = NewPtrClear(120000);
    file = open("/work/dinosaur.rawdv", O_RDONLY, 0666);
    printf("open file: %d\n", file);
#if 0
    {
        int i;
        // we are doing isoch reads with only one buffer at a time

        for(i=0; i<1000; i++) {
            file = open("/work/dinosaur.rawdv", O_RDONLY, 0666);
            printf("open file: %d\n", file);
            while(true) {
                int len;
                len = read(file, myBuffer, 120000);
                if(len < 120000)
                    break;
                isochParamBlock.buffer 		= myBuffer;
                //isochParamBlock.buffer 		= nil;
                isochParamBlock.requestedCount	= 120000;	// NTSC buffer size
                isochParamBlock.actualCount 	= 0;
                isochParamBlock.refCon		= (void *)0x12345678;

                isochParamBlock.completionProc 	= 0;

                err = IDHWrite( theInst, &isochParamBlock);
                if( err != noErr)
                        goto error;
            }
            close(file);
            file = open("/work/dinosaur.rawdv", O_RDONLY, 0666);
            printf("open file: %d\n", file);
        }
    }
#else
#if WRITEBUFF
    read(file, myBuffer, 120000);
    isochParamBlock.buffer 		= myBuffer;
#else
    isochParamBlock.buffer 		= nil;
#endif
    isochParamBlock.requestedCount	= 120000;	// NTSC buffer size
    isochParamBlock.actualCount 	= 0;
    isochParamBlock.refCon		= (void *)theInst;


    isochParamBlock.completionProc 	= DVIsochComponentWriteCallback;

    err = IDHWrite( theInst, &isochParamBlock);
    if( err != noErr)
            goto error;
    printf("Issued write\n");

    while(!done)
            sleep(1);
    sleep(100);
    printf("Did %d frames\n", done);
#endif
    // close the DV device
    err = IDHCloseDevice( theInst);
    if( err != noErr)
            goto error;

    printf("Closed device\n");

error:
    return err;
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
    OSErr err;

    theInst = OpenDefaultComponent('ihlr', 'dv  ');
    printf("Instance is 0x%x\n", theInst);
        if(theInst == NULL)
                return;

    version = CallComponentVersion(theInst);
    printf("Version is 0x%x\n", version);

    do {
        err = IDHGetDeviceList( theInst, &deviceList);
        if( err != noErr)
                goto error;

        nDVDevices = QTCountChildrenOfType( deviceList, kParentAtomIsContainer, kIDHDeviceAtomType);
        if(nDVDevices > 0)
            break;
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
            IDHDeviceID deviceID;
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
                            //break;
                    }
                    printf("\n");
            }
            printf("-----\n");

    }

    if( videoConfig.atom == nil)	// no good configs found
            goto error;

    QTUnlockContainer( deviceList);
    deviceList = NULL;

    printf("setting config\n");
    // set isoch to use this config
    err = IDHSetDeviceConfiguration( theInst, &videoConfig);
    if( err != noErr)
            goto error;

#if 1
    err = doReadTest(theInst);
#else
    err = doWriteTest(theInst);
#endif
    if( err != noErr)
            goto error;

error:
    if( err != noErr)
        printf("error %d(0x%x)\n", err, err);
    if(deviceList) {
            QTUnlockContainer( deviceList);
    }

    CallComponentClose(theInst, 0);

}


int main(void)
{
	UInt32 seed = GetComponentListModSeed();
	UInt32 num;
	Handle aName;
	ComponentDescription desc, aDesc;
	Component aComponent;
        ComponentInstance theInst;
        ComponentResult version;
        
	printf("Component seed is %d\n", seed);
	desc.componentType = 0;				/* A unique 4-byte code indentifying the command set */
        //desc.componentType = 'ihlr';				/* A unique 4-byte code indentifying the command set */
	desc.componentSubType = 0;			/* Particular flavor of this instance */
	desc.componentManufacturer = 0;		/* Vendor indentification */
	desc.componentFlags = 0;				/* 8 each for Component,Type,SubType,Manuf/revision */
	desc.componentFlagsMask = 0;			/* Mask for specifying which flags to consider in search, zero during registration */

	num = CountComponents(&desc);
	printf("%d components match\n", num);
	
	aComponent = 0;
	aName = NewHandleClear(200);
	while (aComponent = FindNextComponent(aComponent, &desc)) {
		OSErr oops;
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

	OpenDV();
	return 0;
}

