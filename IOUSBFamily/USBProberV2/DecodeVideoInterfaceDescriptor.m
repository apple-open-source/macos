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


#import "DecodeVideoInterfaceDescriptor.h"


@implementation DecodeVideoInterfaceDescriptor

+(void)decodeBytes:(UInt8 *)descriptor forDevice:(BusProbeDevice *)thisDevice withDeviceInterface:(IOUSBDeviceInterface **)deviceIntf
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
    
    if ( ((GenericAudioDescriptorPtr)desc)->descType != CS_INTERFACE )
        return;

    if( SC_VIDEOCONTROL == [[thisDevice lastInterfaceClassInfo] subclassNum] )
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
    else if( SC_VIDEOSTREAMING == [[thisDevice lastInterfaceClassInfo] subclassNum] )
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

    [thisDevice addProperty:buf withValue:"" atDepth:INTERFACE_LEVEL];

    // Print the Length and contents of this class-specific descriptor
    //
    sprintf(str, "%u", ((GenericAudioDescriptorPtr)desc)->descLen);
    [thisDevice addProperty:"Length (and contents):" withValue:str atDepth:INTERFACE_LEVEL+1];
    [DescriptorDecoder dumpRawDescriptor:(Byte*)desc forDevice:thisDevice atDepth:INTERFACE_LEVEL+2];

    
    if( SC_VIDEOCONTROL == [[thisDevice lastInterfaceClassInfo] subclassNum] ) // Video Control Subclass
    {
        switch ( ((GenericAudioDescriptorPtr)desc)->descSubType )
        {
            case VC_HEADER:
                pVideoControlHeader = (IOUSBVCInterfaceDescriptor *)desc;
                i = Swap16(&pVideoControlHeader->bcdVDC);
                sprintf((char *)buf, "%1x%1x.%1x%1c",
                        (i>>12)&0x000f, (i>>8)&0x000f, (i>>4)&0x000f, MapNumberToVersion((i>>0)&0x000f));
                [thisDevice addProperty:"Specification Version Number:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, "%u", Swap16(&pVideoControlHeader->wTotalLength) );

                sprintf((char *)buf, "%lu", Swap32(&pVideoControlHeader->dwClockFrequency) );
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

                if( !pVideoInTermDesc->bAssocTerminal )
                    sprintf((char *)buf, "%u [NONE]", pVideoInTermDesc->bAssocTerminal );
                else
                    sprintf((char *)buf, "%u", pVideoInTermDesc->bAssocTerminal );

                [thisDevice addProperty:"Input Terminal ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];


                if( !pVideoInTermDesc->iTerminal )
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
                    for (i = 0, p = &pCameraTermDesc->bmControls[0]; i < pCameraTermDesc->bControlSize; i++, p++ )
                    {
                        // For 0.8, only 18 bits are defined:
                        //
                        if ( i > 2 )
                        {
                            sprintf((char *)buf, "Unknown");
                            [thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
                            break;
                        }

                        if ( (*p) & (1 << 0) )
                        {
                            if ( i == 0 ) 	sprintf((char *)buf, "Scanning Mode");
                            else if ( i == 1 ) 	sprintf((char *)buf, "Iris (Relative)");
                            else if ( i == 2 ) 	sprintf((char *)buf, "Iris (Relative)");
                            if ( strcmp(buf,"") )
                                [thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
                        }

                        if ( (*p) & (1 << 1) )
                        {
                            if ( i == 0 ) 	sprintf((char *)buf, "Auto Exposure Mode");
                            else if ( i == 1 ) 	sprintf((char *)buf, "Zoom (Absolute)");
                            else if ( i == 2 ) 	sprintf((char *)buf, "Tilt (Relative)");
                            if ( strcmp(buf,"") )
                                [thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
                        }

                        if ( (*p) & (1 << 2) )
                        {
                            if ( i == 0 ) 	sprintf((char *)buf, "Auto Exposure Priority");
                            else if ( i == 1 ) 	sprintf((char *)buf, "Zoom (Relative)");
                            else if ( i == 2 ) 	sprintf((char *)buf, "Focus, Auto");
                            if ( strcmp(buf,"") )
                                [thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
                        }

                        if ( (*p) & (1 << 3) )
                        {
                            if ( i == 0 ) 	sprintf((char *)buf, "Exposure Time (Absolute)");
                            else if ( i == 1 ) 	sprintf((char *)buf, "Pan (Absolute)");
                            else if ( i == 2 ) 	sprintf((char *)buf, "Unknown");
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

                if( !pVideoOutTermDesc->bAssocTerminal )
                    sprintf((char *)buf, "%u [NONE]", pVideoOutTermDesc->bAssocTerminal );
                else
                    sprintf((char *)buf, "%u", pVideoOutTermDesc->bAssocTerminal );

                [thisDevice addProperty:"Output Terminal ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];


                if( !pVideoOutTermDesc->iTerminal )
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
                if( !pSelectorUnit2Desc->iSelector )
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
                for (i = 0, p = &pProcessingUnitDesc->bmControls[0]; i < pProcessingUnitDesc->bControlSize; i++, p++ )
                {
                    // For 0.8, only 16 bits are defined:
                    //
                    if ( i > 1 )
                    {
                        sprintf((char *)buf, "Unknown");
                        [thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
                        break;
                    }

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
                if( !pProcessingUnit2Desc->iProcessing )
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
                pExtensionUnitDesc = (IOUSBVCExtensionUnitDescriptor *)desc;

                sprintf((char *)buf, 	"%u", pExtensionUnitDesc->bUnitID );
                [thisDevice addProperty:"Unit ID:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                uuidHI = Swap64(&pExtensionUnitDesc->guidExtensionCodeHi);
                uuidLO = Swap64(&pExtensionUnitDesc->guidExtensionCodeLo);
                
                sprintf((char *)buf, 	"%8.8lx-%4.4lx-%4.4lx-%4.4lx%12.12qx", (UInt32) (uuidHI>>32), (UInt32) ( (uuidHI & 0xffff0000)>>16), (UInt32)(uuidHI & 0x0000ffff), (UInt32) ( (uuidLO & 0xffff000000000000ULL)>>48), (uuidLO & 0x0000FFFFFFFFFFFFULL) );
                [thisDevice addProperty:"Vendor UUID:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                sprintf((char *)buf, 	"%u", pExtensionUnitDesc->bNumControls );
                [thisDevice addProperty:"Number of Controls:" withValue:buf atDepth:INTERFACE_LEVEL+1];


                sprintf((char *)buf, 	"%u", pExtensionUnitDesc->bNrInPins );
                [thisDevice addProperty:"Number of In pins:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                for (i = 0, p = &pExtensionUnitDesc->baSourceID[0]; i < pSelectorUnitDesc->bNrInPins; i++, p++ )
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
                    // For 0.8, only 1 bits is defined:
                    //
                    if ( i == 0 )
                    {
                        if ( (*p) & 0x01)
                        {
                            sprintf((char *)buf, "Enable Processing");
                            [thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
                        }

                        if ( (*p) & 0xfe)
                        {
                            sprintf((char *)buf, "Using Reserved Bit!");
                            [thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
                        }
                    }
                    else
                    if ( i > 1 )
                    {
                        sprintf((char *)buf, "Vendor Specific 0x%x", (*p));
                        [thisDevice addProperty:"" withValue:buf atDepth:INTERFACE_LEVEL+2];
                        break;
                    }
                }

                // At this point, p should be pointing to the iProcessing field:
                pExtensionUnit3Desc = ( IOUSBVCExtensionUnit3Descriptor *) p;
                if( !pExtensionUnit3Desc->iExtension )
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
    else if( SC_VIDEOSTREAMING == [[thisDevice lastInterfaceClassInfo] subclassNum] ) // Video Streaming Subclass
    {
        switch ( ((GenericAudioDescriptorPtr)desc)->descSubType )
                {
                    case VS_INPUT_HEADER:
                        pVSInputHeaderDesc = (IOUSBVSInputHeaderDescriptor *)desc;

                        sprintf((char *)buf, "%u", pVSInputHeaderDesc->bNumFormats );
                        [thisDevice addProperty:"Number of Formats:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                        sprintf((char *)buf, "%u", Swap16(&pVSInputHeaderDesc->wTotalLength) );
                        [thisDevice addProperty:"Total Length of Descriptor:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                        sprintf((char *)buf, "0x%x", pVSInputHeaderDesc->bEndpointAddress );
                        [thisDevice addProperty:"Endpoint Address:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                        sprintf((char *)buf, "Capabilities (0x%x)", pVSInputHeaderDesc->bmInfo );
                        [thisDevice addProperty:buf withValue:"Description" atDepth:INTERFACE_LEVEL+1];

                        if (pVSInputHeaderDesc->bmInfo & 0x01)
                            [thisDevice addProperty:"" withValue:"Dynamic Format Change supported" atDepth:INTERFACE_LEVEL+2];

                        if ( pVSInputHeaderDesc->bmInfo & 0xfe)
                            [thisDevice addProperty:"" withValue:"Unknown capabilities" atDepth:INTERFACE_LEVEL+2];

                        sprintf((char *)buf, "%u", pVSInputHeaderDesc->bTerminalLink );
                        [thisDevice addProperty:"Terminal ID of Output Terminal:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                        switch (pVSInputHeaderDesc->bTerminalLink)
                        {
                            case 0: s = "None"; break;
                            case 1: s = "Method 1"; break;
                            case 2: s = "Method 2"; break;
                            case 3: s = "Method 3"; break;
                            default: s = "Unknown Method";
                        }
                        sprintf((char *)buf, 	"%u (%s)", pVSInputHeaderDesc->bTerminalLink, s );
                        [thisDevice addProperty:"Still Capture Method:" withValue:buf atDepth:INTERFACE_LEVEL+1];
                        
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

                        sprintf((char *)buf, "%u", pMJPEGFormatDesc->bFormatIndex);
                        [thisDevice addProperty:"Format Index:" withValue:buf atDepth:INTERFACE_LEVEL+2];

                        sprintf((char *)buf, "%u", pMJPEGFormatDesc->bNumFrameDescriptors);
                        [thisDevice addProperty:"Number of Frame Descriptors:" withValue:buf atDepth:INTERFACE_LEVEL+2];

                        sprintf((char *)buf, "(0x%x)", pMJPEGFormatDesc->bmFlags );
                        [thisDevice addProperty:"Characteristics " withValue:buf atDepth:INTERFACE_LEVEL+1];

                        if (pMJPEGFormatDesc->bmFlags & 0x01)
                            [thisDevice addProperty:"" withValue:"Fixed Sample Sizes Supported" atDepth:INTERFACE_LEVEL+2];

                        if ( pMJPEGFormatDesc->bmFlags & 0xfe)
                            [thisDevice addProperty:"" withValue:"Unknown characteristics" atDepth:INTERFACE_LEVEL+2];

                        //sprintf((char *)buf, "%u", pMJPEGFormatDesc->bBitsPerPixel);
                        //[thisDevice addProperty:"Bits per pixel in frame:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                        sprintf((char *)buf, "%u", pMJPEGFormatDesc->bDefaultFrameIndex);
                        [thisDevice addProperty:"Optimum Frame Index:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                        sprintf((char *)buf, "%u", pMJPEGFormatDesc->bAspectRatioX);
                        [thisDevice addProperty:"X Aspect Ratio:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                        sprintf((char *)buf, "%u", pMJPEGFormatDesc->bAspectRatioY);
                        [thisDevice addProperty:"Y Aspect Ratio:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                        sprintf((char *)buf, "(0x%x)", pMJPEGFormatDesc->bmInterlaceFlags );
                        [thisDevice addProperty:"Interlace Flags" withValue:buf atDepth:INTERFACE_LEVEL+1];

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
                            [thisDevice addProperty:"Duplication of Stream:" withValue:"Restricted" atDepth:INTERFACE_LEVEL+1];
                        else
                            [thisDevice addProperty:"Duplication of Stream" withValue:"No Restriction" atDepth:INTERFACE_LEVEL+1];
                        break;

                    case VS_FRAME_MJPEG:
                        pMJPEGFrameDesc = (IOUSBVDC_MJPEGFrameDescriptor *)desc;

                        sprintf((char *)buf, "%u", pMJPEGFrameDesc->bFrameIndex);
                        [thisDevice addProperty:"Frame Index:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                        sprintf((char *)buf, "(0x%x)", pMJPEGFrameDesc->bmCapabilities );
                        [thisDevice addProperty:"Capabilities " withValue:buf atDepth:INTERFACE_LEVEL+1];

                        if (pMJPEGFrameDesc->bmCapabilities & 0x01)
                            [thisDevice addProperty:"" withValue:"Still Image supported" atDepth:INTERFACE_LEVEL+2];

                        if ( pMJPEGFrameDesc->bmCapabilities & 0xfe)
                            [thisDevice addProperty:"" withValue:"Unknown capabilities" atDepth:INTERFACE_LEVEL+2];

                            sprintf((char *)buf, "%u", Swap16(&pMJPEGFrameDesc->wWidth));
                        [thisDevice addProperty:"Width:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                        sprintf((char *)buf, "%u", Swap16(&pMJPEGFrameDesc->wHeight));
                        [thisDevice addProperty:"Height:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                        sprintf((char *)buf, "%lu", Swap32(&pMJPEGFrameDesc->dwMinBitRate));
                        [thisDevice addProperty:"Minimum Bit Rate (bps):" withValue:buf atDepth:INTERFACE_LEVEL+1];

                        sprintf((char *)buf, "%lu", Swap32(&pMJPEGFrameDesc->dwMaxBitRate));
                        [thisDevice addProperty:"Maximum Bit Rate (bps):" withValue:buf atDepth:INTERFACE_LEVEL+1];

                        //sprintf((char *)buf, "%u", pMJPEGFrameDesc->bAvgCompressRatio);
                        //[thisDevice addProperty:"Average Compress Ratio:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                        sprintf((char *)buf, "%lu", Swap32(&pMJPEGFrameDesc->dwMaxVideoFrameBufferSize));
                        [thisDevice addProperty:"Maximum frame buffer size (bytes):" withValue:buf atDepth:INTERFACE_LEVEL+1];
                        sprintf((char *)buf, "%lu", Swap32(&pMJPEGFrameDesc->dwDefaultFrameInterval));
                        [thisDevice addProperty:"Default Frame InterwithValue:" withValue:buf atDepth:INTERFACE_LEVEL+1];
                        if (pMJPEGFrameDesc->bFrameIntervalType == 0)
                        {
                            [thisDevice addProperty:"Frame interval type:" withValue:"Continuous" atDepth:INTERFACE_LEVEL+1];

                            sprintf((char *)buf, "%lu ns", Swap32(&pMJPEGFrameDesc->dwMinFrameInterval) * 100 );
                            [thisDevice addProperty:"Shortest frame interval supported:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                            sprintf((char *)buf, "%lu ns", Swap32(&pMJPEGFrameDesc->dwMaxFrameInterval) * 100 );
                            [thisDevice addProperty:"Longest frame interval supported:" withValue:buf atDepth:INTERFACE_LEVEL+1];

                            sprintf((char *)buf, "%lu ns", Swap32(&pMJPEGFrameDesc->dwFrameIntervalStep) * 100 );
                            [thisDevice addProperty:"Frame Interval step:" withValue:buf atDepth:INTERFACE_LEVEL+1];

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
                                sprintf((char *)buf, "%lu", Swap32(&interval) );
                                sprintf((char *)buf2, "Frame Interval for frame %u", i+1 );
                                [thisDevice addProperty:buf2 withValue:buf atDepth:INTERFACE_LEVEL+1];
                            }
                        }
                        break;
                        
                            default:
                        sprintf((char *)buf, "AudioStreaming Subclass" );
                        [thisDevice addProperty:buf withValue:"" atDepth:INTERFACE_LEVEL+1];
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
