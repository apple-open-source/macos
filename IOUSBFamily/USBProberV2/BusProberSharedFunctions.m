/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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


#import "BusProberSharedFunctions.h"

int GetDeviceLocationID( IOUSBDeviceInterface **deviceIntf, UInt32 * locationID ) {
     IOReturn err;
    
    err = (*deviceIntf)->GetLocationID(deviceIntf, locationID);
    if (err) {
        NSLog(@"BusProber: GetDeviceSpeed() failed");
        return -1;
    }
    return 0;
}

int GetDeviceSpeed( IOUSBDeviceInterface **deviceIntf, UInt8 * speed ) {
    IOReturn err;
    
    err = (*deviceIntf)->GetDeviceSpeed(deviceIntf, speed);
    if (err) {
        NSLog(@"BusProber: GetDeviceSpeed() failed");
        return -1;
    }
    return 0;
}

int GetDeviceAddress( IOUSBDeviceInterface **deviceIntf, USBDeviceAddress * address ) {
    IOReturn err;
    
    err = (*deviceIntf)->GetDeviceAddress(deviceIntf, address);
    if (err) {
        NSLog(@"BusProber: GetDeviceAddress() failed");
        return -1;
    }
    return 0;
}

int GetDescriptor(IOUSBDeviceInterface **deviceIntf, UInt8 descType, UInt8 descIndex, void *buf, UInt16 len) {
    IOUSBDevRequest req;
    IOReturn err;
    
    req.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
    req.bRequest = kUSBRqGetDescriptor;
    req.wValue = (descType << 8) | descIndex;
    req.wIndex = 0;
    req.wLength = len;
    req.pData = buf;
    
    err = (*deviceIntf)->DeviceRequest(deviceIntf, &req);
    if (err) {
        return -1;
    }
    return req.wLenDone;
}

