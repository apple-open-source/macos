//
//  DecodeBOSDescriptor.m
//  USBProber
//
//  Created by Fernando Urbina on 3/8/11.
//  Copyright 2011 Independent SW. All rights reserved.
//

#import "DescriptorDecoder.h"
#import "DecodeBOSDescriptor.h"

#ifdef SUPPORTS_SS_USB

@implementation DecodeBOSDescriptor

+ (void)decodeBytes:(IOUSBBOSDescriptor *)bosDescriptor forDevice:(BusProbeDevice *)thisDevice deviceInterface:(IOUSBDeviceRef)deviceIntf {
    char                        buf[128];
	UInt8						numCaps = bosDescriptor->bNumDeviceCaps;
	Byte                        *bosBuf;
    Byte                        *p;
    IOReturn                     ret = kIOReturnSuccess;
	int							i;

    [thisDevice addProperty:"BOS Descriptor" withValue:"" atDepth:BOS_DESCRIPTOR_LEVEL-1];
    	
	if ( bosDescriptor->wTotalLength != 0 )
	{
		// We have device capabilities, so let's get the full descriptor
		Swap16(&bosDescriptor->wTotalLength);
		bosBuf = malloc(bosDescriptor->wTotalLength*sizeof(Byte));
		
		ret = GetDescriptor(deviceIntf, kUSBBOSDescriptor, 0, bosBuf, bosDescriptor->wTotalLength, nil);
		if ( ret != kIOReturnSuccess )
		{
			fprintf(stderr,"Failed to get full BOS descriptor: 0x%x\n", ret);
			return;
		}
		
	    sprintf(buf, "%u", bosDescriptor->wTotalLength);
		[thisDevice addProperty:"Length (and contents):" withValue:buf atDepth:BOS_DESCRIPTOR_LEVEL];
		[DescriptorDecoder dumpRawBOSDescriptor:(IOUSBBOSDescriptor *)bosBuf forDevice:thisDevice atDepth:BOS_DESCRIPTOR_LEVEL+1];
	
		sprintf(buf, "%u", (bosDescriptor->bNumDeviceCaps));
		[thisDevice addProperty:"Number of Capability Descriptors:" withValue:buf atDepth:BOS_DESCRIPTOR_LEVEL];
		
		// point to the start of the caps
		//
		p = &bosBuf[5];
		
		for ( i = 0 ; i < numCaps; i++)
		{
			IOUSBDeviceCapabilityDescriptorHeader	*header = (IOUSBDeviceCapabilityDescriptorHeader *)p;

			sprintf(buf, "%u", (header->bDevCapabilityType));

			switch (header->bDevCapabilityType)
			{
				case kUSBDeviceCapabilityWirelessUSB:
				{
					[thisDevice addProperty:"Wireless USB:" withValue:buf atDepth:BOS_DESCRIPTOR_LEVEL+1];
				}
				break;
					
				case kUSBDeviceCapabilityUSB20Extension:
				{
					IOUSBDeviceCapabilityUSB2Extension *	usb2Cap = (IOUSBDeviceCapabilityUSB2Extension*)p;
					UInt32	attributes = USBToHostLong(usb2Cap->bmAttributes);
					
					[thisDevice addProperty:"USB 2.0 Extension:" withValue:buf atDepth:BOS_DESCRIPTOR_LEVEL+1];
					sprintf(buf, "0x%8.8x", (uint32_t)attributes);
					[thisDevice addProperty:"bmAttributes:" withValue:buf atDepth:BOS_DESCRIPTOR_LEVEL+2];
					if ( attributes &( 1 << kUSB20ExtensionLPMSupported) )
						[thisDevice addProperty:"" withValue:"Bit 1:  Link Power Management supported" atDepth:BOS_DESCRIPTOR_LEVEL+2];
				}
				break;
					
				case kUSBDeviceCapabilitySuperSpeedUSB:
				{
					IOUSBDeviceCapabilitySuperSpeedUSB *	sspeedCap = (IOUSBDeviceCapabilitySuperSpeedUSB*)p;
					UInt8	attributes = (sspeedCap->bmAttributes);
					UInt16	speedsSupported = USBToHostWord(sspeedCap->wSpeedsSupported);
					UInt8	speedFunctionalitySupport = sspeedCap->bFunctionalitySupport;
					UInt8	u1ExitLatency = sspeedCap->bU1DevExitLat;
					UInt16	u2ExitLatency = USBToHostWord(sspeedCap->wU2DevExitLat);
					
					[thisDevice addProperty:"SuperSpeed USB Device:" withValue:buf atDepth:BOS_DESCRIPTOR_LEVEL+1];
					sprintf(buf, "0x%8.8x", attributes);
					[thisDevice addProperty:"bmAttributes:" withValue:buf atDepth:BOS_DESCRIPTOR_LEVEL+2];
					if ( attributes &( 1 << kUSBSuperSpeedLTMCapable) )
						[thisDevice addProperty:"" withValue:"Bit 1:  Latency Tolerance Messages supported" atDepth:BOS_DESCRIPTOR_LEVEL+2];
					
					sprintf(buf, "0x%4.4x", speedsSupported);
					[thisDevice addProperty:"wSpeedsSupported:" withValue:buf atDepth:BOS_DESCRIPTOR_LEVEL+2];
					if ( speedsSupported & (1<<kUSBSuperSpeedSupportsLS) )
						[thisDevice addProperty:"" withValue:"Bit 0:  Low Speed supported" atDepth:BOS_DESCRIPTOR_LEVEL+2];
					if ( speedsSupported & (1<<kUSBSuperSpeedSupportsFS) )
						[thisDevice addProperty:"" withValue:"Bit 1:  Full Speed supported" atDepth:BOS_DESCRIPTOR_LEVEL+2];
					if ( speedsSupported & (1<<kUSBSuperSpeedSupportsHS) )
						[thisDevice addProperty:"" withValue:"Bit 2:  High Speed supported" atDepth:BOS_DESCRIPTOR_LEVEL+2];
					if ( speedsSupported & (1<<kUSBSuperSpeedSupportsSS) )
						[thisDevice addProperty:"" withValue:"Bit 3:  Super Speed (5Gbps) supported" atDepth:BOS_DESCRIPTOR_LEVEL+2];
					
					if ( speedFunctionalitySupport & (1<<kUSBSuperSpeedSupportsLS) )
						sprintf(buf, "0x%4.4x:  Low Speed and above", speedFunctionalitySupport);
					if ( speedFunctionalitySupport & (1<<kUSBSuperSpeedSupportsFS) )
						sprintf(buf, "0x%4.4x:  Full Speed and above", speedFunctionalitySupport);
					if ( speedFunctionalitySupport & (1<<kUSBSuperSpeedSupportsHS) )
						sprintf(buf, "0x%4.4x:  High Speed and above", speedFunctionalitySupport);
					if ( speedFunctionalitySupport & (1<<kUSBSuperSpeedSupportsSS) )
						sprintf(buf, "0x%4.4x:  Super Speed Only", speedFunctionalitySupport);
					[thisDevice addProperty:"bFunctionalitySupport:" withValue:buf atDepth:BOS_DESCRIPTOR_LEVEL+2];
					
					if (u1ExitLatency == 0 )
					{
						[thisDevice addProperty:"bU1ExitLat:" withValue:"Zero" atDepth:BOS_DESCRIPTOR_LEVEL+2];
					}
					else if (u1ExitLatency < 0xB )
					{
						sprintf(buf, "Less than %u µs", u1ExitLatency);
						[thisDevice addProperty:"bU1ExitLat:" withValue:buf atDepth:BOS_DESCRIPTOR_LEVEL+2];
					}
					else
					{
						sprintf(buf, "Illegal value of %u", u1ExitLatency);
						[thisDevice addProperty:"wU2ExitLat:" withValue:buf atDepth:BOS_DESCRIPTOR_LEVEL+2];
					}
					
					if (u2ExitLatency == 0 )
					{
						[thisDevice addProperty:"wU2ExitLat:" withValue:"Zero" atDepth:BOS_DESCRIPTOR_LEVEL+2];
					}
					else if (u2ExitLatency < 0x800 )
					{
						sprintf(buf, "Less than %u µs", u2ExitLatency);
						[thisDevice addProperty:"wU2ExitLat:" withValue:buf atDepth:BOS_DESCRIPTOR_LEVEL+2];
					}
					else
					{
						sprintf(buf, "Illegal value of %u", u2ExitLatency);
						[thisDevice addProperty:"wU2ExitLat:" withValue:buf atDepth:BOS_DESCRIPTOR_LEVEL+2];
					}
				}
				break;
					
				case kUSBDeviceCapabilityContainerID:
				{
					uint64_t					uuidLO;
					uint32_t					data1;
					uint16_t					data2, data3;

					IOUSBDeviceCapabilityContainerID *	container = (IOUSBDeviceCapabilityContainerID*)p;

					data1 = USBToHostLong(* (uint32_t *) &container->containerID[0]);
					data2 = USBToHostWord(* (uint16_t *) &container->containerID[4]);
					data3 = USBToHostWord(* (uint16_t *) &container->containerID[6]);
					uuidLO = OSSwapBigToHostInt64(* (uint64_t *) &container->containerID[8]);
					
					
					sprintf((char *)buf, 	"%8.8x-%4.4x-%4.4x-%4.4x-%12.12qx", data1, data2, data3, 
							(uint32_t) ( (uuidLO & 0xffff000000000000ULL)>>48), (uuidLO & 0x0000FFFFFFFFFFFFULL) );
					[thisDevice addProperty:"ContainerID:" withValue:buf atDepth:BOS_DESCRIPTOR_LEVEL+1];
				}
				break;
			}
			
			// Look at the next cap
			p = p + header->bLength;
		}
	}
		
 //   [thisDevice addNumberProperty:"Number of Ports:" value: hubDescriptor.numPorts size:sizeof(hubDescriptor.numPorts) atDepth:HUB_DESCRIPTOR_LEVEL usingStyle:kHexOutputStyle];

}

@end
#endif
