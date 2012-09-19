/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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

#import "DecodeVideoInterfaceDescriptor.h"


@implementation DecodeVideoInterfaceDescriptor

+(void)decodeBytes:(UInt8 *)descriptor forDevice:(BusProbeDevice *)thisDevice withDeviceInterface:(IOUSBDeviceRef)deviceIntf
{
	
    static  char			buf[256];
    static  char			buf2[256];
    auto IOUSBVCInterfaceDescriptor *		pVideoControlHeader = NULL;
    auto IOUSBVCInputTerminalDescriptor *	pVideoInTermDesc = NULL;
    auto IOUSBVCCameraTerminalDescriptor *	pCameraTermDesc = NULL;
    auto IOUSBVCOutputTerminalDescriptor *	pVideoOutTermDesc = NULL;
    auto IOUSBVCSelectorUnitDescriptor *	pSelectorUnitDesc = NULL;
    auto IOUSBVCSelectorUnit2Descriptor *	pSelectorUnit2Desc = NULL;
    auto IOUSBVCProcessingUnitDescriptor *	pProcessingUnitDesc = NULL;
    auto IOUSBVCProcessingUnit2Descriptor *	pProcessingUnit2Desc = NULL;
    auto IOUSBVCExtensionUnitDescriptor *	pExtensionUnitDesc = NULL;
    auto IOUSBVCExtensionUnit2Descriptor *	pExtensionUnit2Desc = NULL;
    auto IOUSBVCExtensionUnit3Descriptor *	pExtensionUnit3Desc = NULL;
    auto IOUSBVCInterruptEndpointDescriptor *	pVCInterruptEndpintDesc = NULL;
    auto IOUSBVSInputHeaderDescriptor *		pVSInputHeaderDesc = NULL;
    auto IOUSBVSOutputHeaderDescriptor *	pVSOutputHeaderDesc = NULL;
    auto IOUSBVDC_MJPEGFormatDescriptor *	pMJPEGFormatDesc = NULL;
    auto IOUSBVDC_MJPEGFrameDescriptor *	pMJPEGFrameDesc = NULL;
    auto IOUSBVDC_MJPEGDiscreteFrameDescriptor *	pMJPEGDiscreteFrameDesc = NULL;
    auto IOUSBVDC_ColorFormatDescriptor *       pColorFormatDesc = NULL;
    auto IOUSBVDC_UncompressedFormatDescriptor * pUncompressedFormatDesc = NULL;
    auto IOUSBVDC_UncompressedFrameDescriptor * pUncompressedFrameDesc = NULL;
    auto IOUSBVDC_UncompressedDiscreteFrameDescriptor * pUncompressedDiscreteFrameDesc = NULL;
	auto IOUSBVDC_StillImageFrameDescriptor * pVSStillImageFrameDesc = NULL;
	auto IOUSBVDC_DVFormatDescriptor * pDVFormatDesc = NULL;
	auto IOUSBVDC_FrameBasedFormatDescriptor * pFrameBasedFormatDesc = NULL;
	auto IOUSBVDC_FrameBasedFrameDescriptor * pFrameBasedFrameDesc = NULL;
	auto IOUSBVDC_DiscreteFrameBasedFrameDescriptor *	pFrameBasedDiscreteFrameDesc = NULL;
	
    UInt16					i, j;
    UInt8					*p;
    uint32_t					*t;
    char					*s = NULL;
    IOUSBVCInterfaceDescriptor *                desc = (IOUSBVCInterfaceDescriptor *) descriptor;
    UInt64					uuidLO;
    uint32_t                                      data1;
    UInt16                                      data2, data3;
    char					str[256];
	
    if ( desc->bDescriptorType == CS_ENDPOINT )
    {
        pVCInterruptEndpintDesc = (IOUSBVCInterruptEndpointDescriptor *)desc;
		
        switch ( pVCInterruptEndpintDesc->bDescriptorSubType )
        {
            case EP_INTERRUPT:
                sprintf((char *)buf, "VDC Specific Interrupt Endpoint");
                break;
            default:
                sprintf((char *)buf, "Unknown Endpoint SubType Descriptor");
        }
        [thisDevice addProperty:buf withValue:"" atDepth:INTERFACE_LEVEL];
		
        // Print the Length and contents of this class-specific descriptor
        //
        sprintf(str, "%u", pVCInterruptEndpintDesc->bLength);
        [thisDevice addProperty:"Length (and contents):" withValue:str atDepth:INTERFACE_LEVEL+1];
        [DescriptorDecoder dumpRawDescriptor:(Byte*)desc forDevice:thisDevice atDepth:INTERFACE_LEVEL+2];
		
        sprintf((char *)buf, "%u", Swap16(&pVCInterruptEndpintDesc->wMaxTransferSize) );
        [thisDevice addProperty:"Max Transfer Size:" withValue:buf atDepth:INTERFACE_LEVEL+1];
        return;
    }
    
    if ( desc->bDescriptorType != CS_INTERFACE )
        return;
	
    if ( SC_VIDEOCONTROL == [[thisDevice lastInterfaceClassInfo] subclassNum] )
    {
        switch ( desc->bDescriptorSubType )
        {
            case VC_DESCRIPTOR_UNDEFINED:
                sprintf((char *)buf, "Video Control Class Unknown Header");
                break;
            case VC_HEADER:
                sprintf((char *)buf, "VDC (Control) Header");
                break;
            case VC_INPUT_TERMINAL:
                sprintf((char *)buf, "VDC (Control) Input Terminal");
                break;
            case VC_OUTPUT_TERMINAL:
                sprintf((char *)buf, "VDC (Control) Output Terminal");
                break;
            case VC_SELECTOR_UNIT:
                sprintf((char *)buf, "VDC (Control) Selector Unit");
                break;
            case VC_PROCESSING_UNIT:
                sprintf((char *)buf, "VDC (Control) Processing Unit");
                break;
            case VC_EXTENSION_UNIT:
                sprintf((char *)buf, "VDC (Control) Extension Unit");
                break;
            default:
                sprintf((char *)buf, "Unknown SC_VIDEOCONTROL SubType Descriptor");
        }
    }
    else if ( SC_VIDEOSTREAMING == [[thisDevice lastInterfaceClassInfo] subclassNum] )
    {
        switch ( desc->bDescriptorSubType )
        {
            case VS_UNDEFINED:
                sprintf((char *)buf, "VDC (Streaming) Unknown Header");
                break;
            case VS_INPUT_HEADER:
                sprintf((char *)buf, "VDC (Streaming) Input Header");
                break;
            case VS_OUTPUT_HEADER:
                sprintf((char *)buf, "VDC (Streaming) Output Header");
                break;
            case VS_STILL_IMAGE_FRAME:
                sprintf((char *)buf, "VDC (Streaming) Still Image Frame Descriptor");
                break;
            case VS_FORMAT_UNCOMPRESSED:
                sprintf((char *)buf, "VDC (Streaming) Uncompressed Format Descriptor");
                break;
            case VS_FRAME_UNCOMPRESSED:
                sprintf((char *)buf, "VDC (Streaming) Uncompressed Frame Descriptor");
                break;
            case VS_FORMAT_MJPEG:
                sprintf((char *)buf, "VDC (Streaming) MJPEG Format Descriptor");
                break;
            case VS_FRAME_MJPEG:
                sprintf((char *)buf, "VDC (Streaming) MJPEG Frame Descriptor");
                break;
            case VS_FORMAT_MPEG1:
                sprintf((char *)buf, "VDC (Streaming) MPEG1 Format Descriptor");
                break;
            case VS_FORMAT_MPEG2PS:
                sprintf((char *)buf, "VDC (Streaming) MPEG2-PS Format Descriptor");
                break;
            case VS_FORMAT_MPEG2TS:
                sprintf((char *)buf, "VDC (Streaming) MPEG2-TS Format Descriptor");
                break;
            case VS_FORMAT_MPEG4SL:
                sprintf((char *)buf, "VDC (Streaming) MPEG4-SL Format Descriptor");
                break;
            case VS_FORMAT_DV:
                sprintf((char *)buf, "VDC (Streaming) DV Format Descriptor");
                break;
            case VS_COLORFORMAT:
                sprintf((char *)buf, "VDC (Streaming) Color Format Descriptor");
                break;
            case VS_FORMAT_VENDOR:
                sprintf((char *)buf, "VDC (Streaming) Vendor-specific Format Descriptor");
                break;
            case VS_FRAME_VENDOR:
                sprintf((char *)buf, "VDC (Streaming) Vendor-specific Frame Descriptor");
                break;
            case VS_FORMAT_FRAME_BASED:
                sprintf((char *)buf, "VDC (Streaming) Frame-Based Format Descriptor");
                break;
            case VS_FRAME_FRAME_BASED:
                sprintf((char *)buf, "VDC (Streaming) Frame-Based Frame Descriptor");
                break;
            case VS_FORMAT_STREAM_BASED:
                sprintf((char *)buf, "VDC (Streaming) Stream-Based Frame Descriptor");
                break;
            default:
                sprintf((char *)buf, "Unknown SC_VIDEOSTREAMING SubType Descriptor");
        }
    }
    else
        sprintf((char *)buf, "Unknown VDC Interface Subclass Type");
	
    [thisDevice addProperty:buf withValue:"" atDepth:INTERFACE_LEVEL];
	
    // Print the Length and contents of this class-specific descriptor
    //
    sprintf(str, "%u", desc->bLength);
    [thisDevice addProperty:"Length (and contents):" withValue:str atDepth:INTERFACE_LEVEL+1];
    [DescriptorDecoder dumpRawDescriptor:(Byte*)desc forDevice:thisDevice atDepth:INTERFACE_LEVEL+2];
    sprintf((char *)buf, 	"0x%x", desc->bDescriptorType );
    [thisDevice addProperty:"bDescriptorType:" withValue:buf atDepth:INTERFACE_LEVEL+1];
    sprintf((char *)buf, 	"0x%x", desc->bDescriptorSubType );
    [thisDevice addProperty:"bDescriptorSubType:" withValue:buf atDepth:INTERFACE_LEVEL+1];
    
    
    
    if ( SC_VIDEOCONTROL == [[thisDevice lastInterfaceClassInfo] subclassNum] ) // Video Control Subclass
    {
        switch ( desc->bDescriptorSubType )
        {
            case VC_HEADER:
                pVideoControlHeader = (IOUSBVCInterfaceDescriptor *)desc;
                i = Swap16(&pVideoControlHeader->bcdVDC);
                
                // If release is less than 0x0100, then we need to display the release version
                if ( (pVideoControlHeader->bcdVDC & 0xFF00) == 0 )
                {
                    sprintf((char *)buf, "%1x%1x.%1x%1c", (i>>12)&0x000f, (i>>8)&0x000f, (i>>4)&0x000f, MapNumberToVersion((i>>0)&0x000f));
                }
				else
				{
					sprintf((char *)buf, "%1x%1x.%1x%1c", (i>>12)&0x000f, (i>>8)&0x000f, (i>>4)&0x000f, (i>>0)&0x000f);
				}
				
				[thisDevice addProperty:"Specification Version Number:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
                sprintf((char *)buf, "%u", Swap16(&pVideoControlHeader->wTotalLength) );
				
                sprintf((char *)buf, "%u", Swap32(&pVideoControlHeader->dwClockFrequency) );
                [thisDevice addProperty:"Device Clock Frequency (Hz):" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
                sprintf((char *)buf, "%u", pVideoControlHeader->bInCollection );
                [thisDevice addProperty:"Number of Video Streaming Interfaces:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
                // Haven't seen this array filled with more than 1 yet.
                //
                for (i = 0, p = &pVideoControlHeader->baInterfaceNr[0]; i < pVideoControlHeader->bInCollection; i++, p++ )
                {
                    sprintf((char *)buf, "%u", *p );
                    [thisDevice addProperty:"Video Interface Number:" withValue:buf atDepth:INTERFACE_LEVEL+1];
                }
				break;
				
            case VC_INPUT_TERMINAL:
                pVideoInTermDesc = (IOUSBVCInputTerminalDescriptor *)desc;
				
                sprintf((char *)buf, "%u", pVideoInTermDesc->bTerminalID );
                [thisDevice addProperty:"Terminal ID" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
                switch ( Swap16(&pVideoInTermDesc->wTerminalType) )
			{
				case TT_VENDOR_SPECIFIC: 	s="USB vendor specific"; break;
				case TT_STREAMING: 		s="USB streaming"; break;
				case ITT_VENDOR_SPECIFIC: 	s="Vendor Specific Input Terminal"; break;
				case ITT_CAMERA: 		s="Camera Sensor"; break;
				case ITT_MEDIA_TRANSPORT_UNIT: 	s="Sequential Media"; break;
				default: 				s="Invalid Input Terminal Type";
			}
				
				sprintf((char *)buf, 	"0x%x (%s)", pVideoInTermDesc->wTerminalType, s );
                [thisDevice addProperty:"Input Terminal Type:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
                if ( !pVideoInTermDesc->bAssocTerminal )
                    sprintf((char *)buf, "%u [NONE]", pVideoInTermDesc->bAssocTerminal );
                else
                    sprintf((char *)buf, "%u", pVideoInTermDesc->bAssocTerminal );
				
                [thisDevice addProperty:"Input Terminal ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				
                if ( !pVideoInTermDesc->iTerminal )
                {
                    sprintf((char *)buf, "%u [NONE]", pVideoInTermDesc->iTerminal );
                    [thisDevice addProperty:"Input Terminal String Index:" withValue:buf atDepth:INTERFACE_LEVEL+1];
                }
				else
				{
					[thisDevice addStringProperty:"Input Terminal String:" fromStringIndex: (UInt8)pVideoInTermDesc->iTerminal fromDeviceInterface:deviceIntf atDepth:INTERFACE_LEVEL+1];
				}
				
				if ( ITT_CAMERA == pVideoInTermDesc->wTerminalType )
				{
					pCameraTermDesc = (IOUSBVCCameraTerminalDescriptor *)desc;
					
					sprintf((char *)buf, "%u", Swap16(&pCameraTermDesc->wObjectiveFocalLengthMin) );
					[thisDevice addProperty:"Minimum Focal Length" withValue:buf atDepth:INTERFACE_LEVEL+1];
					
					sprintf((char *)buf, "%u", Swap16(&pCameraTermDesc->wObjectiveFocalLengthMax) );
					[thisDevice addProperty:"Maximum Focal Length" withValue:buf atDepth:INTERFACE_LEVEL+1];
					
					sprintf((char *)buf, "%u", Swap16(&pCameraTermDesc->wOcularFocalLength) );
					[thisDevice addProperty:"Ocular Focal Length" withValue:buf atDepth:INTERFACE_LEVEL+1];
					
					if ( pCameraTermDesc->bControlSize != 0 )
					{
						sprintf((char *)buf, "Description");
						[thisDevice addProperty:"Controls Supported" withValue:buf atDepth:INTERFACE_LEVEL+1];
					}
					
					strcpy((char *)buf, "");
					
					/*
					 // Need to swap the following bmControls field
					 //
					 switch ( pCameraTermDesc->bControlSize)
					 {
					 case 1: break;
					 case 2: Swap16(&pCameraTermDesc->bmControls); break;
					 //case 3: Swap24(&pCameraTermDesc->bmControls); break;
					 case 3:  break;
					 case 4: Swap32(&pCameraTermDesc->bmControls); break;
					 }
					 */
					
					for (i = 0, p = &pCameraTermDesc->bmControls[0]; i < pCameraTermDesc->bControlSize; i++, p++ )
					{
						// For 1.0, only 19 bits are defined....
						// p is pointing to the first byte of a little endian field.
						//
						// i = 0, p -> D0-D7
						// i = 1, p -> D8-D15
						// i = 2, p -> D16-D23
						// i = 4, p -> D24-D31
						//
						if ( i > 2 )
						{
							sprintf((char *)buf, "Unknown");
							[thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
						}
						
						if ( (*p) & (1 << 0) )
						{
							if ( i == 0 ) 	sprintf((char *)buf, "Scanning Mode");
							else if ( i == 1 ) 	sprintf((char *)buf, "Iris (Relative)");
							else if ( i == 2 ) 	sprintf((char *)buf, "Reserved");
							if ( strcmp(buf,"") )
								[thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
						}
						
						if ( (*p) & (1 << 1) )
						{
							if ( i == 0 ) 	sprintf((char *)buf, "Auto Exposure Mode");
							else if ( i == 1 ) 	sprintf((char *)buf, "Zoom (Absolute)");
							else if ( i == 2 ) 	sprintf((char *)buf, "Focus, Auto");
							if ( strcmp(buf,"") )
								[thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
						}
						
						if ( (*p) & (1 << 2) )
						{
							if ( i == 0 ) 	sprintf((char *)buf, "Auto Exposure Priority");
							else if ( i == 1 ) 	sprintf((char *)buf, "Zoom (Relative)");
							else if ( i == 2 ) 	sprintf((char *)buf, "Privacy");
							if ( strcmp(buf,"") )
								[thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
						}
						
						if ( (*p) & (1 << 3) )
						{
							if ( i == 0 ) 	sprintf((char *)buf, "Exposure Time (Absolute)");
							else if ( i == 1 ) 	sprintf((char *)buf, "Pan (Absolute)");
							else if ( i == 2 ) 	sprintf((char *)buf, "Reserved");
							if ( strcmp(buf,"") )
								[thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
						}
						
						if ( (*p) & (1 << 4) )
						{
							if ( i == 0 ) 	sprintf((char *)buf, "Exposure Time (Relative)");
							else if ( i == 1 ) 	sprintf((char *)buf, "Pan (Relative)");
							else if ( i == 2 ) 	sprintf((char *)buf, "Unknown");
							if ( strcmp(buf,"") )
								[thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
						}
						
						if ( (*p) & (1 << 5) )
						{
							if ( i == 0 ) 	sprintf((char *)buf, "Focus (Absolute)");
							else if ( i == 1 ) 	sprintf((char *)buf, "Roll (Absolute)");
							else if ( i == 2 ) 	sprintf((char *)buf, "Unknown");
							if ( strcmp(buf,"") )
								[thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
						}
						
						if ( (*p) & (1 << 6) )
						{
							if ( i == 0 ) 	sprintf((char *)buf, "Focus (Relative)");
							else if ( i == 1 ) 	sprintf((char *)buf, "Roll (Relative)");
							else if ( i == 2 ) 	sprintf((char *)buf, "Unknown");
							if ( strcmp(buf,"") )
								[thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
						}
						
						if ( (*p) & (1 << 7) )
						{
							if ( i == 0 ) 	sprintf((char *)buf, "Iris (Absolute)");
							else if ( i == 1 ) 	sprintf((char *)buf, "Tilt (Absolute)");
							else if ( i == 2 ) 	sprintf((char *)buf, "Unknown");
							if ( strcmp(buf,"") )
								[thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
						}
						
					}
				}
				break;
				
            case VC_OUTPUT_TERMINAL:
                pVideoOutTermDesc = (IOUSBVCOutputTerminalDescriptor *)desc;
				
                sprintf((char *)buf, "%u", pVideoOutTermDesc->bTerminalID );
                [thisDevice addProperty:"Terminal ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
                switch ( Swap16(&pVideoOutTermDesc->wTerminalType) )
			{
				case TT_VENDOR_SPECIFIC: 	s="USB vendor specific"; break;
				case TT_STREAMING: 		s="USB streaming"; break;
				case OTT_VENDOR_SPECIFIC: 	s="USB vendor specific"; break;
				case OTT_DISPLAY: 		s="Generic Display"; break;
				case OTT_MEDIA_TRANSPORT_OUTPUT: 	s="Sequential Media Output Terminal"; break;
				default: 				s="Invalid Output Terminal Type";
			}
				
				sprintf((char *)buf, 	"0x%x (%s)", pVideoOutTermDesc->wTerminalType, s );
                [thisDevice addProperty:"Output Terminal Type:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
                if ( !pVideoOutTermDesc->bAssocTerminal )
                    sprintf((char *)buf, "%u [NONE]", pVideoOutTermDesc->bAssocTerminal );
                else
                    sprintf((char *)buf, "%u", pVideoOutTermDesc->bAssocTerminal );
				
                [thisDevice addProperty:"Output Terminal ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				
                if ( !pVideoOutTermDesc->iTerminal )
                {
                    sprintf((char *)buf, "%u [NONE]", pVideoOutTermDesc->iTerminal );
                    [thisDevice addProperty:"Output Terminal String Index:" withValue:buf atDepth:INTERFACE_LEVEL+1];
                }
				else
				{
					[thisDevice addStringProperty:"Output Terminal String:" fromStringIndex: (UInt8)pVideoOutTermDesc->iTerminal fromDeviceInterface:deviceIntf atDepth:INTERFACE_LEVEL+1];
				}
				break;
				
            case VC_SELECTOR_UNIT:
                pSelectorUnitDesc = (IOUSBVCSelectorUnitDescriptor *)desc;
                sprintf((char *)buf, 	"%u", pSelectorUnitDesc->bUnitID );
                [thisDevice addProperty:"Unit ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
                sprintf((char *)buf, 	"%u", pSelectorUnitDesc->bNrInPins );
                [thisDevice addProperty:"Number of pins:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
                //
                for (i = 0, p = &pSelectorUnitDesc->baSourceID[0]; i < pSelectorUnitDesc->bNrInPins; i++, p++ )
                {
                    sprintf((char *)buf2,	"Source ID Pin[%d]:", i);
                    sprintf((char *)buf, "%u", *p );
                    [thisDevice addProperty:buf2 withValue:buf atDepth:INTERFACE_LEVEL+1];
                }
				
				// Now, p will point to the IOUSBVCSelectorUnit2Descriptor
				//
				pSelectorUnit2Desc = (IOUSBVCSelectorUnit2Descriptor *) p;
                if ( !pSelectorUnit2Desc->iSelector )
                {
                    sprintf((char *)buf, "%u [NONE]", pSelectorUnit2Desc->iSelector );
                    [thisDevice addProperty:"Selector Unit String Index:" withValue:buf atDepth:INTERFACE_LEVEL+1];
                }
				else
				{
					[thisDevice addStringProperty:"Selector Unit String:" fromStringIndex: (UInt8)pSelectorUnit2Desc->iSelector fromDeviceInterface:deviceIntf atDepth:INTERFACE_LEVEL+1];
				}
				
				break;
				
            case VC_PROCESSING_UNIT:
                pProcessingUnitDesc = ( IOUSBVCProcessingUnitDescriptor *) desc;
                sprintf((char *)buf, 	"%u", pProcessingUnitDesc->bUnitID );
                [thisDevice addProperty:"Unit ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
                sprintf((char *)buf, 	"%u", pProcessingUnitDesc->bSourceID );
                [thisDevice addProperty:"Source ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
                sprintf((char *)buf, 	"%u", Swap16(&pProcessingUnitDesc->wMaxMultiplier) );
                [thisDevice addProperty:"Digital Multiplier (100X):" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
                if ( pProcessingUnitDesc->bControlSize != 0 )
                {
                    sprintf((char *)buf, "Description");
                    [thisDevice addProperty:"Controls Supported" withValue:buf atDepth:INTERFACE_LEVEL+1];
                }
				
				strcpy((char *)buf, "");
				
                /*
				 // Need to swap the following bmControls field
				 //
				 switch ( pProcessingUnitDesc->bControlSize)
				 {
				 case 1: break;
				 case 2: Swap16(&pProcessingUnitDesc->bmControls); break;
				 case 3: Swap24(&pCameraTermDesc->bmControls); break;
				 case 4: Swap32(&pProcessingUnitDesc->bmControls); break;
				 }
				 */
                
                for (i = 0, p = &pProcessingUnitDesc->bmControls[0]; i < pProcessingUnitDesc->bControlSize; i++, p++ )
                {
                    // For 1.0, only 16 bits are defined:
                    //
                    // p is pointing to the first byte of a little endian field.
                    //
                    // i = 0, p -> D0-D7
                    // i = 1, p -> D8-D15
                    // i = 2, p -> D16-D23
                    // i = 4, p -> D24-D31
                    //
					
                    if ( (*p) & (1 << 0) )
                    {
                        if ( i == 0 ) 		sprintf((char *)buf, "Brightness");
                        else if ( i == 1 ) 	sprintf((char *)buf, "Backlight Compensation");
                        if ( strcmp(buf,"") )
                            [thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
                    }
					
                    if ( (*p) & (1 << 1) )
                    {
                        if ( i == 0 ) 		sprintf((char *)buf, "Contrast");
                        else if ( i == 1 ) 	sprintf((char *)buf, "Gain");
                        if ( strcmp(buf,"") )
                            [thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
                    }
					
                    if ( (*p) & (1 << 2) )
                    {
                        if ( i == 0 ) 		sprintf((char *)buf, "Hue");
                        else if ( i == 1 ) 	sprintf((char *)buf, "Power Line Frequency");
                        if ( strcmp(buf,"") )
                            [thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
                    }
					
                    if ( (*p) & (1 << 3) )
                    {
                        if ( i == 0 ) 		sprintf((char *)buf, "Saturation");
                        else if ( i == 1 ) 	sprintf((char *)buf, "Hue, Auto");
                        if ( strcmp(buf,"") )
                            [thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
                    }
					
                    if ( (*p) & (1 << 4) )
                    {
                        if ( i == 0 ) 		sprintf((char *)buf, "Sharpness");
                        else if ( i == 1 ) 	sprintf((char *)buf, "White Balance Temperature, Auto");
                        if ( strcmp(buf,"") )
                            [thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
                    }
					
                    if ( (*p) & (1 << 5) )
                    {
                        if ( i == 0 ) 		sprintf((char *)buf, "Gamma");
                        else if ( i == 1 ) 	sprintf((char *)buf, "White Balance Component, Auto");
                        if ( strcmp(buf,"") )
                            [thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
                    }
					
                    if ( (*p) & (1 << 6) )
                    {
                        if ( i == 0 ) 		sprintf((char *)buf, "White Balance Temperature");
                        else if ( i == 1 ) 	sprintf((char *)buf, "Digital Multiplier");
                        if ( strcmp(buf,"") )
                            [thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
                    }
					
                    if ( (*p) & (1 << 7) )
                    {
                        if ( i == 0 ) 		sprintf((char *)buf, "White Balance Component");
                        else if ( i == 1 ) 	sprintf((char *)buf, "Digital Multiplier Limit");
                        if ( strcmp(buf,"") )
                            [thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
                    }
					
                }
				
				// At this point, p should be pointing to the iProcessing field:
				pProcessingUnit2Desc = (IOUSBVCProcessingUnit2Descriptor *) p;
                if ( !pProcessingUnit2Desc->iProcessing )
                {
                    sprintf((char *)buf, "%u [NONE]", pProcessingUnit2Desc->iProcessing );
                    [thisDevice addProperty:"Processing Unit String Index:" withValue:buf atDepth:INTERFACE_LEVEL+1];
                }
				else
				{
					[thisDevice addStringProperty:"Selector Unit String:" fromStringIndex: (UInt8)pProcessingUnit2Desc->iProcessing fromDeviceInterface:deviceIntf atDepth:INTERFACE_LEVEL+1];
				}
				break;
				
            case VC_EXTENSION_UNIT:
                sprintf((char *)buf, 	"%u", desc->bLength );
                [thisDevice addProperty:"bLength:" withValue:buf atDepth:INTERFACE_LEVEL+1];
                sprintf((char *)buf, 	"%u", desc->bDescriptorType );
                [thisDevice addProperty:"bDescriptorType:" withValue:buf atDepth:INTERFACE_LEVEL+1];
                sprintf((char *)buf, 	"%u", desc->bDescriptorSubType );
                [thisDevice addProperty:"bDescriptorSubType:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
                pExtensionUnitDesc = (IOUSBVCExtensionUnitDescriptor *)desc;
				
                sprintf((char *)buf, 	"%u", pExtensionUnitDesc->bUnitID );
                [thisDevice addProperty:"Unit ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				data1 = USBToHostLong(* (uint32_t *) &pExtensionUnitDesc->guidFormat[0]);
				data2 = USBToHostWord(* (UInt16 *) &pExtensionUnitDesc->guidFormat[4]);
				data3 = USBToHostWord(* (UInt16 *) &pExtensionUnitDesc->guidFormat[6]);
				uuidLO = OSSwapBigToHostInt64(* (UInt64 *) &pExtensionUnitDesc->guidFormat[8]);
				
				
				sprintf((char *)buf, 	"%8.8x-%4.4x-%4.4x-%4.4x-%12.12qx", data1, data2, data3, 
						(uint32_t) ( (uuidLO & 0xffff000000000000ULL)>>48), (uuidLO & 0x0000FFFFFFFFFFFFULL) );
                [thisDevice addProperty:"Vendor UUID:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
                sprintf((char *)buf, 	"%u", pExtensionUnitDesc->bNumControls );
                [thisDevice addProperty:"Number of Controls:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				
                sprintf((char *)buf, 	"%u", pExtensionUnitDesc->bNrInPins );
                [thisDevice addProperty:"Number of In pins:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
                for (i = 0, p = &pExtensionUnitDesc->baSourceID[0]; i < pExtensionUnitDesc->bNrInPins; i++, p++ )
                {
                    sprintf((char *)buf2,	"Source ID Pin[%d]:", i);
                    sprintf((char *)buf, "%u", *p );
                    [thisDevice addProperty:buf2 withValue:buf atDepth:INTERFACE_LEVEL+1];
                }
				
				// Now, p points to the rest of the Extension Unit descriptor:
				//
				pExtensionUnit2Desc = ( IOUSBVCExtensionUnit2Descriptor *) p;
				
                if ( pExtensionUnit2Desc->bControlSize != 0 )
                {
                    sprintf((char *)buf, "Description");
                    [thisDevice addProperty:"Controls Supported" withValue:buf atDepth:INTERFACE_LEVEL+1];
                }
				
				strcpy((char *)buf, "");
                for (i = 0, p = &pExtensionUnit2Desc->bmControls[0]; i < pExtensionUnit2Desc->bControlSize; i++, p++ )
                {
                    // For 1.0, all bits are vendor specific:
                    //
                    sprintf((char *)buf, "Vendor Specific Byte[i] = 0x%x", (*p));
                    [thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
					
                }
				
				// At this point, p should be pointing to the iProcessing field:
				pExtensionUnit3Desc = ( IOUSBVCExtensionUnit3Descriptor *) p;
                if ( !pExtensionUnit3Desc->iExtension )
                {
                    sprintf((char *)buf, "%u [NONE]", pExtensionUnit3Desc->iExtension );
                    [thisDevice addProperty:"Processing Unit String Index:" withValue:buf atDepth:INTERFACE_LEVEL+1];
                }
				else
				{
					[thisDevice addStringProperty:"Selector Unit String:" fromStringIndex: (UInt8)pExtensionUnit3Desc->iExtension fromDeviceInterface:deviceIntf atDepth:INTERFACE_LEVEL+1];
				}
				break;
            case VC_DESCRIPTOR_UNDEFINED:
            default:
                [thisDevice addProperty:"Undefined Descriptor:" withValue:"" atDepth:INTERFACE_LEVEL+1];
				
				
        }
    }
    else if ( SC_VIDEOSTREAMING == [[thisDevice lastInterfaceClassInfo] subclassNum] ) // Video Streaming Subclass
    {
        switch ( desc->bDescriptorSubType )
		{
			case VS_INPUT_HEADER:
				pVSInputHeaderDesc = (IOUSBVSInputHeaderDescriptor *)desc;
				
				sprintf((char *)buf, "%u", pVSInputHeaderDesc->bNumFormats );
				[thisDevice addProperty:"bNumFormats:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x (%u)", Swap16(&pVSInputHeaderDesc->wTotalLength), pVSInputHeaderDesc->wTotalLength );
				[thisDevice addProperty:"wTotalLength:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x", pVSInputHeaderDesc->bEndpointAddress );
				[thisDevice addProperty:"bEndpointAddress:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "Capabilities (0x%x)", pVSInputHeaderDesc->bmInfo );
				[thisDevice addProperty:buf withValue:"bmInfo" atDepth:INTERFACE_LEVEL+1];
				
				if (pVSInputHeaderDesc->bmInfo & 0x01)
					[thisDevice addProperty:"" withValue:"Dynamic Format Change supported" atDepth:INTERFACE_LEVEL+2];
				
				if ( pVSInputHeaderDesc->bmInfo & 0xfe)
					[thisDevice addProperty:"" withValue:"Unknown capabilities" atDepth:INTERFACE_LEVEL+2];
				
				sprintf((char *)buf, "%u", pVSInputHeaderDesc->bTerminalLink );
				[thisDevice addProperty:"bTerminalLink:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				switch (pVSInputHeaderDesc->bStillCaptureMethod)
			{
				case 0: s = "None"; break;
				case 1: s = "Method 1"; break;
				case 2: s = "Method 2"; break;
				case 3: s = "Method 3"; break;
				default: s = "Unknown Method";
			}
				sprintf((char *)buf, 	"%u (%s)", pVSInputHeaderDesc->bStillCaptureMethod, s );
				[thisDevice addProperty:"bStillCaptureMethod:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "%s", pVSInputHeaderDesc->bTriggerSupport ? "1 (Supported)" : "0 (Not Supported)");
				[thisDevice addProperty:"bTriggerSupport" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				if ( pVSInputHeaderDesc->bTriggerSupport )
				{
					sprintf((char *)buf, "%s", pVSInputHeaderDesc->bTriggerUsage ? "1 (General Purpose)" : "(0) Initiate Still Image Capture");
				}
				else
				{
					sprintf((char *)buf, "%s", "Ignored because bTriggerSupport is 0");
				}
				[thisDevice addProperty:"bTriggerUsage" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x", pVSInputHeaderDesc->bControlSize );
				
				[thisDevice addProperty:"bControlSize:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				for (j = 0; j < pVSInputHeaderDesc->bNumFormats; j++ )
				{
					strcpy((char *)buf, "");
					strcpy((char *)buf2, "");
					for (i = 0, p = &pVSInputHeaderDesc->bmControls[0]; i < pVSInputHeaderDesc->bControlSize; i++, p++ )
					{
						sprintf((char *)buf, "0x%x", *p );
						strcat( buf2, buf);
					}
					sprintf((char *)buf, "bmaControls( Format %d):", j+1 );
					
					[thisDevice addProperty:buf withValue:buf2 atDepth:INTERFACE_LEVEL+1];
					
					strcpy((char *)buf, "");
					for (i = 0, p = &pVSInputHeaderDesc->bmControls[0]; i < pVSInputHeaderDesc->bControlSize; i++, p++ )
					{
						if ( (*p) & (1 << 0) )
						{
							if ( i == 0 )       sprintf((char *)buf, "Key Frame Rate");
							if ( strcmp(buf,"") )
								[thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
						}
						
						if ( (*p) & (1 << 1) )
						{
							if ( i == 0 ) 		sprintf((char *)buf, "P frame Rate");
							if ( strcmp(buf,"") )
								[thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
						}
						
						if ( (*p) & (1 << 2) )
						{
							if ( i == 0 ) 		sprintf((char *)buf, "Compression quality");
							if ( strcmp(buf,"") )
								[thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
						}
						
						if ( (*p) & (1 << 3) )
						{
							if ( i == 0 ) 		sprintf((char *)buf, "Compression window size");
							if ( strcmp(buf,"") )
								[thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
						}
						
						if ( (*p) & (1 << 4) )
						{
							if ( i == 0 ) 		sprintf((char *)buf, "Generate Key Frame");
							if ( strcmp(buf,"") )
								[thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
						}
						
						if ( (*p) & (1 << 5) )
						{
							if ( i == 0 ) 		sprintf((char *)buf, "Update Frame Segment");
							if ( strcmp(buf,"") )
								[thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
						}
						
						if ( (*p) & (1 << 6) )
						{
							sprintf((char *)buf, "Unknown");
							if ( strcmp(buf,"") )
								[thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
						}
						
						if ( (*p) & (1 << 7) )
						{
							sprintf((char *)buf, "Unknown");
							if ( strcmp(buf,"") )
								[thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
						}
						
					}
				}
				break;
			case VS_OUTPUT_HEADER:
				pVSOutputHeaderDesc = (IOUSBVSOutputHeaderDescriptor *)desc;
				
				sprintf((char *)buf, "%u", pVSOutputHeaderDesc->bNumFormats);
				[thisDevice addProperty:"Number of Formats:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "%u", Swap16(&pVSOutputHeaderDesc->wTotalLength) );
				[thisDevice addProperty:"Total Length of Descriptor:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x", pVSOutputHeaderDesc->bEndpointAddress );
				[thisDevice addProperty:"Endpoint Address:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "%u", pVSOutputHeaderDesc->bTerminalLink );
				[thisDevice addProperty:"Terminal ID of Output Terminal:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				break;
			case VS_FORMAT_MJPEG:
				pMJPEGFormatDesc = (IOUSBVDC_MJPEGFormatDescriptor *)desc;
				
				sprintf((char *)buf, "0x%x", pMJPEGFormatDesc->bFormatIndex);
				[thisDevice addProperty:"bFormatIndex:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x", pMJPEGFormatDesc->bNumFrameDescriptors);
				[thisDevice addProperty:"bNumFrameDescriptors:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "(0x%x)", pMJPEGFormatDesc->bmFlags );
				[thisDevice addProperty:"bmFlags" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				if (pMJPEGFormatDesc->bmFlags & 0x01)
					[thisDevice addProperty:"" withValue:"Fixed Sample Sizes Supported" atDepth:INTERFACE_LEVEL+2];
				
				if ( pMJPEGFormatDesc->bmFlags & 0xfe)
					[thisDevice addProperty:"" withValue:"Unknown characteristics" atDepth:INTERFACE_LEVEL+2];
				
				sprintf((char *)buf, "0x%x", pMJPEGFormatDesc->bDefaultFrameIndex);
				[thisDevice addProperty:"bDefaultFrameIndex:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x", pMJPEGFormatDesc->bAspectRatioX);
				[thisDevice addProperty:"bAspectRatioX:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x", pMJPEGFormatDesc->bAspectRatioY);
				[thisDevice addProperty:"bAspectRatioY:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "(0x%x)", pMJPEGFormatDesc->bmInterlaceFlags );
				[thisDevice addProperty:"bmInterlaceFlags" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				if (pMJPEGFormatDesc->bmInterlaceFlags & 0x01)
					[thisDevice addProperty:"Interlaced Stream or Variable" withValue:"Yes" atDepth:INTERFACE_LEVEL+2];
				else
					[thisDevice addProperty:"Interlaced Stream or Variable" withValue:"No" atDepth:INTERFACE_LEVEL+2];
				
				if (pMJPEGFormatDesc->bmInterlaceFlags & 0x02)
					[thisDevice addProperty:"Fields per frame" withValue:"2" atDepth:INTERFACE_LEVEL+2];
				else
					[thisDevice addProperty:"Fields per frame" withValue:"1" atDepth:INTERFACE_LEVEL+2];
				
				if (pMJPEGFormatDesc->bmInterlaceFlags & 0x04)
					[thisDevice addProperty:"Field 1 first" withValue:"Yes" atDepth:INTERFACE_LEVEL+2];
				else
					[thisDevice addProperty:"Field 1 first" withValue:"No" atDepth:INTERFACE_LEVEL+2];
				
				if (pMJPEGFormatDesc->bmInterlaceFlags & 0x08)
					[thisDevice addProperty:"" withValue:"Reserved field used!" atDepth:INTERFACE_LEVEL+2];
				
				i = (pMJPEGFormatDesc->bmInterlaceFlags & 0x30) >> 4;
				switch (i)
			{
				case 0: s = "Field 1 only"; break;
				case 1: s = "Field 2 only"; break;
				case 2: s = "Regular pattern of fields 1 and 2"; break;
				case 3: s = "Random pattern of fields 1 and 2"; break;
			}
				sprintf((char *)buf, 	"%s", s );
				[thisDevice addProperty:"Field Pattern" withValue:buf atDepth:INTERFACE_LEVEL+2];
				
				i = (pMJPEGFormatDesc->bmInterlaceFlags & 0xc0) >> 6;
				switch (i)
			{
				case 0: s = "Bob only"; break;
				case 1: s = "Weave only"; break;
				case 2: s = "Bob or weave"; break;
				case 3: s = "Unknown"; break;
			}
				sprintf((char *)buf, 	"%s", s );
				[thisDevice addProperty:"Display Mode" withValue:buf atDepth:INTERFACE_LEVEL+2];
				
				if (pMJPEGFormatDesc->bCopyProtect == 1)
					[thisDevice addProperty:"bCopyProtect:" withValue:"Restricted" atDepth:INTERFACE_LEVEL+1];
				else
					[thisDevice addProperty:"bCopyProtect" withValue:"No Restriction" atDepth:INTERFACE_LEVEL+1];
				break;
				
			case VS_FRAME_MJPEG:
				pMJPEGFrameDesc = (IOUSBVDC_MJPEGFrameDescriptor *)desc;
				
				sprintf((char *)buf, "%u", pMJPEGFrameDesc->bFrameIndex);
				[thisDevice addProperty:"bFrameIndex:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "(0x%x)", pMJPEGFrameDesc->bmCapabilities );
				[thisDevice addProperty:"bmCapabilities " withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				if (pMJPEGFrameDesc->bmCapabilities & 0x01)
					[thisDevice addProperty:"" withValue:"Still Image supported" atDepth:INTERFACE_LEVEL+2];
				
				if ( pMJPEGFrameDesc->bmCapabilities & 0xfe)
					[thisDevice addProperty:"" withValue:"Unknown capabilities" atDepth:INTERFACE_LEVEL+2];
				
				sprintf((char *)buf, "0x%x (%u)", Swap16(&pMJPEGFrameDesc->wWidth), pMJPEGFrameDesc->wWidth);
				[thisDevice addProperty:"wWidth:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x (%u)", Swap16(&pMJPEGFrameDesc->wHeight), pMJPEGFrameDesc->wHeight);
				[thisDevice addProperty:"wHeight:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x (%u)", Swap32(&pMJPEGFrameDesc->dwMinBitRate), pMJPEGFrameDesc->dwMinBitRate);
				[thisDevice addProperty:"dwMinBitRate (bps):" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x (%u)", Swap32(&pMJPEGFrameDesc->dwMaxBitRate), pMJPEGFrameDesc->dwMaxBitRate);
				[thisDevice addProperty:"dwMaxBitRate (bps):" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x (%u)", Swap32(&pMJPEGFrameDesc->dwMaxVideoFrameBufferSize), pMJPEGFrameDesc->dwMaxVideoFrameBufferSize);
				[thisDevice addProperty:"dwMaxVideoFrameBufferSize (bytes):" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x (%8.3f ms)", Swap32(&pMJPEGFrameDesc->dwDefaultFrameInterval), (double) ( pMJPEGFrameDesc->dwDefaultFrameInterval / 10000) );
				[thisDevice addProperty:"dwDefaultFrameInterval:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				if (pMJPEGFrameDesc->bFrameIntervalType == 0)
				{
					[thisDevice addProperty:"bFrameIntervalType:" withValue:"Continuous" atDepth:INTERFACE_LEVEL+1];
					
					sprintf((char *)buf, "0x%x (%8.3f ms)", Swap32(&pMJPEGFrameDesc->dwMinFrameInterval), (double) ( pMJPEGFrameDesc->dwMinFrameInterval / 10000) );
					[thisDevice addProperty:"dwMinFrameInterval:" withValue:buf atDepth:INTERFACE_LEVEL+2];
					
					sprintf((char *)buf, "0x%x (%8.3f ms)", Swap32(&pMJPEGFrameDesc->dwMaxFrameInterval), (double) ( pMJPEGFrameDesc->dwMaxFrameInterval / 10000) );
					[thisDevice addProperty:"dwMaxFrameInterval:" withValue:buf atDepth:INTERFACE_LEVEL+2];
					
					sprintf((char *)buf, "0x%x (%8.3f ms)", Swap32(&pMJPEGFrameDesc->dwFrameIntervalStep), (double) ( pMJPEGFrameDesc->dwFrameIntervalStep / 10000) );
					[thisDevice addProperty:"dwFrameIntervalStep:" withValue:buf atDepth:INTERFACE_LEVEL+2];
					
				}
				else
				{
					// Need to recast as a IOUSBVDC_MJPEGDiscreteFrameDescriptor
					//
					pMJPEGDiscreteFrameDesc = (IOUSBVDC_MJPEGDiscreteFrameDescriptor *) pMJPEGFrameDesc;
					sprintf((char *)buf, "%u", (pMJPEGDiscreteFrameDesc->bFrameIntervalType));
					[thisDevice addProperty:"Discrete Frame Intervals supported" withValue:buf atDepth:INTERFACE_LEVEL+1];
					for (i = 0, t = &pMJPEGDiscreteFrameDesc->dwFrameInterval[0]; i < pMJPEGDiscreteFrameDesc->bFrameIntervalType; i++, t++ )
					{
						UInt32 interval = *t;
						sprintf((char *)buf, "0x%x (%8.3f ms)", Swap32(&interval), (double) (interval / 10000));
						sprintf((char *)buf2, "dwFrameInterval[%u] (100 ns)", i+1 );
						[thisDevice addProperty:buf2 withValue:buf atDepth:INTERFACE_LEVEL+2];
					}
				}
				break;
				
			case VS_COLORFORMAT:
				pColorFormatDesc = (IOUSBVDC_ColorFormatDescriptor *)desc;
				
				switch (pColorFormatDesc->bColorPrimaries)
			{
				case 0:  s = "Unspecified (Image characteristic unknown)"; break;
				case 1:  s = "BT.709, sRGB (default)"; break;
				case 2:  s = "BT.470-2(M)"; break;
				case 3:  s = "BT.470-2(B,G)"; break;
				case 4:  s = "SMPTE 170M"; break;
				case 5:  s = "SMPTE 240M"; break;
				default: s = "reserved"; break;
			}
				
				sprintf((char *) buf, "%u ( %s )", pColorFormatDesc->bColorPrimaries, s);
				[thisDevice addProperty:"Color Primaries:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				switch (pColorFormatDesc->bTransferCharacteristics)
			{
				case 0:  s = "Unspecified (Image characteristic unknown)"; break;
				case 1:  s = "BT.709 (default)"; break;
				case 2:  s = "BT.470-2(M)"; break;
				case 3:  s = "BT.470-2(B,G)"; break;
				case 4:  s = "SMPTE 170M"; break;
				case 5:  s = "SMPTE 240M"; break;
				case 6:  s = "Linear ( V = Lc)"; break;
				case 7:  s = "sRGB (very similar to BT.709)"; break;
				default: s = "reserved"; break;
			}
				
				sprintf((char *) buf, "%u ( %s )", pColorFormatDesc->bTransferCharacteristics, s);
				[thisDevice addProperty:"Transfer Characteristics:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				switch (pColorFormatDesc->bMatrixCoefficients)
			{
				case 0:  s = "Unspecified (Image characteristic unknown)"; break;
				case 1:  s = "BT.709"; break;
				case 2:  s = "FCC"; break;
				case 3:  s = "BT.470-2(B,G)"; break;
				case 4:  s = "SMPTE 170M (BT.601, default)"; break;
				case 5:  s = "SMPTE 240M"; break;
				default: s = "reserved"; break;
			}
				
				sprintf((char *) buf, "%u ( %s )", pColorFormatDesc->bMatrixCoefficients, s);
				[thisDevice addProperty:"Matrix Coefficients:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				
				break;
				
			case VS_FORMAT_UNCOMPRESSED:
				pUncompressedFormatDesc = (IOUSBVDC_UncompressedFormatDescriptor *)desc;
				
				sprintf((char *)buf, "0x%x", pUncompressedFormatDesc->bFormatIndex);
				[thisDevice addProperty:"bFormatIndex:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x", pUncompressedFormatDesc->bNumFrameDescriptors);
				[thisDevice addProperty:"bNumFrameDescriptors:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				// Decode the GUID per section 2.9 of the FAQ
				//
				data1 = USBToHostLong(* (uint32_t *) &pUncompressedFormatDesc->guidFormat[0]);
				data2 = USBToHostWord(* (UInt16 *) &pUncompressedFormatDesc->guidFormat[4]);
				data3 = USBToHostWord(* (UInt16 *) &pUncompressedFormatDesc->guidFormat[6]);
				uuidLO = OSSwapBigToHostInt64(* (UInt64 *) &pUncompressedFormatDesc->guidFormat[8]);
				
				
				sprintf((char *)buf, 	"%8.8x-%4.4x-%4.4x-%4.4x-%12.12qx", data1, data2, data3, 
						(uint32_t) ( (uuidLO & 0xffff000000000000ULL)>>48), (uuidLO & 0x0000FFFFFFFFFFFFULL) );
				[thisDevice addProperty:"Format GUID:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x (%u)", pUncompressedFormatDesc->bBitsPerPixel,pUncompressedFormatDesc->bBitsPerPixel);
				[thisDevice addProperty:"bBitsPerPixel:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x", pUncompressedFormatDesc->bDefaultFrameIndex);
				[thisDevice addProperty:"bDefaultFrameIndex:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x", pUncompressedFormatDesc->bAspectRatioX);
				[thisDevice addProperty:"bAspectRatioX:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x", pUncompressedFormatDesc->bAspectRatioY);
				[thisDevice addProperty:"bAspectRatioY:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "(0x%x)", pUncompressedFormatDesc->bmInterlaceFlags );
				[thisDevice addProperty:"bmInterlaceFlags" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				if (pUncompressedFormatDesc->bmInterlaceFlags & 0x01)
					[thisDevice addProperty:"Interlace Stream or variable" withValue:"YES" atDepth:INTERFACE_LEVEL+2];
				else
					[thisDevice addProperty:"Interlace Stream or variable" withValue:"NO" atDepth:INTERFACE_LEVEL+2];
				
				if (pUncompressedFormatDesc->bmInterlaceFlags & 0x02)
					[thisDevice addProperty:"Fields per Frame" withValue:"1" atDepth:INTERFACE_LEVEL+2];
				else
					[thisDevice addProperty:"Fields per Frame" withValue:"2" atDepth:INTERFACE_LEVEL+2];
				
				if (pUncompressedFormatDesc->bmInterlaceFlags & 0x04)
					[thisDevice addProperty:"Field 1 First" withValue:"Yes" atDepth:INTERFACE_LEVEL+2];
				else
					[thisDevice addProperty:"Field 1 First" withValue:"No" atDepth:INTERFACE_LEVEL+2];
				
				
				if (pUncompressedFormatDesc->bmInterlaceFlags & 0x08)
					[thisDevice addProperty:"" withValue:"Reserved field used" atDepth:INTERFACE_LEVEL+2];
				
				switch ( pUncompressedFormatDesc->bmInterlaceFlags & 0x30 )
			{
				case 0: [thisDevice addProperty:"Field Pattern" withValue:"Field 1 only" atDepth:INTERFACE_LEVEL+2]; break;
				case 1: [thisDevice addProperty:"Field Pattern" withValue:"Field 2 only" atDepth:INTERFACE_LEVEL+2]; break;
				case 2: [thisDevice addProperty:"Field Pattern" withValue:"Regular pattern of fields 1 and 2" atDepth:INTERFACE_LEVEL+2]; break;
				case 3: [thisDevice addProperty:"Field Pattern" withValue:"Random pattern of fields 1 and 2" atDepth:INTERFACE_LEVEL+2]; break;
			}
				
				switch ( pUncompressedFormatDesc->bmInterlaceFlags & 0xc0 )
			{
				case 0: [thisDevice addProperty:"Display Mode" withValue:"Bob only" atDepth:INTERFACE_LEVEL+2]; break;
				case 1: [thisDevice addProperty:"Display Mode" withValue:"Weave only" atDepth:INTERFACE_LEVEL+2]; break;
				case 2: [thisDevice addProperty:"Display Mode" withValue:"Bob or Weave" atDepth:INTERFACE_LEVEL+2]; break;
				case 3: [thisDevice addProperty:"Display Mode" withValue:"Undefined" atDepth:INTERFACE_LEVEL+2]; break;
			}
				
				sprintf((char *)buf, "0x%x", pUncompressedFormatDesc->bCopyProtect);
				if (pUncompressedFormatDesc->bCopyProtect)
					[thisDevice addProperty:"Copy Protection:" withValue:"Restrict Duplication" atDepth:INTERFACE_LEVEL+1];
				else
					[thisDevice addProperty:"Copy Protection:" withValue:"No Restrictions" atDepth:INTERFACE_LEVEL+1];
				break;
				
			case VS_FRAME_UNCOMPRESSED:
				pUncompressedFrameDesc = (IOUSBVDC_UncompressedFrameDescriptor *)desc;
				
				sprintf((char *)buf, "0x%x", pUncompressedFrameDesc->bFrameIndex);
				[thisDevice addProperty:"bFrameIndex:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "(0x%x)", pUncompressedFrameDesc->bmCapabilities );
				[thisDevice addProperty:"bmCapabilities " withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				if (pUncompressedFrameDesc->bmCapabilities & 0x01)
					[thisDevice addProperty:"" withValue:"Still Image supported" atDepth:INTERFACE_LEVEL+2];
				
				if ( pUncompressedFrameDesc->bmCapabilities & 0xfe)
					[thisDevice addProperty:"" withValue:"Unknown capabilities" atDepth:INTERFACE_LEVEL+2];
				
				sprintf((char *)buf, "0x%x (%u)", Swap16(&pUncompressedFrameDesc->wWidth), pUncompressedFrameDesc->wWidth);
				[thisDevice addProperty:"wWidth:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x (%u)", Swap16(&pUncompressedFrameDesc->wHeight), pUncompressedFrameDesc->wHeight);
				[thisDevice addProperty:"wHeight:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x (%u)", Swap32(&pUncompressedFrameDesc->dwMinBitRate), pUncompressedFrameDesc->dwMinBitRate);
				[thisDevice addProperty:"dwMinBitRate (bps):" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x (%u)", Swap32(&pUncompressedFrameDesc->dwMaxBitRate), pUncompressedFrameDesc->dwMaxBitRate);
				[thisDevice addProperty:"dwMaxBitRate (bps):" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x (%u)", Swap32(&pUncompressedFrameDesc->dwMaxVideoFrameBufferSize), pUncompressedFrameDesc->dwMaxVideoFrameBufferSize);
				[thisDevice addProperty:"dwMaxVideoFrameBufferSize (bytes):" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x (%8.3f ms)", Swap32(&pUncompressedFrameDesc->dwDefaultFrameInterval), (double) (pUncompressedFrameDesc->dwDefaultFrameInterval / 10000));
				[thisDevice addProperty:"dwDefaultFrameInterval (100 ns):" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				if (pUncompressedFrameDesc->bFrameIntervalType == 0)
				{
					[thisDevice addProperty:"bFrameIntervalType:" withValue:"Continuous" atDepth:INTERFACE_LEVEL+1];
					
					sprintf((char *)buf, "0x%x (%8.3f ms)", Swap32(&pUncompressedFrameDesc->dwMinFrameInterval), (double ) (pUncompressedFrameDesc->dwMinFrameInterval / 10000) );
					[thisDevice addProperty:"dwMinFrameInterval (100 ns):" withValue:buf atDepth:INTERFACE_LEVEL+2];
					
					sprintf((char *)buf, "0x%x (%8.3f ms)", Swap32(&pUncompressedFrameDesc->dwMaxFrameInterval),  (double ) (pUncompressedFrameDesc->dwMaxFrameInterval / 10000) );
					[thisDevice addProperty:"dwMaxFrameInterval (100 ns):" withValue:buf atDepth:INTERFACE_LEVEL+2];
					
					sprintf((char *)buf, "0x%x (%8.3f ms)", Swap32(&pUncompressedFrameDesc->dwFrameIntervalStep),  (double ) (pUncompressedFrameDesc->dwFrameIntervalStep / 10000) );
					[thisDevice addProperty:"dwFrameIntervalStep (100 ns):" withValue:buf atDepth:INTERFACE_LEVEL+2];
					
				}
				else
				{
					// Need to recast as a IOUSBVDC_UncompressedDiscreteFrameDescriptor
					//
					pUncompressedDiscreteFrameDesc = (IOUSBVDC_UncompressedDiscreteFrameDescriptor *) pUncompressedFrameDesc;
					
					sprintf((char *)buf, "0x%x", (pUncompressedDiscreteFrameDesc->bFrameIntervalType));
					[thisDevice addProperty:"Discrete Frame Intervals supported" withValue:buf atDepth:INTERFACE_LEVEL+1];
					
					for (i = 0, t = &pUncompressedDiscreteFrameDesc->dwFrameInterval[0]; i < pUncompressedDiscreteFrameDesc->bFrameIntervalType; i++, t++ )
					{
						UInt32 interval = *t;
						sprintf((char *)buf, "0x%x (%8.3f ms)", Swap32(&interval), (double) (interval / 10000));
						sprintf((char *)buf2, "dwFrameInterval[%u] (100 ns)", i+1 );
						[thisDevice addProperty:buf2 withValue:buf atDepth:INTERFACE_LEVEL+2];
					}
				}
				break;
				
				
			case VS_STILL_IMAGE_FRAME:
				pVSStillImageFrameDesc = (IOUSBVDC_StillImageFrameDescriptor *)desc;
				
				sprintf((char *)buf, "0x%x", pVSStillImageFrameDesc->bEndpointAddress );
				[thisDevice addProperty:"bEndpointAddress:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "%d", pVSStillImageFrameDesc->bNumImageSizePatterns );
				[thisDevice addProperty:"bNumImageSizePatterns" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				IOSUBVDC_StillImageSize	*	dimensions;
				
				for (j = 0; j < pVSStillImageFrameDesc->bNumImageSizePatterns; j++ )
				{
					dimensions = (IOSUBVDC_StillImageSize *)&pVSStillImageFrameDesc->dwSize[j];
					
					sprintf((char *)buf, "Width: %d, Height: %d", OSSwapLittleToHostInt16( dimensions->wWidth), OSSwapLittleToHostInt16(dimensions->wHeight) );
					sprintf((char *)buf2, "wWidth[%d], wHeight[%d]", j+1, j+1 );
					
					[thisDevice addProperty:buf2 withValue:buf atDepth:INTERFACE_LEVEL+2];
				}
				
				IOSUBVDC_StillImageCompressionPattern *	pattern = (IOSUBVDC_StillImageCompressionPattern *) &pVSStillImageFrameDesc->dwSize[j];
				sprintf((char *)buf, "%d", pattern->bNumCompressionPattern );
				[thisDevice addProperty:"bCompressionPatterns" withValue:buf atDepth:INTERFACE_LEVEL+2];
				
				for (j = 0; j < pattern->bNumCompressionPattern; j++ )
				{
					sprintf((char *)buf, "%d", pattern->bCompression[j] );
					sprintf((char *)buf2, "bCompression[%d]", j+1);
					
					[thisDevice addProperty:buf2 withValue:buf atDepth:INTERFACE_LEVEL+3];
				}
				
				break;
				
			case VS_FORMAT_DV:
				pDVFormatDesc = (IOUSBVDC_DVFormatDescriptor *)desc;
				
				sprintf((char *)buf, "0x%x", pDVFormatDesc->bFormatIndex);
				[thisDevice addProperty:"bFormatIndex:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x (%u)", Swap32(&pDVFormatDesc->dwMaxVideoFrameBufferSize), pDVFormatDesc->dwMaxVideoFrameBufferSize);
				[thisDevice addProperty:"dwMaxVideoFrameBufferSize (bytes):" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				switch (pDVFormatDesc->bFormatType & 0x3)
			{
				case 0:  s = "SD-DV"; break;
				case 1:  s = "SDL-DV"; break;
				case 2:  s = "HD-DV"; break;
				default: s = "reserved"; break;
			}
				
				if (pDVFormatDesc->bFormatType & 0x80)
				{
					sprintf((char *)buf, "60 Hz %s", s);
				}
				else
				{
					sprintf((char *)buf, "5b0 Hz %s", s);
				}
				
				[thisDevice addProperty:"DV Format:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				break;

			case VS_FORMAT_FRAME_BASED:
				pFrameBasedFormatDesc = (IOUSBVDC_FrameBasedFormatDescriptor *)desc;
				
				sprintf((char *)buf, "0x%x", pFrameBasedFormatDesc->bFormatIndex);
				[thisDevice addProperty:"bFormatIndex:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x", pFrameBasedFormatDesc->bNumFrameDescriptors);
				[thisDevice addProperty:"bNumFrameDescriptors:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				// Decode the GUID per section 2.9 of the FAQ
				//
				data1 = USBToHostLong(* (uint32_t *) &pFrameBasedFormatDesc->guidFormat[0]);
				data2 = USBToHostWord(* (UInt16 *) &pFrameBasedFormatDesc->guidFormat[4]);
				data3 = USBToHostWord(* (UInt16 *) &pFrameBasedFormatDesc->guidFormat[6]);
				uuidLO = OSSwapBigToHostInt64(* (UInt64 *) &pFrameBasedFormatDesc->guidFormat[8]);
				
				
				sprintf((char *)buf, 	"%8.8x-%4.4x-%4.4x-%4.4x-%12.12qx", data1, data2, data3, 
						(uint32_t) ( (uuidLO & 0xffff000000000000ULL)>>48), (uuidLO & 0x0000FFFFFFFFFFFFULL) );
				[thisDevice addProperty:"Format GUID:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x (%u)", pFrameBasedFormatDesc->bBitsPerPixel,pFrameBasedFormatDesc->bBitsPerPixel);
				[thisDevice addProperty:"bBitsPerPixel:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x", pFrameBasedFormatDesc->bDefaultFrameIndex);
				[thisDevice addProperty:"bDefaultFrameIndex:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x", pFrameBasedFormatDesc->bAspectRatioX);
				[thisDevice addProperty:"bAspectRatioX:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x", pFrameBasedFormatDesc->bAspectRatioY);
				[thisDevice addProperty:"bAspectRatioY:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "(0x%x)", pFrameBasedFormatDesc->bmInterlaceFlags );
				[thisDevice addProperty:"bmInterlaceFlags" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				if (pFrameBasedFormatDesc->bmInterlaceFlags & 0x01)
					[thisDevice addProperty:"Interlace Stream or variable" withValue:"YES" atDepth:INTERFACE_LEVEL+2];
				else
					[thisDevice addProperty:"Interlace Stream or variable" withValue:"NO" atDepth:INTERFACE_LEVEL+2];
				
				if (pFrameBasedFormatDesc->bmInterlaceFlags & 0x02)
					[thisDevice addProperty:"Fields per Frame" withValue:"1" atDepth:INTERFACE_LEVEL+2];
				else
					[thisDevice addProperty:"Fields per Frame" withValue:"2" atDepth:INTERFACE_LEVEL+2];
				
				if (pFrameBasedFormatDesc->bmInterlaceFlags & 0x04)
					[thisDevice addProperty:"Field 1 First" withValue:"Yes" atDepth:INTERFACE_LEVEL+2];
				else
					[thisDevice addProperty:"Field 1 First" withValue:"No" atDepth:INTERFACE_LEVEL+2];
				
				if (pFrameBasedFormatDesc->bmInterlaceFlags & 0x08)
					[thisDevice addProperty:"" withValue:"Reserved field used" atDepth:INTERFACE_LEVEL+2];
				

				switch ( pFrameBasedFormatDesc->bmInterlaceFlags & 0x30 )
			{
				case 0: [thisDevice addProperty:"Field Pattern" withValue:"Field 1 only" atDepth:INTERFACE_LEVEL+2]; break;
				case 1: [thisDevice addProperty:"Field Pattern" withValue:"Field 2 only" atDepth:INTERFACE_LEVEL+2]; break;
				case 2: [thisDevice addProperty:"Field Pattern" withValue:"Regular pattern of fields 1 and 2" atDepth:INTERFACE_LEVEL+2]; break;
				case 3: [thisDevice addProperty:"Field Pattern" withValue:"Random pattern of fields 1 and 2" atDepth:INTERFACE_LEVEL+2]; break;
			}
				
				sprintf((char *)buf, "0x%x", pFrameBasedFormatDesc->bCopyProtect);
				if (pFrameBasedFormatDesc->bCopyProtect)
					[thisDevice addProperty:"Copy Protection:" withValue:"Restrict Duplication" atDepth:INTERFACE_LEVEL+1];
				else
					[thisDevice addProperty:"Copy Protection:" withValue:"No Restrictions" atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x", pFrameBasedFormatDesc->bVariableSize);
				[thisDevice addProperty:"bVariableSize:" withValue:buf atDepth:INTERFACE_LEVEL+1];

				break;
				
			case VS_FRAME_FRAME_BASED:
				pFrameBasedFrameDesc = (IOUSBVDC_FrameBasedFrameDescriptor *)desc;
				
				sprintf((char *)buf, "0x%x", pFrameBasedFrameDesc->bFrameIndex);
				[thisDevice addProperty:"bFrameIndex:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "(0x%x)", pFrameBasedFrameDesc->bmCapabilities );
				[thisDevice addProperty:"bmCapabilities " withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				if (pFrameBasedFrameDesc->bmCapabilities & 0x01)
					[thisDevice addProperty:"" withValue:"Still Image supported" atDepth:INTERFACE_LEVEL+2];
				
				if (pFrameBasedFrameDesc->bmCapabilities & 0x02)
					[thisDevice addProperty:"" withValue:"Fixed Frame Rate" atDepth:INTERFACE_LEVEL+2];
				
				if ( pFrameBasedFrameDesc->bmCapabilities & 0xfe)
					[thisDevice addProperty:"" withValue:"Unknown capabilities" atDepth:INTERFACE_LEVEL+2];
				
				sprintf((char *)buf, "0x%x (%u)", Swap16(&pFrameBasedFrameDesc->wWidth), pFrameBasedFrameDesc->wWidth);
				[thisDevice addProperty:"wWidth:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x (%u)", Swap16(&pFrameBasedFrameDesc->wHeight), pFrameBasedFrameDesc->wHeight);
				[thisDevice addProperty:"wHeight:" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x (%u)", Swap32(&pFrameBasedFrameDesc->dwMinBitRate), pFrameBasedFrameDesc->dwMinBitRate);
				[thisDevice addProperty:"dwMinBitRate (bps):" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x (%u)", Swap32(&pFrameBasedFrameDesc->dwMaxBitRate), pFrameBasedFrameDesc->dwMaxBitRate);
				[thisDevice addProperty:"dwMaxBitRate (bps):" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x (%8.3f ms)", Swap32(&pFrameBasedFrameDesc->dwDefaultFrameInterval), (double) (pFrameBasedFrameDesc->dwDefaultFrameInterval / 10000));
				[thisDevice addProperty:"dwDefaultFrameInterval (100 ns):" withValue:buf atDepth:INTERFACE_LEVEL+1];
				
				sprintf((char *)buf, "0x%x (%u)", Swap32(&pFrameBasedFrameDesc->dwBytesPerLine), pFrameBasedFrameDesc->dwBytesPerLine);
				[thisDevice addProperty:"dwBytesPerLine:" withValue:buf atDepth:INTERFACE_LEVEL+1];

				if (pFrameBasedFrameDesc->bFrameIntervalType == 0)
				{
					[thisDevice addProperty:"bFrameIntervalType:" withValue:"Continuous" atDepth:INTERFACE_LEVEL+1];
					
					sprintf((char *)buf, "0x%x (%8.3f ms)", Swap32(&pFrameBasedFrameDesc->dwMinFrameInterval), (double ) (pFrameBasedFrameDesc->dwMinFrameInterval / 10000) );
					[thisDevice addProperty:"dwMinFrameInterval (100 ns):" withValue:buf atDepth:INTERFACE_LEVEL+2];
					
					sprintf((char *)buf, "0x%x (%8.3f ms)", Swap32(&pFrameBasedFrameDesc->dwMaxFrameInterval),  (double ) (pFrameBasedFrameDesc->dwMaxFrameInterval / 10000) );
					[thisDevice addProperty:"dwMaxFrameInterval (100 ns):" withValue:buf atDepth:INTERFACE_LEVEL+2];
					
					sprintf((char *)buf, "0x%x (%8.3f ms)", Swap32(&pFrameBasedFrameDesc->dwFrameIntervalStep),  (double ) (pFrameBasedFrameDesc->dwFrameIntervalStep / 10000) );
					[thisDevice addProperty:"dwFrameIntervalStep (100 ns):" withValue:buf atDepth:INTERFACE_LEVEL+2];
					
				}
				else
				{
					// Need to recast as a IOUSBVDC_DiscreteFrameBasedFrameDescriptor
					//
					pFrameBasedDiscreteFrameDesc = (IOUSBVDC_DiscreteFrameBasedFrameDescriptor *) pFrameBasedFrameDesc;
					
					sprintf((char *)buf, "0x%x", (pFrameBasedDiscreteFrameDesc->bFrameIntervalType));
					[thisDevice addProperty:"Discrete Frame Intervals supported" withValue:buf atDepth:INTERFACE_LEVEL+1];
					
					for (i = 0, t = &pFrameBasedDiscreteFrameDesc->dwFrameInterval[0]; i < pFrameBasedDiscreteFrameDesc->bFrameIntervalType; i++, t++ )
					{
						UInt32 interval = *t;
						sprintf((char *)buf, "0x%x (%8.3f ms)", Swap32(&interval), (double) (interval / 10000));
						sprintf((char *)buf2, "dwFrameInterval[%u] (100 ns)", i+1 );
						[thisDevice addProperty:buf2 withValue:buf atDepth:INTERFACE_LEVEL+2];
					}
				}

				break;
				
				
				// default:
		}
	}       
}

char MapNumberToVersion( int i )
{
    char rev;
    
    switch (i)
    {
        case 1:
            rev = 'a';
            break;
        case 2:
            rev = 'b';
            break;
        case 3:
            rev = 'c';
            break;
        case 4:
            rev = 'd';
            break;
        case 5:
            rev = 'e';
            break;
        case 6:
            rev = 'f';
            break;
        case 7:
            rev = 'g';
            break;
        case 8:
            rev = 'h';
            break;
        case 9:
            rev = 'i';
            break;
        default:
            rev = 'a';
    }
    return rev;
}
@end
