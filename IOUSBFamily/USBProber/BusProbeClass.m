/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
                                                               * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#import "BusProbeClass.h"
#import <stdio.h>
#import <unistd.h>
#import <CoreServices/CoreServices.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/usb/IOUSBLib.h>
#import <IOKit/IOCFPlugIn.h>
#import <mach/mach_port.h>

static Node *busprobeRootNode = nil;
static NSMutableDictionary *vendorNamesDictionary = nil;

@implementation BusProbeClass

+(Node *)busprobeRootNode
{
    return busprobeRootNode;
}


// ________________________________________________________________________________________________
//	USBProbe
//
//	Scan all USB devices
+(void)USBProbe
{
    NSAutoreleasePool *pool=[[NSAutoreleasePool alloc] init];
    CFDictionaryRef matchingDict = NULL;

    mach_port_t				mMasterDevicePort = NULL;
    io_iterator_t			devIter = NULL;
    io_service_t			ioDeviceObj	= NULL;
    int 				outlineViewDeviceNumber = 0; //used to iterate through devices

    [busprobeRootNode clearNode];

    [busprobeRootNode setItemName: @"USB Bus Devices"];
    [busprobeRootNode setItemValue: NULL];

    outlineViewDeviceNumber = 0;

    // This gets the master device mach port through which all messages
    // to the kernel go, and initiates communication with IOKit.
    require_noerr(IOMasterPort(MACH_PORT_NULL, &mMasterDevicePort), errexit);

    // Create matching dictionary for IOUSBDevice's
    matchingDict = IOServiceMatching(kIOUSBDeviceClassName);
    require(matchingDict != NULL, errexit);

    // Get device iterator
    require_noerr(
                  IOServiceGetMatchingServices(
                                               mMasterDevicePort,
                                               matchingDict,		// reference consumed
                                               &devIter), errexit);


    // Walk through devices
    while ((ioDeviceObj = IOIteratorNext(devIter)) != NULL) {
        IOCFPlugInInterface 	**ioPlugin;
        IOUSBDeviceInterface 	**deviceIntf = NULL;
        IOReturn	 			kr;
        SInt32 					score;
        UInt32					locationID;

        // Get self pointer to device
        require_noerr(IOCreatePlugInInterfaceForService(
                                                        ioDeviceObj, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID,
                                                        &ioPlugin, &score), nextDevice);

        kr = (*ioPlugin)->QueryInterface(ioPlugin, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID), (LPVOID *)&deviceIntf);
        (*ioPlugin)->Release(ioPlugin);
        ioPlugin = NULL;
        require_string(kr == kIOReturnSuccess, nextDevice, "QueryInterface failed");

        verify_noerr((*deviceIntf)->GetLocationID(deviceIntf, &locationID));

        [self outputDevice:deviceIntf locationID:locationID deviceNumber:outlineViewDeviceNumber];
        outlineViewDeviceNumber++;


nextDevice:
            if (deviceIntf != NULL)
                (*deviceIntf)->Release(deviceIntf);
        IOObjectRelease(ioDeviceObj);
    }


errexit:
        if (devIter != NULL)
            IOObjectRelease(devIter);


    if (mMasterDevicePort)
        mach_port_deallocate(mach_task_self(), mMasterDevicePort);

    [pool release];
}

