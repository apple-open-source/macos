#include <Carbon/Carbon.h>
#include <QuickTime/QuickTime.h>

#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iokit/IOKitLib.h>

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

    QTUnlockContainer( deviceList);
    deviceList = NULL;


    printf("setting config\n");
    // set isoch to use this config
    err = IDHSetDeviceConfiguration( theInst, &videoConfig);
    if( err != noErr)
            goto error;

    doControlTest(theInst, &videoConfig,
        0xc3, //kAVCPlayOpcode
        0x75 //kAVCPlayForward
    );

error:
    if( err != noErr)
        printf("error %d(0x%x)\n", err, err);
    if(deviceList) {
            QTUnlockContainer( deviceList);
    }

    CallComponentClose(theInst, 0);

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

