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
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iokit/IOKitLib.h>

#include "IsochronousDataHandler.h"
#include "DeviceControl.h"

#define WRITEBUFF 1

#if WRITEBUFF
#define SYNC 0
static Ptr myBuffer1, myBuffer2;
#endif

static int issued = 0;
static int completed = 0;
pthread_mutex_t		globalsMutex;			// lock this before updating globals
pthread_cond_t		syncCond;				// To synchronize threads.
int	finished=0;
static int file = 0;
static     QTAtomSpec videoConfig;
static int frameSize = 120000;	// NTSC 144000 PAL
static char *sFile;
static int sSDL;
static int sDVCPro;
static int sDVCPro50;
static int sFormat = 0;	// DV
static int sLoop;
static UInt64 sGUID = 0;

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

// called when a new isoch write is done
static OSStatus DVIsochComponentWriteCallback( IDHGenericEvent *eventRecord, void *userData)
{
    struct timeval tod;
        OSErr 					result = noErr;
        IDHParameterBlock		*pb = (IDHParameterBlock *) eventRecord;

        ComponentInstance	theInst = userData;
        if(file) {
            int len;
            len = read(file, pb->buffer, frameSize);
            //len = read(file, myBuffer, frameSize);
            if(len < frameSize) {
                pthread_mutex_lock(&globalsMutex);
                finished = 1;
                pthread_mutex_unlock(&globalsMutex);
                pthread_cond_broadcast(&syncCond);
                printf("Completed %d\n", completed++);
                return result;
            }
        }
#if WRITEBUFF
#else
        pb->buffer = nil;
#endif
        // fill out structure
        pb->requestedCount	= frameSize;
        pb->actualCount 	= 0;
        pb->completionProc 	= DVIsochComponentWriteCallback;
        // do another write
        //gettimeofday(&tod, NULL);
        //printf("Completed %d, issuing write %d @ %d:%d\n", completed++, issued, tod.tv_sec, tod.tv_usec);
        result = IDHWrite( theInst, pb);
        if( result != noErr) {
            printf("IDHWrite error %d\n", result);
        }
        issued++;
        return result;
}