// ________________________________________________________________________________________________
//	outputDevice
//
//	Output all of a device's descriptors
+(void)outputDevice:(IOUSBDeviceInterface **)deviceIntf locationID:(UInt32)locationID deviceNumber:(int)deviceNumber
{
    Node *newNode;

    char str[500];
    int len;
    USBClass *deviceClass = NULL;
    USBClass *interfaceClass = NULL;
    NSString *tempString1, *tempString2, *tempString3, *tempString4;
    UInt8 lastInterfaceClass = 0;
    int	currentInterfaceNum = 0;
    UInt8 lastInterfaceSubClass = 0;
    IOUSBDeviceDescriptor dev;
        int iconfig;
        NSString *aTempString;
        char tempCString[500];


    len = GetDescriptor(deviceIntf, kUSBDeviceDesc, 0, &dev, sizeof(dev));

    if (len > 0) {

        Swap16(&dev.bcdUSB);
        Swap16(&dev.idVendor);
        Swap16(&dev.idProduct);
        Swap16(&dev.bcdDevice);

        // Create a new child node for this device
        newNode =  [[Node alloc] init];
        [newNode setItemName:[NSString stringWithFormat:@"USB device @ 0x%08lX: .............................................",locationID]]; // Set the node's name to be the device's location


        [newNode setItemValue:NULL]; // since I'm not actually gathering anything yet
        [busprobeRootNode addChild:newNode];
        [newNode release];

        [self PrintKeyVal:"Device Descriptor" val:"" forDevice:deviceNumber atDepth:DEVICE_DESCRIPTOR_LEVEL-1 forNode:busprobeRootNode];

        NUM(dev, "Descriptor Version Number:", bcdUSB, deviceNumber, DEVICE_DESCRIPTOR_LEVEL, 0);
        deviceClass = [self ClassAndSubClass:"Device" pcls:&dev.bDeviceClass forDevice:deviceNumber atDepth:1];
        NUM(dev, "Device Protocol", bDeviceProtocol, deviceNumber, DEVICE_DESCRIPTOR_LEVEL, 1);
        NUM(dev, "Device MaxPacketSize:", bMaxPacketSize0, deviceNumber, DEVICE_DESCRIPTOR_LEVEL, 1);

        aTempString = RETURNNUM(dev, idVendor, 0);
        sprintf(tempCString,"%s/",[aTempString cString]);
        aTempString = RETURNNUM(dev, idProduct, 0);
        strcat(tempCString,[aTempString cString]);

        tempString1 = RETURNNUM(dev, idVendor, 0);
        tempString2 = RETURNNUM(dev, idProduct, 0);
        tempString3 = RETURNNUM(dev, idVendor, 1);
        aTempString = [NSString stringWithFormat:@"%@/%@   (%@)",tempString1,tempString2,[self vendorNameFromVendorID:tempString3]];

        [self PrintKeyVal:"Device VendorID/ProductID:" val:(char *)[aTempString cString] forDevice:deviceNumber atDepth:1 forNode:busprobeRootNode];
        //[self PrintKeyVal:"Device VendorID/ProductID:" val:tempCString forDevice:deviceNumber atDepth:1 forNode:busprobeRootNode];

        NUM(dev, "Device Version Number:", bcdDevice, deviceNumber, DEVICE_DESCRIPTOR_LEVEL, 0);

        NUM(dev, "Number of Configurations:", bNumConfigurations, deviceNumber, DEVICE_DESCRIPTOR_LEVEL, 1);

        STR(dev, "Manufacturer String:", iManufacturer, deviceNumber, 1);

        STR(dev, "Product String:", iProduct, deviceNumber, 1);

        STR(dev, "Serial Number String:", iSerialNumber, deviceNumber, 1);

        // Add the string for the kind of device that it is.  We look at the class of the device
        // and then add the product name.  If the product name is blank (iProduct is 0), then we
        // put the vendor name from the database in the string
        //
        // Examples:
        //
        // 	Composite Device: "Apple Extended USB Keyboard"
        // 	Hub device from Atmel Corporation
        //	Vendor-specific device from unknown vendor
        //
        tempString1 = [deviceClass className];

        // If our subclass name is different than our class name, then add the sub class to the description
        // following a "/"
        //
        if( ! [[deviceClass subClassName] isEqualToString:@""] &&
            ! [[deviceClass subClassName] isEqualToString:[deviceClass className]] ) {
            tempString1 = [[tempString1 stringByAppendingString:@"/"] stringByAppendingString:[deviceClass subClassName]];
        }

        tempString2 = RETURNSTR(dev, iProduct);

        tempString3 = [tempString1 stringByAppendingString:@" device: "];

        tempString4 = RETURNNUM(dev, idVendor, 1);

        if ([tempString2 isEqualToString:@"0x00"])
        {
            tempString2 = [NSString stringWithFormat:@"%@",[self vendorNameFromVendorID:tempString4]];
            if ([tempString2 isEqualToString:@"0x00"])
                tempString2 = @"(unnamed)";
            else
                tempString3 = [tempString1 stringByAppendingString:@" device from "];
        }

        [[busprobeRootNode childAtIndex:deviceNumber] setItemValue:[tempString3 stringByAppendingString:tempString2]];

        for (iconfig = 0; iconfig < dev.bNumConfigurations; ++iconfig) {
            IOUSBConfigurationDescriptor cfg;

            len = GetDescriptor(deviceIntf, kUSBConfDesc, iconfig, &cfg, sizeof(cfg));
            if (len > 0) {
                /*	struct IOUSBConfigurationDescriptor {
                UInt8 			bLength;
                UInt8 			bDescriptorType;
                UInt16 			wTotalLength;
                UInt8 			bNumInterfaces;
                UInt8 			bConfigurationValue;
                UInt8 			iConfiguration;
                UInt8 			bmAttributes;
                UInt8 			MaxPower;
                }; */

                Byte *configBuf;
                Byte *p, *pend;
                char configHeading[500];

                aTempString = RETURNSTR(cfg, iConfiguration);
                if (strcmp([aTempString cString], "0x00") != 0) {
                    sprintf(configHeading, "Configuration Descriptor: .......................................");
                    sprintf(tempCString, [aTempString cString]);
                    [self PrintKeyVal:configHeading val:tempCString forDevice:deviceNumber atDepth:CONFIGURATION_DESCRIPTOR_LEVEL-1 forNode:busprobeRootNode];
                }
                else {
                    sprintf(configHeading, "Configuration Descriptor");
                    [self PrintKeyVal:configHeading val:"" forDevice:deviceNumber atDepth:CONFIGURATION_DESCRIPTOR_LEVEL-1 forNode:busprobeRootNode];
                }

                Swap16(&cfg.wTotalLength);
                NUM(cfg, "Total Length of Descriptor:", wTotalLength, deviceNumber, CONFIGURATION_DESCRIPTOR_LEVEL, 1);
                NUM(cfg, "Number of Interfaces:", bNumInterfaces, deviceNumber, CONFIGURATION_DESCRIPTOR_LEVEL, 1);
                NUM(cfg, "Configuration Value:", bConfigurationValue, deviceNumber, CONFIGURATION_DESCRIPTOR_LEVEL, 1);
                sprintf(str, "0x%02X", cfg.bmAttributes);
                if (cfg.bmAttributes & 0x40) {
                    strcat(str, " (self-powered");
                }
                else {
                    strcat(str, " (bus-powered");
                }
                if (cfg.bmAttributes & 0x20) {
                    strcat(str, ", remote wakeup");
                }
                strcat(str, ")");
                [self PrintKeyVal:"Attributes:" val:str forDevice:deviceNumber atDepth:CONFIGURATION_DESCRIPTOR_LEVEL forNode:busprobeRootNode];


                aTempString = RETURNNUM(cfg, MaxPower, 1);
                sprintf(tempCString, "%d ma", [aTempString intValue]*2);
                [self PrintKeyVal:"MaxPower:" val:tempCString forDevice:deviceNumber atDepth:CONFIGURATION_DESCRIPTOR_LEVEL forNode:busprobeRootNode];


                configBuf = malloc(cfg.wTotalLength*sizeof(Byte));
                if ( GetDescriptor(deviceIntf, kUSBConfDesc, iconfig, configBuf, cfg.wTotalLength) < 0 )
                    continue;
                p = configBuf;
                pend = p + cfg.wTotalLength;
                p += cfg.bLength;

                // Dump the descriptors in the Configuration Descriptor
                //
                while (p < pend)
                {
                    UInt8 descLen = p[0];
                    UInt8 descType = p[1];
                    
                    //  If this is an interface descriptor, save the interface class and subclass
                    //
                    if ( descType == kUSBInterfaceDesc )
                    {
                        lastInterfaceClass = ((IOUSBInterfaceDescriptor *)p)->bInterfaceClass;
                        lastInterfaceSubClass = ((IOUSBInterfaceDescriptor *)p)->bInterfaceSubClass;
                        currentInterfaceNum = (int) ((IOUSBInterfaceDescriptor *)p)->bInterfaceNumber;
                    }
                    
                    [self DumpDescriptor:deviceIntf p:p forDevice:deviceNumber lastInterfaceClass:lastInterfaceClass lastInterfaceSubClass:lastInterfaceSubClass currentInterfaceNum:currentInterfaceNum];
                    p += descLen;
                }
            }
        }
    }
    else
    {
        // Create a new child node for this device
        newNode =  [[Node alloc] init];
        [newNode setItemName:[NSString stringWithFormat:@"USB device @ 0x%08lX: .............................................",locationID]]; // Set the node's name to be the device's location


        [newNode setItemValue: @"Unknown device (did not respond do inquiry)"];
        [busprobeRootNode addChild:newNode];
        [newNode release];
    }


    // If the device is a hub, then dump the Hub descriptor
    //
    if ( dev.bDeviceClass == kUSBHubClass )
    {
        IOUSBHubDescriptor	cfg;

        len = GetClassDescriptor(deviceIntf, kUSBHUBDesc, 0, &cfg, sizeof(cfg));
        if (len > 0)
        {
            [self DumpDescriptor:deviceIntf p:(Byte *)&cfg forDevice:deviceNumber lastInterfaceClass:lastInterfaceClass lastInterfaceSubClass:lastInterfaceSubClass currentInterfaceNum:currentInterfaceNum];
        }
    }
    
    // Check to see if the device has the "Device Qualifier" descriptor
    //
    IOUSBDeviceQualifierDescriptor	desc;
    
    len = GetDescriptor(deviceIntf, kUSBDeviceQualifierDesc, 0, &desc, sizeof(desc));
    if ( (len > 0) && (dev.bcdUSB == 0x0200) )
    {
        [self DumpDescriptor:deviceIntf p:(Byte *)&desc forDevice:deviceNumber lastInterfaceClass:lastInterfaceClass lastInterfaceSubClass:lastInterfaceSubClass currentInterfaceNum:currentInterfaceNum];
        
        // Since we have a Device Qualifier Descriptor, we can get a "Other Speed Configuration Descriptor" (It's the same as a 
        // regular configuration descriptor)
        //
        for (iconfig = 0; iconfig < desc.bNumConfigurations; ++iconfig)
        {
            IOUSBConfigurationDescriptor cfg;

            len = GetDescriptor(deviceIntf, kUSBOtherSpeedConfDesc, iconfig, &cfg, sizeof(cfg));
            if (len > 0) {
                /*	struct IOUSBConfigurationDescriptor {
                UInt8 			bLength;
                UInt8 			bDescriptorType;
                UInt16 			wTotalLength;
                UInt8 			bNumInterfaces;
                UInt8 			bConfigurationValue;
                UInt8 			iConfiguration;
                UInt8 			bmAttributes;
                UInt8 			MaxPower;
                }; */

                Byte *configBuf;
                Byte *p, *pend;
                char configHeading[500];

                aTempString = RETURNSTR(cfg, iConfiguration);
                if (strcmp([aTempString cString], "0x00") != 0) {
                    sprintf(configHeading, "Other Speed Configuration Descriptor: .......................................");
                    sprintf(tempCString, [aTempString cString]);
                    [self PrintKeyVal:configHeading val:tempCString forDevice:deviceNumber atDepth:CONFIGURATION_DESCRIPTOR_LEVEL-1 forNode:busprobeRootNode];
                }
                else {
                    sprintf(configHeading, "Other Speed Configuration Descriptor");
                    [self PrintKeyVal:configHeading val:"" forDevice:deviceNumber atDepth:CONFIGURATION_DESCRIPTOR_LEVEL-1 forNode:busprobeRootNode];
                }

                Swap16(&cfg.wTotalLength);
                NUM(cfg, "Total Length of Descriptor:", wTotalLength, deviceNumber, CONFIGURATION_DESCRIPTOR_LEVEL, 1);
                NUM(cfg, "Number of Interfaces:", bNumInterfaces, deviceNumber, CONFIGURATION_DESCRIPTOR_LEVEL, 1);
                NUM(cfg, "Configuration Value:", bConfigurationValue, deviceNumber, CONFIGURATION_DESCRIPTOR_LEVEL, 1);
                sprintf(str, "0x%02X", cfg.bmAttributes);
                if (cfg.bmAttributes & 0x40) {
                    strcat(str, " (self-powered");
                }
                else {
                    strcat(str, " (bus-powered");
                }
                if (cfg.bmAttributes & 0x20) {
                    strcat(str, ", remote wakeup");
                }
                strcat(str, ")");
                [self PrintKeyVal:"Attributes:" val:str forDevice:deviceNumber atDepth:CONFIGURATION_DESCRIPTOR_LEVEL forNode:busprobeRootNode];


                aTempString = RETURNNUM(cfg, MaxPower, 1);
                sprintf(tempCString, "%d ma", [aTempString intValue]*2);
                [self PrintKeyVal:"MaxPower:" val:tempCString forDevice:deviceNumber atDepth:CONFIGURATION_DESCRIPTOR_LEVEL forNode:busprobeRootNode];

 
                configBuf = malloc(cfg.wTotalLength*sizeof(Byte));
                if ( GetDescriptor(deviceIntf, kUSBOtherSpeedConfDesc, iconfig, configBuf, cfg.wTotalLength) < 0 )
                    continue;
                p = configBuf;
                pend = p + cfg.wTotalLength;
                p += cfg.bLength;

                // Dump the descriptors in the Configuration Descriptor
                //
               while (p < pend)
                {
                    UInt8 descLen = p[0];
                    UInt8 descType = p[1];
                    
                    //  If this is an interface descriptor, save the interface class and subclass
                    //
                    if ( descType == kUSBInterfaceDesc )
                    {
                        lastInterfaceClass = ((IOUSBInterfaceDescriptor *)p)->bInterfaceClass;
                        lastInterfaceSubClass = ((IOUSBInterfaceDescriptor *)p)->bInterfaceSubClass;
                        currentInterfaceNum = (int) ((IOUSBInterfaceDescriptor *)p)->bInterfaceNumber;
                    }
                    
                    [self DumpDescriptor:deviceIntf p:p forDevice:deviceNumber lastInterfaceClass:lastInterfaceClass lastInterfaceSubClass:lastInterfaceSubClass currentInterfaceNum:currentInterfaceNum];
                    p += descLen;
                }
    
            }
        }    
    }
    [deviceClass release];
    [interfaceClass release];
}

