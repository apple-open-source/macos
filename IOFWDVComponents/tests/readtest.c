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
#define __PRINT__ // Print headers don't compile for some reason
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

#define PAGE_SIZE 4096

static int done = 0;
static int file = 0;
static QTAtomSpec videoConfig;
static IDHDeviceID deviceID;
static IDHNotificationID notificationID;
static int frameSize = 120000;	// NTSC 144000 PAL
static char *sFile;
static int sSDL;
static UInt64 sGUID = 0;
static int sDVCPro50;


static char diskBuffer[PAGE_SIZE];
static int diskBufferEnd;

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
    if(event->eventHeader.event == kIDHEventFrameDropped) {
        IDHDeviceFrameDroppedEvent *drop = (IDHDeviceFrameDroppedEvent *)event;
        printf("Just dropped %d frames, total %d\n",
                                        drop->newlyDropped, drop->totalDropped);
    }
        
        // Reenable notification
   IDHNotifyMeWhen(theInst, event->eventHeader.notificationID, kIDHEventEveryEvent);

    return noErr;
}

// write to disk in 4k chunks.  This keeps the FS from having to read into memory
// a partial page to append new data onto.  this keeps disk latencies down, however
// it is not a full solution as we can't guarantee disk latencies especially on
// a heavily loaded machine. we should really put this on a separate thread.

static void bufferedWrite( char * file, char * buffer, ByteCount length )
{
	ByteCount writeLength = length;
	ByteCount buffer1Length = 0;
	ByteCount buffer2Length = 0;
	
	// assumes length of write is > PAGE_SIZE
	
	if( diskBufferEnd != 0 )
	{
		buffer1Length = PAGE_SIZE - diskBufferEnd;
		
		// fill up our buffer
		bcopy( buffer, diskBuffer + diskBufferEnd, buffer1Length );
		
		// flush it to disk
		if(file)
            write(file, diskBuffer, PAGE_SIZE);
        
		diskBufferEnd = 0;
		writeLength -= buffer1Length;
	}
	
	// buffer is now empty and totalLength is how much we have left to write

	buffer2Length = writeLength % PAGE_SIZE;
	writeLength -= buffer2Length;
	
	// write bulk
	
	if(file)
		write(file, buffer + buffer1Length, writeLength);
    
	// buffer remainder
	
	if( buffer2Length != 0 )
	{
		bcopy( buffer + buffer1Length + writeLength, diskBuffer, buffer2Length );
		diskBufferEnd = buffer2Length;
	}
	
//	printf( "flush with %d bytes, write %d bytes, buffer %d bytes\n", buffer1Length, writeLength, buffer2Length );
	
}

static void flushDiskBuffer( void )
{
	// flush it to disk
	if(file)
		write(file, diskBuffer, diskBufferEnd);
}

// called when a new isoch read is received
static UInt32 lastCall;
static OSStatus DVIsochComponentReadCallback( IDHGenericEvent *eventRecord, void *userData)
{
        OSStatus				result = noErr;
        IDHParameterBlock		*pb = (IDHParameterBlock *) eventRecord;

    
#if 1
    SInt32 delta, deltaCall;
        ComponentInstance	theInst = userData;
    TimeRecord time1;
    IDHGetDeviceTime(theInst, &time1);
    
    delta = (time1.value.lo % 8000) - (pb->reserved1 % 8000);
    if(delta < 0)
        delta += 8000;
    deltaCall = (time1.value.lo % 8000) - (lastCall % 8000);
    if(deltaCall < 0)
        deltaCall += 8000;
#if 0
    printf("frame %p was received at %x, time now is %x:%x, delta %d, delta from last time %d\n",
        pb->buffer, pb->reserved1, time1.value.hi, time1.value.lo, delta, deltaCall);
#endif
        lastCall = time1.value.lo;

#if 0
	{
		CFAbsoluteTime cstart, cend;
		cstart = CFAbsoluteTimeGetCurrent();
#endif

		bufferedWrite( file, pb->buffer, pb->actualCount );

#if 0
		if(file)
            write(file, pb->buffer, pb->actualCount);
            //write(file, pb->buffer, frameSize);
#endif

#if 0
			cend = CFAbsoluteTimeGetCurrent();
			printf( "write(file) %d bytes took %8.3f\n", pb->actualCount, cend-cstart );
	}
#endif

   
       
        result = IDHReleaseBuffer( theInst, pb);
        // fill out structure
        pb->buffer 			= NULL;
        pb->requestedCount	= frameSize;
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

static void doControlTest(ComponentInstance theInst, QTAtomSpec *currentIsochConfig, UInt8 op1, UInt8 op2)
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
        devStatus.version = 0x200;
        result = IDHGetDeviceStatus( theInst, currentIsochConfig, &devStatus);
        if(result)
                goto Exit;
        printf("input format is 0x%x\n", devStatus.inputFormat);
        printf("output formats are 0x%x\n", devStatus.outputFormats);
                
        //result = FWClockPrivSetFWReferenceID(clockInst, (FWReferenceID) devStatus.localNodeID );
        //if(result)
        //	goto Exit;

        // set the clock's fw id
        //clockInst = OpenDefaultComponent(clockComponentType, systemMicrosecondClock);

        if(!controlInst)
                goto Exit;


        // fill up the avc frame
        in[0]	= kAVCControlCommand;
        in[1] 	= IOAVCAddress(kAVCTapeRecorder, 0);
        in[2] 	= op1;
        in[3] 	= op2;

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

Exit:
        if(result != noErr)
                printf("Control error %d(%x)\n", result, result);
}