static OSErr doWriteTest(ComponentInstance theInst)
{
    struct timeval tod;
    TimeRecord time;
    IDHParameterBlock isochParamBlock1, isochParamBlock2;
    ComponentResult err;
    int len;
    
    finished = 0;
    err = pthread_mutex_init(&globalsMutex, NULL);
    err = pthread_cond_init(&syncCond, NULL);

    file = open(sFile, O_RDONLY, 0666);
    printf("open file: %d\n", file);
#if WRITEBUFF
    if(!myBuffer1)
        myBuffer1 = NewPtr(frameSize);
    if(!myBuffer2)
        myBuffer2 = NewPtr(frameSize);
    read(file, myBuffer1, frameSize);
    isochParamBlock1.buffer 		= myBuffer1;
#else
    isochParamBlock1.buffer 		= nil;
#endif
    isochParamBlock1.requestedCount	= frameSize;	// NTSC buffer size
    isochParamBlock1.actualCount 	= 0;
    isochParamBlock1.refCon		= (void *)theInst;

#if SYNC
    isochParamBlock1.completionProc 	= nil;
#else
    isochParamBlock1.completionProc 	= DVIsochComponentWriteCallback;
#endif

    isochParamBlock2 = isochParamBlock1;
    
    // open the DV device for writing
    err = IDHOpenDevice( theInst, kIDHOpenForWriteTransactions);
    if( err != noErr)
            goto error;

    printf("Opened device\n");

    //err = IDHGetDeviceTime(theInst, &time);
    //if( err != noErr)
    //        goto error;
    //gettimeofday(&tod, NULL);
    //printf("issuing write %d @ %d:%d =  0x%x:0x%x\n",
    //issued, tod.tv_sec, tod.tv_usec, time.value.hi, time.value.lo);
    err = IDHWrite( theInst, &isochParamBlock1);
    if( err != noErr)
            goto error;
    err = IDHGetDeviceTime(theInst, &time);
    if( err != noErr)
            goto error;
    gettimeofday(&tod, NULL);
    printf("issued write %d @ %d:%d =  0x%x:0x%x\n",
    issued, tod.tv_sec, tod.tv_usec, time.value.hi, time.value.lo);
    issued++;
#if SEND2
#if WRITEBUFF
    read(file, myBuffer2, frameSize);
    isochParamBlock2.buffer 		= myBuffer2;
#endif
    err = IDHWrite( theInst, &isochParamBlock2);
    if( err != noErr)
            goto error;
    err = IDHGetDeviceTime(theInst, &time);
    if( err != noErr)
            goto error;
    gettimeofday(&tod, NULL);
    printf("issued write %d @ %d:%d =  0x%x:0x%x\n",
    issued, tod.tv_sec, tod.tv_usec, time.value.hi, time.value.lo);
    issued++;
#endif

#if SYNC
    do {
        len = read(file, myBuffer1, frameSize);
        if(len != frameSize) {
            finished = 1;
            break;
        }
        err = IDHWrite( theInst, &isochParamBlock1);
        if( err != noErr)
                goto error;
        err = IDHGetDeviceTime(theInst, &time);
        if( err != noErr)
                goto error;
        gettimeofday(&tod, NULL);
        printf("issued write %d @ %d:%d =  0x%x:0x%x\n",
        issued, tod.tv_sec, tod.tv_usec, time.value.hi, time.value.lo);
        issued++;
    } while (true);
#endif

    // Wait for work thread to finish initializing globals
    err = pthread_mutex_lock(&globalsMutex);
    while(!finished) {
        err = pthread_cond_wait(&syncCond, &globalsMutex);
    }
    err = pthread_mutex_unlock(&globalsMutex);

    // close the DV device
    err = IDHCloseDevice( theInst);
    if( err != noErr)
            goto error;
    printf("Did %d frames\n", issued);

    printf("Closed device\n");

    close(sFile);
    
error:
    if(err)
        printf("Error %d(0x%x) in doWriteTest\n", err, err);
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
                    QTAtom configAtom, mediaAtom, nameAtom;

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
                    nameAtom = QTFindChildByIndex( deviceList, configAtom, kIDHNameAtomType, 1, nil);
                    if( nameAtom != nil) {
                        QTCopyAtomDataToPtr( deviceList, nameAtom, true, 255, cameraName, &size);
                        cameraName[size] = 0;
                        printf(" name '%s' ", cameraName+1);
                    }
                    // is this config an video config?
                    if( mediaType == kIDHVideoMediaAtomType)	// found video device
                    {
                        QTAtom frameSizeAtom;
                        frameSizeAtom = QTFindChildByIndex( deviceList, configAtom,
                            kIDHDataBufferSizeAtomType, 1, nil);
                        // ignore DV_SDL config
                        if(strcmp(cameraName+1, "DV-SDL")) {
                            if(frameSizeAtom) {
                                QTCopyAtomDataToPtr( deviceList, frameSizeAtom, true, sizeof( frameSize), &frameSize, &size);
                                if(sSDL)
                                    frameSize /= 2;
								if (sDVCPro50)
									frameSize *= 2;
                                printf("Config buffer size %d\n", frameSize);
                            }
                            videoConfig.container = deviceList;	// save this config
                            videoConfig.atom = configAtom;
                        }
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

    IDHSetFormat( theInst, sFormat);

    err = doWriteTest(theInst);

    if( err != noErr)
            goto error;

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
    ComponentInstance theInst;
    ComponentResult version;

    int pos = 1;
            
    sFile = "/tmp/dump.dv";
    sSDL = 0;
	sDVCPro = 0;
	sDVCPro50 = 0;
 
    while(argc > pos) {
        if(strcmp(argv[pos], "-sdl") == 0)
            sSDL = 1;
        else if(strcmp(argv[pos], "-DVCPro") == 0)
            sDVCPro = 1;
        else if(strcmp(argv[pos], "-DVCPro50") == 0)
            sDVCPro50 = 1;
        else if(strcmp(argv[pos], "-l") == 0)
            sLoop = 1;
        else if(strcmp(argv[pos], "-guid") == 0 && argc > pos + 1) {
            pos++;
            sGUID = strtoq(argv[pos], NULL, 0);
        }
        else
            sFile = argv[pos];
        pos++;
    }

    if(sSDL) {
        frameSize /= 2;
        sFormat = kIDHDV_SDL;
    }
    else if(sDVCPro) {
        sFormat = kIDHDVCPro_25;
    }
    else if(sDVCPro50) {
		frameSize *= 2;
        sFormat = kIDHDVCPro_50;
    }
	
	
    printf("Reading from %s\n", sFile);
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

    do {
        OpenDV();
    } while (sLoop);
	return 0;
}