int GetClassDescriptor(IOUSBDeviceInterface **deviceIntf, UInt8 descType, UInt8 descIndex, void *buf, UInt16 len)
{
    IOUSBDevRequest req;
    IOReturn err;

    req.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBClass, kUSBDevice);
    req.bRequest = kUSBRqGetDescriptor;
    req.wValue = (descType << 8) | descIndex;
    req.wIndex = 0;
    req.wLength = len;
    req.pData = buf;

    verify_noerr(err = (*deviceIntf)->DeviceRequest(deviceIntf, &req));
    if (err) return -1;
    return req.wLenDone;
}

int GetDescriptor(IOUSBDeviceInterface **deviceIntf, UInt8 descType, UInt8 descIndex, void *buf, UInt16 len)
{
    IOUSBDevRequest req;
    IOReturn err;

    req.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
    req.bRequest = kUSBRqGetDescriptor;
    req.wValue = (descType << 8) | descIndex;
    req.wIndex = 0;
    req.wLength = len;
    req.pData = buf;

    verify_noerr(err = (*deviceIntf)->DeviceRequest(deviceIntf, &req));
    if (err) return -1;
    return req.wLenDone;
}

int GetDescriptorFromInterface(IOUSBDeviceInterface **deviceIntf, UInt8 descType, UInt8 descIndex, UInt16 wIndex, void *buf, UInt16 len)
{
    IOUSBDevRequest req;
    IOReturn err;

    req.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBInterface);
    req.bRequest = kUSBRqGetDescriptor;
    req.wValue = (descType << 8) | descIndex;
    req.wIndex = wIndex;
    req.wLength = len;
    req.pData = buf;

    verify_noerr(err = (*deviceIntf)->DeviceRequest(deviceIntf, &req));
    if (err) return -1;
    return req.wLenDone;
}

// ________________________________________________________________________________________________
//	GetStringDescriptor
//
//	Get a string descriptor from the device.  First, we get the length by getting 2 bytes, and then
//	we use that information to get the actual string.
//
int	GetStringDescriptor(IOUSBDeviceInterface **deviceIntf, UInt8 descIndex, void *buf, UInt16 len, UInt16 lang)
{
    IOUSBDevRequest req;
    UInt8 		desc[256]; // Max possible descriptor length
    int stringLen;
    IOReturn err;
    if (lang == NULL) // set default langID
        lang=0x0409;

    req.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
    req.bRequest = kUSBRqGetDescriptor;
    req.wValue = (kUSBStringDesc << 8) | descIndex;
    req.wIndex = lang;	// English
    req.wLength = 2;
    req.pData = &desc;
    verify_noerr(err = (*deviceIntf)->DeviceRequest(deviceIntf, &req));
    if ( (err != kIOReturnSuccess) && (err != kIOReturnOverrun) )
        return -1;

    // If the string is 0 (it happens), then just return 0 as the length
    //
    stringLen = desc[0];
    if(stringLen == 0)
    {
        return 0;
    }

    // OK, now that we have the string length, make a request for the full length
    //
    req.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
    req.bRequest = kUSBRqGetDescriptor;
    req.wValue = (kUSBStringDesc << 8) | descIndex;
    req.wIndex = lang;	// English
    req.wLength = stringLen;
    req.pData = buf;

    verify_noerr(err = (*deviceIntf)->DeviceRequest(deviceIntf, &req));

    return req.wLenDone;
}


