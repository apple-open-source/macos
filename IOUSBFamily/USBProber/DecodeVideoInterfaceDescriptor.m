/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
//
//  DecodeVideoInterfaceDescriptor.m
//  IOUSBFamily
//
//  Created by Fernando Urbina on Tue Apr 15 2003.
//  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
//

#import "DecodeVideoInterfaceDescriptor.h"


@implementation DecodeVideoInterfaceDescriptor

+(void)DecodeVideoInterfaceDescriptor:(UInt8 *)descriptor deviceIntf:(IOUSBDeviceInterface **)deviceIntf forDevice:(UInt16)deviceNumber atDepth:(UInt16)depth forNode:(Node *)node subClass:(UInt8)interfaceSubClass;
{

    static unsigned char			buf[256];
    static unsigned char			buf2[256];
    auto IOUSBVCInterfaceDescriptor *		pVideoControlHeader;
    auto IOUSBVCInputTerminalDescriptor *	pVideoInTermDesc;
    auto IOUSBVCCameraTerminalDescriptor *	pCameraTermDesc;
    auto IOUSBVCOutputTerminalDescriptor *	pVideoOutTermDesc;
    auto IOUSBVCSelectorUnitDescriptor *	pSelectorUnitDesc = NULL;
    auto IOUSBVCSelectorUnit2Descriptor *	pSelectorUnit2Desc;
    auto IOUSBVCProcessingUnitDescriptor *	pProcessingUnitDesc;
    auto IOUSBVCProcessingUnit2Descriptor *	pProcessingUnit2Desc;
    auto IOUSBVCExtensionUnitDescriptor *	pExtensionUnitDesc;
    auto IOUSBVCExtensionUnit2Descriptor *	pExtensionUnit2Desc;
    auto IOUSBVCExtensionUnit3Descriptor *	pExtensionUnit3Desc;
    auto IOUSBVCInterruptEndpointDescriptor *	pVCInterruptEndpintDesc;
    auto IOUSBVSInputHeaderDescriptor *		pVSInputHeaderDesc;
    auto IOUSBVSOutputHeaderDescriptor *	pVSOutputHeaderDesc;
    auto IOUSBVDC_MJPEGFormatDescriptor *	pMJPEGFormatDesc;
    auto IOUSBVDC_MJPEGFrameDescriptor *	pMJPEGFrameDesc;
    auto IOUSBVDC_MJPEGDiscreteFrameDescriptor *	pMJPEGDiscreteFrameDesc;

    UInt16					i;
    UInt8					*p;
    UInt32					*t;
    char					*s = NULL;
    GenericAudioDescriptorPtr			desc = (GenericAudioDescriptorPtr) descriptor;
    UInt64					uuidHI;
    UInt64					uuidLO;
    char					str[256];

    if ( ((GenericAudioDescriptorPtr)desc)->descType == CS_ENDPOINT )
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
        [self PrintKeyVal:buf val:"" forDevice:deviceNumber atDepth:depth forNode:node];

        // Print the Length and contents of this class-specific descriptor
        //
        sprintf(str, "%u", pVCInterruptEndpintDesc->bLength);
        [self PrintKeyVal:"Length (and contents):" val:str forDevice:deviceNumber atDepth:depth+1 forNode:node];
        [BusProbeClass DumpRawDescriptor:(Byte*)desc forDevice:deviceNumber atDepth:depth+2];

        sprintf((char *)buf, "%u", Swap16(&pVCInterruptEndpintDesc->wMaxTransferSize) );
        [self PrintKeyVal:"Max Transfer Size:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];
        return;
    }
    
    if ( ((GenericAudioDescriptorPtr)desc)->descType != CS_INTERFACE )
        return;

    if( SC_VIDEOCONTROL == interfaceSubClass )
    {
        switch ( ((GenericAudioDescriptorPtr)desc)->descSubType )
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
    else if( SC_VIDEOSTREAMING == interfaceSubClass )
    {
        switch ( ((GenericAudioDescriptorPtr)desc)->descSubType )
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
            default:
                sprintf((char *)buf, "Uknown SC_VIDEOSTREAMING SubType Descriptor");
        }
    }
    else
        sprintf((char *)buf, "Unknown VDC Interface Subclass Type");

    [self PrintKeyVal:buf val:"" forDevice:deviceNumber atDepth:depth forNode:node];

    // Print the Length and contents of this class-specific descriptor
    //
    sprintf(str, "%u", ((GenericAudioDescriptorPtr)desc)->descLen);
    [self PrintKeyVal:"Length (and contents):" val:str forDevice:deviceNumber atDepth:depth+1 forNode:node];
    [BusProbeClass DumpRawDescriptor:(Byte*)desc forDevice:deviceNumber atDepth:depth+2];

    
    if( SC_VIDEOCONTROL == interfaceSubClass ) // Video Control Subclass
    {
        switch ( ((GenericAudioDescriptorPtr)desc)->descSubType )
        {
            case VC_HEADER:
                pVideoControlHeader = (IOUSBVCInterfaceDescriptor *)desc;
                i = Swap16(&pVideoControlHeader->bcdVDC);
                sprintf((char *)buf, "%1x%1x.%1x%1c",
                        (i>>12)&0x000f, (i>>8)&0x000f, (i>>4)&0x000f, MapNumberToVersion((i>>0)&0x000f));
                [self PrintKeyVal:"Specification Version Number:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                sprintf((char *)buf, "%u", Swap16(&pVideoControlHeader->wTotalLength) );

                sprintf((char *)buf, "%lu", Swap32(&pVideoControlHeader->dwClockFrequency) );
                [self PrintKeyVal:"Device Clock Frequency (Hz):" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                sprintf((char *)buf, "%u", pVideoControlHeader->bInCollection );
                [self PrintKeyVal:"Number of Video Streaming Interfaces:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                // Haven't seen this array filled with more than 1 yet.
                //
                for (i = 0, p = &pVideoControlHeader->baInterfaceNr[0]; i < pVideoControlHeader->bInCollection; i++, p++ )
                {
                    sprintf((char *)buf, "%u", *p );
                    [self PrintKeyVal:"Video Interface Number:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];
                }
                break;

            case VC_INPUT_TERMINAL:
                pVideoInTermDesc = (IOUSBVCInputTerminalDescriptor *)desc;

                sprintf((char *)buf, "%u", pVideoInTermDesc->bTerminalID );
                [self PrintKeyVal:"Terminal ID" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

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
                [self PrintKeyVal:"Input Terminal Type:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                if( !pVideoInTermDesc->bAssocTerminal )
                    sprintf((char *)buf, "%u [NONE]", pVideoInTermDesc->bAssocTerminal );
                else
                    sprintf((char *)buf, "%u", pVideoInTermDesc->bAssocTerminal );

                [self PrintKeyVal:"Input Terminal ID:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];


                if( !pVideoInTermDesc->iTerminal )
                {
                    sprintf((char *)buf, "%u [NONE]", pVideoInTermDesc->iTerminal );
                    [self PrintKeyVal:"Input Terminal String Index:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];
                }
                    else
                    {
                        [BusProbeClass PrintStr:deviceIntf name:"Input Terminal String:" strIndex:(UInt8)pVideoInTermDesc->iTerminal forDevice:deviceNumber atDepth:depth+1];
                    }
                    
                if ( ITT_CAMERA == pVideoInTermDesc->wTerminalType )
                {
                    pCameraTermDesc = (IOUSBVCCameraTerminalDescriptor *)desc;

                    sprintf((char *)buf, "%u", Swap16(&pCameraTermDesc->wObjectiveFocalLengthMin) );
                    [self PrintKeyVal:"Minimum Focal Length" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                    sprintf((char *)buf, "%u", Swap16(&pCameraTermDesc->wObjectiveFocalLengthMax) );
                    [self PrintKeyVal:"Maximum Focal Length" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                    sprintf((char *)buf, "%u", Swap16(&pCameraTermDesc->wOcularFocalLength) );
                    [self PrintKeyVal:"Ocular Focal Length" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                    if ( pCameraTermDesc->bControlSize != 0 )
                    {
                        sprintf((char *)buf, "Description");
                        [self PrintKeyVal:"Controls Supported" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];
                    }

                    strcpy((char *)buf, "");
                    for (i = 0, p = &pCameraTermDesc->bmControls[0]; i < pCameraTermDesc->bControlSize; i++, p++ )
                    {
                        // For 0.8, only 18 bits are defined:
                        //
                        if ( i > 2 )
                        {
                            sprintf((char *)buf, "Unknown");
                            [self PrintKeyVal:"" val:buf forDevice:deviceNumber atDepth:depth+2 forNode:node];
                            break;
                        }

                        if ( (*p) & (1 << 0) )
                        {
                            if ( i == 0 ) 	sprintf((char *)buf, "Scanning Mode");
                            else if ( i == 1 ) 	sprintf((char *)buf, "Iris (Relative)");
                            else if ( i == 2 ) 	sprintf((char *)buf, "Iris (Relative)");
                            if ( strcmp(buf,"") )
                                [self PrintKeyVal:"" val:buf forDevice:deviceNumber atDepth:depth+2 forNode:node];
                        }

                        if ( (*p) & (1 << 1) )
                        {
                            if ( i == 0 ) 	sprintf((char *)buf, "Auto Exposure Mode");
                            else if ( i == 1 ) 	sprintf((char *)buf, "Zoom (Absolute)");
                            else if ( i == 2 ) 	sprintf((char *)buf, "Tilt (Relative)");
                            if ( strcmp(buf,"") )
                                [self PrintKeyVal:"" val:buf forDevice:deviceNumber atDepth:depth+2 forNode:node];
                        }

                        if ( (*p) & (1 << 2) )
                        {
                            if ( i == 0 ) 	sprintf((char *)buf, "Auto Exposure Priority");
                            else if ( i == 1 ) 	sprintf((char *)buf, "Zoom (Relative)");
                            else if ( i == 2 ) 	sprintf((char *)buf, "Focus, Auto");
                            if ( strcmp(buf,"") )
                                [self PrintKeyVal:"" val:buf forDevice:deviceNumber atDepth:depth+2 forNode:node];
                        }

                        if ( (*p) & (1 << 3) )
                        {
                            if ( i == 0 ) 	sprintf((char *)buf, "Exposure Time (Absolute)");
                            else if ( i == 1 ) 	sprintf((char *)buf, "Pan (Absolute)");
                            else if ( i == 2 ) 	sprintf((char *)buf, "Unknown");
                            if ( strcmp(buf,"") )
                                [self PrintKeyVal:"" val:buf forDevice:deviceNumber atDepth:depth+2 forNode:node];
                        }

                        if ( (*p) & (1 << 4) )
                        {
                            if ( i == 0 ) 	sprintf((char *)buf, "Exposure Time (Relative)");
                            else if ( i == 1 ) 	sprintf((char *)buf, "Pan (Relative)");
                            else if ( i == 2 ) 	sprintf((char *)buf, "Unknown");
                            if ( strcmp(buf,"") )
                                [self PrintKeyVal:"" val:buf forDevice:deviceNumber atDepth:depth+2 forNode:node];
                        }

                        if ( (*p) & (1 << 5) )
                        {
                            if ( i == 0 ) 	sprintf((char *)buf, "Focus (Absolute)");
                            else if ( i == 1 ) 	sprintf((char *)buf, "Roll (Absolute)");
                            else if ( i == 2 ) 	sprintf((char *)buf, "Unknown");
                            if ( strcmp(buf,"") )
                                [self PrintKeyVal:"" val:buf forDevice:deviceNumber atDepth:depth+2 forNode:node];
                        }

                        if ( (*p) & (1 << 6) )
                        {
                            if ( i == 0 ) 	sprintf((char *)buf, "Focus (Relative)");
                            else if ( i == 1 ) 	sprintf((char *)buf, "Roll (Relative)");
                            else if ( i == 2 ) 	sprintf((char *)buf, "Unknown");
                            if ( strcmp(buf,"") )
                                [self PrintKeyVal:"" val:buf forDevice:deviceNumber atDepth:depth+2 forNode:node];
                        }

                        if ( (*p) & (1 << 7) )
                        {
                            if ( i == 0 ) 	sprintf((char *)buf, "Iris (Absolute)");
                            else if ( i == 1 ) 	sprintf((char *)buf, "Tilt (Absolute)");
                            else if ( i == 2 ) 	sprintf((char *)buf, "Unknown");
                            if ( strcmp(buf,"") )
                                [self PrintKeyVal:"" val:buf forDevice:deviceNumber atDepth:depth+2 forNode:node];
                        }

                    }
                }
                break;

            case VC_OUTPUT_TERMINAL:
                pVideoOutTermDesc = (IOUSBVCOutputTerminalDescriptor *)desc;

                sprintf((char *)buf, "%u", pVideoOutTermDesc->bTerminalID );
                [self PrintKeyVal:"Terminal ID:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

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
                [self PrintKeyVal:"Output Terminal Type:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                if( !pVideoOutTermDesc->bAssocTerminal )
                    sprintf((char *)buf, "%u [NONE]", pVideoOutTermDesc->bAssocTerminal );
                else
                    sprintf((char *)buf, "%u", pVideoOutTermDesc->bAssocTerminal );

                [self PrintKeyVal:"Output Terminal ID:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];


                if( !pVideoOutTermDesc->iTerminal )
                {
                    sprintf((char *)buf, "%u [NONE]", pVideoOutTermDesc->iTerminal );
                    [self PrintKeyVal:"Output Terminal String Index:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];
                }
                    else
                    {
                        [BusProbeClass PrintStr:deviceIntf name:"Output Terminal String:" strIndex:(UInt8)pVideoOutTermDesc->iTerminal forDevice:deviceNumber atDepth:depth+1];
                    }
                break;

            case VC_SELECTOR_UNIT:
                pSelectorUnitDesc = (IOUSBVCSelectorUnitDescriptor *)desc;
                sprintf((char *)buf, 	"%u", pSelectorUnitDesc->bUnitID );
                [self PrintKeyVal:"Unit ID:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                sprintf((char *)buf, 	"%u", pSelectorUnitDesc->bNrInPins );
                [self PrintKeyVal:"Number of pins:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                //
                for (i = 0, p = &pSelectorUnitDesc->baSourceID[0]; i < pSelectorUnitDesc->bNrInPins; i++, p++ )
                {
                    sprintf((char *)buf2,	"Source ID Pin[%d]:", i);
                    sprintf((char *)buf, "%u", *p );
                    [self PrintKeyVal:buf2 val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];
                }

                // Now, p will point to the IOUSBVCSelectorUnit2Descriptor
                //
                pSelectorUnit2Desc = (IOUSBVCSelectorUnit2Descriptor *) p;
                if( !pSelectorUnit2Desc->iSelector )
                {
                    sprintf((char *)buf, "%u [NONE]", pSelectorUnit2Desc->iSelector );
                    [self PrintKeyVal:"Selector Unit String Index:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];
                }
                else
                {
                    [BusProbeClass PrintStr:deviceIntf name:"Selector Unit String:" strIndex:(UInt8)pSelectorUnit2Desc->iSelector forDevice:deviceNumber atDepth:depth+1];
                }
                    
                break;

            case VC_PROCESSING_UNIT:
                pProcessingUnitDesc = ( IOUSBVCProcessingUnitDescriptor *) desc;
                sprintf((char *)buf, 	"%u", pProcessingUnitDesc->bUnitID );
                [self PrintKeyVal:"Unit ID:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                sprintf((char *)buf, 	"%u", pProcessingUnitDesc->bSourceID );
                [self PrintKeyVal:"Source ID:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                sprintf((char *)buf, 	"%u", Swap16(&pProcessingUnitDesc->wMaxMultiplier) );
                [self PrintKeyVal:"Digital Multiplier (100X):" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                if ( pProcessingUnitDesc->bControlSize != 0 )
                {
                    sprintf((char *)buf, "Description");
                    [self PrintKeyVal:"Controls Supported" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];
                }

                strcpy((char *)buf, "");
                for (i = 0, p = &pProcessingUnitDesc->bmControls[0]; i < pProcessingUnitDesc->bControlSize; i++, p++ )
                {
                    // For 0.8, only 16 bits are defined:
                    //
                    if ( i > 1 )
                    {
                        sprintf((char *)buf, "Unknown");
                        [self PrintKeyVal:"" val:buf forDevice:deviceNumber atDepth:depth+2 forNode:node];
                        break;
                    }

                    if ( (*p) & (1 << 0) )
                    {
                        if ( i == 0 ) 		sprintf((char *)buf, "Brightness");
                        else if ( i == 1 ) 	sprintf((char *)buf, "Backlight Compensation");
                        if ( strcmp(buf,"") )
                            [self PrintKeyVal:"" val:buf forDevice:deviceNumber atDepth:depth+2 forNode:node];
                    }

                    if ( (*p) & (1 << 1) )
                    {
                        if ( i == 0 ) 		sprintf((char *)buf, "Contrast");
                        else if ( i == 1 ) 	sprintf((char *)buf, "Gain");
                        if ( strcmp(buf,"") )
                            [self PrintKeyVal:"" val:buf forDevice:deviceNumber atDepth:depth+2 forNode:node];
                    }

                    if ( (*p) & (1 << 2) )
                    {
                        if ( i == 0 ) 		sprintf((char *)buf, "Hue");
                        else if ( i == 1 ) 	sprintf((char *)buf, "Power Line Frequency");
                        if ( strcmp(buf,"") )
                            [self PrintKeyVal:"" val:buf forDevice:deviceNumber atDepth:depth+2 forNode:node];
                    }

                    if ( (*p) & (1 << 3) )
                    {
                        if ( i == 0 ) 		sprintf((char *)buf, "Saturation");
                        else if ( i == 1 ) 	sprintf((char *)buf, "Hue, Auto");
                        if ( strcmp(buf,"") )
                            [self PrintKeyVal:"" val:buf forDevice:deviceNumber atDepth:depth+2 forNode:node];
                    }

                    if ( (*p) & (1 << 4) )
                    {
                        if ( i == 0 ) 		sprintf((char *)buf, "Sharpness");
                        else if ( i == 1 ) 	sprintf((char *)buf, "White Balance Temperature, Auto");
                        if ( strcmp(buf,"") )
                            [self PrintKeyVal:"" val:buf forDevice:deviceNumber atDepth:depth+2 forNode:node];
                    }

                    if ( (*p) & (1 << 5) )
                    {
                        if ( i == 0 ) 		sprintf((char *)buf, "Gamma");
                        else if ( i == 1 ) 	sprintf((char *)buf, "White Balance Component, Auto");
                        if ( strcmp(buf,"") )
                            [self PrintKeyVal:"" val:buf forDevice:deviceNumber atDepth:depth+2 forNode:node];
                    }

                    if ( (*p) & (1 << 6) )
                    {
                        if ( i == 0 ) 		sprintf((char *)buf, "White Balance Temperature");
                        else if ( i == 1 ) 	sprintf((char *)buf, "Digital Multiplier");
                        if ( strcmp(buf,"") )
                            [self PrintKeyVal:"" val:buf forDevice:deviceNumber atDepth:depth+2 forNode:node];
                    }

                    if ( (*p) & (1 << 7) )
                    {
                        if ( i == 0 ) 		sprintf((char *)buf, "White Balance Component");
                        else if ( i == 1 ) 	sprintf((char *)buf, "Digital Multiplier Limit");
                        if ( strcmp(buf,"") )
                            [self PrintKeyVal:"" val:buf forDevice:deviceNumber atDepth:depth+2 forNode:node];
                    }

                }

                // At this point, p should be pointing to the iProcessing field:
                pProcessingUnit2Desc = (IOUSBVCProcessingUnit2Descriptor *) p;
                if( !pProcessingUnit2Desc->iProcessing )
                {
                    sprintf((char *)buf, "%u [NONE]", pProcessingUnit2Desc->iProcessing );
                    [self PrintKeyVal:"Processing Unit String Index:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];
                }
                    else
                    {
                        [BusProbeClass PrintStr:deviceIntf name:"Selector Unit String:" strIndex:(UInt8)pProcessingUnit2Desc->iProcessing forDevice:deviceNumber atDepth:depth+1];
                    }
                    break;

            case VC_EXTENSION_UNIT:
                pExtensionUnitDesc = (IOUSBVCExtensionUnitDescriptor *)desc;

                sprintf((char *)buf, 	"%u", pExtensionUnitDesc->bUnitID );
                [self PrintKeyVal:"Unit ID:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                uuidHI = Swap64(&pExtensionUnitDesc->guidExtensionCodeHi);
                uuidLO = Swap64(&pExtensionUnitDesc->guidExtensionCodeLo);
                
                sprintf((char *)buf, 	"%8.8lx-%4.4lx-%4.4lx-%4.4lx%12.12qx", (UInt32) (uuidHI>>32), (UInt32) ( (uuidHI & 0xffff0000)>>16), (UInt32)(uuidHI & 0x0000ffff), (UInt32) ( (uuidLO & 0xffff000000000000ULL)>>48), (uuidLO & 0x0000FFFFFFFFFFFFULL) );
                [self PrintKeyVal:"Vendor UUID:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                sprintf((char *)buf, 	"%u", pExtensionUnitDesc->bNumControls );
                [self PrintKeyVal:"Number of Controls:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];


                sprintf((char *)buf, 	"%u", pExtensionUnitDesc->bNrInPins );
                [self PrintKeyVal:"Number of In pins:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                for (i = 0, p = &pExtensionUnitDesc->baSourceID[0]; i < pSelectorUnitDesc->bNrInPins; i++, p++ )
                {
                    sprintf((char *)buf2,	"Source ID Pin[%d]:", i);
                    sprintf((char *)buf, "%u", *p );
                    [self PrintKeyVal:buf2 val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];
                }

                // Now, p points to the rest of the Extension Unit descriptor:
                //
                pExtensionUnit2Desc = ( IOUSBVCExtensionUnit2Descriptor *) p;

                if ( pExtensionUnit2Desc->bControlSize != 0 )
                {
                    sprintf((char *)buf, "Description");
                    [self PrintKeyVal:"Controls Supported" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];
                }

                strcpy((char *)buf, "");
                for (i = 0, p = &pExtensionUnit2Desc->bmControls[0]; i < pExtensionUnit2Desc->bControlSize; i++, p++ )
                {
                    // For 0.8, only 1 bits is defined:
                    //
                    if ( i == 0 )
                    {
                        if ( (*p) & 0x01)
                        {
                            sprintf((char *)buf, "Enable Processing");
                            [self PrintKeyVal:"" val:buf forDevice:deviceNumber atDepth:depth+2 forNode:node];
                        }

                        if ( (*p) & 0xfe)
                        {
                            sprintf((char *)buf, "Using Reserved Bit!");
                            [self PrintKeyVal:"" val:buf forDevice:deviceNumber atDepth:depth+2 forNode:node];
                        }
                    }
                    else
                    if ( i > 1 )
                    {
                        sprintf((char *)buf, "Vendor Specific 0x%x", (*p));
                        [self PrintKeyVal:"" val:buf forDevice:deviceNumber atDepth:depth+2 forNode:node];
                        break;
                    }
                }

                // At this point, p should be pointing to the iProcessing field:
                pExtensionUnit3Desc = ( IOUSBVCExtensionUnit3Descriptor *) p;
                if( !pExtensionUnit3Desc->iExtension )
                {
                    sprintf((char *)buf, "%u [NONE]", pExtensionUnit3Desc->iExtension );
                    [self PrintKeyVal:"Processing Unit String Index:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];
                }
                    else
                    {
                        [BusProbeClass PrintStr:deviceIntf name:"Selector Unit String:" strIndex:(UInt8)pExtensionUnit3Desc->iExtension forDevice:deviceNumber atDepth:depth+1];
                    }
                    break;
            case VC_DESCRIPTOR_UNDEFINED:
            default:
                [self PrintKeyVal:"Undefined Descriptor:" val:"" forDevice:deviceNumber atDepth:depth+1 forNode:node];
    

        }
    }
    else if( SC_VIDEOSTREAMING == interfaceSubClass ) // Video Streaming Subclass
    {
        switch ( ((GenericAudioDescriptorPtr)desc)->descSubType )
                {
                    case VS_INPUT_HEADER:
                        pVSInputHeaderDesc = (IOUSBVSInputHeaderDescriptor *)desc;

                        sprintf((char *)buf, "%u", pVSInputHeaderDesc->bNumFormats );
                        [self PrintKeyVal:"Number of Formats:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                        sprintf((char *)buf, "%u", Swap16(&pVSInputHeaderDesc->wTotalLength) );
                        [self PrintKeyVal:"Total Length of Descriptor:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                        sprintf((char *)buf, "0x%x", pVSInputHeaderDesc->bEndpointAddress );
                        [self PrintKeyVal:"Endpoint Address:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                        sprintf((char *)buf, "Capabilities (0x%x)", pVSInputHeaderDesc->bmInfo );
                        [self PrintKeyVal:buf val:"Description" forDevice:deviceNumber atDepth:depth+1 forNode:node];

                        if (pVSInputHeaderDesc->bmInfo & 0x01)
                            [self PrintKeyVal:"" val:"Dynamic Format Change supported" forDevice:deviceNumber atDepth:depth+2 forNode:node];

                        if ( pVSInputHeaderDesc->bmInfo & 0xfe)
                            [self PrintKeyVal:"" val:"Unknown capabilities" forDevice:deviceNumber atDepth:depth+2 forNode:node];

                        sprintf((char *)buf, "%u", pVSInputHeaderDesc->bTerminalLink );
                        [self PrintKeyVal:"Terminal ID of Output Terminal:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                        switch (pVSInputHeaderDesc->bTerminalLink)
                        {
                            case 0: s = "None"; break;
                            case 1: s = "Method 1"; break;
                            case 2: s = "Method 2"; break;
                            case 3: s = "Method 3"; break;
                            default: s = "Unknown Method";
                        }
                        sprintf((char *)buf, 	"%u (%s)", pVSInputHeaderDesc->bTerminalLink, s );
                        [self PrintKeyVal:"Still Capture Method:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];
                        
                        break;
                    case VS_OUTPUT_HEADER:
                        pVSOutputHeaderDesc = (IOUSBVSOutputHeaderDescriptor *)desc;

                        sprintf((char *)buf, "%u", pVSOutputHeaderDesc->bNumFormats);
                        [self PrintKeyVal:"Number of Formats:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                        sprintf((char *)buf, "%u", Swap16(&pVSOutputHeaderDesc->wTotalLength) );
                        [self PrintKeyVal:"Total Length of Descriptor:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                        sprintf((char *)buf, "0x%x", pVSOutputHeaderDesc->bEndpointAddress );
                        [self PrintKeyVal:"Endpoint Address:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                        sprintf((char *)buf, "%u", pVSOutputHeaderDesc->bTerminalLink );
                        [self PrintKeyVal:"Terminal ID of Output Terminal:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                        break;
                    case VS_FORMAT_MJPEG:
                        pMJPEGFormatDesc = (IOUSBVDC_MJPEGFormatDescriptor *)desc;

                        sprintf((char *)buf, "%u", pMJPEGFormatDesc->bFormatIndex);
                        [self PrintKeyVal:"Format Index:" val:buf forDevice:deviceNumber atDepth:depth+2 forNode:node];

                        sprintf((char *)buf, "%u", pMJPEGFormatDesc->bNumFrameDescriptors);
                        [self PrintKeyVal:"Number of Frame Descriptors:" val:buf forDevice:deviceNumber atDepth:depth+2 forNode:node];

                        sprintf((char *)buf, "(0x%x)", pMJPEGFormatDesc->bmFlags );
                        [self PrintKeyVal:"Characteristics " val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                        if (pMJPEGFormatDesc->bmFlags & 0x01)
                            [self PrintKeyVal:"" val:"Fixed Sample Sizes Supported" forDevice:deviceNumber atDepth:depth+2 forNode:node];

                        if ( pMJPEGFormatDesc->bmFlags & 0xfe)
                            [self PrintKeyVal:"" val:"Unknown characteristics" forDevice:deviceNumber atDepth:depth+2 forNode:node];

                        //sprintf((char *)buf, "%u", pMJPEGFormatDesc->bBitsPerPixel);
                        //[self PrintKeyVal:"Bits per pixel in frame:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                        sprintf((char *)buf, "%u", pMJPEGFormatDesc->bDefaultFrameIndex);
                        [self PrintKeyVal:"Optimum Frame Index:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                        sprintf((char *)buf, "%u", pMJPEGFormatDesc->bAspectRatioX);
                        [self PrintKeyVal:"X Aspect Ratio:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                        sprintf((char *)buf, "%u", pMJPEGFormatDesc->bAspectRatioY);
                        [self PrintKeyVal:"Y Aspect Ratio:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                        sprintf((char *)buf, "(0x%x)", pMJPEGFormatDesc->bmInterlaceFlags );
                        [self PrintKeyVal:"Interlace Flags" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                        if (pMJPEGFormatDesc->bmInterlaceFlags & 0x01)
                            [self PrintKeyVal:"Interlaced Stream or Variable" val:"Yes" forDevice:deviceNumber atDepth:depth+2 forNode:node];
                            else
                                [self PrintKeyVal:"Interlaced Stream or Variable" val:"No" forDevice:deviceNumber atDepth:depth+2 forNode:node];

                        if (pMJPEGFormatDesc->bmInterlaceFlags & 0x02)
                            [self PrintKeyVal:"Fields per frame" val:"2" forDevice:deviceNumber atDepth:depth+2 forNode:node];
                        else
                            [self PrintKeyVal:"Fields per frame" val:"1" forDevice:deviceNumber atDepth:depth+2 forNode:node];

                        if (pMJPEGFormatDesc->bmInterlaceFlags & 0x04)
                            [self PrintKeyVal:"Field 1 first" val:"Yes" forDevice:deviceNumber atDepth:depth+2 forNode:node];
                        else
                            [self PrintKeyVal:"Field 1 first" val:"No" forDevice:deviceNumber atDepth:depth+2 forNode:node];

                        if (pMJPEGFormatDesc->bmInterlaceFlags & 0x08)
                            [self PrintKeyVal:"" val:"Reserved field used!" forDevice:deviceNumber atDepth:depth+2 forNode:node];

                        i = (pMJPEGFormatDesc->bmInterlaceFlags & 0x30) >> 4;
                        switch (i)
                        {
                            case 0: s = "Field 1 only"; break;
                            case 1: s = "Field 2 only"; break;
                            case 2: s = "Regular pattern of fields 1 and 2"; break;
                            case 3: s = "Random pattern of fields 1 and 2"; break;
                        }
                            sprintf((char *)buf, 	"%s", s );
                        [self PrintKeyVal:"Field Pattern" val:buf forDevice:deviceNumber atDepth:depth+2 forNode:node];

                        i = (pMJPEGFormatDesc->bmInterlaceFlags & 0xc0) >> 6;
                        switch (i)
                        {
                            case 0: s = "Bob only"; break;
                            case 1: s = "Weave only"; break;
                            case 2: s = "Bob or weave"; break;
                            case 3: s = "Unknown"; break;
                        }
                        sprintf((char *)buf, 	"%s", s );
                        [self PrintKeyVal:"Display Mode" val:buf forDevice:deviceNumber atDepth:depth+2 forNode:node];

                        if (pMJPEGFormatDesc->bCopyProtect == 1)
                            [self PrintKeyVal:"Duplication of Stream:" val:"Restricted" forDevice:deviceNumber atDepth:depth+1 forNode:node];
                        else
                            [self PrintKeyVal:"Duplication of Stream" val:"No Restriction" forDevice:deviceNumber atDepth:depth+1 forNode:node];
                        break;

                    case VS_FRAME_MJPEG:
                        pMJPEGFrameDesc = (IOUSBVDC_MJPEGFrameDescriptor *)desc;

                        sprintf((char *)buf, "%u", pMJPEGFrameDesc->bFrameIndex);
                        [self PrintKeyVal:"Frame Index:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                        sprintf((char *)buf, "(0x%x)", pMJPEGFrameDesc->bmCapabilities );
                        [self PrintKeyVal:"Capabilities " val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                        if (pMJPEGFrameDesc->bmCapabilities & 0x01)
                            [self PrintKeyVal:"" val:"Still Image supported" forDevice:deviceNumber atDepth:depth+2 forNode:node];

                        if ( pMJPEGFrameDesc->bmCapabilities & 0xfe)
                            [self PrintKeyVal:"" val:"Unknown capabilities" forDevice:deviceNumber atDepth:depth+2 forNode:node];

                            sprintf((char *)buf, "%u", Swap16(&pMJPEGFrameDesc->wWidth));
                        [self PrintKeyVal:"Width:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                        sprintf((char *)buf, "%u", Swap16(&pMJPEGFrameDesc->wHeight));
                        [self PrintKeyVal:"Height:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                        sprintf((char *)buf, "%lu", Swap32(&pMJPEGFrameDesc->dwMinBitRate));
                        [self PrintKeyVal:"Minimum Bit Rate (bps):" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                        sprintf((char *)buf, "%lu", Swap32(&pMJPEGFrameDesc->dwMaxBitRate));
                        [self PrintKeyVal:"Maximum Bit Rate (bps):" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                        //sprintf((char *)buf, "%u", pMJPEGFrameDesc->bAvgCompressRatio);
                        //[self PrintKeyVal:"Average Compress Ratio:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                        sprintf((char *)buf, "%lu", Swap32(&pMJPEGFrameDesc->dwMaxVideoFrameBufferSize));
                        [self PrintKeyVal:"Maximum frame buffer size (bytes):" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];
                        sprintf((char *)buf, "%lu", Swap32(&pMJPEGFrameDesc->dwDefaultFrameInterval));
                        [self PrintKeyVal:"Default Frame Interval:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];
                        if (pMJPEGFrameDesc->bFrameIntervalType == 0)
                        {
                            [self PrintKeyVal:"Frame interval type:" val:"Continuous" forDevice:deviceNumber atDepth:depth+1 forNode:node];

                            sprintf((char *)buf, "%lu ns", Swap32(&pMJPEGFrameDesc->dwMinFrameInterval) * 100 );
                            [self PrintKeyVal:"Shortest frame interval supported:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                            sprintf((char *)buf, "%lu ns", Swap32(&pMJPEGFrameDesc->dwMaxFrameInterval) * 100 );
                            [self PrintKeyVal:"Longest frame interval supported:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                            sprintf((char *)buf, "%lu ns", Swap32(&pMJPEGFrameDesc->dwFrameIntervalStep) * 100 );
                            [self PrintKeyVal:"Frame Interval step:" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];

                        }
                        else
                        {
                            // Need to recast as a IOUSBVDC_MJPEGDiscreteFrameDescriptor
                            //
                            pMJPEGDiscreteFrameDesc = (IOUSBVDC_MJPEGDiscreteFrameDescriptor *) pMJPEGFrameDesc;
                            sprintf((char *)buf, "%u", (pMJPEGDiscreteFrameDesc->bFrameIntervalType));
                            [self PrintKeyVal:"Discrete Frame Intervals supported" val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];
                            for (i = 0, t = &pMJPEGDiscreteFrameDesc->dwFrameInterval[0]; i < pMJPEGDiscreteFrameDesc->bFrameIntervalType; i++, t++ )
                            {
                                UInt32 interval = *t;
                                sprintf((char *)buf, "%lu", Swap32(&interval) );
                                sprintf((char *)buf2, "Frame Interval for frame %u", i+1 );
                                [self PrintKeyVal:buf2 val:buf forDevice:deviceNumber atDepth:depth+1 forNode:node];
                            }
                        }
                        break;
                        
                            default:
                        sprintf((char *)buf, "AudioStreaming Subclass" );
                        [self PrintKeyVal:buf val:"" forDevice:deviceNumber atDepth:depth+1 forNode:node];
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