int GetStringDescriptor(IOUSBDeviceInterface **deviceIntf, UInt8 descIndex, void *buf, UInt16 len, UInt16 lang) {
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

char * GetStringFromNumber(UInt32 value, int sizeInBytes, int style) {
    char *format = malloc(32 *sizeof(char));
    char *valstr = malloc(32 *sizeof(char));
    
    memset(format, '\0', 32 *sizeof(char));
    memset(valstr, '\0', 32 *sizeof(char));
    
    if (style == kIntegerOutputStyle) {
        sprintf(valstr, "%d",(int)value);
        return valstr;
    }
    else if (style == kHexOutputStyle) {
        sprintf(format, "0x%%0%dlX", sizeInBytes*2);
        sprintf(valstr, format, value);
        return valstr;
    } else {
        return NULL;
    }
}

char * GetStringFromIndex(UInt8 strIndex, IOUSBDeviceInterface ** deviceIntf) {
    Byte buf[256];
    char *str2 =  malloc(500 * sizeof(char));
    memset(str2,'\0', 500 * sizeof(char));
    
    if (strIndex > 0) {
        int len;
        buf[0] = 0;
        len = GetStringDescriptor(deviceIntf, strIndex, buf, sizeof(buf),NULL);
        
        if (len > 2) {
            Byte *p;
            CFStringRef str;
            for (p = buf + 2; p < buf + len; p += 2)
            {
                Swap16(p);
            }
            
            str = CFStringCreateWithCharacters(NULL, (const UniChar *)(buf+2), (len-2)/2);
            CFStringGetCString(str, (char *)buf, 256, kCFStringEncodingNonLossyASCII);
            CFRelease(str);
            sprintf(str2, "\"%s\"", buf);
            return str2;
        }
        else {
            strcat(str2,"0x00");
            return str2;
        }
    }
    else {
        strcat(str2,"0x00");
        return str2;
    }
}

/*
BusProbeClass * GetClassAndSubClass(UInt8 * pcls) {
    BusProbeClass *bpClass = [[BusProbeClass alloc] init];
    NSString *cls = @"", *sub = @"";
    
    [bpClass setClassNum:pcls[0]];
    [bpClass setSubclassNum:pcls[1]];
    
    switch (pcls[0])
    {
        case kUSBCompositeClass:
            cls = @"Composite";
            break;
        case kUSBAudioClass:
            cls = @"Audio";
            
            switch (pcls[1])
            {
                case 0x01:
                    sub = @"Audio Control";
                    break;
                case 0x02:
                    sub = @"Audio Streaming";
                    break;
                case 0x03:
                    sub = @"MIDI Streaming";
                    break;
                default:
                    sub = @"Unknown";
                    break;
            }
                break;
            
        case kUSBCommClass:
            cls = @"Comm";
            break;
            
        case kUSBHIDClass:			
            cls = @"HID";
            switch (pcls[1])
            {
                case kUSBHIDBootInterfaceSubClass:
                    sub = @"Boot Interface";
                    break;
                default:
                    sub = @"";
                    break;
            }
                break;
            
        case kUSBDisplayClass:
            cls = @"Display";
            break;
            
        case kUSBPrintingClass:
            cls = @"Printing";
            break;
            
        case kUSBMassStorageClass:		
            cls = @"Mass Storage";
            switch (pcls[1])
            {
                case kUSBMassStorageRBCSubClass:        sub = @"Reduced Block Commands"; break;
                case kUSBMassStorageATAPISubClass:  	sub = @"ATAPI"; break;
                case kUSBMassStorageQIC157SubClass:  	sub = @"QIC-157"; break;
                case kUSBMassStorageUFISubClass:  	sub = @"UFI"; break;
                case kUSBMassStorageSFF8070iSubClass:  	sub = @"SFF-8070i"; break;
                case kUSBMassStorageSCSISubClass:  	sub = @"SCSI"; break;
                default:                        	sub = @"Unknown"; break;
            }
                break;
            
        case kUSBHubClass:
            cls = @"Hub";
            break;
            
        case kUSBDataClass:
            cls = @"Data";
            switch (pcls[1])
            {
                case kUSBCommDirectLineSubClass:	sub = @"Direct Line Model";  break;
                case kUSBCommAbstractSubClass:		sub = @"Abstract Model";   break;
                case kUSBCommTelephoneSubClass:		sub = @"Telephone Model"; break;
                case kUSBCommMultiChannelSubClass:	sub = @"Multi Channel Model"; break;
                case kUSBCommCAPISubClass:		sub = @"CAPI Model"; break;
                case kUSBCommEthernetNetworkingSubClass:sub = @"Ethernet Networking Model";  break;
                case kUSBATMNetworkingSubClass:		sub = @"ATM Networking Model";  break;
                default:				sub = @"Unknown Comm Class Model";  break;
            }
                break;
            
        case 0xE0:
            cls = @"Bluetooth Wireless Controller";
            break;
            
        case kUSBApplicationSpecificClass:
            cls = @"Application Specific";
            switch (pcls[1])
            {
                case kUSBDFUSubClass:         	sub = @"Device Firmware Upgrade"; break;
                case kUSBIrDABridgeSubClass:  	sub = @"IrDA Bridge"; break;
                default:                        sub = @"Unknown"; break;
            }
                break;
            
        case kUSBVendorSpecificClass:
            cls = sub = @"Vendor-specific";
            break;
            
        default:
            cls = @"Unknown";
            break;
    }

    [bpClass setClassName:cls];
    [bpClass setSubclassName:sub];
 
    return [bpClass autorelease];
}
*/

BusProbeClass * GetDeviceClassAndSubClass(UInt8 * pcls) {
    BusProbeClass *bpClass = [[BusProbeClass alloc] init];
    NSString *cls = @"", *sub = @"", *protocol = @"";
    
    switch (pcls[0]) {
        case kUSBCompositeClass:
            cls = @"Composite";
            break;
        case kUSBCommClass:
            cls = @"Communication";
            break;
        case kUSBHubClass:
            cls = @"Hub";
            break;
        case 0xDC:
            cls = @"Diagnostic Device";
            switch (pcls[1]) {
                case 0x01:
                    sub = @"Reprogrammable Diagnostic Device";
                    break;
                default:
                    sub = @"Unknown";
                    break;
            }
            switch (pcls[2]) {
                case 0x01:
                    protocol = @"USB2 Compliance Device";
                    break;
                default:
                    protocol = @"Unknown";
                    break;
            }
            break;
        case 0xE0:
            cls = @"Wireless Controller";
            switch (pcls[1]) {
                case 0x01:
                    sub = @"RF Controller";
                    break;
                default:
                    sub = @"Unknown";
                    break;
            }
            switch (pcls[2]) {
                case 0x01:
                    protocol = @"Bluetooth Programming Interface";
                    break;
                default:
                    protocol = @"Unknown";
                    break;
            }
            break;
        case 0xEF:
            cls = @"Miscellaneous";
            switch (pcls[1]) {
                case 0x02:
                    sub = @"Common Class";
                    break;
                default:
                    sub = @"Unknown";
                    break;
            }
            switch (pcls[2]) {
                case 0x01:
                    protocol = @"Interface Association";
                    break;
                default:
                    protocol = @"Unknown";
                    break;
            }
            break;
        case kUSBVendorSpecificClass:
            cls = sub = @"Vendor-specific";
            break;            
        default:
            cls = @"Unknown";
            break;
    }
    
    [bpClass setClassNum:pcls[0]];
    [bpClass setSubclassNum:pcls[1]];
    [bpClass setProtocolNum:pcls[2]];
    
    [bpClass setClassName:cls];
    [bpClass setSubclassName:sub];
    [bpClass setProtocolName:protocol];
    
    return [bpClass autorelease];
}

BusProbeClass * GetInterfaceClassAndSubClass(UInt8 * pcls) {
    BusProbeClass *bpClass = [[BusProbeClass alloc] init];
    NSString *cls = @"", *sub = @"";
    
    switch (pcls[0]) {
        case kUSBAudioClass:
            cls = @"Audio";
            switch (pcls[1]) {
                case 0x01:
                    sub = @"Audio Control";
                    break;
                case 0x02:
                    sub = @"Audio Streaming";
                    break;
                case 0x03:
                    sub = @"MIDI Streaming";
                    break;
                default:
                    sub = @"Unknown";
                    break;
            }
            break;
        case kUSBCommClass:
            cls = @"Communications-Control";
            break;
        case kUSBHIDClass:			
            cls = @"HID";
            switch (pcls[1])
            {
                case kUSBHIDBootInterfaceSubClass:
                    sub = @"Boot Interface";
                    break;
                default:
                    sub = @"";
                    break;
            }
            break;
        case kUSBDisplayClass:
            cls = @"Display";
            break;
        case 0x05:
            cls = @"Physical";
            break;
        case 0x06:
            cls = @"Image";
            break;
        case kUSBPrintingClass:
            cls = @"Printer";
            break;
        case kUSBMassStorageClass:		
            cls = @"Mass Storage";
            switch (pcls[1])
            {
                case kUSBMassStorageRBCSubClass:        sub = @"Reduced Block Commands"; break;
                case kUSBMassStorageATAPISubClass:  	sub = @"ATAPI"; break;
                case kUSBMassStorageQIC157SubClass:  	sub = @"QIC-157"; break;
                case kUSBMassStorageUFISubClass:  	sub = @"UFI"; break;
                case kUSBMassStorageSFF8070iSubClass:  	sub = @"SFF-8070i"; break;
                case kUSBMassStorageSCSISubClass:  	sub = @"SCSI"; break;
                default:                        	sub = @"Unknown"; break;
            }
            break;
            
        case kUSBHubClass:
            cls = @"Hub";
            break;
            
        case kUSBDataClass:
            cls = @"Communications-Data";
            switch (pcls[1])
            {
                case kUSBCommDirectLineSubClass:	sub = @"Direct Line Model";  break;
                case kUSBCommAbstractSubClass:		sub = @"Abstract Model";   break;
                case kUSBCommTelephoneSubClass:		sub = @"Telephone Model"; break;
                case kUSBCommMultiChannelSubClass:	sub = @"Multi Channel Model"; break;
                case kUSBCommCAPISubClass:		sub = @"CAPI Model"; break;
                case kUSBCommEthernetNetworkingSubClass:sub = @"Ethernet Networking Model";  break;
                case kUSBATMNetworkingSubClass:		sub = @"ATM Networking Model";  break;
                default:				sub = @"Unknown Comm Class Model";  break;
            }
            break;
        case 0x0B:
            cls = @"Chip/Smart-Card";
            break;
        case 0x0D:
            cls = @"Content-Security";
            break;
        case 0x0E:
            cls = @"Video";
            break;
        case 0xDC:
            cls = @"Diagnostic Device";
            switch (pcls[1]) {
                case 0x01:
                    sub = @"Reprogrammable Diagnostic Device";
                    break;
                default:
                    sub = @"Unknown";
                    break;
            }
            break;
        case 0xE0:
            cls = @"Wireless Controller";
            switch (pcls[1]) {
                case 0x01:
                    sub = @"RF Controller";
                    break;
                default:
                    sub = @"Unknown";
                    break;
            }
            break;
        case kUSBApplicationSpecificClass:
            cls = @"Application Specific";
            switch (pcls[1]) {
                case 0x01:
                    sub = @"Device Firmware Update";
                    break;
                case 0x02:
                    sub = @"IrDA Bridge";
                    break;
                case 0x03:
                    sub = @"Test & Measurement Class";
                    break;
                default:
                    sub = @"Unknown";
                    break;
            }
            break;
        case kUSBVendorSpecificClass:
            cls = sub = @"Vendor-specific";
            break;
        default:
            cls = @"Unknown";
            break;
    }
    
    [bpClass setClassNum:pcls[0]];
    [bpClass setSubclassNum:pcls[1]];
    
    [bpClass setClassName:cls];
    [bpClass setSubclassName:sub];
    
    return [bpClass autorelease];
}

NSString * VendorNameFromVendorID(NSString * intValueAsString) {
    static NSMutableDictionary * gVendorNamesDictionary = nil;
    NSString *vendorName;
    
    if (gVendorNamesDictionary == nil) {
        gVendorNamesDictionary = [[NSMutableDictionary dictionary] retain];
        
        NSString *vendorListString = [NSString stringWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"USBVendors" ofType:@"txt"]];
        
        if (vendorListString == nil) { 
            NSLog(@"Error reading USBVendors.txt from the Resources directory");
        } else {
            NSArray *vendorsAndIDs = [vendorListString componentsSeparatedByString:@"\n"];
            if (vendorsAndIDs == nil) { 
                NSLog(@"Error parsing USBVendors.txt");
            } else {
                NSEnumerator *enumerator = [vendorsAndIDs objectEnumerator];
                NSString *vendorIDCombo;
                NSArray *aVendor;
                while ((vendorIDCombo = [enumerator nextObject])) {
                    aVendor = [vendorIDCombo componentsSeparatedByString:@"|"];
                    if (aVendor == nil || [aVendor count] < 2) { 
                        continue;
                    }
                    [gVendorNamesDictionary setObject:[aVendor objectAtIndex:1] forKey:[aVendor objectAtIndex:0]];
                }
            }
        }
    }
    
    vendorName = [gVendorNamesDictionary objectForKey:intValueAsString];
    
    return (vendorName != nil ? vendorName : @"unknown vendor");
}

void FreeString(char * cstr) {
    if (cstr != NULL) {
        free(cstr);
        cstr = NULL;
    }
}

UInt16	Swap16(void *p) {
    * (UInt16 *) p = CFSwapInt16LittleToHost(*(UInt16 *)p);
    return * (UInt16 *) p;
}

UInt32	Swap32(void *p) {
    * (UInt32 *) p = CFSwapInt32LittleToHost(*(UInt32 *)p);
    return * (UInt32 *) p;
}

UInt64	Swap64(void *p) {
    * (UInt64 *) p = CFSwapInt64LittleToHost(*(UInt64 *)p);
    return * (UInt64 *) p;
}