+(USBClass *)ClassAndSubClass:(const char *)scope pcls:(UInt8 *)pcls forDevice:(int)deviceNumber atDepth:(int)depth
{
    USBClass *usbc = NULL;
    char *cls = "", *sub = "";
    char name[500];

    switch (pcls[0]) {
        case kUSBCompositeClass:
            cls = "Composite";
            break;
        case kUSBAudioClass:
            cls = "Audio";
            switch (pcls[1]) {
                case 0x01:
                    sub = "Audio Control";
                    break;
                case 0x02:
                    sub = "Audio Streaming";
                    break;
                case 0x03:
                    sub = "MIDI Streaming";
                    break;
                default:
                    sub = "Unknown";
                    break;
            }
                break;
        case kUSBCommClass:			cls = "Comm";			break;
        case kUSBHIDClass:			
            cls = "HID";			
            switch (pcls[1]) {
                case kUSBHIDBootInterfaceSubClass:        sub = "Boot Interface"; break;
                default:                        	sub = ""; break;
            }
            break;
        case kUSBDisplayClass:			cls = "Display";		break;
        case kUSBPrintingClass:			cls = "Printing";		break;
        case kUSBMassStorageClass:		
            cls = "Mass Storage";		
            switch (pcls[1]) {
                case kUSBMassStorageRBCSubClass:        sub = "Reduced Block Commands"; break;
                case kUSBMassStorageATAPISubClass:  	sub = "ATAPI"; break;
                case kUSBMassStorageQIC157SubClass:  	sub = "QIC-157"; break;
                case kUSBMassStorageUFISubClass:  	sub = "UFI"; break;
                case kUSBMassStorageSFF8070iSubClass:  	sub = "SFF-8070i"; break;
                case kUSBMassStorageSCSISubClass:  	sub = "SCSI"; break;
                default:                        	sub = "Unknown"; break;
            }
            break;
        case kUSBHubClass:			cls = "Hub";			break;
        case kUSBDataClass:			cls = "Data";			break;
        case 0xE0:				cls = "Bluetooth Wireless Controller"; break;
        case kUSBApplicationSpecificClass:
            cls = "Application Specific";
            switch (pcls[1]) {
                case kUSBDFUSubClass:         	sub = "Device Firmware Upgrade"; break;
                case kUSBIrDABridgeSubClass:  	sub = "IrDA Bridge"; break;
                default:                        sub = "Unknown"; break;
            }
                break;
        case kUSBVendorSpecificClass: 		cls = sub = "Vendor-specific";	break;
        default:				cls = "Unknown";		break;
    }

    sprintf(name, "%s Class:", scope);
    [self PrintNumStr:name value:pcls[0] size:1 interpret:cls forDevice:deviceNumber atDepth:depth asInt:1];
    sprintf(name, "%s Subclass:", scope);
    [self PrintNumStr:name value:pcls[1] size:1 interpret:sub forDevice:deviceNumber atDepth:depth asInt:1];
    if (usbc == NULL) {
        usbc = [[USBClass alloc] init];

        [usbc setClassName:[NSString stringWithCString:cls]];
        [usbc setSubClassName:[NSString stringWithCString:sub]];
    }


    return usbc;
}



// ________________________________________________________________________________________________
//	PrintDescLenAndType
//
//	Print the length and type fields of a USB descriptor.
+(void)PrintDescLenAndType:(void *)desc forDevice:(int)deviceNumber atDepth:(int)depth
{
    Byte *p = (Byte *)desc;
    char *str;
    [self printNum:"bLength" value:p[0] size:1 forDevice:(int)deviceNumber atDepth:depth asInt:0];


    switch (p[1]) {
        case kUSBDeviceDesc:	str = "Device";			break;
        case kUSBConfDesc:		str = "Configuration";	break;
        case kUSBStringDesc:	str = "String";			break;
        case kUSBInterfaceDesc:	str = "Interface";		break;
        case kUSBEndpointDesc:	str = "Endpoint";		break;
        case kUSBHIDDesc:		str = "HID";			break;
        case kUSBReportDesc:	str = "Report";			break;
        case kUSBPhysicalDesc:	str = "Physical";		break;
        case kUSBHUBDesc:		str = "Hub";			break;
        case CS_INTERFACE:		str = "CS Interface";	break;
        case CS_ENDPOINT:		str = "CS Endpoint";	break;
        default:				str = "(unknown)";		break;
    }

    [self PrintNumStr:"bDescriptorType" value:p[1] size:1 interpret:str forDevice:(int)deviceNumber atDepth:depth asInt:0];

}


// ________________________________________________________________________________________________
//	printNum
//
//	Print a numeric field's name and value
+(void)printNum:(char *)name value:(UInt32)value size:(int)sizeInBytes forDevice:(int)deviceNumber atDepth:(int)depth asInt:(int)asInt
{
    char format[32], valstr[32];

    if (asInt==1){
        sprintf(valstr, "%d",(int)value);
        [self PrintKeyVal:name val:valstr forDevice:deviceNumber atDepth:depth forNode:busprobeRootNode];
        return;
    }
    else {
        sprintf(format, "0x%%0%dlX", sizeInBytes*2);
        sprintf(valstr, format, value);
        [self PrintKeyVal:name val:valstr forDevice:deviceNumber atDepth:depth forNode:busprobeRootNode];
        return;
    }


}

// ________________________________________________________________________________________________
//	returnNum
//
//	Print a numeric field's name and value
+(NSString *)returnNum:(UInt32)value size:(int)sizeInBytes asInt:(int)asInt
{
    char format[32], valstr[32];

    if (asInt==1){
        sprintf(valstr, "%d",(int)value);
        return [NSString stringWithCString:valstr];
    }
    else {
        sprintf(format, "0x%%0%dlX", sizeInBytes*2);
        sprintf(valstr, format, value);
        return [NSString stringWithCString:valstr];
    }

}

// ________________________________________________________________________________________________
//	PrintNumStr
//
//	Print a numeric field's name, value, and interpretation
+(void)PrintNumStr:(char *)name value:(UInt32)value size:(int)sizeInBytes interpret:(char *)interpret forDevice:(int)deviceNumber atDepth:(int)depth asInt:(int)asInt
{
    char format[32], valstr[256];
    if (asInt==1) {
        if (strcmp(interpret,"")==0)
            sprintf(valstr, "%d", (int)value);
        else
            sprintf(valstr, "%d   (%s)", (int)value, interpret);
    }
    else {
        if (strcmp(interpret,"")==0)
            sprintf(format, "0x%%0%dlX   %%s", sizeInBytes*2);
        else
            sprintf(format, "0x%%0%dlX   (%%s)", sizeInBytes*2);
        sprintf(valstr, format, value, interpret);
    }

    [self PrintKeyVal:name val:valstr forDevice:deviceNumber atDepth:depth forNode:busprobeRootNode];
}

UInt16	Swap16(void *p)
{
    * (UInt16 *) p = CFSwapInt16LittleToHost(*(UInt16 *)p);
    return * (UInt16 *) p;
}