static OSStatus doTimeTest(ComponentInstance theInst)
{
    Ptr myBuffer;
    IDHParameterBlock isochParamBlock;
    TimeRecord time1, time2;
    OSStatus err;
    
    // open the DV device for reading
    err = IDHOpenDevice( theInst, kIDHOpenForReadTransactions);
    if( err != noErr)
            goto error;

    printf("Opened device\n");

    printf("GetDeviceTime\n");
    err = IDHGetDeviceTime(theInst, &time1);
    while(true) {
        printf("GetDeviceTime\n");
        err = IDHGetDeviceTime(theInst, &time2);
        if( err != noErr)
                goto error;
        printf("read device time, scale: %d, time 0x%x:0x%x\n",
            time2.scale, time2.value.hi, time2.value.lo);
        printf("Delta is %d.%d\n", time2.value.hi-time1.value.hi, time2.value.lo-time1.value.lo);
        time1 = time2;
        sleep(10);
    }

    // close the DV device
    err = IDHCloseDevice( theInst);
    if( err != noErr)
            goto error;

    printf("Closed device\n");

error:
    return err;
}

static OSStatus startReadTest(ComponentInstance theInst, char *fileName, IDHParameterBlock *isochParamBlock)
{
    Ptr myBuffer;
    TimeRecord time;
    OSStatus err;
    
    // open the DV device for reading
    err = IDHOpenDevice( theInst, kIDHOpenForReadTransactions);
    if( err != noErr)
            goto error;

    printf("Opened device\n");

    doControlTest(theInst, &videoConfig,
        0xc3, //kAVCPlayOpcode
        0x75 //kAVCPlayForward
    );
    printf("GetDeviceTime\n");
    err = IDHGetDeviceTime(theInst, &time);
    if( err != noErr)
            goto error;
    printf("read device time, scale: %d, time 0x%x:0x%x\n",
        time.scale, time.value.hi, time.value.lo);

	diskBufferEnd = 0;
    file = open(fileName, O_CREAT | O_WRONLY | O_TRUNC, 0666);
	
	// non-cached IOs will stream to the disk more smoothly, while
	// cached IOs will be more bursty as the cache is periodically 
	// flushed to disk.
	
	fcntl( file, F_NOCACHE );
	
#if 0
    {
        int i;
        // we are doing isoch reads with only one buffer at a time
        //myBuffer = NewPtrClear(frameSize);

        for(i=0; i<1000; i++) {

           // isochParamBlock.buffer 		= myBuffer;
            isochParamBlock->buffer 		= nil;
            isochParamBlock->requestedCount	= frameSize;	// NTSC buffer size
            isochParamBlock->actualCount 	= 0;
            isochParamBlock->refCon		= (void *)0x12345678;


            isochParamBlock->completionProc 	= 0;

            err = IDHRead( theInst, isochParamBlock);
            if( err != noErr)
                    goto error;
            write(file, isochParamBlock->buffer, isochParamBlock->actualCount);
            err = IDHReleaseBuffer( theInst, isochParamBlock);
            if( err != noErr)
                    goto error;
        }

    }
#else
    isochParamBlock->buffer 		= nil;
    isochParamBlock->requestedCount	= frameSize;	// NTSC buffer size
    isochParamBlock->actualCount 	= 0;
    isochParamBlock->refCon		= (void *)theInst;


    isochParamBlock->completionProc 	= DVIsochComponentReadCallback;

    err = IDHRead( theInst, isochParamBlock);
    if( err != noErr)
            goto error;
    printf("Issued read\n");

    while(!done)
        sleep(1);
#endif

error:
    return err;
}