// ________________________________________________________________________________________________
//	PrintStr
//
//	For a descriptor field which is a string reference, fetch the string from
//	device and print the field.


+(void)PrintStr:(IOUSBDeviceInterface **)deviceIntf name:(char *)name strIndex:(UInt8)strIndex forDevice:(int)deviceNumber atDepth:(int)depth
{
    Byte buf[256];
    char str2[500];
    if (strIndex > 0) {
        int len;
        buf[0] = 0;
        len = GetStringDescriptor(deviceIntf, strIndex, buf, sizeof(buf),NULL);
        if (len > 2) {
            Byte *p;
            CFStringRef str;
            for (p = buf + 2; p < buf + len; p += 2) {
                Swap16(p);
            }

            str = CFStringCreateWithCharacters(NULL, (const UniChar *)(buf+2), (len-2)/2);
            CFStringGetCString(str, (char *)buf, 256, kCFStringEncodingNonLossyASCII);
            CFRelease(str);
            sprintf(str2, "%d \"%s\"", strIndex, buf);
            [self PrintKeyVal:name val:str2 forDevice:deviceNumber atDepth:depth forNode:busprobeRootNode];

        } else  {
            char str[20];
            buf[0] = 0;
            sprintf(str,"%d (none)",strIndex);
            [self PrintKeyVal:name val:str forDevice:deviceNumber atDepth:depth forNode:busprobeRootNode];

        }

    } else {
        char str[20];
        sprintf(str,"%d (none)",strIndex);
        [self PrintKeyVal:name val:str forDevice:deviceNumber atDepth:depth forNode:busprobeRootNode];
    }

}


+(NSString *)ReturnStr:(IOUSBDeviceInterface **)deviceIntf strIndex:(UInt8)strIndex
{
    Byte buf[256];
    char str2[500];
    if (strIndex > 0) {
        int len;
        buf[0] = 0;
        len = GetStringDescriptor(deviceIntf, strIndex, buf, sizeof(buf),NULL);
        if (len > 2) {
            Byte *p;
            CFStringRef str;
            for (p = buf + 2; p < buf + len; p += 2) {
                Swap16(p);
            }

            str = CFStringCreateWithCharacters(NULL, (const UniChar *)(buf+2), (len-2)/2);
            CFStringGetCString(str, (char *)buf, 256, kCFStringEncodingNonLossyASCII);
            CFRelease(str);
            sprintf(str2, "\"%s\"", buf);
            return [NSString stringWithCString:str2];

        } else  {
            buf[0] = 0;
            return @"0x00";
        }

    } else
        return @"0x00";
}


// ________________________________________________________________________________________________
//	dump
+(void)dump:(int)n byte:(Byte *)p forDevice:(int)deviceNumber atDepth:(int)depth
{
    char str1[8192] = "";  // Don't know how long the descriptor will be, so make str nice and big
    char str2[10];
    while (--n >= 0) {
        sprintf(str2, "0x%02X ", *p++);
        strcat(str1, str2);
    }

    [self PrintKeyVal:"Unknown Descriptor" val:str1 forDevice:deviceNumber atDepth:depth forNode:busprobeRootNode];

    return;
}

// ________________________________________________________________________________________________
//	DumpDescriptor
//
+(void)DumpDescriptor:(IOUSBDeviceInterface **)deviceIntf p:(Byte *)p forDevice:(int)deviceNumber  lastInterfaceClass:(UInt8)lastInterfaceClass  lastInterfaceSubClass:(UInt8)lastInterfaceSubClass currentInterfaceNum:(int)currentInterfaceNum
{
        UInt8 descType = p[1];
        char *xferTypes[] = { "Control", "Isochronous", "Bulk", "Interrupt" };
        int xferTypes2[] = { 0, 1, 2, 3 };
        USBClass *interfaceClass = NULL;
        int tempInt1, tempInt2;
        NSString *tempString1;
        char str[500];

        switch (descType) {
            case kUSBInterfaceDesc:
            {
                /*	struct IOUSBInterfaceDescriptor {
                UInt8 			bLength;
                UInt8 			bDescriptorType;
                UInt8 			bInterfaceNumber;
                UInt8 			bAlternateSetting;
                UInt8 			bNumEndpoints;
                UInt8 			bInterfaceClass;
                UInt8 			bInterfaceSubClass;
                UInt8 			bInterfaceProtocol;
                UInt8 			iInterface;
                }; */
                IOUSBInterfaceDescriptor intf;
                char interfaceHeading[500];

                intf = *(IOUSBInterfaceDescriptor *)p;

                sprintf(interfaceHeading, "Interface #%d", (int)intf.bInterfaceNumber);


                [self PrintKeyVal:interfaceHeading val:"" forDevice:deviceNumber atDepth:INTERFACE_LEVEL-1 forNode:busprobeRootNode];

                NUM(intf, "Alternate Setting", bAlternateSetting, deviceNumber, INTERFACE_LEVEL, 1);
                NUM(intf, "Number of Endpoints", bNumEndpoints, deviceNumber, INTERFACE_LEVEL, 1);
                interfaceClass = [self ClassAndSubClass:"Interface" pcls:&intf.bInterfaceClass forDevice:deviceNumber atDepth:INTERFACE_LEVEL];

                tempInt1 = [[busprobeRootNode childAtIndex:deviceNumber] childrenCount];
                tempInt2 = [[[busprobeRootNode childAtIndex:deviceNumber] childAtIndex:tempInt1-1] childrenCount];
                tempString1 = [interfaceClass className];

                // If our subclass name is different than our class name, then add the sub class to the description
                // following a "/"
                //
                if( ! [[interfaceClass subClassName] isEqualToString:@""] &&
                    ! [[interfaceClass subClassName] isEqualToString:[interfaceClass className]] ) {
                    tempString1 = [[tempString1 stringByAppendingString:@"/"] stringByAppendingString:[interfaceClass subClassName]];
                }

                [[[[busprobeRootNode childAtIndex:deviceNumber] childAtIndex:tempInt1-1] childAtIndex:tempInt2-1] setItemName:[NSString stringWithFormat:@"Interface #%d - %s", (int)intf.bInterfaceNumber, [tempString1 cString]]];


                NUM(intf, "Interface Protocol", bInterfaceProtocol, deviceNumber, INTERFACE_LEVEL, 1);

                lastInterfaceClass = intf.bInterfaceClass;
                lastInterfaceSubClass = intf.bInterfaceSubClass;
            }
                break;
            case kUSBEndpointDesc:
            {
                /*	struct IOUSBEndpointDescriptor {
                UInt8 			bLength;
                UInt8 			bDescriptorType;
                UInt8 			bEndpointAddress;
                UInt8 			bmAttributes;
                UInt16 			wMaxPacketSize;
                UInt8 			bInterval;
                }; */
                IOUSBEndpointDescriptor end;
                char endpointHeading[500];
                char temporaryString[500];

                end = *(IOUSBEndpointDescriptor *)p;

                Swap16(&end.wMaxPacketSize);
                switch (xferTypes2[end.bmAttributes & 3]) {
                    case 0:
                        sprintf(endpointHeading, "Endpoint 0x%02X - Control Endpoint", end.bEndpointAddress);
                        [self PrintKeyVal:endpointHeading val:"" forDevice:deviceNumber atDepth:ENDPOINT_LEVEL-1 forNode:busprobeRootNode];
                        break;
                    case 1:
                        if ( (end.bEndpointAddress & kEndpointAddressMask ) == 0 )
                            sprintf(endpointHeading, "Endpoint 0x%02X - Isochronous Output", end.bEndpointAddress);
                        else
                            sprintf(endpointHeading, "Endpoint 0x%02X - Isochronous Input", end.bEndpointAddress);
                        [self PrintKeyVal:endpointHeading val:"" forDevice:deviceNumber atDepth:ENDPOINT_LEVEL-1 forNode:busprobeRootNode];
                        break;
                    case 2:
                        if ( (end.bEndpointAddress & kEndpointAddressMask ) == 0 )
                            sprintf(endpointHeading, "Endpoint 0x%02X - Bulk Output", end.bEndpointAddress);
                        else
                            sprintf(endpointHeading, "Endpoint 0x%02X - Bulk Input", end.bEndpointAddress);
                        [self PrintKeyVal:endpointHeading val:"" forDevice:deviceNumber atDepth:ENDPOINT_LEVEL-1 forNode:busprobeRootNode];
                        break;
                    case 3:
                        if ( (end.bEndpointAddress & kEndpointAddressMask ) == 0 )
                            sprintf(endpointHeading, "Endpoint 0x%02X - Interrupt Output", end.bEndpointAddress);
                        else
                            sprintf(endpointHeading, "Endpoint 0x%02X - Interrupt Input", end.bEndpointAddress);
                        [self PrintKeyVal:endpointHeading val:"" forDevice:deviceNumber atDepth:ENDPOINT_LEVEL-1 forNode:busprobeRootNode];
                        break;
                    default:
                        sprintf(endpointHeading, "Endpoint 0x%02X", end.bEndpointAddress);
                        [self PrintKeyVal:endpointHeading val:"" forDevice:deviceNumber atDepth:ENDPOINT_LEVEL-1 forNode:busprobeRootNode];
                        break;
                }

                if (!(xferTypes2[end.bmAttributes & 3] == 0)) { // we dont need to show direction for Control Endpoints
                    if ( (end.bEndpointAddress & kEndpointAddressMask ) == 0 )
                        sprintf(temporaryString, "0x%02X  (OUT)", end.bEndpointAddress);
                    else
                        sprintf(temporaryString, "0x%02X  (IN)", end.bEndpointAddress);
                    [self PrintKeyVal:"Attributes:" val:temporaryString  forDevice:deviceNumber atDepth:ENDPOINT_LEVEL forNode:busprobeRootNode];
                }

                sprintf(str, "0x%02X  (%s)", end.bmAttributes, xferTypes[end.bmAttributes & 3]);
                [self PrintKeyVal:"Attributes:" val:str  forDevice:deviceNumber atDepth:ENDPOINT_LEVEL forNode:busprobeRootNode];

                sprintf(temporaryString, "%d", end.wMaxPacketSize);
                [self PrintKeyVal:"Max Packet Size:" val:temporaryString forDevice:deviceNumber atDepth:ENDPOINT_LEVEL forNode:busprobeRootNode];

                sprintf(temporaryString, "%d ms", end.bInterval);
                [self PrintKeyVal:"Polling Interval:" val:temporaryString  forDevice:deviceNumber atDepth:ENDPOINT_LEVEL forNode:busprobeRootNode];
            }
                break;
            case CS_INTERFACE:
                [self DoRegularCSInterface:p deviceClass:interfaceClass forDevice:deviceNumber atDepth:INTERFACE_LEVEL-1];
                break;
            case CS_ENDPOINT:
                [self DoRegularCSEndpoint:p deviceClass:interfaceClass forDevice:deviceNumber atDepth:ENDPOINT_LEVEL-1];
                break;
            case HID_DESCRIPTOR:
                /* case DFU_FUNCTIONAL_DESCRIPTOR:  - same value, compiler complains */
            {
                if (lastInterfaceClass == kUSBApplicationSpecificClass && lastInterfaceSubClass == kUSBDFUSubClass)
                {
                    IOUSBDFUDescriptor dfuDescriptor;
                    char temporaryString[500];

                    dfuDescriptor = *(IOUSBDFUDescriptor *)p;

                    [self PrintKeyVal:"DFU Functional Descriptor" val:"" forDevice:deviceNumber
                              atDepth:DFU_DESCRIPTOR_LEVEL-1 forNode:busprobeRootNode];

                    sprintf(temporaryString, "0x%02x (%sDownload, %sUpload, %sManifestation Tolerant, "
                            "Reserved bits: 0x%02x)",
                            dfuDescriptor.bmAttributes,
                            dfuDescriptor.bmAttributes &  (1 << kUSBDFUCanDownloadBit) ? "" : "No ",
                            dfuDescriptor.bmAttributes & ( 1 << kUSBDFUCanUploadBit) ? "" : "No ",
                            dfuDescriptor.bmAttributes & ( 1 << kUSBDFUManifestationTolerantBit) ? "" : "Not ",
                            dfuDescriptor.bmAttributes & ~kUSBDFUAttributesMask);
                    [self PrintKeyVal:"bmAttributes:" val:temporaryString  forDevice:deviceNumber
                              atDepth:DFU_DESCRIPTOR_LEVEL forNode:busprobeRootNode];

                    sprintf(temporaryString, "%d ms", Swap16(&dfuDescriptor.wDetachTimeout) );

                    [self PrintKeyVal:"wDetachTimeout:" val:temporaryString  forDevice:deviceNumber
                              atDepth:DFU_DESCRIPTOR_LEVEL forNode:busprobeRootNode];

                    sprintf(temporaryString, "%d bytes", Swap16(&dfuDescriptor.wTransferSize));

                    [self PrintKeyVal:"wTransferSize:" val:temporaryString  forDevice:deviceNumber
                              atDepth:DFU_DESCRIPTOR_LEVEL forNode:busprobeRootNode];
                } else if (lastInterfaceClass == kUSBHIDClass) {

                    IOUSBHIDDescriptor HIDDesc;
                    int descriptorIncrement=0;

                    HIDDesc = *(IOUSBHIDDescriptor *)p;
                    [self PrintKeyVal:"HID Descriptor" val:"" forDevice:deviceNumber atDepth:HID_DESCRIPTOR_LEVEL-1 forNode:busprobeRootNode];
                    Swap16(&HIDDesc.descVersNum);

                    NUM(HIDDesc, "Descriptor Version Number:", descVersNum, deviceNumber, HID_DESCRIPTOR_LEVEL, 0);
                    NUM(HIDDesc, "Country Code:", hidCountryCode, deviceNumber, HID_DESCRIPTOR_LEVEL, 1);
                    NUM(HIDDesc, "Descriptor Count:", hidNumDescriptors, deviceNumber, HID_DESCRIPTOR_LEVEL, 1);

                    for(descriptorIncrement=1; descriptorIncrement <= HIDDesc.hidNumDescriptors; descriptorIncrement++) {
                        char tempCString[20], descriptorHeading[20];
                        NSString *tempString;

                        sprintf(descriptorHeading, "Descriptor %d", descriptorIncrement);
                        [self PrintKeyVal:descriptorHeading val:"" forDevice:deviceNumber atDepth:HID_DESCRIPTOR_LEVEL forNode:busprobeRootNode];

                        tempString = RETURNNUM(HIDDesc, hidDescriptorType, 0);
                        if ( HIDDesc.hidDescriptorType == kUSBHIDDesc) {
                            UInt16 hidDescriptorLength = ( HIDDesc.hidDescriptorLengthHi  << 8 ) | HIDDesc.hidDescriptorLengthLo;
                            sprintf(tempCString, "%s  (HID Descriptor)", [tempString cString]);
                            [self PrintKeyVal:"Type:" val:tempCString forDevice:deviceNumber atDepth:HID_DESCRIPTOR_LEVEL+1 forNode:busprobeRootNode];
                            sprintf(tempCString, "%d", hidDescriptorLength);
                            [self PrintKeyVal:"Length:" val:tempCString forDevice:deviceNumber atDepth:HID_DESCRIPTOR_LEVEL+1 forNode:busprobeRootNode];
                        }
                        else if (HIDDesc.hidDescriptorType == kUSBReportDesc)
                        {
                            unsigned char *reportdesc;
                            UInt16 hidlen, hidDescriptorLength = ( HIDDesc.hidDescriptorLengthHi  << 8 ) | HIDDesc.hidDescriptorLengthLo;

                            sprintf(tempCString, "%s  (Report Descriptor)", [tempString cString]);
                            [self PrintKeyVal:"Type:" val:tempCString forDevice:deviceNumber atDepth:HID_DESCRIPTOR_LEVEL+1 forNode:busprobeRootNode];
                            sprintf(tempCString, "%d", hidDescriptorLength);
                            [self PrintKeyVal:"Length:" val:tempCString forDevice:deviceNumber atDepth:HID_DESCRIPTOR_LEVEL+1 forNode:busprobeRootNode];
                            reportdesc = malloc(hidDescriptorLength);
                            if (reportdesc){
                                hidlen = GetDescriptorFromInterface(deviceIntf, kUSBReportDesc, 0 /*desc index*/,  currentInterfaceNum, reportdesc, hidDescriptorLength);
                                if (hidlen == hidDescriptorLength)
                                {
                                    [DecodeHIDReport DecodeHIDReport:reportdesc forDevice:deviceNumber atDepth:HID_DESCRIPTOR_LEVEL+1 reportLen:hidlen forNode:busprobeRootNode];
                                }
                                free(reportdesc);
                            }
                        }
                        else if (HIDDesc.hidDescriptorType == kUSBPhysicalDesc) {
                            UInt16 hidDescriptorLength = ( HIDDesc.hidDescriptorLengthHi  << 8 ) | HIDDesc.hidDescriptorLengthLo;
                            sprintf(tempCString, "%s  (Physical Descriptor)", [tempString cString]);
                            [self PrintKeyVal:"Type:" val:tempCString forDevice:deviceNumber atDepth:HID_DESCRIPTOR_LEVEL+1 forNode:busprobeRootNode];
                            sprintf(tempCString, "%d", hidDescriptorLength);
                            [self PrintKeyVal:"Length:" val:tempCString forDevice:deviceNumber atDepth:HID_DESCRIPTOR_LEVEL+1 forNode:busprobeRootNode];
                        }
                        else {
                            UInt16 hidDescriptorLength = ( HIDDesc.hidDescriptorLengthHi  << 8 ) | HIDDesc.hidDescriptorLengthLo;
                            sprintf(tempCString, "%s", [tempString cString]);
                            [self PrintKeyVal:"Type:" val:tempCString forDevice:deviceNumber atDepth:HID_DESCRIPTOR_LEVEL+1 forNode:busprobeRootNode];
                            sprintf(tempCString, "%d", hidDescriptorLength);
                            [self PrintKeyVal:"Length:" val:tempCString forDevice:deviceNumber atDepth:HID_DESCRIPTOR_LEVEL+1 forNode:busprobeRootNode];
                        }
                    }
                }  /* HID Descriptor */
        /*        else
                {
                    // Descriptor 21 for an unknown class.  Just dump it out
                    //
                    [self DumpRawDescriptor:p forDevice:deviceNumber atDepth:CONFIGURATION_DESCRIPTOR_LEVEL+1];
                }*/
            }
                            break;
                            
                        case kUSBHUBDesc:
                        {
                            IOUSBHubDescriptor 	hubDescriptor;
                            char 			temporaryString[500];
                            UInt16			hubChar;

                            hubDescriptor = *(IOUSBHubDescriptor *)p;

                            [self PrintKeyVal:"Hub Descriptor" val:"" forDevice:deviceNumber atDepth:HUB_DESCRIPTOR_LEVEL-1 forNode:busprobeRootNode];

                            NUM(hubDescriptor, "Number of Ports:", numPorts, deviceNumber, HUB_DESCRIPTOR_LEVEL, 0);

                            hubChar = Swap16(&hubDescriptor.characteristics);
                            sprintf(temporaryString, "0x%x (%sswitched %s hub with %s overcurrent protection)",hubChar ,
                                    (((hubChar & 3) == 0) ? "Gang " :
                                     ((hubChar & 3) == 1) ? "Individually " : "Non-"),
                                    ((hubChar & 4) == 4) ? "compound" : "standalone",
                                    ((hubChar & 0x18) == 0) ? "global" :
                                    ((hubChar & 0x18) == 0x8) ? "individual port" : "no");
                            [self PrintKeyVal:"Hub Characteristics:" val:temporaryString  forDevice:deviceNumber
                                      atDepth:HUB_DESCRIPTOR_LEVEL forNode:busprobeRootNode];

                            sprintf(temporaryString, "%d ms", hubDescriptor.powerOnToGood*2);
                            [self PrintKeyVal:"PowerOnToGood time:" val:temporaryString  forDevice:deviceNumber
                                      atDepth:HUB_DESCRIPTOR_LEVEL forNode:busprobeRootNode];

                            sprintf(temporaryString, "%d mA", hubDescriptor.hubCurrent);
                            [self PrintKeyVal:"Controller current:" val:temporaryString  forDevice:deviceNumber
                                      atDepth:HUB_DESCRIPTOR_LEVEL forNode:busprobeRootNode];

                            if (hubDescriptor.numPorts < 8){
                                sprintf(temporaryString, "0x%x", hubDescriptor.removablePortFlags[0]);
                                [self PrintKeyVal:"Device Removeable (byte):" val:temporaryString  forDevice:deviceNumber
                                          atDepth:HUB_DESCRIPTOR_LEVEL forNode:busprobeRootNode];
                                sprintf(temporaryString, "0x%x", hubDescriptor.removablePortFlags[1]);
                                [self PrintKeyVal:"Port Power Control Mask (byte):" val:temporaryString  forDevice:deviceNumber
                                          atDepth:HUB_DESCRIPTOR_LEVEL forNode:busprobeRootNode];
                            } else if (hubDescriptor.numPorts < 16) {
                                sprintf(temporaryString, "0x%lx", (UInt32)Swap16( &( (UInt16 *)hubDescriptor.removablePortFlags)[0]));
                                [self PrintKeyVal:"Device Removeable (byte):" val:temporaryString  forDevice:deviceNumber
                                          atDepth:HUB_DESCRIPTOR_LEVEL forNode:busprobeRootNode];
                                sprintf(temporaryString, "0x%lx", (UInt32)Swap16(&((UInt16 *)hubDescriptor.removablePortFlags)[1]));
                                [self PrintKeyVal:"Port Power Control Mask (byte):" val:temporaryString  forDevice:deviceNumber
                                          atDepth:HUB_DESCRIPTOR_LEVEL forNode:busprobeRootNode];
                            }
                            break;
                        }
                        case kUSBDeviceQualifierDesc:
                        {
                            IOUSBDeviceQualifierDescriptor 	devQualDesc;
                            USBClass *deviceClass = NULL;
                            USBClass *interfaceClass = NULL;

                            devQualDesc = *(IOUSBDeviceQualifierDescriptor *)p;

                            Swap16(&devQualDesc.bcdUSB);
                            [self PrintKeyVal:"Device Qualifier Descriptor" val:"" forDevice:deviceNumber atDepth:DEVICE_QUAL_DESCRIPTOR_LEVEL-1 forNode:busprobeRootNode];

                            NUM(devQualDesc, "Descriptor Version Number:", bcdUSB, deviceNumber, DEVICE_QUAL_DESCRIPTOR_LEVEL, 0);
                            deviceClass = [self ClassAndSubClass:"Device" pcls:&devQualDesc.bDeviceClass forDevice:deviceNumber atDepth:1];
                            NUM(devQualDesc, "Device Protocol", bDeviceProtocol, deviceNumber, DEVICE_QUAL_DESCRIPTOR_LEVEL, 1);
                            NUM(devQualDesc, "Device MaxPacketSize:", bMaxPacketSize0, deviceNumber, DEVICE_QUAL_DESCRIPTOR_LEVEL, 1);
                    
                            NUM(devQualDesc, "Number of Configurations:", bNumConfigurations, deviceNumber, DEVICE_DESCRIPTOR_LEVEL, 1);
                            NUM(devQualDesc, "bReserved:", bReserved, deviceNumber, DEVICE_DESCRIPTOR_LEVEL, 1);
                    
                            [deviceClass release];
                            [interfaceClass release];
                            
                            break;
                        }
                        default:
                            [self DumpRawDescriptor:p forDevice:deviceNumber atDepth:CONFIGURATION_DESCRIPTOR_LEVEL+1];
                            break;
        }
    [interfaceClass release];
    }