static OSStatus stopReadTest(ComponentInstance theInst)
{
    Ptr myBuffer;
    IDHParameterBlock isochParamBlock;
    TimeRecord time;
    OSStatus err;
    	
//    err = IDHReleaseBuffer( theInst, &isochParamBlock);
    err = IDHGetDeviceTime(theInst, &time);
    if( err != noErr)
            goto error;
    printf("read device time, scale: %d, time 0x%x:0x%x\n",
        time.scale, time.value.hi, time.value.lo);

    doControlTest(theInst, &videoConfig,
        0xc4, //kAVCWindOpcode
        0x60 //kAVCWindStop
    );
	
    // close the DV device
    err = IDHCloseDevice( theInst);
    if( err != noErr)
        goto error;

    err = IDHGetDeviceTime(theInst, &time);
    if( err != noErr)
            goto error;
    printf("Closed device, time 0x%x:0x%x\n", time.value.hi, time.value.lo);
    printf("Did %d frames\n", done);

	flushDiskBuffer();
	close( file );
	
error:
    return err;
}

static void OpenDV()
{
    ComponentInstance theInst, theInst2;
    IDHParameterBlock isochParamBlock1, isochParamBlock2;
    ComponentResult version;
    QTAtomContainer deviceList = NULL;
    short nDVDevices, i, j;
    QTAtom deviceAtom;
    UInt32 cmpFlag;
    UInt32 isoversion;
    long size;
    UInt32 format;
    OSStatus err;
    
    theInst = OpenDefaultComponent('ihlr', 'dv  ');
    printf("Instance is 0x%x\n", theInst);
        if(theInst == NULL)
                return;
                
#ifdef TWO
    theInst2 = OpenDefaultComponent('ihlr', 'dv  ');
    printf("Instance2 is 0x%x\n", theInst2);
        if(theInst2 == NULL)
                return;
#endif
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
            UInt64 test;
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
            QTCopyAtomDataToPtr( deviceList, dataAtom, true, sizeof( test), &test, &size);
            printf("guid 0x%016llx ", test);

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
            printf("supported DV types %x\n", deviceStatus.outputFormats);

            if(deviceStatus.inputStandard == ntscIn)
                frameSize = 120000;
            else if(deviceStatus.inputStandard == palIn)
                frameSize = 144000;
            else
                printf("Unknown standard: %d\n", deviceStatus.inputStandard);
            if(sSDL)
                frameSize /= 2;
            else if(sDVCPro50)
                frameSize *= 2;
			
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
            if(sGUID == test)
                break;
    }

    if( videoConfig.atom == nil)	// no good configs found
            goto error;

    printf("setting config\n");
    // set isoch to use this config
    err = IDHSetDeviceConfiguration( theInst, &videoConfig);
    if( err != noErr)
            goto error;

    err = IDHGetFormat( theInst, &format);
    printf("IDHGetFormat returned err %d, format %d\n", err, format);

#ifdef TWO
    err = IDHSetDeviceConfiguration( theInst2, &videoConfig);
    if( err != noErr)
            goto error;
#endif

    //err = doTimeTest(theInst);
    err = startReadTest(theInst, sFile, &isochParamBlock1);
    if( err != noErr)
            goto error;
    //err = startReadTest(theInst2, "/tmp/dump2.dv", &isochParamBlock2);
    //if( err != noErr)
    //        goto error;
    
    while(done < 300)
        sleep((300-done)/30);

    err = IDHDisposeNotification(theInst, notificationID);
    if( err != noErr)
            goto error;

    stopReadTest(theInst);
    //stopReadTest(theInst2);
    
error:
    if( err != noErr)
        printf("error %d(0x%x)\n", err, err);
    if(deviceList) {
        QTUnlockContainer( deviceList);
        QTDisposeAtomContainer(deviceList);
    }

    //usleep(200000);

    //CloseComponent(theInst);

}


int main(int argc, char **argv)
{
	UInt32 seed = GetComponentListModSeed();
	UInt32 num;
	Handle aName;
	ComponentDescription desc, aDesc;
	Component aComponent;

    int pos = 1;
            
    sFile = "/tmp/dump.dv";
    sSDL = 0;
	sDVCPro50 = 0;
    
    while(argc > pos) {
        if(strcmp(argv[pos], "-sdl") == 0)
            sSDL = 1;
        else if(strcmp(argv[pos], "-guid") == 0 && argc > pos + 1) {
            pos++;
            sGUID = strtoq(argv[pos], NULL, 0);
        }
		else if (strcmp(argv[pos], "-DVCPro50") == 0)
            sDVCPro50 = 1;
        else
            sFile = argv[pos];
        pos++;
    }
    printf("Dumping to %s\n", sFile);
        
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

{
    int iters = 0;
    do {
	OpenDV();
    iters++;
    printf("======== Iteration %d done ==========\n", iters);
    done = 200;
    }
    while (0);
}
	return 0;
}