// ________________________________________________________________________________________________
//	DumpRawDescriptor
//
//	When we don't know any better...
+(void)DumpRawDescriptor:(Byte *)p forDevice:(int)deviceNumber atDepth:(int)depth
{
    [self dump:p[0] byte:p forDevice:deviceNumber atDepth:depth];

}

+(void)DoRegularCSInterface:(Byte *)p deviceClass:(USBClass *)interfaceClass forDevice:(int)deviceNumber atDepth:(int)depth
{
    NSString *compositedString;
    char compositedCString[500];
    if ([[interfaceClass subClassName] isEqualToString:@""])
        compositedString = [NSString stringWithFormat:@"%s Class-Specific Interface", [[interfaceClass className] cString], [[interfaceClass subClassName] cString]];
    else
        compositedString = [NSString stringWithFormat:@"%s/%s Class-Specific Interface", [[interfaceClass className] cString], [[interfaceClass subClassName] cString]];

    sprintf(compositedCString,"%s",[compositedString cString]);
    [self PrintKeyVal:compositedCString val:"" forDevice:deviceNumber atDepth:depth forNode:busprobeRootNode];
    [self DumpRawDescriptor:p forDevice:deviceNumber atDepth:depth+1];
}

+(void)DoRegularCSEndpoint:(Byte *)p deviceClass:(USBClass *)interfaceClass forDevice:(int)deviceNumber atDepth:(int)depth;
{
    NSString *compositedString;
    char compositedCString[500];
    if ([[interfaceClass subClassName] isEqualToString:@""])
        compositedString = [NSString stringWithFormat:@"%s Class-Specific Endpoint", [[interfaceClass className] cString], [[interfaceClass subClassName] cString]];
    else
        compositedString = [NSString stringWithFormat:@"%s/%s Class-Specific Endpoint", [[interfaceClass className] cString], [[interfaceClass subClassName] cString]];


    sprintf(compositedCString,"%s",[compositedString cString]);
    [self PrintKeyVal:compositedCString val:"" forDevice:deviceNumber atDepth:depth forNode:busprobeRootNode];
    [self DumpRawDescriptor:p forDevice:deviceNumber atDepth:depth+1];
}

- (void)loadVendorNamesFromFile
{
    NSString *vendorListString = [NSString stringWithContentsOfFile:[[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"USBVendors.txt"]];

    if (vendorListString == nil) { return; }
    else {
        NSArray *vendorsAndIDs = [vendorListString componentsSeparatedByString:@"\n"];
        if (vendorsAndIDs == nil) { return; }
        else {
            NSEnumerator *enumerator = [vendorsAndIDs objectEnumerator];
            NSString *vendorIDCombo;
            NSArray *aVendor;
            while ((vendorIDCombo = [enumerator nextObject])) {
                aVendor = [vendorIDCombo componentsSeparatedByString:@"|"];
                if (aVendor == nil || [aVendor count] < 2) { continue; }
                [vendorNamesDictionary setObject:[aVendor objectAtIndex:1] forKey:[aVendor objectAtIndex:0]];
            }
        }
    }
}

+ (NSString *)vendorNameFromVendorID:(NSString *)intValueAsString
{
    NSString *vendorName = [vendorNamesDictionary objectForKey:intValueAsString];
    if (vendorName != nil)
        return vendorName;
    else
        return @"unknown vendor";
}

// init is like a constructor
- init
{
    self = [super init];
    busprobeRootNode = [[Node alloc] init];
    vendorNamesDictionary = [[NSMutableDictionary alloc] init];
    [self loadVendorNamesFromFile];
    return self;
}


// Data source methods get called automatically

// This method is called repeatedly when the table view is displaying it self.
- (id)outlineView:(NSOutlineView *)ov child:(int)index ofItem:(id)item
{
    // is the parent non-nil?
    if (item)
        // Return the child
        return [item childAtIndex:index];
    else
        // Else return the root
        return busprobeRootNode;
}

// Called repeatedly to find out if there should be an
// "expand triangle" next to the label
- (BOOL)outlineView:(NSOutlineView *)ov isItemExpandable:(id)item
{
    // Returns YES if the node has children
    return [item expandable];
}

// Called repeatedly when the table view is displaying itself
- (int)outlineView:(NSOutlineView *)ov numberOfChildrenOfItem:(id)item
{
    if (item == nil) {
        // The root object;
        return 1;
    }
    return [item childrenCount];
}

// This method gets called repeatedly when the outline view is trying
// to display it self.

- (id)outlineView:(NSOutlineView *)ov
objectValueForTableColumn:(NSTableColumn *)tableColumn
           byItem:(id)item
{
    // What is returned depends upon which column it is
    // going to appear.
    if ([[tableColumn identifier] isEqual:@"itemValue"]){
        //        NSLog([item itemValue]);
        return [item itemValue];
    } else {
        //        NSLog([item itemName]);
        return [item itemName];
    }
}


- (void)dealloc
{
    [busprobeRootNode release];
    [super dealloc];
}


@end
